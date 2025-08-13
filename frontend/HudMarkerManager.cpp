/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : HudMarkerManager.cpp
// PURPOSE : Manages 2D world markers (instancing, layout, etc)
// AUTHOR  : Aaron Baumbach (Ruffian)
// STARTED : 06/02/2020
//
/////////////////////////////////////////////////////////////////////////////////

#include "HudMarkerManager.h"

#include "HudMarkerRenderer.h"

#include "grcore/debugdraw.h"

#include "frontend/MiniMap.h"
#include "frontend/Map/BlipEnums.h"
#include "frontend/HudTools.h"
#include "frontend/ui_channel.h"

#include "Camera/Base/BaseCamera.h"
#include "Camera/CamInterface.h"
#include "Camera/Viewports/ViewportManager.h"

#include "scene/world/GameWorld.h"
#include "script/script_hud.h"
#include "system/controlMgr.h"
#include "Peds/ped.h"

static bool g_HudMarkerOcclusionTestEnabled = false;
static float g_HudMarkerFocusClearGracePeriod = 0.3f;

FRONTEND_OPTIMISATIONS()

CHudMarkerManager* CHudMarkerManager::sm_instance = nullptr;

void CHudMarkerManager::Init(unsigned Mode)
{
	uiDisplayf("CHudMarkerManager::Init - Mode=[%d]", Mode);
	if(Mode == INIT_CORE)
	{
		uiAssertf(sm_instance == nullptr, "HudMarkerManager instance exists during INIT_CORE");
		sm_instance = rage_new CHudMarkerManager();
	}
	if (sm_instance)
	{
		sm_instance->Init_Internal(Mode);
	}
}

void CHudMarkerManager::Shutdown(unsigned Mode)
{
	uiDisplayf("CHudMarkerManager::Shutdown - Mode=[%d]", Mode);
	if (sm_instance)
	{
		sm_instance->Shutdown_Internal(Mode);
	}
	if (Mode == INIT_CORE)
	{
		if (sm_instance)
		{
			delete sm_instance;
			sm_instance = nullptr;
		}
	}
}

void CHudMarkerManager::DeviceLost()
{
}

void CHudMarkerManager::DeviceReset()
{
	if (sm_instance)
	{
		sm_instance->DeviceReset_Internal();
	}
}

#if __BANK
void CHudMarkerManager::InitWidgets()
{
	bkBank *pBank = BANKMGR.FindBank(UI_DEBUG_BANK_NAME);

	if (!pBank)  // create the bank if not found
	{
		pBank = &BANKMGR.CreateBank(UI_DEBUG_BANK_NAME);
	}

	if (uiVerify(sm_instance) && pBank)
	{
		pBank->AddButton("Create HudMarker widgets", datCallback(MFA(CHudMarkerManager::CreateBankWidgets), (datBase*)sm_instance));
	}
}
void CHudMarkerManager::ShutdownWidgets()
{
	bkBank *pBank = BANKMGR.FindBank(UI_DEBUG_BANK_NAME);

	if (pBank)
	{
		pBank->Destroy();
	}
}
#endif //__BANK

bool CHudMarkerManager::IsInitialised() const
{
	return m_markerStates.GetCount() > 0;
}

void CHudMarkerManager::SetIsEnabled(bool IsEnabled)
{
	if (m_isEnabled != IsEnabled)
	{
		m_isEnabled = IsEnabled;
	}
}

bool CHudMarkerManager::ShouldRender() const
{
	return m_isEnabled && !m_hideThisFrame;
}

void CHudMarkerManager::UpdateMovieConfig()
{
	if (m_renderer)
	{
		m_renderer->UpdateMovieConfig();
	}
}

void CHudMarkerManager::Update()
{
	PF_AUTO_PUSH_TIMEBAR("CHudMarkerManager::Update()");

	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on UT
	{
		uiAssertf(0, "CHudMarkerManager::Update - can only be called on the UpdateThread!");
		return;
	}

	//Early out if the system is disabled
	if (!m_isEnabled)
	{
		return;
	}

#if __BANK
	m_numActive = m_activeIds.GetCount();
	m_numDebug = m_debugMarkers.GetCount();
#endif

	const grcViewport* GameViewport = gVpMan.GetCurrentGameGrcViewport();
	if (GameViewport == nullptr)
	{
		uiAssertf(0, "CHudMarkerManager::Update - failed to resolve GrcViewport");
		return;
	}

	CPed* FollowPlayer = CGameWorld::FindFollowPlayer();
	if (FollowPlayer == nullptr)
	{
		uiAssertf(0, "CHudMarkerManager::Update - failed to resolve FollowPlayer");
		return;
	}

	//Determine frame visibility
	if (!CScriptHud::bDisplayHud || 
		gVpMan.AreWidescreenBordersActive() || 
		CScriptHud::bDontDisplayHudOrRadarThisFrame || 
		!CPauseMenu::GetMenuPreference(PREF_DISPLAY_HUD) || 
		!CVfxHelper::ShouldRenderInGameUI())
	{
		m_hideThisFrame = true;
	}

	//Reset transient variables
	m_cachedAspectRatio = CHudTools::GetAspectRatio();
	m_cachedAspectMultiplier = CHudTools::GetAspectRatioMultiplier();
	m_cachedPlayerPedPosition = FollowPlayer->GetPreviousPosition();
	m_cachedMinimapArea = CalculateMinimapArea();

	m_markersWithDisplayText.Reset(false);

	fwRect ClampBoundary = CalculateClampBoundary();

	//Pre update group init
	m_isAnyGroupSolo = false;
	for (HudMarkerGroupID ID = HudMarkerGroupID::Min(); ID.IsValid(); ++ID)
	{
		m_markerGroups[ID].Reset();
		m_isAnyGroupSolo |= m_markerGroups[ID].IsSolo;
	}

	//Early out if we have no markers
	if (m_activeIds.IsEmpty())
	{
		return;
	}

	//Sort: world space distance, descending (as we reverse iterate)
	std::stable_sort(m_activeIds.begin(), m_activeIds.end(), [this](HudMarkerID A, HudMarkerID B)
	{
		return	(m_markerStates[A].PedDistance > m_markerStates[B].PedDistance) ||
				(m_markerStates[B].IsFocused) ||
				(m_markerStates[B].IsWaypoint && !m_markerStates[A].IsFocused);
	});

	uiAssert(m_occlusionTestFrameSkip >= 0);
	int OcclusionTestFrameSpread = m_occlusionTestFrameSkip + 1;
	m_occlusionTestFrame = ++m_occlusionTestFrame % OcclusionTestFrameSpread;

	//Update state of active markers
	for (int i = m_activeIds.GetCount() - 1; i >= 0; --i) //reverse iterate so we can safely remove
	{
		const auto& ID = m_activeIds[i];
		auto& MarkerState = m_markerStates[ID];
		auto GroupPtr = MarkerState.GroupID.IsValid() ? &m_markerGroups[MarkerState.GroupID] : nullptr;

		const bool CanPerformOcclusionTest = (i % OcclusionTestFrameSpread) == m_occlusionTestFrame;

		if (MarkerState.IsPendingRemoval) 
		{
			if (MarkerState.HasRenderableShutdown) //Wait for renderer to cleanup
			{
				RemoveMarker_Internal(i);
			}
		}
		else if (UpdateMarker(MarkerState, GroupPtr, ClampBoundary, FollowPlayer, *GameViewport, CanPerformOcclusionTest))
		{
			if (MarkerState.IsVisible && !MarkerState.IsClamped && MarkerState.DistanceTextAlpha > 0)
			{
				m_markersWithDisplayText.PushAndGrow(ID);
			}
		}
		else
		{
			MarkerState.IsPendingRemoval = true;
		}
	}

	UpdateWaypoint();

#if __BANK
	if (ShouldRender())
	{
		RenderDebug(ClampBoundary);
	}
#endif
}

