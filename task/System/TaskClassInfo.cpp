// File header
#include "Task/System/TaskClassInfo.h"

// Game headers
#include "Animation/AnimDefines.h"
#include "Event/Events.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Peds/QueriableInterface.h"
#include "Task/Animation/TaskCutScene.h"
#include "Task/Animation/TaskMoVEScripted.h"
#include "Task/Animation/TaskScriptedAnimation.h"
#include "Task/Animation/TaskReachArm.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Combat/Cover/TaskSeekCover.h"
#include "Task/Combat/Cover/TaskStayInCover.h"
#include "Task/Combat/SubTasks/TaskAdvance.h"
#include "Task/Combat/SubTasks/TaskCharge.h"
#include "Task/Combat/SubTasks/TaskAimFromGround.h"
#include "Task/Combat/SubTasks/TaskBoatChase.h"
#include "Task/Combat/SubTasks/TaskBoatCombat.h"
#include "Task/Combat/SubTasks/TaskBoatStrafe.h"
#include "Task/Combat/Subtasks/TaskDraggedToSafety.h"
#include "Task/Combat/Subtasks/TaskDraggingToSafety.h"
#include "Task/Combat/SubTasks/TaskHeliChase.h"
#include "Task/Combat/SubTasks/TaskHeliCombat.h"
#include "Task/Combat/SubTasks/TaskPlaneChase.h"
#include "Task/Combat/SubTasks/TaskReactToBuddyShot.h"
#include "task/Combat/Subtasks/TaskSearchInAutomobile.h"
#include "task/Combat/Subtasks/TaskSearchInBoat.h"
#include "task/Combat/Subtasks/TaskSearchInHeli.h"
#include "task/Combat/Subtasks/TaskSearchOnFoot.h"
#include "Task/Combat/Subtasks/TaskShellShocked.h"
#include "Task/Combat/Subtasks/TaskShootAtTarget.h"
#include "Task/Combat/Subtasks/TaskShootOutTire.h"
#include "Task/Combat/Subtasks/TaskStealth.h"
#include "Task/Combat/Subtasks/TaskSubmarineChase.h"
#include "Task/Combat/Subtasks/TaskSubmarineCombat.h"
#include "Task/Combat/Subtasks/TaskTargetUnreachable.h"
#include "Task/Combat/Subtasks/TaskVariedAimPose.h"
#include "Task/Combat/Subtasks/TaskVehicleChase.h"
#include "Task/Combat/Subtasks/TaskVehicleCombat.h"
#include "Task/Combat/TaskAnimatedHitByExplosion.h"
#include "Task/Combat/TaskCombat.h"
#include "Task/Combat/TaskCombatMelee.h"
#include "Task/Combat/TaskCombatMounted.h"
#include "Task/Combat/TaskDamageDeath.h"
#include "Task/Combat/TaskInvestigate.h"
#include "Task/Combat/TaskNewCombat.h"
#include "Task/Combat/TaskReact.h"
#include "Task/Combat/TaskSearch.h"
#include "Task/Combat/TaskToHurtTransit.h"
#include "Task/Combat/TaskWrithe.h"
#include "Task/Crimes/RandomEventManager.h"
#include "Task/Crimes/TaskPursueCriminal.h"
#include "Task/Crimes/TaskReactToPursuit.h"
#include "Task/Combat/TaskSharkAttack.h"
#include "Task/Crimes/TaskStealVehicle.h"
#include "Task/Combat/TaskThreatResponse.h"
#include "Task/Debug/TaskDebug.h"
#include "Task/Default/Patrol/TaskPatrol.h"
#include "Task/Default/TaskArrest.h"
#include "Task/Default/TaskChat.h"
#include "Task/Default/TaskCuffed.h"
#include "Task/Default/TaskFlyingWander.h"
#include "Task/Default/TaskIncapacitated.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Default/TaskSwimmingWander.h"
#include "Task/Default/TaskWander.h"
#include "Task/Default/TaskSlopeScramble.h"
#include "Task/Default/TaskUnalerted.h"
#include "Task/General/TaskBasic.h"
#include "Task/General/TaskBasic.h"
#include "Task/General/TaskSecondary.h"
#include "Task/General/TaskGeneralSweep.h"
#include "Task/General/TaskVehicleSecondary.h"
#include "Task/General/Phone/TaskCallPolice.h"
#include "Task/General/Phone/TaskMobilePhone.h"
#include "Task/Helpers/TaskConversationHelper.h"
#include "Task/Motion/Locomotion/TaskBirdLocomotion.h"
#include "Task/Motion/Locomotion/TaskCombatRoll.h"
#include "Task/Motion/Locomotion/TaskFishLocomotion.h"
#include "Task/Motion/Locomotion/TaskFlightlessBirdLocomotion.h"
#include "Task/Motion/Locomotion/TaskHorseLocomotion.h"
#include "Task/Motion/Locomotion/TaskHumanLocomotion.h"
#include "Task/motion/Locomotion/TaskInWater.h"
#include "Task/Motion/Locomotion/TaskLocomotion.h"
#include "Task/Motion/Locomotion/TaskMotionAnimal.h"
#include "Task/Motion/Locomotion/TaskMotionAiming.h"
#include "Task/Motion/Locomotion/TaskMotionAimingTransition.h"
#include "Task/Motion/Locomotion/TaskMotionParachuting.h"
#include "Task/Motion/Locomotion/TaskMotionJetpack.h"
#include "Task/Motion/Locomotion/TaskMotionDrunk.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Motion/Locomotion/TaskMotionStrafing.h"
#include "Task/Motion/Locomotion/TaskMotionTennis.h"
#include "Task/Motion/Locomotion/TaskQuadLocomotion.h"
#include "Task/Motion/Locomotion/TaskRepositionMove.h"
#include "Task/Motion/Vehicle/TaskMotionInTurret.h"
#include "Task/Movement/Climbing/TaskGoToAndClimbLadder.h"
#include "Task/Movement/Climbing/TaskLadderClimb.h"
#include "Task/Movement/Climbing/TaskRappel.h"
#include "Task/Movement/Climbing/TaskVault.h"
#include "Task/Movement/Climbing/TaskDropDown.h"
#include "Task/Movement/Jumping/TaskFallGetUp.h"
#include "Task/Movement/Jumping/TaskInAir.h"
#include "Task/Movement/Jumping/TaskJump.h"
#include "Task/Movement/TaskAnimatedFallback.h"
#include "Task/Movement/TaskCrawl.h"
#include "Task/Movement/TaskFall.h"
#include "Task/movement/TaskGetUp.h"
#include "Task/Movement/TaskCollisionResponse.h"
#include "Task/Movement/TaskFlyToPoint.h"
#include "Task/Movement/TaskFollowWaypointRecording.h"
#include "Task/Movement/TaskGoto.h"
#include "Task/Movement/TaskGoToPointAiming.h"
#include "Task/Movement/TaskMoveFollowEntityOffset.h"
#include "Task/Movement/TaskMoveFollowLeader.h"
#include "Task/Movement/TaskMoveToTacticalPoint.h"
#include "Task/Movement/TaskMoveWander.h"
#include "Task/Movement/TaskMoveWithinAttackWindow.h"
#include "Task/Movement/TaskMoveWithinDefensiveArea.h"
#include "Task/Movement/TaskParachute.h"
#include "Task/Movement/TaskParachuteObject.h"
#include "Task/Movement/TaskJetpack.h"
#include "Task/Movement/TaskSeekEntity.h"
#include "Task/Movement/TaskSlideToCoord.h"
#include "Task/Movement/TaskGenericMoveToPoint.h"
#include "Task/Movement/TaskTakeOffPedVariation.h"
#include "Task/Physics/AttachTasks/TaskNMGenericAttach.h"
#include "Task/Physics/TaskAnimatedAttach.h"
#include "Task/Physics/TaskBlendFromNM.h"
#include "Task/Physics/TaskNMBalance.h"
#include "Task/Physics/TaskNMBindPose.h"
#include "Task/Physics/TaskNMBrace.h"
#include "Task/Physics/TaskNMBuoyancy.h"
#include "Task/Physics/TaskNMDangle.h"
#include "Task/Physics/TaskNMDraggingToSafety.h"
#include "Task/Physics/TaskNMDrunk.h"
#include "Task/Physics/TaskNMElectrocute.h"
#include "Task/Physics/TaskNMExplosion.h"
#include "Task/Physics/TaskNMFallDown.h"
#include "Task/Physics/TaskNMFlinch.h"
#include "Task/Physics/TaskNMHighFall.h"
#include "Task/Physics/TaskNMInjuredOnGround.h"
#include "Task/Physics/TaskNMJumpRollFromRoadVehicle.h"
#include "Task/Physics/TaskNMOnFire.h"
#include "Task/Physics/TaskNMPrototype.h"
#include "Task/Physics/TaskNMRelax.h"
#include "Task/Physics/TaskNMRiverRapids.h"
#include "Task/Physics/TaskNMRollUpAndRelax.h"
#include "Task/Physics/TaskNMScriptControl.h"
#include "Task/Physics/TaskNMShot.h"
#include "Task/Physics/TaskNMSimple.h"
#include "Task/Physics/TaskNMSit.h"
#include "Task/Physics/TaskNMSlungOverShoulder.h"
#include "Task/Physics/TaskNMThroughWindscreen.h"
#include "Task/Physics/TaskRageRagdoll.h"
#include "Task/Response/TaskAgitated.h"
#include "Task/Response/TaskConfront.h"
#include "Task/Response/TaskDiveToGround.h"
#include "Task/Response/TaskFlee.h"
#include "Task/Response/TaskGangs.h"
#include "Task/Response/TaskGrowlAndFlee.h"
#include "Task/Response/TaskIntimidate.h"
#include "Task/Response/TaskReactToDeadPed.h"
#include "Task/Response/TaskShockingEvents.h"
#include "Task/Response/TaskShove.h"
#include "Task/Response/TaskShoved.h"
#include "Task/Response/TaskDuckAndCover.h"
#include "Task/Response/TaskReactAndFlee.h"
#include "Task/Response/TaskReactInDirection.h"
#include "Task/Response/TaskReactToExplosion.h"
#include "Task/Response/TaskReactToImminentExplosion.h"
#include "Task/Response/TaskSidestep.h"
#include "Task/Scenario/TaskScenarioAnimDebug.h"
#include "Task/Scenario/Types/TaskChatScenario.h"
#include "Task/Scenario/Types/TaskCoupleScenario.h"
#include "Task/Scenario/Types/TaskCowerScenario.h"
#include "Task/Scenario/Types/TaskDeadBodyScenario.h"
#include "Task/Scenario/Types/TaskMoveBetweenPointsScenario.h"
#include "Task/Scenario/Types/TaskParkedVehicleScenario.h"
#include "Task/Scenario/Types/TaskUseScenario.h"
#include "Task/Scenario/Types/TaskVehicleScenario.h"
#include "Task/Scenario/Types/TaskWanderingScenario.h"
#include "Task/Scenario/Types/TaskWanderingInRadiusScenario.h"
#include "Task/Service/Army/TaskArmy.h"
#include "Task/Service/Fire/TaskFirePatrol.h"
#include "Task/Service/Medic/TaskAmbulancePatrol.h"
#include "Task/Service/Police/TaskPolicePatrol.h"
#include "Task/Service/Swat/TaskSwat.h"
#include "Task/Service/Witness/TaskWitness.h"
#include "Task/System/Task.h"
#include "Task/System/TaskManager.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskCarAccessories.h"
#include "Task/Vehicle/TaskCarCollisionResponse.h"
#include "Task/Vehicle/TaskCarUtils.h"

#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Task/Vehicle/TaskGoToVehicleDoor.h"
#include "Task/Vehicle/TaskInVehicle.h"
#include "Task/Vehicle/TaskMountAnimal.h"
#include "Task/Vehicle/TaskMountAnimalWeapon.h"
#include "Task/Vehicle/TaskPlayerOnHorse.h"
#include "Task/Vehicle/TaskRideHorse.h"
#include "Task/Vehicle/TaskDraftHorse.h"
#include "Task/Vehicle/TaskRideTrain.h"
#include "Task/Vehicle/TaskTryToGrabVehicleDoor.h"
#include "Task/Vehicle/TaskVehicleWeapon.h"
#include "Task/Vehicle/TaskPlayerHorseFollow.h"
#include "Task/Vehicle/TaskMountFollowNearestRoad.h"
#include "Task/Weapons/Gun/TaskAimGunOnFoot.h"
#include "Task/Weapons/Gun/TaskAimGunScripted.h"
#include "Task/Weapons/Gun/TaskGun.h"
#include "Task/Weapons/Gun/TaskReloadGun.h"
#include "Task/Weapons/Gun/TaskVehicleDriveBy.h"
#include "Task/Weapons/TaskBomb.h"
#include "Task/Weapons/TaskDetonator.h"
#include "Task/Weapons/TaskPlayerWeapon.h"
#include "Task/Weapons/TaskProjectile.h"
#include "Task/Weapons/TaskSwapWeapon.h"
#include "Task/Weapons/TaskWeapon.h"
#include "Task/Weapons/TaskWeaponBlocked.h"
#include "VehicleAI/Task/TaskVehicleAnimation.h"
#include "VehicleAI/Task/TaskVehicleApproach.h"
#include "VehicleAI/Task/TaskVehicleAttack.h"
#include "VehicleAI/Task/TaskVehicleBlock.h"
#include "VehicleAI/Task/TaskVehicleCircle.h"
#include "VehicleAI/Task/TaskVehicleCruise.h"
#include "VehicleAI/Task/TaskVehicleCruiseBoat.h"
#include "VehicleAI/Task/TaskVehicleDeadDriver.h"
#include "VehicleAI/Task/TaskVehicleEscort.h"
#include "VehicleAI/Task/TaskVehicleFlying.h"
#include "VehicleAI/Task/TaskVehicleFleeBoat.h"
#include "VehicleAI/Task/TaskVehicleFollowRecording.h"
#include "VehicleAI/Task/TaskVehicleGoTo.h"
#include "VehicleAI/Task/TaskVehicleGoToAutomobile.h"
#include "VehicleAI/Task/TaskVehicleGoToBoat.h"
#include "VehicleAI/Task/TaskVehicleGoToHelicopter.h"
#include "VehicleAI/Task/TaskVehicleGoToPlane.h"
#include "VehicleAI/Task/TaskVehicleGoToSubmarine.h"
#include "VehicleAI/Task/TaskVehicleHeliProtect.h"
#include "VehicleAI/Task/TaskVehiclePlaneChase.h"
#include "VehicleAI/Task/TaskVehicleLandPlane.h"
#include "VehicleAI/Task/TaskVehiclePark.h"
#include "VehicleAI/Task/TaskVehiclePlayer.h"
#include "VehicleAI/Task/TaskVehiclePoliceBehaviour.h"
#include "vehicleAi/task/TaskVehiclePullAlongside.h"
#include "VehicleAI/Task/TaskVehiclePursue.h"
#include "VehicleAI/Task/TaskVehicleRam.h"
#include "VehicleAI/Task/TaskVehicleShotTire.h"
#include "VehicleAI/Task/TaskVehicleSpinOut.h"
#include "VehicleAI/Task/TaskVehicleTempAction.h"
#include "VehicleAI/Task/TaskVehicleThreePointTurn.h"
#include "VehicleAI/Task/TaskVehicleGoToLongRange.h"
#include "VehicleAI/VehMission.h"

// This should be a good place to include the task metadata headers, since
// we already include headers for all the tasks above.
#include "Task/Animation/Metadata_parser.h"
#include "Task/Combat/Metadata_parser.h"
#include "Task/Crimes/Metadata_parser.h"
#include "Task/Default/Metadata_parser.h"
#include "Task/General/Metadata_parser.h"
#include "Task/Motion/Metadata_parser.h"
#include "Task/Movement/Metadata_parser.h"
#include "Task/Physics/Metadata_parser.h"
#include "Task/Response/Metadata_parser.h"
#include "Task/Scenario/Metadata_parser.h"
#include "Task/Service/Metadata_parser.h"
#include "Task/Vehicle/Metadata_parser.h"
#include "Task/Weapons/Metadata_parser.h"
#include "VehicleAI/Task/Metadata_parser.h"

AI_OPTIMISATIONS()


#define T2N(a, b)																			\
{																						\
	NOTFINAL_ONLY(m_TaskClassInfo[CTaskTypes::a].m_pName = #a);							\
	m_TaskClassInfo[CTaskTypes::a].m_iTaskSize = sizeof(b);								\
	NOTFINAL_ONLY(m_TaskClassInfo[CTaskTypes::a].m_StaticStateNameFunction = &b::GetStaticStateName;)\
}


////////////////////////////////////////////////////////////////////////////////

CTaskClassInfoManager::CTaskClassInfoManager()
{
	Init();
}

////////////////////////////////////////////////////////////////////////////////

s32	CTaskClassInfoManager::GetLargestTaskSize() const
{
	s32 iLargestTaskIndex = GetLargestTaskIndex();
#if __ASSERT
#if __PS3
	static const s32 TASK_SIZE_TOO_LARGE = 416;
#elif __XENON
	static const s32 TASK_SIZE_TOO_LARGE = 432;
#else
	static const s32 TASK_SIZE_TOO_LARGE = 736;
#endif // __PS3
	if(m_TaskClassInfo[iLargestTaskIndex].m_iTaskSize > TASK_SIZE_TOO_LARGE)
	{
		PrintOnlyLargeTaskNamesAndSizes();
		taskFatalAssertf(0, "Task [%s] is large (%" SIZETFMT "u > %d) - look at optimisation", m_TaskClassInfo[iLargestTaskIndex].m_pName, m_TaskClassInfo[iLargestTaskIndex].m_iTaskSize, TASK_SIZE_TOO_LARGE);
	}
#endif // __ASSERT
	return (s32) m_TaskClassInfo[iLargestTaskIndex].m_iTaskSize;
}

////////////////////////////////////////////////////////////////////////////////

#if !__NO_OUTPUT
bool CTaskClassInfoManager::TaskNameExists( s32 iTaskType ) const
{
	if( iTaskType < 0 || iTaskType >= CTaskTypes::MAX_NUM_TASK_TYPES )
		return false;

	if( m_TaskClassInfo[iTaskType].m_pName )
	{
		return true;
	}

	return false;
}
#endif // !__NO_OUTPUT

////////////////////////////////////////////////////////////////////////////////

#if !__NO_OUTPUT
const char *CTaskClassInfoManager::GetTaskName( s32 iTaskType ) const
{
	if( iTaskType < 0 || iTaskType >= CTaskTypes::MAX_NUM_TASK_TYPES )
		return "invalid task";

	if( m_TaskClassInfo[iTaskType].m_pName )
	{
		return m_TaskClassInfo[iTaskType].m_pName;
	}

	return "No task name";
}

const char *CTaskClassInfoManager::GetTaskStateName( s32 iTaskType, s32 iState ) const
{
    if( iTaskType < 0 || iTaskType >= CTaskTypes::MAX_NUM_TASK_TYPES )
		return "Invalid task";

    if(iState < 0)
        return "Invalid state";

	if( m_TaskClassInfo[iTaskType].m_StaticStateNameFunction )
	{
		return (*m_TaskClassInfo[iTaskType].m_StaticStateNameFunction)(iState);
	}

	return "No state name";
}
#endif // !__NO_OUTPUT

////////////////////////////////////////////////////////////////////////////////

void CTaskClassInfoManager::Init(unsigned UNUSED_PARAM(initMode))
{
	INIT_TASKCLASSINFOMGR;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskClassInfoManager::Shutdown(unsigned UNUSED_PARAM(shutdownMode))
{
	SHUTDOWN_TASKCLASSINFOMGR;
}

////////////////////////////////////////////////////////////////////////////////

#if !__NO_OUTPUT
void CTaskClassInfoManager::PrintTaskNamesAndSizes()
{
	PrintTaskNamesAndSizes(false);
}
#endif // !__NO_OUTPUT

////////////////////////////////////////////////////////////////////////////////

#if !__NO_OUTPUT
void CTaskClassInfoManager::PrintOnlyLargeTaskNamesAndSizes()
{
	PrintTaskNamesAndSizes(true);
}
#endif // !__NO_OUTPUT

////////////////////////////////////////////////////////////////////////////////

#if !__NO_OUTPUT
void CTaskClassInfoManager::PrintTaskNamesAndSizes(const bool bPrintOnlyLargeTasks)
{
	for( s32 i = 0; i < CTaskTypes::MAX_NUM_TASK_TYPES; i++ )
	{
		if( TASKCLASSINFOMGR.m_TaskClassInfo[i].m_pName )
		{
			if (TASKCLASSINFOMGR.m_TaskClassInfo[i].m_iTaskSize >= 380)
				taskErrorf("%s, %" SIZETFMT "u", TASKCLASSINFOMGR.m_TaskClassInfo[i].m_pName, TASKCLASSINFOMGR.m_TaskClassInfo[i].m_iTaskSize);
			else if(!bPrintOnlyLargeTasks)
				taskDisplayf("%s, %" SIZETFMT "u", TASKCLASSINFOMGR.m_TaskClassInfo[i].m_pName, TASKCLASSINFOMGR.m_TaskClassInfo[i].m_iTaskSize);
		}
	}

	// Not needed anymore
	s32 iLargestTaskIndex = TASKCLASSINFOMGR.GetLargestTaskIndex();
	taskDisplayf("Largest task is %s, %" SIZETFMT "u", TASKCLASSINFOMGR.m_TaskClassInfo[iLargestTaskIndex].m_pName, TASKCLASSINFOMGR.m_TaskClassInfo[iLargestTaskIndex].m_iTaskSize);
}
#endif // !__NO_OUTPUT

////////////////////////////////////////////////////////////////////////////////

void CTaskClassInfoManager::Init()
{
	T2N(TASK_MOVE_PLAYER, CTaskMovePlayer);
	T2N(TASK_PLAYER_ON_FOOT, CTaskPlayerOnFoot);
	T2N(TASK_WEAPON, CTaskWeapon);
	T2N(TASK_PLAYER_WEAPON, CTaskPlayerWeapon);
	T2N(TASK_PLAYER_IDLES, CTaskPlayerIdles);
	T2N(TASK_UNINTERRUPTABLE, CTaskUninterruptable);
	T2N(TASK_PAUSE, CTaskPause);
	T2N(TASK_DO_NOTHING, CTaskDoNothing);
	T2N(TASK_GET_UP, CTaskGetUp);
	T2N(TASK_GET_UP_AND_STAND_STILL, CTaskGetUpAndStandStill);
	T2N(TASK_FALL_OVER, CTaskFallOver);
	T2N(TASK_FALL_AND_GET_UP, CTaskFallAndGetUp);
	T2N(TASK_CRAWL, CTaskCrawl);
	T2N(TASK_HIT_RESPONSE, CTaskHitResponse);
	T2N(TASK_USE_SEQUENCE, CTaskUseSequence);
	T2N(TASK_COMPLEX_ON_FIRE, CTaskComplexOnFire);
	T2N(TASK_DAMAGE_ELECTRIC, CTaskDamageElectric);
	T2N(TASK_RAPPEL, CTaskRappel);
	T2N(TASK_VAULT, CTaskVault);
	T2N(TASK_DROP_DOWN, CTaskDropDown);
	T2N(TASK_JUMPVAULT, CTaskJumpVault);
	T2N(TASK_JUMP, CTaskJump);
	T2N(TASK_TRIGGER_LOOK_AT, CTaskTriggerLookAt);
	T2N(TASK_CLEAR_LOOK_AT, CTaskClearLookAt);
	T2N(TASK_SET_CHAR_DECISION_MAKER, CTaskSetCharDecisionMaker);
	T2N(TASK_SET_PED_DEFENSIVE_AREA, CTaskSetPedDefensiveArea);
	T2N(TASK_SIMPLE_CONTROL_MOVEMENT, CTaskComplexControlMovement);
	T2N(TASK_MOVE_STAND_STILL, CTaskMoveStandStill);
	T2N(TASK_COMPLEX_CONTROL_MOVEMENT, CTaskComplexControlMovement);
	T2N(TASK_COMPLEX_MOVE_SEQUENCE, CTaskMoveSequence);
	T2N(TASK_CLIMB_LADDER, CTaskClimbLadder);
	T2N(TASK_GO_TO_AND_CLIMB_LADDER, CTaskGoToAndClimbLadder);
	T2N(TASK_CLIMB_LADDER_FULLY, CTaskClimbLadderFully);
	T2N(TASK_MOVE_AROUND_COVERPOINTS, CTaskMoveAroundCoverPoints);
	T2N(TASK_AMBIENT_CLIPS, CTaskAmbientClips);
	T2N(TASK_NETWORK_CLONE, CTaskNetworkClone);
	T2N(TASK_AFFECT_SECONDARY_BEHAVIOUR, CTaskAffectSecondaryBehaviour);
	T2N(TASK_AMBIENT_LOOK_AT_EVENT, CTaskAmbientLookAtEvent);
	T2N(TASK_OPEN_DOOR, CTaskOpenDoor);
	T2N(TASK_PARKED_VEHICLE_SCENARIO, CTaskParkedVehicleScenario);
	T2N(TASK_WANDERING_SCENARIO, CTaskWanderingScenario);
	T2N(TASK_WANDERING_IN_RADIUS_SCENARIO, CTaskWanderingInRadiusScenario);
	T2N(TASK_MOVE_BETWEEN_POINTS_SCENARIO, CTaskMoveBetweenPointsScenario);
	T2N(TASK_CHAT_SCENARIO, CTaskChatScenario);
	T2N(TASK_MOBILE_PHONE, CTaskMobilePhone);
	T2N(TASK_RUN_CLIP , CTaskRunClip);
	T2N(TASK_RUN_NAMED_CLIP, CTaskRunNamedClip);
	T2N(TASK_SCRIPTED_ANIMATION, CTaskScriptedAnimation);
	T2N(TASK_SYNCHRONIZED_SCENE, CTaskSynchronizedScene);
	T2N(TASK_REACH_ARM, CTaskReachArm);
	T2N(TASK_HIT_WALL, CTaskHitWall);
	T2N(TASK_COWER, CTaskCower);
	T2N(TASK_HANDS_UP, CTaskHandsUp);
	T2N(TASK_CROUCH, CTaskCrouch);
	T2N(TASK_MELEE, CTaskMelee);
	T2N(TASK_MOVE_MELEE_MOVEMENT, CTaskMoveMeleeMovement);
	T2N(TASK_COMPLEX_EVASIVE_STEP, CTaskComplexEvasiveStep);
	T2N(TASK_MOVE_WANDER_AROUND_VEHICLE, CTaskWalkRoundCarWhileWandering);
	T2N(TASK_WALK_ROUND_FIRE, CTaskWalkRoundFire);
	T2N(TASK_COMPLEX_STUCK_IN_AIR, CTaskComplexStuckInAir);
	T2N(TASK_WALK_ROUND_ENTITY, CTaskWalkRoundEntity);
	T2N(TASK_MOVE_WALK_ROUND_VEHICLE, CTaskMoveWalkRoundVehicle);
	T2N(TASK_MOVE_WALK_ROUND_VEHICLE_DOOR, CTaskWalkRoundVehicleDoor);
	T2N(TASK_REACT_TO_GUN_AIMED_AT, CTaskReactToGunAimedAt);
	T2N(TASK_REACT_TO_EXPLOSION, CTaskReactToExplosion);
	T2N(TASK_REACT_TO_IMMINENT_EXPLOSION, CTaskReactToImminentExplosion);
	T2N(TASK_DIVE_TO_GROUND, CTaskDiveToGround);
	T2N(TASK_REACT_AND_FLEE, CTaskReactAndFlee);
	T2N(TASK_REACT_IN_DIRECTION, CTaskReactInDirection);
	T2N(TASK_SIDESTEP, CTaskSidestep);
	T2N(TASK_CALL_POLICE, CTaskCallPolice);
	T2N(TASK_ON_FOOT_DUCK_AND_COVER, CTaskDuckAndCover);
	T2N(TASK_ON_FOOT_SLOPE_SCRAMBLE, CTaskSlopeScramble);
	T2N(TASK_FALL, CTaskFall);

	T2N(TASK_GENERAL_SWEEP, CTaskGeneralSweep);

	T2N(TASK_AGGRESSIVE_RUBBERNECK, CTaskAggressiveRubberneck);
	T2N(TASK_IN_VEHICLE_BASIC, CTaskInVehicleBasic);
	T2N(TASK_CAR_DRIVE_WANDER, CTaskCarDriveWander);
	T2N(TASK_LEAVE_ANY_CAR, CTaskLeaveAnyCar);
	T2N(TASK_COMPLEX_GET_OFF_BOAT, CTaskComplexGetOffBoat);
	T2N(TASK_CAR_DRIVE_POINT_ROUTE, CTaskDrivePointRoute);
	T2N(TASK_CAR_SET_TEMP_ACTION, CTaskCarSetTempAction);
	T2N(TASK_BRING_VEHICLE_TO_HALT, CTaskBringVehicleToHalt);
	T2N(TASK_CAR_DRIVE, CTaskCarDrive);
	T2N(TASK_ENTER_VEHICLE, CTaskEnterVehicle);
	T2N(TASK_ENTER_VEHICLE_ALIGN, CTaskEnterVehicleAlign);
	T2N(TASK_OPEN_VEHICLE_DOOR_FROM_OUTSIDE, CTaskOpenVehicleDoorFromOutside);
	T2N(TASK_ENTER_VEHICLE_SEAT, CTaskEnterVehicleSeat);
	T2N(TASK_CLOSE_VEHICLE_DOOR_FROM_INSIDE, CTaskCloseVehicleDoorFromInside);
	T2N(TASK_IN_VEHICLE_SEAT_SHUFFLE, CTaskInVehicleSeatShuffle);
	T2N(TASK_EXIT_VEHICLE, CTaskExitVehicle);
	T2N(TASK_OPEN_VEHICLE_DOOR_FROM_INSIDE, CTaskOpenVehicleDoorFromInside);
	T2N(TASK_EXIT_VEHICLE_SEAT, CTaskExitVehicleSeat);
	T2N(TASK_CLOSE_VEHICLE_DOOR_FROM_OUTSIDE, CTaskCloseVehicleDoorFromOutside);
	T2N(TASK_PLAYER_DRIVE, CTaskPlayerDrive);
	T2N(TASK_GO_TO_CAR_DOOR_AND_STAND_STILL , CTaskGoToCarDoorAndStandStill);
	T2N(TASK_TRY_TO_GRAB_VEHICLE_DOOR, CTaskTryToGrabVehicleDoor);
	T2N(TASK_MOVE_GO_TO_VEHICLE_DOOR, CTaskMoveGoToVehicleDoor);
	T2N(TASK_SET_PED_IN_VEHICLE, CTaskSetPedInVehicle);
	T2N(TASK_SET_PED_OUT_OF_VEHICLE, CTaskSetPedOutOfVehicle);
	T2N(TASK_VEHICLE_MOUNTED_WEAPON, CTaskVehicleMountedWeapon);
	T2N(TASK_VEHICLE_GUN, CTaskVehicleGun);
	T2N(TASK_VEHICLE_PROJECTILE, CTaskVehicleProjectile);
	T2N(TASK_VEHICLE_COMBAT, CTaskVehicleCombat);
	T2N(TASK_SMASH_CAR_WINDOW, CTaskSmashCarWindow);
	T2N(TASK_GET_ON_TRAIN, CTaskGetOnTrain)
	T2N(TASK_GET_OFF_TRAIN, CTaskGetOffTrain)
	T2N(TASK_RIDE_TRAIN, CTaskRideTrain)
#if __BANK
	T2N(TASK_ATTACHED_VEHICLE_ANIM_DEBUG, CTaskAttachedVehicleAnimDebug);
#endif // __BANK
	T2N(TASK_MOVE_GO_TO_POINT , CTaskMoveGoToPoint);
	T2N(TASK_MOVE_ACHIEVE_HEADING, CTaskMoveAchieveHeading);
	T2N(TASK_MOVE_FACE_TARGET, CTaskMoveFaceTarget);
	T2N(TASK_MOVE_GO_TO_POINT_AND_STAND_STILL, CTaskMoveGoToPointAndStandStill);
	T2N(TASK_MOVE_FOLLOW_POINT_ROUTE, CTaskMoveFollowPointRoute);
	T2N(TASK_MOVE_SEEK_ENTITY_STANDARD, TTaskMoveSeekEntityStandard);
	T2N(TASK_MOVE_SEEK_ENTITY_LAST_NAV_MESH_INTERSECTION, TTaskMoveSeekEntityLastNavMeshIntersection);
	T2N(TASK_MOVE_SEEK_ENTITY_OFFSET_ROTATE, TTaskMoveSeekEntityXYOffsetRotated);
	T2N(TASK_MOVE_SEEK_ENTITY_OFFSET_FIXED, TTaskMoveSeekEntityXYOffsetFixed);
	T2N(TASK_MOVE_SEEK_ENTITY_RADIUS_ANGLE, TTaskMoveSeekEntityRadiusAngleOffset);
	T2N(TASK_GROWL_AND_FLEE, CTaskGrowlAndFlee);
	T2N(TASK_EXHAUSTED_FLEE, CTaskExhaustedFlee);
	T2N(TASK_SCENARIO_FLEE, CTaskScenarioFlee);
	T2N(TASK_SMART_FLEE, CTaskSmartFlee);
	T2N(TASK_FLY_AWAY, CTaskFlyAway); 
	T2N(TASK_WALK_AWAY, CTaskWalkAway);
	T2N(TASK_WANDER, CTaskWander);
	T2N(TASK_WANDER_IN_AREA, CTaskWanderInArea);
	T2N(TASK_FOLLOW_LEADER_IN_FORMATION, CTaskFollowLeaderInFormation);
	T2N(TASK_GO_TO_POINT_ANY_MEANS, CTaskGoToPointAnyMeans);
	T2N(TASK_COMPLEX_TURN_TO_FACE_ENTITY, CTaskTurnToFaceEntityOrCoord);
	T2N(TASK_FOLLOW_LEADER_ANY_MEANS, CTaskFollowLeaderAnyMeans);
#if ENABLE_HORSE
	T2N(TASK_PLAYER_HORSE_FOLLOW, CTaskPlayerHorseFollow);
#endif
	T2N(TASK_FLY_TO_POINT, CTaskFlyToPoint);
	T2N(TASK_FLYING_WANDER, CTaskFlyingWander);
	T2N(TASK_SWIMMING_WANDER, CTaskSwimmingWander);
	T2N(TASK_GO_TO_POINT_AIMING, CTaskGoToPointAiming);
	T2N(TASK_GO_TO_SCENARIO, CTaskGoToScenario);
	T2N(TASK_GENERIC_MOVE_TO_POINT, CTaskGenericMoveToPoint);
	T2N(TASK_FOLLOW_PATROL_ROUTE, CTaskFollowPatrolRoute);
	T2N(TASK_SEEK_ENTITY_AIMING, CTaskSeekEntityAiming);
	T2N(TASK_SLIDE_TO_COORD, CTaskSlideToCoord);
	T2N(TASK_HELICOPTER_STRAFE, CTaskHelicopterStrafe);
	T2N(TASK_MOVE_TRACKING_ENTITY, CTaskMoveTrackingEntity);
	T2N(TASK_MOVE_FOLLOW_NAVMESH, CTaskMoveFollowNavMesh);
	T2N(TASK_MOVE_GO_TO_POINT_ON_ROUTE, CTaskMoveGoToPointOnRoute);
	T2N(TASK_ESCAPE_BLAST, CTaskEscapeBlast);
	T2N(TASK_MOVE_WANDER, CTaskMoveWander);
	T2N(TASK_MOVE_BE_IN_FORMATION, CTaskMoveBeInFormation);
	T2N(TASK_MOVE_CROWD_AROUND_LOCATION, CTaskMoveCrowdAroundLocation);
	T2N(TASK_MOVE_CROSS_ROAD_AT_TRAFFIC_LIGHTS, CTaskMoveCrossRoadAtTrafficLights);
	T2N(TASK_MOVE_WAIT_FOR_TRAFFIC, CTaskMoveWaitForTraffic);
	T2N(TASK_MOVE_GOTO_POINT_STAND_STILL_ACHIEVE_HEADING, CTaskMoveGoToPointStandStillAchieveHeading);
	T2N(TASK_MOVE_WAIT_FOR_NAVMESH_SPECIAL_ACTION_EVENT, CTaskMoveWaitForNavMeshSpecialActionEvent);
	T2N(TASK_MOVE_GOTO_SAFE_POSITION_ON_NAVMESH, CTasMoveGoToSafePositionOnNavMesh);
	T2N(TASK_MOVE_RETURN_TO_ROUTE, CTaskMoveReturnToRoute);
	T2N(TASK_MOVE_GOTO_SHELTER_AND_WAIT, CTaskMoveGoToShelterAndWait);
	T2N(TASK_MOVE_GET_ONTO_MAIN_NAVMESH, CTaskMoveGetOntoMainNavMesh);
	T2N(TASK_MOVE_SLIDE_TO_COORD, CTaskMoveSlideToCoord);
	T2N(TASK_MOVE_GOTO_POINT_RELATIVE_TO_ENTITY_AND_STAND_STILL, CTaskMoveGoToPointRelativeToEntityAndStandStill);
	T2N(TASK_AIM_SWEEP, CTaskAimSweep);
	T2N(TASK_AIM_AND_THROW_PROJECTILE, CTaskAimAndThrowProjectile);
	T2N(TASK_GUN, CTaskGun);
	T2N(TASK_AIM_GUN_ON_FOOT, CTaskAimGunOnFoot);
	T2N(TASK_AIM_FROM_GROUND, CTaskAimFromGround);
	T2N(TASK_AIM_GUN_VEHICLE_DRIVE_BY, CTaskAimGunVehicleDriveBy);
	T2N(TASK_AIM_GUN_SCRIPTED, CTaskAimGunScripted);
	T2N(TASK_RELOAD_GUN, CTaskReloadGun);
	T2N(TASK_WEAPON_BLOCKED, CTaskWeaponBlocked);
	T2N(TASK_ENTER_COVER, CTaskEnterCover);
	T2N(TASK_EXIT_COVER, CTaskExitCover);
	T2N(TASK_AIM_GUN_FROM_COVER_INTRO, CTaskAimGunFromCoverIntro);
	T2N(TASK_AIM_GUN_FROM_COVER_OUTRO, CTaskAimGunFromCoverOutro);
	T2N(TASK_AIM_GUN_BLIND_FIRE, CTaskAimGunBlindFire);
	T2N(TASK_COVER, CTaskCover);
	T2N(TASK_MOTION_IN_COVER, CTaskMotionInCover);
	T2N(TASK_COMBAT_CLOSEST_TARGET_IN_AREA, CTaskCombatClosestTargetInArea);
	T2N(TASK_CUTSCENE, CTaskCutScene);

	T2N(TASK_HUMAN_LOCOMOTION, CTaskHumanLocomotion);
#if ENABLE_HORSE
	T2N(TASK_ON_FOOT_HORSE, CTaskHorseLocomotion);		
#endif
	T2N(TASK_ON_FOOT_FISH, CTaskFishLocomotion);
	T2N(TASK_ON_FOOT_BIRD, CTaskBirdLocomotion);
	T2N(TASK_ON_FOOT_FLIGHTLESS_BIRD, CTaskFlightlessBirdLocomotion);
	T2N(TASK_ON_FOOT_QUAD, CTaskQuadLocomotion);

	T2N(TASK_ADDITIONAL_COMBAT_TASK, CTaskCombatAdditionalTask);
	T2N(TASK_IN_COVER, CTaskInCover);
	T2N(TASK_AGITATED, CTaskAgitated);
	T2N(TASK_AGITATED_ACTION, CTaskAgitatedAction);
	T2N(TASK_CONFRONT, CTaskConfront);
	T2N(TASK_INTIMIDATE, CTaskIntimidate);
	T2N(TASK_SHOVE, CTaskShove);
	T2N(TASK_SHOVED, CTaskShoved);
//	T2N(TASK_FINISHED, CTaskFinished);
	T2N(TASK_CROUCH_TOGGLE, CTaskCrouchToggle);
	T2N(TASK_REVIVE, CTaskRevive);
	T2N(TASK_COMPLEX_USE_MOBILE_PHONE , CTaskComplexUseMobilePhone);
	
	T2N(TASK_PARACHUTE, CTaskParachute);
	T2N(TASK_PARACHUTE_OBJECT, CTaskParachuteObject);
	T2N(TASK_TAKE_OFF_PED_VARIATION, CTaskTakeOffPedVariation);
#if ENABLE_JETPACK
	T2N(TASK_JETPACK, CTaskJetpack);
#endif
	T2N(TASK_COMBAT_SEEK_COVER, CTaskCombatSeekCover);
	T2N(TASK_COMBAT_CHARGE, CTaskCombatChargeSubtask);
	T2N(TASK_COMBAT_MOUNTED, CTaskCombatMounted);
	T2N(TASK_MOVE_CIRCLE, CTaskMoveCircle);
	T2N(TASK_MOVE_COMBAT_MOUNTED, CTaskMoveCombatMounted);
	T2N(TASK_MOVE_WITHIN_ATTACK_WINDOW, CTaskMoveWithinAttackWindow);
	T2N(TASK_MOVE_WITHIN_DEFENSIVE_AREA, CTaskMoveWithinDefensiveArea);
	T2N(TASK_SHOOT_OUT_TIRE, CTaskShootOutTire);
	T2N(TASK_SHELL_SHOCKED, CTaskShellShocked);
	T2N(TASK_SET_AND_GUARD_AREA, CTaskSetAndGuardArea);
	T2N(TASK_STAND_GUARD, CTaskStandGuard);
	T2N(TASK_SHARK_ATTACK, CTaskSharkAttack);
	T2N(TASK_SHARK_CIRCLE, CTaskSharkCircle);
	T2N(TASK_SEPARATE, CTaskSeparate);
	T2N(TASK_NM_RELAX, CTaskNMRelax);
	T2N(TASK_NM_ROLL_UP_AND_RELAX, CTaskNMRollUpAndRelax);
	T2N(TASK_NM_POSE, CTaskNMPose);
	T2N(TASK_WRITHE, CTaskWrithe);
	T2N(TASK_TO_HURT_TRANSIT, CTaskToHurtTransit);
#if __DEV
	T2N(TASK_NM_BIND_POSE, CTaskNMBindPose);
#endif	//	__DEV
	T2N(TASK_NM_GENERIC_ATTACH, CTaskNMGenericAttach)
#if ENABLE_DRUNK
    T2N(TASK_NM_DRUNK, CTaskNMDrunk)
#endif // ENABLE_DRUNK
    T2N(TASK_NM_DRAGGING_TO_SAFETY, CTaskNMDraggingToSafety)
    T2N(TASK_NM_THROUGH_WINDSCREEN, CTaskNMThroughWindscreen)
	T2N(TASK_NM_BRACE, CTaskNMBrace);
	T2N(TASK_NM_SHOT, CTaskNMShot);
	T2N(TASK_NM_HIGH_FALL, CTaskNMHighFall);
	T2N(TASK_NM_BALANCE, CTaskNMBalance);
	T2N(TASK_NM_ELECTROCUTE, CTaskNMElectrocute);
	T2N(TASK_NM_EXPLOSION, CTaskNMExplosion);
	T2N(TASK_NM_ONFIRE, CTaskNMOnFire);
	T2N(TASK_NM_SCRIPT_CONTROL, CTaskNMScriptControl);
	T2N(TASK_NM_JUMP_ROLL_FROM_ROAD_VEHICLE, CTaskNMJumpRollFromRoadVehicle);
	T2N(TASK_NM_FLINCH, CTaskNMFlinch);
	T2N(TASK_NM_SIT, CTaskNMSit);
	T2N(TASK_NM_FALL_DOWN, CTaskNMFallDown);
	T2N(TASK_BLEND_FROM_NM, CTaskBlendFromNM);
	T2N(TASK_NM_CONTROL, CTaskNMControl);
	T2N(TASK_NM_DANGLE, CTaskNMDangle);
	T2N(TASK_NM_SLUNG_OVER_SHOULDER, CTaskNMSlungOverShoulder);
	T2N(TASK_NM_SIMPLE, CTaskNMSimple);
	T2N(TASK_RAGE_RAGDOLL, CTaskRageRagdoll);
	T2N(TASK_SWAP_WEAPON, CTaskSwapWeapon)
	T2N(TASK_SAY_AUDIO, CTaskSayAudio)
	T2N(TASK_SHOCKING_EVENT_WATCH, CTaskShockingEventWatch)
	T2N(TASK_SHOCKING_EVENT_GOTO, CTaskShockingEventGoto)
	T2N(TASK_SHOCKING_EVENT_HURRYAWAY, CTaskShockingEventHurryAway)
	T2N(TASK_SHOCKING_EVENT_REACT_TO_AIRCRAFT, CTaskShockingEventReactToAircraft)
	T2N(TASK_SHOCKING_EVENT_REACT, CTaskShockingEventReact)
	T2N(TASK_SHOCKING_EVENT_BACK_AWAY, CTaskShockingEventBackAway)
	T2N(TASK_SHOCKING_POLICE_INVESTIGATE, CTaskShockingPoliceInvestigate);
	T2N(TASK_SHOCKING_EVENT_STOP_AND_STARE, CTaskShockingEventStopAndStare);
	T2N(TASK_SHOCKING_NICE_CAR_PICTURE, CTaskShockingNiceCarPicture);
	T2N(TASK_SHOCKING_EVENT_THREAT_RESPONSE, CTaskShockingEventThreatResponse);
	T2N(TASK_COMBAT_FLANK, CTaskCombatFlank)
	T2N(TASK_PUT_ON_HELMET, CTaskPutOnHelmet)
	T2N(TASK_TAKE_OFF_HELMET, CTaskTakeOffHelmet)
	T2N(TASK_USE_DROPDOWN_ON_ROUTE, CTaskUseDropDownOnRoute)
	T2N(TASK_USE_CLIMB_ON_ROUTE, CTaskUseClimbOnRoute)
	T2N(TASK_MOVE_WAIT_FOR_TRAFFIC, CTaskMoveWaitForTraffic)
	T2N(TASK_USE_LADDER_ON_ROUTE, CTaskUseLadderOnRoute)
	T2N(TASK_SET_BLOCKING_OF_NON_TEMPORARY_EVENTS, CTaskSetBlockingOfNonTemporaryEvents)
	T2N(TASK_FORCE_MOTION_STATE, CTaskForceMotionState)
	T2N(TASK_CAR_REACT_TO_VEHICLE_COLLISION, CTaskCarReactToVehicleCollision)	
	T2N(TASK_REACT_TO_RUNNING_PED_OVER, CTaskReactToRanPedOver)
	T2N(TASK_CAR_REACT_TO_VEHICLE_COLLISION_GET_OUT, CTaskCarReactToVehicleCollisionGetOut)
	T2N(TASK_COMPLEX_USE_MOBILE_PHONE_AND_MOVEMENT, CTaskComplexUseMobilePhoneAndMovement)
	T2N(TASK_GET_OUT_OF_WATER, CTaskGetOutOfWater);
	T2N(TASK_THROW_PROJECTILE, CTaskThrowProjectile);
	T2N(TASK_COMBAT_ROLL, CTaskCombatRoll);
	T2N(TASK_WAIT_FOR_STEPPING_OUT, CTaskWaitForSteppingOut);
	T2N(TASK_COMBAT, CTaskCombat);
	T2N(TASK_SEARCH, CTaskSearch);
	T2N(TASK_SEARCH_ON_FOOT, CTaskSearchOnFoot);
	T2N(TASK_SEARCH_IN_AUTOMOBILE, CTaskSearchInAutomobile);
	T2N(TASK_SEARCH_IN_BOAT, CTaskSearchInBoat);
	T2N(TASK_SEARCH_IN_HELI, CTaskSearchInHeli);
	T2N(TASK_THREAT_RESPONSE, CTaskThreatResponse);
	T2N(TASK_INVESTIGATE, CTaskInvestigate);
	T2N(TASK_STAND_GUARD_FSM, CTaskStandGuardFSM);
	T2N(TASK_PATROL, CTaskPatrol);
	T2N(TASK_REACT_AIM_WEAPON, CTaskReactAimWeapon);
	T2N(TASK_MOVE_FOLLOW_ENTITY_OFFSET, CTaskMoveFollowEntityOffset);
	T2N(TASK_STAY_IN_COVER, CTaskStayInCover);
	T2N(TASK_CHAT, CTaskChat);
	T2N(TASK_REACT_TO_DEAD_PED, CTaskReactToDeadPed);
	T2N(TASK_VEHICLE_COMBAT, CTaskVehicleCombat);
	T2N(TASK_VEHICLE_PERSUIT, CTaskVehiclePersuit);
	T2N(TASK_VEHICLE_CHASE, CTaskVehicleChase);
	T2N(TASK_BOAT_COMBAT, CTaskBoatCombat);
	T2N(TASK_BOAT_CHASE, CTaskBoatChase);
	T2N(TASK_BOAT_STRAFE, CTaskBoatStrafe);
	T2N(TASK_HELI_COMBAT, CTaskHeliCombat);
	T2N(TASK_HELI_CHASE, CTaskHeliChase);
	T2N(TASK_SUBMARINE_COMBAT, CTaskSubmarineCombat);
	T2N(TASK_SUBMARINE_CHASE, CTaskSubmarineChase);
	T2N(TASK_PLANE_CHASE, CTaskPlaneChase);
	T2N(TASK_DRAGGED_TO_SAFETY, CTaskDraggedToSafety);
	T2N(TASK_DRAGGING_TO_SAFETY, CTaskDraggingToSafety);
	T2N(TASK_TARGET_UNREACHABLE, CTaskTargetUnreachable);
	T2N(TASK_TARGET_UNREACHABLE_IN_INTERIOR, CTaskTargetUnreachableInInterior);
	T2N(TASK_TARGET_UNREACHABLE_IN_EXTERIOR, CTaskTargetUnreachableInExterior);
	T2N(TASK_STEALTH_KILL, CTaskStealthKill);
	T2N(TASK_VARIED_AIM_POSE, CTaskVariedAimPose);
	T2N(TASK_ADVANCE, CTaskAdvance);
	T2N(TASK_CHARGE, CTaskCharge);
	T2N(TASK_MOVE_TO_TACTICAL_POINT, CTaskMoveToTacticalPoint);
	T2N(TASK_REACT_TO_BUDDY_SHOT, CTaskReactToBuddyShot);
	T2N(TASK_CONTROL_VEHICLE, CTaskControlVehicle);
	T2N(TASK_WATCH_INVESTIGATION, CTaskWatchInvestigation);
	T2N(TASK_DYING_DEAD, CTaskDyingDead);
	T2N(TASK_ANIMATED_HIT_BY_EXPLOSION, CTaskAnimatedHitByExplosion);
	T2N(TASK_SEARCH_FOR_UNKNOWN_THREAT, CTaskSearchForUnknownThreat);
	T2N(TASK_MELEE_ACTION_RESULT, CTaskMeleeActionResult);
	T2N(TASK_MELEE_UPPERBODY_ANIM, CTaskMeleeUpperbodyAnims);
	T2N(TASK_MELEE_UNINTERRUPTABLE, CTaskMeleeUninterruptable);
	T2N(TASK_CHECK_PED_IS_DEAD, CTaskCheckPedIsDead);
	T2N(TASK_BOMB, CTaskBomb);
	T2N(TASK_DETONATOR, CTaskDetonator);
	T2N(TASK_COUPLE_SCENARIO, CTaskCoupleScenario);
	T2N(TASK_USE_SCENARIO, CTaskUseScenario);
	T2N(TASK_COWER_SCENARIO, CTaskCowerScenario);
	T2N(TASK_DEAD_BODY_SCENARIO, CTaskDeadBodyScenario);
#if __BANK
	T2N(TASK_SCENARIO_ANIM_DEBUG, CTaskScenarioAnimDebug); //Dev only task for debugging animations when using scenarios
#endif //__BANK
	T2N(TASK_USE_VEHICLE_SCENARIO, CTaskUseVehicleScenario);
    T2N(TASK_STEAL_VEHICLE, CTaskStealVehicle);
	T2N(TASK_REACT_TO_PURSUIT, CTaskReactToPursuit);
	T2N(TASK_UNALERTED, CTaskUnalerted);
	T2N(TASK_ANIMATED_ATTACH, CTaskAnimatedAttach);
	T2N(TASK_SHOOT_AT_TARGET, CTaskShootAtTarget);
	T2N(TASK_MOVE_IN_AIR, CTaskMoveInAir);
#if __DEV
	T2N(TASK_BIND_POSE, CTaskBindPose);
#endif	//	__DEV
	T2N(TASK_FOLLOW_WAYPOINT_RECORDING, CTaskFollowWaypointRecording);
	T2N(TASK_MOTION_PED, CTaskMotionPed);
	T2N(TASK_MOTION_PED_LOW_LOD, CTaskMotionPedLowLod);
#if ENABLE_HORSE
	T2N(TASK_MOTION_ANIMAL, CTaskMotionAnimal);
#endif
	T2N(TASK_MOTION_BASIC_LOCOMOTION, CTaskMotionBasicLocomotion);
	T2N(TASK_MOTION_BASIC_LOCOMOTION_LOW_LOD, CTaskMotionBasicLocomotionLowLod);
	T2N(TASK_MOTION_STRAFING, CTaskMotionStrafing);
	T2N(TASK_MOTION_TENNIS, CTaskMotionTennis);
	T2N(TASK_ON_FOOT_FLIGHTLESS_BIRD, CTaskFlightlessBirdLocomotion);
	T2N(TASK_MOTION_AIMING, CTaskMotionAiming);
	T2N(TASK_MOTION_DIVING, CTaskMotionDiving);
	T2N(TASK_MOTION_SWIMMING, CTaskMotionSwimming);
	T2N(TASK_MOTION_PARACHUTING, CTaskMotionParachuting);
#if ENABLE_JETPACK
	T2N(TASK_MOTION_JETPACK, CTaskMotionJetpack);
#endif
#if ENABLE_DRUNK
	T2N(TASK_MOTION_DRUNK, CTaskMotionDrunk);
#endif // ENABLE_DRUNK
	T2N(TASK_REPOSITION_MOVE, CTaskRepositionMove);
	T2N(TASK_MOTION_AIMING_TRANSITION, CTaskMotionAimingTransition);
	T2N(TASK_SHOVE_PED, CTaskShovePed);
	T2N(TASK_VEHICLE_GOTO, CTaskVehicleGoTo);
	T2N(TASK_VEHICLE_GOTO_SUBMARINE, CTaskVehicleGoToSubmarine);
	T2N(TASK_VEHICLE_GOTO_PLANE, CTaskVehicleGoToPlane);
	T2N(TASK_VEHICLE_GOTO_HELICOPTER, CTaskVehicleGoToHelicopter);
	T2N(TASK_VEHICLE_GOTO_AUTOMOBILE_NEW, CTaskVehicleGoToAutomobileNew);
	T2N(TASK_VEHICLE_GOTO_POINT_AUTOMOBILE, CTaskVehicleGoToPointAutomobile);
	T2N(TASK_VEHICLE_GOTO_BOAT, CTaskVehicleGoToBoat);
	T2N(TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE, CTaskVehicleGoToPointWithAvoidanceAutomobile);
	T2N(TASK_VEHICLE_LAND_PLANE, CTaskVehicleLandPlane);
	T2N(TASK_VEHICLE_PLANE_CHASE, CTaskVehiclePlaneChase);
	T2N(TASK_VEHICLE_PULL_ALONGSIDE, CTaskVehiclePullAlongside);
	T2N(TASK_VEHICLE_PURSUE, CTaskVehiclePursue);
	T2N(TASK_VEHICLE_RAM, CTaskVehicleRam);
	T2N(TASK_VEHICLE_SPIN_OUT, CTaskVehicleSpinOut);
	T2N(TASK_VEHICLE_APPROACH, CTaskVehicleApproach);
	T2N(TASK_VEHICLE_THREE_POINT_TURN, CTaskVehicleThreePointTurn);
	T2N(TASK_VEHICLE_DEAD_DRIVER, CTaskVehicleDeadDriver);
	T2N(TASK_VEHICLE_CRUISE_NEW, CTaskVehicleCruiseNew);
	T2N(TASK_VEHICLE_CRUISE_BOAT, CTaskVehicleCruiseBoat);
	T2N(TASK_VEHICLE_STOP, CTaskVehicleStop);
	T2N(TASK_VEHICLE_PULL_OVER, CTaskVehiclePullOver);
	T2N(TASK_VEHICLE_REACT_TO_COP_SIREN, CTaskVehicleReactToCopSiren);
	T2N(TASK_VEHICLE_GOTO_LONGRANGE, CTaskVehicleGotoLongRange);
	T2N(TASK_VEHICLE_FLEE, CTaskVehicleFlee);
	T2N(TASK_VEHICLE_FLEE_AIRBORNE, CTaskVehicleFleeAirborne);
	T2N(TASK_VEHICLE_FLEE_BOAT, CTaskVehicleFleeBoat);
	T2N(TASK_VEHICLE_FOLLOW_RECORDING, CTaskVehicleFollowRecording);
	T2N(TASK_VEHICLE_FOLLOW, CTaskVehicleFollow);
	T2N(TASK_VEHICLE_BLOCK, CTaskVehicleBlock);
	T2N(TASK_VEHICLE_BLOCK_CRUISE_IN_FRONT, CTaskVehicleBlockCruiseInFront);
	T2N(TASK_VEHICLE_BLOCK_BRAKE_IN_FRONT, CTaskVehicleBlockBrakeInFront);
	T2N(TASK_VEHICLE_BLOCK_BACK_AND_FORTH, CTaskVehicleBlockBackAndForth);
	T2N(TASK_VEHICLE_CRASH, CTaskVehicleCrash);
	T2N(TASK_VEHICLE_LAND, CTaskVehicleLand);
	T2N(TASK_VEHICLE_HOVER, CTaskVehicleHover);
	T2N(TASK_VEHICLE_ATTACK, CTaskVehicleAttack);
	T2N(TASK_VEHICLE_ATTACK_TANK, CTaskVehicleAttackTank);
	T2N(TASK_VEHICLE_CIRCLE, CTaskVehicleCircle);
	T2N(TASK_VEHICLE_POLICE_BEHAVIOUR, CTaskVehiclePoliceBehaviour);
	T2N(TASK_VEHICLE_ESCORT, CTaskVehicleEscort);
	T2N(TASK_VEHICLE_HELI_PROTECT, CTaskVehicleHeliProtect);
	T2N(TASK_VEHICLE_PLAYER_DRIVE, CTaskVehiclePlayerDrive);
	T2N(TASK_VEHICLE_PLAYER_DRIVE_AUTOMOBILE, CTaskVehiclePlayerDriveAutomobile);
	T2N(TASK_VEHICLE_PLAYER_DRIVE_AMPHIBIOUS_AUTOMOBILE, CTaskVehiclePlayerDriveAmphibiousAutomobile);
	T2N(TASK_VEHICLE_PLAYER_DRIVE_BIKE, CTaskVehiclePlayerDriveBike);
	T2N(TASK_VEHICLE_PLAYER_DRIVE_BOAT, CTaskVehiclePlayerDriveBoat);
	T2N(TASK_VEHICLE_PLAYER_DRIVE_SUBMARINE, CTaskVehiclePlayerDriveSubmarine);
	T2N(TASK_VEHICLE_PLAYER_DRIVE_SUBMARINECAR, CTaskVehiclePlayerDriveSubmarineCar);
	T2N(TASK_VEHICLE_PLAYER_DRIVE_PLANE, CTaskVehiclePlayerDrivePlane);
	T2N(TASK_VEHICLE_PLAYER_DRIVE_HELI, CTaskVehiclePlayerDriveHeli);
	T2N(TASK_VEHICLE_PLAYER_DRIVE_AUTOGYRO, CTaskVehiclePlayerDriveAutogyro);
	T2N(TASK_VEHICLE_PLAYER_DRIVE_DIGGER_ARM, CTaskVehiclePlayerDriveDiggerArm);
	T2N(TASK_VEHICLE_PLAYER_DRIVE_TRAIN, CTaskVehiclePlayerDriveTrain);
	T2N(TASK_VEHICLE_NO_DRIVER, CTaskVehicleNoDriver);
	T2N(TASK_VEHICLE_ANIMATION, CTaskVehicleAnimation);
	T2N(TASK_VEHICLE_CONVERTIBLE_ROOF, CTaskVehicleConvertibleRoof);
	T2N(TASK_VEHICLE_TRANSFORM_TO_SUBMARINE, CTaskVehicleTransformToSubmarine);
	T2N(TASK_VEHICLE_SHOT_TIRE, CTaskVehicleShotTire);
	T2N(TASK_VEHICLE_BURNOUT, CTaskVehicleBurnout);
	T2N(TASK_VEHICLE_REV_ENGINE, CTaskVehicleRevEngine);
	T2N(TASK_VEHICLE_SURFACE_IN_SUBMARINE, CTaskVehicleSurfaceInSubmarine);
	T2N(TASK_MOTION_IN_AUTOMOBILE, CTaskMotionInAutomobile);
	T2N(TASK_MOTION_ON_BICYCLE, CTaskMotionOnBicycle);
	T2N(TASK_MOTION_ON_BICYCLE_CONTROLLER, CTaskMotionOnBicycleController);
	T2N(TASK_MOTION_IN_VEHICLE, CTaskMotionInVehicle);
#if ENABLE_MOTION_IN_TURRET_TASK	
	T2N(TASK_MOTION_IN_TURRET, CTaskMotionInTurret);
#endif // ENABLE_MOTION_IN_TURRET_TASK	
	T2N(TASK_REACT_TO_BEING_JACKED, CTaskReactToBeingJacked);
	T2N(TASK_REACT_TO_BEING_ASKED_TO_LEAVE_VEHICLE, CTaskReactToBeingAskedToLeaveVehicle);
	T2N(TASK_POLICE, CTaskPolice);
	T2N(TASK_POLICE_ORDER_RESPONSE, CTaskPoliceOrderResponse);
	T2N(TASK_PURSUE_CRIMINAL, CTaskPursueCriminal);
	T2N(TASK_ARREST_PED, CTaskArrestPed);
	T2N(TASK_BUSTED, CTaskBusted);
	T2N(TASK_POLICE_WANTED_RESPONSE, CTaskPoliceWantedResponse);
	T2N(TASK_FIRE_PATROL, CTaskFirePatrol);
	T2N(TASK_HELI_ORDER_RESPONSE, CTaskHeliOrderResponse);
	T2N(TASK_HELI_PASSENGER_RAPPEL, CTaskHeliPassengerRappel);
	T2N(TASK_AMBULANCE_PATROL, CTaskAmbulancePatrol);
	T2N(TASK_SWAT, CTaskSwat);
	T2N(TASK_SWAT_WANTED_RESPONSE, CTaskSwatWantedResponse);
	T2N(TASK_SWAT_ORDER_RESPONSE, CTaskSwatOrderResponse);
	T2N(TASK_SWAT_GO_TO_STAGING_AREA, CTaskSwatGoToStagingArea);
	T2N(TASK_SWAT_FOLLOW_IN_LINE, CTaskSwatFollowInLine);
	T2N(TASK_WITNESS, CTaskWitness);
	T2N(TASK_GANG_PATROL, CTaskGangPatrol);
	T2N(TASK_ARMY, CTaskArmy);
	T2N(TASK_MOVE_SCRIPTED, CTaskMoVEScripted);

#if ENABLE_HORSE
	T2N(TASK_MOTION_RIDE_HORSE, CTaskMotionRideHorse);
	T2N(TASK_PLAYER_ON_HORSE, CTaskPlayerOnHorse);
	T2N(TASK_MOVE_MOUNTED, CTaskMoveMounted);
	T2N(TASK_MOVE_MOUNTED_SPUR_CONTROL, CTaskMoveMountedSpurControl);
	T2N(TASK_MOUNT_ANIMAL, CTaskMountAnimal);
	T2N(TASK_DISMOUNT_ANIMAL, CTaskDismountAnimal);
	T2N(TASK_MOUNT_SEAT, CTaskMountSeat);
	T2N(TASK_GETTING_MOUNTED, CTaskGettingMounted);
	T2N(TASK_DISMOUNT_SEAT, CTaskDismountSeat);	
	T2N(TASK_GETTING_DISMOUNTED, CTaskGettingDismounted);		
#endif
	T2N(TASK_MOUNT_THROW_PROJECTILE, CTaskMountThrowProjectile);
#if ENABLE_HORSE	
	T2N(TASK_RIDER_REARUP, CTaskRiderRearUp);		
	T2N(TASK_JUMP_MOUNTED, CTaskJumpMounted);
	T2N(TASK_MOVE_DRAFT_ANIMAL, CTaskMoveDraftAnimal);
#endif
	T2N(TASK_ARREST_PED2, CTaskArrestPed2);
#if ENABLE_TASKS_ARREST_CUFFED
	T2N(TASK_ARREST, CTaskArrest);
	T2N(TASK_CUFFED, CTaskCuffed);		
	T2N(TASK_IN_CUSTODY, CTaskInCustody);	
	T2N(TASK_PLAY_CUFFED_SECONDARY_ANIMS, CTaskPlayCuffedSecondaryAnims);	
#endif // ENABLE_TASKS_ARREST_CUFFED
#if CNC_MODE_ENABLED
	T2N(TASK_INCAPACITATED, CTaskIncapacitated);
#endif
#if ENABLE_HORSE
	T2N(TASK_MOUNT_FOLLOW_NEAREST_ROAD, CTaskMountFollowNearestRoad);
#endif

	T2N(TASK_VEHICLE_PARK_NEW, CTaskVehicleParkNew);
	T2N(TASK_VEHICLE_FOLLOW_WAYPOINT_RECORDING, CTaskVehicleFollowWaypointRecording);
	T2N(TASK_VEHICLE_GOTO_NAVMESH, CTaskVehicleGoToNavmesh);

#if __BANK
	T2N(TASK_PREVIEW_MOVE_NETWORK, CTaskPreviewMoveNetwork); //Dev only task for previewing a MoVE network on a ped
#endif //__BANK

	//Vehicle AI temp tasks
	T2N(TASK_VEHICLE_WAIT, CTaskVehicleWait);
	T2N(TASK_VEHICLE_REVERSE, CTaskVehicleReverse);
	T2N(TASK_VEHICLE_BRAKE, CTaskVehicleBrake);
	T2N(TASK_VEHICLE_HANDBRAKE, CTaskVehicleHandBrake);
	T2N(TASK_VEHICLE_TURN, CTaskVehicleTurn);
	T2N(TASK_VEHICLE_GO_FORWARD, CTaskVehicleGoForward);
	T2N(TASK_VEHICLE_SWERVE, CTaskVehicleSwerve);
	T2N(TASK_VEHICLE_FLY_DIRECTION, CTaskVehicleFlyDirection);
	T2N(TASK_VEHICLE_HEADON_COLLISION, CTaskVehicleHeadonCollision);
	T2N(TASK_VEHICLE_BOOST_USE_STEERING_ANGLE, CTaskVehicleBoostUseSteeringAngle);

	T2N(TASK_ANIMATED_FALLBACK, CTaskAnimatedFallback);
}

////////////////////////////////////////////////////////////////////////////////

s32 CTaskClassInfoManager::GetLargestTaskIndex() const
{
	s32 iLargestTaskIndex = 0;
	for( s32 i = 1; i < CTaskTypes::MAX_NUM_TASK_TYPES; i++ )
	{
		if( m_TaskClassInfo[iLargestTaskIndex].m_iTaskSize < m_TaskClassInfo[i].m_iTaskSize )
		{
			iLargestTaskIndex = i;
		}
	}

	return iLargestTaskIndex;
}
