// FILE :    TaskAmbient.cpp
// PURPOSE : General ambient task run in unison with movement task that manages
//			 Streaming and playback of clips based on the file Ambient.dat
// AUTHOR :  Phil H
// CREATED : 08-08-2006

// File header
#include "Task/Default/TaskAmbient.h" 

// Framework headers
#include "fwanimation/clipsets.h"
#include "fwanimation/directorcomponentsyncedscene.h"
#include "grcore/debugdraw.h"
#include "vfx/channel.h"
#include "vfx/ptfx/ptfxmanager.h"

// Game headers
#include "ai/ambient/AmbientModelSetManager.h"
#include "ai/AnimBlackboard.h"
#include "animation/MovePed.h"
#include "camera/viewports/ViewportManager.h"
#include "camera/CamInterface.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "Debug/DebugScene.h"
#include "Event/EventShocking.h"
#include "Event/EventWeapon.h"
#include "Event/ShockingEvents.h"
#include "frontend/MobilePhone.h"
#include "Modelinfo/ModelSeatInfo.h"
#include "modelinfo/Vehiclemodelinfo.h"
#include "ModelInfo/WeaponModelInfo.h"
#include "network/objects/entities/NetObjPlayer.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedMoveBlend/PedMoveBlendOnFoot.h"
#include "Peds/relationships.h"
#include "Scene/World/GameWorld.h"
#include "scene/EntityIterator.h"
#include "streaming/streaming.h"		// For CStreaming::HasObjectLoaded(), etc.
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Default/AmbientAnimationManager.h"
#include "Task/Default/AmbientAudio.h"
#include "Task/Helpers/PropManagementHelper.h"
#include "Task/Helpers/TaskConversationHelper.h"
#include "Task/Motion/TaskMotionBase.h"
#include "Task/Motion/Locomotion/TaskLocomotion.h"
#include "Task/Motion/Locomotion/TaskHumanLocomotion.h"
#include "Task/Movement/TaskMoveWander.h"
#include "Task/Movement/TaskNavMesh.h"
#include "task/Response/TaskShockingEvents.h"
#include "Task/Scenario/ScenarioFinder.h"
#include "Task/Scenario/Info/ScenarioInfoManager.h"
#include "Task/Scenario/Types/TaskUseScenario.h"
#include "Task/System/TaskHelpers.h"
#include "Task/Vehicle/TaskInVehicle.h"
#include "Task/Vehicle/TaskVehicleBase.h"
#include "vehicleAi/task/TaskVehicleGoToAutomobile.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "Vehicles/Metadata/VehicleEntryPointInfo.h"
#include "vfx/systems/VfxEntity.h"
#include "camera/gameplay/ThirdPersonCamera.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/cinematic/CinematicDirector.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "vehicleAi/task/TaskVehicleAnimation.h"
#include "vehicles\automobile.h"
#include "Task/Default/TaskPlayer.h"
#include "Frontend/NewHud.h"

AI_OPTIMISATIONS()
AI_SCENARIO_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()
//OPTIMISATIONS_OFF()

namespace AIStats
{
	EXT_PF_TIMER(UpdateAnimationVFX);
}
using namespace AIStats;

////////////////////////////////////////////////////////////////////////////////
// CLookAtHistory
////////////////////////////////////////////////////////////////////////////////

// Static tuning data
CLookAtHistory::Tunables CLookAtHistory::sm_Tunables;
IMPLEMENT_DEFAULT_TASK_TUNABLES(CLookAtHistory, 0xe80e7cf9);

void CLookAtHistory::AddHistory(const CEntity* pEnt)
{
	const CEntity* pCurrent = pEnt;
	u32 iCurrentTime = fwTimer::GetTimeInMilliseconds();
	if (m_qLastLookAts.IsFull())
	{
		m_qLastLookAts.Drop();
		m_qLastTimes.Drop();
	}
	m_qLastLookAts.Push(RegdConstEnt(pCurrent));
	m_qLastTimes.Push(iCurrentTime);
}

// Return true if we have previously looked near pCheckEntity
bool CLookAtHistory::CheckHistory(const CPed& rOriginPed, const CEntity& rCheckEntity)
{
	Vec3V vOriginPos = rOriginPed.GetTransform().GetPosition();
	for(int i=0; i < m_qLastLookAts.GetCount(); i++)
	{
		const CEntity* pEntity = m_qLastLookAts[i];
		if (pEntity)
		{
			// The new entity is in the history.
			if (pEntity == &rCheckEntity)
			{
				return true;
			}
			else
			{
				// Check if the angle to the new entity is close to the angle to the history entity.
				Vec3V vCheckEntity = rCheckEntity.GetTransform().GetPosition() - vOriginPos;
				vCheckEntity = NormalizeSafe(vCheckEntity, Vec3V(V_ZERO));
				Vec3V vHistoryEntity = pEntity->GetTransform().GetPosition() - vOriginPos;
				vHistoryEntity = NormalizeSafe(vHistoryEntity, Vec3V(V_ZERO));
				if (IsGreaterThanAll(Dot(vCheckEntity, vHistoryEntity), ScalarV(sm_Tunables.m_HistoryCosineThreshold)))
				{
					return true;
				}
			}
		}
	}
	return false;
}

void CLookAtHistory::ProcessHistory()
{
	const u32 iCurrentTime = fwTimer::GetTimeInMilliseconds();
	for(int i=0; i < m_qLastLookAts.GetCount(); i++)
	{
		if (m_qLastLookAts[i])
		{
			if (iCurrentTime - m_qLastTimes[i] >= sm_Tunables.m_MemoryDuration)
			{
				m_qLastLookAts.Drop();
				m_qLastTimes.Drop();
			}
			else
			{
				// Sorted, so if the oldest didn't expire none of them will.
				break;
			}
		}
		else
		{
			// The entity was deleted, pop it off but continue searching the history.
			m_qLastLookAts.Drop();
			m_qLastTimes.Drop();
		}
	}
}


////////////////////////////////////////////////////////////////////////////////

// Static tuning data
CAmbientLookAt::Tunables CAmbientLookAt::sm_Tunables;
IMPLEMENT_DEFAULT_TASK_TUNABLES(CAmbientLookAt, 0x4ec5ae40);

////////////////////////////////////////////////////////////////////////////////

// Static constants
const u8   CAmbientLookAt::MAX_ENTITIES_TO_LOOKAT			= 32;

FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CAmbientLookAt, CONFIGURED_FROM_FILE, 0.23f, atHashString("CAmbientLookAt",0x4ec5ae40));

CAmbientLookAt::CAmbientLookAt()
: m_pLookAtScenarioPoint(NULL)
, m_fLookAtTimer(0.0f)
, m_fTimeSinceLastLookAt(fwRandom::GetRandomNumberInRange( 0.0f, GetTunables().m_DefaultLookAtThreshold ))
, m_fTimeSinceLastScenarioScan(GetTunables().m_TimeBetweenScenarioScans)
, m_bUseExtendedDistance(false)
, m_bCanSayAudio(false)
, m_bUsingCameraLookAt(false)
, m_bUsingMotionLookAt(false)
, m_fTimeSinceLookInterestPoint(GetTunables().m_MinTimeBeforeSwitchLookAt)
, m_bUsingVehicleJumpLookAt(false)
, m_bUsingRoofLookAt(false)
, m_bCanLookAtMyVehicle(false)
, m_bCheckedMyVehicle(false)
, m_bIgnoreFOV(false)
, m_bWaitingToCrossRoadLookRight(false)
, m_bWaitingToCrossRoadLookLeft(false)
, m_bFoundGreetingTarget(false)
, m_uLastGreetingTime(0)
, m_uLookBackStartTime(0)
, m_bLookBackStarted(false)
, m_bAimToIdleLookAtFinished(false)
, m_bExitCoverLookAtFinished(false)
{
	m_LookRequest.SetHashKey(ATSTRINGHASH("AmbientLookAt", 0x02fc3023b));

	ResetLookAtTarget();
}

CAmbientLookAt::LookAtBlockingState CAmbientLookAt::UpdateLookAt( CPed* pPed, CTaskAmbientClips* pAmbientClipTask, float fTimeStep )
{
	LookAtBlockingState iLookatBlockingState = GetLookAtBlockingState( pPed, pAmbientClipTask, m_bUseExtendedDistance );

	// Update the lookat scenario point scan countdown.
	m_fTimeSinceLastScenarioScan += fTimeStep;

	if( pPed->IsPlayer() )
	{
		// Update looking at stuff
		UpdateLookAtPlayer( pPed, iLookatBlockingState, fTimeStep );
	}
	else
	{
		// Update looking at stuff
		UpdateLookAtAI( pPed, iLookatBlockingState, fTimeStep );
	}
	
	// Update commenting on vehicles
	UpdateCommentOnNiceVehicle(pPed);

	return iLookatBlockingState;
}

void CAmbientLookAt::SetCanLookAtMyVehicle(bool bValue)
{
	if (!bValue && m_bCanLookAtMyVehicle)
	{
		m_fLookAtTimer = 0.0f;
	}
	m_bCanLookAtMyVehicle = bValue;
}

float CAmbientLookAt::ScoreInterestingLookAt( CPed* pPed, const CEntity* pEntity )
{
#if 0
	// Do a LOS check
	Vector3 vLOSStartPos;
	pPed->GetBonePosition(vLOSStartPos, BONETAG_HEAD);
	const CEntity* exclusionList[2] = { pPed, pEntity };
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetStartAndEnd(vLOSStartPos, VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()));
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
	probeDesc.SetExcludeEntities(exclusionList, 2);
	probeDesc.SetContext(WorldProbe::LOS_GeneralAI);
	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
		return -1.0f;
	}
#endif // 0

	float fScore = 0.0f;
	const Tunables& tune = GetTunables();
	switch( pEntity->GetType() )
	{
	case ENTITY_TYPE_PED:
		{
			fScore = tune.m_BasicPedScore;

			const CPed* pOtherPed = static_cast<const CPed*>(pEntity);

			if( ShouldConsiderPlayerForScoringPurposes(*pOtherPed) )
			{
				fScore *= tune.m_PlayerPedModifier;

				Vec3V vPlayerPos = pOtherPed->GetTransform().GetPosition();
				Vec3V vPedPos = pPed->GetTransform().GetPosition();
				ScalarV sDistToPlayer = DistSquared(vPedPos, vPlayerPos);

				// Is the player extremely close?
				if (IsLessThanAll(sDistToPlayer, ScalarV(rage::square(tune.m_ClosePlayerDistanceThreshold))))
				{
					fScore *= tune.m_ClosePlayerModifier;
				}
				else if (IsLessThanAll(sDistToPlayer, ScalarV(rage::square(GetInRangePlayerDistanceThreshold(pOtherPed)))))
				{
					// If the player is close
					fScore *= tune.m_InRangePlayerModifier;

					if(pOtherPed->GetIsDrivingVehicle())
					{
						fScore *= tune.m_InRangeDrivingPlayerModifier;
					}
				}

				// Is the player close and approaching?
				if (IsLessThanAll(sDistToPlayer, ScalarV(rage::square(tune.m_ApproachingPlayerDistanceThreshold))))
				{
					//Is the player moving?
					if (pOtherPed->GetMotionData()->GetCurrentMbrY() > MOVEBLENDRATIO_STILL)
					{
						//Is the player moving towards this ped?
						Vec3V vStraightLine = vPedPos - vPlayerPos;
						vStraightLine.SetZ(ScalarV(V_ZERO));
						vStraightLine = NormalizeFastSafe(vStraightLine, Vec3V(V_ZERO));
						if (Dot(pOtherPed->GetTransform().GetForward(), vStraightLine).Getf() > tune.m_ApproachingPlayerCosineThreshold)
						{
							fScore *= tune.m_ApproachingPlayerModifier;
						}
					}
				}

				// Is the player holding a weapon?
				if(pOtherPed->GetWeaponManager() && pOtherPed->GetWeaponManager()->GetEquippedWeaponInfo() && !pOtherPed->GetWeaponManager()->GetEquippedWeaponInfo()->GetIsUnarmed())
				{
					fScore *= tune.m_HoldingWeaponPlayerModifier;
				}

				// Is the player covered in blood?
				CPedDamageSet * pDamageSet = PEDDAMAGEMANAGER.GetPlayerDamageSet();
				if (pDamageSet && pDamageSet->HasBlood())
				{
					fScore *= tune.m_CoveredInBloodPlayerModifier;
				}
			}

			// Is the player ragdolling
			if(pOtherPed->GetRagdollState() > RAGDOLL_STATE_ANIM_DRIVEN)
			{
				fScore *= tune.m_RagdollingModifier;
			}

			if( pPed->GetPedResetFlag( CPED_RESET_FLAG_IsWalkingRoundPlayer ) && 
				pOtherPed == pPed->GetPedIntelligence()->GetClosestPedInRange() )
			{
				fScore *= tune.m_WalkingRoundPedModifier;
			}

			if( pOtherPed->GetMotionData()->GetIsRunning() || pOtherPed->GetMotionData()->GetIsSprinting() )
			{
				fScore *= tune.m_RunningPedModifier;
			}

			if( pOtherPed->GetPedIntelligence()->IsPedClimbing() || 
				pOtherPed->GetPedIntelligence()->IsPedInAir() )
			{
				fScore *= tune.m_ClimbingOrJumpingPedModifier;
			}

			if( pOtherPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT) || 
				pOtherPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_PLAYER_WEAPON) || 
				pOtherPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_MELEE) )
			{
				fScore *= tune.m_FightingModifier;
			}

			if( pOtherPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE) )
			{
				CVehicle* pVehicle = (CVehicle*)pOtherPed->GetPedIntelligence()->GetQueriableInterface()->GetTargetForTaskType(CTaskTypes::TASK_ENTER_VEHICLE);
				if( pVehicle && (pVehicle == pPed->GetMyVehicle() ||
					( pVehicle->GetDriver() && pOtherPed->IsPlayer()) ) )
				{
					fScore *= tune.m_JackingModifier;
				}

				s32 sCurrentTaskState = pOtherPed->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_ENTER_VEHICLE);
				if(sCurrentTaskState == CTaskEnterVehicle::State_PullUpBike || sCurrentTaskState == CTaskEnterVehicle::State_PickUpBike)
				{
					fScore *= tune.m_PickingUpBikeModifier;
				}
			}

			//Check if the ped is hanging around our vehicle.
			if(pPed->GetIsInVehicle() && pOtherPed->GetIsOnFoot())
			{
				spdAABB bbox = pPed->GetMyVehicle()->GetBaseModelInfo()->GetBoundingBox();
				Vec3V vPos = pPed->GetMyVehicle()->GetTransform().UnTransform(pOtherPed->GetTransform().GetPosition());
				ScalarV scDistSq = bbox.DistanceToPointSquared(vPos);
				static dev_float s_fMaxDist = 2.0f;
				ScalarV scMaxDistSq = ScalarVFromF32(square(s_fMaxDist));
				if(IsLessThanAll(scDistSq, scMaxDistSq))
				{
					fScore *= tune.m_HangingAroundVehicleModifier;
				}
			}

			if ( pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_USE_SCENARIO))
			{
				if (pOtherPed->IsPlayer() && pPed->IsGangPed())
				{
					fScore *= tune.m_GangScenarioPedToPlayerModifier;
				}
				else if (pOtherPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_USE_SCENARIO))
				{
					// check if within the other ped is within the field of view
					if (pPed->GetPedIntelligence()->GetPedPerception().ComputeFOVVisibility(pOtherPed, NULL))
					{
						fScore *= tune.m_ScenarioToScenarioPedModifier;
					}
				}
			}

			fScore *= UpdateJeerAtHotPed(pPed,pOtherPed);

			//High/Low importance modifiers
			s32 scenarioType = pOtherPed->GetPedIntelligence()->GetQueriableInterface()->GetRunningScenarioType();
			if (scenarioType != Scenario_Invalid)
			{
				const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(scenarioType);
				switch (pScenarioInfo->GetLookAtImportance())
				{
				case LookAtImportanceHigh:
					fScore *= tune.m_HighImportanceModifier;
					break;
				case LookAtImportanceMedium:
					fScore *= tune.m_MediumImportanceModifier;
					break;
				case LookAtImportanceLow:
					fScore *= tune.m_LowImportanceModifier;
					break;
				}
			}
		}
		break;

	case ENTITY_TYPE_VEHICLE:
		{
			fScore = tune.m_BasicVehicleScore;

			const CVehicle* pVehicle = static_cast<const CVehicle*>(pEntity);

			// Ignore your own vehicle
			if( pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pVehicle == pPed->GetMyVehicle() )
			{
				return -1.0f;
			}

			if( pVehicle->GetDriver() && pVehicle->GetDriver()->IsPlayer() )
			{
				fScore *= tune.m_PlayerPedModifier;
			}

			if( ( pVehicle->GetVelocity().Mag2() > rage::square( tune.m_RecklessCarSpeedMin ) ) ||
				pVehicle->m_nVehicleFlags.bMadDriver )
			{
				float fModifier = 1.0f - ( ( tune.m_RecklessCarSpeedMax - rage::Min( pVehicle->GetVelocity().Mag(), tune.m_RecklessCarSpeedMax ) ) / ( tune.m_RecklessCarSpeedMax - tune.m_RecklessCarSpeedMin ) );
				fScore *= 1.0f + ( ( tune.m_RecklessCarModifier - 1) * fModifier);
			}

			// Sirens or horns more interesting
			if( pVehicle->m_nVehicleFlags.GetIsSirenOn() || pVehicle->IsHornOn() )
			{
				fScore *= tune.m_CarSirenModifier;
			}
		}
		break;

	case ENTITY_TYPE_OBJECT:
		if( !pEntity->IsCollisionEnabled() )
		{
			return -1.0f;
		}
		else
		{
			fScore = tune.m_BasicObjectScore;
		}
		break;

	default:
		// Could be a cut scene object
		return -1.0f;
	}

	if(IsLessThanAll(Dot( (pEntity->GetTransform().GetPosition() - pPed->GetTransform().GetPosition()), pPed->GetTransform().GetB()), ScalarV(V_ZERO)) )
	{
		fScore *= tune.m_BehindPedModifier;
	}

	return fScore;
}

float CAmbientLookAt::ScoreInterestingLookAtPlayer( CPed* pPlayerPed, const CEntity* pEntity )
{
	const Tunables& tune = GetTunables();

	// Don't look at his own car.
	if(pEntity == pPlayerPed->GetMyVehicle())
	{
		return -1.0f;
	}

	// Scale the maximum pick angle cone based on speed
	float fCurrentSpeed = GetMovingSpeed(pPlayerPed);
	float fMaxPickAngle = tune.m_MaxAnglePickPOI - fCurrentSpeed/tune.m_SpeedForNarrowestAnglePickPOI*(tune.m_MaxAnglePickPOI - tune.m_MinAnglePickPOI);
	Vec3V vPlayerToEntity(Subtract(pEntity->GetTransform().GetPosition(), pPlayerPed->GetTransform().GetPosition()));
	vPlayerToEntity = Normalize(vPlayerToEntity);

	if (IsLessThanAll(Dot(vPlayerToEntity, pPlayerPed->GetTransform().GetB()), ScalarV(rage::Cosf(DtoR*fMaxPickAngle))))
	{
		return -1.0f;
	}

	// Ignore the entity if it's outside of the maximum pitching angle. Only for the entities higher than the player.
	if(vPlayerToEntity.GetZf() > 0.0f)
	{
		vPlayerToEntity.SetXf(pPlayerPed->GetTransform().GetB().GetXf());
		vPlayerToEntity = Normalize(vPlayerToEntity);

		if (IsLessThanAll(Dot(vPlayerToEntity, pPlayerPed->GetTransform().GetB()), ScalarV(rage::Cosf(DtoR*tune.m_MaxPitchingAnglePickPOI))))
		{
			return -1.0f;
		}
	}

	// Do a LOS check
	Vector3 vLOSStartPos;
	pPlayerPed->GetBonePosition(vLOSStartPos, BONETAG_HEAD);
	const CEntity* exclusionList[2] = { pPlayerPed, pEntity };
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetStartAndEnd(vLOSStartPos, VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()));
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
	probeDesc.SetExcludeEntities(exclusionList, 2, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
	probeDesc.SetContext(WorldProbe::LOS_GeneralAI);
	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
		return -1.0f;
	}

	float fScore = 0.0f;

	bool bPlayerPissedOff = pPlayerPed->GetPlayerInfo()->PlayerIsPissedOff();

	switch( pEntity->GetType() )
	{
	case ENTITY_TYPE_PED:
		{
			const CPed* pOtherPed = static_cast<const CPed*>(pEntity);

			if( !bPlayerPissedOff )
			{
				fScore = tune.m_BasicPedScore;
			}

			if( !bPlayerPissedOff && ( pOtherPed->GetMotionData()->GetIsRunning() || pOtherPed->GetMotionData()->GetIsSprinting() ) )
			{
				fScore *= tune.m_RunningPedModifier;
			}

			if( !bPlayerPissedOff && (pOtherPed->GetPedIntelligence()->IsPedClimbing() || pOtherPed->GetPedIntelligence()->IsPedInAir()) )
			{
				fScore *= tune.m_ClimbingOrJumpingPedModifier;
			}

			if( pOtherPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT) || 
				pOtherPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_PLAYER_WEAPON) || 
				pOtherPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_MELEE) )
			{
				fScore *= tune.m_FightingModifier;
			}

			if( pOtherPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE) )
			{
				CVehicle* pVehicle = static_cast<CVehicle*>(pOtherPed->GetPedIntelligence()->GetQueriableInterface()->GetTargetForTaskType(CTaskTypes::TASK_ENTER_VEHICLE));
				if( pVehicle && (pVehicle == pPlayerPed->GetMyVehicle() ||
					( pVehicle->GetDriver() && pOtherPed->IsPlayer()) ) )
				{
					fScore *= tune.m_JackingModifier;
				}
			}

			// Player stares at cops
			if( pOtherPed->GetPedType() == PEDTYPE_COP )
			{
				fScore *= tune.m_PlayerCopModifier;
			}

			// Player is currently heterosexual and only likes peds whose sexiness is 5/5
			if( !bPlayerPissedOff && ((pPlayerPed->IsMale() != pOtherPed->IsMale()) && pOtherPed->CheckSexinessFlags(SF_HOT_PERSON)) )
			{
				fScore *= tune.m_PlayerSexyPedModifier;
			}

			// Player looks at gangs hassling him
			if( pOtherPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_AGITATED) )
			{
				fScore *= tune.m_PlayerHasslingModifier;
			}
		}
		break;

	case ENTITY_TYPE_VEHICLE:
		{
			const CVehicle* pVehicle = static_cast<const CVehicle*>(pEntity);

			// Ignore your own vehicle
			if( pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pVehicle == pPlayerPed->GetMyVehicle() )
			{
				return -1.0f;
			}

			if( !bPlayerPissedOff )
			{
				fScore = tune.m_BasicVehicleScore;
			}

			// Reckless cars and crazy drivers
			if( ( pVehicle->GetVelocity().Mag2() > rage::square( tune.m_RecklessCarSpeedMin ) ) || pVehicle->m_nVehicleFlags.bMadDriver )
			{
				fScore *= tune.m_RecklessCarModifier;
			}

			// Sirens or horns more interesting
			if( pVehicle->m_nVehicleFlags.GetIsSirenOn() || pVehicle->IsHornOn() )
			{
				fScore *= tune.m_CarSirenModifier;
			}

			// Swanky cars
			CVehicleModelInfo* pVehicleInfo = pVehicle->GetVehicleModelInfo();
			if( !bPlayerPissedOff && pVehicleInfo && pVehicleInfo->GetVehicleSwankness() >= tune.m_PlayerSwankyCarMin && pVehicleInfo->GetVehicleSwankness() <= tune.m_PlayerSwankyCarMax)
			{
				fScore *= tune.m_PlayerSwankyCarModifier;
			}

			if( pVehicle->GetDriver() && pVehicle->GetDriver()->GetPedType() == PEDTYPE_COP )
			{
				fScore *= tune.m_PlayerCopCarModifier;
			}
		}
		break;

	default:
		// Could be a cut scene object
		return -1.0f;
	}

	return fScore;
}

bool CAmbientLookAt::DoBlendHelperFlagsAllowLookAt(const CPed* pPed)
{
	taskAssert(pPed);

	int iInvalidFlags = APF_BLOCK_IK|APF_BLOCK_HEAD_IK; 
	if (pPed->GetMovePed().GetBlendHelper().IsFlagSet(iInvalidFlags))
	{
		return false;
	}
	
	return true;
}

bool CAmbientLookAt::IsLookAtAllowedForPed(const CPed* pPed, const CTaskAmbientClips* pAmbientClipTask, bool bUseExtendedDistance)
{
	if (!pPed)
	{
		return false;
	}

	return GetLookAtBlockingState(pPed, pAmbientClipTask, bUseExtendedDistance) == LBS_NoBlocking;
}

CAmbientLookAt::LookAtBlockingState CAmbientLookAt::GetLookAtBlockingState(const CPed* pPed, const CTaskAmbientClips* pAmbientClipTask, bool bUseExtendedDistance)
{
	// Disable look ats on mission characters - these are controlled by script
	if( pPed->PopTypeIsMission() && !pPed->IsLocalPlayer() )
	{
		return LBS_BlockLookAts;
	}

	if (pAmbientClipTask && pAmbientClipTask->IsInConversation())
	{
		//Vector3 WorldCoors = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) + Vector3(0,0,1.0f);
		//grcDebugDraw::Text( WorldCoors, CRGBA(64,255,64,255), 0, grcDebugDraw::GetScreenSpaceTextHeight(), "In Conversation");

		return LBS_BlockLookAtsInConversation;
	}

	// Disable ambient lookAts during flee. Look IK is handled in the task.
	if (!pPed->IsLocalPlayer() && pPed->GetPedResetFlag(CPED_RESET_FLAG_MoveBlend_bFleeTaskRunning))
	{
		return LBS_BlockLookAts;
	}

	// Disable/Enable ambient look ats depending on the center of the world
	Vector3 vCentreOfWorld = CGameWorld::FindLocalPlayerCentreOfWorld();

	float fMaxDistance = bUseExtendedDistance ? sm_Tunables.m_ExtendedDistanceFromWorldCenter : sm_Tunables.m_DefaultDistanceFromWorldCenter;
	if( DistSquared(RCC_VEC3V(vCentreOfWorld), pPed->GetTransform().GetPosition()).Getf() > rage::square(fMaxDistance))
	{
		return LBS_BlockLookAts;
	}

	// Block look ats if this is the player and control is disabled
	// This can interfere with scripted sequences where audio is played.
	if( pPed->IsLocalPlayer() && pPed->GetPlayerInfo())
	{
		if( pPed->GetPlayerInfo()->AreControlsDisabled() )
		{
			return LBS_BlockLookAts;
		}
		if( pPed->GetIsInCover())
		{
			const CTaskInCover* pInCoverTask = static_cast<const CTaskInCover*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER));
			if (pInCoverTask && pInCoverTask->GetState() == CTaskInCover::State_Peeking)
			{
				return LBS_BlockLookAts;
			}
		}
		if( CPhoneMgr::CamGetState() )
		{
			return LBS_BlockLookAts;
		}
	}

	if( CTaskAmbientClips::BLOCK_ALL_AMBIENT_IDLES )
	{
		return LBS_BlockLookAts;
	}

	const CTask* pActiveTask = pPed->GetPedIntelligence()->GetTaskActive();
	if( pActiveTask )
	{
		if( !DoBlendHelperFlagsAllowLookAt( pPed ) )
		{
			return LBS_BlockLookAts;
		}
	}

	return LBS_NoBlocking;
}

bool CAmbientLookAt::ShouldConsiderPlayerForScoringPurposes(const CPed& rPed)
{
	//Check if the ped is a player.
	if(rPed.IsPlayer())
	{
		return true;
	}

	//Check if the model info is valid.
	const CPedModelInfo* pPedModelInfo = rPed.GetPedModelInfo();
	if(pPedModelInfo)
	{
		//Grab the model name.
		atHashWithStringNotFinal hModelName = atHashWithStringNotFinal(pPedModelInfo->GetModelNameHash());

		//Iterate over the model names.
		const atArray<atHashWithStringNotFinal>& rModelNames = sm_Tunables.m_ModelNamesToConsiderPlayersForScoringPurposes;
		for(int i = 0; i < rModelNames.GetCount(); ++i)
		{
			//Check if the model name matches.
			if(hModelName == rModelNames[i])
			{
				return true;
			}
		}
	}

	return false;
}

bool CAmbientLookAt::LookAtDestination( CPed* pPed )
{
	if( !gVpMan.GetCurrentViewport() )
	{
		return false;
	}

	// Don't head track if the ped isn't on screen
	if( !pPed->GetIsVisibleInSomeViewportThisFrame() )
	{
		return false;
	}

	Vector3 vDestination;

	// If driving, look towards the next target node
	if( pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle() && pPed->GetMyVehicle()->IsDriver(pPed) )
	{
		//first, see if this guy's steering around any entity.
		//if so, look at that.
		//we do that here to avoid getting involved with the system for
		//scoring lookat targets and waiting a certain amount of time
		//before changing views.
		//const u32 hashKey = ATSTRINGHASH("DrivingDestinationLookat", 0x025255393);

		const CPhysical* pSteeringAroundEntity = pPed->GetMyVehicle()->GetIntelligence()->GetEntitySteeringAround();
		if (pSteeringAroundEntity)
		{
			m_LookRequest.SetTargetEntity(NULL);
			m_LookRequest.SetTargetBoneTag(BONETAG_INVALID);
			m_LookRequest.SetTargetOffset(pSteeringAroundEntity->GetTransform().GetPosition());
			m_LookRequest.SetFlags((pPed->GetScenarioType(*pPed, true) != Scenario_Invalid) ? LOOKIK_USE_ANIM_TAGS : 0);
			m_LookRequest.SetHoldTime(100);

			pPed->GetIkManager().Request(m_LookRequest);
			return true;
		}

		CTaskVehicleGoToPointWithAvoidanceAutomobile* pVehTask = static_cast<CTaskVehicleGoToPointWithAvoidanceAutomobile*>(pPed->GetMyVehicle()->GetIntelligence()->GetTaskManager()->FindTaskByTypeActive(VEHICLE_TASK_TREE_PRIMARY, CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE));
		if (pVehTask)
		{
			pVehTask->FindTargetCoors(pPed->GetMyVehicle(), vDestination);
			//raise it up a bit
			vDestination.z += 1.0f;

			m_LookRequest.SetTargetEntity(NULL);
			m_LookRequest.SetTargetBoneTag(BONETAG_INVALID);
			m_LookRequest.SetTargetOffset(RC_VEC3V(vDestination));
			m_LookRequest.SetFlags((pPed->GetScenarioType(*pPed, true) != Scenario_Invalid) ? LOOKIK_USE_ANIM_TAGS : 0);
			m_LookRequest.SetHoldTime(100);

			pPed->GetIkManager().Request(m_LookRequest);
			return true;
		}
	}

	// Check to see if there is a valid cover point in the correct direction and along the path
	CTaskMoveFollowNavMesh* pNavmeshTask = static_cast<CTaskMoveFollowNavMesh*>(pPed->GetPedIntelligence()->FindTaskActiveMovementByType( CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH ) );
	if( pNavmeshTask )
	{
		Vector3 vStartPos, vEndPos;
		// Look at the next path we're heading towards
		if( pNavmeshTask->GetThisWaypointLine( pPed, vStartPos, vEndPos, 1 ) )
		{
			vDestination = vEndPos + Vector3(0.0f, 0.0f, 1.5f); // bring it up to head height

			m_LookRequest.SetTargetEntity(NULL);
			m_LookRequest.SetTargetBoneTag(BONETAG_INVALID);
			m_LookRequest.SetTargetOffset(RC_VEC3V(vDestination));
			m_LookRequest.SetFlags((pPed->GetScenarioType(*pPed, true) != Scenario_Invalid) ? LOOKIK_USE_ANIM_TAGS | LOOKIK_USE_FOV : LOOKIK_USE_FOV);
			m_LookRequest.SetHoldTime(100);

			pPed->GetIkManager().Request(m_LookRequest);
			return true;
		}
	}
	else
	{
		// Look at the wandering tasks current target
		CTaskMoveWander* pWanderMoveTask = static_cast<CTaskMoveWander*>(pPed->GetPedIntelligence()->FindTaskActiveMovementByType( CTaskTypes::TASK_MOVE_WANDER ) );
		if( pWanderMoveTask )
		{
			if( pWanderMoveTask->GetWanderTarget(vDestination) )
			{
				vDestination.z += 1.0f;
				// Make sure the target isn't too close
				const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
				if( vDestination.Dist2(vPedPosition) > rage::square(2.0f) )
				{
					// Check the dot product to the destination, if its close to the peds current heading, don't head track.
					Vector3 vToDestination = vDestination - vPedPosition;
					vToDestination.z = 0.0f;
					vToDestination.Normalize();

					const float fDot = DotProduct(vToDestination, VEC3V_TO_VECTOR3(pPed->GetTransform().GetB()));
					if( fDot < 0.9659f && fDot > 0.0f )
					{
						m_LookRequest.SetTargetEntity(NULL);
						m_LookRequest.SetTargetBoneTag(BONETAG_INVALID);
						m_LookRequest.SetTargetOffset(RC_VEC3V(vDestination));
						m_LookRequest.SetFlags((pPed->GetScenarioType(*pPed, true) != Scenario_Invalid) ? LOOKIK_USE_ANIM_TAGS | LOOKIK_USE_FOV : LOOKIK_USE_FOV);
						m_LookRequest.SetHoldTime(100);

						pPed->GetIkManager().Request(m_LookRequest);
						return true;
					}
				}
			}
		}
	}

	return false;
}

void CAmbientLookAt::PickWaitingToCrossRoadLookAt(CPed* pPed, const Vector3& vEndPos)
{
	// 30% chance to look at where the ped's going.
	if(fwRandom::GetRandomNumberInRange(0.0f, 3.0f) < 1.0f)
	{
		m_CurrentLookAtTarget.Clear();
		m_CurrentLookAtTarget.SetPosition(vEndPos);

	}
	else // check left or right
	{
		// Pick left/right direction to look at.
		if(m_bWaitingToCrossRoadLookLeft)
		{
			m_bWaitingToCrossRoadLookRight = true;
			m_bWaitingToCrossRoadLookLeft = false;
		}
		else if(m_bWaitingToCrossRoadLookRight)
		{
			m_bWaitingToCrossRoadLookRight = false;
			m_bWaitingToCrossRoadLookLeft = true;
		}
		else
		{
			if(fwRandom::GetRandomTrueFalse())
			{
				m_bWaitingToCrossRoadLookLeft = true;
			}
			else
			{
				m_bWaitingToCrossRoadLookRight = true;
			}
		}

		Mat34V matPed = pPed->GetMatrix();
		Vector3 vLookingDirection = VEC3V_TO_VECTOR3(matPed.a());
		if(m_bWaitingToCrossRoadLookLeft)
		{
			vLookingDirection *= -1.0f;
		}
		m_CurrentLookAtTarget.Clear();
		m_CurrentLookAtTarget.SetPosition(VEC3V_TO_VECTOR3(matPed.d()) + vLookingDirection * fwRandom::GetRandomNumberInRange(50.0f, 100.0f) + VEC3V_TO_VECTOR3(matPed.b()) * fwRandom::GetRandomNumberInRange(50.0f, 100.0f));
	}

	m_fLookAtTimer = fwRandom::GetRandomNumberInRange(GetTunables().m_AITimeWaitingToCrossRoadMin, GetTunables().m_AITimeWaitingToCrossRoadMax);
}

void CAmbientLookAt::ResetLookAtTarget()
{
	m_CurrentLookAtTarget.Clear();

	m_LookRequest.SetTurnRate(LOOKIK_TURN_RATE_SLOW);
	m_LookRequest.SetRequestPriority(CIkManager::IK_LOOKAT_VERY_LOW);

	// Enable neck component by default to go along with head and eyes
	m_LookRequest.SetRotLimNeck(LOOKIK_ROT_ANGLE_YAW, LOOKIK_ROT_LIM_WIDE);
	m_LookRequest.SetRotLimNeck(LOOKIK_ROT_ANGLE_PITCH, LOOKIK_ROT_LIM_NARROW);

	// Use facing direction mode
	m_LookRequest.SetRefDirHead(LOOKIK_REF_DIR_FACING);
	m_LookRequest.SetRefDirNeck(LOOKIK_REF_DIR_FACING);

	m_LookRequest.SetRotLimTorso(LOOKIK_ROT_ANGLE_YAW, LOOKIK_ROT_LIM_OFF);
	m_LookRequest.SetArmCompensation(LOOKIK_ARM_LEFT, LOOKIK_ARM_COMP_OFF);
	m_LookRequest.SetArmCompensation(LOOKIK_ARM_RIGHT, LOOKIK_ARM_COMP_OFF);

	m_LookRequest.SetBlendOutRate(LOOKIK_BLEND_RATE_NORMAL);
}

void CAmbientLookAt::UpdateLookAtAI( CPed* pPed, LookAtBlockingState iLookatBlockingState, float fTimeStep )
{
	if (pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodMotionTask))
	{
		return;
	}

	const Tunables& tune = GetTunables();

	// Try to look at the car jacker.
	LookAtCarJacker(pPed);

	// Force to look at the nearby player if the conditions are met.
	// Still allow to look at the nearby player in a conversation.
	if(m_CurrentLookAtTarget.GetIsValid() || iLookatBlockingState == LBS_BlockLookAtsInConversation)
	{
		CPed* pPlayerPedToLook = ShouldCheckNearbyPlayer(pPed);
		if(pPlayerPedToLook)
		{
			m_CurrentLookAtTarget.Clear();
			m_CurrentLookAtTarget.SetEntity(pPlayerPedToLook);
			m_fLookAtTimer = 3.0f;
		}
	}

	// If no clip is playing randomly choose to do some head tracking
	if( iLookatBlockingState == LBS_NoBlocking &&
		!m_CurrentLookAtTarget.GetIsValid()  &&
		!pPed->IsPlayer() )
	{
		if( m_fLookAtTimer > 0.0f )
		{
			m_fLookAtTimer -= fTimeStep;
		}

		m_fTimeSinceLastLookAt = rage::Min( m_fTimeSinceLastLookAt + fTimeStep, tune.m_DefaultLookAtThreshold );

		if( m_fLookAtTimer <= 0.0f || pPed->GetPedResetFlag( CPED_RESET_FLAG_IsWalkingRoundPlayer ))
		{
			if( PickInterestingLookAt( pPed, false ) )
			{
				taskAssertf(m_CurrentLookAtTarget.GetIsValid(), "PickInterestingLookAt succeeded but target not set");
				m_fTimeSinceLastLookAt = 0.0f;
				const CEntity* pNewEntityForHistory = m_CurrentLookAtTarget.GetEntity();
				if (pNewEntityForHistory)
				{
					m_LookAtHistory.AddHistory(pNewEntityForHistory);
				}
			}
			else
			{
				const CTaskMoveCrossRoadAtTrafficLights* pTask = static_cast<CTaskMoveCrossRoadAtTrafficLights *>(
					pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_CROSS_ROAD_AT_TRAFFIC_LIGHTS));
				if(pTask && pTask->IsWaitingToCrossRoad())
				{
					//Displayf("%p is waiting for lights to change.", pPed);
					PickWaitingToCrossRoadLookAt(pPed, pTask->GetEndPos());
					m_LookRequest.SetTurnRate(fwRandom::GetRandomTrueFalse() ? LOOKIK_TURN_RATE_NORMAL : LOOKIK_TURN_RATE_FAST);
					m_LookRequest.SetRefDirTorso(LOOKIK_REF_DIR_FACING);
					m_LookRequest.SetRotLimTorso(LOOKIK_ROT_ANGLE_YAW, LOOKIK_ROT_LIM_WIDE);
					m_LookRequest.SetArmCompensation(LOOKIK_ARM_LEFT, LOOKIK_ARM_COMP_MED);
					m_LookRequest.SetArmCompensation(LOOKIK_ARM_RIGHT, LOOKIK_ARM_COMP_MED);
				}
				else
				{
					m_fLookAtTimer = fwRandom::GetRandomNumberInRange(tune.m_AITimeBetweenLookAtsFailureMin, tune.m_AITimeBetweenLookAtsFailureMax);
				}
				
			}
		}
		// Otherwise look where we're going
		else
		{
			LookAtDestination( pPed );
		}
	}

	if( m_CurrentLookAtTarget.GetIsValid() )
	{
		if( m_fLookAtTimer > 0.0f )
		{
			m_fLookAtTimer -= fTimeStep;
		}

		// If a target has been specified, continually look at it
		if( m_SpecifiedLookAtTarget.GetIsValid() )
		{
			m_fLookAtTimer = 0.5f;
		}

		if( iLookatBlockingState == LBS_BlockLookAts )
		{
			ResetLookAtTarget();
			m_fLookAtTimer = 0.0f;
		}
		else
		{
			u32 lookTime = 100;

			if(pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodTimesliceIntelligenceUpdate))
			{
				// MAGIC! 1200 is for conversion to ms, plus 20% to account for variability in the time steps.
				lookTime += (int)(fTimeStep*1200.0f);
			}

			bool bRequest = false;

			if( m_CurrentLookAtTarget.GetEntity() )
			{
				// If we are looking at a ped look at their head (not their position as this is roughly their crotch)
				m_LookRequest.SetTargetEntity(m_CurrentLookAtTarget.GetEntity());
				m_LookRequest.SetTargetBoneTag(m_CurrentLookAtTarget.GetEntity()->GetIsTypePed() ? BONETAG_HEAD : BONETAG_INVALID);
				m_LookRequest.SetTargetOffset(Vec3V(V_ZERO));
				
				// If ignore FOV when looking at the player
				if(m_CurrentLookAtTarget.GetEntity()->GetIsTypePed())
				{
					CPed* pPlayerPed = (CPed*)m_CurrentLookAtTarget.GetEntity();
					if(pPlayerPed->IsAPlayerPed())
					{
						m_bIgnoreFOV = ShouldIgnoreFOVWhenLookingAtPlayer(pPed, pPlayerPed);
					}
#if FPS_MODE_SUPPORTED
					// To look at the camera instead of head bone in first person because the first person camera is placed slightly above head.
					if(pPlayerPed->IsLocalPlayer())
					{
						camBaseCamera* pCamera = (camBaseCamera*)camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();
						if(!pCamera)
						{
							pCamera = (camBaseCamera*)camInterface::GetGameplayDirector().GetFirstPersonVehicleCamera();
						}
						if(pCamera)
						{
							Matrix34 matCamera;
							pCamera->GetObjectSpaceCameraMatrix(pPlayerPed, matCamera);
							m_LookRequest.SetTargetBoneTag(BONETAG_INVALID);
							m_LookRequest.SetTargetOffset(VECTOR3_TO_VEC3V(matCamera.d));
						}
					}
#endif	// FPS_MODE_SUPPORTED
				}

				bRequest = true;
			}
			else if( m_CurrentLookAtTarget.GetMode() == CAITarget::Mode_WorldSpacePosition )
			{
				Vector3 vLookatPosition(Vector3::ZeroType);
				m_CurrentLookAtTarget.GetPosition(vLookatPosition);

				m_LookRequest.SetTargetEntity(NULL);
				m_LookRequest.SetTargetBoneTag(BONETAG_INVALID);
				m_LookRequest.SetTargetOffset(RC_VEC3V(vLookatPosition));

				bRequest = true;
			}

			// Scenario peds use anim tags directly and peds in vehicles ignore FOV checks
			// For now, vehicle scenario peds won't use tags.
			const bool bInVehicle = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle);
			u32 useTag = !bInVehicle && (pPed->GetScenarioType(*pPed, true) != Scenario_Invalid) ? LOOKIK_USE_ANIM_TAGS : 0;
			u32 useFOV = !m_bIgnoreFOV && !bInVehicle && !ShouldIgnoreFovDueToAmbientLookAtTask(*pPed) ? LOOKIK_USE_FOV : 0;

			m_LookRequest.SetFlags(useTag | useFOV);
			m_LookRequest.SetHoldTime((u16)lookTime);

			if( bRequest )
			{
				pPed->GetIkManager().Request(m_LookRequest);
			}

			if( m_fLookAtTimer <= 0.0f )
			{
				ResetLookAtTarget();
			}
		}
	}
}

void CAmbientLookAt::UpdateLookAtPlayer( CPed* pPlayerPed, LookAtBlockingState iLookatBlockingState, float fTimeStep )
{
	if( pPlayerPed->GetMotionData()->GetIsStill() )
	{
		const CCollisionRecord* pPedRecord = pPlayerPed->GetFrameCollisionHistory()->GetMostSignificantCollisionRecordOfType( ENTITY_TYPE_PED );

		if(pPedRecord) 
		{
			m_LookRequest.SetTargetEntity(pPedRecord->m_pRegdCollisionEntity.Get());
			m_LookRequest.SetTargetBoneTag(BONETAG_HEAD);
			m_LookRequest.SetTargetOffset(Vec3V(V_ZERO));
			m_LookRequest.SetFlags(0);
			m_LookRequest.SetHoldTime(1000);

			pPlayerPed->GetIkManager().Request(m_LookRequest);
			return;
		}
	}

	// Disable look at when driving a sports bike at full speed.
	bool bOnSportsBike = false;
	if(pPlayerPed->GetIsDrivingVehicle() && pPlayerPed->GetMyVehicle()->GetLayoutInfo())
	{
		atHashWithStringNotFinal layoutInfoHash = pPlayerPed->GetMyVehicle()->GetLayoutInfo()->GetName();
		if(layoutInfoHash == ATSTRINGHASH("LAYOUT_BIKE_SPORT", 0xC8EC6158) || layoutInfoHash == ATSTRINGHASH("LAYOUT_BIKE_SPORT_BATI", 0x472B716C))
		{
			bOnSportsBike = true;
			static const float fNoLookAtSpeedThreshold = 20.0f;
			Vector3 vVelocity = pPlayerPed->GetMyVehicle()->GetVelocity();
			if(vVelocity.Mag2() > fNoLookAtSpeedThreshold * fNoLookAtSpeedThreshold)
			{
				return;
			}
		}
	}

	// Disable look at for very narrow cars to avoid the head clipping through the glass
	// Hack for B*3561388. This needs to be moved to data, so narrow cars avoid using a wide yaw rotation
	// otherwise the head will clip through the cars window
	const bool bDrivingNarrowVehicle = pPlayerPed->GetIsDrivingVehicle() && MI_CAR_VAGNER.IsValid() && MI_CAR_VAGNER ==  pPlayerPed->GetMyVehicle()->GetModelIndex();
	if( bDrivingNarrowVehicle )
		return;

	m_bUsingRoofLookAt = false;
	bool bAimToIdle = false;	// if using the aim to idle transition look at
	bool bExitingCover = false;	// if using exiting cover look at

	static const u32 uLookAtMyVehicleHash = ATSTRINGHASH("LookAtMyVehicle", 0x79037202);

	const Tunables& tune = GetTunables();

	m_fTimeSinceLookInterestPoint += fTimeStep;

	if( iLookatBlockingState == LBS_NoBlocking )
	{
		m_bUsingRoofLookAt = ConvertibleRoofOpenCloseLookAtPlayer(pPlayerPed);
		
		if(m_bUsingRoofLookAt)
		{
			m_bUsingVehicleJumpLookAt = false;
			m_bUsingMotionLookAt = false;
			m_bUsingCameraLookAt = false;
		}
		else
		{
			bool isLookingAround = false;
			bool bLookingBehind = false;
			// always checking if the player is looking around manually with stick
			const CControl *pControl = pPlayerPed->GetControlFromPlayer();
			if(pControl)
			{
				const ioValue& headingControl	= pControl->GetPedLookLeftRight();
				const ioValue& pitchControl		= pControl->GetPedLookUpDown();
				isLookingAround		= !IsNearZero(headingControl.GetUnboundNorm(), SMALL_FLOAT) ||
					!IsNearZero(pitchControl.GetUnboundNorm(), SMALL_FLOAT);
				bLookingBehind = pControl->GetPedLookBehind().IsDown() && !pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UsingCoverPoint);

				if(!m_bAimToIdleLookAtFinished)
				{
					if(fwTimer::GetTimeInMilliseconds() - pPlayerPed->GetPlayerInfo()->m_uLastTimeStopAiming < tune.m_uAimToIdleLookAtTime)
					{
						bAimToIdle = true;
					}
					else
					{
						m_bAimToIdleLookAtFinished = true;
					}
				}
				else if(!m_bExitCoverLookAtFinished)
				{
					const CTaskPlayerOnFoot* pPlayerOnFootTask	= static_cast<const CTaskPlayerOnFoot*>(pPlayerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_ON_FOOT));
					if(pPlayerOnFootTask)
					{
						if(	fwTimer::GetTimeInMilliseconds() - pPlayerOnFootTask->GetLastTimeInCoverTask()  < tune.m_uExitingCoverLookAtTime)
						{
							bExitingCover = true;
						}
						else
						{
							m_bExitCoverLookAtFinished = true;
						}
					}
					else
					{
						m_bExitCoverLookAtFinished = true;
					}

				}
			}

			// Choose camera or motion direction if there is stick input.
			if(bAimToIdle || bExitingCover || m_fTimeSinceLookInterestPoint > tune.m_MinTimeBeforeSwitchLookAt)
			{
				if(bAimToIdle || bExitingCover || isLookingAround || bLookingBehind)
				{
					m_bUsingCameraLookAt = PickCameraLookAtPlayer(pPlayerPed);
					if(m_bUsingCameraLookAt)
					{
						// Check if we should break out of AimToIdle look at.
						if(bAimToIdle)
						{
							UpdateAimToIdleLookAtTarget(pPlayerPed);

							m_bAimToIdleLookAtFinished = ShouldStopAimToIdleLookAt(pPlayerPed);
						}

						m_bUsingMotionLookAt = false;
						pPlayerPed->GetIkManager().AbortLookAt(500, uLookAtMyVehicleHash);
					}
				}
				else
				{
					m_bUsingMotionLookAt = PickMotionLookAtPlayer(pPlayerPed);
					if(m_bUsingMotionLookAt)
					{
						m_bUsingCameraLookAt = false;
						pPlayerPed->GetIkManager().AbortLookAt(500, uLookAtMyVehicleHash);
					}
				}
			}

			m_bUsingVehicleJumpLookAt = false;
			if(!isLookingAround &&  !bLookingBehind)
			{
				if(VehicleJumpLookAtPlayer(pPlayerPed))
				{
					m_bUsingVehicleJumpLookAt = true;
					m_bUsingMotionLookAt = false;
					m_bUsingCameraLookAt = false;
				}
			}
		}
	}

	// Try to look at the car jacker.
	LookAtCarJacker(pPlayerPed);

	// If no clip is playing randomly choose to do some head tracking
	if( iLookatBlockingState == LBS_NoBlocking && !m_bUsingVehicleJumpLookAt &&
		(!m_CurrentLookAtTarget.GetIsValid() || m_bUsingMotionLookAt) )
	{
		if( m_fLookAtTimer > 0.0f )
		{
			m_fLookAtTimer -= fTimeStep;
		}

		m_fTimeSinceLastLookAt = rage::Min( m_fTimeSinceLastLookAt + fTimeStep, tune.m_DefaultLookAtThreshold );
		if( m_fLookAtTimer <= 0.0f )
		{
			bool bUseInterestingLookAt = false;
			bool bLookingAtMyVehicle = false;
			// Camera has the highest priority, then motion direction if turning, then the point of interest, then motion direction if moving. 
			// If no target is found from all above, use camera as default. 
			if(m_bUsingCameraLookAt)
			{
				m_bUsingMotionLookAt = false;
			}
			else if(pPlayerPed->GetAngVelocity().Mag2() > tune.m_MinTurnSpeedMotionOverPOI*tune.m_MinTurnSpeedMotionOverPOI && PickMotionLookAtPlayer(pPlayerPed))
			{
				m_bUsingMotionLookAt = true;
				m_bUsingCameraLookAt = false;
			}
			else if( PickInterestingLookAtPlayer(pPlayerPed, false) )
			{
				taskAssertf( m_CurrentLookAtTarget.GetIsValid(), "PickInterestingLookAtPlayer succeeded but target not set" );
				m_fTimeSinceLastLookAt = 0.0f;
				m_fTimeSinceLookInterestPoint = 0.0f;
				m_bUsingCameraLookAt = false;
				m_bUsingMotionLookAt = false;
				bUseInterestingLookAt = true;
			}
			else if(PickMotionLookAtPlayer(pPlayerPed))
			{
				m_bUsingMotionLookAt = true;
				m_bUsingCameraLookAt = false;
			}
			else if(m_bCanLookAtMyVehicle && !m_bCheckedMyVehicle)
			{
				bLookingAtMyVehicle = LookAtMyVehicle(pPlayerPed);
				if(bLookingAtMyVehicle)
				{
					Vector3 vLookatPosition;
					m_CurrentLookAtTarget.GetPosition(vLookatPosition);
					m_fLookAtTimer = fwRandom::GetRandomNumberInRange(tune.m_PlayerTimeMyVehicleLookAtsMin, tune.m_PlayerTimeMyVehicleLookAtsMax);
					pPlayerPed->GetIkManager().LookAt(
						uLookAtMyVehicleHash,
						NULL,
						(s32)(m_fLookAtTimer*1000),
						BONETAG_INVALID,
						&vLookatPosition,
						LF_WHILE_NOT_IN_FOV);
				}
			}

			// Look at camera direction if there is nothing interesting to look at.
			if(!m_CurrentLookAtTarget.GetIsValid())
			{
				m_bUsingCameraLookAt = PickCameraLookAtPlayer(pPlayerPed);
			}
			else
			{
				m_bCheckedMyVehicle = true;
			}

			if(!bLookingAtMyVehicle)
			{
				float fTimeScale = 1.0f;

				// Scale down look at timer when the player is moving fast. 
				if(bUseInterestingLookAt)
				{
					float fCurrentSpeed = GetMovingSpeed(pPlayerPed);
					const float fMaxScaleDown = 0.5f;
					fTimeScale = Max(1.0f - fCurrentSpeed / tune.m_SpeedForNarrowestAnglePickPOI, fMaxScaleDown);
				}

				m_fLookAtTimer = fwRandom::GetRandomNumberInRange(tune.m_PlayerTimeBetweenLookAtsMin*fTimeScale, tune.m_PlayerTimeBetweenLookAtsMax*fTimeScale);
			}
		}
	}

	if( m_CurrentLookAtTarget.GetIsValid() )
	{
		if( m_fLookAtTimer > 0.0f )
		{
			m_fLookAtTimer -= fTimeStep;
		}

		if( iLookatBlockingState == LBS_BlockLookAts )
		{
			m_CurrentLookAtTarget.Clear();
			m_fLookAtTimer = 0.0f;
		}
		else
		{
			const CEntity* pEntity = m_CurrentLookAtTarget.GetEntity();
			if( pEntity )
			{
				// Check if the target entity is still within the angle limits to avoid head over turn while driving.
				if(pPlayerPed->GetIsDrivingVehicle())
				{
					// Scale the maximum pick angle cone based on speed
					float fCurrentSpeed = GetMovingSpeed(pPlayerPed);
					float fMaxPickAngle = tune.m_MaxAnglePickPOI - fCurrentSpeed/tune.m_SpeedForNarrowestAnglePickPOI*(tune.m_MaxAnglePickPOI - tune.m_MinAnglePickPOI);
					Vec3V vPlayerToEntity(Subtract(pEntity->GetTransform().GetPosition(), pPlayerPed->GetTransform().GetPosition()));
					vPlayerToEntity = Normalize(vPlayerToEntity);

					if (IsLessThanAll(Dot(vPlayerToEntity, pPlayerPed->GetTransform().GetB()), ScalarV(rage::Cosf(DtoR*fMaxPickAngle))))
					{
						m_CurrentLookAtTarget.Clear();
						m_fLookAtTimer = 0.0f;
					}
				}

				if(m_CurrentLookAtTarget.GetIsValid())
				{
					// If we are looking at a ped look at their head (not their position as this is roughly their crotch)
					m_LookRequest.SetTargetEntity(pEntity);
					m_LookRequest.SetTargetBoneTag(pEntity->GetIsTypePed() ? BONETAG_HEAD : BONETAG_INVALID);
					m_LookRequest.SetTargetOffset(Vec3V(V_ZERO));
#if DEBUG_DRAW				
					if(tune.m_PlayerLookAtDebugDraw)
					{
						Vector3 vPlayerHeadPos;
						pPlayerPed->GetBonePosition(vPlayerHeadPos, BONETAG_HEAD);
						grcDebugDraw::Line(vPlayerHeadPos, VEC3V_TO_VECTOR3(m_CurrentLookAtTarget.GetEntity()->GetTransform().GetPosition()), Color_purple1);

						static float fRadius = 0.2f;
						grcDebugDraw::Sphere( VEC3V_TO_VECTOR3(m_CurrentLookAtTarget.GetEntity()->GetTransform().GetPosition()), fRadius, Color_purple);
					}
#endif // DEBUG_DRAW
				}
			}
			else
			{
				// Keep updating camera look at position.
				if(m_bUsingCameraLookAt)
				{
					if(!bAimToIdle)
					{
						m_bUsingCameraLookAt = PickCameraLookAtPlayer(pPlayerPed);
					}
#if DEBUG_DRAW				
					if(tune.m_PlayerLookAtDebugDraw && m_bUsingCameraLookAt)
					{
						const camThirdPersonCamera* pCamera = camInterface::GetGameplayDirector().GetThirdPersonCamera();
						if(pCamera && camInterface::GetGameplayDirector().IsActiveCamera(pCamera))
						{
							Vector3 vLookatPosition(Vector3::ZeroType);
							m_CurrentLookAtTarget.GetPosition(vLookatPosition);
							Vector3 vPlayerHeadPos;
							pPlayerPed->GetBonePosition(vPlayerHeadPos, BONETAG_HEAD);
							grcDebugDraw::Line(vPlayerHeadPos, vLookatPosition, Color_green);
						}
					}
#endif // DEBUG_DRAW
				}
				else if(m_bUsingMotionLookAt) // Keep updating motion look at position
				{
					m_bUsingMotionLookAt = PickMotionLookAtPlayer(pPlayerPed);

#if DEBUG_DRAW				
					if(tune.m_PlayerLookAtDebugDraw && m_bUsingMotionLookAt)
					{
						Vector3 vLookatPosition(Vector3::ZeroType);
						m_CurrentLookAtTarget.GetPosition(vLookatPosition);
						Vector3 vPlayerHeadPos;
						pPlayerPed->GetBonePosition(vPlayerHeadPos, BONETAG_HEAD);
						grcDebugDraw::Line(vPlayerHeadPos, vLookatPosition, Color_blue);
					}
#endif // DEBUG_DRAW

				}
#if DEBUG_DRAW				
				else if(tune.m_PlayerLookAtDebugDraw)
				{
					Vector3 vLookatPosition(Vector3::ZeroType);
					m_CurrentLookAtTarget.GetPosition(vLookatPosition);
					Vector3 vPlayerHeadPos(Vector3::ZeroType);
					pPlayerPed->GetBonePosition(vPlayerHeadPos, BONETAG_HEAD);
					grcDebugDraw::Line(vPlayerHeadPos, vLookatPosition, Color_blue4);
				}
#endif // DEBUG_DRAW

				// The current lookAt target could become invalid after updating motion/camera lookAts.
				if(m_CurrentLookAtTarget.GetIsValid())
				{
					Vector3 vLookatPosition(Vector3::ZeroType);
					if(!m_CurrentLookAtTarget.GetPosition(vLookatPosition))
					{
						Mat34V mtxPlayerPed(pPlayerPed->GetTransform().GetMatrix());
						Vec3V vPosition(AddScaled(mtxPlayerPed.GetCol3(), mtxPlayerPed.GetCol1(), ScalarV(10.0f)));
						vLookatPosition = VEC3V_TO_VECTOR3(vPosition);
					}

					m_LookRequest.SetTargetEntity(NULL);
					m_LookRequest.SetTargetBoneTag(BONETAG_INVALID);
					m_LookRequest.SetTargetOffset(RC_VEC3V(vLookatPosition));

	#if DEBUG_DRAW				
					if(tune.m_PlayerLookAtDebugDraw)
					{
						grcDebugDraw::Sphere( vLookatPosition, 0.05f, Color_red);
					}
	#endif // DEBUG_DRAW

				}
			}

			if(m_CurrentLookAtTarget.GetIsValid())
			{
				m_LookRequest.SetFlags(0);
				m_LookRequest.SetHoldTime(100);

				// Restore to default settings.
				m_LookRequest.SetRotLimNeck(LOOKIK_ROT_ANGLE_YAW, LOOKIK_ROT_LIM_WIDE);
				m_LookRequest.SetRotLimNeck(LOOKIK_ROT_ANGLE_PITCH, LOOKIK_ROT_LIM_NARROW);
				m_LookRequest.SetRotLimHead(LOOKIK_ROT_ANGLE_YAW, LOOKIK_ROT_LIM_WIDE);
				m_LookRequest.SetRotLimHead(LOOKIK_ROT_ANGLE_PITCH, LOOKIK_ROT_LIM_NARROW);
				m_LookRequest.SetRotLimTorso(LOOKIK_ROT_ANGLE_YAW, LOOKIK_ROT_LIM_OFF);
				m_LookRequest.SetBlendOutRate(LOOKIK_BLEND_RATE_NORMAL);

				if(m_bUsingCameraLookAt)
				{
					m_LookRequest.SetTurnRate(tune.m_CameraLookAtTurnRate);
					m_LookRequest.SetBlendInRate(tune.m_CameraLookAtBlendRate);
					m_LookRequest.SetRotLimNeck(LOOKIK_ROT_ANGLE_YAW, tune.m_CameraLookAtRotationLimit);
				}
				else if(m_bUsingMotionLookAt)
				{
					m_LookRequest.SetTurnRate(tune.m_MotionLookAtTurnRate);
					m_LookRequest.SetBlendInRate(tune.m_MotionLookAtBlendRate);
					m_LookRequest.SetRotLimNeck(LOOKIK_ROT_ANGLE_YAW, tune.m_MotionLookAtRotationLimit);
				}
				else if(m_bUsingVehicleJumpLookAt)
				{
					m_LookRequest.SetTurnRate(tune.m_VehicleJumpLookAtTurnRate);
					m_LookRequest.SetBlendInRate(tune.m_VehicleJumpLookAtBlendRate);
					m_LookRequest.SetRotLimNeck(LOOKIK_ROT_ANGLE_YAW, tune.m_POILookAtRotationLimit);
				}
				else
				{
					m_LookRequest.SetTurnRate(tune.m_POILookAtTurnRate);
					m_LookRequest.SetBlendInRate(tune.m_POILookAtBlendRate);
					m_LookRequest.SetRotLimNeck(LOOKIK_ROT_ANGLE_YAW, tune.m_POILookAtRotationLimit);
				}

				// Use slow turn rate in stealth mode.
				if(pPlayerPed->IsUsingStealthMode())
				{
					m_LookRequest.SetTurnRate(LOOKIK_TURN_RATE_SLOW);
				}

				if(bOnSportsBike)
				{
					m_LookRequest.SetRotLimNeck(LOOKIK_ROT_ANGLE_YAW, LOOKIK_ROT_LIM_NARROW);
				}

				if(m_bUsingRoofLookAt)
				{
					m_LookRequest.SetRotLimNeck(LOOKIK_ROT_ANGLE_YAW, LOOKIK_ROT_LIM_WIDEST);
					m_LookRequest.SetRotLimHead(LOOKIK_ROT_ANGLE_YAW, LOOKIK_ROT_LIM_WIDEST);
					m_LookRequest.SetRotLimNeck(LOOKIK_ROT_ANGLE_PITCH, LOOKIK_ROT_LIM_NARROW);
					m_LookRequest.SetRotLimHead(LOOKIK_ROT_ANGLE_PITCH, LOOKIK_ROT_LIM_NARROW);
					m_LookRequest.SetTurnRate(LOOKIK_TURN_RATE_SLOW);
				}
				else if(bAimToIdle || bExitingCover)
				{
					m_LookRequest.SetRotLimNeck(LOOKIK_ROT_ANGLE_YAW, LOOKIK_ROT_LIM_WIDEST);
					m_LookRequest.SetRotLimHead(LOOKIK_ROT_ANGLE_YAW, LOOKIK_ROT_LIM_WIDEST);
					m_LookRequest.SetRotLimTorso(LOOKIK_ROT_ANGLE_YAW, LOOKIK_ROT_LIM_WIDEST);
					m_LookRequest.SetRefDirTorso(LOOKIK_REF_DIR_FACING);
					m_LookRequest.SetBlendOutRate(LOOKIK_BLEND_RATE_FASTEST);
				}

				pPlayerPed->GetIkManager().Request(m_LookRequest);

			}

			if( m_fLookAtTimer <= 0.0f )
			{
				m_CurrentLookAtTarget.Clear();
				m_bUsingCameraLookAt = false;
				m_bUsingMotionLookAt = false;
				m_bUsingVehicleJumpLookAt = false;
			}
		}
	}
}

bool CAmbientLookAt::CanDriverHeadlookAtOtherEntities(CPed* pPed) const
{
	if (!pPed->GetIsDrivingVehicle())
	{
		return true;
	}

	const Tunables& tune = GetTunables();

	Assert(pPed->GetMyVehicle());
	if (pPed->GetMyVehicle()->GetVelocity().Mag2() > tune.m_MaxVelocityForVehicleLookAtSqr)
	{
		return false;
	}

	return true;
}

bool CAmbientLookAt::PickInterestingLookAt( CPed* pPed, bool bPickBest )
{
	// Reset each time to pick up a new interesting point.
	m_bIgnoreFOV = false;

	// Overridden look at point specified
	if( m_SpecifiedLookAtTarget.GetIsValid() )
	{
		m_CurrentLookAtTarget = m_SpecifiedLookAtTarget;
		return true;
	}

	if (!CanDriverHeadlookAtOtherEntities(pPed))
	{
		return false;
	}

	const Tunables& tune = GetTunables();

	// First calculate the threshold of interesting look ats this ped is looking for depending on the last time they looked at anything
	float fThreshHold = tune.m_DefaultLookAtThreshold - m_fTimeSinceLastLookAt;

	CEntity* apEntities[MAX_ENTITIES_TO_LOOKAT];
	float afEntityProbs[MAX_ENTITIES_TO_LOOKAT];
	float fTotalProbability = 0.0f;
	s32 iNoEntities = 0;
	s32 iHighestEntity = 0;

	CEntity* pLookAtTaskTarget = GetAmbientLookAtTaskTarget(*pPed);
	if(pLookAtTaskTarget)
	{
		apEntities[iNoEntities] = pLookAtTaskTarget;
		afEntityProbs[iNoEntities] = 3.0f;
		fTotalProbability += afEntityProbs[iNoEntities];
		++iNoEntities;
	}
	else
	{
		m_bFoundGreetingTarget = false;
		m_LookAtHistory.ProcessHistory();
		CEntityScannerIterator pedList = pPed->GetPedIntelligence()->GetNearbyPeds();
		for( CEntity* pEnt = pedList.GetFirst(); pEnt && iNoEntities < MAX_ENTITIES_TO_LOOKAT; pEnt = pedList.GetNext() )
		{
			CPed* pThisPed = static_cast<CPed*>(pEnt);

			float fScore = ScoreInterestingLookAt( pPed, pThisPed );

			fScore = GetModifiedScoreBasedOnHistory(*pPed, *pEnt, fScore);

			CAmbientAudioExchange exchange;
			if( !m_bFoundGreetingTarget && !ShouldConsiderPlayerForScoringPurposes(*pThisPed) && !pPed->GetIsInVehicle() && CanSayAudioToTarget(*pPed, *pThisPed, exchange))
			{
				// try greeting to non player peds.
				if((m_uLastGreetingTime + tune.m_uAITimeBetweenGreeting) < fwTimer::GetTimeInMilliseconds())
				{
					Vec3V vOtherPedPos = pThisPed->GetTransform().GetPosition();
					Vec3V vPedPos = pPed->GetTransform().GetPosition();
					ScalarV sDistToOtherPed = DistSquared(vPedPos, vOtherPedPos);

					if (IsLessThanAll(sDistToOtherPed, ScalarV(rage::square(tune.m_fAIGreetingDistanceMax))) && IsGreaterThanAll(sDistToOtherPed, ScalarV(rage::square(tune.m_fAIGreetingDistanceMin))))
					{
						fScore *= tune.m_fAIGreetingPedModifier;
						m_bFoundGreetingTarget = true;
						m_uLastGreetingTime = fwTimer::GetTimeInMilliseconds();
					}
				}
			}

			// Only consider if the score reaches the threshold
			if( fScore > fThreshHold )
			{
				apEntities[iNoEntities]	= pThisPed;
				afEntityProbs[iNoEntities]	= fScore;

				if( afEntityProbs[iNoEntities] > afEntityProbs[iHighestEntity] )
				{
					iHighestEntity = iNoEntities;
				}
				fTotalProbability += afEntityProbs[iNoEntities];

				++iNoEntities;
			}
		}

		CEntityScannerIterator vehicleList = pPed->GetPedIntelligence()->GetNearbyVehicles();
		for( CEntity* pEnt = vehicleList.GetFirst(); pEnt && iNoEntities < MAX_ENTITIES_TO_LOOKAT; pEnt = vehicleList.GetNext() )
		{
			CVehicle* pThisVehicle = static_cast<CVehicle*>(pEnt);

			float fScore = ScoreInterestingLookAt( pPed, pThisVehicle );
			
			fScore = GetModifiedScoreBasedOnHistory(*pPed, *pEnt, fScore);

			// Only consider if the score reaches the threshold
			if( fScore > fThreshHold )
			{
				apEntities[iNoEntities]	= pThisVehicle;
				afEntityProbs[iNoEntities]	= fScore;

				if( afEntityProbs[iNoEntities] > afEntityProbs[iHighestEntity] )
				{
					iHighestEntity = iNoEntities;
				}
				fTotalProbability += afEntityProbs[iNoEntities];

				++iNoEntities;
			}
		}
	}

	if( iNoEntities > 0 )
	{
		s32 iPickedEntity = 0;

		if( bPickBest )
		{
			iPickedEntity = iHighestEntity;
		}
		else
		{
			// Pick a random look at
			const float fRandom = fwRandom::GetRandomNumberInRange( 0.0f, fTotalProbability );

			float fProbSum = 0.0f;
			for( s32 i = 0; i < iNoEntities; i++ )
			{
				fProbSum += afEntityProbs[i];
				if( fRandom < fProbSum || ( i == ( iNoEntities - 1 ) ) )
				{
					iPickedEntity = i;
					break;
				}
			}
		}

		CEntity* pPickedEntity = apEntities[iPickedEntity];
		m_CurrentLookAtTarget.SetEntity( pPickedEntity );
		aiAssert(pPickedEntity);

		// Look at entities longer depending on their score
		const float fTimeMultiplier = rage::Max( 1.0f, afEntityProbs[iPickedEntity] * 0.5f );

		m_fLookAtTimer = fwRandom::GetRandomNumberInRange(0.9f, 2.5f) * tune.m_BaseTimeToLook * fTimeMultiplier;

		// AUDIO_OR_GESTURE_TRIGGER(LOOKING_AT_SOMETHING, OCCASIONAL) 
		//		Ped has just picked something to look at.
		//		Could be an object/vehicle/ped depending on the scoring
		//		Could have been chosen as the siren is blazing, or the ped is running, falling etc...
		// "SURPRISED" is pretty damn surprised, so only say this for interesting events (event 0) above a threshold
		// May actually want to move this Say() to tie in with a bigger reaction.

		//Say audio to the target.
		SayAudioToTarget(*pPed, *pPickedEntity);

		return true;
	}
	else
	{
		// The ped didn't see anything interesting to look at, so now check for ambient lookat positions.
		return LookAtNearbyScenarioPoint(*pPed);
	}
}

bool CAmbientLookAt::PickInterestingLookAtPlayer( CPed* pPlayerPed, bool bPickBest )
{
	bool bInFirstPerson = pPlayerPed->IsInFirstPersonVehicleCamera() || pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false);
	CControl *pControl = pPlayerPed->GetControlFromPlayer();
	if ((pControl->InputHowLongAgo() < 3000) || bInFirstPerson)
		return false;

	// Overridden look at point specified
	if( m_SpecifiedLookAtTarget.GetIsValid() )
	{
		m_CurrentLookAtTarget = m_SpecifiedLookAtTarget;
		return true;
	}

	const Tunables& tune = GetTunables();

	// Randomly pick a threshold level
	// Always consider > 20
	s32 iThreshHold = fwRandom::GetRandomNumberInRange(0, 4);
	float fThreshHold = 1.0f;
	switch(iThreshHold)
	{
	case 0:  fThreshHold =  0.0f; break;
	case 1:  fThreshHold =  5.0f; break;
	case 2:  fThreshHold = 10.0f; break;
	case 3:  fThreshHold = 20.0f; break;
	default: fThreshHold =  1.0f; break;
	}

	CEntity* apEntities[MAX_ENTITIES_TO_LOOKAT];
	float afEntityProbs[MAX_ENTITIES_TO_LOOKAT];
	float fTotalProbability = 0.0f;
	s32 iNoEntities = 0;
	s32 iHighestEntity = 0;

	// If we've got a shocking event look at, always look at that
	CEntity* pLookAtTaskTarget = GetAmbientLookAtTaskTarget(*pPlayerPed);
	if(pLookAtTaskTarget)
	{
		apEntities[iNoEntities] = pLookAtTaskTarget;
		afEntityProbs[iNoEntities] = 3.0f;
		fTotalProbability += afEntityProbs[iNoEntities];
		++iNoEntities;
	}
	else
	{
		Vec3V v = pPlayerPed->GetTransform().GetPosition();
		CEntityIterator entityIterator( IteratePeds|IterateVehicles, pPlayerPed, &v, tune.m_MaxDistanceToScanLookAts);
		CEntity* pEntity = entityIterator.GetNext();
		while( pEntity && iNoEntities < MAX_ENTITIES_TO_LOOKAT )
		{
			float fScore = ScoreInterestingLookAtPlayer( pPlayerPed, pEntity );

			// Only consider if the score reaches the threshold
			if( fScore >= fThreshHold )
			{
				apEntities[iNoEntities]	= pEntity;
				afEntityProbs[iNoEntities]	= fScore;

				if( afEntityProbs[iNoEntities] > afEntityProbs[iHighestEntity] )
				{
					iHighestEntity = iNoEntities;
				}
				fTotalProbability += afEntityProbs[iNoEntities];

				++iNoEntities;
			}

			pEntity = entityIterator.GetNext();
		}
	}

	if( iNoEntities > 0 )
	{
		s32 iPickedEntity = 0;

		if( bPickBest )
		{
			iPickedEntity = iHighestEntity;
		}
		else
		{
			// Pick a random look at
			const float fRandom = fwRandom::GetRandomNumberInRange(0.0f, fTotalProbability);

			float fProbSum = 0.0f;
			for( s32 i = 0; i < iNoEntities; i++ )
			{
				fProbSum += afEntityProbs[i];
				if( fRandom < fProbSum || ( i == ( iNoEntities - 1 ) ) )
				{
					iPickedEntity = i;
					break;
				}
			}
		}

		m_CurrentLookAtTarget.SetEntity(apEntities[iPickedEntity]);

		// Look at entities longer depending on their score
		// Minimum of 1 second. Maximum of 6 seconds
		float fPickedProb = rage::Min(afEntityProbs[iPickedEntity], tune.m_MaxPlayerScore);
		fPickedProb /= 10.0f;
		const float fTimeMultiplier = rage::Max(1.0f, fPickedProb);
		m_fLookAtTimer = fwRandom::GetRandomNumberInRange(1.0f, 2.0f) * fTimeMultiplier;

		return true;
	}
	else
	{
		// The ped didn't see anything interesting to look at, so now check for ambient lookat positions.
		return LookAtNearbyScenarioPoint(*pPlayerPed);
	}
}


CEntity* CAmbientLookAt::GetAmbientLookAtTaskTarget(const CPed& ped)
{
	// Look for a CTaskAmbientLookAtEvent running on the secondary controller, and if so,
	// get the target from there.
	CTask* pActiveSecondaryTask = ped.GetPedIntelligence()->GetTaskSecondaryActive();
	if(pActiveSecondaryTask && pActiveSecondaryTask->GetTaskType() == CTaskTypes::TASK_AMBIENT_LOOK_AT_EVENT)
	{
		return static_cast<CTaskAmbientLookAtEvent*>(pActiveSecondaryTask)->GetLookAtTarget();
	}
	return NULL;
}

bool CAmbientLookAt::ShouldIgnoreFovDueToAmbientLookAtTask(const CPed& rPed)
{
	CTask* pActiveSecondaryTask = rPed.GetPedIntelligence()->GetTaskSecondaryActive();
	if(pActiveSecondaryTask && pActiveSecondaryTask->GetTaskType() == CTaskTypes::TASK_AMBIENT_LOOK_AT_EVENT)
	{
		return static_cast<CTaskAmbientLookAtEvent*>(pActiveSecondaryTask)->GetIgnoreFov();
	}

	return false;
}

bool CAmbientLookAt::UpdateCommentOnNiceVehicle(CPed* pPed)
{
	CTask* pActiveSecondaryTask = pPed->GetPedIntelligence()->GetTaskSecondaryActive();
	if(!pActiveSecondaryTask || 
		pActiveSecondaryTask->GetTaskType() != CTaskTypes::TASK_AMBIENT_LOOK_AT_EVENT || 
	   !static_cast<CTaskAmbientLookAtEvent*>(pActiveSecondaryTask)->GetCommentOnVehicle())
	{
		return false;
	}

	bool bComment = false;

	CEvent* pEventToLookAt = static_cast<CTaskAmbientLookAtEvent*>(pActiveSecondaryTask)->GetEventToLookAt();

	CVehicle * pVehicle = pPed->GetMyVehicle();
	if(pVehicle && pPed->GetIsInVehicle())
	{
		//don't comment on the vehicle if this vehicle hasn't slowed down
		float fSpeed = pVehicle->GetVelocity().Mag2();
		if (fSpeed > rage::square(5.0f))
		{
			return false;
		}

		CEventShocking* pShocking = static_cast<CEventShocking*>(pEventToLookAt);
		if(!pShocking || !pShocking->GetSourceEntity())
		{
			return false;
		}

		CVehicle* pNiceVehicle = static_cast<CVehicle*>(pShocking->GetSourceEntity());

		//Check if now is a suitable time to comment on the vehicle
		Vector3 vPedForward = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetForward());
		vPedForward.Normalize();

		Vector3 vDirectionToVehicle = VEC3V_TO_VECTOR3(pNiceVehicle->GetTransform().GetPosition()) -
									  VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());

		float fdot = vPedForward.Dot(vDirectionToVehicle);
		if(fdot < -0.1f || fdot > 0.45f)
		{
			return false;
		}

		//work out which window to open, could be passenger side
		s32 iSeatPos = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
		s32 iEntryPoint = pVehicle->GetDirectEntryPointIndexForSeat(iSeatPos);

		const CVehicleEntryPointInfo* pEntryPointInfo = NULL;
		if (pVehicle->IsEntryIndexValid(iEntryPoint))
		{
			 pEntryPointInfo = pVehicle->GetVehicleModelInfo()->GetEntryPointInfo(iSeatPos);
		}
	
		if( (pEntryPointInfo && (vPedForward.AngleZ(vDirectionToVehicle) > 0 && pEntryPointInfo->GetVehicleSide() != CVehicleEntryPointInfo::SIDE_LEFT )) ||
			(vPedForward.AngleZ(vDirectionToVehicle) < 0 && pEntryPointInfo->GetVehicleSide() != CVehicleEntryPointInfo::SIDE_RIGHT ) )					
		{
			//Open the window in the opposite seat
			iSeatPos++;

			if ( iSeatPos == 2)
			{
				iSeatPos = 0;
			}
		}	

		//Get the entry point again as it could have changed above
		iEntryPoint = pVehicle->GetDirectEntryPointIndexForSeat(iSeatPos);
		if (pVehicle->IsEntryIndexValid(iEntryPoint))
		{
			pEntryPointInfo = pVehicle->GetVehicleModelInfo()->GetEntryPointInfo(iSeatPos);
		}

		//If there is an entry point get the window and open it if possible
		if(pEntryPointInfo)
		{
			eHierarchyId window = pEntryPointInfo->GetWindowId();
			if (window != VEH_INVALID_ID)
			{
				s32 nBoneIndex = pVehicle->GetVehicleModelInfo()->GetBoneIndex(window);
				if ( nBoneIndex > -1)
				{
					pVehicle->RolldownWindow(window);
				}
			}
		}
		bComment = true;
	}
	else
	{
		//On foot, comment straight away
		bComment = true;
	}

	if(bComment)
	{
		CTask* pActiveSecondaryTask = pPed->GetPedIntelligence()->GetTaskSecondaryActive();
		if(pActiveSecondaryTask && pActiveSecondaryTask->GetTaskType() == CTaskTypes::TASK_AMBIENT_LOOK_AT_EVENT)
		{
			CTaskAmbientLookAtEvent* pAmbient = static_cast<CTaskAmbientLookAtEvent*>(pActiveSecondaryTask);
			taskAssert(pAmbient);
			pAmbient->SetCommentOnVehicle(false);
			
			CEventShocking* pShocking = static_cast<CEventShocking*>(pEventToLookAt);
			if(pShocking)
			{
				CVehicle* pShockingVehicle = static_cast<CVehicle*>(pShocking->GetSourceEntity());
				if (pShockingVehicle)
				{
					CVehicleModelInfo* pVehicleInfo = pShockingVehicle->GetVehicleModelInfo();
					if( pVehicleInfo )
					{
						if( pVehicleInfo->GetVehicleSwankness() > SWANKNESS_3)
						{
							if (pPed->GetIsInVehicle())
							{
								pPed->NewSay("NICE_CAR_SHOUT");
							}
							else
							{
								pPed->NewSay("NICE_CAR");
							}
						}
						else
						{
							pPed->NewSay("GENERIC_SHOCKED_MED");
						}

						return true;
					}
				}
			}
		}
	}

	return false;	
}


float CAmbientLookAt::UpdateJeerAtHotPed(const CPed* pPed, const CPed* pOtherPed)
{

	const Tunables& tune = GetTunables();

	if (!pPed->CheckSexinessFlags(SF_JEER_AT_HOT_PED) && !tune.m_HotPedDisableSexinessFlagChecks)
	{
		return 1.0f;
	}

	if (!pOtherPed->CheckSexinessFlags(SF_HOT_PERSON) && !tune.m_HotPedDisableSexinessFlagChecks)
	{
		return 1.0f;
	}

	if ( pOtherPed->GetIsInVehicle())
	{
		return 1.0f;
	}

	if ( pOtherPed->IsAPlayerPed())
	{
		return 1.0f;
	}

	if ( pOtherPed->IsInjured())
	{
		return 1.0f;
	}

	Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	Vector3 vOtherPedPos = VEC3V_TO_VECTOR3(pOtherPed->GetTransform().GetPosition());
	float fHeightDiff = Abs(vOtherPedPos.z - vPedPos.z);

	if (fHeightDiff > tune.m_HotPedMaxHeightDifference)
	{
		return 1.0f;
	}

	float distSq = (vOtherPedPos - vPedPos).Mag2();
	if ( distSq < rage::square(sm_Tunables.m_HotPedMaxDistance) && distSq > rage::square(tune.m_HotPedMinDistance))
	{
		//Check they aren't on the same street?
		//Let's see if a simple dot product check works for this.
		Vector3 vPedForward = VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward());
		Vector3 vDirectionToOtherPed = vOtherPedPos - vPedPos;
		vDirectionToOtherPed.Normalize();
		float fDot = vPedForward.Dot(vDirectionToOtherPed);
		if (fDot > tune.m_HotPedMinDotAngle && fDot < tune.m_HotPedMaxDotAngle)
		{
#if DEBUG_DRAW
			if ( tune.m_HotPedRenderDebug)
			{
				grcDebugDraw::Sphere( VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) + Vector3(0.0f,0.0f,1.2f),0.5f, Color_green,true,30);
				grcDebugDraw::Sphere( VEC3V_TO_VECTOR3(pOtherPed->GetTransform().GetPosition())+ Vector3(0.0f,0.0f,1.2f),0.3f, Color_red,true,30);
				grcDebugDraw::Line( VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition())+ Vector3(0.0f,0.0f,1.2f),VEC3V_TO_VECTOR3(pOtherPed->GetTransform().GetPosition())+ Vector3(0.0f,0.0f,1.2f),Color_green,Color_red,30);
			}
#endif
			return tune.m_PlayerSexyPedModifier;
		}
	}

	return 1.0f;
}

// Project a point in front of the ped and search for nearby scenarios of type CScenarioLookAtInfo.
// If one is found, then store a copy for potential later use.
void CAmbientLookAt::ScanForNewScenarioLookAtPoint(const CPed& rPed)
{
	// Disable viewing scenario lookat points for peds in vehicles and peds using scenarios.
	if (rPed.GetIsInVehicle() || rPed.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_USE_SCENARIO))
	{
		m_pLookAtScenarioPoint = NULL;
		return;
	}

	const Tunables& tunables = GetTunables();

	// Search for scenario points ahead of the ped within a certain radius.
	Vector3 vSearchCenter = VEC3V_TO_VECTOR3(rPed.GetTransform().GetPosition());
	vSearchCenter += VEC3V_TO_VECTOR3(rPed.GetTransform().GetForward()) * tunables.m_ScenarioScanOffsetDistance;

	// Only look for points of type CScenarioLookAtInfo.
	CScenarioFinder::FindOptions args;
	args.m_DesiredScenarioClassId = CScenarioLookAtInfo::GetStaticClassId();

	int iScenarioType = -1;
	m_pLookAtScenarioPoint = CScenarioFinder::FindNewScenario(&rPed, vSearchCenter, tunables.m_ScenarioScanRadius, args, iScenarioType);

}

float CAmbientLookAt::GetModifiedScoreBasedOnHistory(CPed& rPed, CEntity& rEntity, float fCurrentScore)
{
	if (m_LookAtHistory.CheckHistory(rPed, rEntity))
	{
		if(rEntity.GetIsTypePed() && ShouldConsiderPlayerForScoringPurposes((CPed&)(rEntity)))
		{
			fCurrentScore *= GetTunables().m_RecentlyLookedAtPlayerModifier;
		}
		else
		{
			fCurrentScore *= GetTunables().m_RecentlyLookedAtEntityModifier;
		}
	}

	return fCurrentScore;
}

// Check if the ped has previously found a nearby scenario point of type CScenarioLookAtInfo.
bool CAmbientLookAt::LookAtNearbyScenarioPoint(const CPed& rPed)
{
	// Periodically check for a new point to look at.
	if (m_fTimeSinceLastScenarioScan >= GetTunables().m_TimeBetweenScenarioScans)
	{
		//Find a new point.
		ScanForNewScenarioLookAtPoint(rPed);
		m_fTimeSinceLastScenarioScan = 0.0f;
	}
	if (m_pLookAtScenarioPoint)
	{
		const Vector3& vInterestingPoint = VEC3V_TO_VECTOR3(m_pLookAtScenarioPoint->GetPosition());
		m_CurrentLookAtTarget.SetPosition(vInterestingPoint);
		const Tunables& tunables = GetTunables();
		if (rPed.IsAPlayerPed())
		{
			m_fLookAtTimer = fwRandom::GetRandomNumberInRange(tunables.m_PlayerTimeBetweenLookAtsMin, tunables.m_PlayerTimeBetweenLookAtsMax);
		}
		else
		{
			m_fLookAtTimer = fwRandom::GetRandomNumberInRange(tunables.m_AITimeBetweenLookAtsFailureMin, tunables.m_AITimeBetweenLookAtsFailureMax);
		}
		return true;
	}
	else
	{
		return false;
	}
}

bool CAmbientLookAt::PickCameraLookAtPlayer(CPed* pPlayerPed)
{
	camBaseCamera* pCamera = NULL;

	const camBaseCamera* pTPCamera = (camBaseCamera*)camInterface::GetGameplayDirector().GetThirdPersonCamera();
	const camBaseCamera* pFPVehCamera = (camBaseCamera*)camInterface::GetGameplayDirector().GetFirstPersonVehicleCamera();
	const camBaseCamera* pFPShootCamera = (camBaseCamera*)camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();

	if (pTPCamera && camInterface::IsDominantRenderedCamera(*pTPCamera))
	{
		pCamera = const_cast<camBaseCamera*>(pTPCamera);
	}
	else if (pFPVehCamera && camInterface::IsDominantRenderedCamera(*pFPVehCamera))
	{
		pCamera = const_cast<camBaseCamera*>(pFPVehCamera);
	}
	else if (pFPShootCamera && camInterface::IsDominantRenderedCamera(*pFPShootCamera))
	{
		pCamera = const_cast<camBaseCamera*>(pFPShootCamera);
	}

	if(pCamera)
	{
		Vector3 vCameraFront = pCamera->GetBaseFront();
		const CControl *pControl = pPlayerPed->GetControlFromPlayer();
		bool bLookBehind = pControl && pControl->GetPedLookBehind().IsDown() && !pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UsingCoverPoint);
		bool bShouldLookBack = false;
		if(bLookBehind)
		{
			vCameraFront *= -1.0f;

			const Tunables& tune = GetTunables();

			// Look back if the wait time has passed or the look behind button is just pressed.
			if(m_uLookBackStartTime + tune.m_uTimeBetweenLookBacks < fwTimer::GetTimeInMilliseconds() || pControl->GetPedLookBehind().IsPressed())
			{
				bShouldLookBack = true;
				if(!m_bLookBackStarted)
				{
					m_bLookBackStarted = true;
					m_uLookBackStartTime = fwTimer::GetTimeInMilliseconds();
				}
			}

			// Keep looking back for certain amount of time.
			if(m_uLookBackStartTime + tune.m_uTimeToLookBack > fwTimer::GetTimeInMilliseconds())
			{
				bShouldLookBack = true;
			}
			else
			{
				m_bLookBackStarted = false;
			}
		}
		else
		{
			m_bLookBackStarted = false;
		}

		bool bCanUseRearMirror = pPlayerPed->GetIsDrivingVehicle() && pPlayerPed->GetMyVehicle()->GetVehicleType() == VEHICLE_TYPE_CAR 
			&& !pPlayerPed->GetMyVehicle()->m_nVehicleFlags.bIsBus && !pPlayerPed->GetMyVehicle()->m_nVehicleFlags.bIsVan && !pPlayerPed->GetMyVehicle()->m_nVehicleFlags.bIsBig && pPlayerPed->GetMyVehicle()->GetBoneIndex((eHierarchyId)(VEH_DOOR_PSIDE_F)) > -1;
		Vector3 vPlayerHeadPos;
		pPlayerPed->GetBonePosition(vPlayerHeadPos, BONETAG_HEAD);
		Vector3 vPlayerForward = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetForward());
		const Tunables& tune = GetTunables();
		if(vCameraFront.Dot(vPlayerForward) > rage::Cosf(DtoR * tune.m_MaxLookBackAngle))
		{
			m_CurrentLookAtTarget.Clear();
			const Vector3& vLookAtPosition = vPlayerHeadPos + vCameraFront * 5.0f;
			m_CurrentLookAtTarget.SetPosition(vLookAtPosition);
			return true;
		}
		else if(bCanUseRearMirror && bShouldLookBack)
		{
			// Look at the rear mirror
			static dev_float fRight = -0.45f;
			static dev_float fUp = 0.2f; 
			Matrix34 playerMat = MAT34V_TO_MATRIX34(pPlayerPed->GetMatrix());
			playerMat.RotateLocalZ(fRight);
			playerMat.RotateLocalX(fUp);
			Vector3 vRearMirrorDirection = playerMat.b;
			
			m_CurrentLookAtTarget.Clear();
			const Vector3& vLookAtPosition = vPlayerHeadPos + vRearMirrorDirection*5.0f;
			m_CurrentLookAtTarget.SetPosition(vLookAtPosition);
			return true;
		}
		else if(bLookBehind)	// Pick a point behind the player, so he looks back in this case. 
		{
			Vector3 vLookAtDirection = VEC3V_TO_VECTOR3(pPlayerPed->GetMatrix().GetCol1()) * -1.0f;
			if(!bShouldLookBack)
			{
				vLookAtDirection *= -1.0f;
			}
			m_CurrentLookAtTarget.Clear();
			const Vector3& vLookAtPosition = vPlayerHeadPos + vLookAtDirection * 5.0f;
			m_CurrentLookAtTarget.SetPosition(vLookAtPosition);
			return true;
		}
	}

	return false;
}

bool CAmbientLookAt::PickMotionLookAtPlayer(CPed* pPlayerPed)
{
	const CControl *pControl = pPlayerPed->GetControlFromPlayer();
	if(pControl)
	{
		const Vector2 vecStickInput( pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm() ); 
		static dev_float MIN_MOTION_BLEND_RATIO = 0.25f;
		Vector3 vPlayerHeadPos;
		pPlayerPed->GetBonePosition(vPlayerHeadPos, BONETAG_HEAD);
		bool bIsDriving = pPlayerPed->GetIsDrivingVehicle() && !pPlayerPed->IsExitingVehicle();
		if( vecStickInput.Mag2() > rage::square( MIN_MOTION_BLEND_RATIO ) )
		{
			float fDesiredHeading = pPlayerPed->GetDesiredHeading();
			// Use wheel's steer angle if driving. 
			if(bIsDriving)
			{
				CVehicle* pMyVehicle = pPlayerPed->GetMyVehicle();
				if(pMyVehicle)
				{
					Vector3 vFront = VEC3V_TO_VECTOR3(pMyVehicle->GetVehicleForwardDirection()); 
					float fFacingOrient = Selectf(Abs(vFront.z) - 1.0f + VERY_SMALL_FLOAT, 0.0f, rage::Atan2f(-vFront.x, vFront.y));
					CWheel* pWheel = pMyVehicle->GetWheelFromId(VEH_WHEEL_FIRST_WHEEL);
					fDesiredHeading = pWheel ? pWheel->GetSteeringAngle() : pMyVehicle->GetSteerAngle();
					fDesiredHeading = fDesiredHeading + fFacingOrient;
					fDesiredHeading = fwAngle::LimitRadianAngle(fDesiredHeading);
				}
			}
			Matrix34 headingMat, pitchMat;
			headingMat.MakeRotateZ(fDesiredHeading);
			float fDesiredPitch = pPlayerPed->GetDesiredPitch();
			pitchMat.MakeRotateX(fDesiredPitch);
			headingMat.DotFromLeft(pitchMat);

			if(bIsDriving)
			{
				// Should look forward when driving a car if the stick is pulled down 
				const Tunables& tune = GetTunables();
				Vector3 vVehicleForward = VEC3V_TO_VECTOR3(pPlayerPed->GetMyVehicle()->GetVehicleForwardDirection());
				if(headingMat.b.Dot(vVehicleForward) <= rage::Cosf(DtoR * tune.m_MaxLookBackAngle))
				{
					headingMat.b = vVehicleForward;
				}
			}
			m_CurrentLookAtTarget.Clear();
			m_CurrentLookAtTarget.SetPosition(vPlayerHeadPos + headingMat.b * 5.0f);
			return true;
		}
		else if(bIsDriving && GetMovingSpeed(pPlayerPed) > 1.0f)	// try to look forward when the vehicle is moving
		{
			m_CurrentLookAtTarget.Clear();
			return false;
		}
	}

	return false;
}

// Look at vehicle velocity direction if the vehicle is in the air.
bool CAmbientLookAt::VehicleJumpLookAtPlayer(CPed* pPlayerPed)
{
	if(pPlayerPed->GetIsDrivingVehicle() && pPlayerPed->GetMyVehicle() && (pPlayerPed->GetMyVehicle()->InheritsFromAutomobile() ||  pPlayerPed->GetMyVehicle()->InheritsFromBike() || pPlayerPed->GetMyVehicle()->InheritsFromBoat() || pPlayerPed->GetMyVehicle()->InheritsFromQuadBike() || pPlayerPed->GetMyVehicle()->InheritsFromAmphibiousQuadBike()) && pPlayerPed->GetMyVehicle()->IsInAir(true))
	{
		Vector3 vVelocity = pPlayerPed->GetMyVehicle()->GetVelocity();
		static dev_float fMinSpeed = 5.0f;
		if(vVelocity.Mag2() > fMinSpeed * fMinSpeed)
		{
			m_CurrentLookAtTarget.Clear();
			m_CurrentLookAtTarget.SetPosition(VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition()) + vVelocity);
			return true;
		}
	}

	return false;
}

bool CAmbientLookAt::ConvertibleRoofOpenCloseLookAtPlayer(CPed* pPlayerPed)
{
	// Animation tags
	static const atHashString s_OpenLookFront("OPEN_LookFront", 0x8982FCBE);
	static const atHashString s_OpenLookMid("OPEN_LookMid", 0xF6B40E3B);
	static const atHashString s_OpenLookBack("OPEN_LookBack", 0x248DE5D3);
	static const atHashString s_OpenLookEnd("OPEN_LookEnd", 0x7E141DC9);
	static const atHashString s_CloseLookBack("CLOSE_LookBack", 0x32350AE8);
	static const atHashString s_CloseLookMid("CLOSE_LookMid", 0x1A9F9543);
	static const atHashString s_CloseLookFront("CLOSE_LookFront", 0x9992A763);
	static const atHashString s_CloseLookEnd("CLOSE_LookEnd", 0x443D9F6C);

	static dev_float fSpeedLimit = 2.0f;

	if(pPlayerPed->GetIsDrivingVehicle() && pPlayerPed->GetMyVehicle()->DoesVehicleHaveAConvertibleRoofAnimation() && pPlayerPed->GetMyVehicle()->GetVelocity().Mag2() < fSpeedLimit * fSpeedLimit)
	{
		CVehicle* pVehicle = pPlayerPed->GetMyVehicle();
		aiTask *pTask =  pVehicle->GetIntelligence()->GetTaskManager()->GetTree(VEHICLE_TASK_TREE_SECONDARY)->GetActiveTask();
		if(pTask && pTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_CONVERTIBLE_ROOF)
		{
			CTaskVehicleConvertibleRoof *pConvertibleRoofTask =  static_cast<CTaskVehicleConvertibleRoof*>(pTask);
			u32 iAnimHashKey = pVehicle->GetVehicleModelInfo()->GetConvertibleRoofAnimNameHash();
			s32 iSlot = pVehicle->GetVehicleModelInfo()->GetClipDictionaryIndex().Get();
			const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(iSlot, iAnimHashKey);

			if(pClip)
			{
				TUNE_GROUP_FLOAT(ROOF_OPEN_CLOSE_LOOK_AT, fCloseLookBackPitch, 10.0f, -180.0f, 180.0f, 1.0f);
				TUNE_GROUP_FLOAT(ROOF_OPEN_CLOSE_LOOK_AT, fCloseLookBackYaw, -135.0f, -180.0f, 180.0f, 1.0f);
				TUNE_GROUP_FLOAT(ROOF_OPEN_CLOSE_LOOK_AT, fCloseLookMidPitch, 28.0f, -180.0f, 180.0f, 1.0f);
				TUNE_GROUP_FLOAT(ROOF_OPEN_CLOSE_LOOK_AT, fCloseLookMidYaw, -80.0f, -180.0f, 180.0f, 1.0f);
				TUNE_GROUP_FLOAT(ROOF_OPEN_CLOSE_LOOK_AT, fCloseLookFrontPitch, 28.0f, -180.0f, 180.0f, 1.0f);
				TUNE_GROUP_FLOAT(ROOF_OPEN_CLOSE_LOOK_AT, fCloseLookFrontYaw, -30.0f, -180.0f, 180.0f, 1.0f);

				TUNE_GROUP_FLOAT(ROOF_OPEN_CLOSE_LOOK_AT, fOpenLookBackPitch, 10.0f, -180.0f, 180.0f, 1.0f);
				TUNE_GROUP_FLOAT(ROOF_OPEN_CLOSE_LOOK_AT, fOpenLookBackYaw, -135.0f, -180.0f, 180.0f, 1.0f);
				TUNE_GROUP_FLOAT(ROOF_OPEN_CLOSE_LOOK_AT, fOpenLookMidPitch, 28.0f, -180.0f, 180.0f, 1.0f);
				TUNE_GROUP_FLOAT(ROOF_OPEN_CLOSE_LOOK_AT, fOpenLookMidYaw, -80.0f, -180.0f, 180.0f, 1.0f);
				TUNE_GROUP_FLOAT(ROOF_OPEN_CLOSE_LOOK_AT, fOpenLookFrontPitch, 28.0f, -180.0f, 180.0f, 1.0f);
				TUNE_GROUP_FLOAT(ROOF_OPEN_CLOSE_LOOK_AT, fOpenLookFrontYaw, -30.0f, -180.0f, 180.0f, 1.0f);

				bool bShouldLook = false;
				bool bUseInterpolation = false;
				float fDesiredYaw = 0.0f;
				float fDesiredPitch = 0.0f;
				float fNextDesiredYaw = 0.0f;
				float fNextDesiredPitch = 0.0f;
				float fStartPhase = 0.0f;
				float fCurrentPhase = 0.0f;
				float fNextStartPhase = 0.0f;
				float fEndPhase, fNextEndPhase;

				if(pConvertibleRoofTask->GetRoofState() == CTaskVehicleConvertibleRoof::STATE_RAISING)
				{
					fCurrentPhase = pConvertibleRoofTask->GetRoofProgress();
					
					// The roof anim is played backward during raising, i.e., from phase 1.0 to 0.0.
					if(CClipEventTags::GetMoveEventStartAndEndPhases(pClip, s_CloseLookEnd, fStartPhase, fEndPhase) && fCurrentPhase < fStartPhase)
					{
						//Displayf("Close look end");
						return false;
					}
					else if(CClipEventTags::GetMoveEventStartAndEndPhases(pClip, s_CloseLookFront, fStartPhase, fEndPhase) && fCurrentPhase < fStartPhase)
					{
						//Displayf("Close look front");
						bShouldLook = true;
						fDesiredYaw = fCloseLookFrontYaw;
						fDesiredPitch = fCloseLookFrontPitch;
					}
					else if(CClipEventTags::GetMoveEventStartAndEndPhases(pClip, s_CloseLookMid, fStartPhase, fEndPhase) && fCurrentPhase < fStartPhase)
					{
						//Displayf("Close look mid");
						bShouldLook = true;
						bUseInterpolation = true;
						fDesiredYaw = fCloseLookMidYaw;
						fDesiredPitch = fCloseLookMidPitch;
						fNextDesiredYaw = fCloseLookFrontYaw;
						fNextDesiredPitch = fCloseLookFrontPitch;
						CClipEventTags::GetMoveEventStartAndEndPhases(pClip, s_CloseLookFront, fNextStartPhase, fNextEndPhase);
					}
					else if(CClipEventTags::GetMoveEventStartAndEndPhases(pClip, s_CloseLookBack, fStartPhase, fEndPhase) && fCurrentPhase < fStartPhase)
					{
						//Displayf("Close look back");
						bShouldLook = true;
						bUseInterpolation = true;
						fDesiredYaw = fCloseLookBackYaw;
						fDesiredPitch = fCloseLookBackPitch;
						fNextDesiredYaw = fCloseLookMidYaw;
						fNextDesiredPitch = fCloseLookMidPitch;
						CClipEventTags::GetMoveEventStartAndEndPhases(pClip, s_CloseLookMid, fNextStartPhase, fNextEndPhase);
					}
				}
				else if(pConvertibleRoofTask->GetRoofState() == CTaskVehicleConvertibleRoof::STATE_LOWERING)
				{
					fCurrentPhase = pConvertibleRoofTask->GetRoofProgress();

					// The roof anim is played backward during raising, i.e., from phase 1.0 to 0.0.
					if(CClipEventTags::GetMoveEventStartAndEndPhases(pClip, s_OpenLookEnd, fStartPhase, fEndPhase) && fCurrentPhase > fStartPhase)
					{
						//Displayf("Open look end");
						return false;
					}
					else if(CClipEventTags::GetMoveEventStartAndEndPhases(pClip, s_OpenLookBack, fStartPhase, fEndPhase) && fCurrentPhase > fStartPhase)
					{
						//Displayf("Open look back");
						bShouldLook = true;
						fDesiredYaw = fOpenLookBackYaw;
						fDesiredPitch = fOpenLookBackPitch;
					}
					else if(CClipEventTags::GetMoveEventStartAndEndPhases(pClip, s_OpenLookMid, fStartPhase, fEndPhase) && fCurrentPhase > fStartPhase)
					{
						//Displayf("Open look Mid");
						bShouldLook = true;
						bUseInterpolation = true;
						fDesiredYaw = fOpenLookMidYaw;
						fDesiredPitch = fOpenLookMidPitch;
						fNextDesiredYaw = fOpenLookBackYaw;
						fNextDesiredPitch = fOpenLookBackPitch;
						CClipEventTags::GetMoveEventStartAndEndPhases(pClip, s_OpenLookBack, fNextStartPhase, fNextEndPhase);
					}
					else if(CClipEventTags::GetMoveEventStartAndEndPhases(pClip, s_OpenLookFront, fStartPhase, fEndPhase) && fCurrentPhase > fStartPhase)
					{
						//Displayf("Open look front");
						bShouldLook = true;
						bUseInterpolation = true;
						fDesiredYaw = fOpenLookFrontYaw;
						fDesiredPitch = fOpenLookFrontPitch;
						fNextDesiredYaw = fOpenLookMidYaw;
						fNextDesiredPitch = fOpenLookMidPitch;
						CClipEventTags::GetMoveEventStartAndEndPhases(pClip, s_OpenLookMid, fNextStartPhase, fNextEndPhase);
					}
				}

				if(bShouldLook)
				{
					if(bUseInterpolation)
					{
						float fInterpolatedPhase = (fCurrentPhase - fStartPhase) / (fNextStartPhase - fStartPhase);
						fDesiredYaw = Lerp(fInterpolatedPhase, fDesiredYaw, fNextDesiredYaw);
						fDesiredPitch = Lerp(fInterpolatedPhase, fDesiredPitch, fNextDesiredPitch);
					}

					Matrix34 playerMat = MAT34V_TO_MATRIX34(pPlayerPed->GetMatrix());
					playerMat.RotateLocalZ(DEGREES_TO_RADIANS(fDesiredYaw));
					playerMat.RotateLocalX(DEGREES_TO_RADIANS(fDesiredPitch));
					Vector3 vLookAtDirection = playerMat.b;

					m_CurrentLookAtTarget.Clear();
					Vector3 vPlayerHeadPos;
					pPlayerPed->GetBonePosition(vPlayerHeadPos, BONETAG_HEAD);
					m_CurrentLookAtTarget.SetPosition(vPlayerHeadPos + vLookAtDirection);

					return true;

#if 0 //DEBUG_DRAW				
					Vector3 vLookatPosition;
					m_CurrentLookAtTarget.GetPosition(vLookatPosition);
					grcDebugDraw::Line(vPlayerHeadPos, vLookatPosition, Color_wheat);
#endif
				}
			}
		}
	}

	return false;
}


// Look at my vehicle after exit.
bool CAmbientLookAt::LookAtMyVehicle(CPed* pPed)
{
	if(pPed->GetMyVehicle())
	{
		// Look at front or back of the car based on the direction of ped's facing.
		Vec3V vecVehicleForward = pPed->GetMyVehicle()->GetTransform().GetForward();
		if (IsLessThanAll(Dot(vecVehicleForward, pPed->GetTransform().GetForward()), ScalarV(V_ZERO)))
		{
			vecVehicleForward *= ScalarV(V_NEGONE);
		}
		m_CurrentLookAtTarget.Clear();
		m_CurrentLookAtTarget.SetPosition(VEC3V_TO_VECTOR3(pPed->GetMyVehicle()->GetTransform().GetPosition()) + VEC3V_TO_VECTOR3(vecVehicleForward) * pPed->GetMyVehicle()->GetBoundRadius());
	
		return true;
	}

	return false;
}

float CAmbientLookAt::GetMovingSpeed(const CPed* pPed)
{
	float fSpeed = pPed->GetVelocity().Mag();
	if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle())
	{
		fSpeed = pPed->GetMyVehicle()->GetVelocity().Mag();
	}

	return fSpeed;
}

bool CAmbientLookAt::CanLoadAudioForExchange(const CPed& rPed, const CPed& rTarget) const
{
	//Grab the ped position.
	Vec3V vPedPosition = rPed.GetTransform().GetPosition();

	//Grab the target position.
	Vec3V vTargetPosition = rTarget.GetTransform().GetPosition();

	//Grab the ped forward vector.
	Vec3V vPedForward = rPed.GetTransform().GetForward();

	//Generate a vector from the ped to the target.
	Vec3V vPedToTarget = Subtract(vTargetPosition, vPedPosition);

	//Ensure the target is in our front 180.
	ScalarV scDot = Dot(vPedForward, vPedToTarget);
	ScalarV scMinDot = ScalarV(V_ZERO);
	if(IsLessThanAll(scDot, scMinDot))
	{
		return false;
	}

	return true;
}

bool CAmbientLookAt::LoadAudioExchangeForEvent(const CPed& rPed, CAmbientAudioExchange& rExchange) const
{
	//Ensure the ped is responding to an event.
	CEvent* pEvent = rPed.GetPedIntelligence()->GetCurrentEvent();
	if(!pEvent)
	{
		return false;
	}

	//Ensure the event is editable.
	if(!pEvent->HasEditableResponse())
	{
		return false;
	}

	//Grab the editable event.
	CEventEditableResponse* pEventEditable = static_cast<CEventEditableResponse*>(pEvent);

	//Ensure the speech is valid.
	u32 uContext = pEventEditable->GetSpeechHash();
	if(uContext == 0)
	{
		return false;
	}

	//Set the say.
	rExchange.m_Initial.SetFinalizedHash(uContext);

	return true;
}

bool CAmbientLookAt::LoadAudioExchangeBetweenPeds(const CPed& rPed, const CPed& rTarget, CAmbientAudioExchange& rExchange) const
{
	//Ensure we can load audio for an exchange.
	if(!CanLoadAudioForExchange(rPed, rTarget))
	{
		return false;
	}

	//Ensure the exchange from the ped to the target is valid.
	const CAmbientAudioExchange* pExchange = CAmbientAudioManager::GetInstance().GetExchange(rPed, rTarget);
	if(!pExchange)
	{
		return false;
	}

	//Set the exchange.
	rExchange = *pExchange;

	return true;
}

bool CAmbientLookAt::CanSayAudioToTarget(CPed& rPed, CEntity& rTarget, CAmbientAudioExchange& rExchange) const
{
	//Ensure we can say audio.
	if(!m_bCanSayAudio)
	{
		return false;
	}

	//Assert that the ped is not a player.
	aiAssert(!rPed.IsPlayer());

	//Ensure the target is a ped.
	if(!rTarget.GetIsTypePed())
	{
		return false;
	}

	//Grab the target ped.
	CPed& rTargetPed = static_cast<CPed &>(rTarget);

	//Ensure the target is in our FOV.
	if(!rPed.GetPedIntelligence()->GetPedPerception().ComputeFOVVisibility(VEC3V_TO_VECTOR3(rTarget.GetTransform().GetPosition())))
	{
		return false;
	}

	//Load the audio exchange for the event.
	if(!LoadAudioExchangeForEvent(rPed, rExchange))
	{
		//Load the audio exchange between peds.
		if(!LoadAudioExchangeBetweenPeds(rPed, rTargetPed, rExchange))
		{
			return false;
		}
	}

	return true;
}

void CAmbientLookAt::SayAudioToTarget(CPed& rPed, CEntity& rTarget) const
{
	CAmbientAudioExchange exchange;
	if(CanSayAudioToTarget(rPed, rTarget, exchange))
	{
		//Talk to the ped.
		CPed& rTargetPed = static_cast<CPed &>(rTarget);
		CTalkHelper::Talk(rPed, rTargetPed, exchange);
	}
}

// Return the nearby player ped.
CPed* CAmbientLookAt::ShouldCheckNearbyPlayer(CPed* pPed)
{
	// If the player is within range.
	const CEntity* pEntity = m_CurrentLookAtTarget.GetEntity();
	if(!pEntity || !pEntity->GetIsTypePed() || !ShouldConsiderPlayerForScoringPurposes((CPed&)(*pEntity)))
	{
		CPed* pPlayerPed = pPed->GetPedIntelligence()->GetClosestPedInRange();
		if(pPlayerPed && pPlayerPed->IsAPlayerPed())
		{
			Vec3V vPlayerPos = pPlayerPed->GetTransform().GetPosition();
			Vec3V vPedPos = pPed->GetTransform().GetPosition();
			ScalarV sDistToPlayer = DistSquared(vPedPos, vPlayerPos);

			// If the player is extremely close
			if (IsLessThanAll(sDistToPlayer, ScalarV(rage::square(GetTunables().m_ClosePlayerDistanceThreshold))))
			{
				return pPlayerPed;
			}

			// If the player nearby and moving or holding a gun.
			if (IsLessThanAll(sDistToPlayer, ScalarV(rage::square(GetInRangePlayerDistanceThreshold(pPlayerPed)))))
			{
				// If the palyer is running
				if(pPlayerPed->GetMotionData()->GetCurrentMbrY() > MOVEBLENDRATIO_STILL)
				{
					return pPlayerPed;
				}

				// If the player is holding a weapon
				if(pPlayerPed->GetWeaponManager() && pPlayerPed->GetWeaponManager()->GetEquippedWeaponInfo() && !pPlayerPed->GetWeaponManager()->GetEquippedWeaponInfo()->GetIsUnarmed())
				{
					return pPlayerPed;
				}

				// If the player is covered in blood
				CPedDamageSet * pDamageSet = PEDDAMAGEMANAGER.GetPlayerDamageSet();
				if (pDamageSet && pDamageSet->HasBlood())
				{
					return pPlayerPed;
				}
			}

			// If the player is ragdolling
			if (IsLessThanAll(sDistToPlayer, ScalarV(rage::square(GetTunables().m_RagdollPlayerDistanceThreshold))))
			{
				if(pPlayerPed->GetRagdollState() > RAGDOLL_STATE_ANIM_DRIVEN)
				{
					return pPlayerPed;
				}
			}
		}
	}

	return NULL;
}

bool CAmbientLookAt::ShouldIgnoreFOVWhenLookingAtPlayer(CPed* pPed, CPed* pPlayerPed)
{
	Vec3V vPlayerPos = pPlayerPed->GetTransform().GetPosition();
	Vec3V vPedPos = pPed->GetTransform().GetPosition();
	ScalarV sDistToPlayer = DistSquared(vPedPos, vPlayerPos);

	const Tunables& tune = GetTunables();

	if (IsLessThanAll(sDistToPlayer, ScalarV(rage::square(tune.m_ClosePlayerDistanceThreshold))))
	{
		// Ignore FOV when the player is extremely close
		return true;
	}
	else if (IsLessThanAll(sDistToPlayer, ScalarV(rage::square(GetInRangePlayerDistanceThreshold(pPlayerPed)))))
	{
		// Ignore the Dot angle limit when the player is driving.
		if(pPlayerPed->GetIsDrivingVehicle())
		{
			return true;
		}

		Vec3V vToPlayer = vPlayerPos - vPedPos;
		vToPlayer.SetZ(ScalarV(V_ZERO));
		vToPlayer = NormalizeFastSafe(vToPlayer, Vec3V(V_ZERO));
		if (Dot(pPed->GetTransform().GetForward(), vToPlayer).Getf() > tune.m_LookingInRangePlayerMaxDotAngle)
		{
			if (pPlayerPed->GetMotionData()->GetCurrentMbrY() > MOVEBLENDRATIO_STILL)
			{
				return true;
			}

			if(pPlayerPed->GetWeaponManager() && pPlayerPed->GetWeaponManager()->GetEquippedWeaponInfo() && !pPlayerPed->GetWeaponManager()->GetEquippedWeaponInfo()->GetIsUnarmed())
			{
				return true;
			}

			CPedDamageSet * pDamageSet = PEDDAMAGEMANAGER.GetPlayerDamageSet();
			if (pDamageSet && pDamageSet->HasBlood())
			{
				return true;
			}
		}
	}
	else if (IsLessThanAll(sDistToPlayer, ScalarV(rage::square(GetTunables().m_RagdollPlayerDistanceThreshold))))
	{
		// If the player is ragdolling
		if(pPlayerPed->GetRagdollState() > RAGDOLL_STATE_ANIM_DRIVEN)
		{
			return true;
		}
	}

	return false;
}

float CAmbientLookAt::GetInRangePlayerDistanceThreshold(const CPed* pPlayerPed)
{
	const Tunables& tune = GetTunables();

	if(pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_InRaceMode))
	{
		return tune.m_InRangePlayerInRaceDistanceThreshold;
	}
	else
	{
		return tune.m_InRangePlayerDistanceThreshold;
	}
}

bool CAmbientLookAt::LookAtCarJacker(CPed* pPed)
{
	CPed* pCarJacker = pPed->GetCarJacker();
	if(pCarJacker)
	{
		Vec3V vJackerPos = pCarJacker->GetTransform().GetPosition();
		Vec3V vPedPos = pPed->GetTransform().GetPosition();
		ScalarV sDistToJacker = DistSquared(vPedPos, vJackerPos);

		// Is the player extremely close?
		if (IsLessThanAll(sDistToJacker, ScalarV(rage::square(15.0f))))
		{
			m_CurrentLookAtTarget.Clear();
			m_CurrentLookAtTarget.SetEntity(pCarJacker);
			m_fLookAtTimer = 3.0f;

			return true;
		}

		pPed->SetCarJacker(NULL);
	}

	return false;
}

bool CAmbientLookAt::ShouldStopAimToIdleLookAt(CPed* pPed)
{
	Vector3 vLookAtPosition, vLookAtDirection;
	m_CurrentLookAtTarget.GetPosition(vLookAtPosition);
	Vector3 vPlayerHeadPos;
	pPed->GetBonePosition(vPlayerHeadPos, BONETAG_HEAD);
	vLookAtDirection = vLookAtPosition - vPlayerHeadPos;
	vLookAtDirection.z = 0.0f;
	vLookAtDirection.NormalizeSafe();

	Matrix34 matSpine;
	pPed->GetBoneMatrix(matSpine, BONETAG_SPINE3);
	matSpine.b.z = 0.0f;
	matSpine.b.NormalizeSafe();
	//grcDebugDraw::Line(matSpine.d, matSpine.d+matSpine.b*10.0f, Color_red);
	//grcDebugDraw::Line(matSpine.d, matSpine.d+vLookAtDirection*10.0f, Color_blue);

	if(Dot(vLookAtDirection, matSpine.b) < cosf(DtoR*GetTunables().m_fAimToIdleBreakOutAngle))
	{
		return true;
	}

	return false;
}

void CAmbientLookAt::UpdateAimToIdleLookAtTarget(CPed* pPed)
{
	crSkeleton& skeleton = *pPed->GetSkeleton();
	s32 iBoneIndex = pPed->GetBoneIndexFromBoneTag(BONETAG_FACING_DIR);

	if (iBoneIndex == -1)
	{
		return;
	}

	u16 uFacingBoneIdx = (u16)iBoneIndex;
	// Facing dir is mover dir rotated by delta stored in facing dir bone (IK_Root) 
	Mat34V mtxReferenceDir = pPed->GetTransform().GetMatrix(); 
	Transform(mtxReferenceDir, mtxReferenceDir, skeleton.GetLocalMtx(uFacingBoneIdx)); 
	Vector3 vTargetPosition;
	m_CurrentLookAtTarget.GetPosition(vTargetPosition);
	Vec3V vToTarget(Subtract(VECTOR3_TO_VEC3V(vTargetPosition), mtxReferenceDir.GetCol3())); 
	vToTarget = NormalizeFast(vToTarget); 

	//grcDebugDraw::Line(VEC3V_TO_VECTOR3(mtxReferenceDir.GetCol3()), VEC3V_TO_VECTOR3(mtxReferenceDir.GetCol3()+Scale(mtxReferenceDir.GetCol1(),ScalarV(10.0f))), Color_red);
	//grcDebugDraw::Line(VEC3V_TO_VECTOR3(mtxReferenceDir.GetCol3()), VEC3V_TO_VECTOR3(mtxReferenceDir.GetCol3()+Scale(vToTarget,ScalarV(10.0f))), Color_blue);
	
	bool bTargetAtLeft = false;
	if(IsLessThanAll(Dot(vToTarget, mtxReferenceDir.GetCol0()), ScalarV(V_ZERO)))
	{
		bTargetAtLeft = true;
	}

	const Tunables& tune = GetTunables();
	float fAngleLimit = bTargetAtLeft ? DtoR*tune.m_fAimToIdleAngleLimitLeft : DtoR*tune.m_fAimToIdleAngleLimitRight; 
	if(IsLessThanAll(Dot(vToTarget, mtxReferenceDir.GetCol1()), ScalarV(cosf(fAngleLimit))))
	{
		Vec3V vLimitDir = RotateAboutZAxis(mtxReferenceDir.GetCol1(), ScalarV(bTargetAtLeft ? fAngleLimit:-fAngleLimit));
		m_CurrentLookAtTarget.SetPosition(VEC3V_TO_VECTOR3(mtxReferenceDir.GetCol3()+Scale(vLimitDir,ScalarV(5.0f))));

		//grcDebugDraw::Line(VEC3V_TO_VECTOR3(mtxReferenceDir.GetCol3()), VEC3V_TO_VECTOR3(mtxReferenceDir.GetCol3()+Scale(vLimitDir,ScalarV(10.0f))), Color_green);
	}
}

////////////////////////////////////////////////////////////////////////////////

CAmbientEvent::CAmbientEvent()
{
	Clear();
}

CAmbientEvent::CAmbientEvent(AmbientEventType nType, Vec3V_In vPosition, float fTimeToLive, float fTimeToPlayAnimation)
: m_vPosition(vPosition)
, m_nType(nType)
, m_fTimeToLive(fTimeToLive)
, m_fTimeToPlayAnimation(fTimeToPlayAnimation)
, m_bReacting(false)
{

}

CAmbientEvent::~CAmbientEvent()
{

}

void CAmbientEvent::Clear()
{
	//Clear the values.
	m_nType			= AET_No_Type;
	m_vPosition		= Vec3V(V_ZERO);
	m_fTimeToLive	= 0.0f;
	m_bReacting		= false;
}

bool CAmbientEvent::IsReacting() const
{
	//Ensure the event is valid.
	if(!IsValid())
	{
		return false;
	}
	
	//Ensure we are reacting.
	if(!m_bReacting)
	{
		return false;
	}
	
	return true;
}

bool CAmbientEvent::IsValid() const
{
	//Ensure the type is valid.
	if(m_nType == AET_No_Type)
	{
		return false;
	}
	
	return true;
}

// Tick how long the ambient event should be valid for
// Note that the animation is at least valid until the animation is finished playing.
// Returns true if the event has expired.
bool CAmbientEvent::TickLifetime(float fTimeStep)
{
	m_fTimeToLive -= fTimeStep;
	return (m_fTimeToLive <= 0.0f);
}

// Tick how long the ambient event's response animation should be valid for
// Returns true if the animation is done.
bool CAmbientEvent::TickAnimation(float fTimeStep)
{
	//Decrease the time to use the event to play the animation.
	m_fTimeToPlayAnimation -= fTimeStep;
	
	return (m_fTimeToPlayAnimation <= 0.0f);
}

////////////////////////////////////////////////////////////////////////////////

// Static constants
bank_bool  CTaskAmbientClips::BLOCK_ALL_AMBIENT_IDLES					= false;

#if SCENARIO_DEBUG
bool CTaskAmbientClips::sm_DebugPreventBaseAnimStreaming = false;
#endif // SCENARIO_DEBUG

// Static tuning data
CTaskAmbientClips::Tunables CTaskAmbientClips::sm_Tunables;
IMPLEMENT_DEFAULT_TASK_TUNABLES(CTaskAmbientClips, 0xa6627535);

CTaskAmbientClips::CTaskAmbientClips( u32 iFlags, const CConditionalAnimsGroup * pClips, s32 ConditionalAnimChosen /*=-1*/, CPropManagementHelper* propHelper /*=NULL*/ )
: m_pLookAt(NULL)
, m_pConversationHelper(NULL)
, m_pConditionalAnimsGroup(pClips)
, m_fPlayerNearbyTimer(-1.0f)
, m_fNextIdleTimer(-1.0f)
, m_fTimeLastSearchedForBaseClip(0.0f)
, m_fBlendInDelta(SLOW_BLEND_IN_DELTA)
, m_fCleanupBlendOutDelta(SLOW_BLEND_OUT_DELTA)
, m_fBlendOutPhase(1.0f)
, m_ClipSetId(CLIP_SET_ID_INVALID)
, m_ClipId(CLIP_ID_INVALID)
, m_PropClipId(CLIP_ID_INVALID)
, m_VfxStreamingSlot(-1)
, m_ConditionalAnimChosen((s16)ConditionalAnimChosen)
, m_BaseAnimImmediateCondition(NULL)
, m_IdleAnimImmediateCondition(NULL)
, m_fPlayerConversationScenarioDistSq(FLT_MAX)
, m_Flags(ConditionalAnimChosen >= 0 ? iFlags | Flag_UserSpecifiedConditionalAnimChosen : iFlags)
, m_BaseSyncedSceneId(INVALID_SYNCED_SCENE_ID)
, m_ExternalSyncedSceneId(INVALID_SYNCED_SCENE_ID)
, m_IdleSyncedSceneId(INVALID_SYNCED_SCENE_ID)
, m_UpdatesUntilNextConditionalAnimCheck(0)
, m_BlendOutRangeTagsFound(false)
, m_LowLodBaseClipPlaying(false)
, m_LowLodBaseClipConsidered(false)
, m_SlaveReadyForSyncedIdleAnim(false)
, m_VfxInRange(false)
, m_VfxRequested(false)
, m_VfxTagsExistBase(false)
, m_VfxTagsExistIdle(false)
, m_PropTagsExistBase(false)
, m_PropTagsExistIdle(false)
, m_ReachedEndOfBaseAnim(false)
, m_bUseBaseClipForHeadIKCheck(true)
, m_bForceReevaluateChosenConditionalAnim(false)
, m_bIdleClipLoaded(false)
, m_bCanHaveArgument(false)
, m_CloneInitialConditionalAnimChosen(ConditionalAnimChosen)
, m_pPropHelper(propHelper)
, m_bShouldBlockIdles(false)
, m_bShouldRemoveIdles(false)
, m_fBeatTimeS(-1.0f)
, m_fBPM(-1.0f)
, m_iBeatNumber(-1)
, m_bAtBeatMarker(false)
, m_bAtRockOutMarker(false)
, m_bPlayBaseHeadbob(false)
, m_sRockoutStart(0)
, m_sRockoutEnd(0)
, m_iTimeLeftToRockout(0)
, m_iVehVelocityTimerStart(-1)
, m_fRandomBaseDuration(0.0f)
, m_fRandomBaseStartTime(0.0f)
, m_fRandomBaseTimer(0.0f)
, m_bBlendOutHeadbob(false)
, m_bNextBeat(false)
, m_bReevaluateHeadbobBase(false)
, m_fTimeToPlayBaseAnimBeforeAfterRockout(-1.0f)
, m_sBlockedRockoutSection(-1)
, m_bBlockHeadbobNextFrame(false)
, m_vClonePreviousLookAtPosInVehicle(Vector3::ZeroType)
, m_fCloneLookAtTimer(0.0f)
, m_bUsingAlternateLowriderAnims(false)
, m_bInsertedLowriderCallbacks(false)
#if __BANK
, m_DebugStartedBaseAnim(false)
#endif
{
	// Make sure it fit:
	taskAssert(ConditionalAnimChosen >= -0x8000 && ConditionalAnimChosen <= 0x7fff);

	// call reset in the constructor
	m_CloneInitialFlags = m_Flags;

	m_bFlaggedToPlayBaseClip = m_Flags.IsFlagSet(Flag_PlayBaseClip);

	taskAssertf( pClips, "Unknown conditional clips type");
	m_BaseClip.SetAssociatedTask(this);

	SetInternalTaskType(CTaskTypes::TASK_AMBIENT_CLIPS);
}

CTaskAmbientClips::~CTaskAmbientClips()
{
	taskAssert(m_IdleSyncedSceneId == INVALID_SYNCED_SCENE_ID);

	// CleanUp() should have cleaned this up.
	taskAssert(m_VfxStreamingSlot.Get() < 0);
	taskAssert(!m_VfxRequested);

	delete m_pLookAt;
	taskAssert(!m_pPropHelper || !m_pPropHelper->IsPropLoaded() || m_pPropHelper->IsPropSharedAmongTasks());	// CleanUp() should prevent this.
	taskAssert(!m_Flags.IsFlagSet(Flag_IgnoreLowPriShockingEvents));		// CleanUp() should prevent this too.
}

aiTask* CTaskAmbientClips::Copy() const
{
	CTaskAmbientClips* pTask = rage_new CTaskAmbientClips( m_Flags.GetAllFlags(), m_pConditionalAnimsGroup, m_ConditionalAnimChosen, m_pPropHelper );

	if( m_Flags.IsFlagSet( Flag_OverrideInitialIdleTime ) )
	{
		pTask->SetTimeTillNextClip( m_fNextIdleTimer );
	}

	pTask->SetSynchronizedPartner(m_SynchronizedPartner);
	pTask->SetBlendInDelta(m_fBlendInDelta);
	pTask->SetCleanupBlendOutDelta(m_fCleanupBlendOutDelta);

	return pTask;
}

#if !__NO_OUTPUT
atString CTaskAmbientClips::GetName() const
{
	atString name(CTask::GetName());

	// Get the animation context
	const CConditionalAnims* pAnims = NULL;
	if(m_pConditionalAnimsGroup)
	{
		if(m_ConditionalAnimChosen != -1)
		{
			if(m_ConditionalAnimChosen >= m_pConditionalAnimsGroup->GetNumAnims())
			{
				Assertf(m_ConditionalAnimChosen < m_pConditionalAnimsGroup->GetNumAnims(),
					"Invalid m_ConditionalAnimChosen = %d, for Anim group %s has %d anims",
					 m_ConditionalAnimChosen, m_pConditionalAnimsGroup->GetName(), m_pConditionalAnimsGroup->GetNumAnims());

				return name;
			}

			pAnims = m_pConditionalAnimsGroup->GetAnims(m_ConditionalAnimChosen);
			if(pAnims)
			{
				name += ":";
				name += pAnims->GetName();
			}

			if (m_pConversationHelper)
			{
				name += ":";
				name += m_pConversationHelper->GetStateName();
			}
		}
	}
	return name;
}
#endif // !__NO_OUTPUT

#if !__NO_OUTPUT
const char * CTaskAmbientClips::GetStaticStateName( s32 iState  )
{
	static const char* aStateNames[] =
	{
		"State_Start",
		"State_PickPermanentProp",
		"State_NoClipPlaying",
		"State_StreamIdleAnimation",
		"State_SyncFoot",
		"State_PlayIdleAnimation",
		"State_Exit",
	};

	return aStateNames[iState];
}
#endif // !__NO_OUTPUT

void CTaskAmbientClips::InitPropHelper()
{
	// if we don't have a prop helper,
	if (!m_pPropHelper)
	{
		// check if we already have one in our primary task tree
		m_pPropHelper = GetPed()->GetPedIntelligence()->GetActivePropManagementHelper();
	}
	// if we still don't have a prop helper
	if (!m_pPropHelper)
	{
		// create a prop helper
		m_pPropHelper = rage_new CPropManagementHelper();
	}

	// if we have a parent CTaskWander and it does not already have a prop helper
	CTaskWander* pWanderTask = static_cast<CTaskWander*>(GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_WANDER));
	if (pWanderTask && pWanderTask->GetPropHelper() == NULL)
	{
		// inform it that we have a prop helper
		pWanderTask->SetPropHelper(m_pPropHelper);
	}
}

void CTaskAmbientClips::FreePropHelper(bool bClearHelperOnWanderTask)
{
	// free the prop helper
	if (m_pPropHelper)
	{
		m_pPropHelper.free();
		m_pPropHelper = NULL;
	}

	// if we need to clear the helper on CTaskWander
	if (bClearHelperOnWanderTask)
	{
		// if we have a parent CTaskWander
		CTaskWander* pWanderTask = static_cast<CTaskWander*>(GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_WANDER));
		if (pWanderTask && pWanderTask->GetPropHelper() != NULL)
		{
			// inform it that we no longer have a prop helper
			pWanderTask->SetPropHelper(NULL);
		}
	}
}

bool CTaskAmbientClips::GivePedProp( const strStreamingObjectName& prop, bool bCreatePropInLeftHand, bool bDestroyPropInsteadOfDropping )
{
	InitPropHelper();
	m_pPropHelper->RequestProp(prop);
	m_pPropHelper->SetCreatePropInLeftHand(bCreatePropInLeftHand);
	m_pPropHelper->SetDestroyProp(bDestroyPropInsteadOfDropping);
	return m_pPropHelper->GivePedProp(*GetPed());
}

void CTaskAmbientClips::AddEvent(const CAmbientEvent& rEvent)
{
	//Note: Disabled these temporarily for MAGDEMO.
	//		The blend-outs are not looking good.
	static bool s_bDisabled = false;
	if(s_bDisabled)
	{
		return;
	}
	
	//Ensure the current event is not being reacted to.
	if(m_Event.IsReacting())
	{
		return;
	}
	
	//Assign the event.
	m_Event = rEvent;
}

CTaskAmbientClips::FSM_Return CTaskAmbientClips::ProcessPreFSM()
{
	CPed* pPed = GetPed();
	Assert(pPed);

	CNetObjPed     *netObjPed  = static_cast<CNetObjPed *>(pPed->GetNetworkObject());
	if(netObjPed)
	{
		netObjPed->SetTaskOverridingProps(netObjPed->IsClone());
	}

	if (m_pPropHelper)
	{
		//Process prop loading and tags on the idle clip
		CPropManagementHelper::Context c(*pPed);
		c.m_ClipHelper		= GetClipHelper();
		c.m_PropClipSetId	= m_ClipSetId;
		c.m_PropClipId		= m_PropClipId;
		m_pPropHelper->Process(c);

		//...and also the base clip.
		c.m_ClipHelper    = GetBaseClip();
		m_pPropHelper->Process(c);
	}

	// Before each state machine update, we set this to false, and if the task is
	// ready to play the synchronized idle animation, it gets set to true, and otherwise,
	// NotifySlaveNotReadyForSyncedIdleAnim() gets called to tell the master that
	// we are not ready.
	m_SlaveReadyForSyncedIdleAnim = false;

    // the ped intelligence LOD switching for local peds is done in a task that is not run as a clone task
    // so this has to be done here instead
    if(pPed->IsNetworkClone() && pPed->GetIsInVehicle())
    {
        CVehicle* pVehicle = pPed->GetMyVehicle();
		if( pVehicle)
		{
			// Only do this for certain vehicle types
			bool bIsHeli = pVehicle->InheritsFromHeli();
			bool bEnableTimeslicing = bIsHeli || pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR || pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN;
			bool bSetUnconditionalTimeslice = bIsHeli || (!pPed->PopTypeIsMission());
			if( bEnableTimeslicing )
			{
				CPedAILod& lod = pPed->GetPedAiLod();
				lod.ClearBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate);
				if( bSetUnconditionalTimeslice )
				{
					lod.SetUnconditionalTimesliceIntelligenceUpdate(true);
				}
			}
		}
    }

	// Update the conversation helper if we're in a conversation right now [7/26/2012 mdawe]
	if (m_pConversationHelper != NULL)
	{
		if (m_pConversationHelper->IsPhoneConversation() && GetClipHelper())
		{
			ScanClipForPhoneRingRequests();
		}
		m_pConversationHelper->Update();

		// The Flag_IsInConversation will be flipped when m_pConversationHelper moves to the ConversationOver state. [10/2/2012 mdawe]
		if (!m_Flags.IsFlagSet(Flag_IsInConversation))
		{
			if (m_pConversationHelper->IsPhoneConversation())
			{
				bool bRunningScenarioTask = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO) != NULL;

				if (m_pConversationHelper->IsFailure())
				{
					// If the phone conversation failed, quit as soon as possible. 
					RequestTermination();

					// Delete the phone object if it exists; can happen if the model is already loaded and the ped spawned with this ambient clip.
					// i.e., ForcedLoading was set in CPropManagementHelper::PickPermanentProp(). If it weren't so late in the project,
					// another option might be to skip the SetForcedLoading on mobile phone conversation ConditionalAnims. [7/8/2013 mdawe]

					// If we destroyed the prop while the ped is running the scenario task, then they wouldn't be able to play a transition animation.
					if (m_pPropHelper && m_pPropHelper->IsHoldingProp() && !bRunningScenarioTask)
					{
						m_pPropHelper->DisposeOfProp(*pPed);
					}
				}

				if (bRunningScenarioTask)
				{
					QuitAfterBase();
				}
				else
				{
					QuitAfterIdle();
				}
			}
			else if (m_pConversationHelper->IsHangoutConversation() || m_pConversationHelper->IsArgumentConversation())
			{
				delete m_pConversationHelper;
				m_pConversationHelper = NULL;
			}
		}
	}

	// Check if we are in a Breakout scenario, and if so, should we be moving on. [8/7/2012 mdawe]
	if (m_Flags.IsFlagSet(Flag_AllowsConversation))
	{
		PlayConversationIfApplicable();
	}

	if (m_Flags.IsFlagSet(Flag_QuitAfterBase) && (m_BaseClip.IsPlayingClip() && !m_LowLodBaseClipPlaying) && m_BaseClip.GetPhase() >= 0.99f)
	{
		RequestTermination();
	}

	//B*1566258: Head bob syncing to radio
	TUNE_GROUP_BOOL(HEAD_BOB_TUNE, bEnableHeadbobSync, true);
	if (bEnableHeadbobSync)
	{
		if (pPed->GetIsInVehicle() && !pPed->GetMyVehicle()->IsBikeOrQuad() && (!pPed->PopTypeIsMission() || pPed->IsPlayer()))
		{
			HeadbobSyncWithRadio(pPed);
		}
	}

	// Re-evaluate after weapon change.
	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_EquippedWeaponChanged))
	{
		m_bForceReevaluateChosenConditionalAnim = true;
	}

	// Re-evaluate clip if ped is/was using alternate lowrider leans (ie arm on window anims). 
	// Ensures we change ambient clips immediately when the status changes so we don't get any weird blends.
	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UsingAlternateLowriderLeans) != m_bUsingAlternateLowriderAnims)
	{
		m_bUsingAlternateLowriderAnims = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UsingAlternateLowriderLeans);
		m_bForceReevaluateChosenConditionalAnim = true;
		m_bReevaluateHeadbobBase = true;

#if __ASSERT
		gnetDebug3("LOWRIDER DEBUG: %s %s CTaskAmbientClips::ProcessPreFSM: CPED_CONFIG_FLAG_UsingAlternateLowriderLeans != m_bUsingAlternateLowriderAnims: Re-evaluate clips.", 
			GetPed()->GetDebugName(),
			GetPed()->IsNetworkClone()?"CLONE":"LOCAL");
#endif
	}

	return FSM_Continue;
}

void CTaskAmbientClips::HeadbobSyncWithRadio(CPed *pPed)
{
	// Don't process any headbobbing code if it's being blocked by script and we've stopped headbobbing.
	if ((pPed->GetPedResetFlag(CPED_RESET_FLAG_BlockHeadbobbingToRadio) || CTheScripts::GetPlayerIsOnAMission()) && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HeadbobToRadioEnabled))
	{
		return;
	}

	float fBPS = -1.0f;
	float fBPM = -1.0f;
	int iBeatNum = -1;
	s32 sTimeToNextRockout = -1;
	s32 sCurrentTrackPlayTime = -1;
	bool bAtRockoutSection = false;
	bool bValidStation = false;
#if __DEV
	TUNE_GROUP_BOOL(HEAD_BOB_TUNE, bEnableRadioDebugText, false);
#endif
	CVehicle *pVehicle = static_cast<CVehicle*>(pPed->GetVehiclePedInside());
	if (pVehicle)
	{
		audVehicleAudioEntity* pVehAudEntity = pVehicle->GetVehicleAudioEntity();
		if (pVehAudEntity)
		{
			const audRadioStation *pAudRadioStation = pVehAudEntity->GetRadioStation();
			if (pAudRadioStation)
			{
				if (pPed->IsPlayer() && NetworkInterface::IsGameInProgress())
				{
					// Every station is valid for headbobbing in MP for the player
					bValidStation = true;
				}
				else
				{
					// Use radio categories (defined in peds.meta) to determine what stations to headbob to
					RadioGenre radioGenre = pAudRadioStation->GetGenre();
					s32 sFirstGenre = -1;
					s32 sSecondGenre = -1;
					pPed->GetPedRadioCategory(sFirstGenre, sSecondGenre);
					if (radioGenre == (RadioGenre)sFirstGenre || radioGenre == (RadioGenre)sSecondGenre)
					{
						bValidStation = true;
					}
				}
#if __DEV
				if (bEnableRadioDebugText)
				{
					grcDebugDraw::AddDebugOutput("HEADBOB-RADIO: Station Name: %s, Is valid for ped: %i", pVehAudEntity->GetRadioStation()->GetName(), bValidStation);
				}
#endif
				audRadioTrack currentTrack = pAudRadioStation->GetCurrentTrack();
				if (currentTrack.HasBeatInfo())
				{
					currentTrack.GetNextBeat(m_fBeatTimeS, m_fBPM, iBeatNum);
					fBPM = m_fBPM;
					fBPS = m_fBPM / 60.0f;
					const audRadioTrack::audDjSpeechRegion *rockOutRegion = currentTrack.FindNextRegion(audRadioTrack::audDjSpeechRegion::ROCKOUT);
					sCurrentTrackPlayTime = currentTrack.GetPlayTime();
					if (rockOutRegion && sCurrentTrackPlayTime != -1)
					{
						sTimeToNextRockout = (rockOutRegion->startMs - sCurrentTrackPlayTime);
						m_sRockoutStart = rockOutRegion->startMs;
						m_sRockoutEnd = rockOutRegion->endMs;
						if (sCurrentTrackPlayTime >= rockOutRegion->startMs && sCurrentTrackPlayTime < rockOutRegion->endMs && m_sBlockedRockoutSection != m_sRockoutStart)
						{
							m_sBlockedRockoutSection = -1;
							bAtRockoutSection = true;
							sTimeToNextRockout = (rockOutRegion->endMs - sCurrentTrackPlayTime);
						}
						else
						{
							bAtRockoutSection = false;
						}
					}
				}
			}
		}
	}

#if __DEV
	TUNE_GROUP_BOOL(HEAD_BOB_TUNE, bForceValidStation, false);
	if (!bValidStation && bForceValidStation)
	{
		bValidStation = true;
	}
#endif

	if (iBeatNum != m_iBeatNumber)
	{
		m_bNextBeat = true;
	}
	else 
	{
		m_bNextBeat = false;
	}

	m_iBeatNumber = iBeatNum;

	TUNE_GROUP_BOOL(HEAD_BOB_TUNE, bUseRockOutMarkers, true);
	if (!bUseRockOutMarkers)
	{
		m_bAtRockOutMarker = true;
	}

	m_iTimeLeftToRockout = 0;

	bool bCanHeadbob = m_fBPM != -1.0f && !CNewHud::IsRadioWheelActive() && bValidStation && !m_bBlockHeadbobNextFrame;
	bool bWasHeadbobbing = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HeadbobToRadioEnabled);

	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_HeadbobToRadioEnabled, false);

	TUNE_GROUP_BOOL(HEAD_BOB_TUNE, bAlwaysPlayBase, false);
	if (!bAlwaysPlayBase)
	{
		if (bCanHeadbob)
		{
			TUNE_GROUP_FLOAT(HEAD_BOB_TUNE, fMinTimeToWaitBeforeStartingIdles, 0.0f, 0.0f, 100.0f, 0.01f);
			TUNE_GROUP_FLOAT(HEAD_BOB_TUNE, fMaxTimeToWaitBeforeStartingIdles, 2.5f, 0.0f, 100.0f, 0.01f);

			if (m_fTimeToPlayBaseAnimBeforeAfterRockout == -1.0f)
			{
				m_fTimeToPlayBaseAnimBeforeAfterRockout = fwRandom::GetRandomNumberInRange(fMinTimeToWaitBeforeStartingIdles, fMaxTimeToWaitBeforeStartingIdles);
				m_fTimeToPlayBaseAnimBeforeAfterRockout *= 1000.0f;
			}

#if __DEV
			if (bEnableRadioDebugText)
			{
				grcDebugDraw::AddDebugOutput("HEADBOB-RADIO: Time to next Rockout: %.2f, Current Music Time: %.2f, Time To Start Base: %.2f, Time To Stop Base: %.2f", ((float)sTimeToNextRockout * 0.001f), ((float)sCurrentTrackPlayTime * 0.001f), ((float)m_sRockoutStart * 0.001f), ((float)m_sRockoutEnd * 0.001f));
			}
#endif
			if (bAtRockoutSection)
			{
				m_bPlayBaseHeadbob = true;

				float fTimeToPlayBaseBeforeAfter = m_fTimeToPlayBaseAnimBeforeAfterRockout;

				m_sRockoutStart = m_sRockoutStart + (s32)fTimeToPlayBaseBeforeAfter;
				m_sRockoutEnd = m_sRockoutEnd - (s32)fTimeToPlayBaseBeforeAfter;

				m_iTimeLeftToRockout = m_sRockoutEnd - sCurrentTrackPlayTime; 
#if __DEV
				if (bEnableRadioDebugText)
				{
					grcDebugDraw::AddDebugOutput("HEADBOB-RADIO: In Rockout base period! Time left: %d", m_iTimeLeftToRockout);
					grcDebugDraw::AddDebugOutput("HEADBOB-RADIO: Random start/end time within rockout for idles: %.2f, %.2f", ((float)m_sRockoutStart * 0.001f), ((float)m_sRockoutEnd * 0.001f));
				}
#endif
				bool bWithinRockoutPeriod = sCurrentTrackPlayTime >= m_sRockoutStart && sCurrentTrackPlayTime <= m_sRockoutEnd;

				// We're in a rockout period, so set the rock out marker flag so we can play idle flourishes!
				if (sTimeToNextRockout != -1 && bWithinRockoutPeriod)
				{
					pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_HeadbobToRadioEnabled, true);

					m_bAtRockOutMarker = true; 
					m_fRandomBaseTimer = 0.0f;
#if __DEV
					if (bEnableRadioDebugText)
					{
						s32 sTimeLeftToRockoutIdles = m_sRockoutEnd - sCurrentTrackPlayTime; 
						grcDebugDraw::AddDebugOutput("HEADBOB-RADIO: In Rockout idles period! Time left: %d", sTimeLeftToRockoutIdles);
					}
#endif
				}
				else if (sTimeToNextRockout != -1 && !bWithinRockoutPeriod)
				{
					m_bAtRockOutMarker = false;
				}
			}
		}
	}
	else	// bAlwaysPlayBase == true
	{
		if (bCanHeadbob)
		{
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_HeadbobToRadioEnabled, true);
		}
	}

	// If script have blocked headbobbing, un-set the config flag.
	if (pPed->GetPedResetFlag(CPED_RESET_FLAG_BlockHeadbobbingToRadio) || CTheScripts::GetPlayerIsOnAMission())
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_HeadbobToRadioEnabled, false);
	}

	// Force reevaluate the conditional anims if the radio flag has changed
	bool bNeedToReevaluateConditionalAnim = bWasHeadbobbing != pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HeadbobToRadioEnabled);
	if (bNeedToReevaluateConditionalAnim)
	{
		ForceReevaluateConditionalAnim();
		m_bReevaluateHeadbobBase = true;

		// Only start head-bob if flag has been set (all conditions have been met; ie music is playing etc).
		if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HeadbobToRadioEnabled))
		{
			if (!m_bFlaggedToPlayBaseClip)
			{
				m_Flags.SetFlag(Flag_PlayBaseClip);
			}

			m_Flags.SetFlag(Flag_DontRandomizeBaseOnStartup); //Always start the clip with at phase 0.0f to help syncing

			// Ensure we blend out of anims once headbob flag is unset
			m_bBlendOutHeadbob = true;
		}
		else if (m_bBlendOutHeadbob)
		{
			m_bBlendOutHeadbob = false;

			// Don't play base clip if we weren't flagged to do so (ie for driver)
			if (!m_bFlaggedToPlayBaseClip)
			{
				m_Flags.ClearFlag(Flag_PlayBaseClip);
			}

			// We've stopped, blend out of the anims
			TUNE_GROUP_FLOAT(HEAD_BOB_TUNE, fBlendOutDelta, -4.0f, -10.0f, 10.0f, 0.01f);
			if (GetClipHelper())
			{
				GetClipHelper()->StopClip(fBlendOutDelta);	//REALLY_SLOW_BLEND_OUT_DELTA
			}
			StopBaseClip(fBlendOutDelta);	//REALLY_SLOW_BLEND_OUT_DELTA

			m_fTimeToPlayBaseAnimBeforeAfterRockout = -1.0f;
			m_bAtRockOutMarker = false;
			m_bPlayBaseHeadbob = false;
		}
	}

	// Update the beat marker flag and disable head IK
	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HeadbobToRadioEnabled))
	{
		// Block IK since the head is already animated in the headbob anims
		pPed->SetHeadIkBlocked();

		float fRate = CalculateHeadbobAnimRate();

		// Set rate for base/idle clips
		CClipHelper* pHelper = GetBaseClip();
		if (pHelper)
		{
			pHelper->SetRate(fRate);

			//Set at beat market flag so we know when to trigger idle flourish anim
			static fwMvBooleanId beatMarkerId(ATSTRINGHASH("BEAT_LOOP_MARKER", 0x7c8b78cb));
			m_bAtBeatMarker = pHelper->GetBlendHelper().GetBoolean(beatMarkerId);
		}

		//Set the idle clip rate if it's playing
		CClipHelper* pIdleClipHelper = GetClipHelper();
		if (pIdleClipHelper)
		{
			pIdleClipHelper->SetRate(fRate);
		}
	}
	else
	{
		m_bAtBeatMarker = false;
	}

	// Reset block flag.
	m_bBlockHeadbobNextFrame = false;
}

float CTaskAmbientClips::CalculateHeadbobAnimRate()
{
	float fRate = 1.0f;

	// Anims authored at 120 BPM
	TUNE_GROUP_FLOAT(HEAD_BOB_TUNE, ANIM_BPM, 120.0f, 0.0f, 200.0f, 0.01f);

	if (m_fBPM > 0)
	{
		fRate = m_fBPM / ANIM_BPM;
	}

	// B*2220701: Halve the anim rate if it's is over the threshold. Prevents the anim from playing insanely fast and keeps it in time with the music.
	TUNE_GROUP_FLOAT(HEAD_BOB_TUNE, RATE_SCALE_THRESHOLD, 1.25f, 0.0f, 5.0f, 0.01f);
	if (fRate > RATE_SCALE_THRESHOLD)
	{
		fRate *= 0.5f;
	}

#if __DEV
	TUNE_GROUP_BOOL(HEAD_BOB_TUNE, bEnableRateDebugText, false);
	if (bEnableRateDebugText)
	{
		float fBPS = m_fBPM / 60.0f;
		grcDebugDraw::AddDebugOutput("HEADBOB-RADIO MUSIC INFO: Beat: %.2f (%u/4 %.2f BPM, %.1f BPS)", m_fBeatTimeS, m_iBeatNumber, m_fBPM, fBPS);
		grcDebugDraw::AddDebugOutput("HEADBOB-RADIO CLIP RATE: %.2f / %.2f = %.2f", m_fBPM, ANIM_BPM, fRate);
		grcDebugDraw::AddDebugOutput("HEADBOB-RADIO ROCKOUT: At Marker: %i", m_bAtRockOutMarker);
	}
#endif

	//Ensure the rate isn't below zero or insanely high
	TUNE_GROUP_FLOAT(HEAD_BOB_TUNE, RATE_CLAMP_MAX, 2.0f, 0.0f, 5.0f, 0.01f);
	taskAssertf(fRate < RATE_CLAMP_MAX, "HEADBOB: Rate %.2f > %.2f! BPM: %.2f", fRate, RATE_CLAMP_MAX, m_fBPM);
	fRate = Clamp(fRate, 0.0f, RATE_CLAMP_MAX);
	return fRate;
}


bool CTaskAmbientClips::ProcessMoveSignals()
{
	// Return false if nothing is happening.
	bool bReturnValue = false;

	// If either of these bools are set, do the VFX update call and return true.
	if(m_VfxTagsExistIdle || m_VfxTagsExistBase)
	{
		UpdateAllAnimationVFX();
		bReturnValue = true;
	}
	else
	{
		m_VfxInRange = false;
	}

	// If we have a prop management helper and we have object tags
	if ((m_PropTagsExistIdle || m_PropTagsExistBase) && m_pPropHelper)
	{
		// set defaults for context
		CPropManagementHelper::Context c(*GetPed());
		c.m_PropClipSetId	= m_ClipSetId;
		c.m_PropClipId		= m_PropClipId;

		// if our idle clip has object tags
		if (m_PropTagsExistIdle)
		{
			// Process Move events/loading/tags for props on the idle clip
			c.m_ClipHelper		= GetClipHelper();
			m_pPropHelper->ProcessMoveSignals(c);
		}

		if (m_PropTagsExistBase)
		{
			//...and also the base clip.
			c.m_ClipHelper    = GetBaseClip();
			m_pPropHelper->ProcessMoveSignals(c);
		}

		// Return true since we want to always check for prop signals
		bReturnValue = true;
	}

	switch(GetState())
	{
	case State_StreamIdleAnimation:
		m_bIdleClipLoaded = m_AmbientClipRequestHelper.RequestClips();

		// If m_ReachedEndOfBaseAnim isn't already set (because we detected the end of the base animation
		// since the last full update), check if we are about to reach the end of the base animation.
		if(!m_ReachedEndOfBaseAnim)
		{
			CClipHelper* pClipHelper = m_BaseClip.GetClipHelper();
			if(pClipHelper)
			{
				if(pClipHelper->GetPhase() + (pClipHelper->GetPhaseUpdateAmount()*2.0f) > 1.0f)
				{
					// Signal to the regular update function for this state that we reached the end phase.
					m_ReachedEndOfBaseAnim = true;

					// Make sure we update the next frame, we need to start blending in the idle animation
					// without waiting for our regular timesliced update.
					GetPed()->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
				}
			}
		}
		bReturnValue = true;
		break;
	}

	// Return false if nothing is going on, possibly stopping further calls to ProcessMoveSignals().
	return bReturnValue;
}


CTaskAmbientClips::FSM_Return CTaskAmbientClips::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	if(NetworkInterface::IsGameInProgress())
	{
		if(MigrateUpdateFSM(iState,  iEvent))
		{
			return FSM_Continue;
		}
		
		if (iState == State_Start && iEvent == OnUpdate)
		{
			if(GetTimeInState() > 10.0f) 
			{
				taskAssertf(0, "%s was stuck in State_Start too long quitting out! AL_LodTimesliceIntelligenceUpdate %s GetUnconditionalTimesliceIntelligenceUpdate %s Migrate helper active state %d", 
					GetPed()->GetDebugName(),
					GetPed()->GetPedAiLod().IsBlockedFlagSet(CPedAILod::AL_LodTimesliceIntelligenceUpdate)?"TRUE":"FALSE",
					GetPed()->GetPedAiLod().GetUnconditionalTimesliceIntelligenceUpdate()?"TRUE":"FALSE",
					GetTaskAmbientMigrateHelper()?GetTaskAmbientMigrateHelper()->m_State:-1 );

				return FSM_Quit;
			}
		}
	}

	FSM_Begin
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();
		FSM_State(State_PickPermanentProp)
			FSM_OnEnter
				return PickPermanentProp_OnEnter();
			FSM_OnUpdate
				return PickPermanentProp_OnUpdate();
		FSM_State(State_NoClipPlaying)
			FSM_OnEnter
				return NoClipPlaying_OnEnter();
			FSM_OnUpdate
				return NoClipPlaying_OnUpdate();
		FSM_State(State_StreamIdleAnimation)
			FSM_OnEnter
				return StreamIdleAnimation_OnEnter();
			FSM_OnUpdate
				return StreamIdleAnimation_OnUpdate();
			FSM_OnExit
				return StreamIdleAnimation_OnExit();
		FSM_State(State_SyncFoot)
			FSM_OnUpdate
				return SyncFoot_OnUpdate();
		FSM_State(State_PlayIdleAnimation)
			FSM_OnEnter
				return PlayingIdleAnimation_OnEnter();
			FSM_OnUpdate
				return PlayingIdleAnimation_OnUpdate();
			FSM_OnExit
				return PlayingIdleAnimation_OnExit();
		FSM_State(State_Exit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

CTaskAmbientClips::FSM_Return CTaskAmbientClips::ProcessPostFSM()
{
	// Update the prop we are holding
	UpdateProp();

	// Updates the base clip (prop holding or full body)
	UpdateBaseClip();

	CPed* pPed = GetPed();

	// Insert lowrider arm callbacks.
	if (!m_bInsertedLowriderCallbacks && pPed && ShouldInsertLowriderArmNetwork())
	{
		m_bInsertedLowriderCallbacks = true;
		pPed->GetMovePed().GetBlendHelper().SetInsertCallback(MakeFunctorRet(*this, &CTaskAmbientClips::InsertLowriderArmNetworkCB));
		pPed->GetMovePed().GetBlendHelper().SetClipBlendInCallback(MakeFunctor(*this, &CTaskAmbientClips::BlendInLowriderClipCB));
		pPed->GetMovePed().GetBlendHelper().SetClipBlendOutCallback(MakeFunctor(*this, &CTaskAmbientClips::BlendOutLowriderClipCB));
	
#if __ASSERT
		gnetDebug3("LOWRIDER DEBUG: %s %s CTaskAmbientClips::ProcessPostFSM: Insert lowrider arm callbacks", 
			GetPed()->GetDebugName(),
			GetPed()->IsNetworkClone()?"CLONE":"LOCAL");
#endif
	}
	else if (m_bInsertedLowriderCallbacks && pPed && !ShouldInsertLowriderArmNetwork())
	{
		// No longer using alt clipset, reset callbacks.
		pPed->GetMovePed().ResetBlendHelperCallbacks();
		m_bInsertedLowriderCallbacks = false;

#if __ASSERT
		gnetDebug3("LOWRIDER DEBUG: %s %s CTaskAmbientClips::ProcessPostFSM: Reset lowrider arm callbacks", 
			GetPed()->GetDebugName(),
			GetPed()->IsNetworkClone()?"CLONE":"LOCAL");
#endif
	}

	CreateLookAt(pPed);

	// Update the look at clips
	if(m_pLookAt)
	{
		m_pLookAt->SetCanSayAudio(m_Flags.IsFlagSet(Flag_CanSayAudioDuringLookAt));
		m_pLookAt->SetUseExtendedDistance(m_Flags.IsFlagSet( Flag_UseExtendedHeadLookDistance ));
		m_pLookAt->SetCanLookAtMyVehicle(m_Flags.IsFlagSet( Flag_CanLookAtMyVehicle ));
		CAmbientLookAt::LookAtBlockingState iBlockingState = m_pLookAt->UpdateLookAt(pPed, this, GetTimeStep());
		if (iBlockingState == CAmbientLookAt::LBS_BlockLookAts)
		{
			// Lookat is disabled, so recycle the object for the ped so someone else can use it.
			delete m_pLookAt;
			m_pLookAt = NULL;
		}
	}
	
	//If the current event is stale, clear it.  Only do this if we are not reacting to it.
	//We don't want events that haven't been processed to stick around.
	if (!m_Event.IsReacting() && m_Event.IsValid())
	{
		if(m_Event.TickLifetime(GetTimeStep()))
		{
			//Clear the event.
			m_Event.Clear();
		}
	}

	if(m_Flags.IsFlagSet(Flag_SynchronizedMaster))
	{
		// Clear this flag, so we can keep track of if any slaves
		// told us they are not ready, for the next update.
		m_Flags.ClearFlag(Flag_SlaveNotReadyForSyncedIdleAnim);
	}
	else if(m_Flags.IsFlagSet(Flag_SynchronizedSlave))
	{
		// If m_SlaveReadyForSyncedIdleAnim didn't get set during the update,
		// tell the master we are not ready. This may seem a little backwards (we could
		// potentially tell the master that we are ready instead, or the master could
		// ask us), but it's done this way to work in a multi-slave scenario, if we
		// ever need one, without the master needing to know who the slaves are.
		if(!m_SlaveReadyForSyncedIdleAnim)
		{
			CTaskAmbientClips* pMasterTask = FindSynchronizedMasterTask();
			if(pMasterTask)
			{
				pMasterTask->NotifySlaveNotReadyForSyncedIdleAnim();
			}
		}
	}

	// Clear flags set by script. 
	m_bShouldBlockIdles = false;
	m_bShouldRemoveIdles = false;

	return FSM_Continue;
}

void CTaskAmbientClips::CleanUp()
{
	taskAssert(m_IdleSyncedSceneId == INVALID_SYNCED_SCENE_ID);

	// Cache the ped
	CPed* pPed = GetPed();

	if ( m_pPropHelper )
	{
#if __ASSERT
		if( NetworkInterface::IsGameInProgress() && m_pPropHelper->GetPropNameHash().IsNotNull() && !m_pPropHelper->IsPropSharedAmongTasks())
		{
			CTaskAmbientMigrateHelper* pTAMH = GetTaskAmbientMigrateHelper();
			if(	pTAMH && pTAMH->GetHasMigrateData() )
			{
				taskAssertf(0,"%s prop %s getting released while migrating\n",pPed->GetDebugName(),m_pPropHelper->GetPropNameHash().GetCStr());
			}
		}
#endif

		m_pPropHelper->ReleasePropRefIfUnshared(pPed);
		
		// don't free our helper on CTaskWander
		const bool bFreeOnTaskWander = false;
		FreePropHelper(bFreeOnTaskWander);
	}

// 	if(m_VfxRequested)
// 	{
// 		if(m_VfxStreamingSlot.Get() >= 0)
// 		{
// 			CConditionalAnimStreamingVfxManager::GetInstance().UnrequestVfxAssetSlot(m_VfxStreamingSlot.Get());
// 			m_VfxStreamingSlot = -1;
// 		}
// 		m_VfxRequested = false;
// 	}

	// Release any streamed clips
	m_AmbientClipRequestHelper.ReleaseClips();
	m_BaseClipRequestHelper.ReleaseClips();

	// Reset lowrider low-arm callbacks if set.
	if (m_bInsertedLowriderCallbacks)
	{
		pPed->GetMovePed().ResetBlendHelperCallbacks();
	}

	// Remove any clips
	u32 uFlags = CMovePed::Task_IgnoreMoverBlendRotation;

	StopClip(m_fCleanupBlendOutDelta, uFlags);
	m_BaseClip.StopClip(m_fCleanupBlendOutDelta, uFlags);

	CNetObjPed     *netObjPed  = static_cast<CNetObjPed *>(pPed->GetNetworkObject());

	if(netObjPed && netObjPed->GetTaskOverridingProps())
	{
		//If migrating from clone wait till the start of the  
		//migrate to next TaskAmbient to clear this flag in ProcessPreFSM
		if (!GetIsFlagSet(aiTaskFlags::HandleCloneSwapToSameTaskType))
		{   
			netObjPed->SetTaskOverridingProps(false);
		}
	}

	// If we've been overriding the whole clip set (for full body animations), make sure to undo.
	if(m_Flags.IsFlagSet(Flag_HasOnFootClipSetOverride))
	{
		CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();
		if(pTask)
		{
			pTask->ResetOnFootClipSet(m_Flags.IsFlagSet(Flag_ForceFootClipSetRestart));
		}
		m_Flags.ClearFlag(Flag_HasOnFootClipSetOverride);
	}

	// Make sure we don't leave a count on CPedIntelligence::m_iIgnoreLowPriShockingEventsCount.
	SetIgnoreLowPriShockingEvents(false);

	delete m_pConversationHelper;
	m_pConversationHelper = NULL;

	// Clean the reference
	pPed->SetCarJacker(NULL);

	// Clear flags set by script. 
	m_bShouldBlockIdles = false;
	m_bShouldRemoveIdles = false;

	if(NetworkInterface::IsGameInProgress())
	{
		CClipHelper* baseClipHelper = GetBaseClip();
		if(baseClipHelper && baseClipHelper->GetBlendHelper().IsNetworkReadyForInsert())
		{
			gnetDebug3("%s CTaskAmbientClips::CleanUp Network did not get inserted! ReleaseNetworkPlayer FC: %d",
				GetPed()?GetPed()->GetDebugName():"null ped",
				fwTimer::GetFrameCount());

			baseClipHelper->GetBlendHelper().ReleaseNetworkPlayer();
		}
	}
}

CTaskAmbientClips::FSM_Return CTaskAmbientClips::Start_OnUpdate()
{
	// Set if this Ped could be doing a Hangout chat. We can't call this from the constructor, because the
	// task isn't running yet, which means GetPed() can't be used. [8/1/2012 mdawe]
	CPed* pPed = GetPed();
	if (pPed)
	{
		if (m_Flags.IsFlagSet(Flag_AllowsConversation)) 
		{
			//Do we have the correct audio context to play a conversation?
			audSpeechAudioEntity* pAudioEntity = pPed->GetSpeechAudioEntity();
			if ( !(pAudioEntity && pAudioEntity->DoesContextExistForThisPed("CHAT_STATE")) )
			{
				m_Flags.ClearFlag(Flag_AllowsConversation);
			}

			DEV_ONLY(static bool bForceArgument  = false;)
			if (fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < CTaskAmbientClips::sm_Tunables.m_fArgumentProbability DEV_ONLY(|| bForceArgument))
			{
				if ( pAudioEntity && pAudioEntity->SetPedVoiceForContext("PROVOKE_GENERIC") )
				{
					m_bCanHaveArgument = true;
				}
			}
		}
	}

	if (m_pPropHelper)
	{
		// If we're restarting, make sure this is clear [9/17/2012 mdawe]
		// TODO: Check to see if this is still needed? We may not ever hit this case anymore
		m_pPropHelper->ReleasePropRefIfUnshared(pPed);
	}
	else 
	{
		// if we don't have a prop helper, check if we already have one in our primary task tree
		if(pPed && pPed->GetPedIntelligence())
		{
			m_pPropHelper = pPed->GetPedIntelligence()->GetActivePropManagementHelper();
		}
	}

	// If the assigned conditionalAnimsGroup has no animations in it, 
	// check to see if there's a ConditionalAnim we could play to match our currently held prop. 
	// This is to support wandering-prop animations when CTaskWanderingScenario is walking to a "Walk" scenario point.
	if (pPed && m_pConditionalAnimsGroup && m_pConditionalAnimsGroup->GetNumAnims() == 0 && m_pPropHelper)
	{
		FindAndSetBestConditionalAnimsGroupForProp();
	}

	// We couldn't call SetConditionalAnimChosen() from the constructor because at that point,
	// the task isn't running and GetPed() can't be used. Instead, we call it here,
	// temporarily forcing m_ConditionalAnimChosen back to -1 first to detect a change.
	int condAnimsChosen = m_ConditionalAnimChosen;
	if(condAnimsChosen >= 0)
	{
		m_ConditionalAnimChosen = -1;
		SetConditionalAnimChosen(condAnimsChosen);
	}

	if (ShouldInsertLowriderArmNetwork(true) && !m_LowriderArmMoveNetworkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkTaskAmbientLowriderArm))
	{
		m_LowriderArmMoveNetworkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskAmbientLowriderArm);
	}

	if( m_Flags.IsFlagSet(Flag_PickPermanentProp) )
	{
		SetState(State_PickPermanentProp);
	}
	else
	{
		SetState(State_NoClipPlaying);
	}

	return FSM_Continue;
}

CTaskAmbientClips::FSM_Return CTaskAmbientClips::PickPermanentProp_OnEnter()
{
	InitPropHelper();

	// It's possible to already have a prop loaded if TaskUseScenario created one when playing an Enter anim.
	//  For example, if script starts a scenario and forces an enter, we could end up here with a prop loaded. 
	//  [11/15/2012 mdawe]
	if (!m_pPropHelper->IsPropLoaded())
	{
		if ( !m_pPropHelper->PickPermanentProp(GetPed(), m_pConditionalAnimsGroup, m_ConditionalAnimChosen, m_Flags.IsFlagSet(Flag_ForcePropInPickPermanent)) )
		{
			// free our helper on CTaskWander
			const bool bFreeOnTaskWander = true;
			FreePropHelper(bFreeOnTaskWander);
		}
	}

	// if we have a prop management helper
	if (m_pPropHelper)
	{
		// process move signals
		RequestProcessMoveSignalCalls();
	}
	return FSM_Continue;
}

CTaskAmbientClips::FSM_Return CTaskAmbientClips::PickPermanentProp_OnUpdate()
{
	SetState(State_NoClipPlaying);
	return FSM_Continue;
}

CTaskAmbientClips::FSM_Return CTaskAmbientClips::NoClipPlaying_OnEnter()
{
	InitIdleTimer();

	// Make sure we are allowed to check conditions on the first update, if we haven't
	// picked idle animations to use yet.
	m_UpdatesUntilNextConditionalAnimCheck = 0;

	// re-evaluate the first time we start playing to make sure that our anims can be used.
	m_bForceReevaluateChosenConditionalAnim = true;
	
	return FSM_Continue;
}

CTaskAmbientClips::FSM_Return CTaskAmbientClips::NoClipPlaying_OnUpdate()
{
	CPed* pPed = GetPed();
	Assert(pPed);

	if (GetIsFlagSet(aiTaskFlags::TerminationRequested))
	{
		// We don't want to leave this state potentially if a termination has been requested.
		if (!pPed->IsPlayingAGestureAnim())
		{
			SetState(State_Exit);
		}
		return FSM_Continue;
	}

#if FPS_MODE_SUPPORTED
	// If the ped is in first person mode we should not play any idle variations
	if( pPed->IsFirstPersonShooterModeEnabledForPlayer(false) )
		return FSM_Continue;
#endif

	CScenarioCondition::sScenarioConditionData conditionData;
	InitConditionData(conditionData);

	// Check if we are the slave in a synchronized animation setup,
	// and if so, if the master wants to start a synchronized idle animation.
	bool playSlaveIdle = false;
	if(m_Flags.IsFlagSet(Flag_SynchronizedSlave))
	{
		// Find the master task, and see if it wants to play synchronized idles.
		const CTaskAmbientClips* pMasterTask = FindSynchronizedMasterTask();
		if(pMasterTask)
		{
			// Check if the master task is 
			if(pMasterTask->IsRequestingSlavesToPlayIdle())
			{
				playSlaveIdle = true;
			}
		}

		// If the master is requesting us to play a synchronized idle animation,
		// we must not pick a non-synchronized idle animation. Conversely, if the master
		// is not telling us to play a synchronized idle, we should not pick one.
		if(playSlaveIdle)
		{
			conditionData.m_AllowNotSynchronized = false;
		}
		else
		{
			conditionData.m_AllowSynchronized = false;
		}
	}

	//! Check current conditions; they may have changed. In which case, we need to reset chosen ID.
	if(m_bForceReevaluateChosenConditionalAnim)
	{
		if(m_ConditionalAnimChosen >= 0)
		{
			if(pPed->IsNetworkClone())
			{   //Don't wait for the check of valid prop to be associated with ped as this can be missed 
				//when ped is created and ped can be left stuck with a 10 second wait for next base clip check
				conditionData.iScenarioFlags |= SF_IgnoreHasPropCheck;
			}

			const CConditionalAnims * pAnims = m_pConditionalAnimsGroup->GetAnims(m_ConditionalAnimChosen);
			if(pAnims && !pAnims->CheckConditions(conditionData, NULL))
			{
				m_ConditionalAnimChosen = -1;
			}
		}

		m_bForceReevaluateChosenConditionalAnim = false;
	}

 	bool bChooseAnimsAgain=false; 
 	if (m_Flags.IsFlagSet(Flag_ForcePickNewIdleAnim))
 	{
 		bChooseAnimsAgain = true;
 		m_UpdatesUntilNextConditionalAnimCheck = 0;
 	}

	if (m_ConditionalAnimChosen == -1 || bChooseAnimsAgain)
	{
		// If we have recently failed to pick conditional animations with an idle animation,
		// just keep counting down until we are allowed to try again, for performance
		// reasons.
		if(m_UpdatesUntilNextConditionalAnimCheck > 0)
		{
			m_UpdatesUntilNextConditionalAnimCheck--;
		}
		else
		{
			int animsChosen = m_ConditionalAnimChosen;
			s32 iChosenGroupPriority = 0;	// Not actually used.
			CAmbientAnimationManager::ChooseConditionalAnimations(conditionData, iChosenGroupPriority, m_pConditionalAnimsGroup, animsChosen, CConditionalAnims::AT_VARIATION);
			SetConditionalAnimChosen(animsChosen);

			if(animsChosen < 0)
			{
				// MAGIC! Wait for 30 frames (or timesliced updates) before checking again.
				m_UpdatesUntilNextConditionalAnimCheck = 30;
			}
		}
	}

#if SCENARIO_DEBUG
	bool debugOverride = false;
	if (m_Flags.IsFlagSet(Flag_UseDebugAnimData) && 
			(m_DebugInfo.m_AnimType == CConditionalAnims::AT_VARIATION || 
			m_DebugInfo.m_AnimType == CConditionalAnims::AT_REACTION ||
			m_DebugInfo.m_AnimType == CConditionalAnims::AT_COUNT) //this one added to allow proper exiting once we are done playing the clip
		)
	{
		debugOverride = true;
	}
#endif // SCENARIO_DEBUG

	// Still no conditional clips chosen, then no actual clip set can be chosen
	if (m_ConditionalAnimChosen == -1 SCENARIO_DEBUG_ONLY( && !debugOverride))
	{
		// Enable gesture anims if no idle anims are playing.
		if(pPed->GetIsInVehicle())
		{
			pPed->SetGestureAnimsAllowed(true);
		}

		return FSM_Continue;
	}

	// Choose a random clip group if none is selected
#if __ASSERT
	CNetObjPed     *netObjPed  = static_cast<CNetObjPed *>(pPed->GetNetworkObject());

	taskFatalAssertf( m_AmbientClipRequestHelper.GetState() == CAmbientClipRequestHelper::AS_noGroupSelected, "%s Clip requested in an incorrect state! %d",netObjPed?netObjPed->GetLogName():"Null netObjPed",m_AmbientClipRequestHelper.GetState() );
#endif

	const float fTimeStep = GetTimeStep();
	IdleBlockingState iIdleBlockingState = GetIdleBlockingState(fTimeStep);

	//Force a choice of a new idle anim
	if (m_Flags.IsFlagSet(Flag_IdleClipIdsOverridden) || m_Flags.IsFlagSet(Flag_ForcePickNewIdleAnim))
	{
		m_fNextIdleTimer = -1;
	}

	// Count down the next idle timer
	if( m_fNextIdleTimer > 0.0f && iIdleBlockingState == IBS_NoBlocking && ( !m_pLookAt || !m_pLookAt->GetCurrentLookAtTarget().GetIsValid() || pPed->IsAPlayerPed()) )
	{
		m_fNextIdleTimer -= fTimeStep;
	}

	//Check if there is a valid event.
	if(m_Event.IsValid())
	{
		//Set the condition position.
		conditionData.vAmbientEventPosition = m_Event.GetPosition();

		//Set the condition event type.
		conditionData.eAmbientEventType = m_Event.GetType();

		//Choose a reaction.
		const u32 uChosenPropHash = (m_pPropHelper ? static_cast<u32>(m_pPropHelper->GetPropNameHash()): 0);
		if(m_AmbientClipRequestHelper.ChooseRandomClipSet(conditionData, m_pConditionalAnimsGroup->GetAnims(m_ConditionalAnimChosen), CConditionalAnims::AT_REACTION, uChosenPropHash))
		{
			m_ClipSetId = m_AmbientClipRequestHelper.GetClipSet();

			//Note that we are reacting to the event.
			m_Event.SetReacting(true);

			//Play the animation.
			SetState(State_StreamIdleAnimation);
			return FSM_Continue;
		}
		else
		{
			//Clear the event, no reaction was found.
			m_Event.Clear();
		}
	}
	// If the timer is finished, try to find a new 
	// Note: not entirely sure how we want to deal with looking if we are playing synchronized animations
	// as a slave. Perhaps we should treat it simiarly to what we do with idle animations, i.e. let slaves
	// finish looking but not start any new looks, if the master wants to play a synchronized idle.
	// For now, we'll just kick off the idle even if we're looking.
	else if( playSlaveIdle || (m_fNextIdleTimer <= 0.0f) )
	{
		// If we have a permanent prop, we need to be careful to make sure it's the right one,
		// or the assert in StreamIdleAnimation_OnEnter() may fail.
		if(m_pPropHelper && m_pPropHelper->IsPropPersistent())
		{
			if(m_pConditionalAnimsGroup && m_ConditionalAnimChosen >= 0)
			{
				const CConditionalAnims * pAnims = m_pConditionalAnimsGroup->GetAnims(m_ConditionalAnimChosen);

				// Check the prop conditions. Note that these are not the conditions on the idle clip, as selected by
				// m_AmbientClipRequestHelper.ChooseRandomClipSet, but conditions on the whole CConditionalAnims
				// object. Specifically, this should include the requirement to hold a particular prop. 
				if(pAnims && !pAnims->CheckPropConditions(conditionData))
				{
					// The primary cause of ending up here would be if somebody gave us a different prop
					// external to the task. This could happen through debugging widgets and probably in
					// networking games, or because of bugs.

					// If the user didn't explicitly set m_ConditionalAnimChosen, clear it.
					// On the next update, it should get a new random value matching the current conditions.
					// If the user did set m_ConditionalAnimChosen, we might want to exit the task
					// (but we don't yet).
					if(!m_Flags.IsFlagSet(Flag_UserSpecifiedConditionalAnimChosen))
					{
						SetConditionalAnimChosen(-1);
					}

					// Restart the state, without playing the idle animation. InitIdleTimer() will be called,
					// so we shouldn't immediately try to play an idle animation again.
					SetFlag(aiTaskFlags::RestartCurrentState);

					return FSM_Continue;
				}
			}
		}

		// Try to select a group relevant for this context
		bool validChosen = false;
#if SCENARIO_DEBUG
		if (m_Flags.IsFlagSet(Flag_UseDebugAnimData))
		{
			// In the case that we have no next clip to play or we are 
			//  attempting to debug play a base clip only we should not pick a variation.
			if (m_DebugInfo.m_AnimType == CConditionalAnims::AT_VARIATION || m_DebugInfo.m_AnimType == CConditionalAnims::AT_REACTION)
			{
				m_AmbientClipRequestHelper.SetClipAndProp(m_DebugInfo.m_ClipSetId, m_DebugInfo.m_PropName.GetHash());
				validChosen = true;
			}
		}
		else

#endif // SCENARIO_DEBUG
		if (m_Flags.IsFlagSet(Flag_IdleClipIdsOverridden))
		{
			bool bAllowOverrideIdleClip = !NetworkInterface::IsGameInProgress() || (fwClipSetManager::GetClipSet(m_ClipSetId)!=NULL);

			if(taskVerifyf(bAllowOverrideIdleClip,"%s No clip while Flag_IdleClipIdsOverridden TRUE: m_ClipSetId %s", pPed->GetDebugName(),m_ClipSetId.TryGetCStr()))
			{
				m_AmbientClipRequestHelper.SetClipAndProp(m_ClipSetId, 0);
				validChosen = true;
			}
		}
		else
		{
			const u32 uChosenPropHash = (m_pPropHelper ? static_cast<u32>(m_pPropHelper->GetPropNameHash()): 0);
			validChosen = m_AmbientClipRequestHelper.ChooseRandomClipSet( conditionData, m_pConditionalAnimsGroup->GetAnims(m_ConditionalAnimChosen), CConditionalAnims::AT_VARIATION, uChosenPropHash);
		}

		if( validChosen )		
		{
			// Note: we could possibly add something here so that we never request an idle animation
			// to be streamed in if we are currently playing the low LOD fallback base clip, since that
			// should be considered higher priority. We probably wouldn't want to block all idle animations,
			// though, because there may be some in memory already that we could use, which could look
			// better than the fallback base clip.

			// If we're running a synchronized scene for the base clip, we need to leave
			// the CTaskSynchronizedScene subtask running while streaming in idle anims.
			if(m_BaseSyncedSceneId != INVALID_SYNCED_SCENE_ID)
			{
				SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
			}

			SetState(State_StreamIdleAnimation);
			return FSM_Continue;
		}
		// No clips selected - restart the timer
		else
		{
#if SCENARIO_DEBUG
			//If we dont have a next debug clip to play then we are done ... 
			if (m_Flags.IsFlagSet(Flag_UseDebugAnimData) && m_DebugInfo.m_AnimType == CConditionalAnims::AT_COUNT)
			{
				SetState(State_Exit);
				return FSM_Continue;
			}
#endif // SCENARIO_DEBUG
			InitIdleTimer();
		}
	}

	// Enable gesture anims if no idle anims are playing.
	if(pPed->GetIsInVehicle())
	{
		pPed->SetGestureAnimsAllowed(true);
	}

	return FSM_Continue;
}

CTaskAmbientClips::FSM_Return CTaskAmbientClips::StreamIdleAnimation_OnEnter()
{
	// If this fails, we probably didn't clean up properly after the previous idle.
	taskAssert(!m_IdleAnimImmediateCondition);

	// Reset our Idle Loaded flag
	m_bIdleClipLoaded = false;
	RequestProcessMoveSignalCalls(); // Need to keep the clip request around

	// Clear out the flag for that we reached the end of the base animation.
	m_ReachedEndOfBaseAnim = false;

	if( m_pPropHelper && m_pPropHelper->GetPropNameHash().GetHash() != 0 )
	{
#if __ASSERT	
		CPed* pPed = GetPed();

		const char* pedHoldingObject = "No";
		if(pPed->GetWeaponManager() && taskVerifyf(pPed->GetInventory(),"Expect inventory"))
		{
			const CWeaponItem* pItem = pPed->GetInventory()->GetWeapon( OBJECTTYPE_OBJECT );
			if( pItem )
			{
				pedHoldingObject = pItem->GetInfo() ? pItem->GetInfo()->GetName() : "NULL";		
			}
		}

		u32 propHeldByPedHash = 0;

		if( pPed->GetHeldObject(*GetPed()) && pPed->GetHeldObject(*pPed)->GetBaseModelInfo())
		{
			propHeldByPedHash = pPed->GetHeldObject(*pPed)->GetBaseModelInfo()->GetHashKey();
		}

		CNetObjPed     *netObjPed  = static_cast<CNetObjPed *>(pPed->GetNetworkObject());
		taskAssertf( !m_AmbientClipRequestHelper.RequiresProp() || m_AmbientClipRequestHelper.GetRequiredProp() == m_pPropHelper->GetPropNameHash(),
			"%s: New clip requires new prop [%s], but prop helper has permanent prop [%s], Ped holding object %s, ped holding object hash %d, Low Lod %s",
			netObjPed ? netObjPed->GetLogName():"NULL CNetObjPed",
			m_AmbientClipRequestHelper.GetRequiredProp().GetCStr(),
			m_pPropHelper->GetPropNameHash().GetCStr(),
			pedHoldingObject,
			propHeldByPedHash,
			pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodMotionTask)?"Yes":"No");
#endif
	}
	else
	{
		// If this fails, it can probably be fixed by calling m_pPropHelper->ReleaseProp().
		taskAssert(!m_pPropHelper || !m_pPropHelper->IsPropLoaded());

		if( m_AmbientClipRequestHelper.RequiresProp() )
		{
			InitPropHelper();

			m_pPropHelper->RequestProp(m_AmbientClipRequestHelper.GetRequiredProp());
			if (m_pConditionalAnimsGroup && m_ConditionalAnimChosen >= 0)
			{
				const CConditionalAnims* pConditionalAnims = m_pConditionalAnimsGroup->GetAnims( m_ConditionalAnimChosen );
				if ( pConditionalAnims )
				{
					m_pPropHelper->SetCreatePropInLeftHand( pConditionalAnims->GetCreatePropInLeftHand() );
					m_pPropHelper->SetDestroyProp( pConditionalAnims->ShouldDestroyPropInsteadOfDropping() );
				}
			}
		}
		else
		{
			// free our helper on CTaskWander
			const bool bFreeOnTaskWander = true;
			FreePropHelper(bFreeOnTaskWander);
		}
	}
	return FSM_Continue;
}

CTaskAmbientClips::FSM_Return CTaskAmbientClips::StreamIdleAnimation_OnUpdate()
{
	CPed* pPed = GetPed();
	Assert(pPed);

	if (GetIsFlagSet(aiTaskFlags::TerminationRequested) && !pPed->IsPlayingAGestureAnim())
	{
		SetState(State_Exit);
		return FSM_Continue;
	}

	if (m_bForceReevaluateChosenConditionalAnim)
	{
		SetState(State_NoClipPlaying);
		return FSM_Continue;
	}

	if(GetPed()->IsNetworkClone())
	{
		if(m_AmbientClipRequestHelper.GetState() == CAmbientClipRequestHelper::AS_noGroupSelected)
		{
#if __ASSERT
			CNetObjPed     *netObjPed  = static_cast<CNetObjPed *>(pPed->GetNetworkObject());

			Assertf(0,"%s, AS_noGroupSelected, Flags %d, Prop required: %d, m_CloneInitialConditionalAnimChosen %d, m_ConditionalAnimChosen %d",
				netObjPed ? netObjPed->GetLogName():"NULL CNetObjPed",
				m_Flags.GetAllFlags(),
				m_pPropHelper ? !m_pPropHelper->IsPropNotRequired() : false,
				m_CloneInitialConditionalAnimChosen,
				m_ConditionalAnimChosen);
#endif
			return FSM_Quit;
		}
	}

	// Check for streaming the props and clips
	// m_bIdleClipLoaded is updated in ProcessMoveSignals()
	bool bPropsReady  = m_pPropHelper ? (m_pPropHelper->IsPropReady()) : true;

	// Only allow the clip to begin once the base clip has reached the end
	// this is to allow smooth transitions between base clips and idles.
	bool bBaseClipAtEnd;
	if(m_BaseSyncedSceneId != INVALID_SYNCED_SCENE_ID)
	{
		if(fwAnimDirectorComponentSyncedScene::IsValidSceneId(m_BaseSyncedSceneId))
		{
			const float phase = fwAnimDirectorComponentSyncedScene::GetSyncedScenePhase(m_BaseSyncedSceneId);
			const float rate = fwAnimDirectorComponentSyncedScene::GetSyncedSceneRate(m_BaseSyncedSceneId);
			const float duration = fwAnimDirectorComponentSyncedScene::GetSyncedSceneDuration(m_BaseSyncedSceneId);
			const float updateAmount = rate*GetTimeStep();
			bBaseClipAtEnd = (phase*duration + updateAmount*2.0f > duration);		// Not sure about the 2, probably just a somewhat arbitrary factor like done for the unsynchronized case.
		}
		else
		{
			bBaseClipAtEnd = true;	// Seems like we're not playing anything after all, probably would be best to let idles start.
		}
	}
	else
	{
		if (m_Event.IsReacting())
		{
			// If reacting to an event, then consider the base clip as ended immediately.  Can't afford to wait until the base clip loop is done...
			// That could be a 2-4 second delay.
			bBaseClipAtEnd = true;
		}
		else
		{
			// Check if the base clip is about to end.
			// Note that if we are playing the low LOD fallback clip, we probably always want
			// to consider ourselves ready to start an idle.
			bBaseClipAtEnd = m_LowLodBaseClipPlaying || m_ReachedEndOfBaseAnim || m_BaseClip.GetClipHelper() == NULL;
		}
	}

	// We have now consumed m_ReachedEndOfBaseAnim set by ProcessMoveSignals(), set it back to false.
	m_ReachedEndOfBaseAnim = false;

#if SCENARIO_DEBUG
	// The m_fNextIdleTimer seems not to control when clips are played just when they are queued 
	//  So in the case where we want to debug play an variation we want to force it to play ... 
	if (m_Flags.IsFlagSet(Flag_UseDebugAnimData) && 
		(m_DebugInfo.m_AnimType == CConditionalAnims::AT_VARIATION || m_DebugInfo.m_AnimType == CConditionalAnims::AT_REACTION)
	   )
	{
		bBaseClipAtEnd = true;//force the clip to start.
	}
#endif // SCENARIO_DEBUG

	//If everything else is ready ignore the base clip at end check.
	if (m_Flags.IsFlagSet(Flag_IdleClipIdsOverridden))
	{
		bBaseClipAtEnd = true;

		// Need to give the clip a chance to play this frame.
		m_bIdleClipLoaded = m_AmbientClipRequestHelper.RequestClips(false);
	}

	bool canStart = m_bIdleClipLoaded && bPropsReady && bBaseClipAtEnd;

	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HeadbobToRadioEnabled))
	{
		TUNE_GROUP_BOOL(HEAD_BOB_TUNE, bUseAtBeatMarkers, true);
		if (!bUseAtBeatMarkers)
		{
			m_bAtBeatMarker = true;
		}
		canStart = m_bIdleClipLoaded && bPropsReady && (bBaseClipAtEnd || m_bAtBeatMarker);
	}

	// Deal with synchronized idle animations.
	if(m_Flags.IsFlagSet(Flag_SynchronizedMaster))
	{
		// If we are the master, check if the idle animation we are about to play is a synchronized one.
		if(m_AmbientClipRequestHelper.IsClipSetSynchronized())
		{
			// We shouldn't start a synchronized idle animations if the slaves are not ready.
			// As we might conceivably want to be able to handle more than one slave, each one
			// has a chance to say they are not ready, by setting Flag_SlaveNotReadyForSyncedIdleAnim.
			// However, if we just entered this state, we can't expect them to have had a chance to
			// react yet, so we always wait at least a small amount of time (could probably just be one frame).
			if(GetTimeInState() < 0.2f || m_Flags.IsFlagSet(Flag_SlaveNotReadyForSyncedIdleAnim))
			{
				// The slaves aren't ready yet.
				return FSM_Continue;
			}
		}
	}
	else if(m_Flags.IsFlagSet(Flag_SynchronizedSlave))
	{
		// Check if we are about to play a synchronized animation.
		if(m_AmbientClipRequestHelper.IsClipSetSynchronized())
		{
			// We are about to play a synchronized idle as a slave. In this case, we don't
			// wait for the end of our clip, as we need to start at the same time as the
			// master.
			canStart = m_bIdleClipLoaded && bPropsReady;

			if(!canStart)
			{
				// Still waiting for animations or props to be loaded up.
				return FSM_Continue;
			}

			// Tell the master that we are ready to go whenever now.
			m_SlaveReadyForSyncedIdleAnim = true;

			// Check to see if the master has started to play the animation, if not,
			// we have to hold off.
			const CTaskAmbientClips* pMasterTask = FindSynchronizedMasterTask();
			if(pMasterTask)
			{
				const int masterState = pMasterTask->GetState();
				if(masterState == State_PlayIdleAnimation)
				{
					// The master has started playing the idle, so should we.
				}
				else if(masterState == State_StreamIdleAnimation)
				{
					// The master is either waiting to stream in animations,
					// or waiting for the slaves to report that they are ready to go.
					return FSM_Continue;
				}
				else
				{
					// The master is no longer neither streaming nor playing the idle animation.
					// We probably should leave this state too, so we don't get stuck.
					SetState(State_NoClipPlaying);
					if(m_BaseSyncedSceneId != INVALID_SYNCED_SCENE_ID)
					{
						SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
					}
					return FSM_Continue;
				}
			}
		}
	}

	// If all assets are streamed and its a good time to play the clip, choose an clip from the dictionary
	if( canStart )
	{
		taskAssert(m_AmbientClipRequestHelper.GetClipSet() != CLIP_SET_ID_INVALID);

		u32 clipHash = m_ClipId.GetHash();

		bool validPicked = false;
		CPedIntelligence* pPedIntelligence = NULL;
		if (pPed)
		{
			pPedIntelligence = pPed->GetPedIntelligence();
		}

#if SCENARIO_DEBUG
		if (m_Flags.IsFlagSet(Flag_UseDebugAnimData))
		{
			aiAssert(m_DebugInfo.m_AnimType == CConditionalAnims::AT_VARIATION || m_DebugInfo.m_AnimType == CConditionalAnims::AT_REACTION);
			clipHash = m_DebugInfo.m_ClipId.GetHash();
			validPicked = true;
		}
		else
#endif // SCENARIO_DEBUG
		if (m_Flags.IsFlagSet(Flag_IdleClipIdsOverridden))
		{
			clipHash = m_ClipId.GetHash();
			//If we are overriding the clip we want to play then we need to not say we can can start to play 
			// the next clip until we are all lined up... 
			if (m_AmbientClipRequestHelper.GetClipSet() == m_ClipSetId)
			{
				validPicked = true;
			}
		}
		else
		{
			if( NetworkInterface::IsGameInProgress() && (m_AmbientClipRequestHelper.GetState() == CAmbientClipRequestHelper::AS_noGroupSelected || !m_AmbientClipRequestHelper.CheckClipsLoaded()) )
			{
				Assertf(0,"%s m_AmbientClipRequestHelper clips not loaded. Clip set %s, state %d",
					CTaskAmbientMigrateHelper::GetAssertNetObjPedName(this),m_AmbientClipRequestHelper.GetClipSet().GetCStr(),m_AmbientClipRequestHelper.GetState());
				return FSM_Quit;
			}

			validPicked = CAmbientClipRequestHelper::PickRandomClipInClipSetUsingBlackBoard( pPed, m_AmbientClipRequestHelper.GetClipSet(), clipHash );
		}
		
		// Pick a particular random clip from the valid set
		if( validPicked )
		{
			// Store the clip set to play the clip from
			m_ClipSetId = m_AmbientClipRequestHelper.GetClipSet();
			m_ClipId = fwMvClipId(clipHash);

			// Set m_IdleAnimImmediateCondition, if the idle animation comes with conditions that
			// need to be evaluated frequently.
			taskAssert(!m_IdleAnimImmediateCondition);

			if (m_pConditionalAnimsGroup && m_ConditionalAnimChosen >= 0)
			{
				const CScenarioCondition* immediateConditions[1];
				const CConditionalAnims* pConditionalAnims = m_pConditionalAnimsGroup->GetAnims(m_ConditionalAnimChosen);

				//Choose the conditional anims type.
				CConditionalAnims::eAnimType nAnimType = !m_Event.IsReacting() ? CConditionalAnims::AT_VARIATION : CConditionalAnims::AT_REACTION;

				int numImmediateConditions = pConditionalAnims->GetImmediateConditions(m_ClipSetId, immediateConditions, NELEM(immediateConditions), nAnimType);
				if(numImmediateConditions)
				{
					m_IdleAnimImmediateCondition = immediateConditions[0];
				}
			}

			SetState(State_SyncFoot);
		}
		else
		{
			SetState(State_NoClipPlaying);

			// If we're running a synchronized scene for the base clip, we need to leave
			// the CTaskSynchronizedScene subtask running, since we failed to find an idle animation.
			if(m_BaseSyncedSceneId != INVALID_SYNCED_SCENE_ID)
			{
				SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
			}
		}
	}
	return FSM_Continue;
}

CTaskAmbientClips::FSM_Return CTaskAmbientClips::StreamIdleAnimation_OnExit()
{
	// If the state this is going to is playing the clip it will manage the streamed clips, if it isn't
	// release the streamed clips
	if( GetState() != State_SyncFoot )
	{
		m_AmbientClipRequestHelper.ReleaseClips();

		// If we're leaving this state and we are not going to play the idle, clear
		// the condition pointer.
		m_IdleAnimImmediateCondition = NULL;
	}
	return FSM_Continue;
}

CTaskAmbientClips::FSM_Return CTaskAmbientClips::SyncFoot_OnUpdate()
{
	// If the idle we're about to play has foot tags, wait to sync up with locomotion.
	fwClipSet* pClipSet = fwClipSetManager::GetClipSet(m_ClipSetId);
	if (pClipSet)
	{
		const crClip *pClip = pClipSet->GetClip(m_ClipId);
		if (pClip)
		{
			const crTag* pFirstFootTag = CClipEventTags::FindFirstEventTag(pClip, CClipEventTags::Foot);
			if (pFirstFootTag)
			{
				crProperty pFootTagProperty = pFirstFootTag->GetProperty();
				animAssert(pFootTagProperty.GetAttribute(CClipEventTags::Left) || pFootTagProperty.GetAttribute(CClipEventTags::Right));

				bool bStartsLeft = (NULL != pFootTagProperty.GetAttribute(CClipEventTags::Left));

				// Grab the locomotion task
				CTaskMotionBase* pTask = GetPed()->GetCurrentMotionTask();
				if( pTask && pTask->GetTaskType() == CTaskTypes::TASK_HUMAN_LOCOMOTION )
				{
					CTaskHumanLocomotion *pLocomotionTask = static_cast<CTaskHumanLocomotion*>(pTask);
					if (!pLocomotionTask->IsFootDown(bStartsLeft))
					{
						return FSM_Continue;
					}
				}
			}
		}
	}
	
	SetState(State_PlayIdleAnimation);
	return FSM_Continue;
}

CTaskAmbientClips::FSM_Return CTaskAmbientClips::PlayingIdleAnimation_OnEnter()
{
	if( !taskVerifyf(fwAnimManager::GetClipIfExistsBySetId( m_ClipSetId, m_ClipId ), "%s, Clip %s not found in set %s! Previous state %s"
			, GetPed()?GetPed()->GetDebugName():"NULL Ped"
			, m_ClipId != CLIP_ID_INVALID ? m_ClipId.GetCStr() : "CLIP_ID_INVALID"
			, m_ClipSetId != CLIP_SET_ID_INVALID ? m_ClipSetId.GetCStr() : "CLIP_SET_ID_INVALID"
			, GetStateName(GetPreviousState()) ) )
	{
		return FSM_Quit;
	}

	// Cache the ped
	CPed* pPed = GetPed();
	
	//Calculate the blend in delta.
	float fBlendInDelta;
	if(m_Event.IsReacting())
	{
		fBlendInDelta = REALLY_SLOW_BLEND_IN_DELTA;
	}
	else
	{
		fBlendInDelta = NORMAL_BLEND_IN_DELTA;
		if(m_pConditionalAnimsGroup && m_ConditionalAnimChosen!=-1)
		{
			const CConditionalAnims* pAnims = m_pConditionalAnimsGroup->GetAnims(m_ConditionalAnimChosen);
			if(pAnims)
			{
				fBlendInDelta = pAnims->GetBlendInDelta();
			}
		}
	}

	// Will be set from tags further down, if they are present.
	ResetBlendOutPhase();

	// Check if this is a synchronized animation we are about to play.
	if(m_AmbientClipRequestHelper.IsClipSetSynchronized())
	{
		// Determine the clip and dictionary, which we will need to pass in to CTaskSynchronizedScene.
		bool canStart = false;
		atHashString clipDictName;
		fwClipSet* pClipSet = fwClipSetManager::GetClipSet(m_ClipSetId);
		const crClip* pClip = pClipSet->GetClip(m_ClipId);
		if(pClip)
		{
			const crClipDictionary* pClipDict = pClip->GetDictionary();
			if(pClipDict)
			{
				clipDictName = pClipSet->GetClipDictionaryName();
				canStart = true;
			}
		}

		// First off, make sure we don't already have an idle synced scene ID set.
		taskAssert(m_IdleSyncedSceneId == INVALID_SYNCED_SCENE_ID);
		fwSyncedSceneId syncedScene = INVALID_SYNCED_SCENE_ID;

		bool startSyncedScene = false;
		if(taskVerifyf(canStart, "Unable to find clip and clip dictionary for synchronized idle animation."))
		{
			// If we are the master, make sure the user set m_ExternalSyncedSceneId. That's not what
			// we're going to use for the idle animations (as it's potentially used for the master), but
			// we need it for setting up a new scene.
			if(m_Flags.IsFlagSet(Flag_SynchronizedMaster)
					&& taskVerifyf(fwAnimDirectorComponentSyncedScene::IsValidSceneId(m_ExternalSyncedSceneId),
						"Invalid external synced scene ID."))
			{
				syncedScene = fwAnimDirectorComponentSyncedScene::StartSynchronizedScene();
				if(fwAnimDirectorComponentSyncedScene::IsValidSceneId(syncedScene))
				{
					fwAnimDirectorComponentSyncedScene::SetSyncedSceneLooped(syncedScene, false);
					fwAnimDirectorComponentSyncedScene::SetSyncedSceneHoldLastFrame(syncedScene, false);

					// TODO: Maybe adapt more stuff from the code path below?

					const float idleClipRate = fwRandom::GetRandomNumberInRange(0.9f, 1.1f);
					fwAnimDirectorComponentSyncedScene::SetSyncedSceneRate(syncedScene, idleClipRate);

					// Clone the origin from the base synced scene.
					Mat34V origin;
					fwAnimDirectorComponentSyncedScene::GetSyncedSceneOrigin(m_ExternalSyncedSceneId, RC_MATRIX34(origin));
					fwAnimDirectorComponentSyncedScene::SetSyncedSceneOrigin(syncedScene, RC_MATRIX34(origin));

					// Ready to go!
					startSyncedScene = true;
				}
			}
			else if(m_Flags.IsFlagSet(Flag_SynchronizedSlave))
			{
				// Get the idle synced scene ID from the master.
				const CTaskAmbientClips* pMasterTask = FindSynchronizedMasterTask();
				if(pMasterTask)
				{
					syncedScene = pMasterTask->m_IdleSyncedSceneId;
					if(fwAnimDirectorComponentSyncedScene::IsValidSceneId(syncedScene))
					{
						startSyncedScene = true;
					}
				}
			}
		}

		if(startSyncedScene)
		{
			if(fwAnimDirectorComponentSyncedScene::IsValidSceneId(syncedScene))
			{
				// This may not be strictly necessary:
				fwAnimDirectorComponentSyncedScene::AddSyncedSceneRef(syncedScene);

				// Not sure about the blend-out rate.
				const float fBlendOutDelta = NORMAL_BLEND_OUT_DELTA;
				eSyncedSceneFlagsBitSet flags;
				flags.BitSet().Set(SYNCED_SCENE_USE_KINEMATIC_PHYSICS, true);
				CTaskSynchronizedScene* pSyncSceneTask = rage_new CTaskSynchronizedScene(syncedScene, m_ClipId, clipDictName, fBlendInDelta, fBlendOutDelta, flags);
				SetNewTask(pSyncSceneTask);

				m_IdleSyncedSceneId = syncedScene;
			}

			return FSM_Continue;
		}
	}

	StartClipBySetAndClip( pPed, m_ClipSetId, m_ClipId, fBlendInDelta, CClipHelper::TerminationType_OnFinish );

	m_VfxTagsExistIdle = false;
	m_PropTagsExistIdle = false;
	CClipHelper* pHelper = GetClipHelper();
	if( taskVerifyf(pHelper, "Failed to start idle clip!" ) )
	{
		// This flag normally gets set when you use TerminationType_OnFinish on the clip helper,
		// but then when it reaches the end, we won't be able to control the blend-out rate,
		// so we will take the responsibility for calling StopClip() ourselves instead.
		pHelper->UnsetFlag(APF_ISFINISHAUTOREMOVE);

		const crClip* pClip = pHelper->GetClip();
		if (taskVerifyf(pClip, "Expected a valid clip once helper started."))
		{
			m_VfxTagsExistIdle = CheckClipHasVFXTags(*pClip);
			m_PropTagsExistIdle = CheckClipHasPropTags(*pClip);
			if( m_VfxTagsExistIdle || m_PropTagsExistIdle )
			{
				// We just set m_VfxTagsExistIdle or m_PropTagsExistIdle to true, make sure we get calls to
				// ProcessMoveSignals() so the effects get updated properly.
				RequestProcessMoveSignalCalls();
			}
			ParseBlendOutTags(*pClip);
		}

#if __BANK
		//Clip finished so exit... 
		if (m_Flags.IsFlagSet(Flag_UseDebugAnimData))
		{
			aiAssert(m_DebugInfo.m_AnimType == CConditionalAnims::AT_VARIATION || m_DebugInfo.m_AnimType == CConditionalAnims::AT_REACTION);
			m_Flags.SetFlag(Flag_WantNextDebugAnimData);
			StopBaseClip();//stop the base clip running in the background.
		}
#endif
		//Clear the flags that can be used to override idle anim data.
		if (m_Flags.IsFlagSet(Flag_IdleClipIdsOverridden))
		{
			m_Flags.ClearFlag(Flag_IdleClipIdsOverridden);
			m_Flags.SetFlag(Flag_PlayingIdleOverride);
		}
		if (m_Flags.IsFlagSet(Flag_ForcePickNewIdleAnim))
		{
			m_Flags.ClearFlag(Flag_ForcePickNewIdleAnim);
		}

		// m_iInternalLoopRepeats = m_AmbientClipRequestHelper.GetNumInternalLoops();

		// Inform audio that a new ambient clip has been kicked off
		pPed->GetPedAudioEntity()->StartAmbientAnim(GetClipHelper()->GetClip());

		// B*1434308 - Disable rate randomization on Player idles [6/3/2013 mdawe]
		if (!pPed->IsPlayer())
		{
			float idleClipRate = pPed->GetRandomNumberInRangeFromSeed(0.9f, 1.1f);

			// Randomise the playback rate
			GetClipHelper()->SetRate(idleClipRate);
		}

		// Create a prop clip hash
		char buff[64];
		s32 iPath = 0;
		s32 iExtension = 0;
		for(s32 i = istrlen(GetClipHelper()->GetClipName())-1; i >= 0; i--)
		{
			if(GetClipHelper()->GetClipName()[i] == '/' || GetClipHelper()->GetClipName()[i] == '\\')
			{
				iPath = i+1;
				break;
			}
		}
		for(s32 i = istrlen(GetClipHelper()->GetClipName())-1; i >= 0; i--)
		{
			if(GetClipHelper()->GetClipName()[i] == '.')
			{
				iExtension = i;
				break;
			}
		}

		strcpy(buff, "prop_");
		strncpy(&buff[5], &GetClipHelper()->GetClipName()[iPath], iExtension-iPath);
		buff[iExtension-iPath+5] = '\0';
		m_PropClipId = fwMvClipId(atStringHash(buff));

		if(!fwAnimManager::GetClipIfExistsBySetId(m_ClipSetId, m_PropClipId))
		{
			// Clip doesn't exist
			m_PropClipId = CLIP_ID_INVALID;
		}

		// Attempt to play a prop clip
		CObject* pProp = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponObject() : NULL;
		if( pProp && pProp->GetWeapon() )
		{
			pProp->GetWeapon()->StartAnim( m_ClipSetId, m_PropClipId, NORMAL_BLEND_DURATION, GetClipHelper()->GetRate(), GetClipHelper()->GetPhase() );
		}
	}

	// if available, attempt to play any audio lines
	const CConditionalClipSet* pVariationAnim = FindConditionalAnim(CConditionalAnims::AT_VARIATION, GetClipHelper()->GetClipSet().GetHash());
	if(pVariationAnim && pVariationAnim->GetAssociatedSpeech().GetHash() > 0)
	{
		pPed->NewSay(pVariationAnim->GetAssociatedSpeech().GetHash());
	}

	return FSM_Continue;
}

CTaskAmbientClips::FSM_Return CTaskAmbientClips::PlayingIdleAnimation_OnUpdate()
{
	bool attemptToLeaveState = false;

	// cache our ped
	CPed* pPed = GetPed();
	Assert(pPed);

	CTask* pParentTask = GetParent();

	// It is possible for the task to request termination below because of a NULL clip helper.
	// CTaskUseScenario handles this flag getting set, but many other tasks can run CTaskAmbientClips (like CTaskWander).
	// So, the best move is to quit out so the ped doesn't stay stuck forever in this state.
	if (GetIsFlagSet(aiTaskFlags::TerminationRequested))
	{
		if (!pParentTask || pParentTask->GetTaskType() != CTaskTypes::TASK_USE_SCENARIO)
		{
			SetState(State_Exit);
			return FSM_Continue;
		}
	}

	// Don't leave this state if we are quitting after the idle until the idle is actually done.
	// Otherwise we might not last long enough in this state to trip the noclip termination logic above 
	// and actually kill this task.
	if (m_bForceReevaluateChosenConditionalAnim && !m_Flags.IsFlagSet(Flag_QuitAfterIdle))
	{
		SetState(State_NoClipPlaying);
		return FSM_Continue;
	}

	// Check if we are playing a synchronized idle animation.
	if(m_IdleSyncedSceneId != INVALID_SYNCED_SCENE_ID)
	{
		// Check if our CTaskSynchronizedScenario task has finished.
		if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
		{
			attemptToLeaveState = true;
		}
	}
	else
	{
		// If we're playing a synchronized scene for the base clip, we need to make sure the
		// CTaskSynchronizedScene subtask gets created when the idle animation is about to
		// end, otherwise it seems like we get a glitch because there is no base animation
		// to blend with on the frame the idle animation ends.
		if(m_BaseSyncedSceneId != INVALID_SYNCED_SCENE_ID)
		{
			CClipHelper* pHelper = GetClipHelper();
			
			// Are we about to end, and have base animations streamed in?
			bool bIdleClipAtEnd = pHelper == NULL || ((pHelper->GetPhase() + (pHelper->GetPhaseUpdateAmount()*2.0f) > 1.0f));
			if(bIdleClipAtEnd && m_BaseClipRequestHelper.GetState() == CAmbientClipRequestHelper::AS_groupStreamed)
			{
				attemptToLeaveState = true;
			}
		}
	}
	if(attemptToLeaveState)
	{
		// Try to start the animation.
		fwMvClipId clipId(CLIP_ID_INVALID);
		fwMvClipSetId clipSetId = m_BaseClipRequestHelper.GetClipSet();
		AssertVerify(CAmbientClipRequestHelper::PickClipByGroup_SimpleConditions(GetPed(), clipSetId, clipId));
		if(StartSynchronizedBaseClip(clipSetId, clipId, INSTANT_BLEND_IN_DELTA))
		{
			SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
			SetState(State_NoClipPlaying);
			return FSM_Continue;
		}
	}

	// If we're playing a synchronized idle, we probably shouldn't do the stuff further down.
	if(m_IdleSyncedSceneId != INVALID_SYNCED_SCENE_ID)
	{
		return FSM_Continue;
	}

	CClipHelper* pHelper = GetClipHelper();
	if( pHelper == NULL )
	{
		//discard the event once the reaction animation is finished
		if (m_Event.IsReacting())
		{
			m_Event.Clear();
		}

		if (m_Flags.IsFlagSet(Flag_QuitAfterIdle))
		{
			// disable timeslicing until we terminate to allow for the Transition Animation System to sync properly
			pPed->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate | CPedAILod::AL_LodTimesliceAnimUpdate); 
			// if our Ped is not playing a gesture animation
			if (!pPed->IsPlayingAGestureAnim())
			{
				RequestTermination();
			}
			return FSM_Continue;
		}

#if SCENARIO_DEBUG
		//No next clipset info to use next so finished lets exit... 
		if (m_Flags.IsFlagSet(Flag_UseDebugAnimData) && !m_DebugInfo.m_ClipSetId.GetHash())
		{
			SetState(State_Exit);
		}
		else if (m_Flags.IsFlagSet(Flag_UseDebugAnimData) && m_DebugInfo.m_AnimType == CConditionalAnims::AT_BASE)
		{
			SetState(State_NoClipPlaying);
		}
		else
#endif // SCENARIO_DEBUG
		{
			//if we were playing an idle override then we are done with it and need to let normal behavior resume.
			if (m_Flags.IsFlagSet(Flag_PlayingIdleOverride))
			{
				m_Flags.ClearFlag(Flag_PlayingIdleOverride);
				m_fTimeLastSearchedForBaseClip = -1.0f;//This will trigger a base clip reevaluation so we can play the base anim again.
			}
			SetState(State_NoClipPlaying);
		}
		return FSM_Continue;
	}
	else
	{
		//Check if we should end the animation.
		bool bEndAnimation = GetIsFlagSet(aiTaskFlags::TerminationRequested);
		bool bEndingAnimationDueToPhase = false;

		// If script has requested an idle clip override to be played, stop playing this idle so the new one can be picked up.
		if (m_Flags.IsFlagSet(Flag_IdleClipIdsOverridden))
		{
			bEndAnimation = true;
		}
		
		//Check if we are reacting to an event.
		if(m_Event.IsReacting())
		{
			//Tick the event.
			if(m_Event.TickAnimation(GetTimeStep()))
			{
				//The event has expired.
				bEndAnimation = true;
			}
		}

		//B*1767232: End the animation if we equip a new weapon
		// Only for player peds, as there are cliptags on scenario/wander animations that cause weapons to be equipped and
		// removed, and we don't want to cut those animations short.
		if (pPed->IsAPlayerPed() && pPed->GetPedResetFlag(CPED_RESET_FLAG_EquippedWeaponChanged))
		{
			bEndAnimation = true;
		}

		float fBlendOutRateFromTags = 0.0f;
		bool gotRateFromTags = false;
		if(!bEndAnimation)
		{
			// Check if we have passed the phase at which we should blend out. Note that
			// the clip helper is not configured to blend out automatically at the end of the
			// animation, so we want to do this even if we don't have any tags to blend out early.
			const float currentPhase = pHelper->GetPhase();
			if(currentPhase >= m_fBlendOutPhase)
			{
				bEndAnimation = true;
				bEndingAnimationDueToPhase = true;

				// If we found BLEND_OUT_RANGE tags on the clip earlier, we would have stored
				// off the start phase for the blend-out, now we need to take a look at the end phase
				// so we can compute the blend-out rate.
				if(m_BlendOutRangeTagsFound)
				{
					const crClip* pClip = pHelper->GetClip();
					if(pClip)
					{
						float startPhase, endPhase;
						if(ParseBlendOutTagsHelper(*pClip, startPhase, endPhase))
						{
							// Compute the remaining amount of phase until we reach the end. Note
							// that we use the current phase here, so potentially (if the update time step
							// was large enough) we could already have passed the end, in which case this
							// would be a negative value.
							const float remainingPhaseToBlendOut = endPhase - currentPhase;

							// Multiply by the clip duration to get the amount of time the blend-out should take.
							const float remainingUnscaledTimeToBlendOut = remainingPhaseToBlendOut*pClip->GetDuration();

							// Compute the rate to use, as the negative inverse of the time. We also multiply by
							// the playback rate here, in case we have scaled that. The Max() here is done to protect
							// against division by a non-positive value, if so, it should produce an instantaneous blend-out.
							fBlendOutRateFromTags = -pHelper->GetRate()/Max(remainingUnscaledTimeToBlendOut, SMALL_FLOAT);
							gotRateFromTags = true;
						}
					}
				}
			}
		}
		
		// If the clip is being blocked - blend out the clip
		IdleBlockingState iIdleBlockingState = GetIdleBlockingState(GetTimeStep());
		if(bEndAnimation || (iIdleBlockingState == IBS_BlockAndRemoveIdles))
		{
			// Determine the blend-out rate, we get this either from what we computed from the BLEND_OUT_RANGE tags,
			// or from the conditional animation data, if possible.
			float fBlendOutDelta;

			if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsDuckingInVehicle))
			{
				fBlendOutDelta = NORMAL_BLEND_OUT_DELTA;
			}
			else if(pPed->IsLocalPlayer() && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UsingScenario) && pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_CAMERA_VIEW_MODE_SWITCHED_TO_FIRST_PERSON))
			{
				fBlendOutDelta = INSTANT_BLEND_OUT_DELTA;
			}
			else if(gotRateFromTags)
			{
				fBlendOutDelta = fBlendOutRateFromTags;
			}
			else
			{
				fBlendOutDelta = NORMAL_BLEND_OUT_DELTA;
				if(m_pConditionalAnimsGroup && m_ConditionalAnimChosen!=-1)
				{
					const CConditionalAnims* pAnims = m_pConditionalAnimsGroup->GetAnims(m_ConditionalAnimChosen);
					if(pAnims)
					{
						fBlendOutDelta = pAnims->GetBlendOutDelta();
					}
				}
			}

			if (m_Event.IsValid())
			{
				//Also reset the idle timer
				//set the last time we searched for the base clip to 0,
				//because it has to be restreamed after interrupting the animation
				m_fTimeLastSearchedForBaseClip = 0.0f;
				m_fNextIdleTimer = -0.1f;
				// Clear the event if blending out while playing an animation.
				// Otherwise, we received the event while playing a normal idle and need to go back to base to pick it up.
				if (m_Event.IsReacting())
				{
					m_Event.Clear();
				}
				else
				{
					//Transition back to base, do a slow blend out here.
					fBlendOutDelta = REALLY_SLOW_BLEND_OUT_DELTA;
				}
			}

			// force the current MotionState to Idle during blendout.
			const CConditionalClipSet* pVariationAnim = FindConditionalAnim(CConditionalAnims::AT_VARIATION, GetClipHelper()->GetClipSet().GetHash());
			if( bEndAnimation && pVariationAnim && pVariationAnim->GetForceIdleThroughBlendOut() )
			{
				pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Idle, true);
			}
			
			// We need to start Phasing out, so... 
			// Without the check to see if the base clip helper exists for the base clip, the player will crash.
			// We also need the Sync with motion task flag check for guys walking around in the world (their bases should always play in sync with the idles)
			if (m_BaseClip.GetClipHelper() && !m_BaseClip.IsFlagSet(APF_TAG_SYNC_WITH_MOTION_TASK))
			{
				m_BaseClip.SetPhase(0.0f);
			}

			// Clear the current clip id.
			m_ClipId = CLIP_ID_INVALID;

			u32 uFlags = CMovePed::Task_IgnoreMoverBlendRotation;
			GetClipHelper()->StopClip(fBlendOutDelta, uFlags);

			// It's possible that the animation quit early (due to an idle blocking state, perhaps).
			// In this case, we may not have hit the destroy prop tags in the animation, meaning
			// that the prop is now potentially on the ped permanently.
			// The ref counting solution in Cleanup won't work either, because CTaskWander could still be alive and well.
			bool bNotUsingScenario = !pParentTask || pParentTask->GetTaskType() != CTaskTypes::TASK_USE_SCENARIO;
			if (bNotUsingScenario && !bEndingAnimationDueToPhase && m_Flags.IsFlagSet(Flag_QuitAfterIdle) && 
				m_pPropHelper && m_pPropHelper->IsHoldingProp())
			{
				m_pPropHelper->DisposeOfProp(*pPed);
			}

			// ScanClipForPropRequests() below assumes that GetClipHelper() returns non-NULL,
			// or they will assert and perhaps crash. Even if we checked for NULL earlier in this
			// function, the call to StopClip() might have caused it to go NULL. If that happens,
			// we skip the rest of the update to avoid the problems. UpdateLoopingFlag() is completely
			// commented out right now, so it won't mind not getting the update.
			if(!GetClipHelper())
			{
				return FSM_Continue;
			}
		}

		// Update loop flag
		UpdateLoopingFlag();
	}

	return FSM_Continue;
}

CTaskAmbientClips::FSM_Return CTaskAmbientClips::PlayingIdleAnimation_OnExit()
{
	// If we were playing a synchronized idle animation (either as master or slave),
	// we need to decrease the reference count, and clear out the ID.
	if(m_IdleSyncedSceneId != INVALID_SYNCED_SCENE_ID)
	{
		if(fwAnimDirectorComponentSyncedScene::IsValidSceneId(m_IdleSyncedSceneId))
		{
			fwAnimDirectorComponentSyncedScene::DecSyncedSceneRef(m_IdleSyncedSceneId);
		}
		m_IdleSyncedSceneId = INVALID_SYNCED_SCENE_ID;
	}

	// Cache the ped
	CPed* pPed = GetPed();

	// If this clip has started a cyclical audio sound, inform the audio system when it terminates.
	pPed->GetPedAudioEntity()->StopAmbientAnim();

	// Remove any props
	if( m_pPropHelper && m_pPropHelper->IsPropLoaded() && !m_pPropHelper->IsPropPersistent() )
	{
		if ( pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponObject() )
		{
			u32 uObjectHash = pPed->GetWeaponManager()->GetPedEquippedWeapon()->GetObjectHash();
			pPed->GetWeaponManager()->DropWeapon( uObjectHash, true );
		}
	}

	// Release any clips
	m_AmbientClipRequestHelper.ReleaseClips();

	// Release any props
	if (m_pPropHelper && !m_pPropHelper->IsPropPersistent())
	{
		m_pPropHelper->ReleasePropRefIfUnshared(pPed);
	}

	// Clear the current clip id.
	m_ClipId = CLIP_ID_INVALID;

	u32 uFlags = CMovePed::Task_IgnoreMoverBlendRotation;
	StopClip(m_fCleanupBlendOutDelta, uFlags);

	// As we are no longer playing the idle animation, clear the associated condition.
	m_IdleAnimImmediateCondition = NULL;

	// Clear this flag when we are no longer in this state, may help to reduce
	// the number of ProcessMoveSignals() calls.
	m_VfxTagsExistIdle = false;
	m_PropTagsExistIdle = false;

	return FSM_Continue;
}

const CConditionalClipSet* CTaskAmbientClips::FindConditionalAnim(CConditionalAnims::eAnimType animType, const u32 clipSetId)
{
	if(m_pConditionalAnimsGroup && m_ConditionalAnimChosen != -1)
	{
		const CConditionalAnims* pAnims = m_pConditionalAnimsGroup->GetAnims(m_ConditionalAnimChosen);
		if(pAnims)
		{
			const CConditionalAnims::ConditionalClipSetArray* pConditionalAnims = pAnims->GetClipSetArray( animType );
			if( pConditionalAnims )
			{
				for( s32 i = 0; i < pConditionalAnims->GetCount(); i++)
				{
					const CConditionalClipSet* pVariationAnim = (*pConditionalAnims)[i];
					if( pVariationAnim->GetClipSetHash() == clipSetId )
					{
						return pVariationAnim;
					}
				}
			}
		}
	}
	return NULL;
}

float CTaskAmbientClips::GetNextIdleTime() const
{
	// This will be -1 if there has been no clip set chosen (list is empty within the conditional clip group)
	if (m_pConditionalAnimsGroup && m_ConditionalAnimChosen!=-1)
	{
		const CConditionalAnims * pAnims = m_pConditionalAnimsGroup->GetAnims( m_ConditionalAnimChosen );

		if ( pAnims )
		{
			return pAnims->GetNextIdleTime();
		}
	}

	// Defaults 
	return sm_Tunables.m_DefaultTimeBetweenIdles;
}


void CTaskAmbientClips::SetConditionalAnimChosen(int anim)
{
	// Only do something if we get a new value.
	if(m_ConditionalAnimChosen != anim)
	{
#if __ASSERT
		//B*1946851 to help track down MP base clip vfx assert
		if(NetworkInterface::IsGameInProgress() && GetEntity() && !GetPed()->IsNetworkClone())
		{
			fwMvClipSetId		baseClipSet;
			float				fBasePhase =-1.0f;
			CClipHelper* pHelper = GetBaseClip();
			if (pHelper)
			{
				baseClipSet = pHelper->GetClipSet();
				fBasePhase = pHelper->GetPhase();
			}
			gnetDebug3("%s %s CTaskAmbientClips::SetConditionalAnimChosen: State %s. m_ConditionalAnimChosen %d, anim %d, Prop helper %s, Group %s,  m_Flags: 0x%x, Base clip %s phase %.2f",
				GetPed()->GetDebugName(),
				GetPed()->IsNetworkClone()?"CLONE":"LOCAL",
				GetStateName(GetState()),
				m_ConditionalAnimChosen, 
				anim,
				m_pPropHelper?m_pPropHelper->GetPropNameHash().GetCStr():"Null prop helper",
				GetConditionalAnimsGroup()?GetConditionalAnimsGroup()->GetName():"Null group",
				m_Flags.GetAllFlags(),
				baseClipSet.GetCStr(),
				fBasePhase) ;
		}
#endif

		// We'll get these from the CConditionalAnims.
		bool ignoreLowPri = false;

		// Check if we have valid animations selected, and if so, check what behavior
		// they want regarding ignoring low priority shocking events, and destroying vs.
		// dropping the prop.
		if(m_pConditionalAnimsGroup && anim >= 0)
		{
			if(NetworkInterface::IsGameInProgress())
			{
				if( anim>=m_pConditionalAnimsGroup->GetNumAnims() )
				{
					Assertf(0,"%s Network: Anim group %s. anim selected %d. Max expected anim num %d. m_ConditionalAnimChosen %d.",
						CTaskAmbientMigrateHelper::GetAssertNetObjPedName(this),
						m_pConditionalAnimsGroup->GetName(),
						anim, 
						m_pConditionalAnimsGroup->GetNumAnims(),
						m_ConditionalAnimChosen);

					NOTFINAL_ONLY(gnetWarning("%s Network: Anim group %s. anim selected %d. Max expected anim num %d. m_ConditionalAnimChosen %d.",
						GetEntity()?(GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : GetPed()->GetModelName()):"Null entity",
						m_pConditionalAnimsGroup->GetName(),
						anim, 
						m_pConditionalAnimsGroup->GetNumAnims(),
						m_ConditionalAnimChosen);)

					return;
				}
			}
			const CConditionalAnims* pAnims = m_pConditionalAnimsGroup->GetAnims(anim);
			if(pAnims)
			{
				ignoreLowPri = pAnims->ShouldIgnoreLowPriShockingEvents();
				
				if (m_pPropHelper)
				{
					m_pPropHelper->SetCreatePropInLeftHand( pAnims->GetCreatePropInLeftHand() );
					m_pPropHelper->SetDestroyProp( pAnims->ShouldDestroyPropInsteadOfDropping() );
				}

				// If we're changing conditional anims, get rid of the conversation we're in [7/30/2012 mdawe]
				if (!pAnims->IsMobilePhoneConversation())
				{
					m_Flags.ClearFlag(Flag_PhoneConversationShouldPlayEnterAnim);
					delete m_pConversationHelper;
					m_pConversationHelper = NULL;
				}
			}
		}

		// Update the variables.
		SetIgnoreLowPriShockingEvents(ignoreLowPri);

		taskAssert(anim >= -0x8000 && anim <= 0x7fff);
		m_ConditionalAnimChosen = (s16)anim;

		if (CreatePhoneConversationHelper())
		{
			m_Flags.SetFlag(Flag_PhoneConversationShouldPlayEnterAnim);
		}
	}
}


void CTaskAmbientClips::SetIgnoreLowPriShockingEvents(bool b)
{
	// Only do something if we get a new value.
	if(m_Flags.IsFlagSet(Flag_IgnoreLowPriShockingEvents) != b)
	{
		CPed* pPed = GetPed();
		if(pPed)
		{
			// Call StartIgnoreLowPriShockingEvents() or EndIgnoreLowPriShockingEvents()
			// to increase or decrease the low priority shocking ignore count. By going
			// through SetIgnoreLowPriShockingEvents(), this task can only ever hold one
			// count, and can't leak it (assuming we go through the regular cleanup and
			// don't bypass this function).
			CPedIntelligence& intel = *pPed->GetPedIntelligence();
			if(b)
			{
				intel.StartIgnoreLowPriShockingEvents();
			}
			else
			{
				intel.EndIgnoreLowPriShockingEvents();
			}
		}

		// Set the new value.
		if(b)
		{
			m_Flags.SetFlag(Flag_IgnoreLowPriShockingEvents);
		}
		else
		{
			m_Flags.ClearFlag(Flag_IgnoreLowPriShockingEvents);
		}
	}
}


CTaskAmbientClips* CTaskAmbientClips::FindSynchronizedMasterTask() const
{
	const CPed* pPartner = m_SynchronizedPartner;
	if(pPartner)
	{
		CTaskAmbientClips* pPartnerTask = static_cast<CTaskAmbientClips*>(pPartner->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AMBIENT_CLIPS));
		return pPartnerTask;
	}
	return NULL;
}


CTaskAmbientClips::IdleBlockingState CTaskAmbientClips::GetIdleBlockingState(float fTimeStep)
{
	// Cache the ped
	const CPed* pPed = GetPed();

	// Moved up here as sometimes we were blocking new idles, but continuing to play are old one even if these conditions failed
	// if the ped is in a vehicle
	if (pPed->GetIsInVehicle())
	{
		const CVehicle& rVeh = *pPed->GetMyVehicle();
		const bool bIsBikeOrQuad = rVeh.IsBikeOrQuad();
		if (bIsBikeOrQuad)
		{
			if (CTaskMotionInAutomobile::CheckIfVehicleIsDoingBurnout(rVeh, *pPed))
			{
				return IBS_BlockAndRemoveIdles;
			}
		}

		float fVehSpeedMag2 = rVeh.GetVelocity().Mag2();
		TUNE_GROUP_FLOAT(IN_VEHICLE_TUNE, fVehSpeedMovingSqr, 1.0f, 0.0f, 10.0f, 1.0f);
		// if the vehicle is going to fast
		const float fMaxVelocity = bIsBikeOrQuad ? sm_Tunables.m_MaxBikeVelocityForAmbientIdles : sm_Tunables.m_MaxVehicleVelocityForAmbientIdles;
		if (fVehSpeedMag2 > rage::square(fMaxVelocity))
		{
			m_iVehVelocityTimerStart = -1;
			return IBS_BlockAndRemoveIdles;
		}
		else if (fVehSpeedMag2 > fVehSpeedMovingSqr)
		{
			TUNE_GROUP_INT(IN_VEHICLE_TUNE, iTimeDrivingToBlockIdles, 3000, 0, 10000, 1);
			if (m_iVehVelocityTimerStart == -1)
			{
				m_iVehVelocityTimerStart = fwTimer::GetTimeInMilliseconds();
			}
			else if (m_iVehVelocityTimerStart + iTimeDrivingToBlockIdles < fwTimer::GetTimeInMilliseconds())
			{
				m_iVehVelocityTimerStart = -1;
				return IBS_BlockAndRemoveIdles;
			}
		}
		else 
		{
			m_iVehVelocityTimerStart = -1;
		}

		if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HeadbobToRadioEnabled))
		{
			// Only allow idles if we are in a rockout section and are playing the base anim
			if (!(m_bAtRockOutMarker && m_bPlayBaseHeadbob))
			{
				return IBS_BlockAndRemoveIdles;
			}

			// Block new headbob idles if we've only got a small amount of time left in the rockout period.
			TUNE_GROUP_INT(HEAD_BOB_TUNE, iTimeRemainingToStartNewHeadbob, 2000, 0, 10000, 1);
			bool bNotEnoughTimeToStartNewHeadbobIdle = m_iTimeLeftToRockout < iTimeRemainingToStartNewHeadbob;
			if (bNotEnoughTimeToStartNewHeadbobIdle)
			{
				return IBS_BlockNewIdles;
			}
		}

		// Don't play ambient idles when controlling suspension in a lowrider car.
		if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UsingLowriderLeans) && pPed->GetVehiclePedInside() && pPed->GetVehiclePedInside()->m_nVehicleFlags.bPlayerModifiedHydraulics)
		{
			return IBS_BlockAndRemoveIdles;
		}

		// Block/remove idles for a short amount of time after we last modified the suspension.
		if (pPed->GetVehiclePedInside() && pPed->GetVehiclePedInside()->InheritsFromAutomobile())
		{
			CAutomobile *pAutomobile = static_cast<CAutomobile*>(pPed->GetVehiclePedInside());
			u32 uTimeHydraulicsLastModified = pAutomobile ? pAutomobile->GetTimeHydraulicsModified() : 0;
			TUNE_GROUP_INT(LOWRIDER_TUNE, iTimeToBlockIdlesAfterHydraulicsModified, 1500, 0, 10000, 1);
			if (uTimeHydraulicsLastModified + iTimeToBlockIdlesAfterHydraulicsModified > fwTimer::GetTimeInMilliseconds() )
			{
				return IBS_BlockAndRemoveIdles;
			}
		}

		// Don't start a new ambient idle if we're in a lowrider and not using the alt clipset (ie in a convertible and waiting to transition to the arm on window anims), or we're doing an arm transition anim.
		// Clones: just check the task instead of the flags; flags are synced over the network so might not reflect the clone peds actual state.
		if ((pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UsingLowriderLeans) && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UsingAlternateLowriderLeans)) || pPed->IsNetworkClone())
		{
			const CTaskMotionInAutomobile* pAutoMobileTask = static_cast<const CTaskMotionInAutomobile*>(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_AUTOMOBILE));
			if (pAutoMobileTask && pAutoMobileTask->ShouldUseLowriderLeanAltClipset() && (!pAutoMobileTask->IsUsingAlternateLowriderClipset() || pAutoMobileTask->IsDoingLowriderArmTransition()))
			{
				return IBS_BlockAndRemoveIdles;
			}
		}

#if __BANK
		TUNE_GROUP_BOOL(LOWRIDER_TUNE, bBlockAmbientIdles, false);
		if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UsingLowriderLeans) && bBlockAmbientIdles)
		{
			return IBS_BlockAndRemoveIdles;
		}
#endif	//__BANK

		if (rVeh.GetDriver() == pPed)
		{
			// if the ped is driving the vehicle and turning the wheel
			if (Abs(rVeh.GetSteerAngle()) > sm_Tunables.m_MaxSteeringAngleForAmbientIdles)
			{
				return IBS_BlockAndRemoveIdles;
			}
			// if the ped is the local player and has a wanted level
			if (pPed->IsLocalPlayer() && pPed->GetPlayerWanted() && pPed->GetPlayerWanted()->GetWantedLevel() > WANTED_CLEAN)
			{
				// B*2015253: If ped is headbobbing and has been blocked, don't headbob again until the next rockout section
				if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HeadbobToRadioEnabled))
				{
					m_sBlockedRockoutSection = m_sRockoutStart - (s32)m_fTimeToPlayBaseAnimBeforeAfterRockout;
				}

				return IBS_BlockAndRemoveIdles;
			}
		}

		if(pPed->IsInFirstPersonVehicleCamera())
		{
			return IBS_BlockAndRemoveIdles;
		}

#if FPS_MODE_SUPPORTED
		// B*2172500: In-Vehicle FPS: Block/remove all idles if clone ped isn't looking within a narrow cone in front of it (as it is doing a Look At)
		// If ped is looking within this cone, block/remove idles if clone ped moves their camera significantly. Re-allow idles if ped hasn't changed their look at position in over 5 seconds.
		if(pPed->IsNetworkClone() && pPed->IsPlayer() && pPed->IsInFirstPersonVehicleCamera(false, true))
		{
			CNetObjPlayer *netObjPlayer = SafeCast(CNetObjPlayer, pPed->GetNetworkObject());
			grcViewport *pViewport = netObjPlayer ? netObjPlayer->GetViewport() : NULL;
			if (pViewport)
			{
				// Get the look at position.
				Matrix34 camMatrix = RCC_MATRIX34(pViewport->GetCameraMtx());
				Vector3 vCamFace = -camMatrix.c; 
				Vector3 vLookAtPos = camMatrix.d + vCamFace * 2.0f;

				Vector3 vPedForward = VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward());
				static dev_float fLookThreshold = 0.25f;
				Vector3 vCamDirNormalized = vCamFace;
				vCamDirNormalized.Normalize();
				float fAngle = vCamDirNormalized.FastAngle(vPedForward);

				// Block all idles if ped is looking outside of the threshold.
				if (fAngle > fLookThreshold)
				{
					m_fCloneLookAtTimer = 0.0f;
					m_vClonePreviousLookAtPosInVehicle = vLookAtPos;
					return IBS_BlockAndRemoveIdles;
				}
				// If looking relatively straight ahead and the camera moves a significant amount, block and remove the idles. 
				else
				{
					// Check if we should block/remove idles.
					float fPositionDifference = (vLookAtPos - m_vClonePreviousLookAtPosInVehicle).Mag();
					if (m_vClonePreviousLookAtPosInVehicle.IsNonZero())
					{
						static dev_float fMovementThreshold = 0.1f;
						static dev_float fLookAtTimer = 5.0f;

						// Significant movement in LookAt position, reset timer and block idles.
						if (fPositionDifference > fMovementThreshold)
						{
							m_fCloneLookAtTimer = 0.0f;
							m_vClonePreviousLookAtPosInVehicle = vLookAtPos;
							return IBS_BlockAndRemoveIdles;
						}
						else if (m_fCloneLookAtTimer < fLookAtTimer)
						{
							m_fCloneLookAtTimer += fwTimer::GetTimeStep();
							m_vClonePreviousLookAtPosInVehicle = vLookAtPos;
							return IBS_BlockAndRemoveIdles;
						}
					}

					m_vClonePreviousLookAtPosInVehicle = vLookAtPos;
				}
			}
		}
#endif	// FPS_MODE_SUPPORTED

		// if the ped is being shunted or we need to play heavy break
		if (pPed->GetCurrentMotionTask()->GetTaskType() == CTaskTypes::TASK_MOTION_IN_AUTOMOBILE)
		{
			const s32 iCurrentState = pPed->GetCurrentMotionTask()->GetState();
			if (iCurrentState == CTaskMotionInAutomobile::State_Shunt ||
				iCurrentState == CTaskMotionInAutomobile::State_Horn ||
				iCurrentState == CTaskMotionInAutomobile::State_Reverse ||
				iCurrentState == CTaskMotionInAutomobile::State_HeavyBrake ||
				iCurrentState == CTaskMotionInAutomobile::State_CloseDoor || 
				iCurrentState == CTaskMotionInAutomobile::State_FirstPersonReverse ||
				iCurrentState == CTaskMotionInAutomobile::State_PutOnHelmet)
			{
				// B*2015253: If ped is headbobbing and has been blocked, don't headbob again until the next rockout section
				if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HeadbobToRadioEnabled))
				{
					m_sBlockedRockoutSection = m_sRockoutStart - (s32)m_fTimeToPlayBaseAnimBeforeAfterRockout;
				}

				return IBS_BlockAndRemoveIdles;
			}

			const CTaskMotionInAutomobile* pAutoMobileTask = static_cast<const CTaskMotionInAutomobile*>(pPed->GetCurrentMotionTask());
			if(pAutoMobileTask->IsUsingFirstPersonSteeringAnims() || pAutoMobileTask->IsDucking())
			{
				// B*2015253: If ped is headbobbing and has been blocked, don't headbob again until the next rockout section
				if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HeadbobToRadioEnabled))
				{
					m_sBlockedRockoutSection = m_sRockoutStart - (s32)m_fTimeToPlayBaseAnimBeforeAfterRockout;
				}

				return IBS_BlockAndRemoveIdles;
			}

			if (GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_CLOSE_VEHICLE_DOOR_FROM_INSIDE)
			{
				return IBS_BlockAndRemoveIdles;
			}
		}

		if ( m_pLookAt && m_pLookAt->IsLookingAtRoofOpenClose())
		{
			return IBS_BlockAndRemoveIdles;
		}
	}

	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingHelmetVisor))
	{
		return IBS_BlockAndRemoveIdles;
	}

	// If we're playing an idle animation that has some condition that needs to be
	// reevaluated frequently, do so now.
	// Note: GetIdleBlockingState() may be called when no idle animation is playing
	// (from NoClipPlaying_OnUpdate()), but if so, m_IdleAnimImmediateCondition should
	// be NULL.
	if(m_IdleAnimImmediateCondition)
	{
		CScenarioCondition::sScenarioConditionData conditionData;
		conditionData.pPed = pPed;
		if(!m_IdleAnimImmediateCondition->Check(conditionData))
		{
			// Stop the idle animation. The case where this currently happens is if
			// we're playing a walk idle and then stop walking, or a stationary idle
			// and then start walking.
			return IBS_BlockAndRemoveIdles;
		}
	}
	
	//Check if the ped is agitated and fleeing.
	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsAgitated) &&
		pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_SMART_FLEE, true))
	{
		// B*2015253: If ped is headbobbing and has been blocked, don't headbob again until the next rockout section
		if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HeadbobToRadioEnabled))
		{
			m_sBlockedRockoutSection = m_sRockoutStart - (s32)m_fTimeToPlayBaseAnimBeforeAfterRockout;
		}

		return IBS_BlockAndRemoveIdles;
	}

	// B*2203677: Script control to block/remove idle clips.
	if (m_bShouldBlockIdles && !m_bShouldRemoveIdles)
	{
		return IBS_BlockNewIdles;
	}

	if (m_bShouldBlockIdles && m_bShouldRemoveIdles)
	{
		return IBS_BlockAndRemoveIdles;
	}

	//Check to see if we're playing a gesture animation
	if (pPed->IsPlayingAGestureAnim())
	{
		return IBS_BlockAndRemoveIdles;
	}

	if( BLOCK_ALL_AMBIENT_IDLES )
	{
		return IBS_BlockNewIdles;
	}

	// This Ped isn't visible, so it probably shouldn't try to play any new idle animations.
	if( !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen) )
	{
		return IBS_BlockNewIdles;
	}
	
	// Using the low LOD motion task and Ped is not close to the player prevents new idles.
	if( pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodMotionTask) )
	{
		// Determine if we should use the camera's position or the player's for streaming idles.
		TUNE_GROUP_BOOL(LOD_DISTANCE, bUsePlayerCameraPositionInstead, false);
		Vector3 vecPlayerPos; 
		if( !bUsePlayerCameraPositionInstead )
		{
			vecPlayerPos = CGameWorld::FindLocalPlayerCoors();
		}
		else
		{
			vecPlayerPos = camInterface::GetPos();
			vecPlayerPos.SetZ(CGameWorld::FindLocalPlayerCoors().GetZ() - 0.5f);
		}
		const Vector3 vecPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		const Vector3 vecPedDistanceToPlayerPos = vecPedPos - vecPlayerPos;

		// Show how far out we can stream idles anims regardless of LOD.
		TUNE_GROUP_BOOL(LOD_DISTANCE, bShowStreamWhenLowLODDistance, false);
		TUNE_GROUP_FLOAT(LOD_DISTANCE, fDistanceFromPlayer, 50.0f, 0.0f, 1000.0f, 1.0f);
#if __DEV
		if( bShowStreamWhenLowLODDistance )
		{
			Vector3 vecOffset = vecPlayerPos;
			vecOffset.SetZ(vecOffset.GetZ() + 0.1f);
			grcDebugDraw::Cylinder(VECTOR3_TO_VEC3V(vecPlayerPos), VECTOR3_TO_VEC3V(vecOffset), fDistanceFromPlayer, Color_blue1, Color_blue, true, true, 12, 60);
		}
#endif

		if( Dot(vecPedDistanceToPlayerPos, vecPedDistanceToPlayerPos) > rage::square(fDistanceFromPlayer) )
		{
			return IBS_BlockNewIdles;
		}
#if __DEV
		else
		{
			// This guy is close enough to attempt to stream, draw some debug stuff.
			TUNE_GROUP_BOOL(LOD_DISTANCE, bShowPedsCloseEnoughToStreamWhenLowLOD, false);
			if( bShowPedsCloseEnoughToStreamWhenLowLOD )
			{
				grcDebugDraw::Sphere(vecPedPos, 0.4f, Color_GreenYellow, false, 60);
			}
		}
#endif
	}

	if( !m_Flags.IsFlagSet(Flag_PlayIdleClips) )
	{
		return IBS_BlockNewIdles;
	}

	if( pPed->GetAmbientAnimsBlocked() || pPed->GetAmbientIdleAndBaseAnimsBlocked() )
	{
		return IBS_BlockNewIdles;
	}

	// The streaming system is under pressure
	if( strStreamingEngine::GetIsLoadingPriorityObjects() )
	{
		return IBS_BlockNewIdles;
	}

	// We think the streaming system is under pressure because the player is driving fast in a vehicle
	float fPlayerSpeedSq = CGameWorld::FindLocalPlayerSpeed().Mag2();

	static dev_float SPEED_TO_STOP_IDLES = 22.0f;
	if( fPlayerSpeedSq > rage::square(SPEED_TO_STOP_IDLES) )
	{
		return IBS_BlockNewIdles;
	}

	// This ped is well underground
	if( pPed->GetTransform().GetPosition().GetZf() < -1000.0f)
	{
		return IBS_BlockNewIdles;
	}

	// The ped is playing a secondary clip
	const CTask* pSecondaryTask = pPed->GetPedIntelligence()->GetTaskSecondaryPartialAnim();
	if(	pSecondaryTask )
	{
		if( pSecondaryTask->GetTaskType() == CTaskTypes::TASK_MOBILE_PHONE )
		{
			return IBS_BlockAndRemoveIdles;
		}
		else if( pSecondaryTask->GetTaskType() != CTaskTypes::TASK_AGITATED )
		{
			return IBS_BlockNewIdles;
		}
	}

	// A cut scene is playing (don't want to overload the streaming system)
	if(	(CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning()) )
	{
		return IBS_BlockNewIdles;
	}

	// Swimming
	if( pPed->GetIsSwimming() )
	{
		return IBS_BlockNewIdles;
	}

	// Block and remove idles when arrested.
	if( pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed))
	{
		return IBS_BlockAndRemoveIdles;
	}

	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsDuckingInVehicle))
	{
		return IBS_BlockAndRemoveIdles;
	}

	if (IsInArgumentConversation())
	{
		return IBS_BlockNewIdles;
	}

	if (IsInHangoutConversation())
	{
		return IBS_BlockNewIdles;
	}
	
	if (m_Flags.IsFlagSet(Flag_AllowsConversation) && m_fPlayerConversationScenarioDistSq < sm_Tunables.m_playerNearToHangoutDistanceInMetersSquared)
	{
		return IBS_BlockNewIdles;
	}

	// Stop any idle playing that might block an active look at, only important look ats should start when an clip is already playing
	if( GetClipHelper() )
	{
		if( !CAmbientLookAt::DoBlendHelperFlagsAllowLookAt( pPed ) && m_pLookAt && m_pLookAt->GetCurrentLookAtTarget().GetIsValid() )
		{
			return IBS_BlockAndRemoveIdles;
		}
	}

	if( pPed->IsPlayer() )
	{
		const CPed* pPlayerPed = pPed;

		bool bBlockForGunBattle = false;
		if(pPed->IsUsingActionMode())
		{
			bBlockForGunBattle = pPed->IsBattleEventBlockingActionModeIdle();
		}
		else
		{
			bBlockForGunBattle = CEventGunShot::WasShotFiredWithinTime((u32)(sm_Tunables.m_TimeAfterGunshotForPlayerToPlayIdles*1000.0f));
		}

		// Blend out any standing clips if the player starts to move
		// Note: the use of the time step here after calling GetAnimatedAngularVelocity()
		// seems a bit peculiar - shouldn't this condition just be based on the angular velocity
		// of the player, not the amount we turn in one update?
		if( pPlayerPed->GetIsCrouching() || 
			pPlayerPed->GetPedResetFlag( CPED_RESET_FLAG_UsingMobilePhone ) || 
			(Abs(pPlayerPed->GetAnimatedAngularVelocity()*fTimeStep) > 0.01f ) || 
			bBlockForGunBattle ||
			pPlayerPed->GetPlayerInfo()->AreControlsDisabled() || 
			(pPlayerPed->GetCurrentMotionTask() && pPlayerPed->GetCurrentMotionTask()->GetTaskType() == CTaskTypes::TASK_MOTION_BASIC_LOCOMOTION && static_cast<CTaskMotionBasicLocomotion*>(pPlayerPed->GetCurrentMotionTask())->IsPerformingIdleTurn()))
		{
			return IBS_BlockNewIdles;
		}

		// Make sure any standing clips are blended out if the player is moving, or turning
		if( !pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) )
		{
			if( pPlayerPed->IsInMotion() || 
				pPlayerPed->GetIsCrouching() ||
				(pPlayerPed->GetCurrentMotionTask() && pPlayerPed->GetCurrentMotionTask()->GetTaskType() == CTaskTypes::TASK_MOTION_BASIC_LOCOMOTION && static_cast<CTaskMotionBasicLocomotion*>(pPlayerPed->GetCurrentMotionTask())->IsPerformingIdleTurn()))
			{
				return IBS_BlockAndRemoveIdles;
			}
		}

		// Remove any scenario idles when switching to first person
		if ( pPlayerPed->IsLocalPlayer() && pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UsingScenario) && pPlayerPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_CAMERA_VIEW_MODE_SWITCHED_TO_FIRST_PERSON) )
		{
			return IBS_BlockAndRemoveIdles;
		}
	}

	if(m_Flags.IsFlagSet(Flag_BlockIdleClipsAfterGunshots))
	{
		if(CEventGunShot::WasShotFiredWithinTime((u32)(sm_Tunables.m_TimeAfterGunshotToPlayIdles*1000.0f)))
		{
			return IBS_BlockNewIdles;
		}
	}


	if( m_pPropHelper && m_pPropHelper->IsForcedLoading())
	{
		return IBS_BlockNewIdles;
	}

	if (m_Event.IsValid() && !m_Event.IsReacting())
	{
		return IBS_BlockAndRemoveIdles;
	}

	if(pPed->GetWeaponManager())
	{
		const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		if(pWeapon && pWeapon->GetWeaponInfo()->GetBlockAmbientIdles())
		{
			return IBS_BlockAndRemoveIdles;
		}
	}

	return IBS_NoBlocking;
}

CAmbientLookAt* CTaskAmbientClips::CreateLookAt(const CPed* pPed)
{
	if (!m_pLookAt 
		&& CAmbientLookAt::GetPool()->GetNoOfFreeSpaces() > 0
		&& CAmbientLookAt::IsLookAtAllowedForPed(pPed, this, m_Flags.IsFlagSet( Flag_UseExtendedHeadLookDistance )))
	{
		m_pLookAt = rage_new CAmbientLookAt();
	}

	return m_pLookAt;
}

s32 CTaskAmbientClips::GetActiveClipPriority()
{
	if (GetClipHelperStatus() == CClipHelper::Status_FullyBlended)
	{
		// Idle
		return GetClipHelper()->GetPriority();
	}
	else
	{
		// Base
		return m_BaseClip.GetPriority();
	}
}

void CTaskAmbientClips::UpdateProp()
{
	// Cache the ped
	CPed* pPed = GetPed();

	if (pPed->GetWeaponManager() && m_pPropHelper)
	{
		m_pPropHelper->UpdateExistingOrMissingProp(*pPed);
	}
}




void CTaskAmbientClips::UpdateBaseClip()
{
	if (m_pPropHelper && m_pPropHelper->IsPropPersistent() && !m_pPropHelper->IsPropLoading() && !m_pPropHelper->IsForcedLoading())	// Force props to play default prop clips
	{
		UpdateBaseClipPlayback();
	}
	else if (m_Flags.IsFlagSet(Flag_PlayBaseClip))
	{
		bool bAllowBaseClipPlayback = true;
		CPed* pPed = GetPed();
		if (pPed->GetIsInVehicle())
		{
			if (pPed->GetCurrentMotionTask()->GetTaskType() == CTaskTypes::TASK_MOTION_IN_AUTOMOBILE)
			{
				const s32 iCurrentState = pPed->GetCurrentMotionTask()->GetState();
				if (iCurrentState == CTaskMotionInAutomobile::State_CloseDoor ||
					iCurrentState == CTaskMotionInAutomobile::State_Shunt ||
					iCurrentState == CTaskMotionInAutomobile::State_HeavyBrake)
				{
					bAllowBaseClipPlayback = false;
				}
			}

			if (pPed->GetPedIntelligence()->FindTaskActiveByTreeAndType(PED_TASK_TREE_SECONDARY, CTaskTypes::TASK_AGGRESSIVE_RUBBERNECK))
			{
				bAllowBaseClipPlayback = false;
			}

			// Don't play base clip if headbobbing and ...
			if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HeadbobToRadioEnabled))
			{
				CVehicle *pVehicle = pPed->GetVehiclePedInside();
				if (pVehicle && pVehicle->GetDriver() == pPed)
				{
					// if the ped is driving the vehicle and turning the wheel
					if (Abs(pVehicle->GetSteerAngle()) > sm_Tunables.m_MaxSteeringAngleForAmbientIdles)
					{
						bAllowBaseClipPlayback = false;
						m_bBlockHeadbobNextFrame = true;
					}
					// if the ped is the local player and has a wanted level
					if (pPed->IsLocalPlayer() && pPed->GetPlayerWanted() && pPed->GetPlayerWanted()->GetWantedLevel() > WANTED_CLEAN)
					{
						bAllowBaseClipPlayback = false;
						m_bBlockHeadbobNextFrame = true;
					}
				}
				
				//Only start if we've just hit a new beat
				if (!m_BaseClip.IsPlayingClip() && !m_bNextBeat)
				{
					bAllowBaseClipPlayback = false;
				}
			}
		}

		if (bAllowBaseClipPlayback)
		{
			UpdateBaseClipPlayback();
		}
		else if (m_BaseClip.GetClipHelper())
		{
			m_BaseClip.StopClip();
		}
	}
	else if( m_BaseClip.GetClipHelper() )
	{
		m_BaseClip.StopClip();
	}
}

void CTaskAmbientClips::UpdateBaseClipPlayback()
{
	CPed *pPed = GetPed();

	bool bLowriderBlockBaseClip = false;
	if (pPed->GetVehiclePedInside() && pPed->GetVehiclePedInside()->InheritsFromAutomobile())
	{
		// Stop base clip for a short amount of time after we last modified the suspension.
		CAutomobile *pAutomobile = static_cast<CAutomobile*>(pPed->GetVehiclePedInside());
		u32 uTimeHydraulicsLastModified = pAutomobile ? pAutomobile->GetTimeHydraulicsModified() : 0;
		TUNE_GROUP_INT(LOWRIDER_TUNE, iTimeToBlockIdleBaseClipAfterHydraulicsModified, 1250, 0, 10000, 1);
		if (uTimeHydraulicsLastModified + iTimeToBlockIdleBaseClipAfterHydraulicsModified > fwTimer::GetTimeInMilliseconds() )
		{
			bLowriderBlockBaseClip = true;
		}

		// Stop base clip if we should but aren't using the alternate clipset or we're playing an arm transition anim.
		const CTaskMotionInAutomobile* pAutoMobileTask = static_cast<const CTaskMotionInAutomobile*>(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_AUTOMOBILE));
		if (!bLowriderBlockBaseClip && pAutoMobileTask && pAutoMobileTask->ShouldUseLowriderLeanAltClipset() && (!pAutoMobileTask->IsUsingAlternateLowriderClipset() || pAutoMobileTask->IsDoingLowriderArmTransition()))
		{
			bLowriderBlockBaseClip = true;
		}
	}

	if ((pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsDuckingInVehicle) || bLowriderBlockBaseClip || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingHelmetVisor)) && IsBaseClipPlaying())
	{
		TUNE_GROUP_FLOAT(IN_VEHICLE_TUNE, DUCK_AMBIENT_BLEND_DELTA, -4.0f, -16.0f, 4.0f, 1.0f);
		StopBaseClip(DUCK_AMBIENT_BLEND_DELTA);
	}

	// Stop base anims on bikes when they are no longer valid.
	if (pPed->GetVehiclePedInside() && pPed->GetVehiclePedInside()->InheritsFromBike() && IsBaseClipPlaying())
	{
		IdleBlockingState iIdleBlockingState = GetIdleBlockingState(GetTimeStep());
		if(iIdleBlockingState == IBS_BlockAndRemoveIdles)
		{
			TUNE_GROUP_FLOAT(IN_VEHICLE_TUNE, BIKE_AMBIENT_BLEND_DELTA, -4.0f, -16.0f, 4.0f, 1.0f);
			StopBaseClip(BIKE_AMBIENT_BLEND_DELTA);
		}
	}

	m_fTimeLastSearchedForBaseClip -= GetTimeStep();

	CScenarioCondition::sScenarioConditionData conditionData;
	InitConditionData(conditionData);

	if(m_Flags.IsFlagSet(Flag_SynchronizedSlave))
	{
		bool playSynced = false;
		const CTaskAmbientClips* pMasterTask = FindSynchronizedMasterTask();
		if(pMasterTask)
		{
			if(pMasterTask->m_BaseSyncedSceneId != INVALID_SYNCED_SCENE_ID)
			{
				playSynced = true;
			}
		}

		if(playSynced)
		{
			conditionData.m_AllowNotSynchronized = false;
		}
		else
		{
			conditionData.m_AllowSynchronized = false;
		}
	}

#if SCENARIO_DEBUG
	// When trying to transition to a base anim ignore the Reevaluate result as we want to transition to it as soon
	// as the other clip is done playing ... 
	bool debugAlwaysReevaluate = false;
	if (m_Flags.IsFlagSet(Flag_UseDebugAnimData) && m_DebugInfo.m_AnimType == CConditionalAnims::AT_BASE)
	{
		debugAlwaysReevaluate = true;
	}
#endif // SCENARIO_DEBUG

	const bool bShouldReevaluate = ( ShouldReevaluateBaseClip(conditionData) SCENARIO_DEBUG_ONLY(|| debugAlwaysReevaluate) );
	if (m_bReevaluateHeadbobBase) m_bReevaluateHeadbobBase = false;
	if(!bShouldReevaluate)
	{
		return;
	}

	m_fTimeLastSearchedForBaseClip = 10.0f * fwRandom::GetRandomNumberInRange(0.9f, 1.1f);

	if ( m_ConditionalAnimChosen == -1 )
	{
		int animsChosen = m_ConditionalAnimChosen;
		s32 iChosenGroupPriority = 0;	// Not actually used.
		CAmbientAnimationManager::ChooseConditionalAnimations(conditionData, iChosenGroupPriority, m_pConditionalAnimsGroup, animsChosen, CConditionalAnims::AT_BASE);
		SetConditionalAnimChosen(animsChosen);
	}

	// If a base clip is already playing - make sure the conditions are still valid.
	if(IsBaseClipPlaying())
	{
		taskAssert(m_BaseClipRequestHelper.GetClipSet() != CLIP_SET_ID_INVALID);
		taskAssert(m_pConditionalAnimsGroup);

#if __BANK
		bool ignoreConditions = false;
		if (m_Flags.IsFlagSet(Flag_UseDebugAnimData))
		{
			ignoreConditions = true;
		}
#endif
		if (pPed->GetAmbientIdleAndBaseAnimsBlocked())
		{
			StopBaseClip(SLOW_BLEND_OUT_DELTA);
		}

		if( taskVerifyf(m_pConditionalAnimsGroup, "%s has null conditional group! m_ConditionalAnimChosen %d",GetPed()->GetDebugName(),m_ConditionalAnimChosen) && 
			m_ConditionalAnimChosen >=0 && 
			!CheckConditionsOnPlayingBaseClip(conditionData) BANK_ONLY(&& !ignoreConditions))
		{
			// Check to see if we should force this base anim to play until the next idle is streamed in. 
			// For example, this ensures the ped won't put his arm down during a mobile phone call before the exit streams in.
			const CConditionalAnims* pConditionalAnims = m_pConditionalAnimsGroup->GetAnims(m_ConditionalAnimChosen);
			if ( pConditionalAnims && pConditionalAnims->IsForceBaseUntilIdleStreams() && (GetState() != State_PlayIdleAnimation) && !IsInConversation() )
			{
				return;
			}

			if(!StopBaseClip(SLOW_BLEND_OUT_DELTA))
			{
				// Not sure about this case.
				return;
			}
		}
#if __ASSERT
		else
		{
			if (m_pConditionalAnimsGroup && m_ConditionalAnimChosen < 0)
			{
				taskWarningf("%s using m_pConditionalAnimsGroup %s has Invalid m_ConditionalAnimChosen %d", GetPed()->GetDebugName(), m_pConditionalAnimsGroup->GetName(), m_ConditionalAnimChosen);
			}
		}
#endif
	}

#if SCENARIO_DEBUG
	bool debugOverride = false;
	if (m_Flags.IsFlagSet(Flag_UseDebugAnimData) && m_DebugInfo.m_AnimType == CConditionalAnims::AT_BASE)
	{
		static float amount = 0.95f;
		//Make sure we are not playing another animation currently ... 
		if ( !GetIsPlayingAmbientClip() && IsIdleFinished()	)
		{
			debugOverride = true;
		}
		//if the clip is just at the end then we should let the base start playing for blending purposes
		else if (GetClipHelper() && GetClipHelper()->GetPhase() >= amount && !m_DebugStartedBaseAnim) 
		{
			debugOverride = true;
		}
		//if the clip itself sets a blend out phase then we need to make sure we put the base in just before the blend out happens
		else if (GetClipHelper() && GetClipHelper()->GetPhase() >= (m_fBlendOutPhase - 0.05f) && !m_DebugStartedBaseAnim)
		{
			debugOverride = true;
		}
		else
		{
			return;//still not ready to transition yet ... 
		}
	}
#endif // SCENARIO_DEBUG

	if(((m_BaseClip.IsPlayingClip() && !m_LowLodBaseClipPlaying) || m_ConditionalAnimChosen < 0) SCENARIO_DEBUG_ONLY(&& !debugOverride))
	{
		return;
	}

	// No clip is currently playing - try and select and stream in a new one.

	if(!StreamBaseClips(conditionData))
	{
		// The first time we get here, if we don't have the base animation we want,
		// consider starting the fallback base clip.
		if(!m_LowLodBaseClipConsidered)
		{
			StartLowLodBaseClip();
			m_LowLodBaseClipConsidered = true;
		}

		// reset the timer to try again next frame
		m_fTimeLastSearchedForBaseClip = 0.0f;
		return;
	}

	if (!pPed->GetAmbientIdleAndBaseAnimsBlocked() && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsDuckingInVehicle) && !bLowriderBlockBaseClip)
	{
		StartBaseClip();
	}

#if __BANK
	if (m_Flags.IsFlagSet(Flag_UseDebugAnimData))
	{
		m_DebugStartedBaseAnim = true;
	}
#endif
}


void CTaskAmbientClips::StartLowLodBaseClip()
{
	// Check if we have decided on a group to use.
	if(!m_pConditionalAnimsGroup || m_ConditionalAnimChosen < 0)
	{
		return;
	}

	// Get the CConditionalAnims object we have decided to use.
	const CConditionalAnims* pAnims = m_pConditionalAnimsGroup->GetAnims(m_ConditionalAnimChosen);
	if(!pAnims)
	{
		return;
	}

	// Check if we are supposed to blend into the base clip instantaneously. Note that
	// we probably only want to do this once, so we clear it if we start the fallback clip.
	// We actually also clear it even if we are not, because the user probably set it expecting
	// us to play a base animation on the first frame, which we now can't do, and it probably
	// looks better to blend into the animation once it becomes available than to pop in one frame.
	bool binstantBlend = m_Flags.IsFlagSet(Flag_InstantlyBlendInBaseClip);
	m_Flags.ClearFlag(Flag_InstantlyBlendInBaseClip);

	fwMvClipId clipId = pAnims->GetLowLodBaseAnim();
	if(clipId.IsNull())
	{
		// No low LOD base animation specified, if this is a scenario we will probably just
		// end up with the regular standing animations.
		return;
	}

	float fBlend = m_fBlendInDelta;
	if(binstantBlend)
	{
		fBlend = INSTANT_BLEND_IN_DELTA;
	}

	if(m_BaseClip.StartClipBySetAndClip(GetPed(), sm_Tunables.m_LowLodBaseClipSetId, clipId, fBlend, CClipHelper::TerminationType_OnDelete))
	{
		m_LowLodBaseClipPlaying = true;
	}
}


void CTaskAmbientClips::StartBaseClip()
{
	m_LowLodBaseClipPlaying = m_LowLodBaseClipConsidered = false;

	// Clear out any previous immediate condition.
	m_BaseAnimImmediateCondition = NULL;

	fwMvClipSetId clipSetIndex = m_BaseClipRequestHelper.GetClipSet();
	taskFatalAssertf(clipSetIndex != CLIP_SET_ID_INVALID, "Invalid clip set");

	// process weapon clipset overrides
	ProcessWeaponClipSetOverride(clipSetIndex);

	if(ShouldPlayOnFootClipSetOverride())
	{
		StartOnFootClipSetOverride(clipSetIndex);
	}
	else
	{
		fwMvClipId iClipID(CLIP_ID_INVALID);
		bool bExistingClipFound = false;
		float fExistingPhase = 0.0f;
#if SCENARIO_DEBUG
		if (m_Flags.IsFlagSet(Flag_UseDebugAnimData) && m_DebugInfo.m_AnimType == CConditionalAnims::AT_BASE)
		{
			iClipID = m_DebugInfo.m_ClipId;
		}
		else
#endif // SCENARIO_DEBUG
		if (m_Flags.IsFlagSet(Flag_OverriddenBaseAnim))
		{
			iClipID = m_OverriddenBaseClip;
		}
		else
		{
			CPed* pPed = GetPed();
			// Get the first clip player.
			CClipHelper* pClipPlayer = pPed->GetMovePed().GetBlendHelper().FindClipBySetId(clipSetIndex);
			if( pClipPlayer )
			{
				// TODO: Check if this case is still relevant - might be related to dummy peds which no longer exist. /FF

				iClipID = fwMvClipId(pClipPlayer->GetClipHashKey());
				bExistingClipFound = true;
				fExistingPhase = pClipPlayer->GetPhase();
			}

			if( !bExistingClipFound )
			{
				// Try to pick a new base clip randomly.
				if(!CAmbientClipRequestHelper::PickClipByGroup_SimpleConditions(pPed, m_BaseClipRequestHelper.GetClipSet(), iClipID))
				{
					// We failed, trigger an informative assert.
#if __ASSERT
					fwMvClipSetId clipSetId = m_BaseClipRequestHelper.GetClipSet();
					const char* clipSetName = clipSetId.TryGetCStr();
					const u32 clipSetHash = clipSetId.GetHash();
					const fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
					int iNoClipsInSet = pClipSet ? pClipSet->GetClipItemCount() : -1;
					aiAssertf(0, "Ped %s failed to pick an animation from clip set %s (%08x), is it empty? %d items in set.",
							pPed->GetDebugName(), clipSetName ? clipSetName : "?", clipSetHash, iNoClipsInSet);
#endif	// __ASSERT
				}
			}
		}

		float fBlend = m_fBlendInDelta;
		if (m_pConditionalAnimsGroup && m_ConditionalAnimChosen != -1)
		{
			fBlend = m_pConditionalAnimsGroup->GetAnims(m_ConditionalAnimChosen)->GetBlendInDelta();
		}

		bool binstantBlend = m_Flags.IsFlagSet(Flag_InstantlyBlendInBaseClip);
		m_Flags.ClearFlag(Flag_InstantlyBlendInBaseClip);

		if( binstantBlend )
		{
			fBlend = INSTANT_BLEND_IN_DELTA;
		}

		if (m_bPlayBaseHeadbob)
		{
			TUNE_GROUP_FLOAT(HEAD_BOB_TUNE, fBlendInDelta, 1.0f, 0.0f, 10.0f, 0.01f);
			fBlend = fBlendInDelta; //REALLY_SLOW_BLEND_IN_DELTA;
		}

		if(ShouldPlaySynchronizedBaseClip())
		{
			// If we are in State_PlayIdleAnimation, we probably should never try to start
			// a new synchronized base clip, because normally when we get to that state,
			// we would have cleared the base clip's CTaskSynchronizedScene, so it wouldn't
			// be correct to create a new one of those. And, if we're actually playing a synchronized
			// idle animation, doing so would potentially kill the idle.
			if(GetState() == State_PlayIdleAnimation)
			{
				return;
			}

			if(!StartSynchronizedBaseClip(clipSetIndex, iClipID, fBlend))
			{
				// TODO: Think more about this failure case, if it happens.
				taskAssertf(0, "Failed to start synchronized scenario animation.");
			}

			// Note: unlike the synchronized case, we don't call m_BaseClipRequestHelper.ReleaseClips()
			// here. If we did, it might be possible that the animations would get streamed
			// out while we play idle animations.
		}
		else
		{
			StartUnsynchronizedBaseClip(clipSetIndex, iClipID, fBlend, bExistingClipFound, fExistingPhase);

			// Release the streamed clips
			m_BaseClipRequestHelper.ReleaseClips();
		}
	}

	// Check if there are any conditions that we need to test each frame from now on.
	// If so, m_BaseAnimImmediateCondition will point to it.
	if (m_pConditionalAnimsGroup && m_ConditionalAnimChosen >= 0)
	{
		const CScenarioCondition* immediateConditions[1];
		const CConditionalAnims* pConditionalAnims = m_pConditionalAnimsGroup->GetAnims(m_ConditionalAnimChosen);
		int numImmediateConditions = pConditionalAnims->GetImmediateConditions(clipSetIndex, immediateConditions, NELEM(immediateConditions), CConditionalAnims::AT_BASE);
		if(numImmediateConditions)
		{
			m_BaseAnimImmediateCondition = immediateConditions[0];
		}
	}

	// If we're playing a base clip without an enter or exit, we should no longer be playing a ringtone [8/23/2012 mdawe]
	if (m_pConversationHelper && m_pConversationHelper->IsPhoneConversation() && GetState() == State_NoClipPlaying)
	{
		m_pConversationHelper->ClearRingToneFlag();
	}
}

bool CTaskAmbientClips::CreatePhoneConversationHelper()
{
	CPed* pPed = GetPed();
	if (pPed && !pPed->IsNetworkClone())
	{
		if (m_pConditionalAnimsGroup && m_ConditionalAnimChosen >= 0)
		{
			const CConditionalAnims* pConditionalAnims = m_pConditionalAnimsGroup->GetAnims(m_ConditionalAnimChosen);
			if (pConditionalAnims && pConditionalAnims->IsMobilePhoneConversation())
			{
				if (!m_pConversationHelper)
				{
					// The animation should have a condition on it to prevent this check from failing, but just in case.
					if (CTaskConversationHelper::GetPool()->GetNoOfFreeSpaces() > 0) 
					{
						m_pConversationHelper = rage_new CTaskConversationHelper();
						m_pConversationHelper->StartPhoneConversation(GetPed());
					}
				}
				return true;
			}
		}
	}
	return false;
}

void CTaskAmbientClips::StartOnFootClipSetOverride(const fwMvClipSetId& clipSetIndex)
{
	CPed* pPed = GetPed();

	CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();
	if(pTask)
	{
		// Override the whole on-foot clip set, instead of just playing an animation.
		pTask->SetOnFootClipSet(clipSetIndex);
	}

	m_Flags.SetFlag(Flag_HasOnFootClipSetOverride);

	// Make sure we don't think we're still playing some synchronized scene.
	taskAssert(m_BaseSyncedSceneId == INVALID_SYNCED_SCENE_ID);
}

void CTaskAmbientClips::ProcessWeaponClipSetOverride(const fwMvClipSetId& clipSetIndex)
{
	CPed* pPed = GetPed();

	// if we have a motion task
	CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();
	if(pTask)
	{
		// if we should start the override
		bool bSetWeaponClipSetOverride = ShouldSetWeaponClipSetOverride();
		if (bSetWeaponClipSetOverride)
		{
			// override the weapon clipset
			pTask->SetOverrideWeaponClipSet(clipSetIndex);
		}
		// else if it was set from a previous anim
		else if (pTask->HasWeaponClipSetSetFromAnims())
		{
			// clear the override
			pTask->ClearOverrideWeaponClipSet();
		}
		// set the motion task flag
		pTask->SetWeaponClipSetFromAnims(bSetWeaponClipSetOverride);

	}
	// ensure we are not playing a sychronized scene
	taskAssert(m_BaseSyncedSceneId == INVALID_SYNCED_SCENE_ID);
}

bool CTaskAmbientClips::StartSynchronizedBaseClip(const fwMvClipSetId& clipSetIndex /* TODO: RENAME */
		, const fwMvClipId& iClipID /* TODO: RENAME */
		, float fBlend)
{
	m_BaseSyncedSceneId = INVALID_SYNCED_SCENE_ID;

	fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetIndex);
	const crClip* pClip = pClipSet->GetClip(iClipID);
	if(!pClip)
	{
		return false;
	}

	const crClipDictionary* pClipDict = pClip->GetDictionary();
	if(!pClipDict)
	{
		return false;
	}

	fwSyncedSceneId syncedScene = INVALID_SYNCED_SCENE_ID;
	if(m_Flags.IsFlagSet(Flag_SynchronizedSlave))
	{
		const CTaskAmbientClips* pMasterTask = FindSynchronizedMasterTask();
		if(pMasterTask)
		{
			if(pMasterTask->m_BaseSyncedSceneId != INVALID_SYNCED_SCENE_ID)
			{
				syncedScene = pMasterTask->m_BaseSyncedSceneId;
			}
		}
	}
	else if(m_Flags.IsFlagSet(Flag_SynchronizedMaster))
	{
		taskAssert(m_ExternalSyncedSceneId != INVALID_SYNCED_SCENE_ID);
		syncedScene = m_ExternalSyncedSceneId;
	}
	else
	{
		taskAssertf(0, "Neither synchronized flag is set.");
	}

	if(!fwAnimDirectorComponentSyncedScene::IsValidSceneId(syncedScene))
	{
		return false;
	}

	m_BaseSyncedSceneId = syncedScene;

	float blendInDelta = fBlend;
	float blendOutDelta = NORMAL_BLEND_OUT_DELTA;	// Not sure about this one.

	const atHashString clipDictName = pClipSet->GetClipDictionaryName();

	eSyncedSceneFlagsBitSet flags;
	flags.BitSet().Set(SYNCED_SCENE_USE_KINEMATIC_PHYSICS, true);

	CTaskSynchronizedScene* pSyncSceneTask = rage_new CTaskSynchronizedScene(syncedScene, iClipID, clipDictName, blendInDelta, blendOutDelta, flags);
	SetNewTask(pSyncSceneTask);

	return true;
}


bool CTaskAmbientClips::StartUnsynchronizedBaseClip(const fwMvClipSetId& clipSetIndex, const fwMvClipId& iClipID, float fBlend, bool bExistingClipFound, float fExistingPhase)
{
	CPed* pPed = GetPed();

	const bool bIsCloneOnFootLocalInVehicle = pPed->IsNetworkClone() && m_Flags.IsFlagSet(Flag_IsInVehicle) && !pPed->GetIsInVehicle();
#if __BANK
	if (m_Flags.IsFlagSet(Flag_UseDebugAnimData) && m_DebugStartedBaseAnim)
	{
		// do not start it again because it is already started ... just let it get reset ... 
	}
	else
#endif
	{
		// If we think we are in a vehicle but aren't then don't play the clip as the pose will be wrong
		if (!bIsCloneOnFootLocalInVehicle)
		{
			m_BaseClip.StartClipBySetAndClip( pPed, clipSetIndex, iClipID, fBlend, CClipHelper::TerminationType_OnDelete );
		}
	}

	m_VfxTagsExistBase = false;
	m_PropTagsExistBase = false;
	const crClip* pClip = m_BaseClip.GetClip();
	if( pClip )
	{
		m_VfxTagsExistBase = CheckClipHasVFXTags(*pClip);
		m_PropTagsExistBase = CheckClipHasPropTags(*pClip);
		if( m_VfxTagsExistBase || m_PropTagsExistBase )
		{
			// We just set m_VfxTagsExistBase or m_PropTagsExistBase to true, make sure we get calls to
			// ProcessMoveSignals() so the effects get updated properly.
			RequestProcessMoveSignalCalls();
		}

		// Determine how much to phase the base anim in based on whether or not an enter anim has played.
		float fPhase = 0.0f;
		if (!m_Flags.IsFlagSet(rage::u32(Flag_DontRandomizeBaseOnStartup)))
		{
			fPhase = fwRandom::GetRandomNumberInRange(0.0f, 0.9f);
		}
		m_BaseClip.SetPhase(fPhase);

		// Set the clip phase to that found existing (from dummy->ped conversion)
		if( bExistingClipFound )
		{
			m_BaseClip.SetPhase(fExistingPhase);
		}

		// B*1434308 - Disable rate randomization on Player idles [6/3/2013 mdawe]
		if (!pPed->IsPlayer())
		{
			m_BaseClip.SetRate(fwRandom::GetRandomNumberInRange(0.9f, 1.1f));
		}

#if SCENARIO_DEBUG
		if (m_Flags.IsFlagSet(Flag_UseDebugAnimData) && m_DebugInfo.m_AnimType == CConditionalAnims::AT_BASE)
		{
			// if we are still playing a clip (ie at_variation) then we are not ready to get the next debug 
			// anim ... 
			if (!GetClipHelper())
			{
				m_Flags.SetFlag(Flag_WantNextDebugAnimData);
			}
		}
#endif // SCENARIO_DEBUG
		return true;
	}	
	else
	{
		aiAssert(bIsCloneOnFootLocalInVehicle);

		return false;
	}
}


bool CTaskAmbientClips::StopBaseClip(float fBlend)
{
	if(m_BaseSyncedSceneId != INVALID_SYNCED_SCENE_ID)
	{
		CTask* pSubTask = GetSubTask();
		if(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_SYNCHRONIZED_SCENE)
		{
			CTaskSynchronizedScene* pSynchronizedSceneTask = static_cast<CTaskSynchronizedScene*>(pSubTask);
			if(pSynchronizedSceneTask->GetScene() == m_BaseSyncedSceneId)
			{
				if(!pSynchronizedSceneTask->MakeAbortable(aiTask::ABORT_PRIORITY_URGENT, NULL))
				{
					// Couldn't abort the subtask. Don't think I've seen this actually happen in practice.
					return false;
				}
			}
		}

		m_BaseClipRequestHelper.ReleaseClips();
		m_BaseSyncedSceneId = INVALID_SYNCED_SCENE_ID;
	}
	else
	{
		u32 uFlags = CMovePed::Task_IgnoreMoverBlendRotation;
		m_BaseClip.StopClip(fBlend, uFlags);
	}

#if __BANK
	if (m_Flags.IsFlagSet(Flag_UseDebugAnimData))
	{
		m_DebugStartedBaseAnim = false;
	}
#endif

	// There should no longer be a playing base clip, so we should probably
	// clear this:
	m_VfxTagsExistBase = false;
	m_PropTagsExistBase = false;

	return true;
}



bool CTaskAmbientClips::ShouldPlayOnFootClipSetOverride() const
{
	if (m_pConditionalAnimsGroup && m_ConditionalAnimChosen >= 0)
	{
		const CConditionalAnims* pAnims = m_pConditionalAnimsGroup->GetAnims(m_ConditionalAnimChosen);
		if(pAnims && pAnims->GetOverrideOnFootClipSetWithBase())
		{
			return true;
		}
	}
	return false;
}

bool CTaskAmbientClips::ShouldSetWeaponClipSetOverride() const
{
	// if our anims group is valid and we have selected an anim inside
	if (m_pConditionalAnimsGroup && m_ConditionalAnimChosen >= 0)
	{
		// if we should override the weapon clip set with the base anim
		const CConditionalAnims* pAnims = m_pConditionalAnimsGroup->GetAnims(m_ConditionalAnimChosen);
		if (pAnims && pAnims->GetOverrideWeaponClipSetWithBase())
		{
			return true;
		}
	}
	return false;
}

bool CTaskAmbientClips::ShouldPlaySynchronizedBaseClip() const
{
	if(IsSynchronized() && m_BaseClipRequestHelper.IsClipSetSynchronized())
	{
		return true;
	}
	return false;
}


bool CTaskAmbientClips::CheckConditionsOnPlayingBaseClip(const CScenarioCondition::sScenarioConditionData& conditionData) const
{
	fwMvClipSetId clipSet;
	if(m_BaseSyncedSceneId == INVALID_SYNCED_SCENE_ID)
	{
		clipSet = m_BaseClip.GetClipSet();
	}
	else
	{
		clipSet = m_BaseClipRequestHelper.GetClipSet();
	}

	const CConditionalAnims * pAnims;
	if(m_ConditionalAnimChosen >= 0 && taskVerifyf(m_ConditionalAnimChosen < m_pConditionalAnimsGroup->GetNumAnims(),
			"Invalid animation group (%d), for %s.",
			m_ConditionalAnimChosen, m_pConditionalAnimsGroup->GetName()))
	{
		pAnims = m_pConditionalAnimsGroup->GetAnims( m_ConditionalAnimChosen );
	}
	else
	{
		// Not sure exactly under what circumstances this could happen, but a crash was reported
		// which seems to be because m_ConditionalAnimChosen was -1. Might have something to do with
		// the data set up in some unusual way.
		// See B* 278637: "Crash navigating pause menu -   905dc4 - CTaskAmbientClips::UpdateBaseClipPlayback(CClipHelper&, int)+1cc"

		// Update: I looked at this again for B* 486452: "Ignorable assert: TaskAmbient.cpp(2986): [ai_task] Error: Verifyf(m_ConditionalAnimChosen >= 0 && m_ConditionalAnimChosen < m_pConditionalAnimsGroup->GetNumAnims()) FAILED: Invalid animation group (-1), for WANDER.".
		// I think a way in which this could potentially happen is:
		// 1. The ped has a permanent prop.
		// 2. In NoClipPlaying_OnUpdate(), we evaluate the conditions, and set m_ConditionalAnimChosen to -1 if they are not fulfilled.
		// 3. The old base clip continues to play.
		// 4. In UpdateBaseClipPlayback(), we eventually reevaluate the base clip, and call
		//    CheckConditionsOnPlayingBaseClip(), but m_ConditionalAnimChosen is -1.
		// Now, we should stop the base clip in that case, and we'll try to pick another one if we can find one
		// that fulfills the conditions.

		pAnims = NULL;
	}

	if( pAnims == NULL || !( pAnims->CheckConditions(conditionData, NULL) ) )
	{
		return false;
	}
	else
	{
		// Note: this code was changed slightly compared to how it used to work. It used to loop
		// to find a CConditionalClipSet matching the clipset hash we are using, and fail if the
		// conditions for that are unfulfilled. Now, we loop and check the condition for each
		// clipset that's matching our hash, and if the conditions for that are fulfilled, we allow
		// the clipset to continue playing. This should make it work in the uncommon case of
		// the same clipset being used with different conditions.

		bool foundMatching = false;
		const CConditionalAnims::ConditionalClipSetArray* pConditionalAnims = pAnims->GetClipSetArray( CConditionalAnims::AT_BASE );
		if ( pConditionalAnims )
		{
			for ( s32 i = 0; i < pConditionalAnims->GetCount(); i++)
			{
				const CConditionalClipSet* pBaseAnim = (*pConditionalAnims)[i];
				if ( pBaseAnim->GetClipSetHash() == clipSet )
				{
					if(pBaseAnim->CheckConditions(conditionData))
					{
						foundMatching = true;
						break;
					}
				}
			}
		}

		if ( !foundMatching )
		{
			return false;
		}
	}

	return true;
}

void CTaskAmbientClips::InitConditionData(CScenarioCondition::sScenarioConditionData &conditionDataOut) const
{
	s32 iFlags = 0;

	conditionDataOut.pPed = GetPed();
	conditionDataOut.iScenarioFlags = iFlags;
	if(m_Flags.IsFlagSet(Flag_SynchronizedSlave))
	{
		conditionDataOut.m_RoleInSyncedScene = RISS_Slave;
	}
	else if(m_Flags.IsFlagSet(Flag_SynchronizedMaster))
	{
		conditionDataOut.m_RoleInSyncedScene = RISS_Master;
	}
}


bool CTaskAmbientClips::ShouldReevaluateBaseClip(CScenarioCondition::sScenarioConditionData& conditionDataInOut) const
{
	// If m_BaseAnimImmediateCondition is set, we check that condition, and
	// if it's no longer fulfilled, we go through the code as we would use after
	// a timeout.
	if(m_BaseAnimImmediateCondition && !m_BaseAnimImmediateCondition->Check(conditionDataInOut))
	{
		return true;
	}

	// If we're playing a synchronized base animation and we are not supposed to, reevaluate.
	if(!conditionDataInOut.m_AllowSynchronized && m_BaseSyncedSceneId != INVALID_SYNCED_SCENE_ID)
	{
		return true;
	}

	// Conversely, if we are not allowed to play an animation that's not synchronized, but we are,
	// then reevaluate. This could happen if the slave and master were playing unsynchronized animations,
	// and then the master switched to play a synchronized one.
	if(!conditionDataInOut.m_AllowNotSynchronized && m_BaseSyncedSceneId == INVALID_SYNCED_SCENE_ID)
	{
		return true;
	}

	if(m_fTimeLastSearchedForBaseClip <= 0.0f)
	{
		return true;
	}

	if (m_bReevaluateHeadbobBase)
	{
		return true;
	}

	return false;
}


bool CTaskAmbientClips::StreamBaseClips(const CScenarioCondition::sScenarioConditionData& conditionData)
{
	// No clips selected - try to choose a valid set of clips
	if( m_BaseClipRequestHelper.GetState() == CAmbientClipRequestHelper::AS_noGroupSelected  && (m_ConditionalAnimChosen >= 0 BANK_ONLY(|| m_Flags.IsFlagSet(Flag_UseDebugAnimData))))
	{
#if SCENARIO_DEBUG
		if (m_Flags.IsFlagSet(Flag_UseDebugAnimData) && m_DebugInfo.m_AnimType == CConditionalAnims::AT_BASE)
		{
			m_BaseClipRequestHelper.SetClipAndProp(m_DebugInfo.m_ClipSetId, m_DebugInfo.m_PropName.GetHash());
		}
		else
#endif // SCENARIO_DEBUG
		if (m_Flags.IsFlagSet(Flag_OverriddenBaseAnim))
		{
			m_BaseClipRequestHelper.SetClipAndProp(m_OverriddenBaseClipSet, 0);
		}
		else
		{
			const u32 uChosenPropHash = (m_pPropHelper ? static_cast<u32>(m_pPropHelper->GetPropNameHash()): 0);
			m_BaseClipRequestHelper.ChooseRandomClipSet(conditionData, m_pConditionalAnimsGroup->GetAnims(m_ConditionalAnimChosen), CConditionalAnims::AT_BASE, uChosenPropHash);

			// if we have valid base anims chosen AND we do not have a prop helper
			if ( (m_pConditionalAnimsGroup && m_ConditionalAnimChosen >= 0) && !m_pPropHelper )
			{
				// check if our conditional anims have a propset specified
				const CConditionalAnims* pConditionalAnims = m_pConditionalAnimsGroup->GetAnims(m_ConditionalAnimChosen);
				if ( pConditionalAnims && pConditionalAnims->GetPropSetHash() != 0 )
				{
					// that propset should be valid
					const CAmbientPropModelSet* pPropSet = CAmbientAnimationManager::GetPropSetFromHash(pConditionalAnims->GetPropSetHash());
					if (taskVerifyf(pPropSet, "Could not get a propset from conditional anim hash [%s] in TaskAmbient", pConditionalAnims->GetPropSetName()))
					{
						// we should have a valid model index in the propset
						strLocalIndex propModelIndex = strLocalIndex(pPropSet->GetRandomModelIndex());
						if (taskVerifyf(CModelInfo::IsValidModelInfo(propModelIndex.Get()), "Prop set [%s] with model index [%d] not a valid model index", pPropSet->GetName(), propModelIndex.Get()))
						{
							// the prop model should exist for that index
							const CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(propModelIndex));
							Assert(pModelInfo);

							// create our prop management helper
							InitPropHelper();
							m_pPropHelper->RequestProp(pModelInfo->GetModelNameHash());
							m_pPropHelper->SetCreatePropInLeftHand(pConditionalAnims->GetCreatePropInLeftHand());
							m_pPropHelper->SetDestroyProp(pConditionalAnims->ShouldDestroyPropInsteadOfDropping());
						}
					}
				}
			}

			if (CreatePhoneConversationHelper())
			{
				m_Flags.SetFlag(Flag_PhoneConversationShouldPlayEnterAnim);
			}
		}
	}

	// If a group has been selected - try to stream it in
	if( m_BaseClipRequestHelper.GetState() == CAmbientClipRequestHelper::AS_groupSelected )
	{
		m_BaseClipRequestHelper.RequestClips(false); // false = doesn't require ambient clip manager permission to load
	}

	// If the base clips have successfully streamed - pick a clip
	if( m_BaseClipRequestHelper.GetState() != CAmbientClipRequestHelper::AS_groupStreamed )
	{
		return false;
	}

#if SCENARIO_DEBUG
	// Check if the debug option to not stream in base animations is enabled. Note that
	// we actually don't block the streaming, we just block the task's ability to see
	// that it is streamed in.
	if(sm_DebugPreventBaseAnimStreaming)
	{
		return false;
	}
#endif	// SCENARIO_DEBUG

	// By now we should have played our Enter anim [8/22/2012 mdawe]
	m_Flags.ClearFlag(Flag_PhoneConversationShouldPlayEnterAnim);

	return true;
}

//  Returns true if this ConditionalAnims group associated with the task supports reaction animations.
bool CTaskAmbientClips::HasReactionClip(const CEventShocking* pEvent) const
{
	//set up condition data based on the event and current ped
	CScenarioCondition::sScenarioConditionData conditionData;
	conditionData.pPed = GetPed();
	conditionData.vAmbientEventPosition = pEvent->GetEntityOrSourcePosition();
	conditionData.eAmbientEventType = pEvent->GetTunables().m_AmbientEventType;
	//verify that we have chosen a conditional animation group
	if (m_pConditionalAnimsGroup && m_ConditionalAnimChosen != -1)
	{
		const CConditionalAnims* pAnims = m_pConditionalAnimsGroup->GetAnims(m_ConditionalAnimChosen);
		//verify that there is a clipset that meets our demands inside the clipset array for reactions
		if (pAnims && pAnims->CheckClipSet(*(pAnims->GetClipSetArray(CConditionalAnims::AT_REACTION)), conditionData))
		{
			return true;
		}
	}
	//no valid reaction clip was found
	return false;
}

void CTaskAmbientClips::OverrideBaseClip(fwMvClipSetId clipSetId, fwMvClipId clipId)
{
	m_OverriddenBaseClipSet = clipSetId;
	m_OverriddenBaseClip = clipId;
	m_Flags.SetFlag(Flag_OverriddenBaseAnim);
}

void CTaskAmbientClips::PlayAnimAsIdle(fwMvClipSetId clipSetId, fwMvClipId clipId)
{
	m_ClipSetId = clipSetId;
	m_ClipId = clipId;
	m_Flags.SetFlag(Flag_IdleClipIdsOverridden);
}

bool CTaskAmbientClips::IsInPhoneConversation() const
{
	if (m_Flags.IsFlagSet(Flag_IsInConversation) && m_pConversationHelper)
	{
		return m_pConversationHelper->IsPhoneConversation(); 
	}
	return false;
}

bool CTaskAmbientClips::IsInHangoutConversation() const
{
	if (m_Flags.IsFlagSet(Flag_IsInConversation))
	{
		// If m_pConversationHelper is NULL but we're in a conversation, we're the second party of a Hangout-style conversation. [1/22/2013 mdawe]
		return !m_pConversationHelper || m_pConversationHelper->IsHangoutConversation();
	}
	return false;
}

bool CTaskAmbientClips::IsInArgumentConversation() const
{
	if (m_Flags.IsFlagSet(Flag_IsInConversation))
	{
		// If m_pConversationHelper is NULL but we're in a conversation, we're the second party of a Hangout-style conversation. This includes Arguments; safer to include them here. [1/22/2013 mdawe]
		return !m_pConversationHelper || m_pConversationHelper->IsArgumentConversation();
	}
	return false;
}


void CTaskAmbientClips::SetInConversation(bool bOnOff)
{	
	if (bOnOff)
	{
		m_Flags.SetFlag(Flag_IsInConversation);
	}
	else 
	{
		m_Flags.ClearFlag(Flag_IsInConversation);
	}
}

#if SCENARIO_DEBUG
void CTaskAmbientClips::SetOverrideAnimData(const ScenarioAnimDebugInfo& info)
{
	m_DebugInfo = info;
	m_Flags.SetFlag(Flag_UseDebugAnimData);
	m_Flags.ClearFlag(Flag_WantNextDebugAnimData);
}

void CTaskAmbientClips::ResetConditionalAnimGroup()
{
	m_pConditionalAnimsGroup = NULL;
	m_ConditionalAnimChosen = -1;
}
#endif // SCENARIO_DEBUG

bool CTaskAmbientClips::TerminateIfChosenConditionalAnimConditionsAreNotMet()
{
	if (m_ConditionalAnimChosen > -1)
	{
		if (aiVerify(m_ConditionalAnimChosen < m_pConditionalAnimsGroup->GetNumAnims()))
		{
			CScenarioCondition::sScenarioConditionData conditionData;
			InitConditionData(conditionData);
			const CConditionalAnims * pClips = m_pConditionalAnimsGroup->GetAnims(m_ConditionalAnimChosen);
			if (!pClips->CheckConditions(conditionData,NULL))
			{
				return RequestTermination();
			}
		}
	}
	return false;
}

void CTaskAmbientClips::TerminateIfBaseConditionsAreNotMet() 
{
	CScenarioCondition::sScenarioConditionData conditionData;
	InitConditionData(conditionData);
	if (!CheckConditionsOnPlayingBaseClip(conditionData))
	{
		RequestTermination();
	}
}

#if !__FINAL
void CTaskAmbientClips::Debug() const
{
#if DEBUG_DRAW
	static bool bDrawConversationLine = false;
	if (bDrawConversationLine && m_pConversationHelper)
	{
		const CPed* pPlayerPed = FindPlayerPed();
		const CPed* pPed = GetPed();
		if(!pPed || !pPlayerPed)
		{
			return;
		}

		Vec3V pos1 = pPed->GetTransform().GetPosition();
		Vec3V pos2 = pPlayerPed->GetTransform().GetPosition();
		pos1.SetZf(pos1.GetZf() + 0.6f);
		grcDebugDraw::Line(pos1, pos2, Color_AliceBlue);
	}
#endif // DEBUG_DRAW
}
#endif // !__FINAL

void CTaskAmbientClips::UpdateAllAnimationVFX()
{
	bool playIdle = false;
	bool playBase = false;

	if (!GetPed()->IsVisible())
	{
		return;
	}

	// If there is a flag to trigger vfx events, trigger them when its hit
	if(m_VfxTagsExistIdle && GetState() == State_PlayIdleAnimation)
	{
		playIdle = true;
	}

	if(m_VfxTagsExistBase && m_BaseClip.GetClipHelper() && GetClipHelper() == NULL)
	{
		playBase = true;
	}

	if((playIdle || playBase) && m_pConditionalAnimsGroup && m_ConditionalAnimChosen >= 0)
	{
		const CConditionalAnims* chosen = m_pConditionalAnimsGroup->GetAnims(m_ConditionalAnimChosen);
		if(chosen)
		{
			CPed* pPed = GetPed();

			float range = chosen->GetVFXCullRange();
			if(m_VfxInRange)
			{
				range *= 1.1f;	// MAGIC! A little extra, for hysteresis.
			}
			if(!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen))
			{
				range *= sm_Tunables.m_VFXCullRangeScaleNotVisible;
			}

			const Vec3V camPosV = RCC_VEC3V(camInterface::GetPos());
			const Vec3V pedPosV = pPed->GetTransform().GetPosition();

			const ScalarV distSqV = DistSquared(camPosV, pedPosV);
			const ScalarV thresholdDistV(range);
			const ScalarV thresholdDistSqV = Scale(thresholdDistV, thresholdDistV);

			bool inRange = (IsLessThanAll(distSqV, thresholdDistSqV) != false);
			m_VfxInRange = inRange;
			if(!inRange)
			{
				// Not in range, don't spend more time on VFX until we get closer.
				return;
			}
		}
	}
	else
	{
		m_VfxInRange = false;
	}

	// If there is a flag to trigger vfx events, trigger them when its hit
	if(playIdle)
	{
		CClipHelper* pHelper = GetClipHelper();
		if(pHelper)
		{
			UpdateAnimationVFX(pHelper);
		}
	}

	// Check for clip driven vfx on the base clip, if no higher priority clip is playing
	if(playBase)
	{
		UpdateAnimationVFX(m_BaseClip.GetClipHelper());
	}
}


void CTaskAmbientClips::UpdateAnimationVFX(const CClipHelper* clipHelper)
{
	PF_FUNC(UpdateAnimationVFX);

	taskFatalAssertf(clipHelper, "Null clip passed, expected to be valid in UpdateAnimationVFX");

	bool regi = false, trig = false;
	
	// Cache the ped
	CPed* pPed = GetPed();
	CPedWeaponManager* pWeapMgr = pPed->GetWeaponManager();

	// If there is info to be had here... 
	bool gotAnims = (m_pConditionalAnimsGroup && m_ConditionalAnimChosen >= 0);

	if(pWeapMgr || gotAnims)
	{
		regi = clipHelper->IsEvent<crPropertyAttributeBool>(CClipEventTags::ObjectVfx, CClipEventTags::Register, true);
		trig = clipHelper->IsEvent<crPropertyAttributeBool>(CClipEventTags::ObjectVfx, CClipEventTags::Trigger, true);
	}

	if(pWeapMgr)
	{
		// If there is a flag to trigger events, trigger them when its hit
		// e.g. hammer hitting the ground etc...
		if( trig &&
			pWeapMgr->GetEquippedWeaponObject() )
		{
			pWeapMgr->GetEquippedWeaponObject()->ProcessFxAnimTriggered();		
		}

		// If there is a flag to register events, register them when its hit
		// e.g. hammer hitting the ground etc...
		if( regi &&
			pWeapMgr->GetEquippedWeaponObject() )
		{
			pWeapMgr->GetEquippedWeaponObject()->ProcessFxAnimRegistered();		
		}
	}

	if (gotAnims)
	{
		const CConditionalAnims* chosen = m_pConditionalAnimsGroup->GetAnims(m_ConditionalAnimChosen);

		if (chosen && (regi || trig))
		{
// 			if(!m_VfxRequested)
// 			{
// 				//Make sure we have some VFX data ... 
// 				if (chosen->GetVFXCount())
// 				{
// 					const CConditionalClipSetVFX& vfx = chosen->GetVFX(0);
// 					atHashWithStringNotFinal ptFxAssetHashName = g_vfxEntity.GetPtFxAssetName(PT_ANIM_FX, vfx.m_fxName);
// 					const int slot = CConditionalAnimStreamingVfxManager::GetInstance().RequestVfxAssetSlot(ptFxAssetHashName.GetHash());
// 					ptfxAssertf(slot>-1, "cannot find particle asset of type (%d) named (%s) with slot name (%s) in any loaded rpf", PT_ANIM_FX, vfx.m_fxName.GetCStr(), ptFxAssetHashName.GetCStr());
// 					Assert(slot <= 0x7fff);
// 					m_VfxStreamingSlot = (s16)slot;
// 					m_VfxRequested = true;
// 				}
// 			}

//			const strLocalIndex slot = m_VfxStreamingSlot;
//			if (slot.Get()>-1 && ptfxManager::GetAssetStore().HasObjectLoaded(slot))
			{
				aiAssert(clipHelper->GetClip());
				const crClip* pClip = clipHelper->GetClip();

				if (pClip->GetTags())
				{
					crTagIterator it(*pClip->GetTags(),clipHelper->GetLastPhase(), clipHelper->GetPhase(), CClipEventTags::ObjectVfx);
					while (*it)
					{
						const crTag* pTag = *it;			
						const crPropertyAttribute* attrib = pTag->GetProperty().GetAttribute(CClipEventTags::VFXName);
						const crPropertyAttribute* aTri = pTag->GetProperty().GetAttribute(CClipEventTags::Trigger);
						bool isTriggered = (
							aTri && 
							aiVerifyf(aTri->GetType() == crPropertyAttribute::kTypeBool, "Clip [%s] has a tag attribute %s that is not of the expected type [BOOL].", pClip->GetName(), CClipEventTags::Trigger.GetCStr()) && 
							((const crPropertyAttributeBool*)aTri)->GetBool()
							) ? true : false;

						if (attrib && aiVerifyf(attrib->GetType() == crPropertyAttribute::kTypeHashString, "Clip [%s] has a tag attribute %s that is not of the expected type [HashString].", pClip->GetName(), CClipEventTags::VFXName.GetCStr()))
						{
							const crPropertyAttributeHashString* hashAttrib = (const crPropertyAttributeHashString*)attrib;
							const CConditionalClipSetVFX* vfx = chosen->GetVFXByName(hashAttrib->GetHashString());
							if (aiVerifyf(vfx, "%s %s Failed to find vfx data (%s) to be used in conditional anim (%s) Clip name %s, m_VfxTagsExistIdle %s, m_VfxTagsExistBase %s. Task State %s. m_fTimeLastSearchedForBaseClip %.2f.",
								CTaskAmbientMigrateHelper::GetAssertNetObjPedName(this),
								pPed->IsNetworkClone()?"CLONE":"LOCAL",
								hashAttrib->GetHashString().GetCStr(),
								chosen->GetName(),
								clipHelper->GetClipSet().GetCStr(),
								m_VfxTagsExistIdle?"TRUE":"FALSE",
								m_VfxTagsExistBase?"TRUE":"FALSE",
								GetStateName(GetState()),
								m_fTimeLastSearchedForBaseClip))
							{
								UpdateConditionalAnimVFX(*vfx, *pPed, it.GetCurrentIndex(), isTriggered);
							}
						}
						++it;
					}
				}
			}
		}
	}
}

void CTaskAmbientClips::UpdateConditionalAnimVFX(const CConditionalClipSetVFX& vfxData, CPed& ped, int uniqueID, bool isTriggered)
{
	Quaternion tquat;
	CParticleAttr temp;
	temp.m_fxType = PT_ANIM_FX;
	temp.m_allowRubberBulletShotFx = false;
	temp.m_allowElectricBulletShotFx = false;
	temp.m_ignoreDamageModel = true;
	temp.m_playOnParent = false;
	temp.m_onlyOnDamageModel = false;
	temp.m_tagHashName = vfxData.m_fxName;
	temp.SetPos(vfxData.m_offsetPosition);
	temp.m_scale = vfxData.m_scale;
	temp.m_probability = vfxData.m_probability;
	temp.m_boneTag = vfxData.m_boneTag;
	temp.m_hasTint = false;

	if (vfxData.m_color.GetColor() != 0)
	{
		temp.m_hasTint = true;
		temp.m_tintR = vfxData.m_color.GetRed();
		temp.m_tintG = vfxData.m_color.GetGreen();
		temp.m_tintB = vfxData.m_color.GetBlue();
		temp.m_tintA = vfxData.m_color.GetAlpha();
	}

	tquat.FromEulers(vfxData.m_eulerRotation);
	temp.qX = tquat.x;
	temp.qY = tquat.y;
	temp.qZ = tquat.z;
	temp.qW = tquat.w;

#if __ASSERT
	const CConditionalAnims* chosen = m_pConditionalAnimsGroup->GetAnims(m_ConditionalAnimChosen);
	Assert(chosen);
#endif

	if (isTriggered)
	{
		vfxAssertf(!g_vfxEntity.GetIsInfinitelyLooped(temp), "VFX {%s} referenced by GROUP {%s} ANIMS {%s} cant be used as a triggered effect.", vfxData.m_fxName.GetCStr(), m_pConditionalAnimsGroup->GetName(), chosen->GetName());
		g_vfxEntity.TriggerPtFxEntityAnim(&ped, &temp);
	}
	else
	{
		vfxAssertf(g_vfxEntity.GetIsInfinitelyLooped(temp), "VFX {%s} referenced by GROUP {%s} ANIMS {%s} cant be used as a registered effect.", vfxData.m_fxName.GetCStr(), m_pConditionalAnimsGroup->GetName(), chosen->GetName());
		g_vfxEntity.UpdatePtFxEntityAnim(&ped, &temp, uniqueID);
	}
}

void CTaskAmbientClips::UpdateLoopingFlag()
{
}

void CTaskAmbientClips::ScanClipForPhoneRingRequests()
{
	if (aiVerify(m_pConversationHelper && GetClipHelper()))
	{
		if (GetClipHelper()->IsEvent(CClipEventTags::PhoneRing))
		{
			m_pConversationHelper->SetRingToneFlag();
		}
		else
		{
			m_pConversationHelper->ClearRingToneFlag();
		}
	}
}

void CTaskAmbientClips::InitIdleTimer()
{
	// If the idle time has been explicitly set, use that as the initial idle timer
	if( !m_Flags.IsFlagSet(Flag_OverrideInitialIdleTime) )
	{
		m_fNextIdleTimer = fwRandom::GetRandomNumberInRange(0.75f, 1.25f) * GetNextIdleTime();
	}

	m_Flags.ClearFlag(Flag_OverrideInitialIdleTime);

#if SCENARIO_DEBUG
	if (m_Flags.IsFlagSet(Flag_UseDebugAnimData))
	{
		m_fNextIdleTimer = m_DebugInfo.m_BasePlayTime;
	}
#endif
}


bool CTaskAmbientClips::CheckClipHasVFXTags(const crClip& clip)
{
	PF_FUNC(UpdateAnimationVFX);	// Probably similar enough that we don't need a separate timer.

	// Check if the clip contains any Trigger or Register ObjectVfx tags. If not, UpdateAnimationVFX() wouldn't do
	// anything when playing this clip, so we don't have to call it.
	if(CClipEventTags::FindFirstEventTag<crTag, crPropertyAttributeBool>(&clip, CClipEventTags::ObjectVfx, CClipEventTags::Register, true))
	{
		return true;
	}
	else if(CClipEventTags::FindFirstEventTag<crTag, crPropertyAttributeBool>(&clip, CClipEventTags::ObjectVfx, CClipEventTags::Trigger, true))
	{
		return true;
	}
	return false;
}


bool CTaskAmbientClips::CheckClipHasPropTags(const crClip& clip)
{
	// Check if the clip contains any Object Create/Destroy/Release/Flashlight tags. If not, CPropManagementHelper::ProcessMoveSignals() wouldn't do
	// anything when playing this clip, so we don't have to call it.
	if(CClipEventTags::FindFirstEventTag<crTag, crPropertyAttributeBool>(&clip, CClipEventTags::Object, CClipEventTags::Create, true))
	{
		return true;
	}
	else if(CClipEventTags::FindFirstEventTag<crTag, crPropertyAttributeBool>(&clip, CClipEventTags::Object, CClipEventTags::Destroy, true))
	{
		return true;
	}
	else if(CClipEventTags::FindFirstEventTag<crTag, crPropertyAttributeBool>(&clip, CClipEventTags::Object, CClipEventTags::Release, true))
	{
		return true;
	}
	else if (CClipEventTags::FindFirstEventTag<crTag, crPropertyAttributeBool>(&clip, CClipEventTags::FlashLight, CClipEventTags::FlashLightOn))
	{
		return true;
	}
	return false;
}


bool CTaskAmbientClips::ParseBlendOutTagsHelper(const crClip& clip, float& startPhaseOut, float& endPhaseOut)
{
	static const u32 moveEventHash = ATSTRINGHASH("BLEND_OUT_RANGE", 0x7183336);
	return CClipEventTags::GetMoveEventStartAndEndPhases(&clip, moveEventHash, startPhaseOut, endPhaseOut);
}


void CTaskAmbientClips::ParseBlendOutTags(const crClip& clip)
{
	float blendOutStartPhase = 1.0f, blendOutEndPhase = 1.0f;

	// Check for BLEND_OUT_RANGE tags first.
	if(ParseBlendOutTagsHelper(clip, blendOutStartPhase, blendOutEndPhase))
	{
		m_fBlendOutPhase = blendOutStartPhase;
		m_BlendOutRangeTagsFound = true;
		return;
	}

	// Check for Interruptible tags.
	if(CClipEventTags::FindEventPhase(&clip, CClipEventTags::Interruptible, blendOutStartPhase))
	{
		m_fBlendOutPhase = blendOutStartPhase;
		m_BlendOutRangeTagsFound = false;
		return;
	}

	// Didn't find anything, make sure to clear out any previous
	// information we may have stored.
	ResetBlendOutPhase();
}

bool CTaskAmbientClips::ShouldHangoutBreakup() const
{ 
	if (CScenarioManager::AreScenarioBreakoutExitsSuppressed()) 
	{
		return false;
	}
	else
	{
		return m_Flags.IsFlagSet(Flag_PlayerNearbyTimerSet) && m_fPlayerNearbyTimer < 0.f; 
	}
}
	

void CTaskAmbientClips::PlayConversationIfApplicable()
{
	taskAssert(m_Flags.IsFlagSet(Flag_AllowsConversation)); // We shouldn't be calling this on peds in scenarios without the AllowsConversation . [8/7/2012 mdawe]

	CPed* pPlayerPed = FindPlayerPed();
	CPed* pPed = GetPed();
	if (pPlayerPed && pPed)
	{
		// Calculate if the player is "near" this hangout conversation. [8/7/2012 mdawe]
		m_fPlayerConversationScenarioDistSq  = DistSquared(pPlayerPed->GetTransform().GetPosition(), pPed->GetTransform().GetPosition()).Getf();
		
		// If you're allowed to have an argument and the player is within the start-argument-distance
		if (m_bCanHaveArgument && m_fPlayerConversationScenarioDistSq >= sm_Tunables.m_fArgumentDistanceMinSq && m_fPlayerConversationScenarioDistSq <= sm_Tunables.m_fArgumentDistanceMaxSq)
		{
			ProcessArgumentConversation();
		}
		// otherwise, is the player within the normal conversation distance?
		else if (m_fPlayerConversationScenarioDistSq < sm_Tunables.m_playerNearToHangoutDistanceInMetersSquared)
		{
			ProcessNormalConversation(pPlayerPed);
		}
		// Player is no longer nearby. [11/30/2012 mdawe]
		else if (m_Flags.IsFlagSet(Flag_PlayerNearbyTimerSet))
		{
			m_Flags.ClearFlag(Flag_PlayerNearbyTimerSet);
			m_fPlayerNearbyTimer = -1.f;
		}
	}
}

void CTaskAmbientClips::ProcessArgumentConversation()
{
	if (!IsInConversation())
	{
		StartHangoutArgumentConversation();
	}
}

void CTaskAmbientClips::ProcessNormalConversation(CPed* pPlayerPed)
{
	if (!m_Flags.IsFlagSet(Flag_PlayerNearbyTimerSet))
	{
		// set our timer flag here so that we do not keep calling StartHangoutConversation
		//  when no valid Peds are available to talk to.
		m_Flags.SetFlag(Flag_PlayerNearbyTimerSet);

		// Play a chat line if you can. [8/9/2012 mdawe]
		if (StartHangoutConversation())
		{
			m_fPlayerNearbyTimer = fwRandom::GetRandomNumberInRange(sm_Tunables.m_minSecondsNearPlayerUntilHangoutQuit, sm_Tunables.m_maxSecondsNearPlayerUntilHangoutQuit);

			// Check to see if we're within a second of the last ped to set this timer. [10/11/2012 mdawe]
			CPlayerInfo *pPlayerInfo = pPlayerPed->GetPlayerInfo();
			if (pPlayerInfo)
			{
				const float fOneSecond = 1.0f;
				const float fLastNearbyTimer = pPlayerInfo->GetLastPlayerNearbyTimer();
				if ( m_fPlayerNearbyTimer > fLastNearbyTimer - fOneSecond &&
					m_fPlayerNearbyTimer < fLastNearbyTimer + fOneSecond )
				{
					// We were within a second of the last ped to randomly pick this time.
					//  Wait a little longer in case we're nearby, so peds aren't walking off 
					//  simultaneously. [10/11/2012 mdawe]
					m_fPlayerNearbyTimer += fOneSecond;
				}

				pPlayerInfo->SetLastPlayerNearbyTimer(m_fPlayerNearbyTimer);
			}
		}
		// otherwise...
		else
		{
			// We don't have a valid Ped nearby, so we cannot have a conversation, so
			//  set the local nearby timer to let the breakup happen when acceptable. 
			m_fPlayerNearbyTimer = -1.0f;
		}
	}
	// Otherwise, run the timer. [8/7/2012 mdawe]
	if (m_fPlayerNearbyTimer > 0.f)
	{
		m_fPlayerNearbyTimer -= fwTimer::GetTimeStep();
	}
	else 
	{
		// Timer ran out and player is still nearby. [11/30/2012 mdawe]
		if (m_Flags.IsFlagSet(Flag_InBreakoutScenario) && ShouldHangoutBreakup())
		{
			BreakupHangoutScenario();
		}
		else
		{
			m_Flags.ClearFlag(Flag_PlayerNearbyTimerSet);
			m_fPlayerNearbyTimer = -1.f;
		}
	}
}

void CTaskAmbientClips::BreakupHangoutScenario()
{
	taskAssert(m_Flags.IsFlagSet(Flag_InBreakoutScenario)); // We shouldn't be calling this on peds not in breakout scenarios.

	// If we can't say a chat line, leave or do something more interesting than just hanging out [8/9/2012 mdawe]
	//That will be handled in CTaskUseScenario::StartNewScenarioAfterHangout().
	// Let the parent UseScenario task know we want to leave. [8/7/2012 mdawe]
	// check if the base/idle is done and if so allow the ped to exit.
	if  (!GetPed()->PopTypeIsMission() && IsIdleFinished() &&
		(GetBaseClip() && (GetBaseClip()->GetPhase() + GetBaseClip()->GetPhaseUpdateAmount() * 2.0f > 1.0f)))
	{
		CTask* pParentTask = GetParent();
		if (pParentTask && pParentTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO)
		{
			CTaskUseScenario* pParentScenarioTask = static_cast<CTaskUseScenario*>(pParentTask);
			pParentScenarioTask->SetExitingHangoutConversation();
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
//Network clone to local, local to clone, migrate helper functions
////////////////////////////////////////////////////////////////////////////////
bool  CTaskAmbientClips::MigrateProcessPreFSM()
{

	return false;
}

bool CTaskAmbientClips::MigrateUpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	Assertf(NetworkInterface::IsGameInProgress(),"Only expect to call MigrateStart in network games!");

	CTaskAmbientMigrateHelper* pTAMH = GetTaskAmbientMigrateHelper();
	if(	pTAMH ) 
	{
		if( pTAMH->GetHasMigrateData() )
		{
			if(iEvent==OnUpdate)
			{
				if(iState == State_Start )
				{
					return MigrateStart_OnUpdate();
				}
				else if(iState == State_PlayIdleAnimation )
				{
					return MigratePlayingIdleAnimation_OnUpdate();
				}
#if __ASSERT
				else
				{
					if(iState == State_NoClipPlaying && m_Flags.IsFlagSet(Flag_PlayBaseClip) && pTAMH->GetHasIdleClipMigrateData())
					{ 
						//B*2077187 When playing in car music head bob anims this situation can arise when a base anim is forced with Flag_PlayBaseClip
						//but the idle anim was still valid at migrate, just clear the held over idle anim migrate data to ignore it here.
						pTAMH->ClearHasIdleClipMigrateData();
					}
					else
					{
						Assertf(0, "%s Don't expect this OnUpdate state %d!",CTaskAmbientMigrateHelper::GetAssertNetObjPedName(this),iState);
					}
				}
#endif
			}
			else if(iEvent==OnEnter)
			{
				if(iState == State_NoClipPlaying)
				{
					return MigrateNoClipPlaying_OnEnter();
				}
				else if(iState == State_PlayIdleAnimation)
				{
					return MigratePlayingIdleAnimation_OnEnter();
				}
#if __ASSERT
				else
				{
					Assertf(iState == State_Start, "%s Don't expect this OnEnter state %d!",CTaskAmbientMigrateHelper::GetAssertNetObjPedName(this),iState);
				}
#endif
			}
		}
		else
		{
			//finished with migrate helper so make sure it is deleted to free up reference to prop helper
			CPed* pPed = GetPed();

			CNetObjPed     *netObjPed  = pPed?static_cast<CNetObjPed *>(pPed->GetNetworkObject()):NULL;

			if(taskVerifyf(netObjPed, "Expect valid netObjPed here") )
			{
				netObjPed->ClearMigrateTaskHelper();
			}
		}
	}

	return false;
}

bool CTaskAmbientClips::MigrateStart_OnUpdate()
{
	Assertf(NetworkInterface::IsGameInProgress(),"Only expect to ucall MigrateStart in network games!");

	CTaskAmbientMigrateHelper* pTAMH = GetTaskAmbientMigrateHelper();
	if(	pTAMH && 
		pTAMH->GetHasMigrateData() &&
		pTAMH->ApplyClipMigrateDataToTaskAmbient(this))
	{
		switch(pTAMH->m_State)
		{
		case State_NoClipPlaying:
			SetState(pTAMH->m_State);
			return true;
			break;
		case State_SyncFoot:
		case State_StreamIdleAnimation:
			{
				//if was streaming then reset this and go back to State_NoClipPlaying
				m_AmbientClipRequestHelper.ReleaseClips();
				// If we're leaving this state and we are not going to play the idle, clear
				// the condition pointer.
				m_IdleAnimImmediateCondition = NULL;
				SetState(State_NoClipPlaying);
				return true;
			}
			break;
		case State_PlayIdleAnimation:
			SetState(pTAMH->m_State);
			return true;
			break;
		default:
			Assertf(0, "%s Don't expect this state at migrate %d!",CTaskAmbientMigrateHelper::GetAssertNetObjPedName(this),pTAMH->m_State);
			pTAMH->Reset();
			break;
		}
	}

	return false;
}

bool CTaskAmbientClips::MigrateNoClipPlaying_OnEnter()
{
	Assertf(NetworkInterface::IsGameInProgress(),"Only expect to ucall MigrateNoClipPlaying_OnEnter in network games!");
	
	CTaskAmbientMigrateHelper* pTAMH = GetTaskAmbientMigrateHelper();
	if(	pTAMH && pTAMH->GetHasMigrateData() )
	{
		if(pTAMH->GetHasBaseClipMigrateData())
		{
			pTAMH->ApplyMigrateBaseClip(this);
			pTAMH->Reset();			
		}

		return true;
	}

	return false;
}

bool CTaskAmbientClips::MigratePlayingIdleAnimation_OnEnter()
{
	InitIdleTimer();

	Assertf(NetworkInterface::IsGameInProgress(),"Only expect to call MigratePlayingIdleAnimation_OnEnter in network games!");
	CTaskAmbientMigrateHelper* pTAMH = GetTaskAmbientMigrateHelper();
	if(	pTAMH && pTAMH->GetHasMigrateData() )
	{
		if(pTAMH->GetHasIdleClipMigrateData())
		{
			pTAMH->ApplyMigrateIdleClip(this);
		}
		return true;
	}

	return false;
}

bool  CTaskAmbientClips::MigratePlayingIdleAnimation_OnUpdate()
{
	Assertf(NetworkInterface::IsGameInProgress(),"Only expect to call MigratePlayingIdleAnimation_OnUpdate in network games!");
	CTaskAmbientMigrateHelper* pTAMH = GetTaskAmbientMigrateHelper();
	if(	pTAMH && pTAMH->GetHasMigrateData() )
	{
		if(pTAMH->GetHasBaseClipMigrateData())
		{
			Assertf(pTAMH->GetHasIdleClipMigrateData()!=true, "%s Don't expect idle migrate data",CTaskAmbientMigrateHelper::GetAssertNetObjPedName(this));
			pTAMH->ApplyMigrateBaseClip(this, true);
		}
		return true;
	}

	return false;
}

CTaskAmbientMigrateHelper * CTaskAmbientClips::GetTaskAmbientMigrateHelper()
{
	Assertf(NetworkInterface::IsGameInProgress(),"Only expect to call MigratePlayingIdleAnimation_OnEnter in network games!");

	CPed* pPed = GetPed();

	CNetObjPed     *netObjPed  = pPed?static_cast<CNetObjPed *>(pPed->GetNetworkObject()):NULL;

	if(netObjPed)
	{
		CTaskAmbientMigrateHelper* pMigrateTaskHelper = netObjPed->GetMigrateTaskHelper();

		if(pMigrateTaskHelper)
		{
			return pMigrateTaskHelper;
		}
	}

	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

// Clone task implementation for CTaskAmbientClips

////////////////////////////////////////////////////////////////////////////////
// Clone FSM function implementations




////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskAmbientClips::UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent)
{
	if(GetStateFromNetwork() == State_Exit)
	{
		return FSM_Quit;
	}

	return UpdateFSM(iState, iEvent);
}

////////////////////////////////////////////////////////////////////////////////
void CTaskAmbientClips::OnCloneTaskNoLongerRunningOnOwner()
{
	if(ParentTaskScenarioIsTransitioningToLeaving())
	{
		return;
	}

	SetStateFromNetwork(State_Exit);
}

// ParentTaskScenarioIsTransitioningToLeaving checks the parent current running CTaskUseScenario state against 
// the latest from the remote CTaskUseScenario. If the remote is on State_Exit and locally it is on State_PlayAmbients 
// we know that LeaveScenario is about to be called on the parent CTaskUseScenario. 
// LeaveScenario starts the State_Exit locally and needs this TaskAmbient to exist to ensure that animations blend out properly.
bool CTaskAmbientClips::ParentTaskScenarioIsTransitioningToLeaving()
{
	CTask* pParentTask = GetParent();
	if (pParentTask && pParentTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO && taskVerifyf(GetPed(),"Expect valid task ped"))
	{   
		CTaskUseScenario* pParentScenarioTask = static_cast<CTaskUseScenario*>(pParentTask);
		CTaskInfo* pTaskInfo = GetPed()->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_USE_SCENARIO, PED_TASK_PRIORITY_MAX, false);

		if (pTaskInfo)
		{
			if( pParentScenarioTask->GetState()==CTaskUseScenario::State_PlayAmbients &&
				pTaskInfo->GetState()==CTaskUseScenario::State_Exit )
			{
				return true;
			}
		}
	}

	return false;
}


bool CTaskAmbientClips::ControlPassingAllowed(CPed *pPed, const netPlayer& UNUSED_PARAM(player), eMigrationType UNUSED_PARAM(migrationType))
{
	{
		CTaskAmbientMigrateHelper* pTAMH = GetTaskAmbientMigrateHelper();
		if(	pTAMH && pTAMH->MigrateInProgress() )
		{
			LOGGING_ONLY(formatf(m_LastTaskMigrateFailReason, "CTaskAmbientMigrateHelper::MigrateInProgress");)
			return false;
		}

		if( !IsInAllowedStateForMigrate() && (!pPed->IsNetworkClone() || m_cloneTaskInScope) )
		{
			LOGGING_ONLY(formatf(m_LastTaskMigrateFailReason, "IsInAllowedStateForMigrate: %s", IsInAllowedStateForMigrate()?"TRUE":"FALSE");)
			return false;
		}

		if(pPed && pPed->GetPedIntelligence())
		{
			//Check if the ped is hurrying away and out of range to migrate.
			CTaskShockingEventHurryAway* pTaskShockingEventHurryAway = 
				static_cast<CTaskShockingEventHurryAway *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SHOCKING_EVENT_HURRYAWAY));
			if(pTaskShockingEventHurryAway)
			{
				if(pTaskShockingEventHurryAway->IsHurryingAway() && !pTaskShockingEventHurryAway->IsPedfarEnoughToMigrate())
				{
					LOGGING_ONLY(formatf(m_LastTaskMigrateFailReason, "IsHurryingAway && !IsPedfarEnoughToMigrate");)
					return false;
				}
			}

			if(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SHOCKING_NICE_CAR_PICTURE) != NULL)
			{
				LOGGING_ONLY(formatf(m_LastTaskMigrateFailReason, "Has TASK_SHOCKING_NICE_CAR_PICTURE");)
				return false;
			}
		}
	}

	return true;
}

//CTaskFSMClone *CTaskAmbientClips::CreateTaskForClonePed(CPed *)
//{
//	CTaskAmbientClips* pTask = NULL;
//
//	pTask = static_cast<CTaskAmbientClips*>(Copy());
//
//	return pTask;
//}
//
//CTaskFSMClone *CTaskAmbientClips::CreateTaskForLocalPed(CPed *pPed)
//{
//	// do the same as clone ped
//	return CreateTaskForClonePed(pPed);
//}


void CTaskAmbientClips::FindAndSetBestConditionalAnimsGroupForProp()
{
	aiAssert(m_pConditionalAnimsGroup);
	aiAssert(m_pConditionalAnimsGroup->GetNumAnims() == 0); //Should only be calling this if there are no specified animations in the constuctor-assigned anim group, e.g, "Walk."
	aiAssert(GetPed()); // Can't call this before a ped is set up.

	// Must have a prop to find a more appropriate anim group for a prop.
	if (!m_pPropHelper || !m_pPropHelper->GetPropNameHash())
	{
		return;
	}

	// Start by getting the model info for the prop in hand.
	const CPed* pPed = GetPed();
	strLocalIndex uHeldProp = strLocalIndex(CScenarioConditionHasProp::GetPropModelIndex(pPed));
	if(CModelInfo::IsValidModelInfo(uHeldProp.Get()))
	{
		CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(uHeldProp));
		aiAssert(pModelInfo);

		// Look for a better anim from the Wander group
		const CConditionalAnimsGroup* const pWanderGroup = CONDITIONALANIMSMGR.GetConditionalAnimsGroup("WANDER");
		if (pWanderGroup)
		{
			for (u32 uAnimGroupIndex = 0; uAnimGroupIndex < pWanderGroup->GetNumAnims(); ++uAnimGroupIndex)
			{
				// Does this Wander anim have a specified propset?
				s32 iPropSetIndex = CAmbientModelSetManager::GetInstance().GetModelSetIndexFromHash( CAmbientModelSetManager::kPropModelSets, pWanderGroup->GetAnims(uAnimGroupIndex)->GetPropSetHash() );
				if( iPropSetIndex >= 0 )
				{
					const CAmbientPropModelSet* pPropSet = CAmbientModelSetManager::GetInstance().GetPropSet(iPropSetIndex);
					if(pPropSet)
					{
						// Does the Wander anim propset contain the prop we're holding?
						if(pPropSet->GetContainsModel(pModelInfo->GetHashKey()))
						{							
							// We've found a better anim to use. Set it.
							m_pConditionalAnimsGroup = pWanderGroup;
							m_ConditionalAnimChosen = static_cast<s16>(uAnimGroupIndex);
							return;
						}
					}
				}
			}
		}
	}
}


CTaskInfo* CTaskAmbientClips::CreateQueriableState() const
{
	CTaskInfo* pInfo = NULL;

	u32	conditionalAnimGroupHash=0;

	if(m_pConditionalAnimsGroup)
	{
		conditionalAnimGroupHash = m_pConditionalAnimsGroup->GetHash();
	}

	s32 animChosen = m_CloneInitialConditionalAnimChosen;

	u32 localFlags = m_CloneInitialFlags.GetAllFlags();
	if (GetPed()->GetIsInVehicle())
	{
		localFlags |= Flag_IsInVehicle;
	}
	else
	{
		localFlags &=~Flag_IsInVehicle;
	}

#if __BANK
	localFlags &=~Flag_UseDebugAnimData;
	localFlags &=~Flag_WantNextDebugAnimData;
#endif

#if __ASSERT
	Assertf(!NetworkInterface::IsGameInProgress() || !( localFlags & (Flag_SynchronizedMaster|Flag_SynchronizedSlave|Flag_SlaveNotReadyForSyncedIdleAnim) ), "Don't expect synchronized flags to be sent to clone. Flags = %u.", localFlags);
#endif	

	pInfo =  rage_new CClonedTaskAmbientClipsInfo(	localFlags,
													conditionalAnimGroupHash,
													animChosen );

	return pInfo;
}

void CTaskAmbientClips::DoAbort(const AbortPriority UNUSED_PARAM(priority), const aiEvent* pEvent)
{
	if (pEvent)
	{
		CPed* pPed = GetPed();

		// Get the event type we're responding to.
		const int iEventType = pEvent->GetEventType();

		// We need to blend out in vehicle idles instantly if we're warping out of a seat
		if (iEventType == EVENT_NEW_TASK && pPed->GetIsInVehicle())
		{
			const aiTask* pNewTask = static_cast<const CEventNewTask*>(pEvent)->GetNewTask();
			if (pNewTask && pNewTask->GetTaskType() == CTaskTypes::TASK_EXIT_VEHICLE)
			{
				const CVehicle& rVeh = *pPed->GetMyVehicle();
				s32 iPedsSeatIndex = rVeh.GetSeatManager()->GetPedsSeatIndex(pPed);
				if (rVeh.IsSeatIndexValid(iPedsSeatIndex))
				{
					s32 iEntryPointIndex = rVeh.GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iPedsSeatIndex, NULL, SA_directAccessSeat, false, false);
					if (CTaskVehicleFSM::ShouldWarpInAndOutInMP(rVeh, iEntryPointIndex))
					{
						if (GetClipHelper())
						{
							GetClipHelper()->StopClip(INSTANT_BLEND_OUT_DELTA);
						}
						m_BaseClip.StopClip(INSTANT_BLEND_OUT_DELTA);
					}
				}
			}
		}

		// If we're in a phone conversation, check for special audio events [8/16/2012 mdawe]
		if (m_pConversationHelper && m_pConversationHelper->IsPhoneConversation())
		{
			// Initialize the audioContextString. If we're responding to an event, we'll set this appropriately.
			const char* audioContextString = NULL;

			// Is this an explosion event? [8/16/2012 mdawe]
			if (iEventType == EVENT_SHOCKING_EXPLOSION ||
				iEventType == EVENT_EXPLOSION ||
				iEventType == EVENT_EXPLOSION_HEARD)
			{
				audioContextString = "PHONE_SURPRISE_EXPLOSION";
			}
			// Is this a gunfire event? [8/16/2012 mdawe]
			else if (iEventType == EVENT_SHOCKING_GUNSHOT_FIRED || 
					 iEventType == EVENT_SHOT_FIRED				|| 
					 iEventType == EVENT_SHOT_FIRED_WHIZZED_BY	||
					 iEventType == EVENT_SHOT_FIRED_BULLET_IMPACT)
			{
				audioContextString = "PHONE_SURPRISE_GUNFIRE";
			}


			// If we have an audioContextString to say... [8/16/2012 mdawe]
			if (audioContextString != NULL)
			{
				//...and this ped can say it...
				audSpeechAudioEntity* pPedAudioEntity = pPed->GetSpeechAudioEntity();
				if (pPedAudioEntity && pPedAudioEntity->DoesContextExistForThisPed(audioContextString))
				{
					// Stop any existing line and say our new line while we exit. [8/16/2012 mdawe]
					pPedAudioEntity->StopSpeech();
					pPed->NewSay(audioContextString);
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
void CTaskAmbientClips::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	taskAssert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_AMBIENT_CLIPS);

	// Call the base implementation - syncs the state
	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

bool CTaskAmbientClips::CreateHangoutConversationHelper(CPed** pOutOtherPed)
{
	if (CanHangoutChat() && !IsInConversation() && aiVerify(pOutOtherPed))
	{
		int num_spaces = CTaskConversationHelper::GetPool()->GetNoOfFreeSpaces();
		if (num_spaces > 0) 
		{
			CPed* pPed = GetPed();
			//Do we have a nearby ped to talk to?
			CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
			if (entityList.GetCount() != 0)
			{
				for (CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext())
				{
					// check if we have a non-Player Ped nearby
					CPed* pNearbyPed = static_cast<CPed*>(pEnt);
					if (pNearbyPed && !pNearbyPed->IsAPlayerPed())
					{
						//Is the nearby ped running AmbientClipsTask?
						CTask* pNearbyRunningTask = pNearbyPed->GetPedIntelligence()->GetTaskActiveSimplest();
						if (pNearbyRunningTask && pNearbyRunningTask->GetTaskType() == CTaskTypes::TASK_AMBIENT_CLIPS)
						{
							CTaskAmbientClips* pNearbyPedAmbientTask = static_cast<CTaskAmbientClips*>(pNearbyRunningTask);
							//Can the nearby Ped chat, and is he not already in a conversation?
							if (pNearbyPedAmbientTask->CanHangoutChat() && !pNearbyPedAmbientTask->IsInConversation()) 
							{
								if (IsLessThanAll(DistSquared(pPed->GetTransform().GetPosition(), pNearbyPed->GetTransform().GetPosition()), ScalarV(sm_Tunables.m_maxHangoutChatDistSq)))
								{
									//We're clear! Start a conversation with this ped.
									m_pConversationHelper = rage_new CTaskConversationHelper();
									*pOutOtherPed = pNearbyPed;
									return true;
								}
							}
						}
					}
				}
			}
		}
	}
	return false;
}

bool CTaskAmbientClips::StartHangoutConversation()
{
	CPed* pOtherPed;
	if (CreateHangoutConversationHelper(&pOtherPed))
	{
		if (aiVerify(pOtherPed))
		{
			m_pConversationHelper->StartHangoutConversation(GetPed(), pOtherPed);
			return true;
		}
	}
	return false;
}

bool CTaskAmbientClips::StartHangoutArgumentConversation()
{
	CPed* pOtherPed;
	if (CreateHangoutConversationHelper(&pOtherPed))
	{
		if (aiVerify(pOtherPed))
		{
			m_pConversationHelper->StartHangoutArgumentConversation(GetPed(), pOtherPed);
			return true;
		}
	}
	return false;
}

bool CTaskAmbientClips::ShouldInsertLowriderArmNetwork(bool bReturnTrueIfNotUsingAltClipset) const 
{
	const CPed* pPed = GetPed();
	if (pPed)
	{
		CVehicle *pVehicle = pPed->GetVehiclePedInside();
		if (pVehicle && pVehicle->InheritsFromAutomobile())
		{
			CAutomobile *pAutomobile = static_cast<CAutomobile*>(pPed->GetVehiclePedInside());
			if (pAutomobile && pAutomobile->HasHydraulicSuspension())
			{
				// Only create this subtask if we need to do the left arm blending (ie window height < 1.0f).
				float fWindowHeight = pPed->GetVehiclePedInside()->GetVehicleModelInfo() ? pPed->GetVehiclePedInside()->GetVehicleModelInfo()->GetLowriderArmWindowHeight() : 1.0f;
				if (fWindowHeight < 1.0f)
				{
					// Check that we're actually using the alt clipset.
					CTaskMotionInAutomobile *pTaskMotionInAutomobile = static_cast<CTaskMotionInAutomobile*>(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_AUTOMOBILE));
					if (bReturnTrueIfNotUsingAltClipset || (pTaskMotionInAutomobile && pTaskMotionInAutomobile->IsUsingAlternateLowriderClipset()))
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}


bool CTaskAmbientClips::InsertLowriderArmNetworkCB(fwMoveNetworkPlayer* pPlayer, float fBlendDuration, u32)
{
	if (!pPlayer)
		return false;

	if (!m_LowriderArmMoveNetworkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkTaskAmbientLowriderArm))
		return false;
	
	CPed *pPed = GetPed();
	if (!pPed)
		return false;

#if __BANK
	const char* desiredBaseClipName = "NULL";
	const char* desiredClipName = "NULL";
#endif

	const crClip* pLowDoorClip = GetLowriderLowDoorClip(m_ClipSetId, false BANK_ONLY(, desiredClipName));
	if (!pLowDoorClip)
	{
		// No valid idle clip, perhaps we need to use the base clip.
		pLowDoorClip = GetLowriderLowDoorClip(m_BaseClip.GetClipSet(), true BANK_ONLY(, desiredBaseClipName));
	}

	if (taskVerifyf(pLowDoorClip, "CTaskAmbientClips::InsertLowriderArmNetworkCB: Invalid low door clip. Missing data? pPed: %s, Clone: %s. Vehicle: %s. m_BaseClip.GetClipSet: %s, DesiredBaseClipName: %s, m_ClipSetId: %s, DesiredClipName: %s.", GetPed()->GetDebugName(), pPed->IsNetworkClone() ? "CLONE" : "LOCAL", pPed->GetVehiclePedInside() ? pPed->GetVehiclePedInside()->GetDebugName() : "NULL", m_BaseClip.GetClipSet().GetCStr(), desiredBaseClipName, m_ClipSetId.GetCStr(), desiredClipName))
	{
		// Create subnetwork to allow us to merge on the left-arm blend.
		m_LowriderArmMoveNetworkHelper.CreateNetworkPlayer(GetPed(), CClipNetworkMoveInfo::ms_NetworkTaskAmbientLowriderArm);
		pPed->GetMovePed().SetTaskNetwork(m_LowriderArmMoveNetworkHelper.GetNetworkPlayer(), fBlendDuration);

		// Set and use the appropriate base or idle clip.
		SendLowriderArmClipMoveParams(fBlendDuration);
		
		float fWindowHeight = pPed->GetVehiclePedInside()->GetVehicleModelInfo() ? pPed->GetVehiclePedInside()->GetVehicleModelInfo()->GetLowriderArmWindowHeight() : 1.0f;
		m_LowriderArmMoveNetworkHelper.SetFloat(CTaskMotionInVehicle::ms_WindowHeight, fWindowHeight);

		fwMvNetworkId networkTaskAmbientClips("generictask",0x8d0e389d);
		m_LowriderArmMoveNetworkHelper.SetSubNetwork(pPlayer, networkTaskAmbientClips);

		return true;
	}

	return false;
} 

void CTaskAmbientClips::BlendInLowriderClipCB(CSimpleBlendHelper& UNUSED_PARAM(helper), float blendDelta, u32 UNUSED_PARAM(uFlags))
{
	if (!m_LowriderArmMoveNetworkHelper.IsNetworkActive())
		return;

	if (!m_LowriderArmMoveNetworkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkTaskAmbientLowriderArm))
		return;

	CPed *pPed = GetPed();
	if (!pPed)
		return;

	float fBlendDuration = fwAnimHelpers::CalcBlendDuration(blendDelta);
	SendLowriderArmClipMoveParams(fBlendDuration);
}

void CTaskAmbientClips::BlendOutLowriderClipCB(CSimpleBlendHelper& UNUSED_PARAM(helper), float blendDelta, u32 uFlags)
{
	if (!m_LowriderArmMoveNetworkHelper.IsNetworkActive())
		return;

	if (!m_LowriderArmMoveNetworkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkTaskAmbientLowriderArm))
		return;

	CPed *pPed = GetPed();
	if (!pPed)
		return;

	float fBlendDuration = fwAnimHelpers::CalcBlendDuration(blendDelta);
	SendLowriderArmClipMoveParams(fBlendDuration);
	
	// We should probably clear the network if we're blending out the clip.
	bool bShouldClearNetwork = true;
	if (GetState() == State_PlayIdleAnimation)
	{
		// ... Unless we're playing a base clip beneath the idle animation.
		if (m_Flags.IsFlagSet(Flag_PlayBaseClip) && m_BaseClip.GetClipId() != CLIP_ID_INVALID)
		{
			// Make sure this clip is actually valid first.
#if __BANK
			const char* baseClipName = "NULL";
#endif
			const crClip *pBaseClip = GetLowriderLowDoorClip(m_BaseClip.GetClipSet(), true BANK_ONLY(, baseClipName));
			if (pBaseClip)
			{
				bShouldClearNetwork = false;
			}
		}
	}

	if (bShouldClearNetwork)
	{
		ClearLowriderArmNetwork(fBlendDuration, uFlags);
	}
}

void CTaskAmbientClips::SendLowriderArmClipMoveParams(float fBlendDuration)
{
	// We're blending in/out a clip (ie starting the network, or blending between base/idle clips).
	// ...So we probably need to set up or change the arm clip accordingly.
	fwMvClipId lowDoorClipId("LowDoorClip", 0xe999e8aa);
	fwMvClipId lowDoorBaseClipId("LowDoorBaseClip", 0xa1a0cd5b);
	fwMvFlagId useIdleClipFlagId("UseIdleClip", 0x7bce67fe);
	fwMvFloatId clipBlendDurationId("BaseClipBlend", 0xf22e9dcb);

#if __BANK
	const char* clipName = "NULL";
#endif

	// Set both base/idle clips on network.
	const crClip* pLowDoorBaseClip = GetLowriderLowDoorClip(m_BaseClip.GetClipSet(), true BANK_ONLY(, clipName));
	if (pLowDoorBaseClip)
	{
		m_LowriderArmMoveNetworkHelper.SetClip(pLowDoorBaseClip, lowDoorBaseClipId);
	}

	const crClip* pLowDoorClip = GetLowriderLowDoorClip(m_ClipSetId, false BANK_ONLY(, clipName));
	if (pLowDoorClip)
	{
		m_LowriderArmMoveNetworkHelper.SetClip(pLowDoorClip, lowDoorClipId);
	}

	// Only use the base clip if we're flagged to do so and don't have a valid idle clip (this gets set in StreamingAnimation_OnUpdate and cleared in PlayingIdleAnimation_OnExit).
	if (pLowDoorBaseClip && m_Flags.IsFlagSet(Flag_PlayBaseClip) && m_ClipId == CLIP_ID_INVALID)
	{
		m_LowriderArmMoveNetworkHelper.SetFlag(false, useIdleClipFlagId);
	}
	else if (pLowDoorClip)
	{
		m_LowriderArmMoveNetworkHelper.SetFlag(true, useIdleClipFlagId);
	}

	m_LowriderArmMoveNetworkHelper.SetFloat(clipBlendDurationId, fBlendDuration);
}

const crClip* CTaskAmbientClips::GetLowriderLowDoorClip(fwMvClipSetId clipSetId, bool bIsBaseClip BANK_ONLY(,const char* &desiredClipName)) const
{
	const crClip* pLowDoorClip = NULL; 

	if (clipSetId != CLIP_SET_ID_INVALID)
	{
		const char* lowDoorClipName = bIsBaseClip ? "BASE_LOWDOOR" : "IDLE_LOWDOOR";
#if __BANK
		desiredClipName = lowDoorClipName;
#endif
		atHashString lowDoorClipNameHashString(lowDoorClipName);
		int iDictIndex = fwClipSetManager::GetClipDictionaryIndex(clipSetId);
		pLowDoorClip = fwAnimManager::GetClipIfExistsByDictIndex(iDictIndex, lowDoorClipNameHashString.GetHash());
	}

	return pLowDoorClip;
}


void CTaskAmbientClips::ClearLowriderArmNetwork(const float fDuration, u32 uFlags)
{
	if (GetPed())
	{
		GetPed()->GetMovePed().ClearTaskNetwork(m_LowriderArmMoveNetworkHelper, fDuration, uFlags);
	}
}

//-------------------------------------------------------------------------
// Task info for Task Ambient Clip Info
//-------------------------------------------------------------------------

//-------------------------------------------------------------------------
// Constructor
//-------------------------------------------------------------------------
CClonedTaskAmbientClipsInfo::CClonedTaskAmbientClipsInfo( u32 flags, u32 conditionalAnimGroupHash, s32 conditionalAnimChosen )  
: m_conditionalAnimGroupHash(conditionalAnimGroupHash)
, m_Flags(flags)
, m_ConditionalAnimChosen(conditionalAnimChosen)
{
}

CClonedTaskAmbientClipsInfo::CClonedTaskAmbientClipsInfo() 
: m_conditionalAnimGroupHash(0)
, m_Flags(0)
, m_ConditionalAnimChosen(0)
{
}

CTaskFSMClone * CClonedTaskAmbientClipsInfo::CreateCloneFSMTask()
{
	
	CTaskAmbientClips* pTaskAmbientClips = NULL;

	const CConditionalAnimsGroup * pConditionalAnimsGroup  = CONDITIONALANIMSMGR.GetConditionalAnimsGroup(m_conditionalAnimGroupHash);

	if(pConditionalAnimsGroup ==NULL)
	{	
		//There are two places from where CConditionalAnimsGroup's are obtained.  
		//Either from a list maintained in CONDITIONALANIMSMGR, 
		//or they are got from a particular scenario with it is associated.
		//The list of scenarios are maintained in SCENARIOINFOMGR.
		//If we got here then m_conditionalAnimGroupHash from the remote task is not found in the CONDITIONALANIMSMGR
		//list, so now go through the scenarios in SCENARIOINFOMGR to see which scenario must have it.

		s32 iNumScenarios = SCENARIOINFOMGR.GetScenarioCount(false);

		// This might be expensive going through all scenarios though it is only done once at task creation 
		for (s32 iSpecificScenario=0; iSpecificScenario<iNumScenarios; iSpecificScenario++)
		{
			const CScenarioInfo* pScenarioInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(iSpecificScenario);

			if(pScenarioInfo && pScenarioInfo->GetClipData())
			{
				if(pScenarioInfo->GetClipData()->GetHash()==m_conditionalAnimGroupHash)
				{
					pConditionalAnimsGroup = pScenarioInfo->GetClipData();
					break;
				}
			}
		}
	}

	taskAssertf(pConditionalAnimsGroup, "We don't have a valid CConditionalAnimsGroup from either CONDITIONALANIMSMGR or SCENARIOINFOMGR!");

	pTaskAmbientClips = rage_new CTaskAmbientClips(m_Flags, pConditionalAnimsGroup, m_ConditionalAnimChosen );

	return pTaskAmbientClips;
}



////////////////////////////////////////////////////////////////////////////////

CTaskAmbientLookAtEvent::CTaskAmbientLookAtEvent(const CEvent* pEventToLookAt)
		: m_pEventToLookAt(NULL),
		  m_CommentOnVehicle(false),
		  m_bIgnoreFov(false)
{
	// We can't just store a pointer to the user's CEvent object, need to make a copy.
	if(pEventToLookAt && CEvent::CanCreateEvent())
	{
		m_pEventToLookAt = static_cast<CEvent*>(pEventToLookAt->Clone());
	}

	SetInternalTaskType(CTaskTypes::TASK_AMBIENT_LOOK_AT_EVENT);
}


CTaskAmbientLookAtEvent::~CTaskAmbientLookAtEvent()
{
	delete m_pEventToLookAt;
	m_CommentOnVehicle = false;
}


aiTask* CTaskAmbientLookAtEvent::Copy() const
{
	return rage_new CTaskAmbientLookAtEvent(m_pEventToLookAt);
}


CTask::FSM_Return CTaskAmbientLookAtEvent::ProcessPreFSM()
{
	// This task can do nothing useful if there is no event.
	if(!m_pEventToLookAt)
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}


CTask::FSM_Return CTaskAmbientLookAtEvent::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();
		FSM_State(State_Looking)
			FSM_OnUpdate
				return Looking_OnUpdate();
	FSM_End
}


CEntity* CTaskAmbientLookAtEvent::GetLookAtTarget() const
{
	if(!m_pEventToLookAt)
	{
		return NULL;
	}

	// Note: we could add event-dependent logic here. For example, if watching a melee fight event,
	// we may want to randomly look at one or the other participant.

	return m_pEventToLookAt->GetSourceEntity();
}

CEvent* CTaskAmbientLookAtEvent::GetEventToLookAt() const
{
	return m_pEventToLookAt;
}

#if !__FINAL

void CTaskAmbientLookAtEvent::Debug() const
{
#if DEBUG_DRAW
	const CEntity* pTgt = GetLookAtTarget();
	const CPed* pPed = GetPed();
	if(!pPed || !pTgt)
	{
		return;
	}
	CTaskAmbientClips* pTaskAmbientClips = static_cast<CTaskAmbientClips*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AMBIENT_CLIPS));
	if (pTaskAmbientClips && CAmbientLookAt::IsLookAtAllowedForPed(pPed, pTaskAmbientClips, true))
	{
		Vec3V pos1 = pPed->GetTransform().GetPosition();
		Vec3V pos2 = pTgt->GetTransform().GetPosition();
		pos1.SetZf(pos1.GetZf() + 0.6f);
		grcDebugDraw::Line(pos1, pos2, Color_PaleVioletRed);
	}
#endif
}


const char * CTaskAmbientLookAtEvent::GetStaticStateName( s32 iState )
{
	static const char* s_StateNames[] =
	{
		"State_Start",
		"State_Looking"
	};

	if(iState >= 0 && iState < kNumStates)
	{
		CompileTimeAssert(NELEM(s_StateNames) == kNumStates);
		return s_StateNames[iState];
	}
	else
	{
		return "Invalid";
	}
}

#endif	// !__FINAL

CTask::FSM_Return CTaskAmbientLookAtEvent::Start_OnUpdate()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	if( m_pEventToLookAt && m_pEventToLookAt->GetEventType() == EVENT_SHOCKING_SEEN_NICE_CAR)
	{
		m_CommentOnVehicle = true;
	}
	else if (m_pEventToLookAt && m_pEventToLookAt->GetEventType() == EVENT_SHOCKING_SEEN_WEIRD_PED)
	{
		//Check if the source is a player.
		CPed* pSourcePed = m_pEventToLookAt->GetSourcePed();
		bool bHavingConversation = false;
		CTaskAmbientClips* pTaskAmbient = static_cast<CTaskAmbientClips*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AMBIENT_CLIPS));
		if (pTaskAmbient && pTaskAmbient->IsInConversation())
		{
			bHavingConversation = true;
		}

		// Don't say SEE_WEIRDO when having a conversation. If on the phone, TaskConversationHelper will handle it.
		// If not, we don't want to talk over the existing conversation.
		if (!bHavingConversation)
		{
			if(pSourcePed && pSourcePed->IsPlayer())
			{
				//Say the exchange.
				CAmbientAudioExchange exchange;
				exchange.m_Initial.SetContext("SEE_WEIRDO");
				exchange.m_Initial.GetFlags().SetFlag(CAmbientAudio::IsInsulting);
				exchange.m_Response.SetContext("GENERIC_INSULT");
				exchange.m_Response.GetFlags().SetFlag(CAmbientAudio::IsInsulting);
				CTalkHelper::Talk(*pPed, *pSourcePed, exchange);
			}
			else
			{
				//Say the audio.
				CAmbientAudio audio;
				audio.SetContext("SEE_WEIRDO");
				audio.GetFlags().SetFlag(CAmbientAudio::IsInsulting);
				CTalkHelper::Talk(*pPed, audio);
			}
		}
	}
	else if (m_pEventToLookAt && m_pEventToLookAt->IsShockingEvent())
	{
		//Say the audio.
		CEventShocking* pShocking = static_cast<CEventShocking*>(m_pEventToLookAt.Get());
		pPed->RandomlySay(pShocking->GetShockingSpeechHash(), pShocking->GetTunables().m_ShockingSpeechChance);
	}

	if (m_pEventToLookAt && m_pEventToLookAt->IsShockingEvent())
	{
		//Check if we should ignore FOV.
		CEventShocking* pShocking = static_cast<CEventShocking*>(m_pEventToLookAt.Get());
		m_bIgnoreFov = pShocking->GetTunables().m_IgnoreFovForHeadIk;
	}

	SetState(State_Looking);
	return FSM_Continue;
}


CTask::FSM_Return CTaskAmbientLookAtEvent::Looking_OnUpdate()
{
	// If looking at a shocking event, we check to see if it's still in the global group.
	// Some shocking events have a limited lifetime in the global group, while others
	// remain for as long as some condition is fulfilled (for example, for as long as the player
	// is having a weapon drawn). 
	static const float s_fMinTimeToLook = 3.0f;		// MAGIC
	static const float s_fMaxTimeToLook = 10.0f;	// MAGIC
	// Force a minimum reaction time of three seconds so even if the event expires immediately we still see some headtracking.
	if(m_pEventToLookAt && GetTimeInState() >= s_fMinTimeToLook && static_cast<const CEvent*>(m_pEventToLookAt)->IsShockingEvent())
	{
		const CEventShocking* pShocking = static_cast<CEventShocking*>(m_pEventToLookAt.Get());
		CEventGroupGlobal* list = GetEventGlobalGroup();
		const int num = list->GetNumEvents();
		u32 idToFind = pShocking->GetId();
		bool found = false;
		for(int i = 0; i < num; i++)
		{
			fwEvent* ev = list->GetEventByIndex(i);
			if(!ev || !static_cast<const CEvent*>(ev)->IsShockingEvent())
			{
				continue;
			}
			CEventShocking* pEvent = static_cast<CEventShocking*>(ev);
			if(pEvent->GetId() == idToFind)
			{
				found = true;
				break;
			}
		}
		if(!found)
		{
			// This shocking event appears to no longer be active, stop the task.
			// Note that currently, this may not immediately make the ped stop looking at the
			// current target, but it should at least prevent them from picking it again.
			return FSM_Quit;
		}
	}
	else if(GetTimeInState() >= s_fMaxTimeToLook)
	{
		// For other events, it's probably a good idea to have some sort of timeout.
		return FSM_Quit;
	}

	// If this task no longer produces a valid target entity, we should probably end it.
	if(!GetLookAtTarget())
	{
		return FSM_Quit;
	}

	// Note: this task doesn't actually do anything by itself. Under the current
	// implementation, it works by CAmbientLookAt looking for the presence of the
	// task, and asking it what the current target should be.

	return FSM_Continue;
}

bool CTaskAmbientLookAtEvent::MakeAbortable(const AbortPriority priority, const aiEvent* pEvent)
{
	// We do not want to interrupt the task if it has not ran for a minimum amount of time.
	static const float fMIN_TIME_TO_BE_ABORTED_BY_EQUAL_PRIORITY_EVENT = 2.0f;
	if (priority != ABORT_PRIORITY_IMMEDIATE && 
		m_pEventToLookAt && pEvent && 
		pEvent->GetEventPriority() <= m_pEventToLookAt->GetEventPriority() && 
		GetTimeRunning() < fMIN_TIME_TO_BE_ABORTED_BY_EQUAL_PRIORITY_EVENT)
	{
		return false;
	}

	return aiTask::MakeAbortable(priority, pEvent);
}

////////////////////////////////////////////////////////////////////////////////

CTaskAmbientMigrateHelper::CTaskAmbientMigrateHelper()
: m_WaitState(WaitStateNone)
{
	Reset();
}

CTaskAmbientMigrateHelper::CTaskAmbientMigrateHelper(const CTaskAmbientClips* pAmbientTask)
: m_WaitState(WaitStateNone)
{
	GetClipMigrateDataFromTaskAmbient(pAmbientTask);
}

#if __ASSERT
const char* CTaskAmbientMigrateHelper::GetAssertNetObjPedName(const CTaskAmbientClips* pAmbientTask)
{
	const char* netObjPedName = "Null task";

	if(pAmbientTask)
	{
		CNetObjPed* netObjPed		= pAmbientTask->GetEntity()?(pAmbientTask->GetPed()?static_cast<CNetObjPed *>(pAmbientTask->GetPed()->GetNetworkObject()):NULL):NULL;

		netObjPedName	= netObjPed?netObjPed->GetLogName():"Null netObjPed";
	}

	return netObjPedName;
}
#endif

void CTaskAmbientMigrateHelper::Reset()
{
	m_ParentTaskTypeAtMigrate = CTaskTypes::TASK_INVALID_ID;

	m_RealScenarioType = -1;

	m_InitialFlags			   =0;
	m_ConditionalAnimGroupHash =0;
	m_fNextIdleTimer		=-1.0f;
	m_fTimeLastSearchedForBaseClip = 0.0f;
	m_State					=-1;
	m_BaseClipPhase			=-1.0f;
	m_BaseClipRate			=-1.0f;
	m_IdleClipPhase			=-1.0f;
	m_IdleClipRate			=-1.0f;
	m_BaseClipSet	=CLIP_SET_ID_INVALID;
	m_BaseClip		=CLIP_ID_INVALID;
	m_IdleClipSet	=CLIP_SET_ID_INVALID;
	m_IdleClip		=CLIP_ID_INVALID;
	
	m_ClipSetId		=CLIP_SET_ID_INVALID;
	m_ClipId		=CLIP_ID_INVALID;

	m_BaseClipRequestHelper.ReleaseClips();
	m_AmbientClipRequestHelper.ReleaseClips();

}


//The migrate helper could have held onto the peds' prop when the NetObjPed is deleted
//so this function allows the CleanUpGameObject function to remove it before the NetObjPed destructor
//deletes the CTaskAmbientMigrateHelper and our m_pPropHelper.
void CTaskAmbientMigrateHelper::NetObjPedCleanUp(CPed* pPed)
{
	if(m_pPropHelper && taskVerifyf(pPed, "Expect a valid ped here when cleaning up held object")) 
	{
		if(m_pPropHelper->IsHoldingProp())
		{
			m_pPropHelper->SetHoldingProp(pPed, false);
		}

		if(pPed->GetWeaponManager())
		{
			pPed->GetWeaponManager()->SetWeaponInstruction(CPedWeaponManager::WI_AmbientTaskDestroyWeapon);
		}
	}
}

CTaskAmbientMigrateHelper::~CTaskAmbientMigrateHelper()
{
	m_BaseClipRequestHelper.ReleaseClips();
	m_AmbientClipRequestHelper.ReleaseClips();

	// Normally ~PropManagmentHelper() asserts if we're still holding a prop, but the migrate helper could be the last (local) object holding on to the prop.
	//  So don't assert when we're cleaning it up. [1/19/2013 mdawe]
	if (m_pPropHelper)
	{
		CPed* pPed = NULL; //Don't have a ped here, but it doesn't matter.
		m_pPropHelper->ReleasePropRefIfUnshared(pPed);
	}
}

void CTaskAmbientMigrateHelper::GetClipMigrateDataFromTaskAmbient(const CTaskAmbientClips* pAmbientTask)
{
	Reset();

	if(taskVerifyf(pAmbientTask, "Expect a valid task ambient"))
	{
		GetConditionalAnimsGroupDataFromTaskAmbient(pAmbientTask);

		m_fNextIdleTimer				= pAmbientTask->m_fNextIdleTimer;
		m_fTimeLastSearchedForBaseClip	= pAmbientTask->m_fTimeLastSearchedForBaseClip;
		m_ConditionalAnimChosen			= pAmbientTask->m_ConditionalAnimChosen;
		m_pPropHelper					= pAmbientTask->m_pPropHelper;
		m_State							= pAmbientTask->GetState();

		if(m_State==CTaskAmbientClips::State_PlayIdleAnimation)
		{	//This makes it wait a frame before applying base anim
			SetWaiting();
		}

		m_InitialFlags							= pAmbientTask->m_CloneInitialFlags;
		Assertf(m_State!=-1, "%s - Expect a valid state at migrate", GetAssertNetObjPedName(pAmbientTask));

		// Streaming request helpers
		m_AmbientClipRequestHelper		= pAmbientTask->m_AmbientClipRequestHelper;
		m_BaseClipRequestHelper			= pAmbientTask->m_BaseClipRequestHelper;

		m_ClipSetId						= pAmbientTask->m_ClipSetId;
		m_ClipId						= pAmbientTask->m_ClipId;;

		const CClipHelper* baseClipHelper = pAmbientTask->GetBaseClip();
		if(baseClipHelper && pAmbientTask->IsBaseClipPlaying() && baseClipHelper->GetClipHelper())
		{
			m_BaseClipSet	= baseClipHelper->GetClipHelper()->GetClipSet();
			m_BaseClip		= baseClipHelper->GetClipHelper()->GetClipId();
			m_BaseClipPhase	= baseClipHelper->GetClipHelper()->GetPhase();
			m_BaseClipRate	= baseClipHelper->GetClipHelper()->GetRate();

			//use base clip helper to keep a reference to this anim until after migrated
			if(m_BaseClipRequestHelper.GetState()==CAmbientClipRequestHelper::AS_noGroupSelected)
			{
				m_BaseClipRequestHelper.SetClipAndProp(m_BaseClipSet, 0);
				// If a group has been successfully selected - request stream it in
				if( m_BaseClipRequestHelper.GetState() == CAmbientClipRequestHelper::AS_groupSelected )
				{
					m_BaseClipRequestHelper.RequestClips(false); // false = doesn't require ambient clip manager permission to load
				}
			}
		}

		if(pAmbientTask->GetClipHelper())
		{
			m_IdleClipSet	= pAmbientTask->GetClipHelper()->GetClipSet();
			m_IdleClip		= pAmbientTask->GetClipHelper()->GetClipId();
			m_IdleClipPhase	= pAmbientTask->GetClipHelper()->GetPhase();
			m_IdleClipRate	= pAmbientTask->GetClipHelper()->GetRate();
		}

#if __ASSERT
		gnetDebug3("%s   GetClipMigrateDataFromTaskAmbient. m_RealScenarioType %d, prop %s pAmbientTask->m_ConditionalAnimChosen %d, Group %s, m_Flags 0x%x, m_CloneInitialFlags 0x%x",
			GetAssertNetObjPedName(pAmbientTask),
			m_RealScenarioType,
			pAmbientTask->m_pPropHelper?pAmbientTask->m_pPropHelper->GetPropNameHash().GetCStr():"Null prop helper",
			pAmbientTask->m_ConditionalAnimChosen, 
			pAmbientTask->GetConditionalAnimsGroup()?pAmbientTask->GetConditionalAnimsGroup()->GetName():"Null group" ,
			pAmbientTask->m_Flags.GetAllFlags(),
			pAmbientTask->m_CloneInitialFlags.GetAllFlags()) ;

		gnetDebug3(" Task State %s, m_AmbientClipRequestHelper Set %s, State %d. m_BaseClipRequestHelper Set %s, State %d. m_IdleClipSet %s, phase %.2f. m_BaseClipSet %s, phase %.2f\n",
			pAmbientTask->GetStateName(pAmbientTask->GetState()), 
			m_AmbientClipRequestHelper.GetClipSet().GetCStr(),
			m_AmbientClipRequestHelper.GetState(),
			m_BaseClipRequestHelper.GetClipSet().GetCStr(),
			m_BaseClipRequestHelper.GetState(),
			m_IdleClipSet.GetCStr(),
			m_IdleClipPhase,
			m_BaseClipSet.GetCStr(),
			m_BaseClipPhase) ;
#endif
	}
}

void CTaskAmbientMigrateHelper::GetConditionalAnimsGroupDataFromTaskAmbient(const CTaskAmbientClips* pAmbientTask)
{
	if(taskVerifyf(pAmbientTask, "Expect a valid task ambient") )
	{
		m_ConditionalAnimGroupHash		= pAmbientTask->m_pConditionalAnimsGroup?pAmbientTask->m_pConditionalAnimsGroup->GetHash():0;

		const CTask* pParentTask = pAmbientTask->GetParent();
		if (pParentTask) 
		{
			m_ParentTaskTypeAtMigrate = pParentTask->GetTaskType(); 

			if(pParentTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO)
			{
				const CTaskUseScenario* pParentScenarioTask = static_cast<const CTaskUseScenario*>(pParentTask);

				int realScenarioType = pParentScenarioTask->GetScenarioType();

				const CScenarioInfo* pInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(realScenarioType);
				if(taskVerifyf(pInfo, "%s Get Ambient Migrate Expected scenario info for type %d", GetAssertNetObjPedName(pAmbientTask), realScenarioType))
				{
					m_RealScenarioType = realScenarioType;

#if __ASSERT
					const CPed* pPed = pAmbientTask->GetPed();
					const CConditionalAnimsGroup* pMigrateConditionalAnimsGroup = pInfo->GetClipData();
					if(pPed && !pPed->IsNetworkClone() && pMigrateConditionalAnimsGroup)
					{
						taskAssertf(pMigrateConditionalAnimsGroup->GetHash()== m_ConditionalAnimGroupHash, "%s pAnims->GetHash() %u, m_ConditionalAnimGroupHash %u",GetAssertNetObjPedName(pAmbientTask), pMigrateConditionalAnimsGroup->GetHash(),m_ConditionalAnimGroupHash);
					}
#endif
				}
			}
		}
	}
}

bool  CTaskAmbientMigrateHelper::ApplyConditionalAnimsGroupDataToTaskAmbient(CTaskAmbientClips* pAmbientTask)
{
	const CConditionalAnimsGroup * pMigrateConditionalAnimsGroup = NULL;

	if(taskVerifyf(pAmbientTask, "Expect a valid task ambient") )
	{
		if(m_RealScenarioType!=-1)
		{  
			//Is using scenario conditional anim group
			const CScenarioInfo* pInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(m_RealScenarioType);
			if(taskVerifyf(pInfo, "%s Apply Ambient Migrate Expected scenario info for type %d", GetAssertNetObjPedName(pAmbientTask), m_RealScenarioType))
			{
				pMigrateConditionalAnimsGroup = pInfo->GetClipData();
				Assertf(pMigrateConditionalAnimsGroup,"%s null scenario conditional anim group. m_RealScenarioType %d",GetAssertNetObjPedName(pAmbientTask),m_RealScenarioType);
			}
		}
		else
		{
			pMigrateConditionalAnimsGroup  = CONDITIONALANIMSMGR.GetConditionalAnimsGroup(m_ConditionalAnimGroupHash);
		}

		if( m_ConditionalAnimChosen >= 0 && pMigrateConditionalAnimsGroup)
		{
			if( m_ConditionalAnimChosen>=pMigrateConditionalAnimsGroup->GetNumAnims() )
			{
				Assertf(0,"%s Anim group %s. m_ConditionalAnimChosen %d. Max expected anim num %d. m_RealScenarioType %d. m_ConditionalAnimGroupHash 0x%x\n",
					CTaskAmbientMigrateHelper::GetAssertNetObjPedName(pAmbientTask),
					pMigrateConditionalAnimsGroup->GetName(),
					m_ConditionalAnimChosen,
					pMigrateConditionalAnimsGroup->GetNumAnims(),
					m_RealScenarioType,
					m_ConditionalAnimGroupHash);

				NOTFINAL_ONLY(gnetWarning("%s Anim group %s. m_ConditionalAnimChosen %d. Max expected anim num %d. m_RealScenarioType %d. m_ConditionalAnimGroupHash 0x%x\n",
					pAmbientTask->GetEntity()?(pAmbientTask->GetPed()->GetNetworkObject() ? pAmbientTask->GetPed()->GetNetworkObject()->GetLogName() : pAmbientTask->GetPed()->GetModelName()):"Null entity",
					pMigrateConditionalAnimsGroup->GetName(),
					m_ConditionalAnimChosen,
					pMigrateConditionalAnimsGroup->GetNumAnims(),
					m_RealScenarioType,
					m_ConditionalAnimGroupHash);)

				return false;
			}
		}

		if(pMigrateConditionalAnimsGroup && pMigrateConditionalAnimsGroup !=pAmbientTask->GetConditionalAnimsGroup())
		{
#if __ASSERT
			CPed* pPed = pAmbientTask->GetPed();
			Assertf( pPed->IsNetworkClone() || pAmbientTask->GetParent()!=NULL,"%s if Ped is not clone then expect the ambient task to have a parent",GetAssertNetObjPedName(pAmbientTask));
#endif
			pAmbientTask->SetConditionalAnims(pMigrateConditionalAnimsGroup);
		}
	}

	return true;
}


bool CTaskAmbientMigrateHelper::ApplyClipMigrateDataToTaskAmbient( CTaskAmbientClips* pAmbientTask)
{
	if(taskVerifyf(pAmbientTask, "Expect a valid task ambient") )
	{
		Assertf(m_State!=-1, "%s - Expect a valid state at migrate", GetAssertNetObjPedName(pAmbientTask));
		
		CTask* pParentTask = pAmbientTask->GetParent();
		if (pParentTask) 
		{
#if __ASSERT
			gnetDebug3("%s CTaskAmbientMigrateHelper::ApplyClipMigrateDataToTaskAmbient  pParentTask->GetTaskType() %d, m_ParentTaskTypeAtMigrate %d,  Group %s, FC: %d\n",
				GetAssertNetObjPedName(pAmbientTask),
				pParentTask->GetTaskType(),
				m_ParentTaskTypeAtMigrate,
				pAmbientTask->GetConditionalAnimsGroup()?pAmbientTask->GetConditionalAnimsGroup()->GetName():"Null",
				fwTimer::GetFrameCount());
#endif
			if( m_ParentTaskTypeAtMigrate != CTaskTypes::TASK_UNALERTED &&
				pParentTask->GetTaskType()!= CTaskTypes::TASK_UNALERTED &&
				m_ParentTaskTypeAtMigrate != CTaskTypes::TASK_INVALID_ID &&
				pParentTask->GetTaskType() !=m_ParentTaskTypeAtMigrate)
			{   //parent task type is different post migrate
				//so leave 
				Reset();
				return false;
			}
		}

		
		if(!ApplyConditionalAnimsGroupDataToTaskAmbient( pAmbientTask ))
		{
			//Anim mismatch in conditional group post migrate so don't apply migrate data and leave
			Reset();
			return false;
		}

		pAmbientTask->m_AmbientClipRequestHelper	= m_AmbientClipRequestHelper;
		pAmbientTask->m_BaseClipRequestHelper		= m_BaseClipRequestHelper;
		pAmbientTask->m_ClipSetId					= m_ClipSetId;
		pAmbientTask->m_ClipId						= m_ClipId;

		pAmbientTask->m_fNextIdleTimer					= m_fNextIdleTimer;
		pAmbientTask->m_fTimeLastSearchedForBaseClip	= m_fTimeLastSearchedForBaseClip;

		pAmbientTask->m_ClipSetId						= m_ClipSetId;
		pAmbientTask->m_ClipId							= m_ClipId;

		pAmbientTask->m_CloneInitialFlags				= m_InitialFlags;

		taskAssert(m_ConditionalAnimChosen >= -0x8000 && m_ConditionalAnimChosen <= 0x7fff);
		pAmbientTask->m_ConditionalAnimChosen = -1;
		pAmbientTask->m_pPropHelper = m_pPropHelper;

		if( m_ConditionalAnimChosen >= 0 && 
			taskVerifyf(pAmbientTask->GetConditionalAnimsGroup() && pAmbientTask->GetConditionalAnimsGroup()->GetNumAnims()> m_ConditionalAnimChosen,
			"%s Anim group %s num anims %d. num anim chosen %d, m_ConditionalAnimGroupHash %d",GetAssertNetObjPedName(pAmbientTask),pAmbientTask->GetConditionalAnimsGroup()?pAmbientTask->GetConditionalAnimsGroup()->GetName():"Null",pAmbientTask->GetConditionalAnimsGroup()?pAmbientTask->GetConditionalAnimsGroup()->GetNumAnims():-1,m_ConditionalAnimChosen, m_ConditionalAnimGroupHash) )
		{
			pAmbientTask->SetConditionalAnimChosen(m_ConditionalAnimChosen);
		}

		if (m_pPropHelper && pParentTask ) 
		{
			if(pParentTask->GetTaskType()==CTaskTypes::TASK_USE_SCENARIO)
			{
				//Grab the task.
				CTaskUseScenario* pTaskUseScenario = static_cast<CTaskUseScenario *>(pParentTask);
				pTaskUseScenario->SetPropManagmentHelper(m_pPropHelper);			
			}
#if __ASSERT
			else
			{
				Assertf(pParentTask->GetPropHelper()==NULL,
					"%s Don't expect non TASK_USE_SCENARIO parent task %s to have its own different prop helper. Parent prop %s, Migrate prop %s",
					GetAssertNetObjPedName(pAmbientTask),
					TASKCLASSINFOMGR.GetTaskName(pParentTask->GetTaskType()),
					pParentTask->GetPropHelper()->GetPropNameHash().GetCStr(),
					m_pPropHelper->GetPropNameHash().GetCStr());
			}
#endif
		}

#if __ASSERT
		gnetDebug3("%s   ApplyClipMigrateDataToTaskAmbient. prop %s pAmbientTask->m_ConditionalAnimChosen %d, m_ConditionalAnimChosen %d,  Group %s,  m_Flags: 0x%x, m_CloneInitialFlags 0x%x\n",
			GetAssertNetObjPedName(pAmbientTask),
			pAmbientTask->m_pPropHelper?pAmbientTask->m_pPropHelper->GetPropNameHash().GetCStr():"Null prop helper",
			pAmbientTask->m_ConditionalAnimChosen, 
			m_ConditionalAnimChosen,
			pAmbientTask->GetConditionalAnimsGroup()?pAmbientTask->GetConditionalAnimsGroup()->GetName():"Null group",
			pAmbientTask->m_Flags.GetAllFlags(), 
			pAmbientTask->m_CloneInitialFlags.GetAllFlags() ) ;
#endif
		return true;
	}

	Reset();
	return false;
}

void  CTaskAmbientMigrateHelper::ApplyMigrateIdleClip( CTaskAmbientClips* pAmbientTask )
{
	if( taskVerifyf(pAmbientTask,"Expect a valid pAmbientTask") && taskVerifyf(GetHasIdleClipMigrateData(),"%s Don't expect to try to set migrate idle clip if not valid", GetAssertNetObjPedName(pAmbientTask)) )
	{
		Assertf(m_IdleClipPhase<=1.0f,"%s, m_IdleClipPhase out of range %f",GetAssertNetObjPedName(pAmbientTask), m_IdleClipPhase);

		pAmbientTask->StartClipBySetAndClip( pAmbientTask->GetPed(), m_IdleClipSet, m_IdleClip, INSTANT_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish );

		if(	pAmbientTask->GetClipHelper()	!=	NULL	)
		{
			pAmbientTask->GetClipHelper()->SetPhase(m_IdleClipPhase);
			pAmbientTask->GetClipHelper()->SetRate(m_IdleClipRate);

			if(pAmbientTask->GetClipHelper()->GetClip())
			{
				pAmbientTask->m_VfxTagsExistIdle = CTaskAmbientClips::CheckClipHasVFXTags(*pAmbientTask->GetClipHelper()->GetClip());
				pAmbientTask->m_PropTagsExistIdle = CTaskAmbientClips::CheckClipHasPropTags(*pAmbientTask->GetClipHelper()->GetClip());
			}
		}	
		//reset once set
		ClearHasIdleClipMigrateData();
	}
}

void  CTaskAmbientMigrateHelper::ApplyMigrateBaseClip( CTaskAmbientClips* pAmbientTask, bool bDeferredStart )
{
	if(taskVerifyf(pAmbientTask,"Expect a valid pAmbientTask") && taskVerifyf(GetHasBaseClipMigrateData(),"%s Don't expect to try to set migrate base clip if not valid",GetAssertNetObjPedName(pAmbientTask)) )
	{
		if( GetResetWaiting() || (pAmbientTask->GetPed()->GetMovePed().GetBlendHelper().GetNetworkPlayer() && pAmbientTask->GetPed()->GetMovePed().GetBlendHelper().GetNetworkPlayer()->IsReadyForInsert()) )
		{
			gnetDebug3("%s %s CTaskAmbientMigrateHelper::ApplyMigrateBaseClip: Wait for an update or last player to be inserted %s. FC: %d",
				pAmbientTask->GetPed()->GetDebugName(),
				pAmbientTask->GetPed()->IsNetworkClone()?"CLONE":"DEBUG",
				pAmbientTask->GetPed()->GetMovePed().GetBlendHelper().GetNetworkPlayer()?(pAmbientTask->GetPed()->GetMovePed().GetBlendHelper().GetNetworkPlayer()->IsReadyForInsert()?"T":"F"):"null",
				fwTimer::GetFrameCount());

			//Wait a frame before applying base anim
			pAmbientTask->m_fTimeLastSearchedForBaseClip = 10.0f;//This will stop any possibility of the base clip being changed at re-evaluate timeout, and so will wait for deferred clip setting next frame.
			return;
		}

		if(bDeferredStart)
		{
			pAmbientTask->m_fTimeLastSearchedForBaseClip = -1.0f;//This will trigger a base clip reevaluation at first opportunity
			if(m_BaseClipRequestHelper.GetState() != CAmbientClipRequestHelper::AS_noGroupSelected)
			{
				pAmbientTask->m_BaseClipRequestHelper = m_BaseClipRequestHelper;
			}
			else if(GetValidBaseClipRunningAtMigrate())
			{
				//Set up base clip for when idle has finished
				pAmbientTask->m_BaseClipRequestHelper.SetClipAndProp( m_BaseClipSet, m_pPropHelper ? m_pPropHelper->GetPropNameHash().GetHash() : 0 );
			}
			else
			{
				Assertf(0,"%s Defered with no valid base clip running at migrate",GetAssertNetObjPedName(pAmbientTask));
			}
		}
		else if(GetValidBaseClipRunningAtMigrate() && m_BaseClipRequestHelper.GetState()==CAmbientClipRequestHelper::AS_groupStreamed )
		{
			float fBlendDelta = INSTANT_BLEND_IN_DELTA;

			pAmbientTask->m_BaseClip.StartClipBySetAndClip( pAmbientTask->GetPed(), m_BaseClipSet, m_BaseClip, fBlendDelta, CClipHelper::TerminationType_OnDelete );

			CClipHelper* baseClipHelper = pAmbientTask->GetBaseClip();

			if( baseClipHelper !=	NULL)
			{
				Assertf(m_BaseClipPhase<=1.0f,"%s m_BaseClipPhase out of range %f", GetAssertNetObjPedName(pAmbientTask), m_BaseClipPhase);
				baseClipHelper->SetPhase(m_BaseClipPhase);
				baseClipHelper->SetRate(m_BaseClipRate);

				if(baseClipHelper->GetClip())
				{
					pAmbientTask->m_VfxTagsExistBase = CTaskAmbientClips::CheckClipHasVFXTags(*baseClipHelper->GetClip());
					pAmbientTask->m_PropTagsExistIdle = CTaskAmbientClips::CheckClipHasPropTags(*baseClipHelper->GetClip());				
				}
			}
		}
#if __ASSERT
		else
		{
			Assertf(pAmbientTask->m_BaseClipRequestHelper.GetState()!=CAmbientClipRequestHelper::AS_noGroupSelected,
				"%s Not deferred or valid base clip running and m_BaseClipRequestHelper state = AS_noGroupSelected at migrate",GetAssertNetObjPedName(pAmbientTask));
		}
#endif
		//reset once set
		ClearHasBaseClipMigrateData();
	}
}

////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////

