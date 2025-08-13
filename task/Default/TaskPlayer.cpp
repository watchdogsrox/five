// Rage headers
#include "Math/AngMath.h"
#include "crskeleton/skeleton.h"
// Framework headers
#include "ai/task/taskchannel.h"
#include "fwanimation/animmanager.h"
#include "grcore/debugdraw.h"
#include "fwmaths/angle.h"
#include "fwmaths/vector.h"
#include "phbound/boundcylinder.h"

//Game headers
#include "ai/debug/system/AIDebugLogManager.h"
#include "AI/Ambient/ConditionalAnimManager.h"
#include "animation/AnimDefines.h"
#include "animation/move.h"
#include "animation/MovePed.h"
#include "audio/northaudioengine.h"
#include "audio/superconductor.h"
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/aim/FirstPersonShooterCamera.h"
#include "Camera/gameplay/aim/ThirdPersonAimCamera.h"
#include "Control/Gamelogic.h"
#include "Control/Route.h"
#include "Debug/VectorMap.h"
#include "Debug/DebugScene.h"
#include "Event/EventDamage.h"
#include "Event/Events.h"
#include "Event/EventReactions.h"
#include "Event/EventScript.h"
#include "Event/EventShocking.h"
#include "Event/EventWeapon.h"
#include "Event/ShockingEvents.h"
#include "frontend/MobilePhone.h"
#include "frontend/NewHud.h"
#include "Game/Localisation.h"
#include "ModelInfo/VehicleModelInfo.h"
#include "ModelInfo/WeaponModelInfo.h"
#include "PedGroup/PedGroup.h"
#include "Peds/AssistedMovement.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedHelmetComponent.h"
#include "Peds/PedMoveBlend/PedMoveBlendInWater.h"
#include "Peds/PedMoveBlend/PedMoveBlendManager.h"
#include "Peds/PedMoveBlend/PedMoveBlendBase.h"
#include "Peds/PedMoveBlend/PedMoveBlendOnFoot.h"
#include "Peds/PedMoveBlend/PedMoveBlendOnSkis.h"
#include "Peds/PedPlacement.h"
#include "Peds/PedWeapons/PedTargetEvaluator.h"
#include "Peds/Ped.h"
#include "Physics/Physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "renderer/River.h"
#include "scene/world/GameWorld.h"
#include "scene/EntityIterator.h"
#include "Streaming/Streaming.h"
#include "System/Pad.h"
#include "Task/Combat/Cover/Cover.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Combat/Cover/TaskSeekCover.h"
#include "Task/Combat/TaskCombat.h"
#include "Task/Combat/CombatMeleeDebug.h"
#include "Task/Combat/TaskCombatMelee.h"
#include "task/combat/TaskThreatResponse.h"
#include "Task/Default/TaskAmbient.h"
#include "Task/Default/TaskArrest.h"
#include "Task/Default/TaskCuffed.h"
#include "task/Default/TaskIncapacitated.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Default/TaskSlopeScramble.h"
#include "Task/General/TaskBasic.h"
#include "Task/General/TaskSecondary.h"
#include "Task/Motion/Locomotion/TaskBirdLocomotion.h"
#include "Task/Motion/Locomotion/TaskFishLocomotion.h"
#include "Task/Motion/Locomotion/TaskMotionAiming.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Motion/Locomotion/TaskInWater.h"
#include "Task/Motion/Locomotion/TaskCombatRoll.h"
#include "Task/Motion/Locomotion/TaskQuadLocomotion.h"
#include "task/Movement/TaskParachute.h"
#include "task/Movement/TaskTakeOffPedVariation.h"
#include "Task/Movement/Climbing/TaskLadderClimb.h"
#include "Task/Movement/Climbing/TaskGoToAndClimbLadder.h"
#include "Task/Movement/Climbing/TaskVault.h"
#include "Task/Movement/Jumping/TaskJump.h"
#include "Task/Movement/Jumping/TaskInAir.h"
#include "Task/Movement/TaskJetpack.h"
#include "Task/Movement/Climbing/TaskDropDown.h"
#include "Task/Movement/TaskCollisionResponse.h"
#include "Task/Physics/TaskNMHighFall.h"
#include "Task/Physics/TaskNMBalance.h"
#include "Task/Response/TaskAgitated.h"
#include "Task/Response/TaskDuckAndCover.h"
#include "Task/Scenario/Info/ScenarioInfo.h"
#include "task/Scenario/ScenarioFinder.h"
#include "Task/Scenario/ScenarioManager.h"
#include "Task/Scenario/TaskScenario.h" 
#include "Task/Scenario/Types/TaskUseScenario.h" 
#include "Task/System/MotionTaskData.h"
#include "Task/System/TaskHelpers.h"
#include "Task/Animation/TaskAnims.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskCarUtils.h"
#include "Task/Vehicle/TaskInVehicle.h"
#include "Task/General/Phone/TaskMobilePhone.h"

#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskMountAnimal.h"
#include "Task/Vehicle/TaskPlayerOnHorse.h"
#include "Task/Weapons/Gun/TaskReloadGun.h"
#include "Task/Weapons/TaskPlayerWeapon.h"
#include "Task/Weapons/TaskProjectile.h"
#include "Task/Weapons/TaskSwapWeapon.h"
#include "Task/Weapons/TaskWeaponBlocked.h"

#include "text/messages.h"
#include "game/modelindices.h"
#include "vehicles/automobile.h"
#include "vehicles/train.h"
#include "Vehicles/Metadata/VehicleDebug.h"
#include "Vehicles/Metadata/VehicleLayoutInfo.h"
#include "Vehicles/Metadata/VehicleEntryPointInfo.h"
#include "vehicles/Metadata/VehicleMetadataManager.h"
#include "vehicles/Metadata/VehicleSeatInfo.h"
#include "Vehicles/vehicle.h"
#include "vehicleai/pathfind.h"
#include "Weapons/ThrownWeaponInfo.h"
#include "Weapons/inventory/PedWeaponManager.h"
#include "Weapons/projectiles/Projectile.h"
#include "Weapons/projectiles/ProjectileManager.h"
#include "system/PadGestures.h"

#include "Task/Movement/TaskNavMesh.h"
#include "Task/Vehicle/TaskCarAccessories.h"
#include "control/restart.h"
#include "Objects/Object.h"
#include "debug/debug.h"

#include "frontend/MobilePhone.h"
#include "Pickups/Pickup.h"

// network includes
#include "network/NetworkInterface.h"
#include "Network/Events/NetworkEventTypes.h"
#include "Peds/PedFlagsMeta.h"

#define USE_PLAYER_MOVE_TASK			0
#define __ASSISTED_MOVEMENT_ON_SKIS		1
#define LADDER_HACKS					1

AI_OPTIMISATIONS()
AI_COVER_OPTIMISATIONS()
AI_MOTION_OPTIMISATIONS()
AI_MOVEMENT_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

bank_u32 CTaskPlayerOnFoot::ms_iQuickSwitchWeaponPickUpTime = 3000;
dev_float CTaskPlayerOnFoot::ms_fMeleeStartDistance = 8.0f;
dev_float CTaskPlayerOnFoot::ms_fMinTargetCheckDistSq = 4.25f;
dev_u32 CTaskPlayerOnFoot::ms_nRestartMeleeDelayInMS = 10000;
dev_u32 CTaskPlayerOnFoot::ms_nIntroAnimDelayInMS = 2000;
bank_bool CTaskPlayerOnFoot::ms_bAllowStrafingWhenUnarmed = false;
bank_float CTaskPlayerOnFoot::ms_fMaxSnowDepthRatioForJump = 0.4f;
dev_float CTaskPlayerOnFoot::ms_fStreamClosestLadderMinDistSqr = 10.f * 10.f;
bank_bool CTaskPlayerOnFoot::ms_bAllowLadderClimbing = true;
bank_bool CTaskPlayerOnFoot::ms_bRenderPlayerVehicleSearch = false;
bank_bool CTaskPlayerOnFoot::ms_bAnalogueLockonAimControl = true;
bank_bool CTaskPlayerOnFoot::ms_bDisableTalkingThePlayerDown = false;
bank_bool CTaskPlayerOnFoot::ms_bEnableDuckAndCover = false;
bank_bool CTaskPlayerOnFoot::ms_bEnableSlopeScramble = true;

CTaskPlayerOnFoot::Tunables CTaskPlayerOnFoot::sm_Tunables;

IMPLEMENT_DEFAULT_TASK_TUNABLES(CTaskPlayerOnFoot, 0x1122edd9);

dev_float MAX_STICK_INPUT_RECORD_TIME = 10.0f;
dev_float TIME_TO_CONSIDER_OLD_INPUT_VALID = 1.0f;

bool CTaskPlayerOnFoot::Tunables::GetUseThreatWeighting(const CPed& FPS_MODE_SUPPORTED_ONLY(rPed)) const
{
#if FPS_MODE_SUPPORTED
	return rPed.IsFirstPersonShooterModeEnabledForPlayer(false) ? m_UseThreatWeightingFPS : m_UseThreatWeighting;
#else // FPS_MODE_SUPPORTED
	return m_UseThreatWeighting;
#endif // FPS_MODE_SUPPORTED
}

void CTaskPlayerOnFoot::InitTunables()
{
	sm_Tunables.m_ArrestVehicleSpeed = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_ARREST_CROOK_MAX_SPEED", 0xBFBEA130), sm_Tunables.m_ArrestVehicleSpeed);
	sm_Tunables.m_ArrestDistance = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_ARREST_INTERACTION_RADIUS", 0x546DB1F8), sm_Tunables.m_ArrestDistance);
}

bool CTaskPlayerOnFoot::IsAPlayerCopInMP(const CPed& UNUSED_PARAM(ped))
{
	// url:bugstar:2557284 - eTEAM_COP isn't used any more, script use teams 0-3 freely with assuming certain team types. Removing below code.
	//if (NetworkInterface::IsGameInProgress())
	//{
	//	if (ped.IsAPlayerPed() && AssertVerify(ped.GetNetworkObject()))
	//	{
	//		CNetGamePlayer* pPlayer = ped.GetNetworkObject()->GetPlayerOwner();
	//		if (pPlayer)
	//		{
	//			if (AssertVerify(pPlayer->GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX))
	//			{
	//				if (pPlayer->GetTeam() == eTEAM_COP)
	//				{
	//					return true;
	//				}
	//			}
	//		}
	//	}
	//}

	return false;
}


bool CTaskPlayerOnFoot::CheckForUseMobilePhone(const CPed& ped)
{
	if ( ped.IsLocalPlayer() )
	{
		if ( (CPhoneMgr::CamGetState() || CPhoneMgr::IsDisplayed()) && !CPhoneMgr::GetGoingOffScreen() )
		{
			CTaskMobilePhone::PhoneMode phoneMode = CPhoneMgr::CamGetState() ? CTaskMobilePhone::Mode_ToCamera : CTaskMobilePhone::Mode_ToText; 
			return CTaskMobilePhone::CanUseMobilePhoneTask( ped, phoneMode );
		}
	}
	return false;
}

bool CTaskPlayerOnFoot::TestIfRouteToCoverPointIsClear(CCoverPoint& rCoverPoint, CPed& rPed, const Vector3& vDesiredProtectionDir)
{
	// Make sure there's nothing blocking the route to cover
	TUNE_GROUP_FLOAT(PLAYER_COVER_TUNE, ROUTE_CLEAR_CAPSULE_RADIUS, 0.175f, 0.0f, 1.0f, 0.01f);

	Vector3 vCoverPos(Vector3::ZeroType);
	// Get the cover position, and adjust it to be 1m off the ground
	rCoverPoint.ReserveCoverPointForPed(&rPed);
	CCover::FindCoordinatesCoverPoint(&rCoverPoint, &rPed, vDesiredProtectionDir, vCoverPos );
	rCoverPoint.ReleaseCoverPointForPed(&rPed);
	vCoverPos.z += 1.0f;

	// Update this just in case the player's position has changed since the start of the function
	Vector3 vPlayerPedPos = VEC3V_TO_VECTOR3(rPed.GetTransform().GetPosition());
	Vector3 vPedToPos = vCoverPos - vPlayerPedPos;

	// Only test if far enough away from the wall
	if( vPedToPos.Mag2() > rage::square(0.5f) )
	{
		// Bring the end test position away from the wall so the end capsule hemisphere doesn't intersect
		vPedToPos.Normalize();
		vCoverPos -= (vPedToPos*ROUTE_CLEAR_CAPSULE_RADIUS);

		TUNE_GROUP_FLOAT(PLAYER_COVER_TUNE, SECOND_CAPSULE_TEST_HEIGHT_OFFSET, 0.35f, 0.0f, 2.0f, 0.01f);

		// Test the route is clear
		if (DidCapsuleTestHitSomething(rPed, vPlayerPedPos, vCoverPos, ROUTE_CLEAR_CAPSULE_RADIUS))
		{
			return false;
		}
		
		vPlayerPedPos.z -= SECOND_CAPSULE_TEST_HEIGHT_OFFSET;
		vCoverPos.z -= SECOND_CAPSULE_TEST_HEIGHT_OFFSET;

		if (DidCapsuleTestHitSomething(rPed, vPlayerPedPos, vCoverPos, ROUTE_CLEAR_CAPSULE_RADIUS))
		{
			return false;
		}

		TUNE_GROUP_FLOAT(PLAYER_COVER_TUNE, MIN_COVER_HEIGHT_DIFF_TO_TEST_FOR_STEPS, 0.3f, 0.0f, 2.0f, 0.01f);
		TUNE_GROUP_BOOL(PLAYER_COVER_TUNE, ENABLE_STEP_DETECTION_PROBES, true);
		if (ENABLE_STEP_DETECTION_PROBES)
		{
			const float fHeightDiff = vCoverPos.z - vPlayerPedPos.z;
			if (fHeightDiff > MIN_COVER_HEIGHT_DIFF_TO_TEST_FOR_STEPS)
			{
				// Test parallel to ground from player to cover
				TUNE_GROUP_FLOAT(PLAYER_COVER_TUNE, STEP_TEST_Z_OFFSET, 0.3f, 0.0f, 2.0f, 0.01f);
				TUNE_GROUP_FLOAT(PLAYER_COVER_TUNE, MIN_COVER_HEIGHT_DIFF_TO_TEST_FOR_STEPS, 0.3f, 0.0f, 2.0f, 0.01f);
				Vector3 vStartPos = vPlayerPedPos;
				vStartPos.z -= STEP_TEST_Z_OFFSET;
				Vector3 vEndPos = vCoverPos;
				vEndPos.z = vStartPos.z;
				Vector3 vFirstIntersectionPos;
				Vector3 vFirstIntersectionNorm;
				if (DidProbeTestHitSomething(vStartPos, vEndPos, vFirstIntersectionPos, vFirstIntersectionNorm))
				{
					TUNE_GROUP_FLOAT(PLAYER_COVER_TUNE, MAX_STEP_NORMAL_DOT, 0.3f, -1.0f, 1.0f, 0.01f);
					const float fFirstIntersectDot = vFirstIntersectionNorm.Dot(ZAXIS);
					if (fFirstIntersectDot < MAX_STEP_NORMAL_DOT)
					{
						// Test down to ground at cover
						TUNE_GROUP_FLOAT(PLAYER_COVER_TUNE, COVER_DOWN_PROBE_LENGTH, 2.0f, 0.0f, 3.0f, 0.01f);
						Vector3 vStartPos = vCoverPos;
						Vector3 vEndPos = vCoverPos - Vector3(0.0f, 0.0f, COVER_DOWN_PROBE_LENGTH);
						Vector3 vSecondIntersectionPos;
						Vector3 vSecondIntersectionNorm;
						TUNE_GROUP_FLOAT(PLAYER_COVER_TUNE, MIN_GROUND_NORMAL_DOT, 0.7f, -1.0f, 1.0f, 0.01f);
						if (DidProbeTestHitSomething(vStartPos, vEndPos, vSecondIntersectionPos, vSecondIntersectionNorm))
						{
							// If norm 1 is horizontal and norm 2 is vertical rule out cover
							const float fSecondIntersectDot = vSecondIntersectionNorm.Dot(ZAXIS);
							if (fSecondIntersectDot > MIN_GROUND_NORMAL_DOT)
							{
								return false;
							}
						}
					}
				}
			}
		}
	}
	return true;
}

bool CTaskPlayerOnFoot::DidCapsuleTestHitSomething(const CPed& rPed, const Vector3& vStartPos, const Vector3& vEndPos, const float fCapsuleRadius)
{
	static const int iTestFlags = ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_PED_TYPE | ArchetypeFlags::GTA_VEHICLE_TYPE;
	WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
	const s32 MAX_INTERSECTIONS = 4;
	WorldProbe::CShapeTestFixedResults<MAX_INTERSECTIONS> capsuleResults;
	capsuleDesc.SetResultsStructure(&capsuleResults);
	capsuleDesc.SetExcludeEntity(&rPed);
	capsuleDesc.SetCapsule(vStartPos, vEndPos, fCapsuleRadius);
	capsuleDesc.SetIncludeFlags(iTestFlags);

	bool bHitSomething = WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc);

	// Only count peds in cover as obstructions as pushing them out of the way looks bad
	for (s32 i=0; i < capsuleResults.GetNumHits(); ++i)
	{
		bHitSomething = true;

		const CEntity* pHitEntity = CPhysics::GetEntityFromInst(capsuleResults[i].GetHitInst());
		if (pHitEntity && pHitEntity->GetIsTypePed())
		{
			const CPed* pHitPed = static_cast<const CPed*>(pHitEntity);
			if (!pHitPed->GetIsInCover())
			{
				bHitSomething = false;
				continue;
			}
			else
			{
				break;
			}
		}
		else if (capsuleResults[i].GetHitInst() && CTaskCover::HitInstIsFoliage(*capsuleResults[i].GetHitInst(), capsuleResults[i].GetHitComponent()))
		{
			bHitSomething = false;
			continue;
		}
		else
		{
			// Early out, hit something which isn't a ped not in cover
			break;
		}
	}

#if DEBUG_DRAW
	CCoverDebug::sDebugCapsuleParams capsuleParams;
	capsuleParams.vStart = RCC_VEC3V(vStartPos);
	capsuleParams.vEnd = RCC_VEC3V(vEndPos);
	capsuleParams.fRadius = fCapsuleRadius;
	capsuleParams.color = bHitSomething ? Color_red : Color_green;
	capsuleParams.uContextHash = CCoverDebug::CAPSULE_CLEARANCE_TESTS;
	CCoverDebug::AddDebugCapsule(capsuleParams);
	CTask::ms_debugDraw.AddCapsule(RCC_VEC3V(vStartPos), RCC_VEC3V(vEndPos), fCapsuleRadius, bHitSomething ? Color_red : Color_green, 5000, 0, false);
#endif // DEBUG_DRAW
	return bHitSomething;
}

bool CTaskPlayerOnFoot::DidProbeTestHitSomething(const Vector3& vStart, const Vector3& vEnd, Vector3& vIntersectionPos, Vector3& vIntersectionNormal)
{
	WorldProbe::CShapeTestFixedResults<> probeResult;
	s32 iTypeFlags = ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE;
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetResultsStructure(&probeResult);
	probeDesc.SetStartAndEnd(vStart, vEnd);
	probeDesc.SetIncludeFlags(iTypeFlags);
	probeDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
	probeDesc.SetIsDirected(true);

	WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);

	if (!probeResult[0].GetHitDetected())
	{
#if DEBUG_DRAW
		CTask::ms_debugDraw.AddLine(RCC_VEC3V(vStart), RCC_VEC3V(vEnd), Color_brown, 2500, 0);
#endif // DEBUG_DRAW
		return false;
	}

	bool bHitSomething = WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
	for (s32 i=0; i < probeResult.GetNumHits(); ++i)
	{
		bHitSomething = true;

		const CEntity* pHitEntity = CPhysics::GetEntityFromInst(probeResult[i].GetHitInst());
		if (pHitEntity && pHitEntity->GetIsTypeObject() && pHitEntity->GetIsDynamic())
		{
			bHitSomething = false;
			continue;
		}
		else if (PGTAMATERIALMGR->GetPolyFlagStairs(probeResult[i].GetHitMaterialId()))
		{
			bHitSomething = false;
			continue;
		}
		else if (probeResult[i].GetHitInst() && CTaskCover::HitInstIsFoliage(*probeResult[i].GetHitInst(), probeResult[i].GetHitComponent()))
		{
			bHitSomething = false;
			continue;
		}
		else
		{
			// Early out, hit something valid
			vIntersectionPos = probeResult[i].GetHitPosition();
			vIntersectionNormal = probeResult[i].GetHitNormal();
			break;
		}
	}

#if DEBUG_DRAW
	CTask::ms_debugDraw.AddLine(RCC_VEC3V(vStart), bHitSomething ? RCC_VEC3V(vIntersectionPos) : RCC_VEC3V(vEnd), bHitSomething ? Color_purple : Color_cyan, 2500, 0);
	if (bHitSomething)
	{
		CTask::ms_debugDraw.AddArrow(RCC_VEC3V(vIntersectionPos), VECTOR3_TO_VEC3V(vIntersectionPos + vIntersectionNormal), 0.25f, Color_purple, 2500, 0);
	}
#endif // DEBUG_DRAW

	return bHitSomething;
}

u32 CTaskPlayerOnFoot::m_uLastTimeJumpPressed = 0;
u32 CTaskPlayerOnFoot::m_uLastTimeSprintDown = 0;
u32 CTaskPlayerOnFoot::m_uReducingGaitTimeMS = 0;
u32 CTaskPlayerOnFoot::m_nLastFrameProcessedClimbDetector = 0;
//
//
//
CTaskPlayerOnFoot::CTaskPlayerOnFoot()
: m_vLastCoverDir(Vector3::ZeroType)
, m_vLastStickInput(Vector3::ZeroType)
, m_vBlockPos(Vector3::ZeroType)
, m_fBlockHeading(FLT_MAX)
, m_pLadderClipRequestHelper(NULL)
, m_chosenOptionalVehicleSet(CLIP_SET_ID_INVALID)
#if FPS_MODE_SUPPORTED
, m_pFirstPersonIkHelper(NULL)
, m_WasInFirstPerson(false)
#endif // FPS_MODE_SUPPORTED
, m_pMeleeRequestTask(NULL)
, m_pMeleeReactionTask(NULL)
, m_pScriptedTask(NULL)
, m_pTargetVehicle(NULL)
, m_pTargetAnimal(NULL)
, m_pTargetLadder(NULL)
, m_pPreviousTargetLadder(NULL)
, m_bCanReachLadder(false)
, m_bLadderVerticalClear(false)
, m_bUseSmallerVerticalProbeCheck(false)
, m_pTargetUncuffPed(NULL)
, m_pTargetArrestPed(NULL)
, m_iLadderIndex(0)
, m_uLastTimeInCoverTask(0)
, m_uLastTimeInMeleeTask(0xFFFFFFFF)
, m_uLastTimeEnterVehiclePressed(0)
, m_uLastTimeEnterCoverPressed(0)
, m_bWasMeleeStrafing(false)
, m_fIdleTime(0.0f)	
, m_pPedPlayerIntimidating(NULL)
, m_fTimeIntimidating(0.0f)
, m_fTimeBetweenIntimidations(0.0f)
, m_fTimeSinceLastStickInput(MAX_STICK_INPUT_RECORD_TIME)
, m_fTimeSinceLastValidSlopeScramble(0.f)
, m_fTimeSinceLastPlayerEventOpportunity(LARGE_FLOAT)
, m_fTimeSinceBirdProjectile(LARGE_FLOAT)
, m_nFramesOfSlopeScramble(0)
, m_bOptionVehicleGroupLoaded(false)
, m_bScriptedToGoIntoCover(false)
, m_bShouldStartGunTask(false)
, m_bCanStealthKill(false)
, m_bReloadFromIdle(false)
, m_bUsingStealthClips(false)
, m_bJumpedFromCover(false)
, m_bCoverSprintReleased(true)
, m_bPlayerExitedCoverThisUpdate(false)
, m_bGettingOnBottom(false)
, m_eArrestType(eAT_None)
, m_bWaitingForPedArrestUp(false)
, m_iJumpFlags(0)
, m_b1stTimeSeenWhenWanted(false)
, m_bResumeGetInVehicle(false)
, m_bCanBreakOutToMovement(false)
, m_bIdleWeaponBlocked(false)
, m_bCheckTimeInStateForCover(false)
, m_bWaitingForLadder(false)
, m_bEasyLadderEntry(false)
, m_bHasAttemptedToSpawnBirdProjectile(false)
, m_bNeedToSpawnBirdProjectile(false)
, m_nCachedBreakOutToMovementState(-1)
, m_bGetInVehicleAfterCombatRolling(false)
, m_fStandAutoStepUpTimer(0.0f)
#if FPS_MODE_SUPPORTED
, m_fGetInVehicleFPSIkZOffset(0.f)
, m_fTimeSinceLastSwimSprinted(0.0f)
#endif // FPS_MODE_SUPPORTED
, m_bForceInstantIkBlendInThisFrame(false)
, m_pTargetPed(NULL)
, m_pLastTargetPed(NULL)
, m_uArrestFinishTime(0)
, m_uArrestSprintBreakoutTime(0)
, m_bArrestTimerActive(false)
{
	m_optionalVehicleHelper.ReleaseClips();
	SetInternalTaskType(CTaskTypes::TASK_PLAYER_ON_FOOT);

#if __BANK
	AI_LOG_WITH_ARGS("[Player] - TASK_PLAYER_ON_FOOT constructed at 0x%p, see console log for callstack\n", this);
	if ((Channel_ai.FileLevel >= DIAG_SEVERITY_DISPLAY) || (Channel_ai.TtyLevel >= DIAG_SEVERITY_DISPLAY))
	{
		aiDisplayf("Frame %i, TASK_PLAYER_ON_FOOT constructed at 0x%p", fwTimer::GetFrameCount(), this);
		sysStack::PrintStackTrace();
	}
#endif // __BANK

	m_uLastTimeJumpPressed = 0;
	m_uLastTimeSprintDown = 0;
	m_uReducingGaitTimeMS = 0;
	m_nLastFrameProcessedClimbDetector = 0;
}

CTaskPlayerOnFoot::~CTaskPlayerOnFoot()
{
	m_optionalVehicleHelper.ReleaseClips();
	m_chosenOptionalVehicleSet = CLIP_SET_ID_INVALID;
	m_bOptionVehicleGroupLoaded = false;
	if (m_pLadderClipRequestHelper)
	{
		CLadderClipSetRequestHelperPool::ReleaseClipSetRequestHelper(m_pLadderClipRequestHelper);
		m_pLadderClipRequestHelper = NULL;
	}

	if( m_pMeleeReactionTask )
	{
		delete m_pMeleeReactionTask;
		m_pMeleeReactionTask = NULL;
	}

#if FPS_MODE_SUPPORTED
	if (m_pFirstPersonIkHelper)
	{
		delete m_pFirstPersonIkHelper;
		m_pFirstPersonIkHelper = NULL;
	}
#endif // FPS_MODE_SUPPORTED
}


aiTask* CTaskPlayerOnFoot::Copy() const
{
	CTaskPlayerOnFoot* pTask = rage_new CTaskPlayerOnFoot();
	pTask->SetScriptedToGoIntoCover(m_bScriptedToGoIntoCover);
	return pTask;
}

bool CTaskPlayerOnFoot::ProcessPostPreRender()
{
	CPed* pPed = GetPed();

	CheckForArmedMeleeAction(pPed);

	// Check if we have a pending bird projectile to send off.
	if (m_bNeedToSpawnBirdProjectile)
	{
		SpawnBirdProjectile(pPed);
	}

	return CTask::ProcessPostPreRender();
}

void CTaskPlayerOnFoot::ProcessPreRender2()
{
	CPed* pPed = GetPed();

	if (!fwTimer::IsGamePaused())
	{
		if (IsStateValidForIK())
		{
			float fOverrideBlendInDuration = -1.0f;
			float fOverrideBlendOutDuration = -1.0f;
			if (pPed->IsUsingActionMode())
			{
				fOverrideBlendOutDuration = 0.3f;
			}
			pPed->ProcessLeftHandGripIk(true, false, fOverrideBlendInDuration, fOverrideBlendOutDuration);
		}

#if FPS_MODE_SUPPORTED
		if (IsStateValidForFPSIK())
		{
			if (m_pFirstPersonIkHelper == NULL)
			{
				m_pFirstPersonIkHelper = rage_new CFirstPersonIkHelper();
				taskAssert(m_pFirstPersonIkHelper);
			}

			if (m_pFirstPersonIkHelper)
			{
				const CWeaponInfo* pWeaponInfo = CTaskMotionPed::GetWeaponInfo(pPed);
				m_pFirstPersonIkHelper->SetUseSecondaryMotion(pWeaponInfo && pWeaponInfo->GetUseFPSSecondaryMotion());

				const bool bUsingMobilePhone = CheckForUseMobilePhone(*pPed);
				const ScalarV vThreshold(10.0f);
				m_pFirstPersonIkHelper->SetUseLook(!bUsingMobilePhone && pPed->IsNearMirror(vThreshold));

				const bool bUseAnimBlockingTags = pPed->GetPedResetFlag(CPED_RESET_FLAG_PlayFPSIdleFidgets) && pPed->GetMotionData()->GetIsFPSIdle();
				m_pFirstPersonIkHelper->SetUseAnimBlockingTags(bUseAnimBlockingTags);

				const bool bInstant = (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InFPSUnholsterTransition) && !(pWeaponInfo->GetIsThrownWeapon() && pPed->GetMotionData()->GetIsFPSIdle())) || camInterface::GetGameplayDirector().GetJustSwitchedFromSniperScope();
				ArmIkBlendRate blendInRate = (m_bForceInstantIkBlendInThisFrame || bInstant || (pPed->GetPlayerInfo() && pPed->GetPlayerInfo()->GetSwitchedToOrFromFirstPersonCount() == 1)) ? ARMIK_BLEND_RATE_INSTANT : ARMIK_BLEND_RATE_FAST;
				ArmIkBlendRate blendOutRate = (bInstant || (pPed->GetPlayerInfo() && pPed->GetPlayerInfo()->GetSwitchedToOrFromFirstPersonCount() == 2)) ? ARMIK_BLEND_RATE_INSTANT : ARMIK_BLEND_RATE_FASTEST;

				TUNE_GROUP_BOOL(REAR_COVER_AIM_BUGS, DISABLE_NEW_BLEND_RATES, false);
				if (!DISABLE_NEW_BLEND_RATES && !CTaskCover::IsPlayerAimingDirectlyInFirstPerson(*pPed))
				{
					TUNE_GROUP_INT(FIRST_PERSON_COVER_BLEND_TUNE, iCoverIntroRightIKBlendIn, 4, 0, 5, 1);				
					TUNE_GROUP_INT(FIRST_PERSON_COVER_BLEND_TUNE, iCoverOutroRightIKBlendOut, 4, 0, 5, 1);
					if(pPed->GetPedResetFlag(CPED_RESET_FLAG_IsDoingCoverAimIntro))
					{
						blendInRate = (ArmIkBlendRate) iCoverIntroRightIKBlendIn;

					}
					else if(pPed->GetPedResetFlag(CPED_RESET_FLAG_IsDoingCoverAimOutro))
					{
						blendOutRate = (ArmIkBlendRate) iCoverOutroRightIKBlendOut;
					}
				}
				
				m_pFirstPersonIkHelper->SetBlendInRate(blendInRate);
				m_pFirstPersonIkHelper->SetBlendOutRate(blendOutRate);

				m_pFirstPersonIkHelper->Process(pPed);
			}
		}
		else
		{
			if (m_pFirstPersonIkHelper)
			{
				const bool bCameraCut = (camInterface::GetGameplayDirector().GetFirstPersonShooterCamera() == NULL) && 
										((camInterface::GetFrame().GetFlags() & (camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation)) != 0);

				// Reset arm solver on camera cut
				m_pFirstPersonIkHelper->Reset(pPed, bCameraCut);
			}

			// First person look when helper is not running
			if (IsStateValidForFPSIK(true))
			{
				const bool bUsingMobilePhone = CheckForUseMobilePhone(*pPed);
				const ScalarV vThreshold(10.0f);

				if (!bUsingMobilePhone && pPed->IsNearMirror(vThreshold))
				{
					Mat34V mtxEntity(pPed->GetMatrix());
					CFirstPersonIkHelper::ProcessLook(pPed, mtxEntity);
				}
			}
		}
#endif // FPS_MODE_SUPPORTED
	}
}

void CTaskPlayerOnFoot::ProcessPlayerEvents()
{
	CPed* pPed = GetPed();

	m_fTimeSinceLastPlayerEventOpportunity += fwTimer::GetTimeStep();

	if (m_fTimeSinceLastPlayerEventOpportunity >= sm_Tunables.m_TimeBetweenPlayerEvents)
	{
		m_fTimeSinceLastPlayerEventOpportunity = 0.0f;
		if (pPed->GetPedModelInfo()->GetPersonalitySettings().GetIsWeird() || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PlayerIsWeird))
		{
			CEventShockingWeirdPed ev(*pPed);
			CShockingEventsManager::Add(ev);
		}
	}
}

bool CTaskPlayerOnFoot::IsStateValidForIK() const
{
	const CPed* pPed = GetPed();

	if (pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_RELOAD_GUN))
	{
		return false;
	}

	if (pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SWAP_WEAPON))
	{
		return false;
	}

	// Cover handles grip IK itself.
	if (pPed->GetIsInCover())
	{
		return false;
	}

	bool bCheckDistance = true;
#if FPS_MODE_SUPPORTED
	if (pPed->GetIsFPSSwimming() && pPed->GetEquippedWeaponInfo() && pPed->GetEquippedWeaponInfo()->GetIsMelee())
	{
		return false;
	}

	//We do not need left hand IK 
	const bool bFirstPerson = pPed->IsFirstPersonShooterModeEnabledForPlayer(false);

	if(bFirstPerson)
	{
		const CWeapon* pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
		if(pWeapon && pWeapon->GetWeaponInfo()->GetIsUnarmed())
		{
			return false; 
		}
	}

	if(bFirstPerson && pPed->GetPedResetFlag(CPED_RESET_FLAG_IsThrowingProjectileWhileAiming))
	{
		return false;
	}

	if(bFirstPerson && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ForceFPSIKWithUpperBodyAnim))
	{
		bCheckDistance = false;
	}
#endif

	bool bUsingTwoHandedWeapon = false;
	const CWeaponInfo* pWeaponInfo = NULL;
	if( pPed->GetWeaponManager() )
	{
		const CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
		if (pWeaponObject)
		{
			const CWeapon* pWeapon = pWeaponObject->GetWeapon();
			if (pWeapon)
			{
				pWeaponInfo = pWeapon->GetWeaponInfo();
				taskAssert(pWeaponInfo);

				bUsingTwoHandedWeapon = pWeaponInfo->GetIsGun2Handed() || pWeaponInfo->GetIsMelee2Handed();
				if (!pWeaponInfo->GetIsGun1Handed() && !bUsingTwoHandedWeapon && !(bFirstPerson && pWeaponInfo->GetAttachFPSLeftHandIKToRight()))
				{
					return false;
				}

				if (bCheckDistance)
				{
					// Check distance of left hand to weapon grip in animation and disable IK if distance is too great
					const CWeaponModelInfo* pWeaponModelInfo = pWeapon->GetWeaponModelInfo();
					const crSkeleton* pWeaponSkeleton = pWeapon->GetEntity() ? pWeapon->GetEntity()->GetSkeleton() : NULL;
					const crSkeleton* pPlayerSkeleton = pPed->GetSkeleton();

					if (pWeaponModelInfo && pWeaponSkeleton && pPlayerSkeleton)
					{
						// Animators requested R grip be used for 1-handed guns
						s32 boneIdxGrip = pWeaponModelInfo->GetBoneIndex(bUsingTwoHandedWeapon ? WEAPON_GRIP_L : WEAPON_GRIP_R);
						s32 boneIdxHandLeft = -1;

						pPlayerSkeleton->GetSkeletonData().ConvertBoneIdToIndex((u16)BONETAG_L_IK_HAND, boneIdxHandLeft);

						if ((boneIdxGrip >= 0) && (boneIdxHandLeft >= 0))
						{
							Mat34V mtxGrip;
							pWeaponSkeleton->GetGlobalMtx(boneIdxGrip, mtxGrip);

							Mat34V mtxHandLeft;
							pPlayerSkeleton->GetGlobalMtx(boneIdxHandLeft, mtxHandLeft);

							Vec3V vGripPos = mtxGrip.GetCol3();
							Vec3V vHandPos = mtxHandLeft.GetCol3();

#if FPS_MODE_SUPPORTED
							// B*2248699: Update grip position based on change in left hand position from CTaskHumanLocomotion::ProcessPostCamera before we update the peds heading. 
							if (bFirstPerson && pPed->GetPlayerInfo())
							{								
								Vec3V vCachedHandPos = pPed->GetPlayerInfo()->GetLeftHandGripPositionFPS();
								if (!IsZeroAll(vCachedHandPos))
								{
									Vec3V vHandOffset = vHandPos - vCachedHandPos;
									vGripPos = vGripPos + vHandOffset;
								}
								// Reset the cached position.
								Vec3V vZero(V_ZERO);
								pPed->GetPlayerInfo()->SetLeftHandPositionFPS(vZero);
							}
#endif	// FPS_MODE_SUPPORTED

							const ScalarV sDistSquared = DistSquared(vGripPos, vHandPos);
							static dev_float fHandToGripRange = 0.15f;
							static dev_float fExpandedHandToGripRange = 0.2f;
							float fGripRange = pPed->IsUsingActionMode() || pPed->GetMotionData()->GetIsDesiredSprinting(true) ? fExpandedHandToGripRange : fHandToGripRange;

							if(pPed->GetPlayerInfo() && pPed->GetPlayerInfo()->IsPlayerStateValidForFPSManipulation())
							{
								static const float s_fOneOverThirty = 1.f/30.f;
								const float fTimeScaler = Clamp(fwTimer::GetTimeStep() / s_fOneOverThirty, 1.f, 2.f);

								// Modify grip range test by variable time step
								fGripRange *= fTimeScaler;
							}

							if (sDistSquared.Getf() > rage::square(fGripRange))
							{
								return false;
							}
						}
					}
				}
			}
		}
	}

	static const fwMvBooleanId s_EnableLeftHandIkId("EnableLeftHandIk",0xebf13a3d);

#if FPS_MODE_SUPPORTED
	if(bFirstPerson && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ForceFPSIKWithUpperBodyAnim))
	{
		return true;
	}
#endif // FPS_MODE_SUPPORTED

	switch(GetState())
	{
	case(STATE_INITIAL):
	case(STATE_PLAYER_GUN):
	case(STATE_USE_COVER):
	case(STATE_MELEE_REACTION):
		if (pPed->GetIkManager().IsActive(IKManagerSolverTypes::ikSolverTypeTorsoReact))
		{
			return true;
		}

		if(pPed->IsUsingActionMode())
		{
			return true;
		}

		break;
	case(STATE_MELEE):
		{
			if(pPed->IsUsingActionMode())
			{
				CTaskMeleeActionResult *pMeleeActionResult = pPed->GetPedIntelligence()->GetTaskMeleeActionResult();
				if( pMeleeActionResult && pMeleeActionResult->GetMoveNetworkHelper()->GetBoolean(s_EnableLeftHandIkId) )
				{
					return true;
				}
			}
		}
		break;
	case(STATE_JUMP):
		{
			if(pPed->GetPedResetFlag(CPED_RESET_FLAG_IsJumping) || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsLanding))
			{
				CTaskJump* pTaskJump = (CTaskJump*)( pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_JUMP) );
				if(pTaskJump && !pTaskJump->GetIsStateFall() && !pTaskJump->GetIsDivingLaunch() && !pTaskJump->GetIsSkydivingLaunch())
				{
					return true;
				}
			}
		}
		break;
	case(STATE_PLAYER_IDLES):
		{
			if(pPed->GetMotionData()->GetUsingStealth() || pPed->IsUsingActionMode())
			{
				if(pPed->GetMovementModeData().m_UseLeftHandIk)
				{
					return true;
				}
				else
				{
					return false;
				}
			}

			if(pWeaponInfo && pWeaponInfo->GetFlag(CWeaponInfoFlags::DisableLeftHandIkWhenOnFoot))
			{
				return false;
			}

			if (pPed->GetIkManager().IsActive(IKManagerSolverTypes::ikSolverTypeTorsoReact))
			{
				return true;
			}

			// Disable Ik in transitions
			CTaskMotionBase* pMotionTask = pPed->GetCurrentMotionTask(false);
			if(pMotionTask)
			{
				CTaskHumanLocomotion* pLocoTask = pMotionTask->GetTaskType() == CTaskTypes::TASK_HUMAN_LOCOMOTION ? static_cast<CTaskHumanLocomotion*>(pMotionTask) : NULL;
				if(pLocoTask)
				{
					//Since cover->Idles doesn't qualify for IsTransitionToIdleIntroActive, we need to block IK some other way, B* 1384768
					static dev_float sf_CoverIdleIKBlockTime = 0.2f;
					if (!bUsingTwoHandedWeapon && GetPreviousState() == STATE_USE_COVER && GetTimeInState() < sf_CoverIdleIKBlockTime)
						return false;

					if(!pLocoTask->IsTransitionToIdleIntroActive() || pLocoTask->IsFullyInIdle())
					{
						return true;
					}

					if(pLocoTask->GetMoveNetworkHelper() && pLocoTask->GetMoveNetworkHelper()->GetBoolean(s_EnableLeftHandIkId))
					{
						return true;
					}
				}
				else if(pMotionTask->GetTaskType() == CTaskTypes::TASK_MOTION_DIVING || pMotionTask->GetTaskType() == CTaskTypes::TASK_MOTION_SWIMMING)
				{
					return true;
				}
			}
		}
		break;
#if FPS_MODE_SUPPORTED
	case STATE_GET_IN_VEHICLE:
		if(bFirstPerson && pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GO_TO_CAR_DOOR_AND_STAND_STILL))
		{
			return true;
		}
		break;
#endif // FPS_MODE_SUPPORTED
	default:
		break;
	}

	return false;
}

#if FPS_MODE_SUPPORTED
bool CTaskPlayerOnFoot::IsStateValidForFPSIK(bool bExcludeUnarmed) const
{
	const CPed* pPed = GetPed();

	if (!pPed->IsFirstPersonShooterModeEnabledForPlayer(false, false, false, true, false))
	{
		return false;
	}

	const CWeaponInfo* pWeaponInfo = CTaskMotionPed::GetWeaponInfo(pPed);
	if (!pWeaponInfo)
	{
		return false;
	}

	if (pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableFPSArmIK))
	{
		return false;
	}

	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_WasPlayingFPSGetup) || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_WasPlayingFPSMeleeActionResult))
	{
		return false;
	}

	// MN: If FPSAllowAimIKForThrownProjectile is set, we want to disregard the weapon info check (as we might switch to unarmed while throwing)
	// We already check the weapon flag before setting FPSAllowAimIKForThrownProjectile so it should be good
	if(!pWeaponInfo->GetUseFPSAimIK() && (!pWeaponInfo->GetIsUnarmed() || !pPed->GetMotionData()->GetIsFPSLT()) && !pPed->GetPedResetFlag(CPED_RESET_FLAG_FPSAllowAimIKForThrownProjectile))
	{
		return false;
	}

	const CPedMotionData* pMotionData = pPed->GetMotionData();
	if (!pMotionData->GetIsFPSRNG() && !pMotionData->GetIsFPSLT() && !pMotionData->GetIsFPSScope() && !pMotionData->GetIsFPSIdle())
	{
		return false;
	}

	if (pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SWAP_WEAPON) FPS_MODE_SUPPORTED_ONLY( && !pPed->IsFirstPersonShooterModeEnabledForPlayer(false)))
	{
		return false;
	}

	if (pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_TAKE_OFF_HELMET))
	{
		return false;
	}

	switch(GetState())
	{
		case STATE_MELEE:
		{
			CTaskMelee* pTaskMelee = static_cast<CTaskMelee*>(FindSubTaskOfType(CTaskTypes::TASK_MELEE));
			if (pTaskMelee && pTaskMelee->IsBlocking())
			{
				return true;
			}

			// Intentional fall-through as we also want to use IK when aiming/sprinting in the melee state
		}
		case STATE_PLAYER_IDLES:
		case STATE_PLAYER_GUN:
		{
			if (!bExcludeUnarmed && (pWeaponInfo->GetIsUnarmed() || pWeaponInfo->GetIsMeleeFist()))
			{
				if(!CTaskMotionAiming::IsValidForUnarmedIK(*pPed))
				{
					return false;
				}
			}

			if (pPed->GetPedResetFlag(CPED_RESET_FLAG_IsAiming))
			{
				return true;
			}

			TUNE_GROUP_BOOL(FPS_IK, ENABLE_WHILE_SPRINTING, true);
			if (ENABLE_WHILE_SPRINTING && pPed->GetMotionData()->GetIsDesiredSprinting(true))
			{
				return true;
			}

			break;
		}
		case STATE_USE_COVER:
		{
			if (pPed->GetPedResetFlag(CPED_RESET_FLAG_IsDoingCoverAimIntro) || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsAimingFromCover))
			{
				return true;
			}

			if (pPed->GetPedResetFlag(CPED_RESET_FLAG_IsDoingCoverAimOutro))
			{
				if(!pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableRightArmIKInCoverOutroFPS))
				{
					return true;
				}
				else
				{
					return false;
				}
			}

			break;
		}
		case STATE_GET_IN_VEHICLE:
			if(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GO_TO_CAR_DOOR_AND_STAND_STILL))
			{
				return true;
			}
		case STATE_INITIAL:
		case STATE_JUMP:
		case STATE_SWAP_WEAPON:
		case STATE_CLIMB_LADDER:
		case STATE_TAKE_OFF_HELMET:
		case STATE_MELEE_REACTION:
		case STATE_ARREST:
		case STATE_UNCUFF:
		case STATE_MOUNT_ANIMAL:
		case STATE_DUCK_AND_COVER:
		case STATE_SCRIPTED_TASK:
		case STATE_DROPDOWN:
		case STATE_TAKE_OFF_PARACHUTE_PACK:
		case STATE_TAKE_OFF_JETPACK:
		case STATE_TAKE_OFF_SCUBA_GEAR:
		case STATE_RIDE_TRAIN:
		case STATE_JETPACK:
		case STATE_BIRD_PROJECTILE:
		default:
		{
			// Allow IK if we are playing FPS upper body anims for stumbling.
			if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ForceFPSIKWithUpperBodyAnim))
			{
				return true;
			}

			break;
		}
	}

	return false;
}
#endif // FPS_MODE_SUPPORTED

bool CTaskPlayerOnFoot::HasPlayerSelectedGunTask(CPed* FPS_MODE_SUPPORTED_ONLY(pPlayerPed), s32 nState)
{
#if FPS_MODE_SUPPORTED
	if(pPlayerPed->GetPlayerInfo() && pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false) && pPlayerPed->GetMotionData()->GetIsFPSIdle())
	{
		return false;
	}
#endif

	return nState == STATE_PLAYER_GUN;
}

void CTaskPlayerOnFoot::UpdateTalk(CPed* pPlayerPed, bool bCanBeFriendly, bool bCanBeUnfriendly)
{
	CControl *pControl = pPlayerPed->GetControlFromPlayer();
	if(pControl->GetPedTalk().IsPressed())
	{
		//Talk.
		fwFlags8 uFlags = 0;
		if(bCanBeFriendly)
		{
			uFlags.SetFlag(CTalkHelper::Options::CanBeFriendly);
		}
		if(bCanBeUnfriendly)
		{
			uFlags.SetFlag(CTalkHelper::Options::CanBeUnfriendly);
		}
		CTalkHelper::Options options(uFlags, sm_Tunables.m_MaxDistanceToTalk, sm_Tunables.m_MinDotToTalk);
		CTalkHelper::Talk(*pPlayerPed, options);
	}
}

bool CTaskPlayerOnFoot::IsPlayerStickInputValid(const CPed& rPlayerPed)
{
	const CControl* pControl = rPlayerPed.GetControlFromPlayer();
	if (!pControl)
		return false;

	bool bStickInputValid = false;
	Vector2 vStickInput;
	const CTaskPlayerOnFoot* pPlayerOnFootTask = static_cast<const CTaskPlayerOnFoot*>(rPlayerPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_ON_FOOT));
	if (pPlayerOnFootTask)
	{
		 bStickInputValid = pPlayerOnFootTask->GetLastStickInputIfValid(vStickInput);
	}

	if (!bStickInputValid)
	{
		vStickInput.x = pControl->GetPedWalkLeftRight().GetNorm();
		vStickInput.y = pControl->GetPedWalkUpDown().GetNorm();
	}

	if (Abs(vStickInput.x) < sm_Tunables.m_DeadZoneStickNorm && Abs(vStickInput.y) < sm_Tunables.m_DeadZoneStickNorm)
	{
		return false;
	}
	return true;
}

float CTaskPlayerOnFoot::StaticCoverpointScoringCB(const CCoverPoint* pCoverPoint, const Vector3& UNUSED_PARAM(vCoverPointCoords), void* pData)
{
	PlayerCoverSearchData* pCoverSearchData = (PlayerCoverSearchData*)pData;
	const CPed& rPlayerPed = *pCoverSearchData->m_pPlayerPed;

	s32 iNoTexts = 0;

	const Vector3 vPedPos = VEC3V_TO_VECTOR3(rPlayerPed.GetTransform().GetPosition());
	camGameplayDirector& rGameplayDirector = camInterface::GetGameplayDirector();
	const Vector3 vCamFront = rGameplayDirector.GetFrame().GetFront();
	const bool bStickInputValid = IsPlayerStickInputValid(rPlayerPed);
	const bool bHasThreat = pCoverSearchData->m_HasThreat;

	// Then score it
	const float fScore = ScoreCoverPoint(vPedPos, vCamFront, rPlayerPed, *pCoverPoint, false, pCoverSearchData->m_CoverProtectionDir, bStickInputValid, bHasThreat, &iNoTexts);
	return fScore;
}

bool CTaskPlayerOnFoot::StaticCoverpointValidityCallback (const CCoverPoint* pCoverPoint, const Vector3& UNUSED_PARAM(vCoverPointCoords), void* pData)
{
	PlayerCoverSearchData* pCoverSearchData = (PlayerCoverSearchData*)pData;
	const CPed& rPlayerPed = *pCoverSearchData->m_pPlayerPed;

	s32 iNoTexts = 0;

	const Vector3 vPedPos = VEC3V_TO_VECTOR3(rPlayerPed.GetTransform().GetPosition());
	camGameplayDirector& rGameplayDirector = camInterface::GetGameplayDirector();
	const Vector3 vCamFront = rGameplayDirector.GetFrame().GetFront();

	// Validate the static cover point first
	if (!IsCoverValid(vPedPos, vCamFront, rPlayerPed, *pCoverPoint, true, &iNoTexts))
	{
		return false;
	}
	return true;
}

bool CTaskPlayerOnFoot::IsCoverValid(const Vector3& vPedPos, const Vector3& vCamFwd, const CPed& rPed, const CCoverPoint& rCover, bool bIsStaticCover, s32* DEBUG_DRAW_ONLY(piNoTexts))
{
	taskAssertf(rPed.IsLocalPlayer(), "Don't call this on non local player peds");

	Vector3 vCoverPos;
	if (!rCover.GetCoverPointPosition(vCoverPos))
	{
		return false;
	}
	vCoverPos.z += 1.0f;

#if DEBUG_DRAW
	char szText[128];
	s32 iLocalNoTexts = 1;
	s32& iNoTexts = piNoTexts ? *piNoTexts : iLocalNoTexts;
	const s32 iContext = bIsStaticCover ? CCoverDebug::STATIC_COVER_EVALUATION : CCoverDebug::INITIAL_SEARCH_SIMPLE;
#endif // DEBUG_DRAW

	const bool bStickInputValid = IsPlayerStickInputValid(rPed);

	Vector3 vSearchDirection = vCamFwd;	
	Vector3 vThreatDir = vCamFwd;
	Vector3 vDesired(0.0f,1.0f,0.0f);
	bool bGotStickInput = false;
	const CTaskPlayerOnFoot* pPlayerOnFootTask = static_cast<const CTaskPlayerOnFoot*>(rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_ON_FOOT));
	if (pPlayerOnFootTask && pPlayerOnFootTask->GetState() != CTaskPlayerOnFoot::STATE_PLAYER_GUN && bStickInputValid)
	{
		bGotStickInput = true;
		vDesired.RotateZ(rPed.GetDesiredHeading());
		vSearchDirection = vDesired;
		vThreatDir = vDesired;
	}

	bool bHasThreat = false;
	if (sm_Tunables.m_EvaluateThreatFromCoverPoints)
	{
		// Compute the desired protection direction from this cover's position

		if (CTaskCover::ComputeDesiredProtectionDirection(rPed, vCoverPos, vThreatDir))
		{
			bHasThreat = true;
#if DEBUG_DRAW
			CCoverDebug::AddDebugPositionSphereAndDirection(RCC_VEC3V(vCoverPos), RCC_VEC3V(vThreatDir), Color_red, "THREAT", CCoverDebug::ms_Tunables.m_TextOffset * -1, iContext, false);
#endif // DEBUG_DRAW
		}
	}

	Vector3 vPedToCover = vCoverPos - vPedPos;
	vPedToCover.Normalize();
	const float fDistSqd = vPedPos.Dist2(vCoverPos);

	TUNE_GROUP_FLOAT(COVER_TUNE, MIN_DIST_TO_CHECK_BEHIND_TEST, 1.0f, 0.0f, 10.0f, 0.01f)
	aiDebugf2("BEHIND CHECK cover point %p, fDist = %.2f", &rCover, Sqrtf(fDistSqd));
	if (NetworkInterface::IsGameInProgress() && fDistSqd > square(MIN_DIST_TO_CHECK_BEHIND_TEST))
	{
		aiDebugf2("BEHIND CHECK vPedToCover.Dot(vCamFwd) = %.2f", vPedToCover.Dot(vCamFwd));
		TUNE_GROUP_FLOAT(COVER_TUNE, MIN_CAM_ANGLE_TO_CONSIDER_BEHIND, 0.5f, -1.0f, 1.0f, 0.01f)
		if (vPedToCover.Dot(vCamFwd) < MIN_CAM_ANGLE_TO_CONSIDER_BEHIND)
		{
			TUNE_GROUP_FLOAT(COVER_TUNE, MIN_PED_ANGLE_TO_CONSIDER_BEHIND, 0.35f, -1.0f, 1.0f, 0.01f)
			aiDebugf2("BEHIND CHECK vPedToCover.Dot(vDesired) = %.2f", vPedToCover.Dot(vDesired));
			if (!bGotStickInput || vPedToCover.Dot(vDesired) < MIN_PED_ANGLE_TO_CONSIDER_BEHIND)
			{
				return false;
			}
		}
	}

	const Vector3 vCoverDirection = VEC3V_TO_VECTOR3(rCover.GetCoverDirectionVector());

	// We disallow cover points too far and at an acute angle from the camera (unless the cover provides protection against the threat)
	Vector3 vCoverDir = VEC3V_TO_VECTOR3(rCover.GetCoverDirectionVector());
	const bool bUseThreatWeight = sm_Tunables.GetUseThreatWeighting(rPed);
	const bool bProvidesCoverFromThreat = bUseThreatWeight && bHasThreat && CTaskCover::DoesCoverPointProvideCoverFromThreat(vCoverDir, vThreatDir);

	if (!bProvidesCoverFromThreat)
	{
		if (CTaskCover::IsCoverPointPositionTooFarAndAcute(rPed, vCoverPos))
		{
#if DEBUG_DRAW
			formatf(szText, "INVALID: COVER TOO FAR AND ACUTE");
			CCoverDebug::AddDebugText(RCC_VEC3V(vCoverPos), Color_blue, szText, -1, CCoverDebug::ms_Tunables.m_TextOffset * iNoTexts++, iContext);
#endif // DEBUG_DRAW
			return false;
		}
		else
		{
			const Vector3 vToCoverDirection = fDistSqd < 1.0f ? vCoverDirection : vPedToCover;
			const float fSearchDot = vSearchDirection.Dot(vToCoverDirection);
			const float fThreatDot = vThreatDir.Dot(vToCoverDirection);
			if (fSearchDot < sm_Tunables.m_SearchThreatMaxDot && fThreatDot < sm_Tunables.m_SearchThreatMaxDot)
			{
#if DEBUG_DRAW
				formatf(szText, "INVALID: S(%.2f)T(%.2f)", fSearchDot, fThreatDot);
				CCoverDebug::AddDebugText(RCC_VEC3V(vCoverPos), Color_blue, szText, -1, CCoverDebug::ms_Tunables.m_TextOffset * iNoTexts++, iContext);
#endif // DEBUG_DRAW
				return false;
			}
		}
	}

	if (!CTaskCover::IsCoverValid(rCover, rPed))
	{
#if DEBUG_DRAW
		formatf(szText, "INVALID: COVER TOO CLOSE TO ANOTHER PED");
		CCoverDebug::AddDebugText(RCC_VEC3V(vCoverPos), Color_blue, szText, -1, CCoverDebug::ms_Tunables.m_TextOffset * iNoTexts++, iContext);
#endif // DEBUG_DRAW
		return false;
	}

	// Disallow taking cover on the static cover on cars we're stood on (B*1173036)
	if (rCover.GetEntity() && rCover.GetEntity()->GetIsTypeVehicle() && 
		static_cast<const CVehicle*>(rCover.GetEntity())->GetVehicleType() == VEHICLE_TYPE_CAR && 
		rPed.GetGroundPhysical() == rCover.GetEntity())
	{
#if DEBUG_DRAW
		formatf(szText, "INVALID: STOOD ON CAR");
		CCoverDebug::AddDebugText(RCC_VEC3V(vCoverPos), Color_blue, szText, -1, CCoverDebug::ms_Tunables.m_TextOffset * iNoTexts++, iContext);
#endif // DEBUG_DRAW
		return false;
	}

	// Rule out cover not within a certain angle of either the desired or camera heading
	bool bValidForDesiredHeading = true;
	bool bValidForCameraHeading = true;
	float fDesiredAngleToCoverDot = -1.0f;
	float fCameraAngleToCoverDot = -1.0f;

	const float fVeryCloseIgnoreDist = rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_IsAimingGun) ? sm_Tunables.m_VeryCloseIgnoreDesAndCamToleranceDistAimGun : sm_Tunables.m_VeryCloseIgnoreDesAndCamToleranceDist;
	if (fDistSqd > square(fVeryCloseIgnoreDist))
	{
		const float fDesiredHeading = rPed.GetDesiredHeading();
		Vector3 vDesiredDirection(0.0f, 1.0f, 0.0f);
		vDesiredDirection.RotateZ(fDesiredHeading);
		fDesiredAngleToCoverDot = vDesiredDirection.Dot(vPedToCover);
		if (fDesiredAngleToCoverDot < sm_Tunables.m_DesiredDirToCoverMinDot)
		{	
			bValidForDesiredHeading = false;
		}

		fCameraAngleToCoverDot = vCamFwd.Dot(vPedToCover);
		if (fCameraAngleToCoverDot < sm_Tunables.m_CameraDirToCoverMinDot)
		{	
			bValidForCameraHeading = false;
		}
	}

	if (!bValidForDesiredHeading && !bValidForCameraHeading && !bProvidesCoverFromThreat)
	{
#if DEBUG_DRAW
		formatf(szText, "INVALID: D(%.2f), DES(%.2f)/CAM(%.2f) DIR", sqrtf(fDistSqd), fDesiredAngleToCoverDot, fCameraAngleToCoverDot);
		CCoverDebug::AddDebugText(RCC_VEC3V(vCoverPos), Color_blue, szText, -1, CCoverDebug::ms_Tunables.m_TextOffset * iNoTexts++, iContext);
#endif // DEBUG_DRAW
		return false;
	}

	// If we're very close, invalid the cover we're on if moving away from its protection vector
	if (fDistSqd <= 1.0f)
	{
		const CControl *pControl = rPed.GetControlFromPlayer();
		Vector2 vecStick(pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm());
		vecStick *= 128.0f;	
		vecStick.Rotate(camInterface::GetHeading());
		Vector3 vCoverDir = vCoverDirection;
		vecStick.Rotate(-rage::Atan2f(-vCoverDir.x, vCoverDir.y));	
		const bool bNotValidForStickHeading = vecStick.y < CTaskInCover::ms_Tunables.m_InputYAxisQuitValue;
		if (bNotValidForStickHeading)
		{
#if DEBUG_DRAW
			formatf(szText, "INVALID: StickHeading(%.2f)", vecStick.y);
			CCoverDebug::AddDebugText(RCC_VEC3V(vCoverPos), Color_blue, szText, -1, CCoverDebug::ms_Tunables.m_TextOffset * iNoTexts++, iContext);
#endif // DEBUG_DRAW
			return false;
		}
	}

	const bool bIsVehicleCover = rCover.m_pEntity && rCover.m_pEntity->GetIsTypeVehicle();

	// Check to see if there is collision high enough for it to be used by the player
	// Dynamic tests should have already validated this so we only need to do this for static cover
	// These tests need to be simple as we could be potentially testing many cover points
	if (bIsStaticCover)
	{
		// Extend the LOS check to go beyond the cover position by the peds radius and a bit to make sure there is space.
		Vector3 vLosEndCheck = vCoverPos - vPedPos;

		//Check height delta
		static dev_float sf_MinAcceptableStaticHeightDelta = 1.5f;
		if (fabs(vLosEndCheck.z) > sf_MinAcceptableStaticHeightDelta)
		{
#if DEBUG_DRAW
			formatf(szText, "INVALID: height delta too great(%.2f)", fabs(vLosEndCheck.z));
			CCoverDebug::AddDebugText(RCC_VEC3V(vCoverPos), Color_blue, szText, -1, CCoverDebug::ms_Tunables.m_TextOffset * iNoTexts++, iContext);
#endif // DEBUG_DRAW
			return false;
		}

		if (!bIsVehicleCover)
		{
			float fLength = vLosEndCheck.Mag();
			vLosEndCheck.Normalize();
			vLosEndCheck.Scale(fLength);// + PED_RADIUS + 0.01f );
		}
		vLosEndCheck += vPedPos;

		// Do two probes one high, one low (can't do capsule test here since we could be testing loads of cover points)
		Vector3 vStart = vPedPos - Vector3(0.0f, 0.0f, sm_Tunables.m_StaticLosTest1Offset);
		Vector3 vEnd = vLosEndCheck - Vector3(0.0f, 0.0f, sm_Tunables.m_StaticLosTest1Offset);

		Vector3 vIntersectionPos;
		s32 iHitComponent;
		const phInst* pHitInst = CDynamicCoverHelper::DoLineProbeTest(vStart, vEnd, vIntersectionPos, &iHitComponent);

#if DEBUG_DRAW
		CCoverDebug::sDebugLineParams lineParams;
		lineParams.vStart = RCC_VEC3V(vStart);
		lineParams.vEnd = pHitInst ? RCC_VEC3V(vIntersectionPos) : RCC_VEC3V(vEnd);
		lineParams.color = pHitInst ? Color_black : Color_green;
		lineParams.uContextHash = CCoverDebug::STATIC_COVER_EVALUATION;
		CCoverDebug::AddDebugLine(lineParams);
#endif // DEBUG_DRAW

		if (!pHitInst)
		{
			vStart = vPedPos - Vector3(0.0f, 0.0f, sm_Tunables.m_StaticLosTest2Offset);
			vEnd = vLosEndCheck - Vector3(0.0f, 0.0f, sm_Tunables.m_StaticLosTest2Offset);
			pHitInst = CDynamicCoverHelper::DoLineProbeTest(vStart, vEnd, vIntersectionPos, &iHitComponent);

#if DEBUG_DRAW
			CCoverDebug::sDebugLineParams lineParams;
			lineParams.vStart = RCC_VEC3V(vStart);
			lineParams.vEnd = pHitInst ? RCC_VEC3V(vIntersectionPos) : RCC_VEC3V(vEnd);
			lineParams.color = pHitInst ? Color_black : Color_green;
			lineParams.uContextHash = CCoverDebug::STATIC_COVER_EVALUATION;
			CCoverDebug::AddDebugLine(lineParams);
#endif // DEBUG_DRAW

		}

		if (pHitInst)
		{
			bool bCoverValid = false;

			// Allowed to hit the object for Object types
			if (rCover.GetType() == CCoverPoint::COVTYPE_OBJECT)
			{
				const CEntity* pEntity = CPhysics::GetEntityFromInst(pHitInst);
				if (pEntity && pEntity == rCover.m_pEntity)
				{
					bCoverValid = true;
				}
			}
			// Allowed to hit foliage
			else if (CTaskCover::HitInstIsFoliage(*pHitInst, static_cast<u16>(iHitComponent)))
			{
				bCoverValid = true;
			}

			// Otherwise cover is invalid
			if (!bCoverValid)
			{
#if DEBUG_DRAW
				formatf(szText, "INVALID: OBSTRUCTED");
				CCoverDebug::AddDebugText(RCC_VEC3V(vCoverPos), Color_black, szText, -1, CCoverDebug::ms_Tunables.m_TextOffset * iNoTexts++, iContext);
#endif // DEBUG_DRAW
				return false;
			}
		}
		
		// Ignore the height condition for vehicles, assume its valid cover
		if (!bIsVehicleCover)
		{
			Vector3 vStart, vEnd;
			vStart = vCoverPos;
			vStart.z += sm_Tunables.m_CollisionLosHeightOffset;
			const Vector3 vCoverDirection = VEC3V_TO_VECTOR3(rCover.GetCoverDirectionVector());
			vEnd = vStart + (vCoverDirection*0.7f);
			const phInst* pHitInst = CDynamicCoverHelper::DoLineProbeTest(vStart, vEnd, vIntersectionPos);

#if DEBUG_DRAW
			CCoverDebug::sDebugLineParams lineParams;
			lineParams.vStart = RCC_VEC3V(vStart);
			lineParams.vEnd = pHitInst ? RCC_VEC3V(vIntersectionPos) : RCC_VEC3V(vEnd);
			lineParams.color = pHitInst ? Color_red : Color_brown;
			lineParams.uContextHash = CCoverDebug::STATIC_COVER_EVALUATION;
			CCoverDebug::AddDebugLine(lineParams);
#endif // DEBUG_DRAW

			if (!pHitInst)
			{
#if DEBUG_DRAW
				formatf(szText, "INVALID: NO COLLISION FOUND");
				CCoverDebug::AddDebugText(RCC_VEC3V(vCoverPos), Color_brown, szText, -1, CCoverDebug::ms_Tunables.m_TextOffset * iNoTexts++, iContext);
#endif // DEBUG_DRAW
				return false;
			}
		}
	}
	return true;
}

float CTaskPlayerOnFoot::ScoreCoverPoint(const Vector3& vPedPos, const Vector3& vCamFwd, const CPed& rPed, const CCoverPoint& rCover, bool bIgnoreValidityCheck, const Vector3& vDesiredCoverProtectionDir, bool bStickInputValid, bool UNUSED_PARAM(bHasThreat), s32* piNoTexts)
{
	if (!rPed.IsLocalPlayer())
		return CTaskCover::INVALID_COVER_SCORE;

	const bool bIsStaticCover = rPed.GetCoverPoint() != &rCover;
	float fScore = 0.0f;

	if (!bIgnoreValidityCheck)
	{
		if (!IsCoverValid(vPedPos, vCamFwd, rPed, rCover, bIsStaticCover, piNoTexts))
		{
			return CTaskCover::INVALID_COVER_SCORE;
		}
	}

	Vector3 vCoverPos;
	if (!rCover.GetCoverPointPosition(vCoverPos))
	{
		return fScore;
	}
	vCoverPos.z += 1.0f;

#if DEBUG_DRAW
	char szText[128];
	Color32 color = bIsStaticCover ? Color_red : Color_orange;
	s32 iLocalNoTexts = (bIsStaticCover && !bIgnoreValidityCheck) ? 0 : 1;
	s32& iNoTexts = piNoTexts ? *piNoTexts : iLocalNoTexts;
	const s32 iContext = bIsStaticCover ? bIgnoreValidityCheck ? CCoverDebug::INITIAL_SEARCH_SIMPLE : CCoverDebug::STATIC_COVER_EVALUATION : CCoverDebug::INITIAL_SEARCH_SIMPLE;
	if (bIsStaticCover)
	{
		formatf(szText, "%p SCORING (Actual/Max)", &rCover);
		CCoverDebug::AddDebugText(RCC_VEC3V(vCoverPos), color, szText, -1, CCoverDebug::ms_Tunables.m_TextOffset * iNoTexts++, iContext);
	}
#endif // DEBUG_DRAW

	// Scripted PRIORITY bonus
	if (rCover.GetFlag(CCoverPoint::COVFLAGS_IsPriority))
	{
		fScore += sm_Tunables.m_PriorityCoverWeight;
#if DEBUG_DRAW
		formatf(szText, "%-10s (%.2f/%.2f)","PRIORITY", sm_Tunables.m_PriorityCoverWeight, sm_Tunables.m_PriorityCoverWeight);
		CCoverDebug::AddDebugText(RCC_VEC3V(vCoverPos), color, szText, -1, CCoverDebug::ms_Tunables.m_TextOffset * iNoTexts++, iContext);
#endif // DEBUG_DRAW
	}

	// Removed for B*1399215
// 	// Capsule object weighting
// 	if (rCover.m_pEntity && rCover.m_pEntity->GetIsTypeObject())
// 	{
// 		const phInst* pInst = rCover.m_pEntity->GetCurrentPhysicsInst();
// 		if (pInst)
// 		{
// 			const phBound* pBound = pInst->GetArchetype()->GetBound();
// 			if (pBound)
// 			{
// 				float fRadius = -1.0f;
// 					 
// 				if (pBound->GetType()==phBound::CAPSULE)
// 				{
// 					fRadius = pBound->GetRadiusAroundCentroid();
// 				}
// 				else if (pBound->GetType()==phBound::COMPOSITE)
// 				{
// 					// Find the first capsule or cylinder bound and use that radius (a bit dodgy for objects that comprise of multiple of these I guess)
// 					const phBoundComposite* pBoundComposite = static_cast<const phBoundComposite*>(pBound);
// 					const s32 iNumBounds = pBoundComposite->GetNumBounds();
// 					for (s32 i=0; i<iNumBounds; ++i)
// 					{
// 						const phBound* pTestBound = pBoundComposite->GetBound(i);
// 						if (pTestBound)
// 						{
// 							if (pTestBound->GetType()==phBound::CAPSULE) 
// 							{								
// 								const phBoundCapsule* pBndCapsule = (const phBoundCapsule*)pTestBound;
// 								fRadius = pBndCapsule->GetRadius();
// 								break;
// 							}
// 							else if (pTestBound->GetType()==phBound::CYLINDER)
// 							{
// 								const phBoundCylinder* pBndCylinder = (const phBoundCylinder*)pTestBound;
// 								fRadius = pBndCylinder->GetRadius();
// 								break;
// 							}
// 						}
// 					}
// 				}
// 
// 				if (fRadius > 0.0f && fRadius < sm_Tunables.m_SmallCapsuleCoverRadius)
// 				{
// 					fScore -= sm_Tunables.m_SmallCapsuleCoverPenalty;
// #if DEBUG_DRAW
// 					formatf(szText, "%-10s (%.2f/%.2f)","-CAPSULE", sm_Tunables.m_SmallCapsuleCoverPenalty, sm_Tunables.m_SmallCapsuleCoverPenalty);
// 					CCoverDebug::AddDebugText(RCC_VEC3V(vCoverPos), color, szText, -1, CCoverDebug::ms_Tunables.m_TextOffset * iNoTexts++, iContext);
// #endif // DEBUG_DRAW
// 				}
// 			}
// 		}
// 	}

	const Vector3 vCoverDirection = VEC3V_TO_VECTOR3(rCover.GetCoverDirectionVector());
	Vector3 vPedToCover = vCoverPos - vPedPos;
	vPedToCover.Normalize();

	// EDGE bonus
	// Prefer edge cover if its high
 	const bool bIsEdgeCover = rCover.IsEdgeOrDoubleEdgeCoverPoint();
	const bool bIsLowCover = rCover.GetHeight() != CCoverPoint::COVHEIGHT_TOOHIGH;
	if (bIsEdgeCover)
	{
		bool bPreferEdgeCover = false;

		if (!bIsLowCover)
		{
			const bool bIsLeftEdge = rCover.GetUsage() == CCoverPoint::COVUSE_WALLTORIGHT;
			const bool bIsRightEdge = rCover.GetUsage() == CCoverPoint::COVUSE_WALLTOLEFT;
			if (bIsLeftEdge || bIsRightEdge)
			{
				bPreferEdgeCover = true;
			}
		}
		
		if (bPreferEdgeCover)
		{
			fScore += sm_Tunables.m_EdgeCoverWeight;
#if DEBUG_DRAW
			formatf(szText, "%-10s (%.2f/%.2f)","EDGE", sm_Tunables.m_EdgeCoverWeight, sm_Tunables.m_EdgeCoverWeight);
			CCoverDebug::AddDebugText(RCC_VEC3V(vCoverPos), color, szText, -1, CCoverDebug::ms_Tunables.m_TextOffset * iNoTexts++, iContext);
#endif // DEBUG_DRAW
		}
	}

	// VERY CLOSE TO COVER bonus
	bool bHasThreat = false;
	const float fMaxVeryCloseDistSqd = square(sm_Tunables.m_VeryCloseToCoverDist);
	const float fPedToCoverDistSqd = vPedPos.Dist2(vCoverPos);
	const bool bVeryCloseToCover = fPedToCoverDistSqd < fMaxVeryCloseDistSqd;
	if (bVeryCloseToCover)
	{
		const float fVeryCloseScale = 1.0f - Clamp(fPedToCoverDistSqd / fMaxVeryCloseDistSqd, 0.0f, 1.0f);
		const float fVeryCloseDistScore = fVeryCloseScale * sm_Tunables.m_VeryCloseToCoverWeight;
		fScore += fVeryCloseDistScore;
#if DEBUG_DRAW
		formatf(szText, "%-10s (%.2f/%.2f)","CLOSE DIST", fVeryCloseDistScore, sm_Tunables.m_VeryCloseToCoverWeight);
		CCoverDebug::AddDebugText(RCC_VEC3V(vCoverPos), color, szText, -1, CCoverDebug::ms_Tunables.m_TextOffset * iNoTexts++, iContext);
#endif // DEBUG_DRAW
	}

	// THREAT WEIGHTING bonus
	const bool bUseThreatWeight = sm_Tunables.GetUseThreatWeighting(rPed);
	if (bUseThreatWeight || rPed.GetPlayerResetFlag(CPlayerResetFlags::PRF_USE_COVER_THREAT_WEIGHTING))
	{
		Vector3 vThreatDir = vDesiredCoverProtectionDir;
		if (sm_Tunables.m_EvaluateThreatFromCoverPoints)
		{
			// Compute the desired protection direction from this cover's position
			if (CTaskCover::ComputeDesiredProtectionDirection(rPed, vCoverPos, vThreatDir))
			{
				bHasThreat = true;
#if DEBUG_DRAW
				CCoverDebug::AddDebugPositionSphereAndDirection(RCC_VEC3V(vCoverPos), RCC_VEC3V(vThreatDir), Color_yellow, "THREAT", CCoverDebug::ms_Tunables.m_TextOffset * -1, iContext, false);
#endif // DEBUG_DRAW
			}
		}

		if (bHasThreat)
		{
			const float fThreatWeightingScale = Clamp(vCoverDirection.Dot(vThreatDir), 0.0f, 1.0f);
			const float fThreatProtectionScore = fThreatWeightingScale * sm_Tunables.m_ThreatDirWeight;
			fScore += fThreatProtectionScore;
#if DEBUG_DRAW
			formatf(szText, "%-10s (%.2f/%.2f)", "THREAT", fThreatProtectionScore, sm_Tunables.m_ThreatDirWeight);
			CCoverDebug::AddDebugText(RCC_VEC3V(vCoverPos), color, szText, -1, CCoverDebug::ms_Tunables.m_TextOffset * iNoTexts++, iContext);
#endif // DEBUG_DRAW

			if (!bIsLowCover && bIsEdgeCover && !rPed.GetPedResetFlag(CPED_RESET_FLAG_IgnoreThreatEngagePlayerCoverBonus))
			{
				const bool bThreatLeftOfCover = vCoverDirection.CrossZ(vThreatDir) > 0.0f ? true : false;
				const bool bIsLeftEdgeCover = rCover.GetUsage() == CCoverPoint::COVUSE_WALLTORIGHT;
				if ((bThreatLeftOfCover && bIsLeftEdgeCover) || (!bThreatLeftOfCover && !bIsLeftEdgeCover))
				{
					const float fThreatEngageScore = sm_Tunables.m_ThreatEngageDirWeight;
					fScore += fThreatEngageScore;
#if DEBUG_DRAW
					formatf(szText, "%-10s (%.2f/%.2f)", "ENGAGE", fThreatEngageScore, sm_Tunables.m_ThreatEngageDirWeight);
					CCoverDebug::AddDebugText(RCC_VEC3V(vCoverPos), color, szText, -1, CCoverDebug::ms_Tunables.m_TextOffset * iNoTexts++, iContext);
#endif // DEBUG_DRAW
				}
			}
		}
	}

	// DIST TO COVER bonus
	const float fMaxDistSqd = square(CTaskEnterCover::ms_Tunables.m_CoverEntryMaxDistance);
	const float fDistToCoverScale = 1.0f - Clamp(fPedToCoverDistSqd / fMaxDistSqd, 0.0f, 1.0f);
	float fDistToCoverWeight = bHasThreat ? sm_Tunables.m_DistToCoverWeightThreat : sm_Tunables.m_DistToCoverWeight;
	if (!bStickInputValid)
	{
		fDistToCoverWeight += sm_Tunables.m_DistToCoverWeightNoStickBonus;
	}
	const float fDistScore = fDistToCoverScale * fDistToCoverWeight;
	fScore += fDistScore;

#if DEBUG_DRAW
	formatf(szText, "%-10s (%.2f/%.2f)","DIST", fDistScore, fDistToCoverWeight + sm_Tunables.m_DistToCoverWeightNoStickBonus);
	CCoverDebug::AddDebugText(RCC_VEC3V(vCoverPos), color, szText, -1, CCoverDebug::ms_Tunables.m_TextOffset * iNoTexts++, iContext);
#endif // DEBUG_DRAW

	const float fDesiredHeading = rPed.GetDesiredHeading();
	Vector3 vDesiredDirection(0.0f, 1.0f, 0.0f);
	vDesiredDirection.RotateZ(fDesiredHeading);

	// If not pushing the stick, take the direction the camera is pointing as an indication of the cover we want to be at
	if (!bStickInputValid)
	{
		vDesiredDirection = vCamFwd;
	}
	else
	{
		TUNE_GROUP_BOOL(COVER_TUNE, USE_CAMERA_HEADING_IF_COVER_FAR_AND_ACUTE, true);
		if (USE_CAMERA_HEADING_IF_COVER_FAR_AND_ACUTE)
		{
			TUNE_GROUP_FLOAT(COVER_TUNE, FAR_FROM_COVER, 4.0f, 0.0f, 10.0f, 0.01f);
			if (fMaxDistSqd > FAR_FROM_COVER)
			{
				TUNE_GROUP_FLOAT(COVER_TUNE, ANGLE_FROM_CAM, 1.0f, 0.0f, 3.0f, 0.01f);
				const float fCamHeading = camInterface::GetHeading();
				const float fHeadingDelta = fwAngle::LimitRadianAngle(fCamHeading - fDesiredHeading);
				if (fHeadingDelta > ANGLE_FROM_CAM)
				{
					vDesiredDirection = vCamFwd;
				}
			}
		}
	}

	const bool bAimingGun = rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_IsAimingGun);

	// DESIRED HEADING TO COVER ANGLE bonus
	// When aiming we want to prefer cover in the direction of the camera, so weigh the dir to cover less when aiming
	float fDesiredDirToCoverMaxWeight = bAimingGun ? sm_Tunables.m_DesiredDirToCoverAimingWeight : sm_Tunables.m_DesiredDirToCoverWeight;
	const float fDesiredAngleToCoverScale = Clamp(vDesiredDirection.Dot(vPedToCover), 0.0f, 1.0f);
	const float fDesiredAngleScore = fDesiredAngleToCoverScale * fDesiredDirToCoverMaxWeight;
	fScore += fDesiredAngleScore;

#if DEBUG_DRAW
	if (!bStickInputValid)
	{
		formatf(szText, "%-10s (%.2f/%.2f)","NOSTICK", fDesiredAngleScore, fDesiredDirToCoverMaxWeight);
	}
	else
	{
		formatf(szText, "%-10s (%.2f/%.2f)","DES ANG", fDesiredAngleScore, fDesiredDirToCoverMaxWeight);
	}
	CCoverDebug::AddDebugText(RCC_VEC3V(vCoverPos), color, szText, -1, CCoverDebug::ms_Tunables.m_TextOffset * iNoTexts++, iContext);
#endif // DEBUG_DRAW


	// Scale the max desired angle score down based on distance so if we're close to cover
	// this has less of an effect, don't do this when aiming and close to the cover
	const float fMaxCoverDirToCamWeight = bAimingGun && bVeryCloseToCover ? sm_Tunables.m_CoverDirToCameraWeightMaxAimGun : sm_Tunables.m_CoverDirToCameraWeightMax;
	const float fCoverDirMaxDistSqd = square(sm_Tunables.m_CoverDirToCameraWeightMaxScaleDist);
	const float fCoverDirToCameraWeightDistScale = bAimingGun ? 1.0f : Clamp(fPedToCoverDistSqd / fCoverDirMaxDistSqd, 0.0f, 1.0f);
	const float fTotalCoverDirToCameraWeight = (1.0f - fCoverDirToCameraWeightDistScale) * sm_Tunables.m_CoverDirToCameraWeightMin + fCoverDirToCameraWeightDistScale * fMaxCoverDirToCamWeight;

	const float fDesiredCoverDirToCameraScale = Clamp(vCamFwd.Dot(vCoverDirection), 0.0f, 1.0f);
	const float fDesiredCoverDirScore = fDesiredCoverDirToCameraScale * fTotalCoverDirToCameraWeight;
	fScore += fDesiredCoverDirScore;
	float fHighestScore = sm_Tunables.m_PriorityCoverWeight + sm_Tunables.m_EdgeCoverWeight + sm_Tunables.m_VeryCloseToCoverWeight + sm_Tunables.m_DistToCoverWeightNoStickBonus + fDistToCoverWeight + fDesiredDirToCoverMaxWeight + fMaxCoverDirToCamWeight;

	if (sm_Tunables.GetUseThreatWeighting(rPed))
	{
		fHighestScore += sm_Tunables.m_ThreatDirWeight;
		fHighestScore += sm_Tunables.m_ThreatEngageDirWeight;
	}

	// CAMERA HEADING TO COVER ANGLE bonus
	if (bHasThreat || bStickInputValid)
	{
#if DEBUG_DRAW
		formatf(szText, "%-10s (%.2f/%.2f)","CAM ANG", fDesiredCoverDirScore, fMaxCoverDirToCamWeight);
		CCoverDebug::AddDebugText(RCC_VEC3V(vCoverPos), color, szText, -1, CCoverDebug::ms_Tunables.m_TextOffset * iNoTexts++, iContext);
		formatf(szText, "%-8s (%.2f/%.2f)","TOTAL", fScore, fHighestScore);
		CCoverDebug::AddDebugText(RCC_VEC3V(vCoverPos), color, szText, -1, CCoverDebug::ms_Tunables.m_TextOffset * iNoTexts++, iContext);
#endif // DEBUG_DRAW
	}

	fScore = 1.0f - Clamp(fScore / fHighestScore, 0.0f, 1.0f);
#if DEBUG_DRAW
	formatf(szText, "NORM SCORE (Lower is better) : %.2f", fScore);
	Color32 iColor = Color32(0.0f, 1.0f - fScore, 0.0f);
	CCoverDebug::AddDebugText(RCC_VEC3V(vCoverPos), iColor, szText, -1, CCoverDebug::ms_Tunables.m_TextOffset * iNoTexts++, iContext);
#endif // DEBUG_DRAW

	return fScore;
}

#if DEBUG_DRAW
void CTaskPlayerOnFoot::DrawInitialCoverDebug(const CPed& rPed, s32& iNumPlayerTexts, s32 iTextOffset, const Vector3& vDesiredCoverProtectionDir)
{
	if (CCoverDebug::ms_Tunables.m_EnableNewCoverDebugRendering)
	{
		if (!CCoverDebug::ms_bInitializedDebugDrawStores)
		{
			CCoverDebug::ms_Tunables.m_DefaultCoverDebugContext.Init();
			CCoverDebug::ms_bInitializedDebugDrawStores = true;
		}
	}

	const Vec3V vPlayerPos = rPed.GetTransform().GetPosition();
	const Vec3V vPlayerDir = rPed.GetTransform().GetB();
	const Vec3V vDesiredDir = RotateAboutZAxis(Vec3V(V_Y_AXIS_WZERO), ScalarVFromF32(rPed.GetDesiredHeading()));
	const Vec3V vCameraPos = CCoverDebug::GetGamePlayCamPosition();
	const Vec3V vCameraDir = CCoverDebug::GetGamePlayCamDirection();

	CCoverDebug::AddDebugPositionSphereAndDirection(vCameraPos, vCameraDir, Color_blue, "CAMERA", 0, CCoverDebug::INITIAL_SEARCH_SIMPLE);
	CCoverDebug::AddDebugPositionSphereAndDirection(vPlayerPos, RCC_VEC3V(vDesiredCoverProtectionDir), Color_red, "THREAT", iTextOffset * iNumPlayerTexts++, CCoverDebug::INITIAL_SEARCH_SIMPLE);
	CCoverDebug::AddDebugPositionSphereAndDirection(vPlayerPos, vPlayerDir, Color_green, "PLAYER", iTextOffset * iNumPlayerTexts++, CCoverDebug::INITIAL_SEARCH_SIMPLE);

	CCoverDebug::sDebugArrowParams arrowParams;
	arrowParams.vPos = vPlayerPos;
	arrowParams.vDir = vDesiredDir;
	arrowParams.color = Color_yellow;
	arrowParams.szName = "DESIRED HEADING";
	arrowParams.bAddText = true;
	arrowParams.iYTextOffset = iTextOffset * iNumPlayerTexts++;
	arrowParams.uContextHash = CCoverDebug::INITIAL_SEARCH_SIMPLE;
	CCoverDebug::AddDebugDirectionArrow(arrowParams);
}
#endif // DEBUG_DRAW

#if FPS_MODE_SUPPORTED
bool CTaskPlayerOnFoot::UseAnalogRun(const CPed* pPlayerPed) const
{
	TUNE_GROUP_BOOL(FIRST_PERSON_TUNE, DisableAnalogStickRun, false);
	if(DisableAnalogStickRun)
	{
		return false;
	}

	if(!pPlayerPed->GetMotionData()->GetUsingStealth() && pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false, false, true))
	{
		const CControl* pControl = pPlayerPed->GetControlFromPlayer();
		bool bSprintDisabled = pControl && pControl->IsInputDisabled(INPUT_SPRINT) && !CPlayerSpecialAbilityManager::HasSpecialAbilityDisabledSprint();
		if( !bSprintDisabled &&
			(sm_Tunables.m_AllowFPSAnalogStickRunInInteriors || !pPlayerPed->GetIsInInterior() || pPlayerPed->GetPortalTracker()->IsAllowedToRunInInterior() || pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IgnoreInteriorCheckForSprinting)))
		{
			return true;
		}
	}
	return false;
}

void CTaskPlayerOnFoot::ProcessViewModeSwitch(CPed* pPlayerPed)
{
	bool bSwitchedIntoFirstPersonMode = false;
	bool bIsInFirstPersonMode = pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false, false, true);
	TUNE_GROUP_BOOL(FIRST_PERSON_VIEW_MODE_TUNE, DONT_SWITCH_ON_FIRST_FRAME, true);
	if (bIsInFirstPersonMode && !m_WasInFirstPerson && ((DONT_SWITCH_ON_FIRST_FRAME && GetState() != STATE_INITIAL) || pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_SwitchingCharactersInFirstPerson)))
	{
		bSwitchedIntoFirstPersonMode = true;
		if (pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_SwitchingCharactersInFirstPerson))
		{
			pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_SwitchingCharactersInFirstPerson, false);
		}

		TUNE_GROUP_BOOL(FIRST_PERSON_VIEW_MODE_TUNE, FORCE_INSTANT_IK_BLEND_IN_ON_SWITCH, true);
		if (FORCE_INSTANT_IK_BLEND_IN_ON_SWITCH)
		{
			m_bForceInstantIkBlendInThisFrame = true;
		}
	}
	m_WasInFirstPerson = bIsInFirstPersonMode;

	if((pPlayerPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_CAMERA_VIEW_MODE_SWITCHED_TO_OR_FROM_FIRST_PERSON) || bSwitchedIntoFirstPersonMode) && !pPlayerPed->GetIsSwimming())
	{
		if(!pPlayerPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_CAMERA_VIEW_MODE_SWITCHED_TO_FIRST_PERSON) && !bSwitchedIntoFirstPersonMode)
		{
			if(!pPlayerPed->GetMotionData()->GetIsFPSIdle() && pPlayerPed->GetEquippedWeaponInfo() && !pPlayerPed->GetEquippedWeaponInfo()->GetIsUnarmed() && !pPlayerPed->GetEquippedWeaponInfo()->GetIsMeleeFist())
			{
				pPlayerPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Aiming, true);
				pPlayerPed->SetPedResetFlag(CPED_RESET_FLAG_InstantBlendToAimNoSettle, true);
			}
			else
			{
				float fMBR = pPlayerPed->GetMotionData()->GetCurrentMoveBlendRatio().Mag();

				Vector3 vDesiredMBR(pPlayerPed->GetMotionData()->GetDesiredMoveBlendRatio().x, pPlayerPed->GetMotionData()->GetDesiredMoveBlendRatio().y, 0.f);
				if(vDesiredMBR.Mag2() > 0.01f && !pPlayerPed->GetUsingRagdoll())
				{
					vDesiredMBR.Normalize();
					float fDesiredHeading = rage::Atan2f(-vDesiredMBR.x, vDesiredMBR.y);
					fDesiredHeading = fDesiredHeading + pPlayerPed->GetCurrentHeading();
					fDesiredHeading = fwAngle::LimitRadianAngle(fDesiredHeading);
					// Set the heading to the direction of motion
					pPlayerPed->SetHeading(fDesiredHeading);
				}

				if(pPlayerPed->GetMotionData()->GetUsingStealth())
				{
					if(!pPlayerPed->GetMotionData()->GetIsStill(fMBR))
					{
						pPlayerPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Stealth_Walk, true);
					}
					else
					{
						pPlayerPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Stealth_Idle, true);
					}
				}
				else if(pPlayerPed->WantsToUseActionMode())
				{
					if(!pPlayerPed->IsUsingActionMode())
					{
						pPlayerPed->UpdateMovementMode();
					}

					if(!pPlayerPed->GetMotionData()->GetIsStill(fMBR))
					{
						pPlayerPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_ActionMode_Walk, true);
					}
					else
					{
						pPlayerPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_ActionMode_Idle, true);
					}
				}
				else
				{
					if(pPlayerPed->GetMotionData()->GetIsSprinting(fMBR))
					{
						pPlayerPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Sprint, true);
					}
					else if(!pPlayerPed->GetMotionData()->GetIsStill(fMBR))
					{
						pPlayerPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Walk, true);
					}
					else
					{
						pPlayerPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Idle, true);
					}
				}
			}
		}
		else if (!pPlayerPed->GetIsInCover() && (pPlayerPed->GetAttachParent()==NULL))
		{
			// When going into FPS when climbing a ladder we want to keep the heading of the
			// ped as otherwise we would end up rotating the ped away from the ladder
			const CTask* pClimbLadderTask = pPlayerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_CLIMB_LADDER);
			if( pClimbLadderTask == NULL )
			{
				float fCamOrient = fwAngle::LimitRadianAngle(camInterface::GetPlayerControlCamHeading(*pPlayerPed));
				if (!pPlayerPed->GetUsingRagdoll())
				{
					pPlayerPed->SetHeading(fCamOrient);
				}

				if(!pPlayerPed->GetMotionData()->GetIsFPSIdle())
				{
					pPlayerPed->SetPedResetFlag(CPED_RESET_FLAG_InstantBlendToAimNoSettle, true);
				}

				pPlayerPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Aiming, true);

				// Update anims before camera, so the camera can be positioned correctly, based on the new ped pose
				pPlayerPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePreCameraAnimUpdate, true);
			}
		}

		pPlayerPed->SetClothForcePin(1);

		// If we are wanting to aim, ensure the gun task is also restarted
		if(pPlayerPed->GetMotionData()->GetForcedMotionStateThisFrame() == CPedMotionStates::MotionState_Aiming)
		{
			CTaskAimGunOnFoot* pAimGunTask = static_cast<CTaskAimGunOnFoot*>(pPlayerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_ON_FOOT));
			if(pAimGunTask)
			{
				pAimGunTask->SetForceRestart(true);
				pPlayerPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
				pPlayerPed->GetIkManager().SetTorsoYawDeltaLimits(PI, 100.f, 0.f);
			}
		}
	}
}
#endif // FPS_MODE_SUPPORTED

CTask::FSM_Return CTaskPlayerOnFoot::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

#if FPS_MODE_SUPPORTED
	m_bForceInstantIkBlendInThisFrame  = false;
#endif // FPS_MODE_SUPPORTED

	FPS_MODE_SUPPORTED_ONLY(ProcessViewModeSwitch(pPed));

	// Flags
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ForceProcessPedStandingUpdateEachSimStep, true);	// Make sure to do ProcessPedStanding() each physics update step, probably safest.
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPreRender2, true);
	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_InWaterTaskQuitToClimbLadder, false );

	Assert(pPed->IsPlayer());
	CControl *pControl = pPed->GetControlFromPlayer();

#if FPS_MODE_SUPPORTED
	// Set flag to transition ped to swimming/diving motion tasks if left stick is pushed forwards (as opposed to aiming which is used for underwater strafing)

	if(pPed->GetMotionData() && pPed->GetMotionData()->GetCombatRoll() && pControl && HasValidEnterVehicleInput(*pControl))
	{
		m_bGetInVehicleAfterCombatRolling = true;
	}

	if (pPed->GetIsSwimming() && pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		if (pControl)
		{
			Vector2 vStickInput(pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm());

			static dev_float fDiveStrafeThresholdX = 0.75f;
			static dev_float fDiveStrafeThresholdY = 0.0f;

			// Force swim task if we want to enter a vehicle
			bool bIsEnteringVehicle = (GetState() == STATE_GET_IN_VEHICLE);

			// B*2038392: Keep ped in the swim/dive task if sprint button is down 
			bool bSprintButtonDown = pControl->GetPedSprintIsDown();
			//B*2065912: Stay in swim motion task if we're still sprinting (despite letting go of sprint button), unless we're trying to strafe
			bool bIsSprinting = pPed->GetMotionData()->GetIsDesiredSprinting(pPed->GetMotionData()->GetFPSCheckRunInsteadOfSprint());

			// If player tries to run/sprint, reset the swim-sprint timer that is used to keep us in the swim task when script clamp the max MBR
			if (bSprintButtonDown || bIsSprinting)
			{
				m_fTimeSinceLastSwimSprinted = 0.0f;
			}

			// Script have clamped the max MBR, so use a timer from when sprint button was last pressed to keep us in the swim motion task
			bool bKeepSwimming = false;
			if(pPed->GetMotionData()->GetScriptedMaxMoveBlendRatio() < MOVEBLENDRATIO_SPRINT)
			{
				static dev_float fStayInSwimTimer = 1.0f;
				if (m_fTimeSinceLastSwimSprinted < fStayInSwimTimer)
				{
					bKeepSwimming = true;
					m_fTimeSinceLastSwimSprinted += fwTimer::GetTimeStep();
				}
			}

			// If left stick is within the dive threshold, set the flag to force us into TaskMotionDiving, else use TaskMotionAiming for swim-strafing
			bool PushingForwardsToSwim = (Abs(vStickInput.x) < fDiveStrafeThresholdX && vStickInput.y > fDiveStrafeThresholdY);

			// B*2184371: Fix for jittering between strafe/swim tasks due to low energy causing desired MBR to drop to (0,2).
			// Remain in swim state if we recently had the sprint button down and our sprint bar is replenishing.
			static dev_u32 fSprintWasDownTime = 150;
			bool bTryingToSprintButNoEnergy = !bSprintButtonDown && !bIsSprinting && pPed->GetPlayerInfo() && pPed->GetPlayerInfo()->GetPlayerDataSprintReplenishing() && pControl->GetPedSprintHistoryHeldDown(fSprintWasDownTime);

			if (PushingForwardsToSwim || bIsEnteringVehicle || bSprintButtonDown || bIsSprinting || bKeepSwimming || bTryingToSprintButNoEnergy)
			{
				pPed->SetPedResetFlag(CPED_RESET_FLAG_FPSSwimUseSwimMotionTask, true);
			}
			else
			{
				pPed->SetPedResetFlag(CPED_RESET_FLAG_FPSSwimUseAimingMotionTask, true);
				// Force into strafing 
				pPed->SetIsStrafing(true);
			}
		}
	}

	// B*2042421: Creating the mobile phone is delayed by a frame so the "SetIsStrafing" call in CTaskMobilePhone::ProcessPreFSM is delayed by 1 frame 
	// so we were swapping to CTaskMotionPed::State_OnFoot causing us to spin around before switching back to strafing again. This is due to the code that calls
	// CreateOrResumeMobilePhoneTask in CTaskPlayerIdles::MovementAndAmbientClips_OnUpdate waiting for the sub-tasks sub-task to be created! Too scary to change at this point.
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && CTaskPlayerOnFoot::CheckForUseMobilePhone(*pPed) && GetState() != STATE_MELEE)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePedToStrafe, true);
		pPed->SetPedResetFlag(CPED_RESET_FLAG_IsAiming, true);
	}

	// B*2276638: Block weapon switching and cover entry when aiming homing launcher. These inputs are used for swapping homing targets.
	if (pPed->GetPlayerInfo() && pPed->GetEquippedWeaponInfo() && pPed->GetEquippedWeaponInfo()->GetIsOnFootHoming() && pPed->GetPlayerInfo()->IsHardAiming() &&
		pPed->GetPedIntelligence() && pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_AIM_GUN_ON_FOOT))
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching, true);
		pPed->SetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_CAN_USE_COVER);
	}

	// B*2047112: Do a weapon blocked check (as this is only done from TASK_AIM_GUN_ON_FOOT which is not run when sprinting). 
	// If blocked, set CPED_RESET_FLAG_WeaponBlockedInFPSMode which will force us into the gun state to use the blocked anims.
	if (GetState() != STATE_PLAYER_GUN && pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		Vector3 vTargetPos(Vector3::ZeroType);
		CWeapon* pEquippedWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		if (pEquippedWeapon)
		{
			const CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
			if (pWeaponObject)
			{
				const Matrix34& mWeapon = RCC_MATRIX34(pWeaponObject->GetMatrixRef());
				Vector3 vStart( Vector3::ZeroType );
				pEquippedWeapon->CalcFireVecFromAimCamera( pPed, mWeapon, vStart, vTargetPos );

				static dev_float fProbeLength = 1.0f;
				static dev_float fProbeLengthMult = 1.0f;
				if(CTaskWeaponBlocked::IsWeaponBlocked(pPed, vTargetPos, NULL, fProbeLengthMult, fProbeLength))
				{
					pPed->SetPedResetFlag(CPED_RESET_FLAG_WeaponBlockedInFPSMode, true);
				}
			}
		}
	}

	if (!pPed->GetPedResetFlag(CPED_RESET_FLAG_WeaponBlockedInFPSMode) && pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_WEAPON_BLOCKED))
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_WeaponBlockedInFPSMode, true);
	}
#endif	//FPS_MODE_SUPPORTED

	// Update the lockon status (Switching targets etc...)
	UpdatePlayerLockon( pPed );

	// Keeps a history of the last recored stick input
	UpdateMovementStickHistory(pControl);

	ProcessPlayerEvents();

	if(pPed->IsLocalPlayer())
	{
		// CNC: Cop players should enter action mode when they spot a crook (within range).
		if (CNC_PlayerCopShouldTriggerActionMode(pPed))
		{
			TUNE_GROUP_FLOAT(CNC_RESPONSIVENESS, fCopTimeToStayInActionModeAfterSeeingCrook, 10.0f, 0.0f, 60.0f, 1);
			pPed->SetUsingActionMode(true, CPed::AME_CopSeenCrookCNC, fCopTimeToStayInActionModeAfterSeeingCrook);
		}

		const CWanted &Wanted = pPed->GetPlayerInfo()->GetWanted();

		bool bSetActionModeDueToWanted = false;
		if(Wanted.GetWantedLevel() >= WANTED_LEVEL1)
		{
			if(!Wanted.CopsAreSearching() && (Wanted.m_iTimeFirstSpotted>0) )
			{
				m_b1stTimeSeenWhenWanted = true;
			}

			// CNC: Always go into action mode when wanted.
			if (CPlayerInfo::AreCNCResponsivenessChangesEnabled(pPed))
			{
				bSetActionModeDueToWanted = true;
			}
		}
		else
		{
			m_b1stTimeSeenWhenWanted = false;
		}

		pPed->SetUsingActionMode(m_b1stTimeSeenWhenWanted || bSetActionModeDueToWanted, CPed::AME_Wanted);
		
		if(pPed->GetPedIntelligence()->IsBattleAware())
		{
			float fMinTimer = CPedIntelligence::BATTLE_AWARENESS_MIN_TIME;
			pPed->SetUsingActionMode(true, CPed::AME_Combat, fMinTimer, pPed->GetPedIntelligence()->IsBattleAwareForcingRun()); 
		}
	}

	if(NetworkInterface::IsInCopsAndCrooks())
	{
		bool bStopArrest = false;

		//Arrest timers
		if(m_bArrestTimerActive)
		{
			if(CNetwork::GetNetworkTime() > m_uArrestSprintBreakoutTime)
			{
				if(pPed->GetControlFromPlayer()->GetPedSprintIsDown())
				{
					bStopArrest = true;
					m_bArrestTimerActive = false;
				}
			}

			if(CNetwork::GetNetworkTime() > m_uArrestFinishTime)
			{
				bStopArrest = true;
				m_bArrestTimerActive = false;
			}
			else if (CNetwork::GetNetworkTime() < m_uArrestFinishTime)
			{
				//Block a bunch of things we don't want to happen while arresting
				pPed->SetPedResetFlag(CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching, true);
				pPed->SetPedResetFlag(CPED_RESET_FLAG_DisablePlayerJumping, true);
				pPed->SetPedResetFlag(CPED_RESET_FLAG_DisablePlayerVaulting, true);
				pPed->SetPedResetFlag(CPED_RESET_FLAG_DisablePlayerCombatRoll, true);

				if(m_pTargetPed)
				{
					if(CPlayerInfo *pPlayerInfo = pPed->GetPlayerInfo())
					{
						// Set cop to lockon to crook
						pPlayerInfo->GetTargeting().SetLockOnTarget(m_pTargetPed);
					}

					if(!m_pTargetPed->IsVisible())
					{
						bStopArrest = true;
					}
				}
			}
		}

		if(bStopArrest)
		{
			if (CPlayerInfo *pPlayerInfo = pPed->GetPlayerInfo())
			{
				// Remove locked target
				pPlayerInfo->GetTargeting().SetLockOnTarget(nullptr);
			}

			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ForcedAimFromArrest, false);

			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PedIsArresting, false);

			m_pTargetPed = nullptr;

			CPedWeaponManager* pWeaponManager = GetPed()->GetWeaponManager();
			if (pWeaponManager )
			{
				if(pWeaponManager->GetBackupWeapon() != 0)
				{
					pWeaponManager->RestoreBackupWeapon();
				}
			}
		}
	}


	// m_pTargetVehicle is explicitly NULLed before reuse so the target vehicle is available after abort so we can resume
	m_pTargetAnimal = NULL;
	//Ladder
	Vector2 vDesiredMoveRatio;
	pPed->GetMotionData()->GetDesiredMoveBlendRatio(vDesiredMoveRatio);

	if (!m_pLadderClipRequestHelper)
		m_pLadderClipRequestHelper = CLadderClipSetRequestHelperPool::GetNextFreeHelper(pPed);

	// Only if we move, otherwise we might as well just be close to a ladder standing still
	if (!m_bWaitingForLadder && vDesiredMoveRatio.Mag2() != MOVEBLENDRATIO_STILL && GetState() != STATE_CLIMB_LADDER)
	{
		m_pTargetLadder = NULL;
		m_iLadderIndex = 0;

		if (m_pLadderClipRequestHelper)
			m_pLadderClipRequestHelper->m_fLastClosestLadder = 0.f;
	}
	
	m_pTargetUncuffPed = NULL;
	m_pTargetArrestPed = NULL;

	CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
	if (pWeaponManager)
	{
		fwMvClipSetId weaponHoldingClipsetId = CTaskCover::GetWeaponHoldingClipSetForArmament(pPed);
		CTaskCover::RequestCoverClipSet(weaponHoldingClipsetId, SP_High);
	}

#if FPS_MODE_SUPPORTED
	// Store the result
	pPed->GetMotionData()->SetUsingAnalogStickRun(UseAnalogRun(pPed)); 
#endif // FPS_MODE_SUPPORTED

	return FSM_Continue;
}

dev_float TIME_TO_INTIMIDATE_PED = 3.0f;
dev_float DIST_TO_INTIMIDATE_PED = 1.5f;

CTask::FSM_Return CTaskPlayerOnFoot::ProcessPostFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	const float fTimeStep = GetTimeStep();

	if (m_pLadderClipRequestHelper && (!m_pTargetLadder || m_pLadderClipRequestHelper->m_fLastClosestLadder >= ms_fStreamClosestLadderMinDistSqr))
		m_pLadderClipRequestHelper->ReleaseAllClipSets();

	// Reset the flag that prevents the player from entering cover
	m_bPlayerExitedCoverThisUpdate = false;

	if(pPed->GetMotionData() && !pPed->GetMotionData()->GetCombatRoll())
	{
		m_bGetInVehicleAfterCombatRolling = false;
	}

	// Find a ped in front of us to intimidate
	CPed* pPedToIndimidate = NULL;
	if( GetState() == STATE_PLAYER_IDLES && m_fTimeBetweenIntimidations <= 0.0f )
	{
		const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		Vector3 vPedToIntimidatePos(Vector3::ZeroType);
		CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
		for( CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext() )
		{
			CPed* pOtherPed = static_cast<CPed*>(pEnt);
			if( pOtherPed->PopTypeIsRandom() )
			{
				Vector3 vToPed = VEC3V_TO_VECTOR3(pOtherPed->GetTransform().GetPosition()) - vPedPos;
				if( DotProduct(vToPed, VEC3V_TO_VECTOR3(pPed->GetTransform().GetB())) > 0.0f &&
					vToPed.Mag2() < rage::square(DIST_TO_INTIMIDATE_PED) )
				{
					pPedToIndimidate = pOtherPed;
					vPedToIntimidatePos = VEC3V_TO_VECTOR3(pPedToIndimidate->GetTransform().GetPosition());
					break;
				}
			}
		}
	}

	if( m_fTimeBetweenIntimidations > 0.0f )
		m_fTimeBetweenIntimidations -= fTimeStep;

	// Update the intimidate timers.
	if( pPedToIndimidate != m_pPedPlayerIntimidating )
	{
		m_pPedPlayerIntimidating = pPedToIndimidate;
		m_fTimeIntimidating = 0.0f;
	}
	else if( m_pPedPlayerIntimidating )
	{
		m_fTimeIntimidating += fTimeStep;

		if( m_fTimeIntimidating > TIME_TO_INTIMIDATE_PED )
		{
			CTaskAmbientClips* pTaskAmbientClips = static_cast<CTaskAmbientClips*>(m_pPedPlayerIntimidating->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AMBIENT_CLIPS));

			if( m_pPedPlayerIntimidating->GetPedIntelligence()->GetTaskActive() == m_pPedPlayerIntimidating->GetPedIntelligence()->GetTaskDefault() &&
				!m_pPedPlayerIntimidating->GetPedIntelligence()->IsFriendlyWith(*pPed) &&
				CAmbientLookAt::GetLookAtBlockingState(pPed, pTaskAmbientClips, false) == CAmbientLookAt::LBS_NoBlocking)
			{
				CPlayerInfo *pPlayer = CGameWorld::GetMainPlayerInfo();

				s32 scenario = m_pPedPlayerIntimidating->GetPedIntelligence()->GetQueriableInterface()->GetRunningScenarioType();
				bool bTurnToFaceScenario = false;
				if( scenario != Scenario_Invalid)
				{
					const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(scenario);
					bTurnToFaceScenario = pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::TurnToFacePlayerStanding);
				}

				if( m_pPedPlayerIntimidating->GetPedType() == PEDTYPE_COP )
				{
					m_pPedPlayerIntimidating->NewSay("INTIMIDATE", 0 , false, false, -1, pPed, ATSTRINGHASH("INTIMIDATE_RESP", 0x0080ce82f));
					m_pPedPlayerIntimidating->GetIkManager().LookAt(0, pPed, 1000, BONETAG_HEAD, NULL, 0, 500, 500, CIkManager::IK_LOOKAT_HIGH);
				}
				else if( m_pPedPlayerIntimidating->IsGangPed() && pPlayer && pPlayer && pPlayer->bCanBeHassledByGangs )
				{
					// Add an event causing this ped to dislike the other chap
					CEventAcquaintancePedDislike event(pPed);
					m_pPedPlayerIntimidating->GetPedIntelligence()->AddEvent(event);
				}
				else if( scenario != Scenario_Invalid && pPed->CheckBraveryFlags(BF_INTIMIDATE_PLAYER) )
				{
					if( bTurnToFaceScenario )
					{
						const char * pPhrase = (pPed->GetSpeechAudioEntity() && pPed->GetSpeechAudioEntity()->DoesContextExistForThisPed("GENERIC_FUCK_OFF")) ? "GENERIC_FUCK_OFF" : "GENERIC_DEJECTED";
						CTaskSayAudio* pResponse = rage_new CTaskSayAudio(pPhrase, 0.3f, 1.0f, pPed);
						CEventGivePedTask event(PED_TASK_PRIORITY_EVENT_RESPONSE_NONTEMP, pResponse);
						m_pPedPlayerIntimidating->GetPedIntelligence()->AddEvent(event);
					}
					else
					{
						m_pPedPlayerIntimidating->GetIkManager().LookAt(0, pPed, 1000, BONETAG_HEAD, NULL, 0, 500, 500, CIkManager::IK_LOOKAT_HIGH);
						if(!m_pPedPlayerIntimidating->NewSay("GENERIC_FUCK_OFF"))
							m_pPedPlayerIntimidating->NewSay("GENERIC_DEJECTED");
					}
				}
				else if( m_pPedPlayerIntimidating->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_WANDER) )
				{
					m_pPedPlayerIntimidating->GetIkManager().LookAt(0, pPed, 1000, BONETAG_HEAD, NULL, 0, 500, 500, CIkManager::IK_LOOKAT_HIGH);
					m_pPedPlayerIntimidating->NewSay("FOLLOWED");
				}
				else if( !pPed->CheckBraveryFlags(BF_INTIMIDATE_PLAYER) )
				{
					m_pPedPlayerIntimidating->GetIkManager().LookAt(0, pPed, 1000, BONETAG_HEAD, NULL, 0, 500, 500, CIkManager::IK_LOOKAT_HIGH);
					m_pPedPlayerIntimidating->NewSay("GENERIC_DEJECTED");
				}
			}
			m_fTimeIntimidating = 0.0f;
			m_fTimeBetweenIntimidations = 5.0f;
		}
	}

	if(GetState()==STATE_JUMP || GetState()==STATE_DROPDOWN)
	{
		CControl *pControl = pPed->GetControlFromPlayer();
		if(pPed->GetClimbPhysical(true) && pPed->GetClimbPhysical(true)->GetIsTypeVehicle())
		{
			CVehicle *pVehicle = static_cast<CVehicle*>(pPed->GetClimbPhysical(true));
			if(pVehicle->InheritsFromBoat())
			{
				if(pControl->GetPedEnter().IsPressed())
				{
					m_nCachedBreakOutToMovementState = STATE_GET_IN_VEHICLE;
				}
			}
		}
		else
		{
			//! If landing, cache controls for breakout.
			if(pPed->IsOnGround() || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsLanding))
			{
				if(pControl->GetPedCover().IsPressed())
				{
					m_nCachedBreakOutToMovementState = STATE_USE_COVER;
				}
				else if(pControl->GetPedEnter().IsPressed())
				{
					m_nCachedBreakOutToMovementState = STATE_GET_IN_VEHICLE;
				}
			}
			else
			{
				m_nCachedBreakOutToMovementState=-1;
			}
		}
	}

#if FPS_MODE_SUPPORTED
	if(m_pFirstPersonIkHelper && pPed->IsFirstPersonShooterModeEnabledForPlayer(false, false, true))
	{
		TUNE_GROUP_FLOAT(GET_IN_VEHICLE, FPS_IK_OFFSET, -0.35f, -1.f, 1.f, 0.01f);
		TUNE_GROUP_FLOAT(GET_IN_VEHICLE, FPS_IK_BLEND_RATE, 1.f, 0.f, 10.f, 0.01f);

		const float fTargetOffset = (GetState() == STATE_GET_IN_VEHICLE) ? FPS_IK_OFFSET : 0.f;
		Approach(m_fGetInVehicleFPSIkZOffset, fTargetOffset, FPS_IK_BLEND_RATE, fTimeStep);

		const Vec3V vGetInVehicleOffset(0.f, 0.f, m_fGetInVehicleFPSIkZOffset);
		m_pFirstPersonIkHelper->SetOffset(vGetInVehicleOffset);
	}
	else
	{
		if(m_pFirstPersonIkHelper)
		{
			const Vec3V vZero(V_ZERO);
			m_pFirstPersonIkHelper->SetOffset(vZero);
		}

		m_fGetInVehicleFPSIkZOffset = 0.f;
	}
#endif // FPS_MODE_SUPPORTED

	return FSM_Continue;
}

bool CTaskPlayerOnFoot::ProcessSpecialAbilities()
{
	CPed& playerPed = *GetPed();
	CPlayerSpecialAbility* pSpecialAbility = playerPed.GetSpecialAbility();
	if (pSpecialAbility)
	{
		CPlayerSpecialAbilityManager::ProcessControl(&playerPed);
	}
	return false;
}

#if !__FINAL
const char * CTaskPlayerOnFoot::GetStaticStateName( s32 iState )
{
	static const char* sStateNames[] =
	{
		"State_Initial",
		"State_Player_Idles",
		"State_Jump",
		"State_Melee",
		"State_Swap_Weapon",
		"State_Use_Cover",
		"State_Climb_Ladder",
		"State_Player_Gun",
		"State_Get_In_Vehicle",
		"State_Take_Off_Helmet",
		"State_Melee_Reaction",
		"State_Arrest",
		"State_Uncuff",
		"State_MountAnimal",
		"State_DuckAndCover",
		"State_SlopeScramble",
		"State_ScriptedTask",
		"State_DropDown",
		"State_TakeOffParachutePack",
		"State_TakeOffJetpackPack",
		"State_TakeOffScubaGear",
		"State_RideTrain",
		"State_Jetpack",
		"State_BirdProjectile",
	};

	return sStateNames[iState];
}
#endif

CTask::FSM_Return CTaskPlayerOnFoot::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed* pPlayerPed = GetPed();

	if(m_pScriptedTask && iEvent == OnUpdate && GetState() != STATE_SCRIPTED_TASK)
	{
		SetState(STATE_SCRIPTED_TASK);
		return FSM_Continue;
	}

FSM_Begin
	FSM_State(STATE_INITIAL)
		FSM_OnUpdate
			return Initial_OnUpdate(pPlayerPed);

	FSM_State(STATE_PLAYER_IDLES)
		FSM_OnEnter
			PlayerIdles_OnEnter(pPlayerPed);
		FSM_OnUpdate
			return PlayerIdles_OnUpdate(pPlayerPed);
		FSM_OnExit
			PlayerIdles_OnExit(pPlayerPed);

	FSM_State(STATE_JUMP)
		FSM_OnEnter
			Jump_OnEnter(pPlayerPed);
		FSM_OnUpdate
			return Jump_OnUpdate(pPlayerPed);

	FSM_State(STATE_MELEE)
		FSM_OnEnter
			Melee_OnEnter(pPlayerPed);
		FSM_OnUpdate
			return Melee_OnUpdate(pPlayerPed);
		FSM_OnExit
			Melee_OnExit();

	FSM_State(STATE_SWAP_WEAPON)
		FSM_OnEnter
			SwapWeapon_OnEnter(pPlayerPed);
		FSM_OnUpdate
			return SwapWeapon_OnUpdate(pPlayerPed);

	FSM_State(STATE_USE_COVER)
		FSM_OnEnter
			UseCover_OnEnter(pPlayerPed);
		FSM_OnUpdate
			return UseCover_OnUpdate(pPlayerPed);
		FSM_OnExit
			return UseCover_OnExit();

	FSM_State(STATE_CLIMB_LADDER)
		FSM_OnEnter
			ClimbLadder_OnEnter(pPlayerPed);
		FSM_OnUpdate
			return ClimbLadder_OnUpdate(pPlayerPed);
		FSM_OnExit
			return ClimbLadder_OnExit(pPlayerPed);

	FSM_State(STATE_PLAYER_GUN)
		FSM_OnEnter
			PlayerGun_OnEnter(pPlayerPed);
		FSM_OnUpdate
			return PlayerGun_OnUpdate(pPlayerPed);
		FSM_OnExit
			return PlayerGun_OnExit(pPlayerPed);

	FSM_State(STATE_GET_IN_VEHICLE)
		FSM_OnEnter
			GetInVehicle_OnEnter(pPlayerPed);
		FSM_OnUpdate
			return GetInVehicle_OnUpdate(pPlayerPed);

	FSM_State(STATE_MOUNT_ANIMAL)
		FSM_OnEnter
			GetMountAnimal_OnEnter(pPlayerPed);
		FSM_OnUpdate
			return GetMountAnimal_OnUpdate(pPlayerPed);

	FSM_State(STATE_TAKE_OFF_HELMET)
		FSM_OnEnter
			TakeOffHelmet_OnEnter(pPlayerPed);
		FSM_OnUpdate
			return TakeOffHelmet_OnUpdate(pPlayerPed);

	FSM_State(STATE_MELEE_REACTION)
		FSM_OnEnter
			MeleeReaction_OnEnter(pPlayerPed);
		FSM_OnUpdate
			return MeleeReaction_OnUpdate(pPlayerPed);

	FSM_State(STATE_ARREST)
		FSM_OnEnter
			Arrest_OnEnter(pPlayerPed);
		FSM_OnUpdate
			return Arrest_OnUpdate(pPlayerPed);

	FSM_State(STATE_UNCUFF)
		FSM_OnEnter
			Uncuff_OnEnter(pPlayerPed);
		FSM_OnUpdate
			return Uncuff_OnUpdate(pPlayerPed);

	FSM_State(STATE_DUCK_AND_COVER)
		FSM_OnEnter
			DuckAndCover_OnEnter(pPlayerPed);
		FSM_OnUpdate
			return DuckAndCover_OnUpdate(pPlayerPed);

	FSM_State(STATE_SLOPE_SCRAMBLE)
		FSM_OnEnter
			SlopeScramble_OnEnter(pPlayerPed);
		FSM_OnUpdate
			return SlopeScramble_OnUpdate(pPlayerPed);

	FSM_State(STATE_SCRIPTED_TASK)
		FSM_OnEnter
			ScriptedTask_OnEnter(pPlayerPed);
		FSM_OnUpdate
			return ScriptedTask_OnUpdate(pPlayerPed);

	FSM_State(STATE_DROPDOWN)
		FSM_OnEnter
			DropDown_OnEnter(pPlayerPed);
	FSM_OnUpdate
			return DropDown_OnUpdate(pPlayerPed);

	FSM_State(STATE_TAKE_OFF_PARACHUTE_PACK)
		FSM_OnEnter
			TakeOffParachutePack_OnEnter(pPlayerPed);
		FSM_OnUpdate
			return TakeOffParachutePack_OnUpdate(pPlayerPed);

	FSM_State(STATE_TAKE_OFF_JETPACK)
		FSM_OnEnter
			TakeOffJetpack_OnEnter(pPlayerPed);
		FSM_OnUpdate
			return TakeOffJetpack_OnUpdate(pPlayerPed);
		FSM_OnExit
			TakeOffJetpack_OnExit(pPlayerPed);

	FSM_State(STATE_TAKE_OFF_SCUBA_GEAR)
		FSM_OnEnter
			TakeOffScubaGear_OnEnter(pPlayerPed);
		FSM_OnUpdate
			return TakeOffScubaGear_OnUpdate(pPlayerPed);

	FSM_State(STATE_RIDE_TRAIN)
		FSM_OnEnter
			RideTrain_OnEnter();
		FSM_OnUpdate
			return RideTrain_OnUpdate();
	
	FSM_State(STATE_JETPACK)
		FSM_OnEnter
			Jetpack_OnEnter();
		FSM_OnUpdate
			return Jetpack_OnUpdate();

	FSM_State(STATE_BIRD_PROJECTILE)
		FSM_OnEnter
			BirdProjectile_OnEnter();
		FSM_OnUpdate
			return BirdProjectile_OnUpdate();
		FSM_OnExit
			BirdProjectile_OnExit();
			

FSM_End
}

void CTaskPlayerOnFoot::MeleeReaction_OnEnter(CPed* UNUSED_PARAM(pPlayerPed))
{
	Assert(m_pMeleeReactionTask);
	SetNewTask(m_pMeleeReactionTask);
	m_pMeleeReactionTask = NULL;
}

CTask::FSM_Return CTaskPlayerOnFoot::MeleeReaction_OnUpdate(CPed* UNUSED_PARAM(pPlayerPed))
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(STATE_PLAYER_IDLES);
	}

	return FSM_Continue;

}

CTask::FSM_Return CTaskPlayerOnFoot::Initial_OnUpdate(CPed* pPlayerPed)
{
	AI_LOG_WITH_ARGS("[Player] - Ped %s starting CTaskPlayerOnFoot - 0x%p\n", AILogging::GetDynamicEntityNameSafe(pPlayerPed), this);

	if (m_bScriptedToGoIntoCover)
	{
		pPlayerPed->SetPlayerResetFlag(CPlayerResetFlags::PRF_FORCE_PLAYER_INTO_COVER);
	}

	if (pPlayerPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_FORCE_PLAYER_INTO_COVER))
	{
		SetState(STATE_USE_COVER);
		return FSM_Continue;
	}

	// If flagged to enter cover straight after leaving a vehicle, do that now
	if (pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_WantsToEnterCover) && CheckForNearbyCover(pPlayerPed, CSF_ConsiderCloseCover|CSF_ConsiderDynamicCover))
	{
		SetState(STATE_USE_COVER);
		return FSM_Continue;
	}
	else if (pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_WantsToEnterVehicleFromCover) && pPlayerPed->GetMyVehicle())
	{
		SetState(STATE_GET_IN_VEHICLE);
		return FSM_Continue;
	}

	SetState(STATE_PLAYER_IDLES);
	return FSM_Continue;
}

void CTaskPlayerOnFoot::PlayerIdles_OnEnter( CPed* pPlayerPed )
{
	// reset idle timer
	m_fIdleTime = 0.0f;
	CTaskPlayerIdles* pIdleTask = rage_new CTaskPlayerIdles();
	SetNewTask(pIdleTask);
	
	// Set the open door arm IK flag
	pPlayerPed->SetPedConfigFlag( CPED_CONFIG_FLAG_OpenDoorArmIK, true );

	// For safety, reset in case it gets left on.
	pPlayerPed->SetPedConfigFlag( CPED_CONFIG_FLAG_UseReserveParachute, false );

	if (!pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_VaultFromCover))
		pPlayerPed->GetPedIntelligence()->GetClimbDetector().ResetPendingResults();

	pPlayerPed->GetPedIntelligence()->GetDropDownDetector().ResetPendingResults();

	m_bWaitingForLadder = false;

	CControl* pControl = pPlayerPed->GetControlFromPlayer();
	if(pControl)
	{
		switch(GetPreviousState())
		{
		case STATE_JUMP:
		case STATE_PLAYER_GUN:
		case STATE_SWAP_WEAPON:
			break;
		default:
			pControl->ClearToggleRun();
			break;
		}
	}
}

CTask::FSM_Return CTaskPlayerOnFoot::PlayerIdles_OnUpdate(CPed* pPlayerPed)
{
	// Used to check for armed melee actions
	pPlayerPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPostPreRender, true );

	// talk
	UpdateTalk(pPlayerPed, true, true);

	// Check to see if we have a melee task stored off from the previous frame
	if( m_pMeleeRequestTask )
	{
		pPlayerPed->SetIsCrouching( false, -1, false );
		taskDebugf1("STATE_MELEE: CTaskPlayerOnFoot::PlayerIdles_OnUpdate: m_pMeleeRequestTask");
		SetState(STATE_MELEE);
		return FSM_Continue;
	}
	
	ProcessSpecialAbilities();
	
	if(CheckShouldRideTrain())
	{
		SetState(STATE_RIDE_TRAIN);
		return FSM_Continue;
	}

	s32 iDesiredState = GetDesiredStateFromPlayerInput(pPlayerPed);

	if(m_bWaitingForLadder)
	{
		// Ped has given up trying to get on ladder....
				// Ped has given up trying to get on ladder....
		if(!KeepWaitingForLadder(pPlayerPed, m_fBlockHeading))
		{
			m_bWaitingForLadder = false;
			iDesiredState = STATE_PLAYER_IDLES;
		}

		pPlayerPed->SetPedResetFlag(CPED_RESET_FLAG_LadderBlockingMovement, true);
		pPlayerPed->GetMotionData()->SetDesiredMoveBlendRatio(0.0f, 0.0f);
		pPlayerPed->SetVelocity(VEC3_ZERO); 
		pPlayerPed->SetDesiredVelocity(VEC3_ZERO);

		pPlayerPed->SetHeading(m_fBlockHeading); 
		pPlayerPed->SetPosition(m_vBlockPos, true, true);

		if(CTaskClimbLadderFully::IsLadderBaseOrTopBlockedByPedMP(m_pTargetLadder, m_iLadderIndex, CTaskClimbLadderFully::BF_CLIMBING_DOWN)||
			CTaskGoToAndClimbLadder::IsLadderTopBlockedMP(m_pTargetLadder, *pPlayerPed, m_iLadderIndex))
		{
			if(iDesiredState == STATE_CLIMB_LADDER || iDesiredState == STATE_DROPDOWN)
				iDesiredState = STATE_PLAYER_IDLES;
		}
		else
		{
			iDesiredState = STATE_CLIMB_LADDER;
		}
	}
	else if (iDesiredState == STATE_CLIMB_LADDER)
	{
		if (!m_bGettingOnBottom && 
			NetworkInterface::IsGameInProgress() &&
			(CTaskClimbLadderFully::IsLadderBaseOrTopBlockedByPedMP(m_pTargetLadder, m_iLadderIndex, CTaskClimbLadderFully::BF_CLIMBING_DOWN)||
			CTaskGoToAndClimbLadder::IsLadderTopBlockedMP(m_pTargetLadder, *pPlayerPed, m_iLadderIndex)))
		{
			m_vBlockPos = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());
			m_fBlockHeading = pPlayerPed->GetTransform().GetHeading();
			m_bWaitingForLadder = true;
			iDesiredState = STATE_PLAYER_IDLES;
		}
	}

	if (iDesiredState == STATE_PLAYER_IDLES 
#if FPS_MODE_SUPPORTED 
		|| (iDesiredState == STATE_PLAYER_GUN && pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false))
#endif 
		)
	{
		// Might want to take off helmet
		if(pPlayerPed->GetHelmetComponent() && pPlayerPed->GetHelmetComponent()->IsHelmetEnabled() && !pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DontTakeOffHelmet ) 
			&& !CTaskMobilePhone::IsRunningMobilePhoneTask(*pPlayerPed) && !pPlayerPed->GetPlayerInfo()->IsControlsScriptDisabled() && !pPlayerPed->GetIsInWater())
		{
#if FPS_MODE_SUPPORTED
			// B*2055903: Don't take off helmet unless we're in FPS_IDLE. If we're not idle, reset the timer
			if (pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false))
			{
				if (!pPlayerPed->GetMotionData()->GetIsFPSIdle())
				{
					m_fIdleTime = 0.0f;
				}
			}
#endif	//FPS_MODE_SUPPORTED

			m_fIdleTime += GetTimeStep();

			// If no action has been taken by the player within a time, take off the helmet (B*2146344: unless we're playing a scripted anim).
			if((m_fIdleTime > 4.0f || pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_RemoveHelmet)) &&
				!pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingHelmetVisor) &&
				!pPlayerPed->GetPedIntelligence()->FindTaskActiveByTreeAndType(PED_TASK_TREE_SECONDARY, CTaskTypes::TASK_SCRIPTED_ANIMATION))
			{
				SetState(STATE_TAKE_OFF_HELMET);
				return FSM_Continue;
			}
		}
	}

	if(iDesiredState == STATE_PLAYER_IDLES)
	{
		// No state change requested
	#if FPS_MODE_SUPPORTED && 0
		// This is only required if calling PlayerIdles_OnUpdate from PlayerGun_OnUpdate.
		// TODO: remove this if not needed.
		if( pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false) )
		{
			if( iDesiredState != GetState() )
			{
				SetState(iDesiredState);
			}
		}
	#endif // FPS_MODE_SUPPORTED

		// Enable scanning flags
		pPlayerPed->SetPedResetFlag( CPED_RESET_FLAG_SearchingForDoors, true );
		pPlayerPed->SetPedResetFlag( CPED_RESET_FLAG_GestureAnimsAllowed, true );

		//-------------------------------------------------------------------------
		// DEBUG STRAFING
		//-------------------------------------------------------------------------
		if(ms_bAllowStrafingWhenUnarmed)
		{
			if(pPlayerPed->GetWeaponManager())
			{
				CWeapon* pWeapon = pPlayerPed->GetWeaponManager()->GetEquippedWeapon();
				if(CPlayerInfo::IsAiming() && pWeapon && pWeapon->GetWeaponInfo()->GetIsMelee())
				{
					pPlayerPed->SetIsStrafing(true);
				}
				else
				{
					pPlayerPed->SetIsStrafing(false);
				}
			}
			else
			{
				pPlayerPed->SetIsStrafing(false);
			}
		}

		if (CheckShouldPerformArrest())
		{
			ArrestTargetPed(pPlayerPed);
			return FSM_Continue;
		}
		//************************************************************************
		// If we are running into a low obstacle then maybe try to hop up onto it

		ePlayerOnFootState autoState = CheckForAutoCover(pPlayerPed);
		if( autoState != STATE_INVALID )
		{
			SetState(autoState);
			return FSM_Continue;
		}

		//************************************************************************
		// Idle weapon block for heavy weapon minigun. 
		if(pPlayerPed->GetWeaponManager())
		{
			CWeapon* pWeapon = pPlayerPed->GetWeaponManager()->GetEquippedWeapon();
			const CWeaponInfo* pWeaponInfo = pWeapon ? pWeapon->GetWeaponInfo() : NULL;
			if (pWeaponInfo && pWeaponInfo->GetIsHeavy())
			{
				//Expose these to rag if we need to handle more weapons etc.
				const float fForwardOffset = 0.7f;
				const float fUpOffset = -0.44f;

				float fProbeLength = m_bIdleWeaponBlocked ? 1.1f : 1.0f;
				Vector3 vTargetPos = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition()) + (VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetForward()) * fForwardOffset) + (VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetUp()) * fUpOffset);
				if(CTaskWeaponBlocked::IsWeaponBlocked(pPlayerPed, vTargetPos, NULL, 1.0f, fProbeLength))
				{
					const fwMvClipId ms_WeaponBlockedClipId(ATSTRINGHASH("wall_block_idle", 0x0f9b74953));
					const fwMvClipId ms_WeaponBlockedNewClipId(ATSTRINGHASH("wall_block", 0x0ea90630e));

					fwMvClipId wallBlockClipId = CLIP_ID_INVALID;
					const fwMvClipSetId appropriateWeaponClipSetId = pWeaponInfo->GetAppropriateWeaponClipSetId(pPlayerPed);
					if(fwAnimManager::GetClipIfExistsBySetId(appropriateWeaponClipSetId, ms_WeaponBlockedNewClipId))
					{
						wallBlockClipId = ms_WeaponBlockedNewClipId;
					}
					else if(fwAnimManager::GetClipIfExistsBySetId(appropriateWeaponClipSetId, ms_WeaponBlockedClipId))
					{
						wallBlockClipId = ms_WeaponBlockedClipId;
					}

					if(wallBlockClipId != CLIP_ID_INVALID && !m_bIdleWeaponBlocked) 
					{
						m_bIdleWeaponBlocked = true;
						StartClipBySetAndClip(pPlayerPed, appropriateWeaponClipSetId, wallBlockClipId, SLOW_BLEND_IN_DELTA, CClipHelper::TerminationType_OnDelete);
						Assertf(GetClipHelper(), "Blocked weapon clip failed to start");
					}
				}
				else
				{
					if(m_bIdleWeaponBlocked)
					{
						m_bIdleWeaponBlocked = false;
						StopClip(SLOW_BLEND_OUT_DELTA);
					}
				}
			}
			else
			{
				if(m_bIdleWeaponBlocked)
				{
					m_bIdleWeaponBlocked = false;
					StopClip(SLOW_BLEND_OUT_DELTA);
				}
			}
		}

		return FSM_Continue;
	}
	else
	{
		if(iDesiredState != GetState())
			SetState(iDesiredState);
		return FSM_Continue;
	}
}

void CTaskPlayerOnFoot::PlayerIdles_OnExit( CPed* pPlayerPed )
{
	// Reset the open door arm IK flag
	pPlayerPed->SetPedConfigFlag( CPED_CONFIG_FLAG_OpenDoorArmIK, false );

	// Stop stealh prep clips if we are currently running
	if( m_bUsingStealthClips )
	{
		m_bUsingStealthClips = false;
		if( GetClipHelper() )
		{
			StopClip( SLOW_BLEND_OUT_DELTA );
		}
	}

	if(m_bIdleWeaponBlocked)
	{
		m_bIdleWeaponBlocked = false;
		StopClip(NORMAL_BLEND_OUT_DELTA);
	}
}


fwFlags32 CTaskPlayerOnFoot::MakeJumpFlags(CPed* pPlayerPed, const sPedTestResults &pedTestResults)
{
	// There is some 2nd surface depth beyond which the player cannot jump
	// For now we'll assume all 2nd surfaces are 'snow' - but eventually we might
	// need to test which material we're standing on to decide specific depths..
	s32 iFlags = 0;
#if HACK_GTA4_BOUND_GEOM_SECOND_SURFACE
	if(pPlayerPed->GetPedResetFlag( CPED_RESET_FLAG_DisablePlayerJumping ) || pPlayerPed->GetSecondSurfaceDepth() > ms_fMaxSnowDepthRatioForJump)
#else
	if(pPlayerPed->GetPedResetFlag( CPED_RESET_FLAG_DisablePlayerJumping ) || 0.0f > ms_fMaxSnowDepthRatioForJump)
#endif	// HACK_GTA4_BOUND_GEOM_SECOND_SURFACE
	{
		iFlags |= JF_DisableJumpingIfNoClimb;
	}

	if(pPlayerPed->GetPedResetFlag( CPED_RESET_FLAG_DisablePlayerVaulting ) && !m_bJumpedFromCover)
	{
		iFlags |= JF_DisableVault;
	}
	else if (m_bJumpedFromCover)
	{
		iFlags |= JF_ForceVault;
	}

	if(pedTestResults.GetCanAutoJumpVault())
	{
		iFlags |= JF_AutoJump;
	}

	iFlags |= JF_UseCachedHandhold;

	if(pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_VaultFromCover))
	{
		iFlags |= JF_FromCover;
		iFlags |= JF_DisableJumpingIfNoClimb;
	}

	if(pPlayerPed->GetPlayerInfo()->GetPlayerResetFlags().IsFlagSet(CPlayerResetFlags::PRF_SUPER_JUMP_ON))
	{
		iFlags |= JF_SuperJump;
	}

	if(pPlayerPed->GetPlayerInfo()->GetPlayerResetFlags().IsFlagSet(CPlayerResetFlags::PRF_BEAST_JUMP_ON))
	{
		iFlags |= JF_BeastJump;
	}

	return iFlags;
}

void CTaskPlayerOnFoot::Jump_OnEnter(CPed* pPlayerPed)
{
	pPlayerPed->SetPedConfigFlag( CPED_CONFIG_FLAG_VaultFromCover, false );
	pPlayerPed->SetIsCrouching(false,-1,false);

	// There is one frame where the player gets no input to the moveblender, between where the CTaskMovePlayer is active
	// and where CTaskComplexJump::ProcessPlayerInput() is passing input to the moveblender.  Ensure that we get stick input
	// to prevent the ped initiating a stop-clip here.
	pPlayerPed->GetMotionData()->SetDesiredMoveBlendRatio(pPlayerPed->GetMotionData()->GetCurrentMbrY(), 0.0f);
	pPlayerPed->SetPedResetFlag( CPED_RESET_FLAG_DontChangeMbrInSimpleMoveDoNothing, true );

	//@@: location CTASKPLAYERONFOOT_JUMP_ONENTER_GET_CURRENT_MOTIONTASK
	CTaskMotionBase* pMotionTask = pPlayerPed->GetCurrentMotionTask(false);
	if(pMotionTask && pMotionTask->GetSubTask() && pMotionTask->GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOTION_AIMING_TRANSITION)
	{
		// Quit any active transition task, so the independent mover is triggered
		CTaskMotionAimingTransition* pTransitionTask = static_cast<CTaskMotionAimingTransition*>(pMotionTask->GetSubTask());
		pTransitionTask->ForceQuit();
	}

	// B*1900737 - Interrupt reload task and resume if jumping / climbing / vaulting to prevent instant reload.
	CTaskReloadGun* pReloadTask = static_cast<CTaskReloadGun*>(pPlayerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_RELOAD_GUN));
	if(!pReloadTask)
	{
		CTask* pSecondaryTask = pPlayerPed->GetPedIntelligence()->GetTaskSecondaryActive();
		if(pSecondaryTask && pSecondaryTask->GetTaskType() == CTaskTypes::TASK_RELOAD_GUN)
		{
			pReloadTask = static_cast<CTaskReloadGun*>(pSecondaryTask);
		}
	}
	if(pReloadTask)
	{
		pReloadTask->RequestInterrupt();
	}

	// There is some 2nd surface depth beyond which the player cannot jump
	// For now we'll assume all 2nd surfaces are 'snow' - but eventually we might
	// need to test which material we're standing on to decide specific depths..

	SetNewTask(rage_new CTaskJumpVault(m_iJumpFlags));

	m_iJumpFlags = 0;
	m_bCanBreakOutToMovement = false;
	m_nCachedBreakOutToMovementState = -1;

	//! Reset jump pressed button, which forces us to press again before we can jump/climb again.
	m_uLastTimeJumpPressed = 0;
}

CTask::FSM_Return CTaskPlayerOnFoot::Jump_OnUpdate(CPed* pPlayerPed)
{
	s32 iContinueToState = STATE_INVALID; 
	if(GetPreviousState() == STATE_GET_IN_VEHICLE)
		iContinueToState = STATE_GET_IN_VEHICLE;

	const CTaskJumpVault* pJumpVaultTask = static_cast<const CTaskJumpVault*>(pPlayerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_JUMPVAULT));
	if(pJumpVaultTask)
	{
		CControl* pControl = pPlayerPed->GetControlFromPlayer();
		if(pControl && pControl->GetPedJump().IsPressed())
		{
			m_uLastTimeJumpPressed = fwTimer::GetTimeInMilliseconds();
		}

		if(pJumpVaultTask->CanBreakoutToMovementTask())
		{
			//! Need to reset pending results before we start querying again.
			if(!m_bCanBreakOutToMovement)
			{
				pPlayerPed->GetPedIntelligence()->GetClimbDetector().ResetPendingResults();
				pPlayerPed->GetPedIntelligence()->GetDropDownDetector().ResetPendingResults();
				m_bCanBreakOutToMovement = true;
			}

			if(iContinueToState != STATE_INVALID)
			{
				SetState(iContinueToState);
				return FSM_Continue;
			}

			s32 nState = GetBreakOutToMovementState(pPlayerPed);

			//! Reset after processing.
			m_nCachedBreakOutToMovementState = -1;

			if(nState != STATE_INVALID)
			{
				if(nState == GetState())
				{
					SetFlag(aiTaskFlags::RestartCurrentState);
				}
				else
				{
					SetState(nState);
				}

				return FSM_Continue;
			}
		}
	}

	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if(iContinueToState != STATE_INVALID)
			SetState(iContinueToState);
		else
			SetState(STATE_PLAYER_IDLES);

		return FSM_Continue;
	}

	if(sm_Tunables.m_CanMountFromInAir)
	{
		//mounting from above
		if (GetSubTask() && static_cast<const CTaskJumpVault*>(GetSubTask())->GetIsJumping())
		{
			if (CheckForAirMount(pPlayerPed))
			{
				SetState(STATE_MOUNT_ANIMAL);
				return FSM_Continue;
			}
		}
	}

	return FSM_Continue;
}

void CTaskPlayerOnFoot::DropDown_OnEnter(CPed* pPlayerPed)
{
	pPlayerPed->SetIsCrouching(false,-1,false);

	// There is one frame where the player gets no input to the moveblender, between where the CTaskMovePlayer is active
	// and where CTaskComplexJump::ProcessPlayerInput() is passing input to the moveblender.  Ensure that we get stick input
	// to prevent the ped initiating a stop-clip here.
	pPlayerPed->GetMotionData()->SetDesiredMoveBlendRatio(pPlayerPed->GetMotionData()->GetCurrentMbrY(), 0.0f);
	pPlayerPed->SetPedResetFlag( CPED_RESET_FLAG_DontChangeMbrInSimpleMoveDoNothing, true );

	const CDropDown &dropDown = pPlayerPed->GetPedIntelligence()->GetDropDownDetector().GetDetectedDropDown();

	if (dropDown.m_eDropType==eRagdollDrop)
	{
		CTaskNMBehaviour* pFallTask = NULL;
		if (CTaskNMBehaviour::sm_Tunables.m_UseBalanceForEdgeActivation)
		{
			pFallTask = rage_new CTaskNMBalance(500, 10000, NULL, 0);
		}
		else
		{
			pFallTask = rage_new CTaskNMHighFall(500, NULL, CTaskNMHighFall::HIGHFALL_TEETER_EDGE, NULL, false);
		}		
		
		CEventSwitch2NM event(10000, pFallTask, false, 500);
		pPlayerPed->SwitchToRagdoll(event);
	}
	else
	{
		SetNewTask(rage_new CTaskDropDown(dropDown));
	}

	m_bCanBreakOutToMovement = false;
	m_nCachedBreakOutToMovementState = -1;
}

CTask::FSM_Return CTaskPlayerOnFoot::DropDown_OnUpdate(CPed* pPlayerPed)
{
	const CTaskDropDown* pDropDownTask = static_cast<const CTaskDropDown*>(pPlayerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_DROP_DOWN));
	if(pDropDownTask)
	{
		if(pDropDownTask->CanBreakoutToMovementTask())
		{
			//! Need to reset pending results before we start querying again.
			if(!m_bCanBreakOutToMovement)
			{
				pPlayerPed->GetPedIntelligence()->GetClimbDetector().ResetPendingResults();
				pPlayerPed->GetPedIntelligence()->GetDropDownDetector().ResetPendingResults();
				m_bCanBreakOutToMovement = true;
			}

			s32 nState = GetBreakOutToMovementState(pPlayerPed);

			//! Reset after processing.
			m_nCachedBreakOutToMovementState = -1;

			if(nState != STATE_INVALID)
			{
				if(nState == GetState())
				{
					SetFlag(aiTaskFlags::RestartCurrentState);
				}
				else
				{
					SetState(nState);
				}

				return FSM_Continue;
			}
		}
	}

	//mounting from above
	if(sm_Tunables.m_CanMountFromInAir)
	{
		if (pPlayerPed->GetPedIntelligence()->GetDropDownDetector().HasDetectedMount())
		{
			if (CheckForAirMount(pPlayerPed))
			{
				SetState(STATE_MOUNT_ANIMAL);
				return FSM_Continue;
			}
		}
	}

	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(STATE_PLAYER_IDLES);
		return FSM_Continue;
	}

	return FSM_Continue;
}


void CTaskPlayerOnFoot::Melee_OnEnter(CPed* UNUSED_PARAM(pPlayerPed) )
{
	Assert(m_pMeleeRequestTask);
	SetNewTask(m_pMeleeRequestTask);
	m_pMeleeRequestTask = NULL;
}

CTask::FSM_Return CTaskPlayerOnFoot::Melee_OnUpdate(CPed* pPlayerPed)
{
	// talk
	UpdateTalk(pPlayerPed, false, true);

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if (m_bShouldStartGunTask)
		{
			SetState(STATE_PLAYER_GUN);
			return FSM_Continue;
		}
		taskDebugf1("STATE_PLAYER_IDLES: CTaskPlayerOnFoot::Melee_OnUpdate: SubTaskFinished");
		SetState(STATE_PLAYER_IDLES);
	}
	else if(CheckShouldRideTrain())
	{
		SetState(STATE_RIDE_TRAIN);
	}
	else if (CheckShouldPerformArrest())
	{
		ArrestTargetPed(pPlayerPed);
	}
	else if(CheckShouldPerformUncuff())
	{
		SetState(STATE_UNCUFF);
	}
	else if(pPlayerPed->GetPlayerInfo() && pPlayerPed->GetPlayerInfo()->GetPlayerResetFlags().IsFlagSet(CPlayerResetFlags::PRF_SUPER_JUMP_ON)
		&& DoJumpCheck(pPlayerPed, pPlayerPed->GetControlFromPlayer(), false))
	{
		//  Only allow jumping if we're in State_WaitForMeleeAction
		CTaskMelee *pMeleeSubTask = static_cast<CTaskMelee*>(GetSubTask());
		if (pMeleeSubTask && pMeleeSubTask->GetState() == CTaskMelee::State_WaitForMeleeAction)
		{
			SetState(STATE_JUMP);
		}	
	}

	return FSM_Continue;
}

void CTaskPlayerOnFoot::Melee_OnExit()
{
	if (GetPed())
		GetPed()->SetPedConfigFlag( CPED_CONFIG_FLAG_UsingCoverPoint, false );
}

void CTaskPlayerOnFoot::SwapWeapon_OnEnter(CPed* pPlayerPed)
{
	s32 iFlags = SWAP_HOLSTER;
// 	static dev_bool FORCE_NETWORK_SELECTION = false;
// 	if(NetworkInterface::IsGameInProgress() || FORCE_NETWORK_SELECTION)
// 	{
// 		iFlags |= SWAP_INSTANTLY;
// 	}

	if(pPlayerPed->GetWeaponManager())
	{
		const CWeaponInfo* pInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(pPlayerPed->GetWeaponManager()->GetEquippedWeaponHash());
		if(pInfo && pInfo->GetIsCarriedInHand())
		{
			iFlags |= SWAP_DRAW;
		}
	}

#if FPS_MODE_SUPPORTED
	if(pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		iFlags |= SWAP_TO_AIM;
	}
#endif
	//@@: location CTASKPLAYERONFOOT_SWAPWEAPONONENTER
	SetNewTask(rage_new CTaskComplexControlMovement( rage_new CTaskMovePlayer(), rage_new CTaskSwapWeapon(iFlags), CTaskComplexControlMovement::TerminateOnSubtask ));
}

CTask::FSM_Return CTaskPlayerOnFoot::SwapWeapon_OnUpdate(CPed* pPlayerPed)
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || GetSubTask() == NULL)
	{
		SetState(STATE_PLAYER_IDLES);
	}
	else
	{
		weaponAssert(pPlayerPed->GetWeaponManager());
		CWeapon* pWeapon = pPlayerPed->GetWeaponManager()->GetEquippedWeapon();
		if(DoJumpCheck(pPlayerPed, pPlayerPed->GetControlFromPlayer(), pWeapon && pWeapon->GetWeaponInfo()->GetIsHeavy()))
		{
			SetState(STATE_JUMP);
		}
	}

	return FSM_Continue;
}

void CTaskPlayerOnFoot::UseCover_OnEnter(CPed* pPlayerPed)
{	
	CTaskCover* pCoverTask = NULL;
	s32 iCoverFlags = 0;
	
	CControl* pControl = pPlayerPed->GetControlFromPlayer();
	taskAssert(pControl);
	m_bCoverSprintReleased = !pControl->GetPedSprintIsDown();		

	if (m_bScriptedToGoIntoCover || pPlayerPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_FORCE_PLAYER_INTO_COVER))
	{
		pPlayerPed->ClearPlayerResetFlag(CPlayerResetFlags::PRF_FORCE_PLAYER_INTO_COVER);
			
		CCoverPoint* pCoverPoint = pPlayerPed->GetCoverPoint();
		if (pPlayerPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_SKIP_COVER_ENTRY_ANIM))
		{
			// Skip the idle transition if close to the cover (we lose this flag if being put directly into cover as the player when cover task is first run)
			if (pCoverPoint)
			{
				Vector3 vCoverPosition(Vector3::ZeroType);
				if (pCoverPoint->GetCoverPointPosition(vCoverPosition))
				{
					if ((vCoverPosition - VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition())).XYMag2() <= rage::square(CTaskEnterCover::ms_Tunables.m_CoverEntryMaxDirectDistance))
					{
						iCoverFlags = CTaskCover::CF_SkipIdleCoverTransition;	
					}
				}
			}
		}

		if (pPlayerPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_SPECIFY_INITIAL_COVER_HEADING))
		{
			iCoverFlags |= CTaskCover::CF_SpecifyInitialHeading;
			if (pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_InCoverFacingLeft))
			{
				iCoverFlags |= CTaskCover::CF_FacingLeft;
			}
			if (pPlayerPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_FACING_LEFT_IN_COVER))
			{
				iCoverFlags |= CTaskCover::CF_FacingLeft;
			}
		}

		pPlayerPed->ClearPlayerResetFlag(CPlayerResetFlags::PRF_FACING_LEFT_IN_COVER);
		pPlayerPed->ClearPlayerResetFlag(CPlayerResetFlags::PRF_SKIP_COVER_ENTRY_ANIM);
		pPlayerPed->ClearPlayerResetFlag(CPlayerResetFlags::PRF_SPECIFY_INITIAL_COVER_HEADING);
		pCoverTask = rage_new CTaskCover(CAITarget(), iCoverFlags);
		pCoverTask->SetBlendInDuration(pPlayerPed->GetPlayerInfo()->m_fPutPedDirectlyIntoCoverNetworkBlendInDuration);
		m_bScriptedToGoIntoCover = false;
	}
	else
	{
		pCoverTask = rage_new CTaskCover(CAITarget(), iCoverFlags);
	}

	SetNewTask(pCoverTask);
	ConductorMessageData messageData;
	messageData.conductorName = GunfightConductor;
	messageData.message = PlayerGotIntoCover;
	CONDUCTORS_ONLY(SUPERCONDUCTOR.SendConductorMessage(messageData));

	CTaskReloadGun* pReloadTask = static_cast<CTaskReloadGun*>(pPlayerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_RELOAD_GUN));
	if(!pReloadTask)
	{
		CTask* pSecondaryTask = pPlayerPed->GetPedIntelligence()->GetTaskSecondaryActive();
		if(pSecondaryTask && pSecondaryTask->GetTaskType() == CTaskTypes::TASK_RELOAD_GUN)
		{
			pReloadTask = static_cast<CTaskReloadGun*>(pSecondaryTask);
		}
	}

	if(pReloadTask)
	{
		pReloadTask->RequestInterrupt();
	}

	//Reset player sprint counter so we don't run after cover is done if we came in with a run	
	if (pPlayerPed->GetPlayerInfo())
		pPlayerPed->GetPlayerInfo()->SetPlayerDataSprintControlCounter(0.0f);

	m_bCheckTimeInStateForCover = false;
}

CTask::FSM_Return CTaskPlayerOnFoot::UseCover_OnUpdate(CPed* pPlayerPed)
{
	// Check to see if we have a melee task stored off from the previous frame
	if( m_pMeleeRequestTask )
	{
		pPlayerPed->SetIsCrouching( false, -1, false );
		SetState(STATE_MELEE);
		return FSM_Continue;
	}

	ProcessSpecialAbilities();

	if(CheckShouldRideTrain())
	{
		SetState(STATE_RIDE_TRAIN);
		return FSM_Continue;
	}

	pPlayerPed->SetPedResetFlag(CPED_RESET_FLAG_KeepCoverPoint, true);

	if (GetSubTask() && GetSubTask()->GetSubTask() && GetSubTask()->GetSubTask()->GetTaskType() == CTaskTypes::TASK_IN_COVER)
	{
		if( pPlayerPed->GetCoverPoint() )
		{
			m_vLastCoverDir = VEC3V_TO_VECTOR3(pPlayerPed->GetCoverPoint()->GetCoverDirectionVector());
		}
	}

	// Player use of cover can be disabled using
	if (!pPlayerPed->GetPlayerInfo()->CanUseCover())
	{
		SetState(STATE_PLAYER_IDLES);
		ConductorMessageData messageData;
		messageData.conductorName = GunfightConductor;
		messageData.message = PlayerExitCover;
		CONDUCTORS_ONLY(SUPERCONDUCTOR.SendConductorMessage(messageData));

		return FSM_Continue;
	}

	CControl* pControl = pPlayerPed->GetControlFromPlayer();
	taskAssert(pControl);
	bool bEnterPressed = HasValidEnterVehicleInput(*pControl);
	CVehicle * pTargetVehicle = NULL;
	if (bEnterPressed)
	{
		m_pTargetVehicle = NULL;
		pTargetVehicle = CPlayerInfo::ScanForVehicleToEnter(pPlayerPed, VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetB()), false);
	}

	if (!m_bCoverSprintReleased)
		m_bCoverSprintReleased = pControl->GetPedSprintIsReleased();

	// ENTERING CARS, search for vehicles when enter pressed for picking up objects
	const bool bCanEnterVehicle = CheckCanEnterVehicle(pTargetVehicle);
	if (bCanEnterVehicle)
	{
		pPlayerPed->SetIsInCover(1);
		SetState(STATE_GET_IN_VEHICLE);
		m_pTargetVehicle = const_cast<CVehicle*>(pTargetVehicle);
		ConductorMessageData messageData;
		messageData.conductorName = GunfightConductor;
		messageData.message = PlayerExitCover;
		CONDUCTORS_ONLY(SUPERCONDUCTOR.SendConductorMessage(messageData));
		return FSM_Continue;
	}
#if __BANK
	else if (pTargetVehicle)
	{
		AI_LOG_WITH_ARGS("Cannot enter target vehicle %s", AILogging::GetDynamicEntityNameSafe(pTargetVehicle));
	}
	else if (CheckShouldPerformArrest())
	{
		ArrestTargetPed(pPlayerPed);
	}

#endif // __BANK

	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_COVER)
		{
			m_bCheckTimeInStateForCover = GetSubTask()->GetPreviousState() != CTaskCover::State_ExitCover;
		}
		SetState(STATE_PLAYER_IDLES);
		ConductorMessageData messageData;
		messageData.conductorName = GunfightConductor;
		messageData.message = PlayerExitCover;
		CONDUCTORS_ONLY(SUPERCONDUCTOR.SendConductorMessage(messageData));
		return FSM_Continue;
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskPlayerOnFoot::UseCover_OnExit()
{
	SetLastTimeInCoverTask(fwTimer::GetTimeInMilliseconds());
	m_bPlayerExitedCoverThisUpdate = true;
	return FSM_Continue;
}

void CTaskPlayerOnFoot::ClimbLadder_OnEnter(CPed* UNUSED_PARAM(pPlayerPed))
{
	m_bWaitingForLadder = false;

	Assert(m_pTargetLadder);
	SetNewTask(rage_new CTaskGoToAndClimbLadder(m_pTargetLadder,m_iLadderIndex,CTaskGoToAndClimbLadder::DontAutoClimb,CTaskGoToAndClimbLadder::InputState_Nothing, m_bEasyLadderEntry));

	// Let this get handled by CTaskClimbLadder::ProcessLadderBlocking when a network game is running as its done in a different fashion...
	if(!NetworkInterface::IsGameInProgress())
	{
		u32 LadderBlockFlag = (m_bGettingOnBottom ? CTaskClimbLadderFully::BF_CLIMBING_UP : CTaskClimbLadderFully::BF_CLIMBING_DOWN);
		CTaskClimbLadderFully::BlockLadder(m_pTargetLadder, m_iLadderIndex, fwTimer::GetTimeInMilliseconds(), LadderBlockFlag);	// Block ladder for AI and other players
	}
}

CTask::FSM_Return CTaskPlayerOnFoot::ClimbLadder_OnUpdate(CPed* pPlayerPed)
{
	// Let the movement system know that the player is in direct control of this ped
	pPlayerPed->GetMotionData()->SetPlayerHasControlOfPedThisFrame(true);

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(STATE_PLAYER_IDLES);
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskPlayerOnFoot::ClimbLadder_OnExit(CPed* pPlayerPed)
{
	CPedWeaponManager* pWepMgr = pPlayerPed->GetWeaponManager();
	if (pWepMgr)
	{
		pWepMgr->RestoreBackupWeapon();
	}

	// Let this get handled by CTaskClimbLadder::ProcessLadderBlocking when a network game is running as its done in a different fashion...
	if(!NetworkInterface::IsGameInProgress())
	{
		CTaskClimbLadderFully::ReleaseLadder(m_pTargetLadder, m_iLadderIndex);
	}

	return FSM_Continue;
}

void CTaskPlayerOnFoot::TriggerArrestForPed(CPed* CNC_MODE_ENABLED_ONLY(pPed))
{
#if CNC_MODE_ENABLED
	if (pPed && pPed->IsNetworkClone())
	{
		// Start arrest task remotely
		CStartNetworkPedArrestEvent::Trigger(GetPed(), pPed, 0);
	}
	else if (pPed)
	{
		CTaskIncapacitated* task = (CTaskIncapacitated*)pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_INCAPACITATED);
		if (task)
		{
			task->SetIsArrested(true);
			task->SetPedArrester(GetPed());
		}
	}
#endif
}

void CTaskPlayerOnFoot::ArrestTargetPed(CPed* CNC_MODE_ENABLED_ONLY(pPed))
{
#if CNC_MODE_ENABLED
	Assert(m_pTargetPed);
	if (!m_pTargetPed)
	{
		return;
	}
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PedIsArresting, true);

	CPedWeaponManager* pWeaponManager = GetPed()->GetWeaponManager();
	if (pWeaponManager )
	{
		//Cache of currently equip weapon if its unarmed or melee then equip a new weapon for the arrest
		CWeapon* pEquippedWeapon = pWeaponManager->GetEquippedWeapon();
		if (pEquippedWeapon && !pEquippedWeapon->GetWeaponInfo()->GetIsGun())
		{
			pWeaponManager->ClearBackupWeapon();
			pWeaponManager->SetBackupWeapon(pEquippedWeapon->GetWeaponHash());

			//Try to equip a pistol for the arrest
			if(pPed->GetInventory()->GetWeapon(WEAPONTYPE_PISTOL))
			{
				pWeaponManager->EquipWeapon(WEAPONTYPE_PISTOL);
			}
			else
			{
				pWeaponManager->EquipBestWeapon();
			}
		}			
	}

	if(!m_bArrestTimerActive)
	{
		TUNE_GROUP_FLOAT(CNC_ARREST, fArrestTime, 2000.0f, 0.0f, 10000.0f, 0.01f);
		m_uArrestFinishTime = CNetwork::GetNetworkTime() + (u32)fArrestTime;
		TUNE_GROUP_FLOAT(CNC_ARREST, fBreakoutTime, 1500.0f, 0.0f, 10000.0f, 0.01f);
		m_uArrestSprintBreakoutTime = CNetwork::GetNetworkTime() + (u32)fBreakoutTime;
		m_bArrestTimerActive = true;

		//Force aim on target
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ForcedAimFromArrest, true);
	}

	CVehicle* vCrookVehicle = m_pTargetPed->GetVehiclePedInside();

	// Arrest whole vehicle if Ped is in vehicle
	if (vCrookVehicle && vCrookVehicle->GetSeatManager()->GetNumPlayers() > 1)
	{
		for (int seatIndex = 0; seatIndex < MAX_VEHICLE_SEATS; seatIndex++)
		{
			CPed* pSeatedPed = vCrookVehicle->GetSeatManager()->GetPedInSeat(seatIndex);

			if (!pSeatedPed)
			{
				continue;
			}

			if (pSeatedPed->IsPlayer() && pSeatedPed->GetPlayerWanted()->GetWantedLevel() == WANTED_CLEAN)
			{
				continue;
			}

			if (pSeatedPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CanBeArrested) && !pSeatedPed->IsDead())
			{
				CTaskIncapacitated* incapTask = (CTaskIncapacitated*)pSeatedPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_INCAPACITATED);


				if (incapTask && incapTask->GetState() == CTaskIncapacitated::State_Incapacitated && !incapTask->GetIsArrested())
				{
					TriggerArrestForPed(pSeatedPed);
				}
			}
		}
	}
	else if(!m_pTargetPed->GetIsArrested())
	{
		TriggerArrestForPed(m_pLastTargetPed);
	}
#endif
}

bool CTaskPlayerOnFoot::CheckShouldPerformArrest(bool CNC_MODE_ENABLED_ONLY(bIgnoreInput), bool CNC_MODE_ENABLED_ONLY(bIsEnteringVehicle))
{
#if CNC_MODE_ENABLED
	if (!NetworkInterface::IsGameInProgress())
	{
		return false;
	}

	CPed* pPed = GetPed();
	if (!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CanPerformArrest) || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedIsArresting))
	{
		return false;
	}

	// Remove last target if they are no longer incapacitated
	if(m_pLastTargetPed)
	{
		CTaskIncapacitated* incapTask = (CTaskIncapacitated*)m_pLastTargetPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_INCAPACITATED);
		if (!incapTask)
		{
			m_pLastTargetPed = nullptr;
		}
	}

	if(bIsEnteringVehicle)
	{
		if(m_pTargetVehicle)
		{
			const CTaskEnterVehicle* pEnterVehicleSubTask = static_cast<const CTaskEnterVehicle*>(FindSubTaskOfType(CTaskTypes::TASK_ENTER_VEHICLE));
			if(pEnterVehicleSubTask && pEnterVehicleSubTask->GetTargetEntryPoint() > -1)
			{
				//Check if we are within the arrest range
				Vector3 vCarDoorPosition;
				m_pTargetVehicle->GetEntryExitPoint(pEnterVehicleSubTask->GetTargetEntryPoint())->GetEntryPointPosition(m_pTargetVehicle, NULL, vCarDoorPosition);
				if(vCarDoorPosition.Dist2(pPed->GetGroundPos()) <= sm_Tunables.m_ArrestDistance * sm_Tunables.m_ArrestDistance)
				{
					if(m_pTargetVehicle->GetSeatManager()->GetNumPlayers() > 0)
					{
						//Trigger arrest if we find a ped that is arrestable in the vehicle
						for (int seatIndex = 0; seatIndex < MAX_VEHICLE_SEATS; seatIndex++)
						{
							CPed* pSeatedPed = m_pTargetVehicle->GetSeatManager()->GetPedInSeat(seatIndex);
							if(CheckIfPedIsSuitableForArrest(bIgnoreInput, pSeatedPed))
							{
								return true;
							}
						}
					}
				}
			}
		}
	}
	else
	{
		// Find any nearby network players
		const netPlayer *closestPlayers[MAX_NUM_PHYSICAL_PLAYERS];
		const bool bSortResults = true;

		u32 numPlayersInRange = NetworkInterface::GetClosestRemotePlayers(pPed->GetGroundPos(), sm_Tunables.m_ArrestDistance, closestPlayers, bSortResults);
		if (numPlayersInRange == 0)
		{
			return false;
		}

		for (u32 playerIndex = 0; playerIndex < numPlayersInRange; playerIndex++)
		{
			CPed* pTargetPed;
			const CNetGamePlayer* pNetGamePlayer = SafeCast(const CNetGamePlayer, closestPlayers[playerIndex]);
			if (pNetGamePlayer)
			{
				pTargetPed = pNetGamePlayer->GetPlayerPed();
				if(CheckIfPedIsSuitableForArrest(bIgnoreInput, pTargetPed))
				{
					return true;
				}
			}
		}
	}
#endif //CNC_MODE_ENABLED
	return false;
}

bool CTaskPlayerOnFoot::CheckIfPedIsSuitableForArrest(bool CNC_MODE_ENABLED_ONLY(bIgnoreInput), CPed* CNC_MODE_ENABLED_ONLY(pTargetPed))
{
#if CNC_MODE_ENABLED
	CPed* pPed = GetPed();

	// Confirm player is an arrestable type
	if (!pTargetPed || !pTargetPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CanBeArrested) || pTargetPed->IsDead())
	{
		return false;
	}

	CTaskIncapacitated* incapTask = (CTaskIncapacitated*)pTargetPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_INCAPACITATED);

	// Confirm player is incapacitated
	if (incapTask && incapTask->IsArrestableByPed(pPed))
	{
		// Prevent sending duplicate arrests
		if (m_pLastTargetPed && m_pLastTargetPed == pTargetPed)
		{
			return false;
		}

		//Check if ped we are trying to arrest is fading away or not visible
		CNetObjPed* pPedObj = SafeCast(CNetObjPed, pTargetPed->GetNetworkObject());
		if(pPedObj)
		{
			TUNE_GROUP_FLOAT(CNC_ARREST, fTimeToStopArrestDuringCrookFade, 1.0f, 0.0f, 100.0f, 0.01f);
			if((pPedObj->IsFadingOut() && pPedObj->GetAlphaRampingTimeRemaining() < fTimeToStopArrestDuringCrookFade) || !pTargetPed->IsVisible())
			{
				return false;
			}
		}

		CControl& rControl = CControlMgr::GetMainPlayerControl();
		rControl.SetInputExclusive(INPUT_ARREST);

		if (bIgnoreInput || pPed->GetControlFromPlayer()->GetPedArrest().IsPressed())
		{
			m_pTargetPed = pTargetPed;
			m_pLastTargetPed = pTargetPed;
			return true;
		}
	}
	else if (m_pLastTargetPed && m_pLastTargetPed == pTargetPed)
	{
		// If Ped is not running Incapacitated task in the Incapacitated stage but was the last target then clear that target
		// This will allow them to be arrestable again if they enter a suitable state
		m_pLastTargetPed = NULL;
		return false;
	}
#endif
	return false;
}

bool CTaskPlayerOnFoot::CNC_PlayerCopShouldTriggerActionMode(const CPed* pPlayerPed)
{
	if (CPlayerInfo::AreCNCResponsivenessChangesEnabled(pPlayerPed) && pPlayerPed->GetPlayerInfo()->GetArcadeInformation().GetTeam() == eArcadeTeam::AT_CNC_COP)
	{
		const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();
		for(unsigned index = 0; index < netInterface::GetNumPhysicalPlayers(); index++)
		{
			const CNetGamePlayer* pNetGamePlayer = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);
			CPed* pTargetPlayer = pNetGamePlayer->GetPlayerPed();
			if (pTargetPlayer && pTargetPlayer != pPlayerPed)
			{
				const CPlayerInfo* pTargetPlayerInfo = pTargetPlayer->GetPlayerInfo();
				if (pTargetPlayerInfo && pTargetPlayerInfo->GetArcadeInformation().GetTeam() == eArcadeTeam::AT_CNC_CROOK)
				{
					const CWanted& rTargetPlayerWanted = pTargetPlayerInfo->GetWanted();
					if (rTargetPlayerWanted.m_WantedLevel >= WANTED_LEVEL1)
					{
						TUNE_GROUP_FLOAT(CNC_RESPONSIVENESS, fCopActionModeMinDistanceToCrook, 80.0f, 0.0f, 300.0f, 1);
						const float fDistanceToTarget = Mag(pTargetPlayer->GetTransform().GetPosition() - pPlayerPed->GetTransform().GetPosition()).Getf();
						if (fDistanceToTarget < fCopActionModeMinDistanceToCrook)
						{
							if (pTargetPlayer->IsPedVisible())
							{
								return true;
							}
						}
					}
				}
			}
		}
	}

	return false;
}

void CTaskPlayerOnFoot::Arrest_OnEnter(CPed* UNUSED_PARAM(pPlayerPed))
{
	Assert(m_pTargetArrestPed);

#if ENABLE_TASKS_ARREST_CUFFED
	switch(m_eArrestType)
	{
	case(eAT_Cuff):
		{
			CTaskArrest* pArrestTask = rage_new CTaskArrest(m_pTargetArrestPed);
			SetNewTask(pArrestTask);
		}
		break;
	case(eAT_PutInCustody):
		{
			CTaskArrest* pArrestTask = rage_new CTaskArrest(m_pTargetArrestPed, AF_TakeCustody);
			SetNewTask(pArrestTask);
		}
		break;
	default:
		break;
	}
#endif // ENABLE_TASKS_ARREST_CUFFED

	m_bWaitingForPedArrestUp = true;

	m_eArrestType = eAT_None;
}

CTask::FSM_Return CTaskPlayerOnFoot::Arrest_OnUpdate(CPed* UNUSED_PARAM(pPlayerPed))
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(STATE_PLAYER_IDLES);
	}

	return FSM_Continue;
}

bool CTaskPlayerOnFoot::CanPerformUncuff()
{
	CPed *pPlayerPed = GetPed();

	//! Script set this flag when we give the ped cuff keys.
	if (!pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CanPerformUncuff))
	{
		return false;
	}

	//! Don't uncuff is we ourselves are cuffed!
	if (pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed))
	{
		return false;
	}

	CPed* pPedToUncuff = NULL;

	CEntity* pTargetEntity = pPlayerPed->GetPlayerInfo()->GetTargeting().GetTarget();
	if (pTargetEntity && pTargetEntity->GetIsTypePed())
	{
		CPed* pTargetPed = static_cast<CPed*>(pTargetEntity);

		float fScore = 0.0f;
		if(CanUncuffPed(pTargetPed, fScore))
		{
			pPedToUncuff = pTargetPed;
		}
	}
	else
	{
		CPed* pBestTargetPed = NULL;
		float fBestScore = 0.0f;
		float fScore = 0.0f;

		CEntityScannerIterator pedList = pPlayerPed->GetPedIntelligence()->GetNearbyPeds();
		for(CEntity* pEntity = pedList.GetFirst(); pEntity; pEntity = pedList.GetNext())
		{
			CPed *pOtherPed = static_cast<CPed *>(pEntity);

			if(!CanUncuffPed(pOtherPed, fScore))
			{
				continue;
			}

			if(fScore > fBestScore)
			{
				pBestTargetPed = pOtherPed;
				fBestScore = fScore;
			}
		}

		pPedToUncuff = pBestTargetPed;
	}

	if(pPedToUncuff)
	{
		m_pTargetUncuffPed = pPedToUncuff;
		return true;
	}	

	return false;
}

bool CTaskPlayerOnFoot::CanUncuffPed(const CPed *pOtherPed, float &fOutScore) const
{
	const CPed *pPlayerPed = GetPed();

	Vector3 vPlayerForward = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetB());

	const float fMaxUncuffDistance = sm_Tunables.m_ArrestDistance;
	const float fMaxUncuffDot = sm_Tunables.m_ArrestDot;

	if(pOtherPed->IsDead())
	{
		return false;
	}
	
	//Must be handcuffed.
	if(!pOtherPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed))
	{
		return false;
	}

	//Must not be ragdolled.
	if(pOtherPed->GetUsingRagdoll())
	{
		return false;
	}

	//Don't uncuff in vehicles.
	if(pOtherPed->GetIsInVehicle())
	{
		return false;
	}

	//Must be friends to uncuff. Note: Script only want this check for players.
	if(pOtherPed->IsAPlayerPed() && !pPlayerPed->GetPedIntelligence()->IsFriendlyWith(*pOtherPed))
	{
		return false;
	}

	//Must be within distance tolerance.
	float fDistance = Dist(pOtherPed->GetTransform().GetPosition(), pPlayerPed->GetTransform().GetPosition()).Getf();
	if(fDistance > fMaxUncuffDistance)
	{
		return false;
	}

	//Must be within angle tolerance.
	Vector3 vDir = VEC3V_TO_VECTOR3(pOtherPed->GetTransform().GetPosition() - GetPed()->GetTransform().GetPosition());
	vDir.NormalizeFast();

	float fDot = vDir.Dot(vPlayerForward);
	if(fDot <= fMaxUncuffDot)
	{
		return false;
	}

	// LOS check.
	const u32 includeFlags = ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_VEHICLE_TYPE;
	static const s32 iNumExceptions = 2;
	const CEntity* ppExceptions[iNumExceptions] = { NULL };
	ppExceptions[0] = pPlayerPed;
	ppExceptions[1] = pOtherPed;
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetStartAndEnd(VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition()), VEC3V_TO_VECTOR3(pOtherPed->GetTransform().GetPosition()));
	probeDesc.SetIncludeFlags(includeFlags);
	probeDesc.SetExcludeEntities(ppExceptions,iNumExceptions);
	probeDesc.SetExcludeTypeFlags(ArchetypeFlags::GTA_PICKUP_TYPE);
	probeDesc.SetContext(WorldProbe::LOS_GeneralAI);
	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
		return false;
	}

	static dev_float s_fDotFactor = 1.0f;
	static dev_float s_fDistanceFactor = 1.0f;

	fOutScore = (fDot * s_fDotFactor) + (s_fDistanceFactor * (1.0f - (fDistance/fMaxUncuffDistance)));

	return true;
}

bool CTaskPlayerOnFoot::CheckForAirMount(CPed* pPlayerPed)
{
	//mounting from above
	Vector3 vSearchDirection = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetB());
	CPed* pMountableAnimal = const_cast<CPed*>(CPlayerInfo::ScanForAnimalToRide(pPlayerPed, vSearchDirection, false, true));
	if (pMountableAnimal)
	{
		Vector3 vHorsePosition = VEC3V_TO_VECTOR3(pMountableAnimal->GetTransform().GetPosition());
		static dev_float sf_DropFlatHorseRange = 0.75f * 0.75f;
		static dev_float sf_JumpFlatHorseRange = 2.0f * 2.0f;
		static dev_float sf_DropZHorseRange = 1.75f;
		static dev_float sf_JumpZHorseRange = 2.5f;
		static dev_float sf_MinJumpZHorseRange = 1.0f;
		float fFlatHorseRange = GetState() == STATE_DROPDOWN ? sf_DropFlatHorseRange : sf_JumpFlatHorseRange;
		float fZHorseRange = GetState() == STATE_DROPDOWN ? sf_DropZHorseRange : sf_JumpZHorseRange;
		vHorsePosition.Subtract(VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition()));
		if (vHorsePosition.z >= -fZHorseRange && vHorsePosition.z <= -sf_MinJumpZHorseRange)
		{
			vHorsePosition.z=0;
			if (vHorsePosition.Mag2() <= fFlatHorseRange )
			{
				m_pTargetAnimal = pMountableAnimal;
				return true;
			}			
		}			
	}
	return false;
}

bool CTaskPlayerOnFoot::CheckShouldPerformUncuff()
{
	if(GetPed()->GetControlFromPlayer()->GetPedArrest().IsUp())
	{
		m_bWaitingForPedArrestUp = false;
	}

	if(!m_bWaitingForPedArrestUp)
	{
		bool bCanUncuff = CanPerformUncuff(); 
		if (bCanUncuff && GetPed()->GetControlFromPlayer()->GetPedArrest().IsDown())
		{
			return true;
		}
	}

	return false;
}

s8 CTaskPlayerOnFoot::CheckShouldEnterVehicle(CPed* pPlayerPed,CControl *pControl, bool bForceControl)
{
	m_pTargetVehicle = NULL;
	const bool bEnterPressed = HasValidEnterVehicleInput(*pControl) || bForceControl;

	bool bEnterTapped = false;
	CPlayerInfo* pPlayerInfo = pPlayerPed->GetPlayerInfo();
	if (pPlayerInfo && pPlayerInfo->GetLastTimeWarpedOutOfVehicle() + CTaskEnterVehicle::TIME_TO_DETECT_ENTER_INPUT_TAP < fwTimer::GetTimeInMilliseconds())
	{
		bEnterTapped = pControl && pControl->GetPedEnter().HistoryPressed(CTaskEnterVehicle::TIME_TO_DETECT_ENTER_INPUT_TAP) && pControl->GetPedEnter().IsReleased();
	}
	

	if(pPlayerPed->GetMotionData() && pPlayerPed->GetMotionData()->GetCombatRoll())
	{
		return -1;
	}

	if(pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedIsArresting))
	{
		return -1;
	}

	const bool bInputAllowsVehicleSearch = bEnterPressed || (bEnterTapped && pPlayerPed->IsGoonPed());
	if (pPlayerPed->IsGoonPed() && !bEnterTapped)
	{
		AI_LOG_WITH_ARGS("[BossGoonVehicle] Not allowing Goon player %s[%p] to search for vehicles because he's holding enter/exit button\n",
			AILogging::GetDynamicEntityNameSafe(pPlayerPed), pPlayerPed);
	}

	if( (!pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_PedExitedVehicleThisFrame) && bInputAllowsVehicleSearch) || CTaskPlayerOnFoot::ms_bRenderPlayerVehicleSearch  )
	{
		if(!pPlayerPed->GetIsSkiing())
		{
			// Use the most recent stick input to search for a vehicle to enter (if within TIME_TO_CONSIDER_OLD_INPUT_VALID seconds)
			Vector3 vCarSearchDirection = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetB());
			bool bUsingStickInput = HasPlayerSelectedGunTask(pPlayerPed, GetState()) ? false : m_fTimeSinceLastStickInput < TIME_TO_CONSIDER_OLD_INPUT_VALID;
			if( bUsingStickInput )
				vCarSearchDirection = m_vLastStickInput;

#if DEBUG_DRAW
			if( CTaskPlayerOnFoot::ms_bRenderPlayerVehicleSearch )
			{
				grcDebugDraw::Line(VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition()), VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition()) + m_vLastStickInput, bUsingStickInput?Color_green:Color_red, bUsingStickInput?Color_green:Color_red);
			}
#endif // DEBUG_DRAW


			m_pTargetVehicle = const_cast<CVehicle*>(CPlayerInfo::ScanForVehicleToEnter(pPlayerPed, vCarSearchDirection, bUsingStickInput));

			if ((m_pTargetVehicle && m_pTargetVehicle->m_bSpecialEnterExit) && pPlayerPed->IsGoonPed() && !bEnterTapped)
			{
				AI_LOG("[BossGoonVehicle] - Nulling vehicle as player is goon and enter is not tapped");
				m_pTargetVehicle = NULL;
				return -1;
			}
			else if (pPlayerPed->IsGoonPed() && m_pTargetVehicle)
			{
				AI_LOG_WITH_ARGS("[BossGoonVehicle] Goon player %s[%p] will enter vehicle %s: IsSpecialVeh - %s, bTapped - %s\n",
					AILogging::GetDynamicEntityNameSafe(pPlayerPed), pPlayerPed, m_pTargetVehicle->GetModelName(),
					AILogging::GetBooleanAsString(m_pTargetVehicle->m_bSpecialEnterExit),AILogging::GetBooleanAsString(bEnterTapped));
			}
		}
	}

#if __BANK
	DebugVehicleGetIns();
#endif 

#if DEBUG_DRAW
	if( CTaskPlayerOnFoot::ms_bRenderPlayerVehicleSearch && m_pTargetVehicle )
	{
		grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(m_pTargetVehicle->GetTransform().GetPosition())+ZAXIS, 0.5f, Color_green);
		grcDebugDraw::Line(VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition()), VEC3V_TO_VECTOR3(m_pTargetVehicle->GetTransform().GetPosition()), Color_blue);
	}
#endif // DEBUG_DRAW

	//-------------------------------------------------------------------------
	// ENTERING CARS, search for vehicles when enter pressed for picking up objects
	//
	// Currently disabled when swimming.
	//-------------------------------------------------------------------------

	//Ridable animal check
	if( bEnterPressed || (bEnterTapped && pPlayerPed->IsGoonPed())) 
	{
		Vector3 vSearchDirection = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetB());
		bool bUsingStickInput = m_fTimeSinceLastStickInput < TIME_TO_CONSIDER_OLD_INPUT_VALID;
		m_pTargetAnimal = const_cast<CPed*>(CPlayerInfo::ScanForAnimalToRide(pPlayerPed, vSearchDirection, bUsingStickInput));;

		if ( m_pTargetVehicle && CheckCanEnterVehicle(m_pTargetVehicle))
		{
			if ( m_pTargetAnimal ) {
				Vector3 vPedPos = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());
				Vector3 vFlatDist = VEC3V_TO_VECTOR3(m_pTargetAnimal->GetTransform().GetPosition()) - vPedPos;
				vFlatDist.z=0;
				const float animalDist =vFlatDist.Mag2();

				vFlatDist = VEC3V_TO_VECTOR3(m_pTargetVehicle->GetTransform().GetPosition()) - vPedPos;
				vFlatDist.z=0;
				const float vehDist =vFlatDist.Mag2();
				if ( animalDist < vehDist)
					return STATE_MOUNT_ANIMAL;
			}
			return STATE_GET_IN_VEHICLE;
		}
	}

	if (m_pTargetAnimal)
		return STATE_MOUNT_ANIMAL;

	return -1;
}

bool CTaskPlayerOnFoot::CheckShouldRideTrain() const
{
	//Ensure the player is inside a moving train.
	if(!GetPed()->GetPlayerInfo()->IsInsideMovingTrain())
	{
		return false;
	}

	//Ensure control is not allowed inside the moving train.
	if(GetPed()->GetPlayerInfo()->AllowControlInsideMovingTrain())
	{
		return false;
	}

	return true;
}

bool CTaskPlayerOnFoot::CheckShouldSlopeScramble()
{
	CPed* pPlayerPed = GetPed();

	if( !pPlayerPed->GetIsSwimming() && CTaskSlopeScramble::CanDoSlopeScramble(pPlayerPed) )
	{
		//Do the same checks whilst aiming as not aiming to keep the scramble consistent
		m_fTimeSinceLastValidSlopeScramble += GetTimeStep();
		++m_nFramesOfSlopeScramble;
		float TimeBeforeScramble = CTaskSlopeScramble::GetTimeBeforeScramble(pPlayerPed->GetGroundNormal(), pPlayerPed->GetVelocity(), ZAXIS);

		TUNE_GROUP_FLOAT (SLOPE_SCRAMBLE, fDisabledTimerGroundZGeneral, 0.6f, 0.0f, 1.0f, 0.01f);

		// Don't consider time if we are not moving
		if ((m_fTimeSinceLastValidSlopeScramble > TimeBeforeScramble || abs(pPlayerPed->GetGroundNormal().z) > fDisabledTimerGroundZGeneral) && m_nFramesOfSlopeScramble > 4 )
		{
			return true;
		}
	}
	else
	{
		//we're no longer on a slope, reset. 
		m_fTimeSinceLastValidSlopeScramble = 0.0f;
		m_nFramesOfSlopeScramble = 0;
	}

	return false;
}

bool CTaskPlayerOnFoot::CanReachLadder(const CPed* pPed, const CEntity* pLadder, const Vector3& vLadderEmbarkPos)
{
	TUNE_GROUP_BOOL( LADDER_DEBUG, bShowReachLadderProbes, false );
	TUNE_GROUP_FLOAT( LADDER_DEBUG, fReachLadderProbeOffset, 0.720f, 0.0f, 10.0f, 0.1f );
	TUNE_GROUP_FLOAT( LADDER_DEBUG, fReachLadderProbeRadius, 0.140f, 0.0f, 10.0f, 0.1f );

	if( m_LadderProbeResults.GetResultsReady() )
	{
		// Get the results of the test, we can only reach the door if no
		// collisions were found
		m_bCanReachLadder = m_LadderProbeResults.GetNumHits() == 0;
		// Reset the probe state for the next try
		m_LadderProbeResults.Reset();
	}
	else if( !m_LadderProbeResults.GetWaitingOnResults() ) 
	{
		// Get the feet position of the ped as we will be offsetting the probes from that point
		Vector3 vPedFeetPos = VEC3V_TO_VECTOR3( pPed->GetTransform().GetPosition() );
		vPedFeetPos.z -= PED_HUMAN_GROUNDTOROOTOFFSET;

		// The capsule will lie in the xy plane (we're not really interested in height)
		Vector3 vStart = vPedFeetPos;
		vStart.z += fReachLadderProbeOffset;
		Vector3 vEnd = vLadderEmbarkPos;
		vEnd.z = vStart.z;

		// Set the values of the capsule descriptor to do the probe
		WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
		// Set the results structure
		capsuleDesc.SetResultsStructure(&m_LadderProbeResults);
		// Set the capsule dimensions
		capsuleDesc.SetCapsule(vStart, vEnd, fReachLadderProbeRadius);
		// Set collision flags
		u32 uCollisionFlags = (u32)ArchetypeFlags::GTA_FOLIAGE_TYPE | (u32)ArchetypeFlags::GTA_PICKUP_TYPE;
		capsuleDesc.SetIncludeFlags( ~uCollisionFlags );
		// Set the test as a sweep test and make an initial overlap test
		capsuleDesc.SetIsDirected(true);
		capsuleDesc.SetDoInitialSphereCheck(true);
		// We are not interested in the ped or the ladder
		const u8 nbExcludedEntities = 2;
		const CEntity* excludedEntitiesArray[nbExcludedEntities] = { pPed, pLadder };
		capsuleDesc.SetExcludeEntities(excludedEntitiesArray, nbExcludedEntities);
		// Which part of the code is doing this probe? (for debugging)
		capsuleDesc.SetContext(WorldProbe::LOS_GeneralAI);
		// If there is something in the way, we shouldn't be able to reach the ladder
		WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
	}

	bool bDoVerticalProbe = false;
	if (pPed->GetIsSwimming())
	{
		bDoVerticalProbe = true;
	}
	else
	{
		m_bLadderVerticalClear = true;
	}

	if (bDoVerticalProbe)
	{
		if( m_LadderVerticalProbeResults.GetResultsReady() )
		{
			// Get the results of the test, we can only reach the door if no
			// collisions were found
			m_bLadderVerticalClear = m_LadderVerticalProbeResults.GetNumHits() == 0;

			//url:bugstar:6434652 Check if this hit a ladder on the yacht so we can reduce the probe width on the next check
			if(!m_bLadderVerticalClear)
			{
				for(auto result = m_LadderVerticalProbeResults.begin(); result != m_LadderVerticalProbeResults.end(); ++result)
				{
					if(result->GetHitEntity())
					{
						//url:bugstar:6554278 Check if this is a yatch ladder we are trying to climb on. This will allow us to climb on from half way up the ladder
						u32 uResultHash = result->GetHitEntity()->GetModelNameHash();
						if(uResultHash == ATSTRINGHASH("apa_mp_apa_yacht_o1_rail_a", 0xABCF4AAA)
							|| uResultHash == ATSTRINGHASH("apa_mp_apa_yacht_o2_rail_a", 0x75F724B2) || uResultHash == ATSTRINGHASH("apa_mp_apa_yacht_o3_rail_a", 0x620CF1E2)
							|| uResultHash == ATSTRINGHASH("apa_mp_apa_yacht_o1_rail_b", 0xA599BE3F) || uResultHash == ATSTRINGHASH("apa_mp_apa_yacht_o2_rail_b", 0x840EC0E1)
							|| uResultHash == ATSTRINGHASH("apa_mp_apa_yacht_o3_rail_b", 0x54C3574F))
						{
							m_bUseSmallerVerticalProbeCheck = true;
						}
					}
				}
			}


			// Reset the probe state for the next try
			m_LadderVerticalProbeResults.Reset();
		}
		else if( !m_LadderVerticalProbeResults.GetWaitingOnResults() ) 
		{
			//If ped in water, do a shapetest vertically along the ladder against vehicles to see if it's blocked
			const CEntity* pParent = (CEntity*)pLadder->GetAttachParent();
			Vector3 vUp = pParent ? VEC3V_TO_VECTOR3(pParent->GetTransform().GetUp()) : Vector3(0.0f, 0.0f, 1.0f);
			vUp.Normalize();
			Vector3 vEnd = vLadderEmbarkPos + vUp * pPed->GetCapsuleInfo()->GetMaxSolidHeight() * 2;

			// Set the values of the capsule descriptor to do the probe
			WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
			// Set the results structure
			capsuleDesc.SetResultsStructure(&m_LadderVerticalProbeResults);

			//Reduce the size of the probe for the yacht ladder
			float fReduceProbeWidthAmount = 0;
			if(m_bUseSmallerVerticalProbeCheck)
			{
				TUNE_GROUP_FLOAT( LADDER_DEBUG, fLadderVerticalProbecheckReduceWidth, 0.050f, 0.0f, 1000.0f, 0.1f );
				fReduceProbeWidthAmount = fLadderVerticalProbecheckReduceWidth;
				m_bUseSmallerVerticalProbeCheck = false;
			}
			
			// Set the capsule dimensions
			capsuleDesc.SetCapsule(vLadderEmbarkPos, vEnd, (pPed->GetCapsuleInfo()->GetHalfWidth() - fReduceProbeWidthAmount));
			// Set collision flags
			s32 uCollisionFlags = ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE;
			capsuleDesc.SetIncludeFlags( uCollisionFlags );
			// Set the test as a sweep test and make an initial overlap test
			capsuleDesc.SetIsDirected(true);
			capsuleDesc.SetDoInitialSphereCheck(true);
			// We are not interested in the ped or the ladder
			const u8 nbExcludedEntities = 3;
			// Exclude the entity that ladder is attached to in case it's a boat to avoid false positives
			const CEntity* excludedEntitiesArray[nbExcludedEntities] = { pPed, pLadder, pParent };
			capsuleDesc.SetExcludeEntities(excludedEntitiesArray, nbExcludedEntities);
			// Which part of the code is doing this probe? (for debugging)
			capsuleDesc.SetContext(WorldProbe::LOS_GeneralAI);
			// If there is something in the way, we shouldn't be able to reach the ladder
			WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
		}
	}

#if DEBUG_DRAW
	Color32 capsuleColor = m_bCanReachLadder ? Color_green : Color_red;
	Color32 verticalCapsuleColor = m_bLadderVerticalClear ? Color_green : Color_red;
	if(bShowReachLadderProbes)
	{
		// Get the feet position of the ped as we will be offsetting the probes from that point
		Vector3 vPedFeetPos = VEC3V_TO_VECTOR3( pPed->GetTransform().GetPosition() );
		vPedFeetPos.z -= PED_HUMAN_GROUNDTOROOTOFFSET;

		// The capsule will lie in the xy plane (we're not really interested in height)
		Vector3 vStart = vPedFeetPos;
		vStart.z += fReachLadderProbeOffset;
		Vector3 vEnd = vLadderEmbarkPos;
		vEnd.z = vStart.z;

		// Draw the capsule test
		Vec3V vDrawStart = VECTOR3_TO_VEC3V(vStart);
		Vec3V vDrawEnd = VECTOR3_TO_VEC3V(vEnd);
		grcDebugDraw::Capsule( vDrawStart, vDrawEnd, fReachLadderProbeRadius, capsuleColor, false, -1 );

		if (bDoVerticalProbe)
		{
			const CEntity* pParent = (CEntity*)pLadder->GetAttachParent();
			Vector3 vUp = pParent ? VEC3V_TO_VECTOR3(pParent->GetTransform().GetUp()) : Vector3(0.0f, 0.0f, 1.0f);
			vUp.Normalize();
			Vector3 vEnd = vLadderEmbarkPos + vUp * pPed->GetCapsuleInfo()->GetMaxSolidHeight() * 2;

			Vec3V vDrawStart = VECTOR3_TO_VEC3V(vLadderEmbarkPos);
			Vec3V vDrawEnd = VECTOR3_TO_VEC3V(vEnd);
			grcDebugDraw::Capsule( vDrawStart, vDrawEnd, pPed->GetCapsuleInfo()->GetHalfWidth(), verticalCapsuleColor, false, -1 );
		}
	}
#endif

	return m_bCanReachLadder && m_bLadderVerticalClear;
}

bool CTaskPlayerOnFoot::LaddersAreClose(CEntity* pTargetLadder, s32 iLadderIndex, const Vector2& vHackedLadderPos, float fSearchRadius)
{
	bool canGetOffAtTop = false;
	Vector3 vTop(VEC3_ZERO), vBottom(VEC3_ZERO), vLadderForward(VEC3_ZERO);
	if( CTaskGoToAndClimbLadder::FindTopAndBottomOfAllLadderSections(pTargetLadder, iLadderIndex, vTop, vBottom, vLadderForward, canGetOffAtTop) )
	{
		// Get the position of the detected ladder and compare it with the position of
		// the dreaded ladder (flatten the vectors, we are not interested in the vertical
		// component)
		const Vector2 vDetectedLadder( vBottom.x, vBottom.y );

		// Get the distance to the bad ladder
		const Vector2 vDetectedLadderToHackedLadder = vHackedLadderPos - vDetectedLadder;
		const float fSqrdDistToHackLadder = vDetectedLadderToHackedLadder.Mag2();

		// If the found ladder is the dreaded ladder, return false (we'll consider a ladder to
		// be the bad ladder if the found ladder is closer to half a meter to the known position)
		if( fSqrdDistToHackLadder < fSearchRadius * fSearchRadius )
			return true;
	}

	return false;
}

bool CTaskPlayerOnFoot::CheckShouldClimbLadder(CPed* pPlayerPed)
{
#if __BANK
	TUNE_GROUP_BOOL( LADDER_DEBUG, bLogCheckShouldClimbLadder, false );
	if(bLogCheckShouldClimbLadder)
	{
		Displayf( "START OF - CTaskPlayerOnFoot::CheckShouldClimbLadder" );
	}
#endif

	//******************************************************************
	//Ladder
	Vector2 vDesiredMoveRatio; 
	pPlayerPed->GetMotionData()->GetDesiredMoveBlendRatio(vDesiredMoveRatio);

	bool bPassedActionModeCondition = false;
	const CTaskReloadGun* pReloadTask = static_cast<const CTaskReloadGun*>(FindSubTaskOfType(CTaskTypes::TASK_RELOAD_GUN));
	const bool bIsReloading = pReloadTask && !pReloadTask->IsReloadFlagSet(RELOAD_IDLE);
	const bool bUseEasyLadderConditions = NetworkInterface::IsGameInProgress() && CTaskGoToAndClimbLadder::ms_bUseEasyLadderConditionsInMP && !pPlayerPed->GetIsSwimming() && (pPlayerPed->IsUsingActionMode() || pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsAimingGun) || bIsReloading);
	if (bUseEasyLadderConditions)
	{
		Vector2 vCurrentMoveBlendRatio; 
		pPlayerPed->GetMotionData()->GetCurrentMoveBlendRatio(vCurrentMoveBlendRatio);
		bPassedActionModeCondition = vCurrentMoveBlendRatio.Mag2() != MOVEBLENDRATIO_STILL;
	}

	if(pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedIsArresting))
	{
#if __BANK
		if(bLogCheckShouldClimbLadder)
		{
			Displayf( "RETURN FALSE: CPED_CONFIG_FLAG_PedIsArresting == true" );
		}
#endif

		return false;
	}

	// Animals have no business climbing ladders.
	if (pPlayerPed->GetPedType() == PEDTYPE_ANIMAL)
	{
#if __BANK
		if(bLogCheckShouldClimbLadder)
		{
			Displayf( "RETURN FALSE: pPlayerPed->GetPedType() == PEDTYPE_ANIMAL" );
		}
#endif

		return false;
	}

	const bool bDisableLadderClimbingFlag = pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableLadderClimbing);
	const bool bPedWantsToMove = vDesiredMoveRatio.Mag2() != MOVEBLENDRATIO_STILL;
	if( (bPedWantsToMove || bPassedActionModeCondition) && !bDisableLadderClimbingFlag ) //check for pushing forward
	{
		Vector3 vGetOnPosition(VEC3_ZERO); 
		Vector3 vLadderBottomPos(VEC3_ZERO);

		m_pTargetLadder = CTaskGoToAndClimbLadder::ScanForLadderToClimb(*pPlayerPed, m_iLadderIndex, vGetOnPosition, NetworkInterface::IsGameInProgress());
		
#if LADDER_HACKS
		// There is a couple of ladders in game that shouldn't be climbed
		// since art can't out these ladders at this stage, we'll just
		// ignore them from code.
		if(m_pTargetLadder != NULL)
		{
			const Vector2 vHackedLadderPos1(2362.1f, 4938.9f), vHackedLadderPos2(934.0f, -3032.6f);
			const float fSearchRadius = 0.5f;

			// Checks for sp and mp ladders
			if( LaddersAreClose(m_pTargetLadder, m_iLadderIndex, vHackedLadderPos1, fSearchRadius) )
			{
				return false;
			}
			// Checks for mp ladders
			else if( NetworkInterface::IsGameInProgress() )
			{
				if( LaddersAreClose(m_pTargetLadder, m_iLadderIndex, vHackedLadderPos2, fSearchRadius) )
				{
					return false;
				}
			}
		}
#endif
		
		if(m_pPreviousTargetLadder != m_pTargetLadder)
		{
			m_pPreviousTargetLadder = m_pTargetLadder;
			m_LadderProbeResults.Reset();
			m_LadderVerticalProbeResults.Reset();
			m_bCanReachLadder = false;
			m_bLadderVerticalClear = false;
		}
		m_bGettingOnBottom = CTaskGoToAndClimbLadder::IsGettingOnBottom(m_pTargetLadder, *pPlayerPed, m_iLadderIndex, &vLadderBottomPos);

		// We also do this check in CTaskGoToAndClimbLadder which will cause an inf loop if that fail
		if (vLadderBottomPos.z - pPlayerPed->GetTransform().GetPosition().GetZf() > 0.25f)
		{
#if __BANK
			if(bLogCheckShouldClimbLadder)
			{
				Displayf("vLadderBottomPos.z = %f", vLadderBottomPos.z);
				Displayf("pPlayerPed->GetTransform().GetPosition().GetZf() = %f", pPlayerPed->GetTransform().GetPosition().GetZf());
				Displayf("RETURN FALSE: vLadderBottomPos.z - pPlayerPed->GetTransform().GetPosition().GetZf() > 0.25f returned true");
			}
#endif

			return false;
		}

		if (pPlayerPed->GetWeaponManager() && pPlayerPed->GetWeaponManager()->GetEquippedWeapon() && pPlayerPed->GetWeaponManager()->GetEquippedWeapon()->GetIsCooking())
		{
#if __BANK
			if(bLogCheckShouldClimbLadder)
			{
				Displayf("RETURN FALSE: pPlayerPed->GetWeaponManager()->GetEquippedWeapon()->GetIsCooking() returned true");
			}
#endif

			return false; // B* 1536944 [7/5/2013 musson] 
		}

		if(m_pTargetLadder != NULL)
		{
			// Can we physically reach the ladder?
			bool canGetOffAtTop = false;
			Vector3 vTop(VEC3_ZERO), vBottom(VEC3_ZERO), vLadderForward(VEC3_ZERO);
			if( CTaskGoToAndClimbLadder::FindTopAndBottomOfAllLadderSections(m_pTargetLadder, m_iLadderIndex, vTop, vBottom, vLadderForward, canGetOffAtTop) )
			{
				TUNE_GROUP_FLOAT( LADDER_DEBUG, fLadderBottomProbeOffset, 0.280f, 0.0f, 10.0f, 0.1f );
				TUNE_GROUP_FLOAT( LADDER_DEBUG, fLadderTopProbeOffset, -0.330f, -10.0f, 0.0f, 0.1f );

				// Get the ladder pos
				Vector3 vLadderPos = m_bGettingOnBottom ? vBottom : vTop;
				// Set the offset for the probe
				const float fLadderOffset = m_bGettingOnBottom ? fLadderBottomProbeOffset : fLadderTopProbeOffset;
				// Ladder bottom offset
				vLadderPos += vLadderForward * fLadderOffset;
				// If the ladder is physically blocked (cars, peds...) abort
				if( !CanReachLadder(pPlayerPed, m_pTargetLadder, vLadderPos) )
				{
#if __BANK
					if(bLogCheckShouldClimbLadder)
					{
						Displayf("RETURN FALSE: CanReachLadder(pPlayerPed, m_pTargetLadder, vLadderPos) returned false");
					}
#endif

					return false;
				}
			}
		}

		u32 LadderBlockFlag = (m_bGettingOnBottom ? CTaskClimbLadderFully::BF_CLIMBING_UP : CTaskClimbLadderFully::BF_CLIMBING_DOWN);
		if(NetworkInterface::IsGameInProgress())
		{
			if(m_pTargetLadder)
			{
				// use a different system involving freezing the ped is he's getting on at the top to prevent the ped running over an edge and killing himself.
				if(m_bGettingOnBottom)
				{
					if(CTaskClimbLadderFully::IsLadderBaseOrTopBlockedByPedMP(m_pTargetLadder, m_iLadderIndex, LadderBlockFlag))
					{
#if __BANK
						if(bLogCheckShouldClimbLadder)
						{
							Displayf("RETURN FALSE: CTaskClimbLadderFully::IsLadderBaseOrTopBlockedByPedMP returned true");
						}
#endif

						return false;
					}

					if(CTaskClimbLadderFully::IsLadderPhysicallyBlockedAtBaseMP(pPlayerPed, m_pTargetLadder, m_iLadderIndex))
					{
#if __BANK
						if(bLogCheckShouldClimbLadder)
						{
							Displayf("RETURN FALSE: CTaskClimbLadderFully::IsLadderPhysicallyBlockedAtBaseMP returned true");
						}
#endif

						return false;
					}
				}
			}
		}
		else
		{
			if (CTaskClimbLadderFully::IsLadderBlocked(m_pTargetLadder, m_iLadderIndex, fwTimer::GetTimeInMilliseconds(), LadderBlockFlag))
			{
#if __BANK
				if(bLogCheckShouldClimbLadder)
				{
					Displayf("RETURN FALSE: CTaskClimbLadderFully::IsLadderBlocked returned true");
				}
#endif

				return false;
			}
		}

		Vector3 vToLadder = vGetOnPosition - VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());

		// B* 1431309, prevent mounting ladders on top when too far below the top
		if (!m_bGettingOnBottom && vToLadder.z > 0.f)
		{
#if __BANK
			if(bLogCheckShouldClimbLadder)
			{
				Displayf( "m_bGettingOnBottom = %s", m_bGettingOnBottom ? "true" : "false" );
				Displayf( "vToLadder.z = %f", vToLadder.z );
				Displayf( "RETURN FALSE: !m_bGettingOnBottom && vToLadder.z > 0.f" );
			}
#endif

			return false;
		}

#if DEBUG_DRAW
		if(CClimbLadderDebug::ms_bRenderDebug)
		{
			grcDebugDraw::Sphere(vGetOnPosition, 0.05f, Color_green);
			grcDebugDraw::Line(VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition()),vGetOnPosition, Color_blue);
		}
#endif

		float fLadderDist = vToLadder.XYMag2(); 

		//Near top of Ladder
		TUNE_GROUP_FLOAT(LADDER_DEBUG, NearLadderRadius, 1.5f, 0.0f, 10.0f, 0.01f);
		if ( !m_bGettingOnBottom && fLadderDist < (NearLadderRadius*NearLadderRadius))
			pPlayerPed->SetPedResetFlag(CPED_RESET_FLAG_IsNearLaddder, true);
	

		TUNE_GROUP_FLOAT(LADDER_DEBUG, DistanceToStartClimbingModifier, 0.1f, -10.0f, 10.0f, 0.01f);
		TUNE_GROUP_FLOAT(LADDER_DEBUG, DistanceToStartClimbingModifierBack, 0.2f, -10.0f, 10.0f, 0.01f);

		float fStartDistance = 0.0f; 

		//static dev_float DISTANCE_TO_START_CLIMBING = 0.8f;  
		if(m_pTargetLadder)
		{
			Assertf(NetworkInterface::IsGameInProgress() || m_pLadderClipRequestHelper, "Player got ladder but no m_pLadderClipRequestHelper");
	
			if (m_pLadderClipRequestHelper)
			{
				m_pLadderClipRequestHelper->m_fLastClosestLadder = fLadderDist;
				if (fLadderDist < ms_fStreamClosestLadderMinDistSqr)
				{
					int iStreamStates = 0;
					if (pPlayerPed->GetIsSwimming())
						iStreamStates |= CTaskClimbLadder::NEXT_STATE_WATERMOUNT;

					iStreamStates |= CTaskClimbLadder::NEXT_STATE_CLIMBING;
					CTaskClimbLadder::StreamReqResourcesIn(m_pLadderClipRequestHelper, pPlayerPed, iStreamStates);
				}
			}
			
	/*		Matrix34 mat = MAT34V_TO_MATRIX34 (pPlayerPed->GetTransform().GetMatrix()); 

			mat.MakeRotateZ(fwAngle::LimitRadianAngle(pPlayerPed->GetDesiredHeading()));  

			grcDebugDraw::Axis(mat, 0.5, true);*/
			
			// Should dig out this information from the animations that we don't yet know which ones...
			// Not sure how we should deal with this but with the assumption that we don't really change the run get ons
			// We don't need to bother with this just yet.
			// But when/if we fix it, we might as well deal with foot sync also so we consider that "blend distance"
			TUNE_GROUP_FLOAT(LADDER_DEBUG, RunRangeNear, 2.5f, 0.25f, 5.f, 0.1f);
			TUNE_GROUP_FLOAT(LADDER_DEBUG, RunRangeFar, 2.75f, 0.25f, 5.5f, 0.1f);
			TUNE_GROUP_FLOAT(LADDER_DEBUG, RunRangeNearTop, 2.0f, 0.25f, 5.f, 0.1f);
			TUNE_GROUP_FLOAT(LADDER_DEBUG, RunRangeFarTop, 2.25f, 0.25f, 5.5f, 0.1f);
			TUNE_GROUP_FLOAT(LADDER_DEBUG, WalkStartDistanceBase, 0.8f, 0.f, 1.5f, 0.1f);

			if (m_bGettingOnBottom)
			{
				fStartDistance = WalkStartDistanceBase + (vDesiredMoveRatio.y / 10.0f); 
				if(pPlayerPed->GetIsInWater())
				{
					float fBoundPitch = abs(pPlayerPed->GetBoundPitch());
					
					//If the player is swimming need to add extra to the distance check, because the ped capsule keeps him further
					//from the ladder as it is pitched forward.
					const float PitchBasedCapsuleOffset = 0.5f; 

					float AdjustedForPedCapsule = PitchBasedCapsuleOffset * fBoundPitch; 
					
					//clamp the max offset to 0.5m ahead of the ped. 
					AdjustedForPedCapsule = Clamp(AdjustedForPedCapsule, 0.0f, 0.7f);

					fStartDistance += AdjustedForPedCapsule + 0.1f;

					vDesiredMoveRatio.y = Min(vDesiredMoveRatio.y, MOVEBLENDRATIO_WALK);	// Clamp to walk speed
				}
			}
			else
			{
				fStartDistance = 0.5f + DistanceToStartClimbingModifier + (vDesiredMoveRatio.y / 10.0f); 


				// If we dont face the ladder we must be closer before mounting
				Vector3 vToLadderDir = vLadderBottomPos - VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());
				vToLadderDir.z = 0.f;
				vToLadderDir.Normalize();

				Vector3 vPlayerDir = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetForward());

				if (vPlayerDir.Dot(vToLadderDir) < -0.25f)
					fStartDistance -= DistanceToStartClimbingModifierBack;
			}

			bool bIdealMountTopRunDist = (!m_bGettingOnBottom && !CPedMotionData::GetIsLessThanRun(vDesiredMoveRatio.y) && square(RunRangeNearTop) < fLadderDist && square(RunRangeFarTop) > fLadderDist);
			bool bIdealMountBottomRunDist = (m_bGettingOnBottom && !CPedMotionData::GetIsLessThanRun(vDesiredMoveRatio.y) && square(RunRangeNear) < fLadderDist && square(RunRangeFar) > fLadderDist);

			if (!CTaskClimbLadder::IsReqResourcesLoaded(pPlayerPed, CTaskClimbLadder::NEXT_STATE_HIGHLOD))
			{
				// We only got these anims in "highlod", this is to prevent mounting the ladder from a great distance but
				// only playing the normal get on, not the run one
				bIdealMountTopRunDist = false;
				bIdealMountBottomRunDist = false;
			}

			if (bIdealMountTopRunDist || bIdealMountBottomRunDist)
			{
				CTaskMotionBase* pMotionTask = pPlayerPed->GetCurrentMotionTask();
				if(pMotionTask && !pMotionTask->AllowLadderRunMount())
				{
					bIdealMountTopRunDist = false;
					bIdealMountBottomRunDist = false;
				}

				if (!m_bGettingOnBottom && 
					NetworkInterface::IsGameInProgress() &&
					(CTaskClimbLadderFully::IsLadderBaseOrTopBlockedByPedMP(m_pTargetLadder, m_iLadderIndex, CTaskClimbLadderFully::BF_CLIMBING_DOWN) ||
					CTaskGoToAndClimbLadder::IsLadderTopBlockedMP(m_pTargetLadder, *pPlayerPed, m_iLadderIndex)))
				{
					bIdealMountTopRunDist = false;
					bIdealMountBottomRunDist = false;
				}

				TUNE_GROUP_BOOL(LADDER_DEBUG, PREVENT_RUN_TOP_MOUNT_WHILST_AIMING, true);
				if(PREVENT_RUN_TOP_MOUNT_WHILST_AIMING &&
					!m_bGettingOnBottom &&
					NetworkInterface::IsGameInProgress())
				{
					if (pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsAimingGun) || bIsReloading)
					{
						bIdealMountTopRunDist = false;
					}
				}
			}

			vToLadder.z += pPlayerPed->GetCapsuleInfo()->GetGroundToRootOffset();
			// Disable top mount run distance if it is not in line with us
			TUNE_GROUP_FLOAT(LADDER_DEBUG, RunMountTopAboveUsTolerance, 0.15f, 0.0f, 1.0f, 0.1f);
			TUNE_GROUP_FLOAT(LADDER_DEBUG, RunMountTopBelowUsTolerance, -0.25f, -1.0f, -0.0f, 0.1f);
			if (vToLadder.z > RunMountTopAboveUsTolerance || vToLadder.z < RunMountTopBelowUsTolerance)
				bIdealMountTopRunDist = false;

			bool bIdealMountWalkDist = fLadderDist < square(fStartDistance);
			aiDebugf2("bIdealMountWalkDist %s| bIdealMountTopRunDist %s| bIdealMountBottomRunDist %s, fLadderDist = %.2f, fStartDistance = %.2f", bIdealMountWalkDist?"TRUE":"FALSE", bIdealMountTopRunDist?"TRUE":"FALSE", bIdealMountBottomRunDist?"TRUE":"FALSE", fLadderDist, fStartDistance);

			if (bUseEasyLadderConditions)
			{
				TUNE_GROUP_FLOAT(LADDER_TUNE, LadderDistSqdToAlwaysAcceptStrafing, 0.5f, 0.0f, 1.0f, 0.1f);
				bIdealMountWalkDist = fLadderDist < LadderDistSqdToAlwaysAcceptStrafing;
				aiDebugf2("bIdealMountWalkDist = TRUE");
			}
				
			if (bIdealMountWalkDist ||	bIdealMountTopRunDist || bIdealMountBottomRunDist)
			{
				bool bTempIsEasyEntry = false;
				if (CTaskGoToAndClimbLadder::IsPedOrientatedCorrectlyToLadderToGetOn(m_pTargetLadder, *pPlayerPed, m_iLadderIndex, bIdealMountWalkDist, bTempIsEasyEntry))
				{
					return true; 
				}
				else
				{
#if __BANK
					if(bLogCheckShouldClimbLadder)
					{
						Displayf("RETURN FALSE: CTaskGoToAndClimbLadder::IsPedOrientatedCorrectlyToLadderToGetOn failed");
					}
#endif
					aiDebugf2("FAILED CTaskGoToAndClimbLadder::IsPedOrientatedCorrectlyToLadderToGetOn");
				}

				m_bEasyLadderEntry = bTempIsEasyEntry;
			}
		}
	}
#if __BANK
	else if(bLogCheckShouldClimbLadder)
	{
		Displayf( "bPedWantsToMove = %s", bPedWantsToMove ? "true" : "false" );
		Displayf( "bPassedActionModeCondition = %s", bPassedActionModeCondition ? "true" : "false" );
		Displayf( "bDisableLadderClimbingFlag = %s", bDisableLadderClimbingFlag ? "true" : "false" );
		Displayf( "RETURN FALSE: ((bPedWantsToMove || bPassedActionModeCondition) && !bDisableLadderClimbingFlag) failed" );
	}
#endif

#if __BANK
	if(bLogCheckShouldClimbLadder)
	{
		Displayf("END OF - CTaskPlayerOnFoot::CheckShouldClimbLadder");
	}
#endif

	return false;
}

void CTaskPlayerOnFoot::Uncuff_OnEnter(CPed* UNUSED_PARAM(pPlayerPed))
{
	Assert(m_pTargetUncuffPed);

#if ENABLE_TASKS_ARREST_CUFFED
	CTaskArrest* pArrestTask = rage_new CTaskArrest(m_pTargetUncuffPed, AF_UnCuff);
	SetNewTask(pArrestTask);
#endif // ENABLE_TASKS_ARREST_CUFFED

	m_bWaitingForPedArrestUp = true;
}

CTask::FSM_Return CTaskPlayerOnFoot::Uncuff_OnUpdate(CPed* UNUSED_PARAM(pPlayerPed))
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(STATE_PLAYER_IDLES);
	}

	return FSM_Continue;
}

void CTaskPlayerOnFoot::PlayerGun_OnEnter(CPed* UNUSED_PARAM(pPlayerPed))
{
	s32 iFlags = 0;

	if (m_bReloadFromIdle)
	{
		iFlags |= GF_ReloadFromIdle;
	}

	taskDebugf1("CTaskPlayerOnFoot::PlayerGun_OnEnter: Previous State: %d", GetPreviousState());

	SetNewTask(rage_new CTaskPlayerWeapon(iFlags));

	m_bWaitingForLadder = false;
}

CTask::FSM_Return CTaskPlayerOnFoot::PlayerGun_OnUpdate(CPed* pPlayerPed)
{
#if FPS_MODE_SUPPORTED
	const bool bFPSMode = pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer();
	TUNE_GROUP_BOOL(PED_DOOR, AllowOpenDoorArmIk, false);
#endif // FPS_MODE_SUPPORTED

	bool bAllowJumpInGunState = false;
	const CWeaponInfo* pWeaponInfo = pPlayerPed->GetEquippedWeaponInfo();
#if FPS_MODE_SUPPORTED
	bool bAllowOpenDoorArmIK = false;
	if(bFPSMode)
	{
		const bool bIsAWeapon = pWeaponInfo && !pWeaponInfo->GetIsUnarmed();
		const bool bFPSIdle = pPlayerPed->GetMotionData()->GetIsFPSIdle();

		bool bUsingKeyboardAndMouse = false;
#if KEYBOARD_MOUSE_SUPPORT
		bUsingKeyboardAndMouse = pPlayerPed->GetControlFromPlayer()->WasKeyboardMouseLastKnownSource();
#endif

		if(!bFPSIdle && !bUsingKeyboardAndMouse)
		{
			CControl* pControl = pPlayerPed->GetControlFromPlayer();
			if(pControl)
			{
				pControl->ClearToggleRun();
			}
		}
		

		// Allow jump in first person camera when in the fps idle state or we're unarmed
		// Don't allow on the first frame, so we can ensure the MBR is up to date
		bAllowJumpInGunState = (bFPSIdle || !bIsAWeapon) && GetTimeInState() > 0.f;

		// Allow open door arm IK only when in fps idle and unarmed
		bAllowOpenDoorArmIK = bFPSIdle && !bIsAWeapon && AllowOpenDoorArmIk;
	}
	pPlayerPed->SetPedConfigFlag( CPED_CONFIG_FLAG_OpenDoorArmIK, bAllowOpenDoorArmIK );
#endif // FPS_MODE_SUPPORTED

	// Allow jump in gun state if super jump is enabled.
	if (!bAllowJumpInGunState)
	{
		bAllowJumpInGunState = pPlayerPed->GetPlayerInfo() && pPlayerPed->GetPlayerInfo()->GetPlayerResetFlags().IsFlagSet(CPlayerResetFlags::PRF_SUPER_JUMP_ON);
	}

	if (!pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_DisablePlayerJumping))
	{
		pPlayerPed->SetPedResetFlag( CPED_RESET_FLAG_DisablePlayerJumping, !bAllowJumpInGunState );
	}

	CTask* pMotionTask = pPlayerPed->GetCurrentMotionTask(false);
	if(pMotionTask && pMotionTask->GetTaskType() == CTaskTypes::TASK_MOTION_AIMING)
	{
		if(static_cast<CTaskMotionAiming*>(pMotionTask)->GetState() == CTaskMotionAiming::State_Roll)
		{
			pPlayerPed->SetPedResetFlag( CPED_RESET_FLAG_DisablePlayerVaulting, true );
		}
	}

	// Disable simulated aiming if the following input is detected.
	if(pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_SimulatingAiming ))
	{
		CControl *pControl = pPlayerPed->GetControlFromPlayer();
		if(pControl->GetPedWalkLeftRight().GetNorm() != 0.0f || pControl->GetPedWalkUpDown().GetNorm() != 0.0f || pControl->GetPedAimWeaponLeftRight().GetNorm() != 0.0f || pControl->GetPedAimWeaponUpDown().GetNorm() != 0.0f
			|| pControl->GetPedAttack().IsPressed() || pControl->GetPedAttack2().IsPressed() != 0.0f || pControl->GetPedTargetIsDown())
		{
			pPlayerPed->SetPedConfigFlag( CPED_CONFIG_FLAG_SimulatingAiming, false );
		}
	}

	// Check to see if we have a melee task stored off from the previous frame
	if( m_pMeleeRequestTask )
	{
		CTaskReloadGun* pReloadTask = static_cast<CTaskReloadGun*>( pPlayerPed->GetPedIntelligence()->FindTaskActiveByType( CTaskTypes::TASK_RELOAD_GUN ) );
		if( pReloadTask )
		{
			pReloadTask->RequestInterrupt();
		}

		pPlayerPed->SetIsCrouching( false, -1, false );
		taskDebugf1("CTaskPlayerOnFoot::PlayerGun_OnUpdate: SetState(STATE_MELEE) - m_pMeleeRequestTask. Previous State: %d", GetPreviousState());
		SetState(STATE_MELEE);
		return FSM_Continue;
	}

	ProcessSpecialAbilities();
	
	if(CheckShouldRideTrain())
	{
		SetState(STATE_RIDE_TRAIN);
		return FSM_Continue;
	}

	weaponAssert(pPlayerPed->GetWeaponManager());
	CWeapon* pWeapon = pPlayerPed->GetWeaponManager()->GetEquippedWeapon();
	const bool bFirstPersonScopeWeapon = pWeapon && pWeapon->GetHasFirstPersonScope();
	const bool bHeavyWeapon = pWeapon && pWeapon->GetWeaponInfo()->GetIsHeavy();

	if(m_bWaitingForLadder)
	{
		// Ped has given up trying to get on ladder....
		if(!KeepWaitingForLadder(pPlayerPed, m_fBlockHeading))
		{
			m_bWaitingForLadder = false;
			return FSM_Continue;
		}

		pPlayerPed->SetPedResetFlag(CPED_RESET_FLAG_LadderBlockingMovement, true);
		pPlayerPed->GetMotionData()->SetDesiredMoveBlendRatio(0.0f, 0.0f);
		pPlayerPed->SetVelocity(VEC3_ZERO); 
		pPlayerPed->SetDesiredVelocity(VEC3_ZERO);

		pPlayerPed->SetHeading(m_fBlockHeading); 
		pPlayerPed->SetPosition(m_vBlockPos, true, true);

		if(!CTaskClimbLadderFully::IsLadderBaseOrTopBlockedByPedMP(m_pTargetLadder, m_iLadderIndex, CTaskClimbLadderFully::BF_CLIMBING_DOWN) &&
			!CTaskGoToAndClimbLadder::IsLadderTopBlockedMP(m_pTargetLadder, *pPlayerPed, m_iLadderIndex))
		{
			SetState(STATE_CLIMB_LADDER);
		}
	}
	else if( !pPlayerPed->GetIsSwimming() && CTaskSlopeScramble::CanDoSlopeScramble(pPlayerPed) )
	{
		//Do the same checks whilst aiming as not aiming to keep the scramble consistent
		m_fTimeSinceLastValidSlopeScramble += fwTimer::GetTimeStep();
		++m_nFramesOfSlopeScramble;
		float TimeBeforeScramble = CTaskSlopeScramble::GetTimeBeforeScramble(pPlayerPed->GetGroundNormal(), pPlayerPed->GetVelocity(), ZAXIS);

		TUNE_GROUP_FLOAT (SLOPE_SCRAMBLE, fDisabledTimerGroundZMaxAiming, 0.6f, 0.0f, 1.0f, 0.01f);
		TUNE_GROUP_INT (SLOPE_SCRAMBLE, nSlopeScrambleFramesAiming, 4, 0, 1000, 1);

		// Don't consider time if we are not moving
		if ((m_fTimeSinceLastValidSlopeScramble > TimeBeforeScramble || abs(pPlayerPed->GetGroundNormal().z) > fDisabledTimerGroundZMaxAiming) && m_nFramesOfSlopeScramble > nSlopeScrambleFramesAiming )
		{
			SetState(STATE_SLOPE_SCRAMBLE);
		}
	}
	else
	{
		//we're no longer on a slope, reset. 
		m_fTimeSinceLastValidSlopeScramble = 0.0f;
		m_nFramesOfSlopeScramble = 0;
	}

	bool bUsableUnderwater = false; 
	if (pWeaponInfo)
	{
		bUsableUnderwater = pWeaponInfo->GetIsUnderwaterGun();
	}

#if FPS_MODE_SUPPORTED
	bool bPhoneOnScreen = CTaskPlayerOnFoot::CheckForUseMobilePhone(*pPlayerPed);
	// If the player isn't texting but the phone is out, switch to running the texting clip
	if(bFPSMode && bPhoneOnScreen && !pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_IsReloading))
	{
		taskDebugf1("CTaskPlayerOnFoot::PlayerGun_OnUpdate: SetState(STATE_PLAYER_IDLES) - bPhoneOnScreen. Previous State: %d", GetPreviousState());
		SetState(STATE_PLAYER_IDLES);
		return FSM_Continue;
	}
#endif

	// Quit out of gun state if we have just done a viewmode switch and are not forced aiming
	const bool bExitedCoverThisFrame = (fwTimer::GetTimeInMilliseconds() - m_uLastTimeInCoverTask == 0) ? true : false;
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || ((pPlayerPed->GetIsSwimming() && !bUsableUnderwater)) || 
		(pPlayerPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_CAMERA_VIEW_MODE_SWITCHED_TO_OR_FROM_FIRST_PERSON) && pPlayerPed->GetMotionData()->GetForcedMotionStateThisFrame() != CPedMotionStates::MotionState_Aiming && !bExitedCoverThisFrame && !pPlayerPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_CAMERA_VIEW_MODE_SWITCHED_TO_FIRST_PERSON)))
	{
		pPlayerPed->GetPlayerInfo()->m_uLastTimeStopAiming = fwTimer::GetTimeInMilliseconds();
#if FPS_MODE_SUPPORTED
		if(pWeaponInfo && GetIsFlagSet(aiTaskFlags::SubTaskFinished) && pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(true, false, true, true, false))
		{
			taskDebugf1("CTaskPlayerOnFoot::PlayerGun_OnUpdate: RestartCurrentState");
			SetFlag(aiTaskFlags::RestartCurrentState);
		}
		else
#endif // FPS_MODE_SUPPORTED
		{
			taskDebugf1("CTaskPlayerOnFoot::PlayerGun_OnUpdate: SetState(STATE_PLAYER_IDLES) - SubTaskFinished. Previous State: %d", GetPreviousState());
			SetState(STATE_PLAYER_IDLES);
		}
	}

	CControl *pControl = pPlayerPed->GetControlFromPlayer();
	if(CheckForUsingJetpack(pPlayerPed))
	{
		SetState(STATE_JETPACK);
	}
	else if(!m_bPlayerExitedCoverThisUpdate && !bFirstPersonScopeWeapon && !bHeavyWeapon && CheckForNearbyCover(pPlayerPed))
	{
		taskDebugf1("CTaskPlayerOnFoot::PlayerGun_OnUpdate: SetState(STATE_USE_COVER) - CheckForNearbyCover. Previous State: %d", GetPreviousState());
		SetState(STATE_USE_COVER);
	}
	else if(CheckShouldPerformArrest())
	{
		ArrestTargetPed(pPlayerPed);
	}
	else if(CheckShouldPerformUncuff())
	{
		SetState(STATE_UNCUFF);
	}
	else if( /*pWeapon && (pWeapon->GetState() == CWeapon::STATE_RELOADING) &&*/ DoJumpCheck(pPlayerPed, pControl, bHeavyWeapon))
	{
		taskDebugf1("CTaskPlayerOnFoot::PlayerGun_OnUpdate: SetState(STATE_JUMP) - DoJumpCheck. Previous State: %d", GetPreviousState());
		SetState(STATE_JUMP);
	}
	else  if(CheckShouldClimbLadder(pPlayerPed) && !camInterface::GetGameplayDirector().IsFirstPersonAiming())
	{
		if (!m_bGettingOnBottom && 
			NetworkInterface::IsGameInProgress() &&
			(CTaskClimbLadderFully::IsLadderBaseOrTopBlockedByPedMP(m_pTargetLadder, m_iLadderIndex, CTaskClimbLadderFully::BF_CLIMBING_DOWN)||
			CTaskGoToAndClimbLadder::IsLadderTopBlockedMP(m_pTargetLadder, *pPlayerPed, m_iLadderIndex)))
		{
			m_vBlockPos = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());
			m_fBlockHeading = pPlayerPed->GetTransform().GetHeading();
			m_bWaitingForLadder = true;
		}
		else
		{
			SetState(STATE_CLIMB_LADDER);
		}
	}
	else 
	{
		s8 iEnterVehicleState = CheckShouldEnterVehicle(pPlayerPed,pControl, m_bGetInVehicleAfterCombatRolling);

		if(iEnterVehicleState != -1)
		{
			pPlayerPed->SetPedResetFlag(CPED_RESET_FLAG_WantsToEnterVehicleFromAiming, true);
			SetState((ePlayerOnFootState)iEnterVehicleState);
		}
	}

	// Enable scanning flags
	pPlayerPed->SetPedResetFlag( CPED_RESET_FLAG_SearchingForDoors, true );

#if FPS_MODE_SUPPORTED
	if(bFPSMode && GetState() == STATE_PLAYER_GUN)
	{
		PlayerIdles_OnUpdate(pPlayerPed);
	}
#endif // FPS_MODE_SUPPORTED

	return FSM_Continue;
}

CTask::FSM_Return CTaskPlayerOnFoot::PlayerGun_OnExit(CPed* pPlayerPed)
{
	pPlayerPed->SetPedConfigFlag( CPED_CONFIG_FLAG_SimulatingAiming, false );

#if FPS_MODE_SUPPORTED
	pPlayerPed->SetPedConfigFlag( CPED_CONFIG_FLAG_OpenDoorArmIK, false );
#endif // FPS_MODE_SUPPORTED

	return FSM_Continue;
}

void CTaskPlayerOnFoot::GetMountAnimal_OnEnter(CPed* UNUSED_PARAM(pPlayerPed))
{
#if ENABLE_HORSE
	if( m_pTargetAnimal )
	{				
		VehicleEnterExitFlags vehicleFlags;
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::PlayerControlledVehicleEntry);
		if (GetPreviousState() == STATE_DROPDOWN)
		{
			vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DropEntry);
			vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::InAirEntry);
		}
		else if ( GetPreviousState() == STATE_JUMP)
		{
			vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::JumpEntry);
			vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::InAirEntry);
		}
		SeatRequestType iSeatRequestType = SR_Specific;
		s32			iSeat = m_pTargetAnimal->GetPedModelInfo()->GetModelSeatInfo()->GetDriverSeat();

		CTask* pTask = rage_new CTaskMountAnimal(m_pTargetAnimal, iSeatRequestType, iSeat, vehicleFlags);		
		SetNewTask( pTask );
	}
#endif
}

CTask::FSM_Return CTaskPlayerOnFoot::GetMountAnimal_OnUpdate(CPed* ENABLE_HORSE_ONLY(pPlayerPed) )
{
#if ENABLE_HORSE
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if(pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_OnMount ) )
		{
			// Ped got onto mount, quit the on foot task
			return FSM_Quit;
		}
		else
		{
			SetState(STATE_PLAYER_IDLES);
			return FSM_Continue;
		}	
	}

	//start processing inputs on the mount
	if (pPlayerPed->GetMyMount())
		CTaskPlayerOnHorse::DriveHorse(pPlayerPed);

#else
	taskAssert(0);
#endif

	return FSM_Continue;
}

void CTaskPlayerOnFoot::GetInVehicle_OnEnter(CPed* pPlayerPed)
{
	if( m_pTargetVehicle )//&& bEnterHeld)
	{
		VehicleEnterExitFlags vehicleFlags;
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::PlayerControlledVehicleEntry);

		bool bWantsToEnterVehicleFromAiming = pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_WantsToEnterVehicleFromAiming);
#if FPS_MODE_SUPPORTED
		if (pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false))
		{
			bWantsToEnterVehicleFromAiming = pPlayerPed->GetPlayerInfo()->IsAiming();
		}
#endif // FPS_MODE_SUPPORTED

		if (pPlayerPed->GetIsInCover() || bWantsToEnterVehicleFromAiming)
		{
			vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::CombatEntry);
		}

		if (pPlayerPed->GetIsInCover())
		{
			vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::FromCover);
		}

		if( m_pTargetVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN )
		{
			const CTrain* pTrain = static_cast<const CTrain*>(m_pTargetVehicle.Get());
			Assert( pTrain->m_nTrainFlags.bAtStation );
			if (pTrain->m_nTrainFlags.iStationPlatformSides & CTrainTrack::Right)
				vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::PreferRightEntry);
			else
				vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::PreferLeftEntry);
		}

		if (CTaskVehicleFSM::IsVehicleLockedForPlayer(*m_pTargetVehicle, *pPlayerPed))
		{
			if (m_pTargetVehicle->GetDriver() && pPlayerPed->GetPedIntelligence()->IsThreatenedBy(*m_pTargetVehicle->GetDriver()))
			{
				vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::JustPullPedOut);
				vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::UseDirectEntryOnly);
			}
		}

		SeatRequestType iSeatRequestType = SR_Specific;
		s32	iSeat = m_pTargetVehicle->GetDriverSeat();
	
		// Process turreted vehicle initial seat preferences
		int iBestTurretSeat = -1;
		const bool bHasTurret = m_pTargetVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_TURRET_SEAT_ON_VEHICLE);
		const CPed* pDriver = CTaskVehicleFSM::GetPedInOrUsingSeat(*m_pTargetVehicle, iSeat);
		if (pDriver)
		{
			// If theres a friendly driver, see if we want to prefer entering a turret seat
			if (pDriver->GetPedIntelligence()->IsFriendlyWith(*pPlayerPed))
			{
				iBestTurretSeat = CTaskVehicleFSM::FindBestTurretSeatIndexForVehicle(*m_pTargetVehicle, *pPlayerPed, pDriver, iSeat, false, true);
			}
		}
		else
		{
			// If theres no driver and we're stood on the turreted vehicle (not heli or plane), prefer the turret seat
			if (pPlayerPed->GetGroundPhysical() == m_pTargetVehicle && !m_pTargetVehicle->InheritsFromHeli() && !m_pTargetVehicle->InheritsFromPlane() && !m_pTargetVehicle->InheritsFromBoat())
			{
				iBestTurretSeat = m_pTargetVehicle->GetFirstTurretSeat();
			}				
		}

		// Prefer the turret seat if found
		if (m_pTargetVehicle->IsTurretSeat(iBestTurretSeat))
		{
			iSeatRequestType = SR_Prefer;
			iSeat = iBestTurretSeat;
		}
		else if (NetworkInterface::IsGameInProgress())
		{
			if (bHasTurret && pDriver)
			{
				iSeatRequestType = SR_Any;	
			}
			else
			{
				iSeatRequestType = SR_Prefer;
			}
			iSeat = m_pTargetVehicle->GetDriverSeat();
		}
		if(taskVerifyf(m_pTargetVehicle->GetLayoutInfo(),"NULL layout info") && !m_pTargetVehicle->GetLayoutInfo()->GetHasDriver())
		{
			iSeatRequestType = SR_Any;
			iSeat = -1;		
		}
		else
		{
			vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::CanWarpOverPassengers);
		}

		// If player is on skis then switch to on foot
		if(pPlayerPed->GetIsSkiing())
		{
			pPlayerPed->SetIsSkiing(false);
		}

		if( pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_ForcePlayerToEnterVehicleThroughDirectDoorOnly) )
		{
			vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::UseDirectEntryOnly);
		}

		if( pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_WillJackWantedPlayersRatherThanStealCar) )
		{
			vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::JackWantedPlayersRatherThanStealCar);
		}

		// Walk unless run is held or was released within the last second
		static dev_s32 RUN_IF_RUN_HELD_WITHIN_TIME = 3000;
		float fMoveBlendRatio = MOVEBLENDRATIO_WALK;
		CControl* pControl = pPlayerPed->GetControlFromPlayer();
		if( pControl && ( pControl->GetPedSprintIsDown() || pControl->GetPedSprintHistoryReleased(RUN_IF_RUN_HELD_WITHIN_TIME)) )
		{
			fMoveBlendRatio = MOVEBLENDRATIO_RUN;
		}

#if __BANK
		if (CVehicleDebug::ms_bForcePlayerToUseSpecificSeat && m_pTargetVehicle && m_pTargetVehicle->IsSeatIndexValid(CVehicleDebug::ms_iSeatRequested))
		{
			iSeat = CVehicleDebug::ms_iSeatRequested;
			iSeatRequestType = SR_Specific;
			vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::JackIfOccupied);
		}
#endif // __BANK

		CTask* pTask = rage_new CTaskEnterVehicle(m_pTargetVehicle, iSeatRequestType, iSeat, vehicleFlags, 0.0f, fMoveBlendRatio);
		AI_LOG_WITH_ARGS("[Player] - TASK_ENTER_VEHICLE constructed at 0x%p, target vehicle - %s, iSeat - %i\n", pTask, AILogging::GetDynamicEntityNameSafe(m_pTargetVehicle), iSeat);
		SetNewTask( pTask );
	}
}

CTask::FSM_Return CTaskPlayerOnFoot::GetInVehicle_OnUpdate(CPed* pPlayerPed)
{
	//Override with an arrest if we can
	if(CheckShouldPerformArrest(true, true))
	{
		ArrestTargetPed(pPlayerPed);
		SetState(STATE_PLAYER_IDLES);
	}

    if (m_pTargetVehicle)
        CVehicle::RequestHdForVehicle(m_pTargetVehicle);

	// Try to vault if we are stuck whilst navigating on a vehicle
	if (m_pTargetVehicle == pPlayerPed->GetGroundPhysical())
	{
		if (pPlayerPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter() > 10)
		{
			CTaskMove * pTaskSimplestMove = pPlayerPed->GetPedIntelligence()->GetActiveSimplestMovementTask();
			if (pTaskSimplestMove && pTaskSimplestMove->IsTaskMoving() && pTaskSimplestMove->GetMoveInterface()->HasTarget())
			{
				Assert(pTaskSimplestMove->IsMoveTask());
				Vector3 vToTarget = pTaskSimplestMove->GetTarget() - VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());
				vToTarget.z = 0.f;
				vToTarget.Normalize();
	
				CWeapon* pWeapon = pPlayerPed->GetWeaponManager()->GetEquippedWeapon();
				if (Dot(VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetForward()), vToTarget) > 0.967f && 	// We must face the dir somewhat in case we are strafing
					DoJumpCheck(pPlayerPed, pPlayerPed->GetControlFromPlayer(), pWeapon && pWeapon->GetWeaponInfo()->GetIsHeavy(), true))
				{
					//! If it is a boat, apply some reasonable limits on how high we can climb. Force climb after static counter gets high enough.
					bool bHandholdOK = true;

					const CTaskEnterVehicle* pEnterVehicleSubTask = static_cast<const CTaskEnterVehicle*>(FindSubTaskOfType(CTaskTypes::TASK_ENTER_VEHICLE));
					const bool bCanVaultUp = pEnterVehicleSubTask ? pEnterVehicleSubTask->GetState() == CTaskEnterVehicle::State_GoToClimbUpPoint : false;
					if(m_pTargetVehicle->InheritsFromBoat() || bCanVaultUp)
					{
						CClimbHandHoldDetected handHold;
						bool bDetected = pPlayerPed->GetPedIntelligence()->GetClimbDetector().GetDetectedHandHold(handHold);
						if(bDetected)
						{		
							static dev_float s_fMaxClimbHeight = 1.5f;
							static dev_u32 s_nForceClimbStaticCounter = 50;

							if( ((handHold.GetHandHold().GetPoint().GetZ() - pPlayerPed->GetGroundPos().z) > s_fMaxClimbHeight) && 
								pPlayerPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter() < s_nForceClimbStaticCounter)
							{
								bHandholdOK = false;
							}
						}
					}

					if(bHandholdOK)
					{
						SetState(STATE_JUMP);
						return FSM_Continue;
					}
				}
			}
		}
	}

	if(GetIsSubtaskFinished(CTaskTypes::TASK_ENTER_VEHICLE))
	{
		if(pPlayerPed->GetMyVehicle() && pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
		{
			// Ped got into vehicle sucessfully, quit the on foot task
			return FSM_Quit;
		}
		else if (pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_WantsToEnterCover))
		{
			SetState(STATE_INITIAL);
			return FSM_Continue;
		}
		else if (GetTimeInState() > 0.0f)	// Prevent infinite loop when trying to enter a vehicle occupied by a friendly ped
		{
			SetState(STATE_PLAYER_IDLES);
			return FSM_Continue;
		}
	}
	return FSM_Continue;
}

bool CTaskPlayerOnFoot::CheckCanEnterVehicle(CVehicle* pTargetVehicle)
{
	bool bReturn = true;
	if( pTargetVehicle )
	{
		const CPed& ped = *GetPed();

		if (ped.GetWeaponManager() && ped.GetWeaponManager()->GetEquippedWeapon() && ped.GetWeaponManager()->GetEquippedWeapon()->GetIsCooking())
		{
			AI_LOG_WITH_ARGS("[Player] - Unable to enter vehicle %s due to cooking", AILogging::GetDynamicEntityNameSafe(pTargetVehicle));
			return false; // B* 1448761 [6/9/2013 musson] 
		}

		const VehicleType vehType = pTargetVehicle->GetVehicleType();
		const bool bLockedToPlayer = CTaskVehicleFSM::IsVehicleLockedForPlayer(*pTargetVehicle, ped) || pTargetVehicle->GetCarDoorLocks() == CARLOCK_LOCKED;
		if (bLockedToPlayer)
		{
			// Let peds enter vehicles that have a driver (either as a passenger or to pull the driver out)
			if (!pTargetVehicle->GetDriver())
			{
				if (vehType != VEHICLE_TYPE_CAR)
				{
					bReturn = false;
				}
			}
			// Don't even attempt to enter the vehicle if flagged as such
			else if (pTargetVehicle->m_nVehicleFlags.bDontTryToEnterThisVehicleIfLockedForPlayer)
			{
				AI_LOG_WITH_ARGS("[Player] - Unable to enter vehicle %s due to bDontTryToEnterThisVehicleIfLockedForPlayer, pTargetVehicle->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD) ? %s", AILogging::GetDynamicEntityNameSafe(pTargetVehicle), AILogging::GetBooleanAsString(pTargetVehicle->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD)));
				bReturn = false;
			}
		}

		if( vehType == VEHICLE_TYPE_TRAIN )
		{
			const CTrain* pTrain = static_cast<const CTrain*>(pTargetVehicle);
			if( !pTrain->m_nTrainFlags.bAtStation )
			{
				bReturn = false;
			}
		}

		if( vehType == VEHICLE_TYPE_BIKE ||
			vehType == VEHICLE_TYPE_BICYCLE ||
			vehType == VEHICLE_TYPE_QUADBIKE ||
			vehType == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE)
		{
			if (!pTargetVehicle->CanPedEnterCar(&ped))
			{
				bReturn = false;
			}
		}

		if (!bReturn || bLockedToPlayer)
		{
			GetEventScriptAIGroup()->Add(CEventPlayerUnableToEnterVehicle(*pTargetVehicle));
		}
	}
	else
	{
		bReturn = false;
	}

	return bReturn;
}

void CTaskPlayerOnFoot::TakeOffHelmet_OnEnter(CPed* UNUSED_PARAM(pPlayerPed))
{
#if FPS_MODE_SUPPORTED
	m_bGetInVehicleAfterTakingOffHelmet = false;
#endif // FPS_MODE_SUPPORTED

	SetNewTask(rage_new CTaskComplexControlMovement( rage_new CTaskMovePlayer(), rage_new CTaskTakeOffHelmet(), CTaskComplexControlMovement::TerminateOnSubtask));
}

CTask::FSM_Return CTaskPlayerOnFoot::TakeOffHelmet_OnUpdate(CPed* UNUSED_PARAM(pPlayerPed))
{
#if FPS_MODE_SUPPORTED
	CPed *pPed = GetPed();
	CControl* pControl = pPed->GetControlFromPlayer();
	const Vector3 vCarSearchDirection = VEC3V_TO_VECTOR3(pPed->GetTransform().GetB());
	const bool bUsingStickInput = false;
	if( pPed->IsFirstPersonShooterModeEnabledForPlayer(false) &&
		pControl &&
		pControl->GetPedEnter().IsPressed() && 
		CPlayerInfo::ScanForVehicleToEnter(pPed, vCarSearchDirection, bUsingStickInput) != NULL )
	{
		m_bGetInVehicleAfterTakingOffHelmet = true;
	}
#endif // FPS_MODE_SUPPORTED

	if( GetIsFlagSet(aiTaskFlags::SubTaskFinished) )
	{
#if FPS_MODE_SUPPORTED
		if( CheckShouldEnterVehicle(pPed, pControl, m_bGetInVehicleAfterTakingOffHelmet) == STATE_GET_IN_VEHICLE )
			SetState(STATE_GET_IN_VEHICLE);
		else
			SetState(STATE_PLAYER_IDLES);
#else
		SetState(STATE_PLAYER_IDLES);
#endif // FPS_MODE_SUPPORTED
	}
	return FSM_Continue;
}


void CTaskPlayerOnFoot::DuckAndCover_OnEnter(CPed* UNUSED_PARAM(pPlayerPed))
{
	SetNewTask(rage_new CTaskDuckAndCover());
}

CTask::FSM_Return CTaskPlayerOnFoot::DuckAndCover_OnUpdate(CPed* UNUSED_PARAM(pPlayerPed))
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(STATE_PLAYER_IDLES);
	}
	return FSM_Continue;
}

void CTaskPlayerOnFoot::SlopeScramble_OnEnter(CPed* UNUSED_PARAM(pPlayerPed))
{
	SetNewTask(rage_new CTaskSlopeScramble());
}

CTask::FSM_Return CTaskPlayerOnFoot::SlopeScramble_OnUpdate(CPed* UNUSED_PARAM(pPlayerPed))
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(STATE_PLAYER_IDLES);
	}
	return FSM_Continue;
}

void CTaskPlayerOnFoot::ScriptedTask_OnEnter(CPed* UNUSED_PARAM(pPlayerPed))
{
	if(taskVerifyf(m_pScriptedTask, "ScriptedTask cannot be NULL!"))
	{
		SetNewTask(m_pScriptedTask);
		m_pScriptedTask = NULL;
	}
}

CTask::FSM_Return CTaskPlayerOnFoot::ScriptedTask_OnUpdate(CPed* UNUSED_PARAM(pPlayerPed))
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(STATE_PLAYER_IDLES);
	}
	return FSM_Continue;
}

void CTaskPlayerOnFoot::TakeOffParachutePack_OnEnter(CPed* UNUSED_PARAM(pPlayerPed))
{
	//Create the move task.
	CTaskMovePlayer* pMoveTask = rage_new CTaskMovePlayer();

	//Set the max move/blend ratio.
	pMoveTask->SetMaxMoveBlendRatio(MOVEBLENDRATIO_WALK);

	//Get the parachute pack variation.
	ePedVarComp nVariationComponent = PV_COMP_INVALID;
	u8 uVariationDrawableId = 0;
	u8 uVariationDrawableAltId = 0;
	u8 uVariationTexId = 0;
	CTaskParachute::GetParachutePackVariationForPed(*GetPed(), nVariationComponent, uVariationDrawableId, uVariationDrawableAltId, uVariationTexId);

	//Define the parameters.
	static fwMvClipSetId s_ClipSetId("skydive@parachute@pack",0x3587E495);
	static fwMvClipId s_ClipIdForPed("Chute_Off",0xB44359AE);
	static fwMvClipId s_ClipIdForProp("Chute_Off_Bag",0xA13571F8);
	static eAnimBoneTag s_nAttachBone = BONETAG_SPINE3;
	static dev_float s_fForceToApplyAfterInterrupt = 300.0f;

	atHashWithStringNotFinal hProp = CTaskParachute::GetModelForParachutePack(GetPed());

	//Create the sub-task.
	CTaskTakeOffPedVariation* pSubTask = rage_new CTaskTakeOffPedVariation(s_ClipSetId, s_ClipIdForPed, s_ClipIdForProp,
		nVariationComponent, uVariationDrawableId, uVariationDrawableAltId, uVariationTexId, hProp, s_nAttachBone);

	//Set the attach parameters.
	Vec3V vAttachOffset(sm_Tunables.m_ParachutePack.m_AttachOffsetX,
		sm_Tunables.m_ParachutePack.m_AttachOffsetY, sm_Tunables.m_ParachutePack.m_AttachOffsetZ);
	pSubTask->SetAttachOffset(vAttachOffset);
	Vec3V vAttachOrientation(sm_Tunables.m_ParachutePack.m_AttachOrientationX * DtoR,
		sm_Tunables.m_ParachutePack.m_AttachOrientationY * DtoR, sm_Tunables.m_ParachutePack.m_AttachOrientationZ * DtoR);
	QuatV qAttachOrientation = QuatVFromEulersXYZ(vAttachOrientation);
	pSubTask->SetAttachOrientation(qAttachOrientation);

	//Set the blend in parameters.
	pSubTask->SetBlendInDeltaForPed(sm_Tunables.m_ParachutePack.m_BlendInDeltaForPed);
	pSubTask->SetBlendInDeltaForProp(sm_Tunables.m_ParachutePack.m_BlendInDeltaForProp);

	//Set the blend out parameters.
	pSubTask->SetPhaseToBlendOut(sm_Tunables.m_ParachutePack.m_PhaseToBlendOut);
	pSubTask->SetBlendOutDelta(sm_Tunables.m_ParachutePack.m_BlendOutDelta);

	//Set the force.
	pSubTask->SetForceToApplyAfterInterrupt(s_fForceToApplyAfterInterrupt);

	//Set the velocity inheritance.
	if(sm_Tunables.m_ParachutePack.m_VelocityInheritance.m_Enabled)
	{
		pSubTask->SetVelocityInheritance(Vec3V(sm_Tunables.m_ParachutePack.m_VelocityInheritance.m_X,
			sm_Tunables.m_ParachutePack.m_VelocityInheritance.m_Y,
			sm_Tunables.m_ParachutePack.m_VelocityInheritance.m_Z));
	}

	//Create the task.
	CTask* pTask = rage_new CTaskComplexControlMovement(pMoveTask, pSubTask, CTaskComplexControlMovement::TerminateOnSubtask);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskPlayerOnFoot::TakeOffParachutePack_OnUpdate(CPed* BANK_ONLY(pPlayerPed))
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
#if __BANK
		if(CTaskParachute::IsParachutePackVariationActiveForPed(*pPlayerPed) && !pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_DisableTakeOffParachutePack))
		{
			Assertf(0, "Player hasn't taken off parachute pack successfully - it is still on & we are going to get into an infinite loop! Please add a bug with AI Display: Parachute Info!");
		}
#endif

		SetState(STATE_PLAYER_IDLES);
	}
	//Check if we should slope scramble.
	else if(CheckShouldSlopeScramble())
	{
		SetState(STATE_SLOPE_SCRAMBLE);
	}

	return FSM_Continue;
}

void CTaskPlayerOnFoot::TakeOffJetpack_OnEnter(CPed* JETPACK_ONLY(pPlayerPed))
{
#if ENABLE_JETPACK
	//Create the move task.
	CTaskMovePlayer* pMoveTask = rage_new CTaskMovePlayer();

	taskAssert(pPlayerPed->GetJetpack()->GetObject());

	//Set the max move/blend ratio.
	pMoveTask->SetMaxMoveBlendRatio(MOVEBLENDRATIO_WALK);

	//Define the parameters. TO DO - change to jetpack anims?
	fwMvClipSetId s_ClipSetId(sm_Tunables.m_JetpackData.m_ClipSetHash);
	fwMvClipId s_ClipIdForPed(sm_Tunables.m_JetpackData.m_PedClipHash);
	fwMvClipId s_ClipIdForProp(sm_Tunables.m_JetpackData.m_PropClipHash);
	
	atHashWithStringNotFinal jetPackProp = CTaskJetpack::sm_Tunables.m_JetpackModelData.m_JetpackModelName;

	//Create the sub-task.
	CTaskTakeOffPedVariation* pSubTask = rage_new CTaskTakeOffPedVariation(s_ClipSetId, s_ClipIdForPed, s_ClipIdForProp,
		PV_COMP_INVALID, 0, 0, 0, jetPackProp, BONETAG_INVALID, static_cast<CObject*>(pPlayerPed->GetJetpack()->GetObject()), true);

	//Set the blend out parameters.
	pSubTask->SetPhaseToBlendOut(sm_Tunables.m_JetpackData.m_PhaseToBlendOut);
	pSubTask->SetBlendOutDelta(sm_Tunables.m_JetpackData.m_BlendOutDelta);

	//Set the force.
	pSubTask->SetForceToApplyAfterInterrupt(sm_Tunables.m_JetpackData.m_ForceToApplyAfterInterrupt);

	//Set the velocity inheritance.
	if(sm_Tunables.m_JetpackData.m_VelocityInheritance.m_Enabled)
	{
		pSubTask->SetVelocityInheritance(Vec3V(sm_Tunables.m_JetpackData.m_VelocityInheritance.m_X,
			sm_Tunables.m_JetpackData.m_VelocityInheritance.m_Y,
			sm_Tunables.m_JetpackData.m_VelocityInheritance.m_Z));
	}

	//Create the task.
	CTask* pTask = rage_new CTaskComplexControlMovement(pMoveTask, pSubTask, CTaskComplexControlMovement::TerminateOnSubtask);

	//Start the task.
	SetNewTask(pTask);
#endif
}

CTask::FSM_Return CTaskPlayerOnFoot::TakeOffJetpack_OnUpdate(CPed* UNUSED_PARAM(pPlayerPed))
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(STATE_PLAYER_IDLES);
	}
	//Check if we should slope scramble.
	else if(CheckShouldSlopeScramble())
	{
		SetState(STATE_SLOPE_SCRAMBLE);
	}

	return FSM_Continue;
}

void CTaskPlayerOnFoot::TakeOffJetpack_OnExit(CPed *JETPACK_ONLY(pPlayerPed))
{
#if ENABLE_JETPACK
	pPlayerPed->DropJetpackObject();
#endif
}

void CTaskPlayerOnFoot::TakeOffScubaGear_OnEnter(CPed* UNUSED_PARAM(pPlayerPed))
{
	//Clear the scuba mask prop.
	CTaskMotionSwimming::ClearScubaMaskPropForPed(*GetPed());

	//Create the move task.
	CTaskMovePlayer* pMoveTask = rage_new CTaskMovePlayer();

	//Set the max move/blend ratio.
	pMoveTask->SetMaxMoveBlendRatio(MOVEBLENDRATIO_WALK);

	//Get the scuba gear variation.
	ePedVarComp nVariationComponent = PV_COMP_INVALID;
	u8 uVariationDrawableId = 0;
	u8 uVariationDrawableAltId = 0;
	u8 uVariationTexId = 0;
	CTaskMotionSwimming::GetScubaGearVariationForPed(*GetPed(), nVariationComponent, uVariationDrawableId, uVariationDrawableAltId, uVariationTexId);

	//Define the parameters.
	static fwMvClipSetId s_ClipSetId("swimming@scuba@tank",0x1AC58541);
	static fwMvClipId s_ClipIdForPed("scuba_tank_off",0x47BD6C2E);
	static fwMvClipId s_ClipIdForProp("scuba_tank_off_tank",0xA14A0E53);
	static eAnimBoneTag s_nAttachBone = BONETAG_SPINE3;
	static dev_float s_fForceToApplyAfterInterrupt = 300.0f;

	atHashWithStringNotFinal hProp = CTaskMotionSwimming::GetModelForScubaGear(*GetPed());

	//Create the sub-task.
	CTaskTakeOffPedVariation* pSubTask = rage_new CTaskTakeOffPedVariation(s_ClipSetId, s_ClipIdForPed, s_ClipIdForProp,
		nVariationComponent, uVariationDrawableId, uVariationDrawableAltId, uVariationTexId, hProp, s_nAttachBone);

	//Set the attach parameters.
	Vec3V vAttachOffset(sm_Tunables.m_ScubaGear.m_AttachOffsetX,
		sm_Tunables.m_ScubaGear.m_AttachOffsetY, sm_Tunables.m_ScubaGear.m_AttachOffsetZ);
	pSubTask->SetAttachOffset(vAttachOffset);
	Vec3V vAttachOrientation(sm_Tunables.m_ScubaGear.m_AttachOrientationX * DtoR,
		sm_Tunables.m_ScubaGear.m_AttachOrientationY * DtoR, sm_Tunables.m_ScubaGear.m_AttachOrientationZ * DtoR);
	QuatV qAttachOrientation = QuatVFromEulersXYZ(vAttachOrientation);
	pSubTask->SetAttachOrientation(qAttachOrientation);

	//Set the blend out parameters.
	pSubTask->SetPhaseToBlendOut(sm_Tunables.m_ScubaGear.m_PhaseToBlendOut);
	pSubTask->SetBlendOutDelta(sm_Tunables.m_ScubaGear.m_BlendOutDelta);

	//Set the force.
	pSubTask->SetForceToApplyAfterInterrupt(s_fForceToApplyAfterInterrupt);

	//Set the velocity inheritance.
	if(sm_Tunables.m_ScubaGear.m_VelocityInheritance.m_Enabled)
	{
		pSubTask->SetVelocityInheritance(Vec3V(sm_Tunables.m_ScubaGear.m_VelocityInheritance.m_X,
			sm_Tunables.m_ScubaGear.m_VelocityInheritance.m_Y,
			sm_Tunables.m_ScubaGear.m_VelocityInheritance.m_Z));
	}

	//Create the task.
	CTask* pTask = rage_new CTaskComplexControlMovement(pMoveTask, pSubTask, CTaskComplexControlMovement::TerminateOnSubtask);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskPlayerOnFoot::TakeOffScubaGear_OnUpdate(CPed* UNUSED_PARAM(pPlayerPed))
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(STATE_PLAYER_IDLES);
	}
	//Check if we should slope scramble.
	else if(CheckShouldSlopeScramble())
	{
		SetState(STATE_SLOPE_SCRAMBLE);
	}

	return FSM_Continue;
}

void CTaskPlayerOnFoot::RideTrain_OnEnter()
{

}

CTask::FSM_Return CTaskPlayerOnFoot::RideTrain_OnUpdate()
{
	//Check if we should continue riding the train.
	if(CheckShouldRideTrain())
	{
		//Disable first person camera while on the tram
		if (GetPed()->IsLocalPlayer())
		{
			camInterface::GetGameplayDirector().DisableFirstPersonThisUpdate(GetPed());
		}

		//Block weapon switching.
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching, true);

		//Check if we are not using a scenario, and the cinematic camera is active.
		if(!GetSubTask() /*&& camInterface::GetCinematicDirector().IsRendering()*/)
		{
			//Create the options.
			CScenarioFinder::FindOptions opts;
			opts.m_pRequiredAttachEntity = GetPed()->GetPlayerInfo()->GetTrainPedIsInside();

			//Find a scenario attached to the train.
			int iRealScenarioPointType = -1;
			CScenarioPoint* pScenarioPoint = CScenarioFinder::FindNewScenario(GetPed(), VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()),
				50.0f, opts, iRealScenarioPointType);
			if(pScenarioPoint)
			{
				//Create the task.
				CTask* pTask = rage_new CTaskUseScenario(iRealScenarioPointType, pScenarioPoint,
					CTaskUseScenario::SF_Warp | CTaskUseScenario::SF_SkipEnterClip | CTaskUseScenario::SF_IdleForever);

				//Start the task.
				SetNewTask(pTask);
			}
		}
	}
	else
	{
		//Check if we are using a scenario.
		if(GetSubTask() && !GetIsFlagSet(aiTaskFlags::SubTaskFinished))
		{
			//Exit the scenario.
			GetSubTask()->MakeAbortable(ABORT_PRIORITY_URGENT, NULL);
		}
		else
		{
			SetState(STATE_PLAYER_IDLES);
		}
	}

	return FSM_Continue;
}

void CTaskPlayerOnFoot::Jetpack_OnEnter()
{
#if ENABLE_JETPACK
	SetNewTask(rage_new CTaskJetpack());
#endif
}

CTask::FSM_Return CTaskPlayerOnFoot::Jetpack_OnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(STATE_PLAYER_IDLES);
	}

	return FSM_Continue;
}

void CTaskPlayerOnFoot::BirdProjectile_OnEnter()
{
	// Note that we have tried to fire a projectile.
	m_fTimeSinceBirdProjectile = 0.0f;

	// Reset the flags for spawning a bird projectile.
	m_bHasAttemptedToSpawnBirdProjectile = false;
	m_bNeedToSpawnBirdProjectile = false;

	CPed* pPed = GetPed();

	// Look up the bird crap weapon.
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(sm_Tunables.m_BirdWeaponHash);

	if (pWeaponInfo && pPed->GetInventory() && pPed->GetWeaponManager())
	{
		// Give the bird one projectile.
		pPed->GetInventory()->AddWeaponAndAmmo(pWeaponInfo->GetHash(), 1);

		// Equip it.
		pPed->GetWeaponManager()->EquipWeapon(pWeaponInfo->GetHash());
	}
}

CTask::FSM_Return CTaskPlayerOnFoot::BirdProjectile_OnUpdate()
{
	CPed* pPed = GetPed();

	// We want the velocity of the bird to be taken into consideration when the projectile gets its initial velocity.
	pPed->SetPedResetFlag(CPED_RESET_FLAG_IncludePedReferenceVelocityWhenFiringProjectiles, true);

	// The projectile is created in PostPreRender, so we must always run that logic.
	pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPostPreRender, true );

	if (m_bHasAttemptedToSpawnBirdProjectile)
	{
		// This will be set by the spawning of the bird projectile in ProcessPreRender.
		// Go back to regular movement.
		SetState(STATE_PLAYER_IDLES);
		return FSM_Continue;
	}

	// Look up the bird crap weapon.
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(sm_Tunables.m_BirdWeaponHash);

	if (Verifyf(pWeaponInfo, "Expected a valid weapon info for  bird crapping."))
	{
		if (pPed->GetEquippedWeaponInfo() == pWeaponInfo)
		{
			// Make sure the projectile model is streamed in.
			if (pPed->GetInventory() && pPed->GetInventory()->GetIsStreamedIn())
			{
				// Crap!
				m_bNeedToSpawnBirdProjectile = true;
			}
		}
		else
		{
			// Something happened to the equipped weapon, bail out.
			SetState(STATE_PLAYER_IDLES);
		}
	}

	return FSM_Continue;
}

void CTaskPlayerOnFoot::BirdProjectile_OnExit()
{
	CPed* pPed = GetPed();

	// Look up the bird crap weapon.
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(sm_Tunables.m_BirdWeaponHash);
	Assert(pWeaponInfo);

	if (pWeaponInfo && pPed->GetInventory() && pPed->GetWeaponManager())
	{
		// Remove the bird crap weapon if it exists from the ped's inventory.
		pPed->GetInventory()->RemoveWeaponAndAmmo(pWeaponInfo->GetHash());

		// Force the weapon back to unarmed so we don't go through the swap weapon state.
		pPed->GetWeaponManager()->EquipWeapon(pPed->GetDefaultUnarmedWeaponHash());
	}
}

void CTaskPlayerOnFoot::SetScriptedTask(aiTask* pScriptedTask)
{
	m_pScriptedTask = pScriptedTask;
}

void CTaskPlayerOnFoot::ClearScriptedTask()
{
	if(m_pScriptedTask)
	{
		delete m_pScriptedTask; m_pScriptedTask = NULL;
	}
}

bool	CTaskPlayerOnFoot::CheckForDuckAndCover(CPed* pPlayerPed)
{

	if( CTaskPlayerOnFoot::ms_bEnableDuckAndCover )
	{
		CControl* pControl = pPlayerPed->GetControlFromPlayer();
		if(pControl && pControl->GetPedDuckAndCover().IsPressed())
		{
			// See if there are any recent explosions (or similar events to duck and cover from)
			// that we can perceive

			CEventGroupGlobal* list = GetEventGlobalGroup();
			const int num = list->GetNumEvents();

			for(int i=0; i<num; i++ )
			{
				fwEvent* ev = list->GetEventByIndex(i);
				if(!ev || !static_cast<const CEvent*>(ev)->IsShockingEvent())
				{
					continue;
				}

				// First, check if the event can ever trigger duck-and-cover reactions.
				CEventShocking* pEvent = static_cast<CEventShocking*>(ev);
				const CEventShocking::Tunables& tunables = pEvent->GetTunables();
				if(tunables.m_DuckAndCoverCanTriggerForPlayerTime > 0.0f)
				{
					// Check if the event was sensed.
					if(pEvent->CanBeSensedBy(*pPlayerPed, 1.0f, true))
					{
						// Check the time.
						u32	timeSinceRegistration = fwTimer::GetTimeInMilliseconds() - pEvent->GetStartTime();
						if(timeSinceRegistration < (int)(tunables.m_DuckAndCoverCanTriggerForPlayerTime*1000.0f))
						{
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}

s32 CTaskPlayerOnFoot::GetDesiredStateFromPlayerInput(CPed* pPlayerPed)
{
	CControl *pControl = pPlayerPed->GetControlFromPlayer();

	//--------------------------------------------------------------------
	// Switch helmet visor
	//--------------------------------------------------------------------
	if (CTaskMotionInAutomobile::CheckForHelmetVisorSwitch(*pPlayerPed))
	{
		pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingHelmetVisor,true);
		return STATE_TAKE_OFF_HELMET;
	}

	if(!pPlayerPed->GetIsSwimming() &&
		pPlayerPed->GetIsStanding() && !pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInTheAir))
	{
		//--------------------------------------------------------------------------
		// TAKE OFF PARACHUTE PACK
		//--------------------------------------------------------------------------

		//Check if the parachute pack variation is active.
		if(CTaskParachute::IsParachutePackVariationActiveForPed(*pPlayerPed) && 
			!pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_DisableTakeOffParachutePack) &&
			(GetTimeInState() > 0.1f))
		{
			return STATE_TAKE_OFF_PARACHUTE_PACK;
		}

#if ENABLE_JETPACK
		//--------------------------------------------------------------------------
		// TAKE OFF JETPACK
		//--------------------------------------------------------------------------
		if(pPlayerPed->GetHasJetpackEquipped() && HasValidEnterVehicleInput(*pControl) && (GetTimeInState() > 0.1f))
		{
			return STATE_TAKE_OFF_JETPACK;
		}
#endif //ENABLE_JETPACK
	}

	//--------------------------------------------------------------------------
	// TAKE OFF SCUBA GEAR
	//--------------------------------------------------------------------------

	//Check if the scuba gear variation is active.
	if(CTaskMotionSwimming::IsScubaGearVariationActiveForPed(*pPlayerPed) && !pPlayerPed->GetIsSwimming() &&
		pPlayerPed->GetIsStanding() && !pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInTheAir) &&
		!pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_DisableTakeOffScubaGear) &&
		!pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableTakeOffScubaGear) &&
		(GetTimeInState() > 0.1f) && !pPlayerPed->GetGroundPhysical())
	{
		return STATE_TAKE_OFF_SCUBA_GEAR;
	}

	//--------------------------------------------------------------------------
	// SWITCH WEAPON
	//--------------------------------------------------------------------------
	const bool bEnterPressed = HasValidEnterVehicleInput(*pControl) && !pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_ExitVehicleTaskFinishedThisFrame);

	const bool bUseMobile = CTaskMobilePhone::IsRunningMobilePhoneTask(*pPlayerPed);

	if(pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired))
	{
		const bool bShouldUnholsterWeapon = 
			pControl->GetMeleeAttackLight().IsPressed() ||
			pPlayerPed->GetPlayerInfo()->IsAiming() || 
			pPlayerPed->GetPlayerInfo()->IsFiring();
		if(bShouldUnholsterWeapon)
		{
			pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired, false);
		}
	}

	CPedWeaponManager *pWeapMgr = pPlayerPed->GetWeaponManager();
	bool bNeedsWeaponSwitch;
	if ( pWeapMgr && !bEnterPressed )
	{
		bNeedsWeaponSwitch = pWeapMgr->GetRequiresWeaponSwitch();
	}
	else
	{
		bNeedsWeaponSwitch = false;
	}

	if (pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_ExitVehicleTaskFinishedThisFrame) && pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate))
	{
		bNeedsWeaponSwitch = false;
	}

	//! Defer weapon swap until next frame, or we get tag syncing issues.
	if(GetPreviousState() == STATE_JUMP && GetTimeInState() == 0.0f)
	{
		bNeedsWeaponSwitch = false;
	}

	// Harpoon: Don't swap weapons if we're in the SwimIdleIntro state
	if (pPlayerPed->GetIsSwimming())
	{
		const CTaskMotionBase* pMotionTask = pPlayerPed->GetCurrentMotionTask(false);
		if(pMotionTask && pMotionTask->GetTaskType() == CTaskTypes::TASK_MOTION_AIMING && pMotionTask->GetState() == CTaskMotionAiming::State_SwimIdleIntro)
		{
			bNeedsWeaponSwitch = false;
		}
	}

#if FPS_MODE_SUPPORTED
	const bool bInFpsMode = pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false);
#endif // FPS_MODE_SUPPORTED

	if(GetState() == STATE_PLAYER_GUN)
	{
		bool bFPSThrownWeapon = false;
#if FPS_MODE_SUPPORTED
		if(bInFpsMode && pWeapMgr->GetEquippedWeaponInfo() && pWeapMgr->GetEquippedWeaponInfo()->GetIsThrownWeapon())
		{
			bFPSThrownWeapon = true;
			const CTask *pProjectileTask = pPlayerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_AND_THROW_PROJECTILE);
			if(pProjectileTask && (pProjectileTask->GetState() == CTaskAimAndThrowProjectile::State_ThrowProjectile || pProjectileTask->GetState() == CTaskAimAndThrowProjectile::State_PlaceProjectile))
			{
				bNeedsWeaponSwitch = false;
			}
		}
#endif
		if(!bFPSThrownWeapon)
		{
			bNeedsWeaponSwitch = false;
		}
	}

#if FPS_MODE_SUPPORTED
	// Not swimming, not when switching to or from unarmed
	const bool bShouldReloadAsPartOfGunTask = bInFpsMode && bNeedsWeaponSwitch && !pPlayerPed->GetIsSwimming() && 
		pWeapMgr && pWeapMgr->GetEquippedWeaponHash() != pPlayerPed->GetDefaultUnarmedWeaponHash() && pWeapMgr->GetEquippedWeaponObjectHash() != 0 &&
		pWeapMgr->GetEquippedWeaponInfo() && !pWeapMgr->GetEquippedWeaponInfo()->GetIsProjectile();
#endif // FPS_MODE_SUPPORTED

	if (!bUseMobile && pWeapMgr 
#if ENABLE_DRUNK
		&& !pPlayerPed->IsDrunk()
#endif // ENABLE_DRUNK
		)
	{
		if (bNeedsWeaponSwitch && !CControlMgr::IsDisabledControl(pPlayerPed->GetControlFromPlayer()))
		{
			const CWeaponInfo* pInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(pWeapMgr->GetEquippedWeaponHash());
			if(pInfo)
			{
				if (!pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_SwimmingTasksRunning) || pInfo->GetUsableUnderwater())
				{
#if FPS_MODE_SUPPORTED
					if(!bShouldReloadAsPartOfGunTask)
#endif // FPS_MODE_SUPPORTED
					{
						// Ensure weapon is streamed in
						if(pPlayerPed->GetInventory()->GetIsStreamedIn(pInfo->GetHash()))
						{
							if(pInfo->GetIsCarriedInHand() || pWeapMgr->GetEquippedWeaponObject() != NULL)
							{
								taskDebugf1("CTaskPlayerOnFoot::GetDesiredStateFromPlayerInput: STATE_SWAP_WEAPON - ln 5665. Current State: %d,  Previous State: %d", GetState(), GetPreviousState());
								return STATE_SWAP_WEAPON;
							}
						}
					}
				}				
			}
		}
	}

	//-------------------------------------------------------------------------
	// MELEE RANDOM_AMBIENT MOVES
	// This is always handled.
	//
	// Note that SHOOTING is after this since we may wish to do a
	// rifle butts, pistol whips, or executions instead of a normal weapon fire.
	//
	// We use the action manager to test the current conditions (some of which
	// may use weapons or a complex input state such as a button being released
	// and not just pressed down) to check to see if there is a request to do a
	// rifle butt, pistol whip, execution, a running attack, or a stun attack
	// (like a sucker punch or garot move), etc.
	//
	// Currently disabled for swimming.
	//-------------------------------------------------------------------------
	if(pWeapMgr && !pPlayerPed->GetIsSkiing() && !pPlayerPed->GetIsMeleeDisabled() && 
#if ENABLE_DRUNK
		!pPlayerPed->IsDrunk() && 
#endif // ENABLE_DRUNK
		!pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_InAirDefenceSphere))
	{
		//! Don't allow melee if we don't have the correct weapon equipped yet.
		const CWeapon* pEquippedWeapon = pPlayerPed->GetWeaponManager() ? pPlayerPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
		u32 uEquippedWeaponHash = pEquippedWeapon ? pEquippedWeapon->GetWeaponHash() : -1;
		u32 uSelectedWeaponHash =  pPlayerPed->GetWeaponManager() ? pPlayerPed->GetWeaponManager()->GetSelectedWeaponHash() : -1;
		if (uSelectedWeaponHash == uEquippedWeaponHash)
		{
			bool bGoIntoMeleeEvenWhenNoSpecificMoveFound = false;
			bool bAllowStrafeMode = false;

			// Get the weapon (non melee weapons are handled in CTaskPlayerOnFoot::CheckForArmedMeleeAction)
			const CWeaponInfo* pWeaponInfo = pWeapMgr->GetEquippedWeaponInfo();
			if( pWeaponInfo && pWeaponInfo->GetIsMelee() && pWeaponInfo->GetAllowCloseQuarterKills() )
			{
				// See if we already have or can find a target.
				CEntity* pTargetEntity = pPlayerPed->GetPlayerInfo()->GetTargeting().GetLockOnTarget();		
				bool bHasLockOnTarget = pTargetEntity != NULL;

				// Make sure the target is a ped and not in a vehicle and is holding a melee weapon
				CPed* pTargetPed = NULL;
				if( pTargetEntity && pTargetEntity->GetIsTypePed() )
				{
					pTargetPed = static_cast<CPed*>(pTargetEntity);

					// NULL out the target ped as they are in the process of being killed
					if( pTargetPed->GetPedConfigFlag( CPED_CONFIG_FLAG_Knockedout ) ||
						pTargetPed->GetPedConfigFlag( CPED_CONFIG_FLAG_KilledByStealth ) ||
						pTargetPed->GetPedConfigFlag( CPED_CONFIG_FLAG_KilledByTakedown ) )
					{
						pTargetPed = NULL;
					}
				}

				bool bTargetIsFriendlyPed = pTargetPed && pTargetPed->GetPedConfigFlag(CPED_CONFIG_FLAG_AllowPlayerLockOnIfFriendly);
				bool bDontRaiseFistsWhenLockedOn = pTargetPed ? pTargetPed->GetPedResetFlag( CPED_RESET_FLAG_DontRaiseFistsWhenLockedOn ) : false;

				// Allow targeting players on bikes with melee weapons when in a multiplayer game
				bool bAllowMeleeTargetingInVehicle = NetworkInterface::IsGameInProgress() && pTargetPed && pTargetPed->IsPlayer() && pTargetPed->GetVehiclePedInside() 
					&& (pTargetPed->GetVehiclePedInside()->InheritsFromBike() || pTargetPed->GetVehiclePedInside()->InheritsFromQuadBike() || pTargetPed->GetVehiclePedInside()->InheritsFromAmphibiousQuadBike());

				if( pTargetPed && (!pTargetPed->GetIsInVehicle() || bAllowMeleeTargetingInVehicle) )
				{
					Vector3 vPlayerToTarget = VEC3V_TO_VECTOR3( pTargetEntity->GetTransform().GetPosition() - pPlayerPed->GetTransform().GetPosition() );
					if( vPlayerToTarget.Mag2() < rage::square( CTaskPlayerOnFoot::ms_fMeleeStartDistance ) && 
						( !pPlayerPed->GetIsCrouching() || pPlayerPed->CanPedStandUp() ) && !pPlayerPed->GetIsFPSSwimming())
					{
						bGoIntoMeleeEvenWhenNoSpecificMoveFound = true;

						// Check to see if we should go directly into strafe mode
						if( !bDontRaiseFistsWhenLockedOn || ( GetWasMeleeStrafing() && ( GetLastTimeInMeleeTask() + ms_nRestartMeleeDelayInMS ) > fwTimer::GetTimeInMilliseconds() ) )
							bAllowStrafeMode = true;
					}
				}
			
				bool bAttemptTaunt = false;
				if( !bTargetIsFriendlyPed && (CTaskMelee::ShouldCheckForMeleeAmbientMove( pPlayerPed, bAttemptTaunt, false ) || bGoIntoMeleeEvenWhenNoSpecificMoveFound) )
				{
					// Initialize the melee intro bool
					bool bPerformMeleeIntroAnim = false;
					if( !bDontRaiseFistsWhenLockedOn && ( ( GetLastTimeInMeleeTask() + ms_nIntroAnimDelayInMS ) < fwTimer::GetTimeInMilliseconds() ) )
						bPerformMeleeIntroAnim = pWeaponInfo ? pWeaponInfo->GetAllowMeleeIntroAnim() : false;

					//! Note: The target selection will cache the selected action for the target. Just re-use this
					//! CheckForAndGetMeleeAmbientMove() to prevent recalculating it.
					CTaskMelee::ResetLastFoundActionDefinition();

					// Try to find a suitable ambient target.
					if( !pTargetEntity && pPlayerPed->IsLocalPlayer() )
					{
						CPlayerPedTargeting& rTargeting = pPlayerPed->GetPlayerInfo()->GetTargeting();

						pTargetEntity = rTargeting.FindMeleeTarget(pPlayerPed, pWeaponInfo, true);
					}

					m_pMeleeRequestTask = CTaskMelee::CheckForAndGetMeleeAmbientMove( pPlayerPed, 
																					  pTargetEntity,
																					  bHasLockOnTarget, 
																					  bGoIntoMeleeEvenWhenNoSpecificMoveFound, 
																					  bPerformMeleeIntroAnim, 
																					  bAllowStrafeMode, 
																					  pTargetEntity != NULL );

					// One last ditch effort to find a no target melee action
					if( !m_pMeleeRequestTask )
					{
						m_pMeleeRequestTask = CTaskMelee::CheckForAndGetMeleeAmbientMove( pPlayerPed, 
																						  NULL,
																						  false, 
																						  bGoIntoMeleeEvenWhenNoSpecificMoveFound, 
																						  bPerformMeleeIntroAnim, 
																						  bAllowStrafeMode, 
																						  false );
					}

					if( m_pMeleeRequestTask )
					{
						pPlayerPed->SetIsCrouching( false, -1, false );
						taskDebugf1("STATE_MELEE: CTaskPlayerOnFoot::GetDesiredStateFromPlayerInput: m_pMeleeRequestTask");
						return STATE_MELEE;
					}
				}
			}
		}
	}

	bool bSprinting = false;
	const Vector2 &desiredMBR = pPlayerPed->GetMotionData()->GetDesiredMoveBlendRatio();
	if(abs(desiredMBR.x) > 0.01f)
	{
		bSprinting = pPlayerPed->GetMotionData()->GetIsSprinting(desiredMBR.Mag());
	}
	else
	{
		bSprinting = pPlayerPed->GetMotionData()->GetIsSprinting(desiredMBR.y);
	}

	// Cache off whether or not this is a heavy weapon
	bool bHeavyWeapon = false;
	if( pWeapMgr )
	{
		const CWeapon* pWeaponUsable = pWeapMgr->GetEquippedWeapon();
		const CWeaponInfo* pWeaponInfo = pWeaponUsable ? pWeaponUsable->GetWeaponInfo() : NULL;
		bHeavyWeapon = pWeaponInfo ? pWeaponInfo->GetIsHeavy() : false;
	}

	//-------------------------------------------------------------------------
	// COVER
	//-------------------------------------------------------------------------
	bool bWantsToEnterCover = false;
	if(!bHeavyWeapon && !m_bPlayerExitedCoverThisUpdate && !pPlayerPed->GetIsSwimming() && !pPlayerPed->GetIsSkiing() && 
#if ENABLE_DRUNK
		!pPlayerPed->IsDrunk() && 
#endif // ENABLE_DRUNK
		pPlayerPed->GetWeaponManager() && CTaskEnterCover::AreCoreCoverClipsetsLoaded(pPlayerPed))
	{
		if(CheckForNearbyCover(pPlayerPed))
		{
			bWantsToEnterCover = true;
		}
	}

	//-------------------------------------------------------------------------
	// ENTER VEHICLE
	//-------------------------------------------------------------------------

	s8 iEnterVehicleState = CheckShouldEnterVehicle(pPlayerPed,pControl, m_bGetInVehicleAfterCombatRolling);
#if __BANK
	if (iEnterVehicleState == STATE_GET_IN_VEHICLE)
	{
		AI_LOG("[Player] - iEnterVehicleState = STATE_GET_IN_VEHICLE\n");
	}
#endif // __BANK

	//-------------------------------------------------------------------------
	// SHOOTING
	//-------------------------------------------------------------------------
	const CWeaponInfo* pWeaponInfo = pPlayerPed->GetEquippedWeaponInfo();
	bool bUsableUnderwater = false; 
	if (pWeaponInfo)
	{
		bUsableUnderwater = pWeaponInfo->GetUsableUnderwater();
	}
	if (pWeapMgr && (!pPlayerPed->GetIsSwimming() || bUsableUnderwater) && (!bNeedsWeaponSwitch || bShouldReloadAsPartOfGunTask) && 
#if ENABLE_DRUNK
		!pPlayerPed->IsDrunk() && 
#endif // ENABLE_DRUNK
		!CNewHud::IsShowingHUDMenu() && !pPlayerPed->GetPedIntelligence()->FindTaskActiveByTreeAndType(PED_TASK_TREE_SECONDARY, CTaskTypes::TASK_RELOAD_GUN))
	{
#if 0 // CS
		const bool bCarryingThrowableObject = pPlayerPed->GetWeaponMgr()->GetChosenWeaponType()==WEAPONTYPE_OBJECT &&
			pPlayerPed->GetWeaponMgr()->GetWeaponObject() &&
			pPlayerPed->GetWeaponMgr()->GetWeaponObject()->GetModelIndex() != CPedPhoneComponent::GetPhoneModelIndexSafe(pPlayerPed);
#else
		const bool bCarryingThrowableObject = false;
#endif // 0
		const CWeapon* pWeaponUsable = pWeapMgr->GetEquippedWeapon();
		const CWeaponInfo* pWeaponInfo = pWeaponUsable ? pWeaponUsable->GetWeaponInfo() : NULL;

		bool bIsUsingThrownWeapon =  pWeaponInfo && pWeaponInfo->GetIsThrownWeapon();
		bool bIsAWeapon = !pWeaponInfo || !pWeaponInfo->GetIsNotAWeapon(); 

		// Should the player reload or has the player hit the input for reloading?
		bool bWantsToReload = false;
		bool bNeedsToReload = false;
		bool bCanReload = false;
		if(pWeaponUsable)
		{
			bCanReload = pWeaponUsable->GetCanReload() && !pPlayerPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_CAMERA_VIEW_MODE_SWITCHED_TO_OR_FROM_FIRST_PERSON);
			bWantsToReload = (pControl->GetPedReload().IsPressed() || pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ForceReload)) && bCanReload;
#if USE_SIXAXIS_GESTURES
			if(CControlMgr::GetPlayerPad() && CPadGestureMgr::GetMotionControlEnabled(CPadGestureMgr::MC_TYPE_RELOAD) && !bWantsToReload)
			{
				CPadGesture* gesture = CControlMgr::GetPlayerPad()->GetPadGesture();
				if(gesture && gesture->GetHasReloaded())
				{
					bWantsToReload = true;
				}
			}
#endif // USE_SIXAXIS_GESTURES
			bNeedsToReload = pWeaponUsable->GetNeedsToReload(true) || ( pWeaponInfo->GetCreateVisibleOrdnance() && pPlayerPed->GetWeaponManager() && pPlayerPed->GetWeaponManager()->GetPedEquippedWeapon()->GetProjectileOrdnance() == NULL );			
		}

		bool bFirstPersonAimOrFire = false;
		bool bFPSToggleRunReload = false;
#if FPS_MODE_SUPPORTED
		bFirstPersonAimOrFire = (pPlayerPed->GetPlayerInfo()->IsAiming() || pPlayerPed->GetPlayerInfo()->IsFiring()) && pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false);
		bFPSToggleRunReload = pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false) && camFirstPersonShooterCamera::WhenSprintingUseRightStick(pPlayerPed, *pControl) && (bWantsToReload || bNeedsToReload) && bCanReload;
#endif
		bool bKeyboardMouseAimOrFire = false;
		bool bKeyboardMouseReload = false;
#if KEYBOARD_MOUSE_SUPPORT
		bKeyboardMouseAimOrFire = pControl->WasKeyboardMouseLastKnownSource() && (pPlayerPed->GetPlayerInfo()->IsAiming() || pPlayerPed->GetPlayerInfo()->IsFiring());
		bKeyboardMouseReload = pControl->WasKeyboardMouseLastKnownSource() && (bWantsToReload || bNeedsToReload) && bCanReload;
#endif
		bool bCanEnterGunTaskFromSprint = bFirstPersonAimOrFire || bKeyboardMouseAimOrFire || bKeyboardMouseReload || bFPSToggleRunReload || (!bSprinting && pPlayerPed->GetPlayerInfo()->IsSprintAimBreakOutOver()) || bShouldReloadAsPartOfGunTask;

		// Allow aiming if swimming and sprinting
		if(bIsAWeapon && (bIsUsingThrownWeapon || (bCanEnterGunTaskFromSprint || CPlayerInfo::IsCombatRolling() || pPlayerPed->GetIsSwimming())))
		{
			// Can the player fire or has the player hit the input for firing?
			bool bCanPlayerFire    = ( (pWeapMgr->GetIsArmed() && !pWeapMgr->GetIsArmedMelee()) || bCarryingThrowableObject) && !pWeapMgr->GetIsNewEquippableWeaponSelected();
			bool bConsiderAttackTriggerAiming = !bCarryingThrowableObject && !bIsUsingThrownWeapon && !pPlayerPed->GetIsInCover() && !pPlayerPed->GetIsSwimming();
			bool bCanLockonOnFoot = pWeaponUsable ? pWeaponUsable->GetCanLockonOnFoot() : false;
			bool bAimPressed = (pPlayerPed->GetPlayerInfo()->IsAiming(bConsiderAttackTriggerAiming) && (!pWeaponInfo || bCanLockonOnFoot || pWeaponInfo->GetCanFreeAim()));
			bool bFirePressed = pPlayerPed->GetPlayerInfo()->IsFiring();
			bool bCanBeAimedLikeGun = pWeaponInfo && pWeaponInfo->GetCanBeAimedLikeGunWithoutFiring();

			// Force player to aim when trying to fire while swimming
			bool bMustAimToFire = pPlayerPed->GetIsSwimming() ? bAimPressed : true;

			if (pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_ForcePlayerFiring))
			{
				bFirePressed = true;
			}

			// B*2592749: Don't allow fire-only/thrown weapons to enter gun tasks if in an air defence sphere (ie jerry can etc).
			if (bCanPlayerFire && pWeaponInfo && (pWeaponInfo->GetOnlyAllowFiring() || pWeaponInfo->GetIsThrownWeapon()) && pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_InAirDefenceSphere))
			{
				bCanPlayerFire = false;
			}

			// For drop-when-cooked weapons, only go to player gun state when fire trigger is pressed, not held
			if(pWeaponInfo && pWeaponInfo->GetIsThrownWeapon() && pWeaponInfo->GetDropWhenCooked())
			{
				bool bPreviouslySwapping = GetPreviousState() == STATE_SWAP_WEAPON;
#if FPS_MODE_SUPPORTED
				bPreviouslySwapping |= (pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false) && pPlayerPed->GetMotionData()->GetWasFPSUnholster());
#endif
				if(!bPreviouslySwapping)
				{
					bFirePressed = CThrowProjectileHelper::IsPlayerFirePressed(pPlayerPed);
					pPlayerPed->SetPedResetFlag(CPED_RESET_FLAG_StartProjectileTaskWithPrimingDisabled, !bFirePressed);
				}				
			}

			// Reset reload from idle flag
			m_bReloadFromIdle = false;

			//! If projectile is cooking, but we aren't in projectile throw state, just go back to it, to force it to drop.
			bool bCooking = pWeaponUsable && pWeaponUsable->GetIsCooking();

			// Start the task if the player wants to reload and can, or if they want to fire and can
			if((bCanPlayerFire && (bAimPressed || bFirePressed || bCooking)) || ((bWantsToReload || bNeedsToReload) && bCanReload) || bShouldReloadAsPartOfGunTask)
			{
				if (!bAimPressed && (bWantsToReload || bNeedsToReload)
#if FPS_MODE_SUPPORTED
					&& !pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false))
#else				
					)
#endif
				{
					m_bReloadFromIdle = true;
				}

				if (pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired))
				{
					pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired, false);

					bNeedsWeaponSwitch = pWeapMgr->GetRequiresWeaponSwitch();

					if (bNeedsWeaponSwitch)
					{
						const CWeaponInfo* pInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(pWeapMgr->GetEquippedWeaponHash());
						if(pInfo)
						{
							if (!pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_SwimmingTasksRunning) || pInfo->GetUsableUnderwater())
							{
								// Ensure weapon is streamed in
								if(pPlayerPed->GetInventory()->GetIsStreamedIn(pInfo->GetHash()))
								{
									if(pInfo->GetIsCarriedInHand() || pWeapMgr->GetEquippedWeaponObject() != NULL)
									{
										taskDebugf1("CTaskPlayerOnFoot::GetDesiredStateFromPlayerInput: STATE_SWAP_WEAPON - ln 5966. Current State: %d,  Previous State: %d", GetState(), GetPreviousState());
										return STATE_SWAP_WEAPON;
									}
								}
							}				
						}
					}
				}

				//thrown weapons must be swapped 1st
				if (!pWeaponUsable)
				{
					const CWeaponInfo* pEquippedWeaponInfo = pWeapMgr->GetEquippedWeaponInfo();
					if(!pEquippedWeaponInfo || pEquippedWeaponInfo->GetIsThrownWeapon())
						bNeedsWeaponSwitch = true;
				}

				if(!bNeedsWeaponSwitch || bShouldReloadAsPartOfGunTask)
				{
					const bool bSniperWeapon = pWeaponUsable && pWeaponUsable->GetWeaponInfo()->GetGroup() == WEAPONGROUP_SNIPER;
					bool bActivateSniperScope = bSniperWeapon && (CNewHud::IsShowingHUDMenu());
					
					//Allow transition to gun state from diving down (after a min time fTimeBeforeAllowAim to ensure anim transition looks ok)
					bool bIsDivingDown = false;
					CTask *pActiveMotionTask = pPlayerPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_DIVING);
					if (pActiveMotionTask)
					{
						TUNE_GROUP_FLOAT(DIVING_DOWN_AIMING_TUNE, fTimeBeforeAllowAim, 1.25f, 0.0f, 4.0f, 0.01f);
						CTaskMotionDiving *pDivingTask = static_cast<CTaskMotionDiving*>(pPlayerPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_DIVING));
						if (pDivingTask && pDivingTask->GetState() == CTaskMotionDiving::State_DivingDown)
						{
							if (pDivingTask->GetTimeInState() < fTimeBeforeAllowAim)
							{
								bIsDivingDown = true;
							}
							else
							{
								pDivingTask->GetParent()->GetMoveNetworkHelper()->SetBoolean(CTaskMotionPed::ms_TransitionClipFinishedId, true);
							}
						}
					}

					// Harpoon: Only enter gun state if SwimIdleOutro isn't playing
					bool bIsPlayingSwimIdleOutro = false;
					if (pPlayerPed->GetIsSwimming())
					{
						const CTaskMotionBase* pMotionTask = pPlayerPed->GetCurrentMotionTask(false);
						if(pMotionTask && pMotionTask->GetTaskType() == CTaskTypes::TASK_MOTION_AIMING && pMotionTask->GetState() == CTaskMotionAiming::State_SwimIdleOutro)
						{
							bIsPlayingSwimIdleOutro = true;
						}
					}

					if(!bActivateSniperScope && !bIsDivingDown && bMustAimToFire && !bIsPlayingSwimIdleOutro && !bWantsToEnterCover && iEnterVehicleState == -1
#if FPS_MODE_SUPPORTED
						// Don't go in gun state if holding a phone
					   &&!(pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer() && CTaskPlayerOnFoot::CheckForUseMobilePhone(*pPlayerPed))
#endif
					   )
					{
						taskDebugf1("CTaskPlayerOnFoot::GetDesiredStateFromPlayerInput: STATE_PLAYER_GUN - !bActivateSniperScope etc. Current State: %d,  Previous State: %d", GetState(), GetPreviousState());
						return STATE_PLAYER_GUN;
					}
				}
			}
			// Special case for weapons that can be aimed like a gun but do not fire
			else if( bAimPressed && bCanBeAimedLikeGun )
			{
				return STATE_PLAYER_GUN;
			}
		}
	}

	//-------------------------------------------------------------------------
	// Slope Scramble
	//-------------------------------------------------------------------------
	if( !pPlayerPed->GetIsSwimming() && CTaskSlopeScramble::CanDoSlopeScramble(pPlayerPed) )
	{
		m_fTimeSinceLastValidSlopeScramble += fwTimer::GetTimeStep();
		++m_nFramesOfSlopeScramble;
		float TimeBeforeScramble = CTaskSlopeScramble::GetTimeBeforeScramble(pPlayerPed->GetGroundNormal(), pPlayerPed->GetVelocity(), ZAXIS);

		//Displayf("SlopeScrambleTimer: %f, TimeBeforeScramble: %f/n", m_fTimeSinceLastValidSlopeScramble, TimeBeforeScramble);

		TUNE_GROUP_FLOAT (SLOPE_SCRAMBLE, fDisabledTimerGroundZMax, 0.6f, 0.0f, 1.0f, 0.01f);
		TUNE_GROUP_INT (SLOPE_SCRAMBLE, nSlopeScrambleFrames, 4, 0, 1000, 1);

		// Don't consider time if we are not moving
		if ((m_fTimeSinceLastValidSlopeScramble > TimeBeforeScramble || abs(pPlayerPed->GetGroundNormal().z) > fDisabledTimerGroundZMax) && m_nFramesOfSlopeScramble > nSlopeScrambleFrames )
		{
			if(DoJumpCheck(pPlayerPed, NULL, bHeavyWeapon, true))
			{
				return STATE_JUMP;
			}
			return STATE_SLOPE_SCRAMBLE;
		}
	}
	else
	{
		//we're no longer on a slope, reset. 
		m_fTimeSinceLastValidSlopeScramble = 0.0f;
		m_nFramesOfSlopeScramble = 0;
	}

	//-------------------------------------------------------------------------
	// DuckAndCover 
	//-------------------------------------------------------------------------
	if( !pPlayerPed->GetIsSwimming() && CheckForDuckAndCover(pPlayerPed) )
	{
		return STATE_DUCK_AND_COVER;
	}

	//-------------------------------------------------------------------------
	// UNCUFF
	//-------------------------------------------------------------------------
	if(CheckShouldPerformUncuff())
	{
		return STATE_UNCUFF;
	}

	//-------------------------------------------------------------------------
	// SCAN FOR LADDERS, MOUNTS, AND CARS
	// We will normally choose to climb the ladder over entering
	// the car, except where the ladder is behind us.
	//-------------------------------------------------------------------------
	if(iEnterVehicleState != -1)
	{
		return (ePlayerOnFootState)iEnterVehicleState;
	}

	//******************************************************************
	//Ladder
	if (CheckShouldClimbLadder(pPlayerPed))
	{
		return STATE_CLIMB_LADDER;
	}


	// Don't allow player to go into cover whilst using the phone
	if (GetState() == STATE_PLAYER_IDLES)
	{
		if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_PLAYER_IDLES)
		{
			if ( CTaskMobilePhone::IsRunningMobilePhoneTask(*pPlayerPed) && !CPhoneMgr::GetGoingOffScreen())
			{
				return STATE_PLAYER_IDLES;
			}
		}
	}

	//-------------------------------------------------------------------------
	// JETPACK
	//-------------------------------------------------------------------------
	if(CheckForUsingJetpack(pPlayerPed))
	{
		return STATE_JETPACK;
	}

	//-------------------------------------------------------------------------
	// JUMPING INTO COVER
	//-------------------------------------------------------------------------
	if(bWantsToEnterCover)
	{
		return STATE_USE_COVER;
	}

	//-------------------------------------------------------------------------
	// Jump/Vault.
	//-------------------------------------------------------------------------
	if(DoJumpCheck(pPlayerPed, pControl, bHeavyWeapon))
		return STATE_JUMP;

	//-------------------------------------------------------------------------
	// Drop Downs.
	//-------------------------------------------------------------------------
	if(!bHeavyWeapon && CheckForDropDown(pPlayerPed, pControl))
	{
		if(pPlayerPed->GetPedIntelligence()->GetDropDownDetector().HasDetectedParachuteDrop() || pPlayerPed->GetPedIntelligence()->GetDropDownDetector().HasDetectedDive())
		{
			return STATE_JUMP;
		}
		else
		{
			return STATE_DROPDOWN;
		}
	}

#if FPS_MODE_SUPPORTED
	bool bUsingMobilePhone = CTaskMobilePhone::IsRunningMobilePhoneTask(*pPlayerPed) || CTaskPlayerOnFoot::CheckForUseMobilePhone(*pPlayerPed);
	if(pWeaponInfo && (pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(true, false, true) && (!pPlayerPed->GetIsSwimming() && !bUsingMobilePhone)))
	{
		return STATE_PLAYER_GUN;
	}
#endif // FPS_MODE_SUPPORTED

	//--------------------------------------------------------------------
	// Bird crapping
	//--------------------------------------------------------------------
	if (CheckForBirdProjectile(pPlayerPed))
	{
		return STATE_BIRD_PROJECTILE;
	}
	else
	{
		m_fTimeSinceBirdProjectile += GetTimeStep();
	}

	return GetState();
}

bool CTaskPlayerOnFoot::GetLastStickInputIfValid(Vector2& vStickInput) const
{	
	if (m_fTimeSinceLastStickInput < TIME_TO_CONSIDER_OLD_INPUT_VALID)
	{
		vStickInput = Vector2(m_vLastStickInput.x, m_vLastStickInput.y);
		return true;
	}
	return false;
}

void CTaskPlayerOnFoot::UpdatePlayerLockon(CPed* pPlayerPed)
{
	// If the player is entering the car or jumping, disable lock on
	if( pPlayerPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_JUMPVAULT ) ||
		pPlayerPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_CLIMB_LADDER ) ||
		pPlayerPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_ENTER_VEHICLE ) ||
		pPlayerPed->GetUsingRagdoll() )
	{
		pPlayerPed->SetPedResetFlag( CPED_RESET_FLAG_DisablePlayerLockon, true );
	}
}

bool CTaskPlayerOnFoot::IsValidForMotionTask(CTaskMotionBase& task) const
{
	bool isValid = false;
	
	if (task.IsOnFoot() || task.IsInWater())
		isValid = true;

	if(!isValid && GetSubTask())
	{
		//This task is not valid, but an active subtask might be.
		isValid = GetSubTask()->IsValidForMotionTask(task);
	}

	return isValid;
}

#if !__FINAL

void CTaskPlayerOnFoot::Debug() const
{
	if(GetSubTask())
	{
		GetSubTask()->Debug();
	}

	const CPed* pPed = GetPed();
#if __BANK
	if (pPed->IsLocalPlayer())
	{
		pPed->GetPlayerInfo()->GetVehicleClipRequestHelper().Debug();
	}
#endif
	const CControl *pControl = pPed->GetControlFromPlayer();
	bool bPlayerWantsToEnterCover = pControl->GetPedCover().IsPressed();//pControl->GetPedEnter().HistoryPressed(iTimePeriod, &iLastTimeTransitioned) && pControl->GetPedEnter().HistoryReleased(fwTimer::GetTimeInMilliseconds() - iLastTimeTransitioned);

#if __BANK
	Color32 iColor = ((fwTimer::GetTimeInMilliseconds() < pPed->GetPlayerInfo()->GetTimeLastWeaponPickedUp()+ms_iQuickSwitchWeaponPickUpTime)) ? Color_green : Color_red;
	grcDebugDraw::AddDebugOutput(iColor, "Last Weapon Hash : %u, Time Since Last Weapon Pickup: %u, Current Time %u", pPed->GetPlayerInfo()->GetLastWeaponHashPickedUp(), pPed->GetPlayerInfo()->GetTimeLastWeaponPickedUp(), fwTimer::GetTimeInMilliseconds());
#endif

	// If the cover search isn't being rendered, don't search if the button isn't pressed
#if __DEV
	if( !CCoverDebug::ms_Tunables.m_RenderPlayerCoverSearch )
#endif
	{
		if( !bPlayerWantsToEnterCover )
		{
			return;
		}
	}
}
#endif

#if __BANK
void CTaskPlayerOnFoot::DebugVehicleGetIns()
{
#if DEBUG_DRAW
	CEntity* pEnt = CDebugScene::FocusEntities_Get(0);
	TUNE_GROUP_BOOL(VEHICLE_GET_ONS,debugVehicleGetOnClips, false);
	if(debugVehicleGetOnClips && pEnt && pEnt->GetIsTypeVehicle() )
	{
		CVehicle* pTrailer = static_cast<CVehicle*>(pEnt);

		const s32 iNumberEntryExitPoints = pTrailer->GetNumberEntryExitPoints();

		TUNE_GROUP_BOOL(VEHICLE_GET_ONS,debugSpecificEntryPoint, false);
		TUNE_GROUP_INT(VEHICLE_GET_ONS,specificEntryPoint, 0, 0, 10, 1);

		// Go through entry exit point and see if it can 
		for( s32 i = 0; i < iNumberEntryExitPoints; i++ )
		{
			SeatAccess seatAccess = SA_directAccessSeat;
			const CEntryExitPoint* pEntryPoint = pTrailer->GetEntryExitPoint(i);
			Vector3 vOpenDoorPos;
			{
				TUNE_GROUP_BOOL(VEHICLE_GET_ONS,renderMyStuff, true);
				TUNE_GROUP_BOOL(VEHICLE_GET_ONS,renderText, true);

				Matrix34 carMat;
				pTrailer->GetMatrixCopy(carMat);
				if (renderMyStuff)
					grcDebugDraw::Sphere(carMat.d, 0.025f, Color_white);

				Matrix34 localSeatMatrix;
				localSeatMatrix.Identity();

				s32 iSeatIndex = pEntryPoint->GetSeat(seatAccess);

				CVehicleModelInfo* pModeInfo = pTrailer->GetVehicleModelInfo();
				int iBoneIndex = pModeInfo->GetModelSeatInfo()->GetBoneIndexFromSeat(iSeatIndex);
				Assertf(iBoneIndex!=-1, "%s:GetEnterCarMatrix - Bone does not exist", pModeInfo->GetModelName());
				if(iBoneIndex!=-1)
				{
					localSeatMatrix = pTrailer->GetLocalMtx(iBoneIndex);
					Assertf(pTrailer->GetSkeletonData().GetParentIndex(iBoneIndex) == 0,"%s GetEnterCarMatrix expects the seat to be a child of the root",pModeInfo->GetModelName());
				}
				Matrix34 matResult;
				matResult.Identity();
				matResult.Dot(localSeatMatrix, carMat);
				TUNE_GROUP_FLOAT(VEHICLE_GET_ONS,arrowlength, 0.5f, -10.0f, 10.0f, 0.01f);
				TUNE_GROUP_FLOAT(VEHICLE_GET_ONS,arrowsize, 0.1f, -10.0f, 10.0f, 0.01f);
				if (renderMyStuff)
				{
					if (!debugSpecificEntryPoint || (debugSpecificEntryPoint && i == specificEntryPoint))
					{
						grcDebugDraw::Line(carMat.d, matResult.d, Color_white, Color_blue);
						grcDebugDraw::Sphere(matResult.d, 0.05f, Color_blue);
						Vector3 vSeatDir = matResult.b;
						vSeatDir.Normalize();
						grcDebugDraw::Arrow(RCC_VEC3V(matResult.d), VECTOR3_TO_VEC3V(matResult.d + vSeatDir * arrowlength), arrowsize, Color_blue);

						if (renderText)
						{
							char szText[64];
							formatf(szText, "Direct Access Seat For Entry Point %i", i);
							grcDebugDraw::Text(matResult.d, Color_blue, szText);
						}
					}
				}

				Vector3 vResult;
				//s32 iEntryPointIndex = pTrailer->GetEntryExitPointIndex(pEntryPoint);
				//pTrailer->GetOpenDoorOffset(vResult, iEntryPointIndex , pPlayerPed);

				TUNE_GROUP_FLOAT(VEHICLE_GET_ONS,xe, 7.59f, -10.0f, 10.0f, 0.01f);
				TUNE_GROUP_FLOAT(VEHICLE_GET_ONS,ye, 0.91f, -10.0f, 10.0f, 0.01f);
				TUNE_GROUP_FLOAT(VEHICLE_GET_ONS,ze, 2.50f, -10.0f, 10.0f, 0.01f);

				TUNE_GROUP_FLOAT(VEHICLE_GET_ONS,xs, 7.59f, -10.0f, 10.0f, 0.01f);
				TUNE_GROUP_FLOAT(VEHICLE_GET_ONS,ys, 2.43f, -10.0f, 10.0f, 0.01f);
				TUNE_GROUP_FLOAT(VEHICLE_GET_ONS,zs, 1.0f, -10.0f, 10.0f, 0.01f);

				TUNE_GROUP_BOOL(VEHICLE_GET_ONS,useForcedOffset, false);
				Vector3 vStart;
				Vector3 vEnd;

				// Get the local translation of get on from start to end
				if (useForcedOffset)
				{
					vStart = Vector3(xs,ys,zs);
					vEnd = Vector3(xe,ye,ze);

					vResult = vEnd - vStart;
					vResult *= -1.0f;	
				}
// 				else	// Does the same as get opendoor offset, rewritten here so we can debug
// 				{
// 					const CVehicleEntryPointAnimInfo* pClipInfo = pTrailer->GetEntryAnimInfo(iEntryPointIndex);
// 					if(pClipInfo)
// 					{
// 						fwClipGroupId nClipGroupId = CLIP_SET_ID_INVALID;
// 						fwMvClipId nClipId = CLIP_ID_INVALID;
// 
// 						if(pClipInfo->FindClip(VehicleEntryExitClip::GET_IN,nClipGroupId,nClipId))
// 						{
// 							vStart = CClipBlender::GetMoverTrackTranslation(0.0f,nClipGroupId,nClipId);
// 							vEnd = CClipBlender::GetMoverTrackTranslation(1.0f,nClipGroupId,nClipId);
// 
// 							vResult = vEnd - vStart;
// 							vResult *= -1.0f;	
// 						}
// 					}
// 				}

				// Make relative to seat orientation
				vResult.RotateZ(rage::Atan2f(-localSeatMatrix.b.x, localSeatMatrix.b.y));
				localSeatMatrix.d += vResult;

				Matrix34 matResult2;
				matResult2.Identity();
				// Transform the entry position into world space
				matResult2.Dot(localSeatMatrix, carMat);

				if (renderMyStuff)
				{
					if (!debugSpecificEntryPoint || (debugSpecificEntryPoint && i == specificEntryPoint))
					{
						grcDebugDraw::Line(matResult.d, matResult2.d, Color_blue, Color_green);
						grcDebugDraw::Sphere(matResult2.d, 0.025f, Color_green);
						char szText[64];
						formatf(szText, "Entry Point Position For Entry Point %i", i);
						grcDebugDraw::Text(matResult2.d, Color_green, szText);

						if (renderText)
						{
							if (useForcedOffset)
							{
								formatf(szText, "Forced Mover Start Translation, (%.2f,%.2f,%.2f)", vStart.x, vStart.y, vStart.z);
								grcDebugDraw::Text(matResult2.d, Color_green, 0, 10, szText);
								formatf(szText, "Forced Mover End Translation, (%.2f,%.2f,%.2f)", vEnd.x, vEnd.y, vEnd.z);
								grcDebugDraw::Text(matResult.d, Color_blue, 0, 10, szText);
							}
							else
							{
								formatf(szText, "Clip Mover Start Translation, (%.2f,%.2f,%.2f)", vStart.x, vStart.y, vStart.z);
								grcDebugDraw::Text(matResult2.d, Color_green, 0, 10, szText);
								formatf(szText, "Clip Mover End Translation, (%.2f,%.2f,%.2f)", vEnd.x, vEnd.y, vEnd.z);
								grcDebugDraw::Text(matResult.d, Color_blue, 0, 10, szText);
							}
						}
					}
				}

				TUNE_GROUP_BOOL(VEHICLE_GET_ONS,renderEnterCarMatrixStuff, false);
				if(renderEnterCarMatrixStuff && pEntryPoint->GetEntryPointPosition(pTrailer, NULL, vOpenDoorPos ))
				{
					grcDebugDraw::Line(matResult.d, vOpenDoorPos, Color_blue, Color_yellow);
					grcDebugDraw::Sphere(vOpenDoorPos, 0.025f, Color_yellow);
					char szText[64];
					formatf(szText, "Entry Point Position For Entry Point %i", i);
					grcDebugDraw::Text(vOpenDoorPos, Color_yellow, szText);
				}
			}
		}
	}
#endif
}
#endif

void CTaskPlayerOnFoot::CleanUp()
{
	CPed* pPed = GetPed();

	if (pPed->GetPlayerInfo())
	{
		CPlayerPedTargeting & targeting = pPed->GetPlayerInfo()->GetTargeting();

		if(targeting.GetLockOnTarget())
		{
			targeting.ClearLockOnTarget();
		}
		pPed->GetPlayerInfo()->SetPlayerDataFreeAiming( false );
	}

	// Clear any arrest values
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ForcedAimFromArrest, false);
	m_pTargetPed = nullptr;
	m_pLastTargetPed = nullptr;
	m_uArrestFinishTime = 0;
	m_uArrestSprintBreakoutTime = 0;
	m_bArrestTimerActive = false;

	m_optionalVehicleHelper.ReleaseClips();
	m_chosenOptionalVehicleSet = CLIP_SET_ID_INVALID;
	m_bOptionVehicleGroupLoaded = false;
	m_bWaitingForLadder = false;

	if( m_pMeleeReactionTask )
	{
		delete m_pMeleeReactionTask;
		m_pMeleeReactionTask = NULL;
	}

	m_pPedPlayerIntimidating = NULL;

	m_fTimeSinceLastStickInput = MAX_STICK_INPUT_RECORD_TIME;

	if(m_pMeleeRequestTask)
	{
		delete m_pMeleeRequestTask;
		m_pMeleeRequestTask = NULL;
	}

	if (m_pLadderClipRequestHelper)
	{
		CLadderClipSetRequestHelperPool::ReleaseClipSetRequestHelper(m_pLadderClipRequestHelper);
		m_pLadderClipRequestHelper = NULL;
	}

	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PedIsArresting, false);

	ClearScriptedTask();
}

bool CTaskPlayerOnFoot::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	// Hack.  If this is the in-water event, but we have just purposefully quit the CTaskComplexInWater
	// due to wanting to climb a ladder or enter a vehicle - then don't allow this task to be aborted.
	if(pEvent && ((CEvent*)pEvent)->GetEventType()==EVENT_IN_WATER)
	{
		if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InWaterTaskQuitToClimbLadder ))
			return false;
	}

	// If made abortable by a task thats giving the player a melee reaction
	// start the reaction next update
	if( pEvent &&
		m_pMeleeReactionTask == NULL &&
		iPriority != CTask::ABORT_PRIORITY_IMMEDIATE &&
		((CEvent*)pEvent)->GetEventType() == EVENT_DAMAGE &&
		pPed->GetPedIntelligence()->GetPhysicalEventResponseTask() &&
		pPed->GetPedIntelligence()->GetPhysicalEventResponseTask()->GetTaskType() == CTaskTypes::TASK_MELEE &&
		pPed->GetPedIntelligence()->GetEventResponseTask() == NULL )
	{
		m_pMeleeReactionTask = (CTask*)pPed->GetPedIntelligence()->GetPhysicalEventResponseTask()->Copy();
		static_cast<CEvent*>(const_cast<aiEvent*>(pEvent))->Tick();

		// Need to update this bool to stop asserts since we aren't in a FSM update
		// In future would be better if this was set for us in CTask::MakeAbortable
		// but until all tasks are FSM tasks this is not a good idea
		ASSERT_ONLY(SetCanChangeState(true));

		// Switch to the melee response state
		// First exit the current state
		UpdateFSM( GetState(), OnExit );
		SetState(STATE_MELEE_REACTION);
		SetFlag(aiTaskFlags::RestartStateOnResume);
		//UpdateFSM( STATE_MELEE_REACTION, OnEnter);

		ASSERT_ONLY(SetCanChangeState(false));

		return false;
	}

	return CTask::ShouldAbort(iPriority, pEvent);
}

void CTaskPlayerOnFoot::DoAbort(const AbortPriority UNUSED_PARAM(priority), const aiEvent* pEvent)
{
	if (pEvent && pEvent->GetEventType() == EVENT_IN_AIR && GetState() == STATE_GET_IN_VEHICLE)
	{
		// Allow the fall task to interrupt early, Phil gave his blessing on the const_cast
		aiEvent* pNonConstAiEvent = const_cast<aiEvent*>(pEvent);
		static_cast<CEventInAir*>(pNonConstAiEvent)->SetForceInterrupt(true);
		m_bResumeGetInVehicle = true;
	}
}

float CTaskPlayerOnFoot::ComputeNewCoverSearchDirection(CPed* pPlayerPed)
{
	if (pPlayerPed->IsLocalPlayer())
	{
		const CControl* pControl = pPlayerPed->GetControlFromPlayer();
		Vector2 vStickInput(pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm());
		float fCamOrient = camInterface::GetHeading();
		if (vStickInput.Mag2() > 0.0f)
		{
			return fwAngle::LimitRadianAngle(rage::Atan2f(-vStickInput.x, vStickInput.y) + fCamOrient);
		}
		else
		{
			return fwAngle::LimitRadianAngle(fCamOrient);
		}
	}
	return -999.0f;
}

bool CTaskPlayerOnFoot::HasValidEnterVehicleInput(const CControl& rControl) const
{
	if (CPhoneMgr::CamGetState())
		return false;
	
	// Check to see if player has warped out of vehicle recently and block exit (button mash prevention)
	TUNE_GROUP_INT(VEHICLE_ENTRY_TUNE, TIME_AFTER_WARP_TO_BLOCK_ENTRY, 400, 0, 1000, 1);
	const CPed* pPed = GetPed();
	if (pPed)
	{
		CPlayerInfo* pPlayerInfo = GetPed()->GetPlayerInfo();
		if (pPlayerInfo->GetLastTimeWarpedOutOfVehicle() + TIME_AFTER_WARP_TO_BLOCK_ENTRY > fwTimer::GetTimeInMilliseconds())
		{
			return false;
		}
	}

	if (rControl.GetPedEnter().IsPressed())
		return true;

	TUNE_GROUP_INT(ENTER_VEHICLE_TUNE, MAX_TIME_BUTTON_PRESSED_TO_ALLOW_ENTER_VEHICLE, 500, 0, 2000, 100);
	if ((fwTimer::GetTimeInMilliseconds() - m_uLastTimeEnterVehiclePressed) < MAX_TIME_BUTTON_PRESSED_TO_ALLOW_ENTER_VEHICLE)
	{
		return true;
	}

	return false;
}

bool CTaskPlayerOnFoot::HasValidEnterCoverInput(const CControl& rControl) const
{
	if (rControl.GetPedCover().IsPressed())
		return true;

	TUNE_GROUP_INT(ENTER_COVER_TUNE, MAX_TIME_BUTTON_PRESSED_TO_ALLOW_ENTER_COVER, 500, 0, 2000, 100);
	if ((fwTimer::GetTimeInMilliseconds() - m_uLastTimeEnterCoverPressed) < MAX_TIME_BUTTON_PRESSED_TO_ALLOW_ENTER_COVER)
	{
		return true;
	}

	return false;
}

//-------------------------------------------------------------------------
// Checks if player can fly jetpack.
//-------------------------------------------------------------------------
bool CTaskPlayerOnFoot::CheckForUsingJetpack(CPed *pPlayerPed)
{
	if(pPlayerPed->GetIsSwimming())
	{
		return false;
	}

	if(pPlayerPed->GetCurrentMotionTask() && pPlayerPed->GetCurrentMotionTask()->IsUnderWater())
	{
		return false;
	}

#if ENABLE_JETPACK
	//! Scan for jetpack task. TO DO - move somewhere more appropriate?
	if(pPlayerPed->GetHasJetpackEquipped() && 
		CTaskJetpack::DoesPedHaveJetpack(pPlayerPed) &&
		!pPlayerPed->IsUsingJetpack() &&
		pPlayerPed->GetControlFromPlayer() && 
		pPlayerPed->GetControlFromPlayer()->GetVehicleFlyYawRight().GetNorm() > 0.0f)
	{
		return true;
	}

#endif

	return false;
}

bool CTaskPlayerOnFoot::CheckForBirdProjectile(CPed* pPlayerPed)
{
	// Check if you are an animal.
	if (pPlayerPed->GetPedType() != PEDTYPE_ANIMAL)
	{
		return false;
	}

	// Check if you are a flying bird.
	CTask* pMotionTask = pPlayerPed->GetPedIntelligence()->GetMotionTaskActiveSimplest();
	if (!pMotionTask || pMotionTask->GetTaskType() != CTaskTypes::TASK_ON_FOOT_BIRD)
	{
		return false;
	}

	// Check if you are in the right bird locomotion state.
	if (pMotionTask->GetState() != CTaskBirdLocomotion::State_Fly && pMotionTask->GetState() != CTaskBirdLocomotion::State_Glide)
	{
		return false;
	}

	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(sm_Tunables.m_BirdWeaponHash);
	Assert(pWeaponInfo);

	if (!pWeaponInfo)
	{
		return false;
	}

	// Check if it's been long enough since crapping.
	if (m_fTimeSinceBirdProjectile < pWeaponInfo->GetTimeBetweenShots())
	{
		return false;
	}

	// Grab the input.
	CControl* pControl = pPlayerPed->GetControlFromPlayer();

	// Check if the input is down.
	if (!pControl || !pControl->GetPedAttack().IsDown())
	{
		return false;
	}

	// Bombs away!
	return true;
}

void CTaskPlayerOnFoot::SpawnBirdProjectile(CPed* pPlayerPed)
{
	// Clear the request flag.
	m_bNeedToSpawnBirdProjectile = false;

	// Note that we have attempted to spawn a projectile flag.
	m_bHasAttemptedToSpawnBirdProjectile = true;

	const CWeaponInfo* pWeaponInfo = pPlayerPed->GetEquippedWeaponInfo();

	if (pWeaponInfo && pWeaponInfo->GetAmmoInfo())
	{
		Matrix34 mSpawn = MAT34V_TO_MATRIX34(pPlayerPed->GetTransform().GetMatrix());
		CProjectile* pProjectile = CProjectileManager::CreateProjectile(pWeaponInfo->GetAmmoInfo()->GetHash(), pWeaponInfo->GetHash(), pPlayerPed, mSpawn, pWeaponInfo->GetDamage(), pWeaponInfo->GetDamageType(), pWeaponInfo->GetEffectGroup());

		if(pProjectile)
		{
			pProjectile->SetIgnoreDamageEntity(pPlayerPed);

			if (sm_Tunables.m_HideBirdProjectile)
			{
				pProjectile->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, false);
			}

			// There's nothing illegal about bird crap.
			pProjectile->SetDisableExplosionCrimes(true);

			Vector3 vDirection(0.0f, 0.0f, -1.0f);	
			pProjectile->Fire(vDirection, -1.0f);
		}
	}
}

bool CTaskPlayerOnFoot::CheckForNearbyCover(CPed *pPlayerPed, u32 iFlags, bool bForceControl )
{
	CControl *pControl = pPlayerPed->GetControlFromPlayer();

	if (pControl->GetPedSpecialAbility().IsDown())
		return false;

	if(pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedIsArresting))
	{
		return false;
	}

	TUNE_GROUP_FLOAT(COVER_TUNE, MIN_TIM_AFTER_EXIT_BEFORE_ALLOW_REENTER, 0.3f, 0.0f, 1.0f, 0.01f);
	if (m_bCheckTimeInStateForCover)
	{
		bool bCheckTimeInState = false;
		if (GetPreviousState() == STATE_USE_COVER)
		{
			bCheckTimeInState = true;
		}
		else if (CTaskCover::CanUseThirdPersonCoverInFirstPerson(*pPlayerPed))
		{
			bCheckTimeInState = true;
		}
		
		if (bCheckTimeInState)
		{
			if (GetTimeInState() < MIN_TIM_AFTER_EXIT_BEFORE_ALLOW_REENTER)
			{
				return false;
			}
		}
		m_bCheckTimeInStateForCover = false;
	}

	Vector3 vDir = camInterface::GetFront();
	Vector3 vPlayerPedPos = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());
	//Vector3 vInFront = ( vDir * 10.0f ) + vPlayerPedPos;
	Vector3 vCoverCoors;

	// Only when on foot
	if (!pPlayerPed->GetCurrentMotionTask()->IsOnFoot() && !pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_WantsToEnterCover))
		return false;

	const bool bEnterPressed = HasValidEnterCoverInput(*pControl) || pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_WantsToEnterCover) || bForceControl;

	bool bPlayerWantsToEnterCover = bEnterPressed || (iFlags&CSF_DontCheckButtonPress);

	// If the cover search isn't being rendered, don't search if the button isn't pressed
#if __DEV
	if( !CCoverDebug::ms_Tunables.m_RenderPlayerCoverSearch )
#endif
	{
		if( !bPlayerWantsToEnterCover )
		{
			return false;
		}
	}

	// Player use of cover can be disabled using
	if( !pPlayerPed->GetPlayerInfo()->CanUseCover() )
	{
		return false;
	}

	PlayerCoverSearchData coverSearchData;
	coverSearchData.m_pPlayerPed = pPlayerPed;
	coverSearchData.m_iSearchFlags = iFlags;
	coverSearchData.m_vPreviousCoverDir = m_vLastCoverDir;

	if( !CTaskCover::CalculateCoverSearchVariables(coverSearchData) )
	{
		return false;
	}

#if DEBUG_DRAW
	s32 iNumPlayerTexts = 0;
	const Vec3V vPlayerPos = pPlayerPed->GetTransform().GetPosition();
	DrawInitialCoverDebug(*pPlayerPed, iNumPlayerTexts, CCoverDebug::ms_Tunables.m_TextOffset, coverSearchData.m_CoverProtectionDir);
#endif // DEBUG_DRAW

	// I already had a coverpoint as I came out of cover but never moved reuse it
	if (pPlayerPed->GetCoverPoint())
	{
#if DEBUG_DRAW
		CCoverDebug::AddDebugText(vPlayerPos, Color_blue, "REUSING OLD COVERPOINT", -1, CCoverDebug::ms_Tunables.m_TextOffset * iNumPlayerTexts++, CCoverDebug::INITIAL_SEARCH_SIMPLE);
#endif // DEBUG_DRAW
		return true;
	}

#if DEBUG_DRAW
	bool bUsingDynamicCover = false;
	s32 iNoTexts = 0;
#endif // DEBUG_DRAW

	// Store whether the using cover point flag is already set
	const bool bUsingCoverPoint = pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UsingCoverPoint);

	// Find the nearest cover point using the dynamic cover test
	if( iFlags&CSF_ConsiderDynamicCover )
	{
		Vector3 vTestPos = vPlayerPedPos;
		// Use the collision capsule because it isn't affected by leg ik
		if( pPlayerPed->GetCurrentPhysicsInst() )
			vTestPos.z = pPlayerPed->GetCurrentPhysicsInst()->GetMatrix().GetM23f();

		// If we fail with the first tests, change the search direction
#if DEBUG_DRAW
		CCoverDebug::ms_eInitialCheckType = CCoverDebug::FIRST_DYNAMIC_CHECK;
#endif // DEBUG_DRAW
		CCoverPoint newCoverPoint;

		// Test in stick direction first
		bool bFoundNewCover = false;
		const float fNewSearchDir = ComputeNewCoverSearchDirection(pPlayerPed);
		if (fNewSearchDir > -999.0f)
		{
			bFoundNewCover = CPlayerInfo::ms_DynamicCoverHelper.FindNewCoverPoint(&newCoverPoint, pPlayerPed, vTestPos, fNewSearchDir);
		}

		// If nothing is found, test in camera direction
		if (!bFoundNewCover)
		{
#if DEBUG_DRAW
			CCoverDebug::ms_eInitialCheckType = CCoverDebug::SECOND_DYNAMIC_CHECK;
#endif // DEBUG_DRAW
			bFoundNewCover = CPlayerInfo::ms_DynamicCoverHelper.FindNewCoverPoint(&newCoverPoint, pPlayerPed, vTestPos);
		}

		//check if player is pulling away from this position if it's close
		const float fDistToCoverSqd = (vTestPos - vPlayerPedPos).XYMag2();
		if (bFoundNewCover && fDistToCoverSqd <= 1.0f)
		{			
			Vector2 vecStick(pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm());
			vecStick *= 128.0f;	
			vecStick.Rotate(camInterface::GetHeading());
			Vector3 vCoverDirection = VEC3V_TO_VECTOR3(newCoverPoint.GetCoverDirectionVector());
			vecStick.Rotate(-rage::Atan2f(-vCoverDirection.x, vCoverDirection.y));	
			const bool bNotValidForStickHeading = vecStick.y < CTaskInCover::ms_Tunables.m_InputYAxisQuitValue;
			if (bNotValidForStickHeading)
			{
#if DEBUG_DRAW
				Vector3 vDebugCoverPos;
				if (newCoverPoint.GetCoverPointPosition(vDebugCoverPos))
				{				
					char szText[128];
					formatf(szText, "DYNAMIC INVALID: StickHeading(%.2f)", vecStick.y);
					CCoverDebug::AddDebugText(RCC_VEC3V(vDebugCoverPos), Color_blue, szText, -1, CCoverDebug::ms_Tunables.m_TextOffset * iNoTexts++, CCoverDebug::INITIAL_SEARCH_SIMPLE);
				}
#endif // DEBUG_DRAW
				bFoundNewCover = false;
			}
		}

		if (bFoundNewCover)
		{
			pPlayerPed->GetPlayerInfo()->GetDynamicCoverPoint()->Copy(newCoverPoint);
			pPlayerPed->SetCoverPoint(pPlayerPed->GetPlayerInfo()->GetDynamicCoverPoint());
			pPlayerPed->GetCoverPoint()->ReserveCoverPointForPed(pPlayerPed);
#if DEBUG_DRAW
			bUsingDynamicCover = true;
			Vector3 vDebugCoverPos;
			if (pPlayerPed->GetCoverPoint()->GetCoverPointPosition(vDebugCoverPos))
			{
				vDebugCoverPos.z += 1.0f;
				CCoverDebug::AddDebugPositionSphereAndDirection(RCC_VEC3V(vDebugCoverPos), pPlayerPed->GetCoverPoint()->GetCoverDirectionVector(), Color_orange, "DYNAMIC COVER FOUND", CCoverDebug::ms_Tunables.m_TextOffset * iNoTexts++, CCoverDebug::INITIAL_SEARCH_SIMPLE);
			}
#endif // DEBUG_DRAW
		}
	}

	bool bDoneRouteTest = false;
	if( iFlags&CSF_ConsiderStaticCover )
	{
		CCoverPoint* pCoverPoint = CCover::FindClosestCoverPointWithCB( pPlayerPed, vPlayerPedPos, CTaskCover::ms_Tunables.GetMaxPlayerToCoverDist(*pPlayerPed), NULL, CCover::CS_ANY, StaticCoverpointScoringCB, StaticCoverpointValidityCallback, (void*) &coverSearchData );
		if( pCoverPoint )
		{
#if DEBUG_DRAW
			Vector3 vDebugCoverPos;
			if (pCoverPoint->GetCoverPointPosition(vDebugCoverPos))
			{
				vDebugCoverPos.z += 1.0f;
				CCoverDebug::AddDebugPositionSphereAndDirection(RCC_VEC3V(vDebugCoverPos), pCoverPoint->GetCoverDirectionVector(), Color_red, "BEST STATIC COVER FOUND", 0, CCoverDebug::INITIAL_SEARCH_SIMPLE);
			}
#endif // DEBUG_DRAW

			CCoverPoint* pDynamicCoverPoint = pPlayerPed->GetCoverPoint();
			// Default to using the dynamic point if it exists
			bool bStaticCoverPointHasPrecedenceOverDynamicPoint = false;
			bool bHadPreviousPoint = false;
			CCoverPoint backupCoverPoint;
			
			// If we couldn't find a dynamic cover point we must use the static one
			if (!pDynamicCoverPoint)
			{
				Vector3 vStaticCoverDir = VEC3V_TO_VECTOR3(pCoverPoint->GetCoverDirectionVector());
				coverSearchData.m_CoverProtectionDir = vStaticCoverDir;
				bStaticCoverPointHasPrecedenceOverDynamicPoint = true;
				bHadPreviousPoint = true;
				backupCoverPoint = *pCoverPoint;
			}
			// If we found a dynamic cover point aswell, we need to score them against each other to determine which to use
			else
			{
				const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());
				camGameplayDirector& rGameplayDirector = camInterface::GetGameplayDirector();
				const Vector3 vCamFront = rGameplayDirector.GetFrame().GetFront();

#if DEBUG_DRAW
				char szText[128];
				formatf(szText, "%p SCORING (Actual/Max)", pDynamicCoverPoint);
				Vector3 vDynamicCoverPos;
				pDynamicCoverPoint->GetCoverPointPosition(vDynamicCoverPos);
				vDynamicCoverPos.z += 1.0f;
				CCoverDebug::AddDebugText(RCC_VEC3V(vDynamicCoverPos), Color_orange, szText, -1, CCoverDebug::ms_Tunables.m_TextOffset * iNoTexts++, CCoverDebug::INITIAL_SEARCH_SIMPLE);
#endif // DEBUG_DRAW

				const bool bStickInputValid = IsPlayerStickInputValid(*pPlayerPed);
				const float fDynamicCoverPointScore = ScoreCoverPoint(vPedPos, vCamFront, *pPlayerPed, *pDynamicCoverPoint, false, coverSearchData.m_CoverProtectionDir, bStickInputValid, coverSearchData.m_HasThreat
#if DEBUG_DRAW
					, &iNoTexts
#endif // DEBUG_DRAW
					);
				const float fStaticCoverPointScore = coverSearchData.m_BestScore;
#if DEBUG_DRAW
#if __ASSERT
				// Call this again so we get the debug rendering on the simple debug and also to verify the results are the same
				const float fDebugDrawScore = ScoreCoverPoint(vPedPos, vCamFront, *pPlayerPed, *pCoverPoint, true, coverSearchData.m_CoverProtectionDir, bStickInputValid, coverSearchData.m_HasThreat);
				taskAssert(fDebugDrawScore == fStaticCoverPointScore);
#endif // __ASSERT
#endif // DEBUG_DRAW

				// Use the cover point with the best (lowest score)
				if (fStaticCoverPointScore < fDynamicCoverPointScore)
				{
					backupCoverPoint = *pDynamicCoverPoint;
					bHadPreviousPoint = true;
					bStaticCoverPointHasPrecedenceOverDynamicPoint = true;
				}
			}

			if (bStaticCoverPointHasPrecedenceOverDynamicPoint)
			{
				pPlayerPed->SetCoverPoint(pCoverPoint);
				// Position
				Vector3 vPos;
				pPlayerPed->GetCoverPoint()->ReserveCoverPointForPed(pPlayerPed);
				CCover::FindCoordinatesCoverPoint(pPlayerPed->GetCoverPoint(), pPlayerPed, coverSearchData.m_CoverProtectionDir, vPos );
				pPlayerPed->GetCoverPoint()->ReleaseCoverPointForPed(pPlayerPed);
				Vector3 vTestPos = vPos;

				const float fSecondSurfaceInterp=0.0f;

				if( CPedPlacement::FindZCoorForPed(fSecondSurfaceInterp, &vTestPos, NULL) )
				{
					vPos = vTestPos;
				}

				// Heading
				Vector3 vDir = VEC3V_TO_VECTOR3(pPlayerPed->GetCoverPoint()->GetCoverDirectionVector(&RCC_VEC3V(coverSearchData.m_CoverProtectionDir)));
				float fDir = rage::Atan2f(-vDir.x, vDir.y);
				fDir = fwAngle::LimitRadianAngle(fDir);

				// Keep note of the original cover usage so it can be restored (sometimes the dynamic checks are too ruthless when detecting corners).				
				const bool bIsPriority = pPlayerPed->GetCoverPoint()->GetFlag(CCoverPoint::COVFLAGS_IsPriority);
				CCoverPoint newCoverPoint;
#if DEBUG_DRAW
				CCoverDebug::ms_eInitialCheckType = CCoverDebug::FIRST_STATIC_CHECK;
#endif // DEBUG_DRAW
				if( !CPlayerInfo::ms_DynamicCoverHelper.FindNewCoverPoint(&newCoverPoint, pPlayerPed, vPos, fDir) )
				{
#if DEBUG_DRAW
					CCoverDebug::AddDebugText(RCC_VEC3V(vDebugCoverPos), Color_red, "FAILED DYNAMIC CHECKS", -1, CCoverDebug::ms_Tunables.m_TextOffset * -2, CCoverDebug::INITIAL_SEARCH_SIMPLE);
#endif // DEBUG_DRAW
					pPlayerPed->SetCoverPoint(NULL);

					if( bHadPreviousPoint )
					{
						// Need to set the correct cover entity or we may think we're taking cover on a vehicle when we're not!
						CPlayerInfo::ms_DynamicCoverHelper.SetCoverEntryEntity(backupCoverPoint.GetEntity());
						pPlayerPed->GetPlayerInfo()->GetDynamicCoverPoint()->Copy(backupCoverPoint);
						pPlayerPed->SetCoverPoint(pPlayerPed->GetPlayerInfo()->GetDynamicCoverPoint());
						pPlayerPed->GetCoverPoint()->ReserveCoverPointForPed(pPlayerPed);
					}
				}
				else
				{
					// Need to set the correct cover entity or we may think we're taking cover on a vehicle when we're not!
					CPlayerInfo::ms_DynamicCoverHelper.SetCoverEntryEntity(pCoverPoint->GetEntity());

					// We do the obstruction test as part of FindNewCoverPoint
					bDoneRouteTest = true;
					// Make sure the new dynamic cover is near to the static one (dynamic test seem to be returning really bad results on vehicles)
					Vector3 vStaticCoverPos;
					pCoverPoint->GetCoverPointPosition(vStaticCoverPos);
					Vector3 vDynamicCoverPos;
					newCoverPoint.GetCoverPointPosition(vDynamicCoverPos);
					static dev_float STATIC_TO_DYNAMIC_DISTANCE = 1.0f;
					if (vStaticCoverPos.Dist2(vDynamicCoverPos) < rage::square(STATIC_TO_DYNAMIC_DISTANCE))
					{
						pPlayerPed->GetPlayerInfo()->GetDynamicCoverPoint()->Copy(newCoverPoint);
					}
					else
					{
						pPlayerPed->GetPlayerInfo()->GetDynamicCoverPoint()->Copy(*pCoverPoint);
					}
					pPlayerPed->SetCoverPoint(pPlayerPed->GetPlayerInfo()->GetDynamicCoverPoint());
					pPlayerPed->GetCoverPoint()->ReserveCoverPointForPed(pPlayerPed);

					if (bIsPriority)
					{
						pPlayerPed->GetCoverPoint()->SetFlag(CCoverPoint::COVFLAGS_IsPriority);
					}

#if DEBUG_DRAW
					if (pPlayerPed->GetCoverPoint())
					{
						bUsingDynamicCover = false;
					}
#endif // DEBUG_DRAW
				}
			}
		}

	}

	// Ignore far cover
	if( pPlayerPed->GetCoverPoint() )
	{
		// If there is a coverpoint, make sure it isn't too close
		Vector3 vCoverPos;
		pPlayerPed->GetCoverPoint()->GetCoverPointPosition(vCoverPos);
		vCoverPos.z += 1.0f;

		vPlayerPedPos = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());
		if( !(iFlags&CSF_ConsiderFarCover) && vCoverPos.Dist2(vPlayerPedPos) > CTaskCover::ms_Tunables.m_MinDistToCoverAnyDir )
		{
			pPlayerPed->SetCoverPoint(NULL);
		}
		else if( !(iFlags&CSF_ConsiderCloseCover) && vCoverPos.Dist2(vPlayerPedPos) < CTaskCover::ms_Tunables.m_MinDistToCoverAnyDir )
		{
			pPlayerPed->SetCoverPoint(NULL);
		}
	}

	//Ignore cover underwater or too close to an ai ped entering cover there
	if (pPlayerPed->GetCoverPoint())
	{
		// See if there are nearby peds with a certain distance from the cover point
		Vector3 vPos;
		pPlayerPed->GetCoverPoint()->GetCoverPointPosition(vPos);
		Vector3 vPedCoverPos(vPos.x, vPos.y, vPos.z + 1.0f); // Add one to z height as cover position is on the ground
		const CPed* pPedBlockingCover = CTaskEnterCover::GetPedBlockingMyCover(*pPlayerPed, vPedCoverPos);
		if (pPedBlockingCover)
		{
			pPlayerPed->SetCoverPoint(NULL);
		}
		else
		{
			//Is cover point underwater?
			float waterZ = 0;
			if(pPlayerPed->m_Buoyancy.GetWaterLevelIncludingRivers(vPos, &waterZ, true, POOL_DEPTH, REJECTIONABOVEWATER, NULL, pPlayerPed))
			{
				static dev_float s_fAcceptableCoverOffset = 0.5f;
				if (waterZ > (vPos.z + s_fAcceptableCoverOffset))
					pPlayerPed->SetCoverPoint(NULL);
			}			
		}
	}

	if (!bDoneRouteTest && pPlayerPed->GetCoverPoint() && !TestIfRouteToCoverPointIsClear(*pPlayerPed->GetCoverPoint(), *pPlayerPed, coverSearchData.m_CoverProtectionDir))
	{
		pPlayerPed->SetCoverPoint(NULL);
	}

	if( pPlayerPed->GetCoverPoint() )
	{
		pPlayerPed->GetCoverPoint()->ReserveCoverPointForPed(pPlayerPed);

		// Don't check for cover if the player hasn't requested cover (must be testing cover rendering)
		if( !bPlayerWantsToEnterCover )
		{
			pPlayerPed->ReleaseCoverPoint();

			// Making sure flag is cleared if set during the search (ReserveCoverPointForPed would have set the flag)
			if (!bUsingCoverPoint && pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UsingCoverPoint))
			{
				pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_UsingCoverPoint, false);
			}
			return false;
		}
#if DEBUG_DRAW
		CCoverDebug::AddDebugText(vPlayerPos, bUsingDynamicCover ? Color_orange : Color_red, bUsingDynamicCover ? "USING DYNAMIC COVERPOINT" : "USING STATIC COVERPOINT", -1, CCoverDebug::ms_Tunables.m_TextOffset * iNumPlayerTexts++, CCoverDebug::INITIAL_SEARCH_SIMPLE);
#endif // DEBUG_DRAW

		// If the player has pressed the enter button, go into the cover
		return true;
	}
	else
	{
		// Making sure flag is cleared if set during the search (ReserveCoverPointForPed would have set the flag)
		if (!bUsingCoverPoint && pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UsingCoverPoint))
		{
			pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_UsingCoverPoint, false);
		}
	}
	return false;
}

CTaskPlayerOnFoot::ePlayerOnFootState CTaskPlayerOnFoot::CheckForAutoCover(CPed* pPed)
{
	Assert(pPed->IsAPlayerPed());
	// Only when on foot
	if (!pPed->GetCurrentMotionTask()->IsOnFoot())
		return STATE_INVALID;

	// Never when following an 'assisted-movement' route
	CTaskMove * pMoveTask = pPed->GetPedIntelligence()->GetActiveSimplestMovementTask();
	if(pMoveTask && pMoveTask->GetTaskType()==CTaskTypes::TASK_MOVE_PLAYER)
	{
		if(((CTaskMovePlayer*)pMoveTask)->GetAssistedMovementControl()->GetIsUsingRoute())
			return STATE_INVALID;
	}

	// Never when heavy weapon is equipped
	const CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
	const CWeaponInfo* pWeaponInfo = pWeaponManager ? pWeaponManager->GetEquippedWeaponInfo() : false;
	if( pWeaponInfo && pWeaponInfo->GetIsHeavy() )
	{
		return STATE_INVALID;
	}

	// Check for cover first:
	if(!m_bPlayerExitedCoverThisUpdate && CheckForNearbyCover(pPed, CSF_ConsiderCloseCover|CSF_ConsiderDynamicCover/*|CSF_DontCheckButtonPress*/))
	{
		return STATE_USE_COVER;
	}

	return STATE_INVALID;
}

bool CTaskPlayerOnFoot::DoJumpCheck(CPed* pPlayerPed, CControl *UNUSED_PARAM(pControl), bool bHeavyWeapon, bool bForceVaultCheck)
{
#if __BANK

	TUNE_GROUP_BOOL(PED_JUMPING, bConstantTerrainAnalysis ,false);					
	if(bConstantTerrainAnalysis)
	{
		CTaskJump::DoJumpGapTest(pPlayerPed, g_JumpHelper);
	}

	TUNE_GROUP_BOOL(PED_JUMPING, bTestSuperJump, false);
	if(bTestSuperJump)
	{
		pPlayerPed->GetPlayerInfo()->GetPlayerResetFlags().SetFlag(CPlayerResetFlags::PRF_SUPER_JUMP_ON);
	}
	TUNE_GROUP_BOOL(PED_JUMPING, bTestBeastJump, false);
	if(bTestBeastJump)
	{
		pPlayerPed->GetPlayerInfo()->GetPlayerResetFlags().SetFlag(CPlayerResetFlags::PRF_BEAST_JUMP_ON);
	}


#endif	//__BANK 

	m_bJumpedFromCover = (GetPreviousState() == STATE_USE_COVER);

	TUNE_GROUP_FLOAT(VAULTING, fStandAutoStepUpVelThresholdSq, 1.0f, 0.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(VAULTING, fStandAutoStepUpTime, 0.1f, 0.0f, 10.0f, 0.01f);
	float fVelMag2 = pPlayerPed->GetVelocity().XYMag2();
	if(fVelMag2 < rage::square(fStandAutoStepUpVelThresholdSq))
	{
		m_fStandAutoStepUpTimer+=fwTimer::GetTimeStep();
	}
	else
	{
		m_fStandAutoStepUpTimer = 0.0f;
	}

	bool bCanStandAutoVault = m_fStandAutoStepUpTimer > fStandAutoStepUpTime;

	m_VaultTestResults.Reset();
	if(CanJumpOrVault(pPlayerPed, m_VaultTestResults, bHeavyWeapon, GetState(), bForceVaultCheck, bCanStandAutoVault))
	{
		m_iJumpFlags = MakeJumpFlags(pPlayerPed, m_VaultTestResults);

		// Are we going to jump?

		bool bVault = CTaskJumpVault::WillVault(pPlayerPed, m_iJumpFlags);
		if(bVault)
		{
			return true;
		}
		else
		{
			if( !m_iJumpFlags.IsFlagSet(JF_DisableJumpingIfNoClimb) && !bForceVaultCheck )
			{
				CTaskMotionBase *pMotionTask = pPlayerPed->GetCurrentMotionTask();
				float fCurrentMbrYCheck = 0.f;
#if FPS_MODE_SUPPORTED
				const bool bInFpsMode = pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false);
				if(bInFpsMode && pPlayerPed->IsInMotion())
				{
					TUNE_GROUP_BOOL(BUG_2048930, NO_JUMP_WHEN_ROLLING, true);
					const CTaskCombatRoll* pCombatRollTask = static_cast<const CTaskCombatRoll*>(pPlayerPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_COMBAT_ROLL));
					if(NO_JUMP_WHEN_ROLLING)
					{
						if (pCombatRollTask)
							return false;

						const CTaskMotionAiming* pMotionAimingTask = static_cast<const CTaskMotionAiming*>(pPlayerPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_AIMING));
						if (pMotionAimingTask && pMotionAimingTask->GetPreviousState() == CTaskMotionAiming::State_Roll)
						{
							return false;
						}
					}

					TUNE_GROUP_FLOAT(PED_JUMPING, FPS_MbrYCheck, -1.5f, -MOVEBLENDRATIO_SPRINT, MOVEBLENDRATIO_SPRINT, 0.01f);					
					fCurrentMbrYCheck = FPS_MbrYCheck;
				}
#endif // FPS_MODE_SUPPORTED
				const bool bReducedGaitMoving = pMotionTask && pMotionTask->IsGaitReduced() && (pPlayerPed->GetMotionData()->GetDesiredMbrY() > 0.0f);
				const bool bSpeedCheck = bInFpsMode ? (pPlayerPed->GetMotionData()->GetDesiredMbrY() > fCurrentMbrYCheck) : (pPlayerPed->GetMotionData()->GetCurrentMbrY() > fCurrentMbrYCheck);
				const bool bForcedJump = pPlayerPed->GetPlayerInfo()->GetPlayerResetFlags().IsFlagSet(CPlayerResetFlags::PRF_FORCED_JUMP);
				if(bForcedJump || bSpeedCheck || bReducedGaitMoving)
				{
					return true;
				}
			}
		}
	}

	pPlayerPed->SetPedConfigFlag( CPED_CONFIG_FLAG_VaultFromCover, false );
	return false;
}

void CTaskPlayerOnFoot::GetClimbDetectorParam(s32 state,
											  CPed* pPlayerPed, 
											  sPedTestParams &pedTestParams, 
											  bool bDoManualClimbCheck, 
											  bool bAutoVaultOnly, 
											  bool bAttemptingToVaultFromSwimming, 
											  bool bHeavyWeapon, 
											  bool bCanDoStandAutoStepUp)
{

	Assertf(pPlayerPed,"Expect valid ped here");

	TUNE_GROUP_FLOAT(VAULTING, fVaultParallelDot, 0.3f, -1.0f, 1.0f, 0.1f);
	TUNE_GROUP_FLOAT(VAULTING, fVaultForwardDot, 0.3f, -1.0f, 1.0f, 0.1f);
	TUNE_GROUP_FLOAT(VAULTING, fVaultStickDot, 0.3f, -1.0f, 1.0f, 0.1f);

	TUNE_GROUP_FLOAT(VAULTING, fAimingVaultParallelDot, 0.3f, -1.0f, 1.0f, 0.1f);
	TUNE_GROUP_FLOAT(VAULTING, fAimingVaultForwardDot, 0.3f, -1.0f, 1.0f, 0.1f);
	TUNE_GROUP_FLOAT(VAULTING, fAimingVaultStickDot, 0.8f, -1.0f, 1.0f, 0.1f);

	TUNE_GROUP_FLOAT(VAULTING, fAutoVaultParallelDot, 0.8f, -1.0f, 1.0f, 0.1f);
	TUNE_GROUP_FLOAT(VAULTING, fAutoVaultForwardDot, 0.8f, -1.0f, 1.0f, 0.1f);
	TUNE_GROUP_FLOAT(VAULTING, fAutoVaultStickDot, 0.8f, -1.0f, 1.0f, 0.1f);

	// DMKH. Different angles for auto-vault vs manual vault.
	float fParallelDot;
	float fForwardDot;
	float fStickDot;
	if(bDoManualClimbCheck)
	{
		if(HasPlayerSelectedGunTask(pPlayerPed, state))
		{
			fParallelDot = fAimingVaultParallelDot;
			fForwardDot = fAimingVaultForwardDot;
			fStickDot = fAimingVaultStickDot;
			
			//! Always take stick into account - don't do standing vault if aiming.
			pedTestParams.bDoStandingVault = false;
		}
		else
		{
			fParallelDot = fVaultParallelDot;
			fForwardDot = fVaultForwardDot;
			fStickDot = fVaultStickDot;
		}
	}
	else
	{	
		fParallelDot = fAutoVaultParallelDot;
		fForwardDot = fAutoVaultForwardDot;
		fStickDot = fAutoVaultStickDot;
	}

	TUNE_GROUP_FLOAT(VAULTING, fJumpVaultHeightThresholdMin, 0.0f, 0.0f, 2.0f, 0.1f);
	TUNE_GROUP_FLOAT(VAULTING, fJumpVaultHeightThresholdMax, 0.8f, 0.0f, 2.0f, 0.1f);
	TUNE_GROUP_FLOAT(VAULTING, fJumpVaultHeightAllowanceOnSlope, 0.85f, 0.0f, 2.0f, 0.1f);
	TUNE_GROUP_FLOAT(VAULTING, fJumpVaultDepthThresholdMin, 0.0f, 0.0f, 5.0f, 0.1f);
	TUNE_GROUP_FLOAT(VAULTING, fJumpVaultDepthThresholdMax, 1000.0f, 0.0f, 1000.0f, 0.1f);
	TUNE_GROUP_FLOAT(VAULTING, fJumpVaultMinHorizontalClearance, 0.75f, 0.0f, 100.0f, 0.1f);
	TUNE_GROUP_FLOAT(VAULTING, fJumpVaultMBRThreshold, 1.5f, 0.0f, 3.0f, 0.1f);
	TUNE_GROUP_FLOAT(VAULTING, fJumpVaultDistanceMin, 1.8f, 0.0f, 5.0f, 0.1f);
	TUNE_GROUP_FLOAT(VAULTING, fJumpVaultDistanceMax, 2.0f, 0.0f, 5.0f, 0.1f);
	TUNE_GROUP_FLOAT(VAULTING, fJumpVaultDistanceCutoff, 0.45f, 0.0f, 5.0f, 0.1f)
	TUNE_GROUP_FLOAT(VAULTING, fStepUpDistanceCutoffClose, 0.2f, 0.0f, 5.0f, 0.1f);
	TUNE_GROUP_FLOAT(VAULTING, fStepUpDistanceCutoff, 0.225f, 0.0f, 5.0f, 0.1f);
	TUNE_GROUP_FLOAT(VAULTING, fStealthStepUpDistanceCutoffClose, 0.2f, 0.0f, 5.0f, 0.1f);
	TUNE_GROUP_FLOAT(VAULTING, fStealthStepUpDistanceCutoff, 0.375f, 0.0f, 5.0f, 0.1f);
	TUNE_GROUP_FLOAT(VAULTING, fAutoStepUpHeight, 0.375f, 0.0f, 5.0f, 0.05f);
	TUNE_GROUP_FLOAT(VAULTING, fAutoStepUpPedHeight, 0.25f, 0.0f, 5.0f, 0.05f);
	TUNE_GROUP_FLOAT(VAULTING, fJumpVaultSubmergeLevelThreshold, 0.2f, 0.0f, 5.0f, 0.1f);

	TUNE_GROUP_BOOL(VAULTING, bDoJumpVault, true);

	TUNE_GROUP_FLOAT(VAULTING, fAutoVaultDistMBRMin, 0.75f, 0.0f, 5.0f, 0.1f);
	TUNE_GROUP_FLOAT(VAULTING, fAutoVaultDistMBRMax, 1.5f, 0.0f, 5.0f, 0.1f);

	TUNE_GROUP_FLOAT(VAULTING, fVaultDistanceManual, 1.5f, 0.0f, 5.0f, 0.1f);
	TUNE_GROUP_FLOAT(VAULTING, fVaultDistanceStanding, 1.5f, 0.0f, 5.0f, 0.1f);
	TUNE_GROUP_FLOAT(VAULTING, fVaultDistanceStandingVehicle, 1.5f, 0.0f, 5.0f, 0.1f);
	TUNE_GROUP_FLOAT(VAULTING, fFirstPersonVaultDistance, 2.5f, 0.0f, 5.0f, 0.1f);
	TUNE_GROUP_FLOAT(VAULTING, fVaultDistanceInWater, 1.5f, 0.0f, 5.0f, 0.1f);
	TUNE_GROUP_FLOAT(VAULTING, fVaultDistanceStepUp, 1.0f, 0.0f, 5.0f, 0.1f);

	TUNE_GROUP_FLOAT(VAULTING, fHorizontalTestDistance, 2.5f, 0.0f, 10.0f, 0.1f);
	TUNE_GROUP_FLOAT(VAULTING, fLargeHorizontalTestDistance, 5.0f, 0.0f, 10.0f, 0.1f);
	TUNE_GROUP_FLOAT(VAULTING, fFirstPersonHorizontalTestDistance, 5.0f, 0.0f, 10.0f, 0.1f);

	TUNE_GROUP_INT(VAULTING, uStepUpReduceGaitTime, 100, 0, 1000, 100);

	TUNE_GROUP_FLOAT(VAULTING, fZClearanceAdjustment, 0.05f, 0.0f, 10.0f, 0.1f);

	float fCurrentMBR = pPlayerPed->GetMotionData()->GetCurrentMbrY();

	float fVaultDistance;
	float fSelectedVaultDistanceStanding = fVaultDistanceStanding;
	float fSelectedVaultDistanceStandingVehicle = fVaultDistanceStandingVehicle;
	if(bDoManualClimbCheck)
	{
		fVaultDistance = bAttemptingToVaultFromSwimming ? fVaultDistanceInWater : fVaultDistanceManual;

#if FPS_MODE_SUPPORTED
		if( pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false) )
		{
			fVaultDistance = fFirstPersonVaultDistance;
			fSelectedVaultDistanceStanding = fFirstPersonVaultDistance;
			fSelectedVaultDistanceStandingVehicle = fFirstPersonVaultDistance;
		}
#endif
	}
	else
	{
		if(state == STATE_JUMP)
		{
			const CTaskVault* pVaultTask = static_cast<const CTaskVault*>(pPlayerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_VAULT));
			if(pVaultTask && pVaultTask->GetVaultSpeed() == CTaskVault::VaultSpeed_Run)
			{
				fCurrentMBR = MOVEBLENDRATIO_RUN;
			}
		}
		if(state == STATE_DROPDOWN)
		{
			const CTaskDropDown* pDropDownTask = static_cast<const CTaskDropDown*>(pPlayerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_DROP_DOWN));
			if(pDropDownTask && pDropDownTask->GetDropDownSpeed() == CTaskDropDown::DropDownSpeed_Run)
			{
				fCurrentMBR = MOVEBLENDRATIO_RUN;
			}
		}

		float fCurrentSpeed = fCurrentMBR/MOVEBLENDRATIO_SPRINT;
		fVaultDistance = fAutoVaultDistMBRMin + ((fAutoVaultDistMBRMax-fAutoVaultDistMBRMin)*fCurrentSpeed);
	}

	bool bSubmerged = pPlayerPed->m_Buoyancy.GetBuoyancyInfoUpdatedThisFrame() && 
		(pPlayerPed->GetStatus() != NOT_IN_WATER) &&
		(pPlayerPed->m_Buoyancy.GetSubmergedLevel() > fJumpVaultSubmergeLevelThreshold);

	bool bInWater = pPlayerPed->GetIsSwimming() || pPlayerPed->GetWasSwimming() || bSubmerged;

	bool bReducingGaitForStepUp = m_uReducingGaitTimeMS != 0 && 
		(fwTimer::GetTimeInMilliseconds() > (m_uReducingGaitTimeMS + uStepUpReduceGaitTime));

	//! Capsule using stealth can ride up easier than default. In this case, don't step up unless if we are close to handhold, as it can look pretty horrible.
	float fStepUpCutoff;
	if(pPlayerPed->GetMotionData()->GetUsingStealth())
	{
		fStepUpCutoff = bReducingGaitForStepUp ? fStealthStepUpDistanceCutoffClose : fStealthStepUpDistanceCutoff;
	}
	else
	{
		fStepUpCutoff = bReducingGaitForStepUp ? fStepUpDistanceCutoffClose : fStepUpDistanceCutoff;
	}

	pedTestParams.fParallelDot = fParallelDot;
	pedTestParams.fForwardDot = fForwardDot;
	pedTestParams.fStickDot = fStickDot;
	pedTestParams.fVaultDistStanding = bAutoVaultOnly ? -1.0f : fSelectedVaultDistanceStanding;
	pedTestParams.fVaultDistStandingVehicle = bAutoVaultOnly ? -1.0f : fSelectedVaultDistanceStandingVehicle;
	pedTestParams.fVaultDistStepUp = fVaultDistanceStepUp;
	pedTestParams.fJumpVaultHeightThresholdMin = fJumpVaultHeightThresholdMin;
	pedTestParams.fJumpVaultHeightThresholdMax = fJumpVaultHeightThresholdMax;
	pedTestParams.fJumpVaultHeightAllowanceOnSlope = fJumpVaultHeightAllowanceOnSlope;
	pedTestParams.fJumpVaultDepthThresholdMin = fJumpVaultDepthThresholdMin;
	pedTestParams.fJumpVaultDepthThresholdMax = fJumpVaultDepthThresholdMax;
	pedTestParams.fJumpVaultMinHorizontalClearance = fJumpVaultMinHorizontalClearance;
	pedTestParams.bDepthThresholdTestInsideRange = true;
	pedTestParams.fJumpVaultMBRThreshold = fJumpVaultMBRThreshold;
	pedTestParams.fAutoJumpVaultDistMin = fJumpVaultDistanceMin;
	pedTestParams.fAutoJumpVaultDistMax = fJumpVaultDistanceMax;
	pedTestParams.fAutoJumpVaultDistCutoff = fJumpVaultDistanceCutoff;
	pedTestParams.fMaxVaultDistance = fVaultDistance;
	pedTestParams.bDoJumpVault = bDoJumpVault && !bInWater && !bHeavyWeapon && !pPlayerPed->GetMotionData()->GetUsingStealth();
	pedTestParams.bShowStickIntent = true;
	pedTestParams.fUseTestOrientationHeight = CTaskVault::ms_fStepUpHeight;
	pedTestParams.fCurrentMBR = fCurrentMBR;
	pedTestParams.fStepUpDistanceCutoff = fStepUpCutoff;
	pedTestParams.fZClearanceAdjustment = fZClearanceAdjustment;
	pedTestParams.bCanDoStandingAutoStepUp = bCanDoStandAutoStepUp;

	if(pPlayerPed->GetPlayerInfo() && pPlayerPed->GetPlayerInfo()->GetPlayerResetFlags().IsFlagSet(CPlayerResetFlags::PRF_INCREASE_JUMP_SUPPRESSION_RANGE))
	{
		pedTestParams.fHorizontalTestDistanceOverride = fLargeHorizontalTestDistance;
	}
#if FPS_MODE_SUPPORTED
	else if( pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		pedTestParams.fHorizontalTestDistanceOverride = fFirstPersonHorizontalTestDistance;
	}
#endif
	else
	{
		pedTestParams.fHorizontalTestDistanceOverride = fHorizontalTestDistance;
	}

	//! DMKH. Disallow vaulting/jumping large height when arrested.
	if(pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_IsHandCuffed))
	{
		pedTestParams.fOverrideMaxClimbHeight = sm_Tunables.m_MaxEncumberedClimbHeight;
		pedTestParams.bDo2SidedHeightTest = false;
	}
	else if(pPlayerPed->GetGroundPhysical() && pPlayerPed->GetGroundPhysical()->GetIsTypeVehicle())
	{
		if(((CVehicle*)pPlayerPed->GetGroundPhysical())->GetVehicleType()==VEHICLE_TYPE_TRAIN)
		{
			pedTestParams.fOverrideMaxClimbHeight = sm_Tunables.m_MaxTrainClimbHeight;
			pedTestParams.bDo2SidedHeightTest = false;
		}
	}

	if(bAutoVaultOnly && CTaskVault::GetUsingAutoStepUp())
	{
		if( (pedTestParams.fOverrideMaxClimbHeight < 0.0f) || (pedTestParams.fOverrideMaxClimbHeight > CTaskVault::ms_fStepUpHeight) )
		{
			pedTestParams.fOverrideMinClimbHeight = fAutoStepUpHeight;
			pedTestParams.fOverrideMinClimbPedHeight = fAutoStepUpPedHeight;
			pedTestParams.fOverrideMaxClimbHeight = CTaskVault::ms_fStepUpHeight;
			pedTestParams.bDo2SidedHeightTest = false;
		}
	}
}

bool CTaskPlayerOnFoot::CanJumpOrVault(CPed* pPlayerPed, 
									   sPedTestResults &pedTestResults, 
									   bool bHeavyWeapon, 
									   s32 nState,
									   bool bForceVaultCheck,
									   bool bCanDoStandAutoStepUp)
{
	bool bForcedJump = pPlayerPed->GetPlayerInfo()->GetPlayerResetFlags().IsFlagSet(CPlayerResetFlags::PRF_FORCED_JUMP);

#if ENABLE_DRUNK
	if (pPlayerPed->IsDrunk())
		return false;
#endif // ENABLE_DRUNK

	//! DMKH. Not when skiing.
	if(pPlayerPed->GetIsSkiing())
		return false;

	//! And not when crouching and can't stand up.
	if(pPlayerPed->GetIsCrouching() && !pPlayerPed->CanPedStandUp())
		return false;

	CControl* pControl = pPlayerPed->GetControlFromPlayer();
	if(!pControl)
	{
		return false;
	}

	//! DMKH. Disallow vaulting from underwater. If necessary, we can modify this check later.
	CTaskMotionBase *pMotionTask = pPlayerPed->GetCurrentMotionTask();
	if(pMotionTask && pMotionTask->IsUnderWater())
	{
		return false;
	}

	if (CPedType::IsAnimalType(pPlayerPed->GetPedType()))
	{
		if (CPedModelInfo* pModelInfo = pPlayerPed->GetPedModelInfo())
		{
			if (const CMotionTaskDataSet* pMotionTaskDataSet = CMotionTaskDataManager::GetDataSet(pModelInfo->GetMotionTaskDataSetHash().GetHash()))
			{
				if (pMotionTaskDataSet->GetOnFootData()->m_JumpClipSetHash.IsNull())
				{
					return false;
				}
			}
		}
	}

	//Special case material that just forces the stumble behavior
	if (ThePaths.bStreamHeistIslandNodes && pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_TooSteepForPlayer))
	{
		pPlayerPed->SetPedResetFlag( CPED_RESET_FLAG_DisablePlayerJumping, true );
	}

	bool bAttemptingToVaultFromSwimming = false;
	if(pPlayerPed->GetIsSwimming())
	{
		bAttemptingToVaultFromSwimming = true;
		pPlayerPed->SetPedResetFlag( CPED_RESET_FLAG_DisablePlayerJumping, true );
	}

	if(bHeavyWeapon)
	{
		pPlayerPed->SetPedResetFlag( CPED_RESET_FLAG_DisablePlayerJumping, true );
	}

	bool bJumpPressed = false;
	if(bForcedJump || pControl->GetPedJump().IsPressed())
	{
		bJumpPressed = true;
		m_uLastTimeJumpPressed = fwTimer::GetTimeInMilliseconds();
	}
	else
	{
		static dev_u32 SPRINT_JUMP_BREAKOUT_COOLDOWN = 300;
		if(nState==STATE_JUMP)
		{
			if( fwTimer::GetTimeInMilliseconds() < (m_uLastTimeJumpPressed + SPRINT_JUMP_BREAKOUT_COOLDOWN))
			{
				bJumpPressed = true;
			}
		}
	}

	bool bAutoVaultPressed = false;
	static dev_u32 SPRINT_AUTO_VAULT_COOLDOWN = 200;
	if(pControl->GetPedSprintIsDown())
	{
		m_uLastTimeSprintDown = fwTimer::GetTimeInMilliseconds();
		bAutoVaultPressed = true;
	}
	else if( (fwTimer::GetTimeInMilliseconds() - SPRINT_AUTO_VAULT_COOLDOWN) < m_uLastTimeSprintDown )
	{
		bAutoVaultPressed = true;
	}
	
	TUNE_GROUP_FLOAT(VAULTING, fAutoVaultMinMBR, 0.95f, 0.0f, MOVEBLENDRATIO_SPRINT, 0.1f);
	bool bReducingGait = pMotionTask && pMotionTask->IsGaitReduced() && (pPlayerPed->GetMotionData()->GetDesiredMbrY() > fAutoVaultMinMBR);

	if(bReducingGait)
	{
		if(m_uReducingGaitTimeMS == 0)
		{
			m_uReducingGaitTimeMS = fwTimer::GetTimeInMilliseconds();
		}
	}
	else
	{
		m_uReducingGaitTimeMS = 0;
	}

	if(!bAutoVaultPressed)
	{
		bAutoVaultPressed = bReducingGait || (pPlayerPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter() > 0);
	}

	bool bAutoVaultEnabled = CTaskVault::GetUsingAutoVault() || ((CTaskVault::GetUsingAutoVaultOnPress() && bAutoVaultPressed) && !pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_DisablePlayerAutoVaulting));

	static dev_u32 TIME_ELAPSED = 600;
	u32 nTestTime = HasPlayerSelectedGunTask(pPlayerPed, nState) ? 0 : TIME_ELAPSED;

	bool bDoManualClimbCheck = bForceVaultCheck || ((fwTimer::GetTimeInMilliseconds() - nTestTime) <= m_uLastTimeJumpPressed);

	bool bAutoVaultOnly = bAutoVaultEnabled && !bDoManualClimbCheck && !bForceVaultCheck;

	//! Do ground test.
	if(!pPlayerPed->GetIsSwimming())
	{
		//! If purely an auto-vault test, make sure ped is properly grounded.
		if(bAutoVaultOnly && !pPlayerPed->IsOnGround())
		{
			return false;
		}
		//! If in manual climb, be a bit more relaxed and climb if we have detected any valid ground z.
		else if(bDoManualClimbCheck && pPlayerPed->GetGroundPos().z <= PED_GROUNDPOS_RESET_Z)
		{
			return false;
		}
	}

	bool bClimbDisabled = (pPlayerPed->GetPedType() == PEDTYPE_ANIMAL) || pPlayerPed->GetPedResetFlag( CPED_RESET_FLAG_DisablePlayerVaulting);

	if(!bClimbDisabled && (bAutoVaultEnabled || bDoManualClimbCheck) )
	{
		if(bAutoVaultOnly)
		{
			Vector3 vStickInput(pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm(), 0.0f);
			if( (vStickInput.Mag2() > 0.0f) && ((pPlayerPed->GetMotionData()->GetCurrentMbrY() > fAutoVaultMinMBR) || bReducingGait || (nState == STATE_JUMP)) )
			{
				pPlayerPed->SetPedResetFlag(CPED_RESET_FLAG_SearchingForAutoVaultClimb, true);
				pPlayerPed->SetPedResetFlag(CPED_RESET_FLAG_SearchingForClimb, true);
			}
		}
		else
		{
			pPlayerPed->SetPedResetFlag(CPED_RESET_FLAG_SearchingForClimb, true);
		}

		sPedTestParams pedTestParams;
		GetClimbDetectorParam(	nState,
								pPlayerPed, 
								pedTestParams, 
								bDoManualClimbCheck, 
								bAutoVaultOnly, 
								bAttemptingToVaultFromSwimming, 
								bHeavyWeapon,
								bCanDoStandAutoStepUp);

		// If we aren't running climb detection on consecutive frames, reset pending results as they will be stale. Need
		// to re-run. This can happen if script disable climbing/inputs while we have a process in flight. 
		if( (fwTimer::GetFrameCount() - m_nLastFrameProcessedClimbDetector) > 1)
		{
			pPlayerPed->GetPedIntelligence()->GetClimbDetector().ResetPendingResults();
		}

		pPlayerPed->GetPedIntelligence()->GetClimbDetector().Process(NULL, &pedTestParams, &pedTestResults, nState != STATE_JUMP);
		m_nLastFrameProcessedClimbDetector = fwTimer::GetFrameCount();

		CClimbHandHoldDetected handHold;
		bool bDetected = pPlayerPed->GetPedIntelligence()->GetClimbDetector().GetDetectedHandHold(handHold);
	
		// Disable jumping if going to vault soon. Or if we have detected something that 
		// should block the jump (i.e. a wall). ... unless we have super jump enabled!
		bool bSuperJumpEnabled = pPlayerPed->GetPlayerInfo() ? pPlayerPed->GetPlayerInfo()->GetPlayerResetFlags().IsFlagSet(CPlayerResetFlags::PRF_SUPER_JUMP_ON) : false;
		if( (bDetected && pedTestResults.GetGoingToVaultSoon()) || (pPlayerPed->GetPedIntelligence()->GetClimbDetector().IsJumpBlocked() && !bSuperJumpEnabled) )
		{
			pPlayerPed->SetPedResetFlag( CPED_RESET_FLAG_DisablePlayerJumping, true );
		}
	}
	else
	{
		pPlayerPed->GetPedIntelligence()->GetClimbDetector().ResetAutoVaultDelayTimers();
	}

	if((!pedTestResults.GetGoingToVaultSoon() && (bForcedJump || bJumpPressed || pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_VaultFromCover ) || bForceVaultCheck)) || 
		(pedTestResults.GetCanVault() || pedTestResults.GetCanAutoJumpVault()) )
	{
		if(!pedTestResults.GetCanVault() && !pedTestResults.GetCanAutoJumpVault())
		{
			pPlayerPed->SetPedResetFlag( CPED_RESET_FLAG_DisablePlayerVaulting, true );
		}

		if(!pPlayerPed->GetPedResetFlag( CPED_RESET_FLAG_DisablePlayerJumping) || 
			!pPlayerPed->GetPedResetFlag( CPED_RESET_FLAG_DisablePlayerVaulting))
		{
			return true;
		}
	}

	return false;
}

bool CTaskPlayerOnFoot::CheckForDropDown(CPed* pPlayerPed, CControl *pControl, bool bBiasRightSide)
{
	if(!pControl)
	{
		return false;
	}

#if FPS_MODE_SUPPORTED
	if(pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		return false;
	}
#endif

	//! Don't drop down if we are going to vault soon.
	if(m_VaultTestResults.GetGoingToVaultSoon()  )
	{
		return false;
	}

	if(!pPlayerPed->GetIsStanding())
		return false;

	//! And not when crouching and can't stand up.
	if(pPlayerPed->GetIsCrouching() && !pPlayerPed->CanPedStandUp())
		return false;

	//! DMKH. Disallow from underwater.
	CTaskMotionBase *pMotionTask = pPlayerPed->GetCurrentMotionTask();
	if(pPlayerPed->GetIsSwimming() || (pMotionTask && pMotionTask->IsUnderWater()) || pPlayerPed->m_Buoyancy.GetStatus() != NOT_IN_WATER)
	{
		return false;
	}

	//! Allow dropdowns to be disabled by external systems (i.e. script).
	if(pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_DisableDropDowns))
	{
		return false;
	}

	//! Don't drop down if we have a jetpack on our back.
	if(pPlayerPed->GetHasJetpackEquipped())
	{
		return false;
	}

	bool bDropDownEnabled = CTaskDropDown::GetUsingDropDowns();
	if(bDropDownEnabled)
	{
		TUNE_GROUP_FLOAT(DROPDOWN, fDropDownMinMBR, 0.1f, 0.0f, MOVEBLENDRATIO_SPRINT, 0.1f);

		bool bVaulting = false;

		float fSpeed = GetPed()->GetMotionData()->GetCurrentMbrY();
		if(GetState() == STATE_JUMP)
		{
			const CTaskVault* pVaultTask = static_cast<const CTaskVault*>(pPlayerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_VAULT));
			if(pVaultTask)
			{
				bVaulting = true;

				if(pVaultTask->GetVaultSpeed() == CTaskVault::VaultSpeed_Run)
				{
					fSpeed = MOVEBLENDRATIO_RUN;
				}
				else if(pVaultTask->GetVaultSpeed() == CTaskVault::VaultSpeed_Walk)
				{
					fSpeed = MOVEBLENDRATIO_WALK;
				}
			}
		}
		else if(GetState()==STATE_DROPDOWN)
		{
			const CTaskDropDown* pDropDownTask = static_cast<const CTaskDropDown*>(pPlayerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_DROP_DOWN));
			if(pDropDownTask)
			{
				bVaulting = true;

				if(pDropDownTask->GetDropDownSpeed() == CTaskDropDown::DropDownSpeed_Run)
				{
					fSpeed = MOVEBLENDRATIO_RUN;
				}
				else if(pDropDownTask->GetDropDownSpeed() == CTaskDropDown::DropDownSpeed_Walk)
				{
					fSpeed = MOVEBLENDRATIO_WALK;
				}
			}
		}

		// No need to test if not moving.
		if (fSpeed<=MOVEBLENDRATIO_STILL)
			return false;

		pPlayerPed->SetPedResetFlag(CPED_RESET_FLAG_SearchingForDropDown, true);

		TUNE_GROUP_BOOL(DROPDOWN, bDropDownAsync, true);

		u8 nFlags = bDropDownAsync ? CDropDownDetector::DSF_AsyncTests : 0;
		
		//! Need to be pressing run/sprint to auto-dive/parachute.
		CPlayerInfo* pPlayerInfo = pPlayerPed->GetPlayerInfo();
		if(pPlayerInfo->ControlButtonSprint(CPlayerInfo::SPRINT_ON_FOOT, true) >= 1.0f)
		{
			nFlags |= CDropDownDetector::DSF_TestForDive;

			if(CTaskParachute::IsPedInStateToParachute(*pPlayerPed))
			{
				nFlags |= CDropDownDetector::DSF_TestForPararachuteJump;
			}	
		}

		if(sm_Tunables.m_CanMountFromInAir)
		{
			nFlags |= CDropDownDetector::DSF_TestForMount;
		}
		
		if(fSpeed >= MOVEBLENDRATIO_RUN)
		{
			nFlags |= CDropDownDetector::DSF_PedRunning;
		}
		if(bVaulting)
		{
			nFlags |= CDropDownDetector::DSF_FromVault;
		}
		if(bBiasRightSide)
		{
			nFlags |= CDropDownDetector::DSF_BiasRightSide;
		}

		if(!pPlayerInfo->GetPlayerResetFlags().IsFlagSet(CPlayerResetFlags::PRF_BEAST_JUMP_ON))
		{
			nFlags |= CDropDownDetector::DSF_TestForRagdollTeeter;
		}

		pPlayerPed->GetPedIntelligence()->GetDropDownDetector().Process(NULL, NULL, nFlags);

		if(pPlayerPed->GetPedIntelligence()->GetDropDownDetector().HasDetectedDropDown())
		{
			bool bRagdollReaction = pPlayerPed->GetPedIntelligence()->GetDropDownDetector().GetDetectedDropDown().m_eDropType==eRagdollDrop;

			if (!bRagdollReaction)
			{
				if(fSpeed <= fDropDownMinMBR)
					return false;

				if (pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_StairsDetected))
					return false;

#if ENABLE_DRUNK
				if (pPlayerPed->IsDrunk())
					return false;
#endif // ENABLE_DRUNK

				if (pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed))
				{
					return false;
				}
			}

			m_iJumpFlags = 0;
			m_iJumpFlags.SetFlag(JF_DisableVault);

			if(pPlayerPed->GetPedIntelligence()->GetDropDownDetector().HasDetectedDive())
			{
				m_iJumpFlags.SetFlag(JF_ForceDiveJump);
				m_iJumpFlags.SetFlag(JF_AutoJump);
			}
			else if(pPlayerPed->GetPedIntelligence()->GetDropDownDetector().HasDetectedParachuteDrop())
			{
				m_iJumpFlags.SetFlag(JF_ForceParachuteJump);
				m_iJumpFlags.SetFlag(JF_AutoJump);
			}
			
			return true;
		}
	}

	return false;
}

// Keeps a history of the last recored stick input
void CTaskPlayerOnFoot::UpdateMovementStickHistory( CControl * pControl )
{
	if( m_fTimeSinceLastStickInput < MAX_STICK_INPUT_RECORD_TIME)
		m_fTimeSinceLastStickInput += fwTimer::GetTimeStep();

	CPed& rPed = *GetPed();
	bool bShouldTryToKeepCoverPoint = true;
	
	Vector2 vecStickInput;
	vecStickInput.x = pControl->GetPedWalkLeftRight().GetNorm();
	vecStickInput.y = -pControl->GetPedWalkUpDown().GetNorm();
	float fCamOrient = camInterface::GetHeading(); // careful.. this looks dodgy to me - and could be ANY camera - DW

	// GetNorm() with no parameters automatically applies default deadzoning.
	if(vecStickInput.Mag2())
	{
		float fStickAngle = rage::Atan2f(-vecStickInput.x, vecStickInput.y);
		fStickAngle = fStickAngle + fCamOrient;
		fStickAngle = fwAngle::LimitRadianAngle(fStickAngle);
		m_vLastStickInput = Vector3(-rage::Sinf(fStickAngle), rage::Cosf(fStickAngle), 0.0f);
		m_vLastStickInput.Normalize();
		m_fTimeSinceLastStickInput = 0.0f;
		bShouldTryToKeepCoverPoint = false;
	}

	// Don't keep existing cover point if we moved the camera B*1316985
	if (bShouldTryToKeepCoverPoint)
	{
		Vector2 vecCamInput;
		vecCamInput.x = pControl->GetPedAimWeaponLeftRight().GetNorm();
		vecCamInput.y = -pControl->GetPedAimWeaponLeftRight().GetNorm();
		if (vecCamInput.Mag2())
		{
			bShouldTryToKeepCoverPoint = false;
		}
	}

	// Moved the camera in cover away
	if (bShouldTryToKeepCoverPoint)
	{
		if (rPed.GetCoverPoint())
		{
			const Vector3 vCamDir = camInterface::GetFront();
			const Vector3 vCovDir = VEC3V_TO_VECTOR3(rPed.GetCoverPoint()->GetCoverDirectionVector());
			TUNE_GROUP_FLOAT(COVER_TUNE, MAX_COV_DIR_DOT_CAM_DIR_TO_CONSIDER_OLD_POINT_VALID, 0.85f, -1.0f, 1.0f, 0.01f);
			if (vCovDir.Dot(vCamDir) < MAX_COV_DIR_DOT_CAM_DIR_TO_CONSIDER_OLD_POINT_VALID)
			{
				bShouldTryToKeepCoverPoint = false;
			}
		}
	}

	// See if the vehicle we're taking cover against is moving
	if (rPed.GetCoverPoint() && rPed.GetCoverPoint()->GetEntity() && rPed.GetCoverPoint()->GetEntity()->GetIsTypeVehicle())
	{
		if (!rPed.GetGroundPhysical() || rPed.GetGroundPhysical() != rPed.GetCoverPoint()->GetEntity())
		{
			TUNE_GROUP_FLOAT(COVER_TUNE, MAX_VEHICLE_VELOCITY_TO_CONSIDER_VALID_COVER, 0.1f, 0.0f, 0.5f, 0.01f);
			if (static_cast<CVehicle*>(rPed.GetCoverPoint()->GetEntity())->GetVelocity().Mag2() > MAX_VEHICLE_VELOCITY_TO_CONSIDER_VALID_COVER)
			{
				bShouldTryToKeepCoverPoint = false;
			}
		}
	}

	rPed.SetPedResetFlag(CPED_RESET_FLAG_KeepCoverPoint, bShouldTryToKeepCoverPoint);
}

s32 CTaskPlayerOnFoot::GetBreakOutToMovementState(CPed* pPlayerPed)
{
	CControl *pControl = pPlayerPed->GetControlFromPlayer();

	weaponAssert(pPlayerPed->GetWeaponManager());
	CWeapon* pWeapon = pPlayerPed->GetWeaponManager()->GetEquippedWeapon();
	const bool bHeavyWeapon = pWeapon && pWeapon->GetWeaponInfo()->GetIsHeavy();

	if(pPlayerPed->IsOnGround())
	{
		//! Don't allow jumping breakout (unless during beast mode).
		if(!pPlayerPed->GetPlayerInfo()->GetPlayerResetFlags().IsFlagSet(CPlayerResetFlags::PRF_BEAST_JUMP_ON))
		{
			pPlayerPed->SetPedResetFlag( CPED_RESET_FLAG_DisablePlayerJumping, true );
		}

		//-------------------------------------------------------------------------
		// JETPACK
		//-------------------------------------------------------------------------
		if(CheckForUsingJetpack(pPlayerPed))
		{
			return STATE_JETPACK;
		}

		//-------------------------------------------------------------------------
		// JUMPING INTO COVER
		//-------------------------------------------------------------------------
		if(!bHeavyWeapon && !m_bPlayerExitedCoverThisUpdate && !pPlayerPed->GetIsSwimming() && !pPlayerPed->GetIsSkiing() && 
#if ENABLE_DRUNK
			!pPlayerPed->IsDrunk() && 
#endif // ENABLE_DRUNK
			pPlayerPed->GetWeaponManager() && CTaskEnterCover::AreCoreCoverClipsetsLoaded(pPlayerPed))
		{
			if(CheckForNearbyCover(pPlayerPed, CSF_DefaultFlags, m_nCachedBreakOutToMovementState == STATE_USE_COVER))
			{
				return STATE_USE_COVER;
			}
		}

		//-------------------------------------------------------------------------
		// CLIMBING
		//-------------------------------------------------------------------------
		if( DoJumpCheck(pPlayerPed, pControl, bHeavyWeapon))
		{
			return STATE_JUMP;
		}

		//-------------------------------------------------------------------------
		// DROPDOWNS
		//-------------------------------------------------------------------------
		if(!bHeavyWeapon && CheckForDropDown(pPlayerPed, pControl, true))
		{
			if(pPlayerPed->GetPedIntelligence()->GetDropDownDetector().HasDetectedParachuteDrop() || pPlayerPed->GetPedIntelligence()->GetDropDownDetector().HasDetectedDive())
			{
				return STATE_JUMP;
			}
			else
			{
				return STATE_DROPDOWN;
			}
		}

		//-------------------------------------------------------------------------
		// ENTERING VEHICLES.
		//-------------------------------------------------------------------------
		s8 iEnterVehicleState = CheckShouldEnterVehicle(pPlayerPed,pControl, m_nCachedBreakOutToMovementState == STATE_GET_IN_VEHICLE);
		if(iEnterVehicleState != -1)
		{
			pPlayerPed->SetPedResetFlag(CPED_RESET_FLAG_WantsToEnterVehicleFromAiming, true);
			return (ePlayerOnFootState)iEnterVehicleState;
		}
	}

	return STATE_INVALID;
}

bool CTaskPlayerOnFoot::KeepWaitingForLadder(CPed* pPed, float fBlockHeading)
{
	CControl* pControl = pPed->GetControlFromPlayer();
	if(pControl)
	{
		Vector2 vecStickInput;
		vecStickInput.x = pControl->GetPedWalkLeftRight().GetNorm(CTaskMovePlayer::ms_fDeadZone);
		vecStickInput.y = -pControl->GetPedWalkUpDown().GetNorm(CTaskMovePlayer::ms_fDeadZone);
		
		if(vecStickInput.Mag() > CTaskMovePlayer::ms_fStickInCentreMag)
		{
			float fCamOrient = camInterface::GetPlayerControlCamHeading(*pPed);

			float fStickAngle = rage::Atan2f(-vecStickInput.x, vecStickInput.y);
			fStickAngle = fStickAngle + fCamOrient;
			fStickAngle = fwAngle::LimitRadianAngle(fStickAngle);

			if(fabs(fStickAngle - fBlockHeading) > 1.57f)
			{
				return false;
			}
		}
	}

	return true;
}

void CTaskPlayerOnFoot::CheckForArmedMeleeAction( CPed* pPlayerPed )
{
	// We should have never gotten here but clean up if we did
	if( m_pMeleeRequestTask )
	{
		// This is possible when a takedown was created on the same frame as the player was hit... so I guess let it happen normally
		delete m_pMeleeRequestTask;
		m_pMeleeRequestTask = NULL;
	}

	// Do not allow melee actions while HUD is up
	if( CNewHud::IsShowingHUDMenu() )
		return;

	if( GetState() == STATE_SWAP_WEAPON )
		return;

	if( pPlayerPed->GetPedIntelligence()->FindTaskActiveByType( CTaskTypes::TASK_SWAP_WEAPON ) )
	{
		return;	
	}

	// Don't allow melee actions in air defence spheres.
	if (pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_InAirDefenceSphere))
	{
		return;
	}

	CTaskAimGunOnFoot* pAimGunTask = static_cast<CTaskAimGunOnFoot*>( pPlayerPed->GetPedIntelligence()->FindTaskActiveByType( CTaskTypes::TASK_AIM_GUN_ON_FOOT ) );
	if( pAimGunTask && pAimGunTask->GetWaitingToSwitchWeapon() )
	{
		return;
	}

	Assert( pPlayerPed );
	Assert( pPlayerPed->GetPedIntelligence() );


	CWeapon* pEquippedWeapon =  pPlayerPed->GetWeaponManager() ? pPlayerPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
	const CWeaponInfo* pEquippedWeaponInfo = pEquippedWeapon ? pEquippedWeapon->GetWeaponInfo() : NULL;
	if( pEquippedWeaponInfo && (!pEquippedWeaponInfo->GetIsMelee() || pEquippedWeaponInfo->GetCanBeAimedLikeGunWithoutFiring()) && pEquippedWeaponInfo->GetAllowCloseQuarterKills() )
	{
		bool bLightAttackPressed = false, bAlternateAttackPressed = false;
		CPlayerInfo::GetCachedMeleeInputs( bLightAttackPressed, bAlternateAttackPressed );

		// Find the best applicable target without running the melee target evaluation
		CPlayerPedTargeting& rTargeting = pPlayerPed->GetPlayerInfo()->GetTargeting();
		CEntity* pTargetEntity = NULL;
		CPed* pClosestPed = pPlayerPed->GetPedIntelligence()->GetClosestPedInRange();
		if( pClosestPed && !CActionManager::ArePedsFriendlyWithEachOther( pPlayerPed, pClosestPed ))
			pTargetEntity = pClosestPed;
		else if( rTargeting.GetIsFineAiming() )
			pTargetEntity = rTargeting.GetFreeAimTarget();
		else
			pTargetEntity = rTargeting.GetTarget();
		
		//! if we are trying to shoot our target, don't allow melee move against a different ped.
		if(pPlayerPed->IsLocalPlayer() && CPlayerInfo::IsAiming(false) && bAlternateAttackPressed && pClosestPed)
		{
			CEntity *pLockOnTarget = rTargeting.GetLockOnTarget();
			if(pLockOnTarget && (pLockOnTarget != pClosestPed))
			{
				return;
			}

			//! If we have lined up a shot with a non friendly ped, let us shoot instead.
			CEntity *pFreeAimTarget = rTargeting.GetFreeAimTargetRagdoll() ? rTargeting.GetFreeAimTargetRagdoll() : rTargeting.GetFreeAimTarget();
			if(pFreeAimTarget && (pFreeAimTarget !=pClosestPed))
			{
				return;
			}
		}

		// Only if we have a target should we check for an armed takedown
		if( pTargetEntity && pTargetEntity->GetIsTypePed() &&
			DistSquared( pTargetEntity->GetTransform().GetPosition(), pPlayerPed->GetTransform().GetPosition() ).Getf() < ms_fMinTargetCheckDistSq )
		{
			// don't allow melee attacks on ghosted players
			if (pTargetEntity && NetworkInterface::IsGameInProgress() && NetworkInterface::AreInteractionsDisabledInMP(*pPlayerPed, *pTargetEntity))
			{
				return;
			}

			CPed* pTargetPed = static_cast<CPed*>(pTargetEntity);

			// If the weapon is blocked by the target just test for a takedown
			CTaskWeaponBlocked* pWeaponBlockedTask = static_cast<CTaskWeaponBlocked*>( pPlayerPed->GetPedIntelligence()->FindTaskActiveByType( CTaskTypes::TASK_WEAPON_BLOCKED ) );
			if( pWeaponBlockedTask )
			{
				// Peds marked as lower priority melee targets should be avoided here
				if( !pTargetEntity || 
					!pTargetEntity->GetIsTypePed() ||
					!static_cast<CPed*>(pTargetEntity)->GetPedResetFlag(CPED_RESET_FLAG_IsLowerPriorityMeleeTarget) )
				{
					m_pMeleeRequestTask = CTaskMelee::CheckForAndGetMeleeAmbientMove( pPlayerPed, pTargetEntity, true, false, false, true );
				}
			}
			else
			{
				// Check to see if the player is attempting to fire
				const bool bIsLowerPriorityMeleeTarget = pTargetPed->GetPedResetFlag(CPED_RESET_FLAG_IsLowerPriorityMeleeTarget);
				CTaskAimGunOnFoot* pAimGunTask = static_cast<CTaskAimGunOnFoot*>( pPlayerPed->GetPedIntelligence()->FindTaskActiveByType( CTaskTypes::TASK_AIM_GUN_ON_FOOT ) );
				if( pAimGunTask && pAimGunTask->GetWillFire() && !bIsLowerPriorityMeleeTarget )
				{
					// Reset the will fire bool in the case that we found a melee action and meant to fire
					m_pMeleeRequestTask = CTaskMelee::CheckForAndGetMeleeAmbientMove( pPlayerPed, pTargetEntity, true, false, false, true );
					if( m_pMeleeRequestTask )
					{
						pAimGunTask->ResetWillFire();
					}
				}
				else if( ShouldMeleeTargetPed(pTargetPed, pEquippedWeapon, bIsLowerPriorityMeleeTarget) )
				{
					m_pMeleeRequestTask = CTaskMelee::CheckForAndGetMeleeAmbientMove( pPlayerPed, pTargetEntity, true, false, false, true );
				}
			}
		}

		// If we do not have a melee action nor target, check to see if there is a melee target around
		if( !m_pMeleeRequestTask && (bLightAttackPressed || (bAlternateAttackPressed && !pTargetEntity)) )
		{			
			// CNC: Ignore this min distance threshold check for cops as we want to trigger the long barge melee actions.
			// FindMeleeTarget is already clamped to a max range of 10m.
			bool bIgnoreMinDistanceThreshold = false;
			if (NetworkInterface::IsInCopsAndCrooks() && !pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_IsAiming))
			{
				const CPlayerInfo* pPlayerInfo = pPlayerPed->GetPlayerInfo();
				if (pPlayerInfo && pPlayerInfo->GetArcadeInformation().GetTeam() == eArcadeTeam::AT_CNC_COP)
				{
					bIgnoreMinDistanceThreshold = true;
				}
			}

			pTargetEntity = rTargeting.FindMeleeTarget( pPlayerPed, pEquippedWeaponInfo, false, false );
			if( pTargetEntity && pTargetEntity->GetIsTypePed() && 
				(bIgnoreMinDistanceThreshold || DistSquared( pTargetEntity->GetTransform().GetPosition(), pPlayerPed->GetTransform().GetPosition() ).Getf() < ms_fMinTargetCheckDistSq) )
			{
				CPed* pTargetPed = static_cast<CPed*>(pTargetEntity);
				if( ShouldMeleeTargetPed(pTargetPed, pEquippedWeapon, pTargetPed->GetPedResetFlag(CPED_RESET_FLAG_IsLowerPriorityMeleeTarget)) )
				{
					// Reset the will fire bool in the case that we found a melee action and meant to fire
					m_pMeleeRequestTask = CTaskMelee::CheckForAndGetMeleeAmbientMove( pPlayerPed, pTargetEntity, true, false, false, true );
					if( m_pMeleeRequestTask )
					{
						// Check to see if the player is attempting to fire
						CTaskAimGunOnFoot* pAimGunTask = static_cast<CTaskAimGunOnFoot*>( pPlayerPed->GetPedIntelligence()->FindTaskActiveByType( CTaskTypes::TASK_AIM_GUN_ON_FOOT ) );
						if( pAimGunTask && pAimGunTask->GetWillFire() )
						{
							pAimGunTask->ResetWillFire();
						}
					}
				}
			}
		}

		// Last ditch effort to find a move without a target and light attack was pressed (or alternate attack if weapon can't fire)
		if( !m_pMeleeRequestTask && (bLightAttackPressed || (bAlternateAttackPressed && pEquippedWeaponInfo->GetCanBeAimedLikeGunWithoutFiring())) )
		{
			if( !pPlayerPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_RELOAD_GUN ) ||
				!pPlayerPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_COVER ) )
			{
				if( !pEquippedWeapon->GetCanReload() || pEquippedWeapon->GetAmmoTotal() == 0 )
					m_pMeleeRequestTask = CTaskMelee::CheckForAndGetMeleeAmbientMove( pPlayerPed, NULL, false, false, false, true );
			}
		}
	}
}

bool CTaskPlayerOnFoot::ShouldMeleeTargetPed(const CPed* pTargetPed, const CWeapon* pEquippedWeapon, bool bIsLowerPriorityMeleeTarget)
{
	bool bLightAttackPressed = false, bAlternateAttackPressed = false;
	CPlayerInfo::GetCachedMeleeInputs( bLightAttackPressed, bAlternateAttackPressed );

	// We only want to hit low priority targets if we can't reload and we have hit the light attack button
	if( bLightAttackPressed && (!bIsLowerPriorityMeleeTarget || (!pEquippedWeapon->GetCanReload() && !pTargetPed->GetPedResetFlag(CPED_RESET_FLAG_IsReloading))) )
	{
		return true;
	}
	// Otherwise should we check the based on the light attack input?
	else if( bAlternateAttackPressed && !bIsLowerPriorityMeleeTarget )
	{
		return true;
	}

	return false;
}

CTaskPlayerIdles::CTaskPlayerIdles() 
: m_bHasSpaceToPlayIdleAnims(true)
, m_bIsMoving(false)
{
	SetInternalTaskType(CTaskTypes::TASK_PLAYER_IDLES);
}

CTaskPlayerIdles::~CTaskPlayerIdles()
{

}

CTask::FSM_Return CTaskPlayerIdles::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				Start_OnUpdate();

		FSM_State(State_MovementAndAmbientClips)
			FSM_OnEnter
				MovementAndAmbientClips_OnEnter();
			FSM_OnUpdate
				return MovementAndAmbientClips_OnUpdate();
			FSM_OnExit
				return MovementAndAmbientClips_OnExit();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskPlayerIdles::Start_OnUpdate()
{
	SetState(State_MovementAndAmbientClips);
	return FSM_Continue;
}

void CTaskPlayerIdles::MovementAndAmbientClips_OnEnter()
{
	CTaskMovePlayer* pMovePlayerTask = rage_new CTaskMovePlayer();
	SetNewTask(rage_new CTaskComplexControlMovement(pMovePlayerTask, rage_new CTaskAmbientClips(CTaskAmbientClips::Flag_PlayIdleClips, CONDITIONALANIMSMGR.GetConditionalAnimsGroup("PLAYER_IDLES"))));
}

CTask::FSM_Return CTaskPlayerIdles::MovementAndAmbientClips_OnUpdate()
{
	CPed* pPed = GetPed();

	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetFlag(aiTaskFlags::RestartCurrentState);
		return FSM_Continue;
	}

	if( GetSubTask()->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT &&
		GetSubTask()->GetSubTask() )
	{
		bool bPhoneOnScreen = CTaskPlayerOnFoot::CheckForUseMobilePhone(*pPed);
		// If the player isn't texting but the phone is out, switch to running the texting clip
		if( GetSubTask()->GetSubTask()->GetTaskType() == CTaskTypes::TASK_AMBIENT_CLIPS &&
			bPhoneOnScreen)
		{
			Vector2 vMBR;
			pPed->GetMotionData()->GetCurrentMoveBlendRatio(vMBR);

			if(!pPed->GetIsCrouching() || vMBR.Mag2() < 0.1f)
		{
			CTaskMobilePhone::CreateOrResumeMobilePhoneTask(*GetPed());
		}
		}

		if (pPed->IsInMotion() != m_bIsMoving)
		{
			m_bIsMoving = pPed->IsInMotion();

			if (!m_bIsMoving)
			{
				Vector3 v3CapsuleStart = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
				Vector3 v3CapsuleEnd = v3CapsuleStart;
				const float fCapsuleRadius = 0.56f; // This is the maximum width for Idle motion. [11/29/2012 mdawe]

				const CBaseCapsuleInfo* pPedCapsule = pPed->GetCapsuleInfo();
				if (pPedCapsule)
				{
					const float fHeight = pPedCapsule->GetMaxSolidHeight();
					v3CapsuleStart.z -= fHeight / 2.0f;
					v3CapsuleEnd.z   += fHeight / 2.0f;
				}

				WorldProbe::CShapeTestCapsuleDesc capsuleTest;

				capsuleTest.SetIncludeFlags(ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_INCLUDE_TYPES);
				capsuleTest.SetContext(WorldProbe::LOS_GeneralAI);
				capsuleTest.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
				capsuleTest.SetIsDirected(false);
				capsuleTest.SetExcludeEntity(pPed);
				capsuleTest.SetCapsule(v3CapsuleStart, v3CapsuleEnd, fCapsuleRadius);

				m_bHasSpaceToPlayIdleAnims = !WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);
#if DEBUG_DRAW
				const static bool bDrawCapsule = false;
				if (bDrawCapsule)
				{
					CTask::ms_debugDraw.AddCapsule(RCC_VEC3V(v3CapsuleStart), RCC_VEC3V(v3CapsuleEnd), fCapsuleRadius, Color_DarkOrange, 10000, 0, false);
				}
#endif
			}
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskPlayerIdles::MovementAndAmbientClips_OnExit()
{
	// Stop looking at my vehicle.
	CPed* pPed = GetPed();
	if(pPed)
	{
		static const u32 uLookAtMyVehicleHash = ATSTRINGHASH("LookAtMyVehicle", 0x79037202);
		pPed->GetIkManager().AbortLookAt(500, uLookAtMyVehicleHash);
	}
	return FSM_Continue;
}

#if !__FINAL
const char * CTaskPlayerIdles::GetStaticStateName( s32 iState  )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
		case State_Start:						return "State_Start";
		case State_MovementAndAmbientClips:		return "State_MovementAndAmbientClips";
		case State_Finish:						return "State_Finish";
		default: taskAssert(0); 	
	}

	return "State_Invalid";
}
#endif // !__FINAL

float CTaskMovePlayer::ms_fWalkBlend							= 1.0f;
float CTaskMovePlayer::ms_fRunBlend								= 2.0f;
float CTaskMovePlayer::ms_fSprintBlend							= 3.0f;
float CTaskMovePlayer::ms_fStrafeMinMoveBlendRatio				= 0.15f;
float CTaskMovePlayer::ms_fSmallMoveBlendRatioToDoIdleTurn		= 0.35f;
float CTaskMovePlayer::ms_fTimeStickHeldBeforeDoingIdleTurn		= 0.5f;
bool CTaskMovePlayer::ms_bStickyRunButton						= false;
bank_float CTaskMovePlayer::ms_fStickInCentreMag					= 0.45f;
bool CTaskMovePlayer::ms_bAllowRunningWhilstStrafing			= true;
bool CTaskMovePlayer::ms_bDefaultNoSprintingInInteriors			= true;
bool CTaskMovePlayer::ms_bScriptOverrideNoRunningOnPhone		= false;
bool CTaskMovePlayer::ms_bUseMultiPlayerControlInSinglePlayer	= false;
bank_bool CTaskMovePlayer::ms_bFlipSkiControlsWhenBackwards		= true;
float CTaskMovePlayer::ms_fUnderwaterIdleAutoLevelTime			= 6.0f;
dev_float CTaskMovePlayer::ms_fDeadZone							= 0.33f;
dev_float CTaskMovePlayer::ms_fMinDepthForPlayerFish			= -180.0f;

//
CTaskMovePlayer::CTaskMovePlayer()
:	CTaskMove(MOVEBLENDRATIO_STILL),
	m_bStickHasReturnedToCentre(true),
	m_fUnderwaterGlideTimer(0.0f),	
	m_fSwitchStrokeTimerOnSurface(0.0f),
	m_iStroke(0),
	m_fUnderwaterIdleTimer(0.0f),
	m_pAssistedMovementControl(NULL),
	m_fMaxMoveBlendRatio(MOVEBLENDRATIO_SPRINT),
	m_fLockedStickHeading(-1.0f),
	m_fLockedHeading(0.0f),
	m_fStdPreviousDesiredHeading(FLT_MAX),
	m_fStickAngle(0.0f),
	m_bStickAngleTweaked(false)
#if FPS_MODE_SUPPORTED && KEYBOARD_MOUSE_SUPPORT
	, m_bWasSprintingOnPCMouseAndKeyboard(false)
#endif	// FPS_MODE_SUPPORTED && KEYBOARD_MOUSE_SUPPORT
{
#if __PLAYER_ASSISTED_MOVEMENT
	m_pAssistedMovementControl = rage_new CPlayerAssistedMovementControl();
#endif

	SetInternalTaskType(CTaskTypes::TASK_MOVE_PLAYER);
}

CTaskMovePlayer::~CTaskMovePlayer()
{
#if __PLAYER_ASSISTED_MOVEMENT
	delete m_pAssistedMovementControl;
#endif
}

aiTask* CTaskMovePlayer::Copy() const
{
	//Create the task.
	CTaskMovePlayer* pTask = rage_new CTaskMovePlayer();

	//Copy the max move/blend ratio.
	pTask->SetMaxMoveBlendRatio(m_fMaxMoveBlendRatio);

	return pTask;
}

CTask::FSM_Return CTaskMovePlayer::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(Running)
			FSM_OnUpdate
				return StateRunning_OnUpdate(pPed);
	FSM_End
}

CTask::FSM_Return CTaskMovePlayer::StateRunning_OnUpdate(CPed* pPed)
{
	if(!pPed->IsPlayer())
	{
		Assertf(false, "CTaskMovePlayer given to non-player ped");
		return FSM_Quit;
	}

	if (CPhoneMgr::CamGetState() && pPed->IsLocalPlayer())
	{
		pPed->GetMotionData()->SetCurrentMoveBlendRatio(0.0f, 0.0f);
		pPed->GetMotionData()->SetDesiredMoveBlendRatio(0.0f, 0.0f);
		return FSM_Continue;
	}

	if( pPed->GetPedResetFlag( CPED_RESET_FLAG_PlacingCharge ) )
	{
		return FSM_Continue;
	}

	// Let the movement system know that the player is in direct control of this ped
	pPed->GetMotionData()->SetPlayerHasControlOfPedThisFrame(true);

	if(pPed->IsStrafing() || pPed->GetPedResetFlag(CPED_RESET_FLAG_ForcePedToStrafe))
	//|| pControl->GetPedTarget().IsDown())
	{
		m_fStdPreviousDesiredHeading = FLT_MAX;
		ProcessStrafeMove(pPed);
	}
	else
	{
		CTaskMotionBase* pCurrentMotionTask = pPed->GetCurrentMotionTask();
		if (pCurrentMotionTask->GetTaskType() == CTaskTypes::TASK_ON_FOOT_FISH)
		{
			pPed->GetMotionData()->SetFPSCheckRunInsteadOfSprint(false);
			m_fStdPreviousDesiredHeading = FLT_MAX;
			ProcessFishMove(*pPed);
		}
		else if (pCurrentMotionTask->GetTaskType() == CTaskTypes::TASK_ON_FOOT_BIRD)
		{
			pPed->GetMotionData()->SetFPSCheckRunInsteadOfSprint(false);
			m_fStdPreviousDesiredHeading = FLT_MAX;
			ProcessBirdMove(*pPed);
		}
		else if (pCurrentMotionTask->IsInWater())
		{
			pPed->GetMotionData()->SetFPSCheckRunInsteadOfSprint(false);
			m_fStdPreviousDesiredHeading = FLT_MAX;
			ProcessSwimMove(pPed);
		}
		else
		{
			// Prevent player input if running c4 task
			if (!pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_BOMB))
			{
				ProcessStdMove(pPed);
			}
			else
			{
				pPed->GetMotionData()->SetFPSCheckRunInsteadOfSprint(false);
			}
		}
	}

	// Don't allow the crouch status to be changed whilst transitioning
	s32 iMotionState = pPed->GetPrimaryMotionTask()->GetState();
	const bool bDisableCrouchToggle = pPed->GetIsSwimming() || ((pPed->GetPrimaryMotionTask()->GetTaskType() == CTaskTypes::TASK_MOTION_PED) && (iMotionState == CTaskMotionPed::State_CrouchToStand || iMotionState == CTaskMotionPed::State_StandToCrouch));

	if (!bDisableCrouchToggle)
	{
		pPed->GetPlayerInfo()->ControlButtonDuck();
	}

	return FSM_Continue;
}

#if !__FINAL
void CTaskMovePlayer::Debug() const
{
	if(GetSubTask())
		GetSubTask()->Debug();
}
#endif

void CTaskMovePlayer::DoAbort(const AbortPriority UNUSED_PARAM(iPriority), const aiEvent* UNUSED_PARAM(pEvent))
{
}

void CTaskMovePlayer::ProcessStdMove(CPed* pPlayerPed)
{
	CControl *pControl = pPlayerPed->GetControlFromPlayer();
	Vector2 vecStickInput;
	vecStickInput.x = pControl->GetPedWalkLeftRight().GetNorm(ms_fDeadZone);
	// y stick results are positive for down (pulling back) and negative for up, so reverse to match world y direction
	vecStickInput.y = -pControl->GetPedWalkUpDown().GetNorm(ms_fDeadZone);

#if RSG_ORBIS
	TUNE_GROUP_FLOAT(PLAYER_STICK_TUNE, STRAFE_POW, 0.75f, 0.f, 10.f, 0.01f);
	vecStickInput.x = powf(Abs(vecStickInput.x), STRAFE_POW) * Sign(vecStickInput.x);
	vecStickInput.x = Clamp(vecStickInput.x, -1.f, 1.f);
	vecStickInput.y = powf(Abs(vecStickInput.y), STRAFE_POW) * Sign(vecStickInput.y);
	vecStickInput.y = Clamp(vecStickInput.y, -1.f, 1.f);
#endif // RSG_ORBIS

	CPlayerInfo* pPlayerInfo = pPlayerPed->GetPlayerInfo();

#if FPS_MODE_SUPPORTED && KEYBOARD_MOUSE_SUPPORT
	// B*2325743: Reset toggle run value when sprinting so we will drop back into run if we stop.
	if (m_bWasSprintingOnPCMouseAndKeyboard && pControl->WasKeyboardMouseLastKnownSource() && pControl->GetPedSprint().IsDown())
	{
		pPlayerPed->GetControlFromPlayer()->ClearToggleRun();
	}
#endif	// FPS_MODE_SUPPORTED && KEYBOARD_MOUSE_SUPPORT


#if FPS_MODE_SUPPORTED
	// No longer disabling left stick input
	// TODO: remove if no longer needed
	const bool bInFpsMode = pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false);
	bool bIsFpsModeSprinting = false;
	if(bInFpsMode && !pPlayerInfo->GetSimulateGaitInputOn())
	{
		bool bSprinting = pPlayerPed->GetMotionData()->GetIsDesiredSprinting(pPlayerPed->GetMotionData()->GetFPSCheckRunInsteadOfSprint());
		if(bSprinting)
		{
			if(!camInterface::GetGameplayDirector().IsFirstPersonShooterLeftStickControl() || vecStickInput.Mag2() == 0.0f)
			{
				// Disable left stick input when sprinting using first person shooter camera.
				vecStickInput.Set(0.0f, 1.0f);
			}
			bIsFpsModeSprinting = true;
		}
	}
#endif // FPS_MODE_SUPPORTED

	float moveBlendRatio = 0.0f;
	// don't let the player walk when attached... unless they are attached to the ground.
	if(!pPlayerPed->GetIsAttached() || pPlayerPed->GetIsAttachedToGround())
		moveBlendRatio = vecStickInput.Mag();// / ms_fStickRange;

#if FPS_MODE_SUPPORTED
	// B*2099984: CTaskMovePlayer::StateRunning_OnUpdate gets in here for a frame when transitioning between aim/swim motion tasks. 
	// This was causing MBR to be reset to 0 if no stick input. Safest fix for now just update the MBR based on the sprint button.
	if (pPlayerPed->GetIsFPSSwimming() && pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_FPSSwimUseSwimMotionTask))
	{
		float fSprintResult = pPlayerPed->GetPlayerInfo()->ControlButtonSprint(CPlayerInfo::SPRINT_UNDER_WATER, true);
		moveBlendRatio = fSprintResult;
	}
#endif	//FPS_MODE_SUPPORTED

	if(moveBlendRatio > 1.0f)
		moveBlendRatio = 1.0f;

	//! Increase to run speed when in action mode.
// 	if(pPlayerPed->IsBeingForcedToRun())
// 	{
// 		static dev_float s_fRescalePower = 3.0f;
// 		moveBlendRatio = powf(moveBlendRatio, s_fRescalePower);
// 	}

	if(moveBlendRatio < ms_fStickInCentreMag)
		m_bStickHasReturnedToCentre = true;

	// get the orientation of the FIRST follow ped camera, for player controls
	float fCamOrient = fwAngle::LimitRadianAngle(camInterface::GetPlayerControlCamHeading(*pPlayerPed));

	//Clone the camera pitch for the ped pitch if the move blender allows it.
	float fCamPitch = 0.0f;
//	if(pPlayerPed->GetMotionData() && pPlayerPed->GetMotionData()->ShouldCameraControlPitch())
//	{
//		fCamPitch = camInterface::GetPlayerControlCamPitch(*pPlayerPed);
//	}

	if((moveBlendRatio > 0.0f) || (fCamPitch == 0.0f)) //Always allow the pitch to be reset.
	{
		pPlayerPed->SetDesiredPitch(fCamPitch);
	}

	if(moveBlendRatio > 0.0f)
	{
		// reset simulated gait.
		if(pPlayerInfo->GetSimulateGaitInputOn())
		{
			if(!pPlayerInfo->GetSimulateGaitNoInputInterruption() || pPlayerInfo->GetSimulateGaitTimerCount() > pPlayerInfo->GetSimulateGaitDuration())
			{
				pPlayerInfo->ResetSimulateGaitInput();
			}
			else
			{
				// Update the timer
				pPlayerInfo->SetSimulateGaitTimerCount(pPlayerInfo->GetSimulateGaitTimerCount() + fwTimer::GetTimeStep());
			}
		}

		float fStickAngle = rage::Atan2f(-vecStickInput.x, vecStickInput.y);
		bool bPlayeReversingAsQuadAnimal = false;
		if (pPlayerPed->GetPedType() == PEDTYPE_ANIMAL)
		{
			if (const CTaskQuadLocomotion* pQuadMoveTask = static_cast<const CTaskQuadLocomotion*>(pPlayerPed->GetPedIntelligence()->FindTaskActiveByTreeAndType(PED_TASK_TREE_MOTION, CTaskTypes::TASK_ON_FOOT_QUAD)))
			{
				bPlayeReversingAsQuadAnimal = pQuadMoveTask->IsReversing();
			}
		}

		if (bPlayeReversingAsQuadAnimal)
		{
			float fStickThreshold = DtoR * 90.f;
			if (abs(fStickAngle) > fStickThreshold)
			{
				float fPlusMinus = fStickAngle > 0 ? -1.0f : 1.0f;
				float fDelta = abs(fStickAngle) - fStickThreshold;
				fStickAngle = Clamp(fStickAngle, -fStickThreshold, fStickThreshold);
				fStickAngle += (fPlusMinus * fDelta);
			}

			fStickAngle = fStickAngle + fwAngle::LimitRadianAngle(pPlayerPed->GetCurrentHeading());;
		}
		else
		{
			fStickAngle = fStickAngle + fCamOrient;
		}

		fStickAngle = fwAngle::LimitRadianAngle(fStickAngle);

		if(m_fStdPreviousDesiredHeading == FLT_MAX)
			pPlayerPed->SetDesiredHeading(fStickAngle);
		else
		{
			float fAngleDiff = SubtractAngleShorter(m_fStdPreviousDesiredHeading, fStickAngle);
			if(Abs(fAngleDiff) < PI * 0.9f)
			{
				pPlayerPed->SetDesiredHeading(fStickAngle);
			}
		}
		m_fStdPreviousDesiredHeading = fStickAngle;

	#if FPS_MODE_SUPPORTED
		if( bIsFpsModeSprinting )
		{
			// Turn to camera heading when sprinting using first person shooter camera.
			pPlayerPed->SetDesiredHeading(fCamOrient);
			m_fStdPreviousDesiredHeading = fCamOrient;
		}
	#endif // FPS_MODE_SUPPORTED
	}
	else if(pPlayerInfo->GetSimulateGaitInputOn())	// if we want to simulate player's movement
	{
		float fDuration = pPlayerInfo->GetSimulateGaitDuration();
		float fTimerCount = pPlayerInfo->GetSimulateGaitTimerCount();
		if(fDuration < 0.0f || fDuration >= fTimerCount )
		{
			vecStickInput.x = 0.0f;
			vecStickInput.y = 1.0f;

			float fHeading = pPlayerInfo->GetSimulateGaitHeading();
			if(pPlayerInfo->GetSimulateGaitUseRelativeHeading())
			{
				fHeading = fwAngle::LimitRadianAngle(fHeading + pPlayerInfo->GetSimulateGaitStartHeading());
				pPlayerPed->SetDesiredHeading(fHeading);
			}
			else
			{
				fHeading = fwAngle::LimitRadianAngle(fHeading);
				pPlayerPed->SetDesiredHeading(fHeading);
			}

			fTimerCount += fwTimer::GetTimeStep();
			pPlayerInfo->SetSimulateGaitTimerCount(fTimerCount);
		}
		else
		{
			pPlayerInfo->ResetSimulateGaitInput();
		}
	}
	else
	{
		m_fStdPreviousDesiredHeading = FLT_MAX;

		// JR - for the most part we are taking out the idle turns for animals as it feels better.
		if (fabs(moveBlendRatio) < SMALL_FLOAT && pPlayerPed->GetPedType() == PEDTYPE_ANIMAL)
		{
			pPlayerPed->SetDesiredHeading(pPlayerPed->GetCurrentHeading());
		}
	}

	const bool bUseButtonBashing = true;

	// if we want to simulate player's movement
	if(pPlayerInfo->GetSimulateGaitInputOn())	
	{
		moveBlendRatio = pPlayerInfo->GetSimulateGaitMoveBlendRatio();
	}
	else
	{
		float fSprintResult = pPlayerInfo->ControlButtonSprint(CPlayerInfo::SPRINT_ON_FOOT, bUseButtonBashing);

		//DEBUG
		TUNE_GROUP_BOOL(PED_MOVEMENT, bConstantSprint, false);
		if( bConstantSprint )
		{
			moveBlendRatio = MOVEBLENDRATIO_SPRINT;
			fSprintResult = 2.0f;
		}
		//DEBUG

		// Script-specified disabling of sprinting gives a max movespeed of 1.0f (running)
		if(pPlayerInfo->GetPlayerDataPlayerSprintDisabled())
			fSprintResult = Min(fSprintResult, 1.0f);

		if(fSprintResult > 0.0f)
			m_bStickHasReturnedToCentre = false;

		if(ms_bStickyRunButton && !m_bStickHasReturnedToCentre)
			fSprintResult = Max(fSprintResult, 1.0f);

		if(fSprintResult > 1.0f)
		{
			moveBlendRatio = moveBlendRatio > 0.f ? ms_fSprintBlend : 0.f;

			static const float fMaxCrouchedSprintVal = 1.25f;
			if(fSprintResult >= fMaxCrouchedSprintVal)
			{
				if(pPlayerPed->GetIsCrouching())
					pPlayerPed->SetIsCrouching(false);

				// JB : If L3 *doesn't* toggle stealth, then don't let the player
				// exit stealth mode once the script/gamelogic has set him in it.
				if(pPlayerPed->GetMotionData()->GetUsingStealth())
					pPlayerPed->GetMotionData()->SetUsingStealth(false);
			}
		}
		else if(fSprintResult > 0.0f)
		{
			moveBlendRatio = moveBlendRatio > 0.f ? ms_fRunBlend : 0.f;
		}
		else if(pPlayerPed->IsBeingForcedToRun())
		{
#if __XENON
			static dev_float RUN_SPEED_BOUNDARY = 0.75f;
#else
			static dev_float RUN_SPEED_BOUNDARY = 0.9f;
#endif // __XENON
			if(moveBlendRatio >= RUN_SPEED_BOUNDARY)
				moveBlendRatio = MOVEBLENDRATIO_RUN;
		}
		else
		{
			moveBlendRatio *= ms_fWalkBlend;
			moveBlendRatio = rage::Min(moveBlendRatio, ms_fWalkBlend);
		}

#if FPS_MODE_SUPPORTED
		// In first person shooter camera, check for analog stick run, if we are currently not running
		if(ShouldBeRunningDueToAnalogRun(pPlayerPed, moveBlendRatio, vecStickInput))
		{
			moveBlendRatio = MOVEBLENDRATIO_RUN;
		}
#endif // FPS_MODE_SUPPORTED
	}

	moveBlendRatio = Clamp(moveBlendRatio, MOVEBLENDRATIO_STILL, m_fMaxMoveBlendRatio);

	//***************************************************************************************
	//	If the stick input is just a slight nudge, then allow the player to do an idle-turn
	//	on the spot, but zero the moveBlendRatio so that they don't move forwards.
	//	The main reason for this is that now we have elaborate left/right walk-starts they
	//	conflict with the old method of doing an idle turn (ie. does the player want to start
	//	moving, or do they just want to turn on the spot?)

	Vector2 vCurrMoveRatio;
	pPlayerPed->GetMotionData()->GetCurrentMoveBlendRatio(vCurrMoveRatio);

	if(moveBlendRatio > 0.0f && moveBlendRatio < ms_fSmallMoveBlendRatioToDoIdleTurn && vCurrMoveRatio.x == 0.0f && vCurrMoveRatio.y == 0.0f)
	{
		// JR - this doesn't look great on the animals, see B* 2039798
		if(!pPlayerPed->IsBeingForcedToRun() && pPlayerPed->GetPedType() != PEDTYPE_ANIMAL)
		{
			moveBlendRatio = 0.0f;
		}
	}

	const float fPrevMbrSqr = pPlayerPed->GetMotionData()->GetDesiredMoveBlendRatio().Mag2();

	if( FPS_MODE_SUPPORTED_ONLY(!bInFpsMode &&) CNewHud::IsWeaponWheelActive() && moveBlendRatio > 0.f && fPrevMbrSqr > 0.f )
	{
		moveBlendRatio = Min(sqrt(fPrevMbrSqr), MOVEBLENDRATIO_RUN);
	}
	
	if(pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_LadderBlockingMovement))
	{
		moveBlendRatio = MOVEBLENDRATIO_STILL;
	}

	pPlayerPed->GetMotionData()->SetFPSCheckRunInsteadOfSprint(ShouldCheckRunInsteadOfSprint(pPlayerPed, moveBlendRatio));

	if (pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedIsArresting))
	{
		moveBlendRatio = MOVEBLENDRATIO_STILL;
	}
		
	// Set the desired moveblendratios for the player ped
	pPlayerPed->GetMotionData()->SetDesiredMoveBlendRatio(moveBlendRatio, 0.0f);
	m_fMoveBlendRatio = moveBlendRatio;

#if __PLAYER_ASSISTED_MOVEMENT
	if(CPlayerAssistedMovementControl::ms_bAssistedMovementEnabled)
	{
		const bool bForceScan = (moveBlendRatio > 0.0f) && (fPrevMbrSqr==0.0f);
		m_pAssistedMovementControl->ScanForSnapToRoute(pPlayerPed, bForceScan);
		m_pAssistedMovementControl->Process(pPlayerPed, vecStickInput);
	}
#endif

#if FPS_MODE_SUPPORTED && KEYBOARD_MOUSE_SUPPORT
	// B*2325743: Flag if we are using mouse and keyboard and sprinting. Used in CTaskMovePlayer::ProcessStrafeMove.
	m_bWasSprintingOnPCMouseAndKeyboard = false;
	if (pPlayerPed->GetControlFromPlayer() && pPlayerPed->GetControlFromPlayer()->WasKeyboardMouseLastKnownSource() && pPlayerPed->GetControlFromPlayer()->GetPedSprint().IsDown()
		&& pPlayerPed->GetMotionData() && pPlayerPed->GetMotionData()->GetCurrentMoveBlendRatio().Mag() >= 2.0f)
	{
		m_bWasSprintingOnPCMouseAndKeyboard = true;
	}
#endif	// FPS_MODE_SUPPORTED && KEYBOARD_MOUSE_SUPPORT
}

void CTaskMovePlayer::ProcessStrafeMove(CPed* pPlayerPed)
{
	pPlayerPed->SetIsStrafing(true);

#if FPS_MODE_SUPPORTED && KEYBOARD_MOUSE_SUPPORT
	// B*2325743: Just started strafing from sprint whilst using keyboard and mouse (and sprint input is still down). 
	// CControl::GetPedSprintIsDown was returning false due to the toggle run value being true, causing us to walk due to CPlayerInfo::ControlButtonSprint returning 0.0f instead of 1.0f. 
	// ...So force the toggle run value to false while we're still holding sprint down. 
	if (m_bWasSprintingOnPCMouseAndKeyboard && pPlayerPed->GetControlFromPlayer() && pPlayerPed->GetControlFromPlayer()->WasKeyboardMouseLastKnownSource() && pPlayerPed->GetControlFromPlayer()->GetPedSprint().IsDown())
	{
		pPlayerPed->GetControlFromPlayer()->SetToggleRunValue(false);
	}
	// ...If we let go of sprint, reset the toggle run value and clear the "was sprinting" flag.
	else if (m_bWasSprintingOnPCMouseAndKeyboard && pPlayerPed->GetControlFromPlayer() && !pPlayerPed->GetControlFromPlayer()->GetPedSprint().IsDown())
	{
		m_bWasSprintingOnPCMouseAndKeyboard = false;
		pPlayerPed->GetControlFromPlayer()->ClearToggleRun();
	}
#endif	// FPS_MODE_SUPPORTED && KEYBOARD_MOUSE_SUPPORT

	// get the orientation of the FIRST follow ped camera, for player controls.
	const float fCamOrient = fwAngle::LimitRadianAngleSafe(camInterface::GetPlayerControlCamHeading(*pPlayerPed));

#if FPS_MODE_SUPPORTED
	const bool bInFpsMode = pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false);
#endif // FPS_MODE_SUPPORTED

	float fMoveRatio = 0.0f;
	Vector2 vecStickInput(0.0f, 0.0f);
	const CEntity* pLockOnTarget = pPlayerPed->GetPlayerInfo()->GetTargeting().GetLockOnTarget();
	if( pLockOnTarget && pPlayerPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_GUN))
	{
		static dev_float s_gunStrafingMinDistance = 1.2f;
		vecStickInput = ProcessStrafeInputWithRestrictions( pPlayerPed, pLockOnTarget, s_gunStrafingMinDistance, m_fStickAngle, m_bStickAngleTweaked );
		fMoveRatio = vecStickInput.Mag();
	}
	else
	{
		CControl *pControl = pPlayerPed->GetControlFromPlayer();
#if FPS_MODE_SUPPORTED
		if(bInFpsMode)
		{
			vecStickInput.x = pControl->GetPedWalkLeftRight().GetNorm(ioValue::NO_DEAD_ZONE);
			vecStickInput.y = -pControl->GetPedWalkUpDown().GetNorm(ioValue::NO_DEAD_ZONE);
		}
		else
#endif // FPS_MODE_SUPPORTED
		{
			vecStickInput.x = pControl->GetPedWalkLeftRight().GetNorm();
			vecStickInput.y = -pControl->GetPedWalkUpDown().GetNorm();
		}
		
#if RSG_ORBIS
		TUNE_GROUP_FLOAT(PLAYER_STICK_TUNE, STRAFE_POW, 0.75f, 0.f, 10.f, 0.01f);
		vecStickInput.x = powf(Abs(vecStickInput.x), STRAFE_POW) * Sign(vecStickInput.x);
		vecStickInput.x = Clamp(vecStickInput.x, -1.f, 1.f);
		vecStickInput.y = powf(Abs(vecStickInput.y), STRAFE_POW) * Sign(vecStickInput.y);
		vecStickInput.y = Clamp(vecStickInput.y, -1.f, 1.f);
#endif // RSG_ORBIS

#if FPS_MODE_SUPPORTED
		if(bInFpsMode)
		{
			fMoveRatio = rage::ioAddRoundDeadZone(vecStickInput.x, vecStickInput.y, ms_fDeadZone);
		}
		else
#endif // FPS_MODE_SUPPORTED
		{
			fMoveRatio = vecStickInput.Mag();
		}

		// Normalise if > 1.0. Don't want to normalise below this as it'll kill subtle, half stick movement.
		if(fMoveRatio > 1.0f)
		{
			vecStickInput.Normalize();
			fMoveRatio = 1.0f;
		}

		weaponAssert(pPlayerPed->GetWeaponManager());
		if(CTaskPlayerOnFoot::ms_bAllowStrafingWhenUnarmed && CPlayerInfo::IsAiming()
			&& pPlayerPed->GetWeaponManager()->GetEquippedWeapon() && pPlayerPed->GetWeaponManager()->GetEquippedWeapon()->GetWeaponInfo()->GetIsMelee())
		{
			pPlayerPed->SetDesiredHeading(fCamOrient);
		}

#if FPS_MODE_SUPPORTED 
		// FPS Swim Strafing: Rotate based on camera heading and pitch
		if (bInFpsMode && pPlayerPed->GetIsSwimming())
		{
			pPlayerPed->SetDesiredHeading(fCamOrient);

			// Only use pitch if we are under the surface
			CTaskMotionBase* pPrimaryTask = pPlayerPed->GetPrimaryMotionTask();
			if( pPrimaryTask && pPrimaryTask->GetTaskType() == CTaskTypes::TASK_MOTION_PED )
			{
				CTaskMotionPed* pTask = static_cast<CTaskMotionPed*>(pPrimaryTask);
				if (pTask && pTask->CheckForDiving())
				{
					float fPitch = camInterface::GetPlayerControlCamPitch(*pPlayerPed);
					fPitch = Clamp(fPitch, -1.0f, 1.0f) * CTaskMotionDiving::ms_fMaxPitch;
					pPlayerPed->GetMotionData()->SetCurrentPitch(fPitch);			
				}
			}
		}
#endif // FPS_MODE_SUPPORTED

		if( fMoveRatio > 0.0f )
		{
			if(fMoveRatio < ms_fStickInCentreMag)
				m_bStickHasReturnedToCentre = true;

			float fStickAngle = rage::Atan2f(-vecStickInput.x, vecStickInput.y);//fwAngle::GetRadianAngleBetweenPoints(0.0f, 0.0f, -vMoveBlendRatio.x, vMoveBlendRatio.y);
			fStickAngle = fStickAngle + fCamOrient;
			fStickAngle = fwAngle::LimitRadianAngleSafe(fStickAngle);

			Vector3 vecDesiredDirection(-rage::Sinf(fStickAngle), rage::Cosf(fStickAngle), 0.0f);

			vecStickInput.x = fMoveRatio*DotProduct(vecDesiredDirection, VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetA()));
			vecStickInput.y = fMoveRatio*DotProduct(vecDesiredDirection, VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetB()));

			vecStickInput.x = Clamp(vecStickInput.x, -1.0f, 1.0f);
			vecStickInput.y = Clamp(vecStickInput.y, -1.0f, 1.0f);
		}
	}

	CPlayerInfo* pPlayerInfo = pPlayerPed->GetPlayerInfo();
	if( fMoveRatio > 0.0f && !pPlayerInfo->GetSimulateGaitInputOn() )
	{
		float fBlendMult = ms_fWalkBlend;
		float fRescalePow = 3.0f;
		bool bNormaliseStickInput = false;

		float fSprintResult = pPlayerPed->GetPlayerInfo()->ControlButtonSprint(CPlayerInfo::SPRINT_ON_FOOT);

		// Script-specified disabling of sprinting gives a max movespeed of 1.0f (running)
		if(pPlayerPed->GetPlayerInfo()->GetPlayerDataPlayerSprintDisabled())
			fSprintResult = Min(fSprintResult, 1.0f);

		if(fSprintResult > 0.0f)
			m_bStickHasReturnedToCentre = false;

		if(ms_bStickyRunButton && !m_bStickHasReturnedToCentre)
			fSprintResult = Max(fSprintResult, 1.0f);

		// Don't auto run when NoAutoRunWhenFiring is set, e.g., pouring petrol can.
		bool bNoAutoRunWhenFiring = false;
		const CWeapon* pWeapon = pPlayerPed->GetWeaponManager()->GetEquippedWeapon();
		if (pWeapon && (pWeapon->GetWeaponInfo()->GetNoAutoRunWhenFiring() || pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_NoAutoRunWhenFiring)) FPS_MODE_SUPPORTED_ONLY(&& (!pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false) || !pPlayerPed->GetMotionData()->GetIsFPSIdle()))) 
		{
			bNoAutoRunWhenFiring = true;
		}

		if(!bNoAutoRunWhenFiring)
		{
			if(fSprintResult > 1.0f)
			{
				fBlendMult = ms_fSprintBlend;
				bNormaliseStickInput = true;
	
#if FPS_MODE_SUPPORTED
				if(bInFpsMode)
				{
					pPlayerPed->GetMotionData()->SetUsingStealth(false);
					pPlayerPed->GetMotionData()->SetIsUsingStealthInFPS(false);
				}
#endif // FPS_MODE_SUPPORTED
			}
			else if(!pPlayerPed->GetMotionData()->GetUsingStealth() && pWeapon && pWeapon->GetWeaponInfo()->GetIsGunOrCanBeFiredLikeGun() && (pPlayerPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON) || pPlayerPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_RUN_AND_GUN)) && (!pPlayerPed->GetIsInInterior() || pPlayerPed->GetPortalTracker()->IsAllowedToRunInInterior() || pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IgnoreInteriorCheckForSprinting)) )
			{
				fBlendMult = ms_fRunBlend;
				static dev_float s_fRescalePower = 3.0f;
				fRescalePow = s_fRescalePower;
			}
			else if(fSprintResult > 0.0)
			{
				fBlendMult = ms_fRunBlend;
				bNormaliseStickInput = true;
			}

#if FPS_MODE_SUPPORTED
			// In first person shooter camera, check for analog stick run, if we are currently not running
			if(ShouldBeRunningDueToAnalogRun(pPlayerPed, fBlendMult, vecStickInput))
			{
				// When armed and strafing, force run when stick is fully deflected.
				fBlendMult = ms_fRunBlend;
				bNormaliseStickInput = true;
			}
#endif // FPS_MODE_SUPPORTED
		}

		bool isAccuratelyAiming = false;
		camThirdPersonAimCamera* pAimCamera = camInterface::GetGameplayDirector().GetThirdPersonAimCamera();
		if(pAimCamera)
		{
			isAccuratelyAiming = pAimCamera->IsInAccurateMode();
		}

		if(isAccuratelyAiming)
		{
			// Clamp fBlendMult to walking speed
			fBlendMult = ms_fWalkBlend;
		}

		// Scale the stick input
#if FPS_MODE_SUPPORTED
		if(bInFpsMode)
		{
			vecStickInput *= fBlendMult;
		}
		else
#endif // FPS_MODE_SUPPORTED
		{
			float fMag = vecStickInput.Mag();
			if(fMag > 0.f)
			{
				if(bNormaliseStickInput)
				{
					vecStickInput.Normalize();
					fMag = fBlendMult;
				}
				else if(fRescalePow > 1.0f && fMag < 1.0f)
				{
					float fRescaledMag = powf(fMag, fRescalePow);
					vecStickInput.Normalize();
					vecStickInput *= fRescaledMag;
					fMag = fRescaledMag;
				}

				vecStickInput *= fBlendMult;
			}
			else
			{ 
				vecStickInput.x = 0.f;
				vecStickInput.y = 0.f;
			}

			// Clamp the moveblend-ratio to some minimum value.
			// This is because strafing a very slow speeds looks silly, as with current move code
			// we end up with a blend which is very close to idle.

			bool bAdjustStickInput = fMag > 0.0f && fMag < ms_fStrafeMinMoveBlendRatio;
			if(bAdjustStickInput)
			{
				vecStickInput *= (ms_fStrafeMinMoveBlendRatio/fMag);
			}
		}
	}

	// Process simulate gait.
	CControl *pControl = pPlayerPed->GetControlFromPlayer();
	
	if(fMoveRatio > 0.0f || pControl->GetPedTargetIsDown() || pControl->GetPedAttack().IsPressed() || pControl->GetPedAttack2().IsPressed())
	{
		// Disable simulate gait if there is an input detected.
		if(pPlayerInfo->GetSimulateGaitInputOn() ) 
		{
			if(!pPlayerInfo->GetSimulateGaitNoInputInterruption() || pPlayerInfo->GetSimulateGaitTimerCount() > pPlayerInfo->GetSimulateGaitDuration())
			{
				pPlayerInfo->ResetSimulateGaitInput();
			}
			else
			{
				// Update the timer
				pPlayerInfo->SetSimulateGaitTimerCount(pPlayerInfo->GetSimulateGaitTimerCount() + fwTimer::GetTimeStep());
			}
		}
	}
	else if(pPlayerInfo->GetSimulateGaitInputOn())
	{
		float fDuration = pPlayerInfo->GetSimulateGaitDuration();
		float fTimerCount = pPlayerInfo->GetSimulateGaitTimerCount();
		if(fDuration < 0.0f || fDuration >= fTimerCount )
		{
			vecStickInput.x = 0.0f;
			vecStickInput.y = pPlayerInfo->GetSimulateGaitMoveBlendRatio();

			float fStickAngle = pPlayerInfo->GetSimulateGaitHeading();
			if(pPlayerInfo->GetSimulateGaitUseRelativeHeading())
			{
				fStickAngle = fStickAngle + pPlayerInfo->GetSimulateGaitStartHeading();
			}

			fStickAngle = fStickAngle + fCamOrient;
#if FPS_MODE_SUPPORTED
			if(bInFpsMode)
			{
				fStickAngle -= pPlayerPed->GetCurrentHeading();
			}
#endif // FPS_MODE_SUPPORTED
			fStickAngle = fwAngle::LimitRadianAngleSafe(fStickAngle);

			Vector3 vecDesiredDirection(-rage::Sinf(fStickAngle), rage::Cosf(fStickAngle), 0.0f);

			vecStickInput.x = DotProduct(vecDesiredDirection, VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetA()));
			vecStickInput.y = DotProduct(vecDesiredDirection, VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetB()));

			vecStickInput.x = Clamp(vecStickInput.x, -1.0f, 1.0f);
			vecStickInput.y = Clamp(vecStickInput.y, -1.0f, 1.0f);

			fTimerCount += fwTimer::GetTimeStep();
			pPlayerInfo->SetSimulateGaitTimerCount(fTimerCount);
		}
		else
		{
			pPlayerInfo->ResetSimulateGaitInput();
		}
	}

	// if we want to simulate player's movement
	if(pPlayerInfo->GetSimulateGaitInputOn())	
	{
		m_fMoveBlendRatio = pPlayerInfo->GetSimulateGaitMoveBlendRatio();
#if FPS_MODE_SUPPORTED
		if(bInFpsMode)
		{
			pPlayerPed->GetMotionData()->SetDesiredMoveBlendRatio(vecStickInput.y * m_fMoveBlendRatio, vecStickInput.x * m_fMoveBlendRatio);
		}
		else
#endif // FPS_MODE_SUPPORTED
		{
			pPlayerPed->GetMotionData()->SetDesiredMoveBlendRatio(vecStickInput.y, vecStickInput.x);
		}
	}
	else
	{
		if(CNewHud::IsWeaponWheelActive())
		{
			const float fStickInputSqr = vecStickInput.Mag2();
			if(fStickInputSqr > 0.f)
			{
				float fStickInputMag = sqrt(fStickInputSqr);
				vecStickInput.Scale(1.f/fStickInputMag);
				float moveBlendRatio = Min(fStickInputMag, MOVEBLENDRATIO_RUN);
				vecStickInput *= moveBlendRatio;
			}
		}

		if(pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_LadderBlockingMovement))
		{
			vecStickInput *= MOVEBLENDRATIO_STILL;
		}

		if (pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedIsArresting))
		{
			vecStickInput *= MOVEBLENDRATIO_STILL;
		}

		pPlayerPed->GetMotionData()->SetFPSCheckRunInsteadOfSprint(ShouldCheckRunInsteadOfSprint(pPlayerPed, vecStickInput.Mag()));

		// Set the desired move blend (which is smoothed internally) and force
		// the momentum (hit) based movement (which won't be smoothed internally).
		pPlayerPed->GetMotionData()->SetDesiredMoveBlendRatio(vecStickInput.y, vecStickInput.x);
		m_fMoveBlendRatio = vecStickInput.Mag();
		m_fMoveBlendRatio = Clamp(m_fMoveBlendRatio, MOVEBLENDRATIO_STILL, MOVEBLENDRATIO_SPRINT);
#if __PLAYER_ASSISTED_MOVEMENT
		if(CPlayerAssistedMovementControl::ms_bAssistedMovementEnabled)
		{
			m_pAssistedMovementControl->ScanForSnapToRoute(pPlayerPed);
			m_pAssistedMovementControl->Process(pPlayerPed, vecStickInput);
		}
#endif
	}
}


void CTaskMovePlayer::ProcessSwimMove(CPed* pPlayerPed)
{
	CTaskMotionBase* pBaseTask = pPlayerPed->GetCurrentMotionTask();

	const bool bUnderwater = pBaseTask->IsUnderWater();

	m_fUnderwaterGlideTimer -= fwTimer::GetTimeStep();
	m_fUnderwaterGlideTimer = Max(m_fUnderwaterGlideTimer, 0.0f);

	m_fSwitchStrokeTimerOnSurface -= fwTimer::GetTimeStep();
	m_fSwitchStrokeTimerOnSurface = Max(m_fSwitchStrokeTimerOnSurface, 0.0f);
	
	//***********************
	//	Underwater swimming

	//! DMKH. Do not process dive if we are already climbing. Or we have a valid handhold to grab.
	CClimbHandHoldDetected handHold;
	bool bHandHoldDetected = pPlayerPed->GetPedIntelligence()->IsPedClimbing();

	//! TO DO - Create a dive control?
	CControl * pControl = pPlayerPed->GetControlFromPlayer();
	bool bDivePressed =  pControl && pControl->GetPedDive().IsPressed() && !pPlayerPed->GetIsFPSSwimming();

	bool bFPSMode = false;
#if FPS_MODE_SUPPORTED
	if (pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		bFPSMode = true;

		// Go to dive-down state if aiming down and pressing forwards on stick
		if (pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_FPSSwimUseSwimMotionTask))
		{
			float fCamPitch = camInterface::GetPlayerControlCamPitch(*pPlayerPed);		
			TUNE_GROUP_FLOAT(FPS_SWIMMING, fDivePitchThresholdFromSwimming, -0.75f, -1.0f, 1.0f, 0.01f);
			if (fCamPitch <= fDivePitchThresholdFromSwimming)
			{
				bDivePressed = true;
			}
		}
	}
#endif	//FPS_MODE_SUPPORTED

	if(!bHandHoldDetected && bDivePressed)
	{	
		pPlayerPed->SetPedResetFlag(CPED_RESET_FLAG_SearchingForClimb, true);
		pPlayerPed->GetPedIntelligence()->GetClimbDetector().Process();
		bHandHoldDetected = pPlayerPed->GetPedIntelligence()->GetClimbDetector().GetDetectedHandHold(handHold);
	}

	if(bUnderwater)
	{
		CTaskMotionDiving* pTaskMotionDiving = pBaseTask && pBaseTask->GetTaskType() == CTaskTypes::TASK_MOTION_DIVING ? static_cast<CTaskMotionDiving*>(pPlayerPed->GetCurrentMotionTask()) : NULL;

		// Handle diving under
		if(bDivePressed && !bHandHoldDetected)
		{
			pPlayerPed->GetPrimaryMotionTask()->SetWantsToDiveUnderwater();
		}

		CControl * pControl = pPlayerPed->GetControlFromPlayer();
		const float fStickLeftRight = -pControl->GetPedWalkLeftRight().GetNorm();
		float fPitch = pControl->GetPedWalkUpDown().GetNorm();

		// Increase the idle timer if swim-state is idling, and there's no stick input
		// After a certain period we will auto-level the ped to help him orientate.
		if(pTaskMotionDiving && pTaskMotionDiving->GetIsIdle() && Abs(fStickLeftRight) < 0.01f && Abs(fPitch) < 0.01f)
		{
			m_fUnderwaterIdleTimer += fwTimer::GetTimeStep();
		}
		else
		{
			m_fUnderwaterIdleTimer = 0.0f;
		}

		const bool bAutoLevel = (m_fUnderwaterIdleTimer >= ms_fUnderwaterIdleAutoLevelTime);

		// get the orientation of the FIRST follow ped camera, for player controls
		float fCamOrient = camInterface::GetPlayerControlCamHeading(*pPlayerPed);
		float fCamPitch = camInterface::GetPlayerControlCamPitch(*pPlayerPed);		

		// B*2015587: PC Mouse/Keyboard controls - Always pitch based on camera pitch if using mouse/keyboard controls, don't reset it to 0.
		bool bUsingPCControlsUnderwater = false;
#if KEYBOARD_MOUSE_SUPPORT
		if(pControl->WasKeyboardMouseLastKnownSource())
		{
			bUsingPCControlsUnderwater = true;
		}
#endif // KEYBOARD_MOUSE_SUPPORT

		if (pControl->GetPedLookUpDown().GetNorm() == 0 && !bUsingPCControlsUnderwater)
			fCamPitch = 0;

		if(camInterface::GetDebugDirector().IsFreeCamActive())
		{
			fCamOrient = pPlayerPed->GetTransform().GetHeading();
		}

		//*********
		// Pitch
		fPitch = Clamp(fPitch, -1.0f, 1.0f) * CTaskMotionDiving::ms_fMaxPitch;

		if(pTaskMotionDiving && pTaskMotionDiving->IsDivingDown())
			fPitch = -CTaskMotionDiving::ms_fDiveUnderPitch;

		if(bAutoLevel)
		{
			fPitch = 0.0f;
		}
		else
		{
			if (bFPSMode)
			{
				// Just use camera pitch in FPS mode.
				const camFrame& aimCameraFrame	= camInterface::GetPlayerControlCamAimFrame();
				float fCamPitch = aimCameraFrame.ComputePitch();
				fPitch = fwAngle::LimitRadianAngle(fCamPitch);
				fPitch = Clamp(fPitch, -CTaskMotionDiving::ms_fMaxPitch, CTaskMotionDiving::ms_fMaxPitch);
			}
			else
			{
				fPitch = fwAngle::LimitRadianAngle(fPitch + fCamPitch);
				fPitch = Clamp(fPitch, -CTaskMotionDiving::ms_fMaxPitch, CTaskMotionDiving::ms_fMaxPitch);
			}
		}
		
		pPlayerPed->SetDesiredPitch(fPitch);

		//*********
		// Accel

		float fSprintResult = pPlayerPed->GetPlayerInfo()->ControlButtonSprint(CPlayerInfo::SPRINT_UNDER_WATER, true);
		// In FPS mode: use left stick to enter swim (run) state
		if (bFPSMode)
		{
			float fLeftStickPitch = -pControl->GetPedWalkUpDown().GetNorm();
			if (fLeftStickPitch > 0.0f)
			{
				fSprintResult += 1.0f;
			}
		}

		if (fSprintResult >= 1.0f)
		{
			m_fMoveBlendRatio = (fSprintResult > 1.0f) ? MOVEBLENDRATIO_SPRINT : MOVEBLENDRATIO_RUN;

			m_fUnderwaterGlideTimer = 1.0f;
		}
		//if Idle but pushing forward, apply a forward drift B* 1427969
		else if (pTaskMotionDiving && fPitch < -1.0f && pTaskMotionDiving->GetIsIdle())
		{
			pTaskMotionDiving->SetApplyForwardDrift();
		}
		else if(m_fUnderwaterGlideTimer <= 0.0f)
		{
			m_fMoveBlendRatio = MOVEBLENDRATIO_STILL;
		}

		// Restrict speed to breast-stroke when underwater & inside an interior (unless tagged as 'may run')
		if(pPlayerPed->GetPortalTracker()->IsInsideInterior() && !pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IgnoreInteriorCheckForSprinting) && !pPlayerPed->GetPortalTracker()->IsAllowedToRunInInterior())
		{
			m_fMoveBlendRatio = Min(m_fMoveBlendRatio, MOVEBLENDRATIO_WALK);
		}

		pPlayerPed->GetMotionData()->SetDesiredMoveBlendRatio(m_fMoveBlendRatio, 0.0f);

		//*****************
		// Desired heading

		static const float fMaxTurn = HALF_PI * 0.9f;

		float fLeftRightVal = Clamp(fStickLeftRight, -1.0f, 1.0f);
		fLeftRightVal *= fLeftRightVal;
		fLeftRightVal *= fLeftRightVal;
		fLeftRightVal *= Sign(fStickLeftRight);
		fLeftRightVal *= fMaxTurn;

		if (bFPSMode)
		{
			// Ignore left/right inputs in FPS mode. Just want to use camera orientation.
			fLeftRightVal = 0.0f;
		}

		//if(fLeftRightVal != 0.0f)
		float fHdg; 
		
		fHdg = fwAngle::LimitRadianAngle(fLeftRightVal + fCamOrient);

		if(pTaskMotionDiving && !pTaskMotionDiving->CanRotateCamRelative())
		{
			fHdg = fwAngle::LimitRadianAngle(fLeftRightVal + fCamOrient);
		}
		else
		{
			fHdg = fwAngle::LimitRadianAngle(fLeftRightVal + pPlayerPed->GetCurrentHeading());
		}

		pPlayerPed->SetDesiredHeading(fHdg);

	}

	//*************************
	//	On-surface swimming

	else
	{
		//taskAssert(pBaseTask && pBaseTask->GetTaskType()==CTaskTypes::TASK_MOTION_SWIMMING);
		float moveBlendRatio = MOVEBLENDRATIO_STILL;
		CControl *pControl = pPlayerPed->GetControlFromPlayer();
		Vector2 vecStickInput;
		vecStickInput.x = pControl->GetPedWalkLeftRight().GetNorm();
		// y stick results are positive for down (pulling back) and negative for up, so reverse to match world y direction
		vecStickInput.y = -pControl->GetPedWalkUpDown().GetNorm();

		// don't let the player walk when attached
		if(!pPlayerPed->GetIsAttached())
			moveBlendRatio = vecStickInput.Mag();// / ms_fStickRange;

#if FPS_MODE_SUPPORTED
		if (bFPSMode)
		{
			moveBlendRatio = vecStickInput.Mag() * 2.0f;

			// B*2038392: Allow just pressing/tapping sprint button to sprint in FPS mode (without any stick input)
			if (moveBlendRatio == MOVEBLENDRATIO_STILL)
			{
				moveBlendRatio = pPlayerPed->GetPlayerInfo()->ControlButtonSprint(CPlayerInfo::SPRINT_ON_WATER, true);
			}
		}
#endif	//FPS_MODE_SUPPORTED
		
		if(moveBlendRatio > 1.0f && !bFPSMode)
			moveBlendRatio = 1.0f;

		if(moveBlendRatio < ms_fStickInCentreMag)
			m_bStickHasReturnedToCentre = true;

		// get the orientation of the FIRST follow ped camera, for player controls
		float fCamOrient = camInterface::GetPlayerControlCamHeading(*pPlayerPed);

		float fCamPitch = 0.0f;

		if((moveBlendRatio > 0.0f) || (fCamPitch == 0.0f)) //Always allow the pitch to be reset.
		{
			pPlayerPed->SetDesiredPitch(fCamPitch);
		}

		if(moveBlendRatio > 0.0f && !bFPSMode)
		{
			float fStickAngle = rage::Atan2f(-vecStickInput.x, vecStickInput.y);
			fStickAngle = fStickAngle + fCamOrient;
			fStickAngle = fwAngle::LimitRadianAngle(fStickAngle);

			if (m_fLockedStickHeading != -1.0f)
			{
				if (fabs(fStickAngle - m_fLockedStickHeading) > EIGHTH_PI)
				{
					m_fLockedStickHeading = -1.0f;
				}
				else
					fStickAngle = m_fLockedHeading;
			}

			pPlayerPed->SetDesiredHeading(fStickAngle);
		} 
		else if (bFPSMode)
		{
			// Just use camera heading in FPS mode.
			const camFrame& aimCameraFrame	= camInterface::GetPlayerControlCamAimFrame();
			float fCamOrient = aimCameraFrame.ComputeHeading();
			pPlayerPed->SetDesiredHeading(fCamOrient);
		}
		else
		{
			pPlayerPed->SetDesiredHeading(pPlayerPed->GetCurrentHeading());
		}

		//end of new	

		// Handle diving under
		if (!bHandHoldDetected)
		{			
			if (pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_EnablePressAndReleaseDives))
			{
				static dev_u32 s_uMinHoldTimeForDive = 200;
				if (pControl && !pControl->GetPedDive().IsReleasedAfterHistoryHeldDown(s_uMinHoldTimeForDive) && pControl->GetPedDive().IsReleased())
				{
					pPlayerPed->GetPrimaryMotionTask()->SetWantsToDiveUnderwater();
				}
			}
			else if(bDivePressed)
			{
				//Check for dive space
				static dev_float sfMinDiveClearance = 2.0f;
				float fGroundClearance = CTaskMotionDiving::TestGroundClearance(pPlayerPed, sfMinDiveClearance, 0.5f);
				if (fGroundClearance<0) 
				{
					pPlayerPed->GetPrimaryMotionTask()->SetWantsToDiveUnderwater();
				}
			}
		}
				
		//************************
		// Forwards speed (cross)

		const bool bUseButtonBashing = true;

		float fSprintResult = pPlayerPed->GetPlayerInfo()->ControlButtonSprint(CPlayerInfo::SPRINT_ON_WATER, bUseButtonBashing);

		// Script-specified disabling of sprinting gives a max movespeed of 1.0f (running)
		if(pPlayerPed->GetPlayerInfo()->GetPlayerDataPlayerSprintDisabled())
			fSprintResult = Min(fSprintResult, 1.0f);

		if(fSprintResult > 1.0f)
		{
			moveBlendRatio = moveBlendRatio > 0.f ? (ms_fSprintBlend-ms_fRunBlend)*moveBlendRatio+ms_fRunBlend : 0.f;
			moveBlendRatio = rage::Min(moveBlendRatio, ms_fSprintBlend);		
		}
		else if(fSprintResult > 0.0f)
		{
			moveBlendRatio = moveBlendRatio > 0.f ? (ms_fRunBlend-ms_fWalkBlend)*moveBlendRatio+ms_fWalkBlend : 0.f;
			moveBlendRatio = rage::Min(moveBlendRatio, ms_fRunBlend);
		}
		else if (!bFPSMode)	
		{
			moveBlendRatio *= ms_fWalkBlend;
			moveBlendRatio = rage::Min(moveBlendRatio, ms_fWalkBlend);
		}

		moveBlendRatio = Clamp(moveBlendRatio, MOVEBLENDRATIO_STILL, MOVEBLENDRATIO_SPRINT);

		if(moveBlendRatio==MOVEBLENDRATIO_STILL)
		{
			m_fSwitchStrokeTimerOnSurface = 0.0f;
		}

		Vector2 vCurrMoveRatio;
		pPlayerPed->GetMotionData()->GetCurrentMoveBlendRatio(vCurrMoveRatio);

		// Set the desired move blend ratios for the player ped
		pPlayerPed->GetMotionData()->SetDesiredMoveBlendRatio(moveBlendRatio, 0.0f);
		m_fMoveBlendRatio = moveBlendRatio;
	}

#if __PLAYER_ASSISTED_MOVEMENT
	if(CPlayerAssistedMovementControl::ms_bAssistedMovementEnabled)
	{
		Vector2 vecStickInput(
			pPlayerPed->GetControlFromPlayer()->GetPedWalkLeftRight().GetNorm(),
			-pPlayerPed->GetControlFromPlayer()->GetPedWalkUpDown().GetNorm()
		);
		m_pAssistedMovementControl->ScanForSnapToRoute(pPlayerPed);
		m_pAssistedMovementControl->Process(pPlayerPed, vecStickInput);
	}
#endif
}

void CTaskMovePlayer::ProcessBirdMove(CPed& playerPed)
{
	if (playerPed.GetMotionData()->GetIsFlying())
	{
		CTask* pTaskLocomotion = playerPed.GetPedIntelligence()->GetMotionTaskActiveSimplest();
		CTaskBirdLocomotion* pTaskBirdLocomotion = pTaskLocomotion && pTaskLocomotion->GetTaskType() == CTaskTypes::TASK_ON_FOOT_BIRD ? static_cast<CTaskBirdLocomotion*>(pTaskLocomotion) : NULL;

		if (!pTaskBirdLocomotion)
		{
			return;
		}

		bool bFlapping = pTaskBirdLocomotion->IsFlapping();
		bool bGliding = pTaskBirdLocomotion->IsGliding();

		// No control if you aren't flapping or gliding.
		if (!bFlapping && !bGliding)
		{
			return;
		}

		CControl* pControl = playerPed.GetControlFromPlayer();
		Assert(pControl);
		const float fStickLeftRight = -pControl->GetPedWalkLeftRight().GetNorm();
		float fPitch = pControl->GetPedWalkUpDown().GetNorm();

		// Pitch
		fPitch = Clamp(fPitch, -1.0f, 1.0f) * CTaskBirdLocomotion::ms_fMaxPitch;
		fPitch = fwAngle::LimitRadianAngle(fPitch);

		float fMinPitch = -CTaskBirdLocomotion::ms_fMaxPitchPlayer;
		float fMaxPitch = CTaskBirdLocomotion::ms_fMaxPitchPlayer;

		// If you're gliding, then you are limited in your ascension rate.
		static const float sf_GlidingMaxPitch = 15.0f * DtoR;
		
		// If you're above a certain height then you can no longer ascend.
		static const float sf_MaxHeightForUpwardMovement = 1200.0f;

		if (playerPed.GetTransform().GetPosition().GetZf() >= sf_MaxHeightForUpwardMovement)
		{
			fMaxPitch = 0.0f;
		}
		else if (bGliding)
		{
			fMaxPitch = sf_GlidingMaxPitch;
		}

		fPitch = Clamp(fPitch, fMinPitch, fMaxPitch);
		
		playerPed.SetDesiredPitch(fPitch);

		// Desired heading
		static const float fMaxTurn = HALF_PI * 0.9f;

		float fLeftRightVal = Clamp(fStickLeftRight, -1.0f, 1.0f);
		fLeftRightVal *= fLeftRightVal;
		fLeftRightVal *= fLeftRightVal;
		fLeftRightVal *= Sign(fStickLeftRight);
		fLeftRightVal *= fMaxTurn;

		float fHdg = fwAngle::LimitRadianAngle(fLeftRightVal);
		fHdg = fwAngle::LimitRadianAngle(fLeftRightVal + playerPed.GetCurrentHeading());

		playerPed.SetDesiredHeading(fHdg);
			
		m_fMoveBlendRatio = MOVEBLENDRATIO_WALK;
		playerPed.GetMotionData()->SetDesiredMoveBlendRatio(m_fMoveBlendRatio, 0.0f);

		// Check for flapping input (x).
		if (pControl->GetPedSprintIsDown())
		{
			pTaskBirdLocomotion->RequestFlap();
		}
		
		// Check for landing input (square).
		if (pControl->GetPedJump().IsPressed() && pTaskBirdLocomotion->CanLandNow())
		{
			pTaskBirdLocomotion->RequestPlayerLand();
		}
	}
	else
	{
		// Treat birds on the ground as walking peds.
		ProcessStdMove(&playerPed);

		// However, query if the player hits the sprint button.
		// If they do, then start flying.
		CControl* pControl = playerPed.GetControlFromPlayer();
		if (pControl)
		{
			if (pControl->GetPedSprintIsPressed())
			{
				playerPed.GetMotionData()->SetIsFlyingForwards();
			}
		}
	}
}

void CTaskMovePlayer::ProcessFishMove(CPed& playerPed)
{
	CTask* pTaskLocomotion = playerPed.GetPedIntelligence()->GetMotionTaskActiveSimplest();
	CTaskFishLocomotion* pTaskFishLocomotion = pTaskLocomotion && pTaskLocomotion->GetTaskType() == CTaskTypes::TASK_ON_FOOT_FISH ? static_cast<CTaskFishLocomotion*>(pTaskLocomotion) : NULL;

	if (!pTaskFishLocomotion)
	{
		return;
	}

	CControl* pControl = playerPed.GetControlFromPlayer();
	Assert(pControl);
	const float fStickLeftRight = -pControl->GetPedWalkLeftRight().GetNorm();
	float fPitch = pControl->GetPedWalkUpDown().GetNorm();

	bool bMovingStick = fabs(fStickLeftRight) > 0.0f || fabs(fPitch) > 0.0f;


	// Speed
	float fMBR = MOVEBLENDRATIO_STILL;

	// No idle turns.
	if (bMovingStick)
	{
		fMBR = MOVEBLENDRATIO_WALK;
	}

	// If holding sprint button, swim fast.
	float fSprintResult = playerPed.GetPlayerInfo()->ControlButtonSprint(CPlayerInfo::SPRINT_UNDER_WATER, true);
	if (fSprintResult > 0.0f)
	{
		fMBR = MOVEBLENDRATIO_RUN;
	}

	// Store off the MBR.
	m_fMoveBlendRatio = fMBR;

	// Reset the gaitless rate boost.
	pTaskFishLocomotion->SetUseGaitlessRateBoostThisFrame(false);

	// Clamp their movement to be species appropriate.
	// Small fish don't look good when swimming fast.
	if (playerPed.GetTaskData().GetIsFlagSet(CTaskFlags::ForceSlowSwimWhenUnderPlayerControl) && m_fMoveBlendRatio > MOVEBLENDRATIO_WALK)
	{
		m_fMoveBlendRatio = Min(MOVEBLENDRATIO_WALK, m_fMoveBlendRatio);
		pTaskFishLocomotion->SetUseGaitlessRateBoostThisFrame(true);
	}

	playerPed.GetMotionData()->SetDesiredMoveBlendRatio(m_fMoveBlendRatio, 0.0f);

	// Orientation
	// get the orientation of the FIRST follow ped camera, for player controls
	float fCamOrient = camInterface::GetPlayerControlCamHeading(playerPed);
	float fCamPitch = camInterface::GetPlayerControlCamPitch(playerPed);		
	if (pControl->GetPedLookUpDown().GetNorm() == 0)
	{
		fCamPitch = 0;
	}

	if (camInterface::GetDebugDirector().IsFreeCamActive())
	{
		fCamOrient = playerPed.GetTransform().GetHeading();
	}

	// Pitch
	fPitch = Clamp(fPitch, -1.0f, 1.0f) * CTaskMotionDiving::ms_fMaxPitch;
	fPitch = fwAngle::LimitRadianAngle(fPitch + fCamPitch);

	// Limit the minimum that a fish's pitch can be based on their depth so they don't go under the world.
	float fMinPitch = GetFishMinPitch(playerPed);

	// If out of the water limit pitching upwards so we don't pop up and down while skimming it.
	float fMaxPitch = GetFishMaxPitch(playerPed);
	
	if (fMaxPitch < CTaskMotionDiving::ms_fMaxPitch && m_fMoveBlendRatio > MOVEBLENDRATIO_WALK)
	{
		pTaskFishLocomotion->SetForceFastPitching(true);
	}
	else if (fMinPitch > -CTaskMotionDiving::ms_fMaxPitch && m_fMoveBlendRatio > MOVEBLENDRATIO_WALK)
	{
		pTaskFishLocomotion->SetForceFastPitching(true);
	}
	else
	{
		pTaskFishLocomotion->SetForceFastPitching(false);
	}

	fPitch = Clamp(fPitch, fMinPitch, fMaxPitch);

	playerPed.SetDesiredPitch(fPitch);

	// Desired heading
	static const float fMaxTurn = HALF_PI * 0.9f;

	float fLeftRightVal = Clamp(fStickLeftRight, -1.0f, 1.0f);
	fLeftRightVal *= fLeftRightVal;
	fLeftRightVal *= fLeftRightVal;
	fLeftRightVal *= Sign(fStickLeftRight);
	fLeftRightVal *= fMaxTurn;

	float fHdg = fwAngle::LimitRadianAngle(fLeftRightVal + fCamOrient);
	fHdg = fwAngle::LimitRadianAngle(fLeftRightVal + playerPed.GetCurrentHeading());

	playerPed.SetDesiredHeading(fHdg);
}

float CTaskMovePlayer::GetFishMaxPitch(const CPed& playerPed)
{
	float fWaterZ = 0.0f;
	Vec3V vPlayer = playerPed.GetTransform().GetPosition();
	if (!CWaterTestHelper::GetWaterHeightAtPositionIncludingRivers(VEC3V_TO_VECTOR3(vPlayer), &fWaterZ, false))
	{
		// No water at all, so this is clearly bad.
		return 0.0f;
	}

	// Too close to the surface - tend towards flat.
	float fSurfaceSwimmingOffset = playerPed.GetTaskData().GetSurfaceSwimmingDepthOffset();
	if (fWaterZ - vPlayer.GetZf() <= fSurfaceSwimmingOffset)
	{
		// Can't pitch up at all.
		return 0.0f;
	}
	else if (fWaterZ - vPlayer.GetZf() <= fSurfaceSwimmingOffset * 3.0f) // magic
	{

		// Can only pitch up a little bit.
		return 5.0f * DtoR;
	}

	// Can pitch to your heart's content.
	return CTaskMotionDiving::ms_fMaxPitch;
}

float CTaskMovePlayer::GetFishMinPitch(const CPed& playerPed)
{
	// We are nearing the depth where the map is going to teleport us back on land.
	if (playerPed.GetTransform().GetPosition().GetZf() < ms_fMinDepthForPlayerFish)
	{
		return 0.0f;
	}

	// Fine to pitch down as much as you want.
	return -CTaskMotionDiving::ms_fMaxPitch;
}

#if FPS_MODE_SUPPORTED
bool CTaskMovePlayer::ShouldBeRunningDueToAnalogRun(const CPed* pPlayerPed, const float fCurrentMBR, const Vector2& /*vecStickInput*/) const
{
	bool bUsingKeyboardAndMouse = false;
#if KEYBOARD_MOUSE_SUPPORT
	// B*2312353: PC - Don't auto run; allow player to toggle between walk/run in FPS mode when using keyboard/mouse.
	if (pPlayerPed->GetControlFromPlayer() && pPlayerPed->GetControlFromPlayer()->WasKeyboardMouseLastKnownSource())
	{
		bUsingKeyboardAndMouse = true;
	}
#endif	// KEYBOARD_MOUSE_SUPPORT

	// In first person shooter camera, check for analog stick run, if we are currently not running
	if( fCurrentMBR > 0.f && fCurrentMBR < ms_fRunBlend && pPlayerPed->GetMotionData()->GetUsingAnalogStickRun() && !bUsingKeyboardAndMouse)
	{
// 		TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, s_fStickRunThreshold, 0.7f, 0.f, 1.f, 0.01f);
// 		float x = vecStickInput.x;
// 		float y = vecStickInput.y;
// 		float fFpsMoveRatio = rage::ioAddRoundDeadZone(x, y, ioValue::DEFAULT_DEAD_ZONE_VALUE);
// 		Assertf(fFpsMoveRatio <= 1.0f, "ioAddRoundDeadZone saturate failed: %f", fFpsMoveRatio);
// 
// 		if( fFpsMoveRatio > s_fStickRunThreshold )
		{
			return true;
		}
	}

	return false;
}
#endif // FPS_MODE_SUPPORTED

#if FPS_MODE_SUPPORTED
bool CTaskMovePlayer::ShouldCheckRunInsteadOfSprint(CPed* pPlayerPed, float fMoveBlendRatio) const
{
	const CPedMotionData* pMotionData = pPlayerPed->GetMotionData();
	if(pMotionData->GetUsingStealth())
	{
		return false;
	}

	if(!pMotionData->GetUsingAnalogStickRun() || (CPedMotionData::GetIsSprinting(fMoveBlendRatio) && pMotionData->GetScriptedMaxMoveBlendRatio() < MOVEBLENDRATIO_SPRINT))
	{
		return true;
	}

	CPlayerInfo* pPlayerInfo = pPlayerPed->GetPlayerInfo();
	if(pPlayerInfo->GetPlayerDataPlayerSprintDisabled() && pPlayerInfo->ProcessCanSprint(*pPlayerPed, false))
	{
		if(pPlayerInfo->ProcessSprintControl(*pPlayerPed->GetControlFromPlayer(), *pPlayerPed, CPlayerInfo::SPRINT_ON_FOOT, true) > 1.f)
		{
			return true;
		}
	}

	return false;
}
#endif // FPS_MODE_SUPPORTED

void CTaskMovePlayer::LockStickHeading(bool bUseCurrentCamera)
{
	CPed* pPlayerPed = GetPed();
	CControl *pControl = pPlayerPed->GetControlFromPlayer();
	Vector2 vecStickInput;
	vecStickInput.x = pControl->GetPedWalkLeftRight().GetNorm();
	// y stick results are positive for down (pulling back) and negative for up, so reverse to match world y direction
	vecStickInput.y = -pControl->GetPedWalkUpDown().GetNorm();
	float fCamOrient = camInterface::GetPlayerControlCamHeading(*pPlayerPed);
	float fStickAngle = rage::Atan2f(-vecStickInput.x, vecStickInput.y);
	fStickAngle = fStickAngle + fCamOrient;
	fStickAngle = fwAngle::LimitRadianAngle(fStickAngle);
	m_fLockedStickHeading = fStickAngle;
	m_fLockedHeading = bUseCurrentCamera ? fCamOrient : m_fLockedStickHeading;
}

CTaskMoveInAir::CTaskMoveInAir() :
	CTaskMove(MOVEBLENDRATIO_STILL),
	m_bWantsToUseParachute(false)
{
	SetInternalTaskType(CTaskTypes::TASK_MOVE_IN_AIR);
}

CTask::FSM_Return CTaskMoveInAir::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(Running)
			FSM_OnUpdate
				return StateRunning_OnUpdate(pPed);
	FSM_End
}

CTask::FSM_Return CTaskMoveInAir::StateRunning_OnUpdate(CPed* pPed)
{
	if(pPed->IsLocalPlayer())
	{
		ProcessPlayerInput(pPed);
	}
	else if(!pPed->GetPedResetFlag( CPED_RESET_FLAG_IsClimbing))
	{
		ProcessAiInput(pPed);
	}

	return FSM_Continue;
}


// This just checks to see if there is a goto task sequenced after this.
// If so then the goto target of this task is used to apply a desired
// heading & moveblendratio to the ped.
void CTaskMoveInAir::ProcessAiInput(CPed* pPed)
{
	CPedIntelligence * pPedAi = pPed->GetPedIntelligence();
	CTask * pActiveSeqTask = pPedAi->FindTaskActiveByType(CTaskTypes::TASK_USE_SEQUENCE);
	if(!pActiveSeqTask)
		return;

	CTaskUseSequence * pTaskUseSequence = (CTaskUseSequence*)pActiveSeqTask;
	Assert(pTaskUseSequence->GetSequenceID()!=-1);
	CTaskSequenceList* pTaskSequence = (CTaskSequenceList*) &CTaskSequences::ms_TaskSequenceLists[pTaskUseSequence->GetSequenceID()];

	// Check that the task at the current position in the sequence is a navmesh route task
	int iProgress = pTaskUseSequence->GetPrimarySequenceProgress();
	const aiTask * pSeqTask = pTaskSequence->GetTask(iProgress);

	if(pSeqTask->GetTaskType() != CTaskTypes::TASK_JUMPVAULT)
		return;

	if(iProgress+1 < CTaskList::MAX_LIST_SIZE)
	{
		const aiTask * pTask = pTaskSequence->GetTask(iProgress+1);

		if(pTask && pTask->GetTaskType()==CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
		{
			CTaskTypes::eTaskType iNextTaskType = (CTaskTypes::eTaskType) ((CTaskComplexControlMovement*)pTask)->GetMoveTaskType();
			if(iNextTaskType == CTaskTypes::TASK_MOVE_GO_TO_POINT || iNextTaskType == CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL)
			{
				const Vector3 vTarget = ((CTaskComplexControlMovement*)pTask)->GetTarget();
				const float fMBR = ((CTaskComplexControlMovement*)pTask)->GetMoveBlendRatio();

				pPed->SetDesiredHeading(vTarget);
				pPed->GetMotionData()->SetDesiredMoveBlendRatio(fMBR, 0.0f);
				// Prevent the possibility of CTaskSimpleMoveDoNothing resetting this to zero this frame
				pPed->SetPedResetFlag( CPED_RESET_FLAG_DontChangeMbrInSimpleMoveDoNothing, true );
			}
		}
	}
}

void CTaskMoveInAir::ProcessPlayerInput(CPed* pPed)
{
	if (pPed->GetPedResetFlag( CPED_RESET_FLAG_IsLanding ))
	{
		pPed->GetPlayerInfo()->ControlButtonDuck();
	}
}

///////////////////////////////////////////////////////////////////////////////

// eof
