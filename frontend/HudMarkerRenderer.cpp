/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : HudMarkerManager.cpp
// PURPOSE : Manages 2D world markers (instancing, layout, etc)
// AUTHOR  : Aaron Baumbach (Ruffian)
// STARTED : 06/02/2020
//
/////////////////////////////////////////////////////////////////////////////////

#include "HudMarkerRenderer.h"

#include "frontend/HudTools.h"
#include "frontend/ui_channel.h"

#include "fwsys/gameskeleton.h"
#include "profile/timebars.h"
#include "vfx/VfxHelper.h"

#include "frontend/FrontendStatsMgr.h"

#include "frontend/MiniMapCommon.h"

FRONTEND_OPTIMISATIONS()

#define HUD_MARKER_MOVIE "HUD_MARKER"
#define HUD_MARKER_CLIP "HUD_MARKER_CLIP"

#define HUD_MARKER_ICON_NAME "icon"
#define HUD_MARKER_ICON_DEPTH 10

#define HUD_MARKER_WAYPOINT_ICON_NAME "waypoint_icon"
#define HUD_MARKER_WAYPOINT_ICON_DEPTH 11

CHudMarkerRenderer::CHudMarkerRenderer()
	: m_maxNewMarkersPerFrame(10)
	, m_maxVisibleMarkers(HudMarkerID::Size())
	, m_renderScaleformMarkers(true)
	, m_distanceMode(DistanceMode_Default)
#if __BANK
	, m_numVisible(0)
	, m_hideArrows(false)
	, m_refreshMarkers(false)
	, m_debugDistanceMode(DistanceMode_Default)
#endif
{
	uiDisplayf("CHudMarkerRenderer - alloc[%d]", (int)sizeof(*this));
}

void CHudMarkerRenderer::Init(unsigned Mode)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		uiAssertf(0, "CHudMarkerRenderer::Init can only be called on the UpdateThread!");
		return;
	}

	if (Mode == INIT_SESSION)
	{
		uiDisplayf("CHudMarkerRenderer - Init");

		m_markerRenderStates.Reset();
		m_activeIds.Reset();
		m_asMarkers.Reset();

		//Initialise with defaults
		for (int i = 0; i < m_markerRenderStates.GetMaxCount(); i++)
		{
			m_markerRenderStates.Push(SHudMarkerRenderState());
		}
		for (int i = 0; i < m_asMarkers.GetMaxCount(); i++)
		{
			m_asMarkers.Push(GFxValue());
		}
	}
}

void CHudMarkerRenderer::Shutdown(unsigned Mode)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		uiAssertf(0, "CHudMarkerRenderer::Shutdown can only be called on the UpdateThread!");
		return;
	}

	if (Mode == SHUTDOWN_SESSION)
	{
		uiDisplayf("CHudMarkerRenderer - Shutdown");

		ShutdownScaleformUT();
		m_markerRenderStates.Reset();
		m_activeIds.Reset();
		m_asMarkers.Reset();
	}
}

bool CHudMarkerRenderer::IsInitialised() const
{
	return m_markerRenderStates.GetCount() > 0;
}

void CHudMarkerRenderer::DeviceReset()
{
	ShutdownScaleformUT();
	InvalidateAllMarkers();
}

void CHudMarkerRenderer::InvalidateAllMarkers()
{
	uiDisplayf("CHudMarkerRenderer - InvalidateAllMarkers");
	for (auto& State : m_markerRenderStates)
	{
		State.Invalidate();
	}
}

void CHudMarkerRenderer::UpdateMovieConfig()
{
	if (m_movie.IsActive())
	{
		CScaleformMgr::UpdateMovieParams(m_movie.GetMovieID(), CScaleformMgr::GetRequiredScaleMode(m_movie.GetMovieID(), true));
	}
}

void CHudMarkerRenderer::AddDrawListCommands(const HudMarkerIDArray& ActiveIDs, HudMarkerStateArray& StateUT)
{
	PF_AUTO_PUSH_TIMEBAR("CHudMarkerRenderer::AddDrawListCommands()");

	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		uiAssertf(0, "CHudMarkerRenderer::AddDrawListCommands can only be called on the UpdateThread!");
		return;
	}

	if (!m_renderScaleformMarkers || (ActiveIDs.IsEmpty() && m_activeIds.IsEmpty()))
	{
		ShutdownScaleformUT();
	}
	else if (InitScaleformUT())
	{
		//Calling SyncState() during AddDrawListCommands() allows us to avoid race conditions as we can guarentee last frame's DLC has been processed
		//Normally this would be done during RenderThread Synchronise
		//Unfortunately CHudMarkerManager depends on CMinimap's update which also occurs during the BuildDrawList step...
		SyncStateUT(ActiveIDs, StateUT);

		DLC_Delegate(void(void), (this, &CHudMarkerRenderer::Render));
	}
}

bool CHudMarkerRenderer::InitScaleformUT()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		uiAssertf(0, "CHudMarkerRenderer::SyncState can only be called on the UpdateThread!");
		return false;
	}
	if (!m_movie.IsActive())
	{
		uiDisplayf("CHudMarkerRenderer - InitScaleformUT - Creating Movie");
		m_movie.CreateMovie(SF_BASE_CLASS_HUD, HUD_MARKER_MOVIE);
	}
	else if (!m_asMarkerContainer.IsDefined())
	{
		uiDisplayf("CHudMarkerRenderer - InitScaleformUT - Creating Marker Container");
		CScaleformMgr::AutoLock lock(m_movie.GetMovieID());
		GFxValue asMovieObject = CScaleformMgr::GetActionScriptObjectFromRoot(m_movie.GetMovieID());
		if (uiVerify(asMovieObject.IsDefined()))
		{
			asMovieObject.CreateEmptyMovieClip(&m_asMarkerContainer, "asMarkerContainer", 1);
			uiAssert(m_asMarkerContainer.IsDefined());
		}
	}
	else //Init done
	{
		return true;
	}
	return false;
}

void CHudMarkerRenderer::ShutdownScaleformUT()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		uiAssertf(0, "CHudMarkerRenderer::SyncState can only be called on the UpdateThread!");
		return;
	}

	if (m_movie.IsActive())
	{
		uiDisplayf("CHudMarkerRenderer - ShutdownScaleformUT - Removing Movie");

		{//autolock scope
			CScaleformMgr::AutoLock autoLock(m_movie.GetMovieID());
			for (int i = 0; i < m_asMarkers.GetCount(); ++i)
			{
				if (m_asMarkers[i].IsDefined())
				{
					m_asMarkers[i].Invoke("removeMovieClip");
					m_asMarkers[i].SetUndefined();
				}
			}
			if (m_asMarkerContainer.IsDefined())
			{
				m_asMarkerContainer.Invoke("removeMovieClip");
				m_asMarkerContainer.SetUndefined();
			}
		}
		m_movie.RemoveMovie();

#if __BANK
		m_numVisible = 0;
#endif
	}
}

void CHudMarkerRenderer::SyncStateUT(const HudMarkerIDArray& ActiveIDs, HudMarkerStateArray& StateUT)
{
	PF_AUTO_PUSH_TIMEBAR("CHudMarkerRenderer::SyncState()");

	if (!CSystem::IsThisThreadId(SYS_THREAD_UPDATE))  // only on UT
	{
		uiAssertf(0, "CHudMarkerRenderer::SyncState can only be called on the UpdateThread!");
		return;
	}

	bool InvalidateMarkersThisFrame = false;
	if (UpdateDistanceMode())
	{
		InvalidateMarkersThisFrame = true;
	}

	const float HalfPixelWidth = 0.5f / CHudTools::GetUIWidth();
	const float HalfPixelHeight = 0.5f / CHudTools::GetUIHeight();

#define SET_WITH_DIRTY(VAR, FLAG) if (RenderState.VAR != State.VAR)	{  \
			RenderState.VAR = State.VAR; \
			RenderState.DirtyFlags.SetFlag(SHudMarkerRenderState::FLAG); \
		}

#define SET_WITH_DIRTY_FLOAT(VAR, THRESHOLD, FLAG) if (!IsClose(RenderState.VAR, State.VAR, THRESHOLD))	{  \
			RenderState.VAR = State.VAR; \
			RenderState.DirtyFlags.SetFlag(SHudMarkerRenderState::FLAG); \
		}

	//Update RT state from UT state
	m_activeIds = ActiveIDs;
	for (int i = 0; i < m_activeIds.GetCount(); ++i)
	{
		const auto& ID = m_activeIds[i];
		auto& State = StateUT[ID];
		auto& RenderState = m_markerRenderStates[ID];

		RenderState.IsPendingRemoval = State.IsPendingRemoval;

		State.HasRenderableShutdown = RenderState.IsPendingRemoval && !m_asMarkers[ID].IsDefined();

		SET_WITH_DIRTY(IsVisible, Dirty_IsVisible);

		if (State.IsVisible)
		{
			SET_WITH_DIRTY_FLOAT(ScreenPosition.x, HalfPixelWidth, Dirty_ScreenTransform);
			SET_WITH_DIRTY_FLOAT(ScreenPosition.y, HalfPixelHeight, Dirty_ScreenTransform);
			SET_WITH_DIRTY_FLOAT(ScreenRotation, PI/180.0f, Dirty_ScreenTransform);
			SET_WITH_DIRTY_FLOAT(Scale, 0.001f, Dirty_ScreenTransform);
			SET_WITH_DIRTY(Color, Dirty_Color);
			SET_WITH_DIRTY(Alpha, Dirty_Alpha);
			SET_WITH_DIRTY(DistanceTextAlpha, Dirty_DistanceTextAlpha);
			SET_WITH_DIRTY(IconIndex, Dirty_Icon);
			SET_WITH_DIRTY(IsClamped, Dirty_IsClamped);
			SET_WITH_DIRTY(IsFocused, Dirty_IsFocused);
			SET_WITH_DIRTY(IsWaypoint, Dirty_IsWaypoint);

			RenderState.CamDistanceSq = State.CamDistanceSq;

			u16 PedDistance = (u16)State.PedDistance;
			if (RenderState.PedDistance != PedDistance)
			{
				RenderState.PedDistance = PedDistance;
				RenderState.DirtyFlags.SetFlag(SHudMarkerRenderState::Dirty_PedDistance);
			}
		}
	}
#undef SET_WITH_DIRTY
#undef SET_WITH_DIRTY_FLOAT

	if (InvalidateMarkersThisFrame)
	{
		InvalidateAllMarkers();
	}
}

void CHudMarkerRenderer::Render()
{
	PF_AUTO_PUSH_TIMEBAR("CHudMarkerRenderer::Render()");

	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		uiAssertf(0, "CHudMarkerRenderer::Render can only be called on the RenderThread!");
		return;
	}

	UpdateRT();

	m_movie.Render();
}

void CHudMarkerRenderer::UpdateRT()
{
	PF_AUTO_PUSH_TIMEBAR("CHudMarkerRenderer::UpdateRT()");

	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		uiAssertf(0, "CHudMarkerRenderer::UpdateRT can only be called on the RenderThread!");
		return;
	}

	//Sort: Camera distance, descending (back to front)
	std::stable_sort(m_activeIds.begin(), m_activeIds.end(), [this](const HudMarkerID& A, const HudMarkerID& B)
	{
		return	(m_markerRenderStates[A].CamDistanceSq > m_markerRenderStates[B].CamDistanceSq) ||
				(m_markerRenderStates[B].IsFocused) ||
				(m_markerRenderStates[B].IsWaypoint && !m_markerRenderStates[A].IsFocused);
	});

	const float AspectRatioModifier = CHudTools::GetAspectRatioMultiplier();

	char DistanceText[8];
	int NumNewMarkers = 0;
	int NumVisible = 0;

	for (int i = m_activeIds.GetCount() - 1; i >= 0; --i) //Iterate front to back so we can cull furthest
	{
		const auto& ID = m_activeIds[i];
		auto& State = m_markerRenderStates[ID];
		auto& gfxValue = m_asMarkers[ID];
		const u16 Depth = (u16)(i);

		GFxValue::DisplayInfo DisplayInfo;

		if (State.IsPendingRemoval || !State.IsVisible || NumVisible >= m_maxVisibleMarkers)
		{
			if (gfxValue.IsDefined()) //Destroy marker
			{
				gfxValue.Invoke("removeMovieClip");
				gfxValue.SetUndefined();
			}
		}
		else if (gfxValue.IsUndefined() && State.IsVisible)
		{
			if (NumNewMarkers < m_maxNewMarkersPerFrame)
			{
				char InstanceName[5];
				formatf(InstanceName, "%d", ID.Get(), NELEM(InstanceName));  // unique instance name based on the unique id

				//Create marker (depth above max so we don't stomp another clip)
				State.Depth = HudMarkerID::Max() + ID;
				if (m_asMarkerContainer.AttachMovie(&gfxValue, HUD_MARKER_CLIP, InstanceName, State.Depth))
				{
					//Invalid state to force flush to gfx value
					State.Invalidate();

					//Start invisible so we don't show anything before the object is fully created
					DisplayInfo.SetVisible(false);
					gfxValue.SetDisplayInfo(DisplayInfo);

					NumNewMarkers++;
				}
				else
				{
					uiAssertf(false, "Failed to attach movie. File[%s] Instance[%s] Depth[%d]", HUD_MARKER_CLIP, InstanceName, State.Depth);
				}
			}
		}
		else if (gfxValue.IsDefined() && gfxValue.IsDisplayObject() && gfxValue.IsDisplayObjectActive())
		{
#if __BANK
			if (m_refreshMarkers)
			{
				m_refreshMarkers = false;
				State.Invalidate();
			}
#endif

			//Update visibility
			if (State.DirtyFlags.IsFlagSet(SHudMarkerRenderState::Dirty_IsVisible))
			{
				State.DirtyFlags.ClearFlag(SHudMarkerRenderState::Dirty_IsVisible);
				DisplayInfo.SetVisible(State.IsVisible);
			}

			if (State.IsVisible BANK_ONLY(|| m_refreshMarkers))
			{
				NumVisible++;

				//Update depth
				if (State.Depth != Depth)
				{
					GFxValue args[1]; args[0].SetNumber(Depth);
					GFxValue result;
					if (gfxValue.Invoke("swapDepths", &result, args, 1))
					{
						State.Depth = Depth;
					}					
				}

				//Update transform
				if (State.DirtyFlags.IsFlagSet(SHudMarkerRenderState::Dirty_ScreenTransform))
				{
					State.DirtyFlags.ClearFlag(SHudMarkerRenderState::Dirty_ScreenTransform);

					Vector2 Position = State.ScreenPosition;

					//Correct for aspect ratio (scaleform works in 16:9)
					Position.x = 0.5f + ((Position.x - 0.5f) * AspectRatioModifier);

					DisplayInfo.SetPosition(Position.x * ACTIONSCRIPT_STAGE_SIZE_X, Position.y * ACTIONSCRIPT_STAGE_SIZE_Y);

					float ScalePercent = State.Scale * 100.0f;
					DisplayInfo.SetScale(ScalePercent, ScalePercent);

					GFxValue Arrow;
					if (uiVerify(gfxValue.GetMember("arrow", &Arrow)))
					{
						GFxValue::DisplayInfo ArrowDisplayInfo;
						ArrowDisplayInfo.SetRotation(RADIANS_TO_DEGREES(State.ScreenRotation));
						Arrow.SetDisplayInfo(ArrowDisplayInfo);
					}
				}

				//Update icon
				if (State.DirtyFlags.IsFlagSet(SHudMarkerRenderState::Dirty_Icon))
				{
					State.DirtyFlags.ClearFlag(SHudMarkerRenderState::Dirty_Icon);
					if (State.IconIndex >= 0)
					{
						const char* IconName = IconIndexToName(State.IconIndex);
						GFxValue Icon;
						if (uiVerify(gfxValue.AttachMovie(&Icon, IconName, HUD_MARKER_ICON_NAME, HUD_MARKER_ICON_DEPTH)))
						{
							Icon.SetColorTransform(State.Color);
						}
					}
				}

				//Update colour
				if (State.DirtyFlags.IsFlagSet(SHudMarkerRenderState::Dirty_Color))
				{
					State.DirtyFlags.ClearFlag(SHudMarkerRenderState::Dirty_Color);

					GFxValue Arrow;
					if (uiVerify(gfxValue.GetMember("arrow", &Arrow)))
					{
						Arrow.SetColorTransform(State.Color);
					}

					GFxValue Icon;
					if (uiVerify(gfxValue.GetMember("icon", &Icon)))
					{
						Icon.SetColorTransform(State.Color);
					}
				}

				//Update Alpha
				if (State.DirtyFlags.IsFlagSet(SHudMarkerRenderState::Dirty_Alpha))
				{
					State.DirtyFlags.ClearFlag(SHudMarkerRenderState::Dirty_Alpha);

					DisplayInfo.SetAlpha(State.Alpha * (100.0 / 255.0));
				}

				//Update distance text alpha
				if (State.DirtyFlags.IsFlagSet(SHudMarkerRenderState::Dirty_DistanceTextAlpha))
				{
					State.DirtyFlags.ClearFlag(SHudMarkerRenderState::Dirty_DistanceTextAlpha);

					GFxValue Distance;
					if (uiVerify(gfxValue.GetMember("distance", &Distance)))
					{
						GFxValue::DisplayInfo DisplayInfo;
						DisplayInfo.SetVisible(State.DistanceTextAlpha != 0);
						DisplayInfo.SetAlpha(State.DistanceTextAlpha * (100.0 / 255.0));
						Distance.SetDisplayInfo(DisplayInfo);
					}
				}

				//Update distance text
				if (State.DirtyFlags.IsFlagSet(SHudMarkerRenderState::Dirty_PedDistance) && State.DistanceTextAlpha > 0)
				{
					State.DirtyFlags.ClearFlag(SHudMarkerRenderState::Dirty_PedDistance);

					GFxValue Distance;
					if (uiVerify(gfxValue.GetMember("distance", &Distance)))
					{
						GFxValue DistanceTF;
						if (uiVerify(Distance.GetMember("distanceTF", &DistanceTF)))
						{
							if (DistanceToText(State.PedDistance, DistanceText))
							{
								DistanceTF.SetText(DistanceText);
							}
						}
					}
				}

				//Update IsClamped behaviour
				if (State.DirtyFlags.IsFlagSet(SHudMarkerRenderState::Dirty_IsClamped))
				{
					State.DirtyFlags.ClearFlag(SHudMarkerRenderState::Dirty_IsClamped);

					//Show arrow while we are clamped
					GFxValue Arrow;
					if (uiVerify(gfxValue.GetMember("arrow", &Arrow)))
					{
						GFxValue::DisplayInfo ArrowDisplayInfo;
						ArrowDisplayInfo.SetVisible(State.IsClamped BANK_ONLY(&& !m_hideArrows));
						Arrow.SetDisplayInfo(ArrowDisplayInfo);
					}
				}

				//Update IsWaypoint|IsFocused behaviour
				if (State.DirtyFlags.IsFlagSet(SHudMarkerRenderState::Dirty_IsWaypoint) || State.DirtyFlags.IsFlagSet(SHudMarkerRenderState::Dirty_IsFocused))
				{
					State.DirtyFlags.ClearFlag(SHudMarkerRenderState::Dirty_IsWaypoint);
					State.DirtyFlags.ClearFlag(SHudMarkerRenderState::Dirty_IsFocused);
					
					GFxValue Highlight;
					if (uiVerify(gfxValue.GetMember("HUD_MARKER_HIGHLIGHT", &Highlight)))
					{
						Highlight.GotoAndStop(State.IsWaypoint ? 3 : State.IsFocused ? 2 : 1);
						Highlight.SetColorTransform(CHudColour::GetRGBA(HUD_COLOUR_WAYPOINT));
					}
				}
			}

			gfxValue.SetDisplayInfo(DisplayInfo);
		}
	}

#if __BANK
	//Update debug 
	m_numVisible = NumVisible;
#endif
}

const char* CHudMarkerRenderer::IconIndexToName(s32 Index) const
{
	return CMiniMap_Common::GetBlipName(Index);
}

bool CHudMarkerRenderer::UpdateDistanceMode()
{
	auto DesiredDistanceMode = CFrontendStatsMgr::ShouldUseMetric() ? DistanceMode_Metric : DistanceMode_Imperial;

#if __BANK
	if (m_debugDistanceMode != DistanceMode_Default)
	{
		DesiredDistanceMode = (HudMarkerDistanceMode)m_debugDistanceMode;
	}
#endif

	if (m_distanceMode != DesiredDistanceMode)
	{
		m_distanceMode = DesiredDistanceMode;
		uiDisplayf("CHudMarkerRenderer::UpdateDistanceMode - m_distanceMode = %d", m_distanceMode);
		return true;
	}

	return false;
}

bool CHudMarkerRenderer::DistanceToText(u16 DistanceMeters, char(&out_Text)[8]) const
{
	switch (m_distanceMode)
	{
	case DistanceMode_Metric:
	case DistanceMode_Metric_MetresOnly:
	{
		if (DistanceMeters < 1000 || m_distanceMode == DistanceMode_Metric_MetresOnly)
		{
			//Max u16 char length = 5
			formatf(out_Text, "%dm", DistanceMeters, NELEM(out_Text));
		}
		else
		{
			//Max kilometers = u16/1000 = 65.535
			const float DistanceKilometers = (float)DistanceMeters / 1000.0f;
			//Clamping to 2 decimal places: max char length = 4
			formatf(out_Text, "%.2fkm", DistanceKilometers, NELEM(out_Text));
		}
		break;
	}
	case DistanceMode_Imperial:
	case DistanceMode_Imperial_FeetOnly:
	{
		//Mimicking style in MINIMAP.as - formatDistance()
		const float Miles = DistanceMeters * 0.0006213f;
		if (Miles < 0.1f || m_distanceMode == DistanceMode_Imperial_FeetOnly)
		{
			//Feet max = u16*3.2808399 = ~215,010 (6 characters)
			const u32 Feet = (u32)(DistanceMeters * 3.2808399);
			formatf(out_Text, "%dft", Feet, NELEM(out_Text));
		}
		else
		{
			//Max Miles = u16*0.0006213f = ~40.71
			//Clamping to 2 decimal places: max char length = 4
			formatf(out_Text, "%.2fmi", Miles, NELEM(out_Text));
		}
		break;
	}
	default:
		uiAssertf(false, "CHudMarkerRenderer::DistanceToText: Invalid distance mode %d", (int)m_distanceMode);
		return false;
	}
	return true;
}

#if __BANK
void CHudMarkerRenderer::CreateBankWidgets(bkBank *pBank)
{
	if (!uiVerify(pBank))
	{
		return;
	}

	pBank->PushGroup("Scaleform");
	{
		pBank->AddToggle("Render markers", &m_renderScaleformMarkers);
		pBank->AddText("Num visible", &m_numVisible, true);
		pBank->AddSlider("Max new markers per frame", &m_maxNewMarkersPerFrame, 1, HudMarkerID::Size(), 1);
		pBank->AddSlider("Max visible markers", &m_maxVisibleMarkers, 0, HudMarkerID::Size(), 1);
		pBank->AddToggle("Hide Arrows", &m_hideArrows);
		static const char* DistanceModes[DistanceMode_NUM] = { "Default", "Metric", "Metric (Metres Only)", "Imperial", "Imperial (Feet Only)" };
		pBank->AddCombo("DistanceMode", &m_debugDistanceMode, DistanceMode_NUM, DistanceModes);
		pBank->AddButton("Invalidate Markers", datCallback(MFA(CHudMarkerRenderer::InvalidateAllMarkers), (datBase*)this));
	}
	pBank->PopGroup();
}
#endif //__BANK