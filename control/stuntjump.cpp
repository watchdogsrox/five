
/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    stuntjump.h
// PURPOSE : Logic to deal with the stunt jump
// AUTHOR :  Greg
// CREATED : 5/03/04
//
/////////////////////////////////////////////////////////////////////////////////

// rage headers
#include "grcore/debugdraw.h"

// game headers
#include "audio/scriptaudioentity.h"
#include "control/gamelogic.h"
#include "control/stuntjump.h"
#include "core/game.h"
#include "frontend/frontendstatsmgr.h"
#include "frontend/MobilePhone.h"
#include "frontend/NewHud.h"
#include "Frontend/ui_channel.h"
#include "scene/world/gameWorld.h"
#include "peds/ped.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "scene/world/GameWorldHeightMap.h"
#include "script/script.h"
#include "Stats/StatsMgr.h"
#include "Text/Messages.h"
#include "vehicles/boat.h"
#include "vehicles/Automobile.h"
#include "audio/frontendaudioentity.h"
#include "audio/northaudioengine.h"
#include "task/system/AsyncProbeHelper.h"

#include "camera/CamInterface.h"
#include "Stats/StatsInterface.h"
#include "modelinfo/PedModelInfo.h"

VEHICLE_OPTIMISATIONS()

FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CStuntJump, CONFIGURED_FROM_FILE, 0.16f, atHashString("CStuntJump",0xfd0abf92));

#define STUNT_JUMP_MOVIE "STUNT_JUMPS"

// milliseconds
#define TWEEN_OUT_DURATION (660)
#define PRINT_MESSAGE_TIMEOUT (2000) 

#if __BANK
static bool bDisplayStuntJumps = false;
static bool bTriggerJumpMessage = false;
static int iDebugJumpsLeft = -1;

static u32 iDebugNumberOfStuntJumps = 0;
static bool bSetFoundStuntJumps = false;
static bool bSetCompletedStuntJumps = false;

static u32 iDebugStuntJumpBitToSet = 0;

static u64 iDebugFoundBitSet = 0;
static char foundBitSetAsString[52];
static bool bSetFoundStuntJumpsMask = false;
static bool bClearFoundStuntJumpsMask = false;

static u64 iDebugCompletedBitSet = 0;
static char completedBitSetAsString[52];
static bool bSetCompletedStuntJumpsMask = false;
static bool bClearCompletedStuntJumpsMask = false;
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CStuntJump::ShouldActivate(const CVehicle* vehicle)
{
	const Vec3V vVehiclePosition = vehicle->GetTransform().GetPosition();

	bool	boxValid = false;
	Vec3V	vEndBoxCentre;
	if( !m_bIsAngled )
	{
		// aaBB
		if(m_startBox.ContainsPoint(vVehiclePosition))
		{
			vEndBoxCentre = m_endBox.GetCenter();
			boxValid = true;
		}
	}
	else
	{
		if(m_startAngledBox.TestPoint(VEC3V_TO_VECTOR3(vVehiclePosition)))
		{
			vEndBoxCentre = VECTOR3_TO_VEC3V( m_endAngledBox.m_vPt1 + ((m_endAngledBox.m_vPt2 - m_endAngledBox.m_vPt1) * 0.5f) );
			boxValid = true;
		}
	}

	if(boxValid)
	{
		// Make sure the player is travelling in the direction of the end box.
		Vector3 vDirToEndBox = VEC3V_TO_VECTOR3(vEndBoxCentre - vVehiclePosition);
		vDirToEndBox.z = 0.0f;

		if (vDirToEndBox.Dot(vehicle->GetVelocity()) > 0.0f)
			return true;
	}
	return false;
}

bool CStuntJump::IsSuccessful(const CVehicle* vehicle)
{
	if( !m_bIsAngled )
	{
		return m_endBox.ContainsPoint(vehicle->GetTransform().GetPosition());
	}
	return m_endAngledBox.TestPoint( VEC3V_TO_VECTOR3(vehicle->GetTransform().GetPosition()) );
}

#if __BANK
void CStuntJump::DebugRender()
{
	Vector3 playerPosn = CGameWorld::FindLocalPlayerCoors();

	Vector3 Diff;
	if( !m_bIsAngled )
	{
		Diff = playerPosn - VEC3V_TO_VECTOR3(m_startBox.GetMin());
	}
	else
	{
		Diff = playerPosn - m_startAngledBox.m_vPt1;
	}
	
	if (Diff.Mag() < 500.0f)
	{
		if( !m_bIsAngled )
		{
			grcDebugDraw::BoxAxisAligned(m_startBox.GetMin(), m_startBox.GetMax(), Color32(255, 0, 0, 255), false);
			grcDebugDraw::BoxAxisAligned(m_endBox.GetMin(), m_endBox.GetMax(), Color32(0, 255, 0, 255), false);
		}
		else
		{
			m_startAngledBox.DebugDraw(Color32(255, 0, 0, 255));
			m_endAngledBox.DebugDraw(Color32(0, 255, 0, 255));
		}
	}
}
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////

CStuntJumpManager::CStuntJumpManager():
m_bActive(true),
mp_Active(NULL),
m_bHitReward(false),
m_fTimer(0.0f),
m_jumpState(JUMP_INACTIVE),
m_iTweenOutColour(HUD_COLOUR_WHITE),
m_iNumJumps(0),
m_iNumActiveJumps(0),
m_levelsEnabled(0),
m_clearTimer(0),
m_messageMovie(false),
m_bMessageVisible(true),
m_bShowOptionalStuntCameras(false),
m_lastStuntJumpId(0)
{

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStuntJumpManager::Init(unsigned initMode)
{
	//@@: range CSTUNTJUMPMANAGER_INIT {
	if(initMode == INIT_CORE)
	{
		//@@: location CSTUNTJUMPMANAGER_INIT_CORE
		CStuntJump::InitPool( MEMBUCKET_GAMEPLAY );
		SStuntJumpManager::Instantiate();
	}
	else if(initMode == INIT_SESSION)
	{
		//@@: location CSTUNTJUMPMANAGER_INIT_SESSION
		CStuntJump::GetPool()->DeleteAll();
		if(uiVerify(SStuntJumpManager::IsInstantiated()))
		{
			SStuntJumpManager::GetInstance().Clear();
		}
	}
	//@@: } CSTUNTJUMPMANAGER_INIT

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStuntJumpManager::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_CORE)
    {
        CStuntJump::ShutdownPool();
		SStuntJumpManager::Destroy();
    }
    else if(shutdownMode == SHUTDOWN_SESSION)
    {
		if(uiVerify(SStuntJumpManager::IsInstantiated()))
		{
		    SStuntJumpManager::GetInstance().AbortStuntJumpInProgress();
		}
	}
}

void CStuntJumpManager::Clear()
{
	mp_Active = NULL;
	m_bHitReward = false;
	m_fTimer = 0.0f;
	m_clearTimer = 0;
	m_jumpState = JUMP_INACTIVE;
	m_iTweenOutColour = HUD_COLOUR_WHITE;
	m_iNumJumps = 0;
	// enable single player stunt jumps
	m_levelsEnabled = (1<<0);
	m_bMessageVisible = true;
	m_iStuntJumpCounter = 0;
	m_messageMovie.RemoveMovie();
	m_bShowOptionalStuntCameras = false;
    m_lastStuntJumpId = 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStuntJumpManager::AbortStuntJumpInProgress()
{
	if (m_jumpState != JUMP_INACTIVE)
	{
		m_jumpState = JUMP_SHUTTING_DOWN;
		m_clearTimer = 0;
		mp_Active = NULL;
	}
}

void CStuntJumpManager::PrintBigMessageStats(const char* pTitle, const char* pTextTag, float distance, float height)
{
	if(m_messageMovie.BeginMethod("SHOW_SHARD_STUNT_JUMP"))
	{
		m_messageMovie.AddParamLocString(pTitle, false);

		if(pTextTag)
		{
			const int bufferSize = 128;
			char buffer[bufferSize];

			CNumberWithinMessage number[2];
			number[0].Set(distance, 2);
			number[1].Set(height, 2);

			CMessages::InsertNumbersAndSubStringsIntoString(TheText.Get(pTextTag), &number[0], 2, NULL, 0, buffer, bufferSize);
			m_messageMovie.AddParamString(buffer, false);
			m_messageMovie.AddParamString(buffer, false);
		}


		m_messageMovie.EndMethod();
	}

	m_bMessageVisible = true;
}

void CStuntJumpManager::PrintBigMessage(const char* pTitle, const char* UNUSED_PARAM(pText), const char* pTextLine2, int duration, s32 NumberToInsert1)
{
	if(m_messageMovie.BeginMethod("SHOW_SHARD_STUNT_JUMP"))
	{
		m_messageMovie.AddParamLocString(pTitle, false);

		if(pTextLine2)
		{
			if(NumberToInsert1 == NO_NUMBER_TO_INSERT_IN_STRING)
			{
				m_messageMovie.AddParamLocString(pTextLine2, false);
				m_messageMovie.AddParamLocString(pTextLine2, false);
			}
			else
			{
				const int bufferSize = 100;
				char buffer[bufferSize];

				CNumberWithinMessage number;
				number.Set(NumberToInsert1);

				CMessages::InsertNumbersAndSubStringsIntoString(TheText.Get(pTextLine2), &number, 1, NULL, 0, buffer, bufferSize);
				m_messageMovie.AddParamString(buffer, false);
				m_messageMovie.AddParamString(buffer, false);
			}
		}

		m_messageMovie.EndMethod();
	}

	m_bMessageVisible = true;
	m_clearTimer = duration;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStuntJumpManager::Update()
{
	if(SStuntJumpManager::IsInstantiated())
	{
		SStuntJumpManager::GetInstance().UpdateHelper();
	}
}

bool IsPlayerInControlOfVehicle(CVehicle* pVehicle, CPlayerInfo* pPlayerInfo, CPed* pPlayerPed)
{
	if(!pVehicle || !pPlayerPed || !pPlayerInfo)
	{
		return false;
	}

	//player is not playing
	if(pPlayerInfo->GetPlayerState() != CPlayerInfo::PLAYERSTATE_PLAYING)
	{
		return false;
	}

	//player is not in a vehicle
	if(!(pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle )))
	{
		return false;
	}

	//car is buggered
	if(	pVehicle->GetStatus() == STATUS_WRECKED ||
		(!pVehicle->InheritsFromBoat() && (pVehicle->m_nVehicleFlags.bIsDrowning || pVehicle->GetIsInWater())))
	{
		return false;
	}

	return true;
}

bool CStuntJumpManager::CanJump(bool& outCanKeepMessageUp) const
{
	outCanKeepMessageUp = false;

	//stunt jump manager is not active
	if(!m_bActive)
	{	
		return false;
	}

	// don't stunt jump while showing character select or switching character
	if(CNewHud::IsShowingCharacterSwitch() || g_PlayerSwitch.IsActive())
	{
		return false;
	}

	CPed* pPlayerPed = (CPed*)CGameWorld::FindLocalPlayer();
	if (!pPlayerPed)
	{
		return false;
	}

	CPlayerInfo* pPlayerInfo = CGameWorld::GetMainPlayerInfo();
	if(!pPlayerInfo)
	{
		return false;
	}

	//player is not playing
	if(pPlayerInfo->GetPlayerState() != CPlayerInfo::PLAYERSTATE_PLAYING)
	{
		return false;
	}

	CVehicle* pVehicle = CGameWorld::FindLocalPlayerVehicle(false);

	//player is not in a vehicle
	if(!pVehicle)
	{
		outCanKeepMessageUp = true;
		return false;
	}

	//if player is not the driver of the vehicle	
	if(pVehicle->GetDriver() != pPlayerPed)
	{
		outCanKeepMessageUp = true;
		return false;
	}

	//player is in a flying vehicle
	if(pVehicle->GetIsAircraft())
	{
		return false;
	}

	//player is in a boosting vehicle
	if(pVehicle->IsRocketBoosting())
	{
		return false;
	}

	if (!pPlayerInfo->GetAllowStuntJumpCamera())
	{
		if (MI_BIKE_OPPRESSOR2.IsValid() && pVehicle->GetModelIndex() == MI_BIKE_OPPRESSOR2 &&
			pVehicle->GetSpecialFlightModeRatio() >= 1.0f - SMALL_FLOAT)
		{
			return false;
		}

		if (pVehicle->pHandling->mFlags & MF_IS_RC)
		{
			return false;
		}
	}

	return true;
}

static CAsyncProbeHelper  s_GroundProbeHelper;

void CStuntJumpManager::ResetJumpStats(const CVehicle* pVehicle)
{
	m_takeOffPosition = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
	m_maxAltitude = 0.0f;
	s_GroundProbeHelper.ResetProbe();
}

void CStuntJumpManager::UpdateJumpStats(const CVehicle* pVehicle)
{
	static Vector3 s_probeStart;

	// If no probe active start a new one
	if(!s_GroundProbeHelper.IsProbeActive())
	{
		s_probeStart = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
		Vector3 vEnd = s_probeStart;
		vEnd.z = CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(vEnd.x, vEnd.y);

		//Create and fire the probe
		WorldProbe::CShapeTestProbeDesc probeData;
		probeData.SetStartAndEnd(s_probeStart, vEnd);
		probeData.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);

		s_GroundProbeHelper.StartTestLineOfSight(probeData);
	}
	else
	{
		// Otherwise wait on probe result
		ProbeStatus probeStatus;
		Vector3 intersectionPos;
		if (s_GroundProbeHelper.GetProbeResultsWithIntersection(probeStatus, &intersectionPos))
		{
			//Probe is done calculating
			if (probeStatus == PS_Blocked)
			{
				float altitude = s_probeStart.z - intersectionPos.z;
				if(altitude > m_maxAltitude)
					m_maxAltitude = altitude;
			}
		}
	}
}

void CStuntJumpManager::SetMovieVisible(bool isVisible)
{
	if(m_messageMovie.IsActive())
	{
		m_messageMovie.CallMethod("SET_VISIBLE", isVisible);
		m_bMessageVisible = isVisible;
	}
	else
	{
		m_bMessageVisible = false;
	}
}

#if __BANK
void CStuntJumpManager::UpdateBitFieldWidgets(bool &bSetBit, bool &bClearAllBits, u64 &iBitSet, char *pBitSetAsString, bool bFound)
{
	bool bUpdateBitMask = false;
	if (bSetBit)
	{
		iBitSet |= (1ull << iDebugStuntJumpBitToSet);
		bUpdateBitMask = true;

		bSetBit = false;
	}

	if (bClearAllBits)
	{
		iBitSet = 0;
		bUpdateBitMask = true;

		bClearAllBits = false;
	}

	if (bUpdateBitMask)
	{
		if (bFound)
		{
			SetStuntJumpFoundMask(iBitSet);
		}
		else
		{
			SetStuntJumpCompletedMask(iBitSet);
		}

		for (u32 bitLoop = 0; bitLoop < 50; bitLoop++)
		{
			pBitSetAsString[bitLoop] = (iBitSet & (1ull << (49-bitLoop))) ? '1' : '0';
		}
		pBitSetAsString[50] = '\0';
	}
}
#endif	//	__BANK

void CStuntJumpManager::UpdateHelper()
{
	CVehicle* pVehicle = NULL;
	CPlayerInfo* pPlayerInfo = NULL;
	CPed* pPlayerPed = (CPed*)CGameWorld::FindLocalPlayer();
	if (pPlayerPed)
	{
		pVehicle = CGameWorld::FindLocalPlayerVehicle(false);
		pPlayerInfo = CGameWorld::GetMainPlayerInfo();
	}
	
#if __BANK
	if (bDisplayStuntJumps)
	{
		const s32 poolSize = CStuntJump::GetPool()->GetSize();
		for(s32 i=0;i<poolSize;i++)
		{
			if(!CStuntJump::GetPool()->GetIsFree(i))
			{
				CStuntJump* p_stuntJump = CStuntJump::GetPool()->GetSlot(i);
				if(p_stuntJump && p_stuntJump->IsEnabled(m_levelsEnabled))
				{
					p_stuntJump->DebugRender();
				}
			}
		}
	}

	if (bSetFoundStuntJumps)
	{
		Displayf("Calling SetStuntJumpFoundStat with %u", iDebugNumberOfStuntJumps);
		SetStuntJumpFoundStat(iDebugNumberOfStuntJumps);
		Displayf("After calling SetStuntJumpFoundStat(), USJS_FOUND is now %d", GetStuntJumpFoundStat());

		bSetFoundStuntJumps = false;
	}

	if (bSetCompletedStuntJumps)
	{
		Displayf("Calling SetStuntJumpCompletedStat with %u", iDebugNumberOfStuntJumps);
		SetStuntJumpCompletedStat(iDebugNumberOfStuntJumps);
		Displayf("After calling SetStuntJumpCompletedStat(), USJS_COMPLETED is now %d", GetStuntJumpCompletedStat());

		bSetCompletedStuntJumps = false;
	}

	UpdateBitFieldWidgets(bSetFoundStuntJumpsMask, bClearFoundStuntJumpsMask, iDebugFoundBitSet, foundBitSetAsString, true);
	UpdateBitFieldWidgets(bSetCompletedStuntJumpsMask, bClearCompletedStuntJumpsMask, iDebugCompletedBitSet, completedBitSetAsString, false);
#endif
	
	bool canKeepMessageUp = false;
	bool canJump = CStuntJumpManager::CanJump(canKeepMessageUp);

	if(!canJump && (m_jumpState != JUMP_INACTIVE || m_messageMovie.IsActive()))
	{
		if(!canKeepMessageUp && m_jumpState != JUMP_SHUTTING_DOWN)
		{
			AbortStuntJumpInProgress();
		}
	}
	ConductorMessageData message;

	switch(m_jumpState)
	{
	case JUMP_SHUTTING_DOWN:
		{
			bool bWheelsAreUp = CNewHud::IsWeaponWheelVisible() || CNewHud::IsRadioWheelActive();
			if(m_bMessageVisible)
			{
				if(CVfxHelper::ShouldRenderInGameUI() && !bWheelsAreUp)
				{
					int clearTimerBefore = m_clearTimer;
					m_clearTimer -= (int)fwTimer::GetTimeStepInMilliseconds();
					if(m_bPrintStatsMessage && 
						m_clearTimer < 4000 && clearTimerBefore >= 4000)
					{
						if(CFrontendStatsMgr::ShouldUseMetric())
							PrintBigMessageStats("USJC", "USJ_STATSM", m_distance, m_maxAltitude);
						else
							PrintBigMessageStats("USJC", "USJ_STATSI", METERS_TO_FEET(m_distance), METERS_TO_FEET(m_maxAltitude));
					}

					// Animate the shard out
					if(m_clearTimer < TWEEN_OUT_DURATION && clearTimerBefore >= TWEEN_OUT_DURATION)
					{
						m_messageMovie.CallMethod("SHARD_ANIM_OUT", m_iTweenOutColour);
					}

					if(m_clearTimer <= 0 REPLAY_ONLY( || CVideoEditorUi::IsActive() ))
					{
						m_messageMovie.RemoveMovie();
						m_jumpState = JUMP_INACTIVE;
					}
				}
				else
				{
					SetMovieVisible(false);
				}
			}
			else if(CVfxHelper::ShouldRenderInGameUI() REPLAY_ONLY( && !CVideoEditorUi::IsActive() ) && !bWheelsAreUp)
			{
				SetMovieVisible(true);
			}
		}

	case JUMP_INACTIVE:
		{
			if(!NetworkInterface::IsGameInProgress())
			{
				message.conductorName = VehicleConductor;
				message.message = StuntJump;
				message.bExtraInfo = false;
				SUPERCONDUCTOR.SendConductorMessage(message);
			}
			//currently not in a jump
			CPhoneMgr::ClearRemoveFlags(CPhoneMgr::WANTS_PHONE_REMOVED_STUNTJUMP);

			if(!canJump)
			{
				break;
			}

			//player must be on the ground
			if(pVehicle && pVehicle->GetFrameCollisionHistory()->HasCollidedWithAnyOfTypesThisFrame(ENTITY_TYPE_MASK_BUILDING | ENTITY_TYPE_MASK_ANIMATED_BUILDING))
			{
				break;
			}

			float fCarSpeed = pVehicle ? pVehicle->GetVelocity().Mag() : 0.0f;

			//car must be going faster than 15 m/s
			if(pVehicle && pVehicle->InheritsFromBoat())
			{
				if(fCarSpeed < 3.0f)
				{
					break;
				}
			}
			else
			{
				if(fCarSpeed < 12.0f)
				{
					break;
				}
			}

			// Don't start a stunt jump in the car is parachuting
			if(pVehicle->InheritsFromAutomobile() && static_cast<CAutomobile*>(pVehicle)->IsParachuting())
			{
				break;
			}

			if( pVehicle->HasGlider() && pVehicle->GetGliderNormaliseDeployedRatio() != 0.0f )
			{
				break;
			}

			//check against the start boxes of each of the stunt jumps
			//this will be changed to access through a quadtree
			const s32 sizeOfStuntJumpPool = CStuntJump::GetPool()->GetSize();
			for(s32 i=0;i<sizeOfStuntJumpPool;i++)
			{
				CStuntJump* p_stuntJump = CStuntJump::GetPool()->GetSlot(i);			

				// Is this stuntjump currently enabled
				if(p_stuntJump && p_stuntJump->IsEnabled(m_levelsEnabled) && p_stuntJump->ShouldActivate(pVehicle))
				{
					m_jumpState = JUMP_ACTIVATED;
					mp_Active = p_stuntJump;
					m_fTimer = 0.0f;
					m_bHitReward = false;
					m_clearTimer = 0; // For fail cases.

					if(!m_messageMovie.IsActive())
					{
						m_messageMovie.CreateMovie(SF_BASE_CLASS_GENERIC, STUNT_JUMP_MOVIE, Vector2(0.0f, 0.0f), Vector2(1.0f, 1.0f), false);  // let the system shut down its own movies
					}
					break;
				}
			}
		}

		break;

	case JUMP_ACTIVATED:

		{
			// if player leaves the vehicle then go to landed
			if(!IsPlayerInControlOfVehicle(pVehicle,pPlayerInfo,pPlayerPed))
			{
				m_jumpState = JUMP_SHUTTING_DOWN;
				break;
			}

			if(!NetworkInterface::IsGameInProgress())
			{
				message.conductorName = VehicleConductor;
				message.message = StuntJump;
				message.bExtraInfo = true;
				SUPERCONDUCTOR.SendConductorMessage(message);
			}

			if(pVehicle->IsInAir())
			{
				if(!IsStuntJumpFound(mp_Active))
				{
					SetStuntJumpFound(mp_Active);
				}

				CPhoneMgr::SetRemoveFlags(CPhoneMgr::WANTS_PHONE_REMOVED_STUNTJUMP);
				ResetJumpStats(pVehicle);
				m_jumpState = JUMP_JUMPING;
				break;
			}

			bool bVehicleInTrigger = false;
			// if we leave the box and haven't gained air cancel stuntjump
			if( !mp_Active->m_bIsAngled )
			{
				if(mp_Active->m_startBox.ContainsPoint(pVehicle->GetTransform().GetPosition()))
					bVehicleInTrigger = true;
			}
			else
			{
				if(mp_Active->m_startAngledBox.TestPoint(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition())))
					bVehicleInTrigger = true;
			}

			// if the centre of the vehicle has left the box, also check the position above the rear axle, longer vehicles like buses
			// can still be on the ground and the rear wheels in the box but the centre exited.
			if( !bVehicleInTrigger && pVehicle->GetWheelFromId(VEH_WHEEL_LR))
			{
				Vec3V vRearAxle = pVehicle->GetTransform().GetPosition() + (pVehicle->GetVehicleModelInfo()->GetWheelOffset(VEH_WHEEL_LR).GetY() * pVehicle->GetVehicleForwardDirection());
				if( !mp_Active->m_bIsAngled )
				{
					if(mp_Active->m_startBox.ContainsPoint(vRearAxle))
						bVehicleInTrigger = true;
				}
				else
				{
					if(mp_Active->m_startAngledBox.TestPoint(VEC3V_TO_VECTOR3(vRearAxle)))
						bVehicleInTrigger = true;
				}				
			}

			if( !bVehicleInTrigger )
			{
				m_jumpState = JUMP_SHUTTING_DOWN;
			}
		}

		break;

	case JUMP_JUMPING:
		{
			// If stunt jump results is up clear it
			SetMovieVisible(false);

			// if player leaves the vehicle then go to landed
			if(!IsPlayerInControlOfVehicle(pVehicle,pPlayerInfo,pPlayerPed))
			{
				m_jumpState = JUMP_LANDED;
				break;
			}

			//currently in jump
			bool bRestore = false;

			// update height calculations
			UpdateJumpStats(pVehicle);

			// Don't allow any collisions with non map geometry
			bool bSeriousCollisionFound = pVehicle->GetFrameCollisionHistory()->HasCollidedWithAnyOfTypes((u16)~ENTITY_TYPE_MASK_BUILDING);
			if(!bSeriousCollisionFound)
			{
				if(const CCollisionRecord* pCollisionRecord = pVehicle->GetFrameCollisionHistory()->GetMostSignificantCollisionRecordOfType(ENTITY_TYPE_BUILDING))
				{
					// If we do collide with the map, allow collisions on the bottom of the chassis. These collisions should have normals nearly aligned with the vehicle up vector
					const ScalarV minCollisionCosine = ScalarVFromF32(0.866f); // ~30 degrees
					const ScalarV collisionAngleCosine = Dot(RCC_VEC3V(pCollisionRecord->m_MyCollisionNormal),pVehicle->GetTransform().GetUp());
					if(IsLessThanAll(collisionAngleCosine,minCollisionCosine))
					{
						bSeriousCollisionFound = true;
					}
				}
			}

			const bool bDeluxo = MI_CAR_DELUXO.IsValid() && pVehicle->GetModelIndex() == MI_CAR_DELUXO;
			const bool bIsDeluxoFlying = (bDeluxo && pVehicle->GetSpecialFlightModeRatio() >= 1.0f-SMALL_FLOAT);

			if(!(bSeriousCollisionFound || bIsDeluxoFlying) || m_fTimer < 1000.0f)
			{
				// The boat has landed in water, not necessarily stably
				if (pVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT && ((CBoat *)pVehicle)->m_BoatHandling.IsInWater())
				{
					bRestore = true;
				}

				// Only end the jump if the vehicle isn't in midair and is stuck, otherwise the player will fail as soon as one wheel touches the ground
				if (!pVehicle->IsInAir() && pVehicle->GetVelocity().Mag2() < 1.0f)
				{
					bRestore = true;
				}

				if(pVehicle->HasLandedStably())
				{
					bRestore = true;
					//if we hit the second box then we trigger the reward
					if(mp_Active->IsSuccessful(pVehicle))
					{
						m_bHitReward = true;
						m_distance = (VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()) - m_takeOffPosition).Mag();
					}
				}
			}
			else
			{
				//have landed on the ground
				bRestore = true;
			}

			if(bRestore)
			{
				m_jumpState = JUMP_LANDED;
				m_fTimer = 0.0f;			
			}
			if(!fwTimer::IsGamePaused())
			{
				m_fTimer += fwTimer::GetTimeStepInMillsecondsHighRes();
			}
		}
		break;

	case JUMP_LANDED:
		{
			if(!fwTimer::IsGamePaused())
			{
				m_fTimer += fwTimer::GetTimeStepInMillsecondsHighRes();
			}
			if(m_fTimer < 300.0f)
			{
				// The player must maintain control of the vehicle until we tell them they were successful
				if(m_bHitReward && !IsPlayerInControlOfVehicle(pVehicle,pPlayerInfo,pPlayerPed))
				{
					m_bHitReward = false;
				}
				break;
			}
			CPhoneMgr::ClearRemoveFlags(CPhoneMgr::WANTS_PHONE_REMOVED_STUNTJUMP);
			CHelpMessage::Clear(HELP_TEXT_SLOT_STANDARD, false);

			m_clearTimer = 0;
		}
		// Intentionally no break here.
	case JUMP_LANDED_PRINTING_TEXT:
		{
			if (!CVfxHelper::ShouldRenderInGameUI())
			{
				break;
			}
			m_bPrintStatsMessage = false;

			if(m_messageMovie.IsActive())
			{
				m_clearTimer = 0;

				const char* pTitle = "USJ";
				const char* pText = NULL;
				bool bCompleted = IsStuntJumpCompleted(mp_Active);

				if(m_bHitReward)
				{
					if(!NetworkInterface::IsGameInProgress())
					{
						// MP stunt jump audio is handled differently
						Displayf("CStuntJumpManager::PlaySound Playing sound STUNT_JUMP_COMPLETE status: JUMP_LANDED_PRINTING_TEXT");
						g_FrontendAudioEntity.PlaySound("STUNT_JUMP_COMPLETED", "HUD_AWARDS");
					}
					m_iTweenOutColour = HUD_COLOUR_WHITE;

					pTitle = "USJC";
					if (bCompleted)
					{
						// Passed stunt jump but it had been passed before.
						pText = "USJCA";
					}
					else
					{
						pText = "USJC";
						SetStuntJumpCompleted(mp_Active);
						// if in network game attempt to save stats
						StatsInterface::TryMultiplayerSave( STAT_SAVETYPE_STUNTJUMP );
					}
					// Only increment the total jumps completed (this also counts the already completed more than once)
					if(!NetworkInterface::IsGameInProgress())
					{
						StatId statTotal("USJS_TOTAL_COMPLETED");
						StatsInterface::IncrementStat(statTotal, 1.0f);
					}
					else
					{
						StatId statTotal = StatsInterface::GetStatsModelHashId("USJS_TOTAL_COMPLETED");
						StatsInterface::IncrementStat(statTotal, 1.0f);
					}
                    m_lastStuntJumpId = mp_Active->m_id;
				}
				else
				{
					if(!NetworkInterface::IsGameInProgress())
					{
						// MP stunt jump audio is handled differently
						Displayf("CStuntJumpManager::PlaySound Playing sound STUNT_JUMP_FAILED status: JUMP_LANDED_PRINTING_TEXT");
						g_FrontendAudioEntity.PlaySound("STUNT_JUMP_FAILED", "HUD_AWARDS");
					}
					m_iTweenOutColour = HUD_COLOUR_RED;

					pTitle = "USJFAIL";
					if (bCompleted)
					{
						pText = "USJFAILA";
					}
					else
					{
						pText = "USJFAIL";
					}
				}

				// bit of a hack to ensure stunt jumps level 2 and above don't display messages
				if(mp_Active->m_level <= 1)
				{
					int timeToDisplay = 4000;
					if(m_bHitReward)
					{
						m_bPrintStatsMessage = true;
						timeToDisplay = 6000;
					}

					s32 jumpsLeft = m_iNumActiveJumps - GetStuntJumpCompletedStat();

#if __BANK
					// Override jumps left
					if(iDebugJumpsLeft != -1)
					{
						jumpsLeft = iDebugJumpsLeft;
					}

					Displayf("[stuntjump] m_iNumActiveJumps = %d, GetStuntJumpCompletedStat() = %d GetStuntJumpCompletedMask() = %lu",m_iNumActiveJumps, GetStuntJumpCompletedStat(), GetStuntJumpCompletedMask());

					Assertf(GetStuntJumpCompletedStat() <= m_iNumActiveJumps, "CStuntJumpManager::UpdateHelper - stunt jumps completed (%d) is greater than the total number of stunt jumps (%d). GetStuntJumpCompletedMask() = %lu", GetStuntJumpCompletedStat(), m_iNumActiveJumps, GetStuntJumpCompletedMask());
#endif

					if (jumpsLeft==0)
					{
						PrintBigMessage(pTitle, pText, "USJ_ALL", timeToDisplay);
					}
					else if (jumpsLeft==1)
					{
						PrintBigMessage(pTitle, pText, "USJ_1LEFT", timeToDisplay);
					}
					else if (jumpsLeft > 1)
					{
						PrintBigMessage(pTitle, pText, "USJ_LEFT", timeToDisplay, jumpsLeft);
					}
					else
					{
						PrintBigMessage(pTitle, pText, "USJ_FAILSAFE", timeToDisplay);
					}
				}

				m_jumpState = JUMP_SHUTTING_DOWN;
				mp_Active = NULL;
			}
			else if(!m_messageMovie.IsFree())
			{
				m_jumpState = JUMP_LANDED_PRINTING_TEXT;

				if(0 < m_clearTimer)
				{
					m_clearTimer -= (int)fwTimer::GetTimeStepInMilliseconds();
					if(m_clearTimer <= 0)
					{
						AbortStuntJumpInProgress();
					}
				}
				else
				{
					m_clearTimer = PRINT_MESSAGE_TIMEOUT;
				}
			}
			else
			{
				AbortStuntJumpInProgress();
			}
		}
		break;

	}


	pPlayerInfo->SetAllowStuntJumpCamera(false);

#if __BANK
	if(bTriggerJumpMessage && m_jumpState == JUMP_INACTIVE && mp_Active == NULL)
	{
		bTriggerJumpMessage = false;

		const s32 sizeOfStuntJumpPool = CStuntJump::GetPool()->GetSize();
		for(s32 i=0;i<sizeOfStuntJumpPool;i++)
		{
			CStuntJump* p_stuntJump = CStuntJump::GetPool()->GetSlot(i);			

			// Is this stuntjump currently enabled
			if(p_stuntJump)
			{
				m_jumpState = JUMP_LANDED;
				mp_Active = p_stuntJump;
				m_bHitReward = true;
				m_fTimer = 0.0f;
				m_messageMovie.CreateMovie(SF_BASE_CLASS_GENERIC, STUNT_JUMP_MOVIE, Vector2(0.0f, 0.0f), Vector2(1.0f, 1.0f));
				break;
			}
		}
	}
#endif
}

StatId GetStuntJumpStat(const char* pStatName)
{
	if(!NetworkInterface::IsGameInProgress())
	{
		return StatId(pStatName);
	}
	else
	{
		return StatsInterface::GetStatsModelHashId(pStatName);
	}
}
int CStuntJumpManager::GetStuntJumpFoundStat()
{
	StatId stat = GetStuntJumpStat("USJS_FOUND");
	return StatsInterface::GetIntStat(stat);
}
void CStuntJumpManager::SetStuntJumpFoundStat(int number_of_found_stunt_jumps)
{
	StatId stat = GetStuntJumpStat("USJS_FOUND");
	StatsInterface::SetStatData(stat, number_of_found_stunt_jumps);	//	const u32 flags = STATUPDATEFLAG_DEFAULT
}
u64 CStuntJumpManager::GetStuntJumpFoundMask()
{
	StatId stat = GetStuntJumpStat("USJS_FOUND_MASK");
	return StatsInterface::GetUInt64Stat(stat);
}
void CStuntJumpManager::SetStuntJumpFoundMask(u64 bitMask)
{
	StatId stat = GetStuntJumpStat("USJS_FOUND_MASK");
	StatsInterface::SetStatData(stat, bitMask);	//	const u32 flags = STATUPDATEFLAG_DEFAULT
}
void CStuntJumpManager::IncrementStuntJumpFoundStat()
{
	StatId stat = GetStuntJumpStat("USJS_FOUND");
	StatsInterface::IncrementStat(stat, 1.0f);
}

int CStuntJumpManager::GetStuntJumpCompletedStat()
{
	StatId stat = GetStuntJumpStat("USJS_COMPLETED");
	return StatsInterface::GetIntStat(stat);
}
void CStuntJumpManager::SetStuntJumpCompletedStat(int number_of_completed_stunt_jumps)
{
	StatId stat = GetStuntJumpStat("USJS_COMPLETED");
	StatsInterface::SetStatData(stat, number_of_completed_stunt_jumps);	//	const u32 flags = STATUPDATEFLAG_DEFAULT
}
int CStuntJumpManager::GetTotalStuntJumpCompletedStat()
{
	StatId stat = GetStuntJumpStat("USJS_TOTAL_COMPLETED");
	return StatsInterface::GetIntStat(stat);
}
u64 CStuntJumpManager::GetStuntJumpCompletedMask()
{
	StatId stat = GetStuntJumpStat("USJS_COMPLETED_MASK");
	return StatsInterface::GetUInt64Stat(stat);
}
void CStuntJumpManager::SetStuntJumpCompletedMask(u64 bitMask)
{
	StatId stat = GetStuntJumpStat("USJS_COMPLETED_MASK");
	StatsInterface::SetStatData(stat, bitMask);	//	const u32 flags = STATUPDATEFLAG_DEFAULT
}

void CStuntJumpManager::IncrementStuntJumpCompletedStat()
{
	if(!NetworkInterface::IsGameInProgress())
	{
		StatId stat("USJS_COMPLETED");
		StatsInterface::IncrementStat(stat, 1.0f);

		StatId percentStat("PERCENT_USJS");
		int numStuntJumpsFinished = StatsInterface::GetIntStat(stat);
		StatsInterface::SetStatData(percentStat, (int)((100.0f * numStuntJumpsFinished) / GetNumActiveJumps()));
	}
	else
	{
		StatId stat = StatsInterface::GetStatsModelHashId("USJS_COMPLETED");
		StatsInterface::IncrementStat(stat, 1.0f);
	}
}
bool CStuntJumpManager::IsStuntJumpFound(const CStuntJump* pStuntJump)
{
	StatId stat = GetStuntJumpStat("USJS_FOUND_MASK");
	int id = pStuntJump->m_id;//CStuntJump::GetPool()->GetJustIndex(pStuntJump);
	if(id == CStuntJump::INVALID_ID)
		return false;
	u64 stuntJumpsFoundMask = StatsInterface::GetUInt64Stat(stat);
	return (stuntJumpsFoundMask & (1ull<<id)) != 0;
}
void CStuntJumpManager::SetStuntJumpFound(const CStuntJump* pStuntJump)
{
	StatId stat = GetStuntJumpStat("USJS_FOUND_MASK");
	int id = pStuntJump->m_id;//CStuntJump::GetPool()->GetJustIndex(pStuntJump);
	if(id == CStuntJump::INVALID_ID)
		return;
	u64 stuntJumpsFoundMask = StatsInterface::GetUInt64Stat(stat);
	stuntJumpsFoundMask |= (1ull<<id);
	StatsInterface::SetStatData(stat, stuntJumpsFoundMask);

	if (GetStuntJumpFoundStat() == 0)
	{
		if (!NetworkInterface::IsGameInProgress())
		{	//	Bug 890637 - Based on shouldInhibitSlowMotion in camCinematicDirector::UpdateCinematicViewModeSlowMotionControl()
			//	This does mean that you'll never see the message if the first stuntjump you find is during a network game
			CHelpMessage::SetMessageTextAndAddToBrief(HELP_TEXT_SLOT_STANDARD, "USJ_FRST");
		}
	}

	IncrementStuntJumpFoundStat();
}
bool CStuntJumpManager::IsStuntJumpCompleted(const CStuntJump* pStuntJump)
{
	StatId stat = GetStuntJumpStat("USJS_COMPLETED_MASK");
	int id = pStuntJump->m_id;//CStuntJump::GetPool()->GetJustIndex(pStuntJump);
	if(id == CStuntJump::INVALID_ID)
		return false;
	u64 stuntJumpsFoundMask = StatsInterface::GetUInt64Stat(stat);
	return (stuntJumpsFoundMask & (1ull<<id)) != 0;
}
void CStuntJumpManager::SetStuntJumpCompleted(const CStuntJump* pStuntJump)
{
	StatId stat = GetStuntJumpStat("USJS_COMPLETED_MASK");
	int id = pStuntJump->m_id;//CStuntJump::GetPool()->GetJustIndex(pStuntJump);
	if(id == CStuntJump::INVALID_ID)
		return;
	u64 stuntJumpsFoundMask = StatsInterface::GetUInt64Stat(stat);
	stuntJumpsFoundMask |= (1ull<<id);
	StatsInterface::SetStatData(stat, stuntJumpsFoundMask);

	IncrementStuntJumpCompletedStat();

	CTheScripts::SetCodeRequestedAutoSave(true);
}


void CStuntJumpManager::ValidateStuntJumpStats(bool bFixIncorrectTotals)
{
#if !__NO_OUTPUT
	const int MaxStuntJumps = 50;	//	Graeme - I think the max number of stunt jumps is always 50 in GTA5
#endif	//	!__NO_OUTPUT

	int num_completed = GetStuntJumpCompletedStat();
	Displayf("CStuntJumpManager::ValidateStuntJumpStats - the stat for the number of stunt jumps completed is %d. Expected it to be >= 0 and <= %d", num_completed, MaxStuntJumps);
	Assertf(num_completed >= 0 && num_completed <= MaxStuntJumps, "CStuntJumpManager::ValidateStuntJumpStats - the stat for the number of stunt jumps completed is %d. Expected it to be >= 0 and <= %d", num_completed, MaxStuntJumps);


	u64 completed_mask = GetStuntJumpCompletedMask();
	u32 bit_loop = 0;
	s32 number_of_completed_bits_set = 0;
	for (bit_loop = 0; bit_loop < 64; bit_loop++)
	{
		if ((completed_mask & (1ull<<bit_loop)) != 0)
		{
			number_of_completed_bits_set++;
		}
	}
	Displayf("CStuntJumpManager::ValidateStuntJumpStats - the stat for the number of stunt jumps completed = %d. The number of bits set in the completed mask = %d", num_completed, number_of_completed_bits_set);

	if (num_completed != number_of_completed_bits_set)
	{
		Assertf(0, "CStuntJumpManager::ValidateStuntJumpStats - the stat for the number of stunt jumps completed (%d) doesn't match the number of bits set in the completed mask (%d)", num_completed, number_of_completed_bits_set);
		if (bFixIncorrectTotals)
		{
			Displayf("CStuntJumpManager::ValidateStuntJumpStats - attempting to set USJS_COMPLETED to %d to match the number of bits set in the completed mask", number_of_completed_bits_set);
			SetStuntJumpCompletedStat(number_of_completed_bits_set);
			Displayf("CStuntJumpManager::ValidateStuntJumpStats - after calling SetStuntJumpCompletedStat(), USJS_COMPLETED is now %d", GetStuntJumpCompletedStat());
		}
	}

	int num_found = GetStuntJumpFoundStat();
	Displayf("CStuntJumpManager::ValidateStuntJumpStats - the stat for the number of stunt jumps found is %d. Expected it to be >= 0 and <= %d", num_found, MaxStuntJumps);
	Assertf(num_found >= 0 && num_found <= MaxStuntJumps, "CStuntJumpManager::ValidateStuntJumpStats - the stat for the number of stunt jumps found is %d. Expected it to be >= 0 and <= %d", num_found, MaxStuntJumps);

	u64 found_mask = GetStuntJumpFoundMask();
	s32 number_of_found_bits_set = 0;
	for (bit_loop = 0; bit_loop < 64; bit_loop++)
	{
		if ((found_mask & (1ull<<bit_loop)) != 0)
		{
			number_of_found_bits_set++;
		}
	}
	Displayf("CStuntJumpManager::ValidateStuntJumpStats - the stat for the number of stunt jumps found = %d. The number of bits set in the found mask = %d", num_found, number_of_found_bits_set);

	if (num_found != number_of_found_bits_set)
	{
		Assertf(0, "CStuntJumpManager::ValidateStuntJumpStats - the stat for the number of stunt jumps found (%d) doesn't match the number of bits set in the found mask (%d)", num_found, number_of_found_bits_set);
		if (bFixIncorrectTotals)
		{
			Displayf("CStuntJumpManager::ValidateStuntJumpStats - attempting to set USJS_FOUND to %d to match the number of bits set in the found mask", number_of_found_bits_set);
			SetStuntJumpFoundStat(number_of_found_bits_set);
			Displayf("CStuntJumpManager::ValidateStuntJumpStats - after calling SetStuntJumpFoundStat(), USJS_FOUND is now %d", GetStuntJumpFoundStat());
		}
	}
}

s8 CStuntJumpManager::GetLastSuccessfulStuntJump() const
{
    return m_lastStuntJumpId;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStuntJumpManager::Render()
{
	if(SStuntJumpManager::IsInstantiated())
	{
		SStuntJumpManager::GetInstance().RenderHelper();
	}
}

void CStuntJumpManager::RenderHelper()
{
	m_messageMovie.Render();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStuntJumpManager::IsAStuntjumpInProgress()
{
	return SStuntJumpManager::IsInstantiated() ? (SStuntJumpManager::GetInstance().m_jumpState > JUMP_ACTIVATED) : false;
}

bool CStuntJumpManager::IsStuntjumpMessageShowing()
{
	return SStuntJumpManager::IsInstantiated() ? (SStuntJumpManager::GetInstance().m_jumpState == JUMP_SHUTTING_DOWN) : false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////
int CStuntJumpManager::AddOne(Vec3V_In startMin, Vec3V_In startMax, Vec3V_In endMin, Vec3V_In endMax, Vec3V_In vecCamera, int iScore, int level, bool camOptional)
{
	uiAssertf(level >= 0 && level < 32, "stuntjump level can only be 0-31");
	spdAABB startBox(startMin, startMax);
	spdAABB endBox(endMin, endMax);

	uiAssertf(startBox.IsValid(), "Stunt jump start box (%f,%f,%f) (%f,%f,%f) is invalid", 
		startMin.GetXf(), startMin.GetYf(), startMin.GetZf(),
		startMax.GetXf(), startMax.GetYf(), startMax.GetZf());
	uiAssertf(endBox.IsValid(), "Stunt jump end box (%f,%f,%f) (%f,%f,%f) is invalid", 
		endMin.GetXf(), endMin.GetYf(), endMin.GetZf(),
		endMax.GetXf(), endMax.GetYf(), endMax.GetZf());

	int id = CStuntJump::INVALID_ID;
	if(level == 0)
		id = m_iStuntJumpCounter++;

	CStuntJump* p_newStuntJump = rage_new CStuntJump(startBox, endBox, vecCamera, camOptional, iScore, level, id);

	if(!uiVerifyf(p_newStuntJump, "Failed to create a new stunt jump"))
	{
		return 0;
	}

	RegisterAddedStuntJump(p_newStuntJump);

	return CStuntJump::GetPool()->GetIndex(p_newStuntJump);
}

int CStuntJumpManager::AddOneAngled(Vec3V_In startMin, Vec3V_In startMax, float startWidth, Vec3V_In endMin, Vec3V_In endMax, float endWidth, Vec3V_In vecCamera, int iScore, int level, bool camOptional)
{
	uiAssertf(level >= 0 && level < 32, "stuntjump level can only be 0-31");

	int id = CStuntJump::INVALID_ID;
	if(level == 0)
		id = m_iStuntJumpCounter++;

	CStuntJump* p_newStuntJump = rage_new CStuntJump(startMin, startMax, startWidth, endMin, endMax, endWidth, vecCamera, camOptional, iScore, level, id);

	if(!uiVerifyf(p_newStuntJump, "Failed to create a new stunt jump"))
	{
		return 0;
	}

	RegisterAddedStuntJump(p_newStuntJump);

	return CStuntJump::GetPool()->GetIndex(p_newStuntJump);
}

void CStuntJumpManager::RegisterAddedStuntJump(const CStuntJump* p_newStuntJump)
{
	scrThread* pCurrentScript = scrThread::GetActiveThread();
	if(pCurrentScript)
		Displayf("Script %s created StuntJump %d in level %d, id %d", pCurrentScript->GetScriptName(), CStuntJump::GetPool()->GetIndex(p_newStuntJump), p_newStuntJump->m_level, p_newStuntJump->m_id);
	else
		Displayf("Code created StuntJump %d in level %d, id %d", CStuntJump::GetPool()->GetIndex(p_newStuntJump), p_newStuntJump->m_level, p_newStuntJump->m_id);

	m_iNumJumps++;
	if(p_newStuntJump->IsEnabled(m_levelsEnabled))
		m_iNumActiveJumps++;
}

void CStuntJumpManager::DeleteOne(int id)
{
	CStuntJump* pStuntJump = CStuntJump::GetPool()->GetAt(id);
	if(uiVerifyf(pStuntJump, "Trying to delete stunt jump id:%d when it does not exist", id))
	{
		scrThread* pCurrentScript = scrThread::GetActiveThread();
		if(pCurrentScript)
			Displayf("Script %s deleted StuntJump %d from level %d, id %d", pCurrentScript->GetScriptName(), id, pStuntJump->m_level, pStuntJump->m_id);
		else
			Displayf("Code deleted StuntJump %d from level %d, id %d", id, pStuntJump->m_level, pStuntJump->m_id);

		Assertf(pStuntJump->m_id == CStuntJump::INVALID_ID, "Not allowed to remove stunt jump %d with an id %d", id, pStuntJump->m_id);

		m_iNumJumps--;
		if(pStuntJump->IsEnabled(m_levelsEnabled))
			m_iNumActiveJumps--;
		delete pStuntJump;
	}
}

// PURPOSE: Return number of stunt jumps that are currently active
void CStuntJumpManager::RecalculateNumActiveJumps()
{
	m_iNumActiveJumps = 0;
	int i = CStuntJump::GetPool()->GetSize();
	while(i--)
	{
		CStuntJump* pStuntJump = CStuntJump::GetPool()->GetSlot(i);	
		if(pStuntJump && pStuntJump->IsEnabled(m_levelsEnabled))
		{
			m_iNumActiveJumps++;
		}
	}
}

void CStuntJumpManager::EnableSet(int level)
{
	uiAssertf(level >= 0 && level < 32, "stuntjump level can only be 0-31");
	m_levelsEnabled |= (1 << level);

	RecalculateNumActiveJumps();
}

void CStuntJumpManager::DisableSet(int level)
{
	uiAssertf(level >= 0 && level < 32, "stuntjump level can only be 0-31");
	m_levelsEnabled &= ~(1 << level);

	RecalculateNumActiveJumps();
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStuntJumpManager::SetActive(bool bNewState)
{
	m_bActive = bNewState;
}

bool CStuntJumpManager::GetCameraPositionForStuntJump(Vector3& CameraPos)
{
	if(mp_Active)
	{
		CameraPos = VEC3V_TO_VECTOR3(mp_Active->m_vecCamera); 
		return true; 
	}
	return false;
}

bool CStuntJumpManager::IsStuntCamOptional() const
{
	return mp_Active && mp_Active->m_bIsCameraOptional;
}

#if __BANK
void CStuntJumpManager::InitWidgets()
{
	bDisplayStuntJumps = false;
	bkBank *bank = CGameLogic::GetGameLogicBank();
	bank->PushGroup("Stunt Jumps", false);
		bank->AddToggle("Display Stunt Jumps", &bDisplayStuntJumps);
		bank->AddToggle("Trigger Jump Message", &bTriggerJumpMessage);
		bank->AddText("Override Stunt Jumps Left", &iDebugJumpsLeft);

		bank->AddSlider("Number of Stunt Jumps", &iDebugNumberOfStuntJumps, 0, 50, 1);
		bank->AddToggle("Set Found Stunt Jumps", &bSetFoundStuntJumps);
		bank->AddToggle("Set Completed Stunt Jumps", &bSetCompletedStuntJumps);

		bank->AddSlider("Stunt Jump Bit To Set", &iDebugStuntJumpBitToSet, 0, 49, 1);

		bank->AddText("Found Bits", foundBitSetAsString, NELEM(foundBitSetAsString), true);
		bank->AddToggle("Set Found Stunt Jump Bit", &bSetFoundStuntJumpsMask);
		bank->AddToggle("Clear Found Stunt Jump Bits", &bClearFoundStuntJumpsMask);

		bank->AddText("Completed Bits", completedBitSetAsString, NELEM(completedBitSetAsString), true);
		bank->AddToggle("Set Completed Stunt Jump Bit", &bSetCompletedStuntJumpsMask);
		bank->AddToggle("Clear Completed Stunt Jump Bits", &bClearCompletedStuntJumpsMask);

	bank->PopGroup();
}
#endif
