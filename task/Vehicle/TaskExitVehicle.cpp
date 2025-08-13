//
// Task/Vehicle/TaskExitVehicle.cpp
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

// File header
#include "Task/Vehicle/TaskExitVehicle.h"

// Rage headers
#include "math/angmath.h"
#include "physics/simulator.h"
#include "system/stack.h"

// Framework headers 
#include "fwanimation/directorcomponentragdoll.h"

// Game headers
#include "audio/caraudioentity.h"
#include "audio/northaudioengine.h"
#include "animation/AnimManager.h"
#include "animation/FacialData.h"
#include "animation/MovePed.h"
#include "Camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/system/CameraManager.h"
#include "Control/Replay/ReplayBufferMarker.h"
#include "Debug/DebugScene.h"
#include "Event/Events.h"
#include "event/EventDamage.h"
#include "Event/EventShocking.h"
#include "Event/EventWeapon.h"
#include "Event/ShockingEvents.h"
#include "PedGroup/PedGroup.h"
#include "Peds/Ped.h"
#include "Peds/PedHelmetComponent.h"
#include "Peds/PedIntelligence.h"
#include "renderer/PostProcessFXHelper.h"
#include "scene/world/GameWorld.h"
#include "script/script.h"
#include "script/script_cars_and_peds.h"
#include "Task/Animation/TaskAnims.h"
#include "Task/Combat/TaskDamageDeath.h"
#include "Task/Combat/TaskThreatResponse.h"
#include "Task/Default/TaskCuffed.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/General/TaskBasic.h"
#include "Task/General/TaskSecondary.h"
#include "task/General/Phone/TaskCallPolice.h"
#include "task/Motion/Locomotion/TaskInWater.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Motion/Vehicle/TaskMotionInTurret.h"
#include "Task/Movement/TaskFall.h"
#include "Task/Movement/TaskGetUp.h"
#include "Task/Movement/TaskIdle.h"
#include "task/Movement/TaskParachute.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Movement/Jumping/TaskFallGetUp.h"
#include "Task/Movement/Jumping/TaskJump.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Physics/TaskNMBalance.h"
#include "Task/Physics/TaskNMHighFall.h"
#include "Task/Physics/TaskNMJumpRollFromRoadVehicle.h"
#include "Task/Physics/TaskNMThroughWindscreen.h"
#include "task/Response/TaskFlee.h"
#include "Task/Scenario/TaskScenario.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskCarAccessories.h"
#include "Task/Vehicle/TaskCarUtils.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskInVehicle.h"
#include "Vehicles/AmphibiousAutomobile.h"
#include "Vehicles/Bike.h"
#include "Vehicles/Boat.h"
#include "Vehicles/Heli.h"
#include "Vehicles/Planes.h"
#include "Vehicles/Train.h"
#include "Vehicles/Submarine.h"
#include "Vehicles/Vehicle.h"
#include "Vehicles/Metadata/VehicleDebug.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "Vehicles/Metadata/VehicleEntryPointInfo.h"
#include "Vehicles/Metadata/VehicleLayoutInfo.h"
#include "Vehicles/Metadata/VehicleMetadataManager.h"
#include "vfx/vehicleglass/VehicleGlassManager.h"
#include "Vfx/Systems/VfxVehicle.h"
#include "vehicleAi/task/TaskVehiclePark.h"
#include "vehicleAi/task/TaskVehicleTempAction.h"
#include "vehicleAi/task/TaskVehiclePlayer.h"
#include "vehicleAi/task/TaskVehicleAnimation.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "modelinfo/PedModelInfo.h"
#include "stats/StatsInterface.h"
#include "AI/Ambient/ConditionalAnimManager.h"

#if __BANK
#include "Peds/PedCapsule.h"
#endif // __BANK

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

const fwMvRequestId CTaskExitVehicle::ms_ClimbDownRequestId("ClimbDown",0x5330B3EE);
const fwMvRequestId CTaskExitVehicle::ms_ToIdleRequestId("ToIdle",0xDD5EB647);
const fwMvBooleanId CTaskExitVehicle::ms_ToIdleOnEnterId("ToIdle_OnEnter",0x8F39443A);
const fwMvBooleanId CTaskExitVehicle::ms_ClimbDownOnEnterId("ClimbDown_OnEnter",0x79E9D063);
const fwMvBooleanId	CTaskExitVehicle::ms_ToIdleAnimFinishedId("ToIdleAnimFinished",0x42840A5D);
const fwMvBooleanId CTaskExitVehicle::ms_ClimbDownAnimFinishedId("ClimbDownAnimFinished",0xB531A0FF);
const fwMvBooleanId CTaskExitVehicle::ms_DeathInSeatInterruptId("DeathInSeatInterrupt",0x37EAA647);
const fwMvBooleanId CTaskExitVehicle::ms_DeathOutOfSeatInterruptId("DeathOutOfSeatInterrupt",0x68DA07B5);
const fwMvBooleanId CTaskExitVehicle::ms_ClimbDownInterruptId("ClimbDownInterrupt",0xC01A1D16);
const fwMvFloatId	CTaskExitVehicle::ms_ToIdleAnimPhaseId("ToIdlePhase",0x8733E00C);
const fwMvFloatId	CTaskExitVehicle::ms_ClimbDownAnimPhaseId("ClimbDownPhase",0x755B0FC9);
const fwMvFloatId	CTaskExitVehicle::ms_ClimbDownRateId("ClimbDownRate",0x9FCC32CD);
const fwMvFloatId	CTaskExitVehicle::ms_ClosesDoorBlendDurationId("CloseDoorBlendDuration",0xC381FC55);
const fwMvStateId	CTaskExitVehicle::ms_ToIdleStateId("To Idle",0x44F674);
const fwMvBooleanId CTaskExitVehicle::ms_NoDoorInterruptId("NoDoorInterrupt",0x5805B547);
const fwMvStateId	CTaskExitVehicle::ms_ClimbDownStateId("ClimbDown",0x5330B3EE);
const fwMvClipId	CTaskExitVehicle::ms_ClimbDownClipId("ClimbDownClip",0x7B4BD983);
const fwMvClipId	CTaskExitVehicle::ms_ToIdleClipId("ToIdleClip",0xFE5F1272);

////////////////////////////////////////////////////////////////////////////////

// Statics
CTaskExitVehicle::Tunables CTaskExitVehicle::ms_Tunables;

IMPLEMENT_VEHICLE_TASK_TUNABLES(CTaskExitVehicle, 0x006d6bab);

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::StartMoveNetwork(CPed& ped, const CVehicle& vehicle, CMoveNetworkHelper& moveNetworkHelper, const fwMvClipSetId& clipsetId, s32 iEntryPointIndex, float fBlendInDuration)
{
	// Don't restart the move network if its already attached
	if (moveNetworkHelper.IsNetworkAttached())
	{
		// If we shuffled we still need to set the clipset for the new seat
		moveNetworkHelper.SetClipSet(clipsetId);
		return true;
	}

    if(ped.GetMovePed().GetState() != CMovePed::kStateAnimated)
	{
		ped.GetMovePed().SwitchToAnimated(0.0f);
	}

#if __ASSERT
	const CVehicleEntryPointAnimInfo* pEntryClipInfo = vehicle.GetEntryAnimInfo(iEntryPointIndex);
	taskFatalAssertf(pEntryClipInfo, "NULL Entry Info for entry index %i", iEntryPointIndex);
#endif // __ASSERT

	if (!moveNetworkHelper.IsNetworkActive())
	{
		moveNetworkHelper.CreateNetworkPlayer(&ped, CClipNetworkMoveInfo::ms_NetworkTaskExitVehicle);
		moveNetworkHelper.SetClipSet(clipsetId);

		fwMvClipSetId commonClipsetId = CTaskEnterVehicleAlign::GetCommonClipsetId(&vehicle, iEntryPointIndex);
		if (CTaskVehicleFSM::IsVehicleClipSetLoaded(commonClipsetId))
		{
			moveNetworkHelper.SetClipSet(commonClipsetId, CTaskVehicleFSM::ms_CommonVarClipSetId);
		}
	}

	ASSERT_ONLY(taskDebugf2("Frame : %u - %s%s started Exit Vehicle Move Network", fwTimer::GetFrameCount(), ped.IsNetworkClone() ? "Clone ped : " : "Local ped : ", ped.GetDebugName()));
	return ped.GetMovePed().SetTaskNetwork(moveNetworkHelper, fBlendInDuration);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::IsVerticalProbeClear(const Vector3& vTestPos, float fProbeLength, const CPed& ped, Vector3* pvIntersectionPos, Vector3 vOverrideEndPos)
{
	Vector3 vEndPos(vTestPos.x, vTestPos.y, vTestPos.z - fProbeLength);
	if (vOverrideEndPos.IsNonZero())
	{
		vEndPos = vOverrideEndPos;
	}

	WorldProbe::CShapeTestFixedResults<> probeResults;
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetResultsStructure(&probeResults);
	probeDesc.SetStartAndEnd(vTestPos, vEndPos);
	const s32 iIncludeFlags = ArchetypeFlags::GTA_ALL_TYPES_MOVER &~ (ArchetypeFlags::GTA_PED_TYPE | ArchetypeFlags::GTA_RIVER_TYPE);
	probeDesc.SetIncludeFlags(iIncludeFlags);
	probeDesc.SetExcludeEntity(&ped, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);

	if (!WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
#if __BANK
		ms_debugDraw.AddLine(RCC_VEC3V(vTestPos), RCC_VEC3V(vEndPos), Color_green, 1000);
#endif
		return true;
	}

	if (pvIntersectionPos)
	{
		*pvIntersectionPos = probeResults[0].GetHitPosition();
	}

#if __BANK
	ms_debugDraw.AddLine(RCC_VEC3V(vTestPos), RCC_VEC3V(probeResults[0].GetHitPosition()), Color_red, 1000);
#endif

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::IsVehicleOnGround(const CVehicle& vehicle)
{
	// Check if we are touching geometry.
	const bool bIsTouchingGeometry = (vehicle.GetFrameCollisionHistory()->GetFirstBuildingCollisionRecord() != NULL);
	if (bIsTouchingGeometry)
	{
		return true;
	}

	Vec3V vMin, vMax;
	if (CVehicle::GetMinMaxBoundsFromDummyVehicleBound(vehicle, vMin, vMax))
	{
		const float fProbeLength = Abs(vMax.GetZf() - vMin.GetZf()) + ms_Tunables.m_ExtraOffsetForGroundCheck;
		const Vector3 vStartPos = VEC3V_TO_VECTOR3(vehicle.GetTransform().GetPosition());
		const Vector3 vEndPos(vStartPos.x, vStartPos.y, vStartPos.z - fProbeLength);

		WorldProbe::CShapeTestFixedResults<> probeResults;
		WorldProbe::CShapeTestProbeDesc probeDesc;
		probeDesc.SetResultsStructure(&probeResults);
		probeDesc.SetStartAndEnd(vStartPos, vEndPos);
		probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_MOVER);
		probeDesc.SetExcludeEntity(&vehicle);

		if (!WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
		{
#if __BANK
			ms_debugDraw.AddLine(RCC_VEC3V(vStartPos), RCC_VEC3V(vEndPos), Color_green, 1000);
#endif
			return false;
		}

#if __BANK

#if DEBUG_DRAW
		static bank_bool s_bRenderHitEntitiesForVehOnGroundCheck = false;
		if (s_bRenderHitEntitiesForVehOnGroundCheck)
		{
			for (int i = 0; i < probeResults.GetNumHits(); i++)
			{
				if (probeResults[i].GetHitDetected())
				{
					CEntity* pHitEnt = probeResults[i].GetHitEntity();
					if (pHitEnt)
					{
						Vector3 vHitPos = probeResults[i].GetHitPosition();
						static bank_float s_VehOnGroundDebugHitRadius = 0.15f;
						static bank_u32 s_VehOnGroundDebugTTL = 50;
						grcDebugDraw::Sphere(vHitPos, s_VehOnGroundDebugHitRadius, Color_red, false, s_VehOnGroundDebugTTL);
						char str[512];
						formatf(str, "%s[%s]", pHitEnt->GetLogName(), pHitEnt->GetModelName());
						grcDebugDraw::Text(vHitPos, Color_yellow, 0, 0, str);
					}
				}
			}
		}
#endif //DEBUG_DRAW

		ms_debugDraw.AddLine(RCC_VEC3V(vStartPos), RCC_VEC3V(vEndPos), Color_red, 1000);
#endif
		return true;
	}
	else 
	{
		return !vehicle.IsInAir();
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::IsPlayerTryingToAimOrFire(const CPed& rPed)
{
	//Ensure the ped is a player.
	if(!rPed.IsLocalPlayer())
	{
		return false;
	}

	//Ensure the player info is valid.
	CPlayerInfo* pPlayerInfo = rPed.GetPlayerInfo();
	if(!taskVerifyf(pPlayerInfo, "The player info is invalid."))
	{
		return false;
	}

	//Ensure the equipped weapon is fireable
	if(!rPed.GetEquippedWeaponInfo() || !rPed.GetEquippedWeaponInfo()->GetIsGunOrCanBeFiredLikeGun())
	{
		return false;
	}

	//Check if the player is aiming.
	if(pPlayerInfo->IsAiming())
	{
		return true;
	}

	//Check if the player is firing.
	if(pPlayerInfo->IsFiring())
	{
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

CTaskExitVehicle::CTaskExitVehicle(CVehicle* pVehicle, const VehicleEnterExitFlags& iRunningFlags, float fDelayTime, CPed* pPedJacker, s32 iTargetEntryPoint)
: CTaskVehicleFSM(pVehicle, SR_Specific, -1, iRunningFlags, iTargetEntryPoint)
, m_vSeatOffset(V_ZERO)
, m_vExitPosition(V_ZERO)
, m_vFleeExitPosition(Vector3::ZeroType)
, m_vCachedClipTranslation(Vector3::ZeroType)
, m_qCachedClipRotation(Quaternion::IdentityType)
, m_fDelayTime(fDelayTime)
, m_fMinTimeToWaitForOtherPedToExit(0.0f)
, m_pPedJacker(pPedJacker)
, m_pDesiredTarget(NULL)
, m_iLastCompletedState(-1)
, m_iNumTimesInPickDoorState(0)
, m_bVehicleTaskAssigned(false)
, m_bIsRappel(false)
, m_bAimExit(false)
, m_bWantsToEnter(false)
, m_bCurrentAnimFinished(false)
, m_bWantsToGoIntoCover(false)
, m_bHasClosedDoor(false)
, m_bHasValidExitPosition(false)
, m_bSetInactiveCollidesAgainstInactive(false)
, m_bSubTaskExitRateSet(false)
, m_bCloneShouldUseCombatExitRate(false)
, m_nFramesWaited(0)
, m_fLegIkBlendInPhase(-1.0f)
, m_fCachedGroundHeight(-1.0f)
, m_fDesiredFleeExitAngleOffset(0.0f)
, m_uLastTimeUnsettled(0)
, m_fTimeSinceSubTaskFinished(0.0f)
{
	SetInternalTaskType(CTaskTypes::TASK_EXIT_VEHICLE);
	m_uTimeStartedExitTask = fwTimer::GetTimeInMilliseconds();

	AI_LOG_WITH_ARGS("[%s][Creation Event] - %s Vehicle %s | Entrypoint %i\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(pVehicle), AILogging::GetDynamicEntityNameSafe(pVehicle), iTargetEntryPoint);
	AI_LOG_STACK_TRACE(8);
}

////////////////////////////////////////////////////////////////////////////////

CTaskExitVehicle::~CTaskExitVehicle()
{

}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicle::CleanUp()
{
	CPed* pPed = GetPed();

	if(!pPed->IsNetworkClone())
	{
		UnreserveDoor(pPed);
		UnreserveSeat(pPed);
	}
    else
    {
        BANK_ONLY(taskDebugf2("Frame : %u - %s%s Cleaning up exit vehicle task", fwTimer::GetFrameCount(), pPed->IsNetworkClone() ? "Clone ped : " : "Local ped : ", pPed->GetDebugName()));
    }

	if (m_bSetInactiveCollidesAgainstInactive && pPed->GetCurrentPhysicsInst() &&
		pPed->GetCurrentPhysicsInst()->GetLevelIndex() != phInst::INVALID_INDEX )
	{
		// Grab the level index.
		u16 uLevelIndex = pPed->GetCurrentPhysicsInst()->GetLevelIndex();
		// Allow the ped to collide with other inactive things.
		PHLEVEL->SetInactiveCollidesAgainstInactive(uLevelIndex, false);
		m_bSetInactiveCollidesAgainstInactive = false;
	}

	pPed->GetMotionData()->SetGaitReductionBlockTime(); //block gait reduction while we ramp back up
	pPed->GetIkManager().ClearFlag(PEDIK_LEGS_USE_ANIM_BLOCK_TAGS);

	if (!pPed->GetIsInVehicle())
	{
		if ((pPed->GetAttachState() != ATTACH_STATE_DETACHING) && !IsFlagSet(CVehicleEnterExitFlags::IgnoreExitDetachOnCleanup))
		{
			pPed->DetachFromParent(0);
		}

		const bool bExitUnderWater = IsFlagSet(CVehicleEnterExitFlags::ExitUnderWater);
		if (!bExitUnderWater)
		{
			if (m_bAimExit)
			{
				pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Aiming);
				pPed->SetPedResetFlag(CPED_RESET_FLAG_AimWeaponReactionRunning, true);
			}
			else if (IsFlagSet(CVehicleEnterExitFlags::InWater))
			{	
				//! This doesn't seem necessary - ped will blend to swim when we exit task. And this forces
				//! ped to move, which looks glitchy.
				//if(IsFlagSet(CVehicleEnterExitFlags::ExitToSwim))
				//	pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Swimming_TreadWater, true);
			}
			else if (IsFlagSet(CVehicleEnterExitFlags::IsTransitionToSkyDive))
			{
				pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Parachuting);
			}
			else if (IsFlagSet(CVehicleEnterExitFlags::InAirExit))
			{
				pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_DoNothing, true);
			}
			else if (IsFlagSet(CVehicleEnterExitFlags::IsFleeExit))
			{
				pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Run);
			}
			else 
#if ENABLE_DRUNK
			if (!pPed->IsDrunk())
#endif // ENABLE_DRUNK
			{
				pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Idle, true);
			}
			pPed->GetMotionData()->SetDesiredMoveBlendRatio(0.0f);
		}

		float fBlendOutDuration;
		if(bExitUnderWater)
			fBlendOutDuration = ms_Tunables.m_ExitVehicleUnderWaterBlendOutDuration;
		else if(IsPlayerTryingToAimOrFire(*pPed))
			fBlendOutDuration = ms_Tunables.m_ExitVehicleAttempToFireBlendOutDuration;
		else
			fBlendOutDuration = ms_Tunables.m_ExitVehicleBlendOutDuration;
		
		u32 uFlags = 0;
		if (m_bWarping)
		{
			fBlendOutDuration = INSTANT_BLEND_DURATION;
		}
		else if (IsFlagSet(CVehicleEnterExitFlags::IsFleeExit))
		{
			fBlendOutDuration = ms_Tunables.m_FleeExitVehicleBlendOutDuration;
			uFlags |= CMovePed::Task_TagSyncTransition;
		}

		if (!pPed->IsNetworkClone() && pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SEQUENCE))
		{
			uFlags |= CMovePed::Task_IgnoreMoverBlendRotation;
		}

		pPed->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, fBlendOutDuration, uFlags);

		if(!pPed->IsPlayer())
		{
			CTaskMotionBase* pPrimaryMotionTask = pPed->GetPrimaryMotionTask();
			CTaskMotionPed* pMotionPedTask = (pPrimaryMotionTask && pPrimaryMotionTask->GetTaskType() == CTaskTypes::TASK_MOTION_PED) ? static_cast<CTaskMotionPed*>(pPrimaryMotionTask) : NULL;
			if (pMotionPedTask)
			{
				pMotionPedTask->SetBlockAimingTransitionTime(fBlendOutDuration);
			}
		}
	}
	else
	{
		if (IsFlagSet(CVehicleEnterExitFlags::DontSetPedOutOfVehicle))
		{
			pPed->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, SLOW_BLEND_DURATION);
		}
		else
		{
			if (pPed->ShouldBeDead())
			{
				TUNE_GROUP_FLOAT(EXIT_VEHICLE_TUNE, DEAD_BLEND_OUT_DURATION, NORMAL_BLEND_DURATION, 0.0f, 1.0f, 0.01f);
				pPed->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, DEAD_BLEND_OUT_DURATION);
			}
			else
			{
				bool clonePedExitingToParachute = false;
				if(pPed->IsNetworkClone())
				{
					if(pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_PARACHUTE, PED_TASK_PRIORITY_MAX, false))
					{
						// Don't clear out the task sub network before the parachute task has had a change to come in otherwise we get a nasty pop....
						CNetObjPed* netObj = static_cast<CNetObjPed*>(pPed->GetNetworkObject());
						if(netObj)
						{
							// in these specific circumstances, we don't clear out the task sub move network otherwise we get a bind pose 
							// as the parachute task hasn't been given a chance to start running on the clone and insert it's own task sub network
							netObj->TakeControlOfOrphanedMoveNetworkTaskSubNetwork(m_moveNetworkHelper.GetNetworkPlayer(), INSTANT_BLEND_DURATION);
							clonePedExitingToParachute = true;
						}
					}
				}

				if(!clonePedExitingToParachute)
				{
					pPed->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, INSTANT_BLEND_DURATION);
				}
			}

			// Set the ped out of the vehicle if they had begun animating out of the direct access entry
			if (GetState() >= State_ExitSeat && GetState() <= State_SetPedOut)
			{
				pPed->SetPedOutOfVehicle(CPed::PVF_IgnoreSafetyPositionCheck|CPed::PVF_DontResetDefaultTasks);
			}

			if(pPed->IsNetworkClone())
			{
				CNetObjPed* netObj = static_cast<CNetObjPed*>(pPed->GetNetworkObject());

				// When sitting in a plane, a clone ped can be far enough from the local player that TaskExitVehicle is 
				// out of scope. This means the clone ped will not get out and still be attached to the vehicle when it 
				// starts falling so we detach it here. We can't just make sure TaskExitVehicle is in scope for clone peds 
				// in vehicles as the clone task will often get aborted before the ped has been detached anyway and forcing
				// the clone task to finish before it gets replaced will produce noticable position pops due to the owner 
				// falling earlier than the clone and the rate of falling being so high.
				if(netObj->GetPedsTargetVehicleId() == NETWORK_INVALID_OBJECT_ID)
				{
					netObj->SetClonePedOutOfVehicle(true, CPed::PVF_Warp);
				}

				if(pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_PARACHUTE, PED_TASK_PRIORITY_MAX, false))
				{
					// if we get here, it means the clone has tried to jump out of a vehicle to skydiving but the incoming task parachute has killed TaskExitVehicle off
					// before it's has a chance to do some important stuff like inherit the velocity from the vehicle if we're in the air) or kill off collisions 
					// between the vehicle we've just jumped out of (as we may be intersecting).... (i.e. JumpOutOfSeat_OnUpdate never got to finish properly)

					// Stop collisions between the vehicle we've just jumped out of and the ped (until we're not intersecting)...
					CTaskExitVehicle::OnExitMidAir(pPed, m_pVehicle);
					
					// we don't want the clone colliding with the vehicle at all - needs to just follow the owners lead...
					// collision gets re-enabled at a later point in TaskParachute (Deploy, CleanUp)
					m_pVehicle->SetNoCollision(pPed, NO_COLLISION_PERMENANT);

					// Inherit the velocity of the vehicle we've just jumped out of and animation we've been playing....
					Vector3 vDesiredVelocity = VEC3V_TO_VECTOR3(pPed->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(pPed->GetAnimatedVelocity())));
					vDesiredVelocity += m_pVehicle->GetVelocity();
					const Vector3 vDesiredAngVelocity = pPed->GetDesiredAngularVelocity();
					pPed->SetVelocity(vDesiredVelocity);
					pPed->SetDesiredVelocity(vDesiredVelocity);
					pPed->SetAngVelocity(vDesiredAngVelocity);
				}
			}
		}
	}

	if(pPed->GetPlayerInfo())
	{
		pPed->GetPlayerInfo()->SetExitVehicleTaskFinished();
	}

	if(pPed->IsLocalPlayer() && m_pPedJacker && !m_pPedJacker->IsPlayer())
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired, true);
	}

	// if the vehicle has KERS make sure we disable it when the player exits
	CVehicle* pVehicle = GetVehicle();
	if( pPed->IsLocalPlayer() &&
		pVehicle &&
		pVehicle->pHandling->hFlags & HF_HAS_KERS )
	{
		pVehicle->m_Transmission.DisableKERSEffect();
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::ShouldAbort(const AbortPriority priority, const aiEvent* pEvent)
{
	if (priority == ABORT_PRIORITY_IMMEDIATE)
	{
		return true;
	}

	if(ShouldAbortFromInAirEvent(pEvent))
	{
		return true;
	}

	CPed* pPed = GetPed();
	if (pEvent && pEvent->GetEventType() == EVENT_SWITCH_2_NM_TASK && pPed->GetActivateRagdollOnCollision())
	{
		if (CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_ACTIVATE_ON_COLLISION))
		{
			return true;
		}
	}

	if (pEvent && ((CEvent*)pEvent)->RequiresAbortForRagdoll() && GetState()==State_ExitSeat)
	{
		return true;
	}

	if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_EXIT_VEHICLE_SEAT)
	{
		if (GetSubTask()->GetState() == CTaskExitVehicleSeat::State_BeJacked || GetSubTask()->GetState() == CTaskExitVehicleSeat::State_StreamedBeJacked ||
			(GetSubTask()->GetState() == CTaskExitVehicleSeat::State_ExitSeat || GetSubTask()->GetState() == CTaskExitVehicleSeat::State_StreamedExit))
		{
			if (pEvent && pEvent->GetEventType() == EVENT_GIVE_PED_TASK)
			{
				aiDisplayf("Ped %s %p is trying to abort jack early for event ? %s", pPed->GetModelName(), pPed, pEvent ? pEvent->GetName().c_str():"NULL");
				return false;
			}
		}

		// B*2232441: Allow melee events to abort State_ExitToAim.
		if(GetSubTask()->GetState() == CTaskExitVehicleSeat::State_ExitToAim && pEvent && pEvent->GetEventType() == EVENT_GIVE_PED_TASK)
		{
			const CEventGivePedTask *pGivePedTaskEvent = static_cast<const CEventGivePedTask*>(pEvent);
			if(pGivePedTaskEvent->GetTask() && pGivePedTaskEvent->GetTask()->GetTaskType()==CTaskTypes::TASK_MELEE)
			{
				return true;
			}
		}
	}

	if (pEvent && pEvent->GetEventType() == EVENT_DAMAGE && GetState() != State_ShuffleToSeat)
	{
		if (!pPed->GetIsAttachedInCar())
		{
			return true;
		}
		else if (m_moveNetworkHelper.IsNetworkActive())
		{
			// Blend into seated death
			if (m_moveNetworkHelper.GetBoolean(ms_DeathInSeatInterruptId))
			{
				SetRunningFlag(CVehicleEnterExitFlags::DontSetPedOutOfVehicle);
				return true;
			}
			else
			{
				//s32 iFlags = m_pVehicle->InheritsFromPlane() ? CPed::PVF_IgnoreSafetyPositionCheck|CPed::PVF_DontNullVelocity : CPed::PVF_NoCollisionUntilClear|CPed::PVF_IgnoreSafetyPositionCheck|CPed::PVF_DontNullVelocity;
				//pPed->SetPedOutOfVehicle(iFlags);
			}
		}
	}

	if (NetworkInterface::IsGameInProgress() && !pPed->IsLocalPlayer() && !pPed->IsNetworkClone() && GetState() == State_ExitSeat)
	{
		if (pEvent && pEvent->GetEventType() == EVENT_SCRIPT_COMMAND)
		{
			if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_EXIT_VEHICLE_SEAT && GetSubTask()->GetState() == State_Finish)
			{
				if (static_cast<const CEventScriptCommand*>(pEvent)->GetTask() && static_cast<const CEventScriptCommand*>(pEvent)->GetTask()->GetTaskType() == CTaskTypes::TASK_PARACHUTE)
				{
					return true;
				}
			}
			if (IsFlagSet(CVehicleEnterExitFlags::JumpOut) && !pPed->GetIsInVehicle())
			{
				return true;
			}
		}
	}

	if (GetState() > State_ReserveShuffleSeat && GetState() < State_Finish)
	{
		return false;
	}

    if(IsFlagSet(CVehicleEnterExitFlags::BeJacked))
    {
        return false;
    }

	return true;
}

bool CTaskExitVehicle::ShouldAbortFromInAirEvent(const aiEvent* pEvent) const
{
	if (!taskVerifyf(GetPed(), "Ped pointer was NULL"))
	{
		return false;
	}
	
	if(pEvent && pEvent->GetEventType() == EVENT_IN_AIR)
	{
		const CEventInAir *pInAirEvent = static_cast<const CEventInAir*>(pEvent);
		if(pInAirEvent->GetDisableSkydiveTransition() || pInAirEvent->GetCheckForDive())
		{
			return true;
		}

		if(GetPed()->GetPedResetFlag(CPED_RESET_FLAG_CanAbortExitForInAirEvent) || (!IsFlagSet(CVehicleEnterExitFlags::IsTransitionToSkyDive) && IsFlagSet(CVehicleEnterExitFlags::JumpOut) && !GetPed()->GetIsAttachedInCar()))
		{
			return true;
		}
	}

	if(!GetPed()->GetIsAttachedInCar() && pEvent && pEvent->GetEventType() == EVENT_GIVE_PED_TASK)
	{
		const CEventGivePedTask *pGivePedTaskEvent = static_cast<const CEventGivePedTask*>(pEvent);
		if(pGivePedTaskEvent->GetTask() && pGivePedTaskEvent->GetTask()->GetTaskType()==CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
		{
			CTaskComplexControlMovement* pMoveTask = static_cast<CTaskComplexControlMovement*>(pGivePedTaskEvent->GetTask());
			if(pMoveTask->GetMainSubTaskType() == CTaskTypes::TASK_FALL)
			{
				return true;
			}
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicle::DoAbort(const AbortPriority priority, const aiEvent* pEvent)
{
	//Grab the ped.
	CPed* pPed = GetPed();


	// B*1993370: Must set the DontSetPedOutOfVehicle flag here to prevent clone peds that are killed while exiting a vehicle from ragdolling while still inside the vehicle.
	// This is done in CTaskExitVehicle::ShouldAbort for local peds, but this is not called on clone tasks so clone peds were acting differently than local peds.
	// Can't check event type as it's NULL on clones so just checking ped health to see if they should be dead in the seat.
	if(pPed->GetNetworkObject() && pPed->GetNetworkObject()->IsClone())
	{
		if(pPed->ShouldBeDead() && GetState() != State_ShuffleToSeat)
		{
			if (NetworkInterface::IsPedInVehicleOnOwnerMachine(pPed) && m_moveNetworkHelper.IsNetworkActive())
			{
				// Blend into seated death
				if (m_moveNetworkHelper.GetBoolean(ms_DeathInSeatInterruptId))
				{
					SetRunningFlag(CVehicleEnterExitFlags::DontSetPedOutOfVehicle);
				}
			}
		}
	}

	if (pEvent && pEvent->GetEventType() == EVENT_DAMAGE)
	{
		TUNE_GROUP_FLOAT(EXIT_VEHICLE_TUNE, DAMAGE_BLEND_OUT_DURATION, NORMAL_BLEND_DURATION, 0.0f, 1.0f, 0.001f);
		pPed->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, DAMAGE_BLEND_OUT_DURATION);
	}
	
	if (priority == ABORT_PRIORITY_IMMEDIATE)
	{
		// If we're aborting immediately and have started to exit, make sure the ped is left outside of the vehicle
		if (GetState() > State_ReserveShuffleSeat)
		{
			if (!IsFlagSet(CVehicleEnterExitFlags::DontSetPedOutOfVehicle))
			{
				if (pPed->GetIsInVehicle())
				{
					u32 flags = CPed::PVF_DontResetDefaultTasks;

					if (pPed->IsNetworkClone() && pPed->GetIsDeadOrDying())
					{
						// ignore the safety check if the ped is dead, as it will be falling out of the vehicle
						flags |= CPed::PVF_IgnoreSafetyPositionCheck;
					}

					pPed->SetPedOutOfVehicle(flags);
				}
			}
		}
	}
	
	if(ShouldAbortFromInAirEvent(pEvent))
	{
		//Note that the ped exited mid-air.
		CTaskExitVehicle::OnExitMidAir(pPed, m_pVehicle);
	}
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::ProcessPreFSM()
{
	if (!m_pVehicle)
	{
		return FSM_Quit;
	}

    CPed *pPed = GetPed();

    // a network ped can be told to exit a vehicle they are not currently in on
    // the local machine if lag is high or the ped is warped into the vehicle the same frame it is told to exit it
    if(!pPed || (pPed->IsNetworkClone() && m_pVehicle != pPed->GetMyVehicle()))
    {
        return FSM_Quit;
    }

    if(pPed->IsNetworkClone())
    {
        // if we lose our target entry point when we've started getting out proper we need to quit.
        // this can happen if a ped changes owner while exiting a vehicle
        if(m_iTargetEntryPoint == -1 && GetState() >= State_ExitSeat)
        {
            return FSM_Quit;
        }

        if(pPed->GetUsingRagdoll())
        {
            return FSM_Quit;
        }
    }

	pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableCellphoneAnimations, true);	
	pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableDynamicCapsuleRadius, true);	
	pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableGaitReduction, true);
	pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableMotionBaseVelocityOverride, true);

	CTaskVehicleFSM::ProcessBlockRagdollActivation(*pPed, m_moveNetworkHelper);

	if (GetState() > State_PickDoor)
	{
		s32 iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
		if (iSeatIndex > -1)
		{
			if(m_bIsRappel)
			{
				fwMvClipSetId descendClipSet = SelectRappelClipsetForHeliSeat(m_pVehicle, iSeatIndex);
				CTaskVehicleFSM::RequestVehicleClipSet(descendClipSet, SP_High);
			}
		}

		const fwMvClipSetId exitClipsetId = CTaskExitVehicleSeat::GetExitClipsetId(m_pVehicle, m_iTargetEntryPoint, m_iRunningFlags, pPed);
		CTaskVehicleFSM::RequestVehicleClipSet(exitClipsetId, SP_High);
	}

	// If a timer is set, count down
	if (m_fDelayTime > 0.0f )
	{
		m_fDelayTime -= GetTimeStep();
		if (m_fDelayTime <= 0.0f)
		{
			SetTimerRanOut(true);
		}
	}

	// If we're not in the vehicle we've been told to get out of we're finished
	if (m_pVehicle != pPed->GetMyVehicle())
	{
		return FSM_Quit;
	}

	const CVehicleEntryPointAnimInfo* pClipInfo = m_pVehicle->GetVehicleModelInfo()->GetEntryPointAnimInfo(0);
	if (taskVerifyf(pClipInfo, "Vehicle has no entry clip info"))
	{
		if (GetState() < State_ExitSeat && (!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) || !taskVerifyf(m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed) >= 0, "Peds seat index is invalid")))
		{
			return FSM_Quit;
		}
	}

    if(pPed->GetNetworkObject())
    {
		// peds exiting a vehicle must be synced in the network game, but peds in cars are not
		// synchronised by default, in order to save bandwidth as they don't do much
        NetworkInterface::ForceSynchronisation(pPed);

		// prevent the vehicle migrating while the ped is exiting
		if (pPed->GetIsInVehicle() && 
			m_pVehicle->GetNetworkObject() &&
			SafeCast(CNetObjPed, pPed->GetNetworkObject())->MigratesWithVehicle() && 
			!m_pVehicle->GetNetworkObject()->IsClone() &&
			!IsFlagSet(CVehicleEnterExitFlags::BeJacked))
		{
			taskAssertf(GetState() <= State_WaitForOtherPedToExit || GetState() >= State_CloseDoor || !m_pVehicle->GetNetworkObject()->IsPendingOwnerChange(), "%s is exiting %s while it is migrating", pPed->GetNetworkObject()->GetLogName(), m_pVehicle->GetNetworkObject()->GetLogName());

			const unsigned TIME_TO_DELAY_VEHICLE_MIGRATION = 1000;
			NetworkInterface::DelayMigrationForTime(m_pVehicle, TIME_TO_DELAY_VEHICLE_MIGRATION);
		}
	}

	if (pPed->IsLocalPlayer())
	{
		CControl* pControl = pPed->GetControlFromPlayer();
		if (!m_bWantsToGoIntoCover)
		{
			m_bWantsToGoIntoCover = pControl && pControl->GetPedCover().IsPressed();
		}	
	}

	if( ( m_pVehicle->InheritsFromSubmarine() ||
		  m_pVehicle->InheritsFromSubmarineCar() ) &&
		 IsFlagSet(CVehicleEnterExitFlags::ExitUnderWater))
	{
		CSubmarine* pSubmarine = static_cast<CSubmarine*>(m_pVehicle.Get());
		CSubmarineHandling* pSubHandling = pSubmarine->GetSubHandling();
		pSubHandling->ForceNeutralBuoyancy(ms_Tunables.m_JumpOutofSubNeutralBuoyancyTime);
	}

	pPed->SetPedResetFlag( CPED_RESET_FLAG_IsEnteringOrExitingVehicle, true);
	pPed->SetPedResetFlag( CPED_RESET_FLAG_IsExitingVehicle, true);

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter();
			FSM_OnUpdate
				return Start_OnUpdate();	

		FSM_State(State_WaitForCarToSlow)
			FSM_OnEnter
				WaitForCarToSlow_OnEnter();
			FSM_OnUpdate
				return WaitForCarToSlow_OnUpdate();

		FSM_State(State_WaitForEntryToBeClearOfPeds)
			FSM_OnUpdate
				return WaitForEntryToBeClearOfPeds_OnUpdate();

		FSM_State(State_WaitForValidEntry)
			FSM_OnUpdate
				return WaitForValidEntry_OnUpdate();

		FSM_State(State_DelayExitForTime)
			FSM_OnEnter
				DelayExitForTime_OnEnter();
			FSM_OnUpdate
				return DelayExitForTime_OnUpdate();

		FSM_State(State_PickDoor)
			FSM_OnEnter
				return PickDoor_OnEnter();
			FSM_OnUpdate
				return PickDoor_OnUpdate();

		FSM_State(State_WaitForOtherPedToExit)
			FSM_OnUpdate
				return WaitForOtherPedToExit_OnUpdate();

		FSM_State(State_WaitForCarToStop)
			FSM_OnEnter
				WaitForCarToStop_OnEnter();
			FSM_OnUpdate
				return WaitForCarToStop_OnUpdate();

		FSM_State(State_WaitForCarToSettle)
			FSM_OnEnter
				return WaitForCarToSettle_OnEnter();
			FSM_OnUpdate
				return WaitForCarToSettle_OnUpdate();

		FSM_State(State_ReserveShuffleSeat)
			FSM_OnUpdate
				return ReserveShuffleSeat_OnUpdate();

		FSM_State(State_StreamAssetsForShuffle)
			FSM_OnUpdate
				return StreamAssetsForShuffle_OnUpdate();

		FSM_State(State_StreamAssetsForExit)
			FSM_OnUpdate
				return StreamAssetsForExit_OnUpdate();

		FSM_State(State_ShuffleToSeat)
			FSM_OnEnter
				ShuffleToSeat_OnEnter();
			FSM_OnUpdate
				return ShuffleToSeat_OnUpdate();

		FSM_State(State_ReserveSeat)
			FSM_OnUpdate
				return ReserveSeat_OnUpdate();

		FSM_State(State_ReserveDoor)
			FSM_OnUpdate
				return ReserveDoor_OnUpdate();

		FSM_State(State_ExitSeat)
			FSM_OnEnter
				return ExitSeat_OnEnter();
			FSM_OnUpdate
				return ExitSeat_OnUpdate();
			FSM_OnExit
				return ExitSeat_OnExit();

		FSM_State(State_CloseDoor)
			FSM_OnEnter
				CloseDoor_OnEnter();
			FSM_OnUpdate
				return CloseDoor_OnUpdate();
			FSM_OnExit
				return CloseDoor_OnExit();

		FSM_State(State_ExitSeatToIdle)
			FSM_OnEnter
				ExitSeatToIdle_OnEnter();
			FSM_OnUpdate
				return ExitSeatToIdle_OnUpdate();

		FSM_State(State_UnReserveSeatToIdle)
			FSM_OnUpdate
				return UnReserveSeatToIdle_OnUpdate();

		FSM_State(State_UnReserveDoorToIdle)
			FSM_OnUpdate
				return UnReserveDoorToIdle_OnUpdate();

		FSM_State(State_UnReserveSeatToFinish)
			FSM_OnUpdate
				return UnReserveSeatToFinish_OnUpdate();

		FSM_State(State_UnReserveDoorToFinish)
			FSM_OnUpdate
				return UnReserveDoorToFinish_OnUpdate();

		FSM_State(State_UnReserveSeatToClimbDown)
			FSM_OnUpdate
				return UnReserveSeatToClimbDown_OnUpdate();

		FSM_State(State_UnReserveDoorToClimbDown)
			FSM_OnUpdate
				return UnReserveDoorToClimbDown_OnUpdate();	

		FSM_State(State_UnReserveSeatToGetUp)
			FSM_OnUpdate
				return UnReserveSeatToGetUp_OnUpdate();

		FSM_State(State_UnReserveDoorToGetUp)
			FSM_OnUpdate
				return UnReserveDoorToGetUp_OnUpdate();	

		FSM_State(State_SetPedOut)
			FSM_OnEnter
				SetPedOut_OnEnter();
			FSM_OnUpdate
				return SetPedOut_OnUpdate();

		FSM_State(State_ClimbDown)
			FSM_OnEnter
				return ClimbDown_OnEnter();
			FSM_OnUpdate
				return ClimbDown_OnUpdate();

		FSM_State(State_GetUp)
			FSM_OnEnter
				return GetUp_OnEnter();
			FSM_OnUpdate
				return GetUp_OnUpdate();

		FSM_State(State_DetermineFleeExitPosition)
			FSM_OnUpdate
				return DetermineFleeExitPosition_OnUpdate();
			
		FSM_State(State_WaitForFleePath)
			FSM_OnUpdate
				return WaitForFleePath_OnUpdate();
			
		FSM_State(State_Finish)
			FSM_OnUpdate
				return Finish_OnUpdate();

	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::UpdateClonedFSM( s32 iState, CTask::FSM_Event iEvent )
{
	if (IsRunningLocally())
	{
		return UpdateFSM(iState, iEvent);
	}

    // if the state change is not handled by the clone FSM block below
	// we need to keep the state in sync with the values received from the network
	if(!StateChangeHandledByCloneFSM(iState))
	{
		if(iEvent == OnUpdate)
		{
			if(CanUpdateStateFromNetwork())
			{
				SetTaskState(GetStateFromNetwork());
			}
		}
	}

FSM_Begin

		FSM_State(State_Start)
            FSM_OnUpdate
                Start_OnUpdateClone();

		FSM_State(State_StreamAssetsForShuffle)
			FSM_OnUpdate
				return StreamAssetsForShuffle_OnUpdate();

		FSM_State(State_StreamAssetsForExit)
			FSM_OnUpdate
				return StreamAssetsForExit_OnUpdate();

        FSM_State(State_ShuffleToSeat)
			FSM_OnEnter
				ShuffleToSeat_OnEnter();
			FSM_OnUpdate
				return ShuffleToSeat_OnUpdate();

		FSM_State(State_ExitSeat)
			FSM_OnEnter
				ExitSeat_OnEnter();
			FSM_OnUpdate
				return ExitSeat_OnUpdate();
			FSM_OnExit
				return ExitSeat_OnExit();

		FSM_State(State_CloseDoor)
			FSM_OnEnter
				CloseDoor_OnEnter();
			FSM_OnUpdate
				return CloseDoor_OnUpdate();
			FSM_OnExit
				return CloseDoor_OnExit();

		FSM_State(State_ExitSeatToIdle)
			FSM_OnEnter
				ExitSeatToIdle_OnEnter();
			FSM_OnUpdate
				return ExitSeatToIdle_OnUpdate();

		FSM_State(State_WaitForNetworkState)
			FSM_OnUpdate
				return WaitForNetworkState_OnUpdate();

		FSM_State(State_ClimbDown)
			FSM_OnEnter
				return ClimbDown_OnEnter();
			FSM_OnUpdate
				return ClimbDown_OnUpdate();

		FSM_State(State_GetUp)
			FSM_OnEnter
				return GetUp_OnEnter();
			FSM_OnUpdate
				return GetUp_OnUpdate();

		FSM_State(State_Finish)
            FSM_OnEnter
                Finish_OnEnterClone();
			FSM_OnUpdate
				return Finish_OnUpdateClone();

    FSM_State(State_WaitForCarToSlow)
	FSM_State(State_WaitForEntryToBeClearOfPeds)
	FSM_State(State_WaitForValidEntry)
	FSM_State(State_DelayExitForTime)
	FSM_State(State_PickDoor)
	FSM_State(State_WaitForOtherPedToExit)
	FSM_State(State_WaitForCarToStop)
	FSM_State(State_WaitForCarToSettle)
    FSM_State(State_ReserveShuffleSeat)
    FSM_State(State_ReserveSeat)
	FSM_State(State_ReserveDoor)
    FSM_State(State_UnReserveSeatToIdle)
    FSM_State(State_UnReserveDoorToIdle)
	FSM_State(State_UnReserveSeatToFinish)
	FSM_State(State_UnReserveDoorToFinish)
	FSM_State(State_UnReserveSeatToClimbDown)
	FSM_State(State_UnReserveDoorToClimbDown)
	FSM_State(State_UnReserveSeatToGetUp)
	FSM_State(State_UnReserveDoorToGetUp)
    FSM_State(State_SetPedOut)
    FSM_State(State_DetermineFleeExitPosition)
    FSM_State(State_WaitForFleePath)

FSM_End
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::HandlesDeadPed()
{
	if (NetworkInterface::IsGameInProgress() && GetState() < State_StreamAssetsForShuffle)
	{
		return false;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::StateChangeHandledByCloneFSM(s32 iState)
{
	switch(iState)
	{
    case State_Start:
	case State_StreamAssetsForExit:
	case State_StreamAssetsForShuffle:
    case State_ShuffleToSeat:
    case State_ExitSeat:
    case State_CloseDoor:
	case State_ExitSeatToIdle:
	case State_ClimbDown:
    case State_Finish:
		return true;
	default:
		return false;
	}
}

//////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::CanUpdateStateFromNetwork()
{
	s32 iStateFromNetwork = GetStateFromNetwork();
	s32 iCurrentState     = GetState();

	if(iStateFromNetwork != iCurrentState)
	{
		// we can't go back to the open door state from certain states
		if(iStateFromNetwork == State_ExitSeatToIdle)
		{
			if(!IsMoveTransitionAvailable(State_ExitSeatToIdle))
			{
				return false;
			}
		}
		else if(iCurrentState == State_GetUp && iStateFromNetwork == State_ExitSeat)
		{
			return false;
		}
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::ShouldInterruptDueToSequenceTask() const
{
	const CPed* pPed = GetPed();

	// If we're running the exit vehicle task as part of a sequence and its not the last task, early out once clear of the vehicle
	if (!pPed->IsNetworkClone())
	{
		CTaskUseSequence* pSeqTask = static_cast<CTaskUseSequence*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SEQUENCE));
		if (pSeqTask)
		{
			const CTaskSequenceList* pTaskSeqList = pSeqTask->GetTaskSequenceList();
			if (pTaskSeqList)
			{
				const s32 iCurrentIndex = pSeqTask->GetPrimarySequenceProgress();
				const s32 iLastIndex = pTaskSeqList->GetTaskList().GetFirstEmptySlotIndex();
				if (iCurrentIndex > -1 && iLastIndex > -1 && iCurrentIndex + 1 < iLastIndex)
				{
					return true;
				}
			}
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::ProcessPostFSM()
{
	CPed* pPed = GetPed();

	if (m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint))
	{
		const CVehicleEntryPointAnimInfo* pClipInfo = m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint);
		if (pClipInfo && pClipInfo->GetHasClimbDown() && pPed->GetAttachParent())
		{
			if (GetState() == State_ClimbDown || GetState() == State_CloseDoor || GetState() == State_ExitSeatToIdle)
			{
				pPed->SetPedResetFlag(CPED_RESET_FLAG_ApplyAnimatedVelocityWhilstAttached, true);
			}
		}	
	}

    if(!pPed->IsNetworkClone())
    {
	    // If the task should currently have the door reserved, check that it does
	    if (GetState() > State_ReserveDoor && GetState() < State_UnReserveDoorToFinish)
	    {
            if(taskVerifyf(m_pVehicle, "NULL vehicle pointer in CTaskExitVehicle::ProcessPostFSM!"))
            {
		        CComponentReservation* pComponentReservation = m_pVehicle->GetComponentReservationMgr()->GetDoorReservation(m_iTargetEntryPoint);
		        if (pComponentReservation)
		        {
					if (pComponentReservation->GetPedUsingComponent() == pPed)
			        {
						pComponentReservation->SetPedStillUsingComponent(pPed);
					}
					else if (m_pVehicle->GetEntryInfo(m_iTargetEntryPoint)->GetMPWarpInOut() && m_pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_USE_FULL_ANIMS_FOR_MP_WARP_ENTRY_POINTS))
					{
						CComponentReservationManager::ReserveVehicleComponent(pPed, m_pVehicle, pComponentReservation);
					}
		        }
            }
	    }

	    // If the task should currently have the seat reserved, check that it does
	    if (GetState() > State_ReserveSeat && GetState() < State_UnReserveSeatToFinish)
	    {
            if(taskVerifyf(m_pVehicle, "NULL vehicle pointer in CTaskExitVehicle::ProcessPostFSM!"))
            {
		        CComponentReservation* pComponentReservation = m_pVehicle->GetComponentReservationMgr()->GetSeatReservation(m_iTargetEntryPoint, SA_directAccessSeat);
		        if (pComponentReservation && pComponentReservation->GetPedUsingComponent() == pPed)
		        {
			        pComponentReservation->SetPedStillUsingComponent(pPed);
		        }
            }
	    }
    }

	// Force the do nothing motion state
// 	if (GetState()>State_ReserveShuffleSeat)
// 	{
// 		GetPed()->ForceMotionStateThisFrame(CPedMotionStates::MotionState_DoNothing);
// 	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::ProcessPhysics(float UNUSED_PARAM(fTimeStep), int UNUSED_PARAM(nTimeSlice))
{
	if (m_pVehicle)
	{
		if (GetState() == State_ClimbDown || (GetState() == State_ExitSeatToIdle && m_pVehicle->InheritsFromPlane()))
		{
			float fClipPhase = 0.0f;
			const crClip* pClip = GetClipAndPhaseForState(fClipPhase);
			if (pClip)
			{
				// Update the target in case the vehicle is moving
				Vector3 vTargetPosition(Vector3::ZeroType);
				Quaternion qTargetOrientation(Quaternion::IdentityType);
				if (GetState() == State_ClimbDown)
				{
					ComputeClimbDownTarget(vTargetPosition, qTargetOrientation);
				}
				else
				{
					ComputeGetOutInitialOffset(vTargetPosition, qTargetOrientation);
				}
				m_PlayAttachedClipHelper.SetTarget(vTargetPosition, qTargetOrientation);

				// Get the initial position relative to the attach point, we compute the new position from the anim absolutely
				Vector3 vNewPedPosition(Vector3::ZeroType);
				Quaternion qNewPedOrientation(Quaternion::IdentityType);
				if (GetState() == State_ClimbDown)
				{
					ComputeGetOutInitialOffset(vNewPedPosition, qNewPedOrientation);
				}
				else
				{
					CModelSeatInfo::CalculateExitSituation(m_pVehicle, vNewPedPosition, qNewPedOrientation, m_iTargetEntryPoint);
				}

				// Update the situation from the current phase in the clip
				m_PlayAttachedClipHelper.ComputeSituationFromClip(pClip, 0.0f, fClipPhase, vNewPedPosition, qNewPedOrientation);
				qNewPedOrientation.Normalize();

				TUNE_GROUP_BOOL(PLANE_TUNE, DO_MOVER_FIXUP, true);
				if (DO_MOVER_FIXUP)
				{
					// Compute the offsets we'll need to fix up
					Vector3 vOffsetPosition(Vector3::ZeroType);
					Quaternion qOffsetOrientation(Quaternion::IdentityType);
					m_PlayAttachedClipHelper.ComputeOffsets(pClip, fClipPhase, 1.0f, vNewPedPosition, qNewPedOrientation, vOffsetPosition, qOffsetOrientation);

					// Apply the fixup
					m_PlayAttachedClipHelper.ApplyVehicleExitOffsets(vNewPedPosition, qNewPedOrientation, pClip, vOffsetPosition, qOffsetOrientation, fClipPhase);	
				}

				// Pass in the new position and orientation in global space, this will get transformed relative to the vehicle for attachment
				CPed& rPed = *GetPed();
				CTaskVehicleFSM::UpdatePedVehicleAttachment(&rPed, VECTOR3_TO_VEC3V(vNewPedPosition), QUATERNION_TO_QUATV(qNewPedOrientation));
				return true;
			}	
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::ProcessMoveSignals()
{
	if (GetEntity() && GetPed()->IsLocalPlayer())
	{
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_BlockIkWeaponReactions, true);
	}	

	if (m_bCurrentAnimFinished)
	{
		// Already finished, nothing to do.
		return false;
	}

	const int state = GetState();
	if(state == State_ExitSeatToIdle)
	{
		// Check if done running the close door Move state.
		if (IsMoveNetworkStateFinished(ms_ToIdleAnimFinishedId, ms_ToIdleAnimPhaseId))
		{
			// Done, we need no more updates.
			m_bCurrentAnimFinished = true;
			return false;
		}

		// Still waiting.
		return true;
	}

	// Nothing to do.
	return false;
}

////////////////////////////////////////////////////////////////////////////////

CTaskInfo* CTaskExitVehicle::CreateQueriableState() const
{
    unsigned exitSeatMotionState = CClonedExitVehicleInfo::EXIT_SEAT_STILL;
    return rage_new CClonedExitVehicleInfo( GetState(), m_pVehicle, m_iSeatRequestType, m_iSeat, m_iTargetEntryPoint, m_iRunningFlags, m_pPedJacker, exitSeatMotionState, m_BeJackedClipsetId, m_BeJackedClipId, m_bHasClosedDoor, m_bIsRappel, m_bSubTaskExitRateSet, m_bCloneShouldUseCombatExitRate, m_vFleeExitPosition);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicle::ReadQueriableState( CClonedFSMTaskInfo* pTaskInfo )
{
	taskAssert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_EXIT_VEHICLE_CLONED);

    m_bHasClosedDoor = static_cast<CClonedExitVehicleInfo *>(pTaskInfo)->HasClosedDoor();
    m_bIsRappel      = static_cast<CClonedExitVehicleInfo *>(pTaskInfo)->IsRappel();
	m_bCloneShouldUseCombatExitRate = static_cast<CClonedExitVehicleInfo *>(pTaskInfo)->ShouldUseCombatExitRate(); 
	m_bSubTaskExitRateSet = static_cast<CClonedExitVehicleInfo *>(pTaskInfo)->SubTaskExitRateSet();  
	m_vFleeExitPosition = static_cast<CClonedExitVehicleInfo *>(pTaskInfo)->GetFleeTargetPosition();  

	// Call the base implementation - syncs the state
	CTaskVehicleFSM::ReadQueriableState(pTaskInfo);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicle::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Finish);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::IsInScope(const CPed* pPed)
{
	if(pPed && m_pVehicle && (m_pVehicle->InheritsFromPlane() || m_pVehicle->InheritsFromHeli()) )
	{
		// On exiting to skydive / parachuting, we need to make sure the task runs through 
		// so on CleanUp the ped is removed from the vehicle properly via SetPedOutOfVehicle 
		// otherwise on the next update on a clone, CNetObjPed::UpdateVehicleForPed will flush 
		// the task tree and kill off the recently cloned TaskFall & TaskParachute as it 
		// tries to set the clone ped out of the vehicle itself.
		if(IsFlagSet(CVehicleEnterExitFlags::IsTransitionToSkyDive))
		{
			return true;
		}
	}

	return CTaskFSMClone::IsInScope(pPed);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::CanBeReplacedByCloneTask(u32 taskType) const
{
    const CPed *pPed = GetPed();

    if(pPed)
    {
        if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_EXIT_VEHICLE_SEAT)
        {
            // if we are still playing the jump out of seat animation we don't want to replace this task with the
            // jump and roll from vehicle NM task until we have finished and switched to ragdoll or gone passed the 'CanSwitchToNm' anim event
            if(GetSubTask()->GetState() == CTaskExitVehicleSeat::State_JumpOutOfSeat)
            {
                if(taskType == CTaskTypes::TASK_NM_CONTROL)
                {
                    if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_NM_JUMP_ROLL_FROM_ROAD_VEHICLE))
                    {
						float fClipPhase = 0.0f;
						const crClip* pClip = static_cast<const CTaskExitVehicleSeat*>(GetSubTask())->GetClipAndPhaseForState(fClipPhase);
						float fEventPhase = 1.0f;
						if (pClip == NULL || !CClipEventTags::FindEventPhase(pClip, CClipEventTags::CanSwitchToNm, fEventPhase) || fClipPhase < fEventPhase)
						{
							bool bRidingBicycle = pPed->GetMyVehicle() && pPed->GetMyVehicle()->InheritsFromBicycle();

							if(!bRidingBicycle)
							{
								return false;
							}
						}
                    }
                }
            }
        }

        // if we are a dead ped being jacked we don't let the dead/dying task replace us
        if(taskType == CTaskTypes::TASK_DYING_DEAD)
        {
            if(pPed->GetIsDeadOrDying() && IsFlagSet(CVehicleEnterExitFlags::BeJacked))
            {
                return false;
            }
        }

        // don't allow parachuting tasks to replace the exit vehicle task - we want to
        // delay this until the ped is clear from the vehicle
        if(taskType == CTaskTypes::TASK_PARACHUTE)
        {
            return false;
        }
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicle::Start_OnEnter()
{
	taskAssert(m_pVehicle);

	m_bVehicleTaskAssigned = false;

	if (!m_pVehicle->IsNetworkClone())
	{
		AssignVehicleTask();
	}

	CPed& rPed = *GetPed();
	if (rPed.WantsToMoveQuickly())
	{
		SetRunningFlag(CVehicleEnterExitFlags::UseFastClipRate);
	}

	// Cache the seat offset relative to the vehicle in case it moves so we don't need to call GetGlobalMtx
	// Assumes the ped was on the seat to begin with
	if (rPed.IsLocalPlayer())
	{
		m_vSeatOffset = UnTransformFull(m_pVehicle->GetTransform().GetMatrix(), rPed.GetTransform().GetPosition());
	}

	//	Snap heli headsets off ...if not in FPS mode in SP!
	bool bIsKeepingCurrentHelmet = NetworkInterface::IsGameInProgress() && rPed.GetIsKeepingCurrentHelmetInVehicle();
	bool bInFPSModeInSP = !NetworkInterface::IsGameInProgress() && (rPed.IsInFirstPersonVehicleCamera() || rPed.IsFirstPersonShooterModeEnabledForPlayer(false));
	if (!bIsKeepingCurrentHelmet && (m_pVehicle->GetVehicleType() == VEHICLE_TYPE_HELI || m_pVehicle->GetVehicleType() == VEHICLE_TYPE_PLANE) && !bInFPSModeInSP)
	{
		//Reset the override indexs
		//HACK: A different helmet is used for DUSTER and STUNT for single player characters only, make sure it gets removed correctly
		const CPedModelInfo* pPedModelInfo = rPed.GetPedModelInfo();
		const CVehicleModelInfo* pVehicleModelInfo = m_pVehicle->GetVehicleModelInfo();
		if(pVehicleModelInfo && pVehicleModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_PLANE_WEAR_ALTERNATIVE_HELMET))
		{
			s32 iHeadPropIndex = CPedPropsMgr::GetPedPropIdx(GetPed(), ANCHOR_HEAD);
			if( (pPedModelInfo->GetModelNameHash() == MI_PLAYERPED_PLAYER_TWO.GetName().GetHash() && iHeadPropIndex == ALTERNATIVE_FLIGHT_HELMET_PLAYER_TWO) ||
				(pPedModelInfo->GetModelNameHash() == MI_PLAYERPED_PLAYER_ONE.GetName().GetHash() && iHeadPropIndex == ALTERNATIVE_FLIGHT_HELMET_PLAYER_ONE) ||
				(pPedModelInfo->GetModelNameHash() == MI_PLAYERPED_PLAYER_ZERO.GetName().GetHash() && iHeadPropIndex == ALTERNATIVE_FLIGHT_HELMET_PLAYER_ZERO) )
			{
				rPed.GetHelmetComponent()->SetHelmetPropIndex(-1);
				rPed.GetHelmetComponent()->SetHelmetTexIndex(-1);
			}
		}

		rPed.GetHelmetComponent()->DisableHelmet();
	}
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::Start_OnUpdateClone()
{
    if(GetStateFromNetwork() == State_Finish)
    {
        SetTaskState(State_Finish);
    }
    else if(GetStateFromNetwork() > State_Start || m_bIsRappel)
    {
        // we can't do anything until we have a valid seat to exit from
        if (m_iSeat != -1)
        {
            CPed* pPed = GetPed();

            if (m_iSeat == m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed))
            {
                if(m_bIsRappel)
                {
					SetTaskState(State_StreamAssetsForExit);
				}
                else
                {
                    if(GetStateFromNetwork() == State_StreamAssetsForExit ||
                       GetStateFromNetwork() == State_ExitSeat)
                    {
                        SetTaskState(State_StreamAssetsForExit);
                    }
                    else if(GetStateFromNetwork() == State_StreamAssetsForShuffle ||
                            GetStateFromNetwork() == State_ShuffleToSeat)
                    {
                        SetTaskState(State_StreamAssetsForShuffle);
                    }
                }
            }
            else
            {
                // quit the task if the seat information isn't up-to-date
                SetTaskState(State_Finish);
            }
        }
    }

    return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::Start_OnUpdate()
{
	TUNE_BOOL(FORCE_EXIT_TO_AIM, false);
	if (FORCE_EXIT_TO_AIM)
	{
		m_iRunningFlags.BitSet().Set(CVehicleEnterExitFlags::ExitToAim);
	}

	if (!m_bVehicleTaskAssigned && !m_pVehicle->IsNetworkClone())
	{
		// assign the vehicle AI task to the vehicle once it becomes local
		AssignVehicleTask();
	}

	if(m_pVehicle->IsSeatIndexValid(m_iSeat) && !RequestClips())
	{
		return FSM_Continue;
	}

	// the ped cannot exit the vehicle if it is migrating, as it needs to migrate with the vehicle
	if (GetPed()->GetNetworkObject() && 
		m_pVehicle->GetNetworkObject() &&
		SafeCast(CNetObjPed, GetPed()->GetNetworkObject())->MigratesWithVehicle() && 
		!m_pVehicle->GetNetworkObject()->IsClone() && 
		m_pVehicle->GetNetworkObject()->IsPendingOwnerChange())
	{
		return FSM_Continue;
	}

	if (IsFlagSet(CVehicleEnterExitFlags::BeJacked))
	{
		SetTaskState(State_PickDoor);
		return FSM_Continue;
	}

	const CVehicleEntryPointAnimInfo* pClipInfo = m_pVehicle->GetVehicleModelInfo()->GetEntryPointAnimInfo(0);
	if (taskVerifyf(pClipInfo, "Vehicle has no entry clip info"))
	{
		CPed* pPed = GetPed();

		const bool bLocalPlayerInNetworkGame = NetworkInterface::IsGameInProgress() && pPed->IsLocalPlayer();
		// If car is moving and we should wait for it to stop, move to that state
		if ((!pPed->IsPlayer() || bLocalPlayerInNetworkGame) &&
			!IsFlagSet(CVehicleEnterExitFlags::DeadFallOut) &&
			!IsFlagSet(CVehicleEnterExitFlags::BeJacked) &&
			!IsFlagSet(CVehicleEnterExitFlags::ForcedExit) &&
			!IsFlagSet(CVehicleEnterExitFlags::DontWaitForCarToStop) &&
			!IsFlagSet(CVehicleEnterExitFlags::JumpOut))
		{
			if (!m_pVehicle->CanPedStepOutCar(pPed, false, !pPed->IsPlayer()) )
			{
				SetTaskState(State_WaitForCarToSlow);
				return FSM_Continue;
			}
		}

		if ( m_pVehicle->GetLayoutInfo()->GetOnlyExitIfDoorIsClosed() && !IsFlagSet(CVehicleEnterExitFlags::BeJacked))
		{
			const s32 iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
			if (m_pVehicle->IsSeatIndexValid(iSeatIndex))
			{
				const s32 iEntryIndex = m_pVehicle->GetDirectEntryPointIndexForSeat(iSeatIndex);
				if (m_pVehicle->IsEntryIndexValid(iEntryIndex))
				{
					CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(m_pVehicle, iEntryIndex);
					if (pDoor && !pDoor->GetIsClosed() && GetTimeInState() < 1.0f)
					{
						return FSM_Continue;
					}
				}
			}
		}

		if (IsFlagSet(CVehicleEnterExitFlags::DelayForTime))
		{
			//! If we are trying to do a flee exit in a boat, just jump out for now. Need to do a good bit of work to get this looking good, and this is
			//! a simple solution for now.
			if(m_pVehicle->InheritsFromBoat() && IsFlagSet(CVehicleEnterExitFlags::IsFleeExit))
			{
				SetRunningFlag(CVehicleEnterExitFlags::GettingOffBecauseDriverJacked);
				SetRunningFlag(CVehicleEnterExitFlags::JumpOut);
				ClearRunningFlag(CVehicleEnterExitFlags::IsFleeExit);
			}

			SetTaskState(State_DelayExitForTime);
			return FSM_Continue;
		}
		else
		{
			SetTaskState(State_WaitForEntryToBeClearOfPeds);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicle::WaitForCarToSlow_OnEnter()
{
	// Random ped trapped in the players car
	float fTime = 0.0f;
	if (IsFlagSet(CVehicleEnterExitFlags::DelayForTime))
	{
		fTime = m_fDelayTime;
	}
	SetNewTask(rage_new CTaskWaitForSteppingOut(fTime));

	ClearRunningFlag(CVehicleEnterExitFlags::VehicleIsOnSide);
	ClearRunningFlag(CVehicleEnterExitFlags::VehicleIsUpsideDown);
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::WaitForCarToSlow_OnUpdate()
{
	// If the task should delay until the timer runs out, wait
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetTaskState(State_WaitForEntryToBeClearOfPeds);
		return FSM_Continue;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::WaitForEntryToBeClearOfPeds_OnUpdate()
{
	if (!PedShouldWaitForPedsToMove())
	{
		if (m_pVehicle && CVehicle::GetVehicleOrientation(*m_pVehicle) == CVehicle::VO_Upright)
		{
			// B*3554999 wait at least a frame for ai peds to prevent killing the frame rate
			if (!NetworkInterface::IsGameInProgress() || GetPed()->IsAPlayerPed() || GetTimeInState() > 0.0f)
			{
				SetTaskState(State_PickDoor);
				return FSM_Continue;
			}
		}
		else
		{
			SetTaskState(State_WaitForCarToSettle);
			return FSM_Continue;
		}
	}
	else
	{
		// Quit if he got back in
		const CPed& rPed = *GetPed();
		CPedGroup* pPedsGroup = rPed.GetPedsGroup();
		if (pPedsGroup)
		{
			CPed* pLeader = pPedsGroup->GetGroupMembership()->GetLeader();
			if (pLeader && pLeader != &rPed)
			{
				if (pLeader->GetIsInVehicle() && (pLeader->GetMyVehicle() == rPed.GetMyVehicle()))
				{
					if (pLeader->GetAttachState() != ATTACH_STATE_PED_EXIT_CAR)
					{
						SetTaskState(State_Finish);
						return FSM_Continue;
					}
				}
			}
		}
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::WaitForValidEntry_OnUpdate()
{
	static dev_float TIME_TO_WARP_PLAYER_OUT = 5.0f;
	static dev_float TIME_BETWEEN_ENTRY_CHECKS = 0.1f;
	
	CPed& rPed = *GetPed();
	if (rPed.IsLocalPlayer() && GetTimeRunning() > TIME_TO_WARP_PLAYER_OUT)
	{
		ClearRunningFlag(CVehicleEnterExitFlags::UseDirectEntryOnly);
		rPed.SetPedConfigFlag(CPED_CONFIG_FLAG_WaitForDirectEntryPointToBeFreeWhenExiting, false);
		SetTaskState(State_PickDoor);
		return FSM_Continue;
	}
	else if (GetTimeInState() > TIME_BETWEEN_ENTRY_CHECKS)
	{
		SetTaskState(State_PickDoor);
		return FSM_Continue;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicle::DelayExitForTime_OnEnter()
{
	if (!IsFlagSet(CVehicleEnterExitFlags::JumpOut))
	{
		SetNewTask(rage_new CTaskWaitForSteppingOut(m_fDelayTime));
	}
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::DelayExitForTime_OnUpdate()
{
	if (IsFlagSet(CVehicleEnterExitFlags::JumpOut))
	{
		m_fDelayTime -= GetTimeStep();

		if (m_fDelayTime <= 0.0f)
		{
			SetTaskState(State_WaitForEntryToBeClearOfPeds);
			return FSM_Continue;
		}
	}
	else if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetTaskState(State_WaitForEntryToBeClearOfPeds);
		return FSM_Continue;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::PickDoor_OnEnter()
{
	if (!IsRunningFlagSet(CVehicleEnterExitFlags::PreferLeftEntry) && !IsRunningFlagSet(CVehicleEnterExitFlags::PreferRightEntry))
	{
		if (m_pVehicle->InheritsFromBike() || m_pVehicle->GetIsJetSki())
		{
			if (!IsRunningFlagSet(CVehicleEnterExitFlags::GettingOffBecauseDriverJacked))
			{
				SetRunningFlag(CVehicleEnterExitFlags::PreferLeftEntry);
			}
			SetRunningFlag(CVehicleEnterExitFlags::UseDirectEntryOnly);
		}
	}

	if (GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_WaitForDirectEntryPointToBeFreeWhenExiting)
		|| IsRunningFlagSet(CVehicleEnterExitFlags::ThroughWindscreen))
	{
		SetRunningFlag(CVehicleEnterExitFlags::UseDirectEntryOnly);
	}

	const CVehicleSeatAnimInfo* pSeatClipInfo = m_pVehicle->GetSeatAnimationInfo(GetPed());
	if (pSeatClipInfo && pSeatClipInfo->IsTurretSeat())
	{
		SetRunningFlag(CVehicleEnterExitFlags::UseDirectEntryOnly);
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::PickDoor_OnUpdate()
{
	CPed* pPed = GetPed();

	static s32 sMaxTimesInState = 2;
	if (m_iNumTimesInPickDoorState > sMaxTimesInState)
	{
		taskAssertf(0, "Possible infinite loop detected in CTaskExitVehicle::PickDoor_OnUpdate.  \n"
					   "Target entry point is %d", m_iTargetEntryPoint/*, GetRunningFlags() */);
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	// the ped cannot exit the vehicle if it is migrating, as it needs to migrate with the vehicle, and the task is about to be aborted as the ped
	// migrates.
	if (NetworkInterface::IsGameInProgress())
	{
		CNetObjPed* pNetObjPed = SafeCast(CNetObjPed, GetPed()->GetNetworkObject());
		netObject* pNetObjVehicle = m_pVehicle ? m_pVehicle->GetNetworkObject() : NULL;

		if (pNetObjPed &&
			pNetObjVehicle &&
			!pNetObjVehicle->IsClone() && 
			pNetObjVehicle->IsPendingOwnerChange() &&
			pNetObjPed->MigratesWithVehicle())
		{
			return FSM_Continue;
		}
	}

	if (IsVehicleOnGround(*m_pVehicle))
	{
		if (CheckForOnsideExit())
		{
			SetRunningFlag(CVehicleEnterExitFlags::VehicleIsOnSide);
		}
		else if (CheckForUpsideDownExit())
		{
			SetRunningFlag(CVehicleEnterExitFlags::VehicleIsUpsideDown);
		}
	}

	m_iSeat = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);

	if (m_pVehicle->IsSeatIndexValid(m_iSeat))
	{
		s32 iDirectEntryPointIndex = m_pVehicle->GetDirectEntryPointIndexForSeat(m_iSeat);

		if (m_pVehicle->GetSeatAnimationInfo(m_iSeat)->IsStandingTurretSeat() && IsFlagSet(CVehicleEnterExitFlags::JumpOut))
		{
			 iDirectEntryPointIndex = m_pVehicle->ChooseAppropriateJumpOutDirectEntryPointIndexForTurretDirection(m_iSeat);
		}

		if (m_pVehicle->IsEntryIndexValid(iDirectEntryPointIndex))
		{
			const CVehicleEntryPointAnimInfo* pAnimInfo = m_pVehicle->GetEntryAnimInfo(iDirectEntryPointIndex);
			bool bWarpOut = pAnimInfo->GetWarpOut() ? true : false;
#if __BANK
			if (bWarpOut)
				aiDisplayf("Frame : %u - Ped %s warping out of vehicle %s - WarpOut set on %s", fwTimer::GetFrameCount(), pPed->GetDebugName(), m_pVehicle->GetDebugName(), pAnimInfo->GetName().GetCStr());
#endif //__BANK

			bool bWarpFromAlkonost = false;
			if(MI_PLANE_ALKONOST.IsValid() && m_pVehicle->GetModelIndex() == MI_PLANE_ALKONOST )
			{
				const CLandingGear* pLandingGear = 	&( static_cast<const CPlane*>((CVehicle*)m_pVehicle)->GetLandingGear());
				bWarpFromAlkonost = (IsVehicleOnGround(*m_pVehicle) && pLandingGear && pLandingGear->GetPublicState() != CLandingGear::STATE_LOCKED_DOWN);
			}
			bool bIsFormulaCar = m_pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_IS_FORMULA_VEHICLE);
			bool bOnSideOrUpsideDown = IsFlagSet(CVehicleEnterExitFlags::VehicleIsOnSide) || IsFlagSet(CVehicleEnterExitFlags::VehicleIsUpsideDown);
			if (ShouldWarpInAndOutInMP(*m_pVehicle, iDirectEntryPointIndex) || (bOnSideOrUpsideDown && bIsFormulaCar) || bWarpFromAlkonost)
			{
				bWarpOut = true;
			}

			if (!bWarpOut && !m_pVehicle->InheritsFromPlane() && pAnimInfo->GetHasClimbUp() && !IsFlagSet(CVehicleEnterExitFlags::BeJacked))
			{
				Vector3 vNewTargetPosition(Vector3::ZeroType);
				Quaternion qNewTargetOrientation(Quaternion::IdentityType);

				if (taskVerifyf(CTaskVehicleFSM::ComputeTargetTransformInWorldSpace(*m_pVehicle, vNewTargetPosition, qNewTargetOrientation, iDirectEntryPointIndex, CExtraVehiclePoint::GET_IN), "Couldn't compute climb up offset"))
				{			
					TUNE_GROUP_FLOAT(EXIT_VEHICLE_TUNE, CLIMB_OUT_OFFSET_Z, 0.25f, 0.0f, 1.0f, 0.01f);
					const float fHeight = vNewTargetPosition.z - CLIMB_OUT_OFFSET_Z;
					Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
					Vector3 vStart(vPedPos.x, vPedPos.y, fHeight);
					Vector3 vEnd(vNewTargetPosition.x, vNewTargetPosition.y, fHeight);
					if (CheckForObstructions(m_pVehicle, vStart, vEnd))
					{
#if DEBUG_DRAW
						CTask::ms_debugDraw.AddCapsule(RCC_VEC3V(vStart), RCC_VEC3V(vEnd), 0.25f, Color_red, 1000, 0, false);
#endif
						bWarpOut = true;
					}
#if DEBUG_DRAW
					CTask::ms_debugDraw.AddCapsule(RCC_VEC3V(vStart), RCC_VEC3V(vEnd), 0.25f, Color_green, 1000, 0, false);
#endif

				}

				if (!bWarpOut)
				{
					m_iTargetEntryPoint = iDirectEntryPointIndex;

					if (RequestClips())
					{
						if (IsFlagSet(CVehicleEnterExitFlags::IsFleeExit))
						{
							if (pPed->IsNetworkClone())
							{
								SetTaskState(State_WaitForNetworkState);
								return FSM_Continue;
							}
							else
							{
								SetTaskState(State_DetermineFleeExitPosition);
								return FSM_Continue;
							}
						}
						else
						{
							SetTaskState(State_ReserveSeat);
							return FSM_Continue;
						}
					}
					return FSM_Continue;
				}
			}
			
			if (bWarpOut)
			{
				m_bWarping = true;
				m_iRunningFlags.BitSet().Set(CVehicleEnterExitFlags::Warp);
			}
		}
	}
	bool bTechnical2InWater = MI_CAR_TECHNICAL2.IsValid() && m_pVehicle->GetModelIndex() == MI_CAR_TECHNICAL2 && m_pVehicle->GetIsInWater();
	bool bStrombergInWater = MI_CAR_STROMBERG.IsValid() && m_pVehicle->GetModelIndex() == MI_CAR_STROMBERG && m_pVehicle->GetIsInWater();
	bool bUpsideDownLargePlane = IsFlagSet(CVehicleEnterExitFlags::VehicleIsUpsideDown) && ( (MI_PLANE_BOMBUSHKA.IsValid() && m_pVehicle->GetModelIndex() == MI_PLANE_BOMBUSHKA) || 
																							 (MI_PLANE_VOLATOL.IsValid() && m_pVehicle->GetModelIndex() == MI_PLANE_VOLATOL) );
	bool bKnockedOverThruster = MI_JETPACK_THRUSTER.IsValid() && m_pVehicle->GetModelIndex() == MI_JETPACK_THRUSTER && CVehicle::GetVehicleOrientation(*m_pVehicle) != CVehicle::VO_Upright;
	bool bToreadorInWater = MI_CAR_TOREADOR.IsValid() && m_pVehicle->GetModelIndex() == MI_CAR_TOREADOR && m_pVehicle->GetIsInWater();
	bool bAvisaInWater = MI_SUB_AVISA.IsValid() && m_pVehicle->GetModelIndex() == MI_SUB_AVISA && m_pVehicle->GetIsInWater();
	const CVehicleLayoutInfo* pLayoutInfo = m_pVehicle->GetLayoutInfo();
	if((pLayoutInfo && pLayoutInfo->GetWarpInAndOut()) || (bTechnical2InWater && (m_iSeat == 0 || m_iSeat == 1)) || bUpsideDownLargePlane || bStrombergInWater || bKnockedOverThruster || bToreadorInWater || bAvisaInWater)
	{
		m_bWarping = true;
		SetRunningFlag(CVehicleEnterExitFlags::Warp);
		BANK_ONLY(aiDisplayf("Frame : %u - Ped %s warping out of vehicle %s - WarpInAndOut set on %s", fwTimer::GetFrameCount(), pPed->GetDebugName(), m_pVehicle->GetDebugName(), pLayoutInfo->GetName().GetCStr());)
	}

	if (taskVerifyf(m_iSeat != -1, "Invalid ped seat index in CTaskExitVehicle::PickDoor_OnUpdate"))
	{
		if (!IsFlagSet(CVehicleEnterExitFlags::BeJacked) && m_iTargetEntryPoint == -1)
		{
			SetRunningFlag(CVehicleEnterExitFlags::IsExitingVehicle);

			TUNE_GROUP_BOOL(RAGDOLL_ON_EXIT_TUNE, SKIP_COLLISION_TEST_ON_JUMP_OUT, false);
			if (IsFlagSet(CVehicleEnterExitFlags::JumpOut))
			{
				m_bWarping = SKIP_COLLISION_TEST_ON_JUMP_OUT;
			}
			m_iTargetEntryPoint = CModelSeatInfo::EvaluateSeatAccessFromAllEntryPoints(pPed, m_pVehicle, m_iSeatRequestType, m_iSeat, m_bWarping, m_iRunningFlags);
		}
		else
		{

			bool bNetDeadLocalPlayerBeingJackedFromBoat = false;
			Vector3 vNetDeadLocalPlayerBeingJackedFromBoatExitPointOffset(0.0f, 0.0f, 0.0f);

			if(NetworkInterface::IsGameInProgress() && 
				pPed->IsLocalPlayer() && 
				pPed->ShouldBeDead() && 
				IsFlagSet(CVehicleEnterExitFlags::BeJacked) &&
				m_pVehicle->InheritsFromBoat())
			{
				//B* 1808182 - If we are a dead net player being jacked from a boat 
				//pre-check if we are being thrown onto land and adjust exit point height
				//otherwise the player can be warped straight out leaving the jacking ped
				//running jack anim without a player to jack

				//See if ped out pos is below ground
				Vector3 vTargetPosition;
				Quaternion qTargetOrientation;
				CModelSeatInfo::CalculateEntrySituation(m_pVehicle, pPed, vTargetPosition, qTargetOrientation, m_iTargetEntryPoint, 0, 0.0f);

				Vector3 vGroundPos(Vector3::ZeroType);
				if (CTaskVehicleFSM::FindGroundPos(m_pVehicle, GetPed(), vTargetPosition, 2.5f, vGroundPos, false))
				{
					//Adjust up for ground pos by the amount it goes over
					float vHeightAdjust = vGroundPos.z - vTargetPosition.z;
					vNetDeadLocalPlayerBeingJackedFromBoatExitPointOffset.z = vHeightAdjust;
					bNetDeadLocalPlayerBeingJackedFromBoat = true;  
				}
			}

			// Warp the ped to a safe place if the entry point is blocked
			if ( m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint) && !CVehicle::IsEntryPointClearForPed(*m_pVehicle, *pPed, m_iTargetEntryPoint, bNetDeadLocalPlayerBeingJackedFromBoat ? &vNetDeadLocalPlayerBeingJackedFromBoatExitPointOffset:NULL))
			{
				SetRunningFlag(CVehicleEnterExitFlags::Warp);
				BANK_ONLY(aiDisplayf("Frame : %u - Ped %s warping out of vehicle %s - CVehicle::IsEntryPointClearForPed returned false", fwTimer::GetFrameCount(), pPed->GetDebugName(), m_pVehicle->GetDebugName());)
			}
		}


		TUNE_GROUP_BOOL(VEHICLE_ENTRY, FORCE_ENTRYPOINT, false);
		TUNE_GROUP_INT(VEHICLE_ENTRY, FORCE_ENTRYPOINT_INDEX, 0, 0, 16, 1);
		if (FORCE_ENTRYPOINT && m_pVehicle->IsEntryIndexValid(FORCE_ENTRYPOINT_INDEX))
		{
			m_iTargetEntryPoint = FORCE_ENTRYPOINT_INDEX;
		}

		if (!IsFlagSet(CVehicleEnterExitFlags::JumpOut) && CTaskVehicleFSM::PedShouldWarpBecauseVehicleAtAwkwardAngle(*m_pVehicle))
		{
			SetRunningFlag(CVehicleEnterExitFlags::Warp);
			BANK_ONLY(aiDisplayf("Frame : %u - Ped %s warping out of vehicle %s - CTaskVehicleFSM::PedShouldWarpBecauseVehicleAtAwkwardAngle returned true", fwTimer::GetFrameCount(), pPed->GetDebugName(), m_pVehicle->GetDebugName());)

		}

		// If a valid exit point has been found, begin exiting using that
		if (m_iTargetEntryPoint != -1)
		{
			if (IsFlagSet(CVehicleEnterExitFlags::Warp) || CTaskVehicleFSM::ShouldWarpInAndOutInMP(*m_pVehicle, m_iTargetEntryPoint) || CTaskVehicleFSM::PedShouldWarpBecauseHangingPedInTheWay(*m_pVehicle, m_iTargetEntryPoint))
			{
				m_bWarping = true;
				pPed->SetAttachCarEntryExitPoint(m_iTargetEntryPoint);
				SetTaskState(State_SetPedOut);
				return FSM_Continue;
			}

			const CVehicleSeatInfo* pSeatInfo = m_pVehicle->GetSeatInfo(m_iSeat);
			taskAssert(pSeatInfo);

			const CEntryExitPoint* pEntryPoint = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint);
			bool bIsTargetInDirect = pEntryPoint->GetSeat(SA_directAccessSeat) != m_iSeat;

			// If the desired target exit point isn't the same as our direct entry point, we require a shuffle if we have a shuffle link
			// and have an indirect exit path
			if (bIsTargetInDirect && pSeatInfo->GetShuffleLink())
			{
				const CEntryExitPoint* pEntryPoint = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint);
				if (pEntryPoint)
				{
					s32 iPedInShuffleSeat = pEntryPoint->GetSeat(SA_directAccessSeat);
					CPed* pSeatOccupier = m_pVehicle->GetSeatManager()->GetPedInSeat(iPedInShuffleSeat);

					if (pSeatOccupier)
					{
						// See if the occupier is going to exit this vehicle, if so, wait for him
						CTaskExitVehicle* pExitVehicleTask = static_cast<CTaskExitVehicle*>(pSeatOccupier->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE));
						const bool bExitingVehicle = pExitVehicleTask && pExitVehicleTask->GetState() > CTaskExitVehicle::State_PickDoor;
						if (pSeatOccupier->IsExitingVehicle() || bExitingVehicle)
							return FSM_Continue;
						//

						if(!IsFlagSet(CVehicleEnterExitFlags::WarpIfShuffleLinkIsBlocked))
						{
							if (!pSeatOccupier->IsNetworkClone() && pSeatOccupier->IsDead())
							{
								VehicleEnterExitFlags vehicleFlags;
								vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontCloseDoor);
								vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontWaitForCarToStop);
								vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DeadFallOut);
								CEventGivePedTask givePedTask(PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP, rage_new CTaskExitVehicle(m_pVehicle, vehicleFlags, 0.0f, pPed, m_iTargetEntryPoint), true );
								pSeatOccupier->GetPedIntelligence()->AddEvent(givePedTask);
								SetTaskState(State_WaitForOtherPedToExit);
								return FSM_Continue;
							}
							else
							{
								SetTaskState(State_Finish);
								return FSM_Continue;
							}
						}
						
						// Warp the ped out if there is a ped in the shuffle seat
						SetRunningFlag(CVehicleEnterExitFlags::Warp);
						m_bWarping = true;
						BANK_ONLY(aiDisplayf("Frame : %u - Ped %s warping out of vehicle %s - Shuffle link is blocked and CVehicleEnterExitFlags::WarpIfShuffleLinkIsBlocked is set", fwTimer::GetFrameCount(), pPed->GetDebugName(), m_pVehicle->GetDebugName());)

						// Recursive call, try again with the extra flag set
						m_iNumTimesInPickDoorState++;
						return PickDoor_OnUpdate();
					}
					else 
					{
						if (RequestClips())
						{
							SetTaskState(State_ReserveShuffleSeat);
						}
						return FSM_Continue;
					}
				}
			}

			// When being jacked, the ped jacking us will have reserved the seat
			if (!IsFlagSet(CVehicleEnterExitFlags::BeJacked))
			{
				if (IsFlagSet(CVehicleEnterExitFlags::IsFleeExit))
				{
					if (pPed->IsNetworkClone())
					{
						SetTaskState(State_WaitForNetworkState);
						return FSM_Continue;
					}
					else
					{
						SetTaskState(State_DetermineFleeExitPosition);
						return FSM_Continue;
					}
				}
				else
				{
					if (pPed->IsNetworkClone())
					{
						SetTaskState(State_StreamAssetsForExit);
						return FSM_Continue;
					}
					else
					{
						SetTaskState(State_ReserveSeat);
						return FSM_Continue;
					}
				}
			}
			else
			{
				if (RequestClips())
				{
					StartMoveNetwork(*pPed, *m_pVehicle, m_moveNetworkHelper, GetMoveNetworkClipSet(), m_iTargetEntryPoint, GetBlendInDuration());
					SetTaskState(State_ExitSeat);
				}
				return FSM_Continue;
			}	
		}
		else if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_WaitForDirectEntryPointToBeFreeWhenExiting))
		{
			SetTaskState(State_WaitForValidEntry);
			return FSM_Continue;
		}
		else // No valid exit point
		{
			if (pPed->ShouldBeDead())
			{
				SetRunningFlag(CVehicleEnterExitFlags::DontSetPedOutOfVehicle);
				SetTaskState(State_Finish);
				return FSM_Continue;
			}

			m_iTargetEntryPoint = CModelSeatInfo::EvaluateSeatAccessFromAllEntryPoints( pPed, m_pVehicle, m_iSeatRequestType, m_iSeat, m_bWarping, m_iRunningFlags );
			
			// If no exit point is found when warping, get the default first exit point.
			if (m_iTargetEntryPoint == -1)
			{
				taskAssert(m_pVehicle->GetNumberEntryExitPoints()>=0);
				m_iTargetEntryPoint = 0;
			}

			if (IsFlagSet(CVehicleEnterExitFlags::JumpOut) && 
				!IsFlagSet(CVehicleEnterExitFlags::DontWaitForCarToStop) &&
				m_pVehicle->GetDriver() == pPed)
			{
				// Skip this test if we're a plane in the air
				if (!m_pVehicle->InheritsFromPlane() || !m_pVehicle->IsInAir())
				{
					SetTaskState(State_WaitForCarToStop);
					return FSM_Continue;
				}
			}

			// Player warps out
			if (pPed->IsLocalPlayer())
			{
				m_bWarping = true;
				BANK_ONLY(aiDisplayf("Frame : %u - Ped %s warping out of vehicle %s - No valid exit point was found for local player", fwTimer::GetFrameCount(), pPed->GetDebugName(), m_pVehicle->GetDebugName());)
				pPed->SetAttachCarEntryExitPoint(m_iTargetEntryPoint);
				SetTaskState(State_SetPedOut);
				return FSM_Continue;
			}

			// Peds flagged to warp out do so if the door is blocked
			// Also need to warp if being jacked from inside
			if (IsFlagSet(CVehicleEnterExitFlags::WarpIfDoorBlocked) || IsFlagSet(CVehicleEnterExitFlags::JackedFromInside))
			{
				// First we see if any other ped can exit this vehicle
				s32 nMaxPassengers = m_pVehicle->GetMaxPassengers();
				if (nMaxPassengers > 0 && !m_nFramesWaited)
				{
					const CVehicleSeatInfo* pSeatInfo = m_pVehicle->GetSeatInfo(m_iSeat);
					taskAssert(pSeatInfo);

					++nMaxPassengers;	// GetMaxPassengers will exclude the driver seat
					for (int i = 0; i < nMaxPassengers; ++i)
					{
						CPed* OtherPed = m_pVehicle->GetPedInSeat(i);
						if (OtherPed && OtherPed != pPed)
						{
							const CVehicleSeatInfo* pOtherSeatInfo = m_pVehicle->GetSeatInfo(i);
							taskAssert(pOtherSeatInfo);

							if (pSeatInfo == pOtherSeatInfo->GetShuffleLink())
							{
								s32 OtherEntryPoint = CModelSeatInfo::EvaluateSeatAccessFromAllEntryPoints( OtherPed, m_pVehicle, m_iSeatRequestType, i, m_bWarping, m_iRunningFlags );
								if (OtherEntryPoint >= 0)
								{
									//We want to re-evaluate our target exit index when we finish waiting for the other ped to exit.
									m_iTargetEntryPoint = -1;
									
									// Alright some other ped can at least exit, let's try to wait for him first
									SetState(State_WaitForOtherPedToExit);
									return FSM_Continue;
								}
							}
						}
					}
				}
				//

				// Warp the ped in if the door is blocked
				BANK_ONLY(aiDisplayf("Frame : %u - Ped %s warping out of vehicle %s - Flags::WarpIfDoorBlocked = %s, Flags::JackedFromInside = %s", fwTimer::GetFrameCount(), pPed->GetDebugName(), m_pVehicle->GetDebugName(), IsFlagSet(CVehicleEnterExitFlags::WarpIfDoorBlocked) ? "true" : "false", IsFlagSet(CVehicleEnterExitFlags::JackedFromInside) ? "true" : "false");)
				SetRunningFlag(CVehicleEnterExitFlags::Warp);
				m_bWarping = true;

				// Recursive call, try again with the extra flag set
				m_iNumTimesInPickDoorState++;
				return PickDoor_OnUpdate();
			}

			// Temp: Warp peds out of the tourbus for now
			s32 iEntryPointIndex = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(m_iSeat, m_pVehicle);
			if (iEntryPointIndex != -1 && m_pVehicle->GetEntryExitPoint(iEntryPointIndex))
			{
				if (m_pVehicle->GetEntryExitPoint(iEntryPointIndex)->GetSeatAccessor().GetSeatAccessType() == CSeatAccessor::eSeatAccessTypeMultiple)
				{
					m_bWarping = true;
					BANK_ONLY(aiDisplayf("Frame : %u - Ped %s warping out of vehicle %s - Entry point is type CSeatAccessor::eSeatAccessTypeMultiple (tourbus hack?)", fwTimer::GetFrameCount(), pPed->GetDebugName(), m_pVehicle->GetDebugName());)

					pPed->SetAttachCarEntryExitPoint(iEntryPointIndex);
					SetTaskState(State_SetPedOut);
					return FSM_Continue;
				}
			}


			// Quit the exit task, we're stuck in the vehicle
			SetTaskState(State_Finish);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}


CTask::FSM_Return CTaskExitVehicle::WaitForOtherPedToExit_OnUpdate()
{
	// If the min time to wait was set to something less than 0.0f then it means we want to quit the task
	if(m_fMinTimeToWaitForOtherPedToExit < 0.0f)
	{
		return FSM_Quit;
	}

	static const int nFramesToWait = (int)(1.0f / FRAME_LIMITER_MIN_FRAME_TIME);
	taskAssert(nFramesToWait > 0 && nFramesToWait < 255);
	if (m_nFramesWaited > nFramesToWait && (GetTimeInState() > m_fMinTimeToWaitForOtherPedToExit))
	{
		SetState(State_PickDoor);
		return FSM_Continue;
	}

	++m_nFramesWaited;
	return FSM_Continue;
}

void CTaskExitVehicle::WaitForCarToStop_OnEnter()
{
	// Yes stop it please!
	{
		aiTask* pTask = m_pVehicle->GetIntelligence()->GetTaskManager()->GetActiveTask(VEHICLE_TASK_TREE_PRIMARY);
		if (pTask && (pTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_GO_FORWARD || pTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_REVERSE))
			pTask->MakeAbortable(ABORT_PRIORITY_IMMEDIATE, NULL);
	}

	if (!m_pVehicle->IsNetworkClone())
	{
		CTask* pTask = rage_new CTaskVehicleStop(0);
		m_pVehicle->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, pTask, VEHICLE_TASK_PRIORITY_PRIMARY, false);
	}

	ClearRunningFlag(CVehicleEnterExitFlags::VehicleIsOnSide);
	ClearRunningFlag(CVehicleEnterExitFlags::VehicleIsUpsideDown);
}

CTask::FSM_Return CTaskExitVehicle::WaitForCarToStop_OnUpdate()
{
	if (m_pVehicle->CanPedStepOutCar(GetPed()) && GetTimeInState() > 0.1f)
	{
		m_iRunningFlags.BitSet().Clear(CVehicleEnterExitFlags::JumpOut);
		m_iTargetEntryPoint = -1;	// Reevaluate which seat we could exit from
		SetState(State_PickDoor);
		return FSM_Continue;
	}
	else if (GetPed()->IsLocalPlayer())
	{
		CControl* pControl = GetPed()->GetControlFromPlayer();
		if (pControl)
		{
			TUNE_GROUP_INT(EXIT_VEHICLE_TUNE, HELD_DOWN_JUMP_OUT_TIME, 1000, 0, 3000, 1);
			if (pControl->GetPedEnter().HistoryHeldDown(HELD_DOWN_JUMP_OUT_TIME))
			{
				m_iRunningFlags.BitSet().Set(CVehicleEnterExitFlags::JumpOut);
				m_iRunningFlags.BitSet().Set(CVehicleEnterExitFlags::WarpIfDoorBlocked);
				m_iRunningFlags.BitSet().Set(CVehicleEnterExitFlags::DontWaitForCarToStop);
				m_iTargetEntryPoint = -1;	// Reevaluate which seat we could exit from
				SetState(State_PickDoor);
				return FSM_Continue;
			}
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::WaitForCarToSettle_OnEnter()
{
	if (m_pVehicle->IsDriver(GetPed()) && !m_pVehicle->IsNetworkClone())
	{
		CTask* pTask = rage_new CTaskVehicleStop(0);
		m_pVehicle->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, pTask, VEHICLE_TASK_PRIORITY_PRIMARY, false);
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::WaitForCarToSettle_OnUpdate()
{
	if(!ShouldWaitForCarToSettle())
	{
		TUNE_GROUP_INT(EXIT_VEHICLE_TUNE, SETTLE_MIN_TIME, 750, 0, 5000, 1);
		if(CTimeHelpers::GetTimeSince(m_uLastTimeUnsettled) > SETTLE_MIN_TIME)
		{
			SetTaskState(State_PickDoor);
			return FSM_Continue;
		}
	}
	else
	{
		if (!m_pVehicle->CanPedJumpOutCar(GetPed(),true))
		{
			ClearRunningFlag(CVehicleEnterExitFlags::JumpOut);
		}

		m_uLastTimeUnsettled = fwTimer::GetTimeInMilliseconds();

		TUNE_GROUP_FLOAT(EXIT_VEHICLE_TUNE, SETTLE_TIME_OUT, 8.0f, 0.0f, 20.0f, 1.0f);
		if(GetTimeInState() > SETTLE_TIME_OUT)
		{
			SetRunningFlag(CVehicleEnterExitFlags::Warp);
			m_bWarping = true;
			BANK_ONLY(aiDisplayf("Frame : %u - Ped %s warping out of vehicle %s - Timed out waiting for vehicle to settle", fwTimer::GetFrameCount(), GetPed()->GetDebugName(), m_pVehicle->GetDebugName());)

			SetTaskState(State_PickDoor);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicle::ShuffleToSeat_OnEnter()
{
	CPed *pPed = GetPed();
	if (pPed->IsPlayer() && m_pVehicle->GetDriver() == pPed)
	{
		if(!m_pVehicle->IsNetworkClone())
		{
			m_pVehicle->SwitchEngineOff();
		}
	}
	ClearRunningFlag(CVehicleEnterExitFlags::IsStreamedExit);
	SetNewTask(rage_new CTaskInVehicleSeatShuffle(m_pVehicle, &m_moveNetworkHelper, false, m_iSeat));
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::ShuffleToSeat_OnUpdate()
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
        m_iLastCompletedState = GetState();

		// We're entering the indirect access seat, store it here so we can pass it to the set ped in seat task
		m_iSeat = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat);
		taskAssert(m_iSeat > -1 && m_iSeat == m_iSeat);

		CPed *pPed = GetPed();

		// Shuffle failed, quit exit task
		const s32 iCurrentSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
		if (m_iSeat != iCurrentSeatIndex)
		{
			SetTaskState(State_Finish);
			return FSM_Continue;
		}

		// Just die!
		if (pPed->IsFatallyInjured())
		{
			SetTaskState(State_Finish);
			return FSM_Continue;
		}

		const fwMvClipSetId clipSetId = GetMoveNetworkClipSet();
		m_moveNetworkHelper.SetClipSet(clipSetId);
		// Also need to set the common clipset for upside down/side exits!
		fwMvClipSetId commonClipsetId = CTaskEnterVehicleAlign::GetCommonClipsetId(m_pVehicle, m_iTargetEntryPoint);
		if (CTaskVehicleFSM::IsVehicleClipSetLoaded(commonClipsetId))
		{
			m_moveNetworkHelper.SetClipSet(commonClipsetId, CTaskVehicleFSM::ms_CommonVarClipSetId);
		}

		if(pPed->IsNetworkClone())
		{
            s32 iCurrentSeat = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);

            if(m_pVehicle->IsSeatIndexValid(iCurrentSeat))
            {
			    SetTaskState(State_StreamAssetsForExit);
            }
            else
            {
                SetTaskState(State_Finish);
            }
		}
		else
		{
			if(IsRunningFlagSet(CVehicleEnterExitFlags::IsFleeExit))
			{
				SetTaskState(State_DetermineFleeExitPosition);
			}
			else
			{
				if (IsRunningFlagSet(CVehicleEnterExitFlags::WaitForEntryToBeClearOfPeds))
				{
					SetTaskState(State_WaitForEntryToBeClearOfPeds);
				}
				else
				{
					SetTaskState(State_ReserveDoor);
				}
			}
		}
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::ReserveSeat_OnUpdate()
{
	CComponentReservation* pComponentReservation = m_pVehicle->GetComponentReservationMgr()->GetSeatReservationFromSeat(m_pVehicle->GetSeatManager()->GetPedsSeatIndex(GetPed()));
	if (CComponentReservationManager::ReserveVehicleComponent(GetPed(), m_pVehicle, pComponentReservation))
	{
		SetTaskState(State_ReserveDoor);
	}

	TUNE_GROUP_BOOL(EXIT_VEHICLE_TUNE, ALLOW_NON_CLONE_VEHICLE_WARP_OUT, true);
    if(m_pVehicle->IsNetworkClone() || ALLOW_NON_CLONE_VEHICLE_WARP_OUT)
    {
		bool bIsTula = MI_PLANE_TULA.IsValid() && m_pVehicle->GetModelIndex() == MI_PLANE_TULA;
        const float fTimeBeforeWarp = bIsTula ? CTaskExitVehicle::ms_Tunables.m_MaxTimeToReserveComponentBeforeWarpLonger : CTaskExitVehicle::ms_Tunables.m_MaxTimeToReserveComponentBeforeWarp;

        if (GetTimeInState() > fTimeBeforeWarp)
	    {
			m_bWarping = true;
			BANK_ONLY(aiDisplayf("Frame : %u - Ped %s warping out of vehicle %s - Timed out waiting for seat reservation", fwTimer::GetFrameCount(), GetPed()->GetDebugName(), m_pVehicle->GetDebugName());)

		    SetTaskState(State_SetPedOut);
		    return FSM_Continue;
	    }
    }

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::ReserveShuffleSeat_OnUpdate()
{
	CPed* pPed = GetPed();
	CComponentReservation* pComponentReservation = m_pVehicle->GetComponentReservationMgr()->GetSeatReservation(m_iTargetEntryPoint, SA_directAccessSeat);
	if (CComponentReservationManager::ReserveVehicleComponent(pPed, m_pVehicle, pComponentReservation))
	{
		SetTaskState(State_StreamAssetsForShuffle);
		return FSM_Continue;
	}

    if(m_pVehicle->IsNetworkClone())
    {
		bool bIsTula = MI_PLANE_TULA.IsValid() && m_pVehicle->GetModelIndex() == MI_PLANE_TULA;
		const float fTimeBeforeWarp = bIsTula ? CTaskExitVehicle::ms_Tunables.m_MaxTimeToReserveComponentBeforeWarpLonger : CTaskExitVehicle::ms_Tunables.m_MaxTimeToReserveComponentBeforeWarp;

        if (GetTimeInState() > fTimeBeforeWarp)
	    {
		    SetTaskState(State_SetPedOut);
		    return FSM_Continue;
	    }
    }

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::StreamAssetsForShuffle_OnUpdate()
{
	CPed* pPed = GetPed();
	s32 iCurrentSeat = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
	if (taskVerifyf(m_pVehicle->IsSeatIndexValid(iCurrentSeat), "Ped wasn't in a valid seat"))
	{
		s32 iExitPointIndex = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iCurrentSeat, m_pVehicle);
		taskFatalAssertf(m_pVehicle->GetEntryExitPoint(iExitPointIndex), "NULL entry point associated with seat %i", iCurrentSeat);

		s32 iTargetSeat = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetShuffleSeatForSeat(iExitPointIndex, iCurrentSeat);

		// Check to see if the entry point associated with iTargetSeat matches the one we chose to target earlier in PickDoor.
		// If it doesn't, maybe the alternate shuffle link is a better solution?
		s32 iExitPointIndexForTargetSeat = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iTargetSeat, m_pVehicle);
		if (iExitPointIndexForTargetSeat != m_iTargetEntryPoint)
		{
			s32 iAltTargetSeat = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetShuffleSeatForSeat(iExitPointIndex, iCurrentSeat, true);
			s32 iExitPointIndexForAltTargetSeat = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iAltTargetSeat, m_pVehicle);
			if (iExitPointIndexForAltTargetSeat == m_iTargetEntryPoint)
			{
				iTargetSeat = iAltTargetSeat;	
			}
		}

		if (!m_pVehicle->IsSeatIndexValid(iTargetSeat))
		{
			SetTaskState(State_Finish);
			return FSM_Continue;
		}

		fwMvClipSetId shuffleClipsetId = CTaskInVehicleSeatShuffle::GetShuffleClipsetId(m_pVehicle, iCurrentSeat);
		const fwMvClipSetId exitClipSet = GetMoveNetworkClipSet(true);
		if (CTaskVehicleFSM::RequestVehicleClipSetReturnIsLoaded(shuffleClipsetId, SP_High) &&
			CTaskVehicleFSM::RequestVehicleClipSetReturnIsLoaded(exitClipSet, SP_High))
		{
			s32 iDirectEntryPointIndex = m_pVehicle->GetDirectEntryPointIndexForSeat(iCurrentSeat);
			if (!m_moveNetworkHelper.IsNetworkActive())
			{
				StartMoveNetwork(*pPed, *m_pVehicle, m_moveNetworkHelper, exitClipSet, iDirectEntryPointIndex, INSTANT_BLEND_DURATION);
			}

			m_iSeat = iTargetSeat;

			SetTaskState(State_ShuffleToSeat);
			return FSM_Continue;
		}
        else
        {
#if __BANK
			CTaskVehicleFSM::SpewStreamingDebugToTTY(*pPed, *m_pVehicle, iCurrentSeat, shuffleClipsetId, GetTimeInState());
#endif // __BANK

            if(pPed->IsNetworkClone())
            {
                // if we don't manage to stream the anims before the clone ped has finished exiting just quit the task (the
                // ped will be popped to it's new position by the network code)
                if(!pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE))
                {
                    SetTaskState(State_Finish);
			        return FSM_Continue;
                }
            }
        }
	}
    else
    {
        SetTaskState(State_Finish);
        return FSM_Continue;
    }
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::StreamAssetsForExit_OnUpdate()
{
    CPed* pPed = GetPed();
	const float fTimeBeforeWarp = NetworkInterface::IsGameInProgress() ? CTaskEnterVehicle::ms_Tunables.m_MaxTimeStreamClipSetInBeforeWarpMP : CTaskEnterVehicle::ms_Tunables.m_MaxTimeStreamClipSetInBeforeWarpSP;

    if(pPed->IsNetworkClone())
    {
        // if we don't manage to stream the anims before the clone ped has finished exiting just quit the task (the
        // ped will be popped to it's new position by the network code)
        if(!pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE))
        {
            SetTaskState(State_Finish);
            return FSM_Continue;
        }
    }

	if (RequestClips())
	{
		if (pPed->IsLocalPlayer() && IsFlagSet(CVehicleEnterExitFlags::JumpOut) && !m_pVehicle->CanPedJumpOutCar(pPed))
		{
			ClearRunningFlag(CVehicleEnterExitFlags::JumpOut);
		}

		if(!m_bIsRappel && pPed->IsNetworkClone() && !m_bSubTaskExitRateSet)
		{
			return FSM_Continue;
		}

		StartMoveNetwork(*pPed, *m_pVehicle, m_moveNetworkHelper, GetMoveNetworkClipSet(), m_iTargetEntryPoint, GetBlendInDuration());
		SetTaskState(State_ExitSeat);
		return FSM_Continue;
	}

	if (GetTimeInState() > fTimeBeforeWarp)
	{
		SetTaskState(State_SetPedOut);
		return FSM_Continue;
	}
	return FSM_Continue;
}


////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::ReserveDoor_OnUpdate()
{
	// Reserve door component if it exists then exit the seat
	CComponentReservation* pComponentReservation = m_pVehicle->GetComponentReservationMgr()->GetDoorReservation(m_iTargetEntryPoint);
	if (!pComponentReservation || CComponentReservationManager::ReserveVehicleComponent(GetPed(), m_pVehicle, pComponentReservation)
		|| (NetworkInterface::IsGameInProgress() && m_pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_USE_FULL_ANIMS_FOR_MP_WARP_ENTRY_POINTS)))
	{
		SetTaskState(State_StreamAssetsForExit);
		return FSM_Continue;
	}

    if(m_pVehicle->IsNetworkClone())
    {
        bool bIsTula = MI_PLANE_TULA.IsValid() && m_pVehicle->GetModelIndex() == MI_PLANE_TULA;
        const float fTimeBeforeWarp = bIsTula ? CTaskExitVehicle::ms_Tunables.m_MaxTimeToReserveComponentBeforeWarpLonger : CTaskExitVehicle::ms_Tunables.m_MaxTimeToReserveComponentBeforeWarp;

        if (GetTimeInState() > fTimeBeforeWarp)
	    {
			m_bWarping = true;
			BANK_ONLY(aiDisplayf("Frame : %u - Ped %s warping out of vehicle %s - Timed out waiting for door reservation", fwTimer::GetFrameCount(), GetPed()->GetDebugName(), m_pVehicle->GetDebugName());)

		    SetTaskState(State_SetPedOut);
		    return FSM_Continue;
	    }
    }

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::ExitSeat_OnEnter()
{
	if (IsFlagSet(CVehicleEnterExitFlags::IsFleeExit) && !CheckIfFleeExitIsValid())
	{
		ClearRunningFlag(CVehicleEnterExitFlags::IsFleeExit);
		SetRunningFlag(CVehicleEnterExitFlags::DontSetPedOutOfVehicle);
		return FSM_Quit;
	}

	CPed* pPed = GetPed(); 

	if (pPed->IsPlayer() && m_pVehicle->GetDriver() == pPed 
		&& !IsFlagSet(CVehicleEnterExitFlags::JumpOut)
		&& !IsFlagSet(CVehicleEnterExitFlags::ThroughWindscreen))
	{
// 		CControl* pControl = pPed->GetControlFromPlayer();
// 		static dev_u32 TURN_OFF_ENGINE_DURATION = 250;
// 		if (pControl && pControl->GetVehicleExit().HistoryHeldDown(TURN_OFF_ENGINE_DURATION))
// 		{
			bool bSwitchEngineOff = !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_LeaveEngineOnWhenExitingVehicles);
			if (m_pVehicle->GetModelIndex() != MI_BIKE_OPPRESSOR2)
			{
				if (pPed->GetPlayerWanted() && pPed->GetPlayerWanted()->GetWantedLevel() >= WANTED_LEVEL2 && ((float)pPed->GetPlayerWanted()->GetTimeSinceLastSpotted() / 1000.0f) < ms_Tunables.m_TimeSinceLastSpottedToLeaveEngineOn)
				{
					bSwitchEngineOff = false;
				}

				if (pPed->IsAnyActionModeReasonActive())
				{
					bSwitchEngineOff = false;
				}
			}

			if(m_pVehicle->m_nVehicleFlags.bKeepEngineOnWhenAbandoned)
			{
				bSwitchEngineOff = false;
			}

			if (m_pVehicle->GetNumWeaponBlades() > 0)
			{
				bSwitchEngineOff = true;
			}

			if(IsFlagSet(CVehicleEnterExitFlags::BeJacked))
			{
				bSwitchEngineOff = false;
			}

			if (m_pVehicle->GetIsUsingScriptAutoPilot())
			{
				bSwitchEngineOff = false;
			}

			if (CPlayerInfo::AreCNCResponsivenessChangesEnabled(pPed))
			{
				bSwitchEngineOff = false;
			}

            if(!m_pVehicle->IsNetworkClone() && bSwitchEngineOff)
            {
			    m_pVehicle->SwitchEngineOff();
				m_pVehicle->SetSpecialFlightModeTargetRatio( 0.0f );
            }

            if(pPed->IsLocalPlayer())
            {
			    if(m_pVehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_CAR)
			    {
				    static_cast<audCarAudioEntity*>(m_pVehicle->GetVehicleAudioEntity())->TriggerHandbrakeSound();
			    }
            }
//		}
	}

	// If exiting the Stromberg on land, transform vehicle back into normal mode so we don't clip through the wings as we leave
	if (m_pVehicle->InheritsFromSubmarineCar())
	{
		if (m_pVehicle->m_Buoyancy.GetStatus() == NOT_IN_WATER && (m_pVehicle->GetTransformationState() == CTaskVehicleTransformToSubmarine::STATE_SUBMARINE ||
			m_pVehicle->GetTransformationState() == CTaskVehicleTransformToSubmarine::STATE_TRANSFORMING_TO_SUBMARINE))
		{
			audVehicleAudioEntity* vehicleAudioEntity = m_pVehicle->GetVehicleAudioEntity();

			if(vehicleAudioEntity)
			{
				vehicleAudioEntity->SetTriggerFastSubcarTransformSound();
			}

			m_pVehicle->TransformToCar(false);
			m_pVehicle->SetTransformRateForCurrentAnimation(2.5f);
			
			if(m_pVehicle->GetVehicleAudioEntity())
			{
				m_pVehicle->GetVehicleAudioEntity()->StopSubTransformSound();
			}
		}
	}

	// Check if we need to exit to the water (if we're in a boat
	// If we're exiting into the air, force a jump out and ragdoll into nm as soon as we can
	Vector3 vTargetPosition;
	Quaternion qTargetOrientation;
	CModelSeatInfo::CalculateEntrySituation(m_pVehicle, pPed, vTargetPosition, qTargetOrientation, m_iTargetEntryPoint, 0, 0.0f);

	const CVehicleEntryPointAnimInfo* pClipInfo = m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint) ? m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint) : NULL;
	bool bHasGetInFromWater = pClipInfo ? pClipInfo->GetHasGetInFromWater() : false;

	// The get_out clips have some janky fixup issues when exiting in water, so let's use get_out_into_water. Don't have time to put this on a flag, sorry! - Phil B
	if (MI_HELI_SEASPARROW.IsValid() && m_pVehicle->GetModelIndex() == MI_HELI_SEASPARROW)
	{
		bHasGetInFromWater = true;
	}

	if (m_pVehicle->InheritsFromSubmarine())
	{
		if (m_pVehicle->m_Buoyancy.GetStatus() == FULLY_IN_WATER && bHasGetInFromWater)
		{
			SetRunningFlag(CVehicleEnterExitFlags::InWater);
		}

		//Check if the ped should be given scuba gear.
		if(GetPed()->IsPlayer() && GetPed()->GetPlayerInfo()->GetAutoGiveScubaGearWhenExitVehicle() &&
			m_pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_GIVE_SCUBA_GEAR_ON_EXIT) &&
			(m_pVehicle->m_Buoyancy.GetStatus() != NOT_IN_WATER))
		{
			CTaskMotionSwimming::SetScubaGearVariationForPed(*GetPed());
		}
	}
	// If the in water flag is set already, then the ped who gave us this task is jacking us from the water
	else if (!IsFlagSet(CVehicleEnterExitFlags::InWater))
	{
		const bool bFoundLand = !CheckForInAirExit(vTargetPosition);
		const bool bIsVehicleInWater = CheckForVehicleInWater();

		float fWaterHeight = 999999.0f;
		bool bWaterUnderPed = CTaskExitVehicleSeat::CheckForWaterUnderPed(GetPed(), 
			m_pVehicle, 
			CTaskExitVehicleSeat::ms_Tunables.m_InWaterExitProbeLength, 
			CTaskExitVehicleSeat::ms_Tunables.m_InWaterExitDepth, 
			fWaterHeight, 
			true,
			&vTargetPosition);

		bool bIsVehicleSeaplane = m_pVehicle->pHandling && m_pVehicle->pHandling->GetSeaPlaneHandlingData();
		if (bHasGetInFromWater && bIsVehicleInWater && (!bFoundLand || (bWaterUnderPed && (m_pVehicle->InheritsFromBoat() || m_pVehicle->InheritsFromAmphibiousAutomobile() || bIsVehicleSeaplane) && !pPed->IsNetworkClone())))
		{
			SetRunningFlag(CVehicleEnterExitFlags::InWater);
		}
		else if (!bIsVehicleInWater && !bFoundLand && (bHasGetInFromWater || !m_pVehicle->InheritsFromBoat()))
		{
			bool bInAir = false;
			
			if(!m_bIsRappel)
			{
				bInAir = m_pVehicle->IsInAir();
				if(!bInAir && m_pVehicle->GetIsAnyFixedFlagSet() && m_pVehicle->GetIsAircraft())
				{
					bInAir = !IsVehicleOnGround(*m_pVehicle);
				}
			}

			if (bInAir)
			{
				SetRunningFlag(CVehicleEnterExitFlags::InAirExit);
			}
			else if ((m_pVehicle->InheritsFromBoat() || m_pVehicle->InheritsFromAmphibiousAutomobile()) && bHasGetInFromWater)
			{
				// Boat not in water, not in air, but vertical probe check is clear? I think we're on the edge of a cliff, so consider an on-vehicle exit
				SetRunningFlag(CVehicleEnterExitFlags::InWater);
			}
		}

		// We're now confident about doing a boat exit onto land after all the water/land/air checks...
		if(m_pVehicle->InheritsFromBoat() && !m_pVehicle->GetIsJetSki() && !m_pVehicle->InheritsFromAmphibiousQuadBike() && bHasGetInFromWater && !IsFlagSet(CVehicleEnterExitFlags::InWater))
		{
			// ...so one final check to make sure that's not going to put us through the wall (local only, we skipped collision checks earlier to allow the on-vehicle exit point to be valid)
			if(!pPed->IsNetworkClone() && !m_pVehicle->IsEntryPointClearForPed(*m_pVehicle, *pPed, m_iTargetEntryPoint))
			{
				SetRunningFlag(CVehicleEnterExitFlags::InWater);
			}
		}

		TUNE_GROUP_FLOAT(PARACHUTE_BUG, WATER_UNDERNEATH_DISTANCE, 15.0f, 0.0f, 40.0f, 0.1f);
		const bool bWaterCloseUnderneathUs = Abs(fWaterHeight - vTargetPosition.z) < WATER_UNDERNEATH_DISTANCE ? true : false;
		//Check if we should set the parachute pack variation.
		const bool bIsVehicleNotInWater = (m_pVehicle->m_Buoyancy.GetStatus() == NOT_IN_WATER);
		bool bIsFlyingThroughWindScreen = IsFlagSet(CVehicleEnterExitFlags::ThroughWindscreen);
		bool bSetParachutePackVariation = (!bWaterCloseUnderneathUs && !bWaterUnderPed && !bFoundLand && !bIsVehicleInWater && bIsVehicleNotInWater && !bIsFlyingThroughWindScreen && CTaskParachute::DoesPedHaveParachute(*pPed));
		if(bSetParachutePackVariation)
		{
			//Passed all the simple checks -- time for some corner cases.

			//Calculate where we'll be in the future.
			static dev_float s_fTimeToLookAhead = 1.5f;
			Vector3 vVehicleVelocity = m_pVehicle->GetVelocity();
			vVehicleVelocity.z = Min(vVehicleVelocity.z, 0.0f);
			Vector3 vTargetPositionFuture = vTargetPosition + (vVehicleVelocity * s_fTimeToLookAhead);

			//Check if we'll end up underwater.
			static dev_float s_fProbeLength = 30.0f;
			Vector3 vTargetPositionFutureEnd = vTargetPositionFuture;
			vTargetPositionFutureEnd.z -= s_fProbeLength;
			if(CheckForUnderwater(vTargetPositionFutureEnd))
			{
				bSetParachutePackVariation = false;
			}
			//Check if we are not high enough.
			else if(!CheckForInAirExit(vTargetPositionFuture, s_fProbeLength))
			{
				bSetParachutePackVariation = false;
			}
		}

		//Set or clear the parachute pack variation.
		if(bSetParachutePackVariation)
		{
			CTaskParachute::SetParachutePackVariationForPed(*pPed);
		}
		else if(!GetPed()->GetPedResetFlag(CPED_RESET_FLAG_DisableTakeOffParachutePack))
		{
			CTaskParachute::ClearParachutePackVariationForPed(*pPed);
		}
	}

	if(!IsRunningFlagSet(CVehicleEnterExitFlags::IsArrest))
	{
		pPed->AttachPedToEnterCar(m_pVehicle, GetAttachToVehicleFlags(m_pVehicle), m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat), m_iTargetEntryPoint);
	}

	// Check this early, as the player in front will likely unreserve their door halfway through our exit
	s32 iFrontEntryPoint = CTaskVehicleFSM::GetEntryPointInFrontIfBeingUsed(m_pVehicle, m_iTargetEntryPoint, pPed);
	if(m_pVehicle->IsEntryIndexValid(iFrontEntryPoint))
	{
		SetRunningFlag(CVehicleEnterExitFlags::DoorInFrontBeingUsed);
	}

	CTaskExitVehicleSeat* pNewTask = rage_new CTaskExitVehicleSeat(m_pVehicle, m_iTargetEntryPoint, m_iRunningFlags, m_bIsRappel);

	if(pPed->IsNetworkClone())
	{
		if (!m_bIsRappel)
		{
			Assertf(m_bSubTaskExitRateSet,"%s expects the remote task to have had its own sub task started first",pPed->GetDebugName());
			pNewTask->SetCloneShouldUseCombatExitRate(m_bCloneShouldUseCombatExitRate);
		}
	}
	else
	{
		// force a high update level so that the clones get an update a.s.a.p after the exit vehicle is done (otherwise they can get stuck waiting)
		if (pPed->IsAPlayerPed() || pPed->IsLawEnforcementPed() || pPed->PopTypeIsMission())
		{
			NetworkInterface::ForceHighUpdateLevel(pPed);
		}
	}

	if (IsFlagSet(CVehicleEnterExitFlags::DeadFallOut))
		pNewTask->SetPreserveMatrixOnDetach(true);

	SetNewTask(pNewTask);

	if (pPed->IsLocalPlayer())
	{
		CTaskVehicleFSM::SetPassengersToLookAtPlayer(pPed, m_pVehicle, true);
	}

	m_fTimeSinceSubTaskFinished = 0.0f;
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::ExitSeat_OnUpdate()
{
	CPed* pPed = GetPed();
	
	if (m_moveNetworkHelper.IsNetworkActive() && m_moveNetworkHelper.GetBoolean(CTaskExitVehicle::ms_DeathInSeatInterruptId))
	{	
		pPed->SetPedResetFlag(CPED_RESET_FLAG_BlockRagdollActivationInVehicle, true);
	}

	//Move the handle bars for quad/bike hybrids back to neutral
	if (m_pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_IS_QUADBIKE_USING_BIKE_ANIMATIONS))
	{
		float fCurrentSteeringAngle = m_pVehicle->GetSteerAngle();
		const float fTimeFromMaxToStraight = 0.2f;
		if (fCurrentSteeringAngle != 0.0f)
		{
			Assertf(fCurrentSteeringAngle == fCurrentSteeringAngle, "fCurrentSteeringAngle is NAN");
			float fRemainingTimeToStraight = Abs(fCurrentSteeringAngle) * fTimeFromMaxToStraight;
			float fPercentageTimePassedThisTimeStep = fRemainingTimeToStraight < 0.00001f ? 0.0f : (GetTimeStep() / fRemainingTimeToStraight);
			float fAngleAdjustmentThisFrame = fCurrentSteeringAngle * fPercentageTimePassedThisTimeStep;
			float fNewSteeringAngle = fCurrentSteeringAngle - fAngleAdjustmentThisFrame;
			if ((fCurrentSteeringAngle > 0.0f && fNewSteeringAngle < 0.0f) || (fCurrentSteeringAngle < 0.0f && fNewSteeringAngle > 0.0f))
			{
				fNewSteeringAngle = 0.0f;
			}

			m_pVehicle->SetSteerAngle(fNewSteeringAngle);
		}
	}

	CTaskVehicleFSM::ProcessBlockRagdollActivation(*pPed, m_moveNetworkHelper);

	if (CheckForTransitionToRagdollState(*pPed, *m_pVehicle) || ShouldRagdollDueToReversing(*pPed, *m_pVehicle))
	{
		nmEntityDebugf(pPed, "CTaskExitVehicle::ExitSeat_OnUpdate - adding CTaskNMBalance");
		CEventSwitch2NM event(10000, rage_new CTaskNMBalance(500, 10000, NULL, 0));
		if (pPed->GetPedIntelligence()->AddEvent(event) != NULL)
		{
			SetTaskState(State_UnReserveSeatToFinish);
			return FSM_Continue;
		}
	}

	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished) || GetSubTask() == 0)
	{
		m_fTimeSinceSubTaskFinished += GetTimeStep();

		if(m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::ExitSeatOntoVehicle))
		{
			GetPed()->SetPedResetFlag(CPED_RESET_FLAG_IsGoingToStandOnExitedVehicle, true);
		}

		if (!pPed->IsNetworkClone() && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedsInVehiclePositionNeedsReset))
		{
			SetTaskState(State_UnReserveSeatToFinish);
			return FSM_Continue;
		}

		m_bWantsToEnter = GetSubTask() ? static_cast<CTaskExitVehicleSeat*>(GetSubTask())->WantsToEnter() : false;

		if (IsFlagSet(CVehicleEnterExitFlags::ThroughWindscreen))
		{
			if (pPed->IsNetworkClone())
			{
				SetTaskState(State_Finish);
				return FSM_Continue;
			}

			SetTaskState(State_UnReserveSeatToFinish);
			return FSM_Continue;
		}

        if(pPed->IsNetworkClone() && GetStateFromNetwork() == State_ExitSeat)
        {
			TUNE_GROUP_FLOAT(EXIT_VEHICLE_TUNE, MAX_WAIT_AFTER_SUBTASK_FINISHED, 0.5f, 0.0f, 2.0f, 0.01f);
			if (m_fTimeSinceSubTaskFinished < MAX_WAIT_AFTER_SUBTASK_FINISHED || !m_pVehicle->GetIsLandVehicle())
			{
				// wait for an update before deciding what to do next
				AI_LOG_WITH_ARGS("[ExitVehicle] Clone ped %s is waiting for remote ped to update network state, m_fTimeSinceSubTaskFinished = %.2f\n", AILogging::GetDynamicEntityNameSafe(pPed), m_fTimeSinceSubTaskFinished);
				return FSM_Continue;
			}
			else
			{
				AI_LOG_WITH_ARGS("[ExitVehicle] Clone ped %s has by passed waiting for remote ped to update network state, IsLandVehicle = %s\n", AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetBooleanAsString(m_pVehicle->GetIsLandVehicle()));
			}
        }

		m_iLastCompletedState = GetState();

        bool bHaveBeenJacked = false;
        bool bHasJumpedOut   = false;
		bool bHasExitedOnSideOrUpsideDown = false;
		bool bExitToSkyDive = false;
		bool bExitToWater = false;
		bool bExitToSwim = false;
		bool bFleeExit = false;

		if (GetSubTask())
		{
			s32 iPreviousState = GetSubTask()->GetPreviousState();
			bFleeExit = iPreviousState == CTaskExitVehicleSeat::State_FleeExit;
            bHaveBeenJacked = (iPreviousState == CTaskExitVehicleSeat::State_BeJacked || iPreviousState == CTaskExitVehicleSeat::State_StreamedBeJacked)? true : false;
            bHasJumpedOut |= iPreviousState == CTaskExitVehicleSeat::State_JumpOutOfSeat ? true : false;
			bExitToSkyDive |= IsFlagSet(CVehicleEnterExitFlags::IsTransitionToSkyDive);
			bExitToWater |= IsFlagSet(CVehicleEnterExitFlags::ExitUnderWater);
			bExitToSwim |= IsFlagSet(CVehicleEnterExitFlags::ExitToSwim);
            bHasJumpedOut |= iPreviousState == CTaskExitVehicleSeat::State_DetachedFromVehicle ? true : false;
			bHasExitedOnSideOrUpsideDown |= (iPreviousState == CTaskExitVehicleSeat::State_UpsideDownExit || iPreviousState == CTaskExitVehicleSeat::State_OnSideExit) ? true : false;
			m_bAimExit |= iPreviousState == CTaskExitVehicleSeat::State_ExitToAim ? true : false;
		}

		bool bWillCloseDoor = false;
		if (pPed->IsNetworkClone())
		{
			if (GetStateFromNetwork() <= State_ExitSeat && m_fTimeSinceSubTaskFinished < 0.5f)
			{
				return FSM_Continue;
			}
			else
			{
				bWillCloseDoor = m_bHasClosedDoor;
			}
		}
		else
		{
			bWillCloseDoor = GetWillClosedoor() && !m_bWantsToEnter;		
		}

		// If we're running the exit vehicle task as part of a sequence and its not the last task, early out once clear of the vehicle if not closing the door
		if (!bHasJumpedOut && !bWillCloseDoor && !pPed->GetIsInVehicle() && ShouldInterruptDueToSequenceTask())
		{
			if(taskVerifyf(!pPed->IsNetworkClone(), "Trying to unreserve the seat for a network clone!"))
			{
				SetTaskState(State_UnReserveSeatToFinish);
				return FSM_Continue;
			}
		}

		if (bFleeExit)
		{
			SetTaskState(pPed->IsNetworkClone() ? State_Finish : State_UnReserveSeatToFinish);
			return FSM_Continue;
		}

		if (CheckForClimbDown() && !bFleeExit && !bHaveBeenJacked && !bHasJumpedOut && !bExitToSkyDive && !bHasExitedOnSideOrUpsideDown)
		{
			if (pPed->IsNetworkClone())
			{
				SetTaskState(State_WaitForNetworkState);
				return FSM_Continue;
			}
			else
			{
				SetTaskState(State_UnReserveSeatToClimbDown);
				return FSM_Continue;
			}
		}

		const CVehicleEntryPointAnimInfo* pAnimInfo = m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint);
		if (!pPed->ShouldBeDead() && !bExitToSkyDive && !bExitToWater && !bExitToSwim && (bHasJumpedOut || (bHaveBeenJacked && pAnimInfo->GetUseGetUpAfterJack())))
		{
			if (pPed->IsNetworkClone())
			{
				// the ped may have been given a local NM jump roll from vehicle task by the exit vehicle seat subtask, in this case wait for it to be applied 
				if (GetPed() && GetPed()->GetPedIntelligence()->HasPendingLocalCloneTask())
				{
					return FSM_Continue;
				}

				SetTaskState(State_GetUp);
				return FSM_Continue;
			}
			else
			{
				SetTaskState(State_UnReserveSeatToGetUp);
				return FSM_Continue;
			}
		}

		if (bHasJumpedOut || bHasExitedOnSideOrUpsideDown || bExitToSkyDive || m_bAimExit || (pPed->IsNetworkClone() && !m_bHasClosedDoor))
		{
			bWillCloseDoor = false;
		}

		// CNC: Don't close doors when exiting a vehicle if wanted/will trigger action mode.
		if (CPlayerInfo::AreCNCResponsivenessChangesEnabled(pPed))
		{
			if (pPed->IsUsingActionMode())
			{
				bWillCloseDoor = false;
			}

			const CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo();
			if (pPlayerInfo)
			{
				const CWanted& rWanted = pPlayerInfo->GetWanted();
				if(rWanted.GetWantedLevel() >= WANTED_LEVEL1)
				{
					bWillCloseDoor = false;
				}
			}

			if (CTaskPlayerOnFoot::CNC_PlayerCopShouldTriggerActionMode(pPed))
			{
				bWillCloseDoor = false;
			}
		}

		bool bUseIdleTransition = !m_bIsRappel && !m_bAimExit && !bExitToSkyDive && !bHasJumpedOut && !bHaveBeenJacked && !bHasExitedOnSideOrUpsideDown && !m_pVehicle->InheritsFromBike() && 
			pAnimInfo->GetUsesNoDoorTransitionForExit() && !IsFlagSet(CVehicleEnterExitFlags::HasInterruptedAnim) && !IsFlagSet(CVehicleEnterExitFlags::JumpOut);
		
		CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();

		if (!pPed->IsNetworkClone())
		{
			//Check if the player is trying to aim or fire.
			bool bCanInterruptForAimOrFire = !m_bAimExit && !bExitToSkyDive && !bHasJumpedOut && !bHaveBeenJacked && !bHasExitedOnSideOrUpsideDown;
			if(m_bWantsToGoIntoCover || (bCanInterruptForAimOrFire && IsPlayerTryingToAimOrFire(*pPed)))
			{
				if(pWeaponManager && pWeaponManager->GetRequiresWeaponSwitch())
				{
					pWeaponManager->CreateEquippedWeaponObject();
				}

				if (m_bWantsToGoIntoCover)
				{
					pPed->SetPedResetFlag(CPED_RESET_FLAG_WantsToEnterCover, true);
					SetFlag(aiTaskFlags::ProcessNextActiveTaskImmediately);
				}

				//Early out, so we can go into the cover/aiming pose quickly.
				SetTaskState(State_UnReserveSeatToFinish);
				return FSM_Continue;
			}
		}
        else
        {
			CObject* pWeapon = pWeaponManager ? pWeaponManager->GetEquippedWeaponObject() : NULL;

			// make sure the player's held weapon is visible (this can be made invisible as the driveby task quits)
			if( pWeapon && !pWeapon->GetIsVisible() )
			{
				pWeapon->SetIsVisibleForModule( SETISVISIBLE_MODULE_GAMEPLAY, true, true );
			}
			
			if(!pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE))
            {
                bUseIdleTransition = false;
            }
        }

		if (!bWillCloseDoor)
		{
			if (GetPed()->IsNetworkClone())
			{
				AI_LOG_WITH_ARGS("[ExitVehicle] - Clone ped %s - deciding not to close door, m_bHasClosedDoor = %s\n", AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetBooleanAsString(m_bHasClosedDoor));

                if(bUseIdleTransition)
                {
				    SetTaskState(State_ExitSeatToIdle);
                }
                else
                {
                    SetTaskState(State_Finish);
                }
				return FSM_Continue;
			}
			else
			{	
				AI_LOG_WITH_ARGS("[ExitVehicle] - Local ped %s - deciding to close door, m_bHasClosedDoor = %s\n", AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetBooleanAsString(m_bHasClosedDoor));

				if (!bUseIdleTransition)
				{
					if (bExitToSkyDive)
					{
						return FSM_Continue; // Clones hold the last frame of the skydive pose and wait for the jump in air task to prevent popping back upright
					}
					else if (bExitToWater || bExitToSwim)
					{
						if (!GetPed()->IsNetworkClone())
						{
							if (GetPed()->GetPedResetFlag(CPED_RESET_FLAG_IsDrowning))
								GetPed()->SetIsSwimming(true);

							TUNE_GROUP_BOOL(EXIT_UNDER_WATER, DO_FORCE_MOTION_STATE, true);
							if (bExitToWater && DO_FORCE_MOTION_STATE)
							{
								GetPed()->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Diving_Swim);
							}
						}
						else
						{
							return FSM_Continue; // Clones hold the last frame of the skydive pose and wait for the jump in air task to prevent popping back upright
						}
					}
					SetTaskState(State_UnReserveSeatToFinish);
					return FSM_Continue;
				}
				else
				{
					SetTaskState(State_UnReserveSeatToIdle);
					return FSM_Continue;
				}
			}
		}
		else
		{
			SetTaskState(State_CloseDoor);
			return FSM_Continue;
		}
	}
	else
	{

		if( NetworkInterface::IsGameInProgress() && 
			!pPed->IsNetworkClone() && 
			!m_bSubTaskExitRateSet)
		{
			CTaskExitVehicleSeat* pSubTask = static_cast<CTaskExitVehicleSeat*>(FindSubTaskOfType(CTaskTypes::TASK_EXIT_VEHICLE_SEAT));
			if(pSubTask)
			{
				if(pSubTask->GetState() >= CTaskExitVehicleSeat::State_ExitSeat)
				{
					m_bSubTaskExitRateSet = true;
					m_bCloneShouldUseCombatExitRate = pSubTask->GetCloneShouldUseCombatExitRate();
					// Force an update of the ped's game state node, so that clone's have the correct latest info if changed
					if (pPed->GetNetworkObject())
					{
						CNetObjPed* pPedObj = SafeCast(CNetObjPed, pPed->GetNetworkObject());
						pPedObj->RequestForceSendOfTaskState();  //this force syncs the game state node which has the weapon info
					}
				}
			}
		}

		if(pPed->GetIsInVehicle())
		{
			CTaskExitVehicleSeat* pSubTask = static_cast<CTaskExitVehicleSeat*>(FindSubTaskOfType(CTaskTypes::TASK_EXIT_VEHICLE_SEAT));
			if(pSubTask && pSubTask->IsBeingArrested() && !pSubTask->CanBreakOutOfArrest() && pPed->GetAttachState() != ATTACH_STATE_PED_EXIT_CAR)
			{
				pPed->AttachPedToEnterCar(m_pVehicle, GetAttachToVehicleFlags(m_pVehicle), m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat), m_iTargetEntryPoint);
			}
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::ExitSeat_OnExit()
{
	CTaskVehicleFSM::ClearDoorDrivingFlags(m_pVehicle, m_iTargetEntryPoint);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicle::SetPedOut_OnEnter()
{
	taskAssert(m_iTargetEntryPoint != -1);

	CPed* pPed = GetPed();

	u32 iFlags = 0;
	if (m_bWarping)
	{
		// Set warp exit timer (used to prevent instant warp in if mashing entry button)
		if (pPed)
		{
			CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo();
			if(pPlayerInfo)
			{
				pPlayerInfo->SetLastTimeWarpedOutOfVehicle();
			}
		}

		iFlags |= CPed::PVF_Warp;
		iFlags |= CPed::PVF_ClearTaskNetwork;

		if(m_pVehicle->GetIsAircraft() && m_pVehicle->IsInAir())
		{
			iFlags |= CPed::PVF_DontNullVelocity;
			iFlags |= CPed::PVF_InheritVehicleVelocity;

			static const u32 CARGOBOB_LAYOUT_HASH = ATSTRINGHASH("LAYOUT_HELI_CARGOBOB", 0x5e7c2737);
			if(!IsRunningFlagSet(CVehicleEnterExitFlags::ExitSeatOntoVehicle))
			{
				if (m_pVehicle->GetLayoutInfo()->GetName().GetHash() != CARGOBOB_LAYOUT_HASH)
				{
					iFlags |= CPed::PVF_NoCollisionUntilClear;
				}
			}

			u32 iFallEventFlags = FF_InstantBlend;
			CTaskExitVehicle::ProcessForcedSkyDiveExitWarp(*m_pVehicle, m_iTargetEntryPoint, iFallEventFlags);
			CTaskExitVehicle::TriggerFallEvent(pPed, iFallEventFlags);

			//! Process the in-air event this frame, otherwise we end up with a frame of delay.
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAIUpdate, true);
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true);
		}
	}
	if (IsFlagSet(CVehicleEnterExitFlags::BeJacked))
	{
		iFlags |= CPed::PVF_IsBeingJacked;
	}
	if ((!pPed->IsPlayer()) && (m_pVehicle->GetHealth() > CAR_ON_FIRE_HEALTH)) // Player leaves engine running. If car is about to blow up leave engine running for dramatic effect
	{
		iFlags |= CPed::PVF_SwitchOffEngine;	
	}

	// Warp the ped in place, onto the nearest ground
	const CVehicleEntryPointInfo* pEntryInfo = m_pVehicle->GetEntryInfo(m_iTargetEntryPoint);
	if (pEntryInfo && pEntryInfo->GetWarpOutInPlace())
	{
		Vector3 vSetOutPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		dev_float RAISE_AMOUNT = 0.25f;
		vSetOutPos.z += RAISE_AMOUNT;
		Vector3 vGroundPos(Vector3::ZeroType);
		if (CTaskVehicleFSM::FindGroundPos(m_pVehicle, GetPed(), vSetOutPos, 2.5f, vGroundPos, false))
		{
			vSetOutPos.z = vGroundPos.z;
		}
		else
		{
			vSetOutPos.z -= RAISE_AMOUNT;
		}
		pPed->SetPosition(vSetOutPos, false);
		iFlags |= CPed::PVF_IgnoreSafetyPositionCheck;	
	}

	SetNewTask(rage_new CTaskSetPedOutOfVehicle(iFlags));
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::SetPedOut_OnUpdate()
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetTaskState(State_Finish);
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicle::CloseDoor_OnEnter()
{
    CPed *pPed = GetPed();
	CTaskVehicleFSM::SetRemotePedUsingDoorsTrue(*pPed, m_pVehicle, m_iTargetEntryPoint);

    // if we've reached this state of the clone task without playing the clips
    // the close door is dependent on we have to finish the task
    if(pPed->IsNetworkClone() && !IsMoveTransitionAvailable(GetState()))
    {
        SetStateFromNetwork(State_Finish);
    }
    else
    {
		const CVehicleEntryPointAnimInfo* pClipInfo = m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint);
		if (pClipInfo->GetHasClimbDown())
		{
			pPed->AttachPedToEnterCar(m_pVehicle, GetAttachToVehicleFlags(m_pVehicle), m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat), m_iTargetEntryPoint);
			UpdateAttachOffsetsFromCurrentPosition(*pPed);
		}
	    SetNewTask(rage_new CTaskCloseVehicleDoorFromOutside(m_pVehicle, m_iTargetEntryPoint));
		TUNE_GROUP_FLOAT(EXIT_VEHICLE_TUNE, INTERRUPT_DOORBLEND_DURATION, NORMAL_BLEND_DURATION, 0.0f, 1.0f, 0.01f);
		m_moveNetworkHelper.SetFloat(ms_ClosesDoorBlendDurationId, m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::HasInterruptedAnim) ? INTERRUPT_DOORBLEND_DURATION : 0.0f);

        m_bHasClosedDoor = true;
    }
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::CloseDoor_OnUpdate()
{
	CPed* pPed = GetPed();
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPhysicsTasks, true);

	if (CheckForTransitionToRagdollState(*pPed, *m_pVehicle) || ShouldRagdollDueToReversing(*pPed, *m_pVehicle))
	{
		nmEntityDebugf(pPed, "CTaskExitVehicle::CloseDoor_OnUpdate - adding CTaskNMBalance");
		CEventSwitch2NM event(10000, rage_new CTaskNMBalance(500, 10000, NULL, 0));
		if (pPed->GetPedIntelligence()->AddEvent(event) != NULL)
		{
			SetTaskState(State_UnReserveSeatToFinish);
			return FSM_Continue;
		}
	}

	TUNE_GROUP_BOOL(VEHICLE_TUNE, ALLOW_ABORT_CLOSE_DOOR_DUE_TO_ANG_VELOCITY, true);
	if (ALLOW_ABORT_CLOSE_DOOR_DUE_TO_ANG_VELOCITY && NetworkInterface::IsGameInProgress() && !pPed->IsNetworkClone())
	{
		const float fAngVelocity = Abs(m_pVehicle->GetAngVelocity().z);
		TUNE_GROUP_FLOAT(VEHICLE_TUNE, MAX_ANG_VELOCITY_TO_CLOSE_DOOR, 0.5f, 0.0f, 3.14f, 0.01f);
		if (fAngVelocity > MAX_ANG_VELOCITY_TO_CLOSE_DOOR)
		{
			SetTaskState(State_Finish);
			return FSM_Continue;
		}
	}

    if (GetIsSubtaskFinished(CTaskTypes::TASK_CLOSE_VEHICLE_DOOR_FROM_OUTSIDE))
	{
        m_iLastCompletedState = GetState();

		if (pPed->IsNetworkClone())
		{
			SetTaskState(State_Finish);
		}
		else
		{
			// Don't climb down if the landing gear is down
			const CVehicleEntryPointAnimInfo* pClipInfo = m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint);
			bool bShouldClimbDown = pClipInfo->GetHasClimbDown();
			if (m_pVehicle->InheritsFromPlane())
			{
				const CPlane* pPlane = static_cast<const CPlane*>(m_pVehicle.Get());
				if (pPlane->GetLandingGear().GetPublicState() != CLandingGear::STATE_LOCKED_DOWN)
				{
					bShouldClimbDown = false;
				}
			}
			else if( m_pVehicle->InheritsFromHeli() )
			{
				const CHeli* pHeli = static_cast<const CHeli*>(m_pVehicle.Get());
				if( pHeli->HasLandingGear() &&
					pHeli->GetLandingGear().GetPublicState() != CLandingGear::STATE_LOCKED_DOWN )
				{
					bShouldClimbDown = false;
				}
			}

			if (bShouldClimbDown)
			{
				SetTaskState(State_UnReserveSeatToClimbDown);
				return FSM_Continue;
			}
			else
			{
				SetTaskState(State_UnReserveSeatToFinish);
				return FSM_Continue;
			}
		}
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::CloseDoor_OnExit()
{
	//@@: location CTASKEXITVEHICLE_CLOSEDOOR_ONEXIT
	CTaskVehicleFSM::SetRemotePedUsingDoorsFalse(*GetPed(), m_pVehicle, m_iTargetEntryPoint);
	CTaskVehicleFSM::ClearDoorDrivingFlags(m_pVehicle, m_iTargetEntryPoint);
	s32 iRearEntryIndex = CTaskVehicleFSM::FindRearEntryForEntryPoint(m_pVehicle, m_iTargetEntryPoint);
	CTaskVehicleFSM::ClearDoorDrivingFlags(m_pVehicle, iRearEntryIndex);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::ExitSeatToIdle_OnEnter()
{
	const CVehicleEntryPointAnimInfo* pClipInfo = m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint);
	if (pClipInfo->GetHasClimbDown())
	{
		CPed* pPed = GetPed();
		pPed->AttachPedToEnterCar(m_pVehicle, GetAttachToVehicleFlags(m_pVehicle), m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat), m_iTargetEntryPoint);
		UpdateAttachOffsetsFromCurrentPosition(*pPed);
	}
	SetMoveNetworkState(ms_ToIdleRequestId, ms_ToIdleOnEnterId, ms_ToIdleStateId);
	m_bCurrentAnimFinished = false;
	RequestProcessMoveSignalCalls();
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::ExitSeatToIdle_OnUpdate()
{
	CPed* pPed = GetPed();
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPhysicsTasks, true);
	pPed->SetPedResetFlag(CPED_RESET_FLAG_RequiresLegIk, true);

	const CVehicleEntryPointAnimInfo* pEntryAnimInfo = m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint);
	if (pEntryAnimInfo && pEntryAnimInfo->GetDontCloseDoorOutside())
	{
		pPed->GetIkManager().SetFlag(PEDIK_LEGS_INSTANT_SETUP);
	}
	else
	{
		pPed->GetIkManager().SetFlag(PEDIK_LEGS_USE_ANIM_BLOCK_TAGS);
	}

	if (pPed->IsNetworkClone() && GetTimeInState() > 4.0f && GetStateFromNetwork() == State_Finish)
	{
		aiWarningf("Frame: %i, Safety bail out for clone hit, previous state %s", fwTimer::GetFrameCount(), GetStateName(GetPreviousState()));
		pPed->DetachFromParent(DETACH_FLAG_EXCLUDE_VEHICLE);
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	bool bUnused = false;
	
	if (m_moveNetworkHelper.GetBoolean(ms_NoDoorInterruptId))
	{
		VehicleEnterExitFlags vehicleFlags;

		if(pPed->IsLocalPlayer())
		{
			vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::PlayerControlledVehicleEntry);
		}

		if (m_bWantsToGoIntoCover|| m_bWantsToEnter || CTaskVehicleFSM::CheckEarlyExitConditions(*pPed, *m_pVehicle, vehicleFlags, bUnused, true))
		{
			if (m_bWantsToGoIntoCover|| m_bWantsToEnter)
			{
				if (m_bWantsToEnter)
					pPed->SetPedResetFlag(CPED_RESET_FLAG_WantsToEnterVehicleFromCover, true);
				else if (m_bWantsToGoIntoCover)
					pPed->SetPedResetFlag(CPED_RESET_FLAG_WantsToEnterCover, true);

				SetFlag(aiTaskFlags::ProcessNextActiveTaskImmediately);
			}
			pPed->DetachFromParent(DETACH_FLAG_EXCLUDE_VEHICLE);
			SetTaskState(State_Finish);
			return FSM_Continue;
		}
	}

	if (m_bCurrentAnimFinished)
	{
		const CVehicleEntryPointAnimInfo* pClipInfo = m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint);
		if (pClipInfo->GetHasClimbDown())
		{
			m_iLastCompletedState = GetState();
			bool bShouldClimbDown = true;

			// Don't climb down if the landing gear is down
			if( !pPed->IsNetworkClone() )
			{
				if( m_pVehicle->InheritsFromPlane() )
				{
					const CPlane* pPlane = static_cast<const CPlane*>(m_pVehicle.Get());
					if (pPlane->GetLandingGear().GetPublicState() != CLandingGear::STATE_LOCKED_DOWN)
					{
						bShouldClimbDown = false;
					}
				}
				else if( m_pVehicle->InheritsFromHeli() )
				{
					const CHeli* pHeli = static_cast<const CHeli*>(m_pVehicle.Get());
					if( pHeli->HasLandingGear() && 
						pHeli->GetLandingGear().GetPublicState() != CLandingGear::STATE_LOCKED_DOWN )
					{
						bShouldClimbDown = false;
					}
				}
			}

			if (bShouldClimbDown)
			{
				SetTaskState(State_UnReserveSeatToClimbDown);
				return FSM_Continue;
			}
			else if (!pPed->IsNetworkClone())
			{
				SetTaskState(State_UnReserveSeatToFinish);
				return FSM_Continue;
			}
			else
			{
				SetTaskState(State_WaitForNetworkState);
				return FSM_Continue;
			}
		}
		else
		{
			SetTaskState(State_Finish);
			return FSM_Continue;
		}
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicle::Finish_OnEnterClone()
{
    CPed *pPed = GetPed();

    gnetAssertf(pPed->IsNetworkClone(), "Calling Finish_OnEnterClone() on a locally owned ped!");

    if(pPed->GetIsInVehicle() && !NetworkInterface::IsPedInVehicleOnOwnerMachine(pPed))
    {
        pPed->SetPedOutOfVehicle(0);
    }

    // the exit vehicle clone task can finish before the peds vehicle state has
    // been updated over the network, which can lead to the ped being popped back into
    // the vehicle for a frame or two. This prevents the network code from updating
    // the peds vehicle state during this interval
    const unsigned IGNORE_VEHICLE_UPDATES_TIME = 250;
    NetworkInterface::IgnoreVehicleUpdates(GetPed(), IGNORE_VEHICLE_UPDATES_TIME);
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::Finish_OnUpdateClone()
{
	CPed* pPed = GetPed();

	if (pPed->IsDead() || pPed->IsFatallyInjured())
	{
		// we have to wait for the dead dying task to take over and ragdoll the ped
		if (!pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_DYING_DEAD))
		{
			if (GetTimeInState() < 2.0f)
			{
				AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(pPed, "Ped %s is waiting for dying dead task to start on remote machine because this clone ped is dead, time in state %.2f", AILogging::GetDynamicEntityNameSafe(pPed), GetTimeInState());
				return FSM_Continue;
			}
		}
	}

	if (!pPed->GetIsInVehicle())
	{
		if ((pPed->GetAttachState() != ATTACH_STATE_DETACHING) && !IsFlagSet(CVehicleEnterExitFlags::IgnoreExitDetachOnCleanup))
		{
			pPed->DetachFromParent(0);
		}
	}

	return FSM_Quit;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::UnReserveSeatToIdle_OnUpdate()
{
	CPed* pPed = GetPed();
	CComponentReservation* pComponentReservation = m_pVehicle->GetComponentReservationMgr()->GetSeatReservation(m_iTargetEntryPoint, SA_directAccessSeat);
	if (pComponentReservation && pComponentReservation->IsComponentReservedForPed(pPed))
	{
		CComponentReservationManager::UnreserveVehicleComponent(pPed, pComponentReservation);
	}

	SetTaskState(State_UnReserveDoorToIdle);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::UnReserveDoorToIdle_OnUpdate()
{
	CPed* pPed = GetPed();
	CComponentReservation* pComponentReservation = m_pVehicle->GetComponentReservationMgr()->GetDoorReservation(m_iTargetEntryPoint);
	if (pComponentReservation && pComponentReservation->IsComponentReservedForPed(pPed))
	{
		CComponentReservationManager::UnreserveVehicleComponent(pPed, pComponentReservation);
	}
	SetTaskState(State_ExitSeatToIdle);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::UnReserveSeatToFinish_OnUpdate()
{
	CPed* pPed = GetPed();
	CComponentReservation* pComponentReservation = m_pVehicle->GetComponentReservationMgr()->GetSeatReservation(m_iTargetEntryPoint, SA_directAccessSeat);
	if (pComponentReservation && pComponentReservation->IsComponentReservedForPed(pPed))
	{
		CComponentReservationManager::UnreserveVehicleComponent(pPed, pComponentReservation);
	}

	if (!IsFlagSet(CVehicleEnterExitFlags::DontSetPedOutOfVehicle))
	{
		pPed->DetachFromParent(DETACH_FLAG_EXCLUDE_VEHICLE);
	}
	SetTaskState(State_UnReserveDoorToFinish);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::UnReserveDoorToFinish_OnUpdate()
{
	CPed* pPed = GetPed();
	CComponentReservation* pComponentReservation = m_pVehicle->GetComponentReservationMgr()->GetDoorReservation(m_iTargetEntryPoint);
	if (pComponentReservation && pComponentReservation->IsComponentReservedForPed(pPed))
	{
		CComponentReservationManager::UnreserveVehicleComponent(pPed, pComponentReservation);
	}
	SetTaskState(State_Finish);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::UnReserveSeatToClimbDown_OnUpdate()
{
	CPed* pPed = GetPed();
	CComponentReservation* pComponentReservation = m_pVehicle->GetComponentReservationMgr()->GetDoorReservation(m_iTargetEntryPoint);
	if (pComponentReservation && pComponentReservation->IsComponentReservedForPed(pPed))
	{
		CComponentReservationManager::UnreserveVehicleComponent(pPed, pComponentReservation);
	}
	SetTaskState(State_UnReserveDoorToClimbDown);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::UnReserveDoorToClimbDown_OnUpdate()
{
	CPed* pPed = GetPed();
	CComponentReservation* pComponentReservation = m_pVehicle->GetComponentReservationMgr()->GetSeatReservation(m_iTargetEntryPoint, SA_directAccessSeat);
	if (pComponentReservation && pComponentReservation->IsComponentReservedForPed(pPed))
	{
		CComponentReservationManager::UnreserveVehicleComponent(pPed, pComponentReservation);
	}

	SetTaskState(State_ClimbDown);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::UnReserveSeatToGetUp_OnUpdate()
{
	CPed* pPed = GetPed();
	CComponentReservation* pComponentReservation = m_pVehicle->GetComponentReservationMgr()->GetDoorReservation(m_iTargetEntryPoint);
	if (pComponentReservation && pComponentReservation->IsComponentReservedForPed(pPed))
	{
		CComponentReservationManager::UnreserveVehicleComponent(pPed, pComponentReservation);
	}
	SetTaskState(State_UnReserveDoorToGetUp);
	return FSM_Continue;
}
////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::UnReserveDoorToGetUp_OnUpdate()
{
	CPed* pPed = GetPed();
	CComponentReservation* pComponentReservation = m_pVehicle->GetComponentReservationMgr()->GetSeatReservation(m_iTargetEntryPoint, SA_directAccessSeat);
	if (pComponentReservation && pComponentReservation->IsComponentReservedForPed(pPed))
	{
		CComponentReservationManager::UnreserveVehicleComponent(pPed, pComponentReservation);
	}

	SetTaskState(State_GetUp);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::WaitForNetworkState_OnUpdate()
{
	const s32 iNetworkState = GetStateFromNetwork();
	if (iNetworkState >= State_ClimbDown || (IsFlagSet(CVehicleEnterExitFlags::IsFleeExit) && iNetworkState == State_ExitSeat))
	{
		SetTaskState(iNetworkState);
		return FSM_Continue;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::ClimbDown_OnEnter()
{
	if (m_pVehicle->GetLayoutInfo()->GetClimbUpAfterOpenDoor() || IsPlaneHatchEntry(m_iTargetEntryPoint))
	{
		CPed& rPed = *GetPed();
		rPed.AttachPedToEnterCar(m_pVehicle, GetAttachToVehicleFlags(m_pVehicle), m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat), m_iTargetEntryPoint);
		
		if (rPed.GetCurrentPhysicsInst())
		{
			// Grab the level index.
			u16 uLevelIndex = rPed.GetCurrentPhysicsInst()->GetLevelIndex();
			// Allow the ped to collide with other inactive things.
			PHLEVEL->SetInactiveCollidesAgainstInactive(uLevelIndex, true);
			m_bSetInactiveCollidesAgainstInactive = true;
		}

		UpdateAttachOffsetsFromCurrentPosition(rPed);
	}
	SetMoveNetworkState(ms_ClimbDownRequestId, ms_ClimbDownOnEnterId, ms_ClimbDownStateId);
	SetClipsForState();
	SetClipPlaybackRate();	

	// This must be done after the SetClipsForState call because we use the cached clip transform results
	StoreInitialOffsets();	
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::ClimbDown_OnUpdate()
{
	CPed& rPed = *GetPed();
	rPed.SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );

	float fPhase = 0.0f;
	GetClipAndPhaseForState(fPhase);
	CTaskVehicleFSM::ProcessLegIkBlendIn(rPed, m_fLegIkBlendInPhase, fPhase);

	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	bool bShouldFinish = false;
	if (m_moveNetworkHelper.GetBoolean(ms_ClimbDownInterruptId) && CTaskVehicleFSM::CheckForPlayerMovement(rPed))
	{
		bShouldFinish = true;
	}

	const CVehicleEntryPointInfo* pEntryInfo = m_pVehicle->GetEntryInfo(m_iTargetEntryPoint);
	bool bHasClimbDownToWater = pEntryInfo && pEntryInfo->GetHasClimbDownToWater();
	if (bHasClimbDownToWater && m_pVehicle->GetIsInWater() && m_moveNetworkHelper.GetBoolean(CTaskExitVehicleSeat::ms_ExitToSwimInterruptId))
	{
		bShouldFinish = true;
	}

	if (IsMoveNetworkStateFinished(ms_ClimbDownAnimFinishedId, ms_ClimbDownAnimPhaseId))
	{
		bShouldFinish = true;
	}

	if (bShouldFinish)
	{
		if (rPed.GetIsInVehicle())
		{
			rPed.SetPedOutOfVehicle(CPed::PVF_ExcludeVehicleFromSafetyCheck | CPed::PVF_DontNullVelocity);
		}
		else
		{
			rPed.DetachFromParent(DETACH_FLAG_EXCLUDE_VEHICLE);
		}
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::GetUp_OnEnter()
{
	CPed* pPed = GetPed();
	pPed->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, SLOW_BLEND_DURATION);
	SetNewTask(rage_new CTaskGetUp());
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::GetUp_OnUpdate()
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetTaskState(State_Finish);
		return FSM_Continue;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::DetermineFleeExitPosition_OnUpdate()
{
	// Do a little bit of initial logic to decide where our ideal flee start position should be

	// Get the flee threat position
	Vec3V vPosition(V_ZERO);
	const CVehicle& rVeh = *m_pVehicle;	// Could this crash?
	const CPed& rPed = *GetPed();

	const s32 iEntryIndex = GetDirectEntryPointForPed();
	if (!rVeh.IsEntryIndexValid(iEntryIndex))
	{
		SetRunningFlag(CVehicleEnterExitFlags::DontSetPedOutOfVehicle);
		return FSM_Quit;
	}

	if (!taskVerifyf(GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_SMART_FLEE, "Exit task not being run as a child of smart flee"))
	{
		SetRunningFlag(CVehicleEnterExitFlags::DontSetPedOutOfVehicle);
		return FSM_Quit;
	}
	
	Vector3 vEntryPosition;
	Quaternion qEntryOrientation;
	CModelSeatInfo::CalculateEntrySituation(&rVeh, &rPed, vEntryPosition, qEntryOrientation, iEntryIndex);

	float fDesiredFleeExitAngleOffset = 0.0f; 
	const CVehicleEntryPointInfo* pEntryInfo = m_pVehicle->GetEntryInfo(iEntryIndex);
	const bool bIsLeftSeat = pEntryInfo->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT;
	const float fFwdHeading = rVeh.GetTransform().GetHeading();
	const float fRearHeading = fwAngle::LimitRadianAngle(fFwdHeading + PI);
	const float fSideHeading =  fwAngle::LimitRadianAngle(fFwdHeading + (bIsLeftSeat ? HALF_PI : -HALF_PI));	

	const CTaskSmartFlee* pFleeTask = static_cast<CTaskSmartFlee*>(GetParent());
	const CAITarget& rFleeTarget = pFleeTask->GetFleeTarget();
	const Vector3 vSide = bIsLeftSeat ? -VEC3V_TO_VECTOR3(rVeh.GetTransform().GetA()) : VEC3V_TO_VECTOR3(rVeh.GetTransform().GetA());

	bool bRunToRear = false;
	bool bNeedToDoProbeCheck = true;

	Vector3 vFleeFromPosition(Vector3::ZeroType);
	Vector3 vDesiredExitPos(0.0f, ms_Tunables.m_ExitDistance, 0.0f);
	if (rFleeTarget.GetIsValid() && rFleeTarget.GetPosition(vFleeFromPosition))
	{
		Vector3 vToFleePos = vFleeFromPosition - vEntryPosition;
		vToFleePos.Normalize();

		const Vector3 vVehFwd = VEC3V_TO_VECTOR3(rVeh.GetTransform().GetB());
		Vector3 vSideVec = VEC3V_TO_VECTOR3(rVeh.GetTransform().GetA());
		if (bIsLeftSeat)
		{
			vSideVec *= -1.0f;
		}

		TUNE_GROUP_FLOAT(FLEE_TUNE, MIN_FWD_DOT, 0.707f, 0.0f, 1.0f, 0.001f);
		const bool bThreatInFront = vToFleePos.Dot(vVehFwd) > MIN_FWD_DOT ? true : false;
		// Always try to run to the rear if the threat is in front (door is like a shield) 
		if (bThreatInFront)
		{
			bRunToRear = true;
		}
		else 
		{
			// Always try to run directly away to the side if the threat is away from us (needs to travel around the vehicle)
			const bool bTargetOnRightSide = vToFleePos.CrossZ(vVehFwd) > 0.0f;
			const bool bThreatOnOppositeSide = bTargetOnRightSide == bIsLeftSeat;
			if (!bThreatOnOppositeSide)
			{
				const float fSideDot = vSide.Dot(vToFleePos);
				TUNE_GROUP_FLOAT(FLEE_TUNE, MIN_SIDE_DOT, 0.707f, 0.0f, 1.0f, 0.001f);
				if (fSideDot >= MIN_SIDE_DOT)
				{
					bRunToRear = true;
				}
			}
		}

		fDesiredFleeExitAngleOffset = bRunToRear ? fRearHeading : fSideHeading;
		vDesiredExitPos = ComputeExitPosition(vEntryPosition, ms_Tunables.m_ExitProbeDistance, fDesiredFleeExitAngleOffset, bRunToRear, vSideVec);

		// Do a bit of probing to make sure we're not going to run into walls
		if (DoesProbeHitSomething(vEntryPosition, vDesiredExitPos))
		{
			bRunToRear = !bRunToRear;
			fDesiredFleeExitAngleOffset = bRunToRear ? fRearHeading : fSideHeading;
			vDesiredExitPos = ComputeExitPosition(vEntryPosition, ms_Tunables.m_ExitProbeDistance, fDesiredFleeExitAngleOffset, bRunToRear, vSideVec);

			if (DoesProbeHitSomething(vEntryPosition, vDesiredExitPos))
			{
				fDesiredFleeExitAngleOffset = fwAngle::LimitRadianAngle(fSideHeading + fRearHeading);
				vDesiredExitPos = ComputeExitPosition(vEntryPosition, ms_Tunables.m_ExitProbeDistance, fDesiredFleeExitAngleOffset, bRunToRear, vSideVec);
			}
			else
			{
				bNeedToDoProbeCheck = false;
			}
		}
		else
		{
			bNeedToDoProbeCheck = false;
		}
	}
	else
	{
		// Try to run 45 degrees
		fDesiredFleeExitAngleOffset = fwAngle::LimitRadianAngle(fSideHeading + fRearHeading);
	}

	// Probe
	if (bNeedToDoProbeCheck)
	{
		vDesiredExitPos = ComputeExitPosition(vEntryPosition, ms_Tunables.m_ExitProbeDistance, fDesiredFleeExitAngleOffset, bRunToRear, vSide);
		if (DoesProbeHitSomething(vEntryPosition, vDesiredExitPos))
		{
			// Fuck it, stay in the vehicle and panic
			SetRunningFlag(CVehicleEnterExitFlags::DontSetPedOutOfVehicle);
			SetTaskState(State_Finish);
			return FSM_Continue;
		}
	}

	vDesiredExitPos = ComputeExitPosition(vEntryPosition, ms_Tunables.m_ExitDistance, fDesiredFleeExitAngleOffset, bRunToRear, vSide);

#if DEBUG_DRAW
	CTask::ms_debugDraw.AddArrow(RCC_VEC3V(vEntryPosition), RCC_VEC3V(vDesiredExitPos), 0.25f, Color_blue, 2500, 0);
#endif // DEBUG_DRAW

	m_vExitPosition = RCC_VEC3V(vDesiredExitPos);
	m_bHasValidExitPosition = true;
	SetTaskState(State_WaitForFleePath);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::WaitForFleePath_OnUpdate()
{
	const CPed& rPed = *GetPed();
	// If someone reserves this seat in order to jack us, don't bother waiting for a path, just do a normal exit B*1897230
	if (m_pVehicle)
	{
		const s32 iMySeat = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(&rPed);
		if (!m_pVehicle->IsSeatIndexValid(iMySeat))
		{
			// Something went wrong, ped isn't in the vehicle
			SetState(State_Finish);
			return FSM_Continue;
		}

		CComponentReservation* pComponentReservation = m_pVehicle->GetComponentReservationMgr()->GetSeatReservationFromSeat(iMySeat);
		if (pComponentReservation && pComponentReservation->GetPedUsingComponent() && pComponentReservation->GetPedUsingComponent() != &rPed)
		{
			ClearRunningFlag(CVehicleEnterExitFlags::IsFleeExit);
			SetTaskState(State_StreamAssetsForExit);
			return FSM_Continue;
		}
	}

	CTaskMoveFollowNavMesh* pNavTask = static_cast<CTaskMoveFollowNavMesh*>(rPed.GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH));
	if (pNavTask && pNavTask->GetRoute() && pNavTask->GetRouteSize() > 0)
	{
		if (CheckIsRouteValidForFlee(pNavTask))
		{
			SetState(State_ReserveSeat);
			return FSM_Continue;
		}
		else
		{
			// Invalid route, lets stay safe in our vehicle (hopefully)
			SetState(State_Finish);
			return FSM_Continue;
		}
	}
	else if (GetTimeInState() > 5.0f)
	{
		// Failed to find a route, quit out
		SetState(State_Finish);
		return FSM_Continue;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicle::Finish_OnUpdate()
{
	CPed* pPed = GetPed();

	if (!pPed)
	{
		return FSM_Quit;
	}

	if (!pPed->IsNetworkClone() && !pPed->IsPlayer() && m_pPedJacker)
	{
		// Peds get sent a dragged out event so they can react once they've been jacked, for ambient peds we make them wander off if they were commandeered
		if (!m_pPedJacker->GetPedConfigFlag(CPED_CONFIG_FLAG_WillCommandeerRatherThanJack))
		{
			bool bForceFlee = IsRunningFlagSet(CVehicleEnterExitFlags::ForceFleeAfterJack);
			CEventDraggedOutCar event(m_pVehicle, m_pPedJacker, m_iSeat == 0 ? true : false, bForceFlee);
			//Force persist event to make sure if we go in nm then the ped will still react to being dragged out of a vehicle.
			pPed->GetPedIntelligence()->AddEvent(event,true);
		}
	}

	pPed->SetPedResetFlag(CPED_RESET_FLAG_ExitVehicleTaskFinishedThisFrame, true);

	if (!pPed->IsNetworkClone())
	{
		// force the ped so send the tasks immediately, so that the clones know to finish the task a.s.a.p
		NetworkInterface::ForceTaskStateUpdate(*pPed);
	}

	return FSM_Quit;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::IsMoveTransitionAvailable(s32 iNextState) const
{
    switch(iNextState)
    {
    case State_ShuffleToSeat:
    case State_ExitSeat:
        return (m_iLastCompletedState == -1);
    case State_CloseDoor:
	case State_ExitSeatToIdle:
        return (m_iLastCompletedState == State_ExitSeat);
	case State_ClimbDown:
		return (m_iLastCompletedState == State_ExitSeatToIdle) || (m_iLastCompletedState == State_ExitSeat);
    default:
        return true;
    }
}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicle::AssignVehicleTask()
{
	CPed* pPed = GetPed();

	Assert (!m_pVehicle->IsNetworkClone());

	bool bUsingAutoPilot = m_pVehicle->GetIsUsingScriptAutoPilot();

	if (m_pVehicle->InheritsFromTrailer() || m_pVehicle->m_nVehicleFlags.bHasParentVehicle)
	{
		CTask* pTask = rage_new CTaskVehicleNoDriver();
		m_pVehicle->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, pTask, VEHICLE_TASK_PRIORITY_PRIMARY, false);
	}
	// Setup the cars temp action, if the ped is being forced out they either accelerate or break
	else if (IsFlagSet(CVehicleEnterExitFlags::ForcedExit) && !pPed->IsPlayer())
	{
		float fRandom = (float)( pPed->GetRandomSeed()/(float)RAND_MAX_16 );

		CTask* pTask = NULL;
		if (fRandom < 0.25f)
		{
			pTask = rage_new CTaskVehicleReverse(NetworkInterface::GetSyncedTimeInMilliseconds() + 3000, CTaskVehicleReverse::Reverse_Opposite_Direction);
		}
		else if (fRandom < 0.5f)
		{
			pTask = rage_new CTaskVehicleGoForward(NetworkInterface::GetSyncedTimeInMilliseconds() + 3000);
		}

		m_pVehicle->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, pTask, VEHICLE_TASK_PRIORITY_PRIMARY, false);
	}
	// Accelerate if jumping out
	else if (IsFlagSet(CVehicleEnterExitFlags::JumpOut))
	{
		if (!bUsingAutoPilot && m_pVehicle->IsDriver(pPed) && !m_pVehicle->GetIsAquatic())
		{
			// If the vehicle is going forwards as the driver jumps out, make it keep going forwards, otherwise reverse
			Vec3V vVelocity  = VECTOR3_TO_VEC3V(m_pVehicle->GetVelocity());
			vVelocity = Normalize(vVelocity);
			ScalarV vDot = Dot(vVelocity, m_pVehicle->GetTransform().GetB());

			CTask* pTask = NULL;
			if (IsGreaterThanAll(vDot, ScalarV(V_ZERO)))
			{
				pTask = rage_new CTaskVehicleGoForward(NetworkInterface::GetSyncedTimeInMilliseconds() + 3000);
			}
			else
			{
				pTask = rage_new CTaskVehicleReverse(NetworkInterface::GetSyncedTimeInMilliseconds() + 3000, CTaskVehicleReverse::Reverse_Opposite_Direction);
			}

			m_pVehicle->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, pTask, VEHICLE_TASK_PRIORITY_PRIMARY, false);
		}
	}
	// Otherwise stop the car
	else
	{
		if (!bUsingAutoPilot && m_pVehicle->IsDriver(pPed))
		{
			CTask* pTask = rage_new CTaskVehicleStop(0);
			m_pVehicle->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, pTask, VEHICLE_TASK_PRIORITY_PRIMARY, false);
		}
	}

	m_bVehicleTaskAssigned = true;
}

////////////////////////////////////////////////////////////////////////////////

const fwMvClipSetId CTaskExitVehicle::GetMoveNetworkClipSet(bool bUseDirectEntryPointIndex)
{
	if(!m_bIsRappel)
	{
		if ( (MI_BOAT_SPEEDER.IsValid() && m_pVehicle->GetModelId() == MI_BOAT_SPEEDER) ||
			 (MI_BOAT_SPEEDER2.IsValid() && m_pVehicle->GetModelId() == MI_BOAT_SPEEDER2) ||
			 (MI_BOAT_TORO.IsValid() && m_pVehicle->GetModelId() == MI_BOAT_TORO) ||
			 (MI_BOAT_TORO2.IsValid() && m_pVehicle->GetModelId() == MI_BOAT_TORO2))
		{
			bUseDirectEntryPointIndex = true;
		}

		s32 iTargetExitPoint = bUseDirectEntryPointIndex ? m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(m_iSeat, m_pVehicle) : m_iTargetEntryPoint;
		taskFatalAssertf(m_pVehicle->GetEntryAnimInfo(iTargetExitPoint), "NULL Entry clip Info for entry index %i", iTargetExitPoint);
		return CTaskExitVehicleSeat::GetExitClipsetId(m_pVehicle, iTargetExitPoint, m_iRunningFlags, GetPed());
	}
	else
	{
		return SelectRappelClipsetForHeliSeat(m_pVehicle, m_iSeat);
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::RequestClips()
{
	bool bCommonClipSetConditionPassed = true;
	if (IsFlagSet(CVehicleEnterExitFlags::VehicleIsUpsideDown) || IsFlagSet(CVehicleEnterExitFlags::VehicleIsOnSide))
	{
		const fwMvClipSetId commonClipsetId = CTaskEnterVehicleAlign::GetCommonClipsetId(m_pVehicle, m_iTargetEntryPoint);
		bCommonClipSetConditionPassed = CTaskVehicleFSM::RequestVehicleClipSetReturnIsLoaded(commonClipsetId, SP_High);
#if __BANK
		CTaskVehicleFSM::SpewStreamingDebugToTTY(*GetPed(), *m_pVehicle, m_iTargetEntryPoint, commonClipsetId, GetTimeInState());
#endif // __BANK
	}

	if (!m_bIsRappel)
	{
		const fwMvClipSetId exitClipsetId = CTaskExitVehicleSeat::GetExitClipsetId(m_pVehicle, m_iTargetEntryPoint, m_iRunningFlags, GetPed());
		const bool bExitClipSetLoaded = CTaskVehicleFSM::RequestVehicleClipSetReturnIsLoaded(exitClipsetId, SP_High);
#if __BANK
		CTaskVehicleFSM::SpewStreamingDebugToTTY(*GetPed(), *m_pVehicle, m_iTargetEntryPoint, exitClipsetId, GetTimeInState());
#endif // __BANK
		
		return bCommonClipSetConditionPassed && bExitClipSetLoaded;
	}
	else
	{
		fwMvClipSetId descendClipSet = SelectRappelClipsetForHeliSeat(m_pVehicle, m_iSeat);
#if __BANK
		CTaskVehicleFSM::SpewStreamingDebugToTTY(*GetPed(), *m_pVehicle, m_iSeat, descendClipSet, GetTimeInState());
#endif // __BANK
		return CTaskVehicleFSM::RequestVehicleClipSetReturnIsLoaded(descendClipSet, SP_High);
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::CheckForUpsideDownExit() const
{
	TUNE_GROUP_BOOL(VEHICLE_TUNE, ENABLE_UPSIDE_DOWN_EXIT, true);
	if (ENABLE_UPSIDE_DOWN_EXIT && 
		(m_pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR || 
		 m_pVehicle->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE || 
		 m_pVehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR || 
		 m_pVehicle->GetVehicleType() == VEHICLE_TYPE_PLANE || 
		 m_pVehicle->GetVehicleType() == VEHICLE_TYPE_HELI))
	{
		if (CVehicle::GetVehicleOrientation(*m_pVehicle) == CVehicle::VO_UpsideDown)
		{
			return true;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::CheckForOnsideExit() const
{
	TUNE_GROUP_BOOL(VEHICLE_TUNE, ENABLE_ONSIDE_EXIT, true);
	if (ENABLE_ONSIDE_EXIT && (m_pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR || m_pVehicle->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE))
	{
		if (CVehicle::GetVehicleOrientation(*m_pVehicle) == CVehicle::VO_OnSide)
		{
			return true;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::CheckIfFleeExitIsValid() const
{
	const CPed* pPed = GetPed();

	if(pPed->IsNetworkClone())
	{
		return true;
	}

	if (!HasValidExitPosition())
	{
		return false;
	}

	Vector3 vStartPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	Vector3 vExitPosition = VEC3V_TO_VECTOR3(GetExitPosition());

	static const int nTestTypes = ArchetypeFlags::GTA_MAP_TYPE_MOVER|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE;

	const CEntity* exclusionList[MAX_INDEPENDENT_VEHICLE_ENTITY_LIST];
	int nExclusions = 0;

	pPed->GeneratePhysExclusionList(exclusionList, nExclusions, MAX_NUM_ENTITIES_ATTACHED_TO_PED, nTestTypes,TYPE_FLAGS_ALL);

	// This will prevent WorldProbe::CShapeTestTaskData::ConvertExcludeEntitiesToInstances from adding the vehicle the ped is attached to
	u32 excludeEntityOptions = WorldProbe::EIEO_DONT_ADD_VEHICLE;

	// Exclude the ped
	exclusionList[nExclusions++] = pPed;

	// Exclude the vehicle we are testing
	exclusionList[nExclusions++] = m_pVehicle;

	// Exclude objects attached to the vehicle also
	m_pVehicle->GeneratePhysExclusionList(exclusionList, nExclusions, MAX_INDEPENDENT_VEHICLE_ENTITY_LIST, nTestTypes, ArchetypeFlags::GTA_OBJECT_TYPE);

	WorldProbe::CShapeTestFixedResults<> probeResult;
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetResultsStructure(&probeResult);
	probeDesc.SetStartAndEnd(vStartPosition, vExitPosition);
	probeDesc.SetIncludeFlags(nTestTypes);
	probeDesc.SetExcludeEntities(exclusionList, nExclusions, excludeEntityOptions);
	probeDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
	probeDesc.SetIsDirected(true);
	WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);

	if (!probeResult[0].GetHitDetected())
	{
#if DEBUG_DRAW
		CTask::ms_debugDraw.AddLine(RCC_VEC3V(vStartPosition), RCC_VEC3V(vExitPosition), Color_green, 2500, 0);
#endif // DEBUG_DRAW
		return true;
	}

#if DEBUG_DRAW
		CTask::ms_debugDraw.AddLine(RCC_VEC3V(vStartPosition), RCC_VEC3V(probeResult[0].GetHitPosition()), Color_red, 2500, 0);
#endif // DEBUG_DRAW

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::CheckIsRouteValidForFlee(const CTaskMoveFollowNavMesh* pNavTask) const
{
	TUNE_GROUP_FLOAT(FLEE_TUNE, MIN_FLEE_ROUTE_LENGTH_TO_JUST_COWER, 10.0f, 0.0f, 30.0f, 1.0f);
	if (pNavTask->GetTotalRouteLength() < MIN_FLEE_ROUTE_LENGTH_TO_JUST_COWER)
	{
		// The route is quite short, probably a dead end, lets stay safe in our vehicle (hopefully)
		return false;
	}

	if (pNavTask->GetRouteSize() > 1)
	{
		const CPed& rPed = *GetPed();
		const CTaskSmartFlee* pFleeTask = static_cast<const CTaskSmartFlee*>(GetParent());
		const CAITarget& rFleeTarget = pFleeTask->GetFleeTarget();
		Vector3 vFleeTargetPosition;
		if (rFleeTarget.GetIsValid() && rFleeTarget.GetPosition(vFleeTargetPosition))
		{
			const Vector3 vPedPosition = VEC3V_TO_VECTOR3(rPed.GetTransform().GetPosition());
			// If the target is quite far away, allow us to take the route, even if it may initially lead us to the threat
			const float fDistFromTargetSqd = (vFleeTargetPosition - vPedPosition).Mag2();
			TUNE_GROUP_FLOAT(FLEE_TUNE, PED_TO_TARGET_FLEE_ANY_DIR_DIST, 20.f, 0.0f, 50.0f, 0.1f);
			if (fDistFromTargetSqd > square(PED_TO_TARGET_FLEE_ANY_DIR_DIST))
			{
				return true;
			}

			const CNavMeshRoute& rRoute = *pNavTask->GetRoute();
			const Vector3 vFirstNodePosition = rRoute.GetPosition(0);
			Vector3 vToFirstNode = vFirstNodePosition - vPedPosition;
			vToFirstNode.Normalize();
			Vector3 vToFleeTarget = vFleeTargetPosition - vPedPosition;
			vToFleeTarget.Normalize();

			// Try not to run directly at the flee target initially
			TUNE_GROUP_FLOAT(FLEE_TUNE, MIN_TO_TARGET_FIRST_NODE_DOT, 0.9f, 0.0f, 1.0f, 0.001f);
			if (vToFirstNode.Dot(vToFleeTarget) > MIN_TO_TARGET_FIRST_NODE_DOT)
			{
				return false;
			}

			// See if the second node is fairly close to us and also check the direction between the first and second nodes
			// we don't want to turn into the flee target either
			TUNE_GROUP_FLOAT(FLEE_TUNE, MAX_DIST_TO_CHECK_SECOND_NOT_DIRECTION, 10.0f, 0.0f, 20.0f, 0.1f);
			const Vector3 vSecondNodePosition = rRoute.GetPosition(1);
			const float fDistPedToSecondNodeSqd = (vSecondNodePosition - vPedPosition).Mag2();
			if (fDistPedToSecondNodeSqd < square(MAX_DIST_TO_CHECK_SECOND_NOT_DIRECTION))
			{
				Vector3 vToSecondNode = vSecondNodePosition - vToFirstNode;
				vToSecondNode.Normalize();

				TUNE_GROUP_FLOAT(FLEE_TUNE, MIN_TO_TARGET_SECOND_NODE_DOT, 0.707f, 0.0f, 1.0f, 0.001f);
				if (vToFirstNode.Dot(vToFleeTarget) > MIN_TO_TARGET_FIRST_NODE_DOT)
				{
					return false;
				}
			}

			return true;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::CheckForInAirExit(const Vector3& vTestPos) const
{
	return CheckForInAirExit(vTestPos, CTaskExitVehicleSeat::ms_Tunables.m_InAirProbeDistance);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::CheckForInAirExit(const Vector3& vTestPos, float fProbeLength) const
{
	if (GetPed()->IsNetworkClone())
		return false;

	if (m_bIsRappel)
		return false;

	return IsVerticalProbeClear(vTestPos, fProbeLength, *GetPed());
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::CheckForUnderwater(const Vector3& vTestPos)
{
	float fWaterHeightOut = 0.0f;
	if(GetPed()->m_Buoyancy.GetWaterLevelIncludingRivers(vTestPos, &fWaterHeightOut, true, POOL_DEPTH, 999999.9f, NULL))
	{
		if(fWaterHeightOut > vTestPos.z)
		{
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::CheckForVehicleInWater() const
{
	if (m_pVehicle->InheritsFromBoat())
	{
		if (static_cast<const CBoat*>(m_pVehicle.Get())->m_BoatHandling.IsInWater())
		{
			return true;
		}
	}
	else if (m_pVehicle->InheritsFromAmphibiousAutomobile())
	{
		if (static_cast<const CAmphibiousAutomobile*>(m_pVehicle.Get())->GetBoatHandling()->IsInWater())
		{
			return true;
		}
	}
	// Need to do slightly different water detection on the SEASPARROW
	else if (m_pVehicle->pHandling && m_pVehicle->pHandling->GetSeaPlaneHandlingData() && (m_pVehicle->GetIsInWater() || (m_pVehicle->InheritsFromHeli() && m_pVehicle->m_nFlags.bPossiblyTouchesWater)))
	{
		return true;
	}
	else if (m_pVehicle->m_nVehicleFlags.bIsDrowning)
	{
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CTaskExitVehicle::SelectRappelClipsetForHeliSeat(CVehicle* pVehicle, s32 iSeatIndex)
{
	fwMvClipSetId descendClipSet = CLIP_SET_ID_INVALID;

	bool bIsAnnihilator = false;
	if(pVehicle)
	{
		bIsAnnihilator = (MI_HELI_POLICE_2.IsValid() && pVehicle->GetModelIndex() == MI_HELI_POLICE_2) || (MI_HELI_ANNIHILATOR2.IsValid() && pVehicle && pVehicle->GetModelIndex() == MI_HELI_ANNIHILATOR2);
	}

	switch (iSeatIndex)
	{
	case(2):
		descendClipSet = CLIP_SET_VEH_HELI_REAR_DESCEND_DS;
		break;
	case(3):
		descendClipSet = CLIP_SET_VEH_HELI_REAR_DESCEND_PS;
		break;
	case(4):
		descendClipSet = bIsAnnihilator ? CLIP_SET_VEH_HELI_ANNIHILATOR2_DESCEND_RRDS : CLIP_SET_VEH_HELI_REAR_DESCEND_DS;
		break;
	case(5):
		descendClipSet = bIsAnnihilator ? CLIP_SET_VEH_HELI_ANNIHILATOR2_DESCEND_RRPS : CLIP_SET_VEH_HELI_REAR_DESCEND_PS;
		break;
	}

	if (descendClipSet == CLIP_SET_ID_INVALID)
	{
		descendClipSet = (iSeatIndex % 2 == 0) ? CLIP_SET_VEH_HELI_REAR_DESCEND_DS : CLIP_SET_VEH_HELI_REAR_DESCEND_PS;
	}

	return descendClipSet;
}

bool CTaskExitVehicle::CheckForClimbDown() const
{
	return CTaskVehicleFSM::ShouldUseClimbUpAndClimbDown(*m_pVehicle, m_iTargetEntryPoint);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicle::UpdateAttachOffsetsFromCurrentPosition(CPed& ped)
{
	Mat34V vehMtx(V_IDENTITY);
	if (ped.GetGlobalAttachMatrix(vehMtx))
	{
		Mat34V pedMtx = ped.GetTransform().GetMatrix();
		Mat34V pedOffsetMtx(V_IDENTITY);
		UnTransformFull(pedOffsetMtx, vehMtx, pedMtx);
		QuatV qOffsetRot = QuatVFromMat34V(pedOffsetMtx);
		Vec3V vOffsetPos = pedOffsetMtx.GetCol3();
		ped.SetAttachOffset(RCC_VECTOR3(vOffsetPos));
		ped.SetAttachQuat(RCC_QUATERNION(qOffsetRot));
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicle::SetClipPlaybackRate()
{
	const s32 iState = GetState();
	float fClipRate = 1.0f;
	const CAnimRateSet& rAnimRateSet = m_pVehicle->GetLayoutInfo()->GetAnimRateSet();

	const CPed& rPed = *GetPed();
	const bool bShouldUseCombatExitRates = IsFlagSet(CVehicleEnterExitFlags::CombatEntry) || IsFlagSet(CVehicleEnterExitFlags::UseFastClipRate) || rPed.GetPedResetFlag(CPED_RESET_FLAG_UseFastEnterExitVehicleRates);

	// Set the clip rate based on our conditions
	if (bShouldUseCombatExitRates)
	{
		fClipRate = rAnimRateSet.m_NoAnimCombatExit.GetRate();
	}
	else
	{
		fClipRate = rAnimRateSet.m_NormalExit.GetRate();
	}

	bool bIsPlaneWithStairs = m_pVehicle->InheritsFromPlane() && (m_pVehicle->GetLayoutInfo()->GetClimbUpAfterOpenDoor() || IsPlaneHatchEntry(m_iTargetEntryPoint));

	if (bIsPlaneWithStairs)
		fClipRate *= CTaskEnterVehicleSeat::GetMultipleEntryAnimClipRateModifier(m_pVehicle, &rPed, true);

	// Update the move parameters
	if (iState== State_ClimbDown)
	{
		m_moveNetworkHelper.SetFloat(ms_ClimbDownRateId, fClipRate);
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::SetClipsForState()
{
	const s32 iState = GetState();
	const fwMvClipSetId clipSetId = GetMoveNetworkClipSet();

	fwMvClipId clipId = CLIP_ID_INVALID; 
	fwMvClipId moveClipId = CLIP_ID_INVALID;

	switch (iState)
	{
		case State_ClimbDown:	
		{
			bool bDoorIntact = true;
			if (m_pVehicle->GetLayoutInfo()->GetClimbUpAfterOpenDoor() || IsPlaneHatchEntry(m_iTargetEntryPoint))
			{
				const CEntryExitPoint* pEntryPoint = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint);
				if (pEntryPoint)
				{
					CCarDoor* pDoor = m_pVehicle->GetDoorFromBoneIndex(pEntryPoint->GetDoorBoneIndex());
					if (!pDoor || !pDoor->GetIsIntact(m_pVehicle))
					{
						bDoorIntact = false;
					}
				}
			}

			const CVehicleEntryPointInfo* pEntryInfo = m_pVehicle->GetEntryInfo(m_iTargetEntryPoint);
			bool bHasClimbDownToWater = pEntryInfo && pEntryInfo->GetHasClimbDownToWater();

			bool bInWater = bHasClimbDownToWater && m_pVehicle->GetIsInWater();
			if (bInWater)
			{
				clipId = bDoorIntact ? ms_Tunables.m_DefaultClimbDownWaterClipId : ms_Tunables.m_DefaultClimbDownWaterNoDoorClipId;
			}
			else
			{
				clipId = bDoorIntact ? ms_Tunables.m_DefaultClimbDownClipId : ms_Tunables.m_DefaultClimbDownNoDoorClipId;
			}

			moveClipId = ms_ClimbDownClipId; 
			break;
		}
		default: break;
	}

	const crClip* pClip = fwClipSetManager::GetClip(clipSetId, clipId);
	if (!taskVerifyf(pClip, "Couldn't find clip %s from clipset %s", clipId.GetCStr(), clipSetId.GetCStr()))
	{
		return false;
	}

	if (iState == State_ClimbDown)
	{
		// We cache these in the on enter event rather than computing each frame as it's relatively expensive to call
		m_vCachedClipTranslation = fwAnimHelpers::GetMoverTrackTranslationDiff(*pClip, 0.0f, 1.0f);

		// Ignore the initial mover rotation if there is any
		Quaternion qRotStart = fwAnimHelpers::GetMoverTrackRotation(*pClip, 0.0f);	
		qRotStart.UnTransform(m_vCachedClipTranslation);
		m_qCachedClipRotation = fwAnimHelpers::GetMoverTrackRotationDiff(*pClip, 0.0f, 1.0f);

		m_fLegIkBlendInPhase = -1.0f;
		float fUnused;
		CClipEventTags::GetMoveEventStartAndEndPhases(pClip, CTaskVehicleFSM::ms_LegIkBlendInId, m_fLegIkBlendInPhase, fUnused);
	}

	m_moveNetworkHelper.SetClip(pClip, moveClipId);
	return true;
}

////////////////////////////////////////////////////////////////////////////////

const crClip* CTaskExitVehicle::GetClipAndPhaseForState(float& fPhase) const
{
	if (m_moveNetworkHelper.IsNetworkActive())
	{
		switch (GetState())
		{
			case State_ClimbDown:
				{
					fPhase = m_moveNetworkHelper.GetFloat(ms_ClimbDownAnimPhaseId);
					fPhase = rage::Clamp(fPhase, 0.0f, 1.0f);
					return m_moveNetworkHelper.GetClip(ms_ClimbDownClipId);
				}
			break;
			case State_ExitSeatToIdle:
				{
					fPhase = m_moveNetworkHelper.GetFloat(ms_ToIdleAnimPhaseId);
					fPhase = rage::Clamp(fPhase, 0.0f, 1.0f);
					return m_moveNetworkHelper.GetClip(ms_ToIdleClipId);
				}
			default: taskAssertf(0, "Unhandled State");
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicle::StoreInitialOffsets()
{
	// Store the offsets to the seat situation so we can recalculate the starting situation each frame relative to the seat
	Vector3 vTargetPosition(Vector3::ZeroType);
	Quaternion qTargetOrientation(0.0f,0.0f,0.0f,1.0f);

	if (ShouldUseGetOutInitialOffset())
	{
		ComputeGetOutInitialOffset(vTargetPosition, qTargetOrientation);
	}

	m_PlayAttachedClipHelper.SetTarget(vTargetPosition, qTargetOrientation);
	m_PlayAttachedClipHelper.SetInitialOffsets(*GetPed());
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::ShouldUseGetOutInitialOffset() const
{
	return GetState() == State_ClimbDown;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicle::ComputeGetOutInitialOffset(Vector3& vOffset, Quaternion& qOffset)
{
	// We start the climb down at the get out point
	CTaskVehicleFSM::ComputeTargetTransformInWorldSpace(*m_pVehicle, vOffset, qOffset, m_iTargetEntryPoint, CExtraVehiclePoint::GET_OUT);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicle::ComputeClimbDownTarget(Vector3& vOffset, Quaternion& qOffset)
{
	// We start the climb down at the get in point
	ComputeGetOutInitialOffset(vOffset, qOffset);

	// Make the clip offset relative to the initial offset rotation
	Vector3 vClipOffset = m_vCachedClipTranslation;
	qOffset.Transform(vClipOffset);

	// Add to get in point to get the final world space climb down target (sans ground fixup)
	vOffset += vClipOffset;

	// Add the clip rotation to the initial get in rotation to get the final orientation (sans orientation fixup)
	qOffset.Multiply(m_qCachedClipRotation);

	// Remove roll/pitch from the orientation, we like the ped capsule to be upright upon switching to locomotion
	const float fHeading = qOffset.TwistAngle(ZAXIS);
	qOffset.FromRotation(ZAXIS, fHeading);

	if (m_fCachedGroundHeight != -1.0f)
	{
		vOffset.z = m_fCachedGroundHeight;
	}
	else
	{
		Vector3 vGroundPos(Vector3::ZeroType);
		if (CTaskVehicleFSM::FindGroundPos(m_pVehicle, GetPed(), vOffset, CTaskExitVehicleSeat::ms_Tunables.m_GroundFixupHeight, vGroundPos, false))
		{
			// Adjust the reference position
			m_fCachedGroundHeight = vGroundPos.z;
			vOffset.z = m_fCachedGroundHeight;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::DoesProbeHitSomething(const Vector3& vStart, const Vector3& vEnd)
{
	WorldProbe::CShapeTestFixedResults<> probeResult;
	s32 iTypeFlags = ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE;
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetResultsStructure(&probeResult);
	probeDesc.SetStartAndEnd(vStart, vEnd);
	probeDesc.SetIncludeFlags(iTypeFlags);
	probeDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
	probeDesc.SetIsDirected(true);
	WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);

	if (probeResult[0].GetHitDetected())
	{
#if DEBUG_DRAW
		CTask::ms_debugDraw.AddLine(RCC_VEC3V(vStart), RCC_VEC3V(probeResult[0].GetHitPosition()), Color_red, 2500, 0);
#endif // DEBUG_DRAW
		return true;
	}

#if DEBUG_DRAW
	CTask::ms_debugDraw.AddLine(RCC_VEC3V(vStart), RCC_VEC3V(vEnd), Color_green, 2500, 0);
#endif // DEBUG_DRAW

	return false;
}

////////////////////////////////////////////////////////////////////////////////

s32	CTaskExitVehicle::GetDirectEntryPointForPed() const
{
	const CVehicle& rVeh = *m_pVehicle;	// Could this crash?
	const CPed& rPed = *GetPed();
	const s32 iSeatIndex = rVeh.GetSeatManager()->GetPedsSeatIndex(&rPed);
	if (!taskVerifyf(rVeh.IsSeatIndexValid(iSeatIndex), "Ped wasn't in a valid seat"))
	{
		return -1;
	}

	const s32 iEntryIndex = rVeh.GetDirectEntryPointIndexForSeat(iSeatIndex);
	if (!taskVerifyf(rVeh.IsEntryIndexValid(iEntryIndex), "Ped trying to flee doesn't have a valid entry point") && taskVerifyf(GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_SMART_FLEE, "Exit task not being run as a child of smart flee"))
	{
		return -1;
	}

	return iEntryIndex;
}

////////////////////////////////////////////////////////////////////////////////

Vector3 CTaskExitVehicle::ComputeExitPosition(const Vector3& vEntryPosition, const float fLength, const float fAngle, const bool bRunToRear, const Vector3& vSide)
{
	Vector3 vExitPos = Vector3(0.0f, fLength, 0.0f);
	vExitPos.RotateZ(fAngle);	
	vExitPos += vEntryPosition;

	if (bRunToRear)
	{
		vExitPos += vSide * ms_Tunables.m_RearExitSideOffset;
	}
	return vExitPos;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::CheckForTransitionToRagdollState(CPed& rPed, CVehicle& rVeh)
{
	if (!CTaskNMBehaviour::CanUseRagdoll(&rPed, RAGDOLL_TRIGGER_IMPACT_CAR, &rVeh))
		return false;

	if (rPed.GetIsAttached())
		return false;

	if (rPed.IsNetworkClone())
		return false;

	if (rPed.IsLocalPlayer())
		return false;

	if (rPed.GetPedResetFlag(CPED_RESET_FLAG_BlockRagdollActivationInVehicle))
		return false;

	if (rPed.GetPedResetFlag(CPED_RESET_FLAG_IsRappelling))
		return false;

	const CClipEventTags::CBlockRagdollTag* pProp = static_cast<const CClipEventTags::CBlockRagdollTag*>(m_moveNetworkHelper.GetNetworkPlayer()->GetProperty(CClipEventTags::BlockRagdoll));
	if (pProp)
	{
		if (pProp->ShouldBlockFromThisVehicleMoving())
			return false;
	}

#if __DEV
	TUNE_GROUP_BOOL(EXIT_TUNE, FORCE_RAGDOLL_WHEN_GETTING_OUT, false);
	if (FORCE_RAGDOLL_WHEN_GETTING_OUT)
		return true;
#endif // __DEV

	if (NetworkInterface::IsGameInProgress())
		return false;

	if (rVeh.IsUpsideDown())
		return true;

	const float fVelocitySquared = rVeh.GetVelocity().Mag2();
	if (fVelocitySquared < rage::square(ms_Tunables.m_MinVelocityToRagdollPed))
		return false;

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::ShouldRagdollDueToReversing(CPed& rPed, CVehicle& rVeh)
{
	if (!CTaskNMBehaviour::CanUseRagdoll(&rPed, RAGDOLL_TRIGGER_VEHICLE_GRAB, &rVeh))
		return false;

	if (rPed.GetIsAttached())
		return false;

	if (rPed.IsNetworkClone())
		return false;

	if (rPed.GetPedResetFlag(CPED_RESET_FLAG_BlockRagdollActivationInVehicle))
		return false;

	const s32 iEntryPointIndex = GetTargetEntryPoint();
	if (!rVeh.IsEntryIndexValid(iEntryPointIndex))
		return false;

	const CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(&rVeh, iEntryPointIndex);
	if (!pDoor || !pDoor->GetIsIntact(&rVeh))
		return false;

	if (rVeh.GetIsAircraft())
	{
		if (rVeh.IsInAir())
			return false;

		// B*2333219: Don't ragdoll if exiting an aircraft with a plane hatch/climb up.
		const CVehicleEntryPointInfo* pEntryInfo = rVeh.GetEntryInfo(iEntryPointIndex);
		bool bBlockRagdollDueToExitAnim = (pEntryInfo && pEntryInfo->GetIsPlaneHatchEntry()) || (rVeh.GetLayoutInfo() && rVeh.GetLayoutInfo()->GetClimbUpAfterOpenDoor());
		if (bBlockRagdollDueToExitAnim)
			return false;
	}

	const Vector3 vVelocity = rVeh.GetVelocity();
	const float fVelocitySquared = vVelocity.XYMag2();
	if (vVelocity.Dot(VEC3V_TO_VECTOR3(m_pVehicle->GetVehicleForwardDirection())) >= 0.0f)
		return false;

	TUNE_GROUP_FLOAT(EXIT_TUNE, MIN_VELOCITY_TO_RAGDOLL_WHEN_REVERSING, 0.5f, 0.0f, 2.0f, 0.01f);
	if (fVelocitySquared < rage::square(MIN_VELOCITY_TO_RAGDOLL_WHEN_REVERSING))
		return false;

	AI_LOG_WITH_ARGS("Ped %s is ragdolling because vehicle is reversing too fast and it has it's door intact\n", AILogging::GetDynamicEntityNameSafe(&rPed));
	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::CheckForObstructions(const CEntity* pEntity, const Vector3& vStart, const Vector3& vEnd)
{
	s32 iTypeFlags = ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE;
	WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
	capsuleDesc.SetExcludeEntity(pEntity);
	capsuleDesc.SetCapsule(vStart, vEnd, 0.25f);
	capsuleDesc.SetIncludeFlags(iTypeFlags);
	capsuleDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
	capsuleDesc.SetIsDirected(true);
	capsuleDesc.SetDoInitialSphereCheck(true);
	return WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::PedShouldWaitForPedsToMove()
{
	CPed* pPed = GetPed();

	if (IsFlagSet(CVehicleEnterExitFlags::WaitForLeader))
	{
		const Vec3V vPedPosition = pPed->GetTransform().GetPosition();
		CPedGroup* pPedsGroup = pPed->GetPedsGroup();
		if (pPedsGroup)
		{
			CPed* pLeader = pPedsGroup->GetGroupMembership()->GetLeader();
			if (pLeader && pLeader != pPed )
			{
				return IsPedConsideredTooClose(vPedPosition, *pLeader);
			}
		}
	}
	else if (IsFlagSet(CVehicleEnterExitFlags::WaitForEntryToBeClearOfPeds))
	{
		const Vec3V vPedPosition = pPed->GetTransform().GetPosition();
		CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
		for (CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext())
		{
			CPed* pOtherPed = static_cast<CPed*>(pEnt);
			if (IsPedConsideredTooClose(vPedPosition, *pOtherPed))
			{
				return true;
			}
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::IsPedConsideredTooClose(Vec3V_ConstRef vPedPos, const CPed& rOtherPed)
{
	if (rOtherPed.GetAttachState() == ATTACH_STATE_PED_IN_CAR)
		return false;

	// If the ped is still close to us, we should wait to prevent getting out and intersecting them
	Vec3V vLeaderPosition = rOtherPed.GetTransform().GetPosition();
	ScalarV scDistSq = DistSquared(vLeaderPosition, vPedPos);
	ScalarV scDistanceFromLeaderExitVehicleSq = ScalarVFromF32(square(ms_Tunables.m_LeaderExitVehicleDistance));
	if (IsLessThanAll(scDistSq, scDistanceFromLeaderExitVehicleSq))
	{
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::ShouldWaitForCarToSettle() const
{
	//Ensure the flag is set.
	if(!IsRunningFlagSet(CVehicleEnterExitFlags::WaitForCarToSettle))
	{
		return false;
	}

	//Ensure the vehicle is not a bike.
	if(m_pVehicle->InheritsFromBike())
	{
		return false;
	}

	//Ensure the vehicle is not a boat.
	if(m_pVehicle->InheritsFromBoat())
	{
		return false;
	}

	// Never wait to settle if the vehicle is upright
	if(CVehicle::GetVehicleOrientation(*m_pVehicle) == CVehicle::VO_Upright)
	{
		return false;
	}

	//Check if we aren't touching geometry.
	bool bIsTouchingGeometry = (m_pVehicle->GetFrameCollisionHistory()->GetFirstBuildingCollisionRecord() != NULL);
	if(m_pVehicle->m_Buoyancy.GetStatus() == FULLY_IN_WATER)
	{
		return false;
	}
	else if((m_pVehicle->m_Buoyancy.GetStatus() == PARTIALLY_IN_WATER) && !bIsTouchingGeometry)
	{
		return false;
	}

	//Check if the vehicle is in the air, or partially in water.
	if(m_pVehicle->IsInAir())
	{
		//Check if the vehicle is an aircraft.
		if(m_pVehicle->GetIsAircraft())
		{
			return false;
		}

		if(!bIsTouchingGeometry)
		{
			//Check if we are far enough off the ground.
			static dev_float s_fProbeLength = 15.0f;
			if(CheckForInAirExit(VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().GetPosition()), s_fProbeLength))
			{
				return false;
			}
			else
			{
				return true;
			}
		}
	}

	// If we're not upright and are trying to move the vehicle, then force a wait for settle intially
	const CPed& rPed = *GetPed();
	if(rPed.IsLocalPlayer() && GetTimeInState() < 0.1f)
	{
		float fStickX = 0.0f;
		float fStickY = 0.0f;
		const CControl* pControl = rPed.GetControlFromPlayer();
		CAutomobile::GetVehicleMovementStickInput(*pControl, fStickX, fStickY);
		TUNE_GROUP_FLOAT(VEHICLE_SETTLE_TUNE, MIN_INPUT_THRESHOLD, 0.1f, 0.0f, 1.0f, 0.01f);
		if (Abs(fStickX) > MIN_INPUT_THRESHOLD || Abs(fStickY) > MIN_INPUT_THRESHOLD)
		{
			return true;
		}
	}

	//Check if we were waiting for the car to settle.
	bool bWereWaitingForCarToSettle = (m_uLastTimeUnsettled != 0);

	//Choose the max angular velocity.
	static dev_float s_fMaxAngVelocityInitial	= 0.5f;
	static dev_float s_fMaxAngVelocityContinued	= 0.05f;
	float fMaxAngVelocity = !bWereWaitingForCarToSettle ?
		s_fMaxAngVelocityInitial : s_fMaxAngVelocityContinued;

	//Check if the angular velocity is invalid.
	const Vector3& vAngVelocity = m_pVehicle->GetAngVelocity();
	if(Abs(vAngVelocity.x) > fMaxAngVelocity)
	{
		return true;
	}
	else if(Abs(vAngVelocity.y) > fMaxAngVelocity)
	{
		return true;
	}

	//Check if the vehicle is not upright.
	bool bIsUpright = (!m_pVehicle->IsUpsideDown() && !m_pVehicle->IsOnItsSide());
	if(!bIsUpright)
	{
		//Check if we have not come to a stop.
		float fSpeedSq = m_pVehicle->GetVelocity().Mag2();
		static dev_float s_fMaxSpeed = 0.25f;
		float fMaxSpeedSq = square(s_fMaxSpeed);
		if(fSpeedSq > fMaxSpeedSq)
		{
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::GetWillClosedoor()
{
	CPed* pPed = GetPed();
	if (!GetWillUseDoor(pPed))
		return false;

	if (m_bWantsToGoIntoCover)
		return false;

	if (pPed->IsInjured())
		return false;

	if (m_pVehicle->GetIsInWater())
		return false;

	if (m_iTargetEntryPoint == -1)
		return true;

	if (IsFlagSet(CVehicleEnterExitFlags::DontCloseDoor))
		return false;

	const CVehicleEntryPointAnimInfo* pEntryAnimInfo = m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint);
	if (pEntryAnimInfo && pEntryAnimInfo->GetDontCloseDoorOutside())
	{
		return false;
	}

	// We don't interrupt if we need to close the door because we're pushing towards the front
	// Only do this for the player, script controlled peds decide to use the door based on the 
	// flags passed into the task
	if (pPed->IsLocalPlayer())
	{
		s32 iFlags = CTaskVehicleFSM::EF_OnlyAllowInterruptIfTryingToSprint;
		const CEntryExitPoint* pEntryPoint = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint);
		s32 seatIndex = pEntryPoint ? pEntryPoint->GetSeat(SA_directAccessSeat) : -1;
		const CVehicleSeatInfo* pSeatInfo = m_pVehicle->IsSeatIndexValid(seatIndex) ? m_pVehicle->GetSeatInfo(seatIndex) : NULL;
		bool bIsHeliRearSeat = m_pVehicle->InheritsFromHeli() && pSeatInfo && !pSeatInfo->GetIsFrontSeat();
#if FPS_MODE_SUPPORTED
		if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && !bIsHeliRearSeat)
		{
			iFlags = 0;
		}
#endif // FPS_MODE_SUPPORTED
		if (CTaskVehicleFSM::CheckForPlayerExitInterrupt(*pPed, *m_pVehicle, m_iRunningFlags, m_vSeatOffset, iFlags))
		{
			return false;
		}
	}

	const CEntryExitPoint* pEntryPoint = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint);

	CCarDoor* pDoor = m_pVehicle->GetDoorFromBoneIndex(pEntryPoint->GetDoorBoneIndex());

	if (IsFlagSet(CVehicleEnterExitFlags::JumpOut) ||
		IsFlagSet(CVehicleEnterExitFlags::ForcedExit) ||
		IsFlagSet(CVehicleEnterExitFlags::ThroughWindscreen) ||
		IsFlagSet(CVehicleEnterExitFlags::BeJacked) ||
		m_bWarping ||
		pDoor==NULL || !pDoor->GetIsIntact(m_pVehicle) ||
		CUpsideDownCarCheck::IsCarUpsideDown(static_cast<CVehicle*>(m_pVehicle)))
	{
		return false;
	}

	// Don't close door during exit if script are forcing it open
	if (pDoor && (pDoor->GetFlag(CCarDoor::DRIVEN_NORESET) || pDoor->GetFlag(CCarDoor::DRIVEN_NORESET_NETWORK)))
	{
		return false;
	}

	if( ( MI_CAR_APC.IsValid() &&
		  m_pVehicle->GetModelId() == MI_CAR_APC ) ||
		( MI_CAR_SCARAB.IsValid() &&
		  m_pVehicle->GetModelId() == MI_CAR_SCARAB ) ||
		(MI_CAR_SCARAB2.IsValid() &&
			m_pVehicle->GetModelId() == MI_CAR_SCARAB2) ||
		(MI_CAR_SCARAB3.IsValid() &&
			m_pVehicle->GetModelId() == MI_CAR_SCARAB3))
	{
		pDoor->SetTargetDoorOpenRatio( 0.0f, CCarDoor::DRIVEN_AUTORESET|CCarDoor::WILL_LOCK_DRIVEN );
		return false;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

float CTaskExitVehicle::GetBlendInDuration() const
{
	if (IsFlagSet(CVehicleEnterExitFlags::BeJacked))
	{
		return ms_Tunables.m_BeJackedBlendInDuration;
	}
	else if(IsFlagSet(CVehicleEnterExitFlags::ThroughWindscreen))
	{
		return ms_Tunables.m_ThroughWindScreenBlendInDuration;
	}
	return ms_Tunables.m_ExitVehicleBlendInDuration;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::IsConsideredGettingOutOfVehicleForCamera(float minExitSeatPhaseForCameraExit) const
{
	bool bIsExitingVehicleForCamera = false;

	const s32 iState = GetState();

	if (minExitSeatPhaseForCameraExit >= 0.0f
		FPS_MODE_SUPPORTED_ONLY( && (!camInterface::GetGameplayDirector().IsFirstPersonModeEnabled() || 
									 !camInterface::GetGameplayDirector().IsFirstPersonModeAllowed()) ))
	{
		if (iState == CTaskExitVehicle::State_ExitSeat)
		{
			const CTaskExitVehicleSeat* pExitVehicleSeatTask = static_cast<const CTaskExitVehicleSeat*>(GetSubTask());
			if(pExitVehicleSeatTask)
			{
				float fCurrentPhase = 0.0f;
				const crClip* pClip = pExitVehicleSeatTask->GetClipAndPhaseForState(fCurrentPhase);
				bIsExitingVehicleForCamera = (pClip && (fCurrentPhase >= minExitSeatPhaseForCameraExit));
			}
		}
		else
		{
			bIsExitingVehicleForCamera = (((iState > CTaskExitVehicle::State_ExitSeat) &&
				(iState <= CTaskExitVehicle::State_ExitSeatToIdle)) || (iState == State_ClimbDown));
		}
	}
	else
	{
		const CVehicleModelInfo* pVehicleModelInfo = m_pVehicle ? m_pVehicle->GetVehicleModelInfo() : NULL;
		const bool bShouldTransitionOnClimbUpDown = (pVehicleModelInfo && pVehicleModelInfo->ShouldCameraTransitionOnClimbUpDown())
			FPS_MODE_SUPPORTED_ONLY( && (!camInterface::GetGameplayDirector().IsFirstPersonModeEnabled() || !camInterface::GetGameplayDirector().IsFirstPersonModeAllowed()) );

		// NOTE: We don't report that the ped is exiting a vehicle until they have picked a door, as this is required for exit alignment.
		// - Please add any state exclusions here.
		bIsExitingVehicleForCamera = bShouldTransitionOnClimbUpDown ? (iState == State_ClimbDown) : (((iState > CTaskExitVehicle::State_PickDoor) &&
			(iState <= CTaskExitVehicle::State_ExitSeatToIdle)) || (iState == State_ClimbDown));
	}

	return bIsExitingVehicleForCamera;
}

////////////////////////////////////////////////////////////////////////////////

Vec3V_Out CTaskExitVehicle::GetExitPosition() const
{
	taskAssert(m_bHasValidExitPosition);
	return m_vExitPosition;
}

////////////////////////////////////////////////////////////////////////////////

CCarDoor * CTaskExitVehicle::GetDoor()
{
	const s32 iEntryPoint = GetDirectEntryPointForPed();
	if (m_pVehicle->IsEntryIndexValid(iEntryPoint))
	{
		return CTaskVehicleFSM::GetDoorForEntryPointIndex(m_pVehicle, iEntryPoint);
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::IsDoorOpen(float fMinRatio) const
{
	const s32 iEntryPoint = GetDirectEntryPointForPed();
	if (m_pVehicle->IsEntryIndexValid(iEntryPoint))
	{
		//Ensure the door is valid.
		CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(m_pVehicle, iEntryPoint);
		if(!pDoor)
		{
			return false;
		}

		//Ensure the door is open.
		if(pDoor->GetDoorRatio() < fMinRatio)
		{
			return false;
		}

		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicle::OnExitMidAir(CPed* pPed, CVehicle* pVehicle)
{
	//Ensure the ped is valid.
	if(!pPed)
	{
		return;
	}
	
	//Ensure the vehicle is valid.
	if(!pVehicle)
	{
		return;
	}
	
	//Note that the ped is not standing.
	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_IsStanding, false );
	
	//Heli exit animations clip with the heli after detach, they need to be blocked or the ped will ragdoll.
	if(pVehicle->InheritsFromHeli() || pVehicle->InheritsFromPlane())
	{
		pPed->SetNoCollision(pVehicle, NO_COLLISION_RESET_WHEN_NO_IMPACTS);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicle::TriggerFallEvent(CPed *pPed, s32 nFlags)
{
	CTaskComplexControlMovement * pCtrlTask;
	CTaskFall *pFallTask = rage_new CTaskFall(nFlags);

	pCtrlTask = rage_new CTaskComplexControlMovement( rage_new CTaskMoveInAir(), 
		pFallTask, 
		CTaskComplexControlMovement::TerminateOnSubtask );

	s32 iEventPriority = E_PRIORITY_IN_AIR + 1;
	CEventGivePedTask event(PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP,pCtrlTask,false,iEventPriority);
	pPed->GetPedIntelligence()->AddEvent(event);
	pPed->SetUseExtractedZ(false);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicle::IsPlaneHatchEntry(const s32 entryIndex) const
{
	const CVehicleEntryPointInfo* pEntryInfo = m_pVehicle->IsEntryIndexValid(entryIndex) ? m_pVehicle->GetEntryInfo(entryIndex) : NULL;
	return (pEntryInfo && pEntryInfo->GetIsPlaneHatchEntry());
}

void CTaskExitVehicle::ProcessForcedSkyDiveExitWarp(CVehicle& rVeh, s32 iEntryPoint, u32& iFlags)
{
	const CVehicleEntryPointInfo* pEntryInfo = rVeh.GetEntryInfo(iEntryPoint);
	if (!pEntryInfo || !pEntryInfo->GetForceSkyDiveExitInAir() || !NetworkInterface::IsGameInProgress())
		return;

	iFlags |= FF_DisableSkydiveTransition;
	iFlags |= FF_ForceSkyDive;
	
	// Uses Heist4SkyDiveTransition for the Alkonost rear warp
	ANIMPOSTFXMGR.Start(atHashString("Heist4SkyDiveTransition", 0xEAD6839B), 0, false, false, false, 0, AnimPostFXManager::kCameraFlash);

	// Set the camera to be positioned above the player
	camInterface::GetGameplayDirector().SetShouldPerformCustomTransitionAfterWarpFromVehicle();

	// Automatically snap open the rear door (e.g. Plane)
	CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(&rVeh, iEntryPoint);
	if (pDoor && pDoor->GetHierarchyId() == VEH_BOOT && pDoor->GetIsClosed())
	{
		pDoor->SetTargetDoorOpenRatio(1.0f, CCarDoor::PROCESS_FORCE_AWAKE | CCarDoor::SET_TO_FORCE_OPEN, &rVeh);
	}
}


////////////////////////////////////////////////////////////////////////////////

u32 CTaskExitVehicle::GetAttachToVehicleFlags(const CVehicle *pVehicle)
{
	s32 uFlags = ATTACH_STATE_PED_EXIT_CAR | ATTACH_FLAG_COL_ON;
	// B*2293241: Disable collision when exiting a boat.
	if (pVehicle && pVehicle->InheritsFromBoat() && !pVehicle->GetIsJetSki())
	{
		uFlags &= ~ATTACH_FLAG_COL_ON;
	}
	return uFlags;
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL

void CTaskExitVehicle::Debug() const
{
#if __BANK
	// Base Class
	CTask::Debug();

	CPed* pFocusPed = NULL;

	CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);
	if (pFocusEntity && pFocusEntity->GetIsTypePed())
	{
		pFocusPed = static_cast<CPed*>(pFocusEntity);
	}

	if (!pFocusPed)
	{
		pFocusPed = CGameWorld::FindLocalPlayer();
	}

	if (GetPed() == pFocusPed)
	{
		if (pFocusPed->IsLocalPlayer() && m_pVehicle && m_pVehicle->IsSeatIndexValid(m_iSeat))
		{
			CControl* pControl = pFocusPed->GetControlFromPlayer();
			Vector2 vStick(pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm());
			float fStickHeading = rage::Atan2f(-vStick.x, vStick.y) + camInterface::GetHeading();
			float fCarHeading = m_pVehicle->GetTransform().GetHeading();

			Vec3V vPedPos = pFocusPed->GetTransform().GetPosition();
			Vec3V vRenderStartPosition = vPedPos;
			vRenderStartPosition.SetZf(vRenderStartPosition.GetZf() + 1.0f);

			Vec3V vSeatGlobalPos = Transform(m_pVehicle->GetTransform().GetMatrix(), m_vSeatOffset);
			vSeatGlobalPos.SetZf(vRenderStartPosition.GetZf());
			grcDebugDraw::Arrow(vRenderStartPosition, vSeatGlobalPos, 0.25f, Color_yellow);

			s32 iInterruptFlags = GetState() == State_CloseDoor ? CTaskVehicleFSM::EF_AnyStickInputInterrupts : CTaskVehicleFSM::EF_TryingToSprintAlwaysInterrupts | CTaskVehicleFSM::EF_IgnoreDoorTest;
			const bool bIsPullingAway = CTaskVehicleFSM::CheckForPlayerPullingAwayFromVehicle(*pFocusPed, *m_pVehicle, m_vSeatOffset, iInterruptFlags);

			Vec3V vStickDirection = RotateAboutZAxis(Vec3V(V_Y_AXIS_WONE), ScalarVFromF32(fStickHeading));
			vStickDirection += vRenderStartPosition;
			grcDebugDraw::Arrow(vRenderStartPosition, vStickDirection, 0.25f, bIsPullingAway ? Color_green : Color_red);
			Vec3V vCarDirectionEndPosition(0.0f, 1.0f, 0.0f);
			vCarDirectionEndPosition = RotateAboutZAxis(vCarDirectionEndPosition, ScalarVFromF32(fCarHeading));
			vCarDirectionEndPosition += vRenderStartPosition;
			grcDebugDraw::Arrow(vRenderStartPosition, vCarDirectionEndPosition, 0.25f, Color_blue);
		}

		grcDebugDraw::AddDebugOutput(Color_green, "Target Exitpoint : %u", m_iTargetEntryPoint);
		if (pFocusPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
		{
			grcDebugDraw::AddDebugOutput(Color_green, "Ped In Seat");
		}
		else
		{
			grcDebugDraw::AddDebugOutput(Color_red, "Ped Not In Seat");
		}
	}
#endif
}
#endif // !__FINAL

////////////////////////////////////////////////////////////////////////////////

CClonedExitVehicleInfo::CClonedExitVehicleInfo()
: m_pPedJacker(NULL)
{
}

CClonedExitVehicleInfo::CClonedExitVehicleInfo(s32 enterState, 
											   CVehicle* pVehicle, 
											   SeatRequestType iSeatRequestType, 
											   s32 iSeat, 
											   s32 iTargetEntryPoint, 
											   VehicleEnterExitFlags iRunningFlags, 
											   CPed* pPedJacker, 
											   unsigned exitSeatMotionState,
											   const fwMvClipSetId &streamedClipSetId,
											   const fwMvClipId & streamedClipId,
                                               bool hasClosedDoor,
                                               bool isRappel,
											   bool subTaskExitRateSet,
											   bool shouldUseCombatExitRate,
											   Vector3 vFleeExitTargetNodePosition)
: CClonedVehicleFSMInfo(enterState, pVehicle, iSeatRequestType, iSeat, iTargetEntryPoint, iRunningFlags)
, m_pPedJacker(pPedJacker)
, m_ExitSeatMotionState(static_cast<u16>(exitSeatMotionState))
, m_StreamedExitClipSetId(streamedClipSetId)
, m_StreamedExitClipId(streamedClipId)
, m_HasClosedDoor(hasClosedDoor)
, m_IsRappel(isRappel)
, m_SubTaskExitRateSet(subTaskExitRateSet)
, m_ShouldUseCombatExitRate(shouldUseCombatExitRate)
, m_FleeExitTargetPosition(vFleeExitTargetNodePosition)
{

}

CTaskFSMClone * CClonedExitVehicleInfo::CreateCloneFSMTask()
{
	CTaskExitVehicle* pExitVehicleTask = rage_new CTaskExitVehicle(static_cast<CVehicle*>(GetTarget()), GetFlags(), 0.0f, GetPedsJacker(), m_iTargetEntryPoint);
	pExitVehicleTask->SetStreamedExitClipsetId(m_StreamedExitClipSetId);
	pExitVehicleTask->SetStreamedExitClipId(m_StreamedExitClipId);
    pExitVehicleTask->SetIsRappel(m_IsRappel);
	return pExitVehicleTask;
}

const fwMvBooleanId CTaskOpenVehicleDoorFromInside::ms_OpenDoorClipFinishedId("OpenDoorAnimFinished",0xD81E9C8C);
const fwMvClipId CTaskOpenVehicleDoorFromInside::ms_OpenDoorClipId("OpenDoorClip",0x213832E8);
const fwMvFloatId CTaskOpenVehicleDoorFromInside::ms_OpenDoorPhaseId("OpenDoorPhase",0x1B98437A);

CTaskOpenVehicleDoorFromInside::CTaskOpenVehicleDoorFromInside(CVehicle* pVehicle, s32 iExitPointIndex)
: m_pVehicle(pVehicle)
, m_iExitPointIndex(iExitPointIndex)
{
	SetInternalTaskType(CTaskTypes::TASK_OPEN_VEHICLE_DOOR_FROM_INSIDE);
}

CTaskOpenVehicleDoorFromInside::~CTaskOpenVehicleDoorFromInside()
{

}

void CTaskOpenVehicleDoorFromInside::CleanUp()
{
}

CTask::FSM_Return CTaskOpenVehicleDoorFromInside::ProcessPreFSM()
{
	if (!taskVerifyf(m_pVehicle, "NULL vehicle pointer in CTaskOpenVehicleDoorFromInside::ProcessPreFSM"))
	{
		return FSM_Quit;
	}

	if (!taskVerifyf(GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_EXIT_VEHICLE, "Invalid parent task"))
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}	

CTask::FSM_Return CTaskOpenVehicleDoorFromInside::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();	

		FSM_State(State_OpenDoor)
			FSM_OnEnter
				OpenDoor_OnEnter();
			FSM_OnUpdate
				return OpenDoor_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskOpenVehicleDoorFromInside::ProcessPostFSM()
{	
	return FSM_Continue;
}

CTask::FSM_Return CTaskOpenVehicleDoorFromInside::Start_OnUpdate()
{
	if (!taskVerifyf(m_iExitPointIndex != -1 && m_pVehicle->GetEntryExitPoint(m_iExitPointIndex), "Invalid entry point index"))
	{
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	m_pParentNetworkHelper = static_cast<CTaskExitVehicle*>(GetParent())->GetMoveNetworkHelper();

	if (!m_pParentNetworkHelper)
	{
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	SetTaskState(State_OpenDoor);
	return FSM_Continue;
}

void CTaskOpenVehicleDoorFromInside::OpenDoor_OnEnter()
{

}

CTask::FSM_Return CTaskOpenVehicleDoorFromInside::OpenDoor_OnUpdate()
{
	if (m_pParentNetworkHelper->GetBoolean( ms_OpenDoorClipFinishedId))
	{
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	const crClip* pOpenDoorClip = m_pParentNetworkHelper->GetClip( ms_OpenDoorClipId);
	float fOpenDoorPhase = m_pParentNetworkHelper->GetFloat( ms_OpenDoorPhaseId);

	if (pOpenDoorClip)
	{
		if (fOpenDoorPhase < 0.0f)
		{
			fOpenDoorPhase = 0.0f;	// Clip not started yet?
		}

		DriveOpenDoorFromClip(pOpenDoorClip, fOpenDoorPhase);
	}

	return FSM_Continue;
}

void CTaskOpenVehicleDoorFromInside::DriveOpenDoorFromClip(const crClip* pOpenDoorClip, const float fOpenDoorPhase)
{
	taskFatalAssertf(pOpenDoorClip, "NULL clip in CTaskOpenVehicleDoorFromInside::DriveOpenDoorFromClip");

	// Look up bone index of target entry point
	s32 iBoneIndex = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromEntryPoint(m_iExitPointIndex);

	if (iBoneIndex > -1)
	{
		CCarDoor* pDoor = m_pVehicle->GetDoorFromBoneIndex(iBoneIndex);
		if (pDoor)
		{
			float fPhaseToBeginOpenDoor = 0.0f;
			float fPhaseToEndOpenDoor = 1.0f;

			taskVerifyf(CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pOpenDoorClip, CClipEventTags::Door, CClipEventTags::Start, true, fPhaseToBeginOpenDoor), "Clip %s has no door start flag", pOpenDoorClip->GetName());
			taskVerifyf(CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pOpenDoorClip, CClipEventTags::Door, CClipEventTags::Start, false, fPhaseToEndOpenDoor), "Clip %s has no door end flag", pOpenDoorClip->GetName());

			if (fOpenDoorPhase >= fPhaseToEndOpenDoor)
			{
				pDoor->SetTargetDoorOpenRatio(1.0f, CCarDoor::DRIVEN_AUTORESET|CCarDoor::DRIVEN_SPECIAL);
			}
			else if (fOpenDoorPhase >= fPhaseToBeginOpenDoor)
			{
				float fRatio = rage::Min(1.0f, (fOpenDoorPhase - fPhaseToBeginOpenDoor) / (fPhaseToEndOpenDoor - fPhaseToBeginOpenDoor));
				fRatio = MAX(pDoor->GetDoorRatio(), fRatio);
				pDoor->SetTargetDoorOpenRatio(fRatio, CCarDoor::DRIVEN_AUTORESET|CCarDoor::DRIVEN_SPECIAL);
			}
		}
	}
}

#if !__FINAL
const char * CTaskOpenVehicleDoorFromInside::GetStaticStateName( s32 iState )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
	case State_Start:							return "State_Start";
	case State_OpenDoor:						return "State_OpenDoor";
	case State_Finish:							return "State_Finish";
	default: taskAssert(0); 	
	}

	return "State_Invalid";
}

void CTaskOpenVehicleDoorFromInside::Debug() const
{
#if __BANK
	if (m_pVehicle)
	{
		// Look up bone index of target entry point
		s32 iBoneIndex = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromEntryPoint(m_iExitPointIndex);

		if (iBoneIndex > -1)
		{
			CCarDoor* pDoor = m_pVehicle->GetDoorFromBoneIndex(iBoneIndex);
			if (pDoor)
			{
				grcDebugDraw::AddDebugOutput(Color_green, "Door Ratio : %.4f", pDoor->GetDoorRatio());
				grcDebugDraw::AddDebugOutput(Color_green, "Target Door Ratio : %.4f", pDoor->GetTargetDoorRatio());
			}
		}
	}
#endif
}
#endif // !__FINAL

////////////////////////////////////////////////////////////////////////////////

const fwMvBooleanId CTaskExitVehicleSeat::ms_ExitSeatOnEnterId("ExitSeat_OnEnter",0xA9B360BF);
const fwMvBooleanId CTaskExitVehicleSeat::ms_FleeExitOnEnterId("FleeExit_OnEnter",0x96C9AB4C);
const fwMvBooleanId CTaskExitVehicleSeat::ms_BeJackedOnEnterId("BeJacked_OnEnter",0x4E50A5D7);
const fwMvBooleanId CTaskExitVehicleSeat::ms_StreamedExitOnEnterId("StreamedExit_OnEnter",0xF4FB46A1);
const fwMvBooleanId CTaskExitVehicleSeat::ms_StreamedBeJackedOnEnterId("StreamedBeJacked_OnEnter",0x4EE99DF8);
const fwMvBooleanId CTaskExitVehicleSeat::ms_JumpOutOnEnterId("JumpOut_OnEnter",0xDEEE4739);
const fwMvBooleanId CTaskExitVehicleSeat::ms_ThroughWindscreenOnEnterId("ThroughWindscreen_OnEnter",0xC84B7E70);
const fwMvBooleanId CTaskExitVehicleSeat::ms_UpsideDownExitOnEnterId("UpsideDownExit_OnEnter",0xBAEFF4A0);
const fwMvBooleanId CTaskExitVehicleSeat::ms_OnSideExitOnEnterId("OnSideExit_OnEnter",0x49FD793A);
const fwMvBooleanId CTaskExitVehicleSeat::ms_BeJackedClipFinishedId("BeJackedAnimFinished",0x2D96549C);
const fwMvBooleanId CTaskExitVehicleSeat::ms_StreamedExitClipFinishedId("StreamedExitAnimFinished",0x8B59C97B);
const fwMvBooleanId CTaskExitVehicleSeat::ms_StreamedBeJackedClipFinishedId("StreamedBeJackedAnimFinished",0x56659C18);
const fwMvBooleanId CTaskExitVehicleSeat::ms_StandUpInterruptId("StandUpInterrupt",0xA297AEA9);
const fwMvBooleanId CTaskExitVehicleSeat::ms_NoWingEarlyRagdollId("NoWingEarlyRagdoll", 0x154f7a32);
const fwMvBooleanId CTaskExitVehicleSeat::ms_DetachId("Detach",0xFFC528E4);
const fwMvBooleanId CTaskExitVehicleSeat::ms_AimIntroInterruptId("AimIntroInterrupt",0xCFCAC4B6);
const fwMvBooleanId CTaskExitVehicleSeat::ms_ExitSeatClipFinishedId("ExitSeatAnimFinished",0x7AA0BA73);
const fwMvBooleanId CTaskExitVehicleSeat::ms_FleeExitAnimFinishedId("FleeExitAnimFinished",0xC87B4EED);
const fwMvBooleanId CTaskExitVehicleSeat::ms_JumpOutClipFinishedId("JumpOutAnimFinished",0xA8507574);
const fwMvBooleanId CTaskExitVehicleSeat::ms_ThroughWindscreenClipFinishedId("ThroughWindscreenAnimFinished",0x7C59EF80);
const fwMvBooleanId CTaskExitVehicleSeat::ms_InterruptId("Interrupt",0x6D6BC7B7);
const fwMvBooleanId CTaskExitVehicleSeat::ms_ExitSeatInterruptId("ExitSeatInterrupt",0xA589D06B);
const fwMvBooleanId CTaskExitVehicleSeat::ms_LegIkActiveId("LegIkActive",0x98D8531C);
const fwMvBooleanId CTaskExitVehicleSeat::ms_BlendInUpperBodyAimId("BlendInUpperBodyAim",0xC69950E0);
const fwMvBooleanId CTaskExitVehicleSeat::ms_ArrestedId("Arrested",0x9BD4FA0F);
const fwMvBooleanId CTaskExitVehicleSeat::ms_PreventBreakoutId("PreventBreakout",0xA7D81D37);
const fwMvBooleanId CTaskExitVehicleSeat::ms_ExitToSwimInterruptId("ExitToSwimInterrupt",0x1C18C69B);
const fwMvBooleanId CTaskExitVehicleSeat::ms_ExitToSkyDiveInterruptId("ExitToSkyDiveInterrupt",0xbc1d1cdd);
const fwMvBooleanId CTaskExitVehicleSeat::ms_BlendBackToSeatPositionId("BlendBackToSeatPosition",0x9BDEBC1D);
const fwMvBooleanId CTaskExitVehicleSeat::ms_AllowedToAbortForInAirEventId("AllowedToAbortForInAirEvent",0xEDCBA859);
const fwMvClipId CTaskExitVehicleSeat::ms_StreamedExitClipId("StreamedExitClip",0x41EEBA47);
const fwMvClipId CTaskExitVehicleSeat::ms_ExitSeatClipId("ExitSeatClip",0x8A967A93);
const fwMvClipId CTaskExitVehicleSeat::ms_FleeExitClipId("FleeExitClip",0xA4ED7D);
const fwMvClipId CTaskExitVehicleSeat::ms_JumpOutClipId("JumpOutClip",0xB730231B);
const fwMvClipId CTaskExitVehicleSeat::ms_ThroughWindscreenClipId("ThroughWindscreenClip",0xA8292891);
const fwMvClipId CTaskExitVehicleSeat::ms_StreamedBeJackedClipId("StreamedBeJackedClip",0xB10D3D1B);
const fwMvClipId CTaskExitVehicleSeat::ms_ExitToAimLeft180ClipId("Left180Clip",0xB7691843);
const fwMvClipId CTaskExitVehicleSeat::ms_ExitToAimLeft90ClipId("Left90Clip",0x51894B6);
const fwMvClipId CTaskExitVehicleSeat::ms_ExitToAimFrontClipId("FrontClip",0xA7A424);
const fwMvClipId CTaskExitVehicleSeat::ms_ExitToAimRight90ClipId("Right90Clip",0x43B0D244);
const fwMvClipId CTaskExitVehicleSeat::ms_ExitToAimRight180ClipId("Right180Clip",0x181BE36);
const fwMvClipSetVarId CTaskExitVehicleSeat::ms_AdditionalAnimsClipSet("AimClipSet",0xF15A6053);
const fwMvFlagId CTaskExitVehicleSeat::ms_FromInsideId("FromInside",0xB75486AB);
const fwMvFlagId CTaskExitVehicleSeat::ms_IsDeadId("IsDead",0xE2625733);
const fwMvFlagId CTaskExitVehicleSeat::ms_WalkStartFlagId("WalkStart",0xFBE90A9E);
const fwMvFlagId CTaskExitVehicleSeat::ms_ToWaterFlagId("ToWater",0xADD7573);
const fwMvFlagId CTaskExitVehicleSeat::ms_ToAimFlagId("ToAim",0xAFF25888);
const fwMvFlagId CTaskExitVehicleSeat::ms_NoWingFlagId("NoWing",0xC1D51556);
const fwMvFloatId CTaskExitVehicleSeat::ms_ExitSeatPhaseId("ExitSeatPhase",0x2C535D85);
const fwMvFloatId CTaskExitVehicleSeat::ms_FleeExitPhaseId("FleeExitPhase",0x91040847);
const fwMvFloatId CTaskExitVehicleSeat::ms_JumpOutPhaseId("JumpOutPhase",0x5E679498);
const fwMvFloatId CTaskExitVehicleSeat::ms_StreamedExitPhaseId("StreamedExitPhase",0x2F0E77A0);
const fwMvFloatId CTaskExitVehicleSeat::ms_StreamedBeJackedPhaseId("StreamedBeJackedPhase",0x639A83AD);
const fwMvFloatId CTaskExitVehicleSeat::ms_ThroughWindscreenPhaseId("ThroughWindscreenPhase",0x932313C5);
const fwMvFloatId CTaskExitVehicleSeat::ms_ToLocomotionPhaseId("ToLocomotionPhase",0xBE126720);
const fwMvFloatId CTaskExitVehicleSeat::ms_ToLocomotionPhaseOutId("ToLocomotionPhaseOut",0x7B09CEC0);
const fwMvFloatId CTaskExitVehicleSeat::ms_AimBlendDirectionId("AimBlendDirection",0xF8DC25E3);
const fwMvFloatId CTaskExitVehicleSeat::ms_AimPitchId("AimPitch",0x40ABD71);
const fwMvFloatId CTaskExitVehicleSeat::ms_BeJackedRateId("BeJackedRate",0xAE87F37B);
const fwMvFloatId CTaskExitVehicleSeat::ms_ExitSeatRateId("ExitSeatRate",0xF5136572);
const fwMvFloatId CTaskExitVehicleSeat::ms_FleeExitRateId("FleeExitRate",0x53CC236B);
const fwMvFloatId CTaskExitVehicleSeat::ms_StreamedExitRateId("StreamedExitRate",0xd4f6ffb1);
const fwMvFloatId CTaskExitVehicleSeat::ms_ExitSeatBlendDurationId("ExitSeatBlendDuration",0x17330237);
const fwMvRequestId CTaskExitVehicleSeat::ms_AimRequest("Aim",0xB01E2F36);
const fwMvRequestId CTaskExitVehicleSeat::ms_ExitSeatRequestId("ExitSeat",0x1755187F);
const fwMvRequestId CTaskExitVehicleSeat::ms_FleeExitRequestId("FleeExit",0xFC9B6138);
const fwMvRequestId CTaskExitVehicleSeat::ms_ToLocomotionRequestId("ToLocomotion",0xBC9DEBE7);
const fwMvRequestId CTaskExitVehicleSeat::ms_JumpOutRequestId("JumpOut",0xF9114857);
const fwMvRequestId CTaskExitVehicleSeat::ms_ThroughWindscreenRequestId("ThroughWindscreen",0xE8C8ED);
const fwMvRequestId CTaskExitVehicleSeat::ms_BeJackedRequestId("BeJacked",0xE46E3FE1);
const fwMvRequestId CTaskExitVehicleSeat::ms_StreamedExitRequestId("StreamedExit",0x459C2D97);
const fwMvRequestId CTaskExitVehicleSeat::ms_StreamedBeJackedRequestId("StreamedBeJacked",0x8FB93444);
const fwMvRequestId CTaskExitVehicleSeat::ms_UpsideDownExitRequestId("UpsideDownExit",0xC8DBA8E);
const fwMvRequestId CTaskExitVehicleSeat::ms_OnSideExitRequestId("OnSideExit",0x5FB4ED3B);
const fwMvRequestId CTaskExitVehicleSeat::ms_BeJackedCustomClipRequestId("BeJackedCustomClip",0x08953d5c);
const fwMvStateId CTaskExitVehicleSeat::ms_ExitSeatStateId("Exit Seat",0xCD4F6564);
const fwMvStateId CTaskExitVehicleSeat::ms_FleeExitStateId("Flee Exit",0xBD303477);
const fwMvStateId CTaskExitVehicleSeat::ms_ToLocomotionStateId("ToLocomotion",0xBC9DEBE7);
const fwMvStateId CTaskExitVehicleSeat::ms_BeJackedStateId("Be Jacked",0x73855626);
const fwMvStateId CTaskExitVehicleSeat::ms_StreamedExitStateId("StreamedExit",0x459C2D97);
const fwMvStateId CTaskExitVehicleSeat::ms_StreamedBeJackedStateId("StreamedBeJacked",0x8FB93444);
const fwMvStateId CTaskExitVehicleSeat::ms_JumpOutStateId("Jump Out Of Seat",0xA3EDCF7);
const fwMvStateId CTaskExitVehicleSeat::ms_ThroughWindscreenStateId("ThroughWindscreen",0xE8C8ED);
const fwMvStateId CTaskExitVehicleSeat::ms_UpsideDownExitStateId("UpsideDownExit",0xC8DBA8E);
const fwMvStateId CTaskExitVehicleSeat::ms_OnSideExitStateId("OnSideExit",0x5FB4ED3B);
const fwMvClipId CTaskExitVehicleSeat::ms_Clip0Id("Clip0",0xf416c5e4);

bank_float CTaskExitVehicleSeat::ms_fDefaultMinWalkInterruptPhase = 0.53f;
bank_float CTaskExitVehicleSeat::ms_fDefaultMaxWalkInterruptPhase = 0.8f;
bank_float CTaskExitVehicleSeat::ms_fDefaultMinRunInterruptPhase = 0.53f;
bank_float CTaskExitVehicleSeat::ms_fDefaultMaxRunInterruptPhase = 0.8f;

const u32 CTaskExitVehicleSeat::ms_uNumExitToAimClips = 5;

////////////////////////////////////////////////////////////////////////////////

// Statics
CTaskExitVehicleSeat::Tunables CTaskExitVehicleSeat::ms_Tunables;

IMPLEMENT_VEHICLE_TASK_TUNABLES(CTaskExitVehicleSeat, 0xa9c341f5);

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicleSeat::sm_bCloneVehicleFleeExitFix = true;

void CTaskExitVehicleSeat::InitTunables()
{
	sm_bCloneVehicleFleeExitFix = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CLONE_VEHICLE_FLEE_EXIT_FIX", 0x650bf282), sm_bCloneVehicleFleeExitFix);
}

bool CTaskExitVehicleSeat::IsTaskValid(const CVehicle* pVehicle, s32 iExitPointIndex)
{
	if (!pVehicle || !pVehicle->IsEntryIndexValid(iExitPointIndex))
		return false;

	return true;
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CTaskExitVehicleSeat::GetExitClipsetId(const CVehicle* pVehicle, s32 iExitPointIndex, const VehicleEnterExitFlags &flags, const CPed* pPed)
{
	if (!IsTaskValid(pVehicle, iExitPointIndex))
		return CLIP_SET_ID_INVALID;

	const CVehicleEntryPointAnimInfo* pClipInfo = pVehicle->GetEntryAnimInfo(iExitPointIndex);
	taskFatalAssertf(pClipInfo, "NULL Entry clip Info for entry index %i", iExitPointIndex);

	if (pClipInfo->GetUseSeatClipSetAnims() && flags.BitSet().IsSet(CVehicleEnterExitFlags::BeJacked))
	{
		s32 iSeat = pVehicle->GetEntryExitPoint(iExitPointIndex)->GetSeat(SA_directAccessSeat);
		const CVehicleSeatAnimInfo *pSeatInfo = pVehicle->GetSeatAnimationInfo(iSeat);
		if(pSeatInfo)
		{
			return pSeatInfo->GetSeatClipSetId();
		}
	}
	
	if (pPed)
	{
#if __DEV
		TUNE_GROUP_BOOL(EXIT_VEHICLE_TUNE, TEST_FEMALE_ANIMS_ON_PLAYER, false);
		const bool bPassedPlayerTest = !pPed->IsPlayer() || TEST_FEMALE_ANIMS_ON_PLAYER;
#else
		const bool bPassedPlayerTest = !pPed->IsPlayer();
#endif // __DEV

		if (bPassedPlayerTest && !flags.BitSet().IsSet(CVehicleEnterExitFlags::BeJacked) && !pPed->ShouldBeDead() && !pPed->IsMale() && pClipInfo->GetAlternateEntryPointClipSet() != CLIP_SET_ID_INVALID)
		{
			return pClipInfo->GetAlternateEntryPointClipSet();
		}
	}

	bool bTryUseFirstPersonExitClipSet = false;
#if FPS_MODE_SUPPORTED
	if (pPed && pPed->IsLocalPlayer() && pVehicle->InheritsFromBicycle())
	{
		bool bUnused;
		bTryUseFirstPersonExitClipSet = camInterface::GetGameplayDirector().ComputeIsFirstPersonModeAllowed(pPed, true, bUnused);
	}
#endif // FPS_MODE_SUPPORTED
	return pClipInfo->GetExitPointClipSetId(bTryUseFirstPersonExitClipSet ? 1 : 0);
}

////////////////////////////////////////////////////////////////////////////////

CTaskExitVehicleSeat::CTaskExitVehicleSeat(CVehicle* pVehicle, s32 iExitPointIndex, VehicleEnterExitFlags iRunningFlags, bool bIsRappel)
: m_pVehicle(pVehicle)
, m_iExitPointIndex(iExitPointIndex)
, m_iSeatIndex(-1)
, m_bExitedSeat(false)
, m_iRunningFlags(iRunningFlags)
, m_pParentNetworkHelper(NULL)
, m_bIsRappel(bIsRappel)
, m_vIdealTargetPosCached(V_ZERO)
, m_qIdealTargetRotCached(V_IDENTITY)
, m_fInterruptPhase(-1.0f)
, m_fCachedGroundFixUpHeight(CTaskVehicleFSM::INVALID_GROUND_HEIGHT_FIXUP)
, m_bProcessPhysicsCalledThisFrame(false)
, m_bWantsToEnter(false)
, m_bMoveNetworkFinished(false)
, m_bWillBlendOutLeftHandleIk(false)
, m_bWillBlendOutRightHandleIk(false)
, m_bHasProcessedWeaponAttachPoint(false)
, m_bComputedIdealTargets(false)
, m_fStartRollOrientationFixupPhase(0.05f)
, m_fEndRollOrientationFixupPhase(0.15f)
, m_fStartTranslationFixupPhase(0.0f)
, m_fEndTranslationFixupPhase(0.6f)
, m_fStartOrientationFixupPhase(0.0f)
, m_fEndOrientationFixupPhase(0.8f)
, m_bGroundFixupApplied(false)
, m_bRagdollEventGiven(false)
, m_fFleeExitClipHeadingChange(0.0f)
, m_fFleeExitExtraZChange(0.0f)
, m_fExtraHeadingChange(0.0f)
, m_bHasAdjustedForRoute(false)
, m_fAimInterruptPhase(-1.0f)
, m_fUpperBodyAimPhase(-1.0f)
, m_fStartRotationFixUpPhase(1.0f)
, m_iDominantAimClip(0)
, m_fLegIkBlendInPhase(-1.0f)
, m_fExitSeatClipPlaybackRate(1.0f)
, m_fLastAimDirection(LARGE_FLOAT)
, m_bPreserveMatrixOnDetach(false)
, m_vLastUpsideDownPosition(Vector3::ZeroType)
, m_bHasIkAllowTags(false)
, m_bCanBreakoutOfArrest(true)
, m_bWasInWaterAtBeginningOfJumpOut(false)
, m_bCloneShouldUseCombatExitRate(false)
, m_bHasTurnedTurretDueToBeingJacked(false)
, m_bNoExitAnimFound(false)
{
	SetInternalTaskType(CTaskTypes::TASK_EXIT_VEHICLE_SEAT);
}

////////////////////////////////////////////////////////////////////////////////

CTaskExitVehicleSeat::~CTaskExitVehicleSeat()
{

}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicleSeat::CleanUp()
{
	//Process the weapon attach point.
	ProcessWeaponAttachPoint();

	CPed& rPed = *GetPed();
	rPed.SetUseExtractedZ(false);

	rPed.SetPedConfigFlag(CPED_CONFIG_FLAG_IsInTheAir, false);

	ResetRagdollOnCollision(*GetPed());

	rPed.GetIkManager().ClearFlag(PEDIK_LEGS_USE_ANIM_ALLOW_TAGS);
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicleSeat::ProcessPreFSM()
{
	if (!taskVerifyf(m_pVehicle, "NULL vehicle pointer in CTaskExitVehicleSeat::ProcessPreFSM"))
	{
		return FSM_Quit;
	}

	if (!taskVerifyf(GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_EXIT_VEHICLE, "Invalid parent task"))
	{
		return FSM_Quit;
	}

	if (GetState() > State_Start)
	{
		if (!m_pParentNetworkHelper)
		{
			return FSM_Quit;
		}

		if (!m_pParentNetworkHelper->IsNetworkActive())
		{
			return FSM_Quit;
		}
	}

	RequestProcessPhysics();

	m_qThisFramesOrientationDelta = Quaternion(Quaternion::IdentityType);

	//Check if we should process the weapon attach point.
	if(ShouldProcessWeaponAttachPoint())
	{
		//Process the weapon attach point.
		ProcessWeaponAttachPoint();
	}

	bool bExitingOntoVehicle = (m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::ExitSeatOntoVehicle));
	if(bExitingOntoVehicle)
	{
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_IsGoingToStandOnExitedVehicle, true);
	}

	return FSM_Continue;
}	

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicleSeat::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_StreamAssets)
			FSM_OnUpdate
				return StreamAssets_OnUpdate();

		FSM_State(State_ExitSeat)
			FSM_OnEnter
				ExitSeat_OnEnter();
			FSM_OnUpdate
				return ExitSeat_OnUpdate();
			FSM_OnExit
				return ExitSeat_OnExit();

		FSM_State(State_FleeExit)
			FSM_OnEnter
				return FleeExit_OnEnter();
			FSM_OnUpdate
				return FleeExit_OnUpdate();
			FSM_OnExit
				return FleeExit_OnExit();

		FSM_State(State_JumpOutOfSeat)
			FSM_OnEnter
				JumpOutOfSeat_OnEnter();
			FSM_OnUpdate
				return JumpOutOfSeat_OnUpdate();

		FSM_State(State_ExitToAim)
			FSM_OnEnter
				ExitToAim_OnEnter();
			FSM_OnUpdate
				return ExitToAim_OnUpdate();
			FSM_OnExit
				return ExitToAim_OnExit();

		FSM_State(State_ThroughWindscreen)
			FSM_OnEnter
				return ThroughWindscreen_OnEnter();
			FSM_OnUpdate
				return ThroughWindscreen_OnUpdate();

		FSM_State(State_DetachedFromVehicle)
			FSM_OnEnter
				DetachedFromVehicle_OnEnter();
			FSM_OnUpdate
				return DetachedFromVehicle_OnUpdate();

		FSM_State(State_WaitForStreamedBeJacked)
			FSM_OnUpdate
				return WaitForStreamedBeJacked_OnUpdateClone();

		FSM_State(State_WaitForStreamedExit)
			FSM_OnUpdate
				return WaitForStreamedExit_OnUpdateClone();

		FSM_State(State_BeJacked)
			FSM_OnEnter
				return BeJacked_OnEnter();
			FSM_OnUpdate
				return BeJacked_OnUpdate();

		FSM_State(State_StreamedExit)
			FSM_OnEnter
				StreamedExit_OnEnter();
			FSM_OnUpdate
				return StreamedExit_OnUpdate();

		FSM_State(State_StreamedBeJacked)
			FSM_OnEnter
				StreamedBeJacked_OnEnter();
			FSM_OnUpdate
				return StreamedBeJacked_OnUpdate();

		FSM_State(State_UpsideDownExit)
			FSM_OnEnter
				UpsideDownExit_OnEnter();
			FSM_OnUpdate
				return UpsideDownExit_OnUpdate();

		FSM_State(State_OnSideExit)
			FSM_OnEnter
				return OnSideExit_OnEnter();
			FSM_OnUpdate
				return OnSideExit_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return Finish_OnUpdate();

	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicleSeat::ProcessPostFSM()
{	
	CPed* pPed = GetPed();

	if (taskVerifyf(m_pParentNetworkHelper, "NULL Network Helper in CTaskExitVehicleSeat::ProcessPostFSM"))
	{
		const bool bFromInside = m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::JackedFromInside);
		m_pParentNetworkHelper->SetFlag( bFromInside, ms_FromInsideId);
		const bool bIsDead = pPed->IsInjured();
		m_pParentNetworkHelper->SetFlag( bIsDead, ms_IsDeadId);
	}

	if (m_pVehicle->IsEntryIndexValid(m_iExitPointIndex))
	{
		const CVehicleEntryPointAnimInfo* pClipInfo = m_pVehicle->GetEntryAnimInfo(m_iExitPointIndex);
		if (pClipInfo && pClipInfo->GetHasClimbDown())
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ApplyAnimatedVelocityWhilstAttached, true);
		}
	}

	if (m_bExitedSeat && !m_pVehicle->InheritsFromBoat() && !m_pVehicle->InheritsFromSubmarine() && 
		!m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::IsTransitionToSkyDive))
	{
		pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_AnimatedVelocity);
	}

	if(m_pVehicle->InheritsFromBoat())
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_ForceBuoyancyProcessingIfAsleep, true);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicleSeat::ProcessPhysics(float fTimeStep, int nTimeSlice)
{
	if (GetState() > State_Start && m_pParentNetworkHelper)
	{
		CPed& ped = *GetPed();

		if (GetState() == State_FleeExit && ped.GetIsAttached())
		{
			float fPhase = 0.0f;
			const crClip* pClip = GetClipAndPhaseForState(fPhase);

			// We increment the attach position/orientation mimicking the normal non-attached behaviour
			// but we still need to fixup some stuff
			float fAnimatedAngVelocity = ped.GetAnimatedAngularVelocity();
			if (fPhase > m_fStartRotationFixUpPhase)
			{
				if (Abs(m_fExtraHeadingChange) != 0.0f)
				{
					float fExtraHeadingThisFrame = Sign(m_fExtraHeadingChange) * fTimeStep * ms_Tunables.m_FleeExitExtraRotationSpeed;
					if (Abs(fExtraHeadingThisFrame) > Abs(m_fExtraHeadingChange))
					{
						fExtraHeadingThisFrame = m_fExtraHeadingChange;
					}
					m_fExtraHeadingChange -= fExtraHeadingThisFrame;
					fAnimatedAngVelocity += fExtraHeadingThisFrame / fTimeStep;
				}
			}
			ped.SetAnimatedAngularVelocity(fAnimatedAngVelocity);

			bool bDoingAttachFixup = false;
			Vector3 vAnimatedVelocity = ped.GetAnimatedVelocity();
			if (fPhase >= m_fStartTranslationFixupPhase)
			{
				if (Abs(m_fFleeExitExtraZChange) != 0.0f)
				{
					float fExtraTranslationThisFrame = Sign(m_fFleeExitExtraZChange) * fTimeStep * ms_Tunables.m_FleeExitExtraTranslationSpeed;
					
					if (Abs(fExtraTranslationThisFrame) > Abs(m_fFleeExitExtraZChange))
					{
						fExtraTranslationThisFrame = m_fFleeExitExtraZChange;
					}

					m_fFleeExitExtraZChange -= fExtraTranslationThisFrame;
					vAnimatedVelocity.z += fExtraTranslationThisFrame / fTimeStep;

					Vector3 vPedPosition = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
					Vector3 vGroundPos(Vector3::ZeroType);
					if (CTaskVehicleFSM::FindGroundPos(m_pVehicle, &ped, vPedPosition, 2.5f, vGroundPos, false))
					{
						// Here we calculate the initial situation, which in the case of exiting is the seat situation
						// Note we don't use the helper to do this calculation since we assume the ped starts exactly on the seat node
						Vector3 vNewPedPosition(Vector3::ZeroType);
						Quaternion qNewPedOrientation(Quaternion::IdentityType);
						CModelSeatInfo::CalculateSeatSituation(m_pVehicle, vNewPedPosition, qNewPedOrientation, m_iExitPointIndex);

						// Update the situation from the current phase in the clip
						m_PlayAttachedClipHelper.ComputeSituationFromClip(pClip, 0.0f, fPhase, vNewPedPosition, qNewPedOrientation);

						TUNE_GROUP_BOOL(FLEE_EXIT_TUNE, DO_TRANSLATION_MOVER_FIXUP, true);
						if (DO_TRANSLATION_MOVER_FIXUP)
						{
							// Compute the offsets we'll need to fix up
							Vector3 vOffsetPosition(Vector3::ZeroType);
							Quaternion qOffsetOrientation(Quaternion::IdentityType);
							m_PlayAttachedClipHelper.ComputeOffsets(pClip, fPhase, 1.0f, vNewPedPosition, qNewPedOrientation, vOffsetPosition, qOffsetOrientation);

							// Apply the fixup
							Quaternion qDummyRotation;
							m_PlayAttachedClipHelper.ApplyVehicleExitOffsets(vNewPedPosition, qDummyRotation, pClip, vOffsetPosition, qOffsetOrientation, fPhase);	
						}

						Vec3V vLocalPosition(V_ZERO);
						QuatV qLocalOrientation(V_IDENTITY);

						if (CTaskVehicleFSM::ComputeLocalTransformFromGlobal(ped, VECTOR3_TO_VEC3V(vNewPedPosition), QUATERNION_TO_QUATV(qNewPedOrientation), vLocalPosition, qLocalOrientation))
						{
							Vector3 vAttachOffset = ped.GetAttachOffset();
							vAttachOffset.z = vLocalPosition.GetZf();
							ped.SetAttachOffset(vAttachOffset);
							bDoingAttachFixup = true;
						}
					}
				}
			}
			ped.SetAnimatedVelocity(vAnimatedVelocity);

			// Update the attachment offsets from the previous frames animations (usually gets done after this in ped.cpp)
			if (!bDoingAttachFixup)
			{
				ped.ApplyAnimatedVelocityWhilstAttached(fTimeStep);
			}
			else
			{
				// Apply animated rotation only
				QuatV qRotAttachOffset = QUATERNION_TO_QUATV(ped.GetAttachQuat());
				QuatV qOrientationDelta = QuatVFromZAxisAngle(ScalarVFromF32(ped.GetAnimatedAngularVelocity() * fTimeStep));
				// Transform offsets to object space
				qRotAttachOffset = Normalize(Multiply(qRotAttachOffset, qOrientationDelta));
				// Re-apply offsets
				ped.SetAttachQuat(RCC_QUATERNION(qRotAttachOffset));
			}

			if (m_bMoveNetworkFinished)
			{
				// Are animated velocity should have now been updated from the anims
				Vec3V vAnimatedAngVelocity(0.0f, 0.0f, ped.GetAnimatedAngularVelocity());
				Vec3V vAnimatedVelocity = VECTOR3_TO_VEC3V(ped.GetAnimatedVelocity());
				vAnimatedVelocity = ped.GetTransform().Transform3x3(vAnimatedVelocity);
				ped.SetPedOutOfVehicle(CPed::PVF_ExcludeVehicleFromSafetyCheck |CPed::PVF_IgnoreSafetyPositionCheck |CPed::PVF_DontNullVelocity);
				// Reapply the animated velocity to preserve momentum during the detach transition
				ped.SetDesiredVelocity(VEC3V_TO_VECTOR3(vAnimatedVelocity));
				ped.SetVelocity(VEC3V_TO_VECTOR3(vAnimatedVelocity));
				ped.SetDesiredAngularVelocity(VEC3V_TO_VECTOR3(vAnimatedAngVelocity));
				ped.SetAngVelocity(VEC3V_TO_VECTOR3(vAnimatedAngVelocity));
			}
			return true;
		}
		else if (GetState() == State_ExitToAim && ped.GetIsAttached())
		{
			// Update the attachment offsets from the previous frames animations (usually gets done after this in ped.cpp)
			ped.ApplyAnimatedVelocityWhilstAttached(fTimeStep);

			// Only fixup once, in the last update
			if (m_bProcessPhysicsCalledThisFrame || CTask::GetNumProcessPhysicsSteps() == 1)
			{
				// Fix up to this target transform
				CTaskVehicleFSM::sClipBlendParams clipBlendParams;
				if (CTaskVehicleFSM::GetGenericClipBlendParams(clipBlendParams, *m_pParentNetworkHelper, ms_uNumExitToAimClips, true))
				{
					// Compute ideal target position (seat + complete animated transform)
					if (!m_bComputedIdealTargets)
					{
						m_bComputedIdealTargets = CTaskVehicleFSM::ComputeIdealEndTransformRelative(clipBlendParams, m_vIdealTargetPosCached, m_qIdealTargetRotCached);
					}

					if (m_bComputedIdealTargets)
					{
						Vec3V vTargetPos = m_vIdealTargetPosCached;
						QuatV qTargetRot = m_qIdealTargetRotCached;

						// Compute the attach world space transform
						Vec3V vAttachWorld(V_ZERO);
						QuatV qAttachWorld(V_IDENTITY);
						CTaskVehicleFSM::GetAttachWorldTransform(*m_pVehicle, m_iExitPointIndex, vAttachWorld, qAttachWorld);

						// Compute the ideal world space end position if we played the anim from the attach position entirely
						CTaskVehicleFSM::TransformRelativeOffsetsToWorldSpace(vAttachWorld, qAttachWorld, vTargetPos, qTargetRot);

						float fGroundFixupHeight = GetGroundFixupHeight();
						// Do a probe to find the ground and make the orientation upright at this point
						m_bGroundFixupApplied = CTaskVehicleFSM::ApplyGroundAndOrientationFixup(*m_pVehicle, ped, vTargetPos, qTargetRot, m_fCachedGroundFixUpHeight, fGroundFixupHeight);

#if DEBUG_DRAW
						// Debug render the target position
						CTaskAttachedVehicleAnimDebug::RenderMatrixFromVecQuat(vTargetPos, qTargetRot, 100, 0);
#endif

						// Apply da mover fixup
						if (CTaskVehicleFSM::GetGenericClipBlendParams(clipBlendParams, *m_pParentNetworkHelper, ms_uNumExitToAimClips, false))
						{
							float fHeadingDiff = SubtractAngleShorter(ped.GetDesiredHeading(), m_fExitToAimDesiredHeading);
							QuatV qDesiredAngChange = QuatVFromAxisAngle(Vec3V(V_Z_AXIS_WONE), ScalarVFromF32(fHeadingDiff));
							qTargetRot = Multiply(qDesiredAngChange, qTargetRot);
							CTaskVehicleFSM::ApplyMoverFixup(ped, *m_pVehicle, m_iExitPointIndex, clipBlendParams, fTimeStep, vTargetPos, qTargetRot, false);
							return true;
						}
					}
				}
			}

			m_bProcessPhysicsCalledThisFrame = true;
			return false;
		}

		if (m_pVehicle && ped.GetIsAttachedInCar())
		{
			// Here we calculate the initial situation, which in the case of exiting is the seat situation
			// Note we don't use the helper to do this calculation since we assume the ped starts exactly on the seat node
			Vector3 vNewPedPosition(Vector3::ZeroType);
			Quaternion qNewPedOrientation(Quaternion::IdentityType);
			const bool bIsMultiSeatBoat = ( (MI_BOAT_SPEEDER.IsValid() && m_pVehicle->GetModelId() == MI_BOAT_SPEEDER) || 
											(MI_BOAT_SPEEDER2.IsValid() && m_pVehicle->GetModelId() == MI_BOAT_SPEEDER2) ||
											(MI_BOAT_TORO.IsValid() && m_pVehicle->GetModelId() == MI_BOAT_TORO) ||
											(MI_BOAT_TORO2.IsValid() && m_pVehicle->GetModelId() == MI_BOAT_TORO2));
			const bool bUseTargetSeat = bIsMultiSeatBoat && GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_EXIT_VEHICLE;
			s32 iTargetSeat = -1;
			if (bUseTargetSeat)
			{
				const CSeatManager* pSeatManager = m_pVehicle->GetSeatManager();
				iTargetSeat = pSeatManager ? pSeatManager->GetPedsSeatIndex(&ped) : -1;
			}
			CModelSeatInfo::CalculateSeatSituation(m_pVehicle, vNewPedPosition, qNewPedOrientation, m_iExitPointIndex, iTargetSeat);

			float fClipPhase = 0.0f;
			const crClip* pClip = GetClipAndPhaseForState(fClipPhase);

			if (GetState() == State_OnSideExit)
			{
				Vector3 vTargetPosition(Vector3::ZeroType);
				Quaternion qTargetOrientation(Quaternion::IdentityType);
				CModelSeatInfo::CalculateEntrySituation(m_pVehicle, &ped, vTargetPosition, qTargetOrientation, m_iExitPointIndex);

				// Restore the orientation
				Vector3 vTempPosition(Vector3::ZeroType);
				Quaternion qTempOrientation(Quaternion::IdentityType);
				CModelSeatInfo::CalculateExitSituation(m_pVehicle, vTempPosition, qTempOrientation, m_iExitPointIndex, pClip);
				qTargetOrientation = qTempOrientation;

				TUNE_GROUP_BOOL(ON_SIDE_EXIT, DISABLE_GROUND_FIXUP, false);
				TUNE_GROUP_FLOAT(ON_SIDE_EXIT, ON_SIDE_Z_VALUE_TO_DO_GROUND_PROBE, 0.8f, -1.0f, 1.0f, 0.01f);
				if (!DISABLE_GROUND_FIXUP || (m_pVehicle->GetAngVelocity().Mag2() < 0.01f && m_pVehicle->GetTransform().GetC().GetZf() > ON_SIDE_Z_VALUE_TO_DO_GROUND_PROBE))
				{
					Vector3 vGroundPos(Vector3::ZeroType);
					if (CTaskVehicleFSM::FindGroundPos(m_pVehicle, GetPed(), vTargetPosition, GetGroundFixupHeight(), vGroundPos, false))
					{
						// Adjust the reference position
						vTargetPosition.z = vGroundPos.z;
						m_bGroundFixupApplied = true;
					}
					else 
					{
						m_bGroundFixupApplied = false;
					}
				}

				// Convert to new vector library and remove world roll / pitch
				Vec3V vTargetPos = VECTOR3_TO_VEC3V(vTargetPosition);
				QuatV qTargetRot = QUATERNION_TO_QUATV(qTargetOrientation);

				const CVehicleEntryPointInfo* pEntryInfo = m_pVehicle->GetEntryInfo(m_iExitPointIndex);
				const QuatV qWorldVehicleOrientation = m_pVehicle->GetTransform().GetOrientation();
				float fDesiredHeading = QuatVTwistAngle(qWorldVehicleOrientation, Vec3V(V_Z_AXIS_WONE)).Getf();
				// Super hack to ensure the second lerp goes the way we expect it to! 
				TUNE_GROUP_FLOAT(ON_SIDE_EXIT, ANGLE_CHANGE, 0.05f, 0.0f, 1.0f, 0.01f);
				const float fLessThanPi = PI - ANGLE_CHANGE;
				const float fExtraHeadingChange = pEntryInfo->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT ? fLessThanPi : -fLessThanPi;
				fDesiredHeading += fExtraHeadingChange;
				fDesiredHeading = fwAngle::LimitRadianAngle(fDesiredHeading);
				qTargetRot = QuatVFromZAxisAngle(ScalarVFromF32(fDesiredHeading));

				Vec3V vInitialWorldPos = VECTOR3_TO_VEC3V(vNewPedPosition);
				QuatV qInitialWorldRot = QUATERNION_TO_QUATV(qNewPedOrientation);

				const float fTransLerpRatio = rage::Clamp((fClipPhase - m_fStartTranslationFixupPhase) / (m_fEndTranslationFixupPhase - m_fStartTranslationFixupPhase), 0.0f, 1.0f);
				QuatV qNewWorldRot = qInitialWorldRot;
				QuatV qLerpFromTarget = qInitialWorldRot;

				// Fix up the roll (relative to the vehicle) separately, at the end, the ped will have the door on top of his head
				if (fClipPhase >= m_fStartRollOrientationFixupPhase)
				{
					const float fRotateAngle = pEntryInfo->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT ? -HALF_PI : HALF_PI;
					const ScalarV scRotateAngle = ScalarVFromF32(fRotateAngle);
					const QuatV qWorldVehicleOrientation = m_pVehicle->GetTransform().GetOrientation();
					Mat34V orientationMtx;
					Mat34VFromQuatV(orientationMtx, qWorldVehicleOrientation, Vec3V(V_ZERO));
					Mat34VRotateLocalY(orientationMtx, scRotateAngle);
					QuatV qRollTarget = QuatVFromMat34V(orientationMtx);
					qRollTarget = Normalize(qRollTarget);
					qRollTarget = PrepareSlerp(qInitialWorldRot, qRollTarget);
					const float fRollLerpRatio = rage::Clamp((fClipPhase - m_fStartRollOrientationFixupPhase) / (m_fEndRollOrientationFixupPhase - m_fStartRollOrientationFixupPhase), 0.0f, 1.0f);
					qLerpFromTarget = Nlerp(ScalarVFromF32(fRollLerpRatio), qInitialWorldRot, qRollTarget);
				}

				if (m_fStartOrientationFixupPhase < m_fStartRollOrientationFixupPhase)
				{
					m_fStartOrientationFixupPhase = m_fEndRollOrientationFixupPhase;
				}
				if (m_fStartOrientationFixupPhase < m_fEndRollOrientationFixupPhase)
				{
					m_fStartOrientationFixupPhase = m_fEndRollOrientationFixupPhase;
				}

				// Fix up the rest of the orientation
				qTargetRot = PrepareSlerp(qInitialWorldRot, qTargetRot);
				const float fRotLerpRatio = rage::Clamp((fClipPhase - m_fStartOrientationFixupPhase) / (m_fEndOrientationFixupPhase - m_fStartOrientationFixupPhase), 0.0f, 1.0f);
				qNewWorldRot = Nlerp(ScalarVFromF32(fRotLerpRatio), qLerpFromTarget, qTargetRot);
				Vec3V vNewWorldPos = Lerp(ScalarVFromF32(fTransLerpRatio), vInitialWorldPos, vTargetPos);
				CTaskVehicleFSM::UpdatePedVehicleAttachment(&ped, vNewWorldPos, qNewWorldRot);
			}
			else if (GetState() == State_UpsideDownExit)
			{
				// Update the target situation
				UpdateTarget();

				// Note the clip pointer will not exist the frame after starting the clip, but we still need to update the ped's mover
				if (pClip)	
				{
					// During the initial part of the upside down exit, fix the mover up so the skeleton is in the 'ideal' position
					// as this will be different depending on the ground / vehicle roof height
					TUNE_GROUP_FLOAT(UPSIDE_DOWN_EXIT_TUNE, MAX_Z_FIXUP_PHASE, 0.1f, 0.0f, 1.0f, 0.01f);
					if (fClipPhase < MAX_Z_FIXUP_PHASE)
					{	
						const float fZLerpRatio = Clamp(fClipPhase / MAX_Z_FIXUP_PHASE, 0.0f, 1.0f);
						Vector3 vGroundPos;
						Vector3 vDesiredPos = vNewPedPosition;
						if (CTaskVehicleFSM::FindGroundPos(m_pVehicle, GetPed(), vNewPedPosition, GetGroundFixupHeight(), vGroundPos, true, true))
						{
							vDesiredPos.z = vGroundPos.z;
						}
						TUNE_GROUP_FLOAT(UPSIDE_DOWN_EXIT_TUNE, Z_OFFSET_FROM_GROUND, 0.1f, -1.0f, 1.0f, 0.01f);
						vDesiredPos.z += Z_OFFSET_FROM_GROUND;
						vNewPedPosition.Lerp(fZLerpRatio, vDesiredPos);
						// Store the last effective position so we can resume the mover animation from this point 
						// TODO: should be stored relative to vehicle in case it moves
						m_vLastUpsideDownPosition = vNewPedPosition;

						// Let the rotation come through from the clip
						Vector3 vDummyPosition = vNewPedPosition;
						m_PlayAttachedClipHelper.ComputeSituationFromClip(pClip, 0.0f, fClipPhase, vDummyPosition, qNewPedOrientation);
					}
					else
					{
						vNewPedPosition = m_vLastUpsideDownPosition;

						// Update the situation from the current phase in the clip
						m_PlayAttachedClipHelper.ComputeSituationFromClip(pClip, 0.0f, fClipPhase, vNewPedPosition, qNewPedOrientation);
					
						// Remove any roll/pitch whilst maintaining the heading
						QuatV qWorldRot = QUATERNION_TO_QUATV(qNewPedOrientation);
						float fDesiredHeading = QuatVTwistAngle(qWorldRot, Vec3V(V_Z_AXIS_WONE)).Getf();
						aiDebugf1("fDesiredHeading = %.2f", fDesiredHeading);
						QuatV qUprightRot = QuatVFromZAxisAngle(ScalarVFromF32(fDesiredHeading));
						qUprightRot = PrepareSlerp(qWorldRot, qUprightRot);
						TUNE_GROUP_FLOAT(UPSIDE_DOWN_EXIT_TUNE, ORIENTATION_FIXUP_START_PHASE, 0.1f, 0.0f, 1.0f, 0.01f);
						TUNE_GROUP_FLOAT(UPSIDE_DOWN_EXIT_TUNE, ORIENTATION_FIXUP_END_PHASE, 0.2f, 0.0f, 1.0f, 0.01f);
						const float fOrientationFixupRatio = Clamp((fClipPhase - ORIENTATION_FIXUP_START_PHASE) / (ORIENTATION_FIXUP_END_PHASE - ORIENTATION_FIXUP_START_PHASE), 0.0f, 1.0f);
						aiDebugf1("Orientation Fixup Ratio = %.2f", fOrientationFixupRatio);
						if (fOrientationFixupRatio == 1.0f)
						{
							qNewPedOrientation = QUATV_TO_QUATERNION(qUprightRot);
						}
						else
						{
							qNewPedOrientation = QUATV_TO_QUATERNION(Nlerp(ScalarVFromF32(fOrientationFixupRatio), qWorldRot, qUprightRot));
						}
					}

					TUNE_GROUP_BOOL(UPSIDE_DOWN_EXIT_TUNE, DO_TRANSLATION_MOVER_FIXUP, true);
					if (DO_TRANSLATION_MOVER_FIXUP)
					{
						// Compute the offsets we'll need to fix up
						Vector3 vOffsetPosition(Vector3::ZeroType);
						Quaternion qOffsetOrientation(Quaternion::IdentityType);
						m_PlayAttachedClipHelper.ComputeOffsets(pClip, fClipPhase, 1.0f, vNewPedPosition, qNewPedOrientation, vOffsetPosition, qOffsetOrientation);

						// Apply the fixup
						Quaternion qDummyRotation;
						m_PlayAttachedClipHelper.ApplyVehicleExitOffsets(vNewPedPosition, qDummyRotation, pClip, vOffsetPosition, qOffsetOrientation, fClipPhase);	
					}

					qNewPedOrientation.Normalize();
					CTaskVehicleFSM::UpdatePedVehicleAttachment(GetPed(), VECTOR3_TO_VEC3V(vNewPedPosition), QUATERNION_TO_QUATV(qNewPedOrientation));
				}
			}
			else
			{
				// Update the target situation
				UpdateTarget();

				// Note the clip pointer will not exist the frame after starting the clip, but we still need to update the ped's mover
				if (pClip)	
				{
					// Update the situation from the current phase in the clip
					m_PlayAttachedClipHelper.ComputeSituationFromClip(pClip, 0.0f, fClipPhase, vNewPedPosition, qNewPedOrientation);

					bool bShouldApplyTargetFixup = ShouldApplyTargetFixup();

					// Only want to fix up the rotation when exiting turret
					// ...apart from the HALFTRACK. Because of extra collision for shoulder rests, we need to force the existing TECHNICAL animation into a different clear position.
					const bool bIsTurretExit = IsRunningFlagSet(CVehicleEnterExitFlags::ExitSeatOntoVehicle) && m_pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_TURRET_SEAT_ON_VEHICLE);
					const bool bIsHalfTrack = MI_CAR_HALFTRACK.IsValid() && m_pVehicle->GetModelIndex() == MI_CAR_HALFTRACK;
					if (bIsTurretExit && !bIsHalfTrack)
					{
						m_PlayAttachedClipHelper.SetTranslationalOffsetSetEnabled(false); 
					}

					// Only want to apply rotational offset for jumping out of a bike seat
					if (!bShouldApplyTargetFixup && GetState() == State_JumpOutOfSeat && (m_pVehicle->InheritsFromBike() || m_pVehicle->InheritsFromQuadBike() || m_pVehicle->InheritsFromAmphibiousQuadBike() || m_pVehicle->GetIsJetSki()))
					{
						bShouldApplyTargetFixup = true;
						m_PlayAttachedClipHelper.SetTranslationalOffsetSetEnabled(false);
					}

					TUNE_GROUP_BOOL(EXIT_VEHICLE_TUNE, DO_MOVER_FIXUP, true);
					if (bShouldApplyTargetFixup && DO_MOVER_FIXUP)
					{
						bool bCanApplyFixup = true;
						if ( (MI_BOAT_SPEEDER.IsValid() && m_pVehicle->GetModelId() == MI_BOAT_SPEEDER) ||
							 (MI_BOAT_SPEEDER2.IsValid() && m_pVehicle->GetModelId() == MI_BOAT_SPEEDER2) ||
							 (MI_BOAT_TORO.IsValid() && m_pVehicle->GetModelId() == MI_BOAT_TORO) ||
							 (MI_BOAT_TORO2.IsValid() && m_pVehicle->GetModelId() == MI_BOAT_TORO2))
						{
							bCanApplyFixup = false;
						}

						// B*1610417...
						else if (GetState() == State_BeJacked && m_pParentNetworkHelper && m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::JackedFromInside))
						{
							static const fwMvClipSetId truckClipSetId("clipset@veh@truck@ds@enter_exit");
							const fwMvClipSetId beJackedClipSetId = m_pParentNetworkHelper->GetClipSetId();
							fwClipSet* pClipset = fwClipSetManager::GetClipSet(beJackedClipSetId);
							const fwMvClipSetId fallBackId = pClipset ? pClipset->GetFallbackId() : CLIP_SET_ID_INVALID;
							if (beJackedClipSetId == truckClipSetId || fallBackId  == truckClipSetId)
							{
								bCanApplyFixup = false;
							}
						}

						if (bCanApplyFixup)
						{
							// Compute the offsets we'll need to fix up
							Vector3 vOffsetPosition(Vector3::ZeroType);
							Quaternion qOffsetOrientation(Quaternion::IdentityType);
							m_PlayAttachedClipHelper.ComputeOffsets(pClip, fClipPhase, 1.0f, vNewPedPosition, qNewPedOrientation, vOffsetPosition, qOffsetOrientation);

							// Apply the fixup
							m_PlayAttachedClipHelper.ApplyVehicleExitOffsets(vNewPedPosition, qNewPedOrientation, pClip, vOffsetPosition, qOffsetOrientation, fClipPhase);	

							TUNE_GROUP_BOOL(BIKE_TUNE, DO_EXTRA_Z_FIXUP, false);
							if (DO_EXTRA_Z_FIXUP && m_PlayAttachedClipHelper.GetTranslationalOffsetEnabled() && m_pVehicle->InheritsFromBike() && m_pVehicle->GetLayoutInfo()->GetBikeLeansUnlessMoving())
							{
								Vector3 vTargetPosition(Vector3::ZeroType);
								Quaternion qTargetOrientation(Quaternion::IdentityType);
								m_PlayAttachedClipHelper.GetTarget(vTargetPosition, qTargetOrientation);
								if (vNewPedPosition.z < vTargetPosition.z)
								{
									vNewPedPosition.z = vTargetPosition.z;
								}
							}
						}
					}
				}	

				qNewPedOrientation.Normalize();
				CTaskVehicleFSM::UpdatePedVehicleAttachment(GetPed(), VECTOR3_TO_VEC3V(vNewPedPosition), QUATERNION_TO_QUATV(qNewPedOrientation));
			}
		}
	}

	m_bProcessPhysicsCalledThisFrame = true;

	// Base class
	return CTask::ProcessPhysics(fTimeStep, nTimeSlice);
}

bool CTaskExitVehicleSeat::ProcessMoveSignals()
{
	const int state = GetState();

	CPed& rPed = *GetPed();
	if(state == State_ExitSeat || state == State_UpsideDownExit || state == State_OnSideExit)
	{
		// Check if done running the exit seat Move state.
		if (!m_bMoveNetworkFinished &&IsMoveNetworkStateFinished(ms_ExitSeatClipFinishedId, ms_ExitSeatPhaseId))
		{
			// Done, we need no more updates.
			m_bMoveNetworkFinished = true;
		}
	}
	else if(state == State_ExitToAim)
	{
		if(!CPedAILodManager::ShouldDoFullUpdate(*GetPed()))
		{
			SendMoveSignalsForExitToAim();
		}

		// Check if done running the exit to aim Move state.
		if (!m_bMoveNetworkFinished && IsMoveNetworkStateFinished(ms_ExitSeatClipFinishedId, CTaskVehicleFSM::ms_Clip2PhaseId))
		{
			// Done, we need no more updates.
			m_bMoveNetworkFinished = true;
		}

		rPed.SetPedResetFlag(CPED_RESET_FLAG_RequiresLegIk, true);
	}
	else if(state == State_FleeExit)
	{
		if (!m_bMoveNetworkFinished && (IsMoveNetworkStateFinished(ms_FleeExitAnimFinishedId, ms_FleeExitPhaseId) || m_pParentNetworkHelper->GetBoolean(ms_DetachId)))
		{
			// Done, we need no more updates.
			m_bMoveNetworkFinished = true;
		}
	}

	// Always wait so we can set the process physics reset flag
	RequestProcessPhysics();
	return true;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicleSeat::Start_OnUpdate()
{
	if (!taskVerifyf(m_iExitPointIndex != -1 && m_pVehicle->GetEntryExitPoint(m_iExitPointIndex), "Invalid entry point index"))
	{
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	m_pParentNetworkHelper = static_cast<CTaskExitVehicle*>(GetParent())->GetMoveNetworkHelper();

	if (!m_pParentNetworkHelper)
	{
		SetTaskState(State_Finish);
		return FSM_Continue;
	}
	
	s32 iCurrentSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(GetPed());
	if (iCurrentSeatIndex != -1 && m_pVehicle->IsTurretSeat(iCurrentSeatIndex))
	{
		CTaskVehicleFSM::PointTurretInNeturalPosition(*m_pVehicle, iCurrentSeatIndex);
	}

	s32 iInitialState = DecideInitialState();
	if(iInitialState == State_ExitToAim)
	{
		SetTaskState(State_ExitToAim);
		return FSM_Continue;
	}
	else if (!RequestClips(iInitialState))
	{
		SetTaskState(State_StreamAssets);
		return FSM_Continue;
	}
	else
	{
		SetTaskState(iInitialState);
		return FSM_Continue;
	}
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicleSeat::StreamAssets_OnUpdate()
{
	const float fTimeBeforeWarp = NetworkInterface::IsGameInProgress() ? CTaskEnterVehicle::ms_Tunables.m_MaxTimeStreamClipSetInBeforeWarpMP : CTaskEnterVehicle::ms_Tunables.m_MaxTimeStreamClipSetInBeforeWarpSP;

	s32 iInitialState = DecideInitialState();

	if (RequestClips(iInitialState))
	{
		SetTaskState(iInitialState);
		return FSM_Continue;
	}

	if (GetTimeInState() > fTimeBeforeWarp)
	{
		SetTaskState(State_Finish);
		return FSM_Continue;
	}
    else
    {
        CPed* pPed = GetPed();

        if(pPed->IsNetworkClone())
        {
            // if we don't manage to stream the anims before the clone ped has finished exiting just quit the task (the
            // ped will be popped to it's new position by the network code)
            if(!pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE))
            {
                SetTaskState(State_Finish);
			    return FSM_Continue;
            }
        }
    }
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicleSeat::ExitSeat_OnEnter()
{
	CPed& rPed = *GetPed();
	CVehicle& rVeh = *m_pVehicle; // Pointer checked in prefsm

	if(IsGoingToExitOnToVehicle())
	{
		//! Note: May not be true in all cases. Add an entry flag for this?
		SetRunningFlag(CVehicleEnterExitFlags::ExitSeatOntoVehicle);
		CTask* pParentTask = GetParent();
		if(pParentTask && pParentTask->GetTaskType() == CTaskTypes::TASK_EXIT_VEHICLE)
		{
			static_cast<CTaskExitVehicle*>(pParentTask)->SetRunningFlag(CVehicleEnterExitFlags::ExitSeatOntoVehicle);
		}
	}
	else
	{
		SetUpRagdollOnCollision(rPed, rVeh);
	}

	SetMoveNetworkState(ms_ExitSeatRequestId, ms_ExitSeatOnEnterId, ms_ExitSeatStateId);
	m_bHasIkAllowTags = false;
	bool bFoundClip = SetClipsForState();
	if(!bFoundClip)
	{
		m_bNoExitAnimFound = true;
	}

	SetClipPlaybackRate();
	
	//Add shocking event if it's a nice vehicle
	if (rVeh.GetVehicleModelInfo() && rVeh.GetVehicleModelInfo()->GetVehicleSwankness() > SWANKNESS_3)
	{
		rVeh.AddCommentOnVehicleEvent();
	}
	m_fInterruptPhase = -1.0f;

	rPed.SetClothPinRadiusScale(1.0f);
	rPed.SetClothForceSkin(false);
	rPed.SetPedConfigFlag( (ePedConfigFlags)CPED_CONFIG_FLAG_ForceSkinCharacterCloth, false );
	rPed.SetClothNegateForce(false);
	rPed.SetClothWindScale(1.0f);
	if (rVeh.GetVehicleModelInfo() && rVeh.GetVehicleModelInfo()->GetIsAutomobile())
	{
		u8 exitPinFramesNonPlayer = 25;
		u8 exitPinFramesPlayer = 10;
		if( rPed.IsPlayer() )
			rPed.SetClothForcePin(exitPinFramesPlayer);
		else
			rPed.SetClothForcePin(exitPinFramesNonPlayer);

		rPed.SetClothLockCounter(60);
	}

	CTaskVehicleFSM::SetRemotePedUsingDoorsTrue(rPed, &rVeh, m_iExitPointIndex);

	// Let ProcessMoveSignals() handle the communication with the Move network.
	m_bMoveNetworkFinished = false;
	RequestProcessMoveSignalCalls();
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicleSeat::ExitSeat_OnUpdate()
{
	// Don't let the door close on us due to bullets
	CTaskVehicleFSM::SetDoorDrivingFlags(m_pVehicle, m_iExitPointIndex);

	CPed* pPed = GetPed();

	if (m_pParentNetworkHelper->GetBoolean(ms_AllowedToAbortForInAirEventId))
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_CanAbortExitForInAirEvent, true);
	}
	if (m_pVehicle->InheritsFromBike() || m_pParentNetworkHelper->GetBoolean(ms_LegIkActiveId))
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_RequiresLegIk, true);
	}

	if (m_bHasIkAllowTags)
	{
		pPed->GetIkManager().SetFlag(PEDIK_LEGS_USE_ANIM_ALLOW_TAGS);
	}

	if(m_bNoExitAnimFound)
	{
		SetRunningFlag(CVehicleEnterExitFlags::Warp);
		ControlledSetPedOutOfVehicle(CPed::PVF_Warp | CPed::PVF_ClearTaskNetwork);
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	float fPhase = 0.0f;
	const crClip* pClip = GetClipAndPhaseForState(fPhase);
	CTaskVehicleFSM::ProcessLegIkBlendIn(*pPed, m_fLegIkBlendInPhase, fPhase);

	TUNE_GROUP_FLOAT(EXIT_VEHICLE_TUNE, EARLY_UNRESERVE_PHASE, 0.0f, 0.0f, 1.0f, 0.01f);
	if (!pPed->IsNetworkClone() && 
		m_pVehicle->GetLayoutInfo()->GetAllowEarlyDoorAndSeatUnreservation() && 
		fPhase >= EARLY_UNRESERVE_PHASE)
	{
		CTaskVehicleFSM::UnreserveDoor(*pPed, *m_pVehicle, m_iExitPointIndex);
		CTaskVehicleFSM::UnreserveSeat(*pPed, *m_pVehicle, m_iExitPointIndex);
	}

	const CVehicleEntryPointInfo* pEntryInfo = m_pVehicle->IsEntryIndexValid(m_iExitPointIndex) ? m_pVehicle->GetEntryInfo(m_iExitPointIndex) : NULL;
	bool bIsPlaneHatchEntry = pEntryInfo && pEntryInfo->GetIsPlaneHatchEntry();

	if (pClip && !m_pVehicle->GetLayoutInfo()->GetClimbUpAfterOpenDoor() && !bIsPlaneHatchEntry)
	{
		if (m_fInterruptPhase < 0.0f)
		{
			float fUnused;
			m_fInterruptPhase = 1.0f;
			CClipEventTags::GetMoveEventStartAndEndPhases(pClip, ms_ExitSeatInterruptId.GetHash(), m_fInterruptPhase, fUnused);
		}
	}

	CTaskVehicleFSM::ProcessApplyForce(*m_pParentNetworkHelper, *m_pVehicle, *pPed);
	
	if (m_pVehicle->InheritsFromBike())
	{
		const CEntryExitPoint* pEntryPoint = m_pVehicle->GetEntryExitPoint(m_iExitPointIndex);
		if (pEntryPoint && pEntryPoint->CanSeatBeReached(0) == SA_directAccessSeat)
		{
			CTaskVehicleFSM::eVehicleClipFlags eLeftArmIkClipflag  = (m_bWillBlendOutLeftHandleIk  ? CTaskVehicleFSM::VF_IK_Blend_Out : CTaskVehicleFSM::VF_No_IK_Clip);
			CTaskVehicleFSM::eVehicleClipFlags eRightArmIkClipflag = (m_bWillBlendOutRightHandleIk ? CTaskVehicleFSM::VF_IK_Blend_Out : CTaskVehicleFSM::VF_No_IK_Clip);

			// Left hand
			if (!m_bWillBlendOutLeftHandleIk)
			{
				// Keep Ik On
				CTaskVehicleFSM::ProcessBikeHandleArmIk(*m_pVehicle, *pPed, pClip, fPhase, true, false, true);
				// Blend Ik Off
				eLeftArmIkClipflag = CTaskVehicleFSM::ProcessBikeHandleArmIk(*m_pVehicle, *pPed, pClip, fPhase, false, false, true);
				m_bWillBlendOutLeftHandleIk = (eLeftArmIkClipflag == CTaskVehicleFSM::VF_IK_Blend_Out);
			}
			//Right hand
			if (!m_bWillBlendOutRightHandleIk)
			{
				// Keep Ik On
				CTaskVehicleFSM::ProcessBikeHandleArmIk(*m_pVehicle, *pPed, pClip, fPhase, true, true, true);
				// Blend Ik Off
				eRightArmIkClipflag = CTaskVehicleFSM::ProcessBikeHandleArmIk(*m_pVehicle, *pPed, pClip, fPhase, false, true, true);
				m_bWillBlendOutRightHandleIk = (eRightArmIkClipflag == CTaskVehicleFSM::VF_IK_Blend_Out);
			}

			//Force ik on if we haven't started the anim clip yet
			bool bNonZeroSteerAngle = m_pVehicle->GetSteerAngle() != 0.0f;
			if(!pClip || (bNonZeroSteerAngle && eLeftArmIkClipflag == CTaskVehicleFSM::VF_No_IK_Clip))
			{
				CTaskVehicleFSM::ForceBikeHandleArmIk(*m_pVehicle, *pPed, true, false, 0.0f, true);
			}

			if(!pClip || (bNonZeroSteerAngle && eRightArmIkClipflag == CTaskVehicleFSM::VF_No_IK_Clip))
			{
				CTaskVehicleFSM::ForceBikeHandleArmIk(*m_pVehicle, *pPed, true, true, 0.0f, true);
			}
		}
	}

	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	if (ShouldBeConsideredUnderWater())
	{
		SetRunningFlag(CVehicleEnterExitFlags::InWater);
		static_cast<CTaskExitVehicle*>(GetParent())->SetRunningFlag(CVehicleEnterExitFlags::ExitUnderWater);
	}

	//render weapon?
	u32 uSetPedOutOfVehicleFlags = CPed::PVF_ExcludeVehicleFromSafetyCheck | CPed::PVF_DontNullVelocity;
	TUNE_GROUP_BOOL(VEHICLE_EXIT_TUNE, INCLUDE_VEHICLE_IN_SAFETY_CHECK_ON_SLOPE, true);
	if (INCLUDE_VEHICLE_IN_SAFETY_CHECK_ON_SLOPE && m_pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_USE_STRICTER_EXIT_COLLISION_TESTS))
	{
		TUNE_GROUP_FLOAT(VEHICLE_EXIT_TUNE, ON_SLOPE_ROLL_TO_DO_SAFTEY_CHECK, 0.1f, 0.0f, 1.0f, 0.01f);
		const float fRoll = Abs(m_pVehicle->GetTransform().GetRoll());
		if (fRoll > ON_SLOPE_ROLL_TO_DO_SAFTEY_CHECK)
		{
			uSetPedOutOfVehicleFlags &= ~CPed::PVF_ExcludeVehicleFromSafetyCheck;
		}
	}


	const CVehicleSeatAnimInfo* pSeatClipInfo = CVehicleMetadataMgr::GetSeatAnimInfoFromPed(pPed);
	if (pSeatClipInfo && pSeatClipInfo->GetWeaponRemainsVisible())
	{
		uSetPedOutOfVehicleFlags |= CPed::PVF_DontDestroyWeaponObject;
	}

	if (IsRunningFlagSet(CVehicleEnterExitFlags::InWater))
	{
		if (pPed->GetIsInVehicle())
		{
			if (m_pParentNetworkHelper->GetBoolean(ms_DetachId))
			{
				ControlledSetPedOutOfVehicle(uSetPedOutOfVehicleFlags);
			}
		}
		if (m_pParentNetworkHelper && m_pParentNetworkHelper->GetBoolean(ms_ExitToSwimInterruptId))
		{
			ControlledSetPedOutOfVehicle(uSetPedOutOfVehicleFlags);

			//! If we can't swim, breakout.
			if (!pPed->GetPedResetFlag(CPED_RESET_FLAG_IsDrowning))
			{
				CTaskExitVehicle::TriggerFallEvent(GetPed());
			}
			else
			{
				static_cast<CTaskExitVehicle*>(GetParent())->SetRunningFlag(CVehicleEnterExitFlags::ExitToSwim);
			}

			SetTaskState(State_Finish);
		}
	}
	else
	{
		//! If we failed to apply ground fixup, then it means we weren't able to find ground position successfully. In this case, go to fall state
		//! on the detach flag.
		if (!m_bGroundFixupApplied && pPed->GetIsInVehicle() && m_pVehicle->InheritsFromBoat() && m_pParentNetworkHelper->GetBoolean(ms_DetachId))
		{
			CTaskExitVehicle::TriggerFallEvent(pPed);
			
			ControlledSetPedOutOfVehicle(uSetPedOutOfVehicleFlags);
			SetTaskState(State_Finish);
		}
	}

	if(m_pParentNetworkHelper->GetBoolean(ms_DetachId))
	{
		ControlledSetPedOutOfVehicle(uSetPedOutOfVehicleFlags);
	}

	if (m_bMoveNetworkFinished)
	{
		if (pPed->GetIsInVehicle() && !m_pVehicle->GetLayoutInfo()->GetClimbUpAfterOpenDoor() && !bIsPlaneHatchEntry)
		{
			pPed->SetVelocity(VEC3_ZERO);
			ControlledSetPedOutOfVehicle(CPed::PVF_ExcludeVehicleFromSafetyCheck|CPed::PVF_IgnoreSafetyPositionCheck);
		}
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	ProcessOpenDoor(false, true);

	if (m_pParentNetworkHelper->GetBoolean(ms_ExitSeatInterruptId) && !m_pVehicle->GetLayoutInfo()->GetClimbUpAfterOpenDoor() && !bIsPlaneHatchEntry)
	{
		if (pPed->IsLocalPlayer())
		{
			// Stop lookAt IK
			static const u32 uLookAtMyVehicleHash = ATSTRINGHASH("LookAtMyVehicle", 0x79037202);
			pPed->GetIkManager().AbortLookAt(500, uLookAtMyVehicleHash);
		}

		bool bWantsToInterruptExitSeat = false;

		if((pPed->GetOwnedBy()==ENTITY_OWNEDBY_RANDOM || pPed->GetOwnedBy()==ENTITY_OWNEDBY_POPULATION) && m_pVehicle->InheritsFromPlane() && !pPed->IsPlayer())
		{
			// prevent ambient peds from quitting their get-out anim early in this case (see url:bugstar:1392972)
		}
		else if(!pPed->IsNetworkClone() || ( pPed->IsPlayer() && ( m_pVehicle->InheritsFromBike() || m_pVehicle->InheritsFromQuadBike() || m_pVehicle->InheritsFromAmphibiousQuadBike()) ) )
		{
			bWantsToInterruptExitSeat = CTaskVehicleFSM::CheckForPlayerExitInterrupt(*pPed, *m_pVehicle, m_iRunningFlags, static_cast<CTaskExitVehicle*>(GetParent())->GetSeatOffset(), CTaskVehicleFSM::EF_TryingToSprintAlwaysInterrupts | CTaskVehicleFSM::EF_IgnoreDoorTest | CTaskVehicleFSM::EF_OnlyAllowInterruptIfTryingToSprint | CTaskVehicleFSM::EF_AnyStickInputInterrupts);
		}
        else
        {
			bWantsToInterruptExitSeat = DoesClonePedWantToInterruptExitSeatAnim(*pPed);
        }

		// See if the player wants to interrupt the exit
		if (bWantsToInterruptExitSeat)
		{
			if (pPed->IsLocalPlayer())
			{
				const CControl* pControl = pPed->GetControlFromPlayer();
				if (pControl)
				{
					if (pControl->GetPedEnter().HistoryPressed(CTaskVehicleFSM::ms_Tunables.m_TimeToConsiderEnterInputValid))
					{
						m_bWantsToEnter = true;
					}
				}
				static_cast<CTaskExitVehicle*>(GetParent())->SetRunningFlag(CVehicleEnterExitFlags::UseFastClipRate);
			}
			static_cast<CTaskExitVehicle*>(GetParent())->SetRunningFlag(CVehicleEnterExitFlags::HasInterruptedAnim);

			pPed->SetVelocity(VEC3_ZERO);
			CheckAndFixIfPedWentThroughSomething();
			ControlledSetPedOutOfVehicle(CPed::PVF_ExcludeVehicleFromSafetyCheck |CPed::PVF_IgnoreSafetyPositionCheck);		
			SetTaskState(State_Finish);
			return FSM_Continue;
		}
	}
	
	// Ambient lookAts
	if(pPed->IsAPlayerPed())
	{
		if(!GetSubTask())
		{
			CTaskExitVehicle* pExitTask = static_cast<CTaskExitVehicle*>(GetParent());
			if(!CTaskExitVehicle::IsPlayerTryingToAimOrFire(*pPed) && !pExitTask->GetWantsToEnterVehicle() && !pExitTask->GetWantsToEnterCover())
			{
				// Use "EMPTY" ambient context. We don't want any ambient anims to be played. Only lookAts.
				CTaskAmbientClips* pAmbientTask	= rage_new CTaskAmbientClips(0, CONDITIONALANIMSMGR.GetConditionalAnimsGroup(CTaskVehicleFSM::emptyAmbientContext.GetHash()));
				SetNewTask( pAmbientTask );
			}
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicleSeat::ExitSeat_OnExit()
{
	CPed& rPed = *GetPed();
	CTaskVehicleFSM::SetRemotePedUsingDoorsFalse(rPed, m_pVehicle, m_iExitPointIndex);
	//@@: location CTASKEXITVEHICLESEAT_EXITSEAT_ONEXIT_CLEAR_FLAGS
	CTaskVehicleFSM::ClearDoorDrivingFlags(m_pVehicle, m_iExitPointIndex);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicleSeat::FleeExit_OnEnter()
{
	CPed& rPed = *GetPed();
	m_iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(&rPed);
	fwMvClipSetId clipsetId = CTaskExitVehicleSeat::GetExitClipsetId(m_pVehicle, m_iExitPointIndex, m_iRunningFlags, &rPed);
	fwMvClipId clipId = ms_Tunables.m_DefaultFleeExitClipId;
	const crClip* pClip = fwClipSetManager::GetClip(clipsetId, clipId);
	m_pParentNetworkHelper->SetClip(pClip, ms_FleeExitClipId);
	m_bHasAdjustedForRoute = false;

	const float fPedCurrentHeading = rPed.GetCurrentHeading();
	float fPredictedHeading = fPedCurrentHeading;

	CTaskVehicleFSM::sMoverFixupInfo moverFixupInfo;
	if (pClip)
	{
		CTaskVehicleFSM::GetMoverFixupInfoFromClip(moverFixupInfo, pClip, m_pVehicle->GetVehicleModelInfo()->GetModelNameHash());

		Vector3 vTransDiff = fwAnimHelpers::GetMoverTrackTranslationDiff(*pClip, 0.0f, 1.0f);
		const float fCurrentPedHeight = rPed.GetTransform().GetPosition().GetZf();
		const float fPredictedPedHeight = fCurrentPedHeight + vTransDiff.z;
		Quaternion qRotationDiff = fwAnimHelpers::GetMoverTrackRotationDiff(*pClip, 0.0f, 1.0f);
		m_fFleeExitClipHeadingChange = qRotationDiff.TwistAngle(ZAXIS);
		fPredictedHeading = fwAngle::LimitRadianAngle(fPredictedHeading + m_fFleeExitClipHeadingChange);

		//aiDisplayf("Clip rotation : %.2f", m_fFleeExitClipHeadingChange);
		Vector3 vPedOrientation = VEC3V_TO_VECTOR3(rPed.GetTransform().GetB());
		vPedOrientation.RotateZ(m_fFleeExitClipHeadingChange);
		DEBUG_DRAW_ONLY(Vector3 vPedPosition = VEC3V_TO_VECTOR3(rPed.GetTransform().GetPosition()));
		Vec3V vExitPosition;

		if(rPed.IsNetworkClone())
		{	//Clone will take care of its own positioning
			vExitPosition = Vec3V(V_ZERO);
			m_fFleeExitExtraZChange = 0.0f;
		}
		else
		{
			vExitPosition = static_cast<CTaskExitVehicle*>(GetParent())->GetExitPosition();

			Vector3 vGroundPos(Vector3::ZeroType);
			if (CTaskVehicleFSM::FindGroundPos(m_pVehicle, GetPed(), VEC3V_TO_VECTOR3(vExitPosition), 2.5f, vGroundPos, false))
			{
				m_fFleeExitExtraZChange = vExitPosition.GetZf() - fPredictedPedHeight;
			}
		}

#if DEBUG_DRAW
		ms_debugDraw.AddArrow(RCC_VEC3V(vPedPosition), VECTOR3_TO_VEC3V(vPedPosition + vPedOrientation), 0.25f, Color_orange, 500);
		ms_debugDraw.AddLine(RCC_VEC3V(vPedPosition), vExitPosition, Color_purple, 1500);
		ms_debugDraw.AddSphere(vExitPosition, 0.05f, Color_purple, 1500);
#endif // DEBUG_DRAW
	}
		
	CTaskMoveFollowNavMesh* pNavTask = static_cast<CTaskMoveFollowNavMesh*>(rPed.GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH));
	if (pNavTask && pNavTask->GetRoute())
	{
		if (pNavTask->GetRouteSize() > 0)
		{
			Vector3 vDesiredNodePos;
			const Vector3 vFirstNodePosition = pNavTask->GetRoute()->GetPosition(0);
			if (pNavTask->GetRouteSize() > 1)
			{
				const Vector3 vPedPosition = VEC3V_TO_VECTOR3(rPed.GetTransform().GetPosition());
				Vector3 vToFirstNode = vFirstNodePosition - vPedPosition;
				vToFirstNode.Normalize();
				const Vector3 vSecondNodePosition = pNavTask->GetRoute()->GetPosition(1);
				Vector3 vToSecondNode = vSecondNodePosition - vToFirstNode;
				vToSecondNode.Normalize();
		
				// If the second node is roughly in the same direction as our initial direction to the first node, head towards that instead
				TUNE_GROUP_FLOAT(FLEE_TUNE, MIN_ROUTE_DOT_TO_USE_SECOND_NODE, 0.707f, 0.0f, 1.0f, 0.001f);
				if (vToFirstNode.Dot(vToSecondNode) > MIN_ROUTE_DOT_TO_USE_SECOND_NODE)
				{
					rPed.SetDesiredHeading(vSecondNodePosition);
					vDesiredNodePos = vSecondNodePosition;
				}
				else
				{
					rPed.SetDesiredHeading(vFirstNodePosition);
					vDesiredNodePos = vFirstNodePosition;
				}		
			}
			else
			{
				rPed.SetDesiredHeading(vFirstNodePosition);
				vDesiredNodePos = vFirstNodePosition;
			}

			if (GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_EXIT_VEHICLE)
			{
				aiDisplayf("Frame %i, %s Ped %s setting flee exit pos to (%.2f,%.2f,%.2f)", fwTimer::GetFrameCount(), rPed.IsNetworkClone() ? "CLONE" : "LOCAL", rPed.GetNetworkObject() ? rPed.GetNetworkObject()->GetLogName() : rPed.GetModelName(), vDesiredNodePos.x, vDesiredNodePos.y, vDesiredNodePos.z);
				static_cast<CTaskExitVehicle*>(GetParent())->SetFleeExitPosition(vDesiredNodePos);
			}
		}
	}

	if (sm_bCloneVehicleFleeExitFix && rPed.IsNetworkClone() && GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_EXIT_VEHICLE)
	{
		const Vector3 vFleeExitPos = static_cast<CTaskExitVehicle*>(GetParent())->GetFleeExitPosition();
		if (!vFleeExitPos.IsClose(Vector3(0.0f,0.0f,0.0f), SMALL_FLOAT))
		{
			aiDisplayf("Frame %i, %s Ped %s setting desired flee exit pos to (%.2f,%.2f,%.2f)", fwTimer::GetFrameCount(), rPed.IsNetworkClone() ? "CLONE" : "LOCAL", rPed.GetNetworkObject() ? rPed.GetNetworkObject()->GetLogName() : rPed.GetModelName(), vFleeExitPos.x, vFleeExitPos.y, vFleeExitPos.z);
			rPed.SetDesiredHeading(vFleeExitPos);
		}
	}

	const float fPedDesiredHeading = rPed.GetDesiredHeading();
	Vector3 vPredictedHeading(0.0f, 1.0f, 0.0f);
	vPredictedHeading.RotateZ(fPredictedHeading);
	Vector3 vDesiredFleeHeading(0.0f, 1.0f, 0.0f);
	vDesiredFleeHeading.RotateZ(fPedDesiredHeading);
	m_fExtraHeadingChange = -SubtractAngleShorter(fPredictedHeading, fPedDesiredHeading);
	TUNE_GROUP_FLOAT(FLEE_EXIT_TUNE, MAX_EXTRA_HEADING_ANGLE, 0.6f, 0.0f, PI, 0.01f);
	m_fExtraHeadingChange = Sign(m_fExtraHeadingChange) * Clamp(m_fExtraHeadingChange, -MAX_EXTRA_HEADING_ANGLE, MAX_EXTRA_HEADING_ANGLE);

	if (Sign(m_fExtraHeadingChange) < 0.0f)
	{
		m_fStartRotationFixUpPhase = moverFixupInfo.fRotFixUpEndPhase;
	}
	else
	{
		m_fStartRotationFixUpPhase = moverFixupInfo.fRotFixUpStartPhase;;
	}

	m_fStartTranslationFixupPhase = moverFixupInfo.fZFixUpStartPhase;
	m_fEndTranslationFixupPhase = moverFixupInfo.fZFixUpEndPhase;

	fragInstNMGta* pRagdollInst = rPed.GetRagdollInst();
	if (pRagdollInst != NULL)
	{
		// The initial velocity of the ragdoll will be calculated from the previous and current skeleton bounds.
		// This can be a problem when exiting a vehicle quickly. If the character ragdolls as they are quickly exiting a
		// vehicle there can be quite a bit of inherited velocity (see bug #2037601). Trying to keep this change as local 
		// as possible so only enabling this limit while exiting to aim/flee

		// The next time the ragdoll is activated, the initial velocity will be scaled
		static bank_float sfVehicleExitFleeingRagdollIncomingAnimVelocityScale = 0.5f;
		pRagdollInst->SetIncomingAnimVelocityScaleReset(sfVehicleExitFleeingRagdollIncomingAnimVelocityScale);

		taskWarningf("CTaskExitVehicleSeat::FleeExit_OnEnter: Scaling initial ragdoll velocity by %f", sfVehicleExitFleeingRagdollIncomingAnimVelocityScale);
	}

	StoreInitialOffsets();	
	SetMoveNetworkState(ms_FleeExitRequestId, ms_FleeExitOnEnterId, ms_FleeExitStateId);

	// Let ProcessMoveSignals() handle the communication with the Move network.
	m_bMoveNetworkFinished = false;
	RequestProcessMoveSignalCalls();
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicleSeat::FleeExit_OnUpdate()
{
	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	CPed& rPed = *GetPed();

	rPed.SetPedResetFlag(CPED_RESET_FLAG_RequiresLegIk, true);
	rPed.GetIkManager().SetFlag(PEDIK_LEGS_AND_PELVIS_FADE_ON);

	if (rPed.ShouldBeDead() && GetTimeInState() > ms_Tunables.m_MinTimeInStateToAllowRagdollFleeExit)
	{
		SetTaskState(State_DetachedFromVehicle);
		return FSM_Continue;
	}

	ProcessOpenDoor(false, false);
	if (!rPed.GetIsInVehicle())
	{
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicleSeat::FleeExit_OnExit()
{
	CPed& rPed = *GetPed();

	fragInstNMGta* pRagdollInst = rPed.GetRagdollInst();
	if (pRagdollInst != NULL)
	{
		// Reset the incoming anim velocity scale
		pRagdollInst->SetIncomingAnimVelocityScaleReset(-1.0f);

		taskWarningf("CTaskExitVehicleSeat::FleeExit_OnExit: Reset initial ragdoll velocity");
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicleSeat::JumpOutOfSeat_OnEnter()
{
	CPed* pPed = GetPed();
	if (pPed && pPed->IsLocalPlayer())
	{
		StatsInterface::IncrementStat(StatsInterface::GetStatsModelHashId("BAILED_FROM_VEHICLE"), 1.0f);
	}

	// Allow the ped to ragdoll if we get impacted whilst exiting
	SetUpRagdollOnCollision(*pPed, *m_pVehicle);
	m_iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
	SetMoveNetworkState(ms_JumpOutRequestId, ms_JumpOutOnEnterId, ms_JumpOutStateId);
	SetClipsForState();

	m_bWasInWaterAtBeginningOfJumpOut = ShouldBeConsideredUnderWater();

	//! Set ped as in air if jumping out of a plane or heli. Note: Could do this for all vehicles, but it makes sense for these.
	if(m_pVehicle->InheritsFromPlane() || m_pVehicle->InheritsFromHeli())
	{
		GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_IsInTheAir, true);

		audNorthAudioEngine::NotifyLocalPlayerBailedFromAircraft();

		// play ejector seat vfx for flagged vehicles
		if (IsEjectorSeat(m_pVehicle))
		{
			g_vfxVehicle.TriggerPtFxEjectorSeat(m_pVehicle);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicleSeat::JumpOutOfSeat_OnUpdate()
{
	// If we don't have a move network helper we're screwed so bail out
	if (!taskVerifyf(m_pParentNetworkHelper, "Parent Network Helper Was NULL"))
	{
		return FSM_Quit;
	}

	CPed* pPed = GetPed();
	pPed->SetPedResetFlag(CPED_RESET_FLAG_JumpingOutOfVehicle, true);

	// B*2041429 - Because of how the ped jumps out of the jetski we don't want 
	// to trigger NM on collision if we end up going slow
	if( m_pVehicle->GetIsJetSki() && pPed->GetActivateRagdollOnCollisionEvent() )
	{
		const float fSpeedThresholdToTriggerNM = 6.0f;
		if( m_pVehicle->GetVelocity().Mag2() < fSpeedThresholdToTriggerNM * fSpeedThresholdToTriggerNM )
		{
			pPed->ClearActivateRagdollOnCollisionEvent();
		}
	}

	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	float fClipPhase = 0.0f;
	const crClip* pClip = GetClipAndPhaseForState(fClipPhase);

	if (m_pParentNetworkHelper->GetBoolean(ms_StandUpInterruptId) && CTaskVehicleFSM::CheckForPlayerMovement(*pPed))
	{
		// If we didn't exit the seat from the detach event, do it here
		if (!m_bExitedSeat)
		{
			pPed->SetPedOutOfVehicle(CPed::PVF_ExcludeVehicleFromSafetyCheck);
		}

		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	// have we reached the interupt...
	bool JumpOutOfSeatComplete = m_pParentNetworkHelper->GetBoolean(ms_ExitToSkyDiveInterruptId);

	// However, if we're in a network game and diving out of a riot van, we need the clip to play 
	// out further than the interupt otherwise the clone gets stuck in the van as it lags behind
	if(NetworkInterface::IsGameInProgress())
	{
		if(m_pVehicle && m_pVehicle->GetModelIndex() == MI_VAN_RIOT)
		{
			// We're using the "jump out and land on floor" clip to "jump out to skydive"
			float fPhase = m_pParentNetworkHelper->GetFloat( ms_JumpOutPhaseId);
			fPhase = rage::Clamp(fPhase, 0.0f, 1.0f);

			// Need to continue past the flagged interupt and let the anim play for slightly longer...
			TUNE_GROUP_FLOAT (EXIT_VEHICLE_TUNE, fDesiredEndingPhaseJumpOutToSkydiveForRiotVanInMP, 0.6f, 0.0f, 1.0f, 0.01f);
			if(fPhase <= fDesiredEndingPhaseJumpOutToSkydiveForRiotVanInMP)
			{
				JumpOutOfSeatComplete = false;
			}
		}
	}
	
	if (JumpOutOfSeatComplete && CheckForSkyDive(pPed, m_pVehicle, m_iExitPointIndex, false))
	{
		static dev_float s_fUpTest = 0.8f;

		//! If we aren't upright, ragdoll.
		ScalarV scDot = Dot(pPed->GetTransform().GetC(), Vec3V(V_Z_AXIS_WZERO));
		ScalarV scMinDot(ScalarVFromF32(s_fUpTest));
		if(IsLessThanAll(scDot, scMinDot))
		{
			//! no need to ragdoll, this will happen anyway.
		}
		else
		{
			CTaskExitVehicle::OnExitMidAir(pPed, m_pVehicle);

			// !! GTA V HACK !!
			if(m_pVehicle->InheritsFromBike())
				pPed->SetNoCollision(m_pVehicle, NO_COLLISION_RESET_WHEN_NO_BBOX);
			else
				pPed->SetNoCollision(m_pVehicle, NO_COLLISION_RESET_WHEN_NO_IMPACTS);

			static_cast<CTaskExitVehicle*>(GetParent())->SetRunningFlag(CVehicleEnterExitFlags::IsTransitionToSkyDive);
			SetRunningFlag(CVehicleEnterExitFlags::IsTransitionToSkyDive);

			u32 uFlags = CPed::PVF_ExcludeVehicleFromSafetyCheck |
									CPed::PVF_DontSnapMatrixUpright |
									CPed::PVF_DontNullVelocity |
									CPed::PVF_NoCollisionUntilClear |
									CPed::PVF_InheritVehicleVelocity;
			pPed->SetPedOutOfVehicle(uFlags, 50);

			m_bExitedSeat = true;

			CTaskExitVehicle::TriggerFallEvent(pPed, FF_DisableSkydiveTransition | FF_LongerParachuteBlend | FF_ForceSkyDive);

			//Process the in-air event this frame, otherwise we end up with a frame of delay.
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAIUpdate, true);

			SetTaskState(State_Finish);
			return FSM_Continue;
		}
	}

	const bool bConsideredUnderWater = ShouldBeConsideredUnderWater();
	if (bConsideredUnderWater)
	{
		SetRunningFlag(CVehicleEnterExitFlags::InWater);
		static_cast<CTaskExitVehicle*>(GetParent())->SetRunningFlag(CVehicleEnterExitFlags::ExitUnderWater);
	}

	bool bConsiderExitToSwim = m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::InWater) || m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::ExitUnderWater) || m_pVehicle->GetIsAquatic();
	if (m_pVehicle->InheritsFromAmphibiousAutomobile() || m_pVehicle->InheritsFromAmphibiousQuadBike())
	{
		if (!m_pVehicle->GetIsInWater())
		{
			bConsiderExitToSwim = false;
		}
	}

	if (bConsiderExitToSwim && m_pParentNetworkHelper && m_pParentNetworkHelper->GetBoolean(ms_ExitToSwimInterruptId))
	{
		pPed->SetPedOutOfVehicle(CPed::PVF_ExcludeVehicleFromSafetyCheck);

		if (pPed->GetPedResetFlag(CPED_RESET_FLAG_IsDrowning))
		{	
			if(!m_pVehicle->GetIsAquatic() && (m_pVehicle->GetVelocity().Mag2() > (ms_Tunables.m_RagdollIntoWaterVelocity * ms_Tunables.m_RagdollIntoWaterVelocity)))
			{
				if (CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_VEHICLE_JUMPOUT))
				{
					nmEntityDebugf(pPed, "CTaskExitVehicleSeat::JumpOutOfSeat - Added jump from road vehicle event");

					atHashString entryAnimInfoHash; 
					if (m_pVehicle->IsEntryIndexValid(m_iExitPointIndex))
					{
						const CVehicleEntryPointAnimInfo* pAnimInfo = m_pVehicle->GetEntryAnimInfo(m_iExitPointIndex);
						if (pAnimInfo)
						{
							entryAnimInfoHash = pAnimInfo->GetName();
						}
					}

					CEventSwitch2NM event(500, rage_new CTaskNMJumpRollFromRoadVehicle(1000, 5000, true, false, entryAnimInfoHash, m_pVehicle));
					pPed->SwitchToRagdoll(event);
				}
			}
			else
			{
				static_cast<CTaskExitVehicle*>(GetParent())->SetRunningFlag(CVehicleEnterExitFlags::ExitToSwim);
			}
		}
		else
		{
			//! If our boat exit has missed water, then ragdoll if possible. Note: This assumes NM tag comes before swim exit interrupt.
			if(m_pVehicle->GetIsAquatic())
			{
				SetTaskState(State_DetachedFromVehicle);
				return FSM_Continue;
			}
			else
			{
				//! We should be swimming by now, but we aren't, so just bail.
				CTaskExitVehicle::TriggerFallEvent(GetPed());
			}
		}

		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	if (IsMoveNetworkStateFinished(ms_JumpOutClipFinishedId, ms_JumpOutPhaseId))
	{
		// If we didn't exit the seat from the detach event, do it here
		if (!m_bExitedSeat)
		{
			pPed->SetPedOutOfVehicle(CPed::PVF_ExcludeVehicleFromSafetyCheck);
		}

		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	if (!m_bExitedSeat)
	{
		ProcessOpenDoor(false);
	}

	// Network clones always continue to play the animated jump off until the owner gives them a ragdoll task
	// This fixes issues where the owner wasn't allowed to ragdoll but the clone was being forced to ragdoll causing the two to become out of sync
	if (pClip && !m_bExitedSeat && !pPed->IsNetworkClone())
	{
		const bool bWasntUnderWaterButNowIsBikeOnly = m_pVehicle->InheritsFromBike() && !m_bWasInWaterAtBeginningOfJumpOut && bConsideredUnderWater;
		bool bOnBikeAndThinkWeWentThroughSomething = false;

		TUNE_GROUP_BOOL(EXIT_VEHICLE_TUNE, WENT_THROUGH_WALL_FOR_ALL_VEHICLES, true);
		if((m_pVehicle->InheritsFromBike() || m_pVehicle->InheritsFromQuadBike() || m_pVehicle->InheritsFromAmphibiousQuadBike() || WENT_THROUGH_WALL_FOR_ALL_VEHICLES) && CTaskVehicleFSM::CheckIfPedWentThroughSomething(*pPed, *m_pVehicle))
		{
			TUNE_GROUP_BOOL(EXIT_VEHICLE_TUNE, USE_EARLY_RAGDOLL_IF_WENT_THROUGH_SOMETHING, true);
			bOnBikeAndThinkWeWentThroughSomething = true && USE_EARLY_RAGDOLL_IF_WENT_THROUGH_SOMETHING;
		}

		float fEventPhase = 1.0f;
		if (!m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::ExitUnderWater) || bWasntUnderWaterButNowIsBikeOnly)
		{
			bool bPlaneEarlyRagdoll = false;
			if (m_pVehicle->InheritsFromPlane() && m_pParentNetworkHelper->GetBoolean(ms_NoWingEarlyRagdollId))
			{
				const CPlane* pPlane = static_cast<const CPlane*>(m_pVehicle.Get());
				if (!pPlane->IsEntryPointUseable(m_iExitPointIndex))
				{
					bPlaneEarlyRagdoll = true;
				}
			}

			if (bOnBikeAndThinkWeWentThroughSomething || bWasntUnderWaterButNowIsBikeOnly || bPlaneEarlyRagdoll || (CClipEventTags::FindEventPhase(pClip, CClipEventTags::CanSwitchToNm, fEventPhase) && fClipPhase >= fEventPhase))
			{
				// Detach when the clip reaches a certain point
				taskAssertf(!m_bExitedSeat, "Ped set out of vehicle before switching to nm, check the detach tags. ClipSet:%s, Clip:%s",
					m_pParentNetworkHelper->GetClipSet() ? m_pParentNetworkHelper->GetClipSet()->GetClipDictionaryName().TryGetCStr(): "NO CLIP SET",
					pClip ? pClip->GetName() : "NULL"
					);

				bool bUseAnimatedJumpoff = false;
				if (m_pVehicle->InheritsFromBike())
				{
					const float fVelocityToUseAnimatedJumpOff = m_pVehicle->GetVehicleType() == VEHICLE_TYPE_BICYCLE ? ms_Tunables.m_BicycleVelocityToUseAnimatedJumpOff : ms_Tunables.m_BikeVelocityToUseAnimatedJumpOff;
					if (bWasntUnderWaterButNowIsBikeOnly || bOnBikeAndThinkWeWentThroughSomething)
					{
						bUseAnimatedJumpoff = false;
					}
					else
					{
						bUseAnimatedJumpoff = !m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::InAirExit) && GetPreviousState() != State_BeJacked &&  m_pVehicle->GetVelocity().Mag2() < rage::square(fVelocityToUseAnimatedJumpOff);
					}
					//Push the bike away from the direction the player jumps
					m_pVehicle->ApplyImpulse(-1.0f*ms_Tunables.m_BikeExitForce*pPed->GetMassForApplyVehicleForce()*VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().GetA()), -0.1f*VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().GetB()));
				}

				if(m_pVehicle->InheritsFromBoat() || (m_pVehicle->InheritsFromAmphibiousAutomobile() && m_pVehicle->GetIsInWater()))
				{
					TUNE_GROUP_FLOAT (EXIT_VEHICLE_TUNE, fWaterSearchZOffsetForHighFall, 10.0f, 0.0f, 10000.0f, 0.0001f);
					TUNE_GROUP_FLOAT (EXIT_VEHICLE_TUNE, fWaterDepthForHighFallRequired, 1.5f, 0.0f, 10000.0f, 0.0001f);
					TUNE_GROUP_FLOAT (EXIT_VEHICLE_TUNE, fPedHeightAboveWaterForSwimExit, 2.5f, 0.0f, 10000.0f, 0.0001f);

					//! Ragdoll in non-driver seats.
					if(!pPed->IsPlayer())
					{
						SetTaskState(State_DetachedFromVehicle);
						return FSM_Continue;
					}
					else
					{
						float fWaterHeight = 0.0f;
						bool bWaterUnderPed = CheckForWaterUnderPed(pPed, m_pVehicle, fWaterSearchZOffsetForHighFall, fWaterDepthForHighFallRequired, fWaterHeight);
						if(!bWaterUnderPed || GetPed()->GetTransform().GetPosition().GetZf() > (fWaterHeight + fPedHeightAboveWaterForSwimExit))
						{
							//! Note: Just ragdoll non players.
							static dev_float s_fVelThreshold = 5.0f;
							if(m_pVehicle->GetVelocity().Mag2() > (s_fVelThreshold*s_fVelThreshold))
							{
								SetTaskState(State_DetachedFromVehicle);
							}
							else
							{
								pPed->SetPedOutOfVehicle(CPed::PVF_ExcludeVehicleFromSafetyCheck | CPed::PVF_DontNullVelocity);

								CTaskExitVehicle::TriggerFallEvent(GetPed());

								SetTaskState(State_Finish);
							}
							return FSM_Continue;
						}
					}
				}
				else if ((!bUseAnimatedJumpoff && CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_VEHICLE_JUMPOUT)) || m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::DeadFallOut))
				{
					SetTaskState(State_DetachedFromVehicle);
					return FSM_Continue;
				}
			}
		}
	}

	if (!m_bExitedSeat)
	{
		ProcessSetPedOutOfSeat();
	}

	if(m_bExitedSeat)
	{
		//! use extracted animated z (which also prevents ped going into the fall state).
		if(m_pVehicle->InheritsFromBoat() || (m_pVehicle->InheritsFromAmphibiousAutomobile() && m_pVehicle->GetIsInWater()))
		{
			pPed->SetUseExtractedZ(true);
		}
	
		if (m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::InAirExit))
		{
			SetTaskState(State_DetachedFromVehicle);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicleSeat::ExitToAim_OnEnter()
{
	// Clear our running flag (we'll set it again at the end if all passes check)
	m_iRunningFlags.BitSet().Clear(CVehicleEnterExitFlags::ExitToAim);

	// Set the aim clip set based on our weapon
	CPed* pPed = GetPed();

	// Allow the ped to ragdoll if we get impacted whilst exiting
	SetUpRagdollOnCollision(*pPed, *m_pVehicle);

	// if our seat index is not valid then we shouldn't run this state
	m_iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
	if(!m_pVehicle->IsSeatIndexValid(m_iSeatIndex))
	{
		return;
	}

	// Stop if we don't have a weapon manager
	CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
	if(!pWeaponManager)
	{
		return;
	}

	// make sure we have our best weapon otherwise our peds can end up switching weapons after playing the exit animation
	u32 uWeaponHash = pWeaponManager->GetWeaponSelector()->GetBestPedWeapon(pPed, CWeaponInfoManager::GetSlotListBest(), true);
#if __DEV
	TUNE_GROUP_BOOL(EXIT_TO_AIM_TUNE, USE_1H_WEAPON, false);
	if (USE_1H_WEAPON)
	{
		uWeaponHash = pWeaponManager->GetWeaponSelector()->GetBestPedWeapon(pPed, CWeaponInfoManager::GetSlotListBest(), false);
	}
#endif // __DEV
	if(uWeaponHash == 0)
	{
		return;
	}

	// Double check that the ped has this weapon in the inventory and get the weapon info
	const CWeaponInfo* pWeaponInfo = pPed->GetInventory()->GetWeapon(uWeaponHash) ? pPed->GetInventory()->GetWeapon(uWeaponHash)->GetInfo() : NULL;

	// Stop if we don't have a weapon info OR don't have a valid clip set OR the weapon is not a gun OR the weapon doesn't have aiming info
	if(!pWeaponInfo)
	{
		return;
	}
	const fwMvClipSetId appropriateWeaponClipSetId = pWeaponInfo->GetAppropriateWeaponClipSetId(pPed);
	if(appropriateWeaponClipSetId == CLIP_SET_ID_INVALID || !fwClipSetManager::IsStreamedIn_DEPRECATED(appropriateWeaponClipSetId) ||
		!pWeaponInfo->GetIsGun() || !pWeaponInfo->GetAimingInfo())
	{
		return;
	}

	const ExitToAimSeatInfo* pExitToAimSeatInfo = GetExitToAimSeatInfo(m_pVehicle, m_iSeatIndex, m_iExitPointIndex);
	if(!pExitToAimSeatInfo)
	{
		return;
	}

	// Get our clip set and verify it exists and is streamed
	atHashString clipSetHash = pWeaponInfo->GetIsTwoHanded() ? pExitToAimSeatInfo->m_TwoHandedClipSetName : pExitToAimSeatInfo->m_OneHandedClipSetName;
	if(clipSetHash.IsNull())
	{
		return;
	}

	fwMvClipSetId clipSetId(clipSetHash);
	fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
	if(!pClipSet || !pClipSet->IsStreamedIn_DEPRECATED())
	{
		return;
	}

	// Verify we have a list of clips that matches the name provided by the exit to aim info
	const ExitToAimClipSet* pExitToAimClipSet = GetExitToAimClipSet(pExitToAimSeatInfo->m_ExitToAimClipsName);
	if(!pExitToAimClipSet)
	{
		return;
	}

	// Send the clip params to the move network
	const crClip* clips[ms_uNumExitToAimClips];
	for (s32 i=0; i<5; ++i)
	{
		clips[i] = fwClipSetManager::GetClip(clipSetId, pExitToAimClipSet->m_Clips[i]);
		m_pParentNetworkHelper->SetClip(clips[i], CTaskVehicleFSM::GetGenericClipId(i));
	}

	// Clones can't equip weapons - just use inventory syncing.
	if(!pPed->IsNetworkClone())
	{
		// We succeeded at everything else so try to equip the weapon
		if(!pWeaponManager->EquipWeapon(uWeaponHash, -1, true))
		{
			return;
		}
	}

	// We succeeded! Set our running flag again so the state doesn't exit during the update
	m_iRunningFlags.BitSet().Set(CVehicleEnterExitFlags::ExitToAim);

	// the weapon will get synced over the network in the right visibility state.
	if(!pPed->IsNetworkClone())
	{
		// Create our weapon object
		CObject* pWeaponObject = pWeaponManager->GetEquippedWeaponObject();
		if(!pWeaponObject)
		{
			pWeaponManager->CreateEquippedWeaponObject();
		}
		else if(!pWeaponObject->GetIsVisible())
		{
			pWeaponObject->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, true, true);
		}
	}

	// We shouldn't call GetAppropriateWeaponClipSetId(), but I'm a little unsure on if its return value
	// could possibly have changed due to the stuff we did above, so I added the assert to make sure we don't
	// get an unnoticed change in behavior here.
	Assert(appropriateWeaponClipSetId == pWeaponInfo->GetAppropriateWeaponClipSetId(pPed));

	// Set our additional clip set to the weapons clip set and set our flag to aim flag as well as requesting the correct state
	m_pParentNetworkHelper->SetClipSet(appropriateWeaponClipSetId, ms_AdditionalAnimsClipSet);
	m_pParentNetworkHelper->SetFlag(true, ms_ToAimFlagId);
	SetMoveNetworkState(ms_ExitSeatRequestId, ms_ExitSeatOnEnterId, ms_ExitSeatStateId);

	// Set our initial react blend heading
	m_fAimBlendReferenceHeading = pPed->GetCurrentHeading() + HALF_PI;
	m_fAimBlendCurrentHeading = CalculateAimDirection();
	m_fExitToAimDesiredHeading = pPed->GetDesiredHeading();

	m_fAimInterruptPhase = -1.0f;
	m_fUpperBodyAimPhase = -1.0f;

	// Assume the blend curves are linear
	CTaskVehicleFSM::sClipBlendParams clipBlendParams;
	for (s32 i=0; i<ms_uNumExitToAimClips; ++i)
	{
		clipBlendParams.afBlendWeights.PushAndGrow(0.0f);
	}
	CTaskVehicleFSM::PreComputeStaticBlendWeightsFromSignal(clipBlendParams, m_fAimBlendCurrentHeading);
	float fHighestBlendWeight = 0.0f;
	for (s32 i=0; i<ms_uNumExitToAimClips; ++i )
	{
		const float fCurrentWeight = clipBlendParams.afBlendWeights[i];
		if (fCurrentWeight > fHighestBlendWeight)
		{
			m_iDominantAimClip = i;
			fHighestBlendWeight = fCurrentWeight;
		}
	}
	const crClip* pDominantClip = clips[m_iDominantAimClip];
	if (pDominantClip)
	{
		float fUnused;
		CClipEventTags::GetMoveEventStartAndEndPhases(pDominantClip, ms_AimIntroInterruptId, m_fAimInterruptPhase, fUnused);
		CClipEventTags::GetMoveEventStartAndEndPhases(pDominantClip, ms_BlendInUpperBodyAimId, m_fUpperBodyAimPhase, fUnused);
	}

	fragInstNMGta* pRagdollInst = pPed->GetRagdollInst();
	if (pRagdollInst != NULL)
	{
		// The initial velocity of the ragdoll will be calculated from the previous and current skeleton bounds.
		// This can be a problem when exiting a vehicle quickly. If the character ragdolls as they are quickly exiting a
		// vehicle there can be quite a bit of inherited velocity (see bug #2037601). Trying to keep this change as local 
		// as possible so only enabling this limit while exiting to aim/flee

		// The next time the ragdoll is activated, the initial velocity will be scaled
		static bank_float sfVehicleExitAimingRagdollIncomingAnimVelocityScale = 0.5f;
		pRagdollInst->SetIncomingAnimVelocityScaleReset(sfVehicleExitAimingRagdollIncomingAnimVelocityScale);

		taskWarningf("CTaskExitVehicleSeat::ExitToAim_OnEnter: Scaling initial ragdoll velocity by %f", sfVehicleExitAimingRagdollIncomingAnimVelocityScale);
	}

	// Let ProcessMoveSignals() handle the communication with the Move network.
	m_bMoveNetworkFinished = false;
	RequestProcessMoveSignalCalls();
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicleSeat::ExitToAim_OnUpdate()
{
	// If for some reason we failed to start the exit to aim then we should fall back to the exit seat state
	CPed* pPed = GetPed();

	// We need to process the door opening each frame
	pPed->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate | CPedAILod::AL_LodTimesliceAnimUpdate);
	pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);

	if(!m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::ExitToAim))
	{
		fwMvClipSetId exitClipsetId = GetExitClipsetId(m_pVehicle, m_iExitPointIndex, m_iRunningFlags, pPed);
		if (CTaskVehicleFSM::RequestVehicleClipSetReturnIsLoaded(exitClipsetId, SP_High))
		{
			SetTaskState(State_ExitSeat);
			return FSM_Continue;
		}
		else
		{
			SetTaskState(State_StreamAssets);
			return FSM_Continue;
		}
	}

	pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(0.0f);

	//Send the move signals.
	if(!SendMoveSignalsForExitToAim())
	{
		return FSM_Quit;
	}

	if (!m_bExitedSeat)
	{
		ProcessSetPedOutOfSeat();

		if(m_bExitedSeat)
		{
			u32 iSelectedWeapon = pPed->GetWeaponManager()->GetSelectedWeaponHash();
			u32 iEquippedWeapon = pPed->GetWeaponManager()->GetEquippedWeaponObjectHash();
			if((iEquippedWeapon != iSelectedWeapon) && (!pPed->IsNetworkClone()))
			{
				pPed->GetWeaponManager()->EquipWeapon(iEquippedWeapon);
			}
		}
	}

	bool bShouldInterrupt = false;

	float fDominantClipPhase = CTaskVehicleFSM::GetGenericClipPhaseClamped(*m_pParentNetworkHelper, m_iDominantAimClip);

	if (m_fAimInterruptPhase > 0.0f && fDominantClipPhase > m_fAimInterruptPhase)
	{
		if (!pPed->IsAPlayerPed())
		{
			// Calculate the aim vector (this determines the heading and pitch angles to point the clips in the correct direction)
			CTask* pParentTask = GetParent();
			// Vector3 vStart(pPed->GetBonePositionCached((eAnimBoneTag)BONETAG_L_HAND));
			Vector3 vTarget(VEC3_ZERO);
			if (pParentTask && pParentTask->GetTaskType() == CTaskTypes::TASK_EXIT_VEHICLE && static_cast<CTaskExitVehicle*>(pParentTask)->GetDesiredTarget())
			{
				vTarget = VEC3V_TO_VECTOR3(static_cast<CTaskExitVehicle*>(pParentTask)->GetDesiredTarget()->GetTransform().GetPosition());
			}
			else
			{
				vTarget = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) + VEC3V_TO_VECTOR3(pPed->GetTransform().GetB());
			}

			// Calculate a normalised directional vector between the ped and its target
			Vector3 vTargetDir = vTarget - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			vTargetDir.z = 0.0f;
			vTargetDir.Normalize();
			Vector3 vPedForward =  VEC3V_TO_VECTOR3(pPed->GetTransform().GetB());
			vPedForward.z = 0.0f;
			TUNE_GROUP_FLOAT(EXIT_TO_AIM_TUNE, AIM_INTRO_CLOSE_ENOUGH_TOL, 0.02f, 0.0f, 1.0f, 0.01f);
			if (vTargetDir.Dot(vPedForward) >= (1.0f - AIM_INTRO_CLOSE_ENOUGH_TOL))
			{
				bShouldInterrupt = true;
			}		
		}
		else
		{
			bShouldInterrupt = true;
		}
	}

	//ProcessLookIK		
	if (pPed->IsLocalPlayer())
		pPed->GetIkManager().LookAt(0, NULL, 1000, BONETAG_INVALID, NULL, LF_USE_CAMERA_FOCUS, 500, 500, CIkManager::IK_LOOKAT_HIGH);
	else
	{
		CPed* pCurrentTarget = pPed->GetPedIntelligence()->GetCurrentTarget();
		if (pCurrentTarget)
		{
			pPed->GetIkManager().LookAt(0, pCurrentTarget, 1000, BONETAG_INVALID, NULL, LF_WHILE_NOT_IN_FOV, 500, 500, CIkManager::IK_LOOKAT_HIGH);
		}
	}

	if (bShouldInterrupt || m_bMoveNetworkFinished)
	{
		if (pPed->GetIsInVehicle())
		{
			pPed->SetPedOutOfVehicle(CPed::PVF_ExcludeVehicleFromSafetyCheck|CPed::PVF_IgnoreSafetyPositionCheck|CPed::PVF_DontDestroyWeaponObject);
		}

		pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Aiming);
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	ProcessOpenDoor(false);

	if (m_fUpperBodyAimPhase > 0.0f && fDominantClipPhase > m_fUpperBodyAimPhase)
	{
		m_pParentNetworkHelper->SendRequest(ms_AimRequest);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicleSeat::ExitToAim_OnExit()
{
	CPed& rPed = *GetPed();

	fragInstNMGta* pRagdollInst = rPed.GetRagdollInst();
	if (pRagdollInst != NULL)
	{
		// Reset the incoming anim velocity scale
		pRagdollInst->SetIncomingAnimVelocityScaleReset(-1.0f);

		taskWarningf("CTaskExitVehicleSeat::ExitToAim_OnExit: Reset initial ragdoll velocity");
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

float CTaskExitVehicleSeat::CalculateAimDirection()
{
	CPed* pPed = GetPed();
	CTask* pParentTask = GetParent();
	if (pParentTask && pParentTask->GetTaskType() == CTaskTypes::TASK_EXIT_VEHICLE && static_cast<CTaskExitVehicle*>(pParentTask)->GetDesiredTarget())
	{
		Vec3V targetPos = static_cast<CTaskExitVehicle*>(pParentTask)->GetDesiredTarget()->GetMatrix().d();
		pPed->SetDesiredHeading(RC_VECTOR3(targetPos));
	}

#if __DEV
	TUNE_GROUP_BOOL(EXIT_TO_AIM_TUNE, USE_OVERRIDEN_HEADING, false);
	TUNE_GROUP_FLOAT(EXIT_TO_AIM_TUNE, OVERRIDE_HEADING, 0.0f, -3.14f, 3.14f, 0.01f);
	if (USE_OVERRIDEN_HEADING)
	{
		CPed* pPed = GetPed();
		pPed->SetDesiredHeading(OVERRIDE_HEADING);
	}
#endif // __DEV

	const float fOriginalHeading = fwAngle::LimitRadianAngleSafe(m_fAimBlendReferenceHeading);
	const float fDesiredHeading = fwAngle::LimitRadianAngleSafe(pPed->GetDesiredHeading());

	float fHeadingDelta = SubtractAngleShorter(fDesiredHeading, fOriginalHeading);
	if((m_iSeatIndex % 2) != 0)
	{
		fHeadingDelta = fwAngle::LimitRadianAngleSafe(fHeadingDelta + PI);
	}
	fHeadingDelta = Clamp( fHeadingDelta/PI, -1.0f, 1.0f);

	float fAimDirection = ((-fHeadingDelta * 0.5f) + 0.5f);
	if(m_fLastAimDirection > 1.0f)
	{
		m_fLastAimDirection = fAimDirection;
	}
	else if( (m_fLastAimDirection < .25f && fAimDirection > .75f) || (m_fLastAimDirection > .75f && fAimDirection < .25f) )
	{
		return m_fLastAimDirection;
	}

	m_fLastAimDirection = fAimDirection;
	return fAimDirection;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicleSeat::IsPedInCombatOrFleeing(const CPed& ped) const
{
	if (ped.GetPedIntelligence()->FindTaskPrimaryByType(CTaskTypes::TASK_COMBAT) ||
		ped.GetPedIntelligence()->FindTaskPrimaryByType(CTaskTypes::TASK_SMART_FLEE))
	{
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

s32 CTaskExitVehicleSeat::DecideInitialState()
{
#if __BANK
	//Check if we should force jump out of seat.
	TUNE_GROUP_BOOL(TASK_EXIT_VEHICLE_SEAT, bForceJumpOutOfSeat, false);
	if(bForceJumpOutOfSeat)
	{
		return State_JumpOutOfSeat;
	}
#endif
	
	TUNE_GROUP_FLOAT(EXIT_VEHICLE_TUNE, MIN_PITCH_TO_FORCE_JUMP_OUT, 0.85f, 0.0f, 3.0f, 0.01f);
	TUNE_GROUP_FLOAT(EXIT_VEHICLE_TUNE, MAX_PITCH_TO_FORCE_JUMP_OUT, 2.3f, 0.0f, 3.0f, 0.01f);
	const float fPitch = Abs(m_pVehicle->GetTransform().GetPitch());
	if (!m_bIsRappel && fPitch > MIN_PITCH_TO_FORCE_JUMP_OUT && fPitch < MAX_PITCH_TO_FORCE_JUMP_OUT)
	{
		return State_JumpOutOfSeat;
	}

	// If we're exiting onto a lower ground, we need to take into account this
	Vector3 vTargetPosition;
	Quaternion qTargetOrientation;
	CModelSeatInfo::CalculateEntrySituation(m_pVehicle, GetPed(), vTargetPosition, qTargetOrientation, m_iExitPointIndex);
	m_PlayAttachedClipHelper.SetTarget(vTargetPosition, qTargetOrientation);
	const Vector3 vVehPos = VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().GetPosition());
	const bool bIsInAir = m_pVehicle->IsInAir() && static_cast<CTaskExitVehicle*>(GetParent())->CheckForInAirExit(vVehPos);
	const bool bIsInWater = m_pVehicle->m_nVehicleFlags.bIsDrowning;
	const CVehicleModelInfo* pModelInfo = m_pVehicle->GetVehicleModelInfo();
	const bool bIsBoatWithRearActivities = pModelInfo ? m_pVehicle->InheritsFromBoat() && pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_REAR_SEAT_ACTIVITIES) : false;

	if (m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::ThroughWindscreen))
	{
		return State_ThroughWindscreen;
	}
	else if (!bIsInWater && !bIsInAir && m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::VehicleIsOnSide))
	{
		return State_OnSideExit;
	}
	else if (!bIsInAir && m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::VehicleIsUpsideDown))
	{
		return State_UpsideDownExit;
	}
	else if (m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::BeJacked))
	{
		if (m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::IsStreamedExit))
		{
			if(GetPed()->IsNetworkClone())
			{
				return State_WaitForStreamedBeJacked;
			}
			else
			{
				return State_StreamedBeJacked;
			}
		}
		else
		{
			return State_BeJacked;
		}
	}
	else if (m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::IsStreamedExit) && 
		((m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::IsTransitionToSkyDive) && GetPed()->IsNetworkClone())
		|| CheckStreamedExitConditions()))
	{
		if(GetPed()->IsNetworkClone())
		{
			return State_WaitForStreamedExit;
		}
		else
		{
			return State_StreamedExit;
		}
	}
	else if (!m_bIsRappel && ((m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::JumpOut) && !bIsBoatWithRearActivities)
		|| m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::InAirExit)
		|| m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::ExitUnderWater)
		|| m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::DeadFallOut)
		|| ShouldBeConsideredUnderWater()))
	{
		return State_JumpOutOfSeat;
	}
	else if (IsRunningFlagSet(CVehicleEnterExitFlags::IsFleeExit))
	{
		fwMvClipSetId clipsetId = CTaskExitVehicleSeat::GetExitClipsetId(m_pVehicle, m_iExitPointIndex, m_iRunningFlags, GetPed());
		fwMvClipId clipId = ms_Tunables.m_DefaultFleeExitClipId;
		const crClip* pClip = fwClipSetManager::GetClip(clipsetId, clipId);
		if (pClip)
		{
			return State_FleeExit;
		}
		else
		{
			static_cast<CTaskExitVehicle*>(GetParent())->ClearRunningFlag(CVehicleEnterExitFlags::IsFleeExit);
			return State_ExitSeat;
		}
	}
	else
	{
#if __DEV
		TUNE_GROUP_BOOL(EXIT_TO_AIM_TUNE, FORCE_EXIT_TO_AIM, false);
		if (FORCE_EXIT_TO_AIM)
		{
			m_iRunningFlags.BitSet().Set(CVehicleEnterExitFlags::ExitToAim);
		}
#endif // __DEV
		if(m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::ExitToAim))
		{
			return State_ExitToAim;
		}
		else
		{
			return State_ExitSeat;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicleSeat::ProcessWeaponAttachPoint()
{
	//Note that we have processed the weapon attach point.
	m_bHasProcessedWeaponAttachPoint = true;

	//Grab the ped.
	CPed* pPed = GetPed();
	
	//Ensure the weapon manager is valid.
	CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
	if(!pWeaponManager)
	{
		return;
	}

	//Ensure the equipped weapon object is valid.
	if(!pWeaponManager->GetEquippedWeaponObject())
	{
		return;
	}

	//Ensure the weapon is equipped in the left hand.
	if(pWeaponManager->GetEquippedWeaponAttachPoint() != CPedEquippedWeapon::AP_LeftHand)
	{
		return;
	}

	//Note: Equipped weapons always need to be in the right hand, after peds exit their vehicle.
	//		All animations are currently set up with the right hand considered to be the attach bone.
	//		All exit seat animations that originate from a seat with a weapon equipped in the left hand
	//		are expected to be set up so the attachment can be switched at any point during the animation.

	//Switch the attachment to the right hand.
	pWeaponManager->GetPedEquippedWeapon()->AttachObject(CPedEquippedWeapon::AP_RightHand);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicleSeat::ShouldProcessWeaponAttachPoint() const
{
	//Ensure we have not already processed the weapon attach point.
	if(m_bHasProcessedWeaponAttachPoint)
	{
		return false;
	}

	//Ensure we are about to play an exit animation.
	//All states that do not result in some kind of exit animation playing should be listed here.
	//The only exception is the finish state, where the attachment should be processed as a last resort.
	switch(GetState())
	{
		case State_StreamAssets:
		case State_DetachedFromVehicle:
		case State_WaitForStreamedBeJacked:
		case State_WaitForStreamedExit:
		{
			return false;
		}
		default:
		{
			break;
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicleSeat::ApplyDamageThroughWindscreen()
{
	CPed* pPed = GetPed();
	if (pPed)
	{
		const u32 weaponHash = WEAPONTYPE_RAMMEDBYVEHICLE;	// Can't use fall due to attach check in eventdamage.cpp

		//Create the damage event.
		CEventDamage tempDamageEvent(pPed, fwTimer::GetTimeInMilliseconds(), weaponHash);

		//Apply the damage.
		CEntity* inflictor = m_pVehicle;

		//If the vehicle we are inside has valid collision information with another vehicle that is 
		// being driven by a player then use that vehicle has the inflictor.
		if (m_pVehicle)
		{
			u32 timeSinceLastDamage = fwTimer::GetTimeInMilliseconds() - m_pVehicle->GetWeaponDamagedTime();
			if (timeSinceLastDamage < 1000)
			{
				CEntity* damageEntity = m_pVehicle->GetWeaponDamageEntity();
				if (damageEntity && damageEntity->GetIsTypeVehicle())
				{
					CVehicle* damagerVehicle = static_cast< CVehicle* >(damageEntity);
					if (damagerVehicle->GetDriver() && damagerVehicle->GetDriver()->IsPlayer())
					{
						inflictor = damagerVehicle;
					}
				}
			}
		}
#if __DEV
		else if (pPed->IsPlayer() && pPed->GetNetworkObject() && NetworkInterface::IsGameInProgress())
		{
			aiAssertf(0, "NULL m_pVehicle pointer for player %s", pPed->GetNetworkObject()->GetLogName());
		}
#endif // __DEV

		const float fDamageToApply = pPed->IsLocalPlayer() ? ms_Tunables.m_ThroughWindscreenDamagePlayer : ms_Tunables.m_ThroughWindscreenDamageAi;
		CPedDamageCalculator damageCalculator(inflictor, fDamageToApply, weaponHash, -1, false);
		damageCalculator.ApplyDamageAndComputeResponse(pPed, tempDamageEvent.GetDamageResponseData(), CPedDamageCalculator::DF_None);
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicleSeat::RequestClips(s32 iDesiredState)
{
	bool bCommonClipSetConditionPassed = true;
	if (iDesiredState == State_UpsideDownExit || iDesiredState == State_OnSideExit)
	{
		bCommonClipSetConditionPassed = false;
	}

	if (!bCommonClipSetConditionPassed)
	{
		fwMvClipSetId commonClipsetId = CTaskEnterVehicleAlign::GetCommonClipsetId(m_pVehicle, m_iExitPointIndex);
		bCommonClipSetConditionPassed = CTaskVehicleFSM::RequestVehicleClipSetReturnIsLoaded(commonClipsetId, SP_High);
#if __BANK
		CTaskVehicleFSM::SpewStreamingDebugToTTY(*GetPed(), *m_pVehicle, m_iExitPointIndex, commonClipsetId, GetTimeInState());
#endif // __BANK
	}

	fwMvClipSetId exitClipsetId = GetExitClipsetId(m_pVehicle, m_iExitPointIndex, m_iRunningFlags, GetPed());
	bool bExitClipSetConditionPassed = CTaskVehicleFSM::RequestVehicleClipSetReturnIsLoaded(exitClipsetId, SP_High);
#if __BANK
	CTaskVehicleFSM::SpewStreamingDebugToTTY(*GetPed(), *m_pVehicle, m_iExitPointIndex, exitClipsetId, GetTimeInState());
#endif // __BANK

	return bExitClipSetConditionPassed && bCommonClipSetConditionPassed;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicleSeat::SetUpRagdollOnCollision(CPed& rPed, CVehicle& rVeh)
{
	// If the vehicle is on moving ground then it makes things too complicated to allow the ragdoll to activate based on collision
	// The ped ends up getting attached to the bike which forces the ped capsule to deactivate which stops the capsule's position from being updated each physics frame
	// which stops the ragdoll bounds from being updated each physics frame which will cause problems on moving ground (large contact depths since the moving ground and
	// ragdoll bounds will be out-of-sync)
	// Best to just not allow activating the ragdoll based on collision
	if (!rPed.IsNetworkClone() && rVeh.GetReferenceFrameVelocity().Mag2() < square(1.0f))
	{
		atHashString entryAnimInfoHash; 
		if (m_pVehicle->IsEntryIndexValid(m_iExitPointIndex))
		{
			const CVehicleEntryPointAnimInfo* pAnimInfo = m_pVehicle->GetEntryAnimInfo(m_iExitPointIndex);
			if (pAnimInfo)
			{
				entryAnimInfoHash = pAnimInfo->GetName();
			}
		}

		CTaskExitVehicle* pExitVehicleTask = static_cast<CTaskExitVehicle*>(GetParent());
		CPed* pPedJackingMe = pExitVehicleTask->GetPedsJacker();

		if (GetState()==State_BeJacked || (GetState()==State_StreamedBeJacked && pPedJackingMe))
		{
			// need to add the jacked from vehicle event here, so the ped reacts correctly and 
			// does the correct flee getup.
			bool bForceFlee = IsRunningFlagSet(CVehicleEnterExitFlags::ForceFleeAfterJack);
			CEventDraggedOutCar event(m_pVehicle, pPedJackingMe, m_iSeatIndex == 0 ? true : false, bForceFlee);
			event.SetSwitchToNM(true);
			rPed.SetActivateRagdollOnCollisionEvent(event);
		}
		else
		{
			CTask* pTaskNM = rage_new CTaskNMJumpRollFromRoadVehicle(1000, 10000, false, false, entryAnimInfoHash, m_pVehicle);
			CEventSwitch2NM event(10000, pTaskNM);
			rPed.SetActivateRagdollOnCollisionEvent(event);
		}

		rPed.SetActivateRagdollOnCollision(true);
		rPed.SetRagdollOnCollisionIgnorePhysical(&rVeh);
		rPed.SetNoCollisionEntity(&rVeh);
		TUNE_GROUP_FLOAT(RAGDOLL_ON_EXIT_TUNE, ALLOWED_SLOPE, -0.55f, -1.5f, 1.5f, 0.01f);
		rPed.SetRagdollOnCollisionAllowedSlope(ALLOWED_SLOPE);
		TUNE_GROUP_FLOAT(RAGDOLL_ON_EXIT_TUNE, ALLOWED_PENTRATION, 0.1f, 0.0f, 1.0f, 0.01f);
		rPed.SetRagdollOnCollisionAllowedPenetration(GetState() == State_JumpOutOfSeat ? 0.0f : ALLOWED_PENTRATION);

		// Exclude shins on coquette2 when exiting, vehicle is ridiculously low
		static const u32 COQUETTE2_LAYOUT_HASH = ATSTRINGHASH("LAYOUT_STD_COQUETTE2", 0x5b24e188);
		if (m_pVehicle->GetLayoutInfo()->GetName().GetHash() == COQUETTE2_LAYOUT_HASH)
		{
			rPed.SetRagdollOnCollisionAllowedPartsMask(0xFFFFFFFF & ~(BIT(RAGDOLL_HAND_LEFT) | BIT(RAGDOLL_HAND_RIGHT) | BIT(RAGDOLL_FOOT_LEFT) | BIT(RAGDOLL_FOOT_RIGHT) | BIT(RAGDOLL_SHIN_LEFT) | BIT(RAGDOLL_SHIN_RIGHT)));
		}
		else
		{
			rPed.SetRagdollOnCollisionAllowedPartsMask(0xFFFFFFFF & ~(BIT(RAGDOLL_HAND_LEFT) | BIT(RAGDOLL_HAND_RIGHT) | BIT(RAGDOLL_FOOT_LEFT) | BIT(RAGDOLL_FOOT_RIGHT)));
		}
		
		rPed.GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_NEVER_ACTIVATE, false);

		if(rPed.IsLocalPlayer())
		{
			// This type of ejection can cause ragdoll instability, so instruct the inst
			// to try to prevent it vie temporary increased stiffness and increased solver iterations.
			rPed.GetRagdollInst()->GetCacheEntry()->ActivateInstabilityPrevention();
		}
	}
}

void CTaskExitVehicleSeat::ResetRagdollOnCollision(CPed& rPed)
{
	rPed.SetNoCollisionEntity(NULL);
	rPed.SetActivateRagdollOnCollision(false);
	rPed.SetRagdollOnCollisionIgnorePhysical(NULL);
	rPed.ClearActivateRagdollOnCollisionEvent();

	// B*1814957: Prevent peds from colliding with the VOLTIC when they are dead and have just been pulled out of the car.
	// Limbs were getting stuck inside when they were pulled from the car, which was causing the car to then act erratically.
	// The no collision entity remains set for 10 frames, see CPed::SetPedOutOfVehicle for details.
	if(rPed.ShouldBeDead() && m_pVehicle && m_pVehicle->GetModelIndex() == MI_CAR_VOLTIC)
		rPed.SetNoCollisionEntity(m_pVehicle);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicleSeat::ControlledSetPedOutOfVehicle(u32 nFlags)
{
	CPed* pPed = GetPed();

	if (!pPed->IsNetworkClone())
	{
		// Release the component reservation, we're done with it
		const s32 iEntryIndex = m_pVehicle->GetDirectEntryPointIndexForPed(pPed);
		if (m_pVehicle->IsEntryIndexValid(iEntryIndex))
		{	
			CComponentReservation* pComponentReservation = m_pVehicle->GetComponentReservationMgr()->GetSeatReservation(iEntryIndex, SA_directAccessSeat);
			if (pComponentReservation && pComponentReservation->IsComponentReservedForPed(pPed))
			{
				CComponentReservationManager::UnreserveVehicleComponent(pPed, pComponentReservation);
			}
		}
	}
	pPed->SetPedOutOfVehicle(nFlags);
	const bool bShouldResetRagdollOnCollision = m_pVehicle->GetIsAquatic() || (GetState() == State_ExitSeat && m_pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR);
	if(bShouldResetRagdollOnCollision)
	{
		ResetRagdollOnCollision(*pPed);
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicleSeat::SendMoveSignalsForExitToAim()
{
	CPed* pPed = GetPed();

	// Get the direction we are trying to blend to 
	float fTemp = CalculateAimDirection();
	CTaskMotionBase::InterpValue(m_fAimBlendCurrentHeading, fTemp, 1.5f);

	TUNE_BOOL(FORCE_BLEND_HEADING, false);
	TUNE_FLOAT(FORCED_HEADING, 0.5f, 0.0f, 1.0f, 0.1f);
	if (FORCE_BLEND_HEADING)
	{
		m_fAimBlendCurrentHeading = FORCED_HEADING;
	}
	m_pParentNetworkHelper->SetFloat(ms_AimBlendDirectionId, m_fAimBlendCurrentHeading);

	// Calculate the pitch we want to be aiming at
	const CWeapon* pWeapon = GetPed()->GetWeaponManager()->GetEquippedWeapon();
	if (pWeapon && pWeapon->GetWeaponInfo())
	{
		const CAimingInfo* pAimingInfo = pWeapon->GetWeaponInfo()->GetAimingInfo();
		if(!pAimingInfo)
		{
			return false;
		}

		// Get the sweep ranges from the weapon info
		const float fSweepPitchMin = pAimingInfo->GetSweepPitchMin() * DtoR;
		const float fSweepPitchMax = pAimingInfo->GetSweepPitchMax() * DtoR;

		// Calculate the aim vector (this determines the heading and pitch angles to point the clips in the correct direction)
		CTask* pParentTask = GetParent();

		CWeaponTarget target;
		Vector3 vEnd(VEC3_ZERO);
		if (pParentTask && pParentTask->GetTaskType() == CTaskTypes::TASK_EXIT_VEHICLE && static_cast<CTaskExitVehicle*>(pParentTask)->GetDesiredTarget())
		{
			target.SetEntity(static_cast<CTaskExitVehicle*>(pParentTask)->GetDesiredTarget());
		}
		else
		{
			target.SetPosition(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) + VEC3V_TO_VECTOR3(pPed->GetTransform().GetB()));
		}

		target.GetPositionWithFiringOffsets(pPed, vEnd);

		Matrix34 m;
		pPed->GetGlobalMtx(pPed->GetBoneIndexFromBoneTag(BONETAG_R_UPPERARM), m);
		Vector3 vStart = m.d;		

		// Compute desired pitch angle value
		float fDesiredPitch = CTaskAimGun::CalculateDesiredPitch(pPed, vStart, vEnd);

		// Map the angle to the range 0.0->1.0
		m_pParentNetworkHelper->SetFloat(ms_AimPitchId, CTaskAimGun::ConvertRadianToSignal(fDesiredPitch, fSweepPitchMin, fSweepPitchMax, false));
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicleSeat::RequestProcessPhysics()
{
	GetPed()->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );
	m_bProcessPhysicsCalledThisFrame = false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicleSeat::ShouldBeConsideredUnderWater()
{
	CPed& rPed = *GetPed();

	if (!m_pVehicle->GetIsAquatic())
	{
		if (m_pVehicle->m_nVehicleFlags.bIsDrowning && rPed.GetPedResetFlag(CPED_RESET_FLAG_IsDrowning))
		{
			return true;
		}

		Vector3 vEntryPointPos(Vector3::ZeroType);
		Quaternion qEntryOrientation(Quaternion::IdentityType);
		CModelSeatInfo::CalculateEntrySituation(m_pVehicle, GetPed(), vEntryPointPos, qEntryOrientation, m_iExitPointIndex);	
		TUNE_GROUP_FLOAT(EXIT_VEHICLE_TUNE, WATER_EXIT_JUMP_GROUND_PROBE_LENGTH, 1.5f, 0.0f, 3.0f, 0.01f);
		const bool bProbeClear = CTaskExitVehicle::IsVerticalProbeClear(vEntryPointPos, WATER_EXIT_JUMP_GROUND_PROBE_LENGTH, rPed);

		// Check the water height against the ped, if the ped isn't completely submerged don't consider them to be underwater
		TUNE_GROUP_FLOAT (EXIT_VEHICLE_TUNE, CONSIDER_UNDERWATER_PROBE_LENGTH, 5.0f, 0.0f, 10.0f, 0.0001f);
		TUNE_GROUP_FLOAT (EXIT_VEHICLE_TUNE, CONSIDER_UNDERWATER_DEPTH_REQUIRED, 0.5f, 0.0f, 10.0f, 0.0001f);
		Vector3 vTestPos = VEC3V_TO_VECTOR3(rPed.GetTransform().GetPosition()) + Vector3(0.0f, 0.0f, 1.0f);
		float fWaterHeight;
		const bool bWaterUnderPed = CheckForWaterUnderPed(&rPed, m_pVehicle, CONSIDER_UNDERWATER_PROBE_LENGTH, CONSIDER_UNDERWATER_DEPTH_REQUIRED, fWaterHeight, true, &vTestPos);
		if (bWaterUnderPed)
		{
			TUNE_GROUP_FLOAT (EXIT_VEHICLE_TUNE, CONSIDER_UNDERWATER_MAX_HEIGHT_OVER_WATER, -0.25f, -10.0f, 10.0f, 0.0001f);
			const float	fHeightOverWater = rPed.GetTransform().GetPosition().GetZf() - fWaterHeight;
			if (fHeightOverWater > CONSIDER_UNDERWATER_MAX_HEIGHT_OVER_WATER)
			{
				return (m_pVehicle->pHandling && m_pVehicle->pHandling->GetSeaPlaneHandlingData()) ? false : bProbeClear;
			}
			else
			{
				return true;
			}
		}
		return (m_pVehicle->m_nVehicleFlags.bIsDrowning || rPed.GetPedResetFlag(CPED_RESET_FLAG_IsDrowning)) && bProbeClear;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicleSeat::IsStandingOnVehicle(const CPed& rPed) const
{
	if (m_pVehicle && CVehicle::GetVehicleOrientation(*m_pVehicle) == CVehicle::VO_OnSide)
	{
		TUNE_GROUP_FLOAT(ON_SIDE_EXIT, ON_SIDE_Z_VALUE, 0.6f, -1.0f, 1.0f, 0.01f);
		const float fPedUpAxisWorldZ = rPed.GetTransform().GetC().GetZf();
		if (Abs(fPedUpAxisWorldZ) > ON_SIDE_Z_VALUE)
		{
			TUNE_GROUP_FLOAT(ON_SIDE_EXIT, PROBE_LENGTH, 1.25f, 0.0f, 2.0f, 0.01f);
			const Vector3 vStartPos = VEC3V_TO_VECTOR3(rPed.GetTransform().GetPosition());
			const Vector3 vEndPos(vStartPos.x, vStartPos.y, vStartPos.z - PROBE_LENGTH);

			WorldProbe::CShapeTestFixedResults<> probeResults;
			WorldProbe::CShapeTestProbeDesc probeDesc;
			probeDesc.SetResultsStructure(&probeResults);
			probeDesc.SetStartAndEnd(vStartPos, vEndPos);
			probeDesc.SetIncludeEntity(m_pVehicle);

			if (!WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
			{
	#if __BANK
				ms_debugDraw.AddLine(RCC_VEC3V(vStartPos), RCC_VEC3V(vEndPos), Color_green, 1000);
	#endif
				return false;
			}

	#if __BANK
			ms_debugDraw.AddLine(RCC_VEC3V(vStartPos), RCC_VEC3V(probeResults[0].GetHitPosition()), Color_red, 1000);
	#endif
		}
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicleSeat::TurnTurretForBeingJacked(CPed& rPed, CPed* pJacker)
{
	if (m_bHasTurnedTurretDueToBeingJacked)
		return;

	if (!pJacker)
		return;

	TUNE_GROUP_FLOAT(TURRET_TUNE, MIN_TIME_IN_STATE_BEFORE_TURNING_TURRET_FOR_BE_JACKED, 0.1f, 0.0f, 1.0f, 0.01f);
	if (GetTimeInState() < MIN_TIME_IN_STATE_BEFORE_TURNING_TURRET_FOR_BE_JACKED)
		return;

	if (m_pVehicle->IsTurretSeat(m_iSeatIndex) && m_pVehicle->GetSeatAnimationInfo(m_iSeatIndex)->IsStandingTurretSeat() && rPed.GetCarJacker() && m_pVehicle->GetVehicleWeaponMgr())
	{
		CTurret* pTurret = m_pVehicle->GetVehicleWeaponMgr()->GetFirstTurretForSeat(m_iSeatIndex);
		if (pTurret)
		{
			const CTaskEnterVehicle* pEnterVehicleTask = static_cast<const CTaskEnterVehicle*>(pJacker->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
			if (pEnterVehicleTask && !pEnterVehicleTask->IsOnVehicleEntry())
			{
				// Determine jacker approach angle relative to vehicle
				Matrix34 turretMtx;
				pTurret->GetTurretMatrixWorld(turretMtx, m_pVehicle);

				Vector3 vJackerPos = VEC3V_TO_VECTOR3(rPed.GetCarJacker()->GetTransform().GetPosition());
				Vector3 vTurretPos = turretMtx.d;
				Vector3 vToTurret = vTurretPos - vJackerPos;
				vToTurret.Normalize();
				Vector3 vVehFwd = VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().GetB());

				vToTurret = VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().UnTransform3x3(RCC_VEC3V(vToTurret)));
				vVehFwd = VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().UnTransform3x3(RCC_VEC3V(vVehFwd)));	// 0,1,0?

				float fDesiredLocalAngle = -vToTurret.AngleZ(vVehFwd);
				//aiDisplayf("Frame: %i, Desired local %.2f", fwTimer::GetFrameCount(), fDesiredLocalAngle);

				Vector3 vDebugEulers;

				Quaternion qDesiredAimAngle;
				qDesiredAimAngle.FromEulers(Vector3(0.0f, 0.0f, fDesiredLocalAngle));

				pTurret->SetDesiredAngle(qDesiredAimAngle, m_pVehicle);
				m_bHasTurnedTurretDueToBeingJacked = true;
			}
		}
	}
}

bool CTaskExitVehicleSeat::DoesClonePedWantToInterruptExitSeatAnim(const CPed& rPed)
{
	aiAssert(rPed.IsNetworkClone());

	// if the ped has finished running the task on the owner machine it may have interrupted the task - 
	// check how long is left to play on the anim to decide whether to interrupt or not
	if (!rPed.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE_SEAT))
	{
		float fClipPhase = 0.0f;
		const crClip* pClip = GetClipAndPhaseForState(fClipPhase);

		if(pClip)
		{
			float fTimeLeft = pClip->ConvertPhaseToTime(1.0f - fClipPhase);

			if(m_fExitSeatClipPlaybackRate > 0.0f)
			{
				fTimeLeft /= m_fExitSeatClipPlaybackRate;
			}

			TUNE_FLOAT(CLONE_EXIT_SEAT_INTERRUPT_THRESHOLD, 0.3f, 0.0f, 5.0f, 0.1f);

			if(fTimeLeft > CLONE_EXIT_SEAT_INTERRUPT_THRESHOLD)
			{
				AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED_REF(rPed, "[ExitVehicle] Clone ped %s is interrupting exit seat anim in %s\n", AILogging::GetDynamicEntityNameSafe(&rPed), GetStaticStateName(GetState()));
				return true;
			}
		}
	}	
	return false;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicleSeat::ThroughWindscreen_OnEnter()
{
	CPed* pPed = GetPed();
	if (pPed)
	{
		if (pPed->IsLocalPlayer())
		{
			StatsInterface::IncrementStat(StatsInterface::GetStatsModelHashId("THROWNTHROUGH_WINDSCREEN"), 1.0f);
		}
		
		if (!pPed->IsNetworkClone())
		{
			ApplyDamageThroughWindscreen();
		}
	}

	m_pVehicle->SmashWindow(VEH_WINDSCREEN, VEHICLEGLASSFORCE_KICK_ELBOW_WINDOW);
	m_iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
	SetMoveNetworkState(ms_ThroughWindscreenRequestId, ms_ThroughWindscreenOnEnterId, ms_ThroughWindscreenStateId);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicleSeat::ThroughWindscreen_OnUpdate()
{
	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	CPed* pPed = GetPed();

	float fEventPhase = 1.0f;
	float fClipPhase = 1.0f;
	const crClip* pClip = GetClipAndPhaseForState(fClipPhase);

		if (CClipEventTags::FindEventPhase(pClip, CClipEventTags::CanSwitchToNm, fEventPhase))
		{
			if (fClipPhase >= fEventPhase)
			{
				// Detach when the clip reaches a certain point
				taskAssertf(!m_bExitedSeat, "Ped set out of vehicle before switching to nm, check the detach tags. ClipSet:%s, Clip:%s",
					(m_pParentNetworkHelper && m_pParentNetworkHelper->GetClipSet()) ? m_pParentNetworkHelper->GetClipSet()->GetClipDictionaryName().TryGetCStr(): "NO CLIP SET",
					pClip ? pClip->GetName() : "NULL"
					);
				//pPed->SetNoCollision(m_pVehicle, NO_COLLISION_RESET_WHEN_NO_IMPACTS);
				CheckAndFixIfPedWentThroughSomething();
				SetTaskState(State_DetachedFromVehicle);
				return FSM_Continue;
			}
		}
		else
		{
			TUNE_GROUP_FLOAT(EXIT_VEHICLE_TUNE, DEFAULT_THROUGH_WINDSCREEN_PHASE, 0.2f, 0.0f, 1.0f, 0.01f);
			if (fClipPhase >= DEFAULT_THROUGH_WINDSCREEN_PHASE)
			{
				// Detach when the clip reaches a certain point
				taskAssertf(!m_bExitedSeat, "Ped set out of vehicle before switching to nm, check the detach tags. ClipSet:%s, Clip:%s",
					(m_pParentNetworkHelper && m_pParentNetworkHelper->GetClipSet()) ? m_pParentNetworkHelper->GetClipSet()->GetClipDictionaryName().TryGetCStr(): "NO CLIP SET",
					pClip ? pClip->GetName() : "NULL"
					);
				//pPed->SetNoCollision(m_pVehicle, NO_COLLISION_RESET_WHEN_NO_IMPACTS);
				CheckAndFixIfPedWentThroughSomething();
				SetTaskState(State_DetachedFromVehicle);
				return FSM_Continue;
			}
		}

	if (IsMoveNetworkStateFinished(ms_ThroughWindscreenClipFinishedId, ms_ThroughWindscreenPhaseId))
	{
		pPed->SetPedOutOfVehicle(CPed::PVF_ExcludeVehicleFromSafetyCheck);
		SetState(State_Finish);
		return FSM_Continue;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicleSeat::Finish_OnUpdate()
{
    CPed *pPed = GetPed();

    if (!pPed->IsNetworkClone() && m_bRagdollEventGiven && !pPed->IsNetworkClone() && CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_VEHICLE_JUMPOUT))
	{
		// Should be getting a ragdoll event;
		if (GetTimeInState() >= 1.0f)
		{
#if __ASSERT
			pPed->SpewRagdollTaskInfo();
			taskAssertf(0, "Ped 0x%p failed to complete ragdoll detach properly", pPed);
#endif //__ASSERT
			return FSM_Quit;
		}
		return FSM_Continue;
	}

	return FSM_Quit;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicleSeat::GetIsHighInAir(CPed* pPed)
{
	// Look for the ground under the peds root

	Mat34V temp(pPed->GetMatrix());
	Matrix34 matrix = RC_MATRIX34(temp);

	TUNE_GROUP_FLOAT (EXIT_VEHICLE_TUNE, fGroundSearchZOffsetForHighFall, 10.0f, 0.0f, 10000.0f, 0.0001f);

	WorldProbe::CShapeTestFixedResults<> probeResult;
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetStartAndEnd(matrix.d, matrix.d - Vector3(0.0f, 0.0f, fGroundSearchZOffsetForHighFall));
	probeDesc.SetResultsStructure(&probeResult);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_VEHICLE_TYPE);
	probeDesc.SetExcludeEntity(pPed, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
	{
		return false;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicleSeat::CheckForWaterUnderPed(const CPed* pPed, const CVehicle *pVehicle, float fProbeLength, float fWaterDepth, float &fWaterHeightOut, bool bConsiderWaves, Vector3 *pvOverridePos)
{
	// Look for the water under the peds root

	Vector3 vStart;
	if(pvOverridePos)
	{
		vStart = *pvOverridePos;
	}
	else
	{
		Mat34V temp(pPed->GetMatrix());
		Matrix34 matrix = RC_MATRIX34(temp);
		vStart = matrix.d;
	}

	float fFallDistance = fProbeLength;
	float fFallHeight = vStart.z - fFallDistance;
	bool bInLargeVehicle = pVehicle && (pVehicle->GetSeatManager()->GetMaxSeats() > 8);

	const CEntity* exclusionList[2];
	int nExclusions = 0;
	exclusionList[nExclusions++] = pPed;
	exclusionList[nExclusions++] = pVehicle;

	WorldProbe::CShapeTestFixedResults<> probeResult;
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetStartAndEnd(vStart, vStart - Vector3(0.0f, 0.0f, fFallDistance));
	probeDesc.SetResultsStructure(&probeResult);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_VEHICLE_TYPE);
	probeDesc.SetExcludeEntities(exclusionList, nExclusions, bInLargeVehicle ? WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS : EXCLUDE_ENTITY_OPTIONS_NONE);
	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
	{
		// Didn't hit anything within our probe distance, a long fall
		fFallHeight = probeResult[0].GetHitPosition().z;
	}

	//Check for water beneath the ped. Note: If we are overriding test position, we can't use ped's buoyancy helper as the pre-cached tests are done from 
	//the ped position.
	bool bHitWater = false;
	fWaterHeightOut = 0.0f;
	if(pvOverridePos)
	{
		bHitWater = CWaterTestHelper::GetWaterHeightAtPositionIncludingRivers(vStart, &fWaterHeightOut, bConsiderWaves, 999999.9f);
	}
	else
	{
		bHitWater = const_cast<CPed *>(pPed)->m_Buoyancy.GetWaterLevelIncludingRivers(vStart, &fWaterHeightOut, bConsiderWaves, POOL_DEPTH, 999999.9f, NULL) != WATERTEST_TYPE_NONE;
	}

	if(bHitWater)
	{
		if( (fWaterHeightOut - fWaterDepth) > fFallHeight)
		{
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicleSeat::ShouldApplyTargetFixup() const
{
	//! Note: Some boats aren't flagged as in water to prevent playing certain anim variations, so check that here.
	bool bBoatFixup = false;
	if(m_pVehicle->InheritsFromBoat())
	{
		const CBoat* pBoat = static_cast<const CBoat *>(m_pVehicle.Get());
		if(pBoat->m_BoatHandling.IsInWater())
		{
			bBoatFixup = true;
		}
	}

	if (IsRunningFlagSet(CVehicleEnterExitFlags::InWater) || bBoatFixup)
	{
		if (GetState() == State_JumpOutOfSeat)
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	if (m_pVehicle->IsEntryIndexValid(m_iExitPointIndex))
	{
		const CVehicleEntryPointAnimInfo* pAnimInfo = m_pVehicle->GetEntryAnimInfo(m_iExitPointIndex);
		const CVehicleEntryPointInfo* pEntryInfo = m_pVehicle->GetEntryInfo(m_iExitPointIndex);
		bool bIsPlaneHatchEntry = pEntryInfo && pEntryInfo->GetIsPlaneHatchEntry();

		if (pAnimInfo && pAnimInfo->GetUseNewPlaneSystem())
		{
			if (GetState() == State_ExitSeat)
			{
				if (pAnimInfo->GetHasClimbDown())
				{
					return false;
				}
			}
			else
			{
				if (!m_pVehicle->GetLayoutInfo()->GetClimbUpAfterOpenDoor() & !bIsPlaneHatchEntry)
				{
					return false;
				}
			}
		}
	}

	switch (GetState())
	{
		case State_StreamedExit:
		case State_JumpOutOfSeat:
		case State_ThroughWindscreen:
		case State_ExitToAim:
			return false;
		default:
			break;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////

void CTaskExitVehicleSeat::DetachedFromVehicle_OnEnter()
{
	CPed* pPed = GetPed();
	m_bRagdollEventGiven = false;

	// Save the last and current matrix here in case we want to switch to a ragdoll, because those matrices will get 
	// changed in at least three places in this function prior to switching to ragdoll.
	const Mat34V savedLastMat = PHSIM->GetLastInstanceMatrix(pPed->GetRagdollInst());
	const Mat34V savedCurrentMat = pPed->GetRagdollInst()->GetMatrix();
	const Vec3V savedCurrentPos = pPed->GetTransform().GetPosition();

	bool bIsHighInAir = !IsRunningFlagSet(CVehicleEnterExitFlags::InWater) && GetIsHighInAir(pPed);

	u32 uFlags = CPed::PVF_DontNullVelocity;

	//Check if we should use the safety check.
	bool bUseSafetyCheck = false;
	if(!bUseSafetyCheck)
	{
		//Check if we should check if the ped went through something.
		bool bCheck = GetPreviousState() == State_JumpOutOfSeat || GetPreviousState() == State_UpsideDownExit || GetPreviousState() == State_OnSideExit || GetPreviousState() == State_BeJacked || GetPreviousState() == State_StreamedBeJacked;
		if(bCheck && CTaskVehicleFSM::CheckIfPedWentThroughSomething(*pPed, *m_pVehicle))
		{
			bUseSafetyCheck = true;
		}
	}
	if(!bUseSafetyCheck)
	{
		uFlags |= (CPed::PVF_ExcludeVehicleFromSafetyCheck | CPed::PVF_IgnoreSafetyPositionCheck);
	}
	else if (m_pVehicle->GetIsSmallDoubleSidedAccessVehicle())
	{
		// Don't want to include the vehicle for these since theres 'no danger'* of being left stuck inside them
		uFlags |= CPed::PVF_ExcludeVehicleFromSafetyCheck;
	}

	if(bIsHighInAir)
	{
		uFlags |= CPed::PVF_DontSnapMatrixUpright;
	}
	TUNE_GROUP_BOOL(THROUGH_WINDSCREEN_TUNE, SET_NO_COLLISION_ENTITY, true);
	if (SET_NO_COLLISION_ENTITY)
	{
		pPed->SetNoCollisionEntity(m_pVehicle);
	}
	pPed->SetPedOutOfVehicle(uFlags);

	//We can ragdoll if we did not use the safety fixup.
	bool bCanUseRagdoll = !bUseSafetyCheck;
	if(!bCanUseRagdoll)
	{
		//We can ragdoll if we tried to use the safety fixup, but were not actually fixed up.
		if(IsCloseAll(GetPed()->GetTransform().GetPosition(), savedCurrentPos, ScalarVFromF32(SMALL_FLOAT)))
		{
			bCanUseRagdoll = true;
		}
	}

	if (bIsHighInAir)
	{
		CTaskExitVehicle::OnExitMidAir(pPed, m_pVehicle);
	}

	if (taskVerifyf(m_pVehicle->IsSeatIndexValid(m_iSeatIndex), "Invalid seat index %i in detach state, previous state %s", m_iSeatIndex, GetStaticStateName(GetPreviousState())))
	{
		if (GetPreviousState() == State_BeJacked && m_pVehicle->IsEntryIndexValid(m_iExitPointIndex))
		{
			// Store the offsets to the seat situation so we can recalculate the starting situation each frame relative to the seat
			Vector3 vTargetPosition(Vector3::ZeroType);
			Quaternion qTargetOrientation(Quaternion::IdentityType);
			CModelSeatInfo::CalculateSeatSituation(m_pVehicle, vTargetPosition, qTargetOrientation, m_iExitPointIndex);
			pPed->SetDesiredHeading(vTargetPosition);
		}
		else
		{
			if (m_pVehicle->GetTransform().GetC().GetZf() < 0.0f)
			{
				pPed->SetDesiredHeading(fwAngle::LimitRadianAngle(m_pVehicle->GetTransform().GetHeading() + PI));
			}
			else
			{
				pPed->SetDesiredHeading(m_pVehicle->GetTransform().GetHeading());
			}
		}

		if (!m_bPreserveMatrixOnDetach)
			pPed->SetHeading(pPed->GetCurrentHeading());

		TUNE_GROUP_BOOL(THROUGH_WINDSCREEN_TUNE, USE_VEHICLE_VELOCITY, false);
		if (USE_VEHICLE_VELOCITY)
		{
			pPed->SetVelocity(m_pVehicle->GetVelocity());
		}
		else
		{
			TUNE_GROUP_BOOL(THROUGH_WINDSCREEN_TUNE, USE_ANIMATED_VELOCITY, false);
			if (USE_ANIMATED_VELOCITY)
			{
				Vec3V vAnimatedVelocity = VECTOR3_TO_VEC3V(pPed->GetAnimatedVelocity());
				pPed->GetTransform().Transform3x3(vAnimatedVelocity);
				pPed->SetVelocity(VEC3V_TO_VECTOR3(vAnimatedVelocity));
			}
		}
		//pPed->SetNoCollision(m_pVehicle, NO_COLLISION_RESET_WHEN_NO_IMPACTS);

		dev_float SPEED_TO_SAY_SCREAM_AUDIO = 7.5f;
		dev_float SPEED_TO_USE_FALL_TASK_ON_DEATH = 1.5f;

		// Switch the ped to ragdoll
		if (bCanUseRagdoll && CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_VEHICLE_JUMPOUT))
		{
			if (pPed->GetVelocity().Mag2() > rage::square(SPEED_TO_SAY_SCREAM_AUDIO) )
			{
				audSpeechInitParams speechParams;
				speechParams.forcePlay = true;
				speechParams.allowRecentRepeat = true;
				pPed->GetSpeechAudioEntity()->Say("HIGH_FALL", speechParams, ATSTRINGHASH("PAIN_VOICE", 0x048571d8d));
			}

			CEvent* pEvent = NULL;

        	// Restore the current and last matrix saved earlier in this function.  This needs to get called before the switch to ragdoll
			// so that the correct position and incoming velocities can get computed.
			PHSIM->SetLastInstanceMatrix(pPed->GetRagdollInst(), savedLastMat);
			pPed->GetRagdollInst()->SetMatrix(savedCurrentMat);
			
			if(pPed->ShouldBeDead())
			{
				pEvent = rage_new CEventDeath(false, fwTimer::GetTimeInMilliseconds(), true);

				if(m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::ThroughWindscreen))
				{
					((CEventDeath*)pEvent)->SetNaturalMotionTask(rage_new CTaskNMControl(1000, 6000, rage_new CTaskNMThroughWindscreen(1000, 6000, true, m_pVehicle),0));
				}
				else if (bIsHighInAir || (pPed->GetVelocity().Mag2() > rage::square(SPEED_TO_USE_FALL_TASK_ON_DEATH)))
				{
					((CEventDeath*)pEvent)->SetNaturalMotionTask(rage_new CTaskNMControl(1000, 6000, rage_new CTaskNMHighFall(1000),0));
				}
			}
			else
			{
				// Is this actually needed?  We generate a CEventDraggedOutCar event when exiting the CTaskExitVehicle task - which should happen
				// imminently anyway since we're going to be creating a ragdoll event which should supercede our task!
				if(!pPed->IsAPlayerPed() && (GetPreviousState() == State_BeJacked || GetPreviousState() == State_StreamedBeJacked) && 
					GetParent() != NULL && AssertVerify(GetParent()->GetTaskType() == CTaskTypes::TASK_EXIT_VEHICLE))
				{
					CTaskExitVehicle* pExitVehicleTask = static_cast<CTaskExitVehicle*>(GetParent());
					CPed* pPedJackingMe = pExitVehicleTask->GetPedsJacker();
					if (pPedJackingMe)
					{
						// Set the dragged out event as persistent as we need it to kick in after the NM task so that peds don't just wander away
						// after being dragged out!
						bool bForceFlee = pExitVehicleTask->IsRunningFlagSet(CVehicleEnterExitFlags::ForceFleeAfterJack);
						pEvent = rage_new CEventDraggedOutCar(m_pVehicle, pPedJackingMe, m_iSeatIndex == 0 ? true : false, bForceFlee);
						static_cast<CEventDraggedOutCar*>(pEvent)->SetSwitchToNM(true);
					}
				}

				if (!pEvent)
				{
					// Don't want ped trying to roll along the ground if exiting a flying vehicle which isn't in contact with the ground.
					CTaskNMBehaviour* pTaskNM = NULL;
					if(((m_pVehicle->GetVehicleType() == VEHICLE_TYPE_PLANE || m_pVehicle->GetVehicleType() == VEHICLE_TYPE_HELI)
						&& !m_pVehicle->HasContactWheels()) || m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::InAirExit) || bIsHighInAir)
					{
						pTaskNM = rage_new CTaskNMHighFall(1500);
					}
					else if(m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::ThroughWindscreen))
					{
						pTaskNM = rage_new CTaskNMThroughWindscreen(1000, 6000, true, m_pVehicle);
					}
					else
					{
                        if(!pPed->IsNetworkClone() || !pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_NM_JUMP_ROLL_FROM_ROAD_VEHICLE))
                        {
						    bool bIsJack = GetPreviousState() == State_BeJacked;
    						
						    atHashString entryAnimInfoHash; 
						    if (m_pVehicle->IsEntryIndexValid(m_iExitPointIndex))
						    {
							    const CVehicleEntryPointAnimInfo* pAnimInfo = m_pVehicle->GetEntryAnimInfo(m_iExitPointIndex);
							    if (pAnimInfo)
							    {
								    entryAnimInfoHash = pAnimInfo->GetName();
							    }
						    }
    						
						    pTaskNM = rage_new CTaskNMJumpRollFromRoadVehicle( bIsJack ? 150 : 1000, 6000, true, bIsJack, entryAnimInfoHash, m_pVehicle);

							if (bIsJack && pPed->IsNetworkClone())
							{
								// if the ped is being jacked, disable the z blending so that he falls to the ground naturally. Otherwise, he may float down strangely.
								static_cast<CTaskNMJumpRollFromRoadVehicle*>(pTaskNM)->DisableZBlending();
							}
                        }
					}

            		if(pPed->IsNetworkClone())
					{
                        if(pTaskNM)
                        {
						    nmEntityDebugf(pPed, "ExitVehicleSeat::DetachedFromVehicle - adding network clone ragdoll task:");
                		    pPed->SwitchToRagdoll(*this);
                		    pPed->GetPedIntelligence()->AddLocalCloneTask(rage_new CTaskNMControl(pTaskNM->GetMinTime(), pTaskNM->GetMaxTime(), pTaskNM, CTaskNMControl::DO_BLEND_FROM_NM), PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP);
            			    pTaskNM = NULL;
                        }
					}
            		else
            		{
						pEvent = rage_new CEventSwitch2NM(6000, pTaskNM);
			    		pTaskNM = NULL;
					}
				}				
			}

			if (pEvent != NULL)
			{
				nmEntityDebugf(pPed, "ExitVehicleSeat::DetachedFromVehicle - adding ragdoll event:");
				m_bRagdollEventGiven = pPed->SwitchToRagdoll(*pEvent) != 0;
				delete pEvent;
				pEvent = NULL;
            }

            if(m_bRagdollEventGiven)
			{
				if (m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::ThroughWindscreen))
				{
					TUNE_GROUP_BOOL(THROUGH_WINDSCREEN_TUNE, USE_IMPULSE, true);
					if (USE_IMPULSE)
					{
						CTaskMotionInAutomobile* pTask = static_cast<CTaskMotionInAutomobile*>(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_AUTOMOBILE));
						if (pTask)
						{
							//! if the car has rotated a fair bit, use vehicle direction. This prevents ped from exiting the car at funny angles if it spins
							//! after collision.
							Vector3 vThrownForce = pTask->GetThrownForce();
							float fThrownMag = vThrownForce.Mag();

							Vector3 vForceTemp = vThrownForce;
							vForceTemp.z = 0.0f;
							vForceTemp.NormalizeSafe();

							if(vForceTemp.Mag2() > 0.0f)
							{
								Vector3 vVehicleMatrix = MAT34V_TO_MATRIX34(m_pVehicle->GetMatrix()).b;
								vVehicleMatrix.z = 0.0f;
								vVehicleMatrix.NormalizeSafe();
								const float fDot = vForceTemp.Dot(vVehicleMatrix);
								if (fDot <= 0.9f)
								{
									Displayf("[%d] THROUGHWINDSCREEN - choosing vehicle transform", fwTimer::GetFrameCount());

									vThrownForce = MAT34V_TO_MATRIX34(m_pVehicle->GetMatrix()).b;
									vThrownForce *=fThrownMag;
								}
							}

							Displayf("[%d] THROUGHWINDSCREEN - exit throw force: %f:%f:%f", 
								fwTimer::GetFrameCount(),
								vThrownForce.x,
								vThrownForce.y,
								vThrownForce.z);

							Vector3 impulse = pPed->GetMassForApplyVehicleForce() * ((vThrownForce*ms_Tunables.m_AdditionalWindscreenRagdollForceFwd) + (ms_Tunables.m_AdditionalWindscreenRagdollForceUp*ZAXIS));
							Assertf(impulse.Mag() >= 0.0f && impulse.Mag() <= 4500.0f && impulse.Mag()==impulse.Mag(), 
								"CTaskExitVehicleSeat::DetachedFromVehicle_OnEnter() - bad impulse calculation: impulse is (%f,%f,%f), pPed->GetMassForApplyVehicleForce() is %f, vThrownForce is %f,%f,%f, ms_Tunables.m_AdditionalWindscreenRagdollForceFwd is %f, ms_Tunables.m_AdditionalWindscreenRagdollForceUp is %f", 
								impulse.x, impulse.y, impulse.z, pPed->GetMassForApplyVehicleForce(), vThrownForce.x, vThrownForce.y, vThrownForce.z, ms_Tunables.m_AdditionalWindscreenRagdollForceFwd, ms_Tunables.m_AdditionalWindscreenRagdollForceUp);
							
							Displayf("[%d] THROUGHWINDSCREEN - impulse: %f:%f:%f", 
								fwTimer::GetFrameCount(),
								impulse.x,
								impulse.y,
								impulse.z);

							//! Hack. If vehicle is traveling in opposite direction to thrown impulse, rescale velocity to match impulse
							//! direction. This is to prevent ped moving backwards if impulse isn't enough to combat -velocity.
							//! This can happen as it takes several frames before the ped exits the vehicle.
							
							vThrownForce.Normalize();
		
							Vector3 vOldVelocity = pPed->GetVelocity();
							float fVelXYMag = vOldVelocity.XYMag();

							Vector3 vVelocity = vThrownForce;
							//vVelocity *= fThrownMag > fVelXYMag ? fThrownMag : fVelXYMag;
							vVelocity *= fVelXYMag;
							vVelocity.z = vOldVelocity.z;

							pPed->SetVelocity(vVelocity);
							pPed->SetDesiredVelocity(vVelocity);
							
							pPed->ApplyImpulse(impulse, VEC3_ZERO);
						}
					}

                    // disable network blending for clones for a short while. This prevents the clone being
                    // pulled back into the vehicle as they are exiting through the windscreen
                    if(pPed->IsNetworkClone())
                    {
                        const unsigned TIME_TO_DELAY_BLENDING = 500;
                        NetworkInterface::OverrideNetworkBlenderForTime(pPed, TIME_TO_DELAY_BLENDING);
                    }
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicleSeat::DetachedFromVehicle_OnUpdate()
{
	SetTaskState(State_Finish);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicleSeat::BeJacked_OnEnter()
{
	CPed& rPed =* GetPed();
	SetClipsForState();
	SetClipPlaybackRate();
	SetMoveNetworkState(ms_BeJackedRequestId, ms_BeJackedOnEnterId, ms_BeJackedStateId);
	SetUpRagdollOnCollision(rPed, *m_pVehicle);
	m_iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(&rPed);

	CPed* pJacker = static_cast<CTaskExitVehicle*>(GetParent())->GetPedsJacker();
	rPed.SetCarJacker(pJacker);

    if(rPed.IsNetworkClone() && rPed.GetIsDeadOrDying())
    {
        rPed.SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true );
    }

	m_bHasTurnedTurretDueToBeingJacked = false;
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicleSeat::BeJacked_OnUpdate()
{
	CPed& rPed = *GetPed();
	rPed.SetPedResetFlag(CPED_RESET_FLAG_BeingJacked, true);

	CFacialDataComponent* pFacialData = rPed.GetFacialData();
	if(pFacialData)
	{
		pFacialData->RequestFacialIdleClip(CFacialDataComponent::FICT_Stressed);
	}

	TurnTurretForBeingJacked(rPed, rPed.GetCarJacker());

	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	if (!rPed.IsNetworkClone() && rPed.GetIsInVehicle())
	{
		const CPed* pJacker = static_cast<CTaskExitVehicle*>(GetParent())->GetPedsJacker();
		if (pJacker && pJacker->GetUsingRagdoll() && m_pParentNetworkHelper->GetBoolean(ms_BlendBackToSeatPositionId))
		{
			rPed.SetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle, false);
			static_cast<CTaskExitVehicle*>(GetParent())->SetRunningFlag(CVehicleEnterExitFlags::DontSetPedOutOfVehicle);
			rPed.SetPedConfigFlag(CPED_CONFIG_FLAG_PedsInVehiclePositionNeedsReset, true);
			rPed.SetAttachState(ATTACH_STATE_PED_IN_CAR);
			const s32 iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(&rPed);
			if (m_pVehicle->IsSeatIndexValid(iSeatIndex))
			{
				const CVehicleSeatAnimInfo* pSeatAnimInfo = m_pVehicle->GetSeatAnimationInfo(iSeatIndex);
				if (pSeatAnimInfo && !pSeatAnimInfo->GetKeepCollisionOnWhenInVehicle())
				{
					rPed.AttachPedToEnterCar(m_pVehicle, ATTACH_STATE_PED_IN_CAR, m_pVehicle->GetEntryExitPoint(m_iExitPointIndex)->GetSeat(SA_directAccessSeat), m_iExitPointIndex);
				}
			}
			SetTaskState(State_Finish);
			return FSM_Continue;
		}
	}

	float fClipPhase = 0.0f;
	const crClip* pClip = GetClipAndPhaseForState(fClipPhase);
	CTaskVehicleFSM::ProcessLegIkBlendIn(rPed, m_fLegIkBlendInPhase, fClipPhase);

#if __ASSERT
	if (m_pVehicle->IsEntryIndexValid(m_iExitPointIndex))
	{
		if (pClip)
		{
			const fwMvClipSetId clipSetId = m_pParentNetworkHelper->GetClipSetId();
			float fDebugNMPhase = -1.0f;
			if (CClipEventTags::FindEventPhase(pClip, CClipEventTags::CanSwitchToNm, fDebugNMPhase))
			{
				float fDebugDetachStartPhase = 0.0f;
				float fDebugDetachEndPhase = 1.0f;
				if (CClipEventTags::GetMoveEventStartAndEndPhases(pClip, ms_DetachId, fDebugDetachStartPhase, fDebugDetachEndPhase))
				{
					animAssertf(fDebugNMPhase < fDebugDetachStartPhase, "ADD A BUG TO DEFAULT ANIM INGAME - Clip : %s, Clipset : %s, CanSwitchToNM tag needs to come before Detach tag", pClip->GetName(), clipSetId.GetCStr());
				}
			}
		}
	}
#endif // __ASSERT

	if (m_pParentNetworkHelper && m_pParentNetworkHelper->GetBoolean(ms_StandUpInterruptId) && (CTaskVehicleFSM::CheckForPlayerMovement(rPed) || IsPedInCombatOrFleeing(rPed)))
	{
		// If we didn't exit the seat from the detach event, do it here
		if (!m_bExitedSeat)
		{
			rPed.SetPedOutOfVehicle(CPed::PVF_ExcludeVehicleFromSafetyCheck);
		}
		if (m_pVehicle->GetSeatManager()->GetDriver() == &rPed )
		{
			m_pVehicle->GetVehicleAudioEntity()->StopHorn();
		}
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	if (IsMoveNetworkStateFinished(ms_BeJackedClipFinishedId, ms_ExitSeatPhaseId))
	{
		// If we didn't exit the seat from the detach event, do it here
		if (!m_bExitedSeat)
		{
			rPed.SetPedOutOfVehicle(CPed::PVF_ExcludeVehicleFromSafetyCheck);
		}
		if (m_pVehicle->GetSeatManager()->GetDriver() == &rPed )
		{
			m_pVehicle->GetVehicleAudioEntity()->StopHorn();
		}
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	TUNE_GROUP_FLOAT(EXIT_TUNE, DETACH_PHASE, 1.0f, 0.0f, 1.0f, 0.01f);
	float fEventPhase = 1.0f;

	if (CTaskNMBehaviour::CanUseRagdoll(&rPed, RAGDOLL_TRIGGER_VEHICLE_FALLOUT) && !m_bExitedSeat && ((fClipPhase >= DETACH_PHASE) || (pClip && CClipEventTags::FindEventPhase(pClip, CClipEventTags::CanSwitchToNm, fEventPhase) && fClipPhase >= fEventPhase)))
	{
		SetTaskState(State_DetachedFromVehicle);
		return FSM_Continue;
	}

	// If we're being jacked and dead, we should exit into ragdoll as soon as we've been set out of the vehicle
	if (rPed.ShouldBeDead() && m_bExitedSeat)
	{
		if (m_pVehicle->GetSeatManager()->GetDriver() == &rPed )
		{
			m_pVehicle->GetVehicleAudioEntity()->StopHorn();
		}
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	// Look for the detach event on the clip
	if (!m_bExitedSeat)
	{
		ProcessSetPedOutOfSeat();
	}

	// Drive the door open based on the events in the clip
	if (!m_bExitedSeat && m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::JackedFromInside))
	{
		ProcessOpenDoor(false);
	}

	if (NetworkInterface::IsGameInProgress() && !rPed.GetIsAttached())
	{
		CTaskExitVehicle* pExitVehicleTask = static_cast<CTaskExitVehicle*>(GetParent());
		if (pExitVehicleTask)
		{
            if(rPed.IsLocalPlayer())
            {
			    CPed* pPedJackingMe = pExitVehicleTask->GetPedsJacker();
			    if (pPedJackingMe && pPedJackingMe->IsAPlayerPed())
			    {
				    if (pPedJackingMe->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE))
				    {
					    CClonedEnterVehicleInfo* pTaskInfo = static_cast<CClonedEnterVehicleInfo*>(pPedJackingMe->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_ENTER_VEHICLE, PED_TASK_PRIORITY_MAX));
					    if (pTaskInfo && pTaskInfo->GetShouldTriggerElectricTask())
					    {
						    CEventGivePedTask givePedTask(PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP, rage_new CTaskDamageElectric(pTaskInfo->GetClipSetId(), pTaskInfo->GetDamageTotalTime(), pTaskInfo->GetRagdollType()));
						    rPed.GetPedIntelligence()->AddEvent(givePedTask);
							if (m_pVehicle->GetSeatManager()->GetDriver() == &rPed)
							{
								m_pVehicle->GetVehicleAudioEntity()->StopHorn();
							}
						    SetTaskState(State_Finish);
						    return FSM_Continue;
					    }
				    }
			    }
            }
            else if(rPed.IsPlayer())
            {
                if(rPed.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_DAMAGE_ELECTRIC))
                {
					if (m_pVehicle->GetSeatManager()->GetDriver() == &rPed)
					{
						m_pVehicle->GetVehicleAudioEntity()->StopHorn();
					}
                    SetTaskState(State_Finish);
                    return FSM_Continue;
                }
            }
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicleSeat::WaitForStreamedBeJacked_OnUpdateClone()
{
	taskAssert(GetPed()->IsNetworkClone());
	fwMvClipSetId clipsetId = static_cast<CTaskExitVehicle*>(GetParent())->GetStreamedExitClipsetId();
	fwMvClipId clipId = static_cast<CTaskExitVehicle*>(GetParent())->GetStreamedExitClipId();
	if (clipsetId.IsNotNull() && clipId.IsNotNull())
	{
		fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipsetId);
		if (taskVerifyf(pClipSet, "Couldn't find clipset %s", clipsetId.TryGetCStr() ? clipsetId.GetCStr() : "NULL"))
		{
			if (pClipSet->Request_DEPRECATED())
			{
				SetTaskState(State_StreamedBeJacked);
				return FSM_Continue;
			}
		}
		else
		{
			SetTaskState(State_Finish);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicleSeat::WaitForStreamedExit_OnUpdateClone()
{
	taskAssert(GetPed()->IsNetworkClone());
	fwMvClipSetId clipsetId = static_cast<CTaskExitVehicle*>(GetParent())->GetStreamedExitClipsetId();

	if (clipsetId.IsNotNull())
	{
		fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipsetId);
		if (taskVerifyf(pClipSet, "Couldn't find clipset %s", clipsetId.TryGetCStr() ? clipsetId.GetCStr() : "NULL"))
		{
			if (pClipSet->Request_DEPRECATED())
			{
				SetTaskState(State_StreamedExit);
				return FSM_Continue;
			}
		}
		else
		{
			SetTaskState(State_Finish);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicleSeat::StreamedBeJacked_OnEnter()
{
	SetClipPlaybackRate();
	SetMoveNetworkState(ms_StreamedBeJackedRequestId, ms_StreamedBeJackedOnEnterId, ms_StreamedBeJackedStateId);
	SetClipsForState();
	SetUpRagdollOnCollision(*GetPed(), *m_pVehicle);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicleSeat::StreamedBeJacked_OnUpdate()
{
	CPed& rPed = *GetPed();
	rPed.SetPedResetFlag(CPED_RESET_FLAG_BeingJacked, true);

	CFacialDataComponent* pFacialData = rPed.GetFacialData();
	if(pFacialData)
	{
		pFacialData->RequestFacialIdleClip(CFacialDataComponent::FICT_Stressed);
	}

	float fPhase = -1.0f;
	const crClip* pClip = GetClipAndPhaseForState(fPhase);
	CTaskVehicleFSM::ProcessLegIkBlendIn(rPed, m_fLegIkBlendInPhase, fPhase);

	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	float fEventPhase = 1.0f;

	if (!m_bExitedSeat && (pClip && CClipEventTags::FindEventPhase(pClip, CClipEventTags::CanSwitchToNm, fEventPhase) && fPhase >= fEventPhase))
	{
		m_iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(&rPed);
		SetTaskState(State_DetachedFromVehicle);
		return FSM_Continue;
	}

	if (!m_bExitedSeat && m_pParentNetworkHelper->GetBoolean(ms_DetachId))
	{
		// Leaving this in for now as a safeguard to make sure the screen fades out on arrest in case the Arrested Id is never received below.
		if (rPed.IsLocalPlayer() && IsRunningFlagSet(CVehicleEnterExitFlags::IsArrest))
		{
			rPed.SetArrestState(ArrestState_Arrested);
		}

		rPed.SetPedOutOfVehicle(CPed::PVF_ExcludeVehicleFromSafetyCheck);
		m_bExitedSeat = true;
	}

	ProcessOpenDoor(true);

	// If we're a player show is being arrested we need to make sure we set our arrest state at the proper time so the fadeout starts
	if (IsRunningFlagSet(CVehicleEnterExitFlags::IsArrest) && rPed.IsLocalPlayer())
	{
		if(m_pParentNetworkHelper->GetBoolean(ms_ArrestedId))
		{
			rPed.SetArrestState(ArrestState_Arrested);
		}

		// We can break out of the arrest up until the anim tells us we can't (or until we hit the fail safe time)
		if( m_pParentNetworkHelper->GetBoolean(ms_PreventBreakoutId) ||
			GetTimeInState() > ms_Tunables.m_MaxTimeForArrestBreakout ||
			rPed.GetIsArrested() || !rPed.GetIsInVehicle())
		{
			m_bCanBreakoutOfArrest = false;
		}

		if(m_bCanBreakoutOfArrest)
		{
			CControl* pControl = rPed.GetControlFromPlayer();
			if( pControl &&
				(pControl->GetVehicleAccelerate().IsDown() || pControl->GetVehicleBrake().IsDown()) )
			{
				CTask* pExitVehicleTask = GetParent();
				if(pExitVehicleTask && pExitVehicleTask->GetTaskType() == CTaskTypes::TASK_EXIT_VEHICLE)
				{
					static_cast<CTaskExitVehicle*>(pExitVehicleTask)->SetRunningFlag(CVehicleEnterExitFlags::DontSetPedOutOfVehicle);
				}

				rPed.SetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle, false);
				rPed.SetPedConfigFlag(CPED_CONFIG_FLAG_PedsInVehiclePositionNeedsReset, true);
				SetTaskState(State_Finish);
				return FSM_Continue;
			}
		}

		if( (rPed.GetIsInVehicle() || !rPed.GetIsArrested()) &&
			IsMoveNetworkStateFinished(ms_StreamedBeJackedClipFinishedId, ms_StreamedBeJackedPhaseId) )
		{
			rPed.SetPedOutOfVehicle(CPed::PVF_ExcludeVehicleFromSafetyCheck);

			if(!rPed.GetIsArrested())
			{
				SetTaskState(State_Finish);
				return FSM_Continue;
			}
		}
	}
	else if (IsMoveNetworkStateFinished(ms_StreamedBeJackedClipFinishedId, ms_StreamedBeJackedPhaseId))
	{
		// If we didn't exit the seat from the detach event, do it here
		if (!m_bExitedSeat)
		{
			rPed.SetPedOutOfVehicle(CPed::PVF_ExcludeVehicleFromSafetyCheck);
		}
		
		//Get the ped to react to being asked to leave the vehicle.
        if(!rPed.IsNetworkClone())
        {
		    rPed.GetPedIntelligence()->AddTaskAtPriority(rage_new CTaskReactToBeingAskedToLeaveVehicle(m_pVehicle,m_pVehicle->GetDriver()), PED_TASK_PRIORITY_PRIMARY);
        }

		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicleSeat::StreamedExit_OnEnter()
{
	SetMoveNetworkState(ms_StreamedExitRequestId, ms_StreamedExitOnEnterId, ms_StreamedExitStateId);
	SetClipsForState();
	SetClipPlaybackRate();

	// Don't collide with vehicle upon exit.
	bool bIsExitToSkyDive = static_cast<CTaskExitVehicle*>(GetParent())->IsFlagSet(CVehicleEnterExitFlags::IsTransitionToSkyDive);
	if(bIsExitToSkyDive)
	{
		SetRunningFlag(CVehicleEnterExitFlags::IsTransitionToSkyDive);
		SetUpRagdollOnCollision(*GetPed(), *m_pVehicle);

		CPed* pPed = GetPed();
		if (pPed && pPed->IsLocalPlayer())
		{
			StatsInterface::IncrementStat(StatsInterface::GetStatsModelHashId("BAILED_FROM_VEHICLE"), 1.0f);
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicleSeat::StreamedExit_OnUpdate()
{
	CPed& rPed = *GetPed();
	float fPhase = -1.0f;
	GetClipAndPhaseForState(fPhase);
	CTaskVehicleFSM::ProcessLegIkBlendIn(rPed, m_fLegIkBlendInPhase, fPhase);

	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	if (!rPed.IsInjured())
	{
		ProcessOpenDoor(false);
	}

	bool bDetach = m_pParentNetworkHelper->GetBoolean(ms_DetachId);
	bool bEnded = IsMoveNetworkStateFinished(ms_StreamedExitClipFinishedId, ms_ExitSeatPhaseId);

	if(!m_bExitedSeat && (bDetach || bEnded))
	{
		bool bIsExitToSkyDive = static_cast<CTaskExitVehicle*>(GetParent())->IsFlagSet(CVehicleEnterExitFlags::IsTransitionToSkyDive);

		u32 uFlags = CPed::PVF_ExcludeVehicleFromSafetyCheck;
		s32 nNumFramesJustLeftOverride = -1;
		if(bIsExitToSkyDive)
		{
			uFlags |= CPed::PVF_DontSnapMatrixUpright;
			uFlags |= CPed::PVF_DontNullVelocity;
			uFlags |= CPed::PVF_InheritVehicleVelocity;

			//! should already be clear in ejector seats.
			if(IsEjectorSeat(m_pVehicle))
			{
				uFlags |= CPed::PVF_IgnoreSettingJustLeftVehicle;
			}
			else
			{
				if(!static_cast<CTaskExitVehicle*>(GetParent())->IsFlagSet(CVehicleEnterExitFlags::ExitSeatOntoVehicle)
					&& CheckForSkyDive(GetPed(), m_pVehicle, m_iExitPointIndex)) // check again if the conditions are still valid for skydive, don't disable the collision if the plane is too close to ground or water surface
				{
					uFlags |= CPed::PVF_NoCollisionUntilClear;
				}
			
				TUNE_GROUP_INT(SET_PED_OUT_OF_VEHICLE, NUM_FRAMES_TO_CONSIDER_JUST_LEFT_SKYDIVE, 50, 0, 100, 1);
				nNumFramesJustLeftOverride = NUM_FRAMES_TO_CONSIDER_JUST_LEFT_SKYDIVE;
			}
		}
		// Cache off the last matrices so we can apply them to the ragdoll once activated.
		const Mat34V savedLastMat = PHSIM->GetLastInstanceMatrix(rPed.GetRagdollInst());
		const Mat34V savedCurrentMat = rPed.GetRagdollInst()->GetMatrix();

		rPed.SetPedOutOfVehicle(uFlags, nNumFramesJustLeftOverride);

		if(bDetach)
		{
			m_bExitedSeat = true;
		}

		if(bIsExitToSkyDive)
		{
			CTaskExitVehicle::OnExitMidAir(&rPed, m_pVehicle);

			bool bIsUsingRagdoll = false;

			// Restore the current and last matrix saved earlier in this function.  This needs to get called before the switch to ragdoll
			// so that the correct position and incoming velocities can get computed.
			PHSIM->SetLastInstanceMatrix(rPed.GetRagdollInst(), savedLastMat);
			rPed.GetRagdollInst()->SetMatrix(savedCurrentMat);

			//! If we aren't upright, ragdoll.
			ScalarV scDot = Dot(rPed.GetTransform().GetC(), Vec3V(V_Z_AXIS_WZERO));

			static dev_float s_DefaultSkyDiveRagdollThreshold = 0.9f;
			static dev_float s_EjectorSeatSkyDiveRagdollThreshold = 0.6f;

			float fSkyDiveRagdollThreshold = IsEjectorSeat(m_pVehicle) ? s_EjectorSeatSkyDiveRagdollThreshold : s_DefaultSkyDiveRagdollThreshold;

			ScalarV scMinDot(ScalarVFromF32(fSkyDiveRagdollThreshold));
			if(IsLessThanAll(scDot, scMinDot) || rPed.ShouldBeDead())
			{
				if ( CTaskNMBehaviour::CanUseRagdoll(&rPed, RAGDOLL_TRIGGER_VEHICLE_JUMPOUT))
				{
					nmEntityDebugf((&rPed), "ExitVehicleSeat::StreamedExit - adding high fall event:");
					CEventSwitch2NM event(6000, rage_new CTaskNMHighFall(1500));
					rPed.SwitchToRagdoll(event);

					bIsUsingRagdoll = true;
				}
			}

			if(!bIsUsingRagdoll)
			{
				s32 nFallFlags = 0;
				if (!IsEjectorSeat(m_pVehicle))
				{
					bool bAllowParachuting = CTaskParachute::IsPedInStateToParachute(rPed);

					//! Recheck for dive. If it still ok, force it.
					nFallFlags = FF_DisableSkydiveTransition | FF_CheckForDiveOrRagdoll;

					// Force sky dive if we can parachute.
					if (bAllowParachuting)
					{
						nFallFlags |= FF_ForceSkyDive;
					}

					float fHeightAboveWater = -1.0f;
					if(CheckForDiveIntoWater(&rPed, m_pVehicle, bAllowParachuting, fHeightAboveWater) && 
						CTaskMotionSwimming::CanDiveFromPosition(rPed, rPed.GetTransform().GetPosition(), false))
					{
						nFallFlags |= FF_ForceDive;
						nFallFlags |= FF_DivingOutOfVehicle;
					}
				}
				else
				{
					nFallFlags |= FF_ForceSkyDive;

					//! Do a longer blend if we aren't upright.
					ScalarV scMinDot(ScalarVFromF32(s_DefaultSkyDiveRagdollThreshold));
					if(IsLessThanAll(scDot, scMinDot))
					{
						nFallFlags |= FF_LongerParachuteBlend ;
					}
				}
			
				CTaskExitVehicle::TriggerFallEvent(&rPed, nFallFlags);

				//Process the in-air event this frame, otherwise we end up with a frame of delay.
				rPed.SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAIUpdate, true);
			}

			bEnded = true;
		}
	}

	if (bEnded)
	{
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicleSeat::OnSideExit_OnEnter()
{
	SetMoveNetworkState(ms_OnSideExitRequestId, ms_OnSideExitOnEnterId, ms_OnSideExitStateId);

	const fwMvClipSetId commonClipsetId = CTaskEnterVehicleAlign::GetCommonClipsetId(m_pVehicle, m_iExitPointIndex);
	const fwMvClipId clipId = ms_Tunables.m_DefaultCrashExitOnSideClipId;
	const crClip* pClip = fwClipSetManager::GetClip(commonClipsetId, clipId);
	if (!taskVerifyf(pClip, "Couldn't find clip %s from clipset %s", clipId.GetCStr(), commonClipsetId.GetCStr()))
	{
		return FSM_Quit;
	}

	CClipEventTags::FindMoverFixUpStartEndPhases(pClip, m_fStartTranslationFixupPhase, m_fEndTranslationFixupPhase);
	// We fix up the vehicle relative roll separately to avoid rotation lerping issues
	CClipEventTags::FindMoverFixUpStartEndPhases(pClip, m_fStartRollOrientationFixupPhase, m_fEndRollOrientationFixupPhase, false, CClipEventTags::CMoverFixupEventTag::eXAxis);
	// Then the rest of the orientation, we probably should ragdoll if the vehicle starts to self right as we exit
	CClipEventTags::FindMoverFixUpStartEndPhases(pClip, m_fStartOrientationFixupPhase, m_fEndOrientationFixupPhase, false, CClipEventTags::CMoverFixupEventTag::eYAxis);
	
	// Allow the ped to ragdoll if we get impacted whilst exiting
	CPed& rPed = *GetPed();
	SetUpRagdollOnCollision(rPed, *m_pVehicle);

	m_iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(&rPed);
	m_pParentNetworkHelper->SetClip(pClip, ms_ExitSeatClipId);
	// Let ProcessMoveSignals() handle the communication with the Move network.
	m_bMoveNetworkFinished = false;
	RequestProcessMoveSignalCalls();
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicleSeat::OnSideExit_OnUpdate()
{
	CPed& rPed = *GetPed();
	rPed.SetPedResetFlag(CPED_RESET_FLAG_IsExitingOnsideVehicle, true);

	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	ProcessOpenDoor(false);

	// Look for the detach event on the clip
	if (!m_bExitedSeat)
	{
		ProcessSetPedOutOfSeat();
	}

	// If the vehicle is no longer on its side, fuck it, lets ragdoll out
	if (rPed.GetIsAttachedInCar() && !rPed.GetPedResetFlag(CPED_RESET_FLAG_BlockRagdollActivationInVehicle))
	{
		const CVehicle::eVehicleOrientation eVehOrientation = CVehicle::GetVehicleOrientation(*m_pVehicle);
		if (eVehOrientation != CVehicle::VO_OnSide)
		{
			SetTaskState(State_DetachedFromVehicle);
			return FSM_Continue;
		}
	}

	if (ProcessExitSeatInterrupt(rPed))
	{
		return FSM_Continue;
	}

	if (m_bMoveNetworkFinished)
	{
		// If we didn't exit the seat from the detach event, do it here
		if (!m_bExitedSeat)
		{
			GetPed()->SetPedOutOfVehicle(0);
		}

		SetTaskState(State_Finish);
		return FSM_Continue;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicleSeat::UpsideDownExit_OnEnter()
{
	// Allow the ped to ragdoll if we get impacted whilst exiting
	CPed& rPed = *GetPed();
	SetUpRagdollOnCollision(rPed, *m_pVehicle);
#if __ASSERT
	const fwMvClipSetId clipsetId = CTaskEnterVehicleAlign::GetCommonClipsetId(m_pVehicle, m_iExitPointIndex);
	const bool bLoaded = CTaskVehicleFSM::IsVehicleClipSetLoaded(clipsetId);
	taskAssertf(bLoaded, "Upside down exit clipset not loaded");
	aiDisplayf("Setting upside down clipset %s on ped %s, loaded ? %s", clipsetId.GetCStr(), rPed.GetModelName(), bLoaded ? "TRUE" : "FALSE");
#endif // __ASSERT
	SetMoveNetworkState(ms_UpsideDownExitRequestId, ms_UpsideDownExitOnEnterId, ms_UpsideDownExitStateId);
	// Let ProcessMoveSignals() handle the communication with the Move network.
	m_bMoveNetworkFinished = false;
	m_iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(&rPed);
	RequestProcessMoveSignalCalls();
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskExitVehicleSeat::UpsideDownExit_OnUpdate()
{
	CPed& rPed = *GetPed();
	rPed.SetPedResetFlag(CPED_RESET_FLAG_IsExitingUpsideDownVehicle, true);

	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	ProcessOpenDoor(false);

	float fPhase = 0.0f;
	const crClip* pClip = GetClipAndPhaseForState(fPhase);
	// Break off the door and apply an impulse so it's not in our way
	TUNE_GROUP_FLOAT(UPSIDE_DOWN_EXIT_TUNE, BREAK_DOOR_PHASE, 0.05f, 0.0f, 1.0f, 0.01f);
	if (fPhase > BREAK_DOOR_PHASE)
	{
		CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(m_pVehicle, m_iExitPointIndex);
		fragInst* pFragInst = m_pVehicle->GetFragInst();

		if(pFragInst && pDoor && pDoor->GetIsIntact(m_pVehicle))
		{
			if(m_pVehicle->CarPartsCanBreakOff())
			{
				if(pDoor->GetIsLatched(m_pVehicle))
				{
					pDoor->BreakLatch(m_pVehicle);
				}
				fragInst* pDoorFragInst = pDoor->BreakOff(m_pVehicle);

				TUNE_GROUP_BOOL(UPSIDE_DOWN_EXIT_TUNE, APPLY_IMPULSE_TO_DOOR, true);
				if (APPLY_IMPULSE_TO_DOOR && pDoorFragInst)
				{
					phCollider* pDoorCollider = pDoorFragInst ? CPhysics::GetSimulator()->GetCollider(pDoorFragInst->GetLevelIndex()) : NULL;
					if (pDoorCollider)
					{
						const CVehicleEntryPointInfo* pEntryInfo = m_pVehicle->GetEntryInfo(m_iExitPointIndex);
						const bool bLeftSide = pEntryInfo->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT ? true : false;
						Vector3 vecAwayFromVehicle = bLeftSide ? -(VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().GetA())) : (VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().GetA()));

						TUNE_GROUP_FLOAT(UPSIDE_DOWN_EXIT_TUNE, DOOR_BREAK_SPEED, 2.0f, 0.0f, 10.0f, 0.01f);
						Vector3 vecDoorImpulse;
						vecDoorImpulse.Set(DOOR_BREAK_SPEED * vecAwayFromVehicle);
						vecDoorImpulse.Scale(pDoorFragInst->GetArchetype()->GetMass());

						Vector3 vecDoorImpulsePos;
						Matrix34 m = RCC_MATRIX34(pDoorFragInst->GetMatrix());
						vecDoorImpulsePos.Set(m.d);
						pDoorCollider->ApplyImpulse(vecDoorImpulse, vecDoorImpulsePos);
					}
				}
			}
		}
	}

	// Look for the detach event on the clip
	if (!m_bExitedSeat)
	{
		ProcessSetPedOutOfSeat();
	}

	// If the vehicle is no longer upside down, fuck it, lets ragdoll out
	if (rPed.GetIsAttachedInCar() && !rPed.GetPedResetFlag(CPED_RESET_FLAG_BlockRagdollActivationInVehicle) && CTaskNMBehaviour::CanUseRagdoll(&rPed, RAGDOLL_TRIGGER_VEHICLE_FALLOUT))
	{
		float fSwitchToNMPhase;
		if (CClipEventTags::FindEventPhase(pClip, CClipEventTags::CanSwitchToNm, fSwitchToNMPhase) && fPhase >= fSwitchToNMPhase)
		{
			SetTaskState(State_DetachedFromVehicle);
			return FSM_Continue;
		}

		Vector3 vGroundPos;
		Vector3 vPedPosition = VEC3V_TO_VECTOR3(rPed.GetTransform().GetPosition());
		if (!CTaskVehicleFSM::FindGroundPos(m_pVehicle, &rPed, vPedPosition, GetGroundFixupHeight(), vGroundPos, true, true))
		{
			SetTaskState(State_DetachedFromVehicle);
			return FSM_Continue;
		}

		const CVehicle::eVehicleOrientation eVehOrientation = CVehicle::GetVehicleOrientation(*m_pVehicle);
		if (eVehOrientation != CVehicle::VO_UpsideDown)
		{
			SetTaskState(State_DetachedFromVehicle);
			return FSM_Continue;
		}
	}

	if (ProcessExitSeatInterrupt(rPed))
	{
		return FSM_Continue;
	}

	if (m_bMoveNetworkFinished)
	{
		// If we didn't exit the seat from the detach event, do it here
		if (!m_bExitedSeat)
		{
			rPed.SetPedOutOfVehicle(CPed::PVF_IsUpsideDownExit);
		}

		SetTaskState(State_Finish);
		return FSM_Continue;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicleSeat::ProcessOpenDoor(bool bProcessOnlyIfTagsFound, bool bDriveRearDoorClosed)
{
	float fPhase = 0.0f;
	const crClip* pClip = GetClipAndPhaseForState(fPhase);
	if (pClip)
	{
		CPed* pPed = GetPed();
		CTaskVehicleFSM::ProcessOpenDoor(*pClip, fPhase, *m_pVehicle, m_iExitPointIndex, bProcessOnlyIfTagsFound, false, pPed, m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::DoorInFrontBeingUsed));

		if (bDriveRearDoorClosed)
		{
			// If we're a front seat, find the door behind us and try to drive it shut if its not being used
			s32 iRearEntryIndex = CTaskVehicleFSM::FindRearEntryForEntryPoint(m_pVehicle, m_iExitPointIndex);
			TUNE_GROUP_BOOL(DOOR_INTERSECTION_TUNE, ALLOW_DRIVING_REAR_DOOR_DURING_FRONT_EXIT_IN_MP, true);
			if (m_pVehicle->IsEntryIndexValid(iRearEntryIndex) && (ALLOW_DRIVING_REAR_DOOR_DURING_FRONT_EXIT_IN_MP || !NetworkInterface::IsGameInProgress()))
			{
				// Make sure the entry point for the door we're driving shut is relatively close!
				Vector3 vFirstEntryPos, vSecondEntryPos;
				Quaternion qEntryRotUnused;
				CModelSeatInfo::CalculateEntrySituation(m_pVehicle, pPed, vFirstEntryPos, qEntryRotUnused, m_iExitPointIndex, CModelSeatInfo::EF_RoughPosition);
				CModelSeatInfo::CalculateEntrySituation(m_pVehicle, pPed, vSecondEntryPos, qEntryRotUnused, iRearEntryIndex, CModelSeatInfo::EF_RoughPosition);

				TUNE_GROUP_FLOAT(VEHICLE_EXIT_TUNE, MAX_DIST_BETWEEN_ENTRY_TO_DRIVE_REAR_DOOR, 2.0f, 0.0f, 3.0f, 0.01f);
				const float fXYDistSqd = (vFirstEntryPos - vSecondEntryPos).XYMag2();
				if (fXYDistSqd > square(MAX_DIST_BETWEEN_ENTRY_TO_DRIVE_REAR_DOOR))
				{
					return;
				}

				TUNE_GROUP_BOOL(DOOR_INTERSECTION_TUNE, DRIVING_REAR_DOOR_DURING_FRONT_EXIT_SHOULD_SKIP_MAKING_RESERVATION_IN_MP, true);
				CComponentReservation* pComponentReservation = m_pVehicle->GetComponentReservationMgr()->GetDoorReservation(iRearEntryIndex);
				if (pComponentReservation)
				{
					bool bCanDriveDoor = true;

					// Should we make a reservation the rear door when we close it, and potentially delay anyone else entering/exiting?
					const bool bIgnoreReservingDoorsInMp = DRIVING_REAR_DOOR_DURING_FRONT_EXIT_SHOULD_SKIP_MAKING_RESERVATION_IN_MP && NetworkInterface::IsGameInProgress();
					if (!bIgnoreReservingDoorsInMp)
					{
						if (pComponentReservation->GetPedUsingComponent() == pPed)
						{
							pComponentReservation->SetPedStillUsingComponent(pPed);
						}
						else if (!CComponentReservationManager::ReserveVehicleComponent(pPed, m_pVehicle, pComponentReservation))
						{
							bCanDriveDoor = false;
						}
					}

					// If someone else has a rear door reservation, don't try to drive it shut
					if (pComponentReservation->GetPedUsingComponent() != NULL && pComponentReservation->GetPedUsingComponent() != pPed)
					{
						bCanDriveDoor = false;
					}
					
					// Do not close the door if it doesn't have any lock flag set
					CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(m_pVehicle, iRearEntryIndex);
					bool bIsSlidingDoor = pDoor && (pDoor->GetFlag(CCarDoor::AXIS_SLIDE_X) || pDoor->GetFlag(CCarDoor::AXIS_SLIDE_Y) || pDoor->GetFlag(CCarDoor::AXIS_SLIDE_Z));
					if (pDoor && bCanDriveDoor && !bIsSlidingDoor)
					{
						if (!pDoor->GetIsClosed())
						{
							// Make sure we flag FrontEntryUsed here, otherwise this will try and override a rear ped opening the door at a limited angle
							s32 iFlags = CTaskVehicleFSM::VF_CloseDoor | CTaskVehicleFSM::VF_FrontEntryUsed;

							TUNE_GROUP_FLOAT(EXIT_VEHICLE_TUNE, START_PHASE_CLOSE_REAR_DOOR, 0.5f, 0.0f, 1.0f, 0.01f);
							TUNE_GROUP_FLOAT(EXIT_VEHICLE_TUNE, END_PHASE_CLOSE_REAR_DOOR_FORCED, 0.75f, 0.0f, 1.0f, 0.01f);
							CTaskVehicleFSM::DriveDoorFromClip(*m_pVehicle, pDoor, iFlags, fPhase, START_PHASE_CLOSE_REAR_DOOR, bIgnoreReservingDoorsInMp ? END_PHASE_CLOSE_REAR_DOOR_FORCED : 1.0f);
						}
					}
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicleSeat::ProcessSetPedOutOfSeat()
{
	taskAssert(!m_bExitedSeat);

	CPed* pPed = GetPed();

	if (m_pParentNetworkHelper->GetBoolean(ms_DetachId))
	{
		u32 uFlags = CPed::PVF_DontNullVelocity;
		if(GetState() != State_OnSideExit && GetState() != State_UpsideDownExit)
		{
			uFlags |= CPed::PVF_ExcludeVehicleFromSafetyCheck | CPed::PVF_IgnoreSafetyPositionCheck;
		}
		if (GetState() == State_UpsideDownExit)
		{
			uFlags |= CPed::PVF_IsUpsideDownExit;
		}
		if (GetState() == State_OnSideExit)
		{
			if (IsStandingOnVehicle(*pPed))
			{
				uFlags |= CPed::PVF_ExcludeVehicleFromSafetyCheck;
			}
		}
		if (GetState() == State_ExitToAim)
		{
			uFlags |= CPed::PVF_DontDestroyWeaponObject;
		}
		pPed->SetPedOutOfVehicle(uFlags);
		m_bExitedSeat = true;
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicleSeat::ProcessExitSeatInterrupt(CPed& rPed)
{
	if (m_pParentNetworkHelper->GetBoolean(ms_ExitSeatInterruptId))
	{
		bool bWantsToInterruptExitSeat = false;

		if (!rPed.IsNetworkClone())
		{
			bWantsToInterruptExitSeat = CTaskVehicleFSM::CheckForPlayerExitInterrupt(rPed, *m_pVehicle, m_iRunningFlags, static_cast<CTaskExitVehicle*>(GetParent())->GetSeatOffset(), CTaskVehicleFSM::EF_TryingToSprintAlwaysInterrupts | CTaskVehicleFSM::EF_IgnoreDoorTest | CTaskVehicleFSM::EF_OnlyAllowInterruptIfTryingToSprint | CTaskVehicleFSM::EF_AnyStickInputInterrupts);
		}
		else
		{
			bWantsToInterruptExitSeat = DoesClonePedWantToInterruptExitSeatAnim(rPed);
		}

		// See if the player wants to interrupt the exit
		if (bWantsToInterruptExitSeat)
		{
			if (rPed.IsLocalPlayer())
			{
				static_cast<CTaskExitVehicle*>(GetParent())->SetRunningFlag(CVehicleEnterExitFlags::UseFastClipRate);
			}
			static_cast<CTaskExitVehicle*>(GetParent())->SetRunningFlag(CVehicleEnterExitFlags::HasInterruptedAnim);

			rPed.SetVelocity(VEC3_ZERO);
			ControlledSetPedOutOfVehicle(0);
			SetTaskState(State_Finish);
			return true;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicleSeat::UpdateTarget()
{
	// Assume we want to be horizontal, i.e. the rotational offset is the current orientation of the seat matrix
	// Maybe this should use the results of the ground probe to orientate the ped along the slope of the ground
	Vector3 vTargetPosition(Vector3::ZeroType);
	Quaternion qTargetOrientation(Quaternion::IdentityType);

	const crClip* pClip = m_bIsRappel ? m_pParentNetworkHelper->GetClip(ms_ExitSeatClipId) : NULL;

	if (GetState() == State_FleeExit)
	{
		pClip = m_pParentNetworkHelper->GetClip(ms_FleeExitClipId);
	}
	else if (GetState() == State_StreamedBeJacked)
	{
		pClip = m_pParentNetworkHelper->GetClip(ms_StreamedBeJackedClipId);
	}
	else if (GetState() == State_JumpOutOfSeat)
	{
		pClip = m_pParentNetworkHelper->GetClip(ms_JumpOutClipId);
	}
	else if (GetState() == State_BeJacked)
	{
		pClip = m_pParentNetworkHelper->GetClip(ms_ExitSeatClipId);
	}

	// B*3461837: For the TORERO, only pull back when we're in first person so we don't camera clip through the scissor door (as the pull back looks kinda terrible in third person) 
	const CVehicleEntryPointAnimInfo* pAnimInfo = m_pVehicle->GetEntryAnimInfo(m_iExitPointIndex);
	bool bFixUpOnExit = pAnimInfo->GetFixUpMoverToEntryPointOnExit() || (MI_CAR_TORERO.IsValid() && m_pVehicle->GetModelIndex() == MI_CAR_TORERO && GetPed()->IsLocalPlayer() && GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false));
	
	//Don't use FixUpMoverToEntryPointOnExit if we are in rappel task in the buzzard or annihilator2 or annihilator
	if ((MI_HELI_BUZZARD.IsValid() && m_pVehicle->GetModelIndex() == MI_HELI_BUZZARD) || (MI_HELI_ANNIHILATOR2.IsValid() && m_pVehicle->GetModelIndex() == MI_HELI_ANNIHILATOR2) 
		 || (MI_HELI_POLICE_2.IsValid() && m_pVehicle->GetModelIndex() == MI_HELI_POLICE_2))
	{
		if (GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_HELI_PASSENGER_RAPPEL))
		{
			bFixUpOnExit = false;
		}
	}

	if (pAnimInfo->GetHasGetOutToVehicle())
	{
		if (!CTaskVehicleFSM::ComputeTargetTransformInWorldSpace(*m_pVehicle, vTargetPosition, qTargetOrientation, m_iExitPointIndex, CExtraVehiclePoint::GET_OUT))
		{
			CModelSeatInfo::CalculateExitSituation(m_pVehicle, vTargetPosition, qTargetOrientation, m_iExitPointIndex, pClip);
		}
	}
	else if (GetState() == State_UpsideDownExit)
	{
		if (!CTaskVehicleFSM::ComputeTargetTransformInWorldSpace(*m_pVehicle, vTargetPosition, qTargetOrientation, m_iExitPointIndex, CExtraVehiclePoint::UPSIDE_DOWN_EXIT))
		{
			CModelSeatInfo::CalculateEntrySituation(m_pVehicle, GetPed(), vTargetPosition, qTargetOrientation, m_iExitPointIndex);
		}
	}
	else if (GetState() != State_JumpOutOfSeat && GetState() != State_BeJacked && GetState() != State_StreamedBeJacked && (m_pVehicle->InheritsFromBike() || bFixUpOnExit || GetState() == State_OnSideExit))
	{
		// The BOMBUSHKA exit animations don't work so great on the VOLATOL, so we need to do some fixups in cases we don't usually apply them
		if (MI_PLANE_VOLATOL.IsValid() && m_pVehicle->GetModelIndex() == MI_PLANE_VOLATOL && (m_iExitPointIndex == 2 || m_iExitPointIndex == 3))
		{
			if (!CTaskVehicleFSM::ComputeTargetTransformInWorldSpace(*m_pVehicle, vTargetPosition, qTargetOrientation, m_iExitPointIndex, CExtraVehiclePoint::GET_OUT))
			{
				CModelSeatInfo::CalculateExitSituation(m_pVehicle, vTargetPosition, qTargetOrientation, m_iExitPointIndex, pClip);
			}
		}
		else
		{
			CModelSeatInfo::CalculateEntrySituation(m_pVehicle, GetPed(), vTargetPosition, qTargetOrientation, m_iExitPointIndex);

			// Restore the orientation
			Vector3 vTempPosition(Vector3::ZeroType);
			Quaternion qTempOrientation(Quaternion::IdentityType);
			CModelSeatInfo::CalculateExitSituation(m_pVehicle, vTempPosition, qTempOrientation, m_iExitPointIndex, pClip);
			qTargetOrientation = qTempOrientation;
		}
	}
	else
	{
		CModelSeatInfo::CalculateExitSituation(m_pVehicle, vTargetPosition, qTargetOrientation, m_iExitPointIndex, pClip);
	}

#if DEBUG_DRAW
	Vector3 vOrientationEulers;
	qTargetOrientation.ToEulers(vOrientationEulers);
	Vector3 vDebugOrientation(YAXIS);
	vDebugOrientation.RotateZ(vOrientationEulers.z);
	ms_debugDraw.AddSphere(RCC_VEC3V(vTargetPosition), 0.025f, Color_green, 1000);
	Vector3 vRotEnd = vTargetPosition + vDebugOrientation;
	ms_debugDraw.AddArrow(RCC_VEC3V(vTargetPosition), RCC_VEC3V(vRotEnd), 0.025f, Color_green, 1000);
#endif

	Vector3 vTargetOrientationEulers(Vector3::ZeroType);
	qTargetOrientation.ToEulers(vTargetOrientationEulers);
	vTargetOrientationEulers.x = 0.0f;
	vTargetOrientationEulers.y = 0.0f;
	qTargetOrientation.FromEulers(vTargetOrientationEulers, "xyz");

	const CVehicleEntryPointInfo* pEntryInfo = m_pVehicle->IsEntryIndexValid(m_iExitPointIndex) ? m_pVehicle->GetEntryInfo(m_iExitPointIndex) : NULL;
	bool bIsPlaneHatchEntry = pEntryInfo && pEntryInfo->GetIsPlaneHatchEntry();

	if (!m_pVehicle->InheritsFromSubmarine() && !m_pVehicle->GetLayoutInfo()->GetClimbUpAfterOpenDoor() && !bIsPlaneHatchEntry)
	{
		bool bExitingOntoVehicle = (m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::ExitSeatOntoVehicle));

		Vector3 vGroundPos(Vector3::ZeroType);
		const bool bExitAnimHasLongDrop = pAnimInfo->GetExitAnimHasLongDrop();
		if (bExitAnimHasLongDrop)
		{
			vTargetPosition.z += ms_Tunables.m_GroundFixupHeightLargeOffset;
		}

		if (!m_bIsRappel && (pAnimInfo->GetUseNewPlaneSystem() || !pAnimInfo->GetHasClimbUp() || bExitingOntoVehicle || bExitAnimHasLongDrop) && !pAnimInfo->GetHasVaultUp() && CTaskVehicleFSM::FindGroundPos(m_pVehicle, GetPed(), vTargetPosition, GetGroundFixupHeight(bExitAnimHasLongDrop), vGroundPos, !bExitingOntoVehicle && GetState() != State_OnSideExit, !bExitAnimHasLongDrop))
		{
			// Adjust the reference position
			vTargetPosition.z = vGroundPos.z;
			m_bGroundFixupApplied = true;
		}
		else
		{
			m_bGroundFixupApplied = false;

			//! If we are falling into water, try and adjust fixup position to be water height.
			if(m_pVehicle->InheritsFromBoat() || (m_pVehicle->InheritsFromAmphibiousAutomobile() && m_pVehicle->GetIsInWater()))
			{
				float fWaterHeight = 0.0f;
				bool bWaterUnderPed = CheckForWaterUnderPed(GetPed(), m_pVehicle, ms_Tunables.m_GroundFixupHeightBoatInWater, 1.5f, fWaterHeight, false);
				if(bWaterUnderPed)
				{	
					float fWaterHeightWaves = 0.0f;
					bool bWaterUnderPedWaves = CheckForWaterUnderPed(GetPed(), m_pVehicle, ms_Tunables.m_GroundFixupHeightBoatInWater, 1.5f, fWaterHeightWaves, true);
					if(bWaterUnderPedWaves && fWaterHeightWaves < fWaterHeight)
					{
						fWaterHeight = fWaterHeightWaves; 
					}

					vTargetPosition.z = fWaterHeight - ms_Tunables.m_ExtraWaterZGroundFixup;
					m_bGroundFixupApplied = true;
				}
			}
		}
	}

	m_PlayAttachedClipHelper.SetTarget(vTargetPosition, qTargetOrientation);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicleSeat::CheckStreamedExitConditions()
{
	const CVehicleClipRequestHelper* pClipRequestHelper = CTaskVehicleFSM::GetVehicleClipRequestHelper(GetPed());
	const CConditionalClipSet* pClipSet = pClipRequestHelper ? pClipRequestHelper->GetConditionalClipSet() : NULL;

	// Check the requested anims are for the vehicle and entry point we're entering and the clipset conditions are still valid
	if (pClipSet)
	{
		if(pClipSet->GetIsSkyDive())
		{
			if (CheckForSkyDive(GetPed(), m_pVehicle, m_iExitPointIndex))
			{
				static_cast<CTaskExitVehicle*>(GetParent())->SetRunningFlag(CVehicleEnterExitFlags::IsTransitionToSkyDive);
				return true;
			}
		}
		else if(pClipSet->GetIsProvidingCover())
		{
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicleSeat::CheckForSkyDive(const CPed *pPed, const CVehicle *pVehicle, s32 nEntryPoint, bool bAllowDiveIntoWater)
{
	bool bAllowParachuting = CTaskParachute::IsPedInStateToParachute(*pPed);

	if (bAllowParachuting && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ForceExitToSkyDive))
	{
		return true;
	}
	else if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableExitToSkyDive))
	{
		return false;
	}
	else
	{
		float fHeightAboveWater = -1.0f;
		const bool bWantsToDiveIntoWater = CheckForDiveIntoWater(pPed, pVehicle, bAllowParachuting, fHeightAboveWater);
		if(bAllowDiveIntoWater && bWantsToDiveIntoWater)
		{
			return true;
		}
		else if(bAllowParachuting)
		{
			TUNE_GROUP_FLOAT (EXIT_VEHICLE_TUNE, fMinHeightAboveWaterToAllowParachute, 40.0f, 0.0f, 100.0f, 0.1f);
			if (fHeightAboveWater > 0.0f && fHeightAboveWater < fMinHeightAboveWaterToAllowParachute)
			{
				return false;
			}
			Vector3 vEntryPointPos(Vector3::ZeroType);
			Quaternion qEntryOrientation(Quaternion::IdentityType);
			CModelSeatInfo::CalculateEntrySituation(pVehicle, pPed, vEntryPointPos, qEntryOrientation, nEntryPoint);

			// Add additional height to top of probe for cases where the entry point is completely under the ground
			TUNE_GROUP_FLOAT(VEHICLE_EXIT_TUNE, EXTRA_HEIGHT_FOR_SKYDIVE_PROBE, 1.0f, 0.0f, 10.0f, 0.01f);
			vEntryPointPos.z += EXTRA_HEIGHT_FOR_SKYDIVE_PROBE;
			float fSkydiveProbeDistance = ms_Tunables.m_SkyDiveProbeDistance + EXTRA_HEIGHT_FOR_SKYDIVE_PROBE;

			// B*2070676: Do probe test using peds velocity as opposed to doing a vertical test. This gives us a more accurate representation of where the ped will fall. 
			Vector3 vProbeEndPos = pPed->GetVelocity();
			vProbeEndPos.Normalize();
			vProbeEndPos.Scale(fSkydiveProbeDistance);
			vProbeEndPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) + vProbeEndPos;	
			// As before, set the Z component to be the defined probe distance downwards from the peds position. Can't just use the peds velocity here as it sometimes is still going upwards when we jump off the vehicle.
			vProbeEndPos.z = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()).z - fSkydiveProbeDistance;

			return CTaskExitVehicle::IsVerticalProbeClear(vEntryPointPos, fSkydiveProbeDistance, *pPed, NULL, vProbeEndPos);
		}
	}

	return false;
}

bool CTaskExitVehicleSeat::CheckForDiveIntoWater(const CPed *pPed, const CVehicle *pVehicle, bool bCanParachute, float& fHeightOverWater)
{
	TUNE_GROUP_FLOAT (EXIT_VEHICLE_TUNE, fWaterSearchProbeLength, 9999.0f, 0.0f, 10000.0f, 0.0001f);
	TUNE_GROUP_FLOAT (EXIT_VEHICLE_TUNE, fWaterDepthRequired, 2.0f, 0.0f, 10000.0f, 0.0001f);
	TUNE_GROUP_FLOAT (EXIT_VEHICLE_TUNE, fPedHeightAboveWaterForDiveMin, 2.0f, 0.0f, 10000.0f, 0.0001f);
	TUNE_GROUP_FLOAT (EXIT_VEHICLE_TUNE, fPedHeightAboveWaterForDiveMax, 50.0f, 0.0f, 10000.0f, 0.0001f);
	
	// Don't dive out of ejector seats - looks mental.
	if(IsEjectorSeat(pVehicle))
	{
		return false;
	}
	
	float fWaterHeight = 0.0f;
	bool bWaterUnderPed = CheckForWaterUnderPed(pPed, pVehicle, fWaterSearchProbeLength, fWaterDepthRequired, fWaterHeight);

	if (bWaterUnderPed)
	{
		fHeightOverWater = pPed->GetTransform().GetPosition().GetZf() - fWaterHeight;
	}
	else
	{
		fHeightOverWater = -1.0f;
	}

	if(bWaterUnderPed && (pPed->GetTransform().GetPosition().GetZf() > (fWaterHeight + fPedHeightAboveWaterForDiveMin)))
	{
		if(bCanParachute)
		{
			//! If we have a parachute, to still dive, we need to be able to dive from this height, and be less than a certain height
			//! above water.
			if((pPed->GetTransform().GetPosition().GetZf() < (fWaterHeight + fPedHeightAboveWaterForDiveMax)) &&
				(CPlayerInfo::GetDiveHeightForPed(*pPed) > (pPed->GetTransform().GetPosition().GetZf() - fWaterHeight)) )
			{
				return true;
			}
		}
		else
		{
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicleSeat::StoreInitialOffsets()
{
	// Store the offsets to the seat situation so we can recalculate the starting situation each frame relative to the seat
	Vector3 vTargetPosition(Vector3::ZeroType);
	Quaternion qTargetOrientation(Quaternion::IdentityType);

	CModelSeatInfo::CalculateSeatSituation(m_pVehicle, vTargetPosition, qTargetOrientation, m_iExitPointIndex);
	m_PlayAttachedClipHelper.SetTarget(vTargetPosition, qTargetOrientation);
	m_PlayAttachedClipHelper.SetInitialOffsets(*GetPed());
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicleSeat::IsGoingToExitOnToVehicle() const 
{
	const CVehicle& rVeh = *m_pVehicle;

	if(rVeh.IsTank() || rVeh.InheritsFromSubmarine())
	{
		return true;
	}

	if(m_pVehicle->InheritsFromBoat() && !rVeh.GetIsJetSki())
	{
		const CVehicleEntryPointAnimInfo* pClipInfo = m_pVehicle->IsEntryIndexValid(m_iExitPointIndex) ? m_pVehicle->GetEntryAnimInfo(m_iExitPointIndex) : NULL;
		bool bHasGetInFromWater = pClipInfo ? pClipInfo->GetHasGetInFromWater() : false;

		bool bHasExitBoatAnim = bHasGetInFromWater;

		if(bHasExitBoatAnim)
		{
			const CPed& rPed = *GetPed();

			//! On land, we play a get out onto land (unless the entry point is blocked)
			if(IsRunningFlagSet(CVehicleEnterExitFlags::InWater) && rPed.IsPlayer())
			{
				//! If out common point is blocked by someone else exiting vehicle, don't get out.
				s32 iShuffleSeat = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetShuffleSeatForSeat(m_iExitPointIndex, m_iSeatIndex);
				if(iShuffleSeat != -1)
				{
					CPed* pSeatOccupier = m_pVehicle->GetSeatManager()->GetPedInSeat(iShuffleSeat);
					if(pSeatOccupier)
					{
						CTaskExitVehicle* pExitVehicleTask = static_cast<CTaskExitVehicle*>(pSeatOccupier->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE));
						if (pExitVehicleTask && pExitVehicleTask->IsRunningFlagSet(CVehicleEnterExitFlags::ExitSeatOntoVehicle))
						{
							return false;
						}
					}
				}
				
				bool bGiveScubaGear = rPed.GetPlayerInfo() && rPed.GetPlayerInfo()->GetAutoGiveScubaGearWhenExitVehicle() &&
					m_pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_GIVE_SCUBA_GEAR_ON_EXIT);

				if(bGiveScubaGear)
				{
					return false;
				}
			}
			else
			{
				return false;
			}
		}

		return true;
	}

	const CVehicleEntryPointAnimInfo* pClipInfo = m_pVehicle->IsEntryIndexValid(m_iExitPointIndex) ? m_pVehicle->GetEntryAnimInfo(m_iExitPointIndex) : NULL;
	if (pClipInfo && pClipInfo->GetHasGetOutToVehicle())
	{
		return true;
	}

	return false;
}

bool CTaskExitVehicleSeat::IsEjectorSeat(const CVehicle *pVehicle)
{
	return pVehicle && pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_EJECTOR_SEATS);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicleSeat::SetClipsForState()
{
	const s32 iState = GetState();
	fwMvClipSetId clipSetId = m_pParentNetworkHelper->GetClipSetId();
	fwMvClipId clipId = CLIP_ID_INVALID; 
	fwMvClipId moveClipId = CLIP_ID_INVALID;
	bool bValidState = false;

	switch (iState)
	{
		case State_ExitSeat:	
			{
				float fBlendDuration = ms_Tunables.m_DefaultGetOutBlendDuration;
				clipId = ms_Tunables.m_DefaultGetOutClipId;	
				moveClipId = ms_ExitSeatClipId; 

				if (IsRunningFlagSet(CVehicleEnterExitFlags::InWater))
				{
					if(IsRunningFlagSet(CVehicleEnterExitFlags::ExitSeatOntoVehicle))
					{
						clipId = ms_Tunables.m_DefaultGetOutOnToVehicleClipId;	
					}
					else 
					{
						clipId = ms_Tunables.m_DefaultGetOutToWaterClipId;	
					}
				}
				else if (m_pVehicle->InheritsFromPlane())
				{
					// TODO: Put this on a flag
					static const u32 LAYOUT_PLANE_CUBAN = ATSTRINGHASH("LAYOUT_PLANE_CUBAN", 0xef95b6bf);
					static const u32 LAYOUT_PLANE_DUSTER = ATSTRINGHASH("LAYOUT_PLANE_DUSTER", 0x356f8468);
					const u32 uVehicleLayoutHash = m_pVehicle->GetLayoutInfo()->GetName().GetHash();
					if (uVehicleLayoutHash == LAYOUT_PLANE_CUBAN || uVehicleLayoutHash == LAYOUT_PLANE_DUSTER)
					{
						const CPlane* pPlane = static_cast<const CPlane*>(m_pVehicle.Get());
						if (!pPlane->IsEntryPointUseable(m_iExitPointIndex))
						{
							clipId = ms_Tunables.m_DefaultGetOutNoWingId;	
							fBlendDuration = ms_Tunables.m_DefaultGetOutNoWindBlendDuration;
						}
					}
				}
				m_pParentNetworkHelper->SetFloat(ms_ExitSeatBlendDurationId, fBlendDuration);
				bValidState = true;
			}
			break;
		case State_JumpOutOfSeat:
			{
				if (IsRunningFlagSet(CVehicleEnterExitFlags::DeadFallOut))
				{
					clipId = ms_Tunables.m_DeadFallOutClipId;
				}
				else
				{
					clipId = ms_Tunables.m_DefaultJumpOutClipId;	
				}
				moveClipId = ms_JumpOutClipId; 
				bValidState = true;
			}
			break;
		case State_BeJacked:
			{
				// TODO: Move ai logic outa here
				CPed* pPedJackingMe = NULL;
				CTaskExitVehicle* pExitVehicleTask = static_cast<CTaskExitVehicle*>(GetParent());
				if (pExitVehicleTask)
				{
					pPedJackingMe = pExitVehicleTask->GetPedsJacker();
				}

				const CPed& rPed = *GetPed();
				s32 iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(&rPed);
				const CVehicleSeatAnimInfo* pSeatClipInfo = m_pVehicle->GetSeatAnimationInfo(iSeatIndex);
				bool bIsOnTurretStandingJack = false;
				if (pSeatClipInfo->IsTurretSeat() && pPedJackingMe)
				{
					if (pPedJackingMe->GetIsInVehicle())
					{
						m_pParentNetworkHelper->SendRequest(ms_BeJackedCustomClipRequestId);
						clipId = rPed.ShouldBeDead() ? ms_Tunables.m_CustomShuffleBeJackedDeadClipId : ms_Tunables.m_CustomShuffleBeJackedClipId;
						moveClipId = ms_Clip0Id;
						bValidState = true;
						break;
					}
					else if (CTaskVehicleFSM::CanPerformOnVehicleTurretJack(*m_pVehicle, iSeatIndex))
					{
						const CTaskEnterVehicle* pJackersEnterVehicleTask = static_cast<const CTaskEnterVehicle*>(pPedJackingMe->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
						bIsOnTurretStandingJack = pJackersEnterVehicleTask && pJackersEnterVehicleTask->IsOnVehicleEntry() && m_pVehicle->IsTurretSeat(iSeatIndex) && CTaskVehicleFSM::CanPerformOnVehicleTurretJack(*m_pVehicle, iSeatIndex);
						if (bIsOnTurretStandingJack)
						{
							const CVehicleEntryPointAnimInfo* pEntryAnimInfo = m_pVehicle->GetEntryAnimInfo(m_iExitPointIndex);
							if (pEntryAnimInfo && pEntryAnimInfo->GetEntryPointClipSetId(1) != CLIP_SET_ID_INVALID)
							{
								// Ideally the on vehicle jack positions would be real entry nodes
								clipSetId = pEntryAnimInfo->GetEntryPointClipSetId(1);
							}
						}
					}
				}

				bool bFromWater = IsRunningFlagSet(CVehicleEnterExitFlags::InWater);

				const CVehicleEntryPointAnimInfo* pClipInfo = m_pVehicle->IsEntryIndexValid(m_iExitPointIndex) ? m_pVehicle->GetEntryAnimInfo(m_iExitPointIndex) : NULL;
				bool bHasGetInFromWater = pClipInfo ? pClipInfo->GetHasGetInFromWater() : false;

				bool bZhaba = MI_CAR_ZHABA.IsValid() && m_pVehicle->GetModelIndex() == MI_CAR_ZHABA;
				// Ehh hack for now, don't have anims for jack to water for boats
				if (((m_pVehicle->InheritsFromBoat() || m_pVehicle->pHandling->GetSeaPlaneHandlingData()) && m_pVehicle->GetVehicleModelInfo() && !bHasGetInFromWater) || bZhaba)
				{
					bFromWater = false;
				}

				bool bOnVehicleJack = IsRunningFlagSet(CVehicleEnterExitFlags::UseOnVehicleJack);

				m_pParentNetworkHelper->SetFlag(bFromWater, ms_ToWaterFlagId);
				const bool bIsDead = rPed.ShouldBeDead() ? true : false;

				//Check if we can force a flee.
				bool bCanForceFlee = (rPed.PopTypeIsRandom() && !rPed.IsLawEnforcementPed());

				//Check if we should force flee.
				bool bDoesJackerHaveGun = (pPedJackingMe && pPedJackingMe->GetWeaponManager() && pPedJackingMe->GetWeaponManager()->GetEquippedWeapon()
					&& pPedJackingMe->GetWeaponManager()->GetEquippedWeapon()->GetWeaponInfo()->GetIsGun());
				bool bForceFlee = (bCanForceFlee && bDoesJackerHaveGun);
		
#if ENABLE_MOTION_IN_TURRET_TASK
				if (bIsOnTurretStandingJack)
				{
					const CVehicleEntryPointInfo& rEntryInfo = *m_pVehicle->GetEntryInfo(m_iExitPointIndex);
					if (CTaskMotionInTurret::ShouldUseAlternateJackDueToTurretOrientation(*m_pVehicle, rEntryInfo.GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT))
					{
						clipId = bIsDead ? ms_Tunables.m_DefaultBeJackedDeadPedFromOutsideAltClipId : ms_Tunables.m_DefaultBeJackedAlivePedFromOutsideAltClipId;
					}
					else
					{
						clipId = bIsDead ? ms_Tunables.m_DefaultBeJackedDeadPedFromOutsideClipId : ms_Tunables.m_DefaultBeJackedAlivePedFromOutsideClipId;
					}
				}
				else 
#endif // ENABLE_MOTION_IN_TURRET_TASK
				if(bOnVehicleJack)
				{	
					if(bFromWater)
					{
						clipId = bIsDead ? ms_Tunables.m_DefaultBeJackedDeadPedOnVehicleIntoWaterClipId : ms_Tunables.m_DefaultBeJackedAlivePedOnVehicleIntoWaterClipId;
					}
					else
					{
						clipId = bIsDead ? ms_Tunables.m_DefaultBeJackedDeadPedOnVehicleClipId : ms_Tunables.m_DefaultBeJackedAlivePedOnVehicleClipId;
					}
				}
				else if (bFromWater)
				{
					clipId = bIsDead ? ms_Tunables.m_DefaultBeJackedDeadPedFromWaterClipId : ms_Tunables.m_DefaultBeJackedAlivePedFromWaterClipId;
				}
				else
				{
					clipId = ms_Tunables.m_DefaultBeJackedAlivePedFromOutsideClipId;

					//! If a dead clip exists, use it.
					if(bIsDead && fwClipSetManager::GetClip(clipSetId, ms_Tunables.m_DefaultBeJackedDeadPedFromOutsideClipId))
					{
						clipId = ms_Tunables.m_DefaultBeJackedDeadPedFromOutsideClipId;
					}

					if (!bIsDead && pPedJackingMe && !IsRunningFlagSet(CVehicleEnterExitFlags::CombatEntry) && !IsRunningFlagSet(CVehicleEnterExitFlags::SwitchedPlacesWithJacker))
					{
						bool bCachedForcedFlee = bForceFlee;
						GetAlternateClipInfoForPed(*pPedJackingMe, clipSetId, clipId, bForceFlee);
						
						// If we decided to force a flee initially, make sure this is set so the ped flees afterwards
						if (!bCanForceFlee)
						{
							bForceFlee = false;
						}
						else if (bCanForceFlee && bCachedForcedFlee)
						{
							bForceFlee = true;
						}
					}
				}


				// Hack, hack, hack B*1560213
				if (!bIsDead && bCanForceFlee && rPed.PopTypeIsRandom() && pPedJackingMe && pPedJackingMe->IsLocalPlayer() && pPedJackingMe->GetPedsGroup() && pPedJackingMe->GetPedsGroup()->GetGroupMembership()->CountMembersExcludingLeader() > 0)
				{
					static const u32 LAYOUT_BUS_HASH = ATSTRINGHASH("LAYOUT_BUS", 0xa1cdf1da);
					static const u32 LAYOUT_TOURBUS_HASH = ATSTRINGHASH("LAYOUT_TOURBUS", 0x9a1237a6);
					static const u32 LAYOUT_COACH_HASH = ATSTRINGHASH("LAYOUT_COACH", 0xb050699b);

					const u32 uVehicleLayoutHash = m_pVehicle->GetLayoutInfo()->GetName().GetHash();
					if (uVehicleLayoutHash == LAYOUT_BUS_HASH || uVehicleLayoutHash == LAYOUT_TOURBUS_HASH || uVehicleLayoutHash == LAYOUT_COACH_HASH )
					{
						bForceFlee = true;
					}
				}

				if (pPedJackingMe)
				{
					// Add shocking event so that peds can react to the jacking. 
					CEventShockingSeenCarStolen ev(*pPedJackingMe,m_pVehicle.Get());
					CShockingEventsManager::Add(ev);

					// Note that this ped was jacked out of its vehicle.
					GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_JackedOutOfMyVehicle, true);

					if(pPedJackingMe->IsPlayer())
					{
						REPLAY_ONLY(ReplayBufferMarkerMgr::AddMarker(3000,2000,IMPORTANCE_LOW);)
					}
				}

				//Check if the exit vehicle task is valid.
				if(pExitVehicleTask)
				{
					//Check if we should force flee.
					if(bForceFlee)
					{
						taskAssert(bCanForceFlee);

						//Set the flag.
						pExitVehicleTask->SetRunningFlag(CVehicleEnterExitFlags::ForceFleeAfterJack);
					}
				}

				moveClipId = ms_ExitSeatClipId; 
				bValidState = true;
			}
			break;
		case State_StreamedBeJacked:
			{
				//TODO: Remove AI stuff.

				CPed* pPedJackingMe = NULL;
				CTaskExitVehicle* pExitVehicleTask = static_cast<CTaskExitVehicle*>(GetParent());
				if (pExitVehicleTask)
				{
					pPedJackingMe = pExitVehicleTask->GetPedsJacker();
				}

				const CPed& rPed = *GetPed();

				//Check if we can force a flee.
				bool bCanForceFlee = (rPed.PopTypeIsRandom() && !rPed.IsLawEnforcementPed());

				//Check if we should force flee.
				bool bDoesJackerHaveGun = (pPedJackingMe && pPedJackingMe->GetWeaponManager() && pPedJackingMe->GetWeaponManager()->GetEquippedWeapon()
					&& pPedJackingMe->GetWeaponManager()->GetEquippedWeapon()->GetWeaponInfo()->GetIsGun());
				bool bForceFlee = (bCanForceFlee && bDoesJackerHaveGun);

				//Check if the exit vehicle task is valid.
				if(pExitVehicleTask)
				{
					//Check if we should force flee.
					if(bForceFlee)
					{
						taskAssert(bCanForceFlee);

						//Set the flag.
						pExitVehicleTask->SetRunningFlag(CVehicleEnterExitFlags::ForceFleeAfterJack);
					}
				}

				clipSetId = static_cast<CTaskExitVehicle*>(GetParent())->GetStreamedExitClipsetId();
				clipId = static_cast<CTaskExitVehicle*>(GetParent())->GetStreamedExitClipId();
				moveClipId = ms_StreamedBeJackedClipId; 
				bValidState = true;
			}
			break;
		case State_StreamedExit:
			{
				clipSetId = static_cast<CTaskExitVehicle*>(GetParent())->GetStreamedExitClipsetId();
				clipId = static_cast<CTaskExitVehicle*>(GetParent())->GetStreamedExitClipId();
				fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
				if (taskVerifyf(pClipSet && pClipSet->GetClipItemCount() >= 0, "Expect non null clipset and at least one clip item"))
				{
					if(clipId.IsNull())
					{
						s32 iRandomClipIndex = fwRandom::GetRandomNumberInRange(0, pClipSet->GetClipItemCount());
						clipId = pClipSet->GetClipItemIdByIndex(iRandomClipIndex);
					}
					moveClipId = ms_StreamedExitClipId; 
					bValidState = true;
				}
			}
			break;
		default: break;
	}

	if (bValidState)
	{
		taskAssert(clipSetId != CLIP_SET_ID_INVALID);
		const crClip* pClip = fwClipSetManager::GetClip(clipSetId, clipId);
		if (!taskVerifyf(pClip, "Couldn't find clip %s from clipset %s", clipId.GetCStr(), clipSetId.GetCStr()))
		{
			return false;
		}

		// Try to syncronise the local peds be jacked anim to the clone peds jacking anim by setting the initial phase to
		// match the clone ped's jacking clip current time
		if (iState == State_BeJacked)
		{
			CPed& rPed = *GetPed();
			CPed* pJacker = static_cast<CTaskExitVehicle*>(GetParent())->GetPedsJacker();
			if (pJacker && rPed.IsNetworkClone())
			{
				const CTaskEnterVehicle* pEnterTask = static_cast<const CTaskEnterVehicle*>(pJacker->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
				if (pEnterTask && pEnterTask->GetMoveNetworkHelper() && pEnterTask->GetMoveNetworkHelper()->IsNetworkActive())
				{
					const crClip* pJackerClip = pEnterTask->GetMoveNetworkHelper()->GetClip(CTaskEnterVehicle::ms_JackPedClipId);
					if (pJackerClip)
					{
						const float fJackerExitSeatPhase = Clamp(pEnterTask->GetMoveNetworkHelper()->GetFloat(CTaskEnterVehicle::ms_JackPedPhaseId), 0.0f, 1.0f);
						if (fJackerExitSeatPhase > 0.0f)
						{
							// The clips may not be the same duration so we can't just use the phase directly
							const float fJackerExitSeatTimeSecs = pJackerClip->ConvertPhaseToTime(fJackerExitSeatPhase);
							TUNE_GROUP_FLOAT(EXIT_VEHICLE_TUNE, MAX_INITIAL_START_PHASE, 0.25f, 0.0f, 1.0f, 0.01f);
							const float fVictimExitSeatStartPhase = Clamp(pClip->ConvertTimeToPhase(fJackerExitSeatTimeSecs), 0.0f, MAX_INITIAL_START_PHASE);
							m_pParentNetworkHelper->SetFloat(ms_ExitSeatPhaseId, fVictimExitSeatStartPhase);
							rPed.SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true);
							aiDebugf1("Frame : %i, Ped %s(%p) Setting initial start phase %.2f", fwTimer::GetFrameCount(), rPed.GetModelName(), &rPed, fVictimExitSeatStartPhase);
						}
					}
				}
			}
		}

		if (CClipEventTags::FindProperty(pClip, CClipEventTags::LegsIk))
		{
			m_bHasIkAllowTags = true;
		}

		m_fLegIkBlendInPhase = -1.0f;

		if (iState == State_ExitSeat || iState == State_BeJacked || iState == State_StreamedBeJacked || iState == State_StreamedExit)
		{
			float fUnused;
			CClipEventTags::GetMoveEventStartAndEndPhases(pClip, CTaskVehicleFSM::ms_LegIkBlendInId, m_fLegIkBlendInPhase, fUnused);
		}

#if __ASSERT
		float fDetachStart, fDetachEnd;
		const bool bFoundDetachTag = CClipEventTags::GetMoveEventStartAndEndPhases(pClip, ms_DetachId, fDetachStart, fDetachEnd);

		if (iState == State_JumpOutOfSeat)
		{
			float fSwitchToNMPhase;
			if (CClipEventTags::FindEventPhase(pClip, CClipEventTags::CanSwitchToNm, fSwitchToNMPhase))
			{
				if (bFoundDetachTag)
				{
					taskAssertf(fSwitchToNMPhase <= fDetachStart, "ADD A BUG TO DEFAULT ANIM INGAME - Clip : %s, Clipset : %s - Tag %s start phase (%.2f) should come before tag %s's start phase (%.2f)", pClip->GetName(), clipSetId.GetCStr(), CClipEventTags::CanSwitchToNm.GetCStr(), fSwitchToNMPhase, ms_DetachId.GetCStr(), fDetachStart);
				}
			}
		}
#endif // __ASSERT

		taskAssert(moveClipId != CLIP_SET_ID_INVALID);
		m_pParentNetworkHelper->SetClip(pClip, moveClipId);
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicleSeat::SetClipPlaybackRate()
{
	if (!m_pParentNetworkHelper)
		return;

	const CTaskExitVehicle* pExitVehicleTask = static_cast<const CTaskExitVehicle*>(GetParent());
	if (!pExitVehicleTask)
		return;

	const s32 iState = GetState();
	float fClipRate = 1.0f;
	const CAnimRateSet& rAnimRateSet = m_pVehicle->GetLayoutInfo()->GetAnimRateSet();
	const CPed& rPed = *GetPed();

	if (iState == State_BeJacked || iState == State_StreamedBeJacked)
	{
		fClipRate = rAnimRateSet.m_CombatJackEntry.GetRate();
		m_pParentNetworkHelper->SetFloat(ms_BeJackedRateId, fClipRate);
		return;
	}
	else if (iState == State_StreamedExit)
	{
		TUNE_GROUP_FLOAT(EXIT_VEHICLE_TUNE, CLONE_SKYDIVE_RATE, 1.0f, 0.0f, 5.0f, 0.01f);
		const bool bUseCloneSkyDiveRate = rPed.IsNetworkClone() && static_cast<CTaskExitVehicle*>(GetParent())->IsFlagSet(CVehicleEnterExitFlags::IsTransitionToSkyDive);
		fClipRate = bUseCloneSkyDiveRate ? CLONE_SKYDIVE_RATE : 1.0f;
		m_pParentNetworkHelper->SetFloat(ms_StreamedExitRateId, fClipRate);
		return;
	}

	bool bShouldUseCombatExitRates = static_cast<CTaskExitVehicle*>(GetParent())->IsFlagSet(CVehicleEnterExitFlags::CombatEntry) || static_cast<CTaskExitVehicle*>(GetParent())->IsFlagSet(CVehicleEnterExitFlags::UseFastClipRate) || rPed.GetPedResetFlag(CPED_RESET_FLAG_UseFastEnterExitVehicleRates);

	if(rPed.IsNetworkClone())
	{
		bShouldUseCombatExitRates = m_bCloneShouldUseCombatExitRate;
	}
	else
	{		
		m_bCloneShouldUseCombatExitRate = bShouldUseCombatExitRates;
	}

	// Set the clip rate based on our conditions
	if (bShouldUseCombatExitRates)
	{
		fClipRate = rAnimRateSet.m_NoAnimCombatExit.GetRate();
	}
	else
	{
		fClipRate = rAnimRateSet.m_NormalExit.GetRate();
	}

	static u32 LAYOUT_BUS_HASH =  atStringHash("LAYOUT_BUS");
	static u32 LAYOUT_PRISON_BUS_HASH = atStringHash("LAYOUT_PRISON_BUS");
	static u32 LAYOUT_COACH_HASH = atStringHash("LAYOUT_COACH");

	const bool bIsBusLayout = m_pVehicle->GetLayoutInfo()->GetName() == LAYOUT_BUS_HASH;
	const bool bIsPrisonBusLayout = m_pVehicle->GetLayoutInfo()->GetName() == LAYOUT_PRISON_BUS_HASH;
	const bool bIsCoachLayout = m_pVehicle->GetLayoutInfo()->GetName() == LAYOUT_COACH_HASH;

	const CVehicleEntryPointInfo* pEntryInfo = m_pVehicle->IsEntryIndexValid(m_iExitPointIndex) ? m_pVehicle->GetEntryInfo(m_iExitPointIndex) : NULL;
	const bool bIsPlaneHatchEntry = pEntryInfo && pEntryInfo->GetIsPlaneHatchEntry();
	const bool bIsPlaneWithStairs = m_pVehicle->InheritsFromPlane() && (m_pVehicle->GetLayoutInfo()->GetClimbUpAfterOpenDoor() || bIsPlaneHatchEntry);

	if (bIsBusLayout || bIsPrisonBusLayout || bIsCoachLayout || bIsPlaneWithStairs)
		fClipRate *= CTaskEnterVehicleSeat::GetMultipleEntryAnimClipRateModifier(m_pVehicle, &rPed, true);

    m_fExitSeatClipPlaybackRate = fClipRate;
	// Update the move parameters
	m_pParentNetworkHelper->SetFloat(ms_ExitSeatRateId, fClipRate);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicleSeat::CheckAndFixIfPedWentThroughSomething()
{
	if(CTaskVehicleFSM::CheckIfPedWentThroughSomething(*GetPed(), *m_pVehicle))
	{
		GetPed()->SetPedOutOfVehicle(CPed::PVF_ExcludeVehicleFromSafetyCheck);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskExitVehicleSeat::GetAlternateClipInfoForPed(const CPed& jackingPed, fwMvClipSetId& clipSetId, fwMvClipId& clipId, bool& bForceFlee) const
{
	// CNC: Cops are using Michael's entry clipset (less aggressive anims), so we need to use the appropriate exit clipset to match.
	if (CTaskEnterVehicle::CNC_ShouldForceMichaelVehicleEntryAnims(jackingPed))
	{
		const CVehicleEntryPointAnimInfo* pEntryPointClipInfo = m_pVehicle->GetVehicleModelInfo()->GetEntryPointAnimInfo(m_iExitPointIndex);
		if (pEntryPointClipInfo)
		{
			const s32 iMichaelIndex = 0;
			fwFlags32 uInformationFlags = 0;
			fwMvClipSetId beJackedAlivePedClipSet = pEntryPointClipInfo->GetCustomPlayerJackingClipSet(iMichaelIndex, &uInformationFlags);
			if (beJackedAlivePedClipSet != CLIP_SET_ID_INVALID && CTaskVehicleFSM::IsVehicleClipSetLoaded(beJackedAlivePedClipSet))
			{
				fwMvClipId newClipId = pEntryPointClipInfo->GetAlternateBeJackedFromOutSideClipId();
				if (newClipId != CLIP_SET_ID_INVALID)
				{
					clipSetId = beJackedAlivePedClipSet;
					clipId = newClipId;
					bForceFlee = (uInformationFlags.IsFlagSet(CClipSetMap::IF_JackedPedExitsWillingly));
					return;
				}
			}
		}
	}

	if (!NetworkInterface::IsGameInProgress())
	{
		if (jackingPed.IsLocalPlayer())
		{
			s32 iPlayerIndex = CPlayerInfo::GetPlayerIndex(jackingPed);
			if (iPlayerIndex > -1)
			{
				const CVehicleEntryPointAnimInfo* pEntryPointClipInfo = m_pVehicle->GetVehicleModelInfo()->GetEntryPointAnimInfo(m_iExitPointIndex);
				if (pEntryPointClipInfo)
				{
					fwFlags32 uInformationFlags = 0;
					fwMvClipSetId beJackedAlivePedClipSet = pEntryPointClipInfo->GetCustomPlayerJackingClipSet(iPlayerIndex, &uInformationFlags);
					if (beJackedAlivePedClipSet != CLIP_SET_ID_INVALID && CTaskVehicleFSM::IsVehicleClipSetLoaded(beJackedAlivePedClipSet))
					{
						fwMvClipId newClipId = pEntryPointClipInfo->GetAlternateBeJackedFromOutSideClipId();
						if (newClipId != CLIP_SET_ID_INVALID)
						{
							clipSetId = beJackedAlivePedClipSet;
							clipId = newClipId;
							bForceFlee = (uInformationFlags.IsFlagSet(CClipSetMap::IF_JackedPedExitsWillingly));
						}
					}
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

const crClip* CTaskExitVehicleSeat::GetClipAndPhaseForState(float& fPhase) const
{
	if (m_pParentNetworkHelper && m_pParentNetworkHelper->IsNetworkActive())
	{
		switch (GetState())
		{
			case State_FleeExit:
				{
					fPhase = m_pParentNetworkHelper->GetFloat(ms_FleeExitPhaseId);
					fPhase = rage::Clamp(fPhase, 0.0f, 1.0f);
					return m_pParentNetworkHelper->GetClip(ms_FleeExitClipId);
				}
			case State_BeJacked: 
			case State_ExitSeat:
			case State_UpsideDownExit:
			case State_OnSideExit:
			{
				fPhase = m_pParentNetworkHelper->GetFloat( ms_ExitSeatPhaseId);
				fPhase = rage::Clamp(fPhase, 0.0f, 1.0f);
				return m_pParentNetworkHelper->GetClip( ms_ExitSeatClipId);
			}
			case State_ExitToAim:
				{
					fPhase = m_pParentNetworkHelper->GetFloat( CTaskVehicleFSM::ms_Clip2PhaseId);
					fPhase = rage::Clamp(fPhase, 0.0f, 1.0f);
					return m_pParentNetworkHelper->GetClip( CTaskVehicleFSM::ms_Clip2OutId);
				}
			case State_StreamedExit:
				{
					fPhase = m_pParentNetworkHelper->GetFloat( ms_StreamedExitPhaseId);
					fPhase = rage::Clamp(fPhase, 0.0f, 1.0f);
					return m_pParentNetworkHelper->GetClip( ms_StreamedExitClipId);
				}
			case State_StreamedBeJacked:
			{
				fPhase = m_pParentNetworkHelper->GetFloat( ms_StreamedBeJackedPhaseId);
				fPhase = rage::Clamp(fPhase, 0.0f, 1.0f);
				return m_pParentNetworkHelper->GetClip( ms_StreamedBeJackedClipId);
			}
			case State_JumpOutOfSeat:
			{
				fPhase = m_pParentNetworkHelper->GetFloat( ms_JumpOutPhaseId);
				fPhase = rage::Clamp(fPhase, 0.0f, 1.0f);
				return m_pParentNetworkHelper->GetClip( ms_JumpOutClipId);
			}
			case State_ThroughWindscreen:
				{
					fPhase = m_pParentNetworkHelper->GetFloat(ms_ThroughWindscreenPhaseId);
					fPhase = rage::Clamp(fPhase, 0.0f, 1.0f);
					return m_pParentNetworkHelper->GetClip(ms_ThroughWindscreenClipId);
				}
			case State_StreamAssets:
			case State_DetachedFromVehicle:
			case State_WaitForStreamedBeJacked:
			case State_WaitForStreamedExit:
			case State_Finish:
				break;
			default: taskAssertf(0, "Unhandled State");
		}
	}

	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskExitVehicleSeat::HasPassedInterruptTimeForCamera(float fTimeOffset) const
{
	if (GetState() == State_ExitSeat && m_fInterruptPhase > 0.0f)
	{
		float fCurrentPhase = 0.0f;
		const crClip* pClip = GetClipAndPhaseForState(fCurrentPhase);
		if (pClip)
		{
			const float fInterruptTime = rage::Clamp(pClip->ConvertPhaseToTime(m_fInterruptPhase) - fTimeOffset, 0.0f, 20.0f);
			const float fCurrentTime = pClip->ConvertPhaseToTime(fCurrentPhase);
			if (fCurrentTime >= fInterruptTime)
			{
				return true;
			}
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

const CTaskExitVehicleSeat::ExitToAimSeatInfo* CTaskExitVehicleSeat::GetExitToAimSeatInfo(const CVehicle* pVehicle, int iSeatIndex, int iExitPointIndex)
{
	if(!pVehicle)
	{
		return NULL;
	}

	const CVehicleSeatAnimInfo* pSeatAnimInfo = pVehicle->GetSeatAnimationInfo(iSeatIndex);
	if(pSeatAnimInfo)
	{
		atHashWithStringNotFinal seatAimInfoName = pSeatAnimInfo->GetExitToAimInfoName();

		int iNumVehicleAimInfos = ms_Tunables.m_ExitToAimVehicleInfos.GetCount();
		for(int i = 0; i < iNumVehicleAimInfos; i++)
		{
			// Check if the desired aim info name for this seat matches this aim vehicle info
			if(seatAimInfoName == ms_Tunables.m_ExitToAimVehicleInfos[i].m_Name)
			{
				int iNumVehicleSeatAimInfos = ms_Tunables.m_ExitToAimVehicleInfos[i].m_Seats.GetCount();
				for(int j = 0; j < iNumVehicleSeatAimInfos; j++)
				{
					// check if the seat position matches our peds seat position
					ExitToAimSeatInfo* pExitToAimSeatInfo = &ms_Tunables.m_ExitToAimVehicleInfos[i].m_Seats[j];
					if(pExitToAimSeatInfo->m_SeatPosition == iExitPointIndex)
					{
						return pExitToAimSeatInfo;
					}
				}
			}
		}
	}

	return NULL;
}

const CTaskExitVehicleSeat::ExitToAimClipSet* CTaskExitVehicleSeat::GetExitToAimClipSet(atHashString clipSetName)
{
	for(int i = 0; i < ms_Tunables.m_ExitToAimClipSets.GetCount(); i++)
	{
		if(clipSetName == ms_Tunables.m_ExitToAimClipSets[i].m_Name)
		{
			return &ms_Tunables.m_ExitToAimClipSets[i];
		}
	}

	return NULL;
}

float CTaskExitVehicleSeat::GetGroundFixupHeight(bool bUseLargerGroundFixup) const
{
	if(m_pVehicle->InheritsFromBoat() || (m_pVehicle->InheritsFromAmphibiousAutomobile() && m_pVehicle->GetIsInWater()))
	{
		const CBoat* pBoat = static_cast<const CBoat *>(m_pVehicle.Get());
		if(pBoat->m_BoatHandling.IsInWater())
		{
			return m_bGroundFixupApplied ? ms_Tunables.m_GroundFixupHeightBoatInWater : ms_Tunables.m_GroundFixupHeightBoatInWaterInitial;
		}
	}

	return bUseLargerGroundFixup ? ms_Tunables.m_GroundFixupHeightLarge : ms_Tunables.m_GroundFixupHeight;
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
void CTaskExitVehicleSeat::Debug() const
{
#if __BANK
	Vector3 vTarget;
	Quaternion qTarget;
	m_PlayAttachedClipHelper.GetTarget(vTarget, qTarget);
	Matrix34 mTargetMat;
	mTargetMat.FromQuaternion(qTarget);
	mTargetMat.d = vTarget;
	grcDebugDraw::Axis(mTargetMat, 0.5f, true);

#endif
}
#endif // !__FINAL

////////////////////////////////////////////////////////////////////////////////

const fwMvRequestId CTaskCloseVehicleDoorFromOutside::ms_CloseDoorRequestId("CloseDoor",0xECD1883E);
const fwMvBooleanId CTaskCloseVehicleDoorFromOutside::ms_CloseDoorOnEnterId("CloseDoor_OnEnter",0x40C4545);
const fwMvBooleanId CTaskCloseVehicleDoorFromOutside::ms_CloseDoorClipFinishedId("CloseDoorAnimFinished",0xA23C50F3);
const fwMvBooleanId CTaskCloseVehicleDoorFromOutside::ms_CloseDoorInterruptId("CloseDoorInterrupt",0x8644CD73);
const fwMvClipId CTaskCloseVehicleDoorFromOutside::ms_CloseDoorClipId("CloseDoorClip",0xD9C00C84);
const fwMvFloatId CTaskCloseVehicleDoorFromOutside::ms_CloseDoorPhaseId("CloseDoorPhase",0x3F197B37);
const fwMvFloatId CTaskCloseVehicleDoorFromOutside::ms_CloseDoorRateId("CloseDoorRate",0xDCC67D79);
const fwMvStateId CTaskCloseVehicleDoorFromOutside::ms_CloseDoorStateId("Close Door",0x62F3C36C);

////////////////////////////////////////////////////////////////////////////////

CTaskCloseVehicleDoorFromOutside::CTaskCloseVehicleDoorFromOutside(CVehicle* pVehicle, s32 iExitPointIndex)
: m_pParentNetworkHelper(NULL)
, m_pVehicle(pVehicle)
, m_iExitPointIndex(iExitPointIndex)
, m_bMoveNetworkFinished(false)
, m_bTurnedOnDoorHandleIk(false)
, m_bTurnedOffDoorHandleIk(false)
, m_bDontUseArmIk(false)
{
	SetInternalTaskType(CTaskTypes::TASK_CLOSE_VEHICLE_DOOR_FROM_OUTSIDE);
}

////////////////////////////////////////////////////////////////////////////////

CTaskCloseVehicleDoorFromOutside::~CTaskCloseVehicleDoorFromOutside()
{

}

////////////////////////////////////////////////////////////////////////////////

void CTaskCloseVehicleDoorFromOutside::CleanUp()
{
	GetPed()->DetachFromParent(DETACH_FLAG_EXCLUDE_VEHICLE);
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCloseVehicleDoorFromOutside::ProcessPreFSM()
{
	if (!taskVerifyf(m_pVehicle, "NULL vehicle pointer in CTaskCloseVehicleDoorFromOutside::ProcessPreFSM"))
	{
		return FSM_Quit;
	}

	if (!taskVerifyf(GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_EXIT_VEHICLE, "Invalid parent task"))
	{
		return FSM_Quit;
	}

	GetPed()->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );

	return FSM_Continue;
}	

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCloseVehicleDoorFromOutside::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)		
			FSM_OnUpdate
				return Start_OnUpdate();	

		FSM_State(State_CloseDoor)
			FSM_OnEnter
				return CloseDoor_OnEnter();
			FSM_OnUpdate
				return CloseDoor_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskCloseVehicleDoorFromOutside::ProcessPhysics(float fTimeStep, int nTimeSlice)
{
	// Base class
	return CTask::ProcessPhysics(fTimeStep, nTimeSlice);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskCloseVehicleDoorFromOutside::ProcessMoveSignals()
{
	if(m_bMoveNetworkFinished)
	{
		// Already finished, nothing to do.
		return false;
	}

	const int state = GetState();
	if(state == State_CloseDoor)
	{
		// Check if done running the close door Move state.
		if (IsMoveNetworkStateFinished(ms_CloseDoorClipFinishedId, ms_CloseDoorPhaseId))
		{
			// Done, we need no more updates.
			m_bMoveNetworkFinished = true;
			return false;
		}

		// Still waiting.
		return true;
	}

	// Nothing to do.
	return false;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCloseVehicleDoorFromOutside::Start_OnUpdate()
{
	if (!taskVerifyf(m_iExitPointIndex != -1 && m_pVehicle->GetEntryExitPoint(m_iExitPointIndex), "Invalid entry point index"))
	{
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	m_pParentNetworkHelper = static_cast<CTaskExitVehicle*>(GetParent())->GetMoveNetworkHelper();

	if (!m_pParentNetworkHelper)
	{
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	SetTaskState(State_CloseDoor);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCloseVehicleDoorFromOutside::CloseDoor_OnEnter()
{
	m_bDontUseArmIk = false;
	const CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(m_pVehicle, m_iExitPointIndex);
	TUNE_GROUP_FLOAT(EXIT_VEHICLE_TUNE, MAX_OPEN_DOOR_RATIO_TO_USE_IK, 0.8f, 0.0f, 1.0f, 0.01f);
	if (pDoor && pDoor->GetDoorRatio() < MAX_OPEN_DOOR_RATIO_TO_USE_IK)
	{
		m_bDontUseArmIk = true;
	}

	SetMoveNetworkState(ms_CloseDoorRequestId, ms_CloseDoorOnEnterId, ms_CloseDoorStateId);
	SetClipPlaybackRate();

	// Let ProcessMoveSignals() handle the communication with the Move network.
	m_bMoveNetworkFinished = false;
	RequestProcessMoveSignalCalls();

	// Ambient lookAts
	CPed* pPed = GetPed();
	if(pPed->IsAPlayerPed())
	{
		if(!GetSubTask())
		{
			CTaskExitVehicle* pExitTask = static_cast<CTaskExitVehicle*>(GetParent());
			if(!CTaskExitVehicle::IsPlayerTryingToAimOrFire(*pPed) && !pExitTask->GetWantsToEnterVehicle() && !pExitTask->GetWantsToEnterCover())
			{
				// Use "EMPTY" ambient context. We don't want any ambient anims to be played. Only lookAts.
				CTaskAmbientClips* pAmbientTask	= rage_new CTaskAmbientClips(0, CONDITIONALANIMSMGR.GetConditionalAnimsGroup(CTaskVehicleFSM::emptyAmbientContext.GetHash()));
				SetNewTask( pAmbientTask );
			}
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCloseVehicleDoorFromOutside::CloseDoor_OnUpdate()
{
	CPed* pPed = GetPed();

	// We need to force this update to make sure the door is closed
	pPed->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate|CPedAILod::AL_LodTimesliceAnimUpdate);
	pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);

	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	if (m_bMoveNetworkFinished)
	{
		AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(pPed, "[ExitVehicle] CTaskCloseVehicleDoorFromOutside::CloseDoor_OnUpdate : Ped %s earlying out from close door entry %i because move network finished\n", AILogging::GetDynamicEntityNameSafe(pPed), m_iExitPointIndex);
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	float fPhase = 0.0f;
	const crClip* pClip = GetClipAndPhaseForState(fPhase);

	bool bIsPlayerTryingToAimOrFire = CTaskExitVehicle::IsPlayerTryingToAimOrFire(*pPed);
	bool bIsWeaponSwitchRequired = (pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetRequiresWeaponSwitch());
	CTaskExitVehicle* pExitTask = static_cast<CTaskExitVehicle*>(GetParent());
	CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(m_pVehicle, m_iExitPointIndex);

	bool shouldFinish = false;
	if(m_pParentNetworkHelper->GetBoolean(ms_CloseDoorInterruptId))
	{
		shouldFinish = true;
	}
	else if(!pDoor)
	{
		shouldFinish = true;
	}
	else if(pDoor->GetIsClosed())
	{
		// If we are SuperDummy, we can't really trust GetIsClosed() - we don't update the doors
		// in that case, and the door may have been loaded with a target door ratio to be open.
		if(m_pVehicle->IsSuperDummy())
		{
			// If the target door ratio is small enough (see threshold in GetIsClosed()), we should be OK
			// to finish, the door should remain closed. We can also finish by getting the CloseDoorInterrupt
			// signal, but this check should cover us if we don't get that signal.
			if(pDoor->GetTargetDoorRatio() < 0.01f)
			{
				shouldFinish = true;
			}
		}
		else
		{
			shouldFinish = true;
		}
	}

	if (shouldFinish)
	{
		// Stop LookAt IK
		if(pPed->IsAPlayerPed())
		{
			static const u32 uLookAtMyVehicleHash = ATSTRINGHASH("LookAtMyVehicle", 0x79037202);
			pPed->GetIkManager().AbortLookAt(500, uLookAtMyVehicleHash);
		}

        // if this is a clone that has finished running the exit vehicle task quit
        if(pPed->IsNetworkClone() && !pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE))
        {
            SetTaskState(State_Finish);
			return FSM_Continue;
        }

		const bool bPlayerExitInterrupt = CTaskVehicleFSM::CheckForPlayerExitInterrupt(*pPed, *m_pVehicle, pExitTask->GetRunningFlags(), pExitTask->GetSeatOffset(), CTaskVehicleFSM::EF_AnyStickInputInterrupts);
		const bool bSequenceInterrupt = pExitTask->ShouldInterruptDueToSequenceTask();
		const bool bEnterExitCheck = CTaskVehicleFSM::CheckForEnterExitVehicle(*pPed);
		const bool bWantsToEnterVehicle = pExitTask->GetWantsToEnterVehicle();
		const bool bWantsToEnterCover = pExitTask->GetWantsToEnterCover();
		
		if (bPlayerExitInterrupt || bSequenceInterrupt || bEnterExitCheck || 
			bIsPlayerTryingToAimOrFire || (bIsWeaponSwitchRequired && !m_pVehicle->GetLayoutInfo()->GetAutomaticCloseDoor()) ||
			bWantsToEnterVehicle || bWantsToEnterCover)
		{
			if(bIsPlayerTryingToAimOrFire && bIsWeaponSwitchRequired)
			{
				pPed->GetWeaponManager()->CreateEquippedWeaponObject();
			}

			if (pExitTask->GetWantsToEnterVehicle())
				pPed->SetPedResetFlag(CPED_RESET_FLAG_WantsToEnterVehicleFromCover, true);
			else if (pExitTask->GetWantsToEnterCover())
				pPed->SetPedResetFlag(CPED_RESET_FLAG_WantsToEnterCover, true);

			if (pExitTask->GetWantsToEnterVehicle() || pExitTask->GetWantsToEnterCover())
				SetFlag(aiTaskFlags::ProcessNextActiveTaskImmediately);

			AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(pPed, "[ExitVehicle] CTaskCloseVehicleDoorFromOutside::CloseDoor_OnUpdate : Ped %s earlying out from close door entry %i, bPlayerExitInterrupt(%s), bSequenceInterrupt(%s), bEnterExitCheck(%s), bIsPlayerTryingToAimOrFire(%s), bIsWeaponSwitchRequired(%s), bWantsToEnterVehicle(%s), bWantsToEnterCover(%s)\n", 
				AILogging::GetDynamicEntityNameSafe(pPed), m_iExitPointIndex,
				AILogging::GetBooleanAsString(bPlayerExitInterrupt), AILogging::GetBooleanAsString(bSequenceInterrupt), AILogging::GetBooleanAsString(bEnterExitCheck), 
				AILogging::GetBooleanAsString(bIsPlayerTryingToAimOrFire), AILogging::GetBooleanAsString(bIsWeaponSwitchRequired),
				AILogging::GetBooleanAsString(bWantsToEnterVehicle), AILogging::GetBooleanAsString(bWantsToEnterCover));

			SetTaskState(State_Finish);
			return FSM_Continue;
		}
	}

	if (pClip)
	{
		CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(m_pVehicle, m_iExitPointIndex);
		if (pDoor)
		{
			static dev_float sfBeginCloseDoor = 0.1f;
			static dev_float sfEndCloseDoor = 0.5f;
			float fPhaseToBeginCloseDoor = sfBeginCloseDoor;
			float fPhaseToEndCloseDoor = sfEndCloseDoor;

			CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pClip, CClipEventTags::Door, CClipEventTags::Start, true, fPhaseToBeginCloseDoor);
			CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pClip, CClipEventTags::Door, CClipEventTags::Start, false, fPhaseToEndCloseDoor);
			CTaskVehicleFSM::DriveDoorFromClip(*m_pVehicle, pDoor, CTaskVehicleFSM::VF_CloseDoor, fPhase, fPhaseToBeginCloseDoor, fPhaseToEndCloseDoor);

			AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(pPed, "[ExitVehicle] CTaskCloseVehicleDoorFromOutside::CloseDoor_OnUpdate : Ped %s, Door target ratio for entry point %i = %.2f, current ratio = %.2f, phase = %.2f\n", AILogging::GetDynamicEntityNameSafe(pPed), m_iExitPointIndex, CTaskVehicleFSM::GetTargetDoorRatioForEntryPoint(*m_pVehicle, m_iExitPointIndex), CTaskVehicleFSM::GetDoorRatioForEntryPoint(*m_pVehicle, m_iExitPointIndex), fPhase);
			if (!m_pVehicle->GetLayoutInfo()->GetNoArmIkOnOutsideCloseDoor() && !m_bDontUseArmIk)
			{
				CTaskVehicleFSM::ProcessDoorHandleArmIkHelper(*m_pVehicle, *pPed, m_iExitPointIndex, pClip, fPhase, m_bTurnedOnDoorHandleIk, m_bTurnedOffDoorHandleIk);
			}
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

const crClip* CTaskCloseVehicleDoorFromOutside::GetClipAndPhaseForState(float& fPhase)
{
	if (m_pParentNetworkHelper)
	{
		switch (GetState())
		{
			case State_CloseDoor: 
				{
					fPhase = m_pParentNetworkHelper->GetFloat( ms_CloseDoorPhaseId);
					fPhase = rage::Clamp(fPhase, 0.0f, 1.0f);
					return m_pParentNetworkHelper->GetClip( ms_CloseDoorClipId);
				}
			case State_Start:
			case State_Finish:
				break;
			default: taskAssertf(0, "Unhandled State");
		}
	}

	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskCloseVehicleDoorFromOutside::SetClipPlaybackRate()
{
	if (!m_pParentNetworkHelper)
		return;

	const CTaskExitVehicle* pExitVehicleTask = static_cast<const CTaskExitVehicle*>(GetParent());
	if (!pExitVehicleTask)
		return;

	float fClipRate = 1.0f;
	const CAnimRateSet& rAnimRateSet = m_pVehicle->GetLayoutInfo()->GetAnimRateSet();

	const CPed& rPed = *GetPed();
	const bool bShouldUseCombatExitRates = static_cast<CTaskExitVehicle*>(GetParent())->IsFlagSet(CVehicleEnterExitFlags::CombatEntry) || static_cast<CTaskExitVehicle*>(GetParent())->IsFlagSet(CVehicleEnterExitFlags::UseFastClipRate) || rPed.GetPedResetFlag(CPED_RESET_FLAG_UseFastEnterExitVehicleRates);

	// Set the clip rate based on our conditions
	if (bShouldUseCombatExitRates)
	{
		fClipRate = rAnimRateSet.m_NoAnimCombatExit.GetRate();
	}
	else
	{
		fClipRate = rAnimRateSet.m_NormalExit.GetRate();
	}

	// Update the move parameters
	m_pParentNetworkHelper->SetFloat(ms_CloseDoorRateId, fClipRate);
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskCloseVehicleDoorFromOutside::GetStaticStateName( s32 iState )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
	case State_Start:							return "State_Start";
	case State_CloseDoor:						return "State_CloseDoor";
	case State_Finish:							return "State_Finish";
	default: taskAssert(0); 	
	}

	return "State_Invalid";
}

////////////////////////////////////////////////////////////////////////////////

void CTaskCloseVehicleDoorFromOutside::Debug() const
{
#if __BANK
	if (m_pVehicle)
	{
		// Look up bone index of target entry point
		s32 iBoneIndex = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromEntryPoint(m_iExitPointIndex);

		if (iBoneIndex > -1)
		{
			CCarDoor* pDoor = m_pVehicle->GetDoorFromBoneIndex(iBoneIndex);
			if (pDoor)
			{
				grcDebugDraw::AddDebugOutput(Color_green, "Door Ratio : %.4f", pDoor->GetDoorRatio());
				grcDebugDraw::AddDebugOutput(Color_green, "Target Door Ratio : %.4f", pDoor->GetTargetDoorRatio());
			}
		}
	}
#endif
}
#endif // !__FINAL

////////////////////////////////////////////////////////////////////////////////

CTaskReactToBeingJacked::CTaskReactToBeingJacked(CVehicle* pJackedVehicle, CPed* pJackedPed, const CPed* pJackingPed)
: m_pJackedVehicle(pJackedVehicle)
, m_pJackedPed(pJackedPed)
, m_pJackingPed(pJackingPed)
, m_uRunningFlags(0)
{
	SetInternalTaskType(CTaskTypes::TASK_REACT_TO_BEING_JACKED);
}

////////////////////////////////////////////////////////////////////////////////

CTaskReactToBeingJacked::~CTaskReactToBeingJacked()
{

}

////////////////////////////////////////////////////////////////////////////////

void CTaskReactToBeingJacked::CleanUp()
{

}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReactToBeingJacked::ProcessPreFSM()
{
	//Ensure the vehicle is valid.
	if(!m_pJackedVehicle)
	{
		return FSM_Quit;
	}
	
	//Ensure the jacking ped is valid.
	if(!m_pJackingPed)
	{
		return FSM_Quit;
	}

	//Process the flags.
	ProcessFlags();
	
	//Process the head tracking.
	ProcessHeadTracking();

	return FSM_Continue;
}	

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReactToBeingJacked::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();	

		FSM_State(State_WaitForJackToStart)
			FSM_OnEnter
				WaitForJackToStart_OnEnter();
			FSM_OnUpdate
				return WaitForJackToStart_OnUpdate();

		FSM_State(State_WaitForDriverExit)
			FSM_OnUpdate
				return WaitForDriverExit_OnUpdate();	

		FSM_State(State_ExitVehicleOnLeft)
			FSM_OnEnter
				return ExitVehicleOnLeft_OnEnter();
			FSM_OnUpdate
				return ExitVehicleOnLeft_OnUpdate();

		FSM_State(State_ExitVehicleOnRight)
			FSM_OnEnter
				return ExitVehicleOnRight_OnEnter();
			FSM_OnUpdate
				return ExitVehicleOnRight_OnUpdate();

		FSM_State(State_JumpOutOfVehicle)
			FSM_OnEnter
				return JumpOutOfVehicle_OnEnter();
			FSM_OnUpdate
				return JumpOutOfVehicle_OnUpdate();

		FSM_State(State_ThreatResponse)
			FSM_OnEnter
				ThreatResponse_OnEnter();
			FSM_OnUpdate
				return ThreatResponse_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReactToBeingJacked::Start_OnUpdate()
{
	const CPed* pPed = GetPed();

	//Check if we are random, and friendly with the jacking ped.
	if(pPed->PopTypeIsRandom() && pPed->GetPedIntelligence()->IsFriendlyWith(*m_pJackingPed) && !NetworkInterface::IsGameInProgress())
	{
		//We are not friends any more.
		pPed->GetPedIntelligence()->SetFriendlyException(m_pJackingPed);
	}

	//Count the peds in seats.
	m_iNumPedsInSeats = m_pJackedVehicle->GetSeatManager()->CountPedsInSeats(true);
	taskAssert(m_iNumPedsInSeats > 0);

    if (m_pJackingPed && pPed->IsLawEnforcementPed() && !pPed->IsPlayer() && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontInfluenceWantedLevel))
	{
		const CTaskEnterVehicle* pEnterVehicleTask = static_cast<const CTaskEnterVehicle*>(m_pJackingPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
		if (pEnterVehicleTask)
		{
			// Wait if we're the driver, or a ped behind the driver
			const s32 iTargetEntryPointIndex = pEnterVehicleTask->GetTargetEntryPoint();
			const s32 iMyDirectEntryPointIndex = m_pJackedVehicle->GetDirectEntryPointIndexForPed(pPed);
			if (iTargetEntryPointIndex == iMyDirectEntryPointIndex || iTargetEntryPointIndex == CTaskVehicleFSM::FindFrontEntryForEntryPoint(m_pJackedVehicle, iMyDirectEntryPointIndex))
			{
				SetTaskState(State_WaitForJackToStart);
				return FSM_Continue;
			}
		}
		SetTaskState(State_ThreatResponse);
		return FSM_Continue;
	}

	// If we're not in the vehicle being jacked, quit
	if (m_pJackedVehicle != pPed->GetMyVehicle())
	{
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	// If we're the passenger on a bike, get off
	CVehicle* pVehicle = pPed->GetMyVehicle();
	if (m_pJackingPed && pVehicle)
	{
		bool bJackingFromLeftSide = false;
		bool bVanJackClipUsed = false;
		
		CTaskEnterVehicle* pDriverEnterTask = static_cast<CTaskEnterVehicle*>(m_pJackingPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
		if (pDriverEnterTask)
		{
			s32 iEntryPointIndex = pDriverEnterTask->GetTargetEntryPoint();
			if (pVehicle->IsEntryIndexValid(iEntryPointIndex))
			{
				const CVehicleEntryPointAnimInfo* pClipInfo = pVehicle->GetEntryAnimInfo(iEntryPointIndex);
				//van jacking is a longer anim, so I don't want the same delay for the ped reply
				bVanJackClipUsed = pClipInfo && pClipInfo->GetName() == 471872909;

				s32 iPedsSeatIndex = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
				if (pVehicle->IsSeatIndexValid(iPedsSeatIndex))
				{
					const CVehicleEntryPointInfo* pEntryInfo = pVehicle->GetEntryInfo(iEntryPointIndex);
					if (pEntryInfo)
					{
						bJackingFromLeftSide = pEntryInfo->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT ? true : false;
						if ( (pVehicle->InheritsFromBike() || pVehicle->GetIsJetSki()) && pVehicle->GetDriver() != pPed)
						{
							s32 iOppositeEntryIndex = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iPedsSeatIndex, pVehicle, SA_directAccessSeat, true, !bJackingFromLeftSide);
							if (pVehicle->IsEntryIndexValid(iOppositeEntryIndex) && !CVehicle::IsEntryPointClearForPed(*pVehicle, *pPed, iOppositeEntryIndex))
							{
								if(m_pJackingPed->GetSpeechAudioEntity())
								{
									const_cast<audSpeechAudioEntity*>(m_pJackingPed->GetSpeechAudioEntity())->SetJackingSpeechInfo(GetPed(), pVehicle, !bJackingFromLeftSide, bVanJackClipUsed);
								}

								SetTaskState(State_WaitForDriverExit);
								return FSM_Continue;
							}
						}
					}
				}
			}

			if ( (pVehicle->InheritsFromBike() || pVehicle->GetIsJetSki()) && pVehicle->GetDriver() != pPed)
			{
				if(m_pJackingPed->GetSpeechAudioEntity())
				{
					const_cast<audSpeechAudioEntity*>(m_pJackingPed->GetSpeechAudioEntity())->SetJackingSpeechInfo(GetPed(), pVehicle, !bJackingFromLeftSide, bVanJackClipUsed);
				}

				SetTaskState(bJackingFromLeftSide ? State_ExitVehicleOnRight : State_ExitVehicleOnLeft);
				return FSM_Continue;
			}
		}

		if(m_pJackingPed->GetSpeechAudioEntity())
		{
			if(!pVehicle->m_nVehicleFlags.bIsBus || GetPed() == pVehicle->GetDriver())
				const_cast<audSpeechAudioEntity*>(m_pJackingPed->GetSpeechAudioEntity())->SetJackingSpeechInfo(GetPed(), pVehicle, !bJackingFromLeftSide, bVanJackClipUsed);
		}
	}

    if(pPed->IsPlayer())
    {
        SetTaskState(State_Finish);
    }
    else
    {
	    SetTaskState(State_WaitForJackToStart);
    }
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskReactToBeingJacked::WaitForJackToStart_OnEnter()
{
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Assert that the ped is not a player.
	taskAssertf(!pPed->IsPlayer(), "Invalid state for player ped reacting to being jacked!");
	
	//Check if the ped is driving the vehicle.
	if(m_pJackedVehicle->IsDriver(pPed))
	{
		//Assert that the vehicle is not a clone.
		taskAssert(!m_pJackedVehicle->IsNetworkClone());

		//We want to stop the vehicle.
		//Create the vehicle task.
		CTask* pVehicleTask = rage_new CTaskVehicleStop(0);

		//Start the vehicle task.
		m_pJackedVehicle->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, pVehicleTask, VEHICLE_TASK_PRIORITY_PRIMARY, false);
	}
}

CTask::FSM_Return CTaskReactToBeingJacked::WaitForJackToStart_OnUpdate()
{
	if(m_pJackingPed && m_pJackingPed->GetVehiclePedEntering() != m_pJackedVehicle)
	{
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	//Check if we have not tried to say audio.
	if(!m_uRunningFlags.IsFlagSet(RF_HasTriedToSayAudio))
	{
		//Check if the delay has expired.
		static dev_float s_fMinDelay = 0.0f;
		static dev_float s_fMaxDelay = 2.0f;
		float fDelay = GetPed()->GetRandomNumberInRangeFromSeed(s_fMinDelay, s_fMaxDelay);
		if(GetTimeInState() > fDelay)
		{
			//Check if the jacking ped is close to the vehicle.
			spdAABB bbox = m_pJackedVehicle->GetBaseModelInfo()->GetBoundingBox();
			Vec3V vPos = m_pJackedVehicle->GetTransform().UnTransform(m_pJackingPed->GetTransform().GetPosition());
			ScalarV scDistSq = bbox.DistanceToPointSquared(vPos);
			static dev_float s_fMinDistance = 0.25f;
			static dev_float s_fMaxDistance = 5.0f;
			float fDistance = GetPed()->GetRandomNumberInRangeFromSeed(s_fMinDistance, s_fMaxDistance);
			ScalarV scMaxDistSq = ScalarVFromF32(square(fDistance));
			if(IsLessThanAll(scDistSq, scMaxDistSq))
			{
				//Set the flag.
				m_uRunningFlags.SetFlag(RF_HasTriedToSayAudio);

				//Say the audio.
				GetPed()->NewSay("GENERIC_FRIGHTENED_MED");
			}
		}
	}

	static float s_fMaxTimeToWait				= 10.0f;
	static float s_fMaxTimeToWaitBlocked		= 15.0f;
	static float s_fMaxTimeToWaitIfCanceled		= 3.0f;

	bool bBlockingJackReaction = false;
	s32 iPedsSeatIndex = m_pJackedVehicle->GetSeatManager()->GetPedsSeatIndex(GetPed());
	const CVehicleEntryPointInfo* pEntryInfo = NULL;
	if (m_pJackedVehicle->IsSeatIndexValid(iPedsSeatIndex))
	{
		s32 iEntryIndex = m_pJackedVehicle->GetDirectEntryPointIndexForSeat(iPedsSeatIndex);
		if(iEntryIndex != -1 && taskVerifyf(iEntryIndex < m_pJackedVehicle->GetLayoutInfo()->GetNumEntryPoints(), "Found invalid entry index %i for vehicle %s, layout %s", iEntryIndex, m_pJackedVehicle->GetModelName(), m_pJackedVehicle->GetLayoutInfo()->GetName().GetCStr()))
		{
			pEntryInfo = m_pJackedVehicle->GetLayoutInfo()->GetEntryPointInfo(iEntryIndex);
			if(pEntryInfo && pEntryInfo->GetIsBlockingJackReactionUntilJackerIsReady())
			{
				bBlockingJackReaction = true;
			}
		}
	}

	bool bJackerAllowingReaction = false;
	if(bBlockingJackReaction)
	{
		taskAssert(pEntryInfo);

		bJackerAllowingReaction = m_pJackingPed->GetIsInVehicle();
		if(!bJackerAllowingReaction)
		{
			CTaskEnterVehicle* pDriverEnterTask = static_cast<CTaskEnterVehicle*>(m_pJackingPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
			if (pDriverEnterTask && m_pJackedVehicle->IsEntryIndexValid(pDriverEnterTask->GetTargetEntryPoint()))
			{
				const CVehicleEntryPointInfo* pDriverEntryInfo = m_pJackedVehicle->GetLayoutInfo()->GetEntryPointInfo(pDriverEnterTask->GetTargetEntryPoint());

				//! If no sides specified, just assume we are blocking all sides.
				if(pEntryInfo->GetNumBlockReactionSides() == 0 || !pDriverEntryInfo)
				{
					bJackerAllowingReaction = pDriverEnterTask->IsFlagSet(CVehicleEnterExitFlags::AllowBlockedJackReactions);
				}
				else
				{
					CVehicleEntryPointInfo::eSide eDriverEntrySide = pDriverEntryInfo->GetVehicleSide();

					//! Loop through list and see if we are blocking reactions from the drivers entry side.
					bool bFoundSide = false;
					for(int i = 0; i < pEntryInfo->GetNumBlockReactionSides(); ++i)
					{
						if(pEntryInfo->GetBlockReactionSide(i) == eDriverEntrySide)
						{
							bFoundSide = true;
						}
					}

					if(bFoundSide)
					{
						bJackerAllowingReaction = pDriverEnterTask->IsFlagSet(CVehicleEnterExitFlags::AllowBlockedJackReactions);
					}
					else
					{
						bJackerAllowingReaction = HasJackStarted() && !m_pJackedVehicle->IsDriver(GetPed());
					}
				}
			}
		}
	}
	else
	{
		bJackerAllowingReaction = HasJackStarted() && !m_pJackedVehicle->IsDriver(GetPed());
	}

	float fMaxTime = bBlockingJackReaction ? s_fMaxTimeToWaitBlocked : s_fMaxTimeToWait;

	//Check if we have exceeded the max time.
	if(GetTimeInState() > fMaxTime)
	{
		//Finish the task.
		SetTaskState(State_Finish);
		return FSM_Continue;
	}
	//Check if we have exceeded the max time if canceled.
	else if(!m_pJackingPed->GetIsEnteringVehicle(true) && !m_pJackingPed->GetIsInVehicle() && (GetTimeInState() > s_fMaxTimeToWaitIfCanceled))
	{
		//Finish the task.
		SetTaskState(State_Finish);
		return FSM_Continue;
	}
	//Check if the jack is allowed this frame.
	else if(bJackerAllowingReaction)
	{
		if(ShouldJumpOutOfVehicle())
		{
			//Move to threat response.
			SetTaskState(State_JumpOutOfVehicle);
		}
		else
		{
			bool bAllowThreatResponse = true;

			//Don't allow a threat response reaction if we're the driver and the jacking ped is entering
			if (m_pJackingPed && m_pJackingPed->GetIsAttachedInCar())
			{
				bAllowThreatResponse = false;
			}

			if (bAllowThreatResponse)
			{
				//Move to threat response.
				SetTaskState(State_ThreatResponse);
			}
		}
		return FSM_Continue;
	}
	
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReactToBeingJacked::WaitForDriverExit_OnUpdate()
{
	if (!m_pJackedVehicle->GetDriver())
	{
		SetTaskState(State_ExitVehicleOnRight);
		return FSM_Continue;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReactToBeingJacked::ExitVehicleOnLeft_OnEnter()
{
	VehicleEnterExitFlags vehicleFlags;
	vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::PreferLeftEntry);
	vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::GettingOffBecauseDriverJacked);
	SetNewTask(rage_new CTaskLeaveAnyCar(0, vehicleFlags));
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReactToBeingJacked::ExitVehicleOnLeft_OnUpdate()
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_LEAVE_ANY_CAR))
	{
        if(GetPed()->IsPlayer())
        {
            SetTaskState(State_Finish);
        }
        else
        {
		    SetTaskState(State_ThreatResponse);
        }
		return FSM_Continue;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReactToBeingJacked::ExitVehicleOnRight_OnEnter()
{
	VehicleEnterExitFlags vehicleFlags;
	vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::PreferRightEntry);
	vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::GettingOffBecauseDriverJacked);
	SetNewTask(rage_new CTaskLeaveAnyCar(0, vehicleFlags));
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReactToBeingJacked::ExitVehicleOnRight_OnUpdate()
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_LEAVE_ANY_CAR))
	{
		if(GetPed()->IsPlayer())
        {
            SetTaskState(State_Finish);
        }
        else
        {
		    SetTaskState(State_ThreatResponse);
        }
		return FSM_Continue;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReactToBeingJacked::JumpOutOfVehicle_OnEnter()
{
	VehicleEnterExitFlags vehicleFlags;
	vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::GettingOffBecauseDriverJacked);
	vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::JumpOut);
	SetNewTask(rage_new CTaskLeaveAnyCar(0, vehicleFlags));
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReactToBeingJacked::JumpOutOfVehicle_OnUpdate()
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_LEAVE_ANY_CAR))
	{
		if(GetPed()->IsPlayer())
		{
			SetTaskState(State_Finish);
		}
		else
		{
			SetTaskState(State_ThreatResponse);
		}
		return FSM_Continue;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskReactToBeingJacked::ThreatResponse_OnEnter()
{
    taskAssertf(!GetPed()->IsPlayer(), "Invalid state for player ped reacting to being jacked!");

	//Create the task.
	CTaskThreatResponse* pTask = rage_new CTaskThreatResponse(m_pJackingPed);

	//Check if we should force flee.
	bool bForceFlee = false;
	const CTaskEnterVehicle* pTaskEnterVehicle = static_cast<CTaskEnterVehicle *>(
		m_pJackingPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
	if(pTaskEnterVehicle && pTaskEnterVehicle->IsFlagSet(CVehicleEnterExitFlags::ForceFleeAfterJack))
	{
		bForceFlee = true;
	}
	else if(m_pJackedPed)
	{
		const CTaskExitVehicle* pTaskExitVehicle = static_cast<CTaskExitVehicle *>(
			m_pJackedPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE));
		if(pTaskExitVehicle && pTaskExitVehicle->IsFlagSet(CVehicleEnterExitFlags::ForceFleeAfterJack))
		{
			bForceFlee = true;
		}
	}
	if(bForceFlee)
	{
		pTask->SetThreatResponseOverride(CTaskThreatResponse::TR_Flee);
	}

	//Check if we can call the police.
	if(CanCallPolice())
	{
		pTask->GetConfigFlagsForFlee().SetFlag(CTaskSmartFlee::CF_CanCallPolice);
	}

	//Start the task.
	SetNewTask(pTask);
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReactToBeingJacked::ThreatResponse_OnUpdate()
{
	if (!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskReactToBeingJacked::CanCallPolice() const
{
	//Ensure we can call the police.
	if(!CTaskCallPolice::CanMakeCall(*GetPed(), m_pJackingPed))
	{
		return false;
	}

	//Ensure the chances are enforced.
	static float s_fChancesToCallPolice = 0.8f;
	float fChancesToCallPolice = s_fChancesToCallPolice / m_iNumPedsInSeats;
	return (GetPed()->GetRandomNumberInRangeFromSeed(0.0f, 1.0f) < fChancesToCallPolice);
}

bool CTaskReactToBeingJacked::HasJackStarted() const
{
	//Check if the jacking ped is running the enter vehicle task.
	const CTask* pTask = m_pJackingPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE);
	if(pTask)
	{
		//Check if the task is in the right state.
		switch(pTask->GetState())
		{
			case CTaskEnterVehicle::State_OpenDoor:
			{
				// Don't break out during open door for helicopters, open_door anims are long...
				if (m_pJackedVehicle->GetVehicleType() != VEHICLE_TYPE_HELI)
				{
					return true;
				}
				break;
			}
			case CTaskEnterVehicle::State_JackPedFromOutside:
			{
				return true;
			}
			default:
			{
				break;
			}
		}
	}

	//Check if the jacked ped is running the exit vehicle seat task.
	pTask = m_pJackedPed ? m_pJackedPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE_SEAT) : NULL;
	if(pTask)
	{
		//Check if the task is in the right state.
		switch(pTask->GetState())
		{
			case CTaskExitVehicleSeat::State_BeJacked:
			case CTaskExitVehicleSeat::State_StreamedBeJacked:
			{
				return true;
			}
			default:
			{
				break;
			}
		}
	}
	
	return false;
}

void CTaskReactToBeingJacked::ProcessFlags()
{
	//Check if we should disable unarmed drivebys.
	if(ShouldDisableUnarmedDrivebys())
	{
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_DisableUnarmedDrivebys, true);
	}

	//Check if we should panic.
	if(ShouldPanic())
	{
		//Set the panic flag.
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_PanicInVehicle, true);
	}
}

void CTaskReactToBeingJacked::ProcessHeadTracking()
{
	//Ensure the jacking ped is valid.
	const CPed* pJackingPed = m_pJackingPed;
	if(!pJackingPed)
	{
		return;
	}
	
	//Grab the ped.
	CPed* pPed = GetPed();

	//Look at the jacker.
	pPed->GetIkManager().LookAt(0, pJackingPed, 1000,
		BONETAG_HEAD, NULL, LF_WHILE_NOT_IN_FOV, 500, 500, CIkManager::IK_LOOKAT_HIGH);
}

bool CTaskReactToBeingJacked::ShouldDisableUnarmedDrivebys() const
{
	//Check the state.
	switch(GetState())
	{
		case State_ThreatResponse:
		{
			static dev_float s_fMaxTimeInState = 7.0f;
			if(GetTimeInState() > s_fMaxTimeInState)
			{
				return false;
			}

			break;
		}
		default:
		{
			break;
		}
	}

	return true;
}

bool CTaskReactToBeingJacked::ShouldHeadTrack() const
{
	//Check the state.
	switch(GetState())
	{
		case State_WaitForJackToStart:
		{
			return true;
		}
		default:
		{
			return false;
		}
	}
}

bool CTaskReactToBeingJacked::ShouldPanic() const
{
	//Check if we are friendly with the jacker.
	if(GetPed()->GetPedIntelligence()->IsFriendlyWith(*m_pJackingPed))
	{
		return false;
	}

	return true;
}

bool CTaskReactToBeingJacked::ShouldJumpOutOfVehicle() const
{
	//! If this is a boat, with multiple access points, get ped to jump out.
	if(m_pJackedVehicle->InheritsFromBoat())
	{
		s32 iPedsSeatIndex = m_pJackedVehicle->GetSeatManager()->GetPedsSeatIndex(GetPed());
		if (m_pJackedVehicle->IsSeatIndexValid(iPedsSeatIndex))
		{
			s32 iEntryIndex = m_pJackedVehicle->GetDirectEntryPointIndexForSeat(iPedsSeatIndex);
			if(iEntryIndex != -1)
			{
				const CVehicleEntryPointAnimInfo* pEntryAnimInfo = m_pJackedVehicle->GetEntryAnimInfo(iEntryIndex);
				if(pEntryAnimInfo && pEntryAnimInfo->GetUseSeatClipSetAnims())
				{
					return true;
				}
			}
		}
	}
		
	return false;
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
void CTaskReactToBeingJacked::Debug() const
{
	#if __BANK
	#endif // __BANK
}
#endif // !__FINAL



////////////////////////////////////////////////////////////////////////////////

// Statics
CTaskReactToBeingAskedToLeaveVehicle::Tunables CTaskReactToBeingAskedToLeaveVehicle::ms_Tunables;

IMPLEMENT_VEHICLE_TASK_TUNABLES(CTaskReactToBeingAskedToLeaveVehicle, 0xb6f0913d);

////////////////////////////////////////////////////////////////////////////////

CTaskReactToBeingAskedToLeaveVehicle::CTaskReactToBeingAskedToLeaveVehicle(CVehicle* pJackedVehicle, const CPed* pJackingPed)
: m_pJackedVehicle(pJackedVehicle)
, m_pJackingPed(pJackingPed)
{
	SetInternalTaskType(CTaskTypes::TASK_REACT_TO_BEING_ASKED_TO_LEAVE_VEHICLE);
}

////////////////////////////////////////////////////////////////////////////////

CTaskReactToBeingAskedToLeaveVehicle::~CTaskReactToBeingAskedToLeaveVehicle()
{

}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReactToBeingAskedToLeaveVehicle::ProcessPreFSM()
{
	if(!m_pJackedVehicle)
	{
		return FSM_Quit;
	}

	if(!m_pJackingPed)
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}	

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReactToBeingAskedToLeaveVehicle::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();	

		FSM_State(State_WaitForVehicleToLeave)
			FSM_OnEnter
				WaitForVehicleToLeave_OnEnter();
		FSM_OnUpdate
				return WaitForVehicleToLeave_OnUpdate();

		FSM_State(State_EnterAbandonedVehicle)
			FSM_OnEnter
			EnterAbandonedVehicle_OnEnter();
		FSM_OnUpdate
			return EnterAbandonedVehicle_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReactToBeingAskedToLeaveVehicle::Start_OnUpdate()
{

	SetState(State_WaitForVehicleToLeave);

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskReactToBeingAskedToLeaveVehicle::WaitForVehicleToLeave_OnEnter()
{
	CTaskMoveFaceTarget* pSubTask = rage_new CTaskMoveFaceTarget(m_pJackingPed.Get());
	SetNewTask(pSubTask);

}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReactToBeingAskedToLeaveVehicle::WaitForVehicleToLeave_OnUpdate()
{
	if(!m_pJackedVehicle->GetDriver())
	{
		SetState(State_EnterAbandonedVehicle);
	}

	Vector3 vPedPos = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
	Vector3 vDiff = vPedPos - VEC3V_TO_VECTOR3(m_pJackedVehicle->GetTransform().GetPosition());
	float fDistToPed = vDiff.Mag2();

	if(fDistToPed > rage::square(ms_Tunables.m_MaxDistanceToWatchVehicle) || GetTimeInState() >  ms_Tunables.m_MaxTimeToWatchVehicle || !GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finish);
	}
	
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskReactToBeingAskedToLeaveVehicle::EnterAbandonedVehicle_OnEnter()
{
	SetNewTask(rage_new CTaskEnterVehicle(m_pJackedVehicle, SR_Prefer, m_pJackedVehicle->GetDriverSeat()));
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReactToBeingAskedToLeaveVehicle::EnterAbandonedVehicle_OnUpdate()
{
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finish);
	}
	return FSM_Continue;
}