void CHudMarkerManager::AddDrawListCommands()
{
	//Add draw list commands
	bool DidRenderThisFrame = false;
	if (m_renderer && ShouldRender())
	{
		m_renderer->AddDrawListCommands(m_activeIds, m_markerStates);
		DidRenderThisFrame = true;
	}

	//Handle change in system visibility
	if (DidRenderThisFrame != m_didRenderLastFrame)
	{
		if (!DidRenderThisFrame) //If we've hidden all the markers this frame, reset their alphas so they fade back in...
		{
			for (int i = 0; i < m_activeIds.GetCount(); ++i)
			{
				const auto& ID = m_activeIds[i];
				auto& MarkerState = m_markerStates[ID];
				MarkerState.Alpha = 0;
				MarkerState.Scale = 0;
			}
		}
	}

	m_didRenderLastFrame = DidRenderThisFrame;
	m_hideThisFrame = false;
}

bool CHudMarkerManager::IsMarkerValid(HudMarkerID ID) const
{
	return ID.IsValid() && (m_activeIds.Find(ID) != -1) && !m_markerStates[ID].IsPendingRemoval;
}

HudMarkerID CHudMarkerManager::CreateMarker(const SHudMarkerState& InitState)
{
	HudMarkerID ID;

	if (!uiVerify(IsInitialised()))
	{
		return ID;
	}

#if __BANK
	m_waterMark++;
	m_highWaterMark = MAX(m_waterMark, m_highWaterMark);
#endif

	if (!m_freeIds.IsEmpty())
	{
		ID = m_freeIds.Pop();
		m_activeIds.Push(ID);

		SHudMarkerState& State = m_markerStates[ID];
		State = InitState;
		State.ID = ID;

		uiDebugf1("CHudMarkerManager::CreateMarker - ID=[%d] ActiveCount[%d]", ID.Get(), m_activeIds.GetCount());
	}
	else
	{
#if __BANK
		uiAssertf(m_highWaterMark == MAX_NUM_MARKERS, "CHudMarkerManager::CreateMarker - Max num reached - HighWaterMark=[%d]", m_highWaterMark);
		uiWarningf("CHudMarkerManager::CreateMarker - FAILED - HighWaterMark=[%d] - consider increasing MAX_NUM_MARKERS", m_highWaterMark);
#endif
	}

	return ID;
}

HudMarkerID CHudMarkerManager::CreateMarkerForBlip(s32 BlipID, SHudMarkerState* outState)
{
	if (CMiniMap::IsBlipIdInUse(BlipID))
	{
		if (auto IDPtr = m_blipMarkerMap.Access(BlipID))
		{
			return *IDPtr; //there is already a marker for this blip
		}

		SHudMarkerState NewState;
		NewState.BlipID = BlipID;

		if (outState)
		{
			*outState = NewState;
		}

		uiDebugf1("CHudMarkerManager::CreateMarkerForBlip - BlipID=[%d]", BlipID);

		auto ID = CreateMarker(NewState);
		if (ID.IsValid())
		{
			m_blipMarkerMap.Insert(BlipID, ID);
		}
		return ID;
	}
	uiWarningf("CHudMarkerManager::CreateMarkerForBlip - FAILED - BlipID=[%d] not in use", BlipID);
	return HudMarkerID();
}

const SHudMarkerState* CHudMarkerManager::GetMarkerState(HudMarkerID ID) const
{
	if (IsMarkerValid(ID))
	{
		return &m_markerStates[ID];
	}
	return nullptr;
}

SHudMarkerState* CHudMarkerManager::GetMarkerState(HudMarkerID ID)
{
	if (IsMarkerValid(ID))
	{
		return &m_markerStates[ID];
	}
	return nullptr;
}

const SHudMarkerGroup* CHudMarkerManager::GetMarkerGroup(HudMarkerGroupID ID) const
{
	if (ID.IsValid())
	{
		return &m_markerGroups[ID];
	}
	return nullptr;
}

SHudMarkerGroup* CHudMarkerManager::GetMarkerGroup(HudMarkerGroupID ID)
{
	if (ID.IsValid())
	{
		return &m_markerGroups[ID];
	}
	return nullptr;
}

bool CHudMarkerManager::RemoveMarker(HudMarkerID& ID)
{
	if (IsMarkerValid(ID))
	{
		m_markerStates[ID].IsPendingRemoval = true;
		return true;
	}
	ID.Invalidate();
	return false;
}

CHudMarkerManager::CHudMarkerManager()
	: m_isEnabled(true)
	, m_hideThisFrame(false)
	, m_didRenderLastFrame(false)
	, m_clampMode(ClampMode_Radial)
	, m_clampInsetX(0.05f)
	, m_clampInsetY(0.15f)
	, m_clampCloseProximityMinDistance(50.0f)
	, m_clampCloseProximityMaxDistance(200.0f)
	, m_clampCloseProximityInset(0.0f)
	, m_markerScreenHalfSize(0.04f)
	, m_fadeOverDistanceAlpha(200)
	, m_fadeOverDistanceMin(100.0f)
	, m_fadeOverDistanceMax(500.0f)
	, m_scaleMin(0.75f)
	, m_scaleMax(1.25f)
	, m_scaleDistanceMin(100.0f)
	, m_scaleDistanceMax(500.0f)
	, m_occludedAlpha(200)
	, m_hideDistanceTextWhenClamped(true)
	, m_occlusionTestFrame(0)
	, m_occlusionTestFrameSkip(15)
	, m_minimapArea(0.0f, 1.0f, 0.15f, 0.8f)
	, m_minimapAreaBig(0.0f, 1.0f, 0.23f, 0.58f)
	, m_isAnyGroupSolo(false)
	, m_focusRadius(0.1f)
	, m_lastFocusTime(0)
	, m_pendingFocusInput(false)
	, m_focusInputPressedTime(0)
#if __BANK
	, m_waterMark(0)
	, m_highWaterMark(0)
	, m_renderDebugMarkers(false)
	, m_renderDebugText(false)
	, m_renderClampBoundary(false)
	, m_renderMinimapArea(false)
	, m_renderFocusRadius(false)
	, m_pushPopNum(1)
	, m_pushGroup(HudMarkerGroupID::Min())
	, m_pushRange(200.0f)
	, m_numActive(0)
	, m_numDebug(0)
#endif
{
	m_renderer = rage_new CHudMarkerRenderer();
	FatalAssert(m_renderer);

	uiDisplayf("CHudMarkerManager - alloc[%d]", (int)sizeof(*this));
}

CHudMarkerManager::~CHudMarkerManager()
{
	delete m_renderer;
	m_renderer = nullptr;
}

void CHudMarkerManager::Init_Internal(unsigned Mode)
{
	if (Mode == INIT_SESSION)
	{
		m_markerStates.Reset();
		for (int i = 0; i < m_markerStates.GetMaxCount(); i++)
		{
			m_markerStates.Push(SHudMarkerState());
		}
		m_activeIds.Reset();
		m_freeIds.Reset();
		for (HudMarkerID ID = HudMarkerID::Max(); ID.IsValid(); --ID)
		{
			m_freeIds.Push(ID);
		}
		m_markerGroups.Reset();
		for (int i = 0; i < m_markerGroups.GetMaxCount(); i++)
		{
			m_markerGroups.Push(SHudMarkerGroup());
		}
		m_focusedMarker.Invalidate();
		m_lastFocusTime = 0;
		m_pendingFocusInput = false;
		m_focusInputPressedTime = 0;
		m_pendingFocusSelection.Invalidate();
		m_activeWaypointMarker.Invalidate();
	}
	if (m_renderer)
	{
		m_renderer->Init(Mode);
	}
}

void CHudMarkerManager::Shutdown_Internal(unsigned Mode)
{
	if (m_renderer)
	{
		m_renderer->Shutdown(Mode);
	}
	if (Mode == SHUTDOWN_CORE)
	{
#if __BANK
		ShutdownWidgets();
#endif
	}
	else if (Mode == SHUTDOWN_SESSION)
	{
		m_markerStates.Reset();
	}
}

void CHudMarkerManager::DeviceReset_Internal()
{
	if (m_renderer)
	{
		m_renderer->DeviceReset();
	}
}

void CHudMarkerManager::RemoveMarker_Internal(int ActiveIndex)
{
#if __BANK
	m_waterMark--;
#endif
	if (uiVerify(ActiveIndex >= 0 && ActiveIndex < m_activeIds.GetMaxCount()))
	{
		const auto& ID = m_activeIds[ActiveIndex];

		if (m_markerStates[ID].BlipID != INVALID_BLIP_ID)
		{
			m_blipMarkerMap.Delete(m_markerStates[ID].BlipID);
		}

		m_markerStates[ID].~SHudMarkerState(); //call destructor directly as we're not actually freeing up the element

		m_freeIds.Push(m_activeIds[ActiveIndex]);
		m_activeIds.DeleteFast(ActiveIndex);

		uiDebugf1("CHudMarkerManager::RemoveMarker_Internal - ID=[%d]", ID.Get());
	}
}

//Updates the marker state
//Return: false if the marker should be removed
bool CHudMarkerManager::UpdateMarker(SHudMarkerState& State, SHudMarkerGroup* Group, const fwRect& ClampBoundary, CPed* FollowPed, const grcViewport& Viewport, bool CanPerformOcclusionTest) const
{
	if (State.BlipID != INVALID_BLIP_ID && !UpdateMarkerFromBlip(State.BlipID, State))
	{
		State.IsVisible = false;
		return false;
	}

	State.PedDistance = (State.WorldPosition  - FollowPed->GetPreviousPosition()).XYMag();
	State.CamDistanceSq = (State.WorldPosition - VEC3V_TO_VECTOR3(Viewport.GetCameraPosition())).XYMag2();

	bool ShouldClamp = State.ClampEnabled;

	float MinDistance = 0.0f;
	float MaxDistance = State.PedCullDistance;
	bool SkipMaxDistanceCheck = false;
	
	int TargetAlpha = 255;
	int TargetDistanceTextAlpha = 255;
	if (m_hideDistanceTextWhenClamped && State.IsClamped && !State.IsWaypoint)
	{
		TargetDistanceTextAlpha = 0;
	}

	//Resolve group behaviours
	if (Group)
	{
		SkipMaxDistanceCheck |= Group->NumVisible < Group->ForceVisibleNum;
		MinDistance = Group->MinDistance;
		MaxDistance = Group->MaxDistance;
		if (Group->NumVisible < Group->MaxVisible && !Group->IsMuted && (!m_isAnyGroupSolo || Group->IsSolo))
		{
			Group->NumVisible++;
			if (ShouldClamp && Group->NumClamped < Group->MaxClampNum)
			{
				if (Group->NumClamped == 0 && Group->AlwaysShowDistanceTextForClosest)
				{
					TargetDistanceTextAlpha = 255;
				}
				Group->NumClamped++;
			}
			else
			{
				ShouldClamp = false;
			}

			if (State.PedDistance < Group->MinTextDistance)
			{
				TargetDistanceTextAlpha = 0;
			}
		}
		else
		{
			ShouldClamp = false;
			TargetAlpha = 0;
		}
	}

	//Force waypoint behaviours
	if (State.IsWaypoint)
	{
		SkipMaxDistanceCheck = true;
		ShouldClamp = true;
	}

	ProjectMarker(State, ShouldClamp, ClampBoundary, Viewport);

	//Hide for one frame to make sure we're fully initialised
	if (!State.IsInitialised)
	{
		State.IsVisible = false;
		State.IsInitialised = true;
	}

	//Perform alpha fading
	if (State.IsVisible)
	{
		if (State.IsFocused || State.IsWaypoint)
		{
			TargetAlpha = 255;
		}
		else if (!SkipMaxDistanceCheck && State.PedDistance > MaxDistance)
		{
			TargetAlpha = 0;
		}
		else
		{
			const float alphaOverDistance = 1.0f - ClampRange(State.PedDistance, m_fadeOverDistanceMin, m_fadeOverDistanceMax);
			TargetAlpha = (int)MIN(TargetAlpha, m_fadeOverDistanceAlpha + ((255 - m_fadeOverDistanceAlpha) * alphaOverDistance));

			//Disable occlusion while clamped
			if (State.IsClamped)
			{
				CanPerformOcclusionTest = false;
				State.IsOccluded = false;
			}

			//Occlusion test
			if (g_HudMarkerOcclusionTestEnabled && CanPerformOcclusionTest)
			{
				//TODO: PERF: consider making this async?
				Vector3 vTraceStart = State.WorldPosition;
				Vector3 vTraceEnd = VEC3V_TO_VECTOR3(Viewport.GetCameraPosition());
				static const s32 iCollisionFlags = ArchetypeFlags::GTA_ALL_MAP_TYPES;
				static const s32 iIgnoreFlags = WorldProbe::LOS_IGNORE_SHOOT_THRU | WorldProbe::LOS_IGNORE_SHOOT_THRU_FX | WorldProbe::LOS_NO_RESULT_SORTING;
				WorldProbe::CShapeTestProbeDesc probeDesc;
				WorldProbe::CShapeTestFixedResults<> probeResult;
				probeDesc.SetResultsStructure(&probeResult);
				probeDesc.SetStartAndEnd(vTraceStart, vTraceEnd);
				probeDesc.SetIncludeFlags(iCollisionFlags);
				probeDesc.SetExcludeEntity(FollowPed);
				probeDesc.SetOptions(iIgnoreFlags);
				probeDesc.SetContext(WorldProbe::EHud);
				taskAssertf(FPIsFinite(vTraceStart.x) && FPIsFinite(vTraceStart.y) && FPIsFinite(vTraceStart.z), "CHudMarkerManager::UpdateMarker: Occlusion");
				State.IsOccluded = WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
			}

			if (State.IsOccluded)
			{
				TargetAlpha = MIN(TargetAlpha, m_occludedAlpha);
			}
			else if (State.PedDistance < MinDistance)
			{
				TargetAlpha = 0;
			}
		}

		//Hide overlaping distance text - assumes distance accending order
		float OverlapDist = m_markerScreenHalfSize * 2.0f * State.Scale;
		for (int i = 0; i < m_markersWithDisplayText.GetCount(); ++i)
		{
			if (const auto OtherMarkerState = GetMarkerState(m_markersWithDisplayText[i]))
			{
				const Vector2 AdjustedSP(State.ScreenPosition.x, State.ScreenPosition.y);
				const Vector2 OtherAdjustedSP(OtherMarkerState->ScreenPosition.x, OtherMarkerState->ScreenPosition.y);
				const Vector2 Delta = AdjustedSP - OtherAdjustedSP;
				if (abs(Delta.x) < (OverlapDist / m_cachedAspectRatio) && abs(Delta.y) < OverlapDist)
				{
					TargetDistanceTextAlpha = 0;
					break;
				}
			}
		}

		//Smooth Alpha
		if (State.Alpha != TargetAlpha)
		{
			int Step = (int)MAX(1.0f, 255.0f * 4.0f * fwTimer::GetTimeStep());
			State.Alpha = (u8)(State.Alpha + Clamp(TargetAlpha - State.Alpha, -Step, Step));
		}

		//Smooth Distance Text Alpha
		TargetDistanceTextAlpha = MIN(TargetDistanceTextAlpha, TargetAlpha);
		if (State.DistanceTextAlpha != TargetDistanceTextAlpha)
		{
			int Step = (int)MAX(1.0f, 255.0f * 4.0f * fwTimer::GetTimeStep());
			State.DistanceTextAlpha = (u8)(State.DistanceTextAlpha + Clamp(TargetDistanceTextAlpha - State.DistanceTextAlpha, -Step, Step));
		}

		//Update visibility given alpha (perf)
		State.IsVisible = State.Alpha != 0;
	}
	else
	{
		State.Alpha = 0;
		State.DistanceTextAlpha = 0;
	}

	//Update Scale
	if (State.IsVisible)
	{
		float TargetScale = 1.0f;
		if (State.IsWaypoint)
		{
			TargetScale = m_scaleMax;
		}
		else if (State.IsFocused)
		{
			TargetScale = m_scaleMax + 0.1f;
		}
		else
		{
			const float tDist = ClampRange(State.PedDistance, m_scaleDistanceMin, m_scaleDistanceMax);
			TargetScale = Lerp(1.0f - tDist, m_scaleMin, m_scaleMax);
		}

		const float Step = fwTimer::GetTimeStep() / 0.3f;
		State.Scale = State.Scale + Clamp(TargetScale - State.Scale, -Step, Step);
	}

	return true;
}

bool CHudMarkerManager::UpdateMarkerFromBlip(s32 BlipID, SHudMarkerState& State) const
{
	if (CMiniMap::IsBlipIdInUse(BlipID))
	{
		auto Blip = CMiniMap::GetBlip(BlipID);
		State.BlipID = BlipID;
		State.WorldPosition = CMiniMap::GetBlipPositionValue(Blip);

		eBLIP_COLOURS BlipColor = CMiniMap::GetBlipColourValue(Blip);
		if (BlipColor != BLIP_COLOUR_USE_COLOUR32)
		{
			State.Color = CMiniMap_Common::GetColourFromBlipSettings(BlipColor, CMiniMap::IsFlagSet(Blip, BLIP_FLAG_BRIGHTNESS));
		}
		else
		{
			State.Color = CMiniMap::GetBlipColour32Value(Blip);
		}
		
		State.Alpha = MIN(State.Alpha, (u8)CMiniMap::GetBlipAlphaValue(Blip));
		State.IconIndex = CMiniMap::GetBlipObjectNameId(Blip);

		return true;
	}
	return false;
}

void CHudMarkerManager::UpdateWaypoint()
{
	const bool bIsWaypointSelectionAllowed =	
		CPlayerInfo::IsHardAiming() && //Player must be aiming
		!CScriptHud::bUsingMissionCreator &&
		!CMiniMap::IsInGolfMap() &&
		!CMiniMap::GetInPrologue() &&
		!CMiniMap::GetBlockWaypoint() &&
		NetworkInterface::CanSetWaypoint(CGameWorld::FindLocalPlayer());

	float MinFocusDistance = m_focusRadius;
	HudMarkerID BestFocusMarker;

	sWaypointStruct ActiveWaypoint;
	CMiniMap::GetActiveWaypoint(ActiveWaypoint);
	HudMarkerID AssociatedWaypointMarker;

	const u32 TimeMS = sysTimer::GetSystemMsTime();
	const rage::InputType SelectInput = INPUT_HUDMARKER_SELECT;
	const ioValue& InputValue = CControlMgr::GetMainPlayerControl().GetValue(SelectInput);

	//Loop active markers
	for (int i = m_activeIds.GetCount() - 1; i >= 0; --i)
	{
		const auto& ID = m_activeIds[i];
		auto& MarkerState = m_markerStates[ID];

		if (MarkerState.IsPendingRemoval)
			continue;

		//Determine associated waypoint status (there can only be one)
		MarkerState.IsWaypoint = false;
		if (ActiveWaypoint.iBlipIndex != INVALID_BLIP_ID && !AssociatedWaypointMarker.IsValid())
		{
			if (ActiveWaypoint.iAssociatedBlipId == MarkerState.BlipID || //Is blip associated?
				(ActiveWaypoint.vPos - Vector2(MarkerState.WorldPosition.x, MarkerState.WorldPosition.y)).Mag2() < 0.1f) //Is blip very close?
			{
				AssociatedWaypointMarker = ID;
				MarkerState.IsWaypoint = true;
			}
		}

		//Determine best focus marker
		if (bIsWaypointSelectionAllowed && !MarkerState.IsWaypoint && MarkerState.IsVisible && !MarkerState.IsClamped)
		{
			static const Vector2 CenterOffset(0.5f, 0.5f);
			Vector2 CenterAlignedPos = MarkerState.ScreenPosition - CenterOffset;
			CenterAlignedPos.x *= m_cachedAspectRatio;
			const float DistanceFromCenter = CenterAlignedPos.Mag();
			if (DistanceFromCenter < MinFocusDistance)
			{
				MinFocusDistance = DistanceFromCenter;
				BestFocusMarker = ID;
			}
		}
	}

	//Update active waypoint marker
	m_activeWaypointMarker.Invalidate();
	if (AssociatedWaypointMarker.IsValid())
	{
		m_activeWaypointMarker = AssociatedWaypointMarker;
	}
	else if (ActiveWaypoint.iBlipIndex != INVALID_BLIP_ID)
	{
		//No associated marker was found so create a transient one
		if (!m_transientWaypointMarker.IsValid())
		{
			SHudMarkerState NewMarkerState;
			NewMarkerState.IsWaypoint = true;
			NewMarkerState.ClampEnabled = true;
			NewMarkerState.PedCullDistance = FLT_MAX;
			NewMarkerState.WorldHeightOffset = 1.0f; //Raise it off the ground
			m_transientWaypointMarker = CreateMarker(NewMarkerState);
		}
		if (auto pWaypointMarkerState = GetMarkerState(m_transientWaypointMarker))
		{
			if (pWaypointMarkerState->BlipID != ActiveWaypoint.iBlipIndex)
			{
				UpdateMarkerFromBlip(ActiveWaypoint.iBlipIndex, *pWaypointMarkerState);
				pWaypointMarkerState->IconIndex = -1;
			}
		}
		m_activeWaypointMarker = m_transientWaypointMarker;
	}

	//Cleanup transient marker if it's no longer needed
	if (m_transientWaypointMarker != m_activeWaypointMarker)
	{
		RemoveMarker(m_transientWaypointMarker);
	}

	//Update Selection Focus
	if (m_focusedMarker != BestFocusMarker)
	{
		if (!bIsWaypointSelectionAllowed || BestFocusMarker.IsValid() || TimeMS > m_lastFocusTime + (int)(g_HudMarkerFocusClearGracePeriod * 1000.0f))
		{
			if (auto MarkerState = GetMarkerState(m_focusedMarker))
			{
				MarkerState->IsFocused = false;
			}
			if (auto MarkerState = GetMarkerState(BestFocusMarker))
			{
				MarkerState->IsFocused = true;
			}
			m_focusedMarker = BestFocusMarker;
			m_lastFocusTime = TimeMS;
		}
	}
	else if (m_focusedMarker.IsValid())
	{
		m_lastFocusTime = TimeMS;
	}

	//When button is pressed, start pending focus selection
	if (InputValue.IsPressed() && bIsWaypointSelectionAllowed)
	{
		m_pendingFocusSelection.Invalidate();
		m_pendingFocusInput = true;
		m_focusInputPressedTime = sysTimer::GetSystemMsTime();
	}

	//Update pending focus selection if valid
	if (m_pendingFocusInput && m_focusedMarker.IsValid())
	{
		m_pendingFocusSelection = m_focusedMarker;
	}

	//When the button is released and input is in a valid state (not exclusive to another control)
	if (m_pendingFocusInput && InputValue.IsEnabled() && InputValue.IsUp())
	{
		//And input was released within 150ms (so we don't trigger during "look-behind")
		if (sysTimer::GetSystemMsTime() < m_focusInputPressedTime + 150)
		{
			//Set waypoint to focus (if any)
			if (IsMarkerValid(m_pendingFocusSelection))
			{
				const auto& MarkerState = m_markerStates[m_pendingFocusSelection];
				ObjectId objectId = 0;
				if (CMiniMap::IsBlipIdInUse(MarkerState.BlipID))
				{
					if (auto pBlip = CMiniMap::GetBlip(MarkerState.BlipID))
					{
						if (netObject* pNetObj = NetworkUtils::GetNetworkObjectFromEntity(CMiniMap::FindBlipEntity(pBlip)))
						{
							objectId = pNetObj->GetObjectID();
						}
					}
				}
				CMiniMap::SwitchOnWaypoint(MarkerState.WorldPosition.x, MarkerState.WorldPosition.y, objectId);
			}
			//Or clear waypoint
			else
			{
				CMiniMap::SwitchOffWaypoint();
			}
		}
		m_pendingFocusInput = false;
		m_pendingFocusSelection.Invalidate();
	}
}

fwRect CHudMarkerManager::CalculateClampBoundary() const
{
	const bool ConsiderSuperWidescreen = true;
	float x0, x1, y0, y1;
	CHudTools::GetMinSafeZone(x0, y0, x1, y1, m_cachedAspectRatio, ConsiderSuperWidescreen);

	const float AspectMultiplier = MIN(1.0f, m_cachedAspectMultiplier);
	const float Width = (x1 - x0) / AspectMultiplier;
	const float Height = y1 - y0;

	fwRect ClampBoundary(x0, y1, x1, y0);
	ClampBoundary.left += m_clampInsetX * Width;
	ClampBoundary.right -= m_clampInsetX * Width;
	ClampBoundary.top += m_clampInsetY * Height;
	ClampBoundary.bottom -= m_clampInsetY * Height;

	return ClampBoundary;
}

fwRect CHudMarkerManager::CalculateMinimapArea() const
{
	const bool ConsiderSuperWidescreen = true;
	float x0, x1, y0, y1;
	CHudTools::GetMinSafeZone(x0, y0, x1, y1, m_cachedAspectRatio, ConsiderSuperWidescreen);

	fwRect Area = CMiniMap::IsInBigMap() ? m_minimapAreaBig : m_minimapArea;

	//Add safe zone offset (assuming bottom left aligned)
	Area.left += x0;
	Area.right += x0;
	Area.top -= 1.0f - y1;
	Area.bottom -= 1.0f - y1;

	return Area;
}

//Projects State.WorldPosition to HUD(0-1), clamping to boundary if required
//Results written into State
void CHudMarkerManager::ProjectMarker(SHudMarkerState& State, bool ShouldClamp, const fwRect& ClampBoundary, const grcViewport& Viewport) const
{
	State.IsVisible = false;
	State.IsClamped = false;

	//Project
	const Vec4V WorldPosition(State.WorldPosition.x, State.WorldPosition.y, State.WorldPosition.z + State.WorldHeightOffset, 1.0f);
	const Vec4V ProjectedPosition = Multiply(Viewport.GetFullCompositeMtx(), WorldPosition);
	if (IsZeroAll(ProjectedPosition.GetW()))
	{
		return;
	}

	const bool IsBehindCamera = ProjectedPosition.GetWf() < 0;

	//Perspective divide
	State.ScreenPosition.Set(ProjectedPosition.GetXf() / ProjectedPosition.GetWf(), ProjectedPosition.GetYf() / ProjectedPosition.GetWf());

	//Convert to 0-1 space (Note: origin is top left)
	State.ScreenPosition.x /= Viewport.GetWidth();
	State.ScreenPosition.y /= Viewport.GetHeight();

	//Coords are inverted in X behind the camera, so flip them back
	if (IsBehindCamera)
	{
		State.ScreenPosition.x = 1.0f - State.ScreenPosition.x;
		State.ScreenPosition.y = 1.0f - State.ScreenPosition.y;
	}

	Vector2 ClampCenter; ClampBoundary.GetCentre(ClampCenter.x, ClampCenter.y);

	//Do clamp if requested
	if (ShouldClamp)
	{
		State.IsVisible = true;

		Vector2 ClampedPosition = State.ScreenPosition;

		if (m_clampMode == ClampMode_None)
		{
			//do nothing
		}
		else if (m_clampMode == ClampMode_Radial)
		{
			Vector2 HalfSize(ClampBoundary.GetWidth() * 0.5f, abs(ClampBoundary.GetHeight()) * 0.5f);

			//Apply proximity inset
			float ProximityInset = 1.0f - ClampRange(State.PedDistance, m_clampCloseProximityMinDistance, m_clampCloseProximityMaxDistance);
			if (ProximityInset > 0.0f)
			{
				ProximityInset *= m_clampCloseProximityInset;
				HalfSize.x -= ProximityInset / m_cachedAspectRatio;
				HalfSize.y -= ProximityInset;
			}

			//Frame position for unit circle about "ClampCenter"
			ClampedPosition.x = (ClampedPosition.x - ClampCenter.x) / HalfSize.x;
			ClampedPosition.y = (ClampedPosition.y - ClampCenter.y) / HalfSize.y;

			//Clamp to unit circle (forcefully if behind the camera)
			const float Mag2 = ClampedPosition.Mag2();
			if (Mag2 > 1.0f || IsBehindCamera)
			{
				ClampedPosition.InvScale(Sqrtf(Mag2));
				State.IsClamped = true;
			}

			//Transform back
			ClampedPosition = ClampCenter + (ClampedPosition * HalfSize);
		}
		else if (m_clampMode == ClampMode_Rectangular)
		{
			//Force-clamp markers behind the camera
			if (IsBehindCamera)
			{
				ClampedPosition -= ClampCenter;
				ClampedPosition.NormalizeSafe();
				ClampedPosition += ClampCenter;
			}
			//Find intersection with boundary
			atFixedArray<Vector2, 2> Intersections;
			if (CHudTools::LineRectIntersection(ClampCenter, ClampedPosition, ClampBoundary, Intersections))
			{
				ClampedPosition = Intersections[0];
				State.IsClamped = true;
			}
		}

		//Update rotation (before position)
		Vector2 Direction = ClampedPosition - ClampCenter;
		State.ScreenRotation = atan2(Direction.x, -Direction.y);

		//Update position
		State.ScreenPosition = ClampedPosition;
	}
	else //Unclamped
	{
		//Not visible if behind the camera
		State.IsVisible = !IsBehindCamera;
	}

	if (State.IsVisible)
	{
		//Not visible if off screen
		const float HalfSize = m_markerScreenHalfSize * State.Scale;
		const float MinScreenLocX = -HalfSize / m_cachedAspectRatio;
		const float MaxScreenLocX = 1.0f + (HalfSize / m_cachedAspectRatio);
		const float MinScreenLocY = -HalfSize;
		const float MaxScreenLocY = 1.0f + (HalfSize);
		State.IsVisible =	State.ScreenPosition.x >= MinScreenLocX && State.ScreenPosition.x <= MaxScreenLocX &&
							State.ScreenPosition.y >= MinScreenLocY && State.ScreenPosition.y <= MaxScreenLocY;

		//Minimap overlap cull
		if (State.ScreenPosition.x < m_cachedMinimapArea.right && State.ScreenPosition.y > m_cachedMinimapArea.top)
		{
			State.IsVisible = false;
		}
	}
}

//START_DEBUG===========================================
#if __BANK
void CHudMarkerManager::CreateBankWidgets()
{
	static bool bBankCreated = false;
	bkBank *pBank = BANKMGR.FindBank(UI_DEBUG_BANK_NAME);

	if (!uiVerify(pBank) || bBankCreated)
	{
		return;
	}

	bBankCreated = true;
	
	pBank->PushGroup("HUD Markers");
	{
		pBank->AddText("Num Active", &m_numActive, true);
		
		pBank->PushGroup("Clamping");
		{
			static const char* ClampModeOptions[ClampMode_NUM] = { "None", "Radial", "Rectangular" };
			pBank->AddCombo("Clamp Mode", &m_clampMode, ClampMode_NUM, ClampModeOptions);

			pBank->AddToggle("Draw Clamp Boundary", &m_renderClampBoundary);
			pBank->AddSlider("Clamp Inset X", &m_clampInsetX, 0.0f, 1.0f, 0.01f);
			pBank->AddSlider("Clamp Inset Y", &m_clampInsetY, 0.0f, 1.0f, 0.01f);

			pBank->AddSlider("Clamp Close Proximity Min Distance", &m_clampCloseProximityMinDistance, 0.0f, 10000.0f, 1.0f);
			pBank->AddSlider("Clamp Close Proximity Max Distance", &m_clampCloseProximityMaxDistance, 0.0f, 10000.0f, 1.0f);
			pBank->AddSlider("Clamp Close Proximity Inset", &m_clampCloseProximityInset, 0.0f, 1.0f, 0.01f);
			
			pBank->AddSeparator();

			pBank->AddToggle("Hide Distance Text When Clamped", &m_hideDistanceTextWhenClamped);
		}
		pBank->PopGroup();
		pBank->PushGroup("Alpha/Scale");
		{
			pBank->AddSlider("Marker Screen Half Size", &m_markerScreenHalfSize, 0.0f, 1.0f, 0.01f);
			pBank->AddSlider("Scale Min", &m_scaleMin, 0.0f, 2.0f, 0.01f);
			pBank->AddSlider("Scale Max", &m_scaleMax, 0.0f, 2.0f, 0.01f);
			pBank->AddSlider("Scale Distance Min", &m_scaleDistanceMin, 0.0f, 10000.0f, 1.0f);
			pBank->AddSlider("Scale Distance Max", &m_scaleDistanceMax, 0.0f, 10000.0f, 1.0f);
			pBank->AddSlider("Fade Over Distance Alpha", &m_fadeOverDistanceAlpha, 0, 255, 0);
			pBank->AddSlider("Fade Over Distance Min", &m_fadeOverDistanceMin, 0.0f, 10000.0f, 1.0f);
			pBank->AddSlider("Fade Over Distance Max", &m_fadeOverDistanceMax, 0.0f, 10000.0f, 1.0f);
			pBank->AddSlider("Occluded Alpha", &m_occludedAlpha, 0, 255, 0);
			pBank->AddSlider("Occlusion Frame Skip", &m_occlusionTestFrameSkip, 0, 30, 1);
		}
		pBank->PopGroup();
		pBank->PushGroup("Minimap Overlap Culling");
		{
			pBank->AddToggle("Render Debug", &m_renderMinimapArea);
			pBank->AddSeparator();
			pBank->AddTitle("Minimap Area");
			pBank->AddSlider("Left", &m_minimapArea.left, 0.0f, 1.0f, 0.01f);
			pBank->AddSlider("Right", &m_minimapArea.right, 0.0f, 1.0f, 0.01f);
			pBank->AddSlider("Top", &m_minimapArea.top, 0.0f, 1.0f, 0.01f);
			pBank->AddSlider("Bottom", &m_minimapArea.bottom, 0.0f, 1.0f, 0.01f);
			pBank->AddSeparator();
			pBank->AddTitle("Minimap Area (Big)");
			pBank->AddSlider("Left", &m_minimapAreaBig.left, 0.0f, 1.0f, 0.01f);
			pBank->AddSlider("Right", &m_minimapAreaBig.right, 0.0f, 1.0f, 0.01f);
			pBank->AddSlider("Top", &m_minimapAreaBig.top, 0.0f, 1.0f, 0.01f);
			pBank->AddSlider("Bottom", &m_minimapAreaBig.bottom, 0.0f, 1.0f, 0.01f);
		}
		pBank->PopGroup();
		pBank->PushGroup("Groups", false);
		{
			for (HudMarkerGroupID ID = HudMarkerGroupID::Min(); ID.IsValid(); ++ID)
			{
				char GroupName[64];
				formatf(GroupName, "Group_%d", ID.Get(), NELEM(GroupName));
				pBank->PushGroup(GroupName, false);
				{
					auto& Group = m_markerGroups[ID];

					pBank->AddText("Num Visible", &Group.NumVisible, true);
					pBank->AddText("Num Clamped", &Group.NumClamped, true);

					pBank->AddSeparator();

					pBank->AddSlider("Max Clamped", &Group.MaxClampNum, 0, HudMarkerID::Size(), 1);
					pBank->AddSlider("Force Visible", &Group.ForceVisibleNum, 0, HudMarkerID::Size(), 1);
					pBank->AddSlider("Max Visible", &Group.MaxVisible, 0, HudMarkerID::Size(), 1);
					pBank->AddSlider("Min Distance", &Group.MinDistance, 0.0f, 10000.0f, 1.0f);
					pBank->AddSlider("Max Distance", &Group.MaxDistance, 0.0f, 10000.0f, 1.0f);
					pBank->AddSlider("Min Text Distance", &Group.MinTextDistance, 0.0f, 10000.0f, 1.0f);		
					//pBank->AddToggle("Solo", &Group.IsSolo);
					//pBank->AddToggle("Muted", &Group.IsMuted);
				}
				pBank->PopGroup();
			}
		}
		pBank->PopGroup();

		pBank->PushGroup("Picking");
		{
			pBank->AddToggle("Render Debug", &m_renderFocusRadius);
			pBank->AddSlider("Focus Radius", &m_focusRadius, 0.0f, 1.0f, 0.01f);
			pBank->AddSlider("Focus Clear Grace Period", &g_HudMarkerFocusClearGracePeriod, 0.0f, 1.0f, 0.01f);
		}
		pBank->PopGroup();

		if (m_renderer)
		{
			m_renderer->CreateBankWidgets(pBank);
		}

		pBank->PushGroup("Test");
		{
			pBank->AddToggle("Draw Debug Markers", &m_renderDebugMarkers);
			pBank->AddToggle("Draw Debug Text", &m_renderDebugText);
			pBank->AddText("Num Debug", &m_numDebug, true);
			pBank->AddButton("Push Marker", datCallback(MFA(CHudMarkerManager::PushDebugMarker), (datBase*)this));
			pBank->AddButton("Pop Marker", datCallback(MFA(CHudMarkerManager::PopDebugMarker), (datBase*)this));
			pBank->AddSlider("Push/Pop Num", &m_pushPopNum, 1, HudMarkerID::Size(), 1);
			pBank->AddSlider("Push Group", &m_pushGroup, HudMarkerGroupID::Min(), HudMarkerGroupID::Max(), 1);
			pBank->AddSlider("Push Range", &m_pushRange, 1.0f, 500.0f, 1);
		}
		pBank->PopGroup();
	}
	pBank->PopGroup();
}

void CHudMarkerManager::PushDebugMarker()
{
	CPed* FollowPlayer = CGameWorld::FindFollowPlayer();
	if (FollowPlayer == nullptr)
	{
		uiAssertf(0, "CHudMarkerManager::CreateRandomMarker - failed to resolve FollowPlayer");
		return;
	}
	for (int i = 0; i < m_pushPopNum; i++)
	{
		if (!m_debugMarkers.IsFull())
		{
			SHudMarkerState NewState;
			const Vector3 RandOffset(fwRandom::GetRandomNumberInRange(-m_pushRange, m_pushRange), fwRandom::GetRandomNumberInRange(-m_pushRange, m_pushRange), fwRandom::GetRandomNumberInRange(0.0f, 50.0f));
			NewState.WorldPosition = FollowPlayer->GetPreviousPosition() + RandOffset;
			NewState.Color = Color32(fwRandom::GetRandomNumberInRange(0, 255), fwRandom::GetRandomNumberInRange(0, 255), fwRandom::GetRandomNumberInRange(0, 255), 255);
			NewState.IconIndex = fwRandom::GetRandomNumberInRange(0, eBlipSprite_MAX_VALUE);
			NewState.GroupID = HudMarkerGroupID::IsValid(m_pushGroup) ? m_pushGroup : HudMarkerGroupID::Rand();
			NewState.ClampEnabled = (rand() % 2) != 0;
			auto ID = CreateMarker(NewState);
			if (ID.IsValid())
			{
				m_debugMarkers.Push(ID);
			}
			else
			{
				break;
			}
		}
		else
		{
			break;
		}
	}
}

void CHudMarkerManager::PopDebugMarker()
{
	for (int i = 0; i < m_pushPopNum; i++)
	{
		if (!m_debugMarkers.IsEmpty())
		{
			auto ID = m_debugMarkers.Pop();
			RemoveMarker(ID);
		}
		else
		{
			break;
		}
	}
}

void CHudMarkerManager::RenderDebug(const fwRect& BoundaryRect)
{
	PF_AUTO_PUSH_TIMEBAR("CHudMarkerRenderer::RenderDebugMarkersUT()");

	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		sfAssertf(0, "CHudMarkerRenderer::RenderDebugUT can only be called on the UpdateThread!");
		return;
	}

	//Sort: Camera distance, descending
	std::stable_sort(m_activeIds.begin(), m_activeIds.end(), [this](HudMarkerID A, HudMarkerID B)
	{
		return m_markerStates[A].CamDistanceSq > m_markerStates[B].CamDistanceSq;
	});

	const Vector2 MarkerHalfSize(m_markerScreenHalfSize / m_cachedAspectRatio, m_markerScreenHalfSize);
	const Vector2 DebugHalfSize(0.01f / m_cachedAspectRatio, 0.01f);
	const Vector2 DebugOutlineHalfSize = DebugHalfSize * 1.3f;
	for (int i = 0; i < m_activeIds.GetCount(); ++i)
	{
		const auto& ID = m_activeIds[i];
		const auto& State = m_markerStates[ID];

		if (State.IsVisible)
		{
			if (m_renderDebugMarkers)
			{
				grcDebugDraw::RectAxisAligned(State.ScreenPosition - MarkerHalfSize * State.Scale, State.ScreenPosition + MarkerHalfSize * State.Scale, Color32(255, 255, 255, 64));

				const Color32 OutlineColor(State.IsClamped ? Color32(255, 0, 0) : (State.IsFocused ? Color32(255, 255, 255) : Color32(0, 0, 0)));
				grcDebugDraw::RectAxisAligned(State.ScreenPosition - DebugOutlineHalfSize * State.Scale, State.ScreenPosition + DebugOutlineHalfSize * State.Scale, OutlineColor);
				grcDebugDraw::RectAxisAligned(State.ScreenPosition - DebugHalfSize * State.Scale, State.ScreenPosition + DebugHalfSize * State.Scale, State.Color);
			}

			if (m_renderDebugText)
			{
				char DebugText[128];
				formatf(DebugText, "ID=%d\ndepth=%d\ngroup=%d\nclamp=%s\nped=%.1f\ncam=%.1f", (int)State.ID, i, (int)State.GroupID, State.ClampEnabled ? "true" : "false", State.PedDistance, sqrt(State.CamDistanceSq), NELEM(DebugText));  // unique instance name based on the unique id
				grcDebugDraw::TextFontPush(grcSetup::GetFixedWidthFont());
				grcDebugDraw::Text(State.ScreenPosition, DD_ePCS_NormalisedZeroToOne, Color32(225, 225, 225), DebugText);
				grcDebugDraw::TextFontPop();
			}
		}
	}
	if (m_renderClampBoundary)
	{
		Vector2 Min(BoundaryRect.left, BoundaryRect.top);
		Vector2 Max(BoundaryRect.right, BoundaryRect.bottom);
		grcDebugDraw::RectAxisAligned(Min, Max, Color32(255, 255, 255, 64));
	}
	if (m_renderMinimapArea)
	{
		Vector2 Min(m_cachedMinimapArea.left, m_cachedMinimapArea.top);
		Vector2 Max(m_cachedMinimapArea.right, m_cachedMinimapArea.bottom);
		grcDebugDraw::RectAxisAligned(Min, Max, Color32(255, 0, 255, 64));
	}
	if (m_renderFocusRadius)
	{
		Vector2 Min(m_cachedMinimapArea.left, m_cachedMinimapArea.top);
		Vector2 Max(m_cachedMinimapArea.right, m_cachedMinimapArea.bottom);
		grcDebugDraw::Circle(Vector2(0.5f, 0.5f), m_focusRadius, Color32(255, 0, 255, 64));
	}
}
#endif //__BANK
//END_DEBUG===========================================