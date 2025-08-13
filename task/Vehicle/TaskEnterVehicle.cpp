//
// Task/Vehicle/TaskEnterVehicle.h
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

// File header
#include "Task/Vehicle/TaskEnterVehicle.h"

// Rage Headers
#include "math/angmath.h"

// Game Headers
#include "ai/debug/system/AIDebugLogManager.h"
#include "ai/debug/types/TaskDebugInfo.h"
#include "ai/debug/types/VehicleDebugInfo.h"
#include "animation/AnimManager.h"
#include "animation/MovePed.h"
#include "Camera/CamInterface.h"
#include "Camera/Helpers/ControlHelper.h"
#include "Camera/cinematic/CinematicDirector.h"
#include "Camera/Gameplay/GameplayDirector.h"
#include "Control/Replay/ReplayBufferMarker.h"
#include "Debug/DebugScene.h"
#include "event/Events.h"
#include "event/EventDamage.h"
#include "event/EventShocking.h"
#include "event/EventWeapon.h"
#include "event/ShockingEvents.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "modelinfo/ModelSeatInfo.h"
#include "modelinfo/PedModelInfo.h"
#include "modelinfo/VehicleModelInfo.h"
#include "Network/Network.h"
#include "PedGroup/PedGroup.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedHelmetComponent.h"
#include "Peds/PedIntelligence.h"
#include "Physics/physics.h"
#include "Scene/World/GameWorld.h"
#include "script/script.h"
#include "script/script_channel.h"
#include "script/script_cars_and_peds.h"
#include "Task/Combat/TaskCombat.h"
#include "Task/Combat/TaskDamageDeath.h"
#include "Task/Default/TaskCuffed.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/General/TaskBasic.h"
#include "Task/General/TaskSecondary.h"
#include "Task/Motion/Locomotion/TaskHumanLocomotion.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Motion/Vehicle/TaskMotionInTurret.h"
#include "Task/Movement/TaskGoTo.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Movement/TaskSeekEntity.h"
#include "Task/Movement/Climbing/TaskVault.h"
#include "Task/Physics/TaskNMBrace.h"
#include "Task/Physics/TaskNMJumpRollFromRoadVehicle.h"
#include "Task/Animation/TaskAnims.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskCarAccessories.h"
#include "Task/Vehicle/TaskCarUtils.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Task/Vehicle/TaskGoToVehicleDoor.h"
#include "Task/Vehicle/TaskInVehicle.h"
#include "Task/Vehicle/TaskMountAnimal.h"
#include "Task/Vehicle/TaskVehicleBase.h"
#include "Task/Vehicle/TaskTryToGrabVehicleDoor.h"
#include "VehicleAi/Task/TaskVehicleAnimation.h"
#include "Vehicles/Bike.h"
#include "Vehicles/Planes.h"
#include "Vehicles/Train.h"
#include "Vehicles/Vehicle.h"
#include "Vehicles/Metadata/VehicleDebug.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "Vehicles/Metadata/VehicleLayoutInfo.h"
#include "Vehicles/Metadata/VehicleEntryPointInfo.h"
#include "Vehicles/VehicleFactory.h"
#include "vfx/vehicleglass/VehicleGlassManager.h"
#include "stats/StatsInterface.h"
#include "../System/Task.h"
#include "AI/Ambient/ConditionalAnimManager.h"

#include "control/replay/ReplaySettings.h"

#if GTA_REPLAY
#include "control/replay/Ped/PedPacket.h"
#endif

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

// Statics
CTaskEnterVehicle::Tunables CTaskEnterVehicle::ms_Tunables;
const u32 CTaskEnterVehicle::m_uEnterVehicleLookAtHash = ATSTRINGHASH("EnterVehicleLookAt", 0x1E1BCE8A);

IMPLEMENT_VEHICLE_TASK_TUNABLES(CTaskEnterVehicle, 0x8ef4fbbc);

////////////////////////////////////////////////////////////////////////////////

const fwMvBooleanId		CTaskEnterVehicle::ms_BlendInSteeringWheelIkId("BlendInSteeringWheelIk",0x10B2F2A);
const fwMvBooleanId		CTaskEnterVehicle::ms_JackClipFinishedId("JackAnimFinished",0x881D4C73);
const fwMvBooleanId		CTaskEnterVehicle::ms_PickOrPullUpClipFinishedId("PickOrPullUpAnimFinished",0xC7ADA81C);
const fwMvBooleanId		CTaskEnterVehicle::ms_SeatShuffleClipFinishedId("seatShuffleAnimFinished",0xD09C405);
const fwMvBooleanId		CTaskEnterVehicle::ms_AlignToEnterSeatOnEnterId("AlignToEnterSeat_OnEnter",0x408B35C1);
const fwMvBooleanId		CTaskEnterVehicle::ms_AlignToEnterSeatAnimFinishedId("AlignToEnterSeatAnimFinished",0x1F0114B3);
const fwMvBooleanId		CTaskEnterVehicle::ms_ClimbUpOnEnterId("ClimbUp_OnEnter",0x32CE5E9F);
const fwMvBooleanId		CTaskEnterVehicle::ms_ClimbUpAnimFinishedId("ClimbUpAnimFinished",0xC2C7F301);
const fwMvBooleanId		CTaskEnterVehicle::ms_JackPedFromOutsideOnEnterId("JackPedFromOutside_OnEnter",0xE6ABAA1);
const fwMvBooleanId		CTaskEnterVehicle::ms_PickUpBikeOnEnterId("PickUpBike_OnEnter",0x466057C4);
const fwMvBooleanId		CTaskEnterVehicle::ms_PullUpBikeOnEnterId("PullUpBike_OnEnter",0xCAB21D9);
const fwMvBooleanId		CTaskEnterVehicle::ms_StreamedEntryOnEnterId("StreamedEntry_OnEnter",0xE5816FF8);
const fwMvBooleanId		CTaskEnterVehicle::ms_StreamedEntryClipFinishedId("StreamedEntryAnimFinished",0x1CFBA502);
const fwMvBooleanId		CTaskEnterVehicle::ms_InterruptId("Interrupt",0x6D6BC7B7);
const fwMvBooleanId		CTaskEnterVehicle::ms_AlignOnEnterId("Align_OnEnter",0x7E4761D5);
const fwMvBooleanId		CTaskEnterVehicle::ms_AllowBlockedJackReactionsId("AllowBlockedJackReactions",0xdf508e88);
const fwMvClipId		CTaskEnterVehicle::ms_JackPedClipId("JackPedClip",0x6C92B1A2);
const fwMvClipId		CTaskEnterVehicle::ms_PickPullClipId("PickPullClip",0x9D3D0D9F);
const fwMvClipId		CTaskEnterVehicle::ms_SeatShuffleClipId("SeatShuffleClip",0x63D681F1);
const fwMvClipId		CTaskEnterVehicle::ms_AlignToEnterSeatClipId("AlignToEnterSeatClip",0xD118CCEC);
const fwMvClipId		CTaskEnterVehicle::ms_ClimbUpClipId("ClimbUpClip",0xA9B5BBB4);
const fwMvClipId		CTaskEnterVehicle::ms_StreamedEntryClipId("StreamedEntryClip",0x9AE31C6E);
const fwMvFlagId		CTaskEnterVehicle::ms_FromInsideId("FromInside",0xB75486AB);
const fwMvFlagId		CTaskEnterVehicle::ms_IsDeadId("IsDead",0xE2625733);
const fwMvFlagId		CTaskEnterVehicle::ms_IsFatId("IsFat",0xDBEDC678);
const fwMvFlagId		CTaskEnterVehicle::ms_IsPickUpId("IsPickUp",0x72AFCBB5);
const fwMvFlagId		CTaskEnterVehicle::ms_IsFastGetOnId("IsFastGetOn",0xC437D50C);
const fwMvFlagId		CTaskEnterVehicle::ms_FromWaterId("FromWater",0x1EBD32A9);
const fwMvFlagId		CTaskEnterVehicle::ms_GetInFromStandOnId("GetInFromStandOn",0xD8E41092);
const fwMvFlagId		CTaskEnterVehicle::ms_ClimbUpFromWaterId("ClimbUpFromWater",0x899E037F);
const fwMvFlagId		CTaskEnterVehicle::ms_UseSeatClipSetId("UseSeatClipSet",0x37A5CBE5);
const fwMvFloatId		CTaskEnterVehicle::ms_JackPedPhaseId("JackPedPhase",0xECC2BEFD);
const fwMvFloatId		CTaskEnterVehicle::ms_PickPullPhaseId("PickPullPhase",0x1640D479);
const fwMvFloatId		CTaskEnterVehicle::ms_SeatShufflePhaseId("SeatShufflePhase",0xC1216D5C);
const fwMvFloatId		CTaskEnterVehicle::ms_AlignToEnterSeatPhaseId("AlignToEnterSeatPhase",0x51BDC4B7);
const fwMvFloatId		CTaskEnterVehicle::ms_ClimbUpPhaseId("ClimbUpPhase",0x7D642332);
const fwMvFloatId		CTaskEnterVehicle::ms_StreamedEntryPhaseId("StreamedEntryPhase",0x9A034A9D);
const fwMvRequestId		CTaskEnterVehicle::ms_SeatShuffleRequestId("SeatShuffle",0x1AAEFB35);
const fwMvRequestId		CTaskEnterVehicle::ms_SeatShuffleAndJackPedRequestId("ShuffleAndJackPed",0xB113ED2F);
const fwMvRequestId		CTaskEnterVehicle::ms_JackPedRequestId("JackPed",0x78130A7D);
const fwMvRequestId		CTaskEnterVehicle::ms_PickOrPullUpBikeRequestId("PickOrPullUpBike",0xA7215ED9);
const fwMvRequestId		CTaskEnterVehicle::ms_PickUpBikeRequestId("PickUpBike",0xBAED3063);
const fwMvRequestId		CTaskEnterVehicle::ms_PullUpBikeRequestId("PullUpBike",0xB8E1B79B);
const fwMvRequestId		CTaskEnterVehicle::ms_AlignToEnterSeatRequestId("AlignToEnterSeat",0x1925BF78);
const fwMvRequestId		CTaskEnterVehicle::ms_ClimbUpRequestId("ClimbUp",0xADD765B8);
const fwMvRequestId		CTaskEnterVehicle::ms_StreamedEntryRequestId("StreamedEntry",0xB1BBFCDF);
const fwMvRequestId		CTaskEnterVehicle::ms_ArrestRequestId("Arrest",0x2935D83F);
const fwMvRequestId		CTaskEnterVehicle::ms_AlignRequestId("Align",0xE22E0892);
const fwMvStateId		CTaskEnterVehicle::ms_SeatShuffleStateId("Shuffle",0x29315D2B);
const fwMvStateId		CTaskEnterVehicle::ms_JackPedFromOutsideStateId("Jack Ped From OutSide",0x94E8BD3A);
const fwMvStateId		CTaskEnterVehicle::ms_AlignToEnterSeatStateId("AlignToEnterSeat",0x1925BF78);
const fwMvStateId		CTaskEnterVehicle::ms_ClimbUpStateId("ClimbUp",0xADD765B8);
const fwMvStateId		CTaskEnterVehicle::ms_PickUpPullUpStateId("Pick Or Pull Up State",0xC5C61011);
const fwMvStateId		CTaskEnterVehicle::ms_StreamedEntryStateId("StreamedEntry",0xB1BBFCDF);
const fwMvFloatId		CTaskEnterVehicle::ms_ClimbUpRateId("ClimbUpRate",0x631A8F9A);
const fwMvFloatId		CTaskEnterVehicle::ms_GetInRateId("GetInRate",0x1BE1A7B0);
const fwMvFloatId		CTaskEnterVehicle::ms_JackRateId("JackRate",0x98F3A891);
const fwMvFloatId		CTaskEnterVehicle::ms_PickUpRateId("PickUpRate",0xBE73FC68);
const fwMvFloatId		CTaskEnterVehicle::ms_PullUpRateId("PullUpRate",0x79FBE4F7);
const fwMvFloatId		CTaskEnterVehicle::ms_OpenDoorBlendInDurationId("OpenDoorBlendInDuration",0xFC88E0BA);
const fwMvFloatId		CTaskEnterVehicle::ms_OpenDoorToJackBlendInDurationId("OpenDoorToJackBlendInDuration",0xC0D5B66D);
const fwMvNetworkId		CTaskEnterVehicle::ms_AlignNetworkId("AlignNetwork",0xCC918127);
const fwMvClipSetVarId  CTaskEnterVehicle::ms_SeatClipSetId("SeatClipSet",0x19617E01);
const fwMvBooleanId		CTaskEnterVehicle::ms_StartEngineInterruptId("StartEngineInterrupt",0x74ACA9D4);
const fwMvBooleanId		CTaskEnterVehicle::ms_BlendOutEnterSeatAnimId("BlendOutEnterSeatAnim",0x89e7ed6d);

////////////////////////////////////////////////////////////////////////////////

#if __ASSERT
void CTaskEnterVehicle::CheckForFixedPed(const CPed& ped, bool bIsWarping)
{
	if (bIsWarping)
	{
		if (ped.IsBaseFlagSet(fwEntity::IS_FIXED))
		{
			CTask* pPrimaryTask = ped.GetPedIntelligence()->GetTaskAtPriority(PED_TASK_PRIORITY_PRIMARY);
			if (pPrimaryTask)
			{
				CPedScriptedTaskRecordData* pScriptedTaskRecord = CPedScriptedTaskRecord::GetRecordAssociatedWithTask(pPrimaryTask);
				if (pScriptedTaskRecord)
				{
					s32 iIndex = ped.GetPedIntelligence()->m_iCurrentHistroyTop - 1;
					if (ped.GetPedIntelligence()->m_aScriptHistoryName[iIndex] != NULL)
					{
						scriptWarningf("Script : (%s,%d) has called TASK_ENTER_VEHICLE on ped who is frozen: %s - 0x%p", ped.GetPedIntelligence()->m_szScriptName[iIndex],  ped.GetPedIntelligence()->m_aProgramCounter[iIndex], ped.GetDebugName(), &ped);
						scrThread::PrePrintStackTrace();
					}
				}
			}
		}
	}
}
#endif // __ASSERT

////////////////////////////////////////////////////////////////////////////////

s32 CTaskEnterVehicle::GetEventPriorityForJackedPed(const CPed& rJackedPed)
{
	// Up the priority if the ped being jacked is reacting to their vehicle being on fire, B*1613925
	s32 iEventPriority = E_PRIORITY_GIVE_PED_TASK;
	if (!rJackedPed.IsNetworkClone())
	{
		if (!rJackedPed.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE))
		{
			const CEvent* pCurrentEvent = rJackedPed.GetPedIntelligence()->GetCurrentEvent();
			if (pCurrentEvent && pCurrentEvent->GetEventPriority() == EVENT_VEHICLE_ON_FIRE)
			{
				iEventPriority = E_PRIORITY_VEHICLE_ON_FIRE+1;
			}
		}
	}
	return iEventPriority;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::ProcessHeliPreventGoingThroughGroundLogic(const CPed& rPed, const CVehicle& rVeh, Vector3& vNewPedPosition, float fLegIkBlendOutPhase, float fPhase)
{
	taskAssert(rVeh.InheritsFromHeli());
	
	if (fLegIkBlendOutPhase < 0.0f || (fPhase < 0.0f && fPhase < fLegIkBlendOutPhase))
	{
		AI_LOG_WITH_ARGS("Frame : %i, Ped (%s, %p) testing for ground penetration\n", fwTimer::GetFrameCount(), rPed.GetModelName(), &rPed);
		Vector3 vTestPosition = vNewPedPosition;
		TUNE_GROUP_FLOAT(VEHICLE_ENTRY_TUNE, ENTRY_Z_OFFSET, 1.15f, 0.0f, 2.0f, 0.01f);
		vTestPosition.z += ENTRY_Z_OFFSET;
		Vector3 vGroundPos(Vector3::ZeroType);

		TUNE_GROUP_FLOAT(VEHICLE_ENTRY_TUNE, ENTRY_Z_PROBE_DEPTH, 3.5f, 0.0f, 5.0f, 0.01f);
		if (CTaskVehicleFSM::FindGroundPos(&rVeh, &rPed, vTestPosition, ENTRY_Z_PROBE_DEPTH, vGroundPos))
		{
			AI_LOG_WITH_ARGS("Frame : %i, Ped (%s, %p) found ground, depth %.2f, current %.2f\n", fwTimer::GetFrameCount(), rPed.GetModelName(), &rPed, vGroundPos.z, vNewPedPosition.z);
			TUNE_GROUP_FLOAT(VEHICLE_ENTRY_TUNE, MAX_GROUND_PENETRATION_DEPTH, 0.05f, 0.0f, 2.0f, 0.01f);
			if (vNewPedPosition.z < (vGroundPos.z - MAX_GROUND_PENETRATION_DEPTH))
			{
				AI_LOG_WITH_ARGS("Frame : %i, Ped (%s, %p) setting ground height\n", fwTimer::GetFrameCount(), rPed.GetModelName(), &rPed);
				vNewPedPosition.z = vGroundPos.z;
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::CanInterruptEnterSeat(CPed& rPed, const CVehicle& rVeh, const CMoveNetworkHelper& rMoveNetworkHelper, s32 iTargetEntryPointIndex, bool& bWantsToExit, const CTaskEnterVehicle& rEnterTask)
{
	if (!rVeh.IsEntryIndexValid(iTargetEntryPointIndex))
	{
		return false;
	}

	const CEntryExitPoint* pEntryPoint = rVeh.GetEntryExitPoint(iTargetEntryPointIndex);
	if (pEntryPoint)
	{
		// Check whether the player has pressed to exit, once we can interrupt we force the exit
		if (CTaskVehicleFSM::CheckForEnterExitVehicle(rPed))
		{
			bWantsToExit = true;
		}

		bool bFinishEarly = false;

		// If we're going to put on a helmet, do it asap
		if (CTaskMotionInAutomobile::WillPutOnHelmet(rVeh, rPed) && rMoveNetworkHelper.GetBoolean(CTaskMotionInVehicle::ms_PutOnHelmetInterrupt))
		{
			bFinishEarly = true;
		}
		// If we're going to close the door, do it asap
		else if (CTaskEnterVehicle::CheckForCloseDoorInterrupt(rPed, rVeh, rMoveNetworkHelper, pEntryPoint, bWantsToExit, iTargetEntryPointIndex))
		{
			bFinishEarly = true;
		}
		// If going to the opposite seat, interrupt for the shuffle
		else if (CTaskEnterVehicle::CheckForShuffleInterrupt(rPed, rMoveNetworkHelper, pEntryPoint, rEnterTask))
		{
			bFinishEarly = true;
		}
		// If need to start engine, check for interrupt
		else if (rMoveNetworkHelper.GetBoolean(CTaskEnterVehicle::ms_StartEngineInterruptId))
		{
			if (CTaskEnterVehicle::CheckForStartEngineInterrupt(rPed, rVeh, true))
			{
				rPed.SetPedResetFlag(CPED_RESET_FLAG_InterruptedToQuickStartEngine, true);
				bFinishEarly = true;
			}

			if (!bFinishEarly)
			{
				const CCarDoor* pDoor = CTaskMotionInVehicle::GetDirectAccessEntryDoorFromSeat(rVeh, pEntryPoint->GetSeat(SA_directAccessSeat));
				if (!pDoor || !pDoor->GetIsIntact(&rVeh))
				{
					bFinishEarly = true;
				}
			}
		}
		else if (rMoveNetworkHelper.GetBoolean(CTaskEnterVehicle::ms_BlendOutEnterSeatAnimId))
		{
			bFinishEarly = true;
		}
		return bFinishEarly;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::StartMoveNetwork(CPed& ped, const CVehicle& vehicle, CMoveNetworkHelper& moveNetworkHelper, s32 iEntryPointIndex, float fBlendInDuration, Vector3 * pvLocalSpaceEntryModifier, bool bDontRestartIfActive, fwMvClipSetId clipsetOverride)
{
	if (moveNetworkHelper.IsNetworkActive())
    {
		// Don't restart the move network if its already attached
		if (bDontRestartIfActive)
		{
			return true;
		}
		ped.GetMovePed().ClearTaskNetwork(moveNetworkHelper, NORMAL_BLEND_DURATION);
    }

#if __ASSERT
	const CVehicleEntryPointAnimInfo* pEntryClipInfo = vehicle.GetEntryAnimInfo(iEntryPointIndex);
	taskFatalAssertf(pEntryClipInfo, "NULL Entry Info for entry index %i", iEntryPointIndex);
#endif // __ASSERT

	if (vehicle.InheritsFromBike())
	{
		TUNE_GROUP_FLOAT(BIKE_TUNE, BIKE_NETWORK_BLEND_DURATION, SLOW_BLEND_DURATION, 0.0f, REALLY_SLOW_BLEND_DURATION, 0.01f);
		fBlendInDuration = BIKE_NETWORK_BLEND_DURATION;
	}
	moveNetworkHelper.CreateNetworkPlayer(&ped, CClipNetworkMoveInfo::ms_NetworkTaskEnterVehicle);

	fwMvClipSetId entryClipsetId = clipsetOverride == CLIP_SET_ID_INVALID ? CTaskEnterVehicleAlign::GetEntryClipsetId(&vehicle, iEntryPointIndex, &ped) : clipsetOverride;
	if (CTaskVehicleFSM::IsVehicleClipSetLoaded(entryClipsetId))
	{
		taskAssertf(fwClipSetManager::GetClipSet(entryClipsetId), "Couldn't find clipset %s", entryClipsetId.GetCStr());
		moveNetworkHelper.SetClipSet(entryClipsetId);
	}

	fwMvClipSetId commonClipsetId = CTaskEnterVehicleAlign::GetCommonClipsetId(&vehicle, iEntryPointIndex);
	if (CTaskVehicleFSM::IsVehicleClipSetLoaded(commonClipsetId))
	{
		taskAssertf(fwClipSetManager::GetClipSet(commonClipsetId), "Couldn't find clipset %s", commonClipsetId.GetCStr());
		moveNetworkHelper.SetClipSet(commonClipsetId, CTaskVehicleFSM::ms_CommonVarClipSetId);
	}

	u32 flags = CTaskEnterVehicleAlign::ShouldDoStandAlign(vehicle, ped, iEntryPointIndex, pvLocalSpaceEntryModifier) ? CMovePed::Task_None : CMovePed::Task_TagSyncTransition;

	if (ms_Tunables.m_DisableTagSyncIntoAlign)
		flags = CMovePed::Task_None;

	if (ms_Tunables.m_IgnoreRotationBlend)
		flags |= CMovePed::Task_IgnoreMoverBlendRotation;

	ASSERT_ONLY(taskDebugf2("Frame : %u - %s%s started Enter Vehicle Move Network", fwTimer::GetFrameCount(), ped.IsNetworkClone() ? "Clone ped : " : "Local ped : ", ped.GetDebugName()));
	return ped.GetMovePed().SetTaskNetwork(moveNetworkHelper, fBlendInDuration, flags);
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::StartMoveNetworkForArrest()
{
	if(GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_WillArrestRatherThanJack) && CheckForStreamedEntry())
	{
		if(!m_pStreamedEntryClipSet->GetIsArrest())
		{
			return FSM_Quit;
		}
		else
		{
			const bool bCombatEntry = ShouldUseCombatEntryAnimLogic();
			const fwMvClipSetId entryClipsetId = CTaskEnterVehicleAlign::GetEntryClipsetId(m_pVehicle, m_iTargetEntryPoint, GetPed());
			const float fBlendInDuration = bCombatEntry ? ms_Tunables.m_CombatEntryBlendDuration : INSTANT_BLEND_DURATION;
			if (CTaskVehicleFSM::IsVehicleClipSetLoaded(entryClipsetId)
				&& taskVerifyf(StartMoveNetwork(*GetPed(), *m_pVehicle, m_moveNetworkHelper, m_iTargetEntryPoint, fBlendInDuration, &m_vLocalSpaceEntryModifier, true, m_OverridenEntryClipSetId), "Failed to start TASK_ENTER_VEHICLE move network"))
			{
				SetTaskState(State_WaitForStreamedEntry);
			}
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::CheckForStartEngineInterrupt(CPed& ped, const CVehicle& vehicle, bool bIgnoreDriverCheck)
{
	if (ped.IsLocalPlayer())
	{
		if (vehicle.m_nVehicleFlags.bCarNeedsToBeHotwired)
		{
			return true;
		}

		// Make sure the ped is the driver for these checks
		const bool bIsDriver = vehicle.GetDriver() == &ped ? true : false;

		bool bNotAcceleratingReversingOrTurning = true;

		if (bIsDriver || bIgnoreDriverCheck)
		{
			const CControl* pControl = ped.GetControlFromPlayer();
			bNotAcceleratingReversingOrTurning = !pControl || (pControl->GetVehicleAccelerate().IsUp(ioValue::ANALOG_BUTTON_DOWN_THRESHOLD) && 
				pControl->GetVehicleSteeringLeftRight().GetNorm() ==0.0f && 
				pControl->GetVehicleBrake().IsUp(ioValue::ANALOG_BUTTON_DOWN_THRESHOLD));
		}

		return !bNotAcceleratingReversingOrTurning;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::CheckForCloseDoorInterrupt(CPed& ped, const CVehicle& vehicle, const CMoveNetworkHelper& moveNetworkHelper, const CEntryExitPoint* pEntryPoint, bool bWantsToExit, s32 iEntryPointIndex)
{
	if (moveNetworkHelper.GetBoolean(CTaskEnterVehicleSeat::ms_CloseDoorInterruptId) && (bWantsToExit || CTaskMotionInVehicle::CheckForClosingDoor(vehicle, ped, true, pEntryPoint->GetSeat(SA_directAccessSeat), iEntryPointIndex, true)))
	{
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::CheckForShuffleInterrupt(CPed& ped, const CMoveNetworkHelper& moveNetworkHelper, const CEntryExitPoint* pEntryPoint, const CTaskEnterVehicle& enterVehicletask)
{
	if (moveNetworkHelper.GetBoolean(CTaskEnterVehicleSeat::ms_GetInShuffleInterruptId))
	{
		bool bInterruptForShuffle = false;

		if (CTaskVehicleFSM::CheckForEnterExitVehicle(ped))
		{
			bInterruptForShuffle = true;
		}

		// Interrupt early
		if (enterVehicletask.IsFlagSet(CVehicleEnterExitFlags::UseFastClipRate) || enterVehicletask.IsFlagSet(CVehicleEnterExitFlags::CombatEntry))
		{
			bInterruptForShuffle = true;
		}

		if (!bInterruptForShuffle)
		{
			if (enterVehicletask.GetTargetSeat() != pEntryPoint->GetSeat(SA_directAccessSeat))
			{
				if (enterVehicletask.GetSeatRequestType() == SR_Specific || enterVehicletask.GetVehicle()->IsTurretSeat(enterVehicletask.GetTargetSeat()))
				{
					bInterruptForShuffle = true;
				}
			}
		}

		return bInterruptForShuffle;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::PassesPlayerDriverAlignCondition(const CPed& rPed, const CVehicle& rVeh)
{
	if (!rPed.IsLocalPlayer())
	{
		const CPed* pDriver = rVeh.GetSeatManager()->GetDriver();
		if (pDriver && pDriver->IsLocalPlayer())
		{
			// If the players vehicle has been stationary for a while it maybe stuck, so allow the condition to pass
			const CTaskEnterVehicle* pEnterVehicleTask = static_cast<const CTaskEnterVehicle*>(rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
			if (pEnterVehicleTask)
			{
				const u32 uLastMovingTime = pEnterVehicleTask->GetTimeVehicleLastWasMoving();
				if (uLastMovingTime != 0)
				{
					if ((fwTimer::GetTimeInMilliseconds() - uLastMovingTime) > ms_Tunables.m_MinTimeStationaryToIgnorePlayerDriveTest)
					{
						return true;
					}
				}
			}

			const CControl* pControl = pDriver->GetControlFromPlayer();
			if (pControl)
			{
				const float fAccelerate = pControl->GetVehicleAccelerate().GetNorm();
				const float fDeccelerate = Max(pControl->GetVehicleBrake().GetNorm(), pControl->GetVehicleHandBrake().GetNorm());
				TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, ACCELERATE_DELTA_THRESHOLD_TO_ALLOW_ALIGN, 0.1f, 0.0f, 1.0f, 0.01f);
				if (Abs(fAccelerate - fDeccelerate) > ACCELERATE_DELTA_THRESHOLD_TO_ALLOW_ALIGN)
				{
					return false;
				}
			}
		}
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::ProcessSmashWindow(CVehicle& rVeh, CPed& rPed, const s32 iEntryPointIndex, const crClip& rClip, const float fPhase, const CMoveNetworkHelper& rMoveNetworkHelper, bool& bSmashedWindow, bool bProcessIfNoTagsFound)
{
	if (!bSmashedWindow)
	{
		float fSmashWindowPhase = 1.0f;
		bool bFoundTag = false;

		const fwMvClipSetId clipsetId = rMoveNetworkHelper.GetClipSetId();
		if (clipsetId != CLIP_SET_ID_INVALID)
		{
			fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipsetId);
			if (pClipSet)
			{
				// Don't bother asserting, its now become valid to not smash the windows, B*1053194
				bFoundTag = CClipEventTags::FindEventPhase(&rClip, CClipEventTags::Smash, fSmashWindowPhase);
			}
		}

		if ((bFoundTag || bProcessIfNoTagsFound) && fPhase >= fSmashWindowPhase)
		{
			// Swear as you smash the window if pissed off
			if (rPed.IsAPlayerPed() && rPed.GetPlayerInfo() && rPed.GetPlayerInfo()->PlayerIsPissedOff())
			{
				rPed.RandomlySay("GENERIC_CURSE", 0.5f);
			}

			CEventShockingSeenCarStolen ev(rPed, &rVeh);
			CShockingEventsManager::Add(ev);

			eHierarchyId iWindow = GetWindowHierarchyToSmash(rVeh, iEntryPointIndex);
			rVeh.SmashWindow(iWindow, VEHICLEGLASSFORCE_KICK_ELBOW_WINDOW);

			if (rVeh.GetCarDoorLocks() == CARLOCK_LOCKED_BUT_CAN_BE_DAMAGED)
			{
				rVeh.SetCarDoorLocks(CARLOCK_UNLOCKED);
			}

			// If there's any mods attached to the window (i.e. rally netting), hide them
			s8 dummyValue = -1;
			const int windowBoneId = rVeh.GetBoneIndex(iWindow);
			rVeh.GetVehicleDrawHandler().GetVariationInstance().HideModOnBone(&rVeh, windowBoneId, dummyValue);

			CTaskEnterVehicle::PossiblyReportStolentVehicle(rPed, rVeh, false);
			bSmashedWindow = true;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

eHierarchyId CTaskEnterVehicle::GetWindowHierarchyToSmash(const CVehicle& rVeh, const s32 iEntryPointIndex)
{
	return rVeh.GetVehicleModelInfo()->GetEntryPointInfo(iEntryPointIndex)->GetWindowId();
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::ProcessBikeArmIk(const CVehicle& rVeh, CPed& rPed, s32 iEntryPointIndex, const crClip& rClip, float fPhase, bool bIsJacking, const CTaskEnterVehicle::eForceArmIk forceArmIk, bool bUseOrientation)
{
	const CEntryExitPoint* pEntryPoint = rVeh.GetEntryExitPoint(iEntryPointIndex);
	if (pEntryPoint && pEntryPoint->CanSeatBeReached(0) == SA_directAccessSeat)
	{
		eVehicleClipFlags eLeftArmIkFlag = CTaskVehicleFSM::ProcessBikeHandleArmIk(rVeh, rPed, &rClip, fPhase, true, false, bUseOrientation);
		eVehicleClipFlags eRightArmIkFlag = CTaskVehicleFSM::ProcessBikeHandleArmIk(rVeh, rPed, &rClip, fPhase, true, true, bUseOrientation);

		aiDebugf2("Frame : %i, Left Arm Ik On ? %s", fwTimer::GetFrameCount(), eLeftArmIkFlag == CTaskVehicleFSM::VF_IK_Running ? "TRUE" : "FALSE");
		aiDebugf2("Frame : %i, Right Arm Ik On ? %s", fwTimer::GetFrameCount(), eRightArmIkFlag == CTaskVehicleFSM::VF_IK_Running ? "TRUE" : "FALSE");

		bool bTorsoIK = (eLeftArmIkFlag != CTaskVehicleFSM::VF_No_IK_Clip) || (eRightArmIkFlag != CTaskVehicleFSM::VF_No_IK_Clip);

		if (!bIsJacking || (forceArmIk != ForceArmIk_None))
		{
			const bool bNonZeroSteerAngle = rVeh.GetSteerAngle() != 0.0f;
			const bool bForceLeftArm = (forceArmIk == ForceArmIk_Left) || (forceArmIk == ForceArmIk_Both);
			const bool bForceRightArm = (forceArmIk == ForceArmIk_Right) || (forceArmIk == ForceArmIk_Both);

			if((bNonZeroSteerAngle || bForceLeftArm) && (eLeftArmIkFlag == CTaskVehicleFSM::VF_No_IK_Clip))
			{
				CTaskVehicleFSM::ForceBikeHandleArmIk(rVeh, rPed, true, false, 0.0f, bUseOrientation);
				bTorsoIK = true;
			}

			if((bNonZeroSteerAngle || bForceRightArm) && (eRightArmIkFlag == CTaskVehicleFSM::VF_No_IK_Clip))
			{
				CTaskVehicleFSM::ForceBikeHandleArmIk(rVeh, rPed, true, true, 0.0f, bUseOrientation);
				bTorsoIK = true;
			}
		}

		if (bTorsoIK)
		{
			const CVehicleSeatAnimInfo* pSeatAnimInfo = rVeh.GetSeatAnimationInfo(0);
			float fAnimatedLean = 0.5f;
			u32 uFlags = 0;

			if (pSeatAnimInfo)
			{
				if (pSeatAnimInfo->GetUseTorsoLeanIK())
				{
					uFlags |= TORSOVEHICLEIK_APPLY_LEAN;
				}
				else if (pSeatAnimInfo->GetUseRestrictedTorsoLeanIK())
				{
					// Restricted flag not currently used when entering vehicle
					// uFlags |= TORSOVEHICLEIK_APPLY_LEAN_RESTRICTED;
				}
			}

			if (uFlags & (TORSOVEHICLEIK_APPLY_LEAN | TORSOVEHICLEIK_APPLY_LEAN_RESTRICTED))
			{
				CIkRequestTorsoVehicle ikRequest;
				ikRequest.SetAnimatedLean(fAnimatedLean);
				ikRequest.SetFlags(uFlags);
				rPed.GetIkManager().Request(ikRequest);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

CTaskEnterVehicle::CTaskEnterVehicle(CVehicle* pVehicle, SeatRequestType iRequest, s32 iTargetSeatIndex, VehicleEnterExitFlags iRunningFlags, float fTimerSeconds, const float fMoveBlendRatio, const CPed* pTargetPed, fwMvClipSetId overrideClipSetId)
: CTaskVehicleFSM(pVehicle, iRequest, iTargetSeatIndex, iRunningFlags, -1)
, m_vPreviousPedPosition(Vector3::ZeroType)
, m_vLocalSpaceEntryModifier(VEC3_ZERO)
, m_iCurrentSeatIndex(-1)
, m_fTimerSeconds(fTimerSeconds)
, m_fDoorCheckInterval(-1.0f)
, m_pJackedPed(NULL)
, m_fClipBlend(-1.0f)
, m_fBlendInDuration(ms_Tunables.m_NetworkBlendDuration)
, m_TaskTimer(0.0f)
, m_iNetworkWaitState(-1) 
, m_iLastCompletedState(-1)
, m_iWaitForPedTimer(0)
, m_ClipSetId(CLIP_SET_ID_INVALID)
, m_uDamageTotalTime(0)
, m_RagdollType(RAGDOLL_TRIGGER_ELECTRIC)
, m_fOriginalLeanAngle(0.0f)
, m_fLastPhase(0.0f)
, m_uLostClimpUpPhysicalTime(0)
, m_uInvalidEntryPointTime(0)
, m_uVehicleLastMovingTime(fwTimer::GetTimeInMilliseconds())
, m_fInterruptPhase(-1.0f)
, m_StreamedClipSetId(CLIP_SET_ID_INVALID)
, m_StreamedClipId(CLIP_ID_INVALID)
, m_pStreamedEntryClipSet(NULL)
, m_bAnimFinished(false)
, m_bShouldTriggerElectricTask(false)
, m_bWantsToExit(false)
, m_bWillBlendOutHandleIk(false)
, m_bWillBlendOutSeatIk(false)
, m_bWantsToGoIntoCover(false)
, m_bTurnedOnDoorHandleIk(false)
, m_bTurnedOffDoorHandleIk(false)
, m_bLerpingCloneToDoor(false)
, m_ClonePlayerWaitingForLocalPedAttachExitCar(false)
, m_bShouldBePutIntoSeat(false)
, m_bWasJackedPedDeadInitially(false)
, m_bAligningToOpenDoor(false)
, m_bIsPickUpAlign(false)
, m_fInitialHeading(0.0f)
, m_pTargetPed(pTargetPed)
, m_OverridenEntryClipSetId(overrideClipSetId)
, m_fLegIkBlendOutPhase(0.0f)
, m_iTimeStartedAlign(0)
, m_bSmashedWindow(false)
, m_bEnterVehicleLookAtStarted(false)
, m_iCurrentGetInIndex(0)
, m_fExtraGetInFixupStartPhase(0.0f)
, m_fExtraGetInFixupEndPhase(1.0f)
, m_bDoingExtraGetInFixup(false)
, m_bFoundExtraFixupPoints(false)
, m_bCouldEnterVehicleWhenOpeningDoor(false)
, m_bVehicleDoorOpen(false)
, m_bForcedDoorState(false)
, m_bCreatedVehicleWpnMgr(false)
, m_targetPosition(Vector3::ZeroType)
, m_bWantsToShuffle(false)
, m_bFriendlyJackDelayEndedLastFrame(false)
{
	SetPedMoveBlendRatio(fMoveBlendRatio);
	SetInternalTaskType(CTaskTypes::TASK_ENTER_VEHICLE);
}

////////////////////////////////////////////////////////////////////////////////

CTaskEnterVehicle::~CTaskEnterVehicle()
{

}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::ControlPassingAllowed(CPed *pPed, const netPlayer& UNUSED_PARAM(player), eMigrationType UNUSED_PARAM(migrationType))
{
    if(pPed && pPed->GetIsInVehicle())
    {
        return false;
    }
    else
    {
        return GetState() < State_Align || GetState() == State_PickUpBike || GetState() == State_PullUpBike;
    }
}

////////////////////////////////////////////////////////////////////////////////

CTaskInfo* CTaskEnterVehicle::CreateQueriableState() const
{
	return rage_new CClonedEnterVehicleInfo(GetState(), m_pVehicle, m_iSeatRequestType, m_iSeat, m_iTargetEntryPoint, m_iRunningFlags, m_pJackedPed, m_bShouldTriggerElectricTask, m_ClipSetId, m_uDamageTotalTime, m_RagdollType, m_StreamedClipSetId, m_StreamedClipId, m_bVehicleDoorOpen, m_targetPosition);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::ReadQueriableState( CClonedFSMTaskInfo* pTaskInfo )
{
	taskAssert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_ENTER_VEHICLE_CLONED );

	CClonedEnterVehicleInfo* pClonedEnterVehicleInfo = static_cast<CClonedEnterVehicleInfo*>(pTaskInfo);
	s32 oldTargetEntryPoint = m_iTargetEntryPoint;
	m_StreamedClipSetId = pClonedEnterVehicleInfo->GetStreamedClipSetId();
	m_StreamedClipId = pClonedEnterVehicleInfo->GetStreamedClipId();
	m_bVehicleDoorOpen = pClonedEnterVehicleInfo->GetVehicleDoorOpen();
	m_targetPosition = pClonedEnterVehicleInfo->GetTargetPosition();

	CTaskVehicleFSM::ReadQueriableState(pTaskInfo);

	// once we have started the task proper, we don't want the remote state of the
	// entry point updated from network syncs
	if(GetState() >= State_OpenDoor)
	{
		SetTargetEntryPoint(oldTargetEntryPoint, EPR_CLONE_READ_QUAERIABLE_STATE, VEHICLE_DEBUG_DEFAULT_FLAGS);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Finish);
}

bool CTaskEnterVehicle::IsValidForMotionTask(CTaskMotionBase& task) const
{
	if (m_pVehicle && m_pVehicle->HasWaterEntry())
		return (task.IsOnFoot() || task.IsInWater());

	bool bIsValid = CTask::IsValidForMotionTask(task);

	if (!bIsValid && GetSubTask())
		bIsValid = GetSubTask()->IsValidForMotionTask(task);

	return bIsValid;
}

bool CTaskEnterVehicle::CanBeReplacedByCloneTask(u32 taskType) const
{
	// don't allow the ped to die during the enter seat state, this can result in the ped falling through the vehicle (eg entering a bus)
	// the ped needs to complete this state and be placed in the seat before dying
	if (GetState() == State_EnterSeat && taskType == CTaskTypes::TASK_DYING_DEAD)
	{
		return false;
	}

	return CTaskVehicleFSM::CanBeReplacedByCloneTask(taskType);
}


////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::IsInScope(const CPed *pPed)
{
    // don't allow the task to comes back into scope if it left scope
    // as there is no guarantee the task will be in a state fit to continue
    bool isInScope = m_cloneTaskInScope;

    if(isInScope || GetState() <= State_Align)
    {
        isInScope = CTaskVehicleFSM::IsInScope(pPed);
    }

    if(!isInScope)
    {
        // even if the clone task is out of scope we need to make sure any locally
        // controlled peds exit the vehicle if they are being jacked
        EnsureLocallyJackedPedsExitVehicle();
    }

    return isInScope;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::CleanUp()
{
	CPed* pPed = GetPed();

    BANK_ONLY(taskDebugf2("Frame : %u - %s%s Cleaning up enter vehicle task", fwTimer::GetFrameCount(), pPed->IsNetworkClone() ? "Clone ped : " : "Local ped : ", pPed->GetDebugName()));

	ResetRagdollOnCollision(*pPed);

	if (!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
	{
		CObject* pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponObject() : NULL;
		if (pWeapon && !pWeapon->GetIsVisible())
		{
			pWeapon->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, true, true);
		}

		if (pPed->GetAttachState())
		{
			// If we're attached and not in the vehicle it should be safe to exclude the vehicle when detaching as we 
			// shouldn't be intersecting much

			u16 nDetachFlags = DETACH_FLAG_ACTIVATE_PHYSICS | DETACH_FLAG_EXCLUDE_VEHICLE;

			// url:bugstar:6202042 - hack to avoid clipping inside the vehicle and getting stuck when the task is interrupted too close to the seat
			if (m_pVehicle && GetState() > State_Align && m_iSeat != -1)
			{
				TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, MIN_DIST_TO_SEAT_TO_AVOID_CURRENT_DETACH, 0.5f, 0.0f, 30.0f, 0.1f);

#if __DEV || __BANK
				bool bUsingDetachSkipCurrentPos = false;
#endif

				if ((MI_HELI_CARGOBOB.IsValid() && m_pVehicle->GetModelIndex() == MI_HELI_CARGOBOB.GetModelIndex()) ||
					(MI_HELI_CARGOBOB2.IsValid() && m_pVehicle->GetModelIndex() == MI_HELI_CARGOBOB2.GetModelIndex()) ||
					(MI_HELI_CARGOBOB3.IsValid() && m_pVehicle->GetModelIndex() == MI_HELI_CARGOBOB3.GetModelIndex()))
				{
					const bool bIsFrontSeat = m_pVehicle->GetSeatInfo(m_iSeat) && m_pVehicle->GetSeatInfo(m_iSeat)->GetIsFrontSeat();
					if (!bIsFrontSeat)
					{
						Vector3 vSeatPosition;
						Quaternion qSeatOrientation;
						CModelSeatInfo::CalculateSeatSituation(m_pVehicle, vSeatPosition, qSeatOrientation, m_iTargetEntryPoint, m_iSeat);

						float fDistToSeatSq = vSeatPosition.Dist2(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));
						if (fDistToSeatSq < rage::square(MIN_DIST_TO_SEAT_TO_AVOID_CURRENT_DETACH))
						{
							nDetachFlags |= DETACH_FLAG_SKIP_CURRENT_POSITION_CHECK;
							AI_LOG_WITH_ARGS("Ped %s using DETACH_FLAG_SKIP_CURRENT_POSITION_CHECK detach flag when quitting TASK_ENTER_VEHICLE because we're too close [%.2f] to the target seat bone [%i] and want to avoid ending up clipping the vehicle %s[%s]\n"
								, AILogging::GetDynamicEntityNameSafe(pPed)
								, sqrtf(fDistToSeatSq)
								, m_iSeat
								, AILogging::GetDynamicEntityNameSafe(m_pVehicle),
								m_pVehicle->GetModelName());

#if __DEV || __BANK
							bUsingDetachSkipCurrentPos = true;
#endif

						}
					}
				}

#if __DEV || __BANK
				if (!bUsingDetachSkipCurrentPos)
				{
					Vector3 vSeatPosition;
					Quaternion qSeatOrientation;
					CModelSeatInfo::CalculateSeatSituation(m_pVehicle, vSeatPosition, qSeatOrientation, m_iTargetEntryPoint, m_iSeat);

					float fDistToSeatSq = vSeatPosition.Dist2(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));
					if (fDistToSeatSq < rage::square(MIN_DIST_TO_SEAT_TO_AVOID_CURRENT_DETACH))
					{
						nDetachFlags |= DETACH_FLAG_SKIP_CURRENT_POSITION_CHECK;

#if __BANK
						AI_LOG_WITH_ARGS("Ped %s is very close [%.2f] to the target seat bone [%i] when quitting TASK_ENTER_VEHICLE for vehicle %s[%s], but is not using DETACH_FLAG_SKIP_CURRENT_POSITION_CHECK detach flag! Consider adding it to the exception list in code!\n"
							, AILogging::GetDynamicEntityNameSafe(pPed)
							, sqrtf(fDistToSeatSq)
							, m_iSeat
							, AILogging::GetDynamicEntityNameSafe(m_pVehicle),
							m_pVehicle->GetModelName());
#else
						aiAssertf(0, "Ped %s is very close [%.2f] to the target seat bone [%i] when quitting TASK_ENTER_VEHICLE for vehicle %s[%s], but is not using DETACH_FLAG_SKIP_CURRENT_POSITION_CHECK detach flag! Consider adding it to the exception list in code!"
							, AILogging::GetDynamicEntityNameSafe(pPed)
							, sqrtf(fDistToSeatSq)
							, m_iSeat
							, AILogging::GetDynamicEntityNameSafe(m_pVehicle),
							m_pVehicle->GetModelName());
#endif
					}
				}
#endif
			}

			pPed->DetachFromParent(nDetachFlags);
		}

		if (GetPreviousState() == State_UnReserveDoorToFinish)
		{
			pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Idle, true);
		}

		if(m_pVehicle && (m_pVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE || m_pVehicle->GetVehicleType() == VEHICLE_TYPE_BICYCLE))
		{
			static_cast<CBike*>(m_pVehicle.Get())->m_nBikeFlags.bGettingPickedUp = false;
		}
	}
	else
	{
		// Force the in car state here
		pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_InVehicle);
	}

	if (m_bCreatedVehicleWpnMgr && m_pVehicle)
	{
		bool bIsTurretedVehicle = false;
		bool bShouldDestroyWeaponMgr = CTaskVehicleFSM::ShouldDestroyVehicleWeaponManager(*m_pVehicle, bIsTurretedVehicle);
		if (bIsTurretedVehicle && bShouldDestroyWeaponMgr)
		{
			m_pVehicle->DestroyVehicleWeaponWhenInactive();
		}
	}

	static bank_float s_BlendDuration = SLOW_BLEND_DURATION;

	bool bShouldInstantlyBlendOutEnterVehicleAnims = false;

	if (pPed->IsLocalPlayer() && m_pVehicle && (m_pVehicle->InheritsFromBike() || m_pVehicle->InheritsFromQuadBike() || m_pVehicle->InheritsFromAmphibiousQuadBike()))
	{
		// If going between first to third person, we need to instantly blend out any mismatched entry anim
		const bool bInFirstPerson = pPed->IsFirstPersonShooterModeEnabledForPlayer(false);
		s32 iOnBikeViewMode = camControlHelper::GetViewModeForContext(camControlHelperMetadataViewMode::ON_BIKE);
		const bool bWillBeInFirstPersonModeOnVehicle = iOnBikeViewMode == camControlHelperMetadataViewMode::FIRST_PERSON;
		if (m_pVehicle->InheritsFromBicycle())
		{
			if ((!bInFirstPerson && bWillBeInFirstPersonModeOnVehicle) ||
				(bInFirstPerson && !bWillBeInFirstPersonModeOnVehicle))
			{
				bShouldInstantlyBlendOutEnterVehicleAnims = true;
			}
		}

		// Prevent the cinematic mounted cam from blending in side offsets
		if (!bInFirstPerson && bWillBeInFirstPersonModeOnVehicle)
		{
			pPed->GetPlayerInfo()->SetSwitchedToOrFromFirstPerson(true);
		}
	}
	ASSERT_ONLY(taskDebugf2("Frame : %u - %s%s stopped Enter Vehicle Move Network : CTaskEnterVehicle::CleanUp()", fwTimer::GetFrameCount(), pPed->IsNetworkClone() ? "Clone ped : " : "Local ped : ", pPed->GetDebugName()));
	pPed->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, bShouldInstantlyBlendOutEnterVehicleAnims ? INSTANT_BLEND_DURATION : s_BlendDuration);


	pPed->GetIkManager().AbortLookAt(500, m_uEnterVehicleLookAtHash);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::ShouldAbort(const AbortPriority priority, const aiEvent* pEvent)
{
	if (priority == ABORT_PRIORITY_IMMEDIATE)
	{
		return true;
	}

	CPed* pPed = GetPed();
	if (pEvent)
	{
		bool bRagdollActivationBlocked = CTaskVehicleFSM::ProcessBlockRagdollActivation(*pPed, m_moveNetworkHelper);

		if (pEvent->GetEventType() == EVENT_SWITCH_2_NM_TASK)
		{
			if (!bRagdollActivationBlocked && pPed->GetActivateRagdollOnCollision() && CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_ACTIVATE_ON_COLLISION))
			{
				return true;
			}
			else
			{
				const_cast<aiEvent*>(pEvent)->SetFlagForDeletion(true);
			}
		}
		else if (pEvent->GetEventType() == EVENT_GIVE_PED_TASK && static_cast<const CEventGivePedTask*>(pEvent)->GetTask() && static_cast<const CEventGivePedTask*>(pEvent)->GetTask()->GetTaskType() == CTaskTypes::TASK_MELEE)
		{
			if (GetState() < State_OpenDoor)
			{
				return true;
			}
			else if (GetState() == State_OpenDoor && GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_OPEN_VEHICLE_DOOR_FROM_OUTSIDE)
			{
				return GetSubTask()->GetState() == CTaskOpenVehicleDoorFromOutside::State_OpenDoor || GetSubTask()->GetState() == CTaskOpenVehicleDoorFromOutside::State_OpenDoorBlend || GetSubTask()->GetState() == CTaskOpenVehicleDoorFromOutside::State_OpenDoorCombat || GetSubTask()->GetState() == CTaskOpenVehicleDoorFromOutside::State_OpenDoorWater;
			}
			else
			{
				return false;
			}
		}
		else if (pEvent->GetEventType() == EVENT_DAMAGE && 
			GetState() == State_EnterSeat &&
			bRagdollActivationBlocked)
		{
			if (!pPed->GetIsInVehicle())
			{
				if (pPed->ShouldBeDead() && m_pVehicle && m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint) && m_pVehicle->IsSeatIndexValid(m_iSeat))
				{
					const s32 iSeatEntering = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat, m_iSeat);
					if (m_pVehicle->IsSeatIndexValid(iSeatEntering))
					{
						const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
						Vector3 vSeatPosition;
						Quaternion qSeatOrientation;
						CModelSeatInfo::CalculateSeatSituation(m_pVehicle, vSeatPosition, qSeatOrientation, m_iTargetEntryPoint, iSeatEntering);
						TUNE_GROUP_FLOAT(ENTER_VEHICLE_DEATH, DEATH_IN_SEAT_MAX_DIST, 0.3f, 0.0f, 1.0f, 0.01f);
						float fDistSqd = (vSeatPosition - vPedPos).XYMag2();
						if (fDistSqd < square(DEATH_IN_SEAT_MAX_DIST))
						{
							m_bShouldBePutIntoSeat = true;
							pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PedsInVehiclePositionNeedsReset, true);
						}			
					}
				}
			
				return false;
			}
			else
			{
				return true;
			}
		}
	}

	if (GetState() >= State_JackPedFromOutside && GetState() < State_Finish
		&& GetState() != State_WaitForValidEntryPoint
		&& GetState() != State_CloseDoor
		&& !(GetState() == State_TryToGrabVehicleDoor && pEvent && ((CEvent*)pEvent)->RequiresAbortForRagdoll()))
	{
		return false;
	}

	// If set to not respond to combat events, dont
	if( IsFlagSet(CVehicleEnterExitFlags::DontAbortForCombat) )
	{
		if( pPed && pEvent && 
			pPed->GetPedIntelligence()->GetEventResponseTask() &&
			pPed->GetPedIntelligence()->GetEventResponseTask()->GetTaskType() == CTaskTypes::TASK_THREAT_RESPONSE )
		{
			return false;
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::DoAbort(const AbortPriority UNUSED_PARAM(priority), const aiEvent* pEvent)
{
	CPed* pPed = GetPed();

	if(!pPed->IsNetworkClone())
	{
		UnreserveDoor(pPed);
		UnreserveSeat(pPed);
	}

	TUNE_GROUP_BOOL(RAGDOLL_WHILE_ENTER_EXIT_VEHICLE, EXCLUDE_SAFETY_CHECK, true);
	if (pPed->GetAttachState() && EXCLUDE_SAFETY_CHECK && pEvent && pEvent->GetEventType() == EVENT_SWITCH_2_NM_TASK)
	{
		// If we're attached and not in the vehicle it should be safe to exclude the vehicle when detaching as we
		// shouldn't be intersecting much
		pPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS|DETACH_FLAG_EXCLUDE_VEHICLE|DETACH_FLAG_IGNORE_SAFE_POSITION_CHECK);
	}

	if (m_moveNetworkHelper.IsNetworkAttached())
	{
		ASSERT_ONLY(taskDebugf2("Frame : %u - %s%s stopped Enter Vehicle Move Network : CTaskEnterVehicle::DoAbort()", fwTimer::GetFrameCount(), pPed->IsNetworkClone() ? "Clone ped : " : "Local ped : ", pPed->GetDebugName()));
		pPed->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, SLOW_BLEND_DURATION);
	}
}

////////////////////////////////////////////////////////////////////////////////

s32	CTaskEnterVehicle::GetDefaultStateAfterAbort() const
{
	if (IsFlagSet(CVehicleEnterExitFlags::ResumeIfInterupted))
	{
        BANK_ONLY(taskDebugf2("Frame : %u - %s%s : 0x%p in %s:%s : 0x%p getting default state after abort %s", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone ped : " : "Local ped : ", GetPed()->GetDebugName(), GetPed(), GetTaskName(), GetStateName(GetState()), this, GetStateName(State_Start)));
		return State_Start;
	}
	return State_Finish;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::ProcessPreFSM()
{
	if (!m_pVehicle)
	{
		return FSM_Quit;
	}
	else if( Unlikely(m_pVehicle->GetIsInReusePool()) ) 
	{
		return FSM_Quit; // vehicles in the reuse pool as essentially deleted, so this task is invalid
	}

	// If a timer is set, count down
	if (m_fTimerSeconds > 0.0f)
	{
		m_fTimerSeconds -= GetTimeStep();
		if (m_fTimerSeconds <= 0.0f)
		{
			SetTimerRanOut(true);
		}
	}

	if (m_pVehicle->GetVelocity().Mag2() > ms_Tunables.m_MinVelocityToConsiderMoving)
	{
		m_uVehicleLastMovingTime = fwTimer::GetTimeInMilliseconds();
	}

	// don't allow the player to enter vehicles that are fading out, allow clones though (the local vehicle maybe invisible due to the local player being in heli cam)
	CPed* pPed = GetPed();
	if (!IsFlagSet(CVehicleEnterExitFlags::Warp) && GetState() <= State_EnterSeat && !pPed->IsNetworkClone())
	{
		CNetObjVehicle* pVehObj = NetworkInterface::IsGameInProgress() ? static_cast<CNetObjVehicle*>(NetworkUtils::GetNetworkObjectFromEntity(m_pVehicle)) : NULL;

		if (pVehObj && (pVehObj->IsFadingOut() || !m_pVehicle->GetIsVisible()))
		{
			AI_LOG_WITH_ARGS("[%s] Ped %s is quitting TASK_ENTER_VEHICLE because vehicle %s is %s", GetTaskName(), AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetDynamicEntityNameSafe(m_pVehicle), pVehObj->IsFadingOut() ? "fading out" : "invisible");
			return FSM_Quit;
		}
	}

	if (pPed->IsLocalPlayer() && pPed->GetIsAttachedInCar() && m_pVehicle && m_pVehicle->IsTurretSeat(m_iSeat))
	{
		u32 turretCameraHash = camInterface::ComputePovTurretCameraHash(m_pVehicle);
		if(turretCameraHash == 0)
		{
			camInterface::GetGameplayDirector().DisableFirstPersonThisUpdate(pPed);
		}
	}

#if __ASSERT
	if( CNetwork::IsGameInProgress() && 
		(GetState() == State_Align || GetState() == State_AlignToEnterSeat) && 
		GetTimeInState() > 10.0f &&
		pPed->GetNetworkObject() &&
		pPed->GetNetworkObject()->GetClonedState() !=0 )
	{
		CPed* pPedReservingSeat = NULL;
		const CPed* pLocalPedReservingSeat = NULL;
		CComponentReservation* pComponentReservation = NULL;

		if (m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint))
		{
			pComponentReservation = m_pVehicle->GetComponentReservationMgr()->GetSeatReservation(m_iTargetEntryPoint, SA_directAccessSeat, m_iSeat);
			if (pComponentReservation)
			{
				pPedReservingSeat = pComponentReservation->GetPedUsingComponent();
				pLocalPedReservingSeat = pComponentReservation->GetLocalPedRequestingUseOfComponent();
			}
		}

		taskAssertf(0,"%s %s: vehicle %s. State %s, Previous State %s, Net State %s. Sub task: %s. m_iTargetEntryPoint %d, m_iSeat %d, pComponentReservation %s, pLocalPedReservingSeat %s\n",
			pPed->GetDebugName(),
			pPed->IsNetworkClone()?"CLONE":"LOCAL",
			m_pVehicle->GetNetworkObject()?m_pVehicle->GetNetworkObject()->GetLogName():"NUll net obj",
			GetStateName(GetState()),
			GetStateName(GetPreviousState()),
			GetStateName(GetStateFromNetwork()),
			GetSubTask()?TASKCLASSINFOMGR.GetTaskName(GetSubTask()->GetTaskType()):"Null sub task",
			m_iTargetEntryPoint,
			m_iSeat,
			pComponentReservation?(pPedReservingSeat?pPedReservingSeat->GetDebugName():"Null ped"):"Null pComponentReservation",
			pLocalPedReservingSeat?pLocalPedReservingSeat->GetDebugName():"Null ped"
			);
	}
#endif

	// Enter vehicle task is not timeslice friendly
	pPed->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate | CPedAILod::AL_LodTimesliceAnimUpdate);

    if(pPed->IsNetworkClone())
    {
        if(pPed->GetUsingRagdoll() && GetState() != State_TryToGrabVehicleDoor)
        {
            return FSM_Quit;
        }

        if(ShouldProcessAttachment())
        {
            fwAttachmentEntityExtension *pedExtension = pPed->GetAttachmentExtension();

            bool bIsAttached = pedExtension && pedExtension->GetAttachParent();

            if(!bIsAttached)
            {
                return FSM_Quit;
            }
        }

        // when the clone ped is lerping to the door it overrides the network blender,
        // we need to turn this off when we have reached the target position
        if(m_bLerpingCloneToDoor)
        {
            if(GetStateFromNetwork() != State_GoToDoor && GetStateFromNetwork() < State_Align)
            {
                m_bLerpingCloneToDoor = false;
            }

            if(GetStateFromNetwork() >= State_Align && GetState() != State_Start)
            {
                m_bLerpingCloneToDoor = false;
            }

            if(GetStateFromNetwork() != State_GoToDoor)
            {
                NetworkInterface::OverrideMoveBlendRatiosThisFrame(*pPed, 0.0f, 0.0f);
            }
        }
    }	
	else if (!pPed->IsLocalPlayer())
	{
		TUNE_GROUP_BOOL(ENTER_VEHICLE_TUNE, FORCE_COMBAT_ENTRY_FOR_AI_PEDS_GETTING_INTO_PLAYERS_VEHICLE, true);
		TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, TIME_BEFORE_FORCE_COMBAT, 5.0f, 0.0f, 30.0f, 0.1f);
		if (m_pVehicle && m_pVehicle->GetDriver() && m_pVehicle->GetDriver()->IsLocalPlayer() && GetTimeRunning() > TIME_BEFORE_FORCE_COMBAT)
		{
			if (pPed->GetMotionData()->GetCurrentMbrY() > MOVEBLENDRATIO_WALK)
			{
				TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, MIN_VELOCITY_TO_USE_COMBAT_ENTRY, 1.0f, 0.0f, 10.0f, 0.1f);
				if (m_pVehicle->GetVelocity().Mag2() >= square(MIN_VELOCITY_TO_USE_COMBAT_ENTRY))
				{
					SetRunningFlag(CVehicleEnterExitFlags::CombatEntry);
				}
			}
		}
	}

    if(m_pVehicle->InheritsFromBike() && m_pVehicle->IsNetworkClone() && m_iSeat == m_pVehicle->GetDriverSeat())
    {
        if(GetState() >= State_PickUpBike)
        {
            static const unsigned BLENDER_OVERRIDE_TIME_MS = 200;
            NetworkInterface::OverrideNetworkBlenderForTime(m_pVehicle, BLENDER_OVERRIDE_TIME_MS);
        }
    }

	if (pPed->IsLocalPlayer())
	{
		bool bWithinRange = false;
		if (ShouldBeInActionModeForCombatEntry(bWithinRange))
		{
			if (bWithinRange)
			{
				static dev_float MIN_ACTION_MODE_TIME = 0.5f;
				pPed->SetUsingActionMode(true, CPed::AME_VehicleEntry, MIN_ACTION_MODE_TIME, false);
				SetRunningFlag(CVehicleEnterExitFlags::CombatEntry);
			}
		}

		if (CTaskEnterVehicle::CNC_ShouldForceMichaelVehicleEntryAnims(*pPed, true))
		{
			const CPed* pSeatOccupier = GetPedOccupyingDirectAccessSeat();
			if (pSeatOccupier)
			{
				const CPlayerInfo* pTargetPlayerInfo = pSeatOccupier->GetPlayerInfo();
				if (pTargetPlayerInfo && pTargetPlayerInfo->GetArcadeInformation().GetTeam() == eArcadeTeam::AT_CNC_CROOK)
				{
					SetRunningFlag(CVehicleEnterExitFlags::CombatEntry);
				}
				else if (pPed->GetPedIntelligence()->IsThreatenedBy(*pSeatOccupier, false))
				{
					SetRunningFlag(CVehicleEnterExitFlags::CombatEntry);
				}
			}
		}

		if (pPed->GetEquippedWeaponInfo() && pPed->GetEquippedWeaponInfo()->GetIsGun1Handed())
		{
			const s32 iPlayerIndex = CPlayerInfo::GetPlayerIndex(*pPed);
			if (iPlayerIndex == 0)
			{
				pPed->SetPedResetFlag(CPED_RESET_FLAG_ForceUnarmedActionMode, true);
			}
		}

		bool bAPCTurretSeat = MI_CAR_APC.IsValid() && m_pVehicle->GetModelIndex() == MI_CAR_APC && m_iSeat == 1;
		bool bTrailerSmall2 = m_pVehicle->IsTrailerSmall2();
		if (!IsFlagSet(CVehicleEnterExitFlags::WantsToJackFriendlyPed) && (!m_pVehicle->IsTurretSeat(m_iSeat) || bTrailerSmall2) && !bAPCTurretSeat && m_pVehicle->IsAnExclusiveDriverPedOrOpenSeat(pPed) && !CheckShouldProcessEnterRearSeat(*pPed, *m_pVehicle))
		{
			CControl* pControl = pPed->GetControlFromPlayer();
			if (pControl)
			{
				if (pControl->GetPedEnter().HistoryHeldDown(ms_Tunables.m_DurationHeldDownEnterButtonToJackFriendly))
				{
					SetRunningFlag(CVehicleEnterExitFlags::WantsToJackFriendlyPed);
					const s32 iDriversSeat = m_pVehicle->GetDriverSeat();
					SetTargetSeat(iDriversSeat, TSR_NOT_TURRET_SEAT_WANTS_TO_JACK_DRIVER, VEHICLE_DEBUG_DEFAULT_FLAGS);
				}
			}
		}

		// If the local player is entering a car, continually check for other players entering the same car.
		// If a ped is found that could conflict with our entry switch to the preferred method which allows conflicts to be resolved.
		// Otherwise stick with specific to prevent the ped from getting in rear seats if both front entrypoints are blocked.
		
		// In case we are forcing the ped to the front or the back the type request will already be prefer so no need
		// to do the logic shown down there
		const bool bForcePedToSeatInCertainSeats = pPed->GetVehicleEntryConfig() && pPed->GetVehicleEntryConfig()->HasAForcingFlagSetForThisVehicle(m_pVehicle);

#if __BANK
		bool bDebugIgnoreMPPreferLogic = false;
		if (CVehicleDebug::ms_bForcePlayerToUseSpecificSeat && m_pVehicle->IsSeatIndexValid(CVehicleDebug::ms_iSeatRequested))
		{
			bDebugIgnoreMPPreferLogic = true;
		}
#endif // __BANK

		if( CNetwork::IsGameInProgress() && !bForcePedToSeatInCertainSeats && !IsFlagSet(CVehicleEnterExitFlags::Warp) BANK_ONLY(&& !bDebugIgnoreMPPreferLogic))
		{
			// Don't do this for vehicles like the trailer
			const CVehicleLayoutInfo* pLayout = m_pVehicle->GetLayoutInfo();
			bool bUsePreferEntryMethod = false; 

			bool bPreventUsingPreferredMethod = false;

			if (IsFlagSet(CVehicleEnterExitFlags::PlayerControlledVehicleEntry) && pLayout && !pLayout->GetHasDriver())
			{
				bPreventUsingPreferredMethod = true;
				AI_LOG_WITH_ARGS("[CTaskEnterVehicle::ProcessPreFSM] Prevent using SR_Prefer entry type due to PlayerControlledVehicleEntry flag and %s not having a driver\n", m_pVehicle->GetVehicleModelInfo()->GetModelName());
			}
			else if (m_iSeatRequestType == SR_Any && m_pVehicle->GetDriver() && m_pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_TURRET_SEAT_ON_VEHICLE))
			{
				bPreventUsingPreferredMethod = true;
				AI_LOG_WITH_ARGS("[CTaskEnterVehicle::ProcessPreFSM] Prevent using SR_Prefer entry type due to current type being SR_ANY, %s having a driver and FLAG_HAS_TURRET_SEAT_ON_VEHICLE set\n", m_pVehicle->GetVehicleModelInfo()->GetModelName());
			}

			if (!bPreventUsingPreferredMethod)
			{
				aiAssert(m_pVehicle->GetVehicleModelInfo());
				const CModelSeatInfo* pModelSeatInfo = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo();
				if( pModelSeatInfo )
				{
					s32 iDriverSeat = m_pVehicle->GetDriverSeat();
					s32 iDirectEntryPoint = m_pVehicle->GetDirectEntryPointIndexForSeat(iDriverSeat);
					s32 iIndirectEntryPoint = m_pVehicle->GetIndirectEntryPointIndexForSeat(iDriverSeat);
					
					Vector3 vZero(0.0f, 0.0f, 0.0f);
					s32 iActualEntryPoint = -1;

					// If any hated players are in the car and we would prefer to jack them, switch to the prefered seat entry method.
					if( IsFlagSet(CVehicleEnterExitFlags::JackWantedPlayersRatherThanStealCar) )
					{
						for (s32 i=0; i<m_pVehicle->GetSeatManager()->GetMaxSeats(); i++)
						{
							CPed* pSeatPed = m_pVehicle->GetSeatManager()->GetPedInSeat(i);
							if (pSeatPed && pSeatPed->IsPlayer() && pSeatPed != pPed && 
								pSeatPed->GetPlayerWanted() && pSeatPed->GetPlayerWanted()->GetWantedLevel() >= WANTED_LEVEL1 )
							{
								bUsePreferEntryMethod = true;
								AI_LOG_WITH_ARGS("[CTaskEnterVehicle::ProcessPreFSM] Use SR_Prefer type due to JackWantedPlayersRatherThanStealCar and %s having wanted level\n",AILogging::GetDynamicEntityNameSafe(pSeatPed));
							}
						}
					}

					bool bPreferedFromCurrentDriver = false;
					const CPed* pCurrentDriver = m_pVehicle->GetSeatManager()->GetPedInSeat(iDriverSeat);
					if(pCurrentDriver)
					{
						if(pCurrentDriver && (pCurrentDriver->IsAPlayerPed() || pCurrentDriver->GetPedConfigFlag(CPED_CONFIG_FLAG_AIDriverAllowFriendlyPassengerSeatEntry)) && pPed->GetPedIntelligence()->IsFriendlyWith(*pCurrentDriver))
						{
							bUsePreferEntryMethod = true;
							bPreferedFromCurrentDriver = true;
							AI_LOG_WITH_ARGS("[CTaskEnterVehicle::ProcessPreFSM] Use SR_Prefer type due to AIDriverAllowFriendlyPassengerSeatEntry on driver (%s) and him being friendly with us\n",AILogging::GetDynamicEntityNameSafe(pCurrentDriver));
						}
					}
					else
					{
						for(int i = 0; i < CVehicle::MAX_NUM_EXCLUSIVE_DRIVERS; ++i)
						{
							pCurrentDriver = m_pVehicle->GetExclusiveDriverPed(i);
							if(pCurrentDriver && (pCurrentDriver->IsAPlayerPed() || pCurrentDriver->GetPedConfigFlag(CPED_CONFIG_FLAG_AIDriverAllowFriendlyPassengerSeatEntry)) && pPed->GetPedIntelligence()->IsFriendlyWith(*pCurrentDriver))
							{
								bUsePreferEntryMethod = true;
								bPreferedFromCurrentDriver = true;
								AI_LOG_WITH_ARGS("[CTaskEnterVehicle::ProcessPreFSM] Use SR_Prefer type due to AIDriverAllowFriendlyPassengerSeatEntry on EXCLUSIVE driver (%s) and him being friendly with us\n",AILogging::GetDynamicEntityNameSafe(pCurrentDriver));
								break;
							}
						}
					}

					if( !bPreferedFromCurrentDriver && CTaskVehicleFSM::IsAnyGroupMemberUsingEntrypoint(pPed, m_pVehicle, iDirectEntryPoint, m_iRunningFlags, false, vZero, &iActualEntryPoint ) )
					{
						// Dont close the door when using the indirect entry point and another player is about to use it behind us
						if( m_iTargetEntryPoint == iIndirectEntryPoint && iActualEntryPoint == iIndirectEntryPoint && GetState() > State_GoToDoor )
						{
							SetRunningFlag(CVehicleEnterExitFlags::DontCloseDoor);
						}
						bUsePreferEntryMethod = true;
						AI_LOG_WITH_ARGS("[CTaskEnterVehicle::ProcessPreFSM] Use SR_Prefer type due to other players from our team using same direct entry point (%i)\n",iDirectEntryPoint);
					}

					if( CTaskVehicleFSM::IsAnyGroupMemberUsingEntrypoint(pPed, m_pVehicle, iIndirectEntryPoint, m_iRunningFlags, false, vZero, &iActualEntryPoint ) )
					{
						// Dont close the door when using the indirect entry point and another player is about to use it behind us
						if( m_iTargetEntryPoint == iIndirectEntryPoint && iActualEntryPoint == iIndirectEntryPoint && GetState() > State_GoToDoor )
						{
							SetRunningFlag(CVehicleEnterExitFlags::DontCloseDoor);
						}
						bUsePreferEntryMethod = true;
						AI_LOG_WITH_ARGS("[CTaskEnterVehicle::ProcessPreFSM] Use SR_Prefer type due to other players from our team using same indirect entry point (%i)\n",iIndirectEntryPoint);
					}

					const SeatRequestType desiredSeatRequestType = bUsePreferEntryMethod ? SR_Prefer : SR_Specific;
					if (m_iSeatRequestType != desiredSeatRequestType)
					{
						SetSeatRequestType(desiredSeatRequestType, bUsePreferEntryMethod ? SRR_OTHER_PEDS_ENTERING : SRR_NO_OTHER_PEDS_ENTERING, VEHICLE_DEBUG_DEFAULT_FLAGS);
					}
				}
			}
		}

		const CVehicleLayoutInfo* pLayoutInfo = m_pVehicle->GetVehicleModelInfo() ? m_pVehicle->GetVehicleModelInfo()->GetVehicleLayoutInfo() : NULL;
		if (pLayoutInfo && !pLayoutInfo->GetMustCloseDoor() && m_iSeat == 0)
		{
			CControl* pControl = pPed->GetControlFromPlayer();
			if (pControl)
			{
				if(pControl->GetVehicleAccelerate().IsDown(ioValue::ANALOG_BUTTON_DOWN_THRESHOLD) || pControl->GetVehicleBrake().IsDown(ioValue::ANALOG_BUTTON_DOWN_THRESHOLD))
				{
					SetRunningFlag(CVehicleEnterExitFlags::DontCloseDoor);
				}
			}
		}
	}

    EnsureLocallyJackedPedsExitVehicle();

    // force network synchronisation of any peds we may interact with while entering the vehicle
    if(pPed->GetNetworkObject())
    {
        // peds entering a vehicle must be synced in the network game, but peds in cars are not
        // synchronised by default, in order to save bandwidth as they don't do much
        NetworkInterface::ForceSynchronisation(pPed);

        if(m_iTargetEntryPoint != -1)
        {
            const CEntryExitPoint *entryExitPoint = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint);

            if(taskVerifyf(entryExitPoint, "Trying to enter a vehicle via an invalid entry/exit point!"))
            {
                s32 seatIndex = entryExitPoint->GetSeat(SA_directAccessSeat);

                ForceNetworkSynchronisationOfPedInSeat(seatIndex);

                if(m_iSeat != seatIndex)
                {
                    ForceNetworkSynchronisationOfPedInSeat(m_iSeat);
                }

                // if a friendly player is trying to get into the local player's seat and there is a space free across
                // from him, shuffle over
                if(pPed->IsNetworkClone())
                {
                    CPed *pPedInSeat = (seatIndex != -1) ? m_pVehicle->GetSeatManager()->GetPedInSeat(seatIndex) : 0;

					bool bVehiclePreventsAutoShuffleForPeds = (MI_PLANE_BOMBUSHKA.IsValid() && m_pVehicle->GetModelIndex() == MI_PLANE_BOMBUSHKA) ||
															  (MI_PLANE_VOLATOL.IsValid() && m_pVehicle->GetModelIndex() == MI_PLANE_VOLATOL);
                    if(!bVehiclePreventsAutoShuffleForPeds && pPedInSeat && pPedInSeat->IsLocalPlayer() && !pPedInSeat->IsInjured() && 
						CTaskVehicleFSM::CanForcePedToShuffleAcross(*pPed, *pPedInSeat, *m_pVehicle) &&
						(pPedInSeat->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInCustody) || !pPedInSeat->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed)))	// Only shuffle if hand cuffed and in custody
                    {
						if (pPedInSeat->GetPedIntelligence()->IsFriendlyWith(*pPed) || (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed) && pPed->GetPedIntelligence()->IsFriendlyWith(*pPedInSeat)))
						{
							s32 indirectSeatIndex = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetShuffleSeatForSeat(m_iTargetEntryPoint, seatIndex);

							if((seatIndex != m_pVehicle->GetDriverSeat()) && (indirectSeatIndex != m_pVehicle->GetDriverSeat()))
							{
								if(indirectSeatIndex != -1 && !m_pVehicle->GetSeatManager()->GetPedInSeat(indirectSeatIndex))
								{
									CComponentReservation *pComponentReservation = m_pVehicle->GetComponentReservationMgr()->GetSeatReservationFromSeat(indirectSeatIndex);

									if(pComponentReservation && ((pComponentReservation->GetPedUsingComponent() == 0) || (pComponentReservation->GetPedUsingComponent() == pPedInSeat)))		// TODO: UNSURE
									{
										if(!pPedInSeat->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE) &&
											!pPedInSeat->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE))
										{
											CEventGivePedTask givePedTask(PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP, rage_new CTaskInVehicleSeatShuffle(m_pVehicle, NULL), true );
											pPedInSeat->GetPedIntelligence()->AddEvent(givePedTask);
											AI_LOG_WITH_ARGS("[EnterVehicle] Local ped %s decided to shuffle to seat %i, beacuse another ped %s is entering seat %i\n", AILogging::GetDynamicEntityNameSafe(pPedInSeat), indirectSeatIndex, AILogging::GetDynamicEntityNameSafe(pPed), seatIndex);
										}
									}
								}
							}
						}
                    }
                }
            }
        }
    }
	
	pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPostMovement, true );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableGaitReduction, true);
	pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableDynamicCapsuleRadius, true);
	pPed->SetPedResetFlag( CPED_RESET_FLAG_IsEnteringVehicle, true);
	pPed->SetPedResetFlag( CPED_RESET_FLAG_IsEnteringOrExitingVehicle, true);
	pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableSteeringAroundVehicles, true);

	if (GetState() >= State_Align)
	{
		pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableCellphoneAnimations, true);
		pPed->SetPedResetFlag(CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching, true);

		if (NetworkInterface::IsGameInProgress())
		{
			bool bPreventVehicleMigrationDelay = false;
			if (pPed->IsLocalPlayer() && m_pVehicle->GetDriver() && m_pVehicle->GetDriver()->IsPlayer())
			{
				bPreventVehicleMigrationDelay = NetworkInterface::IsVehicleLockedForPlayer(m_pVehicle, pPed);
			}

			if (!bPreventVehicleMigrationDelay)
			{
				const unsigned TIME_TO_DELAY_VEHICLE_MIGRATION = 1000;
				NetworkInterface::DelayMigrationForTime(m_pVehicle, TIME_TO_DELAY_VEHICLE_MIGRATION);
			}
		}
	}

	if (m_moveNetworkHelper.IsNetworkActive())
	{
		CTaskVehicleFSM::ProcessBlockRagdollActivation(*pPed, m_moveNetworkHelper);

		if (m_moveNetworkHelper.GetBoolean(ms_BlendInSteeringWheelIkId))
		{
			CTaskMotionInVehicle::ProcessSteeringWheelArmIk(*m_pVehicle, *pPed);
		}
	}

	if (!CVehicleClipRequestHelper::ms_Tunables.m_DisableStreamedVehicleAnimRequestHelper)
	{
		const fwMvClipSetId commonClipsetId = CTaskEnterVehicleAlign::GetCommonClipsetId(m_pVehicle, m_iTargetEntryPoint, IsOnVehicleEntry());
		CTaskVehicleFSM::RequestVehicleClipSet(commonClipsetId, SP_High);

		const fwMvClipSetId entryClipsetId = CTaskEnterVehicleAlign::GetEntryClipsetId(m_pVehicle, m_iTargetEntryPoint, pPed);
		CTaskVehicleFSM::RequestVehicleClipSet(entryClipsetId, SP_High);

		const fwMvClipSetId inVehicleClipsetId = CTaskMotionInAutomobile::GetInVehicleClipsetId(*pPed, m_pVehicle, m_iSeat);
		CTaskVehicleFSM::RequestVehicleClipSet(inVehicleClipsetId, pPed->IsLocalPlayer() ? SP_High : SP_Medium);

		// B*2023900: stream in new generic FPS helmet anims if we're entering an aircraft
		if (pPed->IsLocalPlayer() && m_pVehicle->GetIsAircraft())
		{
			const fwMvClipSetId sClipSetId = CPedHelmetComponent::GetAircraftPutOnTakeOffHelmetFPSClipSet(m_pVehicle);
			CTaskVehicleFSM::RequestVehicleClipSet(sClipSetId, SP_High);
		}
	}

	//Note that a ped is entering.
	m_pVehicle->GetIntelligence()->PedIsEntering();

	if (CNC_ShouldForceMichaelVehicleEntryAnims(*pPed) && !IsFlagSet(CVehicleEnterExitFlags::CombatEntry))
	{
		const CVehicleEntryPointAnimInfo* pEntryPointClipInfo = m_pVehicle->GetVehicleModelInfo()->GetEntryPointAnimInfo(m_iTargetEntryPoint);
		if (pEntryPointClipInfo)
		{
			const s32 iMichaelIndex = 0;
			fwFlags32 uInformationFlags = 0;
			fwMvClipSetId michaelJackAlivePedClipSet = pEntryPointClipInfo->GetCustomPlayerJackingClipSet(iMichaelIndex, &uInformationFlags);
			if (michaelJackAlivePedClipSet != CLIP_SET_ID_INVALID)
			{
				CTaskVehicleFSM::RequestVehicleClipSet(michaelJackAlivePedClipSet, SP_High);	
			}
		}
	}

	return FSM_Continue;
}	

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed& rPed = *GetPed();

	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();	

		FSM_State(State_ExitVehicle)
			FSM_OnEnter
				ExitVehicle_OnEnter();
			FSM_OnUpdate
				return ExitVehicle_OnUpdate();

		FSM_State(State_GoToVehicle)
			FSM_OnEnter
				GoToVehicle_OnEnter();
			FSM_OnUpdate
				return GoToVehicle_OnUpdate();

		FSM_State(State_PickDoor)
			FSM_OnEnter
				return PickDoor_OnEnter();
			FSM_OnUpdate
				return PickDoor_OnUpdate();

		FSM_State(State_GoToDoor)
			FSM_OnEnter
				GoToDoor_OnEnter();
			FSM_OnUpdate
				return GoToDoor_OnUpdate();

		FSM_State(State_GoToSeat)
			FSM_OnEnter
				GoToSeat_OnEnter();
			FSM_OnUpdate
				return GoToSeat_OnUpdate();

		FSM_State(State_GoToClimbUpPoint)
			FSM_OnEnter
				return GoToClimbUpPoint_OnEnter();
			FSM_OnUpdate
				return GoToClimbUpPoint_OnUpdate();			

		FSM_State(State_StreamAnimsToAlign)
			FSM_OnEnter
				return StreamAnimsToAlign_OnEnter();
			FSM_OnUpdate
				return StreamAnimsToAlign_OnUpdate();

		FSM_State(State_StreamAnimsToOpenDoor)
			FSM_OnUpdate
				return StreamAnimsToOpenDoor_OnUpdate();

		FSM_State(State_Align)
			FSM_OnEnter
				Align_OnEnter();
			FSM_OnUpdate
				return Align_OnUpdate();

		FSM_State(State_ReserveSeat)
			FSM_OnUpdate
				return ReserveSeat_OnUpdate();

		FSM_State(State_ReserveDoorToOpen)
			FSM_OnUpdate
				return ReserveDoorToOpen_OnUpdate();

		FSM_State(State_WaitForStreamedEntry)
			FSM_OnUpdate
				return WaitForStreamedEntry_OnUpdate();

		FSM_State(State_StreamedEntry)
			FSM_OnEnter
				return StreamedEntry_OnEnter();
			FSM_OnUpdate
				return StreamedEntry_OnUpdate();
			
		FSM_State(State_ClimbUp)
			FSM_OnEnter
				return ClimbUp_OnEnter();
			FSM_OnUpdate
				return ClimbUp_OnUpdate();
			FSM_OnExit
				return ClimbUp_OnExit();

		FSM_State(State_VaultUp)
			FSM_OnEnter
				return VaultUp_OnEnter();
			FSM_OnUpdate
				return VaultUp_OnUpdate();

		FSM_State(State_PickUpBike)
			FSM_OnEnter
				PickUpBike_OnEnter();
			FSM_OnUpdate
				return PickUpBike_OnUpdate();

		FSM_State(State_PullUpBike)
			FSM_OnEnter
				PullUpBike_OnEnter();
			FSM_OnUpdate
				return PullUpBike_OnUpdate();

		FSM_State(State_SetUprightBike)
			FSM_OnUpdate
				return SetUprightBike_OnUpdate();

		FSM_State(State_OpenDoor)
			FSM_OnEnter
				return OpenDoor_OnEnter(rPed);
			FSM_OnUpdate
				return OpenDoor_OnUpdate();

		FSM_State(State_AlignToEnterSeat)
			FSM_OnEnter
				return AlignToEnterSeat_OnEnter();
			FSM_OnUpdate
				return AlignToEnterSeat_OnUpdate();

		FSM_State(State_WaitForGroupLeaderToEnter)
			FSM_OnUpdate
				return WaitForGroupLeaderToEnter_OnUpdate();

		FSM_State(State_WaitForEntryPointReservation)
			FSM_OnUpdate
				return WaitForEntryPointReservation_OnUpdate();
		
		FSM_State(State_JackPedFromOutside)
			FSM_OnEnter
				return JackPedFromOutside_OnEnter();
			FSM_OnUpdate
				return JackPedFromOutside_OnUpdate();

		FSM_State(State_UnReserveDoorToShuffle)
			FSM_OnUpdate
				return UnReserveDoorToShuffle_OnUpdate();

		FSM_State(State_UnReserveDoorToFinish)
			FSM_OnUpdate
				return UnReserveDoorToFinish_OnUpdate();

		FSM_State(State_StreamAnimsToEnterSeat)
			FSM_OnUpdate
				return StreamAnimsToEnterSeat_OnUpdate();

		FSM_State(State_EnterSeat)
			FSM_OnEnter
				EnterSeat_OnEnter();
			FSM_OnUpdate
				return EnterSeat_OnUpdate();
			FSM_OnExit
				EnterSeat_OnExit();

        FSM_State(State_WaitForPedToLeaveSeat)
            FSM_OnEnter
                WaitForPedToLeaveSeat_OnEnter();
            FSM_OnUpdate
                return WaitForPedToLeaveSeat_OnUpdate();

		FSM_State(State_SetPedInSeat)
			FSM_OnEnter
				return SetPedInSeat_OnEnter();
			FSM_OnUpdate
				return SetPedInSeat_OnUpdate();

		FSM_State(State_ReserveDoorToClose)
			FSM_OnUpdate
				return ReserveDoorToClose_OnUpdate();

		FSM_State(State_CloseDoor)
			FSM_OnEnter
				CloseDoor_OnEnter();
			FSM_OnUpdate
				return CloseDoor_OnUpdate();

		FSM_State(State_UnReserveSeat)
			FSM_OnUpdate
				return UnReserveSeat_OnUpdate();

		FSM_State(State_ShuffleToSeat)
			FSM_OnEnter
				ShuffleToSeat_OnEnter();
			FSM_OnUpdate
				return ShuffleToSeat_OnUpdate();

		FSM_State(State_TryToGrabVehicleDoor)
			FSM_OnEnter
				return TryToGrabVehicleDoor_OnEnter();
			FSM_OnUpdate
				return TryToGrabVehicleDoor_OnUpdate();

		FSM_State(State_WaitForValidEntryPoint)
			FSM_OnUpdate
				return WaitForValidEntryPoint_OnUpdate();

		FSM_State(State_Ragdoll)
			FSM_OnEnter
				return Ragdoll_OnEnter();
			FSM_OnUpdate
				return Ragdoll_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

        FSM_State(State_WaitForNetworkSync)

	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::UpdateClonedFSM( s32 iState, CTask::FSM_Event iEvent )
{
	// if the state change is not handled by the clone FSM block below
	// we need to keep the state in sync with the values received from the network
	if(!StateChangeHandledByCloneFSM(iState))
	{
		if(iEvent == OnUpdate)
		{
			if(CanUpdateStateFromNetwork())
			{
				AI_FUNCTION_LOG_WITH_ARGS("Clone %s is changing task state to %s", AILogging::GetDynamicEntityNameSafe(GetPed()), GetStaticStateName(GetStateFromNetwork()));
				SetTaskState(GetStateFromNetwork());
			}
		}
	}

	FSM_Begin

		// Wait for a network update to decide which state to start in
		FSM_State(State_Start)
			FSM_OnUpdate
				Start_OnUpdateClone();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return Finish_OnUpdateClone();

        FSM_State(State_ExitVehicle)
			FSM_OnUpdate
				ExitVehicle_OnUpdateClone();

		FSM_State(State_Align)
			FSM_OnEnter
				Align_OnEnterClone();
			FSM_OnUpdate
				Align_OnUpdateClone();
			FSM_OnExit
				Align_OnExitClone();

		FSM_State(State_ClimbUp)
			FSM_OnEnter
				return ClimbUp_OnEnter();
			FSM_OnUpdate
				return ClimbUp_OnUpdate();
			FSM_OnExit
				return ClimbUp_OnExit();

		FSM_State(State_VaultUp)
			FSM_OnEnter
				return VaultUp_OnEnter();
			FSM_OnUpdate
				return VaultUp_OnUpdate();

		FSM_State(State_StreamAnimsToOpenDoor)
			FSM_OnEnter
				StreamAnimsToOpenDoor_OnEnterClone();
			FSM_OnUpdate
				return StreamAnimsToOpenDoor_OnUpdate();
			FSM_OnExit
				StreamAnimsToOpenDoor_OnExitClone();

		FSM_State(State_OpenDoor)
			FSM_OnEnter
				return OpenDoor_OnEnter(*GetPed());
			FSM_OnUpdate
				return OpenDoor_OnUpdateClone();

		FSM_State(State_AlignToEnterSeat)
			FSM_OnEnter
				return AlignToEnterSeat_OnEnter();
			FSM_OnUpdate
				return AlignToEnterSeat_OnUpdate();

		FSM_State(State_PickUpBike)
			FSM_OnEnter
				PickUpBike_OnEnter();
			FSM_OnUpdate
				return PickUpBike_OnUpdate();

		FSM_State(State_PullUpBike)
			FSM_OnEnter
				PullUpBike_OnEnter();
			FSM_OnUpdate
				return PullUpBike_OnUpdate();

		FSM_State(State_SetUprightBike)
			FSM_OnUpdate
				return SetUprightBike_OnUpdate();

		FSM_State(State_JackPedFromOutside)
			FSM_OnEnter
				JackPedFromOutside_OnEnter();
			FSM_OnUpdate
				return JackPedFromOutside_OnUpdate();

		FSM_State(State_StreamAnimsToAlign)
			FSM_OnEnter
				StreamAnimsToAlign_OnEnterClone();
			FSM_OnUpdate			
				return StreamAnimsToAlign_OnUpdate();
			FSM_OnExit				
				StreamAnimsToAlign_OnExitClone();

		FSM_State(State_StreamAnimsToEnterSeat)
			FSM_OnEnter
				StreamAnimsToEnterSeat_OnEnterClone();
			FSM_OnUpdate			
				return StreamAnimsToEnterSeat_OnUpdate();
			FSM_OnExit				
				StreamAnimsToEnterSeat_OnExitClone();


		FSM_State(State_EnterSeat)
			FSM_OnEnter
				EnterSeat_OnEnter();
			FSM_OnUpdate
				EnterSeat_OnUpdate();

		FSM_State(State_WaitForStreamedEntry)
			FSM_OnUpdate
				return WaitForStreamedEntry_OnUpdate();

		FSM_State(State_StreamedEntry)
			FSM_OnEnter
				StreamedEntry_OnEnter();
			FSM_OnUpdate
				StreamedEntry_OnUpdate();

        FSM_State(State_WaitForPedToLeaveSeat)
            FSM_OnEnter
                WaitForPedToLeaveSeat_OnEnter();
            FSM_OnUpdate
                return WaitForPedToLeaveSeat_OnUpdate();

		FSM_State(State_SetPedInSeat)
			FSM_OnEnter
				SetPedInSeat_OnEnter();
			FSM_OnUpdate
				return SetPedInSeat_OnUpdate();

		FSM_State(State_CloseDoor)
			FSM_OnEnter
				CloseDoor_OnEnter();
			FSM_OnUpdate
				return CloseDoor_OnUpdate();

        FSM_State(State_TryToGrabVehicleDoor)
            FSM_OnEnter
				return TryToGrabVehicleDoor_OnEnter();
			FSM_OnUpdate
				return TryToGrabVehicleDoor_OnUpdate();

		FSM_State(State_WaitForValidEntryPoint)
			FSM_OnUpdate
				return WaitForValidEntryPoint_OnUpdate();

		FSM_State(State_ShuffleToSeat)
			FSM_OnEnter
				ShuffleToSeat_OnEnter();
			FSM_OnUpdate
				return ShuffleToSeat_OnUpdate();

		FSM_State(State_WaitForNetworkSync)
			FSM_OnUpdate
				return WaitForNetworkSync_OnUpdate();

		// unhandled states
		FSM_State(State_PickDoor)
        FSM_State(State_GoToClimbUpPoint)
		FSM_State(State_GoToDoor)
		FSM_State(State_ReserveDoorToOpen)
		FSM_State(State_WaitForGroupLeaderToEnter)
		FSM_State(State_WaitForEntryPointReservation)
		FSM_State(State_UnReserveDoorToShuffle)
		FSM_State(State_UnReserveDoorToFinish)
		FSM_State(State_ReserveSeat)
		FSM_State(State_UnReserveSeat)
		FSM_State(State_ReserveDoorToClose)
		FSM_State(State_GoToSeat)
		FSM_State(State_Ragdoll)

	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::StateChangeHandledByCloneFSM(s32 iState)
{
	switch(iState)
	{
		case State_Start:
        case State_ExitVehicle:
		case State_Align:
		case State_ClimbUp:
		case State_VaultUp:
		case State_StreamAnimsToOpenDoor:
		case State_OpenDoor:
		case State_AlignToEnterSeat:
        case State_PickUpBike:
        case State_PullUpBike:
		case State_EnterSeat:
		case State_WaitForStreamedEntry:
		case State_StreamedEntry:
        case State_WaitForPedToLeaveSeat:
		case State_JackPedFromOutside:
		case State_SetPedInSeat:
		case State_CloseDoor:
        case State_TryToGrabVehicleDoor:
		case State_ShuffleToSeat:
		case State_WaitForNetworkSync:
		case State_Finish:
			return true;
		default:
			return false;
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::CanUpdateStateFromNetwork()
{
    s32 iStateFromNetwork = GetStateFromNetwork();
    s32 iCurrentState     = GetState();

    if(iStateFromNetwork != iCurrentState)
    {
        // we can't go back to the open door state from certain states
        if(iStateFromNetwork == State_OpenDoor)
        {
            if(!IsMoveTransitionAvailable(State_OpenDoor))
            {
                return false;
            }
        }

#if __BANK
		if (iCurrentState == State_StreamAnimsToAlign)
		{
			fwMvClipSetId commonClipsetId = CTaskEnterVehicleAlign::GetCommonClipsetId(m_pVehicle, m_iTargetEntryPoint);
			if (!CTaskVehicleFSM::IsVehicleClipSetLoaded(commonClipsetId))
			{
				AI_LOG_WITH_ARGS("[%s] - Clone ped %s prevented from updating state from network to %s in state %s", GetTaskName(), AILogging::GetDynamicEntityNameSafe(GetPed()), GetStaticStateName(iStateFromNetwork), GetStaticStateName(iCurrentState));
				//We are commenting this out so that the behaviour doesn't change. Right now we are just logging out so as to verify the cause.
				//return false;
			}
		}
#endif

		if(iStateFromNetwork == State_CloseDoor && !GetPed()->GetIsInVehicle())
		{
			AI_LOG_WITH_ARGS("[%s] - Clone ped %s prevented from updating state from network to %s in state %s", GetTaskName(), AILogging::GetDynamicEntityNameSafe(GetPed()), GetStaticStateName(iStateFromNetwork), GetStaticStateName(iCurrentState));
			return false;
		}

        // we don't want to change the local task state to these states based on
        // network updates, although these state are processed as part of the clone task,
        // they only make sense to be set based on the internal working of the task
        switch(iStateFromNetwork)
        {
		case State_ReserveSeat:
        case State_StreamAnimsToAlign:
        case State_StreamAnimsToOpenDoor:
        case State_WaitForPedToLeaveSeat:
        case State_SetPedInSeat:
            return false;
        default:
            return true;
        }
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::ProcessPostFSM()
{	
	CPed* pPed = GetPed();

    if(!pPed->IsNetworkClone() && m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint))
    {
	    // If the task should currently have the door reserved, check that it does
	    if (GetState() >= State_ReserveDoorToOpen && GetState() < State_UnReserveDoorToFinish)
	    {
		    CComponentReservation* pComponentReservation = m_pVehicle->GetComponentReservationMgr()->GetDoorReservation(m_iTargetEntryPoint);
		    if (pComponentReservation && !CanUnreserveDoorEarly(*pPed, *m_pVehicle))
		    {
				if (pComponentReservation->GetPedUsingComponent() == pPed)
				{
					pComponentReservation->SetPedStillUsingComponent(pPed);
				}
				else
				{
					CComponentReservationManager::ReserveVehicleComponent(pPed, m_pVehicle, pComponentReservation);
				}
		    }
	    }

	    // If the task should currently have the seat reserved, check that it does
	    if (GetState() >= State_ReserveSeat && GetState() < State_UnReserveSeat)
	    {
			taskAssert(m_pVehicle);
			taskAssert(m_pVehicle->GetComponentReservationMgr());
		    CComponentReservation* pComponentReservation = m_pVehicle->GetComponentReservationMgr()->GetSeatReservation(m_iTargetEntryPoint, SA_directAccessSeat, m_iSeat);
		    if (pComponentReservation && pComponentReservation->GetPedUsingComponent() == pPed)
		    {
			    pComponentReservation->SetPedStillUsingComponent(pPed);
		    }

			// May want to do this for any seat
			if (m_pVehicle->IsTurretSeat(m_iSeat))
			{
				CComponentReservation* pDesiredComponentReservation = m_pVehicle->GetComponentReservationMgr()->GetSeatReservationFromSeat(m_iSeat);
				if (pDesiredComponentReservation && pComponentReservation != pDesiredComponentReservation)
				{
					if (pDesiredComponentReservation->GetPedUsingComponent() == pPed)
					{
						pDesiredComponentReservation->SetPedStillUsingComponent(pPed);
					}
					else if (!pDesiredComponentReservation->GetPedUsingComponent())
					{
						pDesiredComponentReservation->ReserveUseOfComponent(pPed);
					}
				}
			}
	    }
    }

	if (m_moveNetworkHelper.IsNetworkActive() && GetState() > State_Align)
	{
		const bool bIsDead = m_pJackedPed ? m_pJackedPed->IsInjured() : false;
		m_moveNetworkHelper.SetFlag( bIsDead, ms_IsDeadId);
	}

	//be a little bit less aggressive in culling peds entering vehicles
	//2 reasons:
	//1: vehicles have a longer cull distance than peds
	//2: peds with orders have this set, and if they lose their orders
	//they tend to try to return to their vehicle and go on patrol
	//this lets us cover the cap between when they lose their orders
	//and when they reach the vehicle
	pPed->SetPedResetFlag(CPED_RESET_FLAG_CullExtraFarAway, true);

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////
REGISTER_TUNE_GROUP_BOOL(bDisableFix, false);

bool CTaskEnterVehicle::ProcessPhysics(float fTimeStep, int nTimeSlice)
{
	if (m_pVehicle)
	{
		// switch the vehicle back to real or the ped will not be correctly attached to the vehicle once he gets in
		m_pVehicle->m_nVehicleFlags.bPreventBeingDummyThisFrame = true;
		INSTANTIATE_TUNE_GROUP_BOOL(BUG_6862132, bDisableFix);

		CPed* pPed = GetPed();
		const bool bIsInAir = m_pVehicle->IsInAir();

		// If waiting for the heli to stop moving down, make sure we don't get moved down through the ground as we're attached, sigh
		if (!pPed->IsNetworkClone() && GetState() == State_Align && !GetSubTask() && m_pVehicle->InheritsFromHeli() && pPed->GetIsAttached())
		{
			Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			
			AI_LOG_WITH_ARGS("[CTaskEnterVehicle::ProcessPhysics] Ped %s m_pVehicle->IsInAir() = %s, State = %s\n", AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetBooleanAsString(bIsInAir), GetStaticStateName(GetState()));
			if (bDisableFix || bIsInAir)
			{
				ProcessHeliPreventGoingThroughGroundLogic(*pPed, *static_cast<const CVehicle*>(m_pVehicle.Get()), vPedPosition, -1.0f, -1.0f);
			}
			Vec3V vNewPos = RCC_VEC3V(vPedPosition);
			QuatV qNewAng = pPed->GetTransform().GetOrientation();
			qNewAng = Normalize(qNewAng);
			CTaskVehicleFSM::UpdatePedVehicleAttachment(pPed, vNewPos, qNewAng);
			return CTask::ProcessPhysics(fTimeStep, nTimeSlice);
		}

		// States that play clips need to update the peds matrix
		if (ShouldProcessAttachment() && pPed->GetIsAttached())
		{
			const s32 iCurrentState = GetState();

			Vector3 vNewPedPosition(Vector3::ZeroType);
			Quaternion qNewPedOrientation(0.0f,0.0f,0.0f,1.0f);

			// We can get on bikes when they're on their side, so do something 'special' for them
			if (iCurrentState == State_PickUpBike || iCurrentState == State_PullUpBike)
			{
				float fClipPhase = 0.0f;

				const crClip* pClip = GetClipAndPhaseForState(fClipPhase);
				// Need to pass whether we're picking or pulling since the bikes side vector changes
				s32 iEntryFlags = iCurrentState == State_PickUpBike ? CModelSeatInfo::EF_IsPickUp : CModelSeatInfo::EF_IsPullUp;
				if (fClipPhase > 0.0f)
				{
					iEntryFlags |= CModelSeatInfo::EF_AnimPlaying;
				}

				// Compute our desired attach position (somewhere between the pickup/pullup point and the quick get on point)
				Vector3 vTargetPosition;
				Quaternion qTargetOrientation;
				CModelSeatInfo::CalculateEntrySituation(m_pVehicle, pPed, vTargetPosition, qTargetOrientation, m_iTargetEntryPoint, iEntryFlags, fClipPhase, &m_vLocalSpaceEntryModifier);
				qNewPedOrientation.Normalize();
				m_PlayAttachedClipHelper.SetTarget(vTargetPosition, qTargetOrientation);

				// Add on the initial offsets, we'll fixup the initial offset below
				Vector3 vTranOffset = m_PlayAttachedClipHelper.GetInitialTranslationOffset();
				Quaternion qRotOffset = m_PlayAttachedClipHelper.GetInitialRotationOffset();
				vNewPedPosition = vTargetPosition + vTranOffset;
				qNewPedOrientation = qTargetOrientation;

				Quaternion qInitialOrientation;
				qInitialOrientation.MultiplyInverse(qTargetOrientation, qRotOffset);

				float fLerpRatio = fClipPhase;

				if (pClip)
				{
					sMoverFixupInfo moverFixupInfo;
					CTaskVehicleFSM::GetMoverFixupInfoFromClip(moverFixupInfo, pClip, m_pVehicle->GetVehicleModelInfo()->GetModelNameHash());

					if (moverFixupInfo.bFoundZFixup)
					{
						fLerpRatio = Clamp(fClipPhase - moverFixupInfo.fZFixUpStartPhase / moverFixupInfo.fZFixUpEndPhase - moverFixupInfo.fZFixUpStartPhase, 0.0f, 1.0f);
					}
					
					if (!ms_Tunables.m_DisableMoverFixups)
					{
						// Apply the mover fixup to remove the initial offsets
						Vector3 vOffset = -vTranOffset;
						Quaternion qOffset = qRotOffset;
						qOffset.Inverse();
						m_PlayAttachedClipHelper.ApplyVehicleEntryOffsets(vNewPedPosition, qNewPedOrientation, pClip, vOffset, qOffset, fClipPhase);
					}


					TUNE_GROUP_FLOAT(BIKE_TUNE, MAX_PHASE_TO_USE_GROUND_HEIGHT, 0.9f, 0.0f, 1.0f, 0.01f);
					if (fClipPhase <= MAX_PHASE_TO_USE_GROUND_HEIGHT)
					{
						Vector3 vGroundPos(Vector3::ZeroType);
						if (CTaskVehicleFSM::FindGroundPos(m_pVehicle, GetPed(), vNewPedPosition, 2.5f, vGroundPos))
						{
							vNewPedPosition.z = vGroundPos.z;
						}
					}
				}	

				qTargetOrientation.PrepareSlerp(qInitialOrientation);
				qNewPedOrientation.Lerp(fLerpRatio, qInitialOrientation, qTargetOrientation);

				// Make the orientation upright
				qNewPedOrientation.FromRotation(ZAXIS, qNewPedOrientation.TwistAngle(ZAXIS));
				qNewPedOrientation.Normalize();

				// Recompute the initial offsets
				const Vector3 vNewOffset = vNewPedPosition - vTargetPosition;
				Quaternion qNewRotOffset;
				qNewRotOffset.MultiplyInverse(qTargetOrientation, qNewPedOrientation);
				m_PlayAttachedClipHelper.SetInitialTranslationOffset(vNewOffset);
				m_PlayAttachedClipHelper.SetInitialRotationOffset(qNewRotOffset);
			}
			else
			{
				// Update the target situation
				Vector3 vNewTargetPosition(Vector3::ZeroType);
				Quaternion qNewTargetOrientation(0.0f,0.0f,0.0f,1.0f);

				const bool bIsDoingCombatOpenDoor = GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_OPEN_VEHICLE_DOOR_FROM_OUTSIDE && GetSubTask()->GetState() == CTaskOpenVehicleDoorFromOutside::State_OpenDoorCombat;
				if (!bIsDoingCombatOpenDoor)
				{
					if (ShouldUseClimbUpTarget())
					{
						ComputeClimbUpTarget(vNewTargetPosition, qNewTargetOrientation);
					}
					else if (ShouldUseIdealTurretSeatTarget())
					{
						ComputeIdealTurretSeatTarget(vNewTargetPosition, qNewTargetOrientation);
					}
					else if (ShouldUseGetInTarget())
					{
						ComputeGetInTarget(vNewTargetPosition, qNewTargetOrientation);
					}
					else if (ShouldUseEntryTarget())
					{
						ComputeEntryTarget(vNewTargetPosition, qNewTargetOrientation);
					}
					else // Use Seat Target
					{
						ComputeSeatTarget(vNewTargetPosition, qNewTargetOrientation);	
					}
					m_PlayAttachedClipHelper.SetTarget(vNewTargetPosition, qNewTargetOrientation);
				}

#if DEBUG_DRAW
				Vector3 vOrientationEulers;
				qNewTargetOrientation.ToEulers(vOrientationEulers);
				Vector3 vDebugOrientation(0.0f,1.0f,0.0f);
				vDebugOrientation.RotateZ(vOrientationEulers.z);
				ms_debugDraw.AddSphere(RCC_VEC3V(vNewTargetPosition), 0.025f, Color_green, 1000);
				Vector3 vRotEnd = vNewTargetPosition + vDebugOrientation;
				ms_debugDraw.AddArrow(RCC_VEC3V(vNewTargetPosition), RCC_VEC3V(vRotEnd), 0.025f, Color_green, 1000);
#endif

				// Compute initial situation relative to the target
				vNewPedPosition = vNewTargetPosition;
				qNewPedOrientation = qNewTargetOrientation;

				const bool bIsAlignToEnter = (GetState() == State_OpenDoor || GetState() == State_AlignToEnterSeat) ? true : false;
				if (GetPreviousState() == State_PickUpBike || GetPreviousState() == State_PullUpBike || GetPreviousState() == State_SetUprightBike)
				{
					vNewPedPosition = m_vPreviousPedPosition;
					qNewPedOrientation.FromEulers(Vector3(0.0f,0.0f,m_fInitialHeading));
				}
				else if (GetState() != State_CloseDoor)
				{
					m_PlayAttachedClipHelper.ComputeInitialSituation(vNewPedPosition, qNewPedOrientation);
				}

				float fClipPhase = 0.0f;
				const crClip* pClip = GetClipAndPhaseForState(fClipPhase);
				if (pClip)
				{
					float fComputeStartPhase = m_bDoingExtraGetInFixup ? m_fExtraGetInFixupStartPhase : 0.0f;
					float fComputeEndPhase = m_bDoingExtraGetInFixup ? m_fExtraGetInFixupEndPhase : 1.0f;

					// Update the situation from the current phase in the clip
					m_PlayAttachedClipHelper.ComputeSituationFromClip(pClip, fComputeStartPhase, fClipPhase, vNewPedPosition, qNewPedOrientation);

					// Only apply the target fixup if required (e.g. opening the door does not need to move to the target)

					if (!ms_Tunables.m_DisableMoverFixups && (GetVehicleClipFlags() & CTaskVehicleFSM::VF_ApplyToTargetFixup))
					{
						// Compute the offsets we'll need to fix up
						Vector3 vOffsetPosition(Vector3::ZeroType);
						Quaternion qOffsetOrientation(0.0f,0.0f,0.0f,1.0f);
						m_PlayAttachedClipHelper.ComputeOffsets(pClip, fClipPhase, fComputeEndPhase, vNewPedPosition, qNewPedOrientation, vOffsetPosition, qOffsetOrientation);

						float fApplyOffsetsClipPhase = fClipPhase;
						if(m_bDoingExtraGetInFixup)
						{
							fApplyOffsetsClipPhase = (fClipPhase - fComputeStartPhase) / (fComputeEndPhase - fComputeStartPhase);
						}

						bool bGettingOnDirectWhenOnSide = false;
						if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_ENTER_VEHICLE_SEAT)
						{
							bGettingOnDirectWhenOnSide = static_cast<CTaskEnterVehicleSeat*>(GetSubTask())->GetBikeWasOnSideAtBeginningOfGetOn();
						}

						bool bTryLockedDoorState = GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_OPEN_VEHICLE_DOOR_FROM_OUTSIDE && GetSubTask()->GetState() == CTaskOpenVehicleDoorFromOutside::State_TryLockedDoor;

						// Apply the fixup
						const bool bApplyFixUpIfTagsNotFound = (!bIsAlignToEnter || (m_pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HELI_USES_FIXUPS_ON_OPEN_DOOR) && !bTryLockedDoorState) );
						m_PlayAttachedClipHelper.ApplyVehicleEntryOffsets(vNewPedPosition, 
							qNewPedOrientation, 
							pClip, 
							vOffsetPosition, 
							qOffsetOrientation, 
							fApplyOffsetsClipPhase, 
							bApplyFixUpIfTagsNotFound,
							bGettingOnDirectWhenOnSide,
							m_pVehicle);
					}			
				}
				else if (GetState() == State_OpenDoor)
				{
					return false;
				}

				// Try to prevent going through the ground when getting on
				// (we can't do this continually through the get in as we may need to end up closer to the ground when seated)
				// probably should be tag driven, but I don't feel like adding yet another tag... B*1358022
				TUNE_GROUP_BOOL(VEHICLE_ENTRY_TUNE, ENABLE_HELI_GROUND_PENETRATION_PREVENTION, true);
				if (ENABLE_HELI_GROUND_PENETRATION_PREVENTION && m_pVehicle->InheritsFromHeli() && (GetState() == State_EnterSeat || GetState() == State_JackPedFromOutside || GetState() == State_StreamedEntry))
				{
					AI_LOG_WITH_ARGS("[CTaskEnterVehicle::ProcessPhysics] Ped %s m_pVehicle->IsInAir() = %s, State = %s\n", AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetBooleanAsString(bIsInAir), GetStaticStateName(GetState()));
					if (bDisableFix || bIsInAir)
					{
						ProcessHeliPreventGoingThroughGroundLogic(*pPed, *m_pVehicle, vNewPedPosition, m_fLegIkBlendOutPhase, fClipPhase);
					}	
				}
				qNewPedOrientation.Normalize();
			}
			
#if __ASSERT
			if(pPed->IsPlayer())
			{
				CNetObjPed* pPedObj = SafeCast(CNetObjPed, pPed->GetNetworkObject());
				if (pPedObj && pPedObj->GetPlayerOwner() && !pPedObj->GetPlayerOwner()->IsInDifferentTutorialSession())
				{
					if ((vNewPedPosition - VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().GetPosition())).Mag2() > (50.0f * 50.0f))
					{
						taskWarningf("Calculated an attachment position too far away from the vehicle attached to ped (%s) was entering vehicle (%s) task state (%s)!", pPed->GetDebugName(), m_pVehicle->GetModelName(), GetStaticStateName(GetState()));
					}
				}
			}
#endif

			// Can't use the vehicle ground physical as this gets lost when being picked up
			TUNE_GROUP_BOOL(ENTER_VEHICLE_TUNE, FIXUP_RELATIVE_POSITION_ENTER_ON_VEHICLE, true);
			bool bDontSetTranslation = false;
			if (FIXUP_RELATIVE_POSITION_ENTER_ON_VEHICLE && m_pVehicle->InheritsFromBike() && GetState() == State_EnterSeat && (GetPreviousState() == State_PickUpBike || GetPreviousState() == State_PullUpBike || GetPreviousState() == State_SetUprightBike) && pPed->GetGroundPhysical() && pPed->GetGroundPhysical()->GetIsTypeVehicle())
			{
				// Set attachment offsets
				Vector3 vAttachOffset = pPed->GetAttachOffset();
				TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, RELATIVE_SEAT_APPROACH_RATE, 2.0f, 0.0f, 10.0f, 0.01f);
				vAttachOffset.ApproachStraight(VEC3_ZERO, RELATIVE_SEAT_APPROACH_RATE, fTimeStep);
				pPed->SetAttachOffset(vAttachOffset);
				bDontSetTranslation = true;
			}

			// Pass in the new position and orientation in global space, this will get transformed relative to the vehicle for attachment
			CTaskVehicleFSM::UpdatePedVehicleAttachment(pPed, VECTOR3_TO_VEC3V(vNewPedPosition), QUATERNION_TO_QUATV(qNewPedOrientation), bDontSetTranslation);
		}
        else
        {
            if(pPed->IsNetworkClone())
            {
                bool bRemoteStateValidForLerp = (GetStateFromNetwork() == State_GoToDoor);
                bool bLocalStateValidForLerp  = (GetStateFromNetwork() >= State_Align && GetStateFromNetwork() != State_WaitForValidEntryPoint && GetState() == State_Start);
                bool bStateValidForLerp = (bRemoteStateValidForLerp || bLocalStateValidForLerp);

                if(bStateValidForLerp && m_iTargetEntryPoint != -1)
                {
                    Vector3 vEntryPos(Vector3::ZeroType);
                    Quaternion qEntryOrientation(0.0f,0.0f,0.0f,1.0f);
                    CModelSeatInfo::CalculateEntrySituation(m_pVehicle.Get(), pPed, vEntryPos, qEntryOrientation, m_iTargetEntryPoint, 0, 0.0f, &m_vLocalSpaceEntryModifier);

                    Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
                    Vector3 vDeltaPos    = vEntryPos - vPedPosition;

                    // we're only interested in the flat distance here
                    vDeltaPos.z = 0.0f;

                    float fDistSqr = vDeltaPos.Mag2();

                    TUNE_FLOAT(START_CLONE_LERP_TO_DOOR_DIST, 1.0f, 0.0f, 10.0f, 0.1f);
                    TUNE_FLOAT(STOP_CLONE_LERP_TO_DOOR_DIST, 2.0f, 0.0f, 10.0f, 0.1f);

					float fStartCloneLerpToDoorDist = START_CLONE_LERP_TO_DOOR_DIST;

					u32 modelNameHash = m_pVehicle->GetVehicleModelInfo()->GetModelNameHash();
					if(modelNameHash == MI_CAR_BRICKADE.GetName().GetHash())
					{
						// Bump up lerp range for people getting onto this vehicle.
						fStartCloneLerpToDoorDist = 1.5f;
					}

                    if(!m_bLerpingCloneToDoor)
                    {
                        if(fDistSqr <= rage::square(fStartCloneLerpToDoorDist))
                        {
                            m_bLerpingCloneToDoor = true;
                        }
                    }
                    else 
                    {
                        if(fDistSqr > rage::square(STOP_CLONE_LERP_TO_DOOR_DIST))
                        {
                            m_bLerpingCloneToDoor = false;
                        }
                        else
                        {
                            TUNE_FLOAT(CLONE_LERP_RATE, 0.1f, 0.05f, 1.0f, 0.05f);

                            if(fTimeStep > 0.0f)
                            {
                                TUNE_INT(CLONE_FRAMES_TO_REACH_TARGET, 3, 1, 10, 1);

                                float   fDesiredTimeToReachTarget = fTimeStep * CLONE_FRAMES_TO_REACH_TARGET;
                                Vector3 vPedVelocity              = pPed->GetVelocity();
                                Vector3 vDesiredVel               = (vDeltaPos * CLONE_LERP_RATE) / fDesiredTimeToReachTarget;
                                Vector3 vVelToReachTarget         = vDeltaPos / fDesiredTimeToReachTarget;

                                Vector3 vPedPosNextAtCurrentVel = vPedPosition + (vPedVelocity * fDesiredTimeToReachTarget);
                                Vector3 vCurrentVelDeltaPos     = vEntryPos - vPedPosNextAtCurrentVel;
                                Vector3 vPedPosNextAtDesiredVel = vPedPosition + (vDesiredVel  * fDesiredTimeToReachTarget);
                                Vector3 vDesiredVelDeltaPos     = vEntryPos - vPedPosNextAtDesiredVel;
                                float   fCurrentDistDistSqr     = vCurrentVelDeltaPos.Mag2();
                                float   fDesiredDistDistSqr     = vDesiredVelDeltaPos.Mag2();

                                Vector3 vNormalisedDeltaPos = vDeltaPos;
                                vNormalisedDeltaPos.Normalize();
                                vCurrentVelDeltaPos.Normalize();
                                vDesiredVelDeltaPos.Normalize();

                                bool bCurrentVelWillOverShoot = vNormalisedDeltaPos.Dot(vCurrentVelDeltaPos) > 0.0f;
                                bool bDesiredVelWillOverShoot = vNormalisedDeltaPos.Dot(vDesiredVelDeltaPos) > 0.0f;

                                if(!bCurrentVelWillOverShoot)
                                {
                                    if(bDesiredVelWillOverShoot || (fCurrentDistDistSqr < fDesiredDistDistSqr))
                                    {
                                        vDesiredVel = vPedVelocity;
                                    }
                                }
                                else if(bDesiredVelWillOverShoot)
                                {
                                    vDesiredVel = vVelToReachTarget;
                                }

                                // if the remote is still going to the door, limit the lerp speed to the speed of
                                // the ped on the local machine, once the remote is aligning switch to full speed
                                // to catch up as quickly as possible
                                if(GetStateFromNetwork() == State_GoToDoor)
                                {
                                    const float MAX_SPEED_TO_PREDICT = 10.0f;
                                    Vector3 predictedVelocity = NetworkInterface::GetPredictedVelocity(pPed, MAX_SPEED_TO_PREDICT);

                                    if(predictedVelocity.Mag2() < vDesiredVel.Mag2())
                                    {
                                        vDesiredVel.Normalize();
                                        vDesiredVel.Scale(predictedVelocity.Mag());
                                    }
                                }

                                pPed->SetDesiredVelocityClamped(vDesiredVel, CTaskMotionBase::ms_fAccelLimitHigher*fTimeStep);

                                if(fDistSqr < rage::square(0.1f))
                                {
                                    pPed->GetMotionData()->SetCurrentMoveBlendRatio(0.0f, 0.0f);
                                }

                                // we need to blend the heading here - as we are overriding the network blender
                                // but only want to override the position of the ped
                                NetworkInterface::UpdateHeadingBlending(*pPed);
                            }
                        }
                    }
                }
            }
        }
	}

	// Base class
	return CTask::ProcessPhysics(fTimeStep, nTimeSlice);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::ProcessPostMovement()
{
	// Stream clipsets for the vehicle
	if (GetState() >= State_PickDoor && m_pVehicle && m_pVehicle->IsSeatIndexValid(m_iSeat))
	{
		s32 flags = CVehicleClipRequestHelper::AF_StreamEntryClips | CVehicleClipRequestHelper::AF_StreamSeatClips;
		if(m_pVehicle->InheritsFromBoat())
		{
			flags |= CVehicleClipRequestHelper::AF_StreamConnectedSeatAnims;
		}
		m_VehicleClipRequestHelper.RequestClipsFromVehicle(m_pVehicle, flags, m_iSeat);
		m_VehicleClipRequestHelper.Update(fwTimer::GetTimeStep());
	}

	if (m_moveNetworkHelper.IsNetworkAttached())
	{
		m_fLastPhase = m_moveNetworkHelper.GetFloat(ms_PickPullPhaseId);
		m_fLastPhase = rage::Clamp(m_fLastPhase, 0.0f, 1.0f);
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::ProcessMoveSignals()
{
	if (GetEntity() && GetPed()->IsLocalPlayer())
	{
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_BlockIkWeaponReactions, true);
	}

	if(m_bAnimFinished)
	{
		// Already finished, nothing to do.
		return false;
	}

	const int state = GetState();
	fwMvBooleanId finishedEventId;
	fwMvFloatId phaseId;
	bool bRequiresProcessing = false;
	if(state == State_JackPedFromOutside)
	{
		finishedEventId = ms_JackClipFinishedId;
		phaseId = ms_JackPedPhaseId;
		bRequiresProcessing = true;

		if (m_pVehicle)
		{
			m_pVehicle->m_nVehicleFlags.bForceOtherVehiclesToStopForThisVehicle = true;
		}
	}
	else if(state == State_ClimbUp)
	{
		finishedEventId = ms_ClimbUpAnimFinishedId;
		phaseId = ms_ClimbUpPhaseId;
		bRequiresProcessing = true;
	}
	else if(state == State_PickUpBike || state == State_PullUpBike)
	{
		finishedEventId = ms_PickOrPullUpClipFinishedId;
		phaseId = ms_PickPullPhaseId;
		bRequiresProcessing = true;
	}
	else if(state == State_AlignToEnterSeat)
	{
		finishedEventId = ms_AlignToEnterSeatAnimFinishedId;
		phaseId = ms_AlignToEnterSeatPhaseId;
		bRequiresProcessing = true;
	}

	if (GetPed() && GetPed()->IsLocalPlayer() && m_pVehicle)
	{
		m_pVehicle->m_nVehicleFlags.bForceOtherVehiclesToStopForThisVehicle = true;
	}

	if (bRequiresProcessing)
	{
		if (IsMoveNetworkStateFinished(finishedEventId, phaseId))
		{
			// Done, we need no more updates.
			m_bAnimFinished = true;
			return false;
		}

		// Still waiting.
		return true;
	}

	// Nothing to do.
	return false;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::Start_OnUpdate()
{
	ASSERT_ONLY(CheckForFixedPed(*GetPed(), IsFlagSet(CVehicleEnterExitFlags::Warp)));

	CPed* pPed = GetPed();
	if (m_moveNetworkHelper.IsNetworkActive())
	{
		pPed->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, NORMAL_BLEND_DURATION);
		m_moveNetworkHelper.ReleaseNetworkPlayer();
	}
	ClearRunningFlag(CVehicleEnterExitFlags::UsedClimbUp);
	
	// Store last time that tried to enter vehicle so we can wait for him even if he is no longer doing it
	pPed->GetPedIntelligence()->StartedEnterVehicle();

#if __BANK
	if (pPed->IsLocalPlayer())
	{
		CVehicleLockDebugInfo lockInfo(pPed, m_pVehicle);
		lockInfo.Print();
		AI_LOG_WITH_ARGS("[EnterVehicle] INITIAL_PARAMS for ped %s - m_iTargetEntryPoint = %i, m_iSeatRequestType = %s, m_iSeat = %i\n", AILogging::GetDynamicEntityNameSafe(pPed), m_iTargetEntryPoint, CSeatAccessor::GetSeatRequestTypeString(m_iSeatRequestType), m_iSeat);
	}
#endif // __BANK

	// B*1649000
	if (pPed->IsLocalPlayer() && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PreventUsingLowerPrioritySeats))
	{
		aiAssertf(0, "Player shouldn't have this flag set ever");
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PreventUsingLowerPrioritySeats, false);
	}

	taskFatalAssertf(m_pVehicle, "Vehicle pointer is null - how can this happen?");
	taskFatalAssertf(m_pVehicle->GetVehicleModelInfo(), "Vehicle %s has no vehicle model info", m_pVehicle->GetModelName());
	taskAssertf(m_pVehicle->GetNumberEntryExitPoints() > 0, "Vehicle %s doesn't have any entry points", m_pVehicle->GetModelName());
	m_pVehicle->TryToMakePedsFromPretendOccupants();

	const CVehicleEntryPointAnimInfo* pClipInfo = m_pVehicle->GetVehicleModelInfo()->GetEntryPointAnimInfo(0);
	if (taskVerifyf(pClipInfo, "Vehicle has no entry clip info"))
	{
		// Reset the static counter to prevent issues with players/peds thinking they're stuck
		pPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().ResetStaticCounter();

		m_VehicleClipRequestHelper.SetPed(pPed);
		if (pPed->IsLocalPlayer() && (pPed->WantsToUseActionMode() || pPed->IsUsingStealthMode() || pPed->GetPlayerWanted()->GetWantedLevel() > WANTED_CLEAN))
		{
			// CNC: cops shouldn't use combat entry in action/stealth mode unless target is a crook or hostile.
			if (!NetworkInterface::IsInCopsAndCrooks() || !CNC_ShouldForceMichaelVehicleEntryAnims(*pPed, true))
			{
				SetRunningFlag(CVehicleEnterExitFlags::CombatEntry);
			}
		}

		// If we're already in a vehicle that isn't the one we were tasked to enter, or not in the correct seat, and we're not warping, then exit the current vehicle
		if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && !IsFlagSet(CVehicleEnterExitFlags::Warp))
		{
			// Not in correct vehicle or seat, we need to exit our current vehicle (TODO: could maybe shuffle)
			if (pPed->GetMyVehicle() != m_pVehicle || m_iSeat != m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed))
			{
				SetTaskState(State_ExitVehicle);
				return FSM_Continue;
			}
			else
			{
				// Ped already in correct vehicle and seat, finish
				SetTaskState(State_Finish);
				return FSM_Continue;
			}
		}
		else
		{
			taskAssertf(IsFlagSet(CVehicleEnterExitFlags::Warp) || !pPed->GetIsInVehicle(), "Ped 0x%p is inside vehicle %s", pPed, pPed->GetMyVehicle()->GetModelName());
			// See if we're already in the correct vehicle/seat
			if (IsFlagSet(CVehicleEnterExitFlags::Warp) && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) && pPed->GetMyVehicle() == m_pVehicle)
			{
				if (GetSeatRequestType() == SR_Specific && m_pVehicle->IsSeatIndexValid(m_iSeat))
				{
					if (m_pVehicle->GetSeatManager()->GetPedInSeat(m_iSeat) == pPed)
					{
						SetTaskState(State_Finish);
						return FSM_Continue;
					}
				}
			}

			// If we're warping, skip to picking the door
			if (IsFlagSet(CVehicleEnterExitFlags::Warp) || IsFlagSet(CVehicleEnterExitFlags::WarpToEntryPosition) || CTaskVehicleFSM::CanPedWarpIntoVehicle(*pPed, *m_pVehicle))
			{
				SetTaskState(State_PickDoor);
				return FSM_Continue;
			}

			// Don't start evaluating entry points until within range
			taskFatalAssertf(m_pVehicle->GetVehicleModelInfo(), "NULL model info in CTaskEnterVehicle::Start_OnUpdate");
			TUNE_GROUP_FLOAT(VEHICLE_SELECTION_TUNE, CAR_PICK_DOOR_RELATIVE_VELOCITY_MAG, 4.0f, 0.0f, 50.0f, 0.01f);
			const float fCarPickDoorTargetSpeedSq = square(CAR_PICK_DOOR_RELATIVE_VELOCITY_MAG);
			const float fStopRadius = m_pVehicle->GetVehicleModelInfo()->GetBoundingSphereRadius()+ms_Tunables.m_DistanceToEvaluateDoors;
			const Vec3V vPedPosition = pPed->GetTransform().GetPosition();
			const Vec3V vVehPosition = m_pVehicle->GetTransform().GetPosition();
			taskAssertf(IsFiniteAll(vPedPosition), "Ped position wasn't finite (%.2f, %.2f %.2f)", VEC3V_ARGS(vPedPosition));
			taskAssertf(IsFiniteAll(vVehPosition), "Veh position wasn't finite (%.2f, %.2f %.2f)", VEC3V_ARGS(vVehPosition));

			Vector3 vRelativeVelocity = m_pVehicle->GetRelativeVelocity(*pPed);
			if (( IsLessThanAll(DistSquared(vPedPosition, vVehPosition), ScalarVFromF32(fStopRadius*fStopRadius)) && 
				vRelativeVelocity.Mag2() < fCarPickDoorTargetSpeedSq ) )
			{
				SetTaskState(State_PickDoor);
			}
			else
			{
				SetTaskState(State_GoToVehicle);
			}
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::Start_OnUpdateClone()
{
    CPed* pPed = GetPed();

    if (m_moveNetworkHelper.IsNetworkActive())
    {
        pPed->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, NORMAL_BLEND_DURATION);
        m_moveNetworkHelper.ReleaseNetworkPlayer();
    }
    ClearRunningFlag(CVehicleEnterExitFlags::UsedClimbUp);

    if(pPed->GetIsInVehicle())
    {
        if(pPed->GetMyVehicle() != m_pVehicle)
        {
            if(GetStateFromNetwork() == State_GoToVehicle || GetStateFromNetwork() == State_GoToDoor)
            {
                pPed->SetPedOutOfVehicle();
            }
        }
        else
        {
            // if we're already in the vehicle just abort the clone task - if we are in a different seat
            // the network code will fix this up, and this clone task is not intended to support shuffling only
            SetTaskState(State_Finish);

            return FSM_Continue;
        }
    }

    if(StateChangeHandledByCloneFSM(GetStateFromNetwork()))
    {
        if(GetStateFromNetwork() == State_ExitVehicle && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
        {
            CTask *exitVehicleTask = pPed->GetPedIntelligence()->CreateCloneTaskForTaskType(CTaskTypes::TASK_EXIT_VEHICLE);

            if(exitVehicleTask)
            {
                SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
                SetNewTask(exitVehicleTask);
                SetTaskState(State_ExitVehicle);
            }
        }
        else if(GetStateFromNetwork()==State_Finish || !pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(GetTaskType()))
        {
            // need to finish the task once it has finished on the remote clone
            SetTaskState(State_Finish);
        }
        else if(m_iTargetEntryPoint != -1 && (GetStateFromNetwork() >= State_Align))
        {
            if (IsFlagSet(CVehicleEnterExitFlags::Warp) || IsFlagSet(CVehicleEnterExitFlags::WarpOverPassengers))
            {
                m_iCurrentSeatIndex = m_iSeat;
                SetTaskState(State_WaitForPedToLeaveSeat);
            }
            else
            {
                // start the countdown timer if it has not already been started
                if(m_iWaitForPedTimer == 0)
                {
                    const unsigned TIME_TO_WAIT_BEFORE_FORCE_ALIGNING = 250;
                    m_iWaitForPedTimer = TIME_TO_WAIT_BEFORE_FORCE_ALIGNING;
                }

                m_iWaitForPedTimer -= static_cast<s16>(GetTimeStepInMilliseconds());
                m_iWaitForPedTimer = MAX(0, m_iWaitForPedTimer);

                if((m_iWaitForPedTimer == 0) || IsFlagSet(CVehicleEnterExitFlags::EnteringOnVehicle) || CTaskEnterVehicleAlign::ShouldTriggerAlign(*m_pVehicle, *pPed, m_iTargetEntryPoint, &m_vLocalSpaceEntryModifier, IsFlagSet(CVehicleEnterExitFlags::CombatEntry), false, false, IsFlagSet(CVehicleEnterExitFlags::EnteringOnVehicle)))
                {
                    m_iWaitForPedTimer = 0;

                    const CVehicleEntryPointAnimInfo* pClipInfo = m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint);
	                taskFatalAssertf(pClipInfo, "NULL Clip Info for entry index %i", m_iTargetEntryPoint);

					const fwMvClipSetId entryClipsetId = pClipInfo?pClipInfo->GetEntryPointClipSetId():CLIP_SET_ID_INVALID;
                    bool animsLoaded = CTaskVehicleFSM::IsVehicleClipSetLoaded(entryClipsetId);

                    if(CVehicleClipRequestHelper::ms_Tunables.m_DisableStreamedVehicleAnimRequestHelper || animsLoaded)
                    {
                        m_moveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskEnterVehicle);

			            pPed->GetMovePed().SetTaskNetwork(m_moveNetworkHelper, NORMAL_BLEND_DURATION);
			            m_moveNetworkHelper.SetClipSet(entryClipsetId);

						const s32 iState = GetStateFromNetwork();
						if ((iState == State_PickUpBike || iState == State_PullUpBike) && StartMoveNetwork(*pPed, *m_pVehicle, m_moveNetworkHelper, m_iTargetEntryPoint, ms_Tunables.m_BikePickUpAlignBlendDuration, &m_vLocalSpaceEntryModifier, false, m_OverridenEntryClipSetId))
						{
							SetTaskState(iState);
						}
						else
						{
							if (iState == State_OpenDoor && IsFlagSet(CVehicleEnterExitFlags::CombatEntry))
							{
								SetTaskState(State_StreamAnimsToOpenDoor);
								return FSM_Continue;
							}
							else
							{
								SetTaskState(State_StreamAnimsToAlign);
								return FSM_Continue;
							}
						}
                    }
                }
            }
        }
    }

    return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::ExitVehicle_OnEnter()
{
	taskAssert(GetPed()->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ));
	VehicleEnterExitFlags iExitFlags;
	iExitFlags.BitSet().Set(CVehicleEnterExitFlags::DontWaitForCarToStop);
	iExitFlags.BitSet().Set(CVehicleEnterExitFlags::WarpIfDoorBlocked);

	if (IsFlagSet(CVehicleEnterExitFlags::Warp))
	{
		iExitFlags.BitSet().Set(CVehicleEnterExitFlags::Warp);
	}
	SetNewTask(rage_new CTaskLeaveAnyCar(0, iExitFlags));
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::ExitVehicle_OnUpdate()
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
        if (GetPed()->IsLocalPlayer() && GetPreviousState() == State_SetPedInSeat)
        {
            SetTaskState(State_Finish);
        }
        else
		{
            SetTaskState(State_Start);
        }
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::ExitVehicle_OnUpdateClone()
{
    if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetTaskState(State_Start);
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void  CTaskEnterVehicle::GoToVehicle_OnEnter()
{
	m_TaskTimer.Reset(ms_Tunables.m_TimeBetweenPositionUpdates);

	taskFatalAssertf(m_pVehicle->GetVehicleModelInfo(), "NULL model info in CTaskEnterVehicle::GoToVehicle_OnEnter");
	const float fStopRadius = m_pVehicle->GetVehicleModelInfo()->GetBoundingSphereRadius()+ms_Tunables.m_DistanceToEvaluateDoors;

	Vector3 vTarget = VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().GetPosition());
	Vector3 vPedToTarget = vTarget - VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
	vPedToTarget.Normalize();
	vTarget -= vPedToTarget;

	CTaskMoveFollowNavMesh* pTask = rage_new CTaskMoveFollowNavMesh(GetPedMoveBlendRatio(), vTarget, fStopRadius, fStopRadius, -1, true, false, NULL, CTaskMoveFollowNavMesh::ms_fDefaultCompletionRadius, false);
	pTask->SetUseLargerSearchExtents(true);
	pTask->SetDontStandStillAtEnd(true);

	// Don't path through water as this will not be a valid task then
	pTask->SetNeverEnterWater(!m_pVehicle->HasWaterEntry());

	CTask* pCombatTask = NULL;
	if(m_pTargetPed && GetPed()->GetWeaponManager() && GetPed()->GetWeaponManager()->GetIsArmed())
	{
		pCombatTask = rage_new CTaskCombatAdditionalTask(CTaskCombatAdditionalTask::RT_DefaultNoFiring, m_pTargetPed, VEC3V_TO_VECTOR3(m_pTargetPed->GetTransform().GetPosition()));
	}

	SetNewTask(rage_new CTaskComplexControlMovement(pTask, pCombatTask));
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::GoToVehicle_OnUpdate()
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetTaskState(State_PickDoor);
	}
	else
	{
		// Check to see if the player wants to quit entering
		if (CheckPlayerExitConditions(*GetPed(), *m_pVehicle, m_iRunningFlags, GetTimeRunning()))
		{
			SetTaskFinishState(TFR_PED_INTERRUPTED, VEHICLE_DEBUG_DEFAULT_FLAGS);
		}
		else if(IsFlagSet(CVehicleEnterExitFlags::Warp))
		{
			m_bWarping = true;

			SetTaskState(State_PickDoor);
		}
		// If the warp timer had been set and has ran out, warp the ped now
		else if (GetTimerRanOut())
		{
			SetRunningFlag(CVehicleEnterExitFlags::Warp);
			m_bWarping = true;

			SetTaskState(State_PickDoor);
			return FSM_Continue;
		}
		else if (m_TaskTimer.Tick())
		{
			const CPed* pPed = GetPed();
			if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
			{
				CTaskComplexControlMovement* pControlTask = static_cast<CTaskComplexControlMovement*>(GetSubTask());
				CTask * pRunningMoveTask = pControlTask->GetRunningMovementTask(pPed);
				if (pRunningMoveTask && pRunningMoveTask->GetTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
				{
					Vector3 vTarget = VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().GetPosition());
					Vector3 vPedToTarget = vTarget - VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
					vPedToTarget.Normalize();
					vTarget -= vPedToTarget;

					static_cast<CTaskMoveFollowNavMesh*>(pRunningMoveTask)->SetTarget(pPed, VECTOR3_TO_VEC3V(vTarget));
					static_cast<CTaskMoveFollowNavMesh*>(pRunningMoveTask)->SetKeepMovingWhilstWaitingForPath(true);
				}
			}
			m_TaskTimer.Reset(ms_Tunables.m_TimeBetweenPositionUpdates);
		}
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::PickDoor_OnEnter()
{
	if (m_pVehicle->InheritsFromBike())
	{
		m_iRunningFlags.BitSet().Set(CVehicleEnterExitFlags::UseDirectEntryOnly);
	}
	else if((m_pVehicle->GetIsAquatic() && GetPed()->GetGroundPhysical() == m_pVehicle) || m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::EnteringOnVehicle))
	{
		m_iRunningFlags.BitSet().Set(CVehicleEnterExitFlags::UseDirectEntryOnly);
	}

	//! Clear in water flag and entering on vehicle flag if we have to re-pick door.
	ClearRunningFlag(CVehicleEnterExitFlags::InWater);		

	ResetRagdollOnCollision(*GetPed());

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::PickDoor_OnUpdate()
{
	// Pick the car door to enter the vehicle
	CPed* pPed = GetPed();

	// Don't allow entry when bike is being picked up
	if (m_pVehicle->GetVehicleType() == VEHICLE_TYPE_BICYCLE && static_cast<CBike*>(m_pVehicle.Get())->m_nBikeFlags.bGettingPickedUp)
	{
		ASSERT_ONLY(aiDisplayf("Frame %u, %s ped %s quitting enter vehicle task due to bicycle getting picked up", fwTimer::GetFrameCount(), pPed->IsNetworkClone() ? "CLONE" : "LOCAL", pPed->GetDebugName());)
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	if (MI_CAR_APC.IsValid() && m_pVehicle->GetModelIndex() == MI_CAR_APC)
	{
		m_iRunningFlags.BitSet().Set(CVehicleEnterExitFlags::UseDirectEntryOnly);
	}

	// If its possible to friendly jack, go to that seat while we're held down
	TUNE_GROUP_BOOL(TURRET_ENTRY_TUNE, FORCED_TURRET_TAKES_PRIORITY_OVER_DRIVER_JACK, true);
	TUNE_GROUP_BOOL(ENTER_VEHICLE_TUNE, FORCED_REAR_SEAT_ACTIVITIES_TAKES_PRIORITY_OVER_DRIVER_JACK, true);
	const bool bCheckForOptionalJackOrRide = CheckForOptionalJackOrRideAsPassenger(*pPed, *m_pVehicle);
	if (bCheckForOptionalJackOrRide)
	{
		if (NetworkInterface::IsGameInProgress() && m_pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_TURRET_SEAT_ON_VEHICLE))
			SetSeatRequestType(SR_Prefer, SRR_OPTIONAL_JACK_EVALUATION, VEHICLE_DEBUG_DEFAULT_FLAGS);

		if (!IsFlagSet(CVehicleEnterExitFlags::HasCheckedForFriendlyJack))
		{
			CControl* pControl = pPed->GetControlFromPlayer();
			if (pControl && pControl->GetPedEnter().IsDown())
			{
				m_iRunningFlags.BitSet().Set(CVehicleEnterExitFlags::ConsiderJackingFriendlyPeds);
				SetTargetSeat(m_pVehicle->GetDriverSeat(), TSR_FORCED_DRIVER_SEAT_USAGE_ACTIVE, VEHICLE_DEBUG_DEFAULT_FLAGS);
			}
		}
		else
		{				
			SetTargetSeat(m_pVehicle->GetDriverSeat(), TSR_FORCED_DRIVER_SEAT_USAGE_INACTIVE, VEHICLE_DEBUG_DEFAULT_FLAGS);
			ClearRunningFlag(CVehicleEnterExitFlags::ConsiderJackingFriendlyPeds);
		}
	}
	
	// If its possible to force entry onto the turret seat, go to that seat while we're held down
	if ((!bCheckForOptionalJackOrRide || FORCED_TURRET_TAKES_PRIORITY_OVER_DRIVER_JACK) && !IsFlagSet(CVehicleEnterExitFlags::Warp))
	{
		const s32 iBestTurretSeatIndex = CheckForOptionalForcedEntryToTurretSeatAndGetTurretSeatIndex(*pPed, *m_pVehicle);
		if (m_pVehicle->IsSeatIndexValid(iBestTurretSeatIndex))
		{
			CControl* pControl = pPed->GetControlFromPlayer();
			const bool bEnterTapped = pControl && pControl->GetPedEnter().HistoryPressed(CTaskEnterVehicle::TIME_TO_DETECT_ENTER_INPUT_TAP) && pControl->GetPedEnter().IsReleased();
			if (pControl && (pControl->GetPedEnter().IsDown() || (pPed->IsGoonPed() && bEnterTapped && pPed->GetGroundPhysical() == m_pVehicle)))
			{
				SetSeatRequestType(SR_Prefer, SRR_OPTIONAL_TURRET_EVALUATION_HELD_DOWN, VEHICLE_DEBUG_DEFAULT_FLAGS);
				SetTargetSeat(iBestTurretSeatIndex, TSR_FORCED_TURRET_SEAT_USAGE_ACTIVE, VEHICLE_DEBUG_DEFAULT_FLAGS);
			}
			else
			{	
				if (m_pVehicle->IsAnExclusiveDriverPedOrOpenSeat(pPed))			
				{
					SetSeatRequestType(SR_Prefer, SRR_OPTIONAL_TURRET_EVALUATION_RELEASED_DRIVER_AVAILABLE, VEHICLE_DEBUG_DEFAULT_FLAGS);
					SetTargetSeat(m_pVehicle->GetDriverSeat(), TSR_FORCED_TURRET_SEAT_USAGE_INACTIVE, VEHICLE_DEBUG_DEFAULT_FLAGS);
				}
				else
				{
					SetSeatRequestType(SR_Any, SRR_OPTIONAL_TURRET_EVALUATION_RELEASED_DRIVER_NOT_AVAILABLE, VEHICLE_DEBUG_DEFAULT_FLAGS);
					SetTargetSeat(-1, TSR_FORCED_TURRET_SEAT_USAGE_INACTIVE, VEHICLE_DEBUG_DEFAULT_FLAGS);
				}
				ClearRunningFlag(CVehicleEnterExitFlags::ConsiderJackingFriendlyPeds);
			}
		}
	}

	// B*2282719: If vehicle (ie Luxor2) has rear seats tagged for activities, enter rear seat if entry input is held down.
	if ((!bCheckForOptionalJackOrRide || FORCED_REAR_SEAT_ACTIVITIES_TAKES_PRIORITY_OVER_DRIVER_JACK) && CheckShouldProcessEnterRearSeat(*pPed, *m_pVehicle))
	{
		s32 iRearSeatIndex = CheckForOptionalForcedEntryToRearSeatAndGetSeatIndex(*pPed, *m_pVehicle);
		if (m_pVehicle->IsSeatIndexValid(iRearSeatIndex))
		{
			CControl* pControl = pPed->GetControlFromPlayer();
			if (pControl && pControl->GetPedEnter().IsDown())
			{
				SetSeatRequestType(SR_Prefer, SRR_FORCE_GET_INTO_PASSENGER_SEAT, VEHICLE_DEBUG_DEFAULT_FLAGS);
				SetTargetSeat(iRearSeatIndex, TSR_FORCED_USE_REAR_SEATS, VEHICLE_DEBUG_DEFAULT_FLAGS);
			}
			else
			{
				if (m_pVehicle->IsAnExclusiveDriverPedOrOpenSeat(pPed))			
				{
					SetSeatRequestType(SR_Prefer, SRR_OPTIONAL_TURRET_EVALUATION_RELEASED_DRIVER_AVAILABLE, VEHICLE_DEBUG_DEFAULT_FLAGS);
					SetTargetSeat(m_pVehicle->GetDriverSeat(), TSR_FORCED_TURRET_SEAT_USAGE_INACTIVE, VEHICLE_DEBUG_DEFAULT_FLAGS);
				}
				else
				{
					SetSeatRequestType(SR_Any, SRR_OPTIONAL_TURRET_EVALUATION_RELEASED_DRIVER_NOT_AVAILABLE, VEHICLE_DEBUG_DEFAULT_FLAGS);
					SetTargetSeat(-1, TSR_FORCED_TURRET_SEAT_USAGE_INACTIVE, VEHICLE_DEBUG_DEFAULT_FLAGS);
				}
				ClearRunningFlag(CVehicleEnterExitFlags::ConsiderJackingFriendlyPeds);		
			}
		}
	}

	if (m_pVehicle->InheritsFromBoat() && (GetPed()->GetGroundPhysical() == m_pVehicle) && CheckShouldProcessEnterRearSeat(*pPed, *m_pVehicle))
	{
		if(ProcessOptionalForcedRearSeatDelay(pPed, m_pVehicle))
		{
			return FSM_Continue;
		}
	}

	bool bIsOppressor2 = MI_BIKE_OPPRESSOR2.IsValid() && m_pVehicle->GetModelIndex() == MI_BIKE_OPPRESSOR2;

	if (!pPed->IsLocalPlayer() && m_pVehicle->IsInAir() && !IsFlagSet(CVehicleEnterExitFlags::Warp) && !bIsOppressor2)
	{
		SetTaskState(State_WaitForValidEntryPoint);
		return FSM_Continue;
	}

	// Do not allow passengers to enter the vehicle
	if (m_pVehicle->IsSeatIndexValid(m_iSeat) && m_pVehicle->GetDoorLockStateFromSeatIndex(m_iSeat) == CARLOCK_LOCKED_NO_PASSENGERS)
	{
		s32 iDriverSeat = m_pVehicle->GetDriverSeat();
		CPed *pPedInTargetSeat = m_pVehicle->GetSeatManager()->GetPedInSeat(iDriverSeat);
		
		if (m_iSeat != iDriverSeat || pPedInTargetSeat)
		{
			SetTaskFinishState(TFR_VEHICLE_IS_LOCKED_TO_PASSENGERS, VEHICLE_DEBUG_DEFAULT_FLAGS | VEHICLE_DEBUG_PRINT_PED_IN_DRIVER_SEAT);
			return FSM_Continue;
		}

		// Do not allow ped to shuffle in, otherwise peds might enter from both sides simultaneously
		m_iRunningFlags.BitSet().Set(CVehicleEnterExitFlags::UseDirectEntryOnly);
		SetSeatRequestType(SR_Specific, SRR_NO_PASSENGERS_ALLOWED, VEHICLE_DEBUG_DEFAULT_FLAGS);
	}

	if (m_pVehicle->IsSeatIndexValid(m_iSeat) && !m_pVehicle->CanSeatBeAccessedViaAnEntryPoint(m_iSeat))
	{
		m_bWarping = true;
		m_iRunningFlags.BitSet().Set(CVehicleEnterExitFlags::Warp);
		m_iCurrentSeatIndex = m_iSeat;
		SetTaskState(State_WaitForPedToLeaveSeat);
		return FSM_Continue;
	}

	bool bTechnical2InWater = MI_CAR_TECHNICAL2.IsValid() && m_pVehicle->GetModelIndex() == MI_CAR_TECHNICAL2 && m_pVehicle->GetIsInWater();
	bool bStrombergInWater = MI_CAR_STROMBERG.IsValid() && m_pVehicle->GetModelIndex() == MI_CAR_STROMBERG && m_pVehicle->GetIsInWater();
	bool bZhabaInWater = MI_CAR_ZHABA.IsValid() && m_pVehicle->GetModelIndex() == MI_CAR_ZHABA && m_pVehicle->GetIsInWater();
	bool bToreadorInWater = MI_CAR_TOREADOR.IsValid() && m_pVehicle->GetModelIndex() == MI_CAR_TOREADOR && m_pVehicle->GetIsInWater();
	bool bAvisaInWater = MI_SUB_AVISA.IsValid() && m_pVehicle->GetModelIndex() == MI_SUB_AVISA && m_pVehicle->GetIsInWater();
	if (CTaskVehicleFSM::CanPedWarpIntoVehicle(*pPed, *m_pVehicle) || (bTechnical2InWater && (m_iSeat == 0 || m_iSeat == 1)) || bStrombergInWater || (bZhabaInWater && (m_iSeat == 2 || m_iSeat == 3)) || bToreadorInWater || bAvisaInWater)
	{
		m_bWarping = true;
		m_iRunningFlags.BitSet().Set(CVehicleEnterExitFlags::Warp);
	}

	// If we're stood on top of a seat we can warp into, change the seat request so we warp to it
	bool bCanWarpOnToSeat = false;
	bool bShouldWarpOnToSeat = false;
	if (pPed->GetGroundPhysical() == m_pVehicle && pPed->IsLocalPlayer())
	{
		const s32 iClosestSeatIndex = FindClosestSeatToPedWithinWarpRange(*m_pVehicle, *pPed, bCanWarpOnToSeat);
		if (m_pVehicle->IsSeatIndexValid(iClosestSeatIndex))
		{
			// If the car is locked and we are standing on top of a warp seat, just abort the task
			if(!m_pVehicle->CanPedOpenLocks(pPed))
			{
				SetTaskFinishState(TFR_PED_CANT_OPEN_LOCKS, VEHICLE_DEBUG_DEFAULT_FLAGS);
				return FSM_Continue;
			}

			bShouldWarpOnToSeat = true;
			SetTargetSeat(iClosestSeatIndex, TSR_WARP_ONTO_SEAT, VEHICLE_DEBUG_DEFAULT_FLAGS);
			SetSeatRequestType(SR_Specific, SRR_WARPING_FROM_ON_SEAT, VEHICLE_DEBUG_DEFAULT_FLAGS);
			m_bWarping = true;
			SetRunningFlag(CVehicleEnterExitFlags::Warp);
		}
		else if (m_pVehicle->IsTurretSeat(m_iSeat))
		{
			bCanWarpOnToSeat = false;
		}
	}

	// Bail out if out seat is being blocked
	if (pPed->IsLocalPlayer() && IsOpenSeatIsBlocked(*m_pVehicle, m_iSeat))
	{
		// Inform the stuck check timer code that this bike is in a situation that it cannot be ridden.
		m_pVehicle->m_nVehicleFlags.bVehicleInaccesible = true;
		SetTaskFinishState(TFR_VEHICLE_SEAT_BLOCKED, VEHICLE_DEBUG_DEFAULT_FLAGS);
		return FSM_Continue;
	}
	else
	{
		m_pVehicle->m_nVehicleFlags.bVehicleInaccesible = false;
	}

	// Bail out if we're an ai ped trying to get into a passenger seat and the vehicle isn't upright
	CVehicle::eVehicleOrientation vehicleOrientation = CVehicle::GetVehicleOrientation(*m_pVehicle);
	if (!pPed->IsLocalPlayer() && m_pVehicle->GetDriverSeat() != m_iSeat && vehicleOrientation != CVehicle::VO_Upright)
	{
		ASSERT_ONLY(aiDisplayf("Frame %u, %s ped %s quitting enter vehicle task because vehicle isn't upright", fwTimer::GetFrameCount(), pPed->IsNetworkClone() ? "CLONE" : "LOCAL", pPed->GetDebugName());)
		SetTaskState(State_Finish);
		return FSM_Continue;
	}
	
	s32 iSelectedSeat = 0;
	const s32 iDesiredTargetEntryPoint = CModelSeatInfo::EvaluateSeatAccessFromAllEntryPoints(pPed, m_pVehicle, m_iSeatRequestType, m_iSeat, m_bWarping, m_iRunningFlags, &iSelectedSeat
#if __BANK
		, &CVehicleDebug::ms_EntryPointsDebug
#endif
	);
	SetTargetEntryPoint(iDesiredTargetEntryPoint, EPR_INITIAL_ENTRYPOINT_CHOICE, VEHICLE_DEBUG_DEFAULT_FLAGS);

	// Bail out if the vehicle is locked to the player and we don't need to open the door
	const bool bHasOpenDoor = CTaskEnterVehicle::CheckForOpenDoor();
	if (CTaskVehicleFSM::IsVehicleLockedForPlayer(*m_pVehicle, *pPed) && (!bHasOpenDoor || m_pVehicle->IsTurretSeat(iSelectedSeat)))
	{
		SetTaskFinishState(TFR_VEHICLE_LOCKED_FOR_PLAYER, VEHICLE_DEBUG_DEFAULT_FLAGS);
		return FSM_Continue;
	}

	// If there is a valid entry point, go to it
	if (m_iTargetEntryPoint != -1)
	{
		// We are requesting any seat, since we have a valid entry point, set the target seat as the direct access seat of this entry point
		const s32 iSeatEntering = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat, iSelectedSeat);
		if (iSeatEntering != iSelectedSeat)
		{
			s32 iShuffleSeat2 = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetShuffleSeatForSeat(m_iTargetEntryPoint, iSeatEntering, true);
			if (iShuffleSeat2 == iSelectedSeat)
			{
				SetRunningFlag(CVehicleEnterExitFlags::UseAlternateShuffleSeat);
			}
			else
			{
				ClearRunningFlag(CVehicleEnterExitFlags::UseAlternateShuffleSeat);
			}
		}
		
		const bool bIsWarpEntry = IsPedWarpingIntoVehicle(m_iTargetEntryPoint);
        CPed *pPedInTargetSeat = (m_iSeat != -1) ? CTaskVehicleFSM::GetPedInOrUsingSeat(*m_pVehicle, m_iSeat) : NULL;
		
		const bool bCanSetSeat = bIsWarpEntry || m_iSeatRequestType == SR_Any ||
								 (m_iSeatRequestType == SR_Prefer && pPedInTargetSeat && !CanJackPed(pPed, pPedInTargetSeat, m_iRunningFlags) );

		if (bCanSetSeat)
		{
			SetTargetSeat(iSeatEntering, TSR_DIRECT_SEAT_FOR_TARGET_ENTRY_POINT, VEHICLE_DEBUG_DEFAULT_FLAGS);
		}

		//! Set set correctly to chosen seat to avoid getting in driver seat. shouldn't we always do this?
		const CModelSeatInfo* pModelSeatinfo = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo();
		if(pModelSeatinfo)
		{
			if ((m_iSeat == pModelSeatinfo->GetDriverSeat()) && !m_pVehicle->IsAnExclusiveDriverPedOrOpenSeat(pPed))
			{
				SetTargetSeat(iSeatEntering, TSR_ENTERING_DRIVERS_SEAT, VEHICLE_DEBUG_DEFAULT_FLAGS);
			}
			else if (const CSyncedPedVehicleEntryScriptConfig* pVehicleEntryConfig = pPed->GetVehicleEntryConfig())
			{
				const CVehicleSeatInfo* pSeatInfo = pModelSeatinfo->GetLayoutInfo()->GetSeatInfo(m_iSeat);
				const bool bIsFrontSeat = m_pVehicle->IsSeatIndexValid(m_iSeat) && pSeatInfo && pSeatInfo->GetIsFrontSeat();
				const s32 iForcedSeatIndex = pVehicleEntryConfig->GetForcingSeatIndexForThisVehicle(m_pVehicle);

				if (bIsFrontSeat && pVehicleEntryConfig->HasARearForcingFlagSetForThisVehicle(m_pVehicle))
				{
					SetTargetSeat(iSeatEntering, TSR_FORCED_USE_REAR_SEATS, VEHICLE_DEBUG_DEFAULT_FLAGS);
				}
				else if (NetworkInterface::IsGameInProgress() && iForcedSeatIndex >= 0)
				{
					SetTargetSeat(iForcedSeatIndex, TSR_FORCED_USE_SEAT_INDEX, VEHICLE_DEBUG_DEFAULT_FLAGS);
				}
			}
		}

		if (m_pVehicle->IsSeatIndexValid(iSeatEntering) && m_pVehicle->GetDoorLockStateFromSeatIndex(iSeatEntering) == CARLOCK_LOCKED && (!bHasOpenDoor || bIsWarpEntry))
		{
			ASSERT_ONLY(aiDisplayf("Frame %u, %s ped %s quitting enter vehicle task because vehicle is locked", fwTimer::GetFrameCount(), pPed->IsNetworkClone() ? "CLONE" : "LOCAL", pPed->GetDebugName());)
			SetTaskState(State_Finish);
			return FSM_Continue;
		}

		if (m_pVehicle->IsTurretSeat(iSeatEntering) && m_pVehicle->GetSeatAnimationInfo(iSeatEntering)->IsStandingTurretSeat())
		{
			CreateWeaponManagerIfRequired();
		}

		if (bCanWarpOnToSeat)
		{
			if (!bShouldWarpOnToSeat)
			{
				ASSERT_ONLY(aiDisplayf("Frame %u, %s ped %s quitting enter vehicle task becauseped can't warp onto seat", fwTimer::GetFrameCount(), pPed->IsNetworkClone() ? "CLONE" : "LOCAL", pPed->GetDebugName());)
				SetTaskState(State_Finish);
				return FSM_Continue;
			}
		}

		const CVehicleEntryPointInfo* pEntryInfo = m_pVehicle->GetEntryInfo(m_iTargetEntryPoint);
		if(m_pVehicle->GetIsAquatic() && pEntryInfo->GetMPWarpInOut() && pEntryInfo->GetSPEntryAllowedAlso())
		{
			m_bWarping = true;
			SetRunningFlag(CVehicleEnterExitFlags::Warp);
		}

		const CVehicleEntryPointAnimInfo* pAnimInfo = m_pVehicle->GetLayoutInfo()->GetEntryPointAnimInfo(m_iTargetEntryPoint);
		const bool bWarpInAndOutInMP = ShouldWarpInAndOutInMP(*m_pVehicle, m_iTargetEntryPoint);
		const bool bNavigateToWarpEntryPoint = pAnimInfo->GetNavigateToWarpEntryPoint();
		if (bWarpInAndOutInMP && !bNavigateToWarpEntryPoint)
		{
			m_bWarping = true;
			SetRunningFlag(CVehicleEnterExitFlags::Warp);
			const CEntryExitPoint* pEntryPoint = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint);
			if (pEntryPoint)
			{
				m_iCurrentSeatIndex = pEntryPoint->GetSeat(SA_directAccessSeat, m_iSeat);
				SetTargetSeat(m_iCurrentSeatIndex, TSR_WARP_INTO_MP_WARP_SEAT_NO_NAV, VEHICLE_DEBUG_DEFAULT_FLAGS);
			}
		}

		// Warp the ped into the car straight away (This might get set below if certain conditions allow warping)
		if (IsFlagSet(CVehicleEnterExitFlags::Warp) || IsFlagSet(CVehicleEnterExitFlags::WarpOverPassengers))
		{
			// See if its a bike on its side that needs lifting up
			s32 iUnused = 0;
			if (m_pVehicle->InheritsFromBike() && (CheckForBikeOnGround(iUnused) || vehicleOrientation == CVehicle::VO_UpsideDown))
			{
				m_pVehicle->PlaceOnRoadAdjust();

				// Bail out if we couldn't right the bike, it's likely completely stuck
				if (CVehicle::GetVehicleOrientation(*m_pVehicle) != CVehicle::VO_Upright)
				{
					ASSERT_ONLY(aiDisplayf("Frame %u, %s ped %s quitting enter vehicle task because vehicle isn't upright 2", fwTimer::GetFrameCount(), pPed->IsNetworkClone() ? "CLONE" : "LOCAL", pPed->GetDebugName());)
					SetTaskState(State_Finish);
					return FSM_Continue;
				}
			}

			if (IsOpenSeatIsBlocked(*m_pVehicle, m_iSeat))
			{
				ASSERT_ONLY(aiDisplayf("Frame %u, %s ped %s quitting enter vehicle task because open seat is blocked 2", fwTimer::GetFrameCount(), pPed->IsNetworkClone() ? "CLONE" : "LOCAL", pPed->GetDebugName());)
				SetTaskState(State_Finish);
				return FSM_Continue;
			}

			m_iCurrentSeatIndex = m_iSeat;
			SetTaskState(State_WaitForPedToLeaveSeat);
		}
		else if (IsFlagSet(CVehicleEnterExitFlags::WarpToEntryPosition))
		{
			WarpPedToEntryPositionAndOrientate();

			const fwMvClipSetId entryClipsetId = CTaskEnterVehicleAlign::GetEntryClipsetId(m_pVehicle, m_iTargetEntryPoint, pPed);
			if (!CTaskVehicleFSM::IsVehicleClipSetLoaded(entryClipsetId))
			{
				return FSM_Continue;
			}

			if (CheckForOpenDoor())
			{
				SetTaskState(State_ReserveDoorToOpen);
				return FSM_Continue;
			}
			else
			{
				if (taskVerifyf(StartMoveNetwork(*pPed, *m_pVehicle, m_moveNetworkHelper, m_iTargetEntryPoint, INSTANT_BLEND_DURATION, &m_vLocalSpaceEntryModifier, false, m_OverridenEntryClipSetId), "Failed to start TASK_ENTER_VEHICLE move network"))
				{	
					CPed* pSeatOccupier = NULL;
					if (CheckForJackingPedFromOutside(pSeatOccupier))
					{
						SetTaskState(State_JackPedFromOutside);
						return FSM_Continue;
					}
					else if (!pSeatOccupier && CheckForEnteringSeat(pSeatOccupier))
					{
						SetTaskState(State_StreamAnimsToEnterSeat);
						return FSM_Continue;
					}
					else
					{
						ASSERT_ONLY(aiDisplayf("Frame %u, %s ped %s quitting enter vehicle task because couldn't enter or jack, pSeatOccupier = %s", fwTimer::GetFrameCount(), pPed->IsNetworkClone() ? "CLONE" : "LOCAL", pPed->GetDebugName(), pSeatOccupier ? pSeatOccupier->GetDebugName() : "NULL");)
						SetTaskState(State_Finish);
						return FSM_Continue;
					}
				}
			}
		}
		else
		{
			if (CTaskEnterVehicleAlign::ShouldComputeIdealTargetForTurret(*m_pVehicle, IsOnVehicleEntry(), m_iTargetEntryPoint))
			{
				StoreInitialReferencePosition(pPed);
			}
			const bool bPassedCheckForFriendlyJackCheck = !NetworkInterface::IsGameInProgress() || IsFlagSet(CVehicleEnterExitFlags::HasCheckedForFriendlyJack);
			const bool bShouldTriggerAlign = CTaskEnterVehicleAlign::ShouldTriggerAlign(*m_pVehicle, *pPed, m_iTargetEntryPoint, &m_vLocalSpaceEntryModifier, IsFlagSet(CVehicleEnterExitFlags::CombatEntry), IsFlagSet(CVehicleEnterExitFlags::FromCover), false, IsFlagSet(CVehicleEnterExitFlags::EnteringOnVehicle));
			// If the vehicle we're entering is a boat and we're stood on it, go to the seat rather than the door
			if (CheckForGoToSeat())
			{
				SetTaskState(State_GoToSeat);
				return FSM_Continue;
			}
			else if (CheckForBlockedOnVehicleGetIn())
			{
				ASSERT_ONLY(aiDisplayf("Frame %u, %s ped %s quitting enter vehicle task because blocked boat get in", fwTimer::GetFrameCount(), pPed->IsNetworkClone() ? "CLONE" : "LOCAL", pPed->GetDebugName());)
				SetTaskState(State_Finish);
				return FSM_Continue;
			}
			else if (CheckForGoToClimbUpPoint())
			{
				SetTaskState(State_GoToClimbUpPoint);
				return FSM_Continue;
			}
			else if (bPassedCheckForFriendlyJackCheck && !bNavigateToWarpEntryPoint && 
				!CTaskVehicleFSM::PedShouldWarpBecauseHangingPedInTheWay(*m_pVehicle, m_iTargetEntryPoint) && 
				!CTaskVehicleFSM::CanPedWarpIntoHoveringVehicle(pPed, *m_pVehicle, m_iTargetEntryPoint, true) &&
				bShouldTriggerAlign)
			{
				// Prevent getting onto a bike that's falling over
				if (CheckForBikeFallingOver(*m_pVehicle))
				{
					ASSERT_ONLY(aiDisplayf("Frame %u, %s ped %s quitting enter vehicle task because bike falling over", fwTimer::GetFrameCount(), pPed->IsNetworkClone() ? "CLONE" : "LOCAL", pPed->GetDebugName());)
					SetTaskState(State_Finish);
					return FSM_Continue;
				}

				if (ms_Tunables.m_EnableBikePickUpAlign && m_pVehicle->InheritsFromBike())
				{
					s32 iDesiredState = State_Align;
					if (CheckForBikeOnGround(iDesiredState))
					{
						if (CheckIfPlayerIsPickingUpMyBike())
						{
							ASSERT_ONLY(aiDisplayf("Frame %u, %s ped %s quitting enter vehicle task because player picking up my bike", fwTimer::GetFrameCount(), pPed->IsNetworkClone() ? "CLONE" : "LOCAL", pPed->GetDebugName());)
							SetTaskState(State_Finish);
							return FSM_Continue;
						}

						fwMvClipSetId commonClipsetId = CTaskEnterVehicleAlign::GetCommonClipsetId(m_pVehicle, m_iTargetEntryPoint);
						if (!CTaskVehicleFSM::IsVehicleClipSetLoaded(commonClipsetId))
						{
							return FSM_Continue;
						}

						if (StartMoveNetwork(*pPed, *m_pVehicle, m_moveNetworkHelper, m_iTargetEntryPoint, ms_Tunables.m_BikePickUpAlignBlendDuration, &m_vLocalSpaceEntryModifier, false, m_OverridenEntryClipSetId))
						{
							SetTaskState(iDesiredState);
							return FSM_Continue;
						}
					}
				}

				const bool bCombatEntry = IsFlagSet(CVehicleEnterExitFlags::CombatEntry);
				if (bCombatEntry && CheckForOpenDoor() && ShouldUseCombatEntryAnimLogic())
				{
					// I think this should go through State_ReserveDoorToOpen, but doing so will probably introduce a delay in entry because it doesn't go through alignment so there is no time to reserve anything
					// I guess in future we should do a distance based preemptive reservation if we want quick entry
					SetTaskState(State_StreamAnimsToOpenDoor); 
					return FSM_Continue;
				}

				// Wait and see if we want to jack the ped
				if (ProcessFriendlyJackDelay(pPed, m_pVehicle) || ProcessOptionalForcedTurretDelay(pPed, m_pVehicle) || ProcessOptionalForcedRearSeatDelay(pPed, m_pVehicle))
				{
					return FSM_Continue;
				}
				
				SetTaskState(State_Align);
				return FSM_Continue;
			}
			else
			{
				SetTaskState(State_GoToDoor);
				return FSM_Continue;
			}
		}
	}
	// No valid entry points, check for warping or wait
	else
	{
		// Wait and see if we want to jack the ped
		if (ProcessFriendlyJackDelay(pPed, m_pVehicle) || ProcessOptionalForcedTurretDelay(pPed, m_pVehicle) || ProcessOptionalForcedRearSeatDelay(pPed, m_pVehicle))
		{
			return FSM_Continue;
		}

		if (m_iSeat == -1)
		{
			ASSERT_ONLY(aiDisplayf("Frame %u, %s ped %s quitting enter vehicle task because invalid seat index", fwTimer::GetFrameCount(), pPed->IsNetworkClone() ? "CLONE" : "LOCAL", pPed->GetDebugName());)
			SetTaskState(State_Finish);
			return FSM_Continue;
		}

		// Don't allow warping or waiting into a vehicle seat if the door is locked
		if (m_pVehicle->IsSeatIndexValid(m_iSeat) && m_pVehicle->GetDoorLockStateFromSeatIndex(m_iSeat) == CARLOCK_LOCKED)
		{
			ASSERT_ONLY(aiDisplayf("Frame %u, %s ped %s quitting enter vehicle task because can't warp/wait for a locked seat", fwTimer::GetFrameCount(), pPed->IsNetworkClone() ? "CLONE" : "LOCAL", pPed->GetDebugName());)
			SetTaskState(State_Finish);
			return FSM_Continue;
		}

		// Wait a couple of seconds before warping the player in if they're forced to enter directly.
		TUNE_GROUP_BOOL(ENTER_VEHICLE_TUNE, WAIT_IF_FORCED_TO_USE_DIRECT_ENTRY_ONLY, true);
		if (WAIT_IF_FORCED_TO_USE_DIRECT_ENTRY_ONLY && pPed->IsLocalPlayer() && pPed->GetPedResetFlag(CPED_RESET_FLAG_ForcePlayerToEnterVehicleThroughDirectDoorOnly))
		{
			TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, WAIT_TIME_IF_FORCED_TO_USE_DIRECT_ENTRY_ONLY, 2.0f, 0.0f, 5.0f, 0.01f);
			if (GetTimeInState() < WAIT_TIME_IF_FORCED_TO_USE_DIRECT_ENTRY_ONLY)
			{
				return FSM_Continue;
			}
		}

		// Player can warp into the car if its a mission car, or if there is a friendly mission ped that can't be jacked in one of the passenger seats.
		bool bPlayerCanWarpIntoCar = CPlayerInfo::CanPlayerWarpIntoVehicle(*pPed, *m_pVehicle, m_iSeat);

		// Temp: Warp peds into the tourbus for now
		s32 iEntryPointIndex = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(m_iSeat, m_pVehicle);
		static s32 siHackIndex = 2;	// If the third entry point is a multiple seat access point, warp if getting into the rear
		if (iEntryPointIndex != -1 && m_pVehicle->GetNumberEntryExitPoints() > siHackIndex && m_pVehicle->GetEntryExitPoint(siHackIndex))
		{
			if (m_pVehicle->GetEntryExitPoint(siHackIndex)->GetSeatAccessor().GetSeatAccessType() == CSeatAccessor::eSeatAccessTypeMultiple && !bPlayerCanWarpIntoCar)
			{
				if ((!m_pVehicle->GetSeatManager()->GetPedInSeat(m_iSeat) || m_pVehicle->GetSeatManager()->GetPedInSeat(m_iSeat) == pPed) && m_iSeat >= 2)
				{
					m_iCurrentSeatIndex = m_iSeat;
					SetTaskState(State_WaitForPedToLeaveSeat);	
				}
				else if (IsFlagSet(CVehicleEnterExitFlags::ResumeIfInterupted))
				{
					SetTaskState(State_WaitForValidEntryPoint);
				}
				else
				{
					ASSERT_ONLY(aiDisplayf("Frame %u, %s ped %s quitting enter vehicle task because tourbus hack failed", fwTimer::GetFrameCount(), pPed->IsNetworkClone() ? "CLONE" : "LOCAL", pPed->GetDebugName());)
					SetTaskState(State_Finish);
				}
				return FSM_Continue;
			}
		}

		// Try again next frame and the one after that, so any objects flagged as being deleted are gone
		if (!IsFlagSet(CVehicleEnterExitFlags::WarpIfDoorBlocked) && bPlayerCanWarpIntoCar)
		{
			if (IsFlagSet(CVehicleEnterExitFlags::WarpIfDoorBlockedNext))
			{
				SetRunningFlag(CVehicleEnterExitFlags::WarpIfDoorBlocked);
			}
			else
			{
				SetRunningFlag(CVehicleEnterExitFlags::WarpIfDoorBlockedNext);
			}
			return FSM_Continue;
		}

		// No valid entry point found, try again allowing the player to warp over passengers
		bool bInNetworkGame = NetworkInterface::IsGameInProgress();

		if (!bInNetworkGame && IsFlagSet(CVehicleEnterExitFlags::CanWarpOverPassengers) && !IsFlagSet(CVehicleEnterExitFlags::WarpOverPassengers))
		{
			m_bWarping = true;
			m_iRunningFlags.BitSet().Set(CVehicleEnterExitFlags::WarpOverPassengers);

			// Recursive call, try again with the extra flag set
			return PickDoor_OnUpdate();
		}

		// If this ped is set to warp after a time limit, and the doors are blocked, warp them immediately if doors are blocked
		// and the timer is up
		if (IsFlagSet(CVehicleEnterExitFlags::WarpAfterTime) && GetTimerRanOut())
		{
			m_iRunningFlags.BitSet().Set(CVehicleEnterExitFlags::WarpIfDoorBlocked);
		}

		// Warp the ped in if the door is blocked
		if (!m_bWarping && IsFlagSet(CVehicleEnterExitFlags::WarpIfDoorBlocked))
		{
			m_bWarping = true;
			m_iRunningFlags.BitSet().Set(CVehicleEnterExitFlags::Warp);

			// Recursive call, try again with the extra flag set
			return PickDoor_OnUpdate();
		}
		else if (IsFlagSet(CVehicleEnterExitFlags::ResumeIfInterupted) || IsFlagSet(CVehicleEnterExitFlags::WarpAfterTime))
		{
			if (m_iSeatRequestType == SR_Specific && m_pVehicle->IsSeatIndexValid(m_iSeat))
			{
				CPed* pPedOccupyingSeat = m_pVehicle->GetSeatManager()->GetPedInSeat(m_iSeat);
				if (pPedOccupyingSeat && pPed->GetPedIntelligence()->IsFriendlyWith(*pPedOccupyingSeat))
				{
					ASSERT_ONLY(aiDisplayf("Frame %u, %s ped %s quitting enter vehicle task because friend ped %s is occupying seat %i", fwTimer::GetFrameCount(), pPed->IsNetworkClone() ? "CLONE" : "LOCAL", pPed->GetDebugName(), pPedOccupyingSeat->GetDebugName(), m_iSeat);)
					SetTaskState(State_Finish);
					return FSM_Continue;
				}
			}
			SetTaskState(State_WaitForValidEntryPoint);
			return FSM_Continue;
		}
		else
		{
			// We're not allowed to warp so quit
			SetTaskFinishState(TFR_NOT_ALLOWED_TO_WARP, VEHICLE_DEBUG_DEFAULT_FLAGS);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::GoToDoor_OnEnter()
{
	// Going to jack someone, get there quick
	CPed* pPed = GetPed();

	CPed* pSeatOccupier = GetPedOccupyingDirectAccessSeat();
	if (pSeatOccupier && !pPed->GetPedIntelligence()->IsFriendlyWith(*pSeatOccupier))
	{
		SetRunningFlag(CVehicleEnterExitFlags::UseFastClipRate);
		if (ms_Tunables.m_UseCombatEntryForAiJack && !pPed->IsLocalPlayer())
		{
			SetRunningFlag(CVehicleEnterExitFlags::CombatEntry);
		}
	}

	m_fDoorCheckInterval = fwRandom::GetRandomNumberInRange(0.8f, 1.2f)*ms_Tunables.m_TimeBetweenDoorChecks;

	float fTargetRadius = CTaskGoToCarDoorAndStandStill::ms_fTargetRadius;
	float fSlowDownDistance = CTaskGoToCarDoorAndStandStill::ms_fSlowDownDistance;

	if (IsFlagSet(CVehicleEnterExitFlags::CombatEntry) || IsFlagSet(CVehicleEnterExitFlags::UseFastClipRate))
	{
		TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, COMBAT_TARGET_RADIUS, 0.15f, 0.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, COMBAT_SLOW_DOWN_DISTANCE, 0.0f, 0.0f, 8.0f, 0.1f);
		fTargetRadius = COMBAT_TARGET_RADIUS;
		fSlowDownDistance = COMBAT_SLOW_DOWN_DISTANCE;
	}
	else if(pPed->GetIsSwimming() && m_pVehicle->HasWaterEntry())
	{
		TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, SWIM_TO_BOAT_TARGET_RADIUS, 0.25f, 0.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, SWIM_TO_BOAT_SLOW_DOWN_DISTANCE, 2.0f, 0.0f, 8.0f, 0.1f);
		fTargetRadius = SWIM_TO_BOAT_TARGET_RADIUS;
		fSlowDownDistance = SWIM_TO_BOAT_SLOW_DOWN_DISTANCE;
	}
	else if (CTaskEnterVehicleAlign::ms_Tunables.m_ForceStandEnterOnly)
	{
		TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, TARGET_RADIUS, 0.2f, 0.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, SLOW_DOWN_DISTANCE, 3.5f, 0.0f, 8.0f, 0.1f);
		fTargetRadius = TARGET_RADIUS;
		fSlowDownDistance = SLOW_DOWN_DISTANCE;
	}

	Assert(!pPed->GetIsInVehicle());

	if (pPed->GetIsAttached())
	{
		pPed->DetachFromParent(0);
		pPed->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, NORMAL_BLEND_DURATION);
	}

	if (IsFlagSet(CVehicleEnterExitFlags::CombatEntry) || IsFlagSet(CVehicleEnterExitFlags::UseFastClipRate))
	{
		SetPedMoveBlendRatio(MOVEBLENDRATIO_RUN);
	}

	float fMoveBlendRatio = GetPedMoveBlendRatio();

	if (m_pVehicle->GetLayoutInfo() && taskVerifyf(m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint), "Entry point index %i is invalid", m_iTargetEntryPoint))
	{
		if (!pPed->IsAPlayerPed() && pPed->GetPedsGroup())
		{
			fSlowDownDistance = ms_Tunables.m_GroupMemberSlowDownDistance;

			const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			Vector3 vEntryPos(Vector3::ZeroType);
			Quaternion qEntryOrientation(0.0f,0.0f,0.0f,1.0f);
			CModelSeatInfo::CalculateEntrySituation(m_pVehicle.Get(), pPed, vEntryPos, qEntryOrientation, m_iTargetEntryPoint, 0, 0.0f, &m_vLocalSpaceEntryModifier);
			if (vEntryPos.Dist2(vPedPos) < rage::square(ms_Tunables.m_GroupMemberWalkCloseDistance))
			{
				fMoveBlendRatio = MOVEBLENDRATIO_WALK;
			}
		}
	}

	m_uInvalidEntryPointTime = 0;
	CTaskGoToCarDoorAndStandStill* pGoToTask = rage_new CTaskGoToCarDoorAndStandStill( m_pVehicle, fMoveBlendRatio, false, m_iTargetEntryPoint,
																					   fTargetRadius, fSlowDownDistance, CTaskGoToCarDoorAndStandStill::ms_fMaxSeekDistance,
																					   CTaskGoToCarDoorAndStandStill::ms_iMaxSeekTime, true, m_pTargetPed );
	SetNewTask(pGoToTask);
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::GoToDoor_OnUpdate()
{
	CPed* pPed = GetPed();

	if(!pPed->GetIkManager().IsLooking() || !m_bEnterVehicleLookAtStarted)
	{
		m_bEnterVehicleLookAtStarted = StartEnterVehicleLookAt(pPed, m_pVehicle);
	}

	//! If we somehow get on boat whilst trying to get to a door, see if we can enter on the vehicle instead.
	if(m_pVehicle->InheritsFromBoat() && CheckForGoToClimbUpPoint())
	{
		SetTaskState(State_GoToClimbUpPoint);
		return FSM_Continue;
	}

	TUNE_GROUP_FLOAT(COVER_TUNE, SUPPRESS_CORNER_SLOW_DOWN_TIME, 1.5f, 0.0f, 10.0f, 0.1f);
	if (IsFlagSet(CVehicleEnterExitFlags::CombatEntry) && GetTimeInState() < SUPPRESS_CORNER_SLOW_DOWN_TIME)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_SuppressSlowingForCorners, true);
	}

	if(IsFlagSet(CVehicleEnterExitFlags::Warp))
	{
		m_bWarping = true;

		SetTaskState(State_PickDoor);
		return FSM_Continue;
	}
	// If the warp timer had been set and has ran out, warp the ped now
	else if (GetTimerRanOut())
	{
		SetRunningFlag(CVehicleEnterExitFlags::Warp);
		m_bWarping = true;

		SetTaskState(State_PickDoor);
		return FSM_Continue;
	}

	bool bProcessFriendlyJackDelay = true;
	if (m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint))
	{
		const CVehicleEntryPointInfo* pEntryInfo = m_pVehicle->GetEntryInfo(m_iTargetEntryPoint);
		bProcessFriendlyJackDelay = !pEntryInfo->GetMPWarpInOut() || pPed->IsBossPed();
	}

	if(bProcessFriendlyJackDelay && (ProcessFriendlyJackDelay(pPed, m_pVehicle) || ProcessOptionalForcedTurretDelay(pPed, m_pVehicle) || ProcessOptionalForcedRearSeatDelay(pPed, m_pVehicle)))
	{
		if (!m_bFriendlyJackDelayEndedLastFrame)
			m_bFriendlyJackDelayEndedLastFrame = true; 
		return FSM_Continue;
	}

	if (GetSubTask() && GetSubTask()->GetTaskType()== CTaskTypes::TASK_GO_TO_CAR_DOOR_AND_STAND_STILL)
	{
		const bool bShouldWaitForReservations = PedShouldWaitForReservationToEnter();

		if (bShouldWaitForReservations)
		{
			// Calculate the world space position and orientation we would like to lerp to during the align
			Vector3 vEntryPos(Vector3::ZeroType);
			Quaternion qEntryOrientation(0.0f,0.0f,0.0f,1.0f);

			CModelSeatInfo::CalculateEntrySituation(m_pVehicle, pPed, vEntryPos, qEntryOrientation, m_iTargetEntryPoint);
			vEntryPos.z = 0.0f;
			Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			vPedPos.z = 0.0f;
			const float fDistToEntryPointSqd = vEntryPos.Dist2(vPedPos);

			TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, WAIT_FOR_RESERVATION_DISTANCE, 5.0f, 0.0f, 10.0f, 0.1f);
			if (fDistToEntryPointSqd < square(WAIT_FOR_RESERVATION_DISTANCE) && !PedHasReservations())
			{
				SetTaskState(State_WaitForEntryPointReservation);
				return FSM_Continue;
			}
		}

		if (CTaskEnterVehicleAlign::ShouldTriggerAlign(*m_pVehicle, *pPed, m_iTargetEntryPoint, &m_vLocalSpaceEntryModifier, IsFlagSet(CVehicleEnterExitFlags::CombatEntry), IsFlagSet(CVehicleEnterExitFlags::FromCover), true, IsFlagSet(CVehicleEnterExitFlags::EnteringOnVehicle)))
		{
			TUNE_GROUP_BOOL(ENTER_VEHICLE_TUNE, DO_ENTRY_EVALUATION_BEFORE_ALIGN, true);
			if (DO_ENTRY_EVALUATION_BEFORE_ALIGN && GetTimeInState() > 0.0f && !CModelSeatInfo::EvaluateSeatAccessFromEntryPoint(pPed, m_pVehicle, m_iSeatRequestType, m_iSeat, m_iTargetEntryPoint, m_bWarping))
			{
				SetTaskState(State_PickDoor);
				return FSM_Continue;
			}
#if __DEV
			// Calculate the world space position and orientation we would like to lerp to during the align
			Vector3 vEntryPos(Vector3::ZeroType);
			Quaternion qEntryOrientation(0.0f,0.0f,0.0f,1.0f);
			CModelSeatInfo::CalculateEntrySituation(m_pVehicle, pPed, vEntryPos, qEntryOrientation, m_iTargetEntryPoint);
			vEntryPos.z = 0.0f;
			Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			vPedPos.z = 0.0f;
			const float fDistToEntryPoint = vEntryPos.Dist2(vPedPos);
			aiDisplayf("Frame : %i, Ped %s, triggering align %.2f m away from entry point, (Combat entry ? %s)", fwTimer::GetFrameCount(), pPed->GetModelName(), sqrtf(fDistToEntryPoint), IsFlagSet(CVehicleEnterExitFlags::CombatEntry) ? "TRUE":"FALSE");
#endif // __DEV

			if (PedShouldWaitForLeaderToEnter())
			{
				SetTaskState(State_WaitForGroupLeaderToEnter);
				return FSM_Continue;
			}
			else
			{
				const bool bTestPedHeightToSeat = true;
				const CVehicleEntryPointAnimInfo* pEntryAnimInfo = m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint);
				if ((ShouldWarpInAndOutInMP(*m_pVehicle, m_iTargetEntryPoint) && pEntryAnimInfo->GetNavigateToWarpEntryPoint()) || CTaskVehicleFSM::CanPedWarpIntoHoveringVehicle(pPed, *m_pVehicle, m_iTargetEntryPoint, bTestPedHeightToSeat) || CTaskVehicleFSM::PedShouldWarpBecauseHangingPedInTheWay(*m_pVehicle, m_iTargetEntryPoint))
				{
					if (CTaskVehicleFSM::IsVehicleLockedForPlayer(*m_pVehicle, *pPed))
					{
						SetTaskFinishState(TFR_VEHICLE_LOCKED_FOR_PLAYER, VEHICLE_DEBUG_DEFAULT_FLAGS);
						return FSM_Continue;
					}
					else if (!CanPedGetInVehicle(pPed))
					{
						const bool bFollowingLeader = pPed->GetPedIntelligence()->GetCurrentEventType() == EVENT_LEADER_ENTERED_CAR_AS_DRIVER;
						if (bFollowingLeader)
						{
							SetTaskState(State_WaitForValidEntryPoint);
							return FSM_Continue;
						}
						else
						{
							SetTaskFinishState(TFR_PED_CANT_OPEN_LOCKS, VEHICLE_DEBUG_DEFAULT_FLAGS);
							return FSM_Continue;
						}
					}

					SetRunningFlag(CVehicleEnterExitFlags::Warp);
					const CEntryExitPoint* pEntryPoint = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint);
					if (pEntryPoint)
					{
						m_iCurrentSeatIndex = pEntryPoint->GetSeat(SA_directAccessSeat, m_iSeat);
						SetTargetSeat(m_iCurrentSeatIndex, TSR_WARP_INTO_MP_WARP_SEAT, VEHICLE_DEBUG_DEFAULT_FLAGS);
						SetTaskState(State_WaitForPedToLeaveSeat);
						return FSM_Continue;
					}
				}
				else
				{
					if (bShouldWaitForReservations && !PedHasReservations())
					{
						SetTaskState(State_WaitForEntryPointReservation);
						return FSM_Continue;
					}

					// This chunk should prevent peds trying to align when another ped is entering or sitting in that seat
					// in this kind of vehicle that got multiple entrypoints for a single seat as the entrypoints
					// might still be available but the seat is not.
					// To fix B* 1493452, and B*1510082
					const CEntryExitPoint* pEntryPoint = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint);
					if (pEntryPoint)
					{
						const s32 iSeatIndex = pEntryPoint->GetSeat(SA_directAccessSeat, m_iSeat);
						if (m_pVehicle->IsSeatIndexValid(iSeatIndex))
						{
							CPed* pPedUsingSeat = CTaskVehicleFSM::GetPedInOrUsingSeat(*m_pVehicle, iSeatIndex);	// So someone is using or about to get into the seat

							// If we're using the entry point for climb up only, we don't care about the seat occupier
							const CVehicleEntryPointInfo* pEntryInfo = m_pVehicle->GetEntryInfo(m_iTargetEntryPoint);
							if (pEntryInfo && pEntryInfo->GetClimbUpOnly() && pPed->GetGroundPhysical() != m_pVehicle)
							{
								pPedUsingSeat = NULL;
							}

							if (pPedUsingSeat && 
								!CTaskVehicleFSM::CanJackPed(pPed, pPedUsingSeat, m_iRunningFlags) &&				// 
								(!m_pVehicle->GetPedInSeat(m_iSeat) || !IsSeatOccupierShuffling(pPedUsingSeat)))	// If ped is not fully in seat or shuffling away from this seat
							{
								CTaskEnterVehicle* pTaskEnterVehicle = static_cast<CTaskEnterVehicle*>(pPedUsingSeat->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
								if (!pTaskEnterVehicle || pTaskEnterVehicle->GetTargetSeat() == iSeatIndex)
								{
									Displayf("Ped (%s) 0x%p quitting out of enter vehicle task because we think ped (%s) 0x%p is occupying seat %i", pPed->GetModelName(), pPed, pPedUsingSeat->GetModelName(), pPedUsingSeat, iSeatIndex);
									return FSM_Quit;
								}
							}
						}
					}

					bool bProbeClear = true;
					if (NetworkInterface::IsGameInProgress() && !pPed->IsAPlayerPed())
					{
						const Vector3 vStart = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
						Vector3 vEnd(Vector3::ZeroType);
						Quaternion qEntryOrientation(0.0f,0.0f,0.0f,1.0f);
						CModelSeatInfo::CalculateEntrySituation(m_pVehicle, pPed, vEnd, qEntryOrientation, m_iTargetEntryPoint);
						WorldProbe::CShapeTestFixedResults<> probeResult;
						s32 iTypeFlags = ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE;
						WorldProbe::CShapeTestProbeDesc probeDesc;
						probeDesc.SetResultsStructure(&probeResult);
						probeDesc.SetStartAndEnd(vStart, vEnd);
						probeDesc.SetIncludeFlags(iTypeFlags);
						probeDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
						probeDesc.SetIsDirected(true);
						WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
						bProbeClear = !probeResult[0].GetHitDetected();
					}

					if (bProbeClear)
					{
						SetTaskState(State_StreamAnimsToAlign);
						return FSM_Continue;
					}
				}
			}
		}
		else
		{
			if (!pPed->IsAPlayerPed() && pPed->GetPedsGroup())
			{
				const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
				Vector3 vEntryPos(Vector3::ZeroType);
				Quaternion qEntryOrientation(0.0f,0.0f,0.0f,1.0f);
				CModelSeatInfo::CalculateEntrySituation(m_pVehicle.Get(), pPed, vEntryPos, qEntryOrientation, m_iTargetEntryPoint, 0, 0.0f, &m_vLocalSpaceEntryModifier);
				if (vEntryPos.Dist2(vPedPos) < rage::square(ms_Tunables.m_GroupMemberWaitDistance))
				{
					if (PedShouldWaitForLeaderToEnter())
					{
						SetTaskState(State_WaitForGroupLeaderToEnter);
						return FSM_Continue;
					}
				}
			}

			ClearRunningFlag(CVehicleEnterExitFlags::FromCover);
		}
	}
	else if (pPed->IsLocalPlayer())
	{
		if (m_bFriendlyJackDelayEndedLastFrame)
		{
			AI_LOG_WITH_ARGS("[VehicleEntryExit] Couldn't find subtask for %s after waking up from JackingDelay\n",AILogging::GetDynamicEntityNameSafe(pPed));

			m_bFriendlyJackDelayEndedLastFrame = false;

			SetTaskState(State_PickDoor);
			return FSM_Continue;
		}
	}

	m_bFriendlyJackDelayEndedLastFrame = false;

	// Consider the player specific driving requirements if this is a player, or if this ped is in a group with the player.
	bool bUsePlayerConditions = pPed->IsAPlayerPed();
	if( pPed->GetPedsGroup() && pPed->GetPedsGroup()->IsPlayerGroup() )
	{
		bUsePlayerConditions = true;
	}
	
	// We need to add an additional flag for seat entry point evaluation
	VehicleEnterExitFlags iFlagsForSeatEntryEvaluate = m_iRunningFlags;
	iFlagsForSeatEntryEvaluate.BitSet().Set(CVehicleEnterExitFlags::IgnoreEntryPointCollisionCheck);

	if (!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if (GetSubTask())
		{
			if (taskVerifyf(GetSubTask()->GetTaskType()== CTaskTypes::TASK_GO_TO_CAR_DOOR_AND_STAND_STILL, "Subtask wasn't of type TASK_GO_TO_CAR_DOOR_AND_STAND_STILL in CTaskEnterVehicle::GoToDoor_OnUpdate"))
			{
				CTaskGoToCarDoorAndStandStill* pGoToDoorTask = static_cast<CTaskGoToCarDoorAndStandStill*>(GetSubTask());

				if(pGoToDoorTask->HasFailed())
				{
					SetTaskFinishState(TFR_GOTO_DOOR_TASK_FAILED, VEHICLE_DEBUG_DEFAULT_FLAGS);
					return FSM_Continue;
				}
				else if(!pGoToDoorTask->HasAchievedDoor())
				{
					if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_WillArrestRatherThanJack))
					{
						return FSM_Quit;
					}
					else
					{
						if (pPed->IsLocalPlayer() && m_pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR && pPed->GetGroundPhysical() == m_pVehicle)
						{
							TUNE_GROUP_FLOAT(VEHICLE_ENTRY_TUNE, REQUIRED_ROLL_TO_ABORT, 0.2f, 0.0f, 1.0f, 0.01f);
							const float fRoll = Abs(m_pVehicle->GetTransform().GetRoll());
							if (fRoll > REQUIRED_ROLL_TO_ABORT)
							{
								SetTaskState(State_Finish);
								return FSM_Continue;
							}
						}
						SetTaskState(State_PickDoor);
						return FSM_Continue;
					}
				}
				// Obtain the local space offset which was applied to the entry point, to avoid it sticking through any rear door
				m_vLocalSpaceEntryModifier = pGoToDoorTask->GetLocalSpaceEntryPointModifier();
			}
		}
		// Check if we are close enough to do the stand align so we didn't bother doing the gotocar door task
		else if (!CTaskEnterVehicleAlign::ShouldDoStandAlign(*m_pVehicle, *pPed, m_iTargetEntryPoint, &m_vLocalSpaceEntryModifier))
		{
			SetTaskState(State_PickDoor);
			return FSM_Continue;
		}

#if __ASSERT
		Vector3 vEntryPos(Vector3::ZeroType);
		Quaternion qEntryOrientation(0.0f,0.0f,0.0f,1.0f);
		CModelSeatInfo::CalculateEntrySituation(m_pVehicle, pPed, vEntryPos, qEntryOrientation, m_iTargetEntryPoint, 0, 0.0f, &m_vLocalSpaceEntryModifier);
		const float fDistToVehicle = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()).Dist(vEntryPos);
		static float sMaxEnterDistance = 5.0f;

		Vector3 vRelativeVelocity = m_pVehicle->GetRelativeVelocity(*pPed);
		taskAssertf(fDistToVehicle < sMaxEnterDistance, "Ped started to enter seat when greater than %.2f (actual %.2f) meters away from the entrypoint of vehicle %s relative vehicle speed %.2f", sMaxEnterDistance, fDistToVehicle, m_pVehicle->GetDebugName(), vRelativeVelocity.Mag2() > SMALL_FLOAT ? vRelativeVelocity.Mag() : 0.0f);
#endif

		if(StartMoveNetworkForArrest() == FSM_Quit)
		{
			return FSM_Quit;
		}
		else if(GetState() != State_WaitForStreamedEntry)
		{
			AI_LOG_WITH_ARGS("[CTaskEnterVehicle::GoToDoor_OnUpdate] %s is going into State_StreamAnimsToAlign, possibly because TaskEnterVehicle has no subtask\n",AILogging::GetDynamicEntityNameSafe(pPed));
			SetTaskState(State_StreamAnimsToAlign);
		}
	}
	// If the point is no longer valid - restart
	else if (!CModelSeatInfo::EvaluateSeatAccessFromEntryPoint(pPed, m_pVehicle, m_iSeatRequestType, m_iSeat, m_iTargetEntryPoint, m_bWarping, iFlagsForSeatEntryEvaluate))
	{
		if (m_uInvalidEntryPointTime == 0)
		{
			m_uInvalidEntryPointTime = fwTimer::GetTimeInMilliseconds();
		}

		TUNE_GROUP_INT(ENTER_VEHICLE_TUNE, MIN_TIME_INVALID_ENTRY_TO_PICK_DOOR, 2000, 0, 5000, 100);
		if ((fwTimer::GetTimeInMilliseconds() - m_uInvalidEntryPointTime) > MIN_TIME_INVALID_ENTRY_TO_PICK_DOOR)
		{
			SetTaskState(State_PickDoor);
		}
		return FSM_Continue;
	}
	// Check to see if the player wants to quit entering
	else if (CheckPlayerExitConditions(*pPed, *m_pVehicle, m_iRunningFlags, GetTimeRunning()))
	{
		SetTaskFinishState(TFR_PED_INTERRUPTED, VEHICLE_DEBUG_DEFAULT_FLAGS);
	}
	// Quit if entering a floating vehicle
	else if(m_pVehicle->m_nVehicleFlags.bIsDrowning
		|| (m_pVehicle->InheritsFromAutomobile() && m_pVehicle->HasContactWheels()==false && m_pVehicle->GetIsInWater() && !m_pVehicle->HasWaterEntry())
		|| (m_pVehicle->InheritsFromBike() && m_pVehicle->GetTransform().GetC().GetZf() > 0.707f && !m_pVehicle->HasContactWheels() && m_pVehicle->GetIsInWater()))
	{
		SetTaskFinishState(TFR_VEHICLE_FLOATING, VEHICLE_DEBUG_DEFAULT_FLAGS);
	}
	// Quit if the vehicle is on fire or blowing up or whatever
	else if (!m_pVehicle->GetVehicleDamage()->GetIsDriveable(bUsePlayerConditions, true, true))
	{
		SetTaskFinishState(TFR_VEHICLE_UNDRIVEABLE, VEHICLE_DEBUG_DEFAULT_FLAGS);
	}	
	// Quit if the car is on its side, or upside down
	else if (m_pVehicle->GetTransform().GetC().GetZf() < 0.3f && !m_pVehicle->InheritsFromBike())
	{
		SetTaskFinishState(TFR_VEHICLE_NOT_UPRIGHT, VEHICLE_DEBUG_DEFAULT_FLAGS);
	}
	else
	{
		// If target is small, its easy to push around to the wrong side of it.
		// Ensure that we update frequently if close to a bike.
		const bool bIsSmallDoubleSidedAccessVehicle = m_pVehicle->GetIsSmallDoubleSidedAccessVehicle();
		const bool bIsUpright = (m_pVehicle->GetTransform().GetUp().GetZf() > 0.8f);
		const bool bCloseToSmallDoubleSidedAccessVehicle = (bIsSmallDoubleSidedAccessVehicle && bIsUpright && (VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().GetPosition()) - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition())).XYMag2() < 3.0f*3.0f);

		// If we find ourselves on the opposite side of the vehicle, force an immediate re-evaluation of the entry point
		bool bForceReevaluation = false;
		if (bIsSmallDoubleSidedAccessVehicle && m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint))
		{
			const bool bPedOnLeftSideOfVehicle = CTaskVehicleFSM::IsPedHeadingToLeftSideOfVehicle(*pPed, *m_pVehicle);
			const bool bIsLeftSidedEntry = m_pVehicle->GetEntryInfo(m_iTargetEntryPoint)->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT;
			if ((bPedOnLeftSideOfVehicle && !bIsLeftSidedEntry) || (!bPedOnLeftSideOfVehicle && bIsLeftSidedEntry))
			{
				// But rule out the case where we may be walking into the front/rear of the bike, about to push past.
				CEntityBoundAI boundAI(*m_pVehicle.Get(), pPed->GetTransform().GetPosition().GetZf(), pPed->GetCapsuleInfo()->GetHalfWidth());
				s32 iHitSide = boundAI.ComputeHitSideByPosition(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));
				if((iHitSide != CEntityBoundAI::FRONT && iHitSide != CEntityBoundAI::REAR) || pPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter() > 15)
				{
					bForceReevaluation = true;
					m_fDoorCheckInterval = -1.0f;
				}
			}
			else if (NetworkInterface::IsGameInProgress())
			{
				const s32 iDriversSeat = m_pVehicle->GetDriverSeat();
				// See if we can't access the drivers seat directly from this entry point
				const CEntryExitPoint* pEntryPoint = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint);
				if (pEntryPoint && (pEntryPoint->CanSeatBeReached(iDriversSeat) != SA_directAccessSeat))
				{
					const CPed* pPedOccupyingDriversSeat = m_pVehicle->GetSeatManager()->GetPedInSeat(iDriversSeat);
					if (!pPedOccupyingDriversSeat || pPedOccupyingDriversSeat->GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle))
					{
						bool bShouldReevaluate = true;
						CComponentReservation* pComponentReservation = m_pVehicle->GetComponentReservationMgr()->GetSeatReservation(m_iTargetEntryPoint, SA_directAccessSeat, m_iSeat);
						if (pComponentReservation)
						{
							CPed* pPedReservingSeat = pComponentReservation->GetPedUsingComponent();
							if (pPedReservingSeat && pPedReservingSeat->GetPedResetFlag(CPED_RESET_FLAG_IsEnteringVehicle))
							{
								bShouldReevaluate = false;
							}
						}

						if (bShouldReevaluate)
						{
							SetSeatRequestType(SR_Prefer, SRR_FORCE_REEVALUATE_BECAUSE_CANT_DIRECTLY_ACCESS_DRIVER, VEHICLE_DEBUG_DEFAULT_FLAGS | VEHICLE_DEBUG_PRINT_PED_IN_DRIVER_SEAT | VEHICLE_DEBUG_PRINT_SEAT_RESERVATION);
							SetTargetSeat(iDriversSeat, TSR_GO_TO_DOOR_REEVALUATION, VEHICLE_DEBUG_DEFAULT_FLAGS);
							bForceReevaluation = true;
							m_fDoorCheckInterval = -1.0f;
						}
					}
				}
			}
		}

		// Re-evaluate target door
		static dev_float MIN_TIME_MOVING_TO_DOOR_BEFORE_NEW_EVALUATION = 1.f;
		if(bForceReevaluation || GetTimeInState() > MIN_TIME_MOVING_TO_DOOR_BEFORE_NEW_EVALUATION)
		{
			m_fDoorCheckInterval -= GetTimeStep();
			if (m_fDoorCheckInterval <= 0.0f)
			{
				m_fDoorCheckInterval = fwRandom::GetRandomNumberInRange(0.8f, 1.2f)*ms_Tunables.m_TimeBetweenDoorChecks;
				s32 iSelectedSeat = 0;
				s32 iNewTargetEntryPoint = CModelSeatInfo::EvaluateSeatAccessFromAllEntryPoints(pPed, m_pVehicle, m_iSeatRequestType, m_iSeat, m_bWarping, m_iRunningFlags, &iSelectedSeat
#if __BANK
					, &CVehicleDebug::ms_EntryPointsDebug
#endif
					, (bIsSmallDoubleSidedAccessVehicle && bIsUpright && bCloseToSmallDoubleSidedAccessVehicle) ? -1 : m_iTargetEntryPoint );

				if (m_iTargetEntryPoint != iNewTargetEntryPoint && GetSubTask()->MakeAbortable(CTask::ABORT_PRIORITY_URGENT,0))
				{
					if (m_pVehicle->IsEntryIndexValid(iNewTargetEntryPoint))
					{
						SetTargetEntryPoint(iNewTargetEntryPoint, EPR_REEVALUATED_WHEN_GOING_TO_DOOR, VEHICLE_DEBUG_DEFAULT_FLAGS);
						SetFlag(aiTaskFlags::RestartCurrentState);
					}
					else
					{
						SetTaskState(State_WaitForValidEntryPoint);
						return FSM_Continue;
					}
				}
			}
		}
	}

	// If the player presses run, run
	if( IsFlagSet(CVehicleEnterExitFlags::PlayerControlledVehicleEntry) )
	{
		if (GetSubTask() && GetSubTask()->GetTaskType()== CTaskTypes::TASK_GO_TO_CAR_DOOR_AND_STAND_STILL)
		{
			CTaskGoToCarDoorAndStandStill* pGoToDoorTask = static_cast<CTaskGoToCarDoorAndStandStill*>(GetSubTask());
			
			bool bRun = IsFlagSet(CVehicleEnterExitFlags::WantsToJackFriendlyPed);

			CControl* pControl = pPed->GetControlFromPlayer();
			if( pControl && pControl->GetPedSprintIsDown() )
			{
				bRun = true;
			}

			if (!bRun)
			{
				bRun = pPed->WantsToMoveQuickly();
			}

			if (bRun)
			{
				pGoToDoorTask->SetMoveBlendRatio(MOVEBLENDRATIO_RUN);
			}
		}
	}

	//Check if we are moving to the vehicle door.
	CTaskMoveGoToVehicleDoor* pTaskMove = static_cast<CTaskMoveGoToVehicleDoor *>(
		pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_GO_TO_VEHICLE_DOOR));
	if(pTaskMove)
	{
		//Check if the ped is close to the vehicle.
		spdAABB bbox = m_pVehicle->GetBaseModelInfo()->GetBoundingBox();
		Vec3V vPos = m_pVehicle->GetTransform().UnTransform(pPed->GetTransform().GetPosition());
		ScalarV scDistSq = bbox.DistanceToPointSquared(vPos);
		ScalarV scMaxDistSq = ScalarVFromF32(square(ms_Tunables.m_MaxDistanceToReactToJackForGoToDoor));
		if(IsLessThanAll(scDistSq, scMaxDistSq))
		{
			//React to the jack.
			CReactToBeingJackedHelper::CheckForAndMakeOccupantsReactToBeingJacked(*pPed, *m_pVehicle, m_iTargetEntryPoint, true);
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::GoToSeat_OnEnter()
{
	if (taskVerifyf(m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint), "Invalid target entry index %u", m_iTargetEntryPoint))
	{
		Vector3 vNewTargetPosition(Vector3::ZeroType);
		Quaternion qNewTargetOrientation(0.0f,0.0f,0.0f,1.0f);

		if (taskVerifyf(CTaskVehicleFSM::ComputeTargetTransformInWorldSpace(*m_pVehicle, vNewTargetPosition, qNewTargetOrientation, m_iTargetEntryPoint, CExtraVehiclePoint::GET_IN), "Couldn't compute seat get in offset"))
		{
			if (m_pVehicle->GetIsAquatic())
			{
				Vector3 TargetOffset = VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().UnTransform(VECTOR3_TO_VEC3V(vNewTargetPosition)));

				static dev_s32 s_MaxSeekTime = 10000; 
				CTask* pTask = rage_new TTaskMoveSeekEntityXYOffsetRotated(m_pVehicle, s_MaxSeekTime);
				CEntitySeekPosCalculatorXYOffsetRotated seekPosCalculator(TargetOffset);
					((TTaskMoveSeekEntityXYOffsetRotated*)pTask)->SetEntitySeekPosCalculator(seekPosCalculator);

				((TTaskMoveSeekEntityXYOffsetRotated*)pTask)->SetMoveBlendRatio(m_fMoveBlendRatio);
				((TTaskMoveSeekEntityXYOffsetRotated*)pTask)->SetTargetRadius(CTaskMoveGoToPoint::ms_fTargetRadius);

				SetNewTask(rage_new CTaskComplexControlMovement(pTask));
			}
			else
			{
				CTaskMoveGoToPoint * pMoveTask = rage_new CTaskMoveGoToPoint(m_fMoveBlendRatio, vNewTargetPosition);
				SetNewTask(rage_new CTaskComplexControlMovement(pMoveTask));
			}
		}
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::GoToSeat_OnUpdate()
{
	if (!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		m_iLastCompletedState = State_GoToSeat;
		SetTaskState(State_Align);
		return FSM_Continue;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::GoToClimbUpPoint_OnEnter()
{
	m_uLostClimpUpPhysicalTime = 0;

	ResetRagdollOnCollision(*GetPed());

	bool bAllowsOnVehicleNav = m_pVehicle->GetIsAquatic() || m_pVehicle->InheritsFromPlane() || m_pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_CAN_NAVIGATE_TO_ON_VEHICLE_ENTRY);
	if( bAllowsOnVehicleNav && (GetPed()->GetGroundPhysical() == m_pVehicle) )
	{
		SetRunningFlag(CVehicleEnterExitFlags::EnteringOnVehicle);
	}

	if (taskVerifyf(m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint), "Invalid target entry index %u", m_iTargetEntryPoint))
	{
		Vector3 vNewTargetPosition(Vector3::ZeroType);
		Quaternion qNewTargetOrientation(0.0f,0.0f,0.0f,1.0f);
		CTaskEnterVehicleAlign::ComputeAlignTarget(vNewTargetPosition, qNewTargetOrientation, IsFlagSet(CVehicleEnterExitFlags::EnteringOnVehicle), *m_pVehicle, *GetPed(), m_iTargetEntryPoint, &m_vLocalSpaceEntryModifier);

		TUNE_FLOAT(GO_TO_CLIMB_RADIUS, 0.3f, 0.0f, 3.0f, 0.01f);

		//Allow for stationary planes hovering in VTOL mode
		bool bIsPlaneHoveringInVTOL = false;
		if (m_pVehicle->InheritsFromPlane())
		{
			const CPlane* pPlane = static_cast<CPlane*>(m_pVehicle.Get());
			if (pPlane && pPlane->GetVerticalFlightModeAvaliable() && pPlane->GetVerticalFlightModeRatio() > 0)
			{
				bIsPlaneHoveringInVTOL = true;
			}
		}

		if (bAllowsOnVehicleNav && !(m_pVehicle->InheritsFromPlane() && !bIsPlaneHoveringInVTOL))
		{
			Vector3 TargetOffset = VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().UnTransform(VECTOR3_TO_VEC3V(vNewTargetPosition)));

			float fNavMBR = Min(m_fMoveBlendRatio, MOVEBLENDRATIO_WALK);

			static dev_s32 s_MaxSeekTime = 10000; 
			static dev_s32 s_ScanPeriod = 0; 
			TTaskMoveSeekEntityXYOffsetRotated* pTask = rage_new TTaskMoveSeekEntityXYOffsetRotated(m_pVehicle, s_MaxSeekTime, s_ScanPeriod);
			CEntitySeekPosCalculatorXYOffsetRotated seekPosCalculator(TargetOffset);
			((TTaskMoveSeekEntityXYOffsetRotated*)pTask)->SetEntitySeekPosCalculator(seekPosCalculator);

			((TTaskMoveSeekEntityXYOffsetRotated*)pTask)->SetMoveBlendRatio(fNavMBR);
			((TTaskMoveSeekEntityXYOffsetRotated*)pTask)->SetTargetRadius(GO_TO_CLIMB_RADIUS);

			static dev_float s_GoToPointDistance = 100.0f; 
			pTask->SetGoToPointDistance(s_GoToPointDistance);

			SetNewTask(rage_new CTaskComplexControlMovement(pTask));
		}
		else
		{
			CTaskMoveGoToPointAndStandStill* pGotoTask = rage_new CTaskMoveGoToPointAndStandStill(m_fMoveBlendRatio, vNewTargetPosition, GO_TO_CLIMB_RADIUS, CTaskMoveGoToPointAndStandStill::ms_fSlowDownDistance, false, true);
			SetNewTask(rage_new CTaskComplexControlMovement(pGotoTask));
		}
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::GoToClimbUpPoint_OnUpdate()
{
	CPed* pPed = GetPed();
	
	TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, CLIMB_TO_GET_IN_BLEND_IN, 0.45f, 0.0f, 2.0f, 0.1f);
	TUNE_GROUP_INT(ENTER_VEHICLE_TUNE, LOST_GROUND_PHYSICAL_BREAKOUT_TIME, 500, 0, 5000, 100);

	Vector3 vOveridePosForExitCondition = VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().GetPosition());
	if (taskVerifyf(m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint), "Invalid target entry index %u", m_iTargetEntryPoint))
	{
		Quaternion qNewTargetOrientation(0.0f,0.0f,0.0f,1.0f);
		CTaskEnterVehicleAlign::ComputeAlignTarget(vOveridePosForExitCondition, qNewTargetOrientation, IsFlagSet(CVehicleEnterExitFlags::EnteringOnVehicle), *m_pVehicle, *GetPed(), m_iTargetEntryPoint, &m_vLocalSpaceEntryModifier);
	}

	if (m_pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_DONT_STOP_WHEN_GOING_TO_CLIMB_UP_POINT))
	{
		// No stopping, we manually trigger aligns and this allows us to perform a automatic vault as part of the entry consistently
		CTaskMoveGoToPointAndStandStill* pMoveTask = (CTaskMoveGoToPointAndStandStill*)pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL);
		if (pMoveTask)
		{
			pMoveTask->SetDistanceRemaining(10.0f);
		}
	}

	if(ProcessFriendlyJackDelay(pPed, m_pVehicle) && !m_pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_TURRET_SEAT_ON_VEHICLE))
	{
		return FSM_Continue;
	}

	// Check if we managed to navigate close enough to the climb up point
	const bool bIsOnVehicleEntry = IsOnVehicleEntry();
	if (CTaskEnterVehicleAlign::ShouldTriggerAlign(*m_pVehicle, *pPed, m_iTargetEntryPoint, &m_vLocalSpaceEntryModifier, IsFlagSet(CVehicleEnterExitFlags::CombatEntry), IsFlagSet(CVehicleEnterExitFlags::FromCover), false, bIsOnVehicleEntry)
		&& taskVerifyf(StartMoveNetwork(*pPed, *m_pVehicle, m_moveNetworkHelper, m_iTargetEntryPoint, CLIMB_TO_GET_IN_BLEND_IN, &m_vLocalSpaceEntryModifier, false, m_OverridenEntryClipSetId), "Failed to start TASK_ENTER_VEHICLE move network"))
	{
		m_iLastCompletedState = State_GoToClimbUpPoint;
		SetTaskState(State_StreamAnimsToAlign);
		return FSM_Continue;
	}
	// Check to see if the player wants to quit entering
	else if (CheckPlayerExitConditions(*pPed, *m_pVehicle, m_iRunningFlags, GetTimeRunning(), false, &vOveridePosForExitCondition))
	{
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	bool bProcessOptionalDelay = true;
	if (m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint))
	{
		const CVehicleEntryPointInfo* pEntryInfo = m_pVehicle->GetEntryInfo(m_iTargetEntryPoint);
		bProcessOptionalDelay = !pEntryInfo->GetMPWarpInOut() || pPed->IsBossPed();
	}

	if (bProcessOptionalDelay && (ProcessOptionalForcedRearSeatDelay(pPed, m_pVehicle) || ProcessOptionalForcedTurretDelay(pPed, m_pVehicle)))
	{
		return FSM_Continue;
	}

	if (GetIsSubtaskFinished(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT))
	{
		m_iLastCompletedState = State_GoToClimbUpPoint;

		// If we're not at the climb up point or couldn't jack or enter the seat, quit out
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	//! If we have fallen off our ground physical then pick another door.
	CPhysical* pPhys = pPed->GetGroundPhysical();
	if(pPhys != m_pVehicle)
	{
		if(m_uLostClimpUpPhysicalTime == 0)
		{
			m_uLostClimpUpPhysicalTime = fwTimer::GetTimeInMilliseconds();
		}
		
		if( pPed->GetIsInWater() || ((fwTimer::GetTimeInMilliseconds() - m_uLostClimpUpPhysicalTime) > LOST_GROUND_PHYSICAL_BREAKOUT_TIME) )
		{
			ClearRunningFlag(CVehicleEnterExitFlags::EnteringOnVehicle);
			SetTaskState(State_PickDoor);
		}
	}
	else
	{
		m_uLostClimpUpPhysicalTime = 0;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::ReserveSeat_OnUpdate()
{
	CPed* pPed = GetPed();

	TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, MIN_TIME_IN_RESERVE_TO_ALLOW_AIM_BREAK, 0.5f, 0.0f, 2.0f, 0.01f);
	if (CheckPlayerExitConditions(*pPed, *m_pVehicle, m_iRunningFlags, GetTimeRunning()) || (pPed->IsLocalPlayer() && pPed->GetPlayerInfo()->IsAiming(false) && GetTimeInState() > MIN_TIME_IN_RESERVE_TO_ALLOW_AIM_BREAK))
	{
		SetTaskFinishState(TFR_PED_INTERRUPTED, VEHICLE_DEBUG_DEFAULT_FLAGS);
		return FSM_Continue;
	}

	const CEntryExitPoint* pEntryPoint = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint);
	s32 iSeat = pEntryPoint->GetSeat(SA_directAccessSeat, m_iSeat);
	CPed* pSeatOccupier = m_pVehicle->GetSeatManager()->GetPedInSeat(iSeat);
	const bool bSeatOccupierShuffling = IsSeatOccupierShuffling(pSeatOccupier);
	if (pSeatOccupier && !bSeatOccupierShuffling)
	{
		// Need to reevaluate the seat in case someone is entering
		if (!CModelSeatInfo::EvaluateSeatAccessFromEntryPoint(pPed, m_pVehicle, m_iSeatRequestType, m_iSeat, m_iTargetEntryPoint, m_bWarping, m_iRunningFlags ) )
		{
			AI_LOG_WITH_ARGS("[ComponentReservation] - Ped %s returning to pick door state when trying to reserve seat due to seat occupier %s\n", AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetDynamicEntityNameSafe(pSeatOccupier));
			SetTaskState(State_PickDoor);
			return FSM_Continue;
		}
	}

	// We can get stuck waiting for a shuffling ped, so make sure we don't get dragged behind the vehicle
	if (m_bWaitForSeatToBeEmpty && ShouldAbortBecauseVehicleMovingTooFast())
	{
		SetTaskFinishState(TFR_VEHICLE_MOVING_TOO_FAST, VEHICLE_DEBUG_DEFAULT_FLAGS | VEHICLE_DEBUG_RENDER_RELATIVE_VELOCITY);
		return FSM_Continue;
	}

	CComponentReservation* pComponentReservation = m_pVehicle->GetComponentReservationMgr()->GetSeatReservation(m_iTargetEntryPoint, SA_directAccessSeat, m_iSeat);
	if (CComponentReservationManager::ReserveVehicleComponent(pPed, m_pVehicle, pComponentReservation))
	{
		// We can sometimes get into a state where m_bWaitForSeatToBeOccupied is set to true, but no one is getting in
		// could maybe sync the ped who we think is in the seat and query their tasks?
		bool bShouldWait = true;
		TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, MAX_TIME_TO_WAIT_FOR_OCCUPIER, 3.0f, 0.0f, 5.0f, 0.01f);
		if (m_bWaitForSeatToBeOccupied && GetTimeInState() > MAX_TIME_TO_WAIT_FOR_OCCUPIER)
		{
			bShouldWait = false;
			AI_LOG_WITH_ARGS("[ComponentReservation] - Waiting for seat to be occupied for longer than %.2f seconds, assuming it's free\n", MAX_TIME_TO_WAIT_FOR_OCCUPIER);
		}

		bool bShouldWaitForSeatToBeEmpty = (m_bWaitForSeatToBeEmpty && pSeatOccupier) ? true : false;
		if (bShouldWaitForSeatToBeEmpty && m_pVehicle->InheritsFromBoat() && NetworkInterface::IsGameInProgress() && pSeatOccupier->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE))
		{
			bShouldWait = false;
			AI_LOG_WITH_ARGS("[ComponentReservation] - Allowing ped %s to bypass wait for seat to be empty because ped %s is shuffling\n", AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetDynamicEntityNameSafe(pSeatOccupier));
		}

		if(bShouldWait &&
			((m_bWaitForSeatToBeOccupied && !pSeatOccupier) ||
			(m_bWaitForSeatToBeEmpty    &&  pSeatOccupier)))
		{
			// Try again next frame
			AI_LOG_WITH_ARGS("[ComponentReservation] - Waiting for seat to be %s\n", m_bWaitForSeatToBeEmpty ? "empty" : "occupied");
		}
		else
		{
			m_bWaitForSeatToBeOccupied = false;
			m_bWaitForSeatToBeEmpty    = false;

			if (IsFlagSet(CVehicleEnterExitFlags::WarpOverPassengers) && pSeatOccupier && !CTaskVehicleFSM::CanJackPed(pPed, pSeatOccupier, m_iRunningFlags))
			{
				SetRunningFlag(CVehicleEnterExitFlags::IsWarpingOverPassengers);
			}
			else if (pSeatOccupier && CTaskVehicleFSM::CanJackPed(pPed, pSeatOccupier, m_iRunningFlags)
				&& !IsSeatOccupierShuffling(pSeatOccupier))
			{
				m_pJackedPed = pSeatOccupier;
				SetRunningFlag(CVehicleEnterExitFlags::HasJackedAPed);
			}

			fwMvClipSetId entryClipsetId = CTaskEnterVehicleAlign::GetEntryClipsetId(m_pVehicle, m_iTargetEntryPoint, GetPed());
			if (!CTaskVehicleFSM::IsVehicleClipSetLoaded(entryClipsetId))
			{
#if __BANK
				CTaskVehicleFSM::SpewStreamingDebugToTTY(*pPed, *m_pVehicle, m_iTargetEntryPoint, entryClipsetId, GetTimeInState());
#endif // __BANK

				return FSM_Continue;
			}

			// Quit if injured or our move network hasn't been started somehow
			if (pPed->IsInjured() || !taskVerifyf(m_moveNetworkHelper.IsNetworkActive(), "Move network wasn't active when changing state, previous state %s", GetStaticStateName(GetPreviousState())))
			{
				pPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
				SetTaskState(State_Finish);
				return FSM_Continue;
			}
			else if (m_pJackedPed)
			{
                CTaskExitVehicle* pExitVehicleTask = static_cast<CTaskExitVehicle*>(m_pJackedPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE));

                if(pExitVehicleTask && !pExitVehicleTask->IsFlagSet(CVehicleEnterExitFlags::BeJacked))
                {
					SetTaskFinishState(TFR_JACKED_PED_NOT_RUNNING_EXIT_TASK, VEHICLE_DEBUG_DEFAULT_FLAGS | VEHICLE_DEBUG_PRINT_JACKED_PED);
                }
                else
                {
                    SetTaskState(State_JackPedFromOutside);
                }

				return FSM_Continue;
			}
			else
			{
				SetTaskState(State_StreamAnimsToEnterSeat);
				return FSM_Continue;
			}
		}
	}

#if __BANK
	if (pComponentReservation)
	{
		AI_LOG_WITH_ARGS("[ComponentReservation] - Ped %s is waiting to reserve seat %i, owned by %s\n", AILogging::GetDynamicEntityNameSafe(pPed), m_iSeat, AILogging::GetDynamicEntityNameSafe(pComponentReservation->GetOwningEntity()));
	}
#endif // __BANK
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::ReserveDoorToOpen_OnUpdate()
{
	CPed* pPed = GetPed();

	// Need to reevaluate the seat in case someone is entering
	if (!CModelSeatInfo::EvaluateSeatAccessFromEntryPoint(pPed, m_pVehicle, m_iSeatRequestType, m_iSeat, m_iTargetEntryPoint, m_bWarping, m_iRunningFlags ) )
	{
		SetTaskState(State_PickDoor);
		return FSM_Continue;
	}
	if (TryToReserveDoor(*pPed, *m_pVehicle, m_iTargetEntryPoint))
	{
		if (IsFlagSet(CVehicleEnterExitFlags::WarpToEntryPosition))
		{
			SetTaskState(State_StreamAnimsToOpenDoor);
			return FSM_Continue;
		}
		else 
		{
			SetTaskState(State_StreamAnimsToOpenDoor);
			return FSM_Continue;
		}
	}
	else if (CheckPlayerExitConditions(*pPed, *m_pVehicle, m_iRunningFlags, GetTimeRunning()))
	{
		SetTaskFinishState(TFR_PED_INTERRUPTED, VEHICLE_DEBUG_DEFAULT_FLAGS);
		return FSM_Continue;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::StreamAnimsToAlign_OnEnter()
{
	CPed& rPed = *GetPed();
	s32 iSeatIndex = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat);

	const CPed* pOccupyingPed = GetPedOccupyingDirectAccessSeat();
	if (m_pVehicle && m_pVehicle->IsTrailerSmall2() && pOccupyingPed && !IsOnVehicleEntry())
	{
		iSeatIndex = -1;
	}

	rPed.AttachPedToEnterCar(m_pVehicle, GetAttachToVehicleFlags(m_pVehicle), iSeatIndex, m_iTargetEntryPoint);
	QuatV qPedOrientation = QuatVFromMat34V(rPed.GetTransform().GetMatrix());
	UpdatePedVehicleAttachment(&rPed, rPed.GetTransform().GetPosition(), qPedOrientation);
	return FSM_Continue;
}

void CTaskEnterVehicle::StreamAnimsToAlign_OnEnterClone()
{
	CTaskVehicleFSM::SetRemotePedUsingDoorsTrue(*GetPed(), m_pVehicle, m_iTargetEntryPoint);
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::StreamAnimsToAlign_OnUpdate()
{
	CPed& rPed = *GetPed();
	const float fTimeBeforeWarp = NetworkInterface::IsGameInProgress() ? ms_Tunables.m_MaxTimeStreamClipSetInBeforeWarpMP : ms_Tunables.m_MaxTimeStreamClipSetInBeforeWarpSP;
	const bool bIsOnVehicleEntry = IsOnVehicleEntry();
	fwMvClipSetId alignClipsetId = CTaskEnterVehicleAlign::GetCommonClipsetId(m_pVehicle, m_iTargetEntryPoint, bIsOnVehicleEntry);

	if (CTaskVehicleFSM::IsVehicleClipSetLoaded(alignClipsetId))
	{
		aiDisplayf("Frame : %i, clipset %s loaded", fwTimer::GetFrameCount(), alignClipsetId.GetCStr());
		const bool bCombatEntry = IsFlagSet(CVehicleEnterExitFlags::CombatEntry);
		if (bCombatEntry && CheckForOpenDoor() && ShouldUseCombatEntryAnimLogic())
		{
			SetTaskState(State_StreamAnimsToOpenDoor);
			return FSM_Continue;
		}

		if (CTaskEnterVehicleAlign::ShouldTriggerAlign(*m_pVehicle, rPed, m_iTargetEntryPoint, &m_vLocalSpaceEntryModifier, IsFlagSet(CVehicleEnterExitFlags::CombatEntry), IsFlagSet(CVehicleEnterExitFlags::FromCover), false, bIsOnVehicleEntry))
		{
			if (bIsOnVehicleEntry)
			{
				SetRunningFlag(CVehicleEnterExitFlags::EnteringOnVehicle);
			}

			if (ms_Tunables.m_EnableBikePickUpAlign && m_pVehicle->InheritsFromBike())
			{
				s32 iDesiredState = State_Align;
				if (CheckForBikeOnGround(iDesiredState))
				{
					if (CheckIfPlayerIsPickingUpMyBike())
					{
						SetTaskState(State_Finish);
						return FSM_Continue;
					}
					// Seems like we could remove the check below as its covered already?
					fwMvClipSetId commonClipsetId = CTaskEnterVehicleAlign::GetCommonClipsetId(m_pVehicle, m_iTargetEntryPoint);
					if (!CTaskVehicleFSM::IsVehicleClipSetLoaded(commonClipsetId))
					{
						return FSM_Continue;
					}

					if (StartMoveNetwork(rPed, *m_pVehicle, m_moveNetworkHelper, m_iTargetEntryPoint, ms_Tunables.m_BikePickUpAlignBlendDuration, &m_vLocalSpaceEntryModifier, false, m_OverridenEntryClipSetId))
					{
						SetTaskState(iDesiredState);
						return FSM_Continue;
					}
				}
			}
			SetTaskState(State_Align);
			return FSM_Continue;
		}
		else
		{
			SetTaskState(State_PickDoor);
			return FSM_Continue;
		}
	}
	else
	{
		const bool bStreamAnimsToOpenDoor = IsFlagSet(CVehicleEnterExitFlags::CombatEntry) && CheckForOpenDoor() && ShouldUseCombatEntryAnimLogic();
		const bool bShouldTriggerAlign = CTaskEnterVehicleAlign::ShouldTriggerAlign(*m_pVehicle, rPed, m_iTargetEntryPoint, &m_vLocalSpaceEntryModifier, IsFlagSet(CVehicleEnterExitFlags::CombatEntry), IsFlagSet(CVehicleEnterExitFlags::FromCover), false, bIsOnVehicleEntry);

		if (!bStreamAnimsToOpenDoor && bShouldTriggerAlign)
		{
			// Alignment anims not streamed in yet, but make sure leg solver stays blended in this frame since the ped has already been attached to the vehicle in StreamAnimsToAlign_OnEnter
			rPed.SetPedResetFlag(CPED_RESET_FLAG_RequiresLegIk, true);
		}
	}

#if __BANK
	CTaskVehicleFSM::SpewStreamingDebugToTTY(rPed, *m_pVehicle, m_iSeat, alignClipsetId, GetTimeInState());
#endif // __BANK

	if (GetTimeInState() > fTimeBeforeWarp)
	{
		if (m_pVehicle->IsSeatIndexValid(m_iSeat))
		{
			aiDisplayf("Waited more than %.2f seconds to stream entry anims, warping instead", fTimeBeforeWarp);
			m_bWarping = true;
			m_iRunningFlags.BitSet().Set(CVehicleEnterExitFlags::Warp);
			m_iCurrentSeatIndex = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat, m_iSeat);
			SetTaskState(State_WaitForPedToLeaveSeat);
			return FSM_Continue;
		}
	}
	return FSM_Continue;
}

void CTaskEnterVehicle::StreamAnimsToAlign_OnExitClone()
{
	CTaskVehicleFSM::SetRemotePedUsingDoorsFalse(*GetPed(), m_pVehicle, m_iTargetEntryPoint);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::StreamAnimsToOpenDoor_OnEnterClone()
{
	CTaskVehicleFSM::SetRemotePedUsingDoorsTrue(*GetPed(), m_pVehicle, m_iTargetEntryPoint);
}

CTask::FSM_Return CTaskEnterVehicle::StreamAnimsToOpenDoor_OnUpdate()
{
	const float fTimeBeforeWarp = NetworkInterface::IsGameInProgress() ? ms_Tunables.m_MaxTimeStreamClipSetInBeforeWarpMP : ms_Tunables.m_MaxTimeStreamClipSetInBeforeWarpSP;

	CPed* pPed = GetPed();
	const bool bCombatEntry = ShouldUseCombatEntryAnimLogic();
	if (bCombatEntry && !pPed->GetIsAttached())
	{
		pPed->AttachPedToEnterCar(m_pVehicle, GetAttachToVehicleFlags(m_pVehicle), m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat), m_iTargetEntryPoint);
		QuatV qPedOrientation = QuatVFromMat34V(pPed->GetTransform().GetMatrix());
		UpdatePedVehicleAttachment(pPed, pPed->GetTransform().GetPosition(), qPedOrientation);
	}
	const fwMvClipSetId entryClipsetId = CTaskEnterVehicleAlign::GetEntryClipsetId(m_pVehicle, m_iTargetEntryPoint, GetPed());
	const float fBlendInDuration = bCombatEntry ? ms_Tunables.m_CombatEntryBlendDuration : INSTANT_BLEND_DURATION;
	if (CTaskVehicleFSM::IsVehicleClipSetLoaded(entryClipsetId)
		&& taskVerifyf(StartMoveNetwork(*GetPed(), *m_pVehicle, m_moveNetworkHelper, m_iTargetEntryPoint, fBlendInDuration, &m_vLocalSpaceEntryModifier, true, m_OverridenEntryClipSetId), "Failed to start TASK_ENTER_VEHICLE move network"))
	{
		SetTaskState(State_OpenDoor);
		return FSM_Continue;
	}

#if __BANK
	CTaskVehicleFSM::SpewStreamingDebugToTTY(*GetPed(), *m_pVehicle, m_iTargetEntryPoint, entryClipsetId, GetTimeInState());
#endif // __BANK
	
	if (GetTimeInState() > fTimeBeforeWarp)
	{
		if (m_pVehicle->IsSeatIndexValid(m_iSeat))
		{
			aiDisplayf("Waited more than %.2f seconds to stream entry anims, warping instead", fTimeBeforeWarp);
			m_bWarping = true;
			m_iRunningFlags.BitSet().Set(CVehicleEnterExitFlags::Warp);
			m_iCurrentSeatIndex = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat, m_iSeat);
			SetTaskState(State_WaitForPedToLeaveSeat);
			return FSM_Continue;
		}
	}
	return FSM_Continue;
}

void CTaskEnterVehicle::StreamAnimsToOpenDoor_OnExitClone()
{
	CTaskVehicleFSM::SetRemotePedUsingDoorsFalse(*GetPed(), m_pVehicle, m_iTargetEntryPoint);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::Align_OnEnter()
{
	m_iTimeStartedAlign = fwTimer::GetTimeInMilliseconds();
	m_bAligningToOpenDoor = false;
	const CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(m_pVehicle, m_iTargetEntryPoint);
	if (pDoor && (!pDoor->GetIsIntact(m_pVehicle) || pDoor->GetDoorRatio() >= ms_Tunables.m_DoorRatioToConsiderDoorOpen))
	{
		m_bAligningToOpenDoor = true;
	}
	SetUpRagdollOnCollision(*GetPed(), *m_pVehicle);
	SetNewTask(CreateAlignTask());

	if(!GetPed()->GetIkManager().IsLooking() || !m_bEnterVehicleLookAtStarted)
	{
		m_bEnterVehicleLookAtStarted = StartEnterVehicleLookAt(GetPed(), m_pVehicle);
	}

	if (m_pVehicle->InheritsFromBike())
	{
		const float fSide = m_pVehicle->GetTransform().GetA().GetZf();
		m_bIsPickUpAlign = Abs(fSide) < CTaskEnterVehicle::ms_Tunables.m_MinMagForBikeToBeOnSide;
	}

	if (GetPed()->IsLocalPlayer())
	{
		CTask* pTask = GetPed()->GetPedIntelligence()->FindTaskActiveByTreeAndType(PED_TASK_TREE_SECONDARY, CTaskTypes::TASK_RELOAD_GUN);
		if (pTask)
		{
			pTask->MakeAbortable(aiTask::ABORT_PRIORITY_IMMEDIATE, NULL);
		}
	}

	CreateWeaponManagerIfRequired();

	m_bVehicleDoorOpen = !CheckForOpenDoor();

	if (GetPed()->IsLocalPlayer())
	{
		CTaskVehicleFSM::SetPassengersToLookAtPlayer(GetPed(), m_pVehicle, true);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::Align_OnEnterClone()
{
	m_iTimeStartedAlign = fwTimer::GetTimeInMilliseconds();
#if __ASSERT
    bool animsLoaded = false;

    const CVehicleEntryPointAnimInfo *pClipInfo = m_pVehicle ? m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint) : 0;

    if(pClipInfo)
    {
        const fwMvClipSetId entryClipsetId = pClipInfo->GetEntryPointClipSetId();
        animsLoaded = CTaskVehicleFSM::IsVehicleClipSetLoaded(entryClipsetId);
    }

    taskAssertf(animsLoaded, "Anims Weren't Streamed Before Align! (Entry Point %d)", m_iTargetEntryPoint);
#endif // __ASSERT

	CPed *pPed = GetPed();
    if(!IsFlagSet(CVehicleEnterExitFlags::EnteringOnVehicle))
    {
        if(!CTaskEnterVehicleAlign::ShouldTriggerAlign(*m_pVehicle, *pPed, m_iTargetEntryPoint, &m_vLocalSpaceEntryModifier, IsFlagSet(CVehicleEnterExitFlags::CombatEntry)))
        {
            // ensure we are aligned with the door correctly
	        Vector3 vEntryPos(Vector3::ZeroType);
	        Quaternion qEntryOrientation(0.0f,0.0f,0.0f,1.0f);

	        CModelSeatInfo::CalculateEntrySituation(m_pVehicle.Get(), pPed, vEntryPos, qEntryOrientation, m_iTargetEntryPoint, 0, 0.0f, &m_vLocalSpaceEntryModifier);

            
            pPed->Teleport(vEntryPos, pPed->GetCurrentHeading(), true);
        }
    }

	TUNE_GROUP_BOOL(VEHICLE_TUNE, POP_CLONE_DOOR, true);
	if (POP_CLONE_DOOR)
	{
		m_bForcedDoorState = false;
		CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(m_pVehicle, m_iTargetEntryPoint);
		if(pDoor && pDoor->GetIsIntact(m_pVehicle))
		{
			const bool bWillOpenDoor = CheckForOpenDoor();
			if (m_bVehicleDoorOpen && bWillOpenDoor)
			{
				pDoor->ClearFlag(CCarDoor::DRIVEN_MASK);
				pDoor->SetFlag(CCarDoor::PROCESS_FORCE_AWAKE | CCarDoor::SET_TO_FORCE_OPEN);
				m_bForcedDoorState = true;
			}
		}
	}

	SetNewTask(CreateAlignTask());

	CreateWeaponManagerIfRequired();
	CTaskVehicleFSM::SetRemotePedUsingDoorsTrue(*pPed, m_pVehicle, m_iTargetEntryPoint);

	m_ShouldAlterEntryForOpenDoor = CTaskVehicleFSM::ShouldAlterEntryPointForOpenDoor(*pPed, *m_pVehicle, m_iTargetEntryPoint, 0);
	AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(pPed, "[EnterVehicleAlign] Ped %s starting align, m_ShouldAlterEntryForOpenDoor = %s, Door ratio = %.2f\n", AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetBooleanAsString(m_ShouldAlterEntryForOpenDoor), CTaskVehicleFSM::GetDoorRatioForEntryPoint(*m_pVehicle, m_iTargetEntryPoint));
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::Align_OnUpdate()
{
	CTaskVehicleFSM::PointTurretInNeturalPosition(*m_pVehicle, m_iSeat);

	const CPed* pOccupyingPed = GetPedOccupyingDirectAccessSeat();
	if (pOccupyingPed && pOccupyingPed->GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle))
	{
		SetTaskState(State_UnReserveDoorToFinish);
		return FSM_Continue;
	}

	bool bNeedsToOpenDoor = false;

	if (m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint))
	{
		bNeedsToOpenDoor = CheckForOpenDoor();
	}

	CPed& rPed = *GetPed();
	const bool bReservationsComplete = ProcessComponentReservations(bNeedsToOpenDoor);
	const bool bHasSeatReservation = CTaskVehicleFSM::PedHasSeatReservation(*m_pVehicle, rPed, m_iTargetEntryPoint, m_iSeat);

#if __BANK
	if (!bReservationsComplete && (rPed.IsLocalPlayer() || rPed.PopTypeIsMission()))
	{
		AI_LOG_WITH_ARGS("[EnterVehicle] Ped %s is in align state, reservations not complete, has seat reservation = %s\n", AILogging::GetDynamicEntityNameSafe(&rPed), AILogging::GetBooleanAsString(bHasSeatReservation));
	}
#endif // __BANK

	rPed.SetPedResetFlag(CPED_RESET_FLAG_RequiresLegIk, true);
	rPed.SetPedResetFlag(CPED_RESET_FLAG_DisableIndependentMoverFrame, true);
	rPed.GetIkManager().SetFlag(PEDIK_LEGS_AND_PELVIS_FADE_ON);

	if (m_pVehicle->GetVehicleType() == VEHICLE_TYPE_BICYCLE && static_cast<CBike*>(m_pVehicle.Get())->m_nBikeFlags.bGettingPickedUp)
	{
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	// Abort the align if the bike is falling down
	if (m_pVehicle->InheritsFromBike() && !m_bIsPickUpAlign)
	{
		const float fSide = m_pVehicle->GetTransform().GetA().GetZf();
		if (Abs(fSide) < CTaskEnterVehicle::ms_Tunables.m_MinMagForBikeToBeOnSide)
		{
			SetTaskState(State_Finish);
			return FSM_Continue;
		}
	}

	ProcessDoorSteps();

	bool bShouldWarp = false;
	const float fTimeBeforeWarp = NetworkInterface::IsGameInProgress() ? ms_Tunables.m_MaxTimeStreamClipSetInBeforeWarpMP : ms_Tunables.m_MaxTimeStreamClipSetInBeforeWarpSP;
	if (GetSubTask() && GetSubTask()->GetState() == CTaskEnterVehicleAlign::State_StreamAssets && GetTimeInState() > fTimeBeforeWarp)
	{
		bShouldWarp = true;
	}

	TUNE_GROUP_BOOL(ENTER_VEHICLE_TUNE, ENABLE_LOWRIDER_ALIGN_WAIT, true);
	if (ENABLE_LOWRIDER_ALIGN_WAIT && m_pVehicle->InheritsFromAutomobile())
	{
		const CAutomobile* pAutomobile = static_cast<const CAutomobile*>(m_pVehicle.Get());
		if (pAutomobile && pAutomobile->HasHydraulicSuspension())
		{
			TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, LOWRIDER_MAX_TIME_TO_WAIT, 1.0f, 0.0f, 5.0f, 0.01f);
			if (GetTimeInState() < LOWRIDER_MAX_TIME_TO_WAIT)
			{
				TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, LOWRIDER_MAX_Z_VELOCITY_TO_ENTER, 0.5f, -5.0f, 5.0f, 0.01f);
				const float fZVelocity = m_pVehicle->GetVelocity().z;
				if (fZVelocity < -LOWRIDER_MAX_Z_VELOCITY_TO_ENTER)
				{
					aiDisplayf("Frame %i, delaying align to Lowrider vehicle due to fZVelocity %.2f", fwTimer::GetFrameCount(), fZVelocity);
					return FSM_Continue;
				}
				else
				{
					aiDisplayf("Frame %i, fZVelocity %.2f", fwTimer::GetFrameCount(), fZVelocity);
				}
			}
			{
				aiDisplayf("Frame %i, past time in state %.2f", fwTimer::GetFrameCount(), GetTimeInState());
			}
		}
	}

	if (m_pVehicle->IsTrailerSmall2() && m_pVehicle->GetVehicleWeaponMgr() && pOccupyingPed && !IsOnVehicleEntry())
	{
		CTurret* pTurret = m_pVehicle->GetVehicleWeaponMgr()->GetFirstTurretForSeat(0);
		if (pTurret)
		{
			m_pVehicle->ActivatePhysics();

			const float fVehHeading = m_pVehicle->GetTransform().GetHeading();
			const float fTurretHeading = pTurret->GetCurrentHeading(m_pVehicle);
			const float fDiff = Abs(fwAngle::LimitRadianAngle(fVehHeading - fTurretHeading));
			static float fThresh = 0.01f;
			if ( fDiff > fThresh )
			{
				return FSM_Continue;
			}
		}
	}


	// We may need to wait for the seat reservation after the align has finished
	if (bShouldWarp || GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !GetSubTask())
	{
		AI_LOG_WITH_ARGS("[VehicleEntryExit][CTaskEnterVehicle::Align_OnUpdate] - %s Ped %s is trying to transition from %s bShouldWarp = %s\n", AILogging::GetDynamicEntityIsCloneStringSafe(&rPed), AILogging::GetDynamicEntityNameSafe(&rPed), GetStaticStateName(GetState()), AILogging::GetBooleanAsString(bShouldWarp));
		const CVehicleEntryPointAnimInfo* pAnimInfo = m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint);
		const CVehicleEntryPointInfo* pEntryInfo = m_pVehicle->GetEntryInfo(m_iTargetEntryPoint);
		const bool bInWaterEntry = IsFlagSet(CVehicleEnterExitFlags::InWater);

		// If we're not close enough, reevaluate and try again if flagged to
		if (!bShouldWarp && !CTaskEnterVehicleAlign::AlignSucceeded(*m_pVehicle, rPed, m_iTargetEntryPoint, IsFlagSet(CVehicleEnterExitFlags::EnteringOnVehicle), false, bInWaterEntry, &m_vLocalSpaceEntryModifier, false, true, IsFlagSet(CVehicleEnterExitFlags::FromCover)))
		{
			if (IsFlagSet(CVehicleEnterExitFlags::ResumeIfInterupted))
			{
				ASSERT_ONLY(taskDebugf2("Frame : %u - %s%s stopped Enter Vehicle Move Network : CTaskEnterVehicle::Align_OnUpdate()", fwTimer::GetFrameCount(), rPed.IsNetworkClone() ? "Clone ped : " : "Local ped : ", rPed.GetDebugName()));
				rPed.GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, NORMAL_BLEND_DURATION);
				if (rPed.GetIsAttached())
					rPed.DetachFromParent(0);
				SetTaskState(State_PickDoor);
				return FSM_Continue;
			}
			else
			{
				if (CheckForRestartingAlign())
				{
					SetFlag(aiTaskFlags::RestartCurrentState);
					return FSM_Continue;
				}
				else
				{
					SetTaskFinishState(TFR_PED_ALIGN_FAILED, VEHICLE_DEBUG_DEFAULT_FLAGS);
					return FSM_Continue;
				}
			}
		}

		const bool bPedCanEnterCar = m_pVehicle->CanPedEnterCar(&rPed);
		CPed* pSeatOccupier = GetPedOccupyingDirectAccessSeat();

		const bool bUseAlignTransition = !m_pVehicle->InheritsFromBike() && pAnimInfo->GetUsesNoDoorTransitionForEnter();

		if (CheckPlayerExitConditions(rPed, *m_pVehicle, m_iRunningFlags, GetTimeRunning()))
		{
			SetTaskFinishState(TFR_PED_INTERRUPTED, VEHICLE_DEBUG_DEFAULT_FLAGS);
			return FSM_Continue;
		}

		TUNE_GROUP_BOOL(ENTER_VEHICLE_TUNE, ENABLE_HELI_ALIGN_WAIT, true);
		if (ENABLE_HELI_ALIGN_WAIT && m_pVehicle->InheritsFromHeli())
		{
			TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, MAX_TIME_TO_WAIT, 2.5f, 0.0f, 5.0f, 0.01f);
			if (GetTimeInState() < MAX_TIME_TO_WAIT)
			{
				TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, MAX_Z_VELOCITY_TO_ENTER_HELI, 0.5f, 0.0f, 5.0f, 0.01f);
				const float fZVelocity = m_pVehicle->GetVelocity().z;
				if (fZVelocity < -MAX_Z_VELOCITY_TO_ENTER_HELI)
				{
					aiDisplayf("Frame : %i, waiting fZVelocity %.2f", fwTimer::GetFrameCount(), fZVelocity);
					return FSM_Continue;
				}
				else
				{
					aiDisplayf("Frame : %i, fZVelocity %.2f", fwTimer::GetFrameCount(), fZVelocity);
				}
			}
			{
				aiDisplayf("Frame : %i, past time in state %.2f", fwTimer::GetFrameCount(), GetTimeInState());
			}
		}

		// Only wait for reservations if we don't have the seat reservation and the door is already open
		if (!bReservationsComplete && !bNeedsToOpenDoor && !bHasSeatReservation)
		{
			AI_LOG_WITH_ARGS("[EnterVehicle] - %s Ped %s is waiting for reservation(s), vehicle = %s, bNeedsToOpenDoor = %s, FLAG_USE_FULL_ANIMS_FOR_MP_WARP_ENTRY_POINTS = %s\n", AILogging::GetDynamicEntityIsCloneStringSafe(&rPed), AILogging::GetDynamicEntityNameSafe(&rPed), m_pVehicle->GetModelName(), AILogging::GetBooleanAsString(bNeedsToOpenDoor), AILogging::GetBooleanAsString(m_pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_USE_FULL_ANIMS_FOR_MP_WARP_ENTRY_POINTS)));
			if (m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint))
			{
				CComponentReservation* pComponentReservation = m_pVehicle->GetComponentReservationMgr()->GetSeatReservation(m_iTargetEntryPoint, SA_directAccessSeat, m_iSeat);
				if (pComponentReservation)
				{
					CPed* pPedReservingSeat = pComponentReservation->GetPedUsingComponent();
					if (pPedReservingSeat && pPedReservingSeat != &rPed)
					{
						if (!pPedReservingSeat->GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle))
						{
							if (CheckForRestartingAlign())
							{
								SetFlag(aiTaskFlags::RestartCurrentState);
								return FSM_Continue;
							}

							if (IsFlagSet(CVehicleEnterExitFlags::ResumeIfInterupted))
							{
								if (rPed.GetIsAttached())
									rPed.DetachFromParent(0);

								SetTaskState(State_Start);
								return FSM_Continue;
							}
							else
							{
								if (ShouldTryToPickAnotherEntry(*pPedReservingSeat))
								{
									SetTaskState(State_PickDoor);
									return FSM_Continue;
								}

								SetTaskState(State_UnReserveDoorToFinish);
								return FSM_Continue;
							}
						}
					}
				}
			}
			// Wait to get the component reservation
			return FSM_Continue;
		}
		// Check if we should play a streamed entry anim (this replaces the open door/enter seat/jacking states)
		else if (CheckForStreamedEntry())
		{
			// Make sure arresting peds get proper jack variations
			if(rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_WillArrestRatherThanJack) && !m_pStreamedEntryClipSet->GetIsArrest())
			{
				return FSM_Quit;
			}

			SetTaskState(State_WaitForStreamedEntry);
			return FSM_Continue;
		}
		else if (rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_WillArrestRatherThanJack))
		{
			// we need to quit out if we were supposed to arrest but don't have an arrest animation
			return FSM_Quit;
		}
		// Boarding boats from the water, we vault up
		else if (pAnimInfo->GetHasVaultUp() && !IsFlagSet(CVehicleEnterExitFlags::EnteringOnVehicle))
		{
            if(!rPed.IsNetworkClone() || rPed.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_VAULT))
            {
			    m_iLastCompletedState = State_Align;
			    SetTaskState(State_VaultUp);
            }
			return FSM_Continue;
		}
		// Vehicles like tanks require us to climb up first before opening the door
		else if (ShouldClimbUpOnToVehicle(pSeatOccupier, *pAnimInfo, *pEntryInfo))
		{
			fwMvClipSetId entryClipsetId = CTaskEnterVehicleAlign::GetEntryClipsetId(m_pVehicle, m_iTargetEntryPoint, &rPed);
			if (CTaskVehicleFSM::IsVehicleClipSetLoaded(entryClipsetId))
			{
				m_moveNetworkHelper.SetClipSet(entryClipsetId);
				m_iLastCompletedState = State_Align;
				SetTaskState(State_ClimbUp);
			}
			return FSM_Continue;
		}
		// Try to open the door (if there is one)
		else if (CheckForOpenDoor())
		{
			AI_LOG_WITH_ARGS("[EnterVehicle] - Ped %s is transitioning to open door state, bReservationsComplete = %s, bNeedsToOpenDoor = %s, bHasSeatReservation = %s", AILogging::GetDynamicEntityNameSafe(&rPed), AILogging::GetBooleanAsString(bReservationsComplete), AILogging::GetBooleanAsString(bNeedsToOpenDoor), AILogging::GetBooleanAsString(bHasSeatReservation));
			SetTaskState(State_StreamAnimsToOpenDoor);
			return FSM_Continue;
		}
		else if (IsFlagSet(CVehicleEnterExitFlags::JustOpenDoor))
		{
			ASSERT_ONLY(aiDisplayf("Frame %u, %s ped %s quitting enter vehicle task due to just open door", fwTimer::GetFrameCount(), rPed.IsNetworkClone() ? "CLONE" : "LOCAL", rPed.GetDebugName());)
			// If we can't open door, and we are only interested in opening door, then bail.
			SetTaskState(State_Finish);
			return FSM_Continue;
		}
		else if (CheckForClimbUp())
		{
			fwMvClipSetId entryClipsetId = CTaskEnterVehicleAlign::GetEntryClipsetId(m_pVehicle, m_iTargetEntryPoint, &rPed);
			if (CTaskVehicleFSM::IsVehicleClipSetLoaded(entryClipsetId))
			{
				m_moveNetworkHelper.SetClipSet(entryClipsetId);
				m_iLastCompletedState = State_Align;
				SetTaskState(State_ClimbUp);
			}
			return FSM_Continue;
		}
		else if (bPedCanEnterCar && CheckForJackingPedFromOutside(pSeatOccupier))
		{
			if (!EntryClipSetLoadedAndSet())
			{
				return FSM_Continue;
			}

			m_iLastCompletedState = State_Align;

			const s32 iOccupierSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pSeatOccupier);
			if (bUseAlignTransition && !m_pVehicle->IsTurretSeat(iOccupierSeatIndex))
			{
				SetTaskState(State_AlignToEnterSeat);
				return FSM_Continue;
			}
			else
			{
				SetTaskState(State_ReserveSeat);
				return FSM_Continue;
			}
		}
		else if (!bPedCanEnterCar || (!m_pJackedPed && IsFlagSet(CVehicleEnterExitFlags::JustPullPedOut)))
		{
			SetTaskState(State_UnReserveDoorToFinish);
			return FSM_Continue;
		}
		else if (bPedCanEnterCar && CheckForEnteringSeat(pSeatOccupier))
		{
			if (!EntryClipSetLoadedAndSet())
			{
				return FSM_Continue;
			}

			m_iLastCompletedState = State_Align;
			if (bUseAlignTransition)
			{
				SetTaskState(State_AlignToEnterSeat);
				return FSM_Continue;
			}
			else
			{
				SetTaskState(State_ReserveSeat);
				return FSM_Continue;
			}
		}
		else if (!m_pVehicle->InheritsFromBike())
		{
			if (m_pVehicle->GetCarDoorLocks() == CARLOCK_LOCKED_BUT_CAN_BE_DAMAGED)
			{
				m_pVehicle->SetCarDoorLocks(CARLOCK_UNLOCKED);
			}
            else
            {
				ASSERT_ONLY(aiDisplayf("Frame %u, %s ped %s quitting enter vehicle task due to unable to enter vehicle, bPedCanEnterCar = %s", fwTimer::GetFrameCount(), rPed.IsNetworkClone() ? "CLONE" : "LOCAL", rPed.GetDebugName(), bPedCanEnterCar ? "TRUE" : "FALSE");)
                SetTaskState(State_Finish);
				return FSM_Continue;
            }
		}
		else
		{	
			s32 iDesiredState = State_Finish;

			if (bPedCanEnterCar && CheckForBikeOnGround(iDesiredState))
			{
				if (CheckIfPlayerIsPickingUpMyBike())
				{
					SetTaskState(State_Finish);
					return FSM_Continue;
				}

				rPed.AttachPedToEnterCar(m_pVehicle, GetAttachToVehicleFlags(m_pVehicle), m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat), m_iTargetEntryPoint);
			}

			m_vPreviousPedPosition = VEC3V_TO_VECTOR3(rPed.GetTransform().GetPosition());
			SetTaskState(iDesiredState);
			return FSM_Continue;
		}
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::Align_OnUpdateClone()
{
    if (GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !GetSubTask())
    {
        m_iLastCompletedState = State_Align;

        // the task is allowed to go to a previous state at this point as the ped
        // has not started to get into the vehicle yet
        if(CanUpdateStateFromNetwork())
        {
			CPed* pPed = GetPed();
            if(m_iTargetEntryPoint == -1)
            {
                // if the entry point has become invalid, we have to go back to the start state
				ASSERT_ONLY(taskDebugf2("Frame : %u - %s%s stopped Enter Vehicle Move Network : CTaskEnterVehicle::Align_OnUpdateClone()", fwTimer::GetFrameCount(), pPed->IsNetworkClone() ? "Clone ped : " : "Local ped : ", pPed->GetDebugName()));
                pPed->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, NORMAL_BLEND_DURATION);

                SetTaskState(State_Start);
            }
            else
            {
				const s32 iNetworkState = GetStateFromNetwork();
				if (iNetworkState != GetState() && iNetworkState != State_VaultUp) 
				{
					pPed->AttachPedToEnterCar(m_pVehicle, GetAttachToVehicleFlags(m_pVehicle), m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat), m_iTargetEntryPoint);
					QuatV qPedOrientation = QuatVFromMat34V(pPed->GetTransform().GetMatrix());
					qPedOrientation = Normalize(qPedOrientation);

					UpdatePedVehicleAttachment(pPed, pPed->GetTransform().GetPosition(), qPedOrientation);
				}

				if (iNetworkState == State_StreamedEntry)
				{
					SetTaskState(State_WaitForStreamedEntry);
					return FSM_Continue;
				}
                else
				{
					if (iNetworkState == State_GoToDoor || iNetworkState == State_GoToVehicle)
					{
						if (pPed->GetIsAttached())
						{
							taskDisplayf("Detaching and clear clone ped's anims because going to door / vehicle");
							pPed->DetachFromParent(0);
							pPed->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, NORMAL_BLEND_DURATION);
						}
						SetTaskState(State_Start);
						return FSM_Continue;
					}

					if (!IsFlagSet(CVehicleEnterExitFlags::CombatEntry))
					{
						if (iNetworkState == State_OpenDoor && m_ShouldAlterEntryForOpenDoor)
						{
							TUNE_GROUP_BOOL(DOOR_INTERACTION_BUGS, FORCE_POP_OPEN_DOOR_FOR_CLONE, true);
							if (FORCE_POP_OPEN_DOOR_FOR_CLONE)
							{
								CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(m_pVehicle, m_iTargetEntryPoint);
								if (pDoor)
								{
									pDoor->ClearFlag(CCarDoor::DRIVEN_MASK);
									pDoor->SetFlag(CCarDoor::PROCESS_FORCE_AWAKE | CCarDoor::SET_TO_FORCE_OPEN);
								}
							}
							AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(pPed, "[EnterVehicleAlign] Clone %s is waiting in align because remote ped is opening door but we already aligned to the open door\n", AILogging::GetDynamicEntityNameSafe(pPed));
							return FSM_Continue;
						}
						else if (iNetworkState == State_EnterSeat && !m_ShouldAlterEntryForOpenDoor)
						{
							TUNE_GROUP_BOOL(DOOR_INTERACTION_BUGS, FORCE_POP_DOOR_CLOSED_FOR_CLONE, true);
							TUNE_GROUP_BOOL(DOOR_INTERACTION_BUGS, ALLOW_FORCED_CLONE_TRANSITION_TO_OPEN_DOOR, false);
							if (ALLOW_FORCED_CLONE_TRANSITION_TO_OPEN_DOOR)
							{
								if (FORCE_POP_DOOR_CLOSED_FOR_CLONE)
								{
									CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(m_pVehicle, m_iTargetEntryPoint);
									if (pDoor)
									{
										pDoor->SetShutAndLatched(m_pVehicle, false);
									}
								}
								AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(pPed, "[%s] Clone %s is being forced to stream anims for open door in align because remote ped is entering seat but we already aligned to the closed door\n", GetTaskName(), AILogging::GetDynamicEntityNameSafe(pPed));
								SetTaskState(State_StreamAnimsToOpenDoor);
								return FSM_Continue;
							}
						}
					}

					if (iNetworkState == State_CloseDoor)
					{
						AI_LOG_WITH_ARGS("[EnterVehicle] %s ped %s : 0x%p in %s:%s : 0x%p is being forced to State_WaitForPedToLeaveSeat because they aren't in the vehicle and are requesting State_CloseDoor\n", AILogging::GetDynamicEntityIsCloneStringSafe(pPed), AILogging::GetDynamicEntityNameSafe(pPed), pPed, GetTaskName(), GetStateName(GetState()), this);
						m_iCurrentSeatIndex = m_iSeat;
						SetTaskState(State_WaitForPedToLeaveSeat);
						return FSM_Continue;
					}
					
					AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(pPed, "[%s] Clone %s is changing task state to %s\n", GetTaskName(), AILogging::GetDynamicEntityNameSafe(pPed), GetStaticStateName(iNetworkState));
					SetTaskState(iNetworkState);
					return FSM_Continue;
				}
            }
        }
    }

    return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::Align_OnExitClone()
{
	CTaskVehicleFSM::SetRemotePedUsingDoorsFalse(*GetPed(), m_pVehicle, m_iTargetEntryPoint);
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::WaitForStreamedEntry_OnUpdate()
{
	if (m_StreamedClipSetId.IsNotNull() && m_StreamedClipId.IsNotNull())
	{
		fwClipSet* pClipSet = fwClipSetManager::GetClipSet(m_StreamedClipSetId);
		if (taskVerifyf(pClipSet, "Couldn't find clipset %s", m_StreamedClipSetId.TryGetCStr() ? m_StreamedClipSetId.GetCStr() : "Null"))
		{
			if (pClipSet->Request_DEPRECATED())
			{
				SetTaskState(State_StreamedEntry);
				return FSM_Continue;
			}
		}
		else
		{
			SetTaskState(State_Finish);
			return FSM_Continue;
		}
	}

	if (GetPed()->IsNetworkClone())
	{
		s32 iNetworkState = GetStateFromNetwork();
		if (iNetworkState != GetState() && iNetworkState != State_StreamedEntry)
		{
			SetTaskState(iNetworkState);
			return FSM_Continue;
		}
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::StreamedEntry_OnEnter()
{
	CPed& rPed = *GetPed();
	StoreInitialOffsets();
	rPed.AttachPedToEnterCar(m_pVehicle, GetAttachToVehicleFlags(m_pVehicle), m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat), m_iTargetEntryPoint);
	SetMoveNetworkState(rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_WillArrestRatherThanJack) ? ms_ArrestRequestId : ms_StreamedEntryRequestId, ms_StreamedEntryOnEnterId, ms_StreamedEntryStateId);

	CPed* pSeatOccupier = GetPedOccupyingDirectAccessSeat();
	const crClip* pClip = SetClipsForState();

	if (!pClip)
	{
		return FSM_Quit;
	}

	if (pSeatOccupier && !IsSeatOccupierShuffling(pSeatOccupier))
	{
		// Make the ped get out
		if (pSeatOccupier && !pSeatOccupier->IsNetworkClone())
		{
			SetRunningFlag(CVehicleEnterExitFlags::HasJackedAPed);
			fwMvClipSetId clipsetId;
			fwMvClipId clipId;
			u32 uPairedClipSetHash = 0;
			u32 uPairedClipHash = 0;
			if (taskVerifyf(pClip, "Couldn't find clip with id %s", m_StreamedClipId.GetCStr()) && taskVerifyf(CClipEventTags::FindPairedClipAndClipSetNameHashes(pClip, uPairedClipSetHash, uPairedClipHash), "Couldn't find paired clip name in clip %s from clipset %s", clipId.GetCStr(), clipsetId.GetCStr()))
			{
				fwMvClipSetId pairedClipSetId = fwMvClipSetId(uPairedClipSetHash);
				fwMvClipId pairedClipId = fwMvClipId(uPairedClipHash);
				if (taskVerifyf(fwClipSetManager::GetClipSet(pairedClipSetId), "Paired clip set id %s is invalid, is the PairedClipSet tag specified in this anim?", pairedClipSetId.GetCStr()) &&
					taskVerifyf(fwClipSetManager::GetClip(pairedClipSetId, pairedClipId), "Paired clip id %s is invalid, is the PairedClip tag specified in this anim?", pairedClipId.GetCStr()))
				{
					clipsetId = pairedClipSetId;
					clipId = pairedClipId;
				}
			}

			SetClipPlaybackRate();
			VehicleEnterExitFlags vehicleFlags;
			vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::BeJacked);
			vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::IsStreamedExit);
			if(rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_WillArrestRatherThanJack))
			{
				vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::IsArrest);
			}
			CTaskExitVehicle* pExitVehicleTask = rage_new CTaskExitVehicle(m_pVehicle, vehicleFlags, 0.0f, &rPed, m_iTargetEntryPoint);
			pExitVehicleTask->SetStreamedExitClipsetId(clipsetId);
			pExitVehicleTask->SetStreamedExitClipId(clipId);
			const s32 iEventPriority = CTaskEnterVehicle::GetEventPriorityForJackedPed(*pSeatOccupier);
			CEventGivePedTask givePedTask(PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP, pExitVehicleTask, true, iEventPriority );
			pSeatOccupier->GetPedIntelligence()->AddEvent(givePedTask);

#if __BANK
			CGivePedJackedEventDebugInfo instance(pSeatOccupier, this, __FUNCTION__);
			instance.Print();
#endif // __BANK
		}
	    else
	    {
		    m_moveNetworkHelper.SetFloat(ms_JackRateId, 1.0f);
	    }
	}
	else
	{
		m_moveNetworkHelper.SetFloat(ms_JackRateId, 1.0f);
	}

	m_bTurnedOnDoorHandleIk = false;
	m_bTurnedOffDoorHandleIk = false;
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::StreamedEntry_OnUpdate()
{
	CPed& rPed = *GetPed();
	float fPhase = 0.0f;
	const crClip* pClip = GetClipAndPhaseForState(fPhase);
	CTaskVehicleFSM::ProcessLegIkBlendOut(rPed, m_fLegIkBlendOutPhase, fPhase);

	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	const CVehicleEntryPointAnimInfo* pClipInfo = m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint);

	if (pClip)
	{
		CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(m_pVehicle, m_iTargetEntryPoint);
		if (pDoor)
		{
			CTaskEnterVehicle::ProcessSmashWindow(*m_pVehicle, rPed, m_iTargetEntryPoint, *pClip, fPhase, m_moveNetworkHelper, m_bSmashedWindow, false);

			if (m_pStreamedEntryClipSet && m_pStreamedEntryClipSet->GetReplaceStandardJack())
			{
				// Keep doors open
				CTaskVehicleFSM::ProcessOpenDoor(*pClip, 1.0f, *m_pVehicle, m_iTargetEntryPoint, false);
			}
			else
			{
				static dev_float sfPhaseToBeginOpenDoor = 0.5f;
				static dev_float sfPhaseToEndOpenDoor = 1.0f;

				float fPhaseToBeginOpenDoor = sfPhaseToBeginOpenDoor;
				float fPhaseToEndOpenDoor = sfPhaseToEndOpenDoor;

				CTaskOpenVehicleDoorFromOutside::ProcessOpenDoorHelper(this, pClip, pDoor, fPhase, fPhaseToBeginOpenDoor, fPhaseToEndOpenDoor, m_bTurnedOnDoorHandleIk, m_bTurnedOffDoorHandleIk, true);
			}
		}

		float fDestroyWeaponPhase = 1.0f;
		if (CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pClip, CClipEventTags::Object, CClipEventTags::Destroy, true, fDestroyWeaponPhase))
		{
			if (fPhase >= fDestroyWeaponPhase && rPed.GetWeaponManager()->GetEquippedWeaponObject())
			{
				rPed.GetWeaponManager()->DestroyEquippedWeaponObject();
			}
		}

		if(pClipInfo && IsFlagSet(CVehicleEnterExitFlags::HasJackedAPed))
		{
			const bool bPreventInterrupting = pClipInfo->GetPreventJackInterrupt();
			if (m_fInterruptPhase < 0.0f && !bPreventInterrupting)
			{
				if (!pClipInfo->GetJackIncludesGetIn())
				{
					m_fInterruptPhase = 1.0f;
				}

				float fUnused;
				CClipEventTags::GetMoveEventStartAndEndPhases(pClip, ms_InterruptId.GetHash(), fUnused, m_fInterruptPhase);

#if __DEV
				const bool bJackedPedIsDead = m_pJackedPed && m_pJackedPed->ShouldBeDead();
				if ((!bJackedPedIsDead && pClipInfo->GetJackIncludesGetIn()) || (bJackedPedIsDead && pClipInfo->GetDeadJackIncludesGetIn()))
				{
					taskAssertf(m_fInterruptPhase < 1.0f, "ADD A BUG TO DEFAULT ANIM INGAME - Vehicle with entry anim info %s has an Interrupt tag with end phase 1.0, this is likely an error as the jack contains the get in, jack anim %s", pClipInfo->GetName().GetCStr(), pClip->GetName());
					taskAssertf(CClipEventTags::HasMoveEvent(pClip, ms_InterruptId.GetHash()), "ADD A BUG TO DEFAULT ANIM INGAME - Vehicle with entry anim info %s is missing Interrupt tag on jack anim %s", pClipInfo->GetName().GetCStr(), pClip->GetName());
				}
#endif // __DEV
			}
		}
	}

	if (CTaskVehicleFSM::ProcessApplyForce(m_moveNetworkHelper, *m_pVehicle, *GetPed()))
	{
		SetRunningFlag(CVehicleEnterExitFlags::DontApplyForceWhenSettingPedIntoVehicle);
	}

	if (rPed.IsLocalPlayer() && m_moveNetworkHelper.GetBoolean(ms_InterruptId) && CheckPlayerExitConditions(rPed, *m_pVehicle, m_iRunningFlags, GetTimeRunning()))
	{
		SetTaskState(State_UnReserveDoorToFinish);
		return FSM_Continue;
	}

	if (IsFlagSet(CVehicleEnterExitFlags::HasJackedAPed) && !rPed.IsNetworkClone())
	{
		const CEntryExitPoint* pEntryPoint = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint);
		if (pEntryPoint)
		{
			// If we're going to close the door, do it asap
			if (CTaskEnterVehicle::CheckForCloseDoorInterrupt(rPed, *m_pVehicle, m_moveNetworkHelper, pEntryPoint, false, m_iTargetEntryPoint)
				|| CTaskEnterVehicle::CheckForShuffleInterrupt(rPed, m_moveNetworkHelper, pEntryPoint, *this))
			{
				// We're entering the direct access seat, store it here so we can pass it to the set ped in seat task
				m_iCurrentSeatIndex = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat, m_iSeat);
				taskAssert(m_iCurrentSeatIndex > -1);
				m_iLastCompletedState = State_StreamedEntry;
				SetTaskState(State_WaitForPedToLeaveSeat);
				return FSM_Continue;
			}
		}
	}

	// We don't want to quit the task if we are arrested the jacked target and they successfully got marked as arrested
	if (rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_WillArrestRatherThanJack) && m_pTargetPed )
	{
		if(m_pTargetPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE, true))
		{
			return FSM_Continue;
		}
		else if(GetTimeInState() > GetTimeStep())
		{
			return FSM_Quit;
		}
	}

	if (IsMoveNetworkStateFinished(ms_StreamedEntryClipFinishedId, ms_StreamedEntryPhaseId))
	{
		// We're entering the direct access seat, store it here so we can pass it to the set ped in seat task
		m_iCurrentSeatIndex = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat);
		taskAssert(m_iCurrentSeatIndex > -1);
		m_iLastCompletedState = State_StreamedEntry;

		if (m_pStreamedEntryClipSet && m_pStreamedEntryClipSet->GetReplaceStandardJack())
		{
			if (CheckPlayerExitConditions(rPed, *m_pVehicle, m_iRunningFlags, GetTimeRunning()))
			{
				SetTaskState(State_UnReserveDoorToFinish);
				return FSM_Continue;
			}
			else
			{
				if (pClipInfo && pClipInfo->GetJackVariationsIncludeGetIn())
				{		
					SetTaskState(State_WaitForPedToLeaveSeat);
					return FSM_Continue;
				}
				else
				{		
					SetTaskState(State_StreamAnimsToEnterSeat);
					return FSM_Continue;
				}
			}
		}
		else
		{
			SetTaskState(State_WaitForPedToLeaveSeat);
			return FSM_Continue;
		}
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::ClimbUp_OnEnter()
{
	const CVehicleEntryPointAnimInfo* pAnimInfo = m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint) ? m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint) : NULL;
	if(pAnimInfo && pAnimInfo->GetHasClimbUpFromWater())
	{
		bool bInWater = IsFlagSet(CVehicleEnterExitFlags::InWater);
		m_moveNetworkHelper.SetFlag(bInWater, ms_ClimbUpFromWaterId);
	}

	InitExtraGetInFixupPoints();
	SetRunningFlag(CVehicleEnterExitFlags::UsedClimbUp);

	m_bAnimFinished = false;
	RequestProcessMoveSignalCalls();
	StoreInitialOffsets();
	SetMoveNetworkState(ms_ClimbUpRequestId, ms_ClimbUpOnEnterId, ms_ClimbUpStateId);
	SetClipsForState();
	SetClipPlaybackRate();

	if(m_pVehicle->HasWaterEntry() && GetPed()->GetWeaponManager() && GetPed()->GetWeaponManager()->GetEquippedWeaponObject())
	{
		GetPed()->GetWeaponManager()->DestroyEquippedWeaponObject();
	}

	GetPed()->SetIsSwimming(false);

	CTaskVehicleFSM::SetRemotePedUsingDoorsTrue(*GetPed(), m_pVehicle, m_iTargetEntryPoint);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::ClimbUp_OnUpdate()
{
	CPed& rPed = *GetPed();

	ProcessDoorSteps();

	float fPhase = 0.0f;
	const crClip *pClip = GetClipAndPhaseForState(fPhase);
	CTaskVehicleFSM::ProcessLegIkBlendOut(rPed, m_fLegIkBlendOutPhase, fPhase);

	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	UpdateExtraGetInFixupPoints(pClip,fPhase);

	if (CanUnreserveDoorEarly(rPed, *m_pVehicle))
	{
		UnreserveDoor(&rPed);
	}

	if (m_bAnimFinished)
	{
		m_iLastCompletedState = State_ClimbUp;

        bool bCanOpenDoor = true;

        if(rPed.IsNetworkClone())
        {
            s32 iNetworkState = GetStateFromNetwork();

            // don't let the clone get ahead of the local owner
            if(iNetworkState == State_ClimbUp)
            {
                return FSM_Continue;
            }

            bCanOpenDoor = (iNetworkState == State_OpenDoor);
        }

		const CVehicleEntryPointAnimInfo* pAnimInfo = m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint) ? m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint) : NULL;
		const bool bUseAlignTransition = !m_pVehicle->InheritsFromBike() && pAnimInfo && pAnimInfo->GetUsesNoDoorTransitionForEnter();
		CPed* pSeatOccupier = GetPedOccupyingDirectAccessSeat();
		bool bJackingPedFromOutside = CheckForJackingPedFromOutside(pSeatOccupier);
		const CVehicleSeatAnimInfo* pSeatAnimInfo = m_pVehicle->GetSeatAnimationInfo(m_iSeat);

		if (ShouldFinishAfterClimbUp(rPed))
		{
			SetTaskFinishState(TFR_SHOULD_FINISH_AFTER_CLIMBUP, VEHICLE_DEBUG_DEFAULT_FLAGS);
			return FSM_Continue;
		}
		else if (pSeatOccupier && !bJackingPedFromOutside)
		{
			bool bOccupierIsShufflingToDifferentSeat = false;
			bool bIsBombushka = MI_PLANE_BOMBUSHKA.IsValid() && m_pVehicle->GetModelIndex() == MI_PLANE_BOMBUSHKA;
			bool bIsVolatol = MI_PLANE_VOLATOL.IsValid() && m_pVehicle->GetModelIndex() == MI_PLANE_VOLATOL;
			if (bIsBombushka || bIsVolatol)
			{
				if (const CTaskInVehicleSeatShuffle* pShuffleTask = static_cast<const CTaskInVehicleSeatShuffle*>(pSeatOccupier->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE)))
				{
					s32 sShuffleTargetSeat = pShuffleTask->GetTargetSeatIndex();
					if (m_pVehicle->IsSeatIndexValid(sShuffleTargetSeat) && sShuffleTargetSeat != m_iSeat)
					{
						bOccupierIsShufflingToDifferentSeat = true;
					}
				}
			}

			if (pSeatOccupier->GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle) || bOccupierIsShufflingToDifferentSeat)
			{
				SetTaskState(State_StreamAnimsToEnterSeat);
				return FSM_Continue;
			}
#if __BANK
			s32 iDirectSeat = m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint) ? m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat, m_iSeat) : -1;
			AI_LOG_WITH_ARGS("Seat occupier = %s, target entry = %i, target seat = %i, direct seat = %i\n", AILogging::GetDynamicEntityNameSafe(pSeatOccupier), m_iTargetEntryPoint, m_iSeat, iDirectSeat);
#endif // __BANK
			SetTaskFinishState(TFR_OTHER_PED_USING_SEAT, VEHICLE_DEBUG_DEFAULT_FLAGS);
			return FSM_Continue;
		}
		else if (bCanOpenDoor && CheckForOpenDoor() && !ShouldUseClimbUpAfterOpenDoor(m_iTargetEntryPoint, m_pVehicle))
		{
			SetTaskState(State_OpenDoor);
			return FSM_Continue;
		}
		else if (bUseAlignTransition && !ShouldUseClimbUpAfterOpenDoor(m_iTargetEntryPoint, m_pVehicle))
		{
			if (!EntryClipSetLoadedAndSet())
			{
				return FSM_Continue;
			}

			SetTaskState(State_AlignToEnterSeat);
			return FSM_Continue;
		}
		else if (bJackingPedFromOutside)
		{
			SetTaskState(State_JackPedFromOutside);
			return FSM_Continue;
		}
		else if(pAnimInfo->GetUseNavigateToSeatFromClimbUp())
		{
			SetTaskState(State_GoToSeat);
			return FSM_Continue;
		}
		else if (pSeatAnimInfo && pSeatAnimInfo->IsStandingTurretSeat())
		{
			StoreInitialReferencePosition(&rPed);
			SetTaskState(State_StreamAnimsToAlign);
			return FSM_Continue;
		}
		else if (!pSeatOccupier || CheckForEnteringSeat(pSeatOccupier))
		{
			SetTaskState(State_StreamAnimsToEnterSeat);
			return FSM_Continue;
		}
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::ClimbUp_OnExit()
{
	CTaskVehicleFSM::SetRemotePedUsingDoorsFalse(*GetPed(), m_pVehicle, m_iTargetEntryPoint);
	return FSM_Continue;
}

CTask::FSM_Return CTaskEnterVehicle::VaultUp_OnEnter()
{
	CPed* pPed = GetPed();

	const CBaseCapsuleInfo* pCapsuleInfo = pPed->GetCapsuleInfo();
	taskAssert(pCapsuleInfo);

	ResetRagdollOnCollision(*pPed);

	//! Detach as the vault code will do that for us & we have more control over what happens here. E.g. we can skip
	//! safe position check. Note: Disabling collision here is fine as the vault code will do that anyway.
	pPed->DetachFromParent(DETACH_FLAG_EXCLUDE_VEHICLE | DETACH_FLAG_IGNORE_SAFE_POSITION_CHECK | DETACH_FLAG_NO_COLLISION_UNTIL_CLEAR);

	const CBipedCapsuleInfo* pPedCapsuleInfo = pCapsuleInfo ? pCapsuleInfo->GetBipedCapsuleInfo() : 0;
	if (taskVerifyf(pPedCapsuleInfo, "Vaulting requires biped information - this capsule is not a biped type"))
	{
		Vector3 vTargetPos(Vector3::ZeroType);
		Quaternion qTargetRot(0.0f,0.0f,0.0f,1.0f);

		fwFlags8 iVaultFlags;
		iVaultFlags.SetFlag(VF_DontDisableVaultOver);
		iVaultFlags.SetFlag(VF_CloneUseLocalHandhold);

		//! Ask the climb detector if it can find a handhold.
		CClimbHandHoldDetected handHoldDetected;
		pPed->SetPedResetFlag(CPED_RESET_FLAG_SearchingForClimb, true);
		pPed->GetPedIntelligence()->GetClimbDetector().Process();
		if(pPed->GetPedIntelligence()->GetClimbDetector().GetDetectedHandHold(handHoldDetected) /*&& (handHoldDetected.GetPhysical() == m_pVehicle)*/ )
		{
			SetNewTask(rage_new CTaskVault(handHoldDetected, iVaultFlags));
		}
		else
		{
			//! Use hard-coded vault point if we can't find one.
			if(CTaskVehicleFSM::ComputeTargetTransformInWorldSpace(*m_pVehicle, vTargetPos, qTargetRot, m_iTargetEntryPoint, CExtraVehiclePoint::VAULT_HAND_HOLD))
			{
				// Use the heading of the node as the norm of the handhold
				Vector3 vNorm(0.0f,1.0f,0.0f);
				qTargetRot.Transform(vNorm);
				CClimbHandHold handHold;
				handHold.Set(vTargetPos, vNorm);
				CClimbHandHoldDetected handHoldDetected;
				handHoldDetected.SetHandHold(handHold);
				handHoldDetected.SetPhysical(m_pVehicle, 0);
				handHoldDetected.CalcPhysicalHandHoldOffset();
				handHoldDetected.SetDepth(ms_Tunables.m_VaultDepth);

				float fHeight = Max(vTargetPos.z - (GetPed()->GetTransform().GetPosition().GetZf() - pCapsuleInfo->GetGroundToRootOffset()), 0.0f);

				handHoldDetected.SetHeight(fHeight);
				handHoldDetected.SetHorizontalClearance(ms_Tunables.m_VaultHorizClearance);
				handHoldDetected.SetVerticalClearance(ms_Tunables.m_VaultVertClearance);
				handHoldDetected.SetDrop(0.0f);
				SetNewTask(rage_new CTaskVault(handHoldDetected, iVaultFlags));
			}
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::VaultUp_OnUpdate()
{
	if (!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetRunningFlag(CVehicleEnterExitFlags::EnteringOnVehicle);

		if(!m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint))
		{
			Assertf(0, "Ped (%s) has lost target entry point. entry point id: %d seat: %d", GetPed()->GetDebugName(), m_iTargetEntryPoint, m_iSeat);
			SetTaskState(State_Finish);
		}
		else
		{
			const CVehicleEntryPointAnimInfo* pClipInfo = m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint);

			if(m_pVehicle->GetIsAquatic() && pClipInfo->GetHasOnVehicleEntry())
			{
				CPed *pPed = GetPed();

				if(!pPed->IsNetworkClone())
				{
					//! Go back to picking a door, so that we can pick the correct seat to get into.
					SetTaskState(State_PickDoor);
				}
				else if(!pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_VAULT))
				{
					m_iLastCompletedState = State_VaultUp;
					m_iNetworkWaitState   = State_VaultUp;
					SetTaskState(State_WaitForNetworkSync);
				}
			}
			else
			{
				SetTaskState(State_Align);
			}
		}
		return FSM_Continue;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::OpenDoor_OnEnter(CPed& rPed)
{
	fwMvClipSetId entryClipsetId = CTaskEnterVehicleAlign::GetEntryClipsetId(m_pVehicle, m_iTargetEntryPoint, &rPed);

	if (taskVerifyf(CTaskVehicleFSM::IsVehicleClipSetLoaded(entryClipsetId), "Clipset %s wasn't streamed before open door!", entryClipsetId.GetCStr()))
	{
		m_moveNetworkHelper.SetClipSet(entryClipsetId);
	}

	const bool bFromOnVehicle = IsFlagSet(CVehicleEnterExitFlags::EnteringOnVehicle);
	m_moveNetworkHelper.SetFloat(ms_OpenDoorBlendInDurationId, bFromOnVehicle ? ms_Tunables.m_OpenDoorBlendDurationFromOnVehicleAlign : ms_Tunables.m_OpenDoorBlendDurationFromNormalAlign);

	SetNewTask(rage_new CTaskOpenVehicleDoorFromOutside(m_pVehicle, m_iTargetEntryPoint, m_moveNetworkHelper));
	m_bWantsToGoIntoCover = false;
	//@@: location CTASKENTERVEHICLE_OPENDOOR_ONENTER_SETUP_RAGDOLL_COLLISION
	SetUpRagdollOnCollision(rPed, *m_pVehicle);
	if(rPed.GetSpeechAudioEntity())
	{
		if(rPed.IsLocalPlayer())
			rPed.GetSpeechAudioEntity()->SetOpenedDoor();
		//This gets called, but returns quickly if the ped has not been marked as jacking
		rPed.GetSpeechAudioEntity()->TriggerJackingSpeech();
		
	}

	// Abort door handle look at.
	rPed.GetIkManager().AbortLookAt(250, m_uEnterVehicleLookAtHash);

	if(rPed.IsLocalPlayer() && m_pVehicle && m_pVehicle->GetVehicleAudioEntity())
	{
		m_pVehicle->GetVehicleAudioEntity()->OnPlayerOpenDoor();
	}

	if (NetworkInterface::IsGameInProgress())
	{
		m_bCouldEnterVehicleWhenOpeningDoor = m_pVehicle->CanPedEnterCar(&rPed);
	}

	m_bDoingExtraGetInFixup = false;

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::OpenDoor_OnUpdate()
{
	const bool bReservationsComplete = ProcessComponentReservations();
	CPed* pPed = GetPed();

#if __BANK
	if (!bReservationsComplete && (pPed->IsLocalPlayer() || pPed->PopTypeIsMission()))
	{
		AI_LOG_WITH_ARGS("Ped %s is in open door state, reservations not complete\n", AILogging::GetDynamicEntityNameSafe(pPed));
	}
#endif // __BANK

	if (!bReservationsComplete)
	{
		if (CTaskVehicleFSM::AnotherPedHasDoorReservation(*m_pVehicle, *pPed, m_iTargetEntryPoint))
		{
			AI_LOG_WITH_ARGS("[EnterVehicle] - Ped %s is aborting open door because another ped (%s) has door reservation", AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetDynamicEntityNameSafe(CTaskVehicleFSM::GetPedThatHasDoorReservation(*m_pVehicle, m_iTargetEntryPoint)));
			SetTaskState(State_UnReserveDoorToFinish);
			return FSM_Continue;
		}
	}

	// If someone is getting out of the seat we're entering abort
	CPed* pOccupyingPed = GetPedOccupyingDirectAccessSeat();
	if (pOccupyingPed && pOccupyingPed->GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle)
		&& !IsSeatOccupierShuffling(pOccupyingPed))
	{
		SetTaskState(State_UnReserveDoorToFinish);
		return FSM_Continue;
	}

	// If we're doing the combat entry we don't ensure the door is reserved beforehand because we don't go through the align
	// we do however keep trying to reserve the door during the open
	// If we detect someone else has the door reservation, bail out B*1636359
	if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_OPEN_VEHICLE_DOOR_FROM_OUTSIDE)
	{
		if (GetSubTask()->GetState() == CTaskOpenVehicleDoorFromOutside::State_OpenDoorCombat &&
			m_pVehicle && m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint))
		{
			CComponentReservation* pComponentReservation = m_pVehicle->GetComponentReservationMgr()->GetDoorReservation(m_iTargetEntryPoint);
			if (pComponentReservation && pComponentReservation->GetPedUsingComponent() && pComponentReservation->GetPedUsingComponent() != pPed)
			{
				SetTaskFinishState(TFR_OTHER_PED_USING_DOOR, VEHICLE_DEBUG_DEFAULT_FLAGS | VEHICLE_DEBUG_PRINT_DOOR_RESERVATION);
				return FSM_Continue;
			}
		}
	}

	pPed->SetPedResetFlag(CPED_RESET_FLAG_RequiresLegIk, true);
	pPed->GetIkManager().SetFlag(PEDIK_LEGS_AND_PELVIS_FADE_ON);

	static dev_float MAX_TIME_TO_OPEN_DOOR = 8.0f;	// Sigh, truck forced entry anim is 7.4 seconds long...
	if (GetSubTask() && !taskVerifyf(GetSubTask()->GetTimeInState() < MAX_TIME_TO_OPEN_DOOR, "Ped %s stuck in open door state for longer than %.2f seconds, quitting out, movenetwork %s attached", pPed->GetDebugName() ? pPed->GetDebugName() : "NULL", MAX_TIME_TO_OPEN_DOOR, m_moveNetworkHelper.IsNetworkAttached() ? "WAS" : "WAS NOT"))
	{
		SetTaskState(State_UnReserveDoorToFinish);
		return FSM_Continue;
	}

	if (pPed->IsLocalPlayer())
	{
		CControl* pControl = pPed->GetControlFromPlayer();
		if (!m_bWantsToGoIntoCover)
		{
			m_bWantsToGoIntoCover = pControl && pControl->GetPedCover().IsPressed();
		}	

		if (m_pVehicle->IsAlarmActivated())
		{
			CMiniMap::CreateSonarBlipAndReportStealthNoise(pPed, m_pVehicle->GetTransform().GetPosition(), CMiniMap::sm_Tunables.Sonar.fSoundRange_CarHorn, HUD_COLOUR_BLUEDARK);
		}
	}

	if (ShouldTriggerRagdollDoorGrab() 
		&& !NetworkInterface::IsGameInProgress()
		&& !(CTaskNMBehaviour::ms_bDisableBumpGrabForPlayer && pPed->IsPlayer())
		&& CTaskTryToGrabVehicleDoor::IsTaskValid(m_pVehicle, pPed, m_iTargetEntryPoint) 
		&& CTaskTryToGrabVehicleDoor::IsWithinGrabbingDistance(pPed, m_pVehicle, m_iTargetEntryPoint))
	{
		SetTaskState(State_TryToGrabVehicleDoor);
		return FSM_Continue;
	}
	else if (ShouldAbortBecauseVehicleMovingTooFast() && GetPreviousState() != State_ClimbUp)
	{
		if (IsFlagSet(CVehicleEnterExitFlags::ResumeIfInterupted))
		{
			UnreserveDoor(pPed);
			SetTaskState(State_Start);
			return FSM_Continue;
		}
		else
		{
			SetTaskState(State_UnReserveDoorToFinish);
			return FSM_Continue;
		}
	}

	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !GetSubTask())
	{
        m_iLastCompletedState = State_OpenDoor;

		bool bFinishedAnim = true;
		if (GetSubTask())
		{
			bFinishedAnim = static_cast<CTaskOpenVehicleDoorFromOutside*>(GetSubTask())->GetAnimFinished();
		}

		const bool bPedCanEnterVehicle = m_pVehicle->CanPedEnterCar(pPed);
		bool bCanInterruptAfterOpenDoor = !bPedCanEnterVehicle || !m_pVehicle->GetLayoutInfo()->GetPreventInterruptAfterOpenDoor();
		const bool bDidForcedEntry = GetSubTask() && GetSubTask()->GetPreviousState() == CTaskOpenVehicleDoorFromOutside::State_ForcedEntry;
		if (bDidForcedEntry)
		{
			const CVehicleEntryPointAnimInfo* pClipInfo = m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint);
			if (pClipInfo && pClipInfo->GetForcedEntryIncludesGetIn())
			{
				bCanInterruptAfterOpenDoor = false;
			}
		}

		if (NetworkInterface::IsGameInProgress() && !m_bCouldEnterVehicleWhenOpeningDoor && bCanInterruptAfterOpenDoor && !pPed->IsNetworkClone())
		{
			SetTaskState(State_UnReserveDoorToFinish);
			return FSM_Continue;
		}

		// We just tried a locked door, now quit
		if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_OPEN_VEHICLE_DOOR_FROM_OUTSIDE)
		{
			if (GetSubTask()->GetPreviousState() == CTaskOpenVehicleDoorFromOutside::State_TryLockedDoor)
			{
				SetTaskState(State_UnReserveDoorToFinish);
				return FSM_Continue;
			}
		}

		// See if we can jack the ped in the seat
		CPed* pSeatOccupier = NULL;
		CheckForJackingPedFromOutside(pSeatOccupier);

		// Check to see if the ped wants to quit entering
		if (bCanInterruptAfterOpenDoor &&
			(IsFlagSet(CVehicleEnterExitFlags::JustOpenDoor) || 
			!bPedCanEnterVehicle || 
			CheckPlayerExitConditions(*pPed, *m_pVehicle, m_iRunningFlags, GetTimeRunning()) ||
			(!m_pJackedPed && IsFlagSet(CVehicleEnterExitFlags::JustPullPedOut)) ||
			m_bWantsToGoIntoCover ||
			!bFinishedAnim))
		{
			if (m_bWantsToGoIntoCover)
			{
				pPed->SetPedResetFlag(CPED_RESET_FLAG_WantsToEnterCover, true);
				SetFlag(aiTaskFlags::ProcessNextActiveTaskImmediately);
			}

			pPed->DetachFromParent(DETACH_FLAG_EXCLUDE_VEHICLE);
			SetTaskState(State_UnReserveDoorToFinish);
			return FSM_Continue;
		}

		// Check if we should play a streamed entry anim (this replaces the open door/enter seat/jacking states)
		if (!bDidForcedEntry && CheckForClimbUp())
		{
			if (pPed->IsNetworkClone())
			{
				m_iNetworkWaitState = GetState();
				SetTaskState(State_WaitForNetworkSync);
				return FSM_Continue;
			}
			else
			{
				// TODO PB: url:bugstar:6192502, too late in the pack to fix for all climbUp vehicles, so restricting zhaba. Open up for all in future
				bool bCheckReservationsComplete = MI_CAR_ZHABA.IsValid() && m_pVehicle->GetModelIndex() == MI_CAR_ZHABA && m_iTargetEntryPoint != 4 && m_iTargetEntryPoint != 5;
				if (bCheckReservationsComplete)
				{
					if (!bReservationsComplete)
					{
						if (ShouldReEvaluateBecausePedEnteredOurSeatWeCantJack())
						{
							pPed->DetachFromParent(0);
							pPed->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, SLOW_BLEND_DURATION);
							SetTaskState(State_Start);
							return FSM_Continue;
						}
						else
						{
							SetTaskState(State_UnReserveDoorToFinish);
							return FSM_Continue;
						}
					}
				}

				fwMvClipSetId entryClipsetId = CTaskEnterVehicleAlign::GetEntryClipsetId(m_pVehicle, m_iTargetEntryPoint, pPed);
				if (CTaskVehicleFSM::IsVehicleClipSetLoaded(entryClipsetId))
				{
					m_moveNetworkHelper.SetClipSet(entryClipsetId);
					m_iLastCompletedState = State_OpenDoor;
					SetTaskState(State_ClimbUp);
				}
				return FSM_Continue;
			}
		}
		else if (CheckForStreamedEntry())
		{
			SetTaskState(State_WaitForStreamedEntry);
			return FSM_Continue;
		}
		else
		{
			// If there is a ped in the seat, jack the ped
			CPed* pSeatOccupier = NULL;
			if (bReservationsComplete && CheckForJackingPedFromOutside(pSeatOccupier))
			{
				SetTaskState(State_JackPedFromOutside);
				return FSM_Continue;
			}
			// If we can reserve the seat and theres no one in it, then enter it
            else if (CheckForEnteringSeat(pSeatOccupier))
            {
				if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_OPEN_VEHICLE_DOOR_FROM_OUTSIDE)
				{
					if (GetSubTask()->GetPreviousState() == CTaskOpenVehicleDoorFromOutside::State_ForcedEntry)
					{
						const CVehicleEntryPointAnimInfo* pClipInfo = m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint);
						if (pClipInfo)
						{
							if (pClipInfo->GetForcedEntryIncludesGetIn())
							{
								// We're entering the direct access seat, store it here so we can pass it to the set ped in seat task
								m_iCurrentSeatIndex = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat);
								taskAssert(m_iCurrentSeatIndex > -1);
								m_iLastCompletedState = State_OpenDoor;
								SetTaskState(State_WaitForPedToLeaveSeat);
								return FSM_Continue;
							}
						}
					}
				}
				SetTaskState(State_StreamAnimsToEnterSeat);
                return FSM_Continue;
            }
			// If there is still a seat occupier at this point we have to wait for them to shuffle
			else if (IsSeatOccupierShuffling(pSeatOccupier))
			{
				// Wait til its free and we can reserve the seat
				return FSM_Continue;
			}
			else if (!bReservationsComplete)
			{
				if (ShouldReEvaluateBecausePedEnteredOurSeatWeCantJack())
				{
					pPed->DetachFromParent(0);
					pPed->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, SLOW_BLEND_DURATION);
					SetTaskState(State_Start);
					return FSM_Continue;
				}

				// Continue trying to reserve, but allow player to pull away
				if (CheckPlayerExitConditions(*pPed, *m_pVehicle, m_iRunningFlags, GetTimeRunning()))
				{
					SetTaskState(State_UnReserveDoorToFinish);
					return FSM_Continue;
				}
				return FSM_Continue;
			}
			// Someone is using the seat that we can't displace, quit out
			else
			{
				SetTaskState(State_UnReserveDoorToFinish);
				return FSM_Continue;
			}
		}
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::OpenDoor_OnUpdateClone()
{
    if (GetStateFromNetwork() == State_TryToGrabVehicleDoor)
	{
		SetTaskState(State_TryToGrabVehicleDoor);
		return FSM_Continue;
	}
	else if (GetStateFromNetwork() == State_Finish)
	{
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	// This task can return to a previous state (such as going to the door)
    // after the door has been opened
    if( GetIsSubtaskFinished(CTaskTypes::TASK_OPEN_VEHICLE_DOOR_FROM_OUTSIDE) )
	{
        m_iLastCompletedState = State_OpenDoor;

        CPed* pPed = GetPed();

        if (!pPed->IsNetworkClone() && (IsFlagSet(CVehicleEnterExitFlags::JustOpenDoor) || !m_pVehicle->CanPedEnterCar(pPed)))
        {
            SetTaskState(State_Finish);
        }
        else
        {
			CPed* pSeatOccupier = NULL;
			const bool bDidForcedEntry = GetSubTask() && GetSubTask()->GetPreviousState() == CTaskOpenVehicleDoorFromOutside::State_ForcedEntry;
			if (!bDidForcedEntry && CheckForClimbUp())
			{
				m_iNetworkWaitState = GetState();
				SetTaskState(State_WaitForNetworkSync);
				return FSM_Continue;
			}
			else if (CheckForJackingPedFromOutside(pSeatOccupier) && m_pJackedPed->IsNetworkClone())
		    {
				// If there is a local ped in the seat, jack the ped if we can jack them
			    SetTaskState(State_JackPedFromOutside);
			    return FSM_Continue;
		    }
            else if (CheckForEnteringSeat(pSeatOccupier))
            {
                if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_OPEN_VEHICLE_DOOR_FROM_OUTSIDE)
                {
                    if (GetSubTask()->GetPreviousState() == CTaskOpenVehicleDoorFromOutside::State_ForcedEntry)
                    {
                        const CVehicleEntryPointAnimInfo* pClipInfo = m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint);
                        if (pClipInfo)
                        {
                            if (pClipInfo->GetForcedEntryIncludesGetIn())
                            {
                                // We're entering the direct access seat, store it here so we can pass it to the set ped in seat task
                                m_iCurrentSeatIndex = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat);
                                taskAssert(m_iCurrentSeatIndex > -1);
                                m_iLastCompletedState = State_OpenDoor;
                                SetTaskState(State_WaitForPedToLeaveSeat);
                                return FSM_Continue;
                            }
                        }
                    }                    
                }
            }

            // wait for the network state to decide what to do next
            m_iNetworkWaitState = GetState();
            SetTaskState(State_WaitForNetworkSync);
        }
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::PickUpBike_OnEnter()
{
	taskAssert(m_pVehicle->InheritsFromBike());
	CPed& rPed = *GetPed();
	m_vPreviousPedPosition = VEC3V_TO_VECTOR3(rPed.GetTransform().GetPosition());
	rPed.AttachPedToEnterCar(m_pVehicle, GetAttachToVehicleFlags(m_pVehicle), m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat), m_iTargetEntryPoint);
	Vector3 vTargetPosition;
	Quaternion qTargetOrientation;
	CModelSeatInfo::CalculateEntrySituation(m_pVehicle, &rPed, vTargetPosition, qTargetOrientation, m_iTargetEntryPoint, CModelSeatInfo::EF_IsPickUp, 0.0f, &m_vLocalSpaceEntryModifier);
	Vector3 vTransOffset = VEC3V_TO_VECTOR3(rPed.GetTransform().GetPosition()) - vTargetPosition;
	Quaternion qRotOffset;
	const Matrix34& mPedMat = RCC_MATRIX34(rPed.GetMatrixRef());
	Quaternion qInitialOrientation;
	mPedMat.ToQuaternion(qInitialOrientation);
	qRotOffset.MultiplyInverse(qTargetOrientation, qInitialOrientation);
	m_PlayAttachedClipHelper.SetInitialTranslationOffset(vTransOffset);
	m_PlayAttachedClipHelper.SetInitialRotationOffset(qRotOffset);

	SetMoveNetworkState(ms_PickUpBikeRequestId, ms_PickUpBikeOnEnterId, ms_PickUpPullUpStateId);
	SetClipPlaybackRate();
	RequestProcessMoveSignalCalls();

	if (rPed.GetWeaponManager()->GetEquippedWeaponObject())
	{
		rPed.GetWeaponManager()->DestroyEquippedWeaponObject();
	}

	m_bAnimFinished = false;
	m_moveNetworkHelper.SetFlag(true, ms_IsPickUpId);
	m_fOriginalLeanAngle = static_cast<CBike*>(m_pVehicle.Get())->GetLeanAngle();

	m_bWillBlendOutHandleIk = false;
	m_bWillBlendOutSeatIk = false;
}

////////////////////////////////////////////////////////////////////////////////

#if RSG_PC
REGISTER_TUNE_GROUP_BOOL( IGNORE_FORCED_SINGLE_PHYSICS_STEP_AT_HIGH_FRAME_RATE, true )
#endif // RSG_PC

CTask::FSM_Return CTaskEnterVehicle::PickUpBike_OnUpdate()
{
	ProcessComponentReservations();

	// The pickup/pull end up jittering without the two physics updates, possibly due to the fact the bike is moving aswell
	CPed* pPed = GetPed();
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ForceProcessPhysicsUpdateEachSimStep, true);
	pPed->SetPedResetFlag(CPED_RESET_FLAG_RequiresLegIk, true);
	pPed->GetIkManager().SetFlag(PEDIK_LEGS_AND_PELVIS_FADE_ON);

#if RSG_PC
	INSTANTIATE_TUNE_GROUP_BOOL(BIKE_BUG, IGNORE_FORCED_SINGLE_PHYSICS_STEP_AT_HIGH_FRAME_RATE);
	if (IGNORE_FORCED_SINGLE_PHYSICS_STEP_AT_HIGH_FRAME_RATE && pPed->IsLocalPlayer())
	{
		CPhysics::ms_bIgnoreForcedSingleStepThisFrame = true;
	}
#endif // RSG_PC

	static dev_float MAX_TIME_TO_PICK_UP_BIKE = 4.0f;
	if (!taskVerifyf(GetTimeInState() < MAX_TIME_TO_PICK_UP_BIKE, "Ped %s stuck in pick up state for longer than %.2f seconds, quitting out, movenetwork %s attached", pPed->GetDebugName() ? pPed->GetDebugName() : "NULL", MAX_TIME_TO_PICK_UP_BIKE, m_moveNetworkHelper.IsNetworkAttached() ? "WAS" : "WAS NOT"))
	{
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	if (CheckIfPlayerIsPickingUpMyBike())
	{
		pPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS|DETACH_FLAG_EXCLUDE_VEHICLE);
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	m_vPreviousPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	m_fInitialHeading = pPed->GetCurrentHeading();

	if (m_bAnimFinished)
	{
		m_moveNetworkHelper.SetFlag(true, ms_IsFastGetOnId);
		m_iLastCompletedState = State_PickUpBike;

		if(GetPed()->IsNetworkClone())
		{
			// wait for the network state to decide what to do next
			m_iNetworkWaitState = GetState();
			SetTaskState(State_WaitForNetworkSync);
			return FSM_Continue;
		}
		else
		{
			const CPed* pPedInOrUsingSeat = GetPedInOrUsingSeat(*m_pVehicle, m_iSeat);
			const bool bCanEnterSeat = (!pPedInOrUsingSeat || pPedInOrUsingSeat == pPed);
			if (bCanEnterSeat)
			{
				SetTaskState(State_StreamAnimsToEnterSeat);
				return FSM_Continue;
			}
			else
			{
				SetTaskState(State_Finish);
				return FSM_Continue;
			}
		}
	}
	else
	{
		float fPhase = 0.0f;
		const crClip* pClip = GetClipAndPhaseForState(fPhase);

		if (pClip)
		{
			// A bike has one hard coded door (CBike::InitDoors()) we use it to pick or pull it up!
			CCarDoor* pDoor = m_pVehicle->GetDoor(0); 
			if (pDoor)
			{
				TUNE_GROUP_FLOAT(BIKE_TUNE, PHASE_TO_BEGIN_PICK_UP, 0.4f, 0.0f, 1.0f, 0.01f);
				TUNE_GROUP_FLOAT(BIKE_TUNE, PHASE_TO_END_PICK_UP, 0.7f, 0.0f, 1.0f, 0.01f);

				float fPhaseToBeginOpenDoor = PHASE_TO_BEGIN_PICK_UP;
				float fPhaseToEndOpenDoor = PHASE_TO_END_PICK_UP;

				CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pClip, CClipEventTags::Door, CClipEventTags::Start, true, fPhaseToBeginOpenDoor);
				CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pClip, CClipEventTags::Door, CClipEventTags::Start, false, fPhaseToEndOpenDoor);

				CBike* pBike = static_cast<CBike*>(m_pVehicle.Get());
				pBike->m_nBikeFlags.bGettingPickedUp = true;

				// This actually drives the lean angle of the bike 
				PickOrPullUpBike(fPhase, fPhaseToBeginOpenDoor, fPhaseToEndOpenDoor);

				// Process hand ik for handle bar, if we're picking up on the left side, we will use the left hand, if right side, then use right hand
				const bool bIsRightSideEntry = m_pVehicle->GetEntryInfo(m_iTargetEntryPoint)->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT ? false : true;
				if (!m_bWillBlendOutHandleIk)
				{
					CTaskVehicleFSM::ProcessBikeHandleArmIk(*m_pVehicle, *pPed, pClip, fPhase, true, bIsRightSideEntry);
					m_bWillBlendOutHandleIk = (VF_IK_Blend_Out == CTaskVehicleFSM::ProcessBikeHandleArmIk(*m_pVehicle, *pPed, pClip, fPhase, false, bIsRightSideEntry));
				}

				// Use the opposite hand to reach for the seat
				if (!m_bWillBlendOutSeatIk)
				{
					const CEntryExitPoint* pEntryPoint = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint);
					if (pEntryPoint)
					{
						s32 iDirectAccessSeat = pEntryPoint->GetSeat(SA_directAccessSeat);
						if (m_pVehicle->IsSeatIndexValid(iDirectAccessSeat))
						{
							// Blended in
							CTaskVehicleFSM::ProcessSeatBoneArmIk(*m_pVehicle, *pPed, iDirectAccessSeat, pClip, fPhase, true, !bIsRightSideEntry);
							m_bWillBlendOutSeatIk = CTaskVehicleFSM::ProcessSeatBoneArmIk(*m_pVehicle, *pPed, iDirectAccessSeat, pClip, fPhase, false, !bIsRightSideEntry);
						}
					}
				}
			}
		}
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::PickOrPullUpBike(float fPhase, float fPhaseToBeginOpenDoor, float fPhaseToEndOpenDoor) 
{
	CBike& rBike = *static_cast<CBike*>(m_pVehicle.Get());

	// Overshoot the on-stand lean angle while picking up the bike
	float fOnStandLeanAngle = rBike.pHandling->GetBikeHandlingData()->m_fBikeOnStandLeanAngle;
	float fOverShootAmt = Sign(fOnStandLeanAngle) * CTaskEnterVehicle::ms_Tunables.m_BikeEnterLeanAngleOvershootAmt;

	if(Sign(fOnStandLeanAngle) == Sign(m_fOriginalLeanAngle))
	{
		fOverShootAmt *= -1.0f;
	}

	float fPhaseRate = rage::Clamp(fPhase * CTaskEnterVehicle::ms_Tunables.m_BikeEnterLeanAngleOvershootRate, 0.0f, 1.0f);
	float fEndAngle = fOnStandLeanAngle + fOverShootAmt;
	fEndAngle = rage::Lerp(fPhaseRate, m_fOriginalLeanAngle, fEndAngle);

	CTaskVehicleFSM::DriveBikeAngleFromClip(m_pVehicle, VF_OpenDoor, fPhase, fPhaseToBeginOpenDoor, fPhaseToEndOpenDoor, m_fOriginalLeanAngle, fEndAngle);
}

////////////////////////////////////////////////////////////////////////////////
	
void CTaskEnterVehicle::PullUpBike_OnEnter()
{
	taskAssert(m_pVehicle->InheritsFromBike());
	CPed& rPed = *GetPed();
	m_vPreviousPedPosition = VEC3V_TO_VECTOR3(rPed.GetTransform().GetPosition());
	rPed.AttachPedToEnterCar(m_pVehicle, GetAttachToVehicleFlags(m_pVehicle), m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat), m_iTargetEntryPoint);
	Vector3 vTargetPosition;
	Quaternion qTargetOrientation;
	CModelSeatInfo::CalculateEntrySituation(m_pVehicle, &rPed, vTargetPosition, qTargetOrientation, m_iTargetEntryPoint, CModelSeatInfo::EF_IsPullUp, 0.0f, &m_vLocalSpaceEntryModifier);
	Vector3 vTransOffset = VEC3V_TO_VECTOR3(rPed.GetTransform().GetPosition()) - vTargetPosition;
	Quaternion qRotOffset;
	const Matrix34& mPedMat = RCC_MATRIX34(rPed.GetMatrixRef());
	Quaternion qInitialOrientation;
	mPedMat.ToQuaternion(qInitialOrientation);
	qRotOffset.MultiplyInverse(qTargetOrientation, qInitialOrientation);
	m_PlayAttachedClipHelper.SetInitialTranslationOffset(vTransOffset);
	m_PlayAttachedClipHelper.SetInitialRotationOffset(qRotOffset);

	SetMoveNetworkState(ms_PullUpBikeRequestId, ms_PullUpBikeOnEnterId, ms_PickUpPullUpStateId);
	SetClipPlaybackRate();
	RequestProcessMoveSignalCalls();

	if (rPed.GetWeaponManager()->GetEquippedWeaponObject())
	{
		rPed.GetWeaponManager()->DestroyEquippedWeaponObject();
	}

	m_bAnimFinished = false;
	m_moveNetworkHelper.SetFlag(false, ms_IsPickUpId);
	m_fOriginalLeanAngle = static_cast<CBike*>(m_pVehicle.Get())->GetLeanAngle();
	m_bWillBlendOutHandleIk = false;
	m_bWillBlendOutSeatIk = false;

}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::PullUpBike_OnUpdate()
{
	ProcessComponentReservations();

	// The pickup/pull end up jittering without the two physics updates, possibly due to the fact the bike is moving aswell
	CPed* pPed = GetPed();
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ForceProcessPhysicsUpdateEachSimStep, true);
	pPed->SetPedResetFlag(CPED_RESET_FLAG_RequiresLegIk, true);
	pPed->GetIkManager().SetFlag(PEDIK_LEGS_AND_PELVIS_FADE_ON);

#if RSG_PC
	INSTANTIATE_TUNE_GROUP_BOOL(BIKE_BUG, IGNORE_FORCED_SINGLE_PHYSICS_STEP_AT_HIGH_FRAME_RATE);
	if (IGNORE_FORCED_SINGLE_PHYSICS_STEP_AT_HIGH_FRAME_RATE && pPed->IsLocalPlayer())
	{
		CPhysics::ms_bIgnoreForcedSingleStepThisFrame = true;
	}
#endif // RSG_PC

	static dev_float MAX_TIME_TO_PULL_UP_BIKE = 4.0f;
	if (!taskVerifyf(GetTimeInState() < MAX_TIME_TO_PULL_UP_BIKE, "Ped %s stuck in pull up state for longer than %.2f seconds, quitting out, movenetwork %s attached", pPed->GetDebugName() ? pPed->GetDebugName() : "NULL", MAX_TIME_TO_PULL_UP_BIKE, m_moveNetworkHelper.IsNetworkAttached() ? "WAS" : "WAS NOT"))
	{
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	if (CheckIfPlayerIsPickingUpMyBike())
	{
		pPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS|DETACH_FLAG_EXCLUDE_VEHICLE);
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	m_vPreviousPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	m_fInitialHeading = pPed->GetCurrentHeading();

	if (m_bAnimFinished)
	{
		m_moveNetworkHelper.SetFlag(true, ms_IsFastGetOnId);
        m_iLastCompletedState = State_PullUpBike;

        if(pPed->IsNetworkClone())
        {
		    // wait for the network state to decide what to do next
            m_iNetworkWaitState = GetState();
            SetTaskState(State_WaitForNetworkSync);
			return FSM_Continue;
        }
        else
        {
			const CPed* pPedInOrUsingSeat = GetPedInOrUsingSeat(*m_pVehicle, m_iSeat);
			const bool bCanEnterSeat = (!pPedInOrUsingSeat || pPedInOrUsingSeat == pPed);
			if (bCanEnterSeat)
			{
				SetTaskState(State_StreamAnimsToEnterSeat);
				return FSM_Continue;
			}
			else
			{
				SetTaskState(State_Finish);
				return FSM_Continue;
			}
        }
	}
	else
	{
		float fPhase = 0.0f;
		const crClip* pClip = GetClipAndPhaseForState(fPhase);

		if (pClip)
		{
			// A bike has one hard coded door (CBike::InitDoors()) we use it to pick or pull it up!
			CCarDoor* pDoor = m_pVehicle->GetDoor(0); 
			if (pDoor)
			{
				TUNE_GROUP_FLOAT(BIKE_TUNE, PHASE_TO_BEGIN_PULL_UP, 0.5f, 0.0f, 1.0f, 0.01f);
				TUNE_GROUP_FLOAT(BIKE_TUNE, PHASE_TO_END_PULL_UP, 0.8f, 0.0f, 1.0f, 0.01f);

				float fPhaseToBeginOpenDoor = PHASE_TO_BEGIN_PULL_UP;
				float fPhaseToEndOpenDoor = PHASE_TO_END_PULL_UP;

				CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pClip, CClipEventTags::Door, CClipEventTags::Start, true, fPhaseToBeginOpenDoor);
				CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pClip, CClipEventTags::Door, CClipEventTags::Start, false, fPhaseToEndOpenDoor);

				CBike* pBike = static_cast<CBike*>(m_pVehicle.Get());
				pBike->m_nBikeFlags.bGettingPickedUp = true;

				// This actually drives the lean angle of the bike 
				PickOrPullUpBike(fPhase, fPhaseToBeginOpenDoor, fPhaseToEndOpenDoor);

				// Process hand ik for handle bar, if we're pulling up on the left side, we will use the left hand, if right side, then use right hand
				const bool bIsRightSideEntry = m_pVehicle->GetEntryInfo(m_iTargetEntryPoint)->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT ? false : true;		
				if (!m_bWillBlendOutHandleIk)
				{
					CTaskVehicleFSM::ProcessBikeHandleArmIk(*m_pVehicle, *pPed, pClip, fPhase, true, bIsRightSideEntry);
					m_bWillBlendOutHandleIk = (VF_IK_Blend_Out == CTaskVehicleFSM::ProcessBikeHandleArmIk(*m_pVehicle, *pPed, pClip, fPhase, false, bIsRightSideEntry));
				}

				// Use the opposite hand to reach for the seat
				if (!m_bWillBlendOutSeatIk)
				{
					const CEntryExitPoint* pEntryPoint = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint);
					if (pEntryPoint)
					{
						s32 iDirectAccessSeat = pEntryPoint->GetSeat(SA_directAccessSeat);
						if (m_pVehicle->IsSeatIndexValid(iDirectAccessSeat))
						{
							// Blended in
							CTaskVehicleFSM::ProcessSeatBoneArmIk(*m_pVehicle, *pPed, iDirectAccessSeat, pClip, fPhase, true, !bIsRightSideEntry);
							m_bWillBlendOutSeatIk = CTaskVehicleFSM::ProcessSeatBoneArmIk(*m_pVehicle, *pPed, iDirectAccessSeat, pClip, fPhase, false, !bIsRightSideEntry);
						}
					}
				}
			}
		}
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::SetUprightBike_OnUpdate()
{
	taskAssert(m_pVehicle->InheritsFromBike());
	CPed& rPed = *GetPed();

	// B*2276548: Wait a small amount of time for bike to settle before setting bike upright.
	static dev_float fVelocityThreshold = 0.01f;
	static dev_float fMaxWaitTime = 0.5f;
	if (((m_pVehicle->GetVelocity().Mag() > fVelocityThreshold) || (m_pVehicle->GetAngVelocity().Mag() > fVelocityThreshold)) && (GetTimeInState() < fMaxWaitTime))
	{
		return FSM_Continue;
	}

	// If the vehicle couldn't be orientated up bail out.
	m_pVehicle->PlaceOnRoadAdjust();
	if( CVehicle::GetVehicleOrientation(*m_pVehicle) == CVehicle::VO_Upright )
	{
		m_iLastCompletedState = State_SetUprightBike;

		const CPed* pPedInOrUsingSeat = GetPedInOrUsingSeat(*m_pVehicle, m_iSeat);
		const bool bCanEnterSeat = (!pPedInOrUsingSeat || pPedInOrUsingSeat == &rPed);
		if (bCanEnterSeat)
		{
			SetTaskState(State_StreamAnimsToEnterSeat);
			return FSM_Continue;
		}
		else
		{
			SetTaskState(State_Finish);
			return FSM_Continue;
		}
	}
	else
	{
		SetTaskFinishState(TFR_FAILED_TO_REORIENTATE_VEHICLE, VEHICLE_DEBUG_DEFAULT_FLAGS);
		SetTaskState(State_Finish);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::AlignToEnterSeat_OnEnter()
{
	GetPed()->AttachPedToEnterCar(m_pVehicle, GetAttachToVehicleFlags(m_pVehicle), m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat), m_iTargetEntryPoint);

	StoreInitialOffsets();
	if (IsMoveTransitionAvailable(GetState()))
	{
		m_moveNetworkHelper.SendRequest(ms_AlignToEnterSeatRequestId);
	}
	else
	{
		m_moveNetworkHelper.ForceStateChange(ms_AlignToEnterSeatStateId);
	}
	SetClipPlaybackRate();
	m_moveNetworkHelper.WaitForTargetState(ms_AlignToEnterSeatOnEnterId);
	m_bDoingExtraGetInFixup = false;
	m_bAnimFinished = false;
	RequestProcessMoveSignalCalls();
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::AlignToEnterSeat_OnUpdate()
{
	CPed *pPed = GetPed();

	static dev_float MAX_TIME_TO_ALIGN_TO_ENTER_SEAT = 4.0f;
	if (!taskVerifyf(GetTimeInState() < MAX_TIME_TO_ALIGN_TO_ENTER_SEAT, "Ped %s stuck in align to enter seat state for longer than %.2f seconds, quitting out, movenetwork %s attached, previous state %s", GetPed()->GetDebugName() ? GetPed()->GetDebugName() : "NULL", MAX_TIME_TO_ALIGN_TO_ENTER_SEAT, m_moveNetworkHelper.IsNetworkAttached() ? "WAS" : "WAS NOT", GetStaticStateName(GetPreviousState())))
	{
		AI_LOG_WITH_ARGS("[%s] - %s Ped %s is quitting due to time out CTaskEnterVehicle::AlignToEnterSeat_OnUpdate\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(pPed),  AILogging::GetDynamicEntityNameSafe(pPed));
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	if (!m_moveNetworkHelper.IsInTargetState())
	{
		AI_LOG_WITH_ARGS("[%s] - %s Ped %s waiting to be in correct move network state\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(pPed),  AILogging::GetDynamicEntityNameSafe(pPed));
		return FSM_Continue;
	}

#if __BANK
	if (pPed->IsNetworkClone())
	{
		const float fAnimPhase = m_moveNetworkHelper.GetFloat(ms_AlignToEnterSeatPhaseId);
		AI_LOG_WITH_ARGS("[%s] - %s Ped %s waiting for align to enter anim to finish, current phase = %.2f\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(pPed),  AILogging::GetDynamicEntityNameSafe(pPed), fAnimPhase);
	}
#endif 

	if (m_bAnimFinished)
	{
		m_iLastCompletedState = State_AlignToEnterSeat;

        bool bCanChangeState = true;
        s32  iNetworkState   = -1;

        if(pPed->IsNetworkClone())
        {
            bCanChangeState = CanUpdateStateFromNetwork();
            iNetworkState   = GetStateFromNetwork();

            if(iNetworkState == State_Finish)
            {
                SetTaskState(State_Finish);
                return FSM_Continue;
            }
#if __BANK
			else
			{
				AI_LOG_WITH_ARGS("[%s] - %s Ped %s, bCanChangeState = %s, iNetworkState = %i\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(pPed),  AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetBooleanAsString(bCanChangeState), iNetworkState);
			}
#endif
        }

        if(bCanChangeState)
        {
		    CPed* pSeatOccupier = NULL;
		    // Check if we should play a streamed entry anim (this replaces the open door/enter seat/jacking states)
		    if (CheckForStreamedEntry())
		    {
                if(!pPed->IsNetworkClone() || iNetworkState == State_WaitForStreamedEntry || iNetworkState == State_StreamedEntry)
                {
			        SetTaskState(State_WaitForStreamedEntry);
			        return FSM_Continue;
                }
		    }
		    else if (CheckForJackingPedFromOutside(pSeatOccupier))
		    {
                if(!pPed->IsNetworkClone() || iNetworkState == State_JackPedFromOutside)
                {
			        SetTaskState(State_JackPedFromOutside);
			        return FSM_Continue;
                }
		    }
		    else if (CheckForEnteringSeat(pSeatOccupier))
		    {
                if(!pPed->IsNetworkClone() || iNetworkState == State_StreamAnimsToEnterSeat || State_EnterSeat)
                {
			        SetTaskState(State_StreamAnimsToEnterSeat);
			        return FSM_Continue;
                }
		    }
		    else
		    {
			    SetTaskState(State_Finish);
			    return FSM_Continue;
		    }
        }
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::WaitForGroupLeaderToEnter_OnUpdate()
{
	CPed* pPed = GetPed();
	const bool bPedShouldContinueToWait = PedShouldWaitForLeaderToEnter();
#if __DEV
	const CPed* pLeader = pPed->GetPedsGroup() ? pPed->GetPedsGroup()->GetGroupMembership()->GetLeader() : NULL;
	AI_FUNCTION_LOG_WITH_ARGS("Ped %s is waiting for leader %s to enter vehicle %s, continue to wait ? %s", AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetDynamicEntityNameSafe(pLeader), AILogging::GetDynamicEntityNameSafe(m_pVehicle), AILogging::GetBooleanAsString(bPedShouldContinueToWait));
#endif // __DEV

	if (!bPedShouldContinueToWait && !ShouldWarpInAndOutInMP(*m_pVehicle, m_iTargetEntryPoint) && CTaskEnterVehicleAlign::ShouldTriggerAlign(*m_pVehicle, *pPed, m_iTargetEntryPoint, &m_vLocalSpaceEntryModifier, IsFlagSet(CVehicleEnterExitFlags::CombatEntry), false, false, IsFlagSet(CVehicleEnterExitFlags::EnteringOnVehicle)))
	{
		SetTaskState(State_StreamAnimsToAlign);
		return FSM_Continue;
	}
	else
	{
		bool bPickDoor = false;

		// We need to add an additional flag for seat entry point evaluation
		VehicleEnterExitFlags iFlagsForSeatEntryEvaluate = m_iRunningFlags;
		iFlagsForSeatEntryEvaluate.BitSet().Set(CVehicleEnterExitFlags::IgnoreEntryPointCollisionCheck);
		
		// If the point is no longer valid - restart
		if (!CModelSeatInfo::EvaluateSeatAccessFromEntryPoint(pPed, m_pVehicle, m_iSeatRequestType, m_iSeat, m_iTargetEntryPoint, m_bWarping, iFlagsForSeatEntryEvaluate))
		{
			bPickDoor = true;
		}

		if (GetTimeInState() > ms_Tunables.m_GroupMemberWaitMinTime)
		{
			if (!bPedShouldContinueToWait)
			{
				bPickDoor = true;
			}
			else
			{
				const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
				Vector3 vEntryPos(Vector3::ZeroType);
				Quaternion qEntryOrientation(0.0f,0.0f,0.0f,1.0f);
				CModelSeatInfo::CalculateEntrySituation(m_pVehicle.Get(), pPed, vEntryPos, qEntryOrientation, m_iTargetEntryPoint, 0, 0.0f, &m_vLocalSpaceEntryModifier);
				if (vEntryPos.Dist2(vPedPos) > rage::square(ms_Tunables.m_GroupMemberWaitDistance))
				{
					bPickDoor = true;
				}
			}
		}

		if (bPickDoor)
		{
			SetTaskState(State_PickDoor);
			return FSM_Continue;
		}

		if (pPed->GetPedsGroup() && pPed->GetPedsGroup()->GetGroupMembership()->GetLeader())
		{
			pPed->SetDesiredHeading(VEC3V_TO_VECTOR3(pPed->GetPedsGroup()->GetGroupMembership()->GetLeader()->GetTransform().GetPosition()));
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::WaitForEntryPointReservation_OnUpdate()
{
	CPed& rPed = *GetPed();
	CPedGroup* pPedsGroup = rPed.GetPedsGroup();

	if (!pPedsGroup ||!m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint))
	{
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	CPed* pLeader = pPedsGroup->GetGroupMembership()->GetLeader();
	if (!pLeader || pLeader == &rPed || pLeader->GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle))
	{
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	if (PedHasReservations())
	{
		SetTaskState(State_GoToDoor);
		return FSM_Continue;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::JackPedFromOutside_OnEnter()
{
	CPed* pPed = GetPed();

	InitExtraGetInFixupPoints();

	//React to the jack.
	CReactToBeingJackedHelper::CheckForAndMakeOccupantsReactToBeingJacked(*pPed, *m_pVehicle, m_iTargetEntryPoint, false);

	// We may not have set the ped we're jacking in CTask::FSM_Return CTaskEnterVehicle::ReserveSeat_OnUpdate()
	// as we may 'think' the other player cannot jack us. 
	m_pJackedPed = GetPedOccupyingDirectAccessSeat();
	if (m_pJackedPed)
	{
		SetRunningFlag(CVehicleEnterExitFlags::HasJackedAPed);
	}

	m_bWasJackedPedDeadInitially = m_pJackedPed && m_pJackedPed->IsDead();

	WarpPedToGetInPointIfNecessary(*pPed, *m_pVehicle);
	SetUpRagdollOnCollision(*pPed, *m_pVehicle);
	const CVehicleEntryPointAnimInfo* pClipInfo = m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint);

	if(pPed && pPed->GetSpeechAudioEntity())
	{
		//van jacking is a longer anim, so I don't want the same delay for the ped reply
		bool vanJackClipUsed = pClipInfo && pClipInfo->GetName() == 471872909; 
		pPed->GetSpeechAudioEntity()->SetJackingSpeechInfo(m_pJackedPed, m_pVehicle, false, vanJackClipUsed);
	}

	// Make the ped get out
	if (m_pJackedPed && !m_pJackedPed->IsNetworkClone())
	{
		VehicleEnterExitFlags vehicleFlags;
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::BeJacked);
		if (IsFlagSet(CVehicleEnterExitFlags::InWater))
		{
			vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::InWater);
		}
		if (IsFlagSet(CVehicleEnterExitFlags::CombatEntry))
		{
			vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::CombatEntry);
		}
		if (IsFlagSet(CVehicleEnterExitFlags::UseFastClipRate))
		{
			vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::UseFastClipRate);
		}

		bool bOnVehicleJack = IsFlagSet(CVehicleEnterExitFlags::EnteringOnVehicle) && pClipInfo->GetHasGetInFromWater() && pClipInfo->GetUseStandOnGetIn();
		if(bOnVehicleJack)
		{
			vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::UseOnVehicleJack);
		}

		s32 iJackEntyPoint;
		if (pClipInfo->GetUseSeatClipSetAnims())
		{
			s32 nSeatID = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat, m_iSeat);
			iJackEntyPoint = m_pVehicle->GetDirectEntryPointIndexForSeat(nSeatID);
		}
		else
		{
			iJackEntyPoint = m_iTargetEntryPoint;
		}

		AI_LOG_IF_PLAYER(pPed, "[EnterVehicle] - Ped %s is giving ped %s exit vehicle task in CTaskEnterVehicle::JackPedFromOutside_OnEnter", AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetDynamicEntityNameSafe(m_pJackedPed));

		const s32 iEventPriority = CTaskEnterVehicle::GetEventPriorityForJackedPed(*m_pJackedPed);
		CEventGivePedTask givePedTask(PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP, rage_new CTaskExitVehicle(m_pVehicle, vehicleFlags, 0.0f, GetPed(), iJackEntyPoint), true, iEventPriority );
		m_pJackedPed->GetPedIntelligence()->AddEvent(givePedTask);
		// Prevent jacked ped from being time sliced otherwise their being jacked anim can get delayed as they wont get the event
		// and process the exit task immediately
		m_pJackedPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
		m_pJackedPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePreCameraAIUpdate, true);
#if __BANK
		CGivePedJackedEventDebugInfo instance(m_pJackedPed, this, __FUNCTION__);
		instance.Print();
#endif // __BANK
	}
	else
	{
		AI_LOG_IF_PLAYER(pPed, "[EnterVehicle] - Ped %s is NOT giving ped %s exit vehicle task in CTaskEnterVehicle::JackPedFromOutside_OnEnter", AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetDynamicEntityNameSafe(m_pJackedPed));
	}

	if (!pPed->GetIsAttachedInCar())
	{
		pPed->AttachPedToEnterCar(m_pVehicle, GetAttachToVehicleFlags(m_pVehicle), m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat, m_iSeat), m_iTargetEntryPoint);
	}

	if (pClipInfo)
	{
		if (m_bWasJackedPedDeadInitially)
		{
			if (pClipInfo->GetDeadJackIncludesGetIn())
				SetRunningFlag(CVehicleEnterExitFlags::JackAndGetIn);
		}
		else
		{
			if (pClipInfo->GetJackIncludesGetIn())
				SetRunningFlag(CVehicleEnterExitFlags::JackAndGetIn);
		}
	}

	SetClipsForState();
	StoreInitialOffsets();
	SetClipPlaybackRate();
	SetMoveNetworkState(ms_JackPedRequestId, ms_JackPedFromOutsideOnEnterId, ms_JackPedFromOutsideStateId);

	float fBlendInDuration = INSTANT_BLEND_DURATION;
	if (GetPreviousState() == State_OpenDoor && IsFlagSet(CVehicleEnterExitFlags::CombatEntry) && pClipInfo && pClipInfo->GetHasCombatEntry())
	{
		fBlendInDuration = ms_Tunables.m_OpenDoorToJackBlendDuration;
	}
	m_moveNetworkHelper.SetFloat(ms_OpenDoorToJackBlendInDurationId, fBlendInDuration);

	if (taskVerifyf(pPed->GetWeaponManager(), "Ped %s doesn't have a weapon manager", pPed->GetModelName()))
	{
		CObject* pWeapon = pPed->GetWeaponManager()->GetEquippedWeaponObject();
		if (pWeapon && pWeapon->GetIsVisible())
		{
			pWeapon->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, false, true);
		}
	}

	ProcessJackingStatus();
	m_fInterruptPhase = -1.0f;

	// Let ProcessMoveSignals() handle the communication with the Move network.
	m_bAnimFinished = false;
	RequestProcessMoveSignalCalls();

	CTaskEnterVehicle::PossiblyReportStolentVehicle(*pPed, *m_pVehicle, true);

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::JackPedFromOutside_OnUpdate()
{
	CPed& rPed = *GetPed();
	CVehicle& rVeh = *m_pVehicle;

	float fPhase = 0.0f;
	const crClip* pClip = GetClipAndPhaseForState(fPhase);
	CTaskVehicleFSM::ProcessLegIkBlendOut(rPed, m_fLegIkBlendOutPhase, fPhase);

	UpdateExtraGetInFixupPoints(pClip, fPhase);

#if ENABLE_MOTION_IN_TURRET_TASK	
	if (pClip && CTaskVehicleFSM::SeatRequiresHandGripIk(rVeh, m_iSeat))
	{
		CTurret* pTurret = rVeh.GetVehicleWeaponMgr() ? rVeh.GetVehicleWeaponMgr()->GetFirstTurretForSeat(m_iSeat) : NULL;
		if (pTurret)
		{
			CTaskVehicleFSM::ProcessTurretHandGripIk(rVeh, rPed, pClip, fPhase, *pTurret);
		}
	}
#endif // ENABLE_MOTION_IN_TURRET_TASK	

	// Safety abort
	if (GetTimeInState() > 10.0f)
	{
		taskWarningf("Frame %i: Ped %s aborting jack because in state too long, network active ? %s, attached ? %s", fwTimer::GetFrameCount(), rPed.GetModelName(), m_moveNetworkHelper.IsNetworkActive() ? "TRUE" : "FALSE", m_moveNetworkHelper.IsNetworkAttached() ? "TRUE" : "FALSE");
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	CTaskVehicleFSM::ProcessApplyForce(m_moveNetworkHelper, rVeh, rPed);

	const CVehicleEntryPointAnimInfo* pClipInfo = rVeh.GetEntryAnimInfo(m_iTargetEntryPoint);
	const bool bPreventInterrupting = pClipInfo->GetPreventJackInterrupt();
	if (m_fInterruptPhase < 0.0f && !bPreventInterrupting)
	{
		if (!pClipInfo->GetJackIncludesGetIn())
		{
			m_fInterruptPhase = 1.0f;
		}

		float fUnused;
		CClipEventTags::GetMoveEventStartAndEndPhases(pClip, ms_InterruptId.GetHash(), fUnused, m_fInterruptPhase);

#if __DEV
		if(pClip)
		{
			const bool bJackedPedIsDead = m_pJackedPed && m_pJackedPed->ShouldBeDead();
			if ((!bJackedPedIsDead && pClipInfo->GetJackIncludesGetIn()) || (bJackedPedIsDead && pClipInfo->GetDeadJackIncludesGetIn()))
			{
				taskAssertf(m_fInterruptPhase < 1.0f, "ADD A BUG TO DEFAULT ANIM INGAME - Vehicle with entry anim info %s has an Interrupt tag with end phase 1.0, this is likely an error as the jack contains the get in, jack anim %s", pClipInfo->GetName().GetCStr(), pClip->GetName());
				taskAssertf(CClipEventTags::HasMoveEvent(pClip, ms_InterruptId.GetHash()), "ADD A BUG TO DEFAULT ANIM INGAME - Vehicle with entry anim info %s is missing Interrupt tag on jack anim %s", pClipInfo->GetName().GetCStr(), pClip->GetName());
			}
		}
#endif // __DEV
	}

	TUNE_GROUP_FLOAT(JACK_TUNE, MIN_PHASE_TO_ALLOW_JACK_ABORT, 0.25f, 0.0f, 1.0f, 0.01f);
	if (NetworkInterface::IsGameInProgress() && !rPed.IsNetworkClone() && ShouldAbortBecauseVehicleMovingTooFast() && fPhase > MIN_PHASE_TO_ALLOW_JACK_ABORT && fPhase < m_fInterruptPhase)
	{
		SetTaskFinishState(TFR_VEHICLE_MOVING_TOO_FAST, VEHICLE_DEBUG_DEFAULT_FLAGS | VEHICLE_DEBUG_RENDER_RELATIVE_VELOCITY);
		return FSM_Continue;
	}

	if (m_pVehicle->InheritsFromBike())
	{
		CTaskEnterVehicle::ProcessBikeArmIk(rVeh, rPed, m_iTargetEntryPoint, *pClip, fPhase, true);

		TUNE_GROUP_FLOAT(BIKE_TUNE, MIN_Z_AXIS, 0.75f, 0.0f, 1.0f, 0.01f);
		if (!rPed.IsNetworkClone() && !m_pVehicle->IsNetworkClone() && m_pVehicle->GetTransform().GetC().GetZf() < MIN_Z_AXIS)
		{
			const float fOpenRatio = rVeh.GetDoor(0)->GetDoorRatio();
			TUNE_GROUP_FLOAT(BIKE_TUNE, ANGLE_RATIO_TO_RAGDOLL, 0.05f, 0.0f, 1.0f, 0.01f);
			const float fRatioDelta = Abs(1.0f - fOpenRatio);
			if (fRatioDelta > ANGLE_RATIO_TO_RAGDOLL && CTaskNMBehaviour::CanUseRagdoll(&rPed, RAGDOLL_TRIGGER_IMPACT_CAR, &rVeh))
			{
				SetTaskState(State_Ragdoll);
				return FSM_Continue;
			}
		}
	}

	if (IsFlagSet(CVehicleEnterExitFlags::JackAndGetIn) && !rPed.IsNetworkClone())
	{
		const CEntryExitPoint* pEntryPoint = rVeh.GetEntryExitPoint(m_iTargetEntryPoint);
		if (pEntryPoint)
		{
			bool bWantsToExit = false;
			// Check whether the player has pressed to exit, once we can interrupt we force the exit
			if (CTaskVehicleFSM::CheckForEnterExitVehicle(rPed))
			{
				bWantsToExit = true;
			}

			// If we're going to close the door, do it asap
			if (CTaskEnterVehicle::CheckForCloseDoorInterrupt(rPed, rVeh, m_moveNetworkHelper, pEntryPoint, bWantsToExit, m_iTargetEntryPoint)
				|| CTaskEnterVehicle::CheckForShuffleInterrupt(rPed, m_moveNetworkHelper, pEntryPoint, *this))
			{
				// We're entering the direct access seat, store it here so we can pass it to the set ped in seat task
				m_iCurrentSeatIndex = rVeh.GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat, m_iSeat);
				taskAssert(m_iCurrentSeatIndex > -1);
				m_iLastCompletedState = State_JackPedFromOutside;
				SetTaskState(State_WaitForPedToLeaveSeat);
				return FSM_Continue;
			}
		}
	}

	// If a player is jacking a hated ped from any seat other than the driver seat, the player defaults to earlying out and remaining on the outside of the car
	const bool bPlayerJackingHatedPlayerInNonDriverSeat = rPed.IsAPlayerPed() && IsFlagSet(CVehicleEnterExitFlags::JackWantedPlayersRatherThanStealCar) && 
							m_pJackedPed && m_pJackedPed->GetPlayerWanted() && m_pJackedPed->GetPlayerWanted()->GetWantedLevel() >= WANTED_LEVEL1 &&
							rVeh.GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat) != rVeh.GetDriverSeat();

    bool bJackedPedNotResponding = false;

	// Break out of the jack if there is a clone ped in the seat that is not running the exit vehicle task.
    // This can happen while players are timing-out of a session and would lead to peds intersecting in the seat
    if(!rPed.IsNetworkClone() && m_pJackedPed && m_pJackedPed->IsNetworkClone() && 
		m_pJackedPed->GetIsInVehicle() &&
		!m_pJackedPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE))
    {
        bJackedPedNotResponding = true;
    }

    // Break out of the jack after the seat is clear if flagged too, or if the player is pulling away from the car
	const bool bJustPullPedOut = IsFlagSet(CVehicleEnterExitFlags::JustPullPedOut) 
        || bJackedPedNotResponding
		|| rPed.ShouldBeDead()
		|| bPlayerJackingHatedPlayerInNonDriverSeat 
		|| (m_pJackedPed && m_pJackedPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedsJackingMeDontGetIn))
		|| CheckPlayerExitConditions(rPed, rVeh, m_iRunningFlags, CTaskVehicleFSM::MIN_TIME_BEFORE_PLAYER_CAN_ABORT, false);

	// For jacks which contain the get in, we can optionally interrupt the anim to prevent the get in
	if (!rPed.IsNetworkClone() && !bPreventInterrupting && bJustPullPedOut && m_moveNetworkHelper.GetBoolean(ms_InterruptId) && !rVeh.GetLayoutInfo()->GetPreventJustPullingOut())
	{
		rPed.DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS|DETACH_FLAG_EXCLUDE_VEHICLE);
		SetTaskState(State_UnReserveDoorToFinish);
		return FSM_Continue;
	}

    if(rPed.IsNetworkClone() && !rPed.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE))
    {
        if(!NetworkInterface::IsPedInVehicleOnOwnerMachine(&rPed))
        {
            SetTaskState(State_Finish);
		    return FSM_Continue;
        }
    }

	if (m_bAnimFinished)
	{
		if (rVeh.InheritsFromBike() && rVeh.GetLayoutInfo()->GetBikeLeansUnlessMoving())
		{
			m_moveNetworkHelper.SetFlag(true, ms_IsFastGetOnId);
		}

        m_iLastCompletedState = State_JackPedFromOutside;

        if(rPed.IsNetworkClone())
        {
            if (IsFlagSet(CVehicleEnterExitFlags::JackAndGetIn))
			{
                // We're entering the direct access seat, store it here so we can pass it to the set ped in seat task
				m_iCurrentSeatIndex = rVeh.GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat, m_iSeat);
				taskAssert(m_iCurrentSeatIndex > -1);
				m_iLastCompletedState = State_JackPedFromOutside;
				SetTaskState(State_WaitForPedToLeaveSeat);
				return FSM_Continue;
            }
            else
            {
                // wait for the state from the network to change before advancing the state
                m_iNetworkWaitState = GetState();
                SetTaskState(State_WaitForNetworkSync);
                return FSM_Continue;
            }
        }
        else
        {
		    if (bJustPullPedOut && !bPreventInterrupting && ((!IsFlagSet(CVehicleEnterExitFlags::JackAndGetIn) && !rVeh.InheritsFromBike()) || m_moveNetworkHelper.GetBoolean(ms_InterruptId)))
		    {
			    // Need to detach to reactivate physics
			    rPed.DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS|DETACH_FLAG_EXCLUDE_VEHICLE);
			    SetTaskState(State_UnReserveDoorToFinish);
                return FSM_Continue;
		    }
		    else
		    {
				if (IsFlagSet(CVehicleEnterExitFlags::JackAndGetIn))
				{
					// We're entering the direct access seat, store it here so we can pass it to the set ped in seat task
					m_iCurrentSeatIndex = rVeh.GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat, m_iSeat);
					taskAssert(m_iCurrentSeatIndex > -1);
					m_iLastCompletedState = State_JackPedFromOutside;
					SetTaskState(State_WaitForPedToLeaveSeat);
					return FSM_Continue;
				}
			    else
				{
					SetTaskState(State_StreamAnimsToEnterSeat);
					return FSM_Continue;
				}
		    }
        }
	}

	if (pClip)
	{
		// Keep doors open
		CTaskVehicleFSM::ProcessOpenDoor(*pClip, 1.0f, rVeh, m_iTargetEntryPoint, false, true);
	}

	if(m_moveNetworkHelper.GetBoolean(ms_AllowBlockedJackReactionsId))
	{
		SetRunningFlag(CVehicleEnterExitFlags::AllowBlockedJackReactions);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::UnReserveDoorToShuffle_OnUpdate()
{
	UnreserveDoor(GetPed());
	SetTaskState(State_ShuffleToSeat);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::UnReserveDoorToFinish_OnUpdate()
{
	UnreserveDoor(GetPed());
	SetTaskFinishState(TFR_PED_UNRESERVED_DOOR, VEHICLE_DEBUG_DEFAULT_FLAGS);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::StreamAnimsToEnterSeat_OnEnterClone()
{
	CTaskVehicleFSM::SetRemotePedUsingDoorsTrue(*GetPed(), m_pVehicle, m_iTargetEntryPoint);
}

CTask::FSM_Return CTaskEnterVehicle::StreamAnimsToEnterSeat_OnUpdate()
{
	const CVehicleEntryPointAnimInfo* pEntryAnimInfo = m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint);
	if (!taskVerifyf(pEntryAnimInfo, "Couldn't find entry anim info for entry point %i, vehicle %s", m_iTargetEntryPoint, m_pVehicle->GetDebugName()))
	{
		return FSM_Quit;
	}

	s32 iSeatIndex = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat, m_iSeat);
	if (!taskVerifyf(m_pVehicle->IsSeatIndexValid(iSeatIndex), "Seat index %i is not valid for direct access seat for entry point %i, vehicle %s", iSeatIndex, m_iTargetEntryPoint, m_pVehicle->GetDebugName()))
	{
		return FSM_Quit;
	}

	fwMvClipSetId entryClipsetId = m_OverridenEntryClipSetId;
	if (entryClipsetId == CLIP_SET_ID_INVALID)
	{
		const CVehicleSeatAnimInfo* pSeatAnimInfo = m_pVehicle->GetSeatAnimationInfo(iSeatIndex);
		entryClipsetId = (pEntryAnimInfo->GetUseSeatClipSetAnims() && pSeatAnimInfo) ? pSeatAnimInfo->GetSeatClipSetId() : CTaskEnterVehicleAlign::GetEntryClipsetId(m_pVehicle, m_iTargetEntryPoint, GetPed());
	}

	if (!taskVerifyf(entryClipsetId != CLIP_SET_ID_INVALID, "Entry clipset id is invalid for entry point %i, vehicle %s", m_iTargetEntryPoint, m_pVehicle->GetDebugName()))
	{
		return FSM_Quit;
	}

	if (CTaskVehicleFSM::RequestVehicleClipSetReturnIsLoaded(entryClipsetId, SP_High))
	{
		SetTaskState(State_EnterSeat);
		return FSM_Continue;
	}
	else if (!taskVerifyf(GetTimeInState() < 5.0f, "Couldn't stream clipset %s in time, bailing out", entryClipsetId.GetCStr()))
	{
		return FSM_Quit;
	}
	return FSM_Continue;
}

void CTaskEnterVehicle::StreamAnimsToEnterSeat_OnExitClone()
{
	CTaskVehicleFSM::SetRemotePedUsingDoorsFalse(*GetPed(), m_pVehicle, m_iTargetEntryPoint);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::EnterSeat_OnEnter()
{
	fwMvClipSetId entryClipsetId = m_OverridenEntryClipSetId == CLIP_SET_ID_INVALID ? CTaskEnterVehicleAlign::GetEntryClipsetId(m_pVehicle, m_iTargetEntryPoint, GetPed()) : m_OverridenEntryClipSetId;
	if (taskVerifyf(CTaskVehicleFSM::IsVehicleClipSetLoaded(entryClipsetId), "Clipset %s wasn't streamed before enter seat!", entryClipsetId.GetCStr()))
	{
		m_moveNetworkHelper.SetClipSet(entryClipsetId);
	}

	//! Find additional fixup points if we have any.
	InitExtraGetInFixupPoints();

	CPed* pPed = GetPed();
	WarpPedToGetInPointIfNecessary(*pPed, *m_pVehicle);
	StoreInitialOffsets();
	bool forceMoveState = IsFlagSet(CVehicleEnterExitFlags::WarpToEntryPosition) ?  false : !IsMoveTransitionAvailable(State_EnterSeat);
    taskAssertf(!forceMoveState || GetPed()->IsNetworkClone(), "Trying to force the move state on a network clone!, previous state %s, last completed state %s", GetStaticStateName(GetPreviousState()), GetStaticStateName(m_iLastCompletedState));
	SetNewTask(rage_new CTaskEnterVehicleSeat(m_pVehicle, m_iTargetEntryPoint, m_iSeat, m_iRunningFlags, m_moveNetworkHelper, forceMoveState));
	
	if (pPed->GetAttachState() != ATTACH_STATE_PED_ENTER_CAR)
	{
		pPed->AttachPedToEnterCar(m_pVehicle, GetAttachToVehicleFlags(m_pVehicle), m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat, m_iSeat), m_iTargetEntryPoint);
	}

	if (taskVerifyf(pPed->GetWeaponManager(), "Ped %s doesn't have a weapon manager", pPed->GetModelName()))
	{
		CObject* pWeapon = pPed->GetWeaponManager()->GetEquippedWeaponObject();
		if (pWeapon && pWeapon->GetIsVisible())
		{
			pWeapon->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, false, true);
		}
	}

	// Only do the checks below if the ped is empty - otherwise we might throw this event twice:
	// once here and once from the ped being jacked.
	if (pPed->IsLocalPlayer() && (m_pVehicle->GetNumberOfPassenger() < 1 && !m_pVehicle->GetDriver()))
	{

		// Throw a shocking SeenCarStolen event.
		bool bRequiresNearbyAlertFlagOnPed = true;

		// Check if this vehicle sends a shocking event for being stolen when started.
		if (m_pVehicle->ShouldAlertNearbyWhenVehicleIsEntered() || m_pVehicle->IsAlarmActivated())
		{
			// Reset the alert notification now that the player has entered it.
			m_pVehicle->SetShouldAlertNearbyWhenVehicleIsEntered(false);
			bRequiresNearbyAlertFlagOnPed = false;
		}
		// Otherwise, Peds need to have the "NearbyAlertWhenVehicleEntered" flag set to react to this event.
		CEventShockingSeenCarStolen ev(*pPed, m_pVehicle, bRequiresNearbyAlertFlagOnPed);
		CShockingEventsManager::Add(ev);
		
		if (VEHICLE_TYPE_CAR == m_pVehicle->GetVehicleType() && m_pVehicle->IsLawEnforcementVehicleModelId(m_pVehicle->GetModelId()))
		{
			if (NetworkInterface::IsGameInProgress() && StatsInterface::IsGangMember(pPed))
			{
				StatsInterface::IncrementStat(StatsInterface::GetStatsModelHashId("COPCARS_ENTERED_AS_CROOK"), 1.0f);
			}
		}

		REPLAY_ONLY(ReplayBufferMarkerMgr::AddMarker(3000,2000,IMPORTANCE_LOW);)
	}

	m_bShouldBePutIntoSeat = false;
}


////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::EnterSeat_OnUpdate()
{
	CTaskVehicleFSM::PointTurretInNeturalPosition(*m_pVehicle, m_iSeat);

	// Prevent getting on to a seat someone else is getting onto or already in
	CPed& rPed = *GetPed();
	if (!rPed.IsNetworkClone() && m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint))
	{
		const s32 iSeatIndex = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat, m_iSeat);
		if (m_pVehicle->IsSeatIndexValid(iSeatIndex))
		{
			CPed* pPedUsingSeat = CTaskVehicleFSM::GetPedInOrUsingSeat(*m_pVehicle, iSeatIndex);
			if (pPedUsingSeat && pPedUsingSeat != &rPed)
			{
				bool bShouldQuit = true;

				// If the ped using the seat is in it, then allow us to enter if they are shuffling to the other seat
				if (m_pVehicle->GetPedInSeat(iSeatIndex) == pPedUsingSeat && IsSeatOccupierShuffling(pPedUsingSeat))
				{
					bShouldQuit = false;
				}	

				if (bShouldQuit)
				{
					AI_LOG_WITH_ARGS("[VehicleEntryExit][CTaskEnterVehicle::EnterSeat_OnUpdate] - Ped %s is quitting because ped %s is in our seat %i, target entry point %i\n", AILogging::GetDynamicEntityNameSafe(&rPed), AILogging::GetDynamicEntityNameSafe(pPedUsingSeat), iSeatIndex, m_iTargetEntryPoint);
					return FSM_Quit;
				}
			}
		}
	}

#if ENABLE_MOTION_IN_TURRET_TASK	
	if (CTaskVehicleFSM::SeatRequiresHandGripIk(*m_pVehicle, m_iSeat) && !m_pVehicle->GetSeatAnimationInfo(m_iSeat)->IsStandingTurretSeat())
	{
		CTurret* pTurret = m_pVehicle->GetVehicleWeaponMgr() ? m_pVehicle->GetVehicleWeaponMgr()->GetFirstTurretForSeat(m_iSeat) : NULL;
		if (pTurret)
		{
			CTaskMotionInTurret::IkHandsToTurret(rPed, *m_pVehicle, *pTurret);
		}
	}
#endif // ENABLE_MOTION_IN_TURRET_TASK	

	// Don't let the door close on us due to bullets
	CTaskVehicleFSM::SetDoorDrivingFlags(m_pVehicle, m_iTargetEntryPoint);

	float fPhase = 0.0f;
	const crClip *pClip = GetClipAndPhaseForState(fPhase);
	
	UpdateExtraGetInFixupPoints(pClip, fPhase);

	static dev_float MAX_TIME_TO_GET_IN = 8.0f;	
	if (!GetSubTask() || GetSubTask()->GetTimeInState() >= MAX_TIME_TO_GET_IN)
	{
#if __BANK
		if (GetSubTask())
		{
			float fPhase = -1.0f;
			const crClip* pClip = static_cast<CTaskEnterVehicleSeat*>(GetSubTask())->GetClipAndPhaseForState(fPhase);
			aiDisplayf("Vehicle %s (%s), Clip %s, ClipSet %s, Duration %.2f, Phase %.2f", m_pVehicle->GetModelName(), AILogging::GetDynamicEntityNameSafe(m_pVehicle), pClip ? pClip->GetName() : "NULL", m_moveNetworkHelper.GetClipSetId().GetCStr(), pClip ? pClip->GetDuration() : -1.0f, fPhase);
		}
		taskAssertf(0, "Ped %s stuck in enter seat state for longer than %.2f seconds, quitting out, movenetwork %s attached", GetPed()->GetDebugName() ? GetPed()->GetDebugName() : "NULL", MAX_TIME_TO_GET_IN, m_moveNetworkHelper.IsNetworkAttached() ? "WAS" : "WAS NOT");
#endif // __BANK
		SetTaskState(State_UnReserveSeat);
		return FSM_Continue;
	}

	// Safety code in case someone managed to enter the seat before us somehow
	if (!NetworkInterface::IsGameInProgress() && m_pVehicle->InheritsFromBike())
	{
		CPed* pOccupyingPed = GetPedOccupyingDirectAccessSeat();
		if (pOccupyingPed && !pOccupyingPed->GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle))
		{
			SetTaskState(State_UnReserveSeat);
			return FSM_Continue;
		}
	}

	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished) || m_bShouldBePutIntoSeat)
	{
		// Need to cache the exit vehicle check done when entering the seat
		if (GetSubTask())
		{
			m_bWantsToExit = static_cast<CTaskEnterVehicleSeat*>(GetSubTask())->WantsToExit();
		}

		// We're entering the direct access seat, store it here so we can pass it to the set ped in seat task
		m_iCurrentSeatIndex = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat, m_iSeat);
		taskAssert(m_iCurrentSeatIndex > -1);
        m_iLastCompletedState = State_EnterSeat;
        SetTaskState(State_WaitForPedToLeaveSeat);
	}
	return FSM_Continue;
}

void CTaskEnterVehicle::EnterSeat_OnExit()
{
	CPed* pPed = GetPed();
	if(pPed->IsLocalPlayer() && m_pVehicle && NetworkInterface::IsVehicleLockedForPlayer(m_pVehicle, pPed))
	{
		AI_LOG_WITH_ARGS("[EnterSeat_OnExit] - Ped %s being given TASK_EXIT_VEHICLE because they are being ejected due to vehicle being locked for this ped using IsLockedForPlayer() for vehicle: %s \n", pPed->GetNetworkObject()->GetLogName(), m_pVehicle->GetLogName());
		VehicleEnterExitFlags vehicleFlags;
		CTaskExitVehicle* pExitTask = rage_new CTaskExitVehicle(m_pVehicle, vehicleFlags);
		pExitTask->SetRunningFlag(CVehicleEnterExitFlags::DontWaitForCarToStop);
		CEventGivePedTask givePedTask(PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP, pExitTask, true );
		pPed->GetPedIntelligence()->AddEvent(givePedTask);
		return;
	}

	if (m_pVehicle && m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint))
	{
		CTaskVehicleFSM::ClearDoorDrivingFlags(m_pVehicle, m_iTargetEntryPoint);

		if( pPed->IsLocalPlayer() &&
			m_pVehicle && m_pVehicle->InheritsFromAutomobile() )
		{
			static_cast<CAutomobile*>( m_pVehicle.Get() )->SetMinHydraulicsGroundClearance();
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::WaitForPedToLeaveSeat_OnEnter()
{
    m_iWaitForPedTimer = 0;

    if(NetworkInterface::IsGameInProgress())
    {
        bool pedAlreadyInSeat = (m_iCurrentSeatIndex != -1) && (m_pVehicle->GetSeatManager()->GetPedInSeat(m_iCurrentSeatIndex) != 0);

        if(pedAlreadyInSeat)
        {
            const unsigned WAIT_FOR_PED_DURATION_MS = 3000; // This was 500ms, but has had to be increased due to poor framerate
            m_iWaitForPedTimer = WAIT_FOR_PED_DURATION_MS;
        }
    }
#if !__FINAL
	// needs to be in !__FINAL for output enabled __FINAL builds to compile
	aiDebugf1("Frame : %i, Ped %p, OnEnter m_iWaitForPedTimer = %i, Previous state %s", fwTimer::GetFrameCount(), GetPed(), m_iWaitForPedTimer, GetStaticStateName(GetPreviousState()));
#endif
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::WaitForPedToLeaveSeat_OnUpdate()
{
	CPed *pPed = GetPed();
	if (!pPed->GetIsAttached() && CheckPlayerExitConditions(*pPed, *m_pVehicle, m_iRunningFlags, GetTimeRunning()))
	{
		AI_LOG_WITH_ARGS("[EnterVehicle] - Ped %s is transitioning to finish state due to player exit conditions\n", AILogging::GetDynamicEntityNameSafe(pPed));
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

    bool bProcessedComponentReservations = ProcessComponentReservations();

	const bool bSeatIndexValid = m_pVehicle->IsSeatIndexValid(m_iCurrentSeatIndex);

#if __BANK
	if (pPed->IsLocalPlayer() || pPed->PopTypeIsMission())
	{
		AI_LOG_WITH_ARGS("[%s] - Ped %s is waiting for seat %i in vehicle %s to be free, ped in seat = %s\n", GetTaskName(), AILogging::GetDynamicEntityNameSafe(pPed), m_iCurrentSeatIndex, AILogging::GetDynamicEntityNameSafe(m_pVehicle), bSeatIndexValid ? AILogging::GetDynamicEntityNameSafe(m_pVehicle->GetSeatManager()->GetPedInSeat(m_iCurrentSeatIndex)) : "Invalid");
	}
#endif // __BANK

    // peds must wait for an existing ped to leave the seat they are being set in. This can happen in network games
    bool pedAlreadyInSeat = bSeatIndexValid && (m_pVehicle->GetSeatManager()->GetPedInSeat(m_iCurrentSeatIndex) != 0);

	const bool bIsNetworkGame = NetworkInterface::IsGameInProgress() ? true : false;
    if(pedAlreadyInSeat && bIsNetworkGame)
    {
		if (IsFlagSet(CVehicleEnterExitFlags::Warp) && bSeatIndexValid)
		{
			CPed* pSeatOccupier = m_pVehicle->GetSeatManager()->GetPedInSeat(m_iCurrentSeatIndex);
			if (pSeatOccupier && !pSeatOccupier->IsNetworkClone() && pSeatOccupier->ShouldBeDead() && !pSeatOccupier->GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle))
			{
				pSeatOccupier->SetPedOutOfVehicle(0);
				pSeatOccupier->GetPedIntelligence()->AddTaskDefault(pSeatOccupier->ComputeDefaultTask(*pSeatOccupier));
				aiWarningf("Network game : I have set %s ped %s out of the vehicle because ped %s has warped into seat %u ", pPed->IsNetworkClone() ? "Clone" : "Local", pSeatOccupier->GetModelName(), pPed->GetModelName(), m_iCurrentSeatIndex);
			}
		}	

        if(m_iWaitForPedTimer == 0)
        {
            if(pPed->GetMyVehicle() == m_pVehicle && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
            {
                pPed->SetPedOutOfVehicle();
            }

			AI_LOG_WITH_ARGS("[%s] - Ped %s transitioning to finish state because ped %s is in our target seat %i, m_iWaitForPedTimer = 0", GetTaskName(), AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetDynamicEntityNameSafe(m_pVehicle->GetSeatManager()->GetPedInSeat(m_iCurrentSeatIndex)), m_iCurrentSeatIndex);
            SetTaskState(State_Finish);
        }

		aiDebugf1("Frame : %i, Ped %p, m_iWaitForPedTimer = %i", fwTimer::GetFrameCount(), pPed, m_iWaitForPedTimer);
        m_iWaitForPedTimer -= static_cast<s16>(GetTimeStepInMilliseconds());
        m_iWaitForPedTimer = MAX(0, m_iWaitForPedTimer);
    }
    else
    {
        if (!bProcessedComponentReservations)
	    {
		    return FSM_Continue;
	    }

        m_iWaitForPedTimer = 0;
		
		CPed* pSeatOccupier = NULL;

		if (bSeatIndexValid)
		{
			// When we warp in we may not have the direct entry point set correctly, so get the occupier in the target seat
			if (IsFlagSet(CVehicleEnterExitFlags::Warp))
			{
				pSeatOccupier = m_pVehicle->GetSeatManager()->GetPedInSeat(m_iCurrentSeatIndex);
			}
			else
			{
				pSeatOccupier = GetPedOccupyingDirectAccessSeat();
			}
		}

		// Single player peds being warped in set the occupying ped out
		if (!bIsNetworkGame && pSeatOccupier && IsFlagSet(CVehicleEnterExitFlags::Warp))
		{
			pSeatOccupier->SetPedOutOfVehicle(CPed::PVF_Warp|CPed::PVF_ClearTaskNetwork);
			pSeatOccupier->GetPedIntelligence()->AddTaskDefault(pSeatOccupier->ComputeDefaultTask(*pSeatOccupier));
			aiWarningf("I have set %s ped %s out of the vehicle because ped %s has warped into seat %u ", pPed->IsNetworkClone() ? "Clone" : "Local", pSeatOccupier->GetModelName(), pPed->GetModelName(), m_iCurrentSeatIndex);
			pSeatOccupier = NULL;
		}

		bool bShouldFinish = !bSeatIndexValid;

		if (!bShouldFinish)
		{
			if (CheckForEnteringSeat(pSeatOccupier, IsFlagSet(CVehicleEnterExitFlags::Warp), m_iLastCompletedState == State_PickUpBike || m_iLastCompletedState == State_PullUpBike || m_iLastCompletedState == State_SetUprightBike))
			{
				// Wait until the ped has stopped shuffling before putting him in
				if (!IsSeatOccupierShuffling(pSeatOccupier))
				{
					SetTaskState(State_SetPedInSeat);
				}
				return FSM_Continue;
			}
			else
			{
				AI_LOG_WITH_ARGS("[%s] - %s ped %s, couldn't enter seat occupier = %s ped %s, m_iLastCompletedState = %s\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(pPed), AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetDynamicEntityIsCloneStringSafe(pSeatOccupier), AILogging::GetDynamicEntityNameSafe(pSeatOccupier), GetStaticStateName(m_iLastCompletedState));

				if (bIsNetworkGame && IsFlagSet(CVehicleEnterExitFlags::Warp))
				{
					AI_LOG_WITH_ARGS("[%s] - %s ped %s is waiting to get the seat reservation for seat %u in order to warp in\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(pPed), AILogging::GetDynamicEntityNameSafe(pPed), m_iCurrentSeatIndex);
					return FSM_Continue;
				}
				else
				{
					bShouldFinish = true;
				}
			}
		}

		AI_LOG_WITH_ARGS("[%s] - Failed to add %s ped %s to seat %u because it was occupied by %s ped %s, IsFlagSet(CVehicleEnterExitFlags::Warp) ? %s, m_bWarping  ? %s, bShouldFinish = %s\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(pPed), pPed->GetDebugName(), m_iCurrentSeatIndex, AILogging::GetDynamicEntityIsCloneStringSafe(pSeatOccupier), AILogging::GetDynamicEntityNameSafe(pSeatOccupier), AILogging::GetBooleanAsString(IsFlagSet(CVehicleEnterExitFlags::Warp)), AILogging::GetBooleanAsString(m_bWarping), AILogging::GetBooleanAsString(bShouldFinish));
		if (bShouldFinish)
		{
			SetTaskState(State_Finish);
			return FSM_Continue;
		}
	}
    return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::GivePedRewards()
{
	if(m_pVehicle)
	{
		CPed* pPed = GetPed();

		CRewardedVehicleExtension* pExtention = pPed->GetExtension<CRewardedVehicleExtension>();
		if(!pExtention)
		{
			pExtention = rage_new CRewardedVehicleExtension();
			Assert(pExtention);
			pPed->GetExtensionList().Add(*pExtention);
		}

		if(pExtention)
		{
			pExtention->GivePedRewards(pPed, m_pVehicle);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::WarpPedToGetInPointIfNecessary(CPed& rPed, const CVehicle& rVeh)
{
	if (ShouldUseClimbUpAfterOpenDoor(m_iTargetEntryPoint, &rVeh))
	{
		Vector3 vGetInPos(Vector3::ZeroType);
		Quaternion qGetInRot(Quaternion::IdentityType);
		ComputeGetInTarget(vGetInPos, qGetInRot);
		Matrix34 getInMtx;
		getInMtx.FromQuaternion(qGetInRot);
		getInMtx.d = vGetInPos;
		rPed.SetMatrix(getInMtx);
	}
}

////////////////////////////////////////////////////////////////////////////////

s32 CTaskEnterVehicle::FindClosestSeatToPedWithinWarpRange(const CVehicle& rVeh, const CPed& rPed, bool& bCanWarpOnToSeat) const
{
	s32 iClosestSeatIndex = -1;
	float fClosestDistSqd = 999.0f;

	const Vector3 vPedPos = VEC3V_TO_VECTOR3(rPed.GetTransform().GetPosition());
	for (s32 i=0; i<rVeh.GetSeatManager()->GetMaxSeats(); ++i)
	{
		// Player in sp cannot warp onto anything other than the drivers seat
		if (!NetworkInterface::IsGameInProgress() && rPed.IsLocalPlayer() && i != rVeh.GetDriverSeat())
		{
			continue;
		}

		if (rVeh.IsSeatIndexValid(i))
		{
			const CVehicleSeatAnimInfo* pSeatAnimInfo = rVeh.GetSeatAnimationInfo(i);
			if (pSeatAnimInfo->GetWarpIntoSeatIfStoodOnIt())
			{
				const s32 iEntryPoint = rVeh.GetDirectEntryPointIndexForSeat(i);
				if (rVeh.IsEntryIndexValid(iEntryPoint))
				{
					if (!rVeh.GetPedInSeat(i))
					{
						const CComponentReservation* pComponentReservation = const_cast<CVehicle&>(rVeh).GetComponentReservationMgr()->GetSeatReservationFromSeat(i);
						if (!pComponentReservation->GetPedUsingComponent())
						{
							Vector3 vSeatPosition;
							Quaternion qSeatOrientation;
							CModelSeatInfo::CalculateSeatSituation(&rVeh, vSeatPosition, qSeatOrientation, iEntryPoint, i);
							TUNE_GROUP_FLOAT(VEHICLE_ENTRY_TUNE, MAX_WARP_ON_SEAT_DIST, 0.5f, 0.0f, 2.0f, 0.01f);
							float fDistSqd = (vSeatPosition - vPedPos).XYMag2();
							if (fDistSqd < fClosestDistSqd && fDistSqd < square(MAX_WARP_ON_SEAT_DIST))
							{
								bCanWarpOnToSeat = true;
								iClosestSeatIndex = i;
								fClosestDistSqd = fDistSqd;
							}
						}
					}
				}
			}
		}
	}
	return iClosestSeatIndex;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::CNC_ShouldForceMichaelVehicleEntryAnims(const CPed& ped, bool bSkipCombatEntryFlagCheck)
{
	TUNE_GROUP_BOOL(CNC_RESPONSIVENESS, bCopsUseMichaelVehicleEntryAnims, true);
	if (!bCopsUseMichaelVehicleEntryAnims)
	{
		return false;
	}

	// CNC: Cops should use Michael's entry clipset (less aggressive anims).
	if (NetworkInterface::IsInCopsAndCrooks())
	{
		if (!bSkipCombatEntryFlagCheck)
		{
			const CPedIntelligence* pIntelligence = ped.GetPedIntelligence();
			if (pIntelligence)
			{
				const CTaskEnterVehicle* pEnterVehicleTask = static_cast<const CTaskEnterVehicle*>(pIntelligence->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
				if (pEnterVehicleTask && pEnterVehicleTask->IsFlagSet(CVehicleEnterExitFlags::CombatEntry))
				{
					return false;
				}
			}
		}

		const CPlayerInfo* pPlayerInfo = ped.GetPlayerInfo();
		if (pPlayerInfo && pPlayerInfo->GetArcadeInformation().GetTeam() == eArcadeTeam::AT_CNC_COP)
		{
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::GetAlternateClipInfoForPed(const CPed& ped, fwMvClipSetId& clipSetId, fwMvClipId& clipId, bool& bForceFlee) const
{
	// CNC: Cops should use Michael's entry clipset (less aggressive anims).
	if (CNC_ShouldForceMichaelVehicleEntryAnims(ped) && !IsFlagSet(CVehicleEnterExitFlags::CombatEntry))
	{
		const CVehicleEntryPointAnimInfo* pEntryPointClipInfo = m_pVehicle->GetVehicleModelInfo()->GetEntryPointAnimInfo(m_iTargetEntryPoint);
		if (pEntryPointClipInfo)
		{
			const s32 iMichaelIndex = 0;
			fwFlags32 uInformationFlags = 0;
			fwMvClipSetId jackAlivePedClipSet = pEntryPointClipInfo->GetCustomPlayerJackingClipSet(iMichaelIndex, &uInformationFlags);
			if (jackAlivePedClipSet != CLIP_SET_ID_INVALID && CTaskVehicleFSM::IsVehicleClipSetLoaded(jackAlivePedClipSet))
			{
				fwMvClipId newClipId = pEntryPointClipInfo->GetAlternateJackFromOutSideClipId();
				if (newClipId != CLIP_SET_ID_INVALID)
				{
					clipSetId = jackAlivePedClipSet;
					clipId = newClipId;
					bForceFlee = (uInformationFlags.IsFlagSet(CClipSetMap::IF_JackedPedExitsWillingly));
					return;
				}
			}
		}
	}

	if (!NetworkInterface::IsGameInProgress())
	{
		if (ped.IsLocalPlayer() && !IsFlagSet(CVehicleEnterExitFlags::CombatEntry))
		{
			s32 iPlayerIndex = CPlayerInfo::GetPlayerIndex(ped);
			if (iPlayerIndex > -1)
			{
				const CVehicleEntryPointAnimInfo* pEntryPointClipInfo = m_pVehicle->GetVehicleModelInfo()->GetEntryPointAnimInfo(m_iTargetEntryPoint);
				if (pEntryPointClipInfo)
				{
					fwFlags32 uInformationFlags = 0;
					fwMvClipSetId jackAlivePedClipSet = pEntryPointClipInfo->GetCustomPlayerJackingClipSet(iPlayerIndex, &uInformationFlags);
					if (jackAlivePedClipSet != CLIP_SET_ID_INVALID && CTaskVehicleFSM::IsVehicleClipSetLoaded(jackAlivePedClipSet))
					{
						fwMvClipId newClipId = pEntryPointClipInfo->GetAlternateJackFromOutSideClipId();
						if (newClipId != CLIP_SET_ID_INVALID)
						{
							clipSetId = jackAlivePedClipSet;
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

const crClip* CTaskEnterVehicle::SetClipsForState()
{
	const CVehicle& rVeh = *m_pVehicle;
	CPed& rPed = *GetPed();
	const s32 iState = GetState();
	const CVehicleEntryPointAnimInfo& rEntryAnimInfo = *rVeh.GetEntryAnimInfo(m_iTargetEntryPoint);
	const bool bFromWater = IsFlagSet(CVehicleEnterExitFlags::InWater) && rEntryAnimInfo.GetHasGetInFromWater();

	bool bIsOnTurretStandingJack = false;
	fwMvClipSetId clipSetId = m_moveNetworkHelper.GetClipSetId();
	taskAssertf(clipSetId != CLIP_SET_ID_INVALID, "ClipSetId was invalid");
	s32 nSeatID = -1;
	if (iState == State_JackPedFromOutside)
	{
		nSeatID = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat, m_iSeat);
		bIsOnTurretStandingJack = IsOnVehicleEntry() && rVeh.IsTurretSeat(nSeatID) && CTaskVehicleFSM::CanPerformOnVehicleTurretJack(rVeh, nSeatID) && rEntryAnimInfo.GetEntryPointClipSetId(1) != CLIP_SET_ID_INVALID;

		if (rEntryAnimInfo.GetUseSeatClipSetAnims())
		{
		const CVehicleSeatAnimInfo* pSeatAnimInfo =rVeh.GetSeatAnimationInfo(nSeatID);
		clipSetId = pSeatAnimInfo->GetSeatClipSetId();
		taskAssert(clipSetId != CLIP_SET_ID_INVALID);
	}
		else if (bIsOnTurretStandingJack)
		{
			// Ideally the on vehicle jack positions would be real entry nodes
			clipSetId = rEntryAnimInfo.GetEntryPointClipSetId(1);
		}
	}

	fwMvClipId clipId = CLIP_ID_INVALID;
	fwMvClipId moveClipId = CLIP_ID_INVALID;
	bool bValidState = false;
	bool bOnVehicleJack = false;

	switch (iState)
	{
		case State_ClimbUp:
		{
			bool bDoorIntact = true;
			if (ShouldUseClimbUpAfterOpenDoor(m_iTargetEntryPoint, &rVeh))
			{
				const CEntryExitPoint* pEntryPoint = rVeh.GetEntryExitPoint(m_iTargetEntryPoint);
				if (pEntryPoint)
				{
					const CCarDoor* pDoor = rVeh.GetDoorFromBoneIndex(pEntryPoint->GetDoorBoneIndex());
					if (!pDoor || !pDoor->GetIsIntact(&rVeh))
					{
						bDoorIntact = false;
					}
				}
			}

			moveClipId = ms_ClimbUpClipId; 
			clipId = bDoorIntact ? ms_Tunables.m_DefaultClimbUpClipId : ms_Tunables.m_DefaultClimbUpNoDoorClipId;	
			bValidState = true;
		}
		break;
		case State_JackPedFromOutside:
			{
				bOnVehicleJack = IsFlagSet(CVehicleEnterExitFlags::EnteringOnVehicle) && rEntryAnimInfo.GetHasGetInFromWater() && rEntryAnimInfo.GetUseStandOnGetIn();

				moveClipId = ms_JackPedClipId;

#if ENABLE_MOTION_IN_TURRET_TASK
				if (bIsOnTurretStandingJack)
				{
					const CVehicleEntryPointInfo& rEntryInfo = *rVeh.GetEntryInfo(m_iTargetEntryPoint);
					if (CTaskMotionInTurret::ShouldUseAlternateJackDueToTurretOrientation(*m_pVehicle, rEntryInfo.GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT))
					{
						clipId = m_bWasJackedPedDeadInitially ? ms_Tunables.m_DefaultJackDeadPedFromOutsideAltClipId : ms_Tunables.m_DefaultJackAlivePedFromOutsideAltClipId;
					}
					else
					{
						clipId = m_bWasJackedPedDeadInitially ? ms_Tunables.m_DefaultJackDeadPedFromOutsideClipId : ms_Tunables.m_DefaultJackAlivePedFromOutsideClipId;
					}
				}
				else
#endif //ENABLE_MOTION_IN_TURRET_TASK
				if(bOnVehicleJack)
				{
					if(bFromWater)
					{
						clipId = m_bWasJackedPedDeadInitially ? ms_Tunables.m_DefaultJackDeadPedOnVehicleIntoWaterClipId : ms_Tunables.m_DefaultJackPedOnVehicleIntoWaterClipId;
					}
					else
					{
						clipId = m_bWasJackedPedDeadInitially ? ms_Tunables.m_DefaultJackDeadPedFromOnVehicleClipId : ms_Tunables.m_DefaultJackPedFromOnVehicleClipId;
					}
				}
				else if (bFromWater)
				{
					clipId = m_bWasJackedPedDeadInitially ? ms_Tunables.m_DefaultJackDeadPedFromWaterClipId : ms_Tunables.m_DefaultJackAlivePedFromWaterClipId;
				}
				else
				{
					clipId = ms_Tunables.m_DefaultJackAlivePedFromOutsideClipId;
					
					//! If a dead clip exists, use it.
					if(m_bWasJackedPedDeadInitially && fwClipSetManager::GetClip(clipSetId, ms_Tunables.m_DefaultJackDeadPedFromOutsideClipId))
					{
						clipId = ms_Tunables.m_DefaultJackDeadPedFromOutsideClipId;
					}

					if (!m_bWasJackedPedDeadInitially && !IsFlagSet(CVehicleEnterExitFlags::CombatEntry))
					{
						bool bForceFlee = false;
						GetAlternateClipInfoForPed(rPed, clipSetId, clipId, bForceFlee);
						if(bForceFlee && m_pJackedPed && m_pJackedPed->PopTypeIsRandom() && !m_pJackedPed->IsLawEnforcementPed())
						{
							SetRunningFlag(CVehicleEnterExitFlags::ForceFleeAfterJack);
						}
					}
				}
				bValidState = true;
			}
			break;
		case State_StreamedEntry:
			{
				if (taskVerifyf(m_StreamedClipSetId.IsNotNull() && m_StreamedClipId.IsNotNull(), "Invalid ClipsetId (%s) or ClipId (%s)", m_StreamedClipSetId.TryGetCStr() ? m_StreamedClipSetId.GetCStr() : "NULL", m_StreamedClipId.TryGetCStr() ? m_StreamedClipId.GetCStr() : "NULL"))
				{
					clipId = m_StreamedClipId;
					moveClipId = ms_StreamedEntryClipId;

					if (!rPed.IsNetworkClone() && taskVerifyf(m_pStreamedEntryClipSet, "Expected non null streamed entry clipset"))
					{
						m_StreamedClipSetId = m_pStreamedEntryClipSet->GetClipSetId();
						fwClipSet* pClipSet = fwClipSetManager::GetClipSet(m_StreamedClipSetId);
						if (taskVerifyf(pClipSet && pClipSet->GetClipItemCount() >= 0, "Expect non null clipset and at least one clip item"))
						{
							if (m_pStreamedEntryClipSet->GetRemoveWeapons())
							{
								CObject* pWeapon = rPed.GetWeaponManager()->GetEquippedWeaponObject();
								if (pWeapon && pWeapon->GetIsVisible())
								{
									pWeapon->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, false, true);
								}
							}
						}
					}
					clipSetId = m_StreamedClipSetId;
				}
				else
				{
					return NULL;
				}
			}
			break;
		default:
			break;
	}

	const crClip* pClip = fwClipSetManager::GetClip(clipSetId, clipId);
	if (!taskVerifyf(pClip, "Couldn't find clip %s from clipset %s for state %s", clipId.GetCStr(), clipSetId.GetCStr(), GetStaticStateName(iState)))
	{
		return NULL;
	}

	m_fLegIkBlendOutPhase = 0.0f;

	bool bFindLegIkBlendOutPhase = (iState == State_StreamedEntry) || (iState == State_ClimbUp);
	if (iState == State_JackPedFromOutside)
	{
#if __ASSERT
		VerifyPairedJackClip(bOnVehicleJack, bFromWater, rPed, clipSetId, clipId, *pClip);
#endif // __ASSERT

		const bool bJackedPedIsDead = m_pJackedPed && m_pJackedPed->ShouldBeDead();
		if ((!bJackedPedIsDead && rEntryAnimInfo.GetJackIncludesGetIn()) || (bJackedPedIsDead && rEntryAnimInfo.GetDeadJackIncludesGetIn()))
		{
			bFindLegIkBlendOutPhase = true;
		}
		else
		{
			// Don't blend it out if we're not including the get in
			m_fLegIkBlendOutPhase = 1.1f;
		}
	}

	if (bFindLegIkBlendOutPhase)
	{
		float fUnused;
		CClipEventTags::GetMoveEventStartAndEndPhases(pClip, ms_LegIkBlendOutId, m_fLegIkBlendOutPhase, fUnused);
	}

	m_moveNetworkHelper.SetClip(pClip, moveClipId);
	return pClip;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::ProcessFriendlyJackDelay(CPed *pPed, CVehicle *pVehicle)
{
	// If its possible to friendly jack, wait until we know for sure the intention
	if (CheckForOptionalJackOrRideAsPassenger(*pPed, *pVehicle))
	{
		if (!IsFlagSet(CVehicleEnterExitFlags::HasCheckedForFriendlyJack) && !IsFlagSet(CVehicleEnterExitFlags::WantsToJackFriendlyPed))
		{
			bool bNotTryingToJackAnyMore = false;
			const CControl* pControl = pPed->GetControlFromPlayer();
			if (pControl && !pControl->GetPedEnter().IsDown())
			{
				bNotTryingToJackAnyMore = true;
			}

			if (bNotTryingToJackAnyMore || 1000.0f * GetTimeRunning() > static_cast<float>(ms_Tunables.m_DurationHeldDownEnterButtonToJackFriendly + 100))
			{
				SetRunningFlag(CVehicleEnterExitFlags::HasCheckedForFriendlyJack);
				if (GetState() == State_PickDoor)
				{
					SetFlag(aiTaskFlags::RestartCurrentState);
				}
				else
				{
					SetTaskState(State_PickDoor);
				}
			}

			return true;
		}
	}

	return false;
}

bool CTaskEnterVehicle::ProcessOptionalForcedTurretDelay(CPed *pPed, CVehicle *pVehicle)
{
	// If its possible to friendly jack, wait until we know for sure the intention
	const s32 iTurretIndex = CheckForOptionalForcedEntryToTurretSeatAndGetTurretSeatIndex(*pPed, *pVehicle);
	if (pVehicle->IsSeatIndexValid(iTurretIndex))
	{
		if (!IsFlagSet(CVehicleEnterExitFlags::HasCheckedForFriendlyJack) && !IsFlagSet(CVehicleEnterExitFlags::WantsToJackFriendlyPed))
		{
			bool bNotTryingToJackAnyMore = false;
			const CControl* pControl = pPed->GetControlFromPlayer();
			if (pControl && !pControl->GetPedEnter().IsDown())
			{
				bNotTryingToJackAnyMore = true;
			}

			if (bNotTryingToJackAnyMore || 1000.0f * GetTimeRunning() > static_cast<float>(ms_Tunables.m_DurationHeldDownEnterButtonToJackFriendly + 100))
			{
				SetRunningFlag(CVehicleEnterExitFlags::HasCheckedForFriendlyJack);
				if (GetState() == State_PickDoor)
				{
					SetFlag(aiTaskFlags::RestartCurrentState);
				}
				else
				{
					SetTaskState(State_PickDoor);
				}
			}

			return true;
		}
	}

	return false;
}

bool CTaskEnterVehicle::ProcessOptionalForcedRearSeatDelay(CPed *pPed, CVehicle *pVehicle)
{
	// If its possible to enter rear seat for activities, wait until we know for sure the intention
	const s32 iRearSeatIndex = CheckForOptionalForcedEntryToRearSeatAndGetSeatIndex(*pPed, *pVehicle);
	if (pVehicle->IsSeatIndexValid(iRearSeatIndex))
	{
		if (!IsFlagSet(CVehicleEnterExitFlags::HasCheckedForFriendlyJack) && !IsFlagSet(CVehicleEnterExitFlags::WantsToJackFriendlyPed))
		{
			bool bNotTryingToJackAnyMore = false;
			const CControl* pControl = pPed->GetControlFromPlayer();
			if (pControl && !pControl->GetPedEnter().IsDown())
			{
				bNotTryingToJackAnyMore = true;
			}

			if (bNotTryingToJackAnyMore || 1000.0f * GetTimeRunning() > static_cast<float>(ms_Tunables.m_DurationHeldDownEnterButtonToJackFriendly + 100))
			{
				SetRunningFlag(CVehicleEnterExitFlags::HasCheckedForFriendlyJack);
				if (GetState() == State_PickDoor)
				{
					SetFlag(aiTaskFlags::RestartCurrentState);
				}
				else
				{
					SetTaskState(State_PickDoor);
				}
			}

			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::ProcessDoorSteps()
{
	// Make the door (steps come fully down if we're gonna just play the get in)
	if (ShouldUseClimbUpAfterOpenDoor(m_iTargetEntryPoint, m_pVehicle) && !CheckForOpenDoor())
	{
		s32 iDoorBoneIndex = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromEntryPoint(m_iTargetEntryPoint);
		CCarDoor* pDoor = m_pVehicle->GetDoorFromBoneIndex(iDoorBoneIndex);
		if (pDoor)
		{
			pDoor->SetTargetDoorOpenRatio(1.0f, CCarDoor::DRIVEN_AUTORESET|CCarDoor::DRIVEN_SPECIAL);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::CheckForBikeFallingOver(const CVehicle& rVeh)
{
	if (rVeh.InheritsFromBike())
	{
		TUNE_GROUP_FLOAT(BIKE_ENTER_TUNE, MAX_ANG_VELOCITY_TO_ENTER, 0.25f, 0.0f, 3.0f, 0.01f);
		TUNE_GROUP_FLOAT(BIKE_ENTER_TUNE, MAX_ANG_VELOCITY_TO_ENTER_NETWORK, 0.4f, 0.0f, 3.0f, 0.01f);
		const float fMaxAngVelSqd = square(NetworkInterface::IsGameInProgress() ? MAX_ANG_VELOCITY_TO_ENTER_NETWORK : MAX_ANG_VELOCITY_TO_ENTER);
		if (square(rVeh.GetAngVelocity().x) > fMaxAngVelSqd || square(rVeh.GetAngVelocity().y) > fMaxAngVelSqd)
		{
			return true;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::ShouldLeaveDoorOpenForGroupMembers(const CPed& rPed, s32 iSeatIndex, s32 iEntryPointIndex) const
{
	if (iSeatIndex > -1 && iEntryPointIndex > -1)
	{
		const CPedGroup* pPedsGroup = rPed.GetPedsGroup();
		if (pPedsGroup)
		{
			const CPed* pLeader = pPedsGroup->GetGroupMembership()->GetLeader();
			if (pLeader && pLeader == &rPed)
			{
				for (s32 i=0; i<CPedGroupMembership::LEADER; i++)
				{
					const CPed* pMember = pPedsGroup->GetGroupMembership()->GetMember(i);
					if (pMember && pMember != pLeader)
					{
						if (iSeatIndex == pMember->m_PedConfigFlags.GetPassengerIndexToUseInAGroup())
						{
							return true;
						}		

						if (pMember->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE))
						{
							s32 iTargetEntryPoint = pMember->GetPedIntelligence()->GetQueriableInterface()->GetTargetEntryPointForTaskType(CTaskTypes::TASK_ENTER_VEHICLE);
							if (iTargetEntryPoint == iEntryPointIndex)
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

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::InitExtraGetInFixupPoints()
{
	m_fExtraGetInFixupStartPhase = 0.0f;
	m_fExtraGetInFixupEndPhase = 1.0f;
	m_bDoingExtraGetInFixup = false;
	m_bFoundExtraFixupPoints = false;
	m_iCurrentGetInIndex = 0;
	
	Vector3 vpos;
	Quaternion quat;

	//! Check that we have a fixup point?
	for(int i = CExtraVehiclePoint::MAX_GET_IN_POINTS-1; i > 0; i--)
	{
		CExtraVehiclePoint::ePointType point = CExtraVehiclePoint::ePointType(CExtraVehiclePoint::GET_IN + i);

		if (CTaskVehicleFSM::ComputeTargetTransformInWorldSpace(*m_pVehicle, vpos, quat, m_iTargetEntryPoint, point))
		{
			m_bFoundExtraFixupPoints = true;
			break;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::CanUnreserveDoorEarly(const CPed& rPed, const CVehicle& rVeh)
{
	if (rPed.IsNetworkClone())
		return false;

	const bool bAllowEarlyDoorAndSeatUnreservationLayout = rVeh.GetLayoutInfo()->GetAllowEarlyDoorAndSeatUnreservation();
	if (!bAllowEarlyDoorAndSeatUnreservationLayout && !rVeh.GetEntryInfo(m_iTargetEntryPoint)->GetIsPlaneHatchEntry())
		return false;

	if (GetState() != State_ClimbUp)
		return false;

	TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, EARLY_DOOR_UNRESERVE_PHASE_LAYOUT, 0.25f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, EARLY_DOOR_UNRESERVE_PHASE_HATCH, 0.15f, 0.0f, 1.0f, 0.01f);
	float fPhase = 0.0f;
	GetClipAndPhaseForState(fPhase);
	const float fEarlyDoorUnreservationPhase = bAllowEarlyDoorAndSeatUnreservationLayout ? EARLY_DOOR_UNRESERVE_PHASE_LAYOUT : EARLY_DOOR_UNRESERVE_PHASE_HATCH;
	return fPhase >= fEarlyDoorUnreservationPhase;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::UpdateExtraGetInFixupPoints(const crClip *pClip, float fPhase)
{
	if(m_bFoundExtraFixupPoints)
	{
		u32 iOldGetInIndex = m_iCurrentGetInIndex;

		//! Reset if we go past. 
		if(m_bDoingExtraGetInFixup && (fPhase > m_fExtraGetInFixupEndPhase) )
		{
			m_iCurrentGetInIndex = 0;
			m_fExtraGetInFixupStartPhase = fPhase;
			m_fExtraGetInFixupEndPhase = 1.0f;
		}

		const CClipEventTags::CEventTag<CClipEventTags::CFixupToGetInPointTag>* pFixupGetInTag = 
			CClipEventTags::FindFirstEventTag<CClipEventTags::CFixupToGetInPointTag>(pClip, CClipEventTags::FixupToGetInPoint, m_fExtraGetInFixupStartPhase, 1.0f);

		if(pFixupGetInTag)
		{
			m_iCurrentGetInIndex = rage::Max(1, pFixupGetInTag->GetData()->GetGetInPointIndex());
			m_fExtraGetInFixupEndPhase = pFixupGetInTag->GetStart();
			m_bDoingExtraGetInFixup = true;
		}

		//! TO DO - Remove. This is the old system. It has been decremented in favour of "FixupToGetInPoint"
		CClipEventTags::Key IncrementGetInTag("IncrementGetIn",0x2c1698e8);
		if(CClipEventTags::FindEventPhase(pClip, IncrementGetInTag, m_fExtraGetInFixupEndPhase, m_fExtraGetInFixupStartPhase, 1.0f))
		{
			m_bDoingExtraGetInFixup = true;
			m_iCurrentGetInIndex = 1;
		}

		//! Re-cache initial offset if our fixup point changes.
		if(m_iCurrentGetInIndex != iOldGetInIndex)
		{
			StoreInitialOffsets();
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

// Look at the door handle when entering a vehicle
bool CTaskEnterVehicle::StartEnterVehicleLookAt(CPed *pPed, CVehicle *pVehicle)
{
	if(pPed && pVehicle)
	{
		// Look at the jacked ped or front seat ped.
		CPed* pSeatOccupier;
		
		if(!CheckForJackingPedFromOutside(pSeatOccupier))
		{
			for(int i = 0; i < pVehicle->GetSeatManager()->GetMaxSeats(); i++)
			{
				if(pVehicle->GetSeatInfo(i)->GetIsFrontSeat())
				{
					pSeatOccupier = pVehicle->GetSeatManager()->GetPedInSeat(i);
					if(pSeatOccupier)
					{
						break;
					}
				}
			}
		}

		if(pSeatOccupier)
		{
			pPed->GetIkManager().LookAt(
				m_uEnterVehicleLookAtHash,
				pSeatOccupier,
				-1,
				BONETAG_HEAD,
				NULL,
				LF_WHILE_NOT_IN_FOV|LF_NARROW_PITCH_LIMIT);

			return true;
		}

		const CEntryExitPoint* pEntryPoint = pVehicle->GetEntryExitPoint(m_iTargetEntryPoint);

		s32 doorHandleBoneIndex = pEntryPoint->GetDoorHandleBoneIndex();
		if(doorHandleBoneIndex > -1)
		{
			CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(pVehicle, m_iTargetEntryPoint);
			if(!pDoor)
			{
				return false;
			}
			
			Matrix34 matTargeToLook;

			// Look at the door handle if the door is close. Otherwise, try to look at somewhere above the target seat.
			static dev_float fMinRatio = 0.1f;
			if(pDoor->GetDoorRatio() > fMinRatio)
			{
				const s32 iSeatBoneIndex = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromSeat(GetTargetSeat());

				if(iSeatBoneIndex > -1)
				{
					pVehicle->GetGlobalMtx(iSeatBoneIndex, matTargeToLook);

					matTargeToLook.d += matTargeToLook.c * 0.5f;
					matTargeToLook.d += matTargeToLook.b * 0.5f;
				}
				else
				{
					return false;
				}
			}
			else
			{
				pVehicle->GetGlobalMtx(doorHandleBoneIndex, matTargeToLook);
			}

			pPed->GetIkManager().LookAt(
				m_uEnterVehicleLookAtHash,
				NULL,
				-1,
				BONETAG_INVALID,
				&(matTargeToLook.d),
				LF_WHILE_NOT_IN_FOV|LF_NARROW_PITCH_LIMIT);

			return true;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

#if __ASSERT
void CTaskEnterVehicle::VerifyPairedJackClip(bool bOnVehicleJack, bool bFromWater, const CPed& rPed, fwMvClipSetId clipSetId, fwMvClipId jackerClipId, const crClip& rJackerClip) const
{
	// The jacked ped will be set out of the vehicle via a move event called Detach,
	// that ped needs to be set out, before we attempt to set this ped in
	// the following code verifies the associated tags are correctly placed
	// NB: theres probably a better solution to this
	fwMvClipId pairedClipId = CLIP_ID_INVALID;

	if(bOnVehicleJack)
	{	
		if(bFromWater)
		{
			pairedClipId = m_bWasJackedPedDeadInitially ? CTaskExitVehicleSeat::ms_Tunables.m_DefaultBeJackedDeadPedOnVehicleIntoWaterClipId : CTaskExitVehicleSeat::ms_Tunables.m_DefaultBeJackedAlivePedOnVehicleIntoWaterClipId;
		}
		else
		{
			pairedClipId = m_bWasJackedPedDeadInitially ? CTaskExitVehicleSeat::ms_Tunables.m_DefaultBeJackedDeadPedOnVehicleClipId : CTaskExitVehicleSeat::ms_Tunables.m_DefaultBeJackedAlivePedOnVehicleClipId;
		}
	}
	else if (bFromWater)
	{
		pairedClipId = m_bWasJackedPedDeadInitially ? CTaskExitVehicleSeat::ms_Tunables.m_DefaultBeJackedDeadPedFromWaterClipId : CTaskExitVehicleSeat::ms_Tunables.m_DefaultBeJackedAlivePedFromWaterClipId;
	}
	else
	{
		pairedClipId = CTaskExitVehicleSeat::ms_Tunables.m_DefaultBeJackedAlivePedFromOutsideClipId;

		//! If a dead clip exists, use it.
		if(m_bWasJackedPedDeadInitially && fwClipSetManager::GetClip(clipSetId, CTaskExitVehicleSeat::ms_Tunables.m_DefaultBeJackedDeadPedFromOutsideClipId))
		{
			pairedClipId = CTaskExitVehicleSeat::ms_Tunables.m_DefaultBeJackedDeadPedFromOutsideClipId;
		}

		if (!m_bWasJackedPedDeadInitially && !IsFlagSet(CVehicleEnterExitFlags::CombatEntry))
		{
			bool bForceFlee = false;
			GetAlternateClipInfoForPed(rPed, clipSetId, pairedClipId, bForceFlee);
		}
	}
	const crClip* pPairedClip = fwClipSetManager::GetClip(clipSetId, pairedClipId);
	if (taskVerifyf(pPairedClip, "Couldn't find clip %s from clipset %s", pairedClipId.GetCStr(), clipSetId.GetCStr()))
	{
		float fUnused = 1.0f;
		float fEarliestInterruptPhase = 1.0f;
		CClipEventTags::GetMoveEventStartAndEndPhases(&rJackerClip, CTaskEnterVehicleSeat::ms_CloseDoorInterruptId, fEarliestInterruptPhase, fUnused);
		float fEarliestSetPedInDuration = rJackerClip.GetDuration() * fEarliestInterruptPhase;
		float fEarliestBeJackDetachPhase = 0.0f;
		CClipEventTags::GetMoveEventStartAndEndPhases(pPairedClip, CTaskExitVehicleSeat::ms_DetachId, fEarliestBeJackDetachPhase, fUnused);
		float fBeJackedDetachDuration = pPairedClip->GetDuration() * fEarliestBeJackDetachPhase;
		taskAssertf(fBeJackedDetachDuration < fEarliestSetPedInDuration, "ADD A BUG TO DEFAULT ANIM INGAME - Clip : %s, Clipset : %s, Detach tag (Phase : %.2f, Duration %.2f) should come before end of jack animation's close door interrupt tag %s (Phase : %.2f, Duration %.2f)", clipSetId.GetCStr(), pairedClipId.GetCStr(), fEarliestBeJackDetachPhase, fBeJackedDetachDuration, jackerClipId.GetCStr(), fEarliestInterruptPhase, fEarliestSetPedInDuration);
	}
}
#endif // __ASSERT

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::SetSeatRequestType(SeatRequestType seatRequestType, s32 BANK_ONLY(changeReason), s32 BANK_ONLY(iDebugFlags))
{
#if __BANK
	TUNE_GROUP_BOOL(VEHICLE_ENTRY_DEBUG, RENDER_SET_SEAT_REQUEST_DEBUG_FOR_ALL_PEDS, false);
	if (RENDER_SET_SEAT_REQUEST_DEBUG_FOR_ALL_PEDS || GetPed()->IsPlayer())
	{
		CSetSeatRequestTypeDebugInfo instance(seatRequestType, changeReason, this, iDebugFlags);
		instance.Print();
	}
#endif // __BANK
	m_iSeatRequestType = seatRequestType;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::SetTargetEntryPoint(s32 iTargetEntryPoint, s32 BANK_ONLY(changeReason), s32 BANK_ONLY(iDebugFlags))
{
#if __BANK
	TUNE_GROUP_BOOL(VEHICLE_ENTRY_DEBUG, RENDER_SET_TARGET_ENTRY_DEBUG_FOR_ALL_PEDS, false);
	if (RENDER_SET_TARGET_ENTRY_DEBUG_FOR_ALL_PEDS || GetPed()->IsPlayer() || GetPed()->PopTypeIsMission())
	{
		CSetTargetEntryPointDebugInfo instance(iTargetEntryPoint, changeReason, this, iDebugFlags);
		instance.Print();
	}
#endif // __BANK
	m_iTargetEntryPoint = iTargetEntryPoint;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::SetTargetSeat(s32 iTargetSeat, s32 BANK_ONLY(changeReason), s32 BANK_ONLY(iDebugFlags))
{
#if __BANK
	TUNE_GROUP_BOOL(VEHICLE_ENTRY_DEBUG, RENDER_SET_TARGET_SEAT_DEBUG_FOR_ALL_PEDS, false);
	if (RENDER_SET_TARGET_SEAT_DEBUG_FOR_ALL_PEDS || GetPed()->IsPlayer() || GetPed()->PopTypeIsMission())
	{
		CSetTargetSeatDebugInfo instance(iTargetSeat, changeReason, this, iDebugFlags);
		instance.Print();
	}
#endif // __BANK

	if (iTargetSeat == m_pVehicle->GetDriverSeat() && !m_pVehicle->IsAnExclusiveDriverPedOrOpenSeat(GetPed()))
	{
		AI_LOG_WITH_ARGS("Ped %s trying to enter drivers seat (%i) but isn't an exclusive driver, SET_TARGET_SEAT above is NOT being applied", AILogging::GetDynamicEntityNameSafe(GetPed()), iTargetSeat);
		return;
	}

	m_iSeat = iTargetSeat;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::SetTaskFinishState(s32 BANK_ONLY(iFinishReason), s32 BANK_ONLY(iDebugFlags))
{
#if __BANK
	TUNE_GROUP_BOOL(VEHICLE_ENTRY_DEBUG, RENDER_SET_FINISH_STATE_FOR_ALL_PEDS, false);
	if (RENDER_SET_FINISH_STATE_FOR_ALL_PEDS || GetPed()->IsPlayer() || GetPed()->PopTypeIsMission())
	{
		CSetTaskFinishStateDebugInfo instance(iFinishReason, this, iDebugFlags);
		instance.Print();
	}
#endif // __BANK
	SetState(State_Finish);
}

////////////////////////////////////////////////////////////////////////////////

u32 CTaskEnterVehicle::GetAttachToVehicleFlags(const CVehicle* pVehicle)
{
	s32 uFlags = ATTACH_STATE_PED_ENTER_CAR | ATTACH_FLAG_COL_ON;
	// B*2293241: Disable collision when entering a boat.
	if (pVehicle && (pVehicle->InheritsFromBoat() || (pVehicle->InheritsFromAmphibiousAutomobile() && pVehicle->GetIsInWater())))
	{
		uFlags &= ~ATTACH_FLAG_COL_ON;
	}
	return uFlags;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::SetPedInSeat_OnEnter()
{
	// Don't ever warp onto these seats because if they be intersecting something we'll ragdoll off immediately
	if (m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint) && m_pVehicle->IsSeatIndexValid(m_iSeat))
	{
		// Set warp entry timer (used to prevent instant warp out if mashing entry button)
		CPed* pPed = GetPed();
		if (pPed && m_bWarping)
		{
			CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo();
			if(pPlayerInfo)
			{
				pPlayerInfo->SetLastTimeWarpedIntoVehicle();
			}
		}

		const CVehicleSeatAnimInfo* pSeatAnimInfo = m_pVehicle->GetSeatAnimationInfo(m_iSeat);
		if (pSeatAnimInfo && pSeatAnimInfo->GetKeepCollisionOnWhenInVehicle())
		{
			if (!CVehicle::IsEntryPointClearForPed(*m_pVehicle, *pPed, m_iTargetEntryPoint))
			{
				AI_LOG_WITH_ARGS("Ped %s is quitting enter vehicle task because entry point %i isn't clear", AILogging::GetDynamicEntityNameSafe(pPed), m_iTargetEntryPoint);
				return FSM_Quit;
			}
		}
	}

	//Ensure this is setup on enter - so it's immediately available for accessing and knowing we are about to shuffle
	m_bWantsToShuffle = CheckForShuffle();

	//B*1827739: Set flag if we're warping into vehicle. 
	//Flag checked in camGameplayDirector::UpdateStartFollowingVehicl; sets follow cam to directly behind vehicle to avoid seeing ped pop into vehicle
	if (m_pVehicle->GetEntryInfo(m_iTargetEntryPoint) && m_pVehicle->GetEntryInfo(m_iTargetEntryPoint)->GetMPWarpInOut())
	{
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_IsWarpingIntoVehicleMP, true);
	}

	// Keep a note that a ped just entered a car who's in the players group
	PedEnteredVehicle();

	// Force an ai/anim update if warping to update the peds pose this frame
	s32 iSetPedInVehicleFlags = IsFlagSet(CVehicleEnterExitFlags::Warp) ? CPed::PVF_Warp : CPed::PVF_KeepInVehicleMoveBlender;

	if (m_bShouldBePutIntoSeat)
	{
		iSetPedInVehicleFlags |= CPed::PVF_KeepCurrentOffset;
	}

	if (IsFlagSet(CVehicleEnterExitFlags::DontApplyForceWhenSettingPedIntoVehicle))
	{
		iSetPedInVehicleFlags |= CPed::PVF_DontApplyEnterVehicleForce;
	}

	// Give vehicle rewards in SP and freemode MP.
	if(GetPed()->IsLocalPlayer() && (!NetworkInterface::IsGameInProgress() || NetworkInterface::IsInFreeMode()))
	{
		GivePedRewards();
	}

	//sometimes pretend occupants fail to be created before
	//a real occupant gets there and enters the vehicle
	//if so, clear out that stuff here
	if (m_pVehicle)
	{
		m_pVehicle->m_nVehicleFlags.bUsingPretendOccupants = false;
		m_pVehicle->m_nVehicleFlags.bFailedToResetPretendOccupants = false;
		m_pVehicle->GetIntelligence()->ResetPretendOccupantEventData();
		m_pVehicle->GetIntelligence()->FlushPretendOccupantEventGroup();
	}

	if (m_pVehicle && m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint))
	{
		const CVehicleEntryPointInfo* pEntryInfo = m_pVehicle->GetEntryInfo(m_iTargetEntryPoint);
		if (pEntryInfo)
		{
			CPed& rPed = *GetPed();
			if (pEntryInfo->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT)
			{
				rPed.SetPedResetFlag(CPED_RESET_FLAG_PedEnteredFromLeftEntry, true);
			}
		}
	}

	CPed* pPed = GetPed();
	if (pPed)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableCellphoneAnimations, true);
	}

    // Keep the in vehicle move network if in the vehicle already
	SetNewTask(rage_new CTaskSetPedInVehicle(m_pVehicle, m_iCurrentSeatIndex, iSetPedInVehicleFlags));
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::SetPedInSeat_OnUpdate()
{
	const float fTimeBeforeSkipCloseDoor = ms_Tunables.m_MaxTimeStreamClipSetInBeforeSkippingCloseDoor;

	CPed* pPed = GetPed();

	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !GetSubTask())
	{
		s32 iTurretSeat = 0;
		bool bUsesAlternate = false;
		if(CanShuffleToTurretSeat(*pPed,*m_pVehicle,iTurretSeat, bUsesAlternate))
		{
			if(bUsesAlternate)
			{
				SetRunningFlag(CVehicleEnterExitFlags::UseAlternateShuffleSeat);
			}
			else
			{
				ClearRunningFlag(CVehicleEnterExitFlags::UseAlternateShuffleSeat);
			}
			
			SetTargetSeat(iTurretSeat, TSR_FORCE_USE_TURRET, VEHICLE_DEBUG_DEFAULT_FLAGS);
		}

        if (pPed->IsLocalPlayer())
        {
            if(m_pVehicle && (m_pVehicle->GetStatus() == STATUS_WRECKED) && NetworkInterface::IsGameInProgress() && !pPed->ShouldBeDead())
            {
                SetTaskState(State_ExitVehicle);
                return FSM_Continue;
            }
        }

        // some of the logic in this function can cause network clones to finish the task before
        // they have shuffled to the correct seat if there are network delays, make sure the task
        // waits until either the task has finished on the local machine before quitting the task
        bool bCanFinishTaskNow = true;

        if(pPed->IsNetworkClone() && m_iCurrentSeatIndex != m_iSeat && GetStateFromNetwork() != State_Finish)
        {
            bCanFinishTaskNow = false;
        }

		m_bWantsToShuffle = CheckForShuffle();
		if ((!m_bWantsToShuffle && m_bWantsToExit) || pPed->IsFatallyInjured())
		{
            if(bCanFinishTaskNow)
            {
			    SetTaskState(State_Finish);
            }
			return FSM_Continue;
		}

		s32 iDesiredState = State_Finish;

		// There is a problem with instantly blending in the in vehicle network, so force ped to close door, otherwise we get a pop when accelerating
		if (!m_pVehicle->InheritsFromBike())
		{
			if (!m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint))
			{
				// Lookup the entry point from our seat
				const s32 iNewTargetEntryPoint = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed), m_pVehicle);
				SetTargetEntryPoint(iNewTargetEntryPoint, EPR_SETTING_PED_IN_SEAT, VEHICLE_DEBUG_DEFAULT_FLAGS);
			}
			
			if (m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint))
			{
				taskAssert(m_pVehicle->GetVehicleModelInfo());
				const CEntryExitPoint* pEntryPoint = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryExitPoint(m_iTargetEntryPoint);
				if (taskVerifyf(pEntryPoint, "Entry point invalid"))
				{
					const CVehicleEntryPointAnimInfo* pAnimInfo = m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint);	
					const bool bCanCloseDoor = pAnimInfo ? !pAnimInfo->GetDontCloseDoorInside() : false;
					const CVehicleEntryPointInfo* pEntryInfo =  m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint) ? m_pVehicle->GetEntryInfo(m_iTargetEntryPoint) : NULL;
					if (bCanCloseDoor && pEntryInfo && !pEntryInfo->GetIsPlaneHatchEntry() && !m_pVehicle->GetLayoutInfo()->GetAutomaticCloseDoor())
					{
						const bool bNeedsToHotwire = m_pVehicle->GetSeatManager()->GetDriver() == pPed && m_pVehicle->m_nVehicleFlags.bCarNeedsToBeHotwired;
						bool bShouldSkipCloseDoor = !m_pVehicle->GetLayoutInfo()->GetMustCloseDoor() && (GetTimeInState() > fTimeBeforeSkipCloseDoor || bNeedsToHotwire);
						const bool bDontCloseDoorToAction = !m_pVehicle->InheritsFromPlane() && (pPed->WantsToUseActionMode() || (pPed->GetPlayerWanted() && pPed->GetPlayerWanted()->GetWantedLevel() > WANTED_CLEAN));
						if (pPed->GetPedResetFlag(CPED_RESET_FLAG_DontCloseVehicleDoor) || (m_bWantsToShuffle && bDontCloseDoorToAction))
						{
							bShouldSkipCloseDoor = true;
						}

						if (!CTaskMotionInVehicle::CheckIfStairsConditionPasses(*m_pVehicle, *pPed))
						{
							bShouldSkipCloseDoor = true;
						}

						if (CTaskMotionInAutomobile::WantsToDuck(*pPed) || (m_bWantsToShuffle && m_pVehicle->IsTurretSeat(m_iSeat)))
						{
							if (pPed->IsLocalPlayer() && pPed->GetPlayerInfo()->IsAiming())
							{
								bShouldSkipCloseDoor = true;
							}
						}

						if (m_bWantsToShuffle && !bShouldSkipCloseDoor)
						{
							if (!bShouldSkipCloseDoor)
							{
								s32 iShuffleSeat = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetShuffleSeatForSeat(m_iTargetEntryPoint, m_iCurrentSeatIndex, IsFlagSet(CVehicleEnterExitFlags::UseAlternateShuffleSeat));
								if (m_pVehicle->IsSeatIndexValid(iShuffleSeat))
								{
									const CPed* pShufflePed = m_pVehicle->GetSeatManager()->GetPedInSeat(iShuffleSeat);
									if (pShufflePed)
									{
										bShouldSkipCloseDoor = true;
									}
								}
							}
							
							if (!bShouldSkipCloseDoor && ShouldLeaveDoorOpenForGroupMembers(*pPed, m_iCurrentSeatIndex, m_iTargetEntryPoint))
							{
								bShouldSkipCloseDoor = true;
							}
						}

						if (pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableInVehicleActions))
						{
							bShouldSkipCloseDoor = true;
						}

						const CCarDoor* pDoor = m_pVehicle->GetDoorFromBoneIndex(pEntryPoint->GetDoorBoneIndex());

						// Don't close door during entry if script are forcing it open
						if (pDoor && (pDoor->GetFlag(CCarDoor::DRIVEN_NORESET) || pDoor->GetFlag(CCarDoor::DRIVEN_NORESET_NETWORK)))
						{
							bShouldSkipCloseDoor = true;
						}

						if(!bShouldSkipCloseDoor && pDoor && pDoor->GetIsIntact(pPed->GetMyVehicle()) && !pDoor->GetIsClosed() && !IsFlagSet(CVehicleEnterExitFlags::DontCloseDoor) && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed) 
							&& CTaskCloseVehicleDoorFromInside::IsDoorClearOfPeds(*m_pVehicle, *pPed))
						{
							// Wait until clipset is loaded
							fwMvClipSetId entryClipsetId = CTaskEnterVehicleAlign::GetEntryClipsetId(m_pVehicle, m_iTargetEntryPoint, pPed);
							if (!CTaskVehicleFSM::IsVehicleClipSetLoaded(entryClipsetId) && !pPed->IsNetworkClone())
							{
								return FSM_Continue;
							}

							if (pPed->IsNetworkClone())
							{
								if (GetStateFromNetwork() > State_EnterSeat)
								{
									iDesiredState = State_CloseDoor;
								}
								else
								{
									return FSM_Continue;
								}
							}
							else
							{
								iDesiredState = State_ReserveDoorToClose;
							}
						}
						else 
						{
							if (m_bWantsToShuffle)
							{
								iDesiredState = State_ShuffleToSeat;
							}
						}	

					}
					else if (m_bWantsToShuffle)
					{
						iDesiredState = State_ShuffleToSeat;
					}

					if (pPed->IsNetworkClone() && iDesiredState == State_Finish && m_ClonePlayerWaitingForLocalPedAttachExitCar)
					{   //if clone has the iDesiredState left at finish, yet the destination seat has a local ped waiting to be detached allow to continue
						taskAssertf(pPed->IsPlayer(),"%s Only expect m_ClonePlayerWaitingForLocalPedAttachExitCar to be used on players",pPed->GetDebugName());

                        CNetObjPed *netObjPed = static_cast<CNetObjPed *>(pPed->GetNetworkObject());

                        if(netObjPed && netObjPed->GetPedsTargetSeat() == m_iSeat)
                        {
						    return FSM_Continue;
                        }
					}
				}
			}
		}

		// Clones need to wait before setting the shuffle state, because we can end up without the correct pose if the get in anim blends
		// out before the shuffle task can proceed
		if (pPed->IsNetworkClone() && iDesiredState == State_ShuffleToSeat)
		{
			const bool bPedRunningShuffleTask = pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE);
			if (!bPedRunningShuffleTask
				&& GetStateFromNetwork() < State_ShuffleToSeat)
			{
				return FSM_Continue;
			}

            if(!bPedRunningShuffleTask && bCanFinishTaskNow)
            {
                SetTaskState(State_Finish);
                return FSM_Continue;
            }

			const s32 iRemoteStateForShuffleTask = bPedRunningShuffleTask ? pPed->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE) : -1;
			if (iRemoteStateForShuffleTask > CTaskInVehicleSeatShuffle::State_ReserveShuffleSeat)
			{
				SetTaskState(iDesiredState);
			}
		}
		else
		{
            if(bCanFinishTaskNow || iDesiredState != State_Finish)
            {
			    SetTaskState(iDesiredState);
            }
		}
	}
    else
    {
        // if we get to this state before we have a valid seat index or there is still a ped in the seat
	    // we have to quit the task and rely on the network syncing code to place the ped in the vehicle
	    if (pPed->IsNetworkClone())
	    {
		    if (m_iCurrentSeatIndex == -1 || (m_pVehicle->GetSeatManager()->GetPedInSeat(m_iCurrentSeatIndex) != NULL))
		    {
			    SetTaskState(State_Finish);
			    return FSM_Continue;
		    }
	    }
    }
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::CanShuffleToTurretSeat(const CPed& rPed, const CVehicle& rVeh, s32& iTurretSeat, bool& BUsesAlternateShuffle)
{
	if (rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_PreventAutoShuffleToTurretSeat))
		return false;

	if (rPed.GetIsDrivingVehicle())
		return false;

	if (rPed.ShouldBeDead())
		return false;

	if (!rVeh.GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_TURRET_SEAT_ON_VEHICLE) ||
		!rVeh.GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_PREFER_ENTER_TURRET_AFTER_DRIVER))
		return false;

	const s32 iCurrentSeatIndex = rVeh.GetSeatManager()->GetPedsSeatIndex(&rPed);
	if (!rVeh.IsSeatIndexValid(iCurrentSeatIndex) || rVeh.IsTurretSeat(iCurrentSeatIndex))
		return false;

#if __BANK	// Don't auto-shuffle if we're forcing a non-turret seat in RAG
	if (CVehicleDebug::ms_bForcePlayerToUseSpecificSeat && !rVeh.IsTurretSeat(CVehicleDebug::ms_iSeatRequested))
		return false;
#endif // __BANK

	iTurretSeat = rVeh.GetFirstTurretSeat();

	const CModelSeatInfo* pModelSeatInfo = rVeh.GetVehicleModelInfo()->GetModelSeatInfo();
	const s32 iCurrentEntryIndex = pModelSeatInfo->GetEntryPointIndexForSeat(iCurrentSeatIndex, &rVeh);
	if (!rVeh.IsEntryIndexValid(iCurrentEntryIndex))
		return false;

	const s32 iShuffleSeat1 = pModelSeatInfo->GetShuffleSeatForSeat(iCurrentEntryIndex, iCurrentSeatIndex, false);
	const s32 iShuffleSeat2 = pModelSeatInfo->GetShuffleSeatForSeat(iCurrentEntryIndex, iCurrentSeatIndex, true);
	if (iTurretSeat != iShuffleSeat1 && iTurretSeat != iShuffleSeat2)
		return false;

	BUsesAlternateShuffle = iTurretSeat == iShuffleSeat2;

	CPed* pOccupier = CTaskVehicleFSM::GetPedInOrUsingSeat(rVeh, iTurretSeat);
	if (pOccupier && !pOccupier->ShouldBeDead())
	{
		return false;
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::ReserveDoorToClose_OnUpdate()
{
	if (TryToReserveDoor(*GetPed(), *m_pVehicle, m_iTargetEntryPoint))
	{
		SetTaskState(State_CloseDoor);
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::CloseDoor_OnEnter()
{
    CPed *pPed = GetPed();

    // if we get to this state before we have a valid seat or entry point index we have to
    // quit the task and rely on the network syncing code to place the ped in the vehicle
    if(pPed && pPed->IsNetworkClone() && (m_iTargetEntryPoint == -1 || m_iCurrentSeatIndex == -1 || !IsMoveTransitionAvailable(GetState())))
    {
        SetStateFromNetwork(State_Finish);
    }
    else
    {
        bool bShouldCloseDoor = true;

        if(pPed->IsNetworkClone())
        {
            if(!pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE) ||
               (pPed->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_ENTER_VEHICLE) != State_CloseDoor))
            {
                bShouldCloseDoor = false;
            }

            fwMvClipSetId entryClipsetId = CTaskEnterVehicleAlign::GetEntryClipsetId(m_pVehicle, m_iTargetEntryPoint, pPed);
            bool bAnimsLoaded = CTaskVehicleFSM::IsVehicleClipSetLoaded(entryClipsetId);

            if(!bAnimsLoaded)
            {
                bShouldCloseDoor = false;
            }
        }

        if(bShouldCloseDoor)
        {
	        StoreInitialOffsets();
	        SetNewTask(rage_new CTaskCloseVehicleDoorFromInside(m_pVehicle, m_iTargetEntryPoint, m_moveNetworkHelper));
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::CloseDoor_OnUpdate()
{
	CPed *pPed = GetPed();

	const CVehicle *pVehiclePedInside = pPed->GetVehiclePedInside();
	if (pVehiclePedInside && Verifyf(pVehiclePedInside == m_pVehicle, ""))
	{
		if(pVehiclePedInside->HasRaisedRoof())
		{
			const s32 iPedsSeatIndex = pVehiclePedInside->GetSeatManager()->GetPedsSeatIndex(pPed);
			if (pVehiclePedInside->IsSeatIndexValid(iPedsSeatIndex))
			{
				const CVehicleSeatInfo *pSeatInfo = pVehiclePedInside->GetSeatInfo(iPedsSeatIndex);
				if (Verifyf(pSeatInfo, "Could not find seat info for vehicle %s, seat index %i", pVehiclePedInside->GetModelName(), iPedsSeatIndex))
				{
					bool bDisableHairScale = false;
					const CVehicleModelInfo *pVehicleModelInfo = pVehiclePedInside->GetVehicleModelInfo();
					if(pVehicleModelInfo)
					{
						float fMinSeatHeight = pVehicleModelInfo->GetMinSeatHeight();
						float fHairHeight = pPed->GetHairHeight();

						if(fHairHeight >= 0.0f && fMinSeatHeight >= 0.0f)
						{
							const float fSeatHeadHeight = 0.59f; /* This is a conservative approximate measurement */
							if((fSeatHeadHeight + fHairHeight + CVehicle::sm_HairScaleDisableThreshold) < fMinSeatHeight)
							{
								bDisableHairScale = true;
							}
						}
					}

					float fHairScale = pVehiclePedInside->GetIsHeli() ? 0.0f : pSeatInfo->GetHairScale();
					if(fHairScale < 0.0f && !bDisableHairScale)
					{
						pPed->SetPedResetFlag(CPED_RESET_FLAG_OverrideHairScale, true);
						pPed->SetTargetHairScale(fHairScale);
					}
				}
			}
		}
	}

	// Force the do nothing motion state
	pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_DoNothing);
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !GetSubTask())
	{
        m_iLastCompletedState = State_CloseDoor;

		if (pPed->IsNetworkClone())
		{
            // if we skip from the align state directly to the close door state the clone ped
            // will not be in the vehicle, and we have to finish the task
			if ((m_iCurrentSeatIndex != -1) && CheckForShuffle())
			{
				SetTaskState(State_ShuffleToSeat);
			}
			else
			{
				SetTaskState(State_Finish);
			}
		}
		else
		{
			SetTaskState(State_UnReserveSeat);
		}
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::CheckForShuffle()
{
	m_ClonePlayerWaitingForLocalPedAttachExitCar = false;

	// Only shuffle it we aren't at our target seat, and the seat we can shuffle to is the target seat
	if( m_iCurrentSeatIndex != m_iSeat)
	{
		if( m_pVehicle && m_pVehicle->IsSeatIndexValid(m_iCurrentSeatIndex))
		{
			CPed* pPed = GetPed();

			if (m_iCurrentSeatIndex == 0 && pPed->IsLocalPlayer())
			{
				const bool bIsDune3 = MI_CAR_DUNE3.IsValid() && m_pVehicle->GetModelIndex() == MI_CAR_DUNE3;
				if (bIsDune3)
				{
					if (CTaskMotionInAutomobile::WantsToDuck(*pPed))
						return false;

					const CControl* pControl = pPed->GetControlFromPlayer();
					if (pControl)
					{
						if (pControl->GetVehicleAccelerate().IsDown())
							return false;

						if (pControl->GetVehicleBrake().IsDown())
							return false;
					}

					if (pPed->GetPlayerInfo()&& pPed->GetPlayerInfo()->IsAiming())
						return false;
				}
			}

			s32 iCurrentEntryPointIndex = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(m_iCurrentSeatIndex, m_pVehicle);
			taskFatalAssertf(m_pVehicle->GetEntryExitPoint(iCurrentEntryPointIndex), "NULL entry point associated with seat %i", m_iCurrentSeatIndex);
			s32 iShuffleSeat = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetShuffleSeatForSeat(iCurrentEntryPointIndex, m_iCurrentSeatIndex, IsFlagSet(CVehicleEnterExitFlags::UseAlternateShuffleSeat));
			if( iShuffleSeat == m_iSeat )
			{
				// Don't shuffle if there is a nearby friendly player trying to get into the driver seat
				if( NetworkInterface::IsGameInProgress() && pPed && pPed->IsLocalPlayer() && 
					iShuffleSeat == m_pVehicle->GetDriverSeat() )
				{
					CPed* pDriverPed = m_pVehicle->GetSeatManager() ? m_pVehicle->GetSeatManager()->GetPedInSeat(m_pVehicle->GetDriverSeat()) : NULL;
					if (pDriverPed && CTaskVehicleFSM::ArePedsFriendly(pDriverPed,pPed))
					{
						CControl* pControl = pPed->GetControlFromPlayer();
						if (pControl && !pControl->GetPedEnter().IsDown())
						{
							AI_LOG_WITH_ARGS("[EnterVehicle] Ped %s decided not to shuffle to drivers seat because it is occupied by a friendly Ped %s (and not trying to jack)\n",
								AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetDynamicEntityNameSafe(pDriverPed));
							m_iSeat = m_iCurrentSeatIndex;
							return false;
						}
					}

					if (CTaskVehicleFSM::IsAnyPlayerEnteringDirectlyAsDriver( pPed, m_pVehicle ))
					{
						AI_LOG_WITH_ARGS("[EnterVehicle] Ped %s decided not to shuffle to drivers seat because player entering directly as driver\n", AILogging::GetDynamicEntityNameSafe(pPed));
						return false;
					}
				}

				if (pPed->IsNetworkClone() && pPed->IsPlayer() && IsFlagSet(CVehicleEnterExitFlags::WantsToJackFriendlyPed) && GetStateFromNetwork() == State_Finish)
				{
					const CPed* pPedUsingSeat = CTaskVehicleFSM::GetPedInOrUsingSeat(*m_pVehicle, m_iSeat);

					if(pPedUsingSeat && !pPedUsingSeat->IsNetworkClone())
					{
						//B*1849564 if clones remote task has completed, and the local friendly ped who was to be jacked is still in his seat, then return false
						AI_LOG_WITH_ARGS("[EnterVehicle] Clone player ped %s decided not to shuffle to drivers seat because another ped %s is using the seat\n", AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetDynamicEntityNameSafe(pPedUsingSeat));
						return false;
					}
				}

				if (pPed->IsNetworkClone() && !IsFlagSet(CVehicleEnterExitFlags::WantsToJackFriendlyPed) && GetStateFromNetwork() != State_ShuffleToSeat)
				{
					const CPed* pPedUsingSeat = CTaskVehicleFSM::GetPedInOrUsingSeat(*m_pVehicle, m_iSeat);
					if (pPedUsingSeat && pPedUsingSeat != pPed && pPedUsingSeat->GetAttachState() != ATTACH_STATE_PED_EXIT_CAR)
					{
						if(pPed->IsPlayer() && !pPedUsingSeat->IsNetworkClone() && pPedUsingSeat->GetAttachState() != ATTACH_STATE_PED_EXIT_CAR)
						{
                            // don't wait forever for this to prevent the clone task getting permanently stuck
                            const float MAX_TIME_TO_WAIT = 2.0f;

                            if(gnetVerifyf(GetTimeInState() < MAX_TIME_TO_WAIT, "Waiting for local ped to exit car for too long!"))
                            {
                                BANK_ONLY(aiDisplayf("Frame %d: Clone ped %s (%p) waiting for local ped %s to exit seat (Time in state: %.2f)", fwTimer::GetFrameCount(), pPed->GetDebugName(), pPed, pPedUsingSeat->GetDebugName(), GetTimeInState()));
							    m_ClonePlayerWaitingForLocalPedAttachExitCar =true;
                            }
						}
						AI_LOG_WITH_ARGS("[EnterVehicle] Clone ped %s decided not to shuffle to drivers seat because another ped %s is using the seat\n", AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetDynamicEntityNameSafe(pPedUsingSeat));
						return false;
					}
				}
				return true;
			}
		}
	}
	AI_LOG_WITH_ARGS("[EnterVehicle] Ped %s decided not to shuffle to drivers seat because desired seat %i was the same as current seat %i\n", AILogging::GetDynamicEntityNameSafe(GetPed()), m_iSeat, m_iCurrentSeatIndex);
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::ProcessJackingStatus()
{
	SetRunningFlag(CVehicleEnterExitFlags::HasJackedAPed);

	if (NetworkInterface::IsGameInProgress() && m_pJackedPed)
	{
		bool bPedBeingJackedSetToOnlyBePulledOut = m_pJackedPed->GetPedConfigFlag(CPED_CONFIG_FLAG_JustGetsPulledOutWhenElectrocuted);

		// If we're a cop and the ped we're jacking is flagged to be arrestable, just pull them out
		if (CTaskPlayerOnFoot::IsAPlayerCopInMP(*GetPed()))
		{
			if ((!m_pJackedPed->ShouldBeDead() && m_pJackedPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CanBeArrested)) ||
				(m_pJackedPed->IsAPlayerPed() && m_pJackedPed->GetPlayerInfo() && m_pJackedPed->GetPlayerInfo()->GetWanted().GetWantedLevel() > WANTED_CLEAN))
			{
				bPedBeingJackedSetToOnlyBePulledOut = true;
			}
		}

		if (!bPedBeingJackedSetToOnlyBePulledOut)
		{		
			if (m_pJackedPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_DAMAGE_ELECTRIC))
			{
				CClonedDamageElectricInfo* pElectricDamageInfo = static_cast<CClonedDamageElectricInfo*>(m_pJackedPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_DAMAGE_ELECTRIC, PED_TASK_PRIORITY_MAX));
				if (pElectricDamageInfo)
				{
					m_bShouldTriggerElectricTask = true;
					m_ClipSetId = pElectricDamageInfo->GetClipSetId();
					m_uDamageTotalTime = pElectricDamageInfo->GetDamageTotalTime();
					m_RagdollType = pElectricDamageInfo->GetRagdollType();
				}

				bPedBeingJackedSetToOnlyBePulledOut = true;
			}
		}

		if (bPedBeingJackedSetToOnlyBePulledOut)
		{
			SetRunningFlag(CVehicleEnterExitFlags::JustPullPedOut);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::ProcessComponentReservations(bool bRequiresOnlyDoor)
{
	CPed& rPed = *GetPed();

	// Clones don't need to reserve
	if (rPed.IsNetworkClone())
	{
		return true;
	}

	// See if we can reserve the seat and door (if it exists)
	bool bReservationsComplete = false;
	bool bDoorReserved = false;

	if (m_pVehicle)
	{
		if (m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint))
		{
			// Climb up only points don't actually end up in a seat, so skip making reservations altogether
			const CVehicleEntryPointInfo* pEntryInfo = m_pVehicle->GetEntryInfo(m_iTargetEntryPoint);
			if (pEntryInfo->GetClimbUpOnly())
			{
				bReservationsComplete = true;
			}
			else
			{
				// Need to pass in the seat we're trying to get to for warp entry, otherwise we may end up trying to reserve the wrong seat
				const bool bIsWarpEntry = IsPedWarpingIntoVehicle(m_iTargetEntryPoint);
				bDoorReserved = TryToReserveDoor(rPed, *m_pVehicle, m_iTargetEntryPoint);
				const bool bSeatReserved = TryToReserveSeat(rPed, *m_pVehicle, m_iTargetEntryPoint, bIsWarpEntry ? m_iSeat : -1);

				if (bDoorReserved && bSeatReserved)
				{
					bReservationsComplete = true;
				}
			}
		}
		else if (m_bWarping && m_pVehicle->IsSeatIndexValid(m_iSeat))
		{
			if (TryToReserveSeat(rPed, *m_pVehicle, m_iTargetEntryPoint, m_iSeat))
			{
				bReservationsComplete = true;
			}
		}
	}

	return bReservationsComplete || (bRequiresOnlyDoor && bDoorReserved);
}

////////////////////////////////////////////////////////////////////////////////

CPed* CTaskEnterVehicle::GetPedOccupyingDirectAccessSeat() const
{
	if (m_iTargetEntryPoint > -1)
	{
		s32 iTargetSeat = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat, m_iSeat);
		return m_pVehicle->GetSeatManager()->GetPedInSeat(iTargetSeat);
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::WarpIfPossible()
{
	if (m_pVehicle)
	{
		if (m_pVehicle->GetCarDoorLocks() == CARLOCK_LOCKED)
		{
			return false;
		}

		if (GetPed()->IsLocalPlayer() && m_pVehicle->GetCarDoorLocks() == CARLOCK_LOCKOUT_PLAYER_ONLY)
		{
			return false;
		}
		
		if (m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint) && m_pVehicle->GetDoorLockStateFromEntryPoint(m_iTargetEntryPoint) == CARLOCK_LOCKED)
		{
			return false;
		}

		const bool bLockedForPlayer = CTaskVehicleFSM::IsVehicleLockedForPlayer(*m_pVehicle, *GetPed());
		if (bLockedForPlayer)
		{
			return false;
		}
	}

	//Ensure the state is valid.
	switch(GetState())
	{
		case State_GoToVehicle:
		case State_GoToDoor:
		{
			break;
		}
		default:
		{
			return false;
		}
	}

	//Set the flag.
	SetRunningFlag(CVehicleEnterExitFlags::Warp);
	return true;
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CTaskEnterVehicle::GetRequestedEntryClipSetIdForOpenDoor() const
{
	return CTaskEnterVehicleAlign::GetEntryClipsetId(m_pVehicle, m_iTargetEntryPoint, GetPed());
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CTaskEnterVehicle::GetRequestedCommonClipSetId() const
{
	return CTaskEnterVehicleAlign::GetCommonClipsetId(m_pVehicle, m_iTargetEntryPoint, IsFlagSet(CVehicleEnterExitFlags::EnteringOnVehicle));
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::CheckForClimbUp() const
{
	if (ShouldUseClimbUpAfterOpenDoor(m_iTargetEntryPoint, m_pVehicle))
	{
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::ShouldClimbUpOnToVehicle(CPed* pSeatOccupier, const CVehicleEntryPointAnimInfo& rAnimInfo, const CVehicleEntryPointInfo& rEntryInfo)
{
	if (!rAnimInfo.GetHasClimbUp())
		return false;

	if (IsFlagSet(CVehicleEnterExitFlags::EnteringOnVehicle))
		return false;

	if (!CheckForClimbUpIfLocked())
		return false;

	const s32 iSeatIndex = pSeatOccupier ? m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pSeatOccupier) : -1;
	const bool bIsBarrageRearTurret = MI_CAR_BARRAGE.IsValid() && m_pVehicle->GetModelIndex() == MI_CAR_BARRAGE && iSeatIndex == 3; // This works on TECHNCIAL because it's a standing turret network. BARRAGE rear needs to use seated move networks.
	if (m_pVehicle->IsTurretSeat(iSeatIndex) && pSeatOccupier && (CTaskVehicleFSM::CanPerformOnVehicleTurretJack(*m_pVehicle, iSeatIndex) || bIsBarrageRearTurret))
	{
		return false;
	}
	else
	{
		return !pSeatOccupier || rAnimInfo.GetHasOnVehicleEntry() || rEntryInfo.GetClimbUpOnly();
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::CheckForStreamedEntry()
{
	const CPed& ped = *GetPed();
	if (ped.IsLocalPlayer())
	{
		CPed* pSeatOccupier = GetPedOccupyingDirectAccessSeat();
		
		if (CheckForOpenDoor() || CheckForJackingPedFromOutside(pSeatOccupier) || CheckForEnteringSeat(pSeatOccupier))
		{
			if (!NetworkInterface::IsGameInProgress() || !pSeatOccupier || !pSeatOccupier->IsAPlayerPed())
			{
				// Check the requested anims are for the vehicle and entry point we're entering and the clipset conditions are still valid
				const CVehicleClipRequestHelper& vehClipHelper = ped.GetPlayerInfo()->GetVehicleClipRequestHelper();
				CScenarioCondition::sScenarioConditionData conditionData;
				conditionData.pPed = &ped;
				conditionData.pTargetVehicle = m_pVehicle;
				conditionData.iTargetEntryPoint = m_iTargetEntryPoint;
				if (vehClipHelper.GetTargetVehicle() == m_pVehicle && vehClipHelper.GetTargetEntryPointIndex() == m_iTargetEntryPoint && vehClipHelper.GetConditionalClipSet() && vehClipHelper.GetConditionalClipSet()->CheckConditions(conditionData))
				{
					m_pStreamedEntryClipSet = vehClipHelper.GetConditionalClipSet();
					if (m_pStreamedEntryClipSet && !m_pStreamedEntryClipSet->GetIsSkyDive())
					{
						if (m_pStreamedEntryClipSet->GetReplaceStandardJack() && (GetState() != State_OpenDoor && GetState() != State_AlignToEnterSeat))
						{
							return false;
						}
						m_StreamedClipSetId = m_pStreamedEntryClipSet->GetClipSetId();

						fwClipSet* pClipSet = fwClipSetManager::GetClipSet(m_StreamedClipSetId);
						if (taskVerifyf(pClipSet && pClipSet->GetClipItemCount() >= 0, "Expect non null clipset and at least one clip item"))
						{
							s32 iRandomClipIndex = fwRandom::GetRandomNumberInRange(0, pClipSet->GetClipItemCount());
							m_StreamedClipId = pClipSet->GetClipItemIdByIndex(iRandomClipIndex);
						}
						return m_StreamedClipSetId.IsNotNull() && m_StreamedClipId.IsNotNull();
					}
				}
			}
		}
	}
	else if(ped.GetPedConfigFlag(CPED_CONFIG_FLAG_WillArrestRatherThanJack))
	{
		m_pStreamedEntryClipSet = m_VehicleClipRequestHelper.ChooseVariationClipSetFromEntryIndex(m_pVehicle, m_iTargetEntryPoint);
		if (m_pStreamedEntryClipSet)
		{
			m_StreamedClipSetId = m_pStreamedEntryClipSet->GetClipSetId();

			fwClipSet* pClipSet = fwClipSetManager::GetClipSet(m_StreamedClipSetId);
			if (taskVerifyf(pClipSet && pClipSet->GetClipItemCount() >= 0, "Expect non null clipset and at least one clip item"))
			{
				s32 iRandomClipIndex = fwRandom::GetRandomNumberInRange(0, pClipSet->GetClipItemCount());
				m_StreamedClipId = pClipSet->GetClipItemIdByIndex(iRandomClipIndex);
			}
			return m_StreamedClipSetId.IsNotNull() && m_StreamedClipId.IsNotNull();
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::ShouldUseCombatEntryAnimLogic(bool bCheckSubtask) const
{
	if (!IsFlagSet(CVehicleEnterExitFlags::CombatEntry))
	{
		return false;
	}

	if (m_pVehicle && m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint))
	{
		if ((!ms_Tunables.m_UseCombatEntryForAiJack && !GetPed()->IsLocalPlayer()) && (CTaskEnterVehicle::GetPedOccupyingDirectAccessSeat() || IsFlagSet(CVehicleEnterExitFlags::HasJackedAPed)))
		{
			return false;
		}

		// Never do combat entry anim logic on cars that need unlocking
		if (m_pVehicle->GetDoorLockStateFromEntryPoint(m_iTargetEntryPoint) != CARLOCK_UNLOCKED)
		{
			if (GetPed()->IsLocalPlayer() || m_pVehicle->GetDoorLockStateFromEntryPoint(m_iTargetEntryPoint) != CARLOCK_LOCKOUT_PLAYER_ONLY)
			{
				return false;
			}
		}

		if (NetworkInterface::IsGameInProgress() && (GetState() == State_StreamAnimsToAlign || GetState() == State_PickDoor))
		{
			const CPed& rPed = *GetPed();
			if (!m_pVehicle->CanPedEnterCar(&rPed))
			{
				return false;
			}
			else if (rPed.IsPlayer() && rPed.GetNetworkObject())
			{
				const CNetGamePlayer* player		= rPed.GetNetworkObject()->GetPlayerOwner();
				const CNetObjVehicle* vehicleNetObj	= ((const CNetObjVehicle*)m_pVehicle->GetNetworkObject());
				if (player && vehicleNetObj)
				{
					if (vehicleNetObj->IsLockedForPlayer(player->GetPhysicalPlayerIndex()))
					{
						return false;
					}
				}
			}
		}

		const CVehicleEntryPointAnimInfo* pEntryAnimInfo = m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint);
		if (!pEntryAnimInfo->GetHasCombatEntry())
		{
			return false;
		}

		if (bCheckSubtask && GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_OPEN_VEHICLE_DOOR_FROM_OUTSIDE)
		{
			if (GetState() < CTaskOpenVehicleDoorFromOutside::State_OpenDoorWater)
			{
				return false;
			}
		}
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::CheckForOpenDoor() const
{
    bool isBike = m_pVehicle->InheritsFromBike();

    if(!isBike)
    {
	    if (m_iTargetEntryPoint > -1)
	    {
			const float fDoorConsideredOpenRatio = CVehicle::GetDoorRatioToConsiderDoorOpen(*m_pVehicle, false);
		    s32 iDoorBoneIndex = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromEntryPoint(m_iTargetEntryPoint);
		    CCarDoor* pDoor = m_pVehicle->GetDoorFromBoneIndex(iDoorBoneIndex);
		    return pDoor && pDoor->GetIsIntact(m_pVehicle) && pDoor->GetDoorRatio() < fDoorConsideredOpenRatio;
	    }
    }
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::CheckForClimbUpIfLocked() const
{
	if (m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint) 
		&& m_pVehicle->GetDoorLockStateFromEntryPoint(m_iTargetEntryPoint) != CARLOCK_NONE && m_pVehicle->GetDoorLockStateFromEntryPoint(m_iTargetEntryPoint) != CARLOCK_UNLOCKED && m_pVehicle->GetDoorLockStateFromEntryPoint(m_iTargetEntryPoint) != CARLOCK_LOCKED_BUT_CAN_BE_DAMAGED)
	{
		const CVehicleEntryPointInfo* pEntryPointInfo = m_pVehicle->GetVehicleModelInfo()->GetEntryPointInfo(m_iTargetEntryPoint);
		if (pEntryPointInfo && pEntryPointInfo->GetTryLockedDoorOnGround())
		{
			return false;
		}
	}
	else if(NetworkInterface::IsGameInProgress())
	{
		CPed const * ped = GetPed();

		if(ped && ped->IsAPlayerPed())
		{
			if(AssertVerify(ped->GetNetworkObject()))
			{
				CNetGamePlayer* player			= ped->GetNetworkObject()->GetPlayerOwner();
				CNetObjVehicle* vehicleNetObj	= ((CNetObjVehicle*)m_pVehicle->GetNetworkObject());
		
				if(player && vehicleNetObj)
				{
					if(vehicleNetObj->IsLockedForPlayer(player->GetPhysicalPlayerIndex()))
					{
						// vehicle may be locked for player but we might want him to climb up anyway...
						const CVehicleEntryPointInfo* pEntryPointInfo = m_pVehicle->GetVehicleModelInfo()->GetEntryPointInfo(m_iTargetEntryPoint);
						if (pEntryPointInfo && pEntryPointInfo->GetTryLockedDoorOnGround())
						{
							return false;
						}
					}
				}
			}
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::CheckForJackingPedFromOutside(CPed*& pSeatOccupier)
{
	// Don't early out until this has been set
	pSeatOccupier = GetPedOccupyingDirectAccessSeat();

	//Allow for stationary planes hovering in VTOL mode
	bool bIsPlaneHoveringInVTOL = false;
	if (m_pVehicle->InheritsFromPlane())
	{
		const CPlane* pPlane = static_cast<CPlane*>(m_pVehicle.Get());
		if (pPlane && pPlane->GetVerticalFlightModeAvaliable() && pPlane->GetVerticalFlightModeRatio() > 0)
		{
			bIsPlaneHoveringInVTOL = true;
		}
	}

	// If the entry point is too far from the ground, bail out
	const CPed* pPed = GetPed();
	if (m_pVehicle->IsInAir() && !bIsPlaneHoveringInVTOL)
	{
		Vector3 vEntryPosition(Vector3::ZeroType);
		Quaternion qEntryOrientation(0.0f,0.0f,0.0f,1.0f);
		CModelSeatInfo::CalculateEntrySituation(m_pVehicle, pPed, vEntryPosition, qEntryOrientation, m_iTargetEntryPoint, 0, 0.0f, &m_vLocalSpaceEntryModifier);

		TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, MAX_HEIGHT_FROM_ENTRY, 2.0f, 0.0f, 2.0f, 0.01f);
		Vector3 vGroundPos(Vector3::ZeroType);
		if (!CTaskVehicleFSM::FindGroundPos(m_pVehicle, GetPed(), vEntryPosition, MAX_HEIGHT_FROM_ENTRY, vGroundPos))
		{
			return false;
		}
	}

	// If the ped in the seat is shuffling there is no need to jack them
	if (CTaskVehicleFSM::CanJackPed(pPed, pSeatOccupier, m_iRunningFlags) 
		&& !IsSeatOccupierShuffling(pSeatOccupier)) 
	{
		if (GetState() == State_OpenDoor && pSeatOccupier && pSeatOccupier->GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle))
		{
			m_pJackedPed = NULL;
		}
		else
		{
			m_pJackedPed = pSeatOccupier;
		}
	}
	else
	{
		m_pJackedPed = NULL;
	}

	return m_pJackedPed != NULL;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::CheckForEnteringSeat(CPed* pSeatOccupier, bool bIsWarping, bool bIgnoreSideCheck)
{
	CPed* pPed = GetPed();

	if (m_pVehicle->InheritsFromBike() && ms_Tunables.m_EnableNewBikeEntry)
	{
		// Need to pick up/pull up the bike first
		s32 iUnused = 0;
		if (m_pVehicle->GetDriver() == NULL && (bIgnoreSideCheck || CheckForBikeOnGround(iUnused)))
		{
			if (!bIsWarping)
			{
				AI_LOG_WITH_ARGS("[%s] - %s Ped %s couldn't enter bike seat. Driver == NULL, bIgnoreSideCheck - %s\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(pPed), AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetBooleanAsString(bIgnoreSideCheck));
				return false;
			}
			else
			{
				// Wake it up so it processes stabilisation
				m_pVehicle->ActivatePhysics();
			}
		}

		if (GetPreviousState() != State_PickUpBike && GetPreviousState() != State_PullUpBike && GetPreviousState() != State_SetUprightBike && GetState() != State_WaitForPedToLeaveSeat && CheckForBikeFallingOver(*m_pVehicle) && !m_pVehicle->IsNetworkClone())
		{
			AI_LOG_WITH_ARGS("[%s] - %s Ped %s, couldn't enter bike seat. Bike falling over\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(pPed), AILogging::GetDynamicEntityNameSafe(pPed));
			return false;
		}
	}

	// We can enter the seat if no one is in in and we have the seat reserved
	CComponentReservation* pComponentReservation = NULL;

	if (m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint))
		pComponentReservation = m_pVehicle->GetComponentReservationMgr()->GetSeatReservation(m_iTargetEntryPoint, SA_directAccessSeat, m_iSeat);
	else if (m_pVehicle->IsSeatIndexValid(m_iCurrentSeatIndex))
		pComponentReservation = m_pVehicle->GetComponentReservationMgr()->GetSeatReservationFromSeat(m_iCurrentSeatIndex);

    bool bReservationValid = true;

    if(!pPed->IsNetworkClone())
    {
		const CVehicleEntryPointInfo* pEntryInfo = m_pVehicle->GetEntryInfo(m_iTargetEntryPoint);
		if (pEntryInfo && pEntryInfo->GetClimbUpOnly() && !IsFlagSet(CVehicleEnterExitFlags::Warp))
		{        
			// B*2843266: I think we should always be doing this logic (only check reservations, not make them) but limiting to 'climb up only' points for safety
			// This skips the seat reservation, used if you're climbing a point that doesnt actually link to a seat
			bReservationValid = pComponentReservation && pComponentReservation->IsComponentReservedForPed(pPed);
		}
		else
		{
			bReservationValid = CComponentReservationManager::ReserveVehicleComponent(pPed, m_pVehicle, pComponentReservation);
		}
    }

	if ((!pSeatOccupier || IsSeatOccupierShuffling(pSeatOccupier)) && bReservationValid)
		return true;

	if (bReservationValid && pSeatOccupier && pSeatOccupier->GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle) && m_pVehicle->GetLayoutInfo()->GetAllowEarlyDoorAndSeatUnreservation())
		return true;

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::CheckForGoToSeat() const
{
	/*CPhysical* pPhys = GetPed()->GetGroundPhysical();
	if (pPhys && pPhys->GetIsTypeVehicle() && m_pVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT && pPhys == m_pVehicle)
	{
		const CVehicleEntryPointAnimInfo* pClipInfo = m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint);
		if (pClipInfo && !pClipInfo->GetHasGetInFromWater())
		{
			return true;
		}
	}*/
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::CheckForBlockedOnVehicleGetIn() const
{
	const CPed* pPed = GetPed();
	CPhysical* pPhys = pPed->GetGroundPhysical();
	if (IsFlagSet(CVehicleEnterExitFlags::EnteringOnVehicle) || (pPhys && pPhys->GetIsTypeVehicle() && pPhys == m_pVehicle))
	{
		const CVehicleEntryPointAnimInfo* pClipInfo = m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint);

		if(m_pVehicle->HasWaterEntry() && pClipInfo->GetHasOnVehicleEntry())
		{
			//! Do a probe to GET IN point for boats.
			Vector3 vNewTargetPosition(Vector3::ZeroType);
			Quaternion qNewTargetOrientation(0.0f,0.0f,0.0f,1.0f);

			if (ComputeTargetTransformInWorldSpace(*m_pVehicle, vNewTargetPosition, qNewTargetOrientation, m_iTargetEntryPoint, CExtraVehiclePoint::GET_IN))
			{	
				Vector3 vStart = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
				Vector3 vEnd = vNewTargetPosition;
				static dev_float s_fCapsuleRadius = 0.1f;

				WorldProbe::CShapeTestCapsuleDesc pedCapsuleDesc;
				WorldProbe::CShapeTestHitPoint result;
				WorldProbe::CShapeTestResults probeResult(result);
				pedCapsuleDesc.SetResultsStructure( &probeResult );
				pedCapsuleDesc.SetCapsule( vStart, vEnd, s_fCapsuleRadius );
				pedCapsuleDesc.SetIsDirected( true );
				pedCapsuleDesc.SetIncludeFlags( ArchetypeFlags::GTA_VEHICLE_TYPE );
				pedCapsuleDesc.SetIncludeEntity( m_pVehicle );
				pedCapsuleDesc.SetDomainForTest( WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS );
				pedCapsuleDesc.SetDoInitialSphereCheck( true );		

				const bool bHitBoat = WorldProbe::GetShapeTestManager()->SubmitTest( pedCapsuleDesc );
				if(bHitBoat)
				{
#if DEBUG_DRAW
					ms_debugDraw.AddLine(VECTOR3_TO_VEC3V(vStart), VECTOR3_TO_VEC3V(vEnd), Color_purple, 5000);
#endif // DEBUG_DRAW
					return true;
				}
			}
		}
	}

	return false;
}

bool CTaskEnterVehicle::CheckForGoToClimbUpPoint() const
{
	const CPed* pPed = GetPed();
	if (IsOnVehicleEntry())
	{
		// Do a collision check, including the vehicle to see if we can get there!
		const CVehicleEntryPointAnimInfo* pClipInfo = m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint);
		if (pClipInfo && pClipInfo->GetHasClimbUp())
		{
			// Maybe we should just have a func to compute the matrix?
			Vector3 vEntryPos(Vector3::ZeroType);
			Quaternion qEntryRot(0.0f, 0.0f, 0.0f, 1.0f);
			CTaskEnterVehicleAlign::ComputeAlignTarget(vEntryPos, qEntryRot, true, *m_pVehicle, *pPed, m_iTargetEntryPoint);
			Matrix34 testMatrix;
			testMatrix.d = vEntryPos;
			testMatrix.FromQuaternion(qEntryRot);
#if DEBUG_DRAW
			const Vec3V vPedPos = pPed->GetTransform().GetPosition();
			const Vec3V vTestPos = RCC_VEC3V(testMatrix.d);
#endif // DEBUG_DRAW

			bool bAquaticVehicleExceptions = (MI_CAR_APC.IsValid() && m_pVehicle->GetModelIndex() == MI_CAR_APC) || (MI_CAR_TECHNICAL2.IsValid() && m_pVehicle->GetModelIndex() == MI_CAR_TECHNICAL2) || 
											 (MI_CAR_ZHABA.IsValid() && m_pVehicle->GetModelIndex() == MI_CAR_ZHABA) || ((MI_BOAT_PATROLBOAT.IsValid() && m_pVehicle->GetModelIndex() == MI_BOAT_PATROLBOAT));
			bool bExcludeVehicleFromTest = !m_pVehicle->GetIsAquatic() || bAquaticVehicleExceptions;

			if (bExcludeVehicleFromTest)
			{
				TUNE_GROUP_FLOAT(VEHICLE_ENTRY_TUNE, MAX_DIST_TO_IGNORE_VEHICLE_FROM_TEST, 2.0f, 0.0f, 10.0f, 0.1f);
				TUNE_GROUP_FLOAT(VEHICLE_ENTRY_TUNE, MAX_DIST_TO_IGNORE_VEHICLE_FROM_TEST_PLANE, 5.0f, 0.0f, 10.0f, 0.1f);
				TUNE_GROUP_FLOAT(VEHICLE_ENTRY_TUNE, MAX_DIST_TO_IGNORE_VEHICLE_FROM_TEST_WASTE, 6.0f, 0.0f, 10.0f, 0.1f);
				TUNE_GROUP_FLOAT(VEHICLE_ENTRY_TUNE, MAX_DIST_TO_IGNORE_VEHICLE_FROM_TEST_TANK, 5.0f, 0.0f, 10.0f, 0.1f);
				
				float fMaxDistToIgnoreVehicleFromTest = MAX_DIST_TO_IGNORE_VEHICLE_FROM_TEST;

				if (m_pVehicle->InheritsFromPlane())
				{
					fMaxDistToIgnoreVehicleFromTest = MAX_DIST_TO_IGNORE_VEHICLE_FROM_TEST_PLANE;
				}
				else if (MI_CAR_WASTELANDER.IsValid() && m_pVehicle->GetModelIndex() == MI_CAR_WASTELANDER)
				{
					fMaxDistToIgnoreVehicleFromTest = MAX_DIST_TO_IGNORE_VEHICLE_FROM_TEST_WASTE;
				}
				else if (m_pVehicle->IsTank())
				{
					fMaxDistToIgnoreVehicleFromTest = MAX_DIST_TO_IGNORE_VEHICLE_FROM_TEST_TANK;
				}

				Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
				const float fXYDistToEntryPointSqd = (vPedPos - vEntryPos).XYMag2();
				if (fXYDistToEntryPointSqd > rage::square(fMaxDistToIgnoreVehicleFromTest))
				{
					bExcludeVehicleFromTest = false;
				}
			}

			if (!CModelSeatInfo::IsPositionClearForPed(pPed, m_pVehicle, testMatrix, bExcludeVehicleFromTest))
			{
#if DEBUG_DRAW
				ms_debugDraw.AddLine(vPedPos, vTestPos, Color_red, 1000);
#endif // DEBUG_DRAW
				return false;
			}
#if DEBUG_DRAW
			ms_debugDraw.AddLine(vPedPos, vTestPos, Color_green, 1000);
#endif // DEBUG_DRAW
			return true;
		}

		if (pClipInfo->GetHasOnVehicleEntry())
		{
			return true;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::CheckForRestartingAlign() const
{
	bool bDoorConsideredClosed = false;
	bool bDoorConsideredOpen = false;

	const CCarDoor* pDoor = m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint) ? CTaskVehicleFSM::GetDoorForEntryPointIndex(m_pVehicle, m_iTargetEntryPoint) : NULL;
	if (pDoor && pDoor->GetIsIntact(m_pVehicle))
	{
		bDoorConsideredClosed = pDoor->GetDoorRatio() <= ms_Tunables.m_DoorRatioToConsiderDoorClosed;
		bDoorConsideredOpen = pDoor->GetDoorRatio() >= ms_Tunables.m_DoorRatioToConsiderDoorOpen;
	}

	if (m_bAligningToOpenDoor)
	{
		return bDoorConsideredClosed;
	}
	return bDoorConsideredOpen;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::WarpPedToEntryPositionAndOrientate()
{
	CPed* pPed = GetPed();

	Vector3 vEntryPosition(Vector3::ZeroType);
	Quaternion qEntryOrientation(0.0f,0.0f,0.0f,1.0f);
	CModelSeatInfo::CalculateEntrySituation(m_pVehicle, pPed, vEntryPosition, qEntryOrientation, m_iTargetEntryPoint, 0, 0.0f, &m_vLocalSpaceEntryModifier);
	Vector3 vGroundPos(Vector3::ZeroType);
	if (CTaskVehicleFSM::FindGroundPos(m_pVehicle, GetPed(), vEntryPosition, 2.5f, vGroundPos))
	{
		vEntryPosition.z = vGroundPos.z;
	}

	pPed->Teleport(vEntryPosition, qEntryOrientation.TwistAngle(ZAXIS), true);
	pPed->AttachPedToEnterCar(m_pVehicle, GetAttachToVehicleFlags(m_pVehicle), m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat, m_iSeat), m_iTargetEntryPoint);
	UpdatePedVehicleAttachment(pPed, RCC_VEC3V(vEntryPosition), RCC_QUATV(qEntryOrientation));
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::IsSeatOccupierShuffling(CPed* pSeatOccupier) const
{
	if (pSeatOccupier)
	{
        CQueriableInterface *pQueriableInterface = pSeatOccupier->GetPedIntelligence()->GetQueriableInterface();

		if(pQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE) && 
            pQueriableInterface->GetStateForTaskType(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE) >= CTaskInVehicleSeatShuffle::State_Shuffle)
		{
			return true;
		}

        CTask *pShuffleTask = pSeatOccupier->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE);

        if(pShuffleTask && pShuffleTask->GetState() >= CTaskInVehicleSeatShuffle::State_Shuffle)
        {
            return true;
        }
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::ShouldProcessAttachment() const
{
	if (m_iTargetEntryPoint != -1)
	{
		switch (GetState())
		{
			case State_OpenDoor:
				{
					if (!ShouldUseCombatEntryAnimLogic())
					{
						return true;
					}
					else if (GetSubTask())
					{
						return static_cast<const CTaskOpenVehicleDoorFromOutside*>(GetSubTask())->IsFixupFinished();
					}
					break;
				}
			case State_JackPedFromOutside:
			case State_ClimbUp:
			case State_AlignToEnterSeat:
			case State_PickUpBike:
			case State_PullUpBike:
			case State_EnterSeat:
			case State_StreamedEntry:
				return true;
			default:
				break;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::ComputeGlobalTranslationDeltaFromClip(const crClip& clip, float fCurrentPhase, Vector3& vDelta, const Quaternion& qPedOrientation)
{
	vDelta = fwAnimHelpers::GetMoverTrackDisplacement(clip, m_fLastPhase, fCurrentPhase);
	qPedOrientation.Transform(vDelta);	// Rotate by the original orientation to make the translation relative
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::ShouldFinishAfterClimbUp(CPed& rPed)
{
	const CVehicleEntryPointInfo* pEntryInfo = m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint) ? m_pVehicle->GetEntryInfo(m_iTargetEntryPoint) : NULL;
	const bool bQuitAfterClimbUp = pEntryInfo && pEntryInfo->GetClimbUpOnly();
	if (bQuitAfterClimbUp)
	{
		AI_FUNCTION_LOG("Entry info flagged as climb up only");
		return true;
	}

	const bool bCloneNoLongerRunningEnterVehicleForBoat = rPed.IsNetworkClone() && m_pVehicle->InheritsFromBoat() && GetStateFromNetwork() == State_Finish;
	if (bCloneNoLongerRunningEnterVehicleForBoat)
	{
		AI_FUNCTION_LOG("Clone no longer running enter vehicle for boat");
		return true;
	}

	if (ShouldUseClimbUpAfterOpenDoor(m_iTargetEntryPoint, m_pVehicle))
	{
		return false;
	}

	if (m_pVehicle->GetLayoutInfo()->GetPreventInterruptAfterClimbUp())
	{
		return false;
	}

	if (CheckPlayerExitConditions(rPed, *m_pVehicle, m_iRunningFlags, GetTimeRunning()))
	{
		AI_FUNCTION_LOG("Player interrupted");
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::ShouldAbortBecauseVehicleMovingTooFast() const
{
	if (m_pVehicle->GetIsAquatic() || (GetPed()->GetIsInWater() && m_pVehicle->HasWaterEntry()))
		return false;

	if (ShouldUseClimbUpAfterOpenDoor(m_iTargetEntryPoint, m_pVehicle))
		return false;

	//Allow for stationary planes hovering in VTOL mode
	if (m_pVehicle->InheritsFromPlane() && m_pVehicle->IsInAir() && !CTaskExitVehicle::IsVehicleOnGround(*m_pVehicle))
	{
		const CPlane* pPlane = static_cast<CPlane*>(m_pVehicle.Get());
		if (pPlane && pPlane->GetVerticalFlightModeAvaliable() && pPlane->GetVerticalFlightModeRatio() > 0)
		{
			return false;
		}
	}

	const CPed* pPed = GetPed();

	float fAiOpenDoorAbortSpeed = ms_Tunables.m_MinSpeedToAbortOpenDoor;
	if (!pPed->IsLocalPlayer() && IsFlagSet(CVehicleEnterExitFlags::CombatEntry))
	{
		fAiOpenDoorAbortSpeed = ms_Tunables.m_MinSpeedToAbortOpenDoorCombat;
	}

	float fMinSpeedToAbort = pPed->IsLocalPlayer() ? ms_Tunables.m_MinSpeedToAbortOpenDoorPlayer : fAiOpenDoorAbortSpeed;

	if (NetworkInterface::IsGameInProgress() && GetState() == State_OpenDoor && GetSubTask() 
		&& GetSubTask()->GetTaskType() == CTaskTypes::TASK_OPEN_VEHICLE_DOOR_FROM_OUTSIDE && GetSubTask()->GetState() == CTaskOpenVehicleDoorFromOutside::State_TryLockedDoor)
	{
		TUNE_GROUP_FLOAT(VEHICLE_TUNE, MIN_SPEED_FOR_ABORT_TRY_LOCKED_DOOR, 0.1f, 0.0f, 1.0f, 0.01f);
		fMinSpeedToAbort = MIN_SPEED_FOR_ABORT_TRY_LOCKED_DOOR;
	}

	if (m_pVehicle->GetRelativeVelocity(*pPed).Mag2() >= square(fMinSpeedToAbort))
		return true;

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::ShouldTriggerRagdollDoorGrab() const
{
	if (m_pVehicle->GetIsAquatic())
		return false;

	const CPed* pPed = GetPed();

	float fAiOpenDoorRagdollSpeed = ms_Tunables.m_MinSpeedToRagdollOpenDoor;
	if (!pPed->IsLocalPlayer() && IsFlagSet(CVehicleEnterExitFlags::CombatEntry))
	{
		fAiOpenDoorRagdollSpeed = ms_Tunables.m_MinSpeedToRagdollOpenDoorCombat;
	}

	const float fMinSpeedToRagdoll =  pPed->IsLocalPlayer() ? ms_Tunables.m_MinSpeedToRagdollOpenDoorPlayer : fAiOpenDoorRagdollSpeed;
	if (m_pVehicle->GetRelativeVelocity(*pPed).Mag2() >= square(fMinSpeedToRagdoll))
		return true;

	if (m_pVehicle->IsInAir() && !CTaskExitVehicle::IsVehicleOnGround(*m_pVehicle))
		return true;

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::EntryClipSetLoadedAndSet()
{
	const CPed& rPed = *GetPed();
	const fwMvClipSetId clipSetId = CTaskEnterVehicleAlign::GetEntryClipsetId(m_pVehicle, m_iTargetEntryPoint, &rPed);
	const bool bIsLoaded = CTaskVehicleFSM::IsVehicleClipSetLoaded(clipSetId);
	if (!bIsLoaded)
	{
		aiDisplayf("Frame %i: Ped %s (%p) waiting for clipset %s to be loaded", fwTimer::GetFrameCount(), rPed.GetModelName(), &rPed, clipSetId.TryGetCStr());
		return false;
	}
	m_moveNetworkHelper.SetClipSet(clipSetId);
	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::ShouldReEvaluateBecausePedEnteredOurSeatWeCantJack() const
{
	const CPed* pPed = GetPed();

	if (!m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint))
		return false;

	s32 iTargetSeat = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat, m_iSeat);
	if (!m_pVehicle->IsSeatIndexValid(iTargetSeat))
		return false;

	const CPed* pPedUsingSeat = CTaskVehicleFSM::GetPedInOrUsingSeat(*m_pVehicle, iTargetSeat);
	if (!pPedUsingSeat)
		return false;

	if (CTaskVehicleFSM::CanJackPed(pPed, pPedUsingSeat))
		return false;

	AI_LOG_WITH_ARGS("Ped %s rechoosing entry because ped %s is using the seat and we can't jack them\n", AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetDynamicEntityNameSafe(pPedUsingSeat));
	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::IsAtClimbUpPoint() const
{
	if (taskVerifyf(m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint), "Invalid target entry index %u", m_iTargetEntryPoint))
	{
		Vector3 vNewTargetPosition(Vector3::ZeroType);
		Quaternion qNewTargetOrientation(0.0f,0.0f,0.0f,1.0f);
		CTaskEnterVehicleAlign::ComputeAlignTarget(vNewTargetPosition, qNewTargetOrientation, IsFlagSet(CVehicleEnterExitFlags::EnteringOnVehicle), *m_pVehicle, *GetPed(), m_iTargetEntryPoint, &m_vLocalSpaceEntryModifier);

		Vector3 vPedPos = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
		vPedPos.z = 0.0f;
		vNewTargetPosition.z = 0.0f;
		if (vPedPos.Dist2(vNewTargetPosition) < rage::square(ms_Tunables.m_ClimbAlignTolerance))
		{
			return true;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::CheckForOptionalJackOrRideAsPassenger(const CPed& rPed, const CVehicle& rVeh) const
{
#if __BANK
	if (CVehicleDebug::ms_bForcePlayerToUseSpecificSeat && rVeh.IsSeatIndexValid(CVehicleDebug::ms_iSeatRequested))
	{
		return false;
	}
#endif // __BANK

	if ((rVeh.GetVehicleModelInfo() && rVeh.IsSeatIndexValid(m_iSeat) && rVeh.GetVehicleModelInfo()->GetSeatInfo(m_iSeat) && rVeh.GetVehicleModelInfo()->GetSeatInfo(m_iSeat)->GetPreventJackInWater()) && rVeh.GetIsInWater())
	{
		return false;
	}

	// If we're a player trying to get into a vehicle with a driver, decide if we can ride as a passenger
	const CPed* pPedUsingDriversSeat = NetworkInterface::IsGameInProgress() ? CTaskVehicleFSM::GetPedInOrUsingSeat(rVeh, rVeh.GetDriverSeat()) : rVeh.GetSeatManager()->GetDriver();
	if (rPed.IsLocalPlayer() && pPedUsingDriversSeat)
	{
		// If we're stood on top of a vehicle we can warp into if over the seat, don't bother doing the optional jack stuff
		if (rPed.GetGroundPhysical() == &rVeh && rVeh.IsSeatIndexValid(m_iSeat))
		{
			const CVehicleSeatAnimInfo* pSeatAnimInfo = rVeh.GetSeatAnimationInfo(m_iSeat);
			if (pSeatAnimInfo->GetWarpIntoSeatIfStoodOnIt())
			{
				return false;
			}
		}

		const CPed& rDriver = *pPedUsingDriversSeat;
		// The driver is an ai ped flagged to be able to drive the player as a passenger (e.g. taxi driver)
		const bool bCanRideAsAiPedsPassenger = !rDriver.IsAPlayerPed() && rDriver.GetPedConfigFlag(CPED_CONFIG_FLAG_AICanDrivePlayerAsRearPassenger) && !rPed.GetPedIntelligence()->IsThreatenedBy(rDriver);
		if (bCanRideAsAiPedsPassenger)
		{
			return true;
		}
		// The driver is a friendly player and the local player is flagged to be
		const bool bCanRideAsPlayerPedsPassenger = rDriver.IsAPlayerPed() && CTaskVehicleFSM::ArePedsFriendly(&rPed, &rDriver) && rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_PlayerCanJackFriendlyPlayers);
		if (bCanRideAsPlayerPedsPassenger)
		{
			return true;
		}
	}
	return false;
}

s32 CTaskEnterVehicle::CheckForOptionalForcedEntryToTurretSeatAndGetTurretSeatIndex(const CPed& rPed, const CVehicle& rVeh) const
{
	if (!rPed.IsLocalPlayer())
		return -1;
	
	if (rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_DisableTurretOrRearSeatPreference))
		return -1;

	bool bCherno = MI_CAR_CHERNOBOG.IsValid() && rVeh.GetModelIndex() == MI_CAR_CHERNOBOG;
	if (bCherno && rVeh.GetDriver())
		return -1;

	// Vehicle needs to have a turret
	const bool bAPC = MI_CAR_APC.IsValid() && rVeh.GetModelIndex() == MI_CAR_APC;
	const bool bHasTurret = rVeh.GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_TURRET_SEAT_ON_VEHICLE);
	if (!bAPC && !bHasTurret)
		return -1;

	const bool bOnTula = MI_PLANE_TULA.IsValid() && rVeh.GetModelIndex() == MI_PLANE_TULA && rPed.GetGroundPhysical() == &rVeh;
	if (bOnTula)
		return -1;

	// Vehicle needs to be unlocked (we are bailing out later for turret seats in TFR_VEHICLE_LOCKED_FOR_PLAYER, but script want to force locked door check anim on some vehicles)
	if (CTaskVehicleFSM::IsVehicleLockedForPlayer(rVeh, rPed))
		return -1;		

#if __BANK
	if (CVehicleDebug::ms_bForcePlayerToUseSpecificSeat && rVeh.IsSeatIndexValid(CVehicleDebug::ms_iSeatRequested))
		return -1;
#endif // __BANK

	// Only do this if no one is using the driver seat
	const s32 iDriversSeat = rVeh.GetDriverSeat();
	const CPed* pPedUsingDriversSeat = NetworkInterface::IsGameInProgress() ? CTaskVehicleFSM::GetPedInOrUsingSeat(rVeh, iDriversSeat) : rVeh.GetSeatManager()->GetDriver();

	// B*3721908: If the driver of this vehicle is a non-friendly non-player ped, we never want to allow hold enter to enter the turret. Instead, jack on both tap and hold.
	if (pPedUsingDriversSeat && !pPedUsingDriversSeat->GetPedIntelligence()->IsFriendlyWith(rPed) && !pPedUsingDriversSeat->IsPlayer())
	{
		return -1;
	}

	TUNE_GROUP_BOOL(TURRET_ENTRY_TUNE, CONSIDER_JACKING_FRIEND_PEDS, false);
	return CTaskVehicleFSM::FindBestTurretSeatIndexForVehicle(rVeh, rPed, pPedUsingDriversSeat, iDriversSeat, CONSIDER_JACKING_FRIEND_PEDS, false);
}

s32 CTaskEnterVehicle::CheckForOptionalForcedEntryToRearSeatAndGetSeatIndex(const CPed& rPed, const CVehicle& rVeh) const
{
	if (!rPed.IsLocalPlayer())
		return -1;

	if (rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_DisableTurretOrRearSeatPreference))
		return -1;

	bool bCherno = MI_CAR_CHERNOBOG.IsValid() && rVeh.GetModelIndex() == MI_CAR_CHERNOBOG;
	if (bCherno && rVeh.GetDriver())
		return -1;

#if __BANK
	if (CVehicleDebug::ms_bForcePlayerToUseSpecificSeat && rVeh.IsSeatIndexValid(CVehicleDebug::ms_iSeatRequested))
		return -1;
#endif // __BANK

	// Only do this if vehicle has rear seat activities or Boss ped is using the vehicle.
	if (!CheckShouldProcessEnterRearSeat(rPed, rVeh))
		return -1;

	// Get first free seat tagged for rear seat activities, else just get first free rear passenger seat.
	// Copied similar logic that Tez added for CTaskVehicleFSM::FindBestTurretSeatIndexForVehicle.
	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(rPed.GetTransform().GetPosition());
	int iBestSeatIndex = -1;
	float fClosestActivitySeatDist = 10000.0f;

	CVehicleModelInfo* pVehicleModelInfo = rVeh.GetVehicleModelInfo();
	if (pVehicleModelInfo)
	{
		const CModelSeatInfo* pModelSeatinfo = pVehicleModelInfo->GetModelSeatInfo();
		if (pModelSeatinfo)
		{
			const s32 iNumSeats = pVehicleModelInfo->GetModelSeatInfo()->GetNumSeats();
			for (s32 iSeatIndex = 0; iSeatIndex < iNumSeats; iSeatIndex++)
			{
				if (rVeh.IsSeatIndexValid(iSeatIndex))
				{
					// If rear seat is a turret, then don't prefer it if vehicle is locked, script want to at least try opening the (front or any other) door in some cases
					if (rVeh.IsTurretSeat(iSeatIndex) && CTaskVehicleFSM::IsVehicleLockedForPlayer(rVeh, rPed))
					{
						continue;
					}

					const bool bIsFrontSeat = rVeh.GetSeatInfo(iSeatIndex) && rVeh.GetSeatInfo(iSeatIndex)->GetIsFrontSeat();
					CPed *pPedInSeat = rVeh.GetSeatManager()->GetPedInSeat(iSeatIndex);
					if (!bIsFrontSeat && !pPedInSeat)
					{
						// Ignore seats that don't have entry points for boss peds
						if (rPed.IsBossPed() && !rVeh.CanSeatBeAccessedViaAnEntryPoint(iSeatIndex))
						{
							continue;
						}

						// Do a distance test to consider the best seat (we only evaluate the first direct entry point here).
						const s32 iDirectEntryPoint = rVeh.GetDirectEntryPointIndexForSeat(iSeatIndex);

						if (!rVeh.IsEntryIndexValid(iDirectEntryPoint))
						{
							continue;
						}

						float fDist2EntryPoint = FLT_MAX;

						if(!IsOnVehicleEntry())
						{
							fDist2EntryPoint = ComputeSquaredDistanceFromPedPosToEntryPoint(rPed, vPedPosition, rVeh, iDirectEntryPoint);
						}
						else
						{
							fDist2EntryPoint = ComputeSquaredDistanceFromPedPosToSeat(vPedPosition, rVeh, iDirectEntryPoint, iSeatIndex);
						}

						if (fDist2EntryPoint < fClosestActivitySeatDist)
						{
							fClosestActivitySeatDist = fDist2EntryPoint;
							iBestSeatIndex = iSeatIndex;
						}
					}
				}
			}
		}
	}
	return iBestSeatIndex;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::CheckShouldProcessEnterRearSeat(const CPed& rPed, const CVehicle& rVeh) const
{	
	if (IsFlagSet(CVehicleEnterExitFlags::Warp))
	{
		return false;
	}

	// Early out if we already chose turret seat while standing on top of a vehicle
	if (m_iSeat != -1 && rVeh.IsTurretSeat(m_iSeat) && rPed.GetGroundPhysical() == &rVeh)
	{
		return false;
	}

	const bool bOnTula = MI_PLANE_TULA.IsValid() && rVeh.GetModelIndex() == MI_PLANE_TULA && rPed.GetGroundPhysical() == &rVeh;
	const bool bCherno = MI_CAR_CHERNOBOG.IsValid() && rVeh.GetModelIndex() == MI_CAR_CHERNOBOG && rVeh.GetDriver();
	if ((rVeh.GetVehicleModelInfo() && rVeh.GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_PREFER_FRONT_SEAT))
		|| bOnTula || bCherno)
	{
		return false;
	}

	// Early out if the vehicle passenger seats are locked
	if (rVeh.GetCarDoorLocks() == CARLOCK_LOCKED_NO_PASSENGERS)
	{
		return false;
	}

	bool bAPC = MI_CAR_APC.IsValid() && rVeh.GetModelIndex() == MI_CAR_APC;
	if (rPed.IsBossPed() && !bAPC)
	{
		// Always get into the back of vehicles using this flag
		if (rVeh.m_bSpecialEnterExit)
		{			
			return true;
		}

		// Only get into the vehicle if there are no peds inside that are not in the same Boss/Goon organization
		if( rVeh.GetSeatManager() && rVeh.GetSeatManager()->CountPedsInSeats(true, true, false, NULL, rPed.GetBossID()) == 0 )
		{
			if(!CTaskVehicleFSM::IsVehicleLockedForPlayer(rVeh, rPed))
			{
				return true;
			}
		}
		else
		{
			AI_LOG_WITH_ARGS("[BossGoonVehicle] Boss player %s[%p] can't enter rear seat of vehicle %s[%p] - vehicle has non-organisation passengers.\n",
				AILogging::GetDynamicEntityNameSafe(&rPed), &rPed, rVeh.GetModelName(), &rVeh);
		}
		
	}

	// Always get into the rear of vehicles with rear seat activities or if AllowPedRearEntry reset flag is set on the ped.
	if (rVeh.GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_REAR_SEAT_ACTIVITIES) || rPed.GetPedResetFlag(CPED_RESET_FLAG_AllowPedRearEntry))
	{
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::CheckIfPlayerIsPickingUpMyBike() const
{
	// A better fix would be to disallow choosing an entry point that directly accesses the seat if the other one is in use
	const CPed* pPed = GetPed();
	if (!pPed->IsNetworkClone())
	{
		const fwEntity* pChildAttachment = m_pVehicle->GetChildAttachment();
		while (pChildAttachment)
		{
			if(static_cast<const CEntity*>(pChildAttachment)->GetIsTypePed() && pChildAttachment != pPed)
			{
				const CPed* pAttachedPed = static_cast<const CPed*>(pChildAttachment);
				const CTaskEnterVehicle* pEnterTask = static_cast<const CTaskEnterVehicle*>(pAttachedPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
				if (pEnterTask)
				{
					if ((pEnterTask->GetState() == State_PickUpBike || pEnterTask->GetState() == State_PullUpBike || pEnterTask->GetState() == State_SetUprightBike) ||
						(pEnterTask->GetPreviousState() == State_PickUpBike || pEnterTask->GetPreviousState() == State_PullUpBike || pEnterTask->GetPreviousState() == State_SetUprightBike))
					{
						return true;
					}
				}
			}
			pChildAttachment = pChildAttachment->GetSiblingAttachment();
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

aiTask* CTaskEnterVehicle::CreateAlignTask()
{
	bool bUseFastAlignRate = false;
	TUNE_FLOAT(MBR_TO_USE_FAST, 1.0, 0.0f, 4.0f, 0.01f);
	if (GetPed()->GetMotionData()->GetCurrentMbrY() > MBR_TO_USE_FAST)
	{
		bUseFastAlignRate = true;
	}

	if (GetPed()->WantsToMoveQuickly())
	{
		SetRunningFlag(CVehicleEnterExitFlags::UseFastClipRate);
	}

	GetPed()->GetMotionData()->SetUsingStealth(false);

#if DEBUG_DRAW
	TUNE_BOOL(DEBUG_VEHICLE_ALIGN, false);
	if (DEBUG_VEHICLE_ALIGN)
	{
		CPed& ped = *GetPed();
		Vector3 vEntryPos(Vector3::ZeroType);
		Quaternion qEntryOrientation(0.0f,0.0f,0.0f,1.0f);
		CModelSeatInfo::CalculateEntrySituation(m_pVehicle, &ped, vEntryPos, qEntryOrientation, m_iTargetEntryPoint, 0, 0.0f, &m_vLocalSpaceEntryModifier);
		vEntryPos.z = 0.0f;
		Vector3 vPedPos = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
		vPedPos.z = 0.0f;
		const float fDistToEntryPoint = vEntryPos.Dist(vPedPos);
		Displayf("Dist : %.2f", fDistToEntryPoint);
		vPedPos.z = ped.GetTransform().GetPosition().GetZf();
		vEntryPos.z = vPedPos.z;
		ms_debugDraw.AddLine(RCC_VEC3V(vPedPos), RCC_VEC3V(vEntryPos), Color_purple, 5000);
		char szText[128];
		formatf(szText, "Dist : %.4f", fDistToEntryPoint);
		ms_debugDraw.AddText(RCC_VEC3V(vPedPos), 0, 0, szText, Color_purple, 5000);
	}
#endif // DEBUG_DRAW

	
	CTaskEnterVehicleAlign* pTask = rage_new CTaskEnterVehicleAlign(m_pVehicle, m_iTargetEntryPoint, m_vLocalSpaceEntryModifier, bUseFastAlignRate);
	taskFatalAssertf(pTask->GetMoveNetworkHelper(), "Couldn't Get MoveNetworkHelper From Task (%s)", pTask->GetTaskName());
	pTask->GetMoveNetworkHelper()->SetupDeferredInsert(&m_moveNetworkHelper, ms_AlignOnEnterId, ms_AlignNetworkId);
	return pTask;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::SetClipPlaybackRate()
{
	const s32 iState = GetState();
	float fClipRate = 1.0f;
	const CAnimRateSet& rAnimRateSet = m_pVehicle->GetLayoutInfo()->GetAnimRateSet();

	// Jacks just have two settings for SP vs MP
	if (iState == State_JackPedFromOutside || iState == State_StreamedEntry)
	{
		fClipRate = rAnimRateSet.m_CombatJackEntry.GetRate();
		m_moveNetworkHelper.SetFloat(ms_JackRateId, fClipRate);
		return;
	}

	// Otherwise determine if we should speed up the entry anims and which tuning values to use (anim vs non anim)
	const CPed& rPed = *GetPed();
	const bool bShouldUseCombatEntryRates = IsFlagSet(CVehicleEnterExitFlags::CombatEntry) || IsFlagSet(CVehicleEnterExitFlags::UseFastClipRate) || rPed.GetPedResetFlag(CPED_RESET_FLAG_UseFastEnterExitVehicleRates);
	bool bUseAnimCombatEntryRates = false;
	bool bUseNonAnimCombatEntryRates = false;

	if (bShouldUseCombatEntryRates)
	{
		const bool bHasCombatEntryAnims = m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint)->GetHasCombatEntry();
		const bool bStateValidForCombatEntryAnims = (iState == State_EnterSeat);
		if (bHasCombatEntryAnims && bStateValidForCombatEntryAnims)
		{
			bUseAnimCombatEntryRates = true;
		}
		else
		{
			bUseNonAnimCombatEntryRates = true;
		}
	}

	float fFatPedEntryRateScale = 1.0f;

	const fwMvClipSetId clipSetId = rPed.GetPedModelInfo()->GetMovementClipSet();
	if(rPed.IsLocalPlayer() && CTaskHumanLocomotion::CheckClipSetForMoveFlag(clipSetId, CTaskEnterVehicle::ms_IsFatId))
	{
		TUNE_GROUP_FLOAT(VEHICLE_ENTRY_TUNE, FAT_PED_VEH_ENTRY_RATE_SCALE, 0.8f, 0.0f, 2.0f, 0.01f);
		fFatPedEntryRateScale = FAT_PED_VEH_ENTRY_RATE_SCALE;
	}

	// Set the clip rate based on our conditions
	if (bUseAnimCombatEntryRates)
	{
		fClipRate = rAnimRateSet.m_AnimCombatEntry.GetRate();
	}
	else if (bUseNonAnimCombatEntryRates)
	{
		fClipRate = rAnimRateSet.m_NoAnimCombatEntry.GetRate() * fFatPedEntryRateScale;
	}
	else
	{
		fClipRate = rAnimRateSet.m_NormalEntry.GetRate();
	}

	static u32 LAYOUT_PLANE_LUXOR2 = atStringHash("LAYOUT_PLANE_LUXOR2");
	static u32 LAYOUT_PLANE_LUXOR = atStringHash("LAYOUT_PLANE_JET");
	static u32 LAYOUT_PLANE_NIMBUS = atStringHash("LAYOUT_PLANE_NIMBUS");
	static u32 LAYOUT_PLANE_BOMBUSHKA = atStringHash("LAYOUT_PLANE_BOMBUSHKA");
	const bool bIsPassengerJet = m_pVehicle->GetLayoutInfo()->GetName() == LAYOUT_PLANE_LUXOR2 || m_pVehicle->GetLayoutInfo()->GetName() == LAYOUT_PLANE_LUXOR || 
								 m_pVehicle->GetLayoutInfo()->GetName() == LAYOUT_PLANE_NIMBUS || m_pVehicle->GetLayoutInfo()->GetName() == LAYOUT_PLANE_BOMBUSHKA;

	if (bIsPassengerJet)
		fClipRate *= CTaskEnterVehicleSeat::GetMultipleEntryAnimClipRateModifier(m_pVehicle, &rPed);

	if (GetState() == State_ClimbUp)
	{
		m_moveNetworkHelper.SetFloat(ms_ClimbUpRateId, fClipRate);
	}
	else if (GetState() == State_AlignToEnterSeat)
	{
		m_moveNetworkHelper.SetFloat(CTaskOpenVehicleDoorFromOutside::ms_OpenDoorRateId, fClipRate);
	}
	else if (GetState() == State_PickUpBike)
	{
		const bool bUseMPAnimRates = CAnimSpeedUps::ShouldUseMPAnimRates();
		const float fDefaultRate = bUseMPAnimRates ? ms_Tunables.m_MPEntryPickUpPullUpRate : 1.0f;
		fClipRate = bShouldUseCombatEntryRates ? ms_Tunables.m_CombatEntryPickUpPullUpRate : fDefaultRate;
		m_moveNetworkHelper.SetFloat(ms_PickUpRateId, fClipRate);
	}
	else if (GetState() == State_PullUpBike)
	{
		const bool bUseMPAnimRates = CAnimSpeedUps::ShouldUseMPAnimRates();
		const float fDefaultRate = bUseMPAnimRates ? ms_Tunables.m_MPEntryPickUpPullUpRate : 1.0f;
		fClipRate = bShouldUseCombatEntryRates ? ms_Tunables.m_CombatEntryPickUpPullUpRate : fDefaultRate;
		m_moveNetworkHelper.SetFloat(ms_PullUpRateId, fClipRate);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::SetUpRagdollOnCollision(CPed& rPed, CVehicle& rVeh)
{
	// If the vehicle is on moving ground then it makes things too complicated to allow the ragdoll to activate based on collision
	// The ped ends up getting attached to the bike which forces the ped capsule to deactivate which stops the capsule's position from being updated each physics frame
	// which stops the ragdoll bounds from being updated each physics frame which will cause problems on moving ground (large contact depths since the moving ground and
	// ragdoll bounds will be out-of-sync)
	// Best to just not allow activating the ragdoll based on collision
	if (!rPed.IsNetworkClone() && !rPed.GetActivateRagdollOnCollision() && rVeh.GetReferenceFrameVelocity().Mag2() < square(1.0f) && CTaskNMBehaviour::CanUseRagdoll(&rPed, RAGDOLL_TRIGGER_ACTIVATE_ON_COLLISION))
	{
		CTask* pTaskNM = rage_new CTaskNMJumpRollFromRoadVehicle(1000, 10000, false, false, atHashString::Null(), &rVeh);
		CEventSwitch2NM event(10000, pTaskNM);
		rPed.SetActivateRagdollOnCollisionEvent(event);
		rPed.SetActivateRagdollOnCollision(true);
		rPed.SetRagdollOnCollisionIgnorePhysical(&rVeh);
		TUNE_GROUP_FLOAT(RAGDOLL_ON_ENTER_TUNE, ALLOWED_SLOPE, -0.55f, -1.5f, 1.5f, 0.01f);
		rPed.SetRagdollOnCollisionAllowedSlope(ALLOWED_SLOPE);
		TUNE_GROUP_FLOAT(RAGDOLL_ON_ENTER_TUNE, ALLOWED_PENTRATION, 0.1f, 0.0f, 1.0f, 0.01f);
		rPed.SetRagdollOnCollisionAllowedPenetration(ALLOWED_PENTRATION);
		rPed.SetRagdollOnCollisionAllowedPartsMask(0xFFFFFFFF & ~(BIT(RAGDOLL_HAND_LEFT) | BIT(RAGDOLL_HAND_RIGHT) | BIT(RAGDOLL_FOOT_LEFT) | BIT(RAGDOLL_FOOT_RIGHT)));
	}
}

void CTaskEnterVehicle::ResetRagdollOnCollision(CPed& rPed)
{
	rPed.SetNoCollisionEntity(NULL);
	rPed.SetActivateRagdollOnCollision(false);
	rPed.SetRagdollOnCollisionIgnorePhysical(NULL);
	rPed.ClearActivateRagdollOnCollisionEvent();
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::CheckIsEntryPointBlockedByRearDoor(CPhysical * pVehicle, s32 iEntryPoint)
{
	//---------------------------------------
	// Is the target vehicle an automobile ?

	if(!pVehicle->GetIsTypeVehicle())
		return false;
	if( !((CVehicle*)pVehicle)->InheritsFromAutomobile() )
		return false;
	CAutomobile * pAuto = (CAutomobile*)pVehicle;

	//---------------------------------------
	// Does this vehicle have 4 side doors ?

	s32 iNumEntryExitPoints = pAuto->GetNumberEntryExitPoints();
	if(iNumEntryExitPoints < 4)
		return false;
	Assert(iEntryPoint < iNumEntryExitPoints);

	const CEntryExitPoint * pTargetEP = pAuto->GetEntryExitPoint(iEntryPoint);

	//----------------------------------
	// Are we heading to a front door ?

	const eHierarchyId iTargetID = CCarEnterExit::DoorBoneIndexToHierarchyID(pAuto, pTargetEP->GetDoorBoneIndex());
	if(iTargetID != VEH_DOOR_DSIDE_F && iTargetID != VEH_DOOR_PSIDE_F)
		return false;

	//------------------------------------------------------------------------
	// Is the rear door on the same side currently open (beyond some ratio) ?

	const eHierarchyId iRearDoorID = (eHierarchyId) (((int)iTargetID) + 1);
	Assert(iRearDoorID == VEH_DOOR_DSIDE_R || iRearDoorID == VEH_DOOR_PSIDE_R);

	CCarDoor * pRearDoor = pAuto->GetDoorFromId(iRearDoorID);
	if( !pRearDoor || pRearDoor->GetIsClosed() || pRearDoor->GetDoorRatio() < 0.5f )
		return false;

	//--------------------------------------------------------------
	// Are the front and rear doors in reasonably close proximity ?

	TUNE_FLOAT(REAR_DOOR_OBSTRUCTION_DISTANCE, 1.5f, 0.0f, 8.0f, 0.1f);

	CCarDoor * pFrontDoor = pAuto->GetDoorFromId(iTargetID);

	Matrix34 matFront, matRear;
	if( !pFrontDoor->GetLocalMatrix(pAuto, matFront) )
		return false;
	if( !pRearDoor->GetLocalMatrix(pAuto, matRear) )
		return false;

	Vector3 vDiff = matRear.d - matFront.d;
	if(vDiff.Mag2() > REAR_DOOR_OBSTRUCTION_DISTANCE*REAR_DOOR_OBSTRUCTION_DISTANCE)
		return false;

	return true;
}

Vector3 CTaskEnterVehicle::CalculateLocalSpaceEntryModifier(CPhysical * pVehicle, s32 iEntryPoint)
{
	if( !CTaskEnterVehicle::CheckIsEntryPointBlockedByRearDoor( pVehicle, iEntryPoint ) )
		return VEC3_ZERO;

	//------------------------------------------------------------------------
	// Return the distance by which to adjust the entry point in object space

	TUNE_FLOAT(REAR_DOOR_ADJUST_DISTANCE, 0.15f, 0.0f, 8.0f, 0.01f);

	return Vector3(0.0f, REAR_DOOR_ADJUST_DISTANCE, 0.0f);
}

void CTaskEnterVehicle::PossiblyReportStolentVehicle(CPed& ped, CVehicle& vehicle, bool bRequireDriver)
{
	switch(CCrime::sm_eReportCrimeMethod)
	{
	case CCrime::REPORT_CRIME_DEFAULT:
		if(ped.IsPlayer())
		{
			CPed* pDriver = vehicle.GetDriver();
			if( !bRequireDriver ||
				(pDriver && !pDriver->IsPlayer()) )
			{		
				if(!vehicle.m_nVehicleFlags.bHasBeenOwnedByPlayer)
				{
					if(vehicle.GetVehicleType() == VEHICLE_TYPE_CAR)
					{
						CCrime::ReportCrime(CRIME_STEAL_CAR, &vehicle, &ped);
					}
					else
					{
						CCrime::ReportCrime(CRIME_STEAL_VEHICLE, &vehicle, &ped);
					}
				}
				else if(pDriver && pDriver->ShouldBeDead() && !ped.GetPedIntelligence()->IsFriendlyWith(*pDriver))
				{
					CCrime::ReportCrime(CRIME_JACK_DEAD_PED, &vehicle, &ped);
				}
			}
		}
		break;
	case CCrime::REPORT_CRIME_ARCADE_CNC: 
		return;
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::CheckForBikeOnGround(s32& iState) const
{
	if (m_pVehicle->InheritsFromBike() && m_pVehicle->GetLayoutInfo()->GetUsePickUpPullUp())
	{
		const float fSide = m_pVehicle->GetTransform().GetA().GetZf();
		if (Abs(fSide) > ms_Tunables.m_MinMagForBikeToBeOnSide)
		{
			const CVehicleEntryPointInfo* pEntryInfo = m_pVehicle->GetEntryInfo(m_iTargetEntryPoint);
			taskAssert(pEntryInfo);

			if (m_pVehicle->ShouldOrientateVehicleUpForPedEntry(GetPed()))
			{
				iState = State_SetUprightBike;
			}
			else if ((fSide < 0.0f && pEntryInfo->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT)
				|| (fSide > 0.0f && pEntryInfo->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_RIGHT))
			{
				iState = State_PullUpBike;
			}
			else
			{
				iState = State_PickUpBike;
			}
			return true;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::ShouldBeInActionModeForCombatEntry(bool& bWithinRange)
{
#if __DEV
	TUNE_GROUP_BOOL(ENTER_VEHICLE_TUNE, ALLOW_FORCED_ACTION_MODE_FOR_OPEN_DOOR, true);
	if (!ALLOW_FORCED_ACTION_MODE_FOR_OPEN_DOOR)
	{
		return false;
	}
#endif // __DEV

	const CPed& rPed = *GetPed();
	if (!rPed.IsLocalPlayer())
		return false;

	if (IsFlagSet(CVehicleEnterExitFlags::FromCover))
		return false;

	if (GetState() != State_GoToDoor && GetState() != State_Align && GetState() != State_EnterSeat)
		return false;

	if (!m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint))
		return false;

	if (CTaskEnterVehicle::CNC_ShouldForceMichaelVehicleEntryAnims(rPed, true))
		return false;

	TUNE_GROUP_BOOL(ENTER_VEHICLE_TUNE, ALLOW_FORCED_ACTION_MODE_FOR_NO_DOOR, true);
	const bool bIgnoreCombatEntryAnimCheck = ALLOW_FORCED_ACTION_MODE_FOR_NO_DOOR;
	if (!bIgnoreCombatEntryAnimCheck && !m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint)->GetHasCombatEntry())
		return false;

	bool bCombatEntry = IsFlagSet(CVehicleEnterExitFlags::CombatEntry) || bIgnoreCombatEntryAnimCheck;

	const bool bPassedMbrTest = rPed.GetMotionData()->GetCurrentMbrY() > MOVEBLENDRATIO_WALK;
	if (bCombatEntry || bPassedMbrTest)
	{
		Vector3 vEntryPos(Vector3::ZeroType);
		Quaternion qEntryOrientation(0.0f,0.0f,0.0f,1.0f);
		CModelSeatInfo::CalculateEntrySituation(m_pVehicle, &rPed, vEntryPos, qEntryOrientation, m_iTargetEntryPoint);
		vEntryPos.z = 0.0f;
		Vector3 vPedPos = VEC3V_TO_VECTOR3(rPed.GetTransform().GetPosition());
		vPedPos.z = 0.0f;
		const float fDistToEntryPointSqd = vEntryPos.Dist2(vPedPos);
		if (fDistToEntryPointSqd < square(ms_Tunables.m_DistToEntryToAllowForcedActionMode))
		{
			bCombatEntry = bPassedMbrTest;
			bWithinRange = true;
		}
	}

	if (bCombatEntry)
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(m_pVehicle.Get());
		CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(pVehicle, m_iTargetEntryPoint);
		if (!pDoor || !pDoor->GetIsIntact(pVehicle))
		{
			return true;
		}

		const float fDoorConsideredOpenRatio = CTaskEnterVehicle::ms_Tunables.m_DoorRatioToConsiderDoorOpen;
		if (pDoor->GetDoorRatio() >= fDoorConsideredOpenRatio)
		{
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::UnReserveSeat_OnUpdate()
{
	CPed* pPed = GetPed();

	UnreserveSeat(pPed);

	if (CheckForShuffle())
	{
		SetTaskState(State_UnReserveDoorToShuffle);
	}
	else
	{
		SetTaskState(State_UnReserveDoorToFinish);
	}
	
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::ShuffleToSeat_OnEnter()
{
    CPed *pPed = GetPed();

    // if we get to this state before we have a valid seat or entry point index we have to
    // quit the task and rely on the network syncing code to place the ped in the vehicle
    if(pPed && pPed->IsNetworkClone() && (m_iTargetEntryPoint == -1 || m_iCurrentSeatIndex == -1 || !IsMoveTransitionAvailable(GetState())))
    {
        SetStateFromNetwork(State_Finish);
    }
    else
    {
		StoreInitialOffsets();
	    SetNewTask(rage_new CTaskInVehicleSeatShuffle(m_pVehicle, &m_moveNetworkHelper, false, m_iSeat));

		s32 iCurrentEntryPointIndex = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(m_iCurrentSeatIndex, m_pVehicle);
	    taskFatalAssertf(m_pVehicle->GetEntryExitPoint(iCurrentEntryPointIndex), "NULL entry point associated with seat %i", m_iCurrentSeatIndex);
	    s32 iTargetSeatIndex = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetShuffleSeatForSeat(iCurrentEntryPointIndex, m_iCurrentSeatIndex, IsFlagSet(CVehicleEnterExitFlags::UseAlternateShuffleSeat));
		if (taskVerifyf(m_pVehicle->IsSeatIndexValid(iTargetSeatIndex), "Seat index %i is not valid for vehicle %s, current seat %i, current entry %i", iTargetSeatIndex, m_pVehicle->GetDebugName(), m_iCurrentSeatIndex, iCurrentEntryPointIndex))
		{
			m_pJackedPed = m_pVehicle->GetSeatManager()->GetPedInSeat(iTargetSeatIndex);
		}
    }

	ProcessJackingStatus();
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::ShuffleToSeat_OnUpdate()
{
	CPed *pPed = GetPed();

	const CVehicle *pVehiclePedInside = pPed->GetVehiclePedInside();
	if (pVehiclePedInside && Verifyf(pVehiclePedInside == m_pVehicle, ""))
	{
		if(pVehiclePedInside->HasRaisedRoof())
		{
			if (pVehiclePedInside->IsSeatIndexValid(m_iSeat))
			{
				const CVehicleSeatInfo *pSeatInfo = pVehiclePedInside->GetSeatInfo(m_iSeat);
				if (Verifyf(pSeatInfo, "Could not find seat info for vehicle %s, seat index %i", pVehiclePedInside->GetModelName(), m_iSeat))
				{
					bool bDisableHairScale = false;
					const CVehicleModelInfo *pVehicleModelInfo = pVehiclePedInside->GetVehicleModelInfo();
					if(pVehicleModelInfo)
					{
						float fMinSeatHeight = pVehicleModelInfo->GetMinSeatHeight();
						float fHairHeight = pPed->GetHairHeight();

						if(fHairHeight >= 0.0f && fMinSeatHeight >= 0.0f)
						{
							const float fSeatHeadHeight = 0.59f; /* This is a conservative approximate measurement */
							if((fSeatHeadHeight + fHairHeight + CVehicle::sm_HairScaleDisableThreshold) < fMinSeatHeight)
							{
								bDisableHairScale = true;
							}
						}
					}

					float fHairScale = pVehiclePedInside->GetIsHeli() ? 0.0f : pSeatInfo->GetHairScale();
					if(fHairScale < 0.0f && !bDisableHairScale)
					{
						pPed->SetPedResetFlag(CPED_RESET_FLAG_OverrideHairScale, true);
						pPed->SetTargetHairScale(fHairScale);
					}
				}
			}
		}
	}

   if (GetIsSubtaskFinished(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE))
	{
		s32 nTargetSeat = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetShuffleSeatForSeat(m_iTargetEntryPoint, m_iCurrentSeatIndex, IsFlagSet(CVehicleEnterExitFlags::UseAlternateShuffleSeat));
		m_iCurrentSeatIndex = nTargetSeat;
        m_iLastCompletedState = State_ShuffleToSeat;
		SetTaskState(State_Finish);
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::TryToGrabVehicleDoor_OnEnter()
{
    CPed *pPed = GetPed();
    pPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS|DETACH_FLAG_EXCLUDE_VEHICLE);

	SetNewTask(rage_new CTaskTryToGrabVehicleDoor(m_pVehicle, m_iTargetEntryPoint));
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::TryToGrabVehicleDoor_OnUpdate()
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_TRY_TO_GRAB_VEHICLE_DOOR))
	{
		CPed *pPed = GetPed();
		if (IsFlagSet(CVehicleEnterExitFlags::ResumeIfInterupted))
		{
			if (m_moveNetworkHelper.IsNetworkActive())
			{
				pPed->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, INSTANT_BLEND_DURATION);
				m_moveNetworkHelper.ReleaseNetworkPlayer();
			}

			// Someone's playing with us, so do combat entry, more likely to succeed
			if (!pPed->IsLocalPlayer())
			{
				SetRunningFlag(CVehicleEnterExitFlags::CombatEntry);
			}
			SetTaskState(State_Start);
			return FSM_Continue;
		}
		else
		{
            if(pPed->IsNetworkClone())
            {
                if(CanUpdateStateFromNetwork())
                {
                    SetTaskState(GetStateFromNetwork());
                }
            }
            else
            {
			    SetTaskState(State_UnReserveDoorToFinish);
			    return FSM_Continue;
            }
		}
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::WaitForNetworkSync_OnUpdate()
{
    s32 stateFromNetwork = GetStateFromNetwork();

    if(stateFromNetwork != m_iNetworkWaitState)
    {
		if (m_iLastCompletedState != State_OpenDoor && stateFromNetwork == State_ClimbUp)
		{
			 return FSM_Continue;
		}

        if(CanUpdateStateFromNetwork())
        {
            CPed *pPed = GetPed();

			if(stateFromNetwork < State_Align && m_iLastCompletedState >= State_Align && pPed->GetAttachState() && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
			{
				AI_LOG_WITH_ARGS("[EnterVehicle] %s ped %s : 0x%p in %s:%s : 0x%p restarting clone task as state from network has gone backwards!\n", AILogging::GetDynamicEntityIsCloneStringSafe(pPed), AILogging::GetDynamicEntityNameSafe(pPed), pPed, GetTaskName(), GetStateName(GetState()), this);
				pPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS|DETACH_FLAG_EXCLUDE_VEHICLE);
				SetTaskState(State_Start);
			}
			else if (stateFromNetwork == State_CloseDoor && GetPreviousState() == State_OpenDoor)
			{   
				AI_LOG_WITH_ARGS("[EnterVehicle] %s ped %s : 0x%p in %s:%s : 0x%p is being forced to State_EnterSeat because they aren't in the vehicle\n", AILogging::GetDynamicEntityIsCloneStringSafe(pPed), AILogging::GetDynamicEntityNameSafe(pPed), pPed, GetTaskName(), GetStateName(GetState()), this);
				SetTaskState(State_EnterSeat);
			}
			else
			{
				if (stateFromNetwork == State_Finish && pPed->GetIsInVehicle())
				{
					AI_LOG_WITH_ARGS("[EnterVehicle] %s ped %s : 0x%p in %s:%s : 0x%p is being forced to State_WaitForPedToLeaveSeat because they aren't in the vehicle, m_iSeat = %i\n", AILogging::GetDynamicEntityIsCloneStringSafe(pPed), AILogging::GetDynamicEntityNameSafe(pPed), pPed, GetTaskName(), GetStateName(GetState()), this, m_iSeat);
					m_iCurrentSeatIndex = m_iSeat;
					SetTaskState(State_WaitForPedToLeaveSeat);
				}
				else
				{
					SetTaskState(stateFromNetwork);
				}
			}
        }
    }

    return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::WaitForValidEntryPoint_OnUpdate()
{
	CPed& rPed = *GetPed();

	if (GetTimerRanOut() && !rPed.IsNetworkClone())
	{
		SetRunningFlag(CVehicleEnterExitFlags::Warp);
		m_bWarping = true;

		SetTaskState(State_PickDoor);
		return FSM_Continue;
	}

	bool bIsOppressor2 = MI_BIKE_OPPRESSOR2.IsValid() && m_pVehicle->GetModelIndex() == MI_BIKE_OPPRESSOR2;

	if (m_pVehicle->IsInAir() && !rPed.IsLocalPlayer() && !bIsOppressor2)
	{
		if (rPed.GetAttachState())
		{
			rPed.DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
		}

		ASSERT_ONLY(taskDebugf2("Frame : %u - %s%s stopped Enter Vehicle Move Network : CTaskEnterVehicle::WaitForValidEntryPoint_OnUpdate()", fwTimer::GetFrameCount(), rPed.IsNetworkClone() ? "Clone ped : " : "Local ped : ", rPed.GetDebugName()));
		rPed.GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, NORMAL_BLEND_DURATION);
		return FSM_Continue;
	}

	if (!rPed.IsNetworkClone())
	{
		static bank_float s_fWaitTime = 2.0f;
		if (GetTimeInState() > s_fWaitTime)
		{
			SetTaskState(State_Start);
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::Ragdoll_OnEnter()
{
	CTaskNMBrace* pNMTask = rage_new CTaskNMBrace(1000, 6000, m_pVehicle);
	CEventSwitch2NM switchToNmEvent(6000, pNMTask);
	GetPed()->SwitchToRagdoll(switchToNmEvent);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::Ragdoll_OnUpdate()
{
	// Should be getting a ragdoll event;
	if (!taskVerifyf(GetTimeInState() < 1.0f, "Ped 0x%p failed to complete ragdoll detach properly", GetPed()))
	{
		return FSM_Quit;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicle::Finish_OnUpdateClone()
{
    // wait for the enter vehicle task to finish on the parent machine
    CPed* pPed = GetPed();

    if(!pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE, true, GetNetSequenceID()))
    {
        // the enter vehicle clone task can finish before the peds vehicle state has
        // been updated over the network, which can lead to the ped being popped out
        // of the vehicle for a frame or two. This prevents the network code from updating
        // the peds vehicle state during this interval
        const unsigned IGNORE_VEHICLE_UPDATES_TIME = 250;
        NetworkInterface::IgnoreVehicleUpdates(GetPed(), IGNORE_VEHICLE_UPDATES_TIME);

        return FSM_Quit;
    }

    return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::PedEnteredVehicle()
{
	CPed* pPed = GetPed();
	CPedGroup* pPedsGroup = pPed->GetPedsGroup();
	if (pPedsGroup)
	{
		CPed* pLeader = pPedsGroup->GetGroupMembership()->GetLeader();
		if (pLeader && pLeader != pPed)
		{
			if (pLeader->IsLocalPlayer())
			{
				TIME_LAST_PLAYER_GROUP_MEMBER_ENTERED = fwTimer::GetTimeInMilliseconds();
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::CreateWeaponManagerIfRequired()
{
	if (m_pVehicle->IsTurretSeat(m_iSeat))
	{
		if (!m_pVehicle->GetVehicleWeaponMgr())
		{
			m_pVehicle->CreateVehicleWeaponMgr();
			m_bCreatedVehicleWpnMgr = true;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

dev_u32 TIME_BETWEEN_GROUP_MEMBERS_ENTERING = 500;

bool CTaskEnterVehicle::PedShouldWaitForLeaderToEnter()
{
	CPed* pPed = GetPed();

  if (IsFlagSet(CVehicleEnterExitFlags::WaitForLeader))
	{
		CPedGroup* pPedsGroup = pPed->GetPedsGroup();
		if (pPedsGroup)
		{
			CPed* pLeader = pPedsGroup->GetGroupMembership()->GetLeader();
			if (pLeader && pLeader != pPed )
			{
				CPed* pSeatOccupier = NULL;
				if (CheckForJackingPedFromOutside(pSeatOccupier))
				{
					return false;
				}

				if (pLeader->GetIsInVehicle() && (pLeader->GetMyVehicle() == m_pVehicle))
				{
					return false;
				}

				if (pLeader->IsLocalPlayer() && ((TIME_LAST_PLAYER_GROUP_MEMBER_ENTERED + TIME_BETWEEN_GROUP_MEMBERS_ENTERING ) > fwTimer::GetTimeInMilliseconds()))
				{
					return true;
				}
				else if (pLeader->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) ||
					(pLeader->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE) &&
					pLeader->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType( CTaskTypes::TASK_ENTER_VEHICLE ) >= CTaskEnterVehicle::State_EnterSeat &&
					pLeader->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType( CTaskTypes::TASK_ENTER_VEHICLE ) <= CTaskEnterVehicle::State_Finish) )
				{
					return false;
				}
				else
				{
					return true;
				}
			}
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::PedShouldWaitForReservationToEnter()
{
	CPed& rPed = *GetPed();

	if (rPed.IsLocalPlayer())
		return false;

	if (!m_pVehicle->GetLayoutInfo()->GetWaitForRerservationInGroup())
		return false;

	CPedGroup* pPedsGroup = rPed.GetPedsGroup();
	if (!pPedsGroup)
		return false;

	CPed* pLeader = pPedsGroup->GetGroupMembership()->GetLeader();
	if (!pLeader || pLeader == &rPed)
		return false;

	CPed* pSeatOccupier = NULL;
	if (CheckForJackingPedFromOutside(pSeatOccupier))
		return false;

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::PedHasReservations()
{
	CPed& rPed = *GetPed();

	CPedGroup* pPedsGroup = rPed.GetPedsGroup();
	if (!pPedsGroup)
		return true;

	CPed* pLeader = pPedsGroup->GetGroupMembership()->GetLeader();
	if (!pLeader || pLeader == &rPed)
		return true;

	if (!pLeader->GetIsInVehicle() )
		return false;

	if (pLeader->GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle))
		return false;

	if (TryToReserveDoor(rPed, *m_pVehicle, m_iTargetEntryPoint) &&
		TryToReserveSeat(rPed, *m_pVehicle, m_iTargetEntryPoint))
	{
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::CanPedGetInVehicle(CPed* pPed)
{
	if (!m_pVehicle->CanPedEnterCar(pPed))
		return false;

	if (m_pVehicle->GetCarDoorLocks() == CARLOCK_PARTIALLY)
	{
		if (m_pVehicle->GetDoorLockStateFromEntryPoint(m_iTargetEntryPoint) == CARLOCK_LOCKED)
		{
			return false;
		}
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::TryToReserveDoor(CPed& rPed, CVehicle& rVeh, s32 iEntryPointIndex)
{
	CComponentReservation* pComponentReservation = rVeh.GetComponentReservationMgr()->GetDoorReservation(iEntryPointIndex);

	if (!pComponentReservation || IsFlagSet(CVehicleEnterExitFlags::Warp)
		|| (NetworkInterface::IsGameInProgress() && rVeh.GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_USE_FULL_ANIMS_FOR_MP_WARP_ENTRY_POINTS)))
	{
		// No door to reserve / warping / forcing due to vehicle flag
		return true; 
	}
	else if (CComponentReservationManager::ReserveVehicleComponent(&rPed, &rVeh, pComponentReservation))
	{
		pComponentReservation->SetPedStillUsingComponent(&rPed);
		return true;
	}

	AI_LOG_WITH_ARGS("[VehicleEntryExit][CTaskEnterVehicle::TryToReserveDoor] - %s Ped %s is trying to reserve door for entry point %i, vehicle %s, owned by %s, ped who has reservation\n", AILogging::GetDynamicEntityIsCloneStringSafe(&rPed), AILogging::GetDynamicEntityNameSafe(&rPed), iEntryPointIndex, AILogging::GetDynamicEntityNameSafe(&rVeh), AILogging::GetDynamicEntityOwnerGamerNameSafe(&rVeh), AILogging::GetDynamicEntityNameSafe(pComponentReservation->GetPedUsingComponent()));
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::TryToReserveSeat(CPed& rPed, CVehicle& rVeh, s32 iEntryPointIndex, s32 iSeatIndex)
{
	CComponentReservation* pComponentReservation = rVeh.IsEntryIndexValid(iEntryPointIndex) ? rVeh.GetComponentReservationMgr()->GetSeatReservation(iEntryPointIndex, SA_directAccessSeat, m_iSeat) : rVeh.GetComponentReservationMgr()->GetSeatReservationFromSeat(iSeatIndex);

	if (!taskVerifyf(pComponentReservation, "NULL component reservation for entrypoint %i, seat %i", iEntryPointIndex, iSeatIndex))
	{
		// No seat to reserve
		return false; 
	}
	else if (CComponentReservationManager::ReserveVehicleComponent(&rPed, &rVeh, pComponentReservation))
	{
		pComponentReservation->SetPedStillUsingComponent(&rPed);
		return true;
	}

	AI_LOG_WITH_ARGS("[VehicleEntryExit][CTaskEnterVehicle::TryToReserveSeat] - %s Ped %s is trying to reserve seat %i for entry point %i, vehicle %s, owned by %s, ped who has reservation %s\n", AILogging::GetDynamicEntityIsCloneStringSafe(&rPed), AILogging::GetDynamicEntityNameSafe(&rPed), rVeh.IsEntryIndexValid(iEntryPointIndex) ? m_iSeat : iSeatIndex, iEntryPointIndex, AILogging::GetDynamicEntityNameSafe(&rVeh), AILogging::GetDynamicEntityOwnerGamerNameSafe(&rVeh), AILogging::GetDynamicEntityNameSafe(pComponentReservation->GetPedUsingComponent()));
	return false;
}

////////////////////////////////////////////////////////////////////////////////

const crClip* CTaskEnterVehicle::GetClipAndPhaseForState(float& fPhase)
{
	if (m_moveNetworkHelper.IsNetworkActive())
	{
		switch (GetState())
		{
		case State_JackPedFromOutside:
			{
				fPhase = m_moveNetworkHelper.GetFloat( ms_JackPedPhaseId);
				fPhase = rage::Clamp(fPhase, 0.0f, 1.0f);
				return m_moveNetworkHelper.GetClip( ms_JackPedClipId);
			}
		case State_ClimbUp:
			{
				fPhase = m_moveNetworkHelper.GetFloat(ms_ClimbUpPhaseId);
				fPhase = rage::Clamp(fPhase, 0.0f, 1.0f);
				return m_moveNetworkHelper.GetClip(ms_ClimbUpClipId);
			}
		case State_OpenDoor:
			{
				if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_OPEN_VEHICLE_DOOR_FROM_OUTSIDE)
				{
					return static_cast<CTaskOpenVehicleDoorFromOutside*>(GetSubTask())->GetClipAndPhaseForState(fPhase);
				}
                return NULL;
			}
		case State_AlignToEnterSeat:
			{
				fPhase = m_moveNetworkHelper.GetFloat(ms_AlignToEnterSeatPhaseId);
				fPhase = rage::Clamp(fPhase, 0.0f, 1.0f);
				return m_moveNetworkHelper.GetClip(ms_AlignToEnterSeatClipId);
			}
		case State_EnterSeat:
			{
				if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_ENTER_VEHICLE_SEAT)
				{
					return static_cast<CTaskEnterVehicleSeat*>(GetSubTask())->GetClipAndPhaseForState(fPhase);
				}
                return NULL;
			}
		case State_CloseDoor:
			{
				if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_CLOSE_VEHICLE_DOOR_FROM_INSIDE)
				{
					return static_cast<CTaskCloseVehicleDoorFromInside*>(GetSubTask())->GetClipAndPhaseForState(fPhase);
				}
                return NULL;
			}
		case State_PickUpBike:
		case State_PullUpBike:
			{
				fPhase = m_moveNetworkHelper.GetFloat(ms_PickPullPhaseId);
				fPhase = rage::Clamp(fPhase, 0.0f, 1.0f);

				const float fPhaseToBeginOpenDoor =  0.55f;
				const float fPhaseToEndOpenDoor = 0.8f;

				if( fPhase < fPhaseToBeginOpenDoor )
				{
					m_fClipBlend = MIN( 1.0f, fPhaseToBeginOpenDoor > 0.0f ? fPhase/fPhaseToBeginOpenDoor : 1.0f);
				}
				else if( fPhase >= fPhaseToBeginOpenDoor && fPhase <= fPhaseToEndOpenDoor )
				{
					if((fPhaseToEndOpenDoor - fPhaseToBeginOpenDoor) > 0.0f)
					{
						m_fClipBlend = 1.0f - ((fPhase - fPhaseToBeginOpenDoor)/(fPhaseToEndOpenDoor- fPhaseToBeginOpenDoor));
					}
				}

				return m_moveNetworkHelper.GetClip(ms_PickPullClipId);
			}
		case State_StreamedEntry:
			{
				fPhase = m_moveNetworkHelper.GetFloat(ms_StreamedEntryPhaseId);
				fPhase = rage::Clamp(fPhase, 0.0f, 1.0f);
				return m_moveNetworkHelper.GetClip(ms_StreamedEntryClipId);
			}
		default: taskAssertf(0, "Unhandled State");
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::ShouldUseClimbUpTarget() const 
{
	const bool bUseClimbUpTargetDueToTurret = m_pVehicle->IsTurretSeat(m_iSeat) && m_pVehicle->GetSeatAnimationInfo(m_iSeat)->IsStandingTurretSeat() && GetState() == State_JackPedFromOutside && !IsOnVehicleEntry();

	if (bUseClimbUpTargetDueToTurret)
		return false;

	if (GetState() != State_ClimbUp && !bUseClimbUpTargetDueToTurret)
		return false;

	if (m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint))
	{
		const CVehicleEntryPointInfo* pEntryPointInfo = m_pVehicle->GetEntryInfo(m_iTargetEntryPoint);
		taskAssert(pEntryPointInfo);

		const CVehicleExtraPointsInfo* pExtraVehiclePointsInfo = pEntryPointInfo->GetVehicleExtraPointsInfo();

		if (pExtraVehiclePointsInfo)
		{
			const CExtraVehiclePoint* pExtraVehiclePoint = pExtraVehiclePointsInfo->GetExtraVehiclePointOfType(CExtraVehiclePoint::CLIMB_UP_FIXUP_POINT);
			if (pExtraVehiclePoint)
			{
				return true;
			}
		}
	}
	return false;
}

bool CTaskEnterVehicle::ShouldUseIdealTurretSeatTarget() const
{
	return m_pVehicle->IsTurretSeat(m_iSeat) && m_pVehicle->GetSeatAnimationInfo(m_iSeat)->IsStandingTurretSeat() && GetState() == State_JackPedFromOutside && !IsOnVehicleEntry() && m_pVehicle->GetVehicleWeaponMgr()->GetFirstTurretForSeat(m_iSeat);
}

bool CTaskEnterVehicle::ShouldUseGetInTarget() const
{
	bool bForceFixupToGetIn = m_pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HELI_USES_FIXUPS_ON_OPEN_DOOR);

	// Climb ups always use GET_IN target.
	if (GetState() == State_ClimbUp)
	{
		return true;
	}

	switch(GetState())
	{
	case(State_ClimbUp):
		return true;
	case(State_EnterSeat):
	case(State_JackPedFromOutside):
		{
			bForceFixupToGetIn = false;

			// Allow additional GET_IN targets if they are specified in layout info.
			if(m_bDoingExtraGetInFixup && m_iCurrentGetInIndex > 0) 
			{
				return true;
			}	
			break;
		}
	case(State_AlignToEnterSeat):
		{
			if (bForceFixupToGetIn)
			{
				return true;
			}
			break;
		}
	default:
		break;
	}

	if (ShouldUseClimbUpAfterOpenDoor(m_iTargetEntryPoint, m_pVehicle) || bForceFixupToGetIn)
	{
		if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_OPEN_VEHICLE_DOOR_FROM_OUTSIDE)
		{
			if (static_cast<const CTaskOpenVehicleDoorFromOutside*>(GetSubTask())->GetState() == CTaskOpenVehicleDoorFromOutside::State_ForcedEntry || bForceFixupToGetIn)
			{
				return true;
			}
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::ShouldUseEntryTarget() const
{
	bool bForcedEntryAndGetIn = false;

	if(m_pVehicle->InheritsFromSubmarine())
	{
		return false;
	}

	const CVehicleEntryPointAnimInfo* pClipInfo = m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint);
	if (pClipInfo)
	{
		if (pClipInfo->GetForcedEntryIncludesGetIn())
		{
			if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_OPEN_VEHICLE_DOOR_FROM_OUTSIDE)
			{
				bForcedEntryAndGetIn = static_cast<const CTaskOpenVehicleDoorFromOutside*>(GetSubTask())->GetState() == CTaskOpenVehicleDoorFromOutside::State_ForcedEntry ? true : false;
			}
		}
	}

	bool bJackShouldFixUpToGetInPosition = GetState() == State_JackPedFromOutside && !pClipInfo->GetJackIncludesGetIn() && !pClipInfo->GetHasOnVehicleEntry();
	if (GetState() == State_JackPedFromOutside && m_bWasJackedPedDeadInitially && !pClipInfo->GetDeadJackIncludesGetIn())
	{
		bJackShouldFixUpToGetInPosition = true;
	}

	const bool bStreamedShouldFixUpToGetInPosition = GetState() == State_StreamedEntry && m_pStreamedEntryClipSet && m_pStreamedEntryClipSet->GetReplaceStandardJack() && !pClipInfo->GetJackVariationsIncludeGetIn();
	return bJackShouldFixUpToGetInPosition || bStreamedShouldFixUpToGetInPosition || ((!bForcedEntryAndGetIn && (GetState() == State_OpenDoor )) || GetState() == State_AlignToEnterSeat);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::StoreInitialOffsets()
{
	// Store the offsets to the seat situation so we can recalculate the starting situation each frame relative to the seat
	Vector3 vTargetPosition(Vector3::ZeroType);
	Quaternion qTargetOrientation(0.0f,0.0f,0.0f,1.0f);

	if (ShouldUseClimbUpTarget())
	{
		ComputeClimbUpTarget(vTargetPosition, qTargetOrientation);
	}
	else if (ShouldUseIdealTurretSeatTarget())
	{
		ComputeIdealTurretSeatTarget(vTargetPosition, qTargetOrientation);
	}
	else if (ShouldUseGetInTarget())
	{
		ComputeGetInTarget(vTargetPosition, qTargetOrientation);
	}
	else if (ShouldUseEntryTarget())
	{
		ComputeEntryTarget(vTargetPosition, qTargetOrientation);
	}
	else
	{
		ComputeSeatTarget(vTargetPosition, qTargetOrientation);
	}
	m_PlayAttachedClipHelper.SetTarget(vTargetPosition, qTargetOrientation);
	m_PlayAttachedClipHelper.SetInitialOffsets(*GetPed());
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::ComputeClimbUpTarget(Vector3& vOffset, Quaternion& qOffset)
{
	taskVerifyf(CTaskVehicleFSM::ComputeTargetTransformInWorldSpace(*m_pVehicle, vOffset, qOffset, m_iTargetEntryPoint, CExtraVehiclePoint::ePointType(CExtraVehiclePoint::CLIMB_UP_FIXUP_POINT)), "Couldn't compute climb up target");
}

void CTaskEnterVehicle::ComputeIdealTurretSeatTarget(Vector3& vOffset, Quaternion& qOffset)
{
	Vector3 vClimbUpOffset;
	Quaternion qClimbUpOffset;
	CTaskVehicleFSM::ComputeTargetTransformInWorldSpace(*m_pVehicle, vClimbUpOffset, qClimbUpOffset, m_iTargetEntryPoint, CExtraVehiclePoint::ePointType(CExtraVehiclePoint::CLIMB_UP_FIXUP_POINT));
	Vector3 vSeatOffset;
	Quaternion qSeatOffset;
	CModelSeatInfo::CalculateSeatSituation(m_pVehicle, vSeatOffset, qSeatOffset, m_iTargetEntryPoint, m_iSeat);
	if (GetState() == State_JackPedFromOutside)
	{
		float fMinPhaseToLerp = 0.3f;
		float fMaxPhaseToLerp = 0.6f;
		float fPhase = 0.0f;
		const crClip* pClip = GetClipAndPhaseForState(fPhase);
		CClipEventTags::GetMoveEventStartAndEndPhases(pClip, ms_LerpTargetFromClimbUpToSeatId, fMinPhaseToLerp, fMaxPhaseToLerp);
		const float fLerpRatio = Clamp((fPhase - fMinPhaseToLerp) / (fMaxPhaseToLerp - fMinPhaseToLerp), 0.0f, 1.0f);
		vOffset = Lerp(fLerpRatio, vClimbUpOffset, vSeatOffset);
		qClimbUpOffset.PrepareSlerp(qSeatOffset);
		qOffset.Lerp(fLerpRatio, qClimbUpOffset, qSeatOffset);
		qOffset.Normalize();
	}
	else
	{
		vOffset = vClimbUpOffset;
		qOffset = qClimbUpOffset;
	}
}

void CTaskEnterVehicle::ComputeGetInTarget(Vector3& vOffset, Quaternion& qOffset)
{
	taskVerifyf(CTaskVehicleFSM::ComputeTargetTransformInWorldSpace(*m_pVehicle, vOffset, qOffset, m_iTargetEntryPoint, CExtraVehiclePoint::ePointType(CExtraVehiclePoint::GET_IN + m_iCurrentGetInIndex)), "Couldn't compute get in target (forcing get in fixup without extra vehicle points?)");
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::ComputeEntryTarget(Vector3& vOffset, Quaternion& qOffset)
{
	s32 iFlags = ShouldUseCombatEntryAnimLogic() ? CModelSeatInfo::EF_IsCombatEntry : CModelSeatInfo::EF_DontApplyEntryOffsets | CModelSeatInfo::EF_DontApplyOpenDoorOffsets;

	bool bDontAdjustForGroundHeight = false;
	if (GetState() == State_AlignToEnterSeat || GetState() == State_OpenDoor || GetState() == State_EnterSeat)
	{
		// May want to apply this to similar vehicle entry types on water
		if ((MI_CAR_APC.IsValid() && m_pVehicle->GetModelIndex() == MI_CAR_APC) || (MI_HELI_SEASPARROW.IsValid() && m_pVehicle->GetModelIndex() == MI_HELI_SEASPARROW))
		{
			iFlags |= CModelSeatInfo::EF_DontAdjustForVehicleOrientation;
			bDontAdjustForGroundHeight = true;
		}
	}

	CModelSeatInfo::CalculateEntrySituation(m_pVehicle, GetPed(), vOffset, qOffset, m_iTargetEntryPoint, iFlags, 0.0f, &m_vLocalSpaceEntryModifier);

	if (!bDontAdjustForGroundHeight)
	{
		// Alter the z coord based on the ground position
		Vector3 vGroundPos(Vector3::ZeroType);
		if (CTaskVehicleFSM::FindGroundPos(m_pVehicle, GetPed(), vOffset, 2.5f, vGroundPos))
		{
			// Adjust the reference position
			vOffset = vGroundPos;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::ComputeSeatTarget(Vector3& vOffset, Quaternion& qOffset)
{
	s32 iSeatIndex = (GetState() == State_EnterSeat || GetState() == State_JackPedFromOutside) ? m_iSeat : -1;
	CModelSeatInfo::CalculateSeatSituation(m_pVehicle, vOffset, qOffset, m_iTargetEntryPoint, iSeatIndex);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::GetTranslationBlendPhaseFromClip(const crClip* UNUSED_PARAM(pClip), float& fStartPhase, float& fEndPhase)
{
	/*TUNE_GROUP_BOOL(ENTER_VEHICLE_JACK_TUNE, X_USE_EVENTS, false);
	if (X_USE_EVENTS)
	{
		// We blend in the fixup based on events in the clip
		CClipEventTags::FindLoopPhases(pClip, 1, fStartPhase, fEndPhase);
	}
	else */
	if (GetState() == State_JackPedFromOutside)
	{
		TUNE_GROUP_FLOAT(ENTER_VEHICLE_JACK_TUNE, X_DEFAULT_PHASE_TO_START_BLEND_OFFSET, 0.0f, 0.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(ENTER_VEHICLE_JACK_TUNE, X_DEFAULT_PHASE_TO_END_BLEND_OFFSET, 0.3f, 0.0f, 1.0f, 0.01f);

		fStartPhase = X_DEFAULT_PHASE_TO_START_BLEND_OFFSET;
		fEndPhase = X_DEFAULT_PHASE_TO_END_BLEND_OFFSET;
	}
	else if (GetState() == State_OpenDoor)
	{
		TUNE_GROUP_FLOAT(ENTER_VEHICLE_OPEN_DOOR_TUNE, X_DEFAULT_PHASE_TO_START_BLEND_OFFSET, 0.0f, 0.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(ENTER_VEHICLE_OPEN_DOOR_TUNE, X_DEFAULT_PHASE_TO_END_BLEND_OFFSET, 0.3f, 0.0f, 1.0f, 0.01f);

		fStartPhase = X_DEFAULT_PHASE_TO_START_BLEND_OFFSET;
		fEndPhase = X_DEFAULT_PHASE_TO_END_BLEND_OFFSET;
	}
	else if (GetState() == State_EnterSeat)
	{
		TUNE_GROUP_FLOAT(ENTER_VEHICLE_SEAT_TUNE, X_DEFAULT_PHASE_TO_START_BLEND_OFFSET, 0.0f, 0.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(ENTER_VEHICLE_SEAT_TUNE, X_DEFAULT_PHASE_TO_END_BLEND_OFFSET, 0.3f, 0.0f, 1.0f, 0.01f);
		
		fStartPhase = X_DEFAULT_PHASE_TO_START_BLEND_OFFSET;
		fEndPhase = X_DEFAULT_PHASE_TO_END_BLEND_OFFSET;
	}
}

void CTaskEnterVehicle::GetOrientationBlendPhaseFromClip(const crClip* UNUSED_PARAM(pClip), float& fStartPhase, float& fEndPhase)
{
	/*TUNE_GROUP_BOOL(ENTER_VEHICLE_JACK_TUNE, USE_EVENTS, false);
	if (USE_EVENTS)
	{
		// We blend in the fixup based on events in the clip
		CClipEventTags::FindLoopPhases(pClip, 2, fStartPhase, fEndPhase);
	}
	else */
	if (GetState() == State_JackPedFromOutside)
	{
		TUNE_GROUP_FLOAT(ENTER_VEHICLE_JACK_TUNE, DEFAULT_PHASE_TO_START_BLEND_OFFSET, 0.0f, 0.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(ENTER_VEHICLE_JACK_TUNE, DEFAULT_PHASE_TO_END_BLEND_OFFSET, 1.0f, 0.0f, 1.0f, 0.01f);

		fStartPhase = DEFAULT_PHASE_TO_START_BLEND_OFFSET;
		fEndPhase = DEFAULT_PHASE_TO_END_BLEND_OFFSET;
	}
	else if (GetState() == State_OpenDoor)
	{
		TUNE_GROUP_FLOAT(ENTER_VEHICLE_OPEN_DOOR_TUNE, DEFAULT_PHASE_TO_START_BLEND_OFFSET, 0.0f, 0.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(ENTER_VEHICLE_OPEN_DOOR_TUNE, DEFAULT_PHASE_TO_END_BLEND_OFFSET, 1.0f, 0.0f, 1.0f, 0.01f);

		fStartPhase = DEFAULT_PHASE_TO_START_BLEND_OFFSET;
		fEndPhase = DEFAULT_PHASE_TO_END_BLEND_OFFSET;
	}
	else if (GetState() == State_EnterSeat)
	{
		TUNE_GROUP_FLOAT(ENTER_VEHICLE_SEAT_TUNE, DEFAULT_PHASE_TO_START_BLEND_OFFSET, 0.0f, 0.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(ENTER_VEHICLE_SEAT_TUNE, DEFAULT_PHASE_TO_END_BLEND_OFFSET, 1.0f, 0.0f, 1.0f, 0.01f);

		fStartPhase = DEFAULT_PHASE_TO_START_BLEND_OFFSET;
		fEndPhase = DEFAULT_PHASE_TO_END_BLEND_OFFSET;
	}
}

s32 CTaskEnterVehicle::GetVehicleClipFlags()
{
	s32 iFlags = CTaskVehicleFSM::VF_EntryPoint;

	const s32 iState = GetState();
	const CVehicleEntryPointAnimInfo* pAnimInfo = m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint);
	if (iState == State_EnterSeat || iState == State_StreamedEntry) 
	{
		// Temp hack until tank anims reauthored to new seat position
		if ((pAnimInfo->GetIgnoreMoverFixUp() && GetState() == State_EnterSeat) ||
			GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_WillArrestRatherThanJack))
		{
			iFlags |= CTaskVehicleFSM::VF_InitialXOffsetFixup;
			iFlags |= CTaskVehicleFSM::VF_InitialRotOffsetFixup;
			iFlags |= CTaskVehicleFSM::VF_ApplyInitialOffset;
			return iFlags;
		}
		else if (iState == State_StreamedEntry && m_pStreamedEntryClipSet && m_pStreamedEntryClipSet->GetReplaceStandardJack())
		{
			iFlags |= CTaskVehicleFSM::VF_ApplyInitialOffset;
			iFlags |= CTaskVehicleFSM::VF_ApplyToTargetFixup;		
		}
		else
		{
			iFlags |= CTaskVehicleFSM::VF_ApplyToTargetFixup;		
			iFlags |= CTaskVehicleFSM::VF_InitialXOffsetFixup;
			iFlags |= CTaskVehicleFSM::VF_InitialRotOffsetFixup;
			iFlags |= CTaskVehicleFSM::VF_ApplyInitialOffset;
		}
	}
	else if (iState == State_JackPedFromOutside)
	{
		iFlags |= CTaskVehicleFSM::VF_ApplyInitialOffset;
		iFlags |= CTaskVehicleFSM::VF_ApplyToTargetFixup;		
	}
	else if (iState == State_CloseDoor)
	{
		iFlags |= CTaskVehicleFSM::VF_Seat;
		iFlags |= CTaskVehicleFSM::VF_ApplyInitialOffset;
	}
	else if (iState == State_OpenDoor || iState == State_AlignToEnterSeat)
	{
		iFlags |= CTaskVehicleFSM::VF_ApplyInitialOffset;
		iFlags |= CTaskVehicleFSM::VF_ApplyToTargetFixup;		
	}
	else if (iState == State_PickUpBike || iState == State_PullUpBike || iState == State_ClimbUp)
	{
		iFlags |= CTaskVehicleFSM::VF_ApplyToTargetFixup;		
	}

	return iFlags;
}

bool CTaskEnterVehicle::IsMoveTransitionAvailable(s32 iNextState) const
{
    bool isBike = m_pVehicle->InheritsFromBike();

    switch(iNextState)
    {
    case State_OpenDoor:
        return ((m_iLastCompletedState == -1) ||
                (m_iLastCompletedState == State_Align));
    case State_JackPedFromOutside:
        return ((m_iLastCompletedState == -1)              ||
                (m_iLastCompletedState == State_Align)     ||
				(m_iLastCompletedState == State_ClimbUp)   ||
                (m_iLastCompletedState == State_OpenDoor)  ||
				(m_iLastCompletedState == State_AlignToEnterSeat));
    case State_CloseDoor:
        return ((m_iLastCompletedState == State_EnterSeat)     ||
                (m_iLastCompletedState == State_ShuffleToSeat));
    case State_ShuffleToSeat:
		return ((m_iLastCompletedState == State_JackPedFromOutside) ||
			    (m_iLastCompletedState == State_EnterSeat) ||
                (m_iLastCompletedState == State_CloseDoor));
    case State_EnterSeat:
        {
            if(isBike)
            {
                return ((m_iLastCompletedState == State_Align) ||
                        (m_iLastCompletedState == State_PickUpBike) ||
                        (m_iLastCompletedState == State_PullUpBike) ||
                        (m_iLastCompletedState == State_JackPedFromOutside) ||
						(m_iLastCompletedState == State_SetUprightBike));
            }
            else
            {
                return ((m_iLastCompletedState == State_Align) ||
                        (m_iLastCompletedState == State_OpenDoor) ||
                        (m_iLastCompletedState == State_JackPedFromOutside) ||
						(m_iLastCompletedState == State_AlignToEnterSeat) ||
						(m_iLastCompletedState == State_ClimbUp) ||
						(m_iLastCompletedState == State_StreamedEntry) ||
						(m_iLastCompletedState == State_GoToClimbUpPoint));
            }
		}
	case State_AlignToEnterSeat:
	case State_ClimbUp:
		{
			return m_iLastCompletedState == State_Align || m_iLastCompletedState == State_OpenDoor;
		}
    default:
        break;
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::IsConsideredGettingInVehicleForCamera(float fTimeOffsetForInterrupt)
{
	bool bIsEnteringVehicleForCamera = false;

	bool bPreventInterrupting = false;
	if(m_pVehicle && m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint))	
	{			
		const CVehicleEntryPointAnimInfo* pClipInfo = m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint);
		bPreventInterrupting = pClipInfo && pClipInfo->GetPreventJackInterrupt();
	}

	const s32 iState = GetState();
	if(iState == State_JackPedFromOutside && bPreventInterrupting)
	{
		bIsEnteringVehicleForCamera = true;
	}
	else if((m_fInterruptPhase > 0.0f) && ((iState == State_JackPedFromOutside) || ((iState == State_StreamedEntry) && IsFlagSet(CVehicleEnterExitFlags::HasJackedAPed))))
	{
		//Wait for the interrupt tag, as this will indicate that the jacking element of a concatenated animation is complete and the ped is
		//actually entering the vehicle.
		float fCurrentPhase = 0.0f;
		const crClip* pClip = GetClipAndPhaseForState(fCurrentPhase);
		if (pClip)
		{
			const float fInterruptTime = rage::Clamp(pClip->ConvertPhaseToTime(m_fInterruptPhase) - fTimeOffsetForInterrupt, 0.0f, 20.0f);
			const float fCurrentTime = pClip->ConvertPhaseToTime(fCurrentPhase);
			bIsEnteringVehicleForCamera = (fCurrentTime >= fInterruptTime);
		}
	}
	else
	{
		if(iState == State_OpenDoor)
		{
			const CTask* subTask = GetSubTask();
			if(subTask && (subTask->GetTaskType() == CTaskTypes::TASK_OPEN_VEHICLE_DOOR_FROM_OUTSIDE))
			{
				if((subTask->GetState() == CTaskOpenVehicleDoorFromOutside::State_ForcedEntry) && m_pVehicle)
				{
					const CVehicleEntryPointAnimInfo* clipInfo = m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint);
					if(clipInfo && clipInfo->GetForcedEntryIncludesGetIn())
					{
						return true;
					}
				}
			}
		}

		const bool bIsWarping = IsFlagSet(CVehicleEnterExitFlags::Warp);

		const CVehicleModelInfo* pVehicleModelInfo = m_pVehicle ? m_pVehicle->GetVehicleModelInfo() : NULL;
		const bool bShouldTransitionOnClimbUpDown = pVehicleModelInfo && pVehicleModelInfo->ShouldCameraTransitionOnClimbUpDown();

		// Don't report that the ped is entering a vehicle until they start to enter the seat, as entry can fail or be aborted by the user
		// prior to this point - unless the vehicle is flagged for the camera to transition during climb up.
		// - Please add any new state exclusions here.
		bIsEnteringVehicleForCamera	= ((bShouldTransitionOnClimbUpDown && (iState == CTaskEnterVehicle::State_ClimbUp)) ||
		  (((iState >= CTaskEnterVehicle::State_EnterSeat) && (iState < CTaskEnterVehicle::State_Finish)) &&
			(iState != CTaskEnterVehicle::State_TryToGrabVehicleDoor) &&
			(iState != CTaskEnterVehicle::State_WaitForPedToLeaveSeat || !bIsWarping) &&
			(iState != CTaskEnterVehicle::State_WaitForNetworkSync) &&
			(iState != CTaskEnterVehicle::State_WaitForValidEntryPoint)));
	}

	return bIsEnteringVehicleForCamera;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::IsConsideredGettingInVehicleForAmbientScale()
{
	bool bIsEnteringVehicleForAmbientScale = false;

	const s32 iState = GetState();
	if (iState == State_JackPedFromOutside && m_fInterruptPhase > 0.0f)
	{
		//Wait for the interrupt tag, as this will indicate that the jacking element of a concatenated animation is complete and the ped is
		//actually entering the vehicle.
		float fCurrentPhase = 0.0f;
		const crClip* pClip = GetClipAndPhaseForState(fCurrentPhase);
		if (pClip)
		{
			const float fInterruptTime = m_fInterruptPhase;
			const float fCurrentTime = fCurrentPhase;
			bIsEnteringVehicleForAmbientScale = (fCurrentTime >= fInterruptTime);
		}
	}
	else
	{
		const bool bIsWarping = IsFlagSet(CVehicleEnterExitFlags::Warp);

		// Don't report that the ped is entering a vehicle until they start to enter the seat, as entry can fail or be aborted by the user
		// prior to this point.
		// - Please add any new state exclusions here.
		bIsEnteringVehicleForAmbientScale	= (iState >= CTaskEnterVehicle::State_EnterSeat) && (iState < CTaskEnterVehicle::State_Finish) &&
			(iState != CTaskEnterVehicle::State_TryToGrabVehicleDoor) &&
			(iState != CTaskEnterVehicle::State_WaitForPedToLeaveSeat || !bIsWarping) &&
			(iState != CTaskEnterVehicle::State_WaitForNetworkSync) &&
			(iState != CTaskEnterVehicle::State_WaitForValidEntryPoint);
	}

	return bIsEnteringVehicleForAmbientScale;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::EnsureLocallyJackedPedsExitVehicle()
{
	CPed* pPed = GetPed();

#if __BANK
    if(!m_cloneTaskInScope)
    {
        BANK_ONLY(taskDebugf2("Frame : %u - %s%s : 0x%p in %s:%s : 0x%p EnsureLocallyJackedPedsExitVehicle() - running for out of scope clone task!", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone ped : " : "Local ped : ", GetPed()->GetDebugName(), GetPed(), GetTaskName(), GetStateName(GetState()), this));
		AI_LOG_IF_PLAYER(pPed, "[%s] - Ped %s is running CTaskEnterVehicle::EnsureLocallyJackedPedsExitVehicle - running for out of scope clone task!\n", GetTaskName(), AILogging::GetDynamicEntityNameSafe(pPed));
    }
#endif // __BANK

    if(pPed->IsNetworkClone() && m_pVehicle && m_iTargetEntryPoint != -1)
    {	
        s32 stateToCheck = GetStateFromNetwork();

        if(m_cloneTaskInScope && GetState() != State_Start)
        {
            stateToCheck = GetState();
        }

		AI_LOG_IF_PLAYER(pPed, "[%s] - Ped %s is running CTaskEnterVehicle::EnsureLocallyJackedPedsExitVehicle, m_iTargetEntryPoint = %i, current state = %s, stateToCheck = %s\n", GetTaskName(), AILogging::GetDynamicEntityNameSafe(pPed), m_iTargetEntryPoint, GetStaticStateName(GetState()), GetStaticStateName(stateToCheck));

        CPed *pPedInTargetSeat = 0;
        s32   iEntryPointIndex = m_iTargetEntryPoint;

        switch(stateToCheck)
        {
        case State_JackPedFromOutside:
        case State_EnterSeat:
        case State_WaitForPedToLeaveSeat:
            {
                s32 seatIndex = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat, m_iSeat);
                pPedInTargetSeat = (seatIndex != -1) ? m_pVehicle->GetSeatManager()->GetPedInSeat(seatIndex) : 0;
            }
            break;
        case State_ShuffleToSeat:
            {
                // we only want to kick off the jack on local peds if the task hasn't started yet (e.g. not streamed anims
                // in yet) - otherwise we wait for the task to catch up so the anims match up better
                if(GetState() == State_Start)
                {
                    s32 seatIndex = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_indirectAccessSeat);

                    if(seatIndex != -1)
                    {
                        pPedInTargetSeat = m_pVehicle->GetSeatManager()->GetPedInSeat(seatIndex);
                        iEntryPointIndex = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(seatIndex, m_pVehicle);
                    }
                }
            }
            break;
        default:
            break;
        }

#if __BANK
        if(!m_cloneTaskInScope)
        {
            BANK_ONLY(taskDebugf2("Frame : %u - %s%s : 0x%p in %s:%s : 0x%p EnsureLocallyJackedPedsExitVehicle() - Checked state %s, Ped in target seat %s", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone ped : " : "Local ped : ", GetPed()->GetDebugName(), GetPed(), GetTaskName(), GetStateName(GetState()), this, GetStateName(stateToCheck), pPedInTargetSeat ? pPedInTargetSeat->GetDebugName() : "None"));
        }
#endif // __BANK

        if(pPedInTargetSeat)
        {
            if(pPedInTargetSeat != pPed &&
               !pPedInTargetSeat->IsNetworkClone() &&
               !pPedInTargetSeat->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE) &&
			   !pPedInTargetSeat->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE))
            {
                bool bPedAlreadyBeingJacked = false;

                if(pPedInTargetSeat->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE))
                {
                    CTaskExitVehicle* pExitVehicleTask = static_cast<CTaskExitVehicle*>(pPedInTargetSeat->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE));
                    
                    if(pExitVehicleTask && pExitVehicleTask->IsFlagSet(CVehicleEnterExitFlags::BeJacked))
                    {
                        bPedAlreadyBeingJacked = true;
                    }
                }

                if(!bPedAlreadyBeingJacked && CTaskVehicleFSM::CanJackPed(pPed, pPedInTargetSeat, m_iRunningFlags))
                {
#if __BANK
                    if(!m_cloneTaskInScope)
                    {
                        BANK_ONLY(taskDebugf2("Frame : %u - %s%s : 0x%p in %s:%s : 0x%p EnsureLocallyJackedPedsExitVehicle() - Giving Ped in target seat %s exit vehicle task", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone ped : " : "Local ped : ", GetPed()->GetDebugName(), GetPed(), GetTaskName(), GetStateName(GetState()), this, pPedInTargetSeat ? pPedInTargetSeat->GetDebugName() : "None"));
                    }
#endif // __BANK
					AI_LOG_IF_PLAYER(pPed, "[%s] - Ped %s is giving ped %s exit vehicle task in CTaskEnterVehicle::EnsureLocallyJackedPedsExitVehicle\n", GetTaskName(), AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetDynamicEntityNameSafe(pPedInTargetSeat));

					VehicleEnterExitFlags vehicleFlags;
				    vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::BeJacked);
				    const s32 iEventPriority = CTaskEnterVehicle::GetEventPriorityForJackedPed(*pPedInTargetSeat);
                    CEventGivePedTask givePedTask(PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP, rage_new CTaskExitVehicle(m_pVehicle, vehicleFlags, 0.0f, pPed, iEntryPointIndex), true, iEventPriority );
	                pPedInTargetSeat->GetPedIntelligence()->AddEvent(givePedTask);
				    pPedInTargetSeat->SetPedResetFlag(CPED_RESET_FLAG_ForcePreCameraAIUpdate, true);
#if __BANK
				    CGivePedJackedEventDebugInfo instance(pPedInTargetSeat, this, __FUNCTION__);
				    instance.Print();
#endif // __BANK
                }
            }
            else
            {
				AI_LOG_IF_PLAYER(pPed, "[%s] - Ped %s is NOT giving ped %s exit vehicle task in CTaskEnterVehicle::EnsureLocallyJackedPedsExitVehicle\n", GetTaskName(), AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetDynamicEntityNameSafe(pPedInTargetSeat));

#if __BANK
                if(!m_cloneTaskInScope)
                {
                    BANK_ONLY(taskDebugf2("Frame : %u - %s%s : 0x%p in %s:%s : 0x%p EnsureLocallyJackedPedsExitVehicle() - Can't jack ped in target seat: same ped:%s, is clone:%s, running enter vehicle task:%s, running shuffle task:%s",
                                          fwTimer::GetFrameCount(),
                                          GetPed()->IsNetworkClone() ? "Clone ped : " : "Local ped : ",
                                          GetPed()->GetDebugName(),
                                          GetPed(),
                                          GetTaskName(),
                                          GetStateName(GetState()),
                                          this,
                                          (pPedInTargetSeat == pPed) ? "true" : "false",
                                          pPedInTargetSeat->IsNetworkClone() ? "true" : "false",
                                          pPedInTargetSeat->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE)  ? "true" : "false",
										  pPedInTargetSeat->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE) ? "true" : "false"));
                }
#endif // __BANK
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::ForceNetworkSynchronisationOfPedInSeat(s32 seatIndex)
{
    CPed *pPedInSeat = (seatIndex != -1) ? m_pVehicle->GetSeatManager()->GetPedInSeat(seatIndex) : 0;

    if(pPedInSeat && pPedInSeat->GetNetworkObject() && !pPedInSeat->IsNetworkClone())
    {
        NetworkInterface::ForceSynchronisation(pPedInSeat);
    }
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::IsOnVehicleEntry() const
{
	const CPed* pPed = GetPed();
	CPhysical* pPhys = pPed->GetGroundPhysical();
	if (IsFlagSet(CVehicleEnterExitFlags::EnteringOnVehicle) || (pPhys && pPhys->GetIsTypeVehicle() && pPhys == m_pVehicle))
	{
		return true;
	}
	return false;
}

bool CTaskEnterVehicle::ShouldTryToPickAnotherEntry(const CPed& rPedReservingSeat)
{
	if (!NetworkInterface::IsGameInProgress())
		return false;

	if (!rPedReservingSeat.GetPedResetFlag(CPED_RESET_FLAG_IsEnteringVehicle))
		return false;

	CPed& rPed = *GetPed();
	if (!rPedReservingSeat.GetPedIntelligence()->IsFriendlyWith(rPed))
		return false;

	bool bShouldTryToPickAnotherEntry = false;
	// See if we can get into the passenger seat instead
	if (m_pVehicle->InheritsFromBike() && 
		m_iSeat == m_pVehicle->GetDriverSeat() &&
		m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetNumSeats() > 1 &&
		m_pVehicle->IsSeatIndexValid(1))
	{
		if (!m_pVehicle->GetPedInSeat(1))
		{
			bShouldTryToPickAnotherEntry = true;
			SetSeatRequestType(SR_Specific, SRR_FORCE_GET_INTO_PASSENGER_SEAT, VEHICLE_DEBUG_DEFAULT_FLAGS);
			// Bad seat usage
			SetTargetSeat(1, TSR_FORCE_PASSENGER_SEAT, VEHICLE_DEBUG_DEFAULT_FLAGS);
		}
	}
	else if (rPed.IsLocalPlayer() &&
		m_pVehicle->InheritsFromBoat())
	{
		bShouldTryToPickAnotherEntry = true;
		SetSeatRequestType(SR_Any, SRR_TRY_ANOTHER_SEAT, VEHICLE_DEBUG_DEFAULT_FLAGS);
	}

	if (bShouldTryToPickAnotherEntry)
	{
		if (rPed.GetIsAttached())
			rPed.DetachFromParent(0);

		AI_LOG_WITH_ARGS("%s %s stopped Enter Vehicle Move Network : CTaskEnterVehicle::ShouldTryToPickAnotherEntry()\n", AILogging::GetDynamicEntityIsCloneStringSafe(&rPed), AILogging::GetDynamicEntityNameSafe(&rPed));
		rPed.GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, NORMAL_BLEND_DURATION);
	}
	return bShouldTryToPickAnotherEntry;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicle::StoreInitialReferencePosition( CPed* pPed )
{
	// Store the initial position of the ped, this is used to compute the ideal target to enter the seat, assuming we can rotate the turret manually
	Vec3V vPedPos = pPed->GetTransform().GetPosition();
	QuatV qPedRot = pPed->GetTransform().GetOrientation();
	CTaskVehicleFSM::UnTransformFromWorldSpaceToRelative(*m_pVehicle, m_pVehicle->GetTransform().GetPosition(), m_pVehicle->GetTransform().GetOrientation(), vPedPos, qPedRot);
	m_vLocalSpaceEntryModifier = VEC3V_TO_VECTOR3(vPedPos);

	// Flag this ped as entering on vehicle and needing to turn the turret
	SetRunningFlag(CVehicleEnterExitFlags::EnteringOnVehicle);
	SetRunningFlag(CVehicleEnterExitFlags::NeedToTurnTurret);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicle::ShouldUseClimbUpAfterOpenDoor(const s32 entryIndex, const CVehicle* pVehicle) const
{
	return CTaskVehicleFSM::ShouldUseClimbUpAndClimbDown(*pVehicle, entryIndex);
}

bool CTaskEnterVehicle::IsPedWarpingIntoVehicle(s32 iTargetEntryPoint) const
{
	const CVehicleEntryPointInfo* pEntryInfo = m_pVehicle->GetEntryInfo(iTargetEntryPoint);
	if (pEntryInfo && pEntryInfo->GetMPWarpInOut())
		return true;

	// This was added to fix url:bugstar:4204027 for DLC vehicles from then on, not sure of the effects of fixing it for every vehicle in the game
	if (m_pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_CHECK_WARP_TASK_FLAG_DURING_ENTER) && IsFlagSet(CVehicleEnterExitFlags::Warp))
		return true;

	return false;
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL

void CTaskEnterVehicle::Debug() const
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

	if (GetState() == State_WaitForValidEntryPoint && m_pVehicle)
	{
		Vec3V vPedPos = GetPed()->GetTransform().GetPosition();
		Vec3V vVehPos = m_pVehicle->GetTransform().GetPosition();
		grcDebugDraw::Line(vPedPos, vVehPos, Color_orange);
		s32 iSelectedSeat = 0;
		const s32 iDesiredTargetEntryPoint = CModelSeatInfo::EvaluateSeatAccessFromAllEntryPoints(GetPed(), m_pVehicle, m_iSeatRequestType, m_iSeat, m_bWarping, m_iRunningFlags, &iSelectedSeat);
		char szText[128];
		formatf(szText, "Desired Entry Point = %i", iDesiredTargetEntryPoint);
		grcDebugDraw::Text(vPedPos, Color_orange, 0, 0, szText);
	}

	if (GetPed() == pFocusPed)
	{
		if (m_pVehicle && m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint))
		{
			Vector3 vNewTargetPosition(Vector3::ZeroType);
			Quaternion qNewTargetOrientation(0.0f,0.0f,0.0f,1.0f);
			CTaskEnterVehicleAlign::ComputeAlignTarget(vNewTargetPosition, qNewTargetOrientation, IsFlagSet(CVehicleEnterExitFlags::EnteringOnVehicle), *m_pVehicle, *pFocusPed, m_iTargetEntryPoint, &m_vLocalSpaceEntryModifier);
			TUNE_GROUP_BOOL(ENTER_VEHICLE_TUNE, RENDER_ALIGN_TARGET, true);
			if (RENDER_ALIGN_TARGET)
			{
				grcDebugDraw::Sphere(vNewTargetPosition, 0.05f, Color_cyan);
			}
		}

		grcDebugDraw::AddDebugOutput(Color_green, "Target Seat : %u", m_iSeat);
		grcDebugDraw::AddDebugOutput(Color_green, "Target Entrypoint : %u", m_iTargetEntryPoint);
		if (pFocusPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
		{
			grcDebugDraw::AddDebugOutput(Color_green, "Ped In Seat");
		}
		else
		{
			grcDebugDraw::AddDebugOutput(Color_red, "Ped Not In Seat");
		}
	}

	if (m_pVehicle && m_pVehicle->InheritsFromBike())
	{
		grcDebugDraw::AddDebugOutput(Color_green, "Lean Angle : %.4f", static_cast<const CBike*>(m_pVehicle.Get())->GetLeanAngle());
	}

	static bool RENDER_ENTRY_POINTS = false;
	if (RENDER_ENTRY_POINTS)
	{
		Vector3 vTargetPos(Vector3::ZeroType);
		Quaternion qTargetOrientation(0.0f,0.0f,0.0f,1.0f);

		m_PlayAttachedClipHelper.GetTarget(vTargetPos, qTargetOrientation);

		Matrix34 mTemp(Matrix34::IdentityType);
		mTemp.FromQuaternion(qTargetOrientation);
		mTemp.d = vTargetPos;

		static float sfScale = 0.5f;
		grcDebugDraw::Axis(mTemp, sfScale);
	}
#endif
}
#endif // !__FINAL

CTaskFSMClone *CClonedEnterVehicleInfo::CreateCloneFSMTask()
{
	CTaskEnterVehicle* pEnterTask = rage_new CTaskEnterVehicle(static_cast<CVehicle*>(GetTarget()), m_iSeatRequestType, GetSeat(), m_iRunningFlags, 0.0f, m_createdLocalTaskMBR > 0.f ? m_createdLocalTaskMBR : MOVEBLENDRATIO_RUN);
	pEnterTask->SetStreamedClipSetId(m_StreamedClipSetId);
	pEnterTask->SetStreamedClipId(m_StreamedClipId);
	return pEnterTask;
}

CTask *CClonedEnterVehicleInfo::CreateLocalTask(fwEntity* pEntity)
{
	m_createdLocalTaskMBR = -1.f; //reset
	if (pEntity && pEntity->GetType() == ENTITY_TYPE_PED)
	{
		CPed* pPed = static_cast<CPed*>(pEntity);
		if (pPed && pPed->GetMotionData())
		{
			m_createdLocalTaskMBR = pPed->GetMotionData()->GetCurrentMoveBlendRatio().Mag();
		}
	}

    return CreateCloneFSMTask();
}

CClonedEnterVehicleInfo::CClonedEnterVehicleInfo()
{
}

CClonedEnterVehicleInfo::CClonedEnterVehicleInfo(s32 enterState, 
												 CVehicle* pVehicle, 
												 SeatRequestType iSeatRequestType, 
												 s32 iSeat, 
												 s32 iTargetEntryPoint, 
												 VehicleEnterExitFlags iRunningFlags, 
												 CPed* pJackedPed, 
												 bool bShouldTriggerElectricTask, 
												 const fwMvClipSetId& clipSetId, 
												 u32 damageTotalTime, 
												 eRagdollTriggerTypes ragdollType,
												 const fwMvClipSetId &streamedClipSetId,
												 const fwMvClipId & streamedClipId,
												 bool bVehicleDoorOpen,
												 const Vector3& targetPosition)
: CClonedVehicleFSMInfo(enterState, pVehicle, iSeatRequestType, iSeat, iTargetEntryPoint, iRunningFlags)
, m_pJackedPed(pJackedPed)
, m_bShouldTriggerElectricTask(bShouldTriggerElectricTask)
, m_clipSetId(clipSetId)
, m_uDamageTotalTime(damageTotalTime)
, m_RagdollType(ragdollType)
, m_StreamedClipSetId(streamedClipSetId)
, m_StreamedClipId(streamedClipId)
, m_bVehicleDoorOpen(bVehicleDoorOpen)
, m_targetPosition(targetPosition)
, m_createdLocalTaskMBR(-1.0f)
{

}

////////////////////////////////////////////////////////////////////////////////

// Statics
CTaskEnterVehicleAlign::Tunables CTaskEnterVehicleAlign::ms_Tunables;

IMPLEMENT_VEHICLE_TASK_TUNABLES(CTaskEnterVehicleAlign, 0xe8749847);

////////////////////////////////////////////////////////////////////////////////

const fwMvBooleanId CTaskEnterVehicleAlign::ms_ShortAlignOnEnterId("ShortAlign_OnEnter",0x5C700A9F);
const fwMvBooleanId CTaskEnterVehicleAlign::ms_OrientatedAlignOnEnterId("OrientatedAlign_OnEnter",0xD6A14855);
const fwMvBooleanId CTaskEnterVehicleAlign::ms_AlignClipFinishedId("AlignAnimFinished",0x34D942E6);
const fwMvBooleanId CTaskEnterVehicleAlign::ms_FrontAlignClipFinishedId("FrontAlignAnimFinished",0x9F268A90);
const fwMvBooleanId CTaskEnterVehicleAlign::ms_SideAlignClipFinishedId("SideAlignAnimFinished",0x6A7893AE);
const fwMvBooleanId CTaskEnterVehicleAlign::ms_RearAlignClipFinishedId("RearAlignAnimFinished",0x1C4CA5CD);
const fwMvBooleanId CTaskEnterVehicleAlign::ms_DisableEarlyAlignBlendOutId("DisableEarlyAlignBlendOut",0xfd054007);
const fwMvFlagId	CTaskEnterVehicleAlign::ms_RunFlagId("Run",0x1109B569);
const fwMvFlagId	CTaskEnterVehicleAlign::ms_LeftFootFirstFlagId("LeftFootFirst",0xF5E61837);
const fwMvFlagId	CTaskEnterVehicleAlign::ms_StandFlagId("Stand",0xCE7F7450);
const fwMvFlagId	CTaskEnterVehicleAlign::ms_BikeOnGroundId("BikeOnGround",0x8E4AC680);
const fwMvFlagId	CTaskEnterVehicleAlign::ms_FromSwimmingId("FromSwimming",0x9CA0FB98);
const fwMvFlagId	CTaskEnterVehicleAlign::ms_FromOnVehicleId("FromOnVehicle",0xEF7B2946);
const fwMvFlagId	CTaskEnterVehicleAlign::ms_ShortAlignFlagId("ShortAlign",0x64968391);
const fwMvFloatId	CTaskEnterVehicleAlign::ms_AlignPhaseId("AlignPhase",0x870DF6B5);
const fwMvFloatId	CTaskEnterVehicleAlign::ms_AlignAngleId("AlignAngle",0x6B3DEC39);
const fwMvFloatId	CTaskEnterVehicleAlign::ms_AlignFrontBlendWeightId("front_blend_weight",0xC052E552);
const fwMvFloatId	CTaskEnterVehicleAlign::ms_AlignSideBlendWeightId("side_blend_weight",0x8FA95C96);
const fwMvFloatId	CTaskEnterVehicleAlign::ms_AlignRearBlendWeightId("rear_blend_weight",0x7C580AC5);

const fwMvFloatId	CTaskEnterVehicleAlign::ms_StandAlign180LBlendWeightId("stand_180L_blend_weight",0x3AD2D264);
const fwMvFloatId	CTaskEnterVehicleAlign::ms_StandAlign90LBlendWeightId("stand_90L_blend_weight",0xA72D1A1);
const fwMvFloatId	CTaskEnterVehicleAlign::ms_StandAlign0BlendWeightId("stand_0_blend_weight",0x19DB107);
const fwMvFloatId	CTaskEnterVehicleAlign::ms_StandAlign90RBlendWeightId("stand_90R_blend_weight",0xABB3FCF);
const fwMvFloatId	CTaskEnterVehicleAlign::ms_StandAlign180RBlendWeightId("stand_180R_blend_weight",0x2E589F4B);

const fwMvFloatId	CTaskEnterVehicleAlign::ms_AlignFrontPhaseId("align_front_phase",0x169EA0A3);
const fwMvFloatId	CTaskEnterVehicleAlign::ms_AlignSidePhaseId("align_side_phase",0x2E607CC4);
const fwMvFloatId	CTaskEnterVehicleAlign::ms_AlignRearPhaseId("align_rear_phase",0xCDEF31F8);

const fwMvFloatId	CTaskEnterVehicleAlign::ms_StandAlign180LPhaseId("stand_180L_phase",0x54A989F4);
const fwMvFloatId	CTaskEnterVehicleAlign::ms_StandAlign90LPhaseId("stand_90L_phase",0xA9E74B61);
const fwMvFloatId	CTaskEnterVehicleAlign::ms_StandAlign0PhaseId("stand_0_phase",0x8E3ED49);
const fwMvFloatId	CTaskEnterVehicleAlign::ms_StandAlign90RPhaseId("stand_90R_phase",0xF9F7E1B4);
const fwMvFloatId	CTaskEnterVehicleAlign::ms_StandAlign180RPhaseId("stand_180R_phase",0x82C995FF);

const fwMvFloatId	CTaskEnterVehicleAlign::ms_AlignRateId("AlignRate",0x4B89546D);
const fwMvClipId	CTaskEnterVehicleAlign::ms_AlignFrontClipId("align_front_clip",0xDFC10BD6);
const fwMvClipId	CTaskEnterVehicleAlign::ms_AlignSideClipId("align_side_clip",0xBBEB9B1D);
const fwMvClipId	CTaskEnterVehicleAlign::ms_AlignRearClipId("align_rear_clip",0x4C50EA26);
const fwMvClipId	CTaskEnterVehicleAlign::ms_AlignClipId("AlignClip",0x130AE784);

const fwMvClipId	CTaskEnterVehicleAlign::ms_StandAlign180LClipId("stand_180L_clip",0x91C6B90D);
const fwMvClipId	CTaskEnterVehicleAlign::ms_StandAlign90LClipId("stand_90L_clip",0x5661831D);
const fwMvClipId	CTaskEnterVehicleAlign::ms_StandAlign0ClipId("stand_0_clip",0x26E9BF48);
const fwMvClipId	CTaskEnterVehicleAlign::ms_StandAlign90RClipId("stand_90R_clip",0x93DD60A0);
const fwMvClipId	CTaskEnterVehicleAlign::ms_StandAlign180RClipId("stand_180R_clip",0xFBBC5AA4);

const fwMvStateId	CTaskEnterVehicleAlign::ms_ShortAlignStateId("ShortAlign",0x64968391);
const fwMvStateId	CTaskEnterVehicleAlign::ms_OrientatedAlignStateId("OrientatedAlign",0xDACA85E4);

#define ALIGN_DEBUG_ONLY(x) BANK_ONLY(if (ms_Tunables.m_RenderDebugToTTY) x)

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicleAlign::IsTaskValid(const CEntity* pVehicle, s32 iEntryPointIndex)
{
	if (!IsEntryIndexValid(pVehicle, iEntryPointIndex))
		return false;

	return true;
}

bool CTaskEnterVehicleAlign::IsEntryIndexValid(const CEntity* pVehicle, s32 iEntryPointIndex)
{
	if (!pVehicle)
		return false;

	if (iEntryPointIndex >= 0)
	{
		if (pVehicle->GetIsTypePed()) 
		{
			const CModelSeatInfo* pModelSeatinfo  = static_cast<const CPed*>(pVehicle)->GetPedModelInfo()->GetModelSeatInfo();
			return (pModelSeatinfo && iEntryPointIndex < pModelSeatinfo->GetNumberEntryExitPoints());			
		} 
		else if (pVehicle->GetIsTypeVehicle())
		{
			return static_cast<const CVehicle*>(pVehicle)->IsEntryIndexValid(iEntryPointIndex);
		}		
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CTaskEnterVehicleAlign::GetEntryClipsetId(const CEntity* pEntity, s32 iEntryPointIndex, const CPed* pEnteringPed)
{
	if (!IsTaskValid(pEntity, iEntryPointIndex))
		return CLIP_SET_ID_INVALID;

	if (pEntity->GetIsTypePed()) 
	{		
		const CVehicleEntryPointAnimInfo* pEntryInfo = static_cast<const CPed&>(*pEntity).GetPedModelInfo()->GetLayoutInfo()->GetEntryPointAnimInfo(iEntryPointIndex);
		if (taskVerifyf(pEntryInfo, "NULL entry anim info for ped %s, entry point %i", pEntity->GetModelName(), iEntryPointIndex))
		{
			return pEntryInfo->GetEntryPointClipSetId();
		}
	} 
	else if (pEntity->GetIsTypeVehicle())
	{
		const CVehicle& rVeh = static_cast<const CVehicle&>(*pEntity);
		const CVehicleEntryPointAnimInfo* pEntryInfo = rVeh.GetEntryAnimInfo(iEntryPointIndex);
#if FPS_MODE_SUPPORTED
		if (rVeh.InheritsFromBicycle() && pEnteringPed && pEnteringPed->IsFirstPersonShooterModeEnabledForPlayer(false))
		{
			return pEntryInfo->GetEntryPointClipSetId(1);
		}
#endif // FPS_MODE_SUPPORTED

		if (taskVerifyf(pEntryInfo, "NULL entry anim info for vehicle %s, entry point %i", pEntity->GetModelName(), iEntryPointIndex))
		{
			if (!NetworkInterface::IsGameInProgress() && pEnteringPed && !pEnteringPed->IsMale() && pEntryInfo->GetAlternateEntryPointClipSet() != CLIP_SET_ID_INVALID)
			{
				return pEntryInfo->GetAlternateEntryPointClipSet();
			}
			return pEntryInfo->GetEntryPointClipSetId();
		}
	}
	return CLIP_SET_ID_INVALID;	
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CTaskEnterVehicleAlign::GetCommonClipsetId(const CEntity* pEntity, s32 iEntryPointIndex, bool bOnVehicle)
{
	if (!IsTaskValid(pEntity, iEntryPointIndex))
		return CLIP_SET_ID_INVALID;

	if (pEntity->GetIsTypePed()) 
	{		
		const CVehicleEntryPointAnimInfo* pEntryInfo = static_cast<const CPed*>(pEntity)->GetPedModelInfo()->GetLayoutInfo()->GetEntryPointAnimInfo(iEntryPointIndex);
		if (taskVerifyf(pEntryInfo, "NULL entry anim info for ped %s, entry point %i", pEntity->GetModelName(), iEntryPointIndex))
		{
			return bOnVehicle ? pEntryInfo->GetCommonOnVehicleClipSet() : pEntryInfo->GetCommonClipSetId();
		}
	} 
	else if (pEntity->GetIsTypeVehicle())
	{
		const CVehicleEntryPointAnimInfo* pEntryInfo = static_cast<const CVehicle*>(pEntity)->GetEntryAnimInfo(iEntryPointIndex);
		if (taskVerifyf(pEntryInfo, "NULL entry anim info for vehicle %s, entry point %i", pEntity->GetModelName(), iEntryPointIndex))
		{
			if(bOnVehicle)
			{
				const CVehicle* pVehicle = static_cast<const CVehicle*>(pEntity);
				s32 iTargetSeat = pVehicle->GetEntryExitPoint(iEntryPointIndex)->GetSeat(SA_directAccessSeat);
				if (pVehicle->IsSeatIndexValid(iTargetSeat) && pVehicle->GetSeatManager()->GetPedInSeat(iTargetSeat) 
					&& !pVehicle->GetSeatManager()->GetPedInSeat(iTargetSeat)->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE))
				{
					const CVehicleEntryPointInfo* pEntryPointInfo = pVehicle->GetEntryInfo(iEntryPointIndex);
					if (pEntryPointInfo && pEntryPointInfo->GetVehicleExtraPointsInfo() && pEntryPointInfo->GetVehicleExtraPointsInfo()->GetExtraVehiclePointOfType(CExtraVehiclePoint::ON_BOARD_JACK))
					{
						if (pEntryInfo->GetCommonOBoardJackVehicleClipSet() != CLIP_SET_ID_INVALID)
						{
							return pEntryInfo->GetCommonOBoardJackVehicleClipSet();
						}
					}
				}

				if (pEntryInfo->GetCommonOnVehicleClipSet() != CLIP_SET_ID_INVALID)
			{
				return pEntryInfo->GetCommonOnVehicleClipSet();
			}
			}
			return pEntryInfo->GetCommonClipSetId();
		}
	}
	return CLIP_SET_ID_INVALID;	
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicleAlign::AlignSucceeded(const CEntity& vehicle, const CPed& ped, s32 iEntryPointIndex, bool bEnterOnVehicle, bool bCombatEntry, bool bInWaterEntry, Vector3 * pvLocalSpaceOffset, bool bUseCloserTolerance, bool bFinishedAlign, bool bFromCover)
{
	// Calculate the world space position and orientation we would like to lerp to during the align
	Vector3 vEntryPos(Vector3::ZeroType);
	Quaternion qEntryOrientation(0.0f,0.0f,0.0f,1.0f);
	ComputeAlignTarget(vEntryPos, qEntryOrientation, bEnterOnVehicle, vehicle, ped, iEntryPointIndex, pvLocalSpaceOffset, false, bInWaterEntry);

	TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, MAX_HEADING_DELTA_TO_ACCEPT_FROM_COVER, 1.59f, 0.0f, 3.0f, 0.01f);
	TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, MAX_HEADING_DELTA_TO_ACCEPT, 0.25f, 0.0f, 1.0f, 0.01f);
	const float fMaxAcceptableHeadingDelta = bFromCover ? MAX_HEADING_DELTA_TO_ACCEPT_FROM_COVER : MAX_HEADING_DELTA_TO_ACCEPT;
	const float fEntryHeading = qEntryOrientation.TwistAngle(ZAXIS);
	const float fPedHeading = ped.GetCurrentHeading();
	const float fDeltaHeading = fwAngle::LimitRadianAngle(fEntryHeading - fPedHeading);
	if (Abs(fDeltaHeading) > fMaxAcceptableHeadingDelta)
	{
		aiDisplayf("Frame : %i, Ped %s, align failed, heading delta %.2f", fwTimer::GetFrameCount(), ped.GetModelName(), fDeltaHeading);
		return false;
	}

	vEntryPos.z = 0.0f;
	Vector3 vPedPos = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
	vPedPos.z = 0.0f;
	const float fDistToEntryPoint = vEntryPos.Dist2(vPedPos);

	TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, MBR_TO_CONSIDER_SLIDE_BACK, 0.1f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, COMBAT_ENTRY_RADIUS, 0.15f, 0.0f, 1.0f, 0.01f);
	float fDistTolerance = bCombatEntry ? rage::square(COMBAT_ENTRY_RADIUS) : rage::square(ms_Tunables.m_StandAlignMaxDist);

	if (!bFinishedAlign && !bUseCloserTolerance && vehicle.GetIsTypeVehicle() && ped.GetMotionData()->GetCurrentMbrY() < MBR_TO_CONSIDER_SLIDE_BACK)
	{
		Vector3 vPedToEntry = vEntryPos - vPedPos;
		vPedToEntry.z = 0.0f;
		vPedToEntry.Normalize();
		Vector3 vToVeh = VEC3V_TO_VECTOR3(vehicle.GetTransform().GetPosition()) - vPedPos;
		vToVeh.z = 0.0f;
		vToVeh.Normalize();
		if (vPedToEntry.Dot(vToVeh) < 0.0f)
		{
			TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, BEHIND_ENTRY_TOLERANCE, 0.05f, 0.0f, 1.0f, 0.01f);
			fDistTolerance = BEHIND_ENTRY_TOLERANCE;
		}
	}

	TUNE_GROUP_BOOL(FIRST_PERSON_ENTER_VEHICLE_TUNE, USE_CLOSER_POSITION_TOLERANCE_IN_FIRST_PERSON, true);
	TUNE_GROUP_FLOAT(FIRST_PERSON_ENTER_VEHICLE_TUNE, POSITION_TOLERANCE, 0.005f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_ENTER_VEHICLE_TUNE, POSITION_TOLERANCE_NOKOTA, 0.01f, 0.0f, 1.0f, 0.01f);
	bool bIsNokota = MI_PLANE_NOKOTA.IsValid() && vehicle.GetModelIndex() == MI_PLANE_NOKOTA;
	bool bUseFurtherPositionToleranceFP = bIsNokota;
	if (USE_CLOSER_POSITION_TOLERANCE_IN_FIRST_PERSON FPS_MODE_SUPPORTED_ONLY(&& ped.IsFirstPersonShooterModeEnabledForPlayer(false)))
	{
		fDistTolerance = bUseFurtherPositionToleranceFP ? POSITION_TOLERANCE_NOKOTA : POSITION_TOLERANCE;
	}
	else if (bUseCloserTolerance)
	{
		TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, CLOSE_TOLERANCE, 0.01f, 0.0f, 1.0f, 0.01f);
		fDistTolerance = CLOSE_TOLERANCE;
	}

	const bool bAlignSucceeded = fDistToEntryPoint < fDistTolerance ? true : false;

#if __DEV
	const Vector3 vToEntryPos = vEntryPos - VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
	const float fDistToEntry = vToEntryPos.XYMag();
	aiDisplayf("Frame : %i, Ped %s, align succeeded %s, %.2f m away from entry point(Desired : %.2f)", fwTimer::GetFrameCount(), ped.GetModelName(), bAlignSucceeded ? "TRUE": "FALSE", fDistToEntry, sqrtf(fDistTolerance));
#endif // __DEV

	return bAlignSucceeded;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicleAlign::ShouldDoStandAlign(const CEntity& vehicle, const CPed& ped, s32 iEntryPointIndex, Vector3 * pvLocalSpaceOffset)
{
	if (ms_Tunables.m_ForceStandEnterOnly)
	{
		return true;
	}
	// Calculate the world space position and orientation we would like to lerp to during the align
	Vector3 vEntryPos(Vector3::ZeroType);
	Quaternion qEntryOrientation(0.0f,0.0f,0.0f,1.0f);

	CModelSeatInfo::CalculateEntrySituation(&vehicle, &ped, vEntryPos, qEntryOrientation, iEntryPointIndex, 0, 0.0f, pvLocalSpaceOffset);
	vEntryPos.z = 0.0f;
	Vector3 vPedPos = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
	vPedPos.z = 0.0f;
	const float fDistToEntryPoint = vEntryPos.Dist2(vPedPos);
	return fDistToEntryPoint < ms_Tunables.m_StandAlignMaxDist*ms_Tunables.m_StandAlignMaxDist ? true : false;
}
/////////////////////////////////////////////////////////////////////////

const CVehicleEntryPointAnimInfo* CTaskEnterVehicleAlign::GetEntryAnimInfo(const CEntity* vehicle, s32 iEntryPointIndex)
{
	const CVehicleEntryPointAnimInfo* pClipInfo=NULL;
	if (vehicle->GetIsTypePed()) 
	{		
		pClipInfo = static_cast<const CPed*>(vehicle)->GetPedModelInfo()->GetLayoutInfo()->GetEntryPointAnimInfo(iEntryPointIndex);
	}
	else if (vehicle->GetIsTypeVehicle()) 
	{
		pClipInfo = static_cast<const CVehicle*>(vehicle)->GetEntryAnimInfo(iEntryPointIndex);
	}	
	
	return pClipInfo;
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleEntryPointInfo* CTaskEnterVehicleAlign::GetEntryInfo(s32 iEntryPointIndex) const
{
	const CVehicleEntryPointInfo* pEntryInfo=NULL;
	if (m_pVehicle->GetIsTypePed()) 
	{		
		pEntryInfo = static_cast<const CPed*>(m_pVehicle.Get())->GetPedModelInfo()->GetLayoutInfo()->GetEntryPointInfo(iEntryPointIndex);
	}
	else if (m_pVehicle->GetIsTypeVehicle()) 
	{
		pEntryInfo = static_cast<const CVehicle*>(m_pVehicle.Get())->GetEntryInfo(iEntryPointIndex);
	}	
	return pEntryInfo;
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleLayoutInfo* CTaskEnterVehicleAlign::GetLayoutInfo(const CEntity* pEnt)
{
	const CVehicleLayoutInfo* pLayoutInfo=NULL;
	if (pEnt->GetIsTypePed()) 
	{		
		pLayoutInfo = static_cast<const CPed*>(pEnt)->GetPedModelInfo()->GetLayoutInfo();
	}
	else if (pEnt->GetIsTypeVehicle()) 
	{
		pLayoutInfo = static_cast<const CVehicle*>(pEnt)->GetLayoutInfo();
	}	
	return pLayoutInfo;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicleAlign::PassesGroundConditionForAlign(const CPed& rPed, const CEntity& rEnt)
{
	if (!rEnt.GetIsTypeVehicle())
		return true;
	
	if (rPed.IsOnGround())
		return true;

	const CVehicle& rVeh = static_cast<const CVehicle&>(rEnt);
	if (rVeh.GetIsAquatic() || rVeh.pHandling->GetSeaPlaneHandlingData())
		return true;

	aiDisplayf("Frame %i: %s Ped %s failed PassesGroundConditionForAlign", fwTimer::GetFrameCount(), rPed.IsNetworkClone() ? "CLONE" : "LOCAL", rPed.GetNetworkObject() ? rPed.GetNetworkObject()->GetLogName() : rPed.GetModelName());
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicleAlign::PassesOnVehicleHeightDeltaConditionForAlign(const CPed& rPed, const CEntity& rEnt, bool bEntryOnVehicle, s32 iEntryPointIndex, const Vector3* pvLocalSpaceOffset)
{
	if (!bEntryOnVehicle)
		return true;

	if (!rEnt.GetIsTypeVehicle())
		return true;

	const CVehicle& rVeh = static_cast<const CVehicle&>(rEnt);
	if (rVeh.GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_TURRET_SEAT_ON_VEHICLE))
	{
		if (!rVeh.GetVehicleWeaponMgr())
			return true;

		s32 iDirectSeat = rVeh.GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryExitPoint(iEntryPointIndex)->GetSeat(SA_directAccessSeat);
		const CTurret* pTurret = rVeh.GetVehicleWeaponMgr()->GetFirstTurretForSeat(iDirectSeat);
		if (!pTurret)
			return true;

		bool bIsAATrailer = MI_TRAILER_TRAILERSMALL2.IsValid() && rVeh.GetModelIndex() == MI_TRAILER_TRAILERSMALL2;
		if ((rVeh.GetSeatAnimationInfo(iDirectSeat)->IsStandingTurretSeat() || bIsAATrailer) && !rPed.GetGroundPhysical())
		{
			const CTaskEnterVehicle* pEnterVehicleTask = static_cast<const CTaskEnterVehicle*>(rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
			if (pEnterVehicleTask && !pEnterVehicleTask->IsFlagSet(CVehicleEnterExitFlags::UsedClimbUp))
			{
				return false;
			}
		}
	}
	else if (!rVeh.InheritsFromPlane())
	{
		return true;
	}

	Vector3 vAlignPos;
	Quaternion qAlignRot;
	ComputeAlignTarget(vAlignPos, qAlignRot, bEntryOnVehicle, rVeh, rPed, iEntryPointIndex, pvLocalSpaceOffset);

	const float fDeltaZ = Abs(rPed.GetTransform().GetPosition().GetZf() - vAlignPos.z);
	TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, MAX_Z_DELTA_FOR_ALIGN, 0.6f, 0.0f, 1.0f, 0.01f);
	if (fDeltaZ < MAX_Z_DELTA_FOR_ALIGN)
		return true;

#if __BANK
	aiDisplayf("Frame %i: %s Ped %s failed PassesHeightDeltaConditionForAlign, Delta = %.2f", fwTimer::GetFrameCount(), AILogging::GetDynamicEntityIsCloneStringSafe(&rPed), AILogging::GetDynamicEntityNameSafe(&rPed), fDeltaZ);
#endif // __BANK
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicleAlign::ShouldTriggerAlign(const CEntity& vehicle, const CPed& ped, s32 iEntryPointIndex, Vector3 * pvLocalSpaceOffset, bool bCombatEntry, bool bFromCover, bool bDisallowUnderWater, bool bIsOnVehicleEntry)
{
	if(ped.GetPedConfigFlag(CPED_CONFIG_FLAG_WillArrestRatherThanJack))
	{
		return false;
	}

	const CVehicleEntryPointAnimInfo* pClipInfo= CTaskEnterVehicleAlign::GetEntryAnimInfo(&vehicle, iEntryPointIndex);	
	if (taskVerifyf(pClipInfo, "NULL entry point clip info for entrypoint %u", iEntryPointIndex))
	{
		const bool bHasOnVehicleEntry = pClipInfo->GetHasOnVehicleEntry();
		const bool bOnGroundPhysical = ped.GetGroundPhysical() == &vehicle;
		if (!bHasOnVehicleEntry && bOnGroundPhysical)
		{
			return false;
		}

		if (!ped.IsLocalPlayer() && !ped.IsNetworkClone() && vehicle.GetIsTypeVehicle())
		{
			const CVehicle& rVeh = static_cast<const CVehicle&>(vehicle);
			const s32 iSeatIndex = rVeh.GetEntryExitPoint(iEntryPointIndex)->GetSeat(SA_directAccessSeat);
			if (!rVeh.IsTurretSeat(iSeatIndex))
			{
				const Vector3 vRelativeVelocity = rVeh.GetRelativeVelocity(ped);
				if (vRelativeVelocity.Mag2() >= square(CTaskEnterVehicle::ms_Tunables.m_MinSpeedToAbortOpenDoorCombat))
				{
					return false;
				}

				if (!CTaskEnterVehicle::PassesPlayerDriverAlignCondition(ped,rVeh))
				{
					return false;
				}
			}
		}

		//! prevent aligning underwater. must surface 1st.
		if (bDisallowUnderWater && ped.GetPrimaryMotionTask() && (ped.GetPrimaryMotionTask()->IsDiving()))
		{
			return false;
		}

		// Prevent aligning when not on the ground for non aquatic vehicles
		if (!PassesGroundConditionForAlign(ped, vehicle) && !bIsOnVehicleEntry)
		{
			return false;
		}

		bool bDoDistanceCheck = true;

		const bool bFromWater = pClipInfo->GetHasGetInFromWater() || pClipInfo->GetHasClimbUpFromWater();

		if(bFromWater && (ped.GetAttachParent() == &vehicle))
		{
			bDoDistanceCheck = false;
		}

		if(bDoDistanceCheck)
		{
		// Calculate the world space position and orientation we would like to lerp to during the align
		Vector3 vEntryPos(Vector3::ZeroType);
		Quaternion qEntryOrientation(0.0f,0.0f,0.0f,1.0f);
		Vector3 vPedPos = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());

		// Maybe we should just have a func to compute the matrix?
		CTaskEnterVehicleAlign::ComputeAlignTarget(vEntryPos, qEntryOrientation, bIsOnVehicleEntry, vehicle, ped, iEntryPointIndex, pvLocalSpaceOffset);

		if (bIsOnVehicleEntry)
		{
			bool bIsMogul = MI_PLANE_MOGUL.IsValid() && vehicle.GetModelIndex() == MI_PLANE_MOGUL;
			if (bIsMogul)
			{
				TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, MOGUL_ONVEHICLE_ENTRY_HEIGHT_DIFF_THRESHOLD, 0.2f, 0.0f, 5.0f,0.1f);
				float fHeightDiff = Abs(vEntryPos.GetZ() - vPedPos.GetZ());
				if (fHeightDiff > MOGUL_ONVEHICLE_ENTRY_HEIGHT_DIFF_THRESHOLD)
				{
					return false;
				}
			}
		}

		vEntryPos.z = 0.0f;
		vPedPos.z = 0.0f;
		const float fDistToEntryPoint = vEntryPos.Dist2(vPedPos);

		Vector3 vToTarget = vEntryPos - vPedPos;
		vToTarget.Normalize();
		Vector3 vPedForward = VEC3V_TO_VECTOR3(ped.GetTransform().GetB());
		vPedForward.z = 0.0f;
		vPedForward.Normalize();

		float fAlignDist = 0.0f;

		if (vehicle.GetIsTypeVehicle() && CTaskVehicleFSM::ShouldWarpInAndOutInMP(static_cast<const CVehicle&>(vehicle), iEntryPointIndex) && pClipInfo->GetNavigateToWarpEntryPoint())
		{
			TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, NAVIGATE_TO_WARP_ENTRY_ALIGN_RADIUS, 2.5f, 0.0f, 5.0f, 0.01f);
			fAlignDist = NAVIGATE_TO_WARP_ENTRY_ALIGN_RADIUS;
		}
		else
		{
			TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, MIN_MBR_TO_USE_COMBAT_TRIGGER, 0.1f, 0.0f, 1.0f, 0.01f);
			TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, TURRET_ENTRY_RADIUS, 1.0f, 0.0f, 1.0f, 0.01f);
			TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, TURRET_ENTRY_RADIUS_FOR_JACK, 0.25f, 0.0f, 1.0f, 0.01f);
			if (!PassesOnVehicleHeightDeltaConditionForAlign(ped, vehicle, bIsOnVehicleEntry, iEntryPointIndex, pvLocalSpaceOffset))
			{
				return false;
			}

			if (ShouldComputeIdealTargetForTurret(vehicle, bIsOnVehicleEntry, iEntryPointIndex))
			{
				const CVehicle& rVeh = static_cast<const CVehicle&>(vehicle);
				s32 iDirectSeat = rVeh.GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryExitPoint(iEntryPointIndex)->GetSeat(SA_directAccessSeat);
				const CPed* pOccupyingPed = rVeh.GetSeatManager()->GetPedInSeat(iDirectSeat);
				fAlignDist = pOccupyingPed ? TURRET_ENTRY_RADIUS_FOR_JACK : TURRET_ENTRY_RADIUS;
			}
			else if (!bIsOnVehicleEntry && pClipInfo->GetHasClimbUp())
			{
				fAlignDist = CTaskEnterVehicle::ms_Tunables.m_ClimbAlignTolerance;
			}
			else if (bCombatEntry && ped.GetMotionData()->GetCurrentMbrX() > MIN_MBR_TO_USE_COMBAT_TRIGGER && ped.GetMotionData()->GetCurrentMbrY() > MIN_MBR_TO_USE_COMBAT_TRIGGER)
			{
				TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, COMBAT_ENTRY_TRIGGER_RADIUS_FROM_FRONT, 0.15f, 0.0f, 1.0f, 0.01f);
				TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, COMBAT_ENTRY_TRIGGER_RADIUS, 0.5f, 0.0f, 1.0f, 0.01f);
				TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, COMBAT_ENTRY_COVER_TRIGGER_RADIUS, 0.5f, 0.0f, 1.0f, 0.01f);

				if (bFromCover && fDistToEntryPoint < rage::square(COMBAT_ENTRY_COVER_TRIGGER_RADIUS))
				{
					fAlignDist = COMBAT_ENTRY_TRIGGER_RADIUS;
				}
				else
				{
					Vector3 vVehForward = VEC3V_TO_VECTOR3(vehicle.GetTransform().GetB());
					vVehForward.z = 0.0f;
					Vector3 vToEntry = vEntryPos - vPedPos;
					if (vToEntry.Dot(vVehForward) < 0.0f)
					{
						fAlignDist = COMBAT_ENTRY_TRIGGER_RADIUS_FROM_FRONT;
					}
					else
					{
						fAlignDist = COMBAT_ENTRY_TRIGGER_RADIUS;
					}
				}
			}
			else
			{
				fAlignDist = CTaskMoveGoToVehicleDoor::ComputeTargetRadius(ped, vehicle, vEntryPos, true, bCombatEntry);
			}
		}

		return fDistToEntryPoint < rage::square(fAlignDist) ? true : false;
	}
		else
		{
			return true;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicleAlign::ShouldComputeIdealTargetForTurret(const CEntity& rEnt, bool bEntryOnVehicle, s32 iEntryPointIndex)
{
	if (!bEntryOnVehicle)
		return false;

	if (!rEnt.GetIsTypeVehicle())
		return false;
		
	const CVehicle& rVeh = static_cast<const CVehicle&>(rEnt);
	if (!rVeh.GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_TURRET_SEAT_ON_VEHICLE))
		return false;

	if (!rVeh.IsEntryIndexValid(iEntryPointIndex))
		return false;

	s32 iDirectSeat = rVeh.GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryExitPoint(iEntryPointIndex)->GetSeat(SA_directAccessSeat);
	if (!rVeh.IsTurretSeat(iDirectSeat))
		return false;
		
	bool bBarrage = MI_CAR_BARRAGE.IsValid() && rVeh.GetModelIndex() == MI_CAR_BARRAGE && iDirectSeat == 3;
	if (!bBarrage && !rVeh.GetSeatAnimationInfo(iDirectSeat)->IsStandingTurretSeat())
		return false;

	const CVehicleWeaponMgr* pVehWeaponMgr = rVeh.GetVehicleWeaponMgr();
	if (!pVehWeaponMgr)
		return false;

	const CTurret* pTurret = pVehWeaponMgr->GetFirstTurretForSeat(iDirectSeat);
	if (!pTurret)
		return false;

	return true;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicleAlign::ComputeAlignTarget(Vector3& vTransTarget, Quaternion& qRotTarget, bool bEntryOnVehicle, const CEntity& rEnt, const CPed& rPed, s32 iEntryPointIndex, const Vector3* pvLocalSpaceOffset, bool bCombatEntry, bool bInWaterEntry, bool bForcedDoorState, bool bForceOpen)
{
	if (rEnt.GetIsTypeVehicle() && bEntryOnVehicle)
	{
		const CVehicle& rVeh = static_cast<const CVehicle&>(rEnt);
		
		// Standing turret seats use a dynamically computed target based on the initial peds position
		if (ShouldComputeIdealTargetForTurret(rVeh, bEntryOnVehicle, iEntryPointIndex) && pvLocalSpaceOffset)
		{
			s32 iDirectSeat = rVeh.GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryExitPoint(iEntryPointIndex)->GetSeat(SA_directAccessSeat);
			const CPed* pOccupyingPed = rVeh.GetSeatManager()->GetPedInSeat(iDirectSeat);
			if (!pOccupyingPed || !CTaskVehicleFSM::ComputeTargetTransformInWorldSpace(rVeh, vTransTarget, qRotTarget, iEntryPointIndex, CExtraVehiclePoint::ON_BOARD_JACK))
			{
				Matrix34 idealMtx;
				const CTaskEnterVehicle* pEnterVehicleTask = static_cast<const CTaskEnterVehicle*>(rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
				if (pEnterVehicleTask && pEnterVehicleTask->IsFlagSet(CVehicleEnterExitFlags::UsedClimbUp))
				{
					idealMtx = MAT34V_TO_MATRIX34(rPed.GetTransform().GetMatrix());
				}
				else
				{
					const CTurret* pTurret = rVeh.GetVehicleWeaponMgr()->GetFirstTurretForSeat(iDirectSeat);
					idealMtx = CTaskVehicleFSM::ComputeIdealSeatMatrixForPosition(*pTurret, rVeh, *pvLocalSpaceOffset, iDirectSeat);
				}
				vTransTarget = idealMtx.d;
				idealMtx.ToQuaternion(qRotTarget);
			}
			return;
		}

		// If the vehicle has a separate entry point when stood on top, use that
		const CVehicleEntryPointAnimInfo* pAnimInfo = rVeh.GetEntryAnimInfo(iEntryPointIndex);
		if (pAnimInfo->GetHasOnVehicleEntry())
		{
			if (!CTaskVehicleFSM::ComputeTargetTransformInWorldSpace(rVeh, vTransTarget, qRotTarget, iEntryPointIndex, CExtraVehiclePoint::GET_IN))
			{
				CModelSeatInfo::CalculateEntrySituation(&rVeh, &rPed, vTransTarget, qRotTarget, iEntryPointIndex);
			}
			return;
		}
	}

	s32 iFlags = bCombatEntry ? CModelSeatInfo::EF_IsCombatEntry : 0;
	
	if(bInWaterEntry)
	{
		iFlags |= CModelSeatInfo::EF_AdjustForWater;
	}

	if (bForcedDoorState)
	{
		if (bForceOpen)
		{
			iFlags |= CModelSeatInfo::EF_ForceUseOpenDoor;
		}
		else
		{
			iFlags |= CModelSeatInfo::EF_DontApplyOpenDoorOffsets;
		}
	}

	CModelSeatInfo::CalculateEntrySituation(&rEnt, &rPed, vTransTarget, qRotTarget, iEntryPointIndex, iFlags, 0.0f, pvLocalSpaceOffset);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicleAlign::SetClipPlaybackRate()
{
	float fClipRate = 1.0f;
	const CAnimRateSet& rAnimRateSet = GetLayoutInfo(m_pVehicle)->GetAnimRateSet();

	// Otherwise determine if we should speed up the entry anims and which tuning values to use (anim vs non anim)
	const CPed& rPed = *GetPed();
	const bool bShouldUseCombatEntryRates = static_cast<CTaskEnterVehicle*>(GetParent())->IsFlagSet(CVehicleEnterExitFlags::CombatEntry) || static_cast<CTaskEnterVehicle*>(GetParent())->IsFlagSet(CVehicleEnterExitFlags::UseFastClipRate) || rPed.GetPedResetFlag(CPED_RESET_FLAG_UseFastEnterExitVehicleRates);
	bool bUseNonAnimCombatEntryRates = false;

	if (bShouldUseCombatEntryRates)
	{
		bUseNonAnimCombatEntryRates = true;
	}

	// Set the clip rate based on our conditions
	if (bUseNonAnimCombatEntryRates)
	{
		fClipRate = rAnimRateSet.m_NoAnimCombatEntry.GetRate();
	}
	else
	{
		fClipRate = rAnimRateSet.m_NormalEntry.GetRate();
	}

	m_MoveNetworkHelper.SetFloat(ms_AlignRateId, fClipRate);
}

////////////////////////////////////////////////////////////////////////////////

CTaskEnterVehicleAlign::CTaskEnterVehicleAlign(CEntity* pVehicle, s32 iEntryPointIndex, const Vector3 & vLocalSpaceOffset, bool bUseFastRate)
: m_pVehicle(pVehicle)
, m_iEntryPointIndex(iEntryPointIndex)
, m_vLocalSpaceOffset(vLocalSpaceOffset)
, m_vLocalInitialPosition(Vector3::ZeroType)
, m_fInitialHeading(0.0f)
, m_fAlignAngleSignal(-1.0f)
, m_fLastPhase(0.0f)
, m_bStartedStandAlign(false)
, m_bComputedMoverTrackChanges(false)
, m_fTotalHeadingChange(0.0f)
, m_vTotalTranslationChange(Vector3::ZeroType)
, m_fFixUpRate(-1.0f)
, m_bApproachFromLeft(false)
, m_fArmIkStartPhase(-1.0f)
, m_bReachRightArm(false)
, m_iMainClipIndex(-1)
, m_bRun(false)
, m_bHeadingReached(false)
, m_vInitialPosition(Vector3::ZeroType)
, m_vLastPosition(Vector3::ZeroType)
, m_fLastHeading(0.0f)
, m_bLastSituationValid(false)
, m_fAverageSpeedDuringBlendIn(-1.0f)
, m_bComputedApproach(false)
, m_bOrientatedAlignStateFinishedThisFrame(false)
, m_fTranslationFixUpStartPhase(-1.0f)
, m_fTranslationFixUpEndPhase(-1.0f)
, m_fRotationFixUpStartPhase(-1.0f)
, m_fRotationFixUpEndPhase(-1.0f)
, m_fFirstPhysicsTimeStep(0.0f)
, m_bProcessPhysicsCalledThisFrame(false)
, m_qFirstPhysicsOrientationDelta(V_IDENTITY)
, m_vFirstPhysicsTranslationDelta(V_ZERO)
, m_vInitialSeatOffset(V_ZERO)
, m_bUseFastRate(bUseFastRate)
, m_bInWaterAlignment(false)
, m_fDestroyWeaponPhase(-1.0f)
{
	SetInternalTaskType(CTaskTypes::TASK_ENTER_VEHICLE_ALIGN);
}

////////////////////////////////////////////////////////////////////////////////

CTaskEnterVehicleAlign::~CTaskEnterVehicleAlign()
{
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicleAlign::CleanUp()
{
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicleAlign::ProcessPreFSM()
{
	if (!IsTaskValid(m_pVehicle, m_iEntryPointIndex))
	{
		return FSM_Quit;
	}

	if (!taskVerifyf(GetParent() && (GetParent()->GetTaskType() == CTaskTypes::TASK_ENTER_VEHICLE || GetParent()->GetTaskType() == CTaskTypes::TASK_MOUNT_ANIMAL), "Invalid parent task"))
	{
		return FSM_Quit;
	}

	CPed* pPed = GetPed();

#if ENABLE_DRUNK
	pPed->SetPedResetFlag(CPED_RESET_FLAG_BlockDrunkBehaviour, true);
#endif // ENABLE_DRUNK
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPhysicsTasks, true);
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostMovement, true);

	m_bComputedAttachThisFrame = false;

	m_fFirstPhysicsTimeStep = 0.0f;

	m_bProcessPhysicsCalledThisFrame = false;
	m_qFirstPhysicsOrientationDelta = QuatV(V_IDENTITY);
	m_vFirstPhysicsTranslationDelta = Vec3V(V_ZERO);

	return FSM_Continue;
}	

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicleAlign::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();	

		FSM_State(State_StreamAssets)
			FSM_OnUpdate
				return StreamAssets_OnUpdate();

		FSM_State(State_OrientatedAlignment)
			FSM_OnEnter
				return OrientatedAlignment_OnEnter();
			FSM_OnUpdate
				return OrientatedAlignment_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicleAlign::ProcessPostFSM()
{	
	UpdateMoVE();
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicleAlign::ProcessPhysics(float fTimeStep, int nTimeSlice)
{
	if (!m_pVehicle)
	{
		return false;
	}

	if (GetState() > State_Start && m_MoveNetworkHelper.IsNetworkActive())
	{
		if (ms_Tunables.m_UseAttachDuringAlign && !m_pVehicle->GetIsTypePed() && GetPed()->GetIsAttached())
		{
			ComputeOrientatedAttachPosition(fTimeStep);
		}
		else
		{
			ComputeDesiredVelocity(fTimeStep);
		}	
	}

	m_fFirstPhysicsTimeStep = fTimeStep;

	// Base class
	return CTask::ProcessPhysics(fTimeStep, nTimeSlice);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicleAlign::ProcessPostMovement()
{
	if (m_iMainClipIndex != -1)
	{
		m_fLastPhase = GetActivePhase(m_iMainClipIndex);
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicleAlign::Start_OnUpdate()
{
	CTaskMotionBase* motionTask	= GetPed()->GetPrimaryMotionTask();

	m_bInWaterAlignment = GetPed()->GetIsSwimming() || (motionTask && motionTask->IsInWater());

	SetTaskState(State_StreamAssets);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CTaskEnterVehicleAlign::GetRequestedEntryClipSetId() const
{
	return GetEntryClipsetId(m_pVehicle, m_iEntryPointIndex, GetPed());
}

fwMvClipSetId CTaskEnterVehicleAlign::GetRequestedCommonClipSetId(bool bEnterOnVehicle) const
{
	return GetCommonClipsetId(m_pVehicle, m_iEntryPointIndex, bEnterOnVehicle);
}

CTask::FSM_Return CTaskEnterVehicleAlign::StreamAssets_OnUpdate()
{
	CTaskEnterVehicle* pParentEnterVehicleTask = (GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_ENTER_VEHICLE) ? static_cast<CTaskEnterVehicle*>(GetParent()) : NULL;
	CPed* pPed = GetPed();

	const bool bEnterOnVehicle = pParentEnterVehicleTask ? pParentEnterVehicleTask->IsOnVehicleEntry() : false;
	bool bEnterOnVehicleConditionPassed = !bEnterOnVehicle;

	fwMvClipSetId entryClipsetId = GetRequestedEntryClipSetId();

	if (!bEnterOnVehicleConditionPassed)
	{
		if(entryClipsetId == CLIP_SET_ID_INVALID)
		{
			SetTaskState(State_Finish);
		}
		bEnterOnVehicleConditionPassed = CTaskVehicleFSM::IsVehicleClipSetLoaded(entryClipsetId);
	}

	fwMvClipSetId commonClipsetId = GetRequestedCommonClipSetId(bEnterOnVehicle);
	const bool bCommonClipSetLoaded = CTaskVehicleFSM::IsVehicleClipSetLoaded(commonClipsetId);
	if(!bCommonClipSetLoaded || !bEnterOnVehicleConditionPassed)
    {
		if(commonClipsetId == CLIP_SET_ID_INVALID)
		{
			SetTaskState(State_Finish);
		}

#if __BANK
        if(pPed->IsNetworkClone())
        {
            if(commonClipsetId.GetCStr())
            {
                AI_LOG_WITH_ARGS("[CTaskEnterVehicleAlign::StreamAssets_OnUpdate] - Running align task before align anims have been streamed in! (clipset is %s%s)\n", commonClipsetId.GetCStr(), bEnterOnVehicleConditionPassed ? "" : " enter conditions not passed");
            }
            else
            {
				AI_LOG_WITH_ARGS("[CTaskEnterVehicleAlign::StreamAssets_OnUpdate] - Running align task before align anims have been streamed in! (clipset hash is %d%s)\n", commonClipsetId.GetHash(), bEnterOnVehicleConditionPassed ? "" : " enter conditions not passed");
            }
        }
#endif // __BANK
	}
	else if (bCommonClipSetLoaded && bEnterOnVehicleConditionPassed)
	{
		taskAssertf(m_MoveNetworkHelper.HasDeferredInsertParent(), "Expected align network helper to have deferred insert parent");
		
		if (m_pVehicle->GetIsTypeVehicle())  
		{
			float fBlendInDuration = pParentEnterVehicleTask->GetNetworkBlendInDuration();
			bool bUnused;
			if (pParentEnterVehicleTask->ShouldBeInActionModeForCombatEntry(bUnused))
			{
				fBlendInDuration = CTaskEnterVehicle::ms_Tunables.m_NetworkBlendDurationOpenDoorCombat;
			}
			if ( !CTaskEnterVehicle::StartMoveNetwork(*pPed, *static_cast<const CVehicle*>(m_pVehicle.Get()), *m_MoveNetworkHelper.GetDeferredInsertParent(), m_iEntryPointIndex, fBlendInDuration, &m_vLocalSpaceOffset, false, pParentEnterVehicleTask->GetEntryClipSetOverride()))
			{
				SetTaskState(State_Finish);
				return FSM_Continue;
			}
		} 
		else if (m_pVehicle->GetIsTypePed())  
		{
#if ENABLE_HORSE
			if ( !CTaskMountAnimal::StartMoveNetwork(pPed, static_cast<const CPed*>(m_pVehicle.Get()), *m_MoveNetworkHelper.GetDeferredInsertParent(), m_iEntryPointIndex, NORMAL_BLEND_DURATION))
			{
				SetTaskState(State_Finish);
				return FSM_Continue;
			}
#endif
		}
		
		// Create and insert the subnetwork now as we need it for going straight into the align
		m_MoveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskAlign);
		ASSERT_ONLY(const CVehicleEntryPointAnimInfo* pEntryClipInfo= CTaskEnterVehicleAlign::GetEntryAnimInfo(m_pVehicle, m_iEntryPointIndex);)
		taskFatalAssertf(pEntryClipInfo, "NULL Entry Info for entry index %i", m_iEntryPointIndex);
		taskAssertf(fwClipSetManager::GetClipSet(pEntryClipInfo->GetCommonClipSetId()), "Couldn't find clipset %s", pEntryClipInfo->GetCommonClipSetId().GetCStr());
		m_MoveNetworkHelper.DeferredInsert();
		m_MoveNetworkHelper.SetClipSet(commonClipsetId, CTaskVehicleFSM::ms_CommonVarClipSetId);
		if (CTaskVehicleFSM::IsVehicleClipSetLoaded(entryClipsetId))
		{
			m_MoveNetworkHelper.SetClipSet(entryClipsetId);
		}
		//React to the jack.
		CReactToBeingJackedHelper::CheckForAndMakeOccupantsReactToBeingJacked(*GetPed(), *m_pVehicle, m_iEntryPointIndex, false);

		// Store initial location
		m_vLocalInitialPosition = VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().UnTransform(pPed->GetTransform().GetPosition()));
		m_fInitialHeading = pPed->GetTransform().GetHeading();
		m_vInitialPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

		Vector3 vSeatPosition(Vector3::ZeroType);
		Quaternion qSeatOrientation(0.0f,0.0f,0.0f,1.0f);
		CModelSeatInfo::CalculateSeatSituation(m_pVehicle, vSeatPosition, qSeatOrientation, m_iEntryPointIndex);
		
		Matrix34 seatMtx;
		seatMtx.FromQuaternion(qSeatOrientation);
		seatMtx.d = vSeatPosition;

		Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		seatMtx.UnTransform(vPedPos);

		m_vInitialSeatOffset = VECTOR3_TO_VEC3V(vPedPos);
		SetTaskState(State_OrientatedAlignment);
		return FSM_Continue;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return  CTaskEnterVehicleAlign::OrientatedAlignment_OnEnter()
{
	CPed* pPed = GetPed();

	if (ms_Tunables.m_UseAttachDuringAlign && !m_pVehicle->GetIsTypePed())
	{
		int iSeatIndex = -1;
		if (m_pVehicle->GetIsTypeVehicle())
		{
			bool bStandingOnVehicle = pPed->GetGroundPhysical() && pPed->GetGroundPhysical() == m_pVehicle;
			CTaskEnterVehicle* pEnterVehicleTask = static_cast<CTaskEnterVehicle*>(GetParent());
			const CVehicle* pVehicle = static_cast<const CVehicle*>(m_pVehicle.Get());
			if (pVehicle->GetIsAquatic() || pVehicle->pHandling->GetSeaPlaneHandlingData())
			{
				const bool bIsSwimming = pPed->GetIsSwimming() || m_bInWaterAlignment;
				if (!bIsSwimming && bStandingOnVehicle)
				{
					pEnterVehicleTask->SetRunningFlag(CVehicleEnterExitFlags::EnteringOnVehicle);
				}
				m_MoveNetworkHelper.SetFlag(bIsSwimming, ms_FromSwimmingId);
				if (bIsSwimming)
				{
					pEnterVehicleTask->SetRunningFlag(CVehicleEnterExitFlags::InWater);	
				}
			}

			iSeatIndex = pVehicle->GetEntryExitPoint(m_iEntryPointIndex)->GetSeat(SA_directAccessSeat);
			const CPed* pOccupyingPed = pVehicle->GetSeatManager()->GetPedInSeat(iSeatIndex);

			// Uber hack if we need to turn the turret, we attach to the drivers seat to avoid spinning with the turret as it turns
			if ( pEnterVehicleTask->IsFlagSet(CVehicleEnterExitFlags::NeedToTurnTurret) 
				|| (pVehicle->GetSeatAnimationInfo(iSeatIndex)->IsStandingTurretSeat() && !pEnterVehicleTask->IsOnVehicleEntry()) 
				|| pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_TURRET_ENTRY_ATTACH_TO_DRIVER_SEAT) )
			{
				iSeatIndex = 0;
			}

			if (pVehicle->IsTrailerSmall2() && pOccupyingPed && !bStandingOnVehicle)
			{
				iSeatIndex = -1;
			}

			pPed->AttachPedToEnterCar(static_cast<CPhysical*>(m_pVehicle.Get()), CTaskEnterVehicle::GetAttachToVehicleFlags(pVehicle), iSeatIndex, m_iEntryPointIndex);
			QuatV qPedOrientation = QuatVFromZAxisAngle(ScalarVFromF32(pPed->GetCurrentHeading()));
			CTaskVehicleFSM::UpdatePedVehicleAttachment(pPed, pPed->GetTransform().GetPosition(), qPedOrientation);
		}
		else if (m_pVehicle->GetIsTypePed())
		{
			CPed* pMount = static_cast<CPed*>(m_pVehicle.Get());
			pPed->SetPedOnMount(pMount);
			//Vector3 attachOffset(Vector3::ZeroType);
			//s16 boneIndex = (s16)pMount->GetBoneIndexFromBoneTag(BONETAG_SKEL_SADDLE);	
			//iSeatIndex = static_cast<CPed*>(m_pVehicle.Get())->GetPedModelInfo()->GetModelSeatInfo()->GetEntryExitPoint(m_iEntryPointIndex)->GetSeat(SA_directAccessSeat);
			//pPed->AttachPedToMountAnimal(pMount, boneIndex, ATTACH_STATE_PED_ENTER_CAR, iSeatIndex, m_iEntryPointIndex);
		}		
	}

	SetInitialSignals();

	// Decide whether to do a short or long alignment
	CTaskEnterVehicle* pEnterVehicleTask = static_cast<CTaskEnterVehicle*>(GetParent());
	if (m_pVehicle->GetIsTypeVehicle() && pEnterVehicleTask->IsFlagSet(CVehicleEnterExitFlags::EnteringOnVehicle))
	{
		fwMvClipSetId commonOnVehicleClipsetId = CTaskEnterVehicleAlign::GetCommonClipsetId(m_pVehicle, m_iEntryPointIndex, true);
		if (CTaskVehicleFSM::IsVehicleClipSetLoaded(commonOnVehicleClipsetId))
		{
			m_MoveNetworkHelper.SetClipSet(commonOnVehicleClipsetId, CTaskVehicleFSM::ms_CommonVarClipSetId);
		}
	}

	if (pPed->GetEquippedWeaponInfo() && pPed->GetEquippedWeaponInfo()->GetRemoveEarlyWhenEnteringVehicles())
	{
		pPed->GetWeaponManager()->DestroyEquippedWeaponObject();
	}

	if (m_pVehicle->GetIsTypeVehicle())
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(m_pVehicle.Get());	
		if (pPed->IsNetworkClone() && 
			pVehicle->IsSeatIndexValid(pEnterVehicleTask->GetTargetSeat()) &&
			pVehicle->GetSeatAnimationInfo(pEnterVehicleTask->GetTargetSeat())->IsStandingTurretSeat() &&
			CTaskVehicleFSM::SeatRequiresHandGripIk(*pVehicle, pEnterVehicleTask->GetTargetSeat()) && pEnterVehicleTask->IsFlagSet(CVehicleEnterExitFlags::EnteringOnVehicle))
		{
			pEnterVehicleTask->StoreInitialReferencePosition(pPed);
		}
	}

	m_fDestroyWeaponPhase = -1.0f;
	m_MoveNetworkHelper.GetDeferredInsertParent()->SendRequest(CTaskEnterVehicle::ms_AlignRequestId);
	m_MoveNetworkHelper.WaitForTargetState(ms_OrientatedAlignOnEnterId);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicleAlign::OrientatedAlignment_OnUpdate()
{
	// Wait until our clip state matches the ai state
	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	CPed *pPed = GetPed();
	CTaskEnterVehicle* pEnterVehicleTask = static_cast<CTaskEnterVehicle*>(GetParent());
	bool bEnteringOnVehicle = false;

	const s32 iTargetSeat = pEnterVehicleTask->GetTargetSeat();
	if (m_pVehicle->GetIsTypeVehicle())
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(m_pVehicle.Get());	

		s32 iMainClipIndex = GetMainClipIndex();
		float fPhase = 0.0f;
		const crClip* pClip = NULL;
		if (iMainClipIndex > -1)
		{
			fPhase = GetActivePhase(m_iMainClipIndex);
			pClip = GetActiveClip(iMainClipIndex);
		}

#if ENABLE_MOTION_IN_TURRET_TASK	
		bEnteringOnVehicle = pEnterVehicleTask->IsFlagSet(CVehicleEnterExitFlags::EnteringOnVehicle);
		if (pClip && pEnterVehicleTask && CTaskVehicleFSM::SeatRequiresHandGripIk(*pVehicle, iTargetSeat) && bEnteringOnVehicle)
		{
			CTurret* pTurret = pVehicle->GetVehicleWeaponMgr() ? pVehicle->GetVehicleWeaponMgr()->GetFirstTurretForSeat(iTargetSeat) : NULL;
			if (pTurret)
			{
				ProcessTurretTurn(*pEnterVehicleTask, *pVehicle);
				const CPed* pOccupier = pVehicle->GetPedInSeat(iTargetSeat);
				if (!pOccupier)
				{
				CTaskVehicleFSM::ProcessTurretHandGripIk(*pVehicle, *pPed, pClip, fPhase, *pTurret);
			}
		}
		}
#endif // ENABLE_MOTION_IN_TURRET_TASK	
		
		const CEntryExitPoint* pEntryPoint = pVehicle->GetEntryExitPoint(m_iEntryPointIndex);
		CCarDoor* pDoor = pVehicle->GetDoorFromBoneIndex(pEntryPoint->GetDoorBoneIndex());

		if (pClip)
		{
			if(pPed->GetWeaponManager() && m_fDestroyWeaponPhase < 0.0f && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_WillArrestRatherThanJack))
			{
				if (!static_cast<CTaskEnterVehicle*>(GetParent())->CTaskEnterVehicle::CheckForStreamedEntry())	// Need to know if it needs the prop?
				{
					const crClip* pClip = GetActiveClip(iMainClipIndex);
					// Destroy or drop weapon depending on clip
					if(pClip)
					{
						CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pClip, CClipEventTags::Object, CClipEventTags::Destroy, true, m_fDestroyWeaponPhase);
					}
				}
			}

			if (m_fDestroyWeaponPhase >= 0.0f && fPhase >= m_fDestroyWeaponPhase && pPed->GetWeaponManager()->GetEquippedWeaponObject())
			{
				pPed->GetWeaponManager()->DestroyEquippedWeaponObject();
			}
		}

		const bool bDoorIsClosedEnough = (pDoor && pDoor->GetIsIntact(pVehicle) && pDoor->GetDoorRatio() <= CTaskEnterVehicle::ms_Tunables.m_DoorRatioToConsiderDoorOpen);
		if (bDoorIsClosedEnough || IsEnteringStandingTurretSeat())
		{
			if (pClip)
			{
				TUNE_BOOL(DISABLE_ALIGN_ARM_IK, true);
				if (!DISABLE_ALIGN_ARM_IK)
				{
					// If we're haven't found the arm ik start phase, try to do so
					if (!IsArmIkPhaseValid())
					{
						float fPhaseToStopIk = 1.0f;	// Unused
						CClipEventTags::FindIkTags(GetActiveClip(iMainClipIndex), m_fArmIkStartPhase, fPhaseToStopIk, m_bReachRightArm, true);

						static bool FORCE_IK_ON = false;
						if (FORCE_IK_ON)
						{
							m_fArmIkStartPhase = 0.0f;
						}
					}

					// Reach for the door handle if we're past the required clip phase and we have one
					if (IsArmIkPhaseValid() && fPhase >= m_fArmIkStartPhase)
					{
						CTaskVehicleFSM::ProcessDoorHandleArmIk(*pVehicle, *pPed, m_iEntryPointIndex, m_bReachRightArm);
					}
				}
			}
		}
	}

	bool bFinish = false;

	const CVehicleEntryPointAnimInfo* pAnimInfo = CTaskEnterVehicleAlign::GetEntryAnimInfo(m_pVehicle, m_iEntryPointIndex);	
	Vector3 vEntryPos(Vector3::ZeroType);
	Quaternion qEntryOrientation(0.0f,0.0f,0.0f,1.0f);
	ComputeAlignTarget(vEntryPos, qEntryOrientation);

	Vector3 vEntryOrientation;
	qEntryOrientation.ToEulers(vEntryOrientation);
	const float fEntryOrientation = fwAngle::LimitRadianAngle(vEntryOrientation.z);
	float fHeadingDiff = fwAngle::LimitRadianAngle(fEntryOrientation - pPed->GetCurrentHeading());

	bool bUseCloseTolerance = false;

	// Aim for a closer alignment if we've just started the align anims
	TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, MAX_PHASE_TO_USE_CLOSE_TOLERANCE, 0.2f, 0.0f, 1.0f, 0.01f);

	float fActivePhase = -1.0f;
	if (GetMainClipIndex() == -1)
	{
		bUseCloseTolerance = true;
	}
	else
	{
		fActivePhase = GetActivePhase(m_iMainClipIndex);
		if (fActivePhase <= MAX_PHASE_TO_USE_CLOSE_TOLERANCE)
		{
			bUseCloseTolerance = true;
		}
	}

	TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, EARLY_DEFAULT_EXIT_HEADING_TOLERANCE, 0.25f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, EARLY_SMALL_EXIT_HEADING_TOLERANCE, 0.025f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, EARLY_EXIT_HEADING_TOLERANCE_BOAT, 0.05f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, EARLY_EXIT_HEADING_TOLERANCE_REAR_BIKE, 0.05f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, EARLY_EXIT_PHASE_TURRET, 0.6f, 0.0f, 1.0f, 0.01f);

	Vector3 vLocalSpaceEntryModifier = static_cast<CTaskEnterVehicle*>(GetParent())->GetLocalSpaceEntryModifier();
	bool bDisableEarlyExit = m_MoveNetworkHelper.GetBoolean(ms_DisableEarlyAlignBlendOutId);
	float fEarlyExitTolerance = EARLY_DEFAULT_EXIT_HEADING_TOLERANCE;
	if(m_pVehicle->GetIsTypeVehicle())
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(m_pVehicle.Get());	
		// Always aim for a close alignment when jacking as the anims need to be closely synced
		const CPed* pPedOccupyingDirectAccessSeat = static_cast<CTaskEnterVehicle*>(GetParent())->GetPedOccupyingDirectAccessSeat();
		const CEntryExitPoint* pEntryPoint = pVehicle->GetEntryExitPoint(m_iEntryPointIndex);
		const s32 iDirectSeatIndex = pEntryPoint ? pEntryPoint->GetSeat(SA_directAccessSeat) : -1;
		const bool bDisableFinerAlignToleranceForSeat = pVehicle->IsSeatIndexValid(iDirectSeatIndex) ? pVehicle->GetSeatInfo(iDirectSeatIndex)->GetDisableFinerAlignTolerance() : false;
		TUNE_GROUP_BOOL(FIRST_PERSON_ENTER_VEHICLE_TUNE, USE_CLOSER_HEADING_TOLERANCE_IN_FIRST_PERSON, true);
		TUNE_GROUP_FLOAT(FIRST_PERSON_ENTER_VEHICLE_TUNE, HEADING_TOLERANCE, 0.005f, 0.0f, 1.0f, 0.01f);
		if (USE_CLOSER_HEADING_TOLERANCE_IN_FIRST_PERSON FPS_MODE_SUPPORTED_ONLY(&& pPed->IsFirstPersonShooterModeEnabledForPlayer(false)))
		{
			fEarlyExitTolerance = HEADING_TOLERANCE;
		}
		else if ((!bDisableFinerAlignToleranceForSeat && pVehicle->GetLayoutInfo()->GetUseFinerAlignTolerance()) || pPedOccupyingDirectAccessSeat || pVehicle->GetDoorLockStateFromEntryPoint(m_iEntryPointIndex) != CARLOCK_UNLOCKED)
		{
			fEarlyExitTolerance = EARLY_SMALL_EXIT_HEADING_TOLERANCE;
		}		
		else if(pVehicle->GetIsAquatic() || (pPed->GetIsInWater() && pVehicle->HasWaterEntry()))
		{
			fEarlyExitTolerance = EARLY_EXIT_HEADING_TOLERANCE_BOAT;
		}
		else if(pVehicle->InheritsFromBike())
		{
			if (pVehicle->IsEntryIndexValid(m_iEntryPointIndex))
			{
				if (iDirectSeatIndex != 0)
				{
					fEarlyExitTolerance = EARLY_EXIT_HEADING_TOLERANCE_REAR_BIKE;
				}
			}
		}
		else if (IsEnteringStandingTurretSeat())
		{
			if (bEnteringOnVehicle && !pPed->GetGroundPhysical() && fActivePhase < EARLY_EXIT_PHASE_TURRET)
			{
				bDisableEarlyExit = true;
			}
			else
			{
				CTurret* pTurret = pVehicle->GetVehicleWeaponMgr() ? pVehicle->GetVehicleWeaponMgr()->GetFirstTurretForSeat(iTargetSeat) : NULL;
				if (pTurret)
				{
					// Check to ensure the turret is pointing in the correct direction before we can early out
					Matrix34 idealMtx = CTaskVehicleFSM::ComputeIdealSeatMatrixForPosition(*pTurret, *pVehicle, vLocalSpaceEntryModifier, iTargetSeat);
					const float fCurrentHeading = pTurret->GetCurrentHeading(pVehicle);
					const float fDesiredHeading = Selectf(Abs(idealMtx.b.z) - 1.0f + VERY_SMALL_FLOAT, rage::Atan2f(idealMtx.c.x,idealMtx.a.x), rage::Atan2f(-idealMtx.b.x, idealMtx.b.y));

					TUNE_GROUP_FLOAT(TURRET_TUNE, EARLY_EXIT_MIN_TURRET_HEADING_DELTA, 0.1f, 0.0f, 1.0f, 0.01f);
					if (Abs(fwAngle::LimitRadianAngle(fDesiredHeading - fCurrentHeading)) > EARLY_EXIT_MIN_TURRET_HEADING_DELTA)
					{
						bDisableEarlyExit = true;
					}
				}
			}
		}
	}
	
    BANK_ONLY(taskDebugf2("Frame : %u - %s%s : %p : in %s:%s: EntryHeading:%.2f PedHeading:%.2f EarlyExit:%.2f", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone ped : " : "Local ped : ", GetPed()->GetDebugName(), GetPed(), GetTaskName(), GetStateName(GetState()), fEntryOrientation, pPed->GetCurrentHeading(), fEarlyExitTolerance));

	if (!bDisableEarlyExit && Abs(fHeadingDiff) < fEarlyExitTolerance)
	{
		bFinish = true;
	}

	const bool bCombatEntry = static_cast<CTaskEnterVehicle*>(GetParent())->ShouldUseCombatEntryAnimLogic();
	const bool bEnterOnVehicle = static_cast<CTaskEnterVehicle*>(GetParent())->IsFlagSet(CVehicleEnterExitFlags::EnteringOnVehicle);
	const bool bInWaterEntry = static_cast<CTaskEnterVehicle*>(GetParent())->IsFlagSet(CVehicleEnterExitFlags::InWater);
	const bool bFromCoverEntry = static_cast<CTaskEnterVehicle*>(GetParent())->IsFlagSet(CVehicleEnterExitFlags::FromCover);

	const Vector3 vToEntryPos = vEntryPos - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	const float fDistToEntrySqd = vToEntryPos.XYMag2();
	if (fDistToEntrySqd > square(ms_Tunables.m_MinDistAwayFromEntryPointToConsiderFinished))
	{
		bFinish = false;
	}
	else if (!CTaskEnterVehicleAlign::AlignSucceeded(*m_pVehicle, *pPed, m_iEntryPointIndex, bEnterOnVehicle && pAnimInfo->GetHasOnVehicleEntry(), bCombatEntry, bInWaterEntry, &vLocalSpaceEntryModifier, bUseCloseTolerance))
	{
		bFinish = false;
	}

	// Check for finishing
	if (bFinish || (GetMainClipIndex() != -1 && HaveAlignClipsFinished()))
	{
		if (!bFinish)
		{
			bool bSetHeading = true;
			if (GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_ENTER_VEHICLE)
			{
				if (static_cast<const CTaskEnterVehicle*>(GetParent())->CheckForRestartingAlign())
				{
					bSetHeading = false;
				}
			}

			if (bSetHeading && !bFromCoverEntry)
			{
				// Set the heading now, we 'should' reach this as part of process physics, but since we're quitting now we'll never get this call
				pPed->SetHeading(fEntryOrientation);
				TUNE_GROUP_BOOL(ENTER_VEHICLE_TUNE, UPDATE_ATTACH_ORIENTATION_AT_END_OF_ALIGN, true);
				if (UPDATE_ATTACH_ORIENTATION_AT_END_OF_ALIGN)
				{
					Quaternion qRotation;
					qRotation.FromRotation(ZAXIS, fEntryOrientation);
					CTaskVehicleFSM::UpdatePedVehicleAttachment(pPed, pPed->GetTransform().GetPosition(), QUATERNION_TO_QUATV(qRotation), true);
				}
			}
		}

		if (m_fDestroyWeaponPhase > 0.0f)
		{
			CPedWeaponManager* pWeaponMgr = pPed->GetWeaponManager();			
			CWeapon* pWeapon = pWeaponMgr->GetEquippedWeapon();
			if (!pWeapon || !pWeapon->GetIsCooking()) 
				pWeaponMgr->DestroyEquippedWeaponObject();
		}
#if __DEV
		aiDisplayf("Frame : %i, Ped %s, finishing align %.2f m away from entry point, (Use close tolerance ? %s, combat entry ? %s, phase %.2f", fwTimer::GetFrameCount(), pPed->GetModelName(), sqrtf(fDistToEntrySqd), bUseCloseTolerance ? "TRUE":"FALSE", bCombatEntry ? "TRUE":"FALSE", fActivePhase);
#endif // __DEV

		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicleAlign::ComputeOrientatedAttachPosition(float fTimeStep)
{
	CPed* pPed = GetPed();

	// Calculate the world space position and orientation we would like to lerp to during the align
	Vector3 vTemp(Vector3::ZeroType);
	Quaternion qTemp(0.0f,0.0f,0.0f,1.0f);
	ComputeAlignTarget(vTemp, qTemp);

	const bool bEnterOnVehicle = static_cast<CTaskEnterVehicle*>(GetParent())->IsFlagSet(CVehicleEnterExitFlags::EnteringOnVehicle);
	const bool bEnteringStandingTurretSeat = IsEnteringStandingTurretSeat();
	TUNE_GROUP_BOOL(TURRET_TUNE, DISABLE_GROUND_FIX_UP_FOR_TURRET, false);
	// Alter the z coord based on the ground position
	if ((!bEnterOnVehicle && !m_bInWaterAlignment) || bEnteringStandingTurretSeat)
	{
		// Hack for technical to prevent aligning on the edge of the vehicle,
		// we push the ground test forwards a bit to avoid the rim
		Vector3 vGroundCheckPosition = vTemp;
		if (bEnteringStandingTurretSeat && !DISABLE_GROUND_FIX_UP_FOR_TURRET)
		{
			Vector3 vFwd;
			qTemp.Transform(Vector3(0.0f, 1.0f, 0.0f), vFwd);
			TUNE_GROUP_FLOAT(TURRET_TUNE, PUSH_FWD_SCALE, 0.2f, -1.0f, 1.0f, 0.01f);
			vFwd.Scale(PUSH_FWD_SCALE);
			vGroundCheckPosition += vFwd;
		}

		Vector3 vGroundPos(Vector3::ZeroType);
		
		TUNE_GROUP_FLOAT(BUG_6862132, PROBE_DEPTH, 2.5f, 0.0f, 10.0f, 0.01f);
		if (CTaskVehicleFSM::FindGroundPos(m_pVehicle, GetPed(), vGroundCheckPosition, PROBE_DEPTH, vGroundPos, !bEnteringStandingTurretSeat))
		{
			// Adjust the reference position
			vTemp.z = vGroundPos.z;

#if AI_DEBUG_OUTPUT_ENABLED
			if (m_pVehicle)
			{
				AI_LOG_WITH_ARGS("[BUG_6862132] FindGroundPos() for [%s] Model Name:[%s] with Start=(%.2f, %.2f, %.2f) and depth %.2f and bIgnoreEntity=%s returned vGroundPos=(%.2f, %.2f, %.2f)\n",
					AILogging::GetDynamicEntityNameSafe(static_cast<CDynamicEntity*>(m_pVehicle.Get())), m_pVehicle->GetModelName(), VEC3V_ARGS(VECTOR3_TO_VEC3V(vGroundCheckPosition)), PROBE_DEPTH, bEnteringStandingTurretSeat ? "FALSE" : "TRUE", VEC3V_ARGS(VECTOR3_TO_VEC3V(vGroundPos)));
			}
#endif
		}
	}

	// Convert to new vector library
	Vec3V vEntryPos = VECTOR3_TO_VEC3V(vTemp);
	ASSERT_ONLY(QuatV qEntryRot = QUATERNION_TO_QUATV(qTemp);)
	taskAssertf(IsFiniteAll(vEntryPos), "vEntryPos wasn't finite");
	taskAssertf(IsFiniteAll(qEntryRot), "qEntryRot wasn't finite");

	// Get the peds current position and compute the position change from the animated velocity (forcing direction towards the entry point)
	Vec3V vNewPos = pPed->GetTransform().GetPosition();

	//Hack - Do this for all the vehicles that are affected by bugstar://7436812 - Comet7 - Ped clips into the floor / ground while entering the vehicle when competition suspension is applied.
	bool convertibleRoofVehs = m_pVehicle ? MI_CAR_COMET7.IsValid() && m_pVehicle->GetModelIndex() == MI_CAR_COMET7 : false;

	if (convertibleRoofVehs)
	{
		vNewPos.SetZf(vEntryPos.GetZf());
	}

	const Vec3V vToEntryPos = vEntryPos - vNewPos;

	const float fDistToEntryPointSqd = bEnteringStandingTurretSeat ? VEC3V_TO_VECTOR3(vToEntryPos).Mag2() : VEC3V_TO_VECTOR3(vToEntryPos).XYMag2();
	Vec3V vDesiredPosChange = Normalize(vToEntryPos);

	// Handle the singularity case vEntryPos == vNewPos
	if (!IsFiniteAll(vDesiredPosChange))
	{
		vDesiredPosChange = Vec3V(V_ZERO);
	}

	float fAnimatedVel = pPed->GetAnimatedVelocity().Mag();

	// Always clamp it if quite close already
	bool bClampAnimatedVelocityInitial = false;
	s32 iDominantClip = GetMainClipIndex();
	float fPhase = 0.0f;
	if (iDominantClip > -1)
	{
		fPhase = GetActivePhase(iDominantClip);
	}

	if (fDistToEntryPointSqd <= square(ms_Tunables.m_MinDistToAlwaysClampInitialOrientation))
	{
		aiDebugf1("Clamp initial velocity ? %s, dist to entry %.2f", "TRUE", sqrtf(fDistToEntryPointSqd));
		bClampAnimatedVelocityInitial = true;
	}
	else
	{
		// Always clamp it if the direction we're heading in isn't roughly the same as the direction to the entry point
		// to avoid a sideways pop
		Vec3V vPedFwd = pPed->GetTransform().GetB();
		const ScalarV scEntryToPedFwdDot = Dot(vPedFwd, vDesiredPosChange);
		if (IsLessThanAll(scEntryToPedFwdDot, ScalarVFromF32(ms_Tunables.m_MinPedFwdToEntryDotToClampInitialOrientation)))
		{
			if (iDominantClip > -1)
			{
				TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, ANIMATED_VELOCITY_TO_SMOOTH, 3.0f, 0.0f, 15.0f, 0.01f);
				TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, MAX_PHASE_TO_CLAMP_VELOCITY, 0.55f, 0.0f, 5.0f, 0.01f);
				if (fPhase < MAX_PHASE_TO_CLAMP_VELOCITY && fAnimatedVel > ANIMATED_VELOCITY_TO_SMOOTH)
				{
					aiDebugf1("Clamping Animated Velocity Initial (phase : %.2f, vel : %.2f", fPhase, fAnimatedVel);
					bClampAnimatedVelocityInitial = true;
				}
			}
			else
			{
				bClampAnimatedVelocityInitial = true;
			}
		}
		aiDebugf1("Clamp initial velocity ? %s, ped fwd dot entry %.2f", bClampAnimatedVelocityInitial?"TRUE":"FALSE", scEntryToPedFwdDot.Getf());
	}

	TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, MIN_PHASE_TO_FIX_UP_QUICK, 0.0f, 0.0f, 5.0f, 0.01f);
	TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, MIN_PHASE_TO_FIX_UP, 0.3f, 0.0f, 5.0f, 0.01f);
	const bool bCombatEntry = static_cast<CTaskEnterVehicle*>(GetParent())->IsFlagSet(CVehicleEnterExitFlags::CombatEntry);
	const float fMinFixupPhase = (bCombatEntry || bEnteringStandingTurretSeat) ? MIN_PHASE_TO_FIX_UP_QUICK : MIN_PHASE_TO_FIX_UP;
	if (fPhase >= fMinFixupPhase)
	{
		TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, MIN_ANIMATED_VEL_COVER, 2.0f, 0.0f, 5.0f, 0.01f);
		TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, MIN_ANIMATED_VEL, 1.5f, 0.0f, 5.0f, 0.01f);
		const float fMinAnimatedVel = bCombatEntry ? MIN_ANIMATED_VEL_COVER : MIN_ANIMATED_VEL;
		if (fAnimatedVel < fMinAnimatedVel)
		{
			TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, FORCED_ANIMATED_VEL_COVER, 2.5f, 0.0f, 5.0f, 0.01f);
			TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, FORCED_ANIMATED_VEL, 1.5f, 0.0f, 5.0f, 0.01f);
			TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, FORCED_ANIMATED_VEL_FPS, 2.0f, 0.0f, 5.0f, 0.01f);
			const bool bIsFPS = pPed->IsFirstPersonShooterModeEnabledForPlayer(false);
			fAnimatedVel = bCombatEntry ? FORCED_ANIMATED_VEL_COVER : (bIsFPS ? FORCED_ANIMATED_VEL_FPS : FORCED_ANIMATED_VEL);
		}
		aiDebugf1("Fixed Up Animated Velocity %.2f", fAnimatedVel);
	}

	if (bClampAnimatedVelocityInitial)
	{
		TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, MIN_ANIMATED_VEL_INITIAL, 0.0f, 0.0f, 5.0f, 0.01f);
		TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, MIN_MAX_ANIMATED_VEL_INITIAL, 0.05f, 0.0f, 5.0f, 0.01f);
		TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, MAX_MAX_ANIMATED_VEL_INITIAL, 3.0f, 0.0f, 5.0f, 0.01f);

		TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, MAX_SCALE_DIST, 0.6f, 0.0f, 5.0f, 0.01f);
		const float fDistScale = fDistToEntryPointSqd / square(MAX_SCALE_DIST);
		const float fMaxAnimatedVelInitial = fDistScale * MAX_MAX_ANIMATED_VEL_INITIAL + (1.0f - fDistScale) * MIN_MAX_ANIMATED_VEL_INITIAL;

		aiDebugf1("Dist To Entry : %.2f, Dist Scale : %.2f, Max Animated Vel : %.2f", sqrtf(fDistToEntryPointSqd), fDistScale, fMaxAnimatedVelInitial);
		aiDebugf1("Desired Animated Velocity : %.2f", fAnimatedVel);
		fAnimatedVel = rage::Clamp(fAnimatedVel, MIN_ANIMATED_VEL_INITIAL, fMaxAnimatedVelInitial);
		aiDebugf1("Desired Clamped Animated Velocity : %.2f", fAnimatedVel);
	}

	ScalarV_ConstRef scTransDelta = ScalarVFromF32(fAnimatedVel * fTimeStep);
	vDesiredPosChange = Scale(vDesiredPosChange, scTransDelta);

	// Get the peds current orientation and compute the heading change from the desired angular velocity (ignore roll / pitch)
	QuatV qNewAng = QuatVFromMat34V(pPed->GetTransform().GetMatrix());
	taskAssertf(IsFiniteAll(qNewAng), "qNewAng wasn't finite");
	QuatV qDesiredAngChange = QuatVFromAxisAngle(Vec3V(V_Z_AXIS_WONE), ScalarVFromF32(pPed->GetAnimatedAngularVelocity() * fTimeStep));
	taskAssertf(IsFiniteAll(qDesiredAngChange), "qDesiredAngChange wasn't finite");

	if(CTask::GetNumProcessPhysicsSteps() > 1)
	{
		if (!m_bProcessPhysicsCalledThisFrame)
		{
			// Store the deltas since we don't actually update the attachment until after the second physics timestep
			m_qFirstPhysicsOrientationDelta = qDesiredAngChange;
			m_vFirstPhysicsTranslationDelta = vDesiredPosChange;
			m_bProcessPhysicsCalledThisFrame = true;
			return;
		}

		// Add the previous timestep's deltas on
		vDesiredPosChange += m_vFirstPhysicsTranslationDelta;
		qDesiredAngChange = Multiply(qDesiredAngChange,  m_qFirstPhysicsOrientationDelta);
	}

	// Update the transform by adding the changes
	aiDebugf1("Ped position : %.2f, %.2f, %.2f", VEC3V_ARGS(pPed->GetTransform().GetPosition()));
	if (IsLessThanAll(DistSquared(vNewPos, vEntryPos), ScalarVFromF32(ms_Tunables.m_MinSqdDistToSetPos)))
	{
		vNewPos = vEntryPos;
		aiDebugf1("New position 1: %.2f, %.2f, %.2f", VEC3V_ARGS(vNewPos));
	}
	else
	{
		vNewPos += vDesiredPosChange;
		aiDebugf1("New position 2: %.2f, %.2f, %.2f", VEC3V_ARGS(vNewPos));
	}

	qNewAng = Normalize(Multiply(qNewAng, qDesiredAngChange));

	if (m_pVehicle->GetIsTypeVehicle() && static_cast<const CVehicle*>(m_pVehicle.Get())->InheritsFromHeli())
	{
		Vector3 vPedPosition = RCC_VECTOR3(vNewPos);
		const bool bIsInAir = static_cast<const CVehicle*>(m_pVehicle.Get())->IsInAir();
		
		INSTANTIATE_TUNE_GROUP_BOOL(BUG_6862132, bDisableFix);
		AI_LOG_WITH_ARGS("[CTaskEnterVehicle::EnterVehicleAlign] Ped %s m_pVehicle->IsInAir() = %s, State = %s\n", AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetBooleanAsString(bIsInAir), GetStaticStateName(GetState()));
		if (bDisableFix || bIsInAir)
		{
			CTaskEnterVehicle::ProcessHeliPreventGoingThroughGroundLogic(*pPed, *static_cast<const CVehicle*>(m_pVehicle.Get()), vPedPosition, -1.0f, -1.0f);
		}

		vNewPos = RCC_VEC3V(vPedPosition);
	}

	// Update vehicle attachment offsets from new global space position/orientation
	//Displayf("New Pos : %.2f, %.2f, %.2f", vNewPos.GetXf(), vNewPos.GetYf(), vNewPos.GetZf());
	taskAssertf(IsFiniteAll(vNewPos), "vNewPos wasn't finite");
	taskAssertf(IsFiniteAll(qNewAng), "qNewAng wasn't finite");
	CTaskVehicleFSM::UpdatePedVehicleAttachment(pPed, vNewPos, qNewAng);
}

float CTaskEnterVehicleAlign::LerpTowardsFromDirection(float fFrom, float fTo, bool bLeft, float fPerc)
{
	if (Abs(fFrom - fTo) < SMALL_FLOAT)
	{
		return fTo;
	}

	if (fFrom < fTo)
	{
		// No need to wrap round, just lerp straight there
		if (!bLeft)
		{
			return fwAngle::LerpTowards(fFrom, fTo, fPerc);
		}
		else // Need to go past PI round to -PI and approach from the right
		{
			const float fLeftDelta = Abs(- PI - fFrom);
			const float fRightDelta = Abs (PI - fTo);
			float fTotalDelta = fLeftDelta + fRightDelta;
			fTotalDelta *= fPerc;

			if (Abs(fTotalDelta - fLeftDelta) < SMALL_FLOAT)
			{
				return PI;
			}
			else if (fTotalDelta > fLeftDelta)	
			{
				const float fNewVal = PI - (fTotalDelta - fLeftDelta);
				return fNewVal;
			}
			else
			{
				const float fNewVal = fFrom - fTotalDelta;
				return fNewVal;
			}
		}
	}
	else // (fTo < fFrom)
	{
		if (bLeft)
		{
			return fwAngle::LerpTowards(fFrom, fTo, fPerc);
		}
		else
		{
			const float fLeftDelta = Abs(- PI - fTo);
			const float fRightDelta = Abs (PI - fFrom);
			float fTotalDelta = fLeftDelta + fRightDelta;
			fTotalDelta *= fPerc;

			if (Abs(fTotalDelta - fRightDelta) < SMALL_FLOAT)
			{
				return -PI;
			}
			else if (fTotalDelta > fRightDelta)
			{
				const float fNewVal = -PI + (fTotalDelta - fRightDelta);
				return fNewVal;
			}
			else
			{
				const float fNewVal = fFrom + fTotalDelta;
				return fNewVal;
			}
		}
	}
}

void CTaskEnterVehicleAlign::ComputeDesiredVelocity(float fTimeStep)
{
	taskAssertf(m_pVehicle, "NULL vehicle pointer in CTaskEnterVehicleAlign::ComputeDesiredVelocity");

	CPed* pPed = GetPed();

	// Calculate the world space position and orientation we would like to get to during the align
	Vector3 vEntryPos(Vector3::ZeroType);
	Quaternion qEntryOrientation(0.0f,0.0f,0.0f,1.0f);

	CModelSeatInfo::CalculateEntrySituation(m_pVehicle, pPed, vEntryPos, qEntryOrientation, m_iEntryPointIndex, 0, 0.0f, &m_vLocalSpaceOffset);

	// Alter the z coord based on the ground position
	Vector3 vGroundPos(Vector3::ZeroType);
	if (CTaskVehicleFSM::FindGroundPos(m_pVehicle, GetPed(), vEntryPos, 2.5f, vGroundPos))
	{
		// Adjust the reference position
		vEntryPos = vGroundPos;
	}

	// We scale the desired velocity once we first get valid clip, blend weight and phases back from the move network
	// Find the most blended in clip
	s32 iDominantClip = GetMainClipIndex();
	if (iDominantClip > -1)
	{
		// Cache the peds current position
		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

		// Compute the flat xy vector to the entry position, store the absolute magnitude before normalising
		Vector3 vToTarget = vEntryPos - vPedPosition;
		vToTarget.z = 0.0f;
		const float fDistToTarget = vToTarget.Mag();

		// Whilst the network is blending in, we won't get the full mover translation from the aligns, so get an estimate for 
		// the velocity to apply if we can't scale the existing desired velocity
		if (m_fAverageSpeedDuringBlendIn < 0.0f)
		{
			const float fBlendInDuration = static_cast<CTaskEnterVehicle*>(GetParent())->GetNetworkBlendInDuration();
			const crClip* pMainClip = GetActiveClip(iDominantClip);
			if (!pMainClip)
				return;

			FindMoverFixUpPhases(pMainClip);

			const float fMainDuration = pMainClip->GetDuration();
			const float fBlendInMainPhase = fBlendInDuration / fMainDuration;
			ALIGN_DEBUG_ONLY(Displayf("Main Blend In Completed Phase : %.2f", fBlendInMainPhase));
			// Get the translation change in the main clip up to the phase at which we should be fully blended in
			Vector3 vClipTotalDist(fwAnimHelpers::GetMoverTrackTranslationDiff(*pMainClip, 0.f, fBlendInMainPhase));
			ALIGN_DEBUG_ONLY(Displayf("Main clip total blend in dist : %.2f", vClipTotalDist.Mag()));
			m_fAverageSpeedDuringBlendIn = vClipTotalDist.Mag() / fBlendInDuration;
			ALIGN_DEBUG_ONLY(Displayf("Average blend in speed : %.2f", m_fAverageSpeedDuringBlendIn));
		}

#if __BANK
		static bool DEBUG_MOTION_SCALING = false;
		static float REMAINING_TIME = 0.03f;
		const crClip* pClip = GetActiveClip(iDominantClip);
		if (pClip)
		{
			const float fMainDuration = pClip->GetDuration();
			const float fMainPhase = GetActivePhase(iDominantClip);
			const float fRemainingTime = fMainDuration * (1.0f - fMainPhase);
			if (DEBUG_MOTION_SCALING && fRemainingTime < REMAINING_TIME)
			{
				Displayf("Main duration : %.2f, Main phase : %.2f, Remaining Time : %.2f", fMainDuration, fMainPhase, fRemainingTime);
				static float XTOL = 0.05f;
				if (fDistToTarget > XTOL)
				{
					taskAssertf(0, "Failed to scale translation to nail target to within %.2f m, dist to entry %.2f m", XTOL, fDistToTarget);
				}
				static float RTOL = 0.25f;
				Vector3 vEulers(Vector3::ZeroType);
				qEntryOrientation.ToEulers(vEulers);
				const float fHeadingDelta = SubtractAngleShorter(vEulers.z, pPed->GetCurrentHeading());
				if (Abs(fHeadingDelta) > RTOL)
				{
					taskAssertf(0, "Failed to scale rotation to nail target to within %.2f rad, angle to entry %.2f rad", RTOL, fHeadingDelta);
				}
			}
		}
#endif // __BANK

		ALIGN_DEBUG_ONLY(Displayf("To target dist : %.2f", fDistToTarget));
		vToTarget.Normalize();

		// Cache the peds current heading
		const float fCurrentHeading =  pPed->GetMotionData()->GetCurrentHeading();

		// Calculate the world space heading we're aiming to get to
		Vector3 vEntryEulers(Vector3::ZeroType);
		qEntryOrientation.ToEulers(vEntryEulers);
		float fAttachHeading = fwAngle::LimitRadianAngle(vEntryEulers.z);

		// Work out which direction we should be rotating during the align
		bool bLeftApproach = ShouldApproachFromLeft();

		//const float fInitialHeadingDelta = bLeftApproach ? CClipHelper::ComputeHeadingDeltaFromLeft(m_fInitialHeading, fAttachHeading) : CTaskAimGunFromCoverIntro::ComputeHeadingDeltaFromRight(m_fInitialHeading, fAttachHeading);
		//Displayf("Initial Heading Delta : %.2f", fInitialHeadingDelta);
		
		// Compute how much we'd need to turn based on the approach direction
		const float fCurrentHeadingDelta = bLeftApproach ? CClipHelper::ComputeHeadingDeltaFromLeft(fCurrentHeading, fAttachHeading) : CClipHelper::ComputeHeadingDeltaFromRight(fCurrentHeading, fAttachHeading);
		ALIGN_DEBUG_ONLY(Displayf("Current Heading Delta : %.2f", fCurrentHeadingDelta));
		
		// Cached the weights and floats
		const s32 iMaxClips = m_bStartedStandAlign ? ms_iNumStandAlignClips : ms_iNumMoveAlignClips;

		float afCachedWeights[ms_iNumStandAlignClips];
		float afCachedPhases[ms_iNumStandAlignClips];
		
		float fTotalWeight = 0.0f;

		for (s32 i=0; i<iMaxClips; i++)
		{
			afCachedWeights[i] = GetActiveWeight(i);
			fTotalWeight += afCachedWeights[i];
			afCachedPhases[i] = GetActivePhase(i);
		}
		
		// Renormalize blend weights
		if (fTotalWeight != 1.0f && taskVerifyf(fTotalWeight > 0.0f, "Total Blend Weight (%.2f) Wasn't Valid", fTotalWeight))
		{
			for (s32 i=0; i<iMaxClips; i++)
			{
				afCachedWeights[i] /= fTotalWeight;
				//Displayf("New Weight %.4f", afCachedWeights[i]);
			}
		}	
		
		Vector3 vCombinedClipTotalDist(Vector3::ZeroType);
		float   fCombinedClipTotalHeading = 0.0f;
		float	fCombinedClipTotalDist = 0.0f;

		// Calculate how much the current blend of clips will move and rotate to from the current phase to the end
		for (s32 i=0; i<iMaxClips; i++)
		{
			const crClip* pClip = GetActiveClip(i);
			if(taskVerifyf(pClip, "Couldn't find clip (%d)", i))
			{
				// Factor this into the clip total dist
				float fBlendWeight = afCachedWeights[i];

				// Get the remaining translation change left in the clip
				Vector3 vClipTotalDist(fwAnimHelpers::GetMoverTrackTranslationDiff(*pClip, afCachedPhases[i]+m_fFirstPhysicsTimeStep, 1.f));

				// Factor this into the clip total dist
				vCombinedClipTotalDist += vClipTotalDist * fBlendWeight;

				// Get the remaining orientation change left in the clip
				//Quaternion qClipTotalRot = fwAnimHelpers::GetMoverTrackRotationDiff(*pClip, afCachedPhases[i]+m_fFirstPhysicsTimeStep, 1.f);

				Quaternion qCurrentClipRotation = fwAnimHelpers::GetMoverTrackRotation(*pClip, afCachedPhases[i]+m_fFirstPhysicsTimeStep);
				Quaternion qEndClipRotation = fwAnimHelpers::GetMoverTrackRotation(*pClip, 1.f);

				//Displayf("Clip %i / Start Heading %.4f", i, qCurrentClipRotation.TwistAngle(ZAXIS));
				//Displayf("Clip %i / End Heading %.4f", i, qEndClipRotation.TwistAngle(ZAXIS));

				const float fHeadingChange = qEndClipRotation.TwistAngle(ZAXIS) - qCurrentClipRotation.TwistAngle(ZAXIS);
				//Displayf("Clip %i / Heading Change (Long) %.4f", i, fHeadingChange);

				fCombinedClipTotalHeading += fHeadingChange * fBlendWeight;
			}
		}

		fCombinedClipTotalDist = vCombinedClipTotalDist.Mag();

		ALIGN_DEBUG_ONLY(Displayf("Remaining Dist : %.2f", fCombinedClipTotalDist));
		ALIGN_DEBUG_ONLY(Displayf("Remaining Heading : %.2f", fCombinedClipTotalHeading));
	
		// Get the angular velocity that is about to be applied from the clips
		float fDesiredAngularVelocity = pPed->GetDesiredAngularVelocity().z;
		// Force it to go in the direction we want it to
		const float fDirection = bLeftApproach ? -1.0f : 1.0f;
		// Clamp so it doesn't overshoot
		float fRotationalVelocityZ = fDirection * Min(Abs(fDesiredAngularVelocity), Abs(fCurrentHeadingDelta/fTimeStep));

		///////////////////////////////////////////////////////////////////
		// Apply rotation scaling to make the ped hit the desired angle
		///////////////////////////////////////////////////////////////////

// 		float fMinPhaseToStartRotScale = ms_Tunables.m_StdVehicleMinPhaseToStartRotFixup;
// 
// 		if (m_pVehicle->InheritsFromBike())
// 		{
// 			fMinPhaseToStartRotScale = ms_Tunables.m_BikeVehicleMinPhaseToStartRotFixup;
// 		}

		if (ms_Tunables.m_ApplyRotationScaling && GetActivePhase(m_iMainClipIndex) >= m_fRotationFixUpStartPhase)
		{
			// Compute the closest heading delta to the target
			const float fLeftDelta = Abs(CClipHelper::ComputeHeadingDeltaFromLeft(fCurrentHeading, fAttachHeading));
			const float fRightDelta = Abs(CClipHelper::ComputeHeadingDeltaFromRight(fCurrentHeading, fAttachHeading));
			const float fSmallHeadingDelta = Min(fLeftDelta, fRightDelta);

			if (fSmallHeadingDelta < ms_Tunables.m_HeadingReachedTolerance)
			{
				m_bHeadingReached = true;
			}

			// If we avoid a zero divide, scale the desired velocity by scaling by the current delta / remaining clip delta
			if (Abs(fCombinedClipTotalHeading) > SMALL_FLOAT)
			{
				const float fMaxRotScale = ms_Tunables.m_MaxRotationalSpeedScale * ms_Tunables.m_DefaultAlignRate;
				const float fScaleFactor = Clamp(Abs(fCurrentHeadingDelta / fCombinedClipTotalHeading), ms_Tunables.m_MinRotationalSpeedScale, fMaxRotScale);
				ALIGN_DEBUG_ONLY(Displayf("Rot Scale : %.2f", fScaleFactor));
				
				fRotationalVelocityZ *= fScaleFactor;
				fRotationalVelocityZ = fDirection * Abs(fRotationalVelocityZ);
			}
			else
			{
				// Otherwise just apply a velocity towards the target
				const float fMaxRotVel = ms_Tunables.m_MaxRotationalSpeed * ms_Tunables.m_DefaultAlignRate;
				ALIGN_DEBUG_ONLY(Displayf("Rot Velocity : %.2f", fMaxRotVel));

				fRotationalVelocityZ = Min(fMaxRotVel, Abs(fSmallHeadingDelta)/fTimeStep);
				if (fLeftDelta < fRightDelta)
				{
					fRotationalVelocityZ *= -1.0f;
				}
			}
		}

		if (!ms_Tunables.m_DisableRotationOvershootCheck)
		{
			bool bOverShot = false;

			// See if we'll overshoot if we apply this rotation and clamp it if needed
			const float fPredictedNewHeading = fwAngle::LimitRadianAngle(fCurrentHeading + fRotationalVelocityZ * fTimeStep);
			const float fPredictedHeadingDelta = bLeftApproach ? CClipHelper::ComputeHeadingDeltaFromLeft(fPredictedNewHeading, fAttachHeading) : CClipHelper::ComputeHeadingDeltaFromRight(fPredictedNewHeading, fAttachHeading);

			if (Abs(fPredictedHeadingDelta) > Abs(fCurrentHeadingDelta))
			{
				bOverShot = true;
			}

			if (bOverShot)
			{
				const float fSmallHeadingDelta = Min(Abs(fwAngle::LimitRadianAngle(fAttachHeading - pPed->GetCurrentHeading())),Abs(fwAngle::LimitRadianAngle(pPed->GetCurrentHeading() - fAttachHeading)));
				fRotationalVelocityZ = fSmallHeadingDelta/fTimeStep;
				ALIGN_DEBUG_ONLY(Displayf("Overshot Rot Velocity : %.2f", fRotationalVelocityZ));
			}

			if (!m_bHeadingReached)
			{
				// Apply
				Vector3 vDesiredAngularVelocity(0.0f, 0.0f, fRotationalVelocityZ);
				pPed->SetDesiredAngularVelocity(vDesiredAngularVelocity);
			}
			else
			{
				pPed->SetAngVelocity(VEC3_ZERO);
				pPed->SetDesiredAngularVelocity(VEC3_ZERO);
			}
		}

		///////////////////////////////////////////////////////////////////
		// Apply translation scaling to make the ped hit the desired angle
		///////////////////////////////////////////////////////////////////
		float fTranslationalVelocity = pPed->GetDesiredVelocity().Mag();

		if (ms_Tunables.m_ApplyTranslationScaling && GetActivePhase(m_iMainClipIndex) >= m_fTranslationFixUpStartPhase)
		{
			//const bool bTaskNetworkFullyBlended = pPed->GetMovePed().IsTaskNetworkFullyBlended();

			// Restrict the translational scaling to when the task network is fully blended or we're stand aligning
			//if (bTaskNetworkFullyBlended || m_bStartedStandAlign)
			{
				static float MIN_CLIP_DIST_TO_APPLY_SCALING = 0.1f;
				// Only scale the clip if we have something worth scaling
				if (fCombinedClipTotalDist > MIN_CLIP_DIST_TO_APPLY_SCALING)
				{
					const float fMaxTransScale = ms_Tunables.m_MaxTranslationalScale * ms_Tunables.m_DefaultAlignRate;
					const float fScaleFactor = Clamp(fDistToTarget / fCombinedClipTotalDist, ms_Tunables.m_MinTranslationalScale, fMaxTransScale);

					ALIGN_DEBUG_ONLY(Displayf("Trans Scale : %.2f", fScaleFactor));
					fTranslationalVelocity *= fScaleFactor;
				}
				else
				{
					// Cap the maximum velocity we apply in case the vehicle is moving
					if (m_bStartedStandAlign)
					{
						const float fMaxTransVelocity = ms_Tunables.m_MaxTranslationalStandSpeed * ms_Tunables.m_DefaultAlignRate;
						fTranslationalVelocity = Min(fMaxTransVelocity, Abs(fDistToTarget/fTimeStep));
					}
					else
					{
						const float fMaxTransVelocity = ms_Tunables.m_MaxTranslationalMoveSpeed * ms_Tunables.m_DefaultAlignRate;
						fTranslationalVelocity = Min(fMaxTransVelocity, Abs(fDistToTarget/fTimeStep));
					}

					ALIGN_DEBUG_ONLY(Displayf("Trans Vel : %.2f", fTranslationalVelocity));
				}
			}
// 			else // Whilst blending in using walk/run aligns, just use the average speed computed from the main clip
// 			{
// 				fTranslationalVelocity = Min(m_fAverageSpeedDuringBlendIn, Abs(fDistToTarget/fTimeStep));
// 				ALIGN_DEBUG_ONLY(Displayf("1 Pre Trans Vel : %.2f", fTranslationalVelocity));
// 			}
		}

		if (!ms_Tunables.m_DisableTranslationOvershootCheck)
		{
			// Initialise our desired velocity
			Vector3 vDesiredVel(Vector3::ZeroType);

			// Compute the initial direction to the entrypoint
			Vector3 vOldToTarget = vEntryPos - m_vInitialPosition;
			vOldToTarget.z = 0.0f;

			// If the current to target is opposite from the initial we must have overshot the target position,
			// so scale it to go to the entry position
			if (vToTarget.Dot(vOldToTarget) < 0.0f)
			{
				vDesiredVel = vToTarget;
				vDesiredVel.Scale(fDistToTarget/fTimeStep);
			}
			else
			{
				vDesiredVel = vToTarget;
				vDesiredVel.Scale(fTranslationalVelocity);
			}

#if __ASSERT
			if (vDesiredVel.Mag() > 100.0f)
			{
				taskAssertf(0, "Computed Large Desired Velocity : Dist To Target : %.4f, Timestep : %.4f, TransVelocity : %.4f", fDistToTarget, fTimeStep, fTranslationalVelocity);
			}
#endif

			ALIGN_DEBUG_ONLY(Displayf("Desired velocity : %.2f", vDesiredVel.Mag()));
			pPed->SetDesiredVelocityClamped(vDesiredVel, CTaskMotionBase::ms_fAccelLimitHigher*fTimeStep);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicleAlign::FindMoverFixUpPhases(const crClip* pClip)
{
	if (!pClip)
		return;

	if (m_fTranslationFixUpStartPhase < 0.0f)
	{
		if (!CClipEventTags::FindMoverFixUpStartEndPhases(pClip, m_fTranslationFixUpStartPhase, m_fTranslationFixUpEndPhase, true))
		{
			m_fTranslationFixUpStartPhase = 0.0f;
			m_fTranslationFixUpEndPhase = 0.8f;
		}
	}

	if (m_fRotationFixUpStartPhase < 0.0f)
	{
		if (!CClipEventTags::FindMoverFixUpStartEndPhases(pClip, m_fRotationFixUpStartPhase, m_fRotationFixUpEndPhase, false))
		{
			m_fRotationFixUpStartPhase = 0.8f;
			m_fRotationFixUpEndPhase = 1.0f;
		}
	}
};

////////////////////////////////////////////////////////////////////////////////

const crClip* CTaskEnterVehicleAlign::GetActiveClip(s32 iIndex) const
{
	switch(GetState())
	{
	case State_OrientatedAlignment:
		{
			if (m_bStartedStandAlign)
			{
				switch(iIndex)
				{
				case 0:
					return m_MoveNetworkHelper.GetClip(ms_StandAlign180RClipId);
				case 1:
					return m_MoveNetworkHelper.GetClip(ms_StandAlign90RClipId);
				case 2: 
					return m_MoveNetworkHelper.GetClip(ms_StandAlign0ClipId);
				case 3:
					return m_MoveNetworkHelper.GetClip(ms_StandAlign90LClipId);
				case 4: 
					return m_MoveNetworkHelper.GetClip(ms_StandAlign180LClipId);
				default:
					taskAssertf(0, "iIndex [%d] not handled", iIndex);
					break;
				}
			}
			else
			{
				switch(iIndex)
				{
				case 0:
					return m_MoveNetworkHelper.GetClip(ms_AlignFrontClipId);
				case 1:
					return m_MoveNetworkHelper.GetClip(ms_AlignSideClipId);
				case 2:
					return m_MoveNetworkHelper.GetClip(ms_AlignRearClipId);
				default:
					taskAssertf(0, "iIndex [%d] not handled", iIndex);
					break;
				}
			}
		}
	default:
		break;
	}

	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

float CTaskEnterVehicleAlign::GetActivePhase(s32 iIndex) const
{
	switch(GetState())
	{
	case State_OrientatedAlignment:
		{
			if (m_bStartedStandAlign)
			{
				switch(iIndex)
				{
				case 0:
					return Clamp(m_MoveNetworkHelper.GetFloat(ms_StandAlign180RPhaseId), 0.0f, 1.0f);
				case 1:
					return Clamp(m_MoveNetworkHelper.GetFloat(ms_StandAlign90RPhaseId), 0.0f, 1.0f);
				case 2: 
					return Clamp(m_MoveNetworkHelper.GetFloat(ms_StandAlign0PhaseId), 0.0f, 1.0f);
				case 3:
					return Clamp(m_MoveNetworkHelper.GetFloat(ms_StandAlign90LPhaseId), 0.0f, 1.0f);
				case 4: 
					return Clamp(m_MoveNetworkHelper.GetFloat(ms_StandAlign180LPhaseId), 0.0f, 1.0f);
				default:
					taskAssertf(0, "iIndex [%d] not handled", iIndex);
					break;
				}
			}
			else
			{
				switch(iIndex)
				{
				case 0:
					return Clamp(m_MoveNetworkHelper.GetFloat(ms_AlignFrontPhaseId), 0.0f, 1.0f);
				case 1:
					return Clamp(m_MoveNetworkHelper.GetFloat(ms_AlignSidePhaseId), 0.0f, 1.0f);
				case 2: 
					return Clamp(m_MoveNetworkHelper.GetFloat(ms_AlignRearPhaseId), 0.0f, 1.0f);
				default:
					taskAssertf(0, "iIndex [%d] not handled", iIndex);
					break;
				}
			}
		}
	default:
		break;
	}

	return 0.0f;
}

////////////////////////////////////////////////////////////////////////////////

float CTaskEnterVehicleAlign::GetActiveWeight(s32 iIndex) const
{
	switch(GetState())
	{
	case State_OrientatedAlignment:
		{
			if (m_bStartedStandAlign)
			{
				switch (iIndex)
				{
				case 0:
					return Clamp(m_MoveNetworkHelper.GetFloat(ms_StandAlign180RBlendWeightId), 0.0f, 1.0f);
				case 1:
					return Clamp(m_MoveNetworkHelper.GetFloat(ms_StandAlign90RBlendWeightId), 0.0f, 1.0f);
				case 2:
					return Clamp(m_MoveNetworkHelper.GetFloat(ms_StandAlign0BlendWeightId), 0.0f, 1.0f);
				case 3:
					return Clamp(m_MoveNetworkHelper.GetFloat(ms_StandAlign90LBlendWeightId), 0.0f, 1.0f);
				case 4:
					return Clamp(m_MoveNetworkHelper.GetFloat(ms_StandAlign180LBlendWeightId), 0.0f, 1.0f);
				default:
					taskAssertf(0, "iIndex [%d] not handled", iIndex);
					break;
				}
			}
			else
			{
				switch (iIndex)
				{
				case 0:
					return Clamp(m_MoveNetworkHelper.GetFloat(ms_AlignFrontBlendWeightId), 0.0f, 1.0f);
				case 1:
					return Clamp(m_MoveNetworkHelper.GetFloat(ms_AlignSideBlendWeightId), 0.0f, 1.0f);
				case 2:
					return Clamp(m_MoveNetworkHelper.GetFloat(ms_AlignRearBlendWeightId), 0.0f, 1.0f);
				default:
					taskAssertf(0, "iIndex [%d] not handled", iIndex);
					break;
				}
			}	
		}
	default:
		return 0.0f;
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicleAlign::UpdateMoVE()
{
	if (m_MoveNetworkHelper.IsNetworkActive())
	{
		ComputeAlignAngleSignal();

		m_MoveNetworkHelper.SetFloat(ms_AlignAngleId, m_fAlignAngleSignal);

		SetClipPlaybackRate();
	}
}


//////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicleAlign::ComputeAlignAngleSignal()
{
	// Compute the angle between the ped and the entry point as a move parameter between 0 (front) to 1 (rear)
	if (m_fAlignAngleSignal < 0.0f)
	{
		// Calculate the world space position and orientation we would like to nail to during the align
		Vector3 vEntryPos(Vector3::ZeroType);
		Quaternion qEntryOrientation(0.0f,0.0f,0.0f,1.0f);
		ComputeAlignTarget(vEntryPos, qEntryOrientation);

		// The stand align angle signal is computed slightly differently
		if (m_bStartedStandAlign)
		{
			Vector3 vEulers;
			qEntryOrientation.ToEulers(vEulers, "xyz");
			float fEntryHeading = fwAngle::LimitRadianAngle(vEulers.z);
			Vec3V vEntryForward(V_Y_AXIS_WZERO);
			const CVehicleEntryPointInfo* pEntryPointInfo = GetEntryInfo(m_iEntryPointIndex); 
			const bool bIsLeftSide = pEntryPointInfo->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT;
			const bool bIsRearSide = pEntryPointInfo->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_REAR;
			const Vec3V vPedForward = GetPed()->GetTransform().GetB();

			vEntryForward = RotateAboutZAxis(vEntryForward, INTRIN_TO_SCALARV(fEntryHeading));
			// Compute magnitude of angle 0 to PI
			float fAngleMag = Dot(vEntryForward, vPedForward).Getf();
			m_fAlignAngleSignal = AcosfSafe(fAngleMag);

			//Displayf("Align Angle Mag: %.4f", m_fAlignAngleSignal);

			// Now this is between 0.0-0.5
			m_fAlignAngleSignal /= TWO_PI;

			//Displayf("Align Angle Norm: %.4f", m_fAlignAngleSignal);

			float fSign = Sign(Cross(vEntryForward, vPedForward).GetZf());

			//Displayf("Align Angle Sign: %.4f", fSign);

			// A signal of 0.5 results in a 100% blend in the stand align 0 clip, 
			// so we can add/subtract up to 0.5 off from this to get the other angled blends
			m_fAlignAngleSignal = 0.5f + fSign * m_fAlignAngleSignal;

			//Displayf("Align Angle Signal Final: %.4f", m_fAlignAngleSignal);		

			if (!bIsRearSide)
			{
				if (Sign(Cross(vEntryForward, vPedForward).GetZf()) < 0.0f)
				{
					m_bApproachFromLeft = bIsLeftSide ? true : false;
				}
				else
				{
					m_bApproachFromLeft = bIsLeftSide ? false : true;
				}	
			}
			else
				m_bApproachFromLeft = false;		
		}
		else
		{
			// Get ped and entry positions
			Vec3V vPedPos = GetPed()->GetTransform().GetPosition();
			Vec3V vEntPos = VECTOR3_TO_VEC3V(vEntryPos);

			if (IsCloseAll(vPedPos, vEntPos, Vec3V(V_FLT_SMALL_3)))
			{
				vPedPos -= GetPed()->GetTransform().GetB();
			}

			Vec3V vToTargetPos = Normalize(vEntPos - vPedPos);
			Vec3V vVehForward = m_pVehicle->GetTransform().GetForward();

			// Compute magnitude of angle 0 to PI
			float fAngleMag = Dot(vToTargetPos, vVehForward).Getf();
			m_fAlignAngleSignal = AcosfSafe(fAngleMag);

			//Displayf("Align Angle Mag: %.4f", m_fAlignAngleSignal);

			// Normalise to range 0.0-1.0 for the final move parameter
			m_fAlignAngleSignal /= PI;

			Vector3 vEulers;
			qEntryOrientation.ToEulers(vEulers, "xyz");
			float fEntryHeading = fwAngle::LimitRadianAngle(vEulers.z);
			const float fPedHeading = GetPed()->GetCurrentHeading();

			m_bApproachFromLeft = fPedHeading - fEntryHeading > 0.0f ? true : false;
		}

		taskAssertf(m_fAlignAngleSignal >=0.0f && m_fAlignAngleSignal <= 1.0f, "Align angle was not normalised correctly");

		//Displayf("Align Angle Signal : %.4f", m_fAlignAngleSignal);
	}
}

////////////////////////////////////////////////////////////////////////////////

s32 CTaskEnterVehicleAlign::GetMainClipIndex()
{
	// If the clip isn't negative we've already computed the dominant clip, so just return the stored index
	if (m_iMainClipIndex != -1)
	{
		return m_iMainClipIndex;
	}

	m_iMainClipIndex = FindMostBlendedClip(m_bStartedStandAlign ?  ms_iNumStandAlignClips : ms_iNumMoveAlignClips);
	return m_iMainClipIndex;
}

////////////////////////////////////////////////////////////////////////////////

s32 CTaskEnterVehicleAlign::FindMostBlendedClip(s32 iNumClips) const
{
	float fBestWeight = -1.0f;
	s32 iBestClip = -1;

	for (s32 iCurrentClip = 0; iCurrentClip<iNumClips; iCurrentClip++)
	{
		float fCurrentWeight = GetActiveWeight(iCurrentClip);
		if (fCurrentWeight > 0.0f && fCurrentWeight > fBestWeight)
		{
			iBestClip = iCurrentClip;
			fBestWeight = fCurrentWeight;
		}
	}
	return iBestClip;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicleAlign::ShouldApproachFromLeft()
{
	// Determine which direction to orientate the ped
	s32 iDominantClip = GetMainClipIndex();
	taskAssert(iDominantClip != -1);

	if (m_bStartedStandAlign)
	{		
		const CVehicleEntryPointInfo* pEntryInfo = GetEntryInfo(m_iEntryPointIndex); 
		taskAssertf(pEntryInfo, "Missing entry info for %s, index %u", m_pVehicle->GetModelName(), m_iEntryPointIndex);
		const bool bRearSide = pEntryInfo->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_REAR;
		const bool bLeftSide = !bRearSide && pEntryInfo->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT ? true : false;
		const s32 iCloseClip = bLeftSide ? eStandAlign90L : eStandAlign90R;
		if (iDominantClip == iCloseClip || bRearSide)
		{
			if (m_bComputedApproach)
			{
				return m_bApproachFromLeft;
			}
	
			m_bApproachFromLeft = IsShortestAlignDirectionLeft();
			m_bComputedApproach = true;
			return m_bApproachFromLeft;
		}

		const s32 iFarClip = bLeftSide ? eStandAlign180L : eStandAlign180R;
		if (iDominantClip == iFarClip)
		{
			return bLeftSide ? true : false;
		}

		return bLeftSide ? false : true;
	}
	else
	{
		if (iDominantClip == eMoveAlignFront)
		{
			const CVehicleEntryPointInfo* pEntryInfo = GetEntryInfo(m_iEntryPointIndex); 			
			taskAssertf(pEntryInfo, "Missing entry info for %s, index %u", m_pVehicle->GetModelName(), m_iEntryPointIndex);
			return (pEntryInfo->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT) ? false : true;
		}

		if (m_bComputedApproach)
		{
			return m_bApproachFromLeft;
		}

		m_bApproachFromLeft = IsShortestAlignDirectionLeft();
		m_bComputedApproach = true;
		return m_bApproachFromLeft;
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicleAlign::ComputeRemainingMoverChanges(s32 iNumClips, Vector3& vClipTotalDist, float UNUSED_PARAM(fRotOffset), float fStartScalePhase)
{
	const crClip* pMainClip = GetActiveClip(m_iMainClipIndex);

	if (pMainClip)
	{
		const CPed* pPed = GetPed();

		// Get the distance the clip moves over its duration
		vClipTotalDist = fwAnimHelpers::GetMoverTrackTranslationDiff(*pMainClip, fStartScalePhase, 1.f);
		vClipTotalDist = VEC3V_TO_VECTOR3(pPed->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vClipTotalDist)));

		// Check for a second blended clip
		for (s32 iCurrentClip = 0; iCurrentClip<iNumClips; iCurrentClip++)
		{
			if (iCurrentClip != m_iMainClipIndex)
			{
				// Cache the weight, phase and clip
				const float fCurrentWeight = GetActiveWeight(iCurrentClip);
				const crClip* pCurrentClip = GetActiveClip(iCurrentClip);

				if (fCurrentWeight > 0.0f && taskVerifyf(pCurrentClip, "NULL clip"))
				{
					// Get the distance the clip moves over its duration
					Vector3 vSecondClipTotalDist(fwAnimHelpers::GetMoverTrackTranslationDiff(*pCurrentClip, fStartScalePhase, 1.f));
					vSecondClipTotalDist = VEC3V_TO_VECTOR3(pPed->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vSecondClipTotalDist)));

					// Factor this into the clip total dist
					float fBlendWeight = GetActiveWeight();
					vClipTotalDist = vClipTotalDist * (1.0f-fBlendWeight) + vSecondClipTotalDist * fBlendWeight;
					break;
				}
			}
		}
	}

//	for (s32 iCurrentClip = 0; iCurrentClip<iNumClips; iCurrentClip++)
// 	{
// 		// Cache the weight, phase and clip
// 		const float fCurrentWeight = GetActiveWeight(iCurrentClip);
// 		const float fCurrentPhase = GetActivePhase(iCurrentClip);
// 		const crClip* pCurrentClip = GetActiveClip(iCurrentClip);
// 	
// 		if (fCurrentWeight > 0.0f && taskVerifyf(pCurrentClip, "NULL clip"))
// 		{
// 			taskAssertf(fCurrentPhase >= 0.0f, "Expected phase to be non negative at this point, missing output parameter in network?");
// 
// 			// Combine the mover changes linearly using the blend weights
// 			Vector3 vClipMoverTransChange(Vector3::ZeroType);
// 			vClipMoverTransChange = fwAnimDirector::GetMoverTrackPositionChange(pCurrentClip, fCurrentPhase, 1.0f);
// 			vClipMoverTransChange = VEC3V_TO_VECTOR3(pPed->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vClipMoverTransChange)));
// 			vClipMoverTransChange.Scale(fCurrentWeight);
// 			vTransOffset += vClipMoverTransChange;
// 
// 			const Quaternion qClipMoverRotChange = fwAnimDirector::GetMoverTrackOrientationChange(pCurrentClip, fCurrentPhase, 1.0f);
// 			Vector3 vRotChangeEulers(Vector3::ZeroType);
// 			qClipMoverRotChange.ToEulers(vRotChangeEulers);
// 			fRotOffset += fwAngle::LimitRadianAngle(vRotChangeEulers.z) * fCurrentWeight;
// 		}
// 	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicleAlign::HaveAlignClipsFinished() const
{
	if (m_iMainClipIndex > -1)
	{
		if (!m_bStartedStandAlign)
		{
			switch (m_iMainClipIndex)
			{
				case eMoveAlignFront: return m_MoveNetworkHelper.GetBoolean(ms_FrontAlignClipFinishedId);
				case eMoveAlignSide: return m_MoveNetworkHelper.GetBoolean(ms_SideAlignClipFinishedId);
				case eMoveAlignRear: return m_MoveNetworkHelper.GetBoolean(ms_RearAlignClipFinishedId);
				default: break;
			}	
		}

		// 1.0f means the alignment has finished and we should be at the attach position and orientated correctly
		float fAlignPhase = GetActivePhase(m_iMainClipIndex);

		if (fAlignPhase == 1.0f)
		{
			if (ShouldWaitForAllAlignAnimsToFinish())
			{
				return AreAllWeightedAnimsFinished();
			}
			return true;
		}
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicleAlign::ShouldWaitForAllAlignAnimsToFinish() const
{
	if (!m_bStartedStandAlign)
		return false;

	if (!m_pVehicle->GetIsTypeVehicle())
		return false;

	const CTaskEnterVehicle* pParentEnterVehicleTask = (GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_ENTER_VEHICLE) ? static_cast<const CTaskEnterVehicle*>(GetParent()) : NULL;
	const bool bEnterOnVehicle = pParentEnterVehicleTask ? pParentEnterVehicleTask->IsOnVehicleEntry() : false;
	if (!bEnterOnVehicle)
		return false;

	const CVehicle& rVeh = *static_cast<const CVehicle*>(m_pVehicle.Get());
	if (rVeh.InheritsFromPlane())
	{
		return true;
	}

	if (rVeh.GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_TURRET_SEAT_ON_VEHICLE))
	{
		if (rVeh.IsEntryIndexValid(m_iEntryPointIndex))
		{
			const CEntryExitPoint* pEntryPoint = rVeh.GetEntryExitPoint(m_iEntryPointIndex);
			const s32 iSeatIndex = pEntryPoint->GetSeat(SA_directAccessSeat);
			if (pEntryPoint && rVeh.IsTurretSeat(iSeatIndex))
			{
				return true;
			}
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicleAlign::AreAllWeightedAnimsFinished() const
{
	if (m_fAlignAngleSignal <= 0.25f)
	{
		return GetActivePhase(eStandAlign180R) == 1.0f && GetActivePhase(eStandAlign90R) == 1.0f;
	}
	else if (m_fAlignAngleSignal <= 0.5f)
	{
		return (GetActivePhase(eStandAlign90R) == 1.0f) && GetActivePhase(eStandAlign0) == 1.0f;
	}
	else if (m_fAlignAngleSignal <= 0.75f)
	{
		return (GetActivePhase(eStandAlign0) == 1.0f) && GetActivePhase(eStandAlign90L) == 1.0f;
	}
	else
	{
		return GetActivePhase(eStandAlign90L) == 1.0f && GetActivePhase(eStandAlign180L) == 1.0f;
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicleAlign::SetInitialSignals()
{
	CPed* pPed = GetPed();
	TUNE_GROUP_BOOL(ENTER_VEHICLE_TUNE, ENABLE_RUN_ALIGNS, false);
	if (ENABLE_RUN_ALIGNS)
		m_bRun = (pPed->GetMotionData()->GetIsRunning() || pPed->GetMotionData()->GetIsSprinting()) ? true : false;
	else
		m_bRun = false;

	m_bStartedStandAlign = ShouldDoStandAlign(*m_pVehicle, *pPed, m_iEntryPointIndex, &m_vLocalSpaceOffset);
	m_MoveNetworkHelper.SetFlag(m_bRun, ms_RunFlagId);
	m_MoveNetworkHelper.SetFlag(m_bStartedStandAlign, ms_StandFlagId);

	CTaskMotionBase* pTask = pPed->GetCurrentMotionTask();
	if (pTask && pTask->GetTaskType() == CTaskTypes::TASK_HUMAN_LOCOMOTION)
	{
		bool bUseLeftFootTransition = static_cast<CTaskHumanLocomotion*>(pTask)->UseLeftFootTransition();
		
		if (ms_Tunables.m_ReverseLeftFootAlignAnims)
		{
			// Reverse as anims seem to be the wrong way round
			bUseLeftFootTransition = !bUseLeftFootTransition;
		}

		if (bUseLeftFootTransition)
		{
			m_MoveNetworkHelper.SetFlag(true, ms_LeftFootFirstFlagId); 
		}
	}

	if (m_pVehicle->GetIsTypeVehicle() && CTaskEnterVehicle::ms_Tunables.m_EnableNewBikeEntry)
	{
		s32 iUnused = 0;
		const bool bBikeOnGround = static_cast<CTaskEnterVehicle*>(GetParent())->CheckForBikeOnGround(iUnused);
		m_MoveNetworkHelper.SetFlag(bBikeOnGround, ms_BikeOnGroundId);
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicleAlign::IsShortestAlignDirectionLeft() const
{
	Vector3 vEntryPos(Vector3::ZeroType);
	Quaternion qEntryOrientation(0.0f,0.0f,0.0f,1.0f);

	CModelSeatInfo::CalculateEntrySituation(m_pVehicle, GetPed(), vEntryPos, qEntryOrientation, m_iEntryPointIndex, 0, 0.0f, &m_vLocalSpaceOffset);
	Vector3 vEulers(Vector3::ZeroType);
	qEntryOrientation.ToEulers(vEulers, "xyz");
	const float fEntryHeading = fwAngle::LimitRadianAngle(vEulers.z);

	const float fLeftDelta = CClipHelper::ComputeHeadingDeltaFromLeft(m_fInitialHeading, fEntryHeading);
	const float fRightDelta = CClipHelper::ComputeHeadingDeltaFromRight(m_fInitialHeading, fEntryHeading);

	return Abs(fLeftDelta) < Abs(fRightDelta) ? true : false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicleAlign::ComputeAlignTarget(Vector3& vTransTarget, Quaternion& qRotTarget)
{
	CTaskEnterVehicle* pParentEnterVehicleTask = (GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_ENTER_VEHICLE) ? static_cast<CTaskEnterVehicle*>(GetParent()) : NULL;
	bool bEnterOnVehicle = pParentEnterVehicleTask ? pParentEnterVehicleTask->IsFlagSet(CVehicleEnterExitFlags::EnteringOnVehicle) : false;

	bool bForcedDoorState = false;
	bool bForceDoorOpen = false;
	if (GetPed()->IsNetworkClone() && pParentEnterVehicleTask)
	{
		bForcedDoorState = pParentEnterVehicleTask->GetForcedDoorState();
		if (bForcedDoorState)
		{
			bForceDoorOpen = pParentEnterVehicleTask->GetVehicleDoorOpen();
		}
	}
	Vector3 vLocalSpaceModifier = pParentEnterVehicleTask->GetLocalSpaceEntryModifier();
	ComputeAlignTarget(vTransTarget, qRotTarget, bEnterOnVehicle, *m_pVehicle, *GetPed(), m_iEntryPointIndex, &vLocalSpaceModifier, false, m_bInWaterAlignment, bForcedDoorState, bForceDoorOpen);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicleAlign::IsEnteringStandingTurretSeat() const
{
	if (!m_pVehicle->GetIsTypeVehicle())
		return false;

	const CVehicle& rVeh = *static_cast<const CVehicle*>(m_pVehicle.Get());
	if (rVeh.GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_TURRET_SEAT_ON_VEHICLE))
	{
		if (rVeh.IsEntryIndexValid(m_iEntryPointIndex))
		{
			const CEntryExitPoint* pEntryPoint =rVeh.GetEntryExitPoint(m_iEntryPointIndex);
			const s32 iSeatIndex = pEntryPoint->GetSeat(SA_directAccessSeat);
			if (pEntryPoint && rVeh.IsTurretSeat(iSeatIndex))
			{
				if (rVeh.GetSeatAnimationInfo(iSeatIndex)->IsStandingTurretSeat())
				{
					return true;
				}
			}
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicleAlign::ProcessTurretTurn( CTaskEnterVehicle& rEnterVehicleTask, CVehicle& rVeh )
{
	// TODO: drive from door tags?
	TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, TIME_BEFORE_TURN_TURRET, 0.1f, 0.0f, 1.0f, 0.01f);
	if (GetTimeInState() < TIME_BEFORE_TURN_TURRET)
		return;

	CVehicleWeaponMgr* pVehWeaponMgr = rVeh.GetVehicleWeaponMgr();
	if (!pVehWeaponMgr)
		return;

	if (pVehWeaponMgr->GetNumTurrets() == 0)
		return;

	const s32 iSeatIndex = rEnterVehicleTask.GetTargetSeat();
	if (!rVeh.IsSeatIndexValid(iSeatIndex))
		return;

	CTurret* pTurret = pVehWeaponMgr->GetFirstTurretForSeat(iSeatIndex);
	if (!pTurret)
		return;

	if (!rVeh.GetSeatAnimationInfo(iSeatIndex)->IsStandingTurretSeat())
		return;

	if (rVeh.GetSeatManager()->GetPedInSeat(iSeatIndex))
		return;

	if (!rEnterVehicleTask.IsFlagSet(CVehicleEnterExitFlags::NeedToTurnTurret))
	{
		Vector3 vTargetPosition = rEnterVehicleTask.GetTargetPosition();
		pTurret->AimAtWorldPos(vTargetPosition, &rVeh, false, false);
	}
	else
	{
		Matrix34 matTurretWorldTest;
		pTurret->GetTurretMatrixWorld(matTurretWorldTest, &rVeh);
		Vector3 vTurretPos = matTurretWorldTest.d;
		Vector3 vWorldRefPos = CTaskVehicleFSM::ComputeWorldSpacePosFromLocal(rVeh, rEnterVehicleTask.GetLocalSpaceEntryModifier());
		Vector3 vToPedPos = vWorldRefPos - vTurretPos;
		vToPedPos.RotateZ(PI);
		TUNE_GROUP_FLOAT(TURRET_TUNE, TURN_SCALE, 20.0f, 0.0f, 50.0f, 1.0f);
		const Vector3 vTargetPos = vTurretPos + vToPedPos * TURN_SCALE;
		rEnterVehicleTask.SetTargetPosition(vTargetPos);
#if DEBUG_DRAW
		ms_debugDraw.AddSphere(RCC_VEC3V(vTargetPos), 0.05f, Color_blue, 2000);
#endif // DEBUG_DRAW	
		rEnterVehicleTask.ClearRunningFlag(CVehicleEnterExitFlags::NeedToTurnTurret);
	}
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
void CTaskEnterVehicleAlign::Debug() const
{
#if DEBUG_DRAW
	// Calculate the world space position and orientation we would like to lerp to during the align
	Vector3 vEntryPos(Vector3::ZeroType);
	Quaternion qEntryOrientation(0.0f,0.0f,0.0f,1.0f);

	CModelSeatInfo::CalculateEntrySituation(m_pVehicle, GetPed(), vEntryPos, qEntryOrientation, m_iEntryPointIndex, 0, 0.0f, &m_vLocalSpaceOffset);

	Matrix34 mEntryMtx;
	mEntryMtx.FromQuaternion(qEntryOrientation);
	mEntryMtx.d = vEntryPos;

	static dev_float sfScale = 0.1f;
	grcDebugDraw::Axis(mEntryMtx, sfScale);

	Vector3 vGroundPos(Vector3::ZeroType);
	if (CTaskVehicleFSM::FindGroundPos(m_pVehicle, GetPed(), vEntryPos, 2.5f, vGroundPos))
	{
		// Adjust the reference position
		vEntryPos = vGroundPos;
	}
	grcDebugDraw::Line(RCC_VEC3V(mEntryMtx.d), RCC_VEC3V(vEntryPos), Color_blue);

	mEntryMtx.d = vEntryPos;
	grcDebugDraw::Axis(mEntryMtx, sfScale);

#endif // DEBUG_DRAW
}
#endif // !__FINAL

////////////////////////////////////////////////////////////////////////////////

const fwMvRequestId CTaskOpenVehicleDoorFromOutside::ms_TryLockedDoorId("TryLockedDoor",0x305E0BF1);
const fwMvStateId CTaskOpenVehicleDoorFromOutside::ms_TryLockedDoorStateId("Try Locked Door",0xB7D01AC);
const fwMvRequestId CTaskOpenVehicleDoorFromOutside::ms_ForcedEntryId("ForcedEntry",0x6ABF7836);
const fwMvRequestId CTaskOpenVehicleDoorFromOutside::ms_ForcedEntryStateId("Forced Entry",0xC6451EB9);
const fwMvRequestId CTaskOpenVehicleDoorFromOutside::ms_OpenDoorId("OpenDoor",0x61F507C5);
const fwMvRequestId CTaskOpenVehicleDoorFromOutside::ms_OpenDoorStateId("Open Door",0xAB6691C8);
const fwMvBooleanId CTaskOpenVehicleDoorFromOutside::ms_OpenDoorClipFinishedId("OpenDoorAnimFinished",0xD81E9C8C);
const fwMvBooleanId CTaskOpenVehicleDoorFromOutside::ms_TryLockedDoorOnEnterId("TryLockedDoor_OnEnter",0xE68694B6);
const fwMvBooleanId CTaskOpenVehicleDoorFromOutside::ms_TryLockedDoorClipFinishedId("TryLockedDoorAnimFinished",0x1E0CC46F);
const fwMvBooleanId CTaskOpenVehicleDoorFromOutside::ms_ForcedEntryOnEnterId("ForcedEntry_OnEnter",0x28C10362);
const fwMvBooleanId CTaskOpenVehicleDoorFromOutside::ms_OpenDoorOnEnterId("OpenDoor_OnEnter",0xCADA414C);
const fwMvClipId CTaskOpenVehicleDoorFromOutside::ms_ForcedEntryClipId("ForcedEntryClip",0x92A81E96);
const fwMvClipId CTaskOpenVehicleDoorFromOutside::ms_OpenDoorClipId("OpenDoorClip",0x213832E8);
const fwMvClipId CTaskOpenVehicleDoorFromOutside::ms_HighOpenDoorClipId("HighOpenDoorClip",0x155D064E);
const fwMvClipId CTaskOpenVehicleDoorFromOutside::ms_TryLockedDoorClipId("TryLockedDoorClip",0xCC5E43DA);
const fwMvFloatId CTaskOpenVehicleDoorFromOutside::ms_OpenDoorRateId("OpenDoorRate",0xEA74352);
const fwMvFloatId CTaskOpenVehicleDoorFromOutside::ms_ForcedEntryPhaseId("ForcedEntryPhase",0x9C86331E);
const fwMvFloatId CTaskOpenVehicleDoorFromOutside::ms_OpenDoorPhaseId("OpenDoorPhase",0x1B98437A);
const fwMvFloatId CTaskOpenVehicleDoorFromOutside::ms_TryLockedDoorPhaseId("TryLockedDoorPhase",0xA08490D3);
const fwMvFloatId CTaskOpenVehicleDoorFromOutside::ms_OpenDoorVehicleAngleId("OpenDoorVehicleAngle",0x789FA7D9);
const fwMvFloatId CTaskOpenVehicleDoorFromOutside::ms_OpenDoorBlendId("OpenDoorBlend",0xF3B7291D);
const fwMvFlagId CTaskOpenVehicleDoorFromOutside::ms_UseOpenDoorBlendId("UseOpenDoorBlend",0xD9D24D1A);
const fwMvClipId CTaskOpenVehicleDoorFromOutside::ms_OpenDoorHighClipId("OpenDoorHighClip",0x15B75B64);
const fwMvFloatId CTaskOpenVehicleDoorFromOutside::ms_OpenDoorHighPhaseId("OpenDoorHighPhase",0xFABD25DF);
const fwMvBooleanId	CTaskOpenVehicleDoorFromOutside::ms_OpenDoorHighAnimFinishedId("OpenDoorHighAnimFinished",0xB487E7E1);
const fwMvBooleanId	CTaskOpenVehicleDoorFromOutside::ms_CombatDoorOpenLerpEndId("CombatDoorOpenLerpEnd",0xF94C3A12);

////////////////////////////////////////////////////////////////////////////////

// Statics
CTaskOpenVehicleDoorFromOutside::Tunables CTaskOpenVehicleDoorFromOutside::ms_Tunables;

IMPLEMENT_VEHICLE_TASK_TUNABLES(CTaskOpenVehicleDoorFromOutside, 0x66a2dedf);

////////////////////////////////////////////////////////////////////////////////

void CTaskOpenVehicleDoorFromOutside::ProcessOpenDoorHelper(CTaskVehicleFSM* pVehicleTask, const crClip* pClip, CCarDoor* pDoor, float fPhase, float fPhaseToBeginOpenDoor, float fPhaseToEndOpenDoor, bool& bIkTurnedOn, bool& bIkTurnedOff, bool bProcessOnlyIfTagsFound, bool bFromOutside)
{
	if (!ms_Tunables.m_EnableOpenDoorHandIk)
		return;

	const bool bStartTagFound = CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pClip, CClipEventTags::Door, CClipEventTags::Start, true, fPhaseToBeginOpenDoor);
	const bool bEndTagFound = CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pClip, CClipEventTags::Door, CClipEventTags::Start, false, fPhaseToEndOpenDoor);

	if (bProcessOnlyIfTagsFound && (!bStartTagFound || !bEndTagFound))
	{
		return;
	}

	s32 iFlags = CTaskVehicleFSM::VF_OpenDoor;

	if (bFromOutside)
	{
		iFlags |= CTaskVehicleFSM::VF_FromOutside;
	}

	CVehicle& vehicle = *pVehicleTask->GetVehicle();
	const s32 iEntryPointIndex = pVehicleTask->GetTargetEntryPoint();

	if (vehicle.GetLayoutInfo() && vehicle.GetLayoutInfo()->GetUseDoorOscillation())
	{
		iFlags |= CTaskVehicleFSM::VF_UseOscillation;
	}

	// If we're a front seat, find the door behind us and try to drive it shut if its not being used
	CPed& ped = *pVehicleTask->GetPed();
	s32 iFrontEntryIndex = CTaskVehicleFSM::GetEntryPointInFrontIfBeingUsed(&vehicle, iEntryPointIndex, &ped);
	if (vehicle.IsEntryIndexValid(iFrontEntryIndex))
	{
		iFlags |= CTaskVehicleFSM::VF_FrontEntryUsed;
	}

	CTaskVehicleFSM::DriveDoorFromClip(vehicle, pDoor, iFlags, fPhase, fPhaseToBeginOpenDoor, fPhaseToEndOpenDoor);

	// Lets not do any ik if the ped is handcuffed
	if (ped.GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed))
	{
		return;
	}

	TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, AUTOMATIC_DOOR_OPEN_RATIO_TO_ALLOW_HANDLE_IK, 0.2f, 0.0f, 1.0f, 0.01f);
	if (!vehicle.GetLayoutInfo()->GetNoArmIkOnOutsideOpenDoor() && (!vehicle.GetLayoutInfo()->GetAutomaticCloseDoor() || pDoor->GetDoorRatio() <= AUTOMATIC_DOOR_OPEN_RATIO_TO_ALLOW_HANDLE_IK))
	{
		ProcessDoorHandleArmIkHelper(vehicle, ped, pVehicleTask->GetTargetEntryPoint(), pClip, fPhase, bIkTurnedOn, bIkTurnedOff);
	}
}

////////////////////////////////////////////////////////////////////////////////

CTaskOpenVehicleDoorFromOutside::CTaskOpenVehicleDoorFromOutside(CVehicle* pVehicle, s32 iEntryPointIndex, CMoveNetworkHelper& moveNetworkHelper)
: CTaskVehicleFSM(pVehicle, SR_Specific, -1, m_iRunningFlags, iEntryPointIndex)
, m_MoveNetworkHelper(moveNetworkHelper)
, m_bSmashedWindow(false)
, m_iLastCompletedState(-1)
, m_bTurnedOnDoorHandleIk(false)
, m_bTurnedOffDoorHandleIk(false)
, m_bAnimFinished(false)
, m_bProcessPhysicsCalledThisFrame(false)
, m_bFixupFinished(false)
, m_bOpenDoorFinished(false)
, m_bWantsToExit(false)
, m_fOriginalHeading(0.0f)
, m_qInitialOrientation(V_IDENTITY)
, m_vInitialTranslation(V_ZERO)
, m_fCombatDoorOpenLerpEnd(0.35f)
{
	SetInternalTaskType(CTaskTypes::TASK_OPEN_VEHICLE_DOOR_FROM_OUTSIDE);
}

////////////////////////////////////////////////////////////////////////////////

CTaskOpenVehicleDoorFromOutside::~CTaskOpenVehicleDoorFromOutside()
{

}

////////////////////////////////////////////////////////////////////////////////

void CTaskOpenVehicleDoorFromOutside::CleanUp()
{
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskOpenVehicleDoorFromOutside::ProcessPreFSM()
{
	if (!taskVerifyf(m_pVehicle, "NULL vehicle pointer in CTaskOpenVehicleDoorFromOutside::ProcessPreFSM"))
	{
		return FSM_Quit;
	}

	if (!taskVerifyf(GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_ENTER_VEHICLE, "Invalid parent task"))
	{
		return FSM_Quit;
	}

	if (!taskVerifyf(m_iTargetEntryPoint != -1 && m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint), "Invalid entry point index"))
	{
		return FSM_Quit;
	}

	m_bProcessPhysicsCalledThisFrame = false;
	m_qFirstPhysicsOrientationDelta = QuatV(V_IDENTITY);
	m_vFirstPhysicsTranslationDelta = Vec3V(V_ZERO);

	GetPed()->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );

	return FSM_Continue;
}	

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskOpenVehicleDoorFromOutside::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();	

		FSM_State(State_TryLockedDoor)
			FSM_OnEnter
				return TryLockedDoor_OnEnter();
			FSM_OnUpdate
				return TryLockedDoor_OnUpdate();

		FSM_State(State_ForcedEntry)
			FSM_OnEnter
				return ForcedEntry_OnEnter();
			FSM_OnUpdate
				return ForcedEntry_OnUpdate();

		FSM_State(State_OpenDoor)
			FSM_OnEnter
				return OpenDoor_CommonOnEnter();
			FSM_OnUpdate
				return OpenDoor_CommonOnUpdate();
			FSM_OnExit
				return OpenDoor_CommonOnExit();

		FSM_State(State_OpenDoorBlend)
			FSM_OnEnter
				return OpenDoor_CommonOnEnter();
			FSM_OnUpdate
				return OpenDoor_CommonOnUpdate();
			FSM_OnExit
				return OpenDoor_CommonOnExit();

		FSM_State(State_OpenDoorCombat)
			FSM_OnEnter
				return OpenDoor_CommonOnEnter();
			FSM_OnUpdate
				return OpenDoor_CommonOnUpdate();
			FSM_OnExit
				return OpenDoor_CommonOnExit();

		FSM_State(State_OpenDoorWater)
			FSM_OnEnter
				return OpenDoor_CommonOnEnter();
		FSM_OnUpdate
				return OpenDoor_CommonOnUpdate();
		FSM_OnExit
				return OpenDoor_CommonOnExit();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskOpenVehicleDoorFromOutside::ProcessPhysics(float UNUSED_PARAM(fTimeStep), int UNUSED_PARAM(nTimeSlice))
{
	// We do the align and open door during the same anim, lets forget the animated velocity and 
	// just position the ped manually ourself based on a tag in the anim, get the animators to animate the mover linearly
	if (GetState() == State_OpenDoorCombat && !m_bFixupFinished && m_pVehicle)
	{
		CPed& rPed = *GetPed();

		float fPhase = 0.0f;
		GetClipAndPhaseForState(fPhase);

		// Once we've reached the critical point, we need to start aligning to the open door orientation
		bool bUseOpenDoorAlignPoint = fPhase <= m_fCombatDoorOpenLerpEnd ? false : true;

		if (bUseOpenDoorAlignPoint && !m_bOpenDoorFinished)
		{
			// Set the initial offset to our current position, so the lerping now begins from this point
			ComputeLocalTransformFromGlobal(rPed, rPed.GetTransform().GetPosition(), rPed.GetTransform().GetOrientation(), m_vInitialTranslation, m_qInitialOrientation);
			m_bOpenDoorFinished = true;
		}

		// Calculate the world space position and orientation we would like to lerp to during the align
		Vector3 vTemp(Vector3::ZeroType);
		Quaternion qTemp(0.0f,0.0f,0.0f,1.0f);
		const bool bEnterOnVehicle = static_cast<CTaskEnterVehicle*>(GetParent())->IsFlagSet(CVehicleEnterExitFlags::EnteringOnVehicle);
		CTaskEnterVehicleAlign::ComputeAlignTarget(vTemp, qTemp, bEnterOnVehicle, *m_pVehicle, rPed, m_iTargetEntryPoint, NULL, bUseOpenDoorAlignPoint);

		// Alter the z coord based on the ground position
		Vector3 vGroundPos(Vector3::ZeroType);
		if (CTaskVehicleFSM::FindGroundPos(m_pVehicle, GetPed(), vTemp, 2.5f, vGroundPos))
		{
			// Adjust the reference position
			vTemp = vGroundPos;
		}

		// Convert to new vector library
		Vec3V vEntryPos = VECTOR3_TO_VEC3V(vTemp);
		QuatV qEntryRot = QUATERNION_TO_QUATV(qTemp);

		Vec3V vInitialWorldPos(V_ZERO);
		QuatV qInitialWorldRot(V_IDENTITY);

		// Maybe we should just do the lerps in attach space?
		if (CTaskVehicleFSM::ComputeGlobalTransformFromLocal(rPed, m_vInitialTranslation, m_qInitialOrientation, vInitialWorldPos, qInitialWorldRot))
		{
			const float fLerpRatio = fPhase <= m_fCombatDoorOpenLerpEnd ? rage::Clamp(fPhase / m_fCombatDoorOpenLerpEnd, 0.0f, 1.0f) : rage::Clamp((fPhase - m_fCombatDoorOpenLerpEnd) / (1.0f - m_fCombatDoorOpenLerpEnd), 0.0f, 1.0f);
			// Make sure we go the shortest route
			qEntryRot = PrepareSlerp(qInitialWorldRot, qEntryRot);
			Vec3V vNewWorldPos = Lerp(ScalarVFromF32(fLerpRatio), vInitialWorldPos, vEntryPos);
			QuatV qNewWorldRot = Nlerp(ScalarVFromF32(fLerpRatio), qInitialWorldRot, qEntryRot);
			CTaskVehicleFSM::UpdatePedVehicleAttachment(&rPed, vNewWorldPos, qNewWorldRot);
		}
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskOpenVehicleDoorFromOutside::ProcessMoveSignals()
{
	if(m_bAnimFinished)
	{
		// Already finished, nothing to do.
		return false;
	}

	const int state = GetState();

	if(state == State_OpenDoor || state == State_OpenDoorBlend || state == State_OpenDoorCombat || state == State_OpenDoorWater)
	{
		bool bUseHighClipEvents = ShouldUseHighClipEvents();

		if (bUseHighClipEvents)
		{
			// Check if done playing the open door high clips.
			if (IsMoveNetworkStateFinished(ms_OpenDoorHighAnimFinishedId, ms_OpenDoorHighPhaseId))
			{
				// Done, we need no more updates.
				m_bAnimFinished = true;
				return false;
			}
		}
		else
		{
			// Check if done playing the open door clips.
			if (IsMoveNetworkStateFinished(ms_OpenDoorClipFinishedId, ms_OpenDoorPhaseId))
			{
				// Done, we need no more updates.
				m_bAnimFinished = true;
				return false;
			}
		}

		// Still waiting.
		return true;
	}
	else if(state == State_TryLockedDoor)
	{
		// Check if done running the try locked door Move state.
		if (IsMoveNetworkStateFinished(ms_TryLockedDoorClipFinishedId, ms_TryLockedDoorPhaseId))
		{
			// Done, we need no more updates.
			m_bAnimFinished = true;
			return false;
		}

		// Still waiting.
		return true;
	}
	else if(state == State_ForcedEntry)
	{
		// Check if done running the forced entry Move state.
		if (IsMoveNetworkStateFinished(ms_OpenDoorClipFinishedId, ms_ForcedEntryPhaseId))
		{
			// Done, we need no more updates.
			m_bAnimFinished = true;
			return false;
		}

		// Still waiting.
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskOpenVehicleDoorFromOutside::UpdateClonedFSM( s32 iState, CTask::FSM_Event iEvent )
{
	FSM_Begin

		// Wait for a network update to decide which state to start in
		FSM_State(State_Start)
			FSM_OnUpdate
				if(GetStateFromNetwork() > State_Start)
				{
					SetTaskState(GetStateFromNetwork());
				}
				else if(GetStateFromNetwork() == -1)
				{
				    SetTaskState(State_Finish);
				}

		FSM_State(State_TryLockedDoor)
			FSM_OnEnter
				return TryLockedDoor_OnEnter();
			FSM_OnUpdate
				return TryLockedDoor_OnUpdate();

		FSM_State(State_ForcedEntry)
			FSM_OnEnter
				return ForcedEntry_OnEnter();
			FSM_OnUpdate
				return ForcedEntry_OnUpdate();

        FSM_State(State_OpenDoor)
			FSM_OnEnter
				return OpenDoor_CommonOnEnter();
			FSM_OnUpdate
				return OpenDoor_CommonOnUpdate();
			FSM_OnExit
				return OpenDoor_CommonOnExit();

		FSM_State(State_OpenDoorBlend)
			FSM_OnEnter
				return OpenDoor_CommonOnEnter();
			FSM_OnUpdate
				return OpenDoor_CommonOnUpdate();
			FSM_OnExit
				return OpenDoor_CommonOnExit();

		FSM_State(State_OpenDoorCombat)
			FSM_OnEnter
				return OpenDoor_CommonOnEnter();
			FSM_OnUpdate
				return OpenDoor_CommonOnUpdate();
			FSM_OnExit
				return OpenDoor_CommonOnExit();

		FSM_State(State_OpenDoorWater)
			FSM_OnEnter
				return OpenDoor_CommonOnEnter();
		FSM_OnUpdate
				return OpenDoor_CommonOnUpdate();
		FSM_OnExit
				return OpenDoor_CommonOnExit();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

CTaskInfo* CTaskOpenVehicleDoorFromOutside::CreateQueriableState() const
{
	return rage_new CClonedEnterVehicleOpenDoorInfo(GetState(), m_pVehicle, m_iSeatRequestType, m_iSeat, m_iTargetEntryPoint, m_iRunningFlags);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskOpenVehicleDoorFromOutside::ReadQueriableState( CClonedFSMTaskInfo* pTaskInfo )
{
	Assert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_ENTER_VEHICLE_OPEN_DOOR_CLONED );
	// Call the base implementation - syncs the state
	CTaskVehicleFSM::ReadQueriableState(pTaskInfo);
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskOpenVehicleDoorFromOutside::Start_OnUpdate()
{
	CPed* pPed = GetPed();

	const CEntryExitPoint* pEntryPoint = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint);
	taskAssert(pEntryPoint);
	int iDoorBoneIndex = pEntryPoint->GetDoorBoneIndex();
	const CCarDoor* pDoor = m_pVehicle->GetDoorFromBoneIndex(iDoorBoneIndex);

	m_fOriginalHeading = pPed->GetCurrentHeading();	// Should be relative to vehicle in case it moves

	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ForcePedToBeDragged))
	{
		if (pDoor && pDoor->GetIsClosed())
		{
			SetTaskState(State_TryLockedDoor);
			return FSM_Continue;
		}
	}

	// Disabled for both mp and the player in sp, B*1110546 unless we can't enter the vehicle
	dev_s32 TIME_AFTER_LAST_SHOT_TO_SKIP_TRYING_LOCKED_DOOR = 5000;
	bool bSkipTryLockedDoor = (m_pVehicle->GetDoorLockStateFromEntryPoint(m_iTargetEntryPoint) == CARLOCK_LOCKED_INITIALLY || m_pVehicle->GetDoorLockStateFromEntryPoint(m_iTargetEntryPoint) == CARLOCK_LOCKED_BUT_CAN_BE_DAMAGED) && (NetworkInterface::IsGameInProgress() || pPed->IsLocalPlayer());

	const CVehicleEntryPointInfo* pEntryPointInfo = m_pVehicle->GetVehicleModelInfo()->GetEntryPointInfo(m_iTargetEntryPoint);
	if (pEntryPointInfo && pEntryPointInfo->GetTryLockedDoorOnGround())
	{
		bSkipTryLockedDoor = false;
	}

	const CVehicleEntryPointAnimInfo* pEntryAnimInfo = m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint);
	const bool bIsTula = MI_PLANE_TULA.IsValid() && m_pVehicle->GetModelIndex() == MI_PLANE_TULA;
	const bool bIsZhaba = MI_CAR_ZHABA.IsValid() && m_pVehicle->GetModelIndex() == MI_CAR_ZHABA;
	const bool bWaterEntry = static_cast<CTaskEnterVehicle*>(GetParent())->IsFlagSet(CVehicleEnterExitFlags::InWater) && ((pEntryAnimInfo && pEntryAnimInfo->GetHasGetInFromWater()) || bIsTula || bIsZhaba);
	if (bWaterEntry)
	{
		bSkipTryLockedDoor = true;
	}

#if __DEV
	TUNE_GROUP_BOOL(ENTER_VEHICLE_TUNE, FORCE_SKIP_TRY_LOCKED_DOOR, false);
	if (FORCE_SKIP_TRY_LOCKED_DOOR)
	{
		bSkipTryLockedDoor = true;
	}
#endif // __DEV
	bool bConvertibleWithRoofLowered = false;
	const bool bLockedForPlayer = CTaskVehicleFSM::IsVehicleLockedForPlayer(*m_pVehicle, *pPed);
	if( pPed->IsAPlayerPed() )
	{
		// If the vehicle is locked for the player try the locked door (B*1549129)
		if (pDoor && pDoor->GetIsIntact(m_pVehicle) && pDoor->GetIsClosed() && bLockedForPlayer)
		{
			bSkipTryLockedDoor = false;
		}
		else
		{
			// Can't skip trying these doors
			if( m_pVehicle->GetDoorLockStateFromEntryPoint(m_iTargetEntryPoint) == CARLOCK_LOCKED_INITIALLY )
			{
				if(!m_pVehicle->GetDriver() || m_pVehicle->GetDriver()->IsInjured() )
				{
					// B*2785640 - If vehicle lock state is partial, then only unlock the doors that are initially locked.
					if (m_pVehicle->GetCarDoorLocks() == CARLOCK_PARTIALLY)
					{
						for (int i=0; i < m_pVehicle->GetNumDoors(); i++)
						{
							if (m_pVehicle->GetDoorLockStateFromDoorIndex(i) == CARLOCK_LOCKED_INITIALLY)
							{
								m_pVehicle->SetDoorLockStateFromDoorIndex(i, CARLOCK_UNLOCKED);
							}
						}
					}
					else
					{
						m_pVehicle->SetCarDoorLocks(CARLOCK_UNLOCKED);
					}
				}
			}
			else if( m_pVehicle->GetDoorLockStateFromEntryPoint(m_iTargetEntryPoint) == CARLOCK_LOCKED_BUT_CAN_BE_DAMAGED ) 
			{
				if (m_pVehicle->DoesVehicleHaveAConvertibleRoofAnimation())
				{
					bConvertibleWithRoofLowered = m_pVehicle->GetConvertibleRoofState() == CTaskVehicleConvertibleRoof::STATE_LOWERED || m_pVehicle->GetConvertibleRoofState() == CTaskVehicleConvertibleRoof::STATE_LOWERING;
					bSkipTryLockedDoor = bConvertibleWithRoofLowered || pPed->IsAPlayerPed();

					if (bConvertibleWithRoofLowered)
					{
						m_pVehicle->SetCarDoorLocks(CARLOCK_UNLOCKED);
					}
				}
				else if (!m_pVehicle->CarHasRoof())
				{
					bSkipTryLockedDoor = true;
					m_pVehicle->SetCarDoorLocks(CARLOCK_UNLOCKED);
				}
				else if( pPed->GetPlayerWanted() && pPed->GetPlayerWanted()->GetWantedLevel() > WANTED_CLEAN )
				{
					bSkipTryLockedDoor = true;
				}
				else if( (CEventGunShot::GetLastTimeShotFired() + TIME_AFTER_LAST_SHOT_TO_SKIP_TRYING_LOCKED_DOOR) > fwTimer::GetTimeInMilliseconds())
				{
					bSkipTryLockedDoor = true;
				}
				else if( pDoor && !pDoor->GetIsClosed() )
				{
					bSkipTryLockedDoor = true;
				}
			}
			else if( m_pVehicle->GetDoorLockStateFromEntryPoint(m_iTargetEntryPoint) == CARLOCK_LOCKED ) 
			{
				if( pDoor && !pDoor->GetIsClosed() )
				{
					bSkipTryLockedDoor = true;
				}
			}
		}
	}
	// AI unlock initially locked cars to get in
	else if( m_pVehicle->GetDoorLockStateFromEntryPoint(m_iTargetEntryPoint) == CARLOCK_LOCKED_INITIALLY )
	{
		m_pVehicle->SetCarDoorLocks(CARLOCK_UNLOCKED);
	}

	bool bDoorIsLocked = false;
	if (m_pVehicle->GetCarDoorLocks() == CARLOCK_PARTIALLY)
	{
		bDoorIsLocked = m_pVehicle->GetDoorLockStateFromEntryPoint(m_iTargetEntryPoint) == CARLOCK_LOCKED;
	}

	if(!bConvertibleWithRoofLowered && (bDoorIsLocked || !CCarEnterExit::CarHasOpenableDoor(*m_pVehicle,pEntryPoint->GetDoorBoneIndex(),*pPed) || bLockedForPlayer))
	{
		bool networkPedCanEnterVehicle = true;
		if(NetworkInterface::IsGameInProgress() && pPed->IsPlayer())
		{
			CNetGamePlayer* player			= pPed->GetNetworkObject()->GetPlayerOwner();
			CNetObjVehicle* vehicleNetObj	= ((CNetObjVehicle*)m_pVehicle->GetNetworkObject());
	
			if(player && vehicleNetObj)
			{
				networkPedCanEnterVehicle = !vehicleNetObj->IsLockedForPlayer(player->GetPhysicalPlayerIndex());
			}
		}

		if (pEntryPointInfo)
		{
			eHierarchyId iWindow = CTaskEnterVehicle::GetWindowHierarchyToSmash(*m_pVehicle, m_iTargetEntryPoint);
			if( m_pVehicle->CanDoorsBeDamaged() &&
				!m_pVehicle->HasWindowToSmash(iWindow) && !pEntryPointInfo->GetIgnoreSmashWindowCheck() &&
				networkPedCanEnterVehicle)
			{
				m_pVehicle->SetCarDoorLocks(CARLOCK_UNLOCKED);
				m_pVehicle->TriggerCarAlarm();
			}
			else if (((bSkipTryLockedDoor || pEntryPointInfo->GetTryLockedDoorOnGround()) && pDoor && pDoor->GetIsClosed() && m_pVehicle->CanDoorsBeDamaged())
				&& (!NetworkInterface::IsGameInProgress() || networkPedCanEnterVehicle ) && !bWaterEntry)
			{
				SetTaskState(State_ForcedEntry);
				return FSM_Continue;
			}
			else
			{
				if (bSkipTryLockedDoor)
				{
					return FSM_Quit;
				}

				// Swear as you find the vehicle locked if pissed off
				if(pPed->IsAPlayerPed() && pPed->GetPlayerInfo() && pPed->GetPlayerInfo()->PlayerIsPissedOff())
					pPed->RandomlySay("GENERIC_CURSE", 0.5f);

				SetTaskState(State_TryLockedDoor);
				return FSM_Continue;
			}
		}
	}

	if (!taskVerifyf(m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint), "Ped 0x%p has invalid entrypoint index %i in vehicle %s", pPed, m_iTargetEntryPoint, m_pVehicle->GetModelName()))
	{
		return FSM_Quit;
	}

	if (bWaterEntry)
	{
		SetTaskState(State_OpenDoorWater);
		return FSM_Continue;		
	}
	else if (static_cast<CTaskEnterVehicle*>(GetParent())->ShouldUseCombatEntryAnimLogic(false))
	{
		SetTaskState(State_OpenDoorCombat);
		return FSM_Continue;
	}
	else
	{
		if (pEntryAnimInfo && pEntryAnimInfo->GetUseOpenDoorBlendAnims())
		{	
			SetTaskState(State_OpenDoorBlend);
			return FSM_Continue;
		}
		else
		{
			SetTaskState(State_OpenDoor);
			return FSM_Continue;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskOpenVehicleDoorFromOutside::TryLockedDoor_OnEnter()
{
	fwMvClipSetId clipSetId = m_MoveNetworkHelper.GetClipSetId();
	fwMvClipId clipId = ms_Tunables.m_DefaultTryLockedDoorClipId;
	GetAlternateClipInfoForPed(GetState(), *GetPed(), clipSetId, clipId);

	const crClip* pClip = fwClipSetManager::GetClip(clipSetId, clipId);
	if (!taskVerifyf(pClip, "Couldn't find clip %s from clipset %s", clipId.GetCStr(), clipSetId.GetCStr()))
	{
		return FSM_Quit;
	}

	m_MoveNetworkHelper.SetClip(pClip, ms_TryLockedDoorClipId);
	static_cast<CTaskEnterVehicle*>(GetParent())->StoreInitialOffsets();
	SetMoveNetworkState(ms_TryLockedDoorId, ms_TryLockedDoorOnEnterId, ms_TryLockedDoorStateId);
	SetClipPlaybackRate();

	m_bTurnedOnDoorHandleIk = false;
	m_bTurnedOffDoorHandleIk = false;

	// Let ProcessMoveSignals() handle the communication with the Move network.
	m_bAnimFinished = false;
	RequestProcessMoveSignalCalls();

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskOpenVehicleDoorFromOutside::TryLockedDoor_OnUpdate()
{
	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	CPed& ped = *GetPed();
	float fTryLockedDoorPhase = 0.0f;
	const crClip* pTryLockedDoorClip = GetClipAndPhaseForState(fTryLockedDoorPhase);
	if (pTryLockedDoorClip && m_pVehicle && m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint))
	{
		ProcessDoorHandleArmIkHelper(*m_pVehicle, ped, m_iTargetEntryPoint, pTryLockedDoorClip, fTryLockedDoorPhase, m_bTurnedOnDoorHandleIk, m_bTurnedOffDoorHandleIk);
	}

	bool bShouldQuit = false;
	if (ped.IsLocalPlayer() && CheckPlayerExitConditions(ped, *m_pVehicle, static_cast<CTaskEnterVehicle*>(GetParent())->GetRunningFlags(), CTaskVehicleFSM::MIN_TIME_BEFORE_PLAYER_CAN_ABORT))
	{
		bShouldQuit = true;
	}

	if (m_bAnimFinished)
	{
		m_iLastCompletedState = State_TryLockedDoor;

		if (GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_ForcePedToBeDragged))
		{
			SetFlag(aiTaskFlags::RestartCurrentState);
			// Set the task state for debug purpose
			SetTaskState(State_TryLockedDoor);
			return FSM_Continue;
		}

		if (m_pVehicle->GetCarDoorLocks() == CARLOCK_PARTIALLY)
		{
			bShouldQuit = m_pVehicle->GetDoorLockStateFromEntryPoint(m_iTargetEntryPoint) == CARLOCK_LOCKED;
		}

		// B*1454857 Never break into personal vehicles
        CPed* pPed = GetPed();
        if (!m_pVehicle->CanPedEnterCar(pPed) || m_pVehicle->IsPersonalVehicle() || m_pVehicle->IsPersonalOperationsVehicle())
        {
            bShouldQuit = true;
        }

		if (!bShouldQuit && ped.GetPedConfigFlag(CPED_CONFIG_FLAG_DisableForcedEntryForOpenVehiclesFromTryLockedDoor))
		{
			if (ped.IsPlayer() && !NetworkInterface::IsVehicleLockedForPlayer(m_pVehicle, &ped))
			{
				bShouldQuit = true;
			}
		}

		if (!bShouldQuit)
		{
			SetTaskState(State_ForcedEntry);
			return FSM_Continue;
		}
	}

	if (bShouldQuit)
	{
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskOpenVehicleDoorFromOutside::ForcedEntry_OnEnter()
{
	static_cast<CTaskEnterVehicle*>(GetParent())->ClearRunningFlag(CVehicleEnterExitFlags::CombatEntry);
	fwMvClipSetId clipSetId = m_MoveNetworkHelper.GetClipSetId();
	fwMvClipId clipId = ms_Tunables.m_DefaultForcedEntryClipId;
	GetAlternateClipInfoForPed(GetState(), *GetPed(), clipSetId, clipId);

	const crClip* pClip = fwClipSetManager::GetClip(clipSetId, clipId);
	if (!pClip)
	{
		// Probably should never get this far
		AI_LOG_WITH_ARGS("[EnterVehicleOpenDoor] - Ped %s quitting open door from outside task because couldn't find clip %s in clipset %s\n", AILogging::GetDynamicEntityNameSafe(GetPed()), clipId.GetCStr(), clipSetId.GetCStr());
		return FSM_Quit;
	}

	m_MoveNetworkHelper.SetClip(pClip, ms_ForcedEntryClipId);

	m_bTurnedOnDoorHandleIk = false;
	m_bTurnedOffDoorHandleIk = false;
	static_cast<CTaskEnterVehicle*>(GetParent())->StoreInitialOffsets();
	SetMoveNetworkState(ms_ForcedEntryId, ms_ForcedEntryOnEnterId, ms_ForcedEntryStateId);
	SetClipPlaybackRate();

	// Let ProcessMoveSignals() handle the communication with the Move network.
	m_bAnimFinished = false;
	RequestProcessMoveSignalCalls();

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskOpenVehicleDoorFromOutside::ForcedEntry_OnUpdate()
{
	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	// The event and clip signals are reused for the open door state so we can call processopendoor in both states
	if (m_bAnimFinished)
	{
		if (m_pVehicle->GetCarDoorLocks() == CARLOCK_LOCKED_BUT_CAN_BE_DAMAGED)
			m_pVehicle->SetCarDoorLocks(CARLOCK_UNLOCKED);

		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	ProcessOpenDoor(m_bTurnedOnDoorHandleIk, m_bTurnedOffDoorHandleIk);

	CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(m_pVehicle, m_iTargetEntryPoint);
	if(pDoor)
	{
		// Car alarm goes off as soon as door is unlatched
		if(!pDoor->GetIsLatched(m_pVehicle))
		{
			if (!m_pVehicle->GetDriver())
			{
				if(m_pVehicle->GetVehicleAudioEntity())
				{
					m_pVehicle->GetVehicleAudioEntity()->TriggerAlarm();
				}
			}
		}
	}

	CPed& rPed = *GetPed();
	float fPhase = 0.0f;
	const crClip* pClip = GetClipAndPhaseForState(fPhase);
	if (pClip)
	{
		CTaskEnterVehicle::ProcessSmashWindow(*m_pVehicle, rPed, m_iTargetEntryPoint, *pClip, fPhase, m_MoveNetworkHelper, m_bSmashedWindow);
	}

	const CVehicleEntryPointAnimInfo* pClipInfo = m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint);
	if (pClipInfo && pClipInfo->GetForcedEntryIncludesGetIn())
	{
		if (CTaskEnterVehicle::CanInterruptEnterSeat(rPed, *m_pVehicle, m_MoveNetworkHelper, m_iTargetEntryPoint, m_bWantsToExit, *static_cast<const CTaskEnterVehicle*>(GetParent())))
		{
			SetTaskState(State_Finish);
			return FSM_Continue;
		}
	}

	if (rPed.IsLocalPlayer() && m_MoveNetworkHelper.GetBoolean(CTaskEnterVehicle::ms_InterruptId) && CheckPlayerExitConditions(rPed, *m_pVehicle, static_cast<CTaskEnterVehicle*>(GetParent())->GetRunningFlags(), CTaskVehicleFSM::MIN_TIME_BEFORE_PLAYER_CAN_ABORT))
	{
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskOpenVehicleDoorFromOutside::OpenDoor_CommonOnEnter()
{
	if (!SetClipsForState())
	{
		return FSM_Quit;
	}

	static_cast<CTaskEnterVehicle*>(GetParent())->StoreInitialOffsets();
	SetClipPlaybackRate();
	SetMoveNetworkState(ms_OpenDoorId, ms_OpenDoorOnEnterId, ms_OpenDoorStateId);

	m_bTurnedOnDoorHandleIk = false;
	m_bTurnedOffDoorHandleIk = false;

	// Let ProcessMoveSignals() handle the communication with the Move network.
	m_bAnimFinished = false;
	RequestProcessMoveSignalCalls();

	CPed* pPed = GetPed();
	if (GetState() == State_OpenDoorCombat)
	{
		pPed->GetWeaponManager()->DestroyEquippedWeaponObject();
		
		const fwMvClipSetId clipSetId = m_MoveNetworkHelper.GetClipSetId();
		fwMvClipId clipId = ms_Tunables.m_CombatOpenDoorClipId;
		const crClip* pClip = fwClipSetManager::GetClip(clipSetId, clipId);
		float fUnused = 1.0f;
//		taskVerifyf(CClipEventTags::GetMoveEventStartAndEndPhases(pClip, ms_CombatDoorOpenLerpEndId, m_fCombatDoorOpenLerpEnd, fUnused), "Combat open door anim requires a CombatDoorOpenLerpEnd move event");
		CClipEventTags::GetMoveEventStartAndEndPhases(pClip, ms_CombatDoorOpenLerpEndId, m_fCombatDoorOpenLerpEnd, fUnused);
		ComputeLocalTransformFromGlobal(*pPed, pPed->GetTransform().GetPosition(), pPed->GetTransform().GetOrientation(), m_vInitialTranslation, m_qInitialOrientation);
	}

	CTaskVehicleFSM::SetRemotePedUsingDoorsTrue(*pPed, m_pVehicle, m_iTargetEntryPoint);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskOpenVehicleDoorFromOutside::OpenDoor_CommonOnUpdate()
{
	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	if(m_bAnimFinished)
	{
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	bool bUseHighClipEvents = ShouldUseHighClipEvents();
	ProcessOpenDoor(m_bTurnedOnDoorHandleIk, m_bTurnedOffDoorHandleIk, bUseHighClipEvents, true);

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskOpenVehicleDoorFromOutside::OpenDoor_CommonOnExit()
{
	CPed& rPed = *GetPed();
	CTaskVehicleFSM::SetRemotePedUsingDoorsFalse(rPed, m_pVehicle, m_iTargetEntryPoint);
	CTaskVehicleFSM::ClearDoorDrivingFlags(m_pVehicle, m_iTargetEntryPoint);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

const crClip* CTaskOpenVehicleDoorFromOutside::GetClipAndPhaseForState(float& fPhase)
{
	switch (GetState())
	{
		case State_OpenDoor:
		case State_OpenDoorBlend:
		case State_OpenDoorCombat:
		case State_OpenDoorWater:
			{
				fPhase = m_MoveNetworkHelper.GetFloat(ms_OpenDoorPhaseId);
				fPhase = rage::Clamp(fPhase, 0.0f, 1.0f);
				return m_MoveNetworkHelper.GetClip(ms_OpenDoorClipId);
			}
		case State_TryLockedDoor: 
			{
				fPhase = m_MoveNetworkHelper.GetFloat(ms_TryLockedDoorPhaseId);
				fPhase = rage::Clamp(fPhase, 0.0f, 1.0f);
				return m_MoveNetworkHelper.GetClip(ms_TryLockedDoorClipId);
			}
		case State_ForcedEntry:
			{
				fPhase = m_MoveNetworkHelper.GetFloat(ms_ForcedEntryPhaseId);
				fPhase = rage::Clamp(fPhase, 0.0f, 1.0f);
				return m_MoveNetworkHelper.GetClip(ms_ForcedEntryClipId);
			}
		case State_Start: break;  // network clones can remain in the start state for a few frames while waiting on a network update
        case State_Finish: break;
		default: taskAssertf(0, "Unhandled State");
	}

	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskOpenVehicleDoorFromOutside::ProcessOpenDoor(bool& bIkTurnedOn, bool& bIkTurnedOff, bool bUseHighClip, bool bFromOutside)
{
	float fPhase = 0.0f;

	const crClip* pClip = bUseHighClip ? m_MoveNetworkHelper.GetClip(ms_OpenDoorHighClipId) : GetClipAndPhaseForState(fPhase);

	if (bUseHighClip)
	{
		fPhase = m_MoveNetworkHelper.GetFloat(ms_OpenDoorHighPhaseId);
	}

	if (pClip)
	{
		CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(m_pVehicle, m_iTargetEntryPoint);
		if (pDoor)
		{
			static dev_float sfPhaseToBeginOpenDoor = 0.5f;
			static dev_float sfPhaseToEndOpenDoor = 1.0f;

			float fPhaseToBeginOpenDoor = sfPhaseToBeginOpenDoor;
			float fPhaseToEndOpenDoor = sfPhaseToEndOpenDoor;

			ProcessOpenDoorHelper(this, pClip, pDoor, fPhase, fPhaseToBeginOpenDoor, fPhaseToEndOpenDoor, bIkTurnedOn, bIkTurnedOff, false, bFromOutside);

			CCarDoor* pDoor2 = m_pVehicle->GetSecondDoorFromEntryPointIndex(m_iTargetEntryPoint);
			if (pDoor2)
			{
				bool bUnused1, bUnused2;
				ProcessOpenDoorHelper(this, pClip, pDoor2, fPhase, fPhaseToBeginOpenDoor, fPhaseToEndOpenDoor, bUnused1, bUnused2, false, bFromOutside);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskOpenVehicleDoorFromOutside::SetClipPlaybackRate()
{
	const s32 iState = GetState();
	float fClipRate = 1.0f;
	const CAnimRateSet& rAnimRateSet = m_pVehicle->GetLayoutInfo()->GetAnimRateSet();

	// Otherwise determine if we should speed up the entry anims and which tuning values to use (anim vs non anim)
	const CPed& rPed = *GetPed();
	const bool bShouldUseCombatEntryRates = static_cast<CTaskEnterVehicle*>(GetParent())->IsFlagSet(CVehicleEnterExitFlags::CombatEntry) || static_cast<CTaskEnterVehicle*>(GetParent())->IsFlagSet(CVehicleEnterExitFlags::UseFastClipRate) || rPed.GetPedResetFlag(CPED_RESET_FLAG_UseFastEnterExitVehicleRates);

	// Set the clip rate based on our conditions
	if (bShouldUseCombatEntryRates)
	{
		if (iState == State_OpenDoorCombat)
		{
			fClipRate = rAnimRateSet.m_AnimCombatEntry.GetRate();
		}
		else
		{
			if ((iState == State_TryLockedDoor || iState == State_ForcedEntry) && !rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_UseLockpickVehicleEntryAnimations))
			{
				fClipRate = rAnimRateSet.m_ForcedEntry.GetRate();
			}
			else
			{
				fClipRate = rAnimRateSet.m_NoAnimCombatEntry.GetRate();
			}
		}
	}
	else
	{
		fClipRate = rAnimRateSet.m_NormalEntry.GetRate();
	}

	m_MoveNetworkHelper.SetFloat(ms_OpenDoorRateId, fClipRate);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskOpenVehicleDoorFromOutside::ShouldUseHighClipEvents() const
{
	bool bUseHighClipEvents = false;

	const s32 iState = GetState();
	if (iState == State_OpenDoorBlend)
	{
		const float fOpenDoorBlend = m_MoveNetworkHelper.GetFloat(ms_OpenDoorBlendId);
		bUseHighClipEvents = fOpenDoorBlend > ms_Tunables.m_MinBlendWeightToUseHighClipEvents ? true : false;
		//Displayf("Use High Clip Events : %i", bUseHighClipEvents);
	}

	return bUseHighClipEvents;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskOpenVehicleDoorFromOutside::GetAlternateClipInfoForPed(s32 iState, const CPed& ped, fwMvClipSetId& clipSetId, fwMvClipId& clipId) const
{
	s32 iPlayerIndex = -1;

	if (!NetworkInterface::IsGameInProgress())
	{
		if (ped.IsLocalPlayer())
		{
			iPlayerIndex = CPlayerInfo::GetPlayerIndex(ped);
		}
	}
	else if (ped.GetPedConfigFlag(CPED_CONFIG_FLAG_UseLockpickVehicleEntryAnimations))
	{
		iPlayerIndex = 1;
	}

	if (iPlayerIndex > -1)
	{
		// Franklin doesn't do his custom forced entry anim he has a wanted level / being shot etc B*1143756
		if (iState == State_ForcedEntry && iPlayerIndex == 1 && ped.WantsToUseActionMode() && !ped.GetPedConfigFlag(CPED_CONFIG_FLAG_UseLockpickVehicleEntryAnimations))
		{
			return;
		}

		const CVehicleEntryPointAnimInfo* pEntryPointClipInfo = m_pVehicle->GetVehicleModelInfo()->GetEntryPointAnimInfo(m_iTargetEntryPoint);
		if (pEntryPointClipInfo)
		{
			fwFlags32 uInformationFlags = 0;
			fwMvClipSetId forcedBreakInClipSetId = pEntryPointClipInfo->GetCustomPlayerBreakInClipSet(iPlayerIndex, &uInformationFlags);
			if (!(uInformationFlags.IsFlagSet(CClipSetMap::IF_ScriptFlagOnly) && !ped.GetPedConfigFlag(CPED_CONFIG_FLAG_UseLockpickVehicleEntryAnimations)))
			{
				if (forcedBreakInClipSetId != CLIP_SET_ID_INVALID && CTaskVehicleFSM::IsVehicleClipSetLoaded(forcedBreakInClipSetId))
				{
					fwMvClipId newClipId = iState == State_TryLockedDoor ? pEntryPointClipInfo->GetAlternateTryLockedDoorClipId() : pEntryPointClipInfo->GetAlternateForcedEntryClipId();
					if (newClipId != CLIP_SET_ID_INVALID)
					{
						clipSetId = forcedBreakInClipSetId;
						clipId = newClipId;
					}
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskOpenVehicleDoorFromOutside::SetClipsForState()
{
	const fwMvClipSetId clipSetId = m_MoveNetworkHelper.GetClipSetId();

	if (GetState() == State_OpenDoor)
	{
		fwMvClipId clipId = ms_Tunables.m_DefaultOpenDoorClipId;

		const crClip* pClip = fwClipSetManager::GetClip(clipSetId, clipId);
		if (!taskVerifyf(pClip, "Couldn't find clip %s from clipset %s", clipId.GetCStr(), clipSetId.GetCStr()))
		{
			return false;
		}

		m_MoveNetworkHelper.SetClip(pClip, ms_OpenDoorClipId);
		return true;
	}
	else if (GetState() == State_OpenDoorBlend)
	{
		if (m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint))
		{
			const CVehicleEntryPointAnimInfo* pEntryAnimInfo = m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint);
			if (pEntryAnimInfo && pEntryAnimInfo->GetUseOpenDoorBlendAnims() && !static_cast<CTaskEnterVehicle*>(GetParent())->IsFlagSet(CVehicleEnterExitFlags::CombatEntry))
			{	
				const CEntryExitPoint* pEntryPoint = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint);		
				s32 iDoorHandleBoneIndex = pEntryPoint->GetDoorHandleBoneIndex();
				if (iDoorHandleBoneIndex > -1)
				{
					// Possibly should use hand position? - Seems fairly close atm though
					Vector3 vPedPos = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
					// Get the handle position
					Matrix34 mHandleMtx;
					m_pVehicle->GetGlobalMtx(iDoorHandleBoneIndex, mHandleMtx);
					// Get the height difference and normalise
					float fHeightDiff = rage::Clamp(mHandleMtx.d.z - vPedPos.z, 0.0f, 1.0f);
					if (m_pVehicle->GetLayoutInfo()->GetUseVanOpenDoorBlendParams())
					{
						fHeightDiff = rage::Clamp(fHeightDiff - ms_Tunables.m_MinHandleHeightDiffVan, 0.0f, 1.0f);
						fHeightDiff /= ms_Tunables.m_MaxHandleHeightDiffVan;
					}
					else
					{
						fHeightDiff /= ms_Tunables.m_MaxHandleHeightDiff;
					}
					m_MoveNetworkHelper.SetFlag(true, ms_UseOpenDoorBlendId);
					aiDebugf2("Height Signal : %.4f", fHeightDiff);
					m_MoveNetworkHelper.SetFloat(ms_OpenDoorVehicleAngleId, fHeightDiff);
				}
			}
		}

		fwMvClipId clipId0 = ms_Tunables.m_DefaultOpenDoorClipId;
		fwMvClipId clipId1 = ms_Tunables.m_HighOpenDoorClipId;

		const crClip* pClip0 = fwClipSetManager::GetClip(clipSetId, clipId0);
		const crClip* pClip1 = fwClipSetManager::GetClip(clipSetId, clipId1);
		if (!taskVerifyf(pClip0, "Couldn't find clip %s from clipset %s", clipId0.GetCStr(), clipSetId.GetCStr()) ||
			!taskVerifyf(pClip1, "Couldn't find clip %s from clipset %s", clipId1.GetCStr(), clipSetId.GetCStr()))
		{
			return false;
		}

		m_MoveNetworkHelper.SetClip(pClip0, ms_OpenDoorClipId);
		m_MoveNetworkHelper.SetClip(pClip1, ms_HighOpenDoorClipId);
		return true;
	}
	else if (GetState() == State_OpenDoorCombat)
	{
		fwMvClipId clipId = ms_Tunables.m_CombatOpenDoorClipId;

		const crClip* pClip = fwClipSetManager::GetClip(clipSetId, clipId);
		if (!taskVerifyf(pClip, "Couldn't find clip %s from clipset %s", clipId.GetCStr(), clipSetId.GetCStr()))
		{
			return false;
		}

		m_MoveNetworkHelper.SetClip(pClip, ms_OpenDoorClipId);
		return true;
	}
	else if (GetState() == State_OpenDoorWater)
	{
		fwMvClipId clipId = ms_Tunables.m_WaterOpenDoorClipId;

		const crClip* pClip = fwClipSetManager::GetClip(clipSetId, clipId);
		if (!taskVerifyf(pClip, "Couldn't find clip %s from clipset %s", clipId.GetCStr(), clipSetId.GetCStr()))
		{
			return false;
		}

		m_MoveNetworkHelper.SetClip(pClip, ms_OpenDoorClipId);
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

// DAN TEMP - function for ensuring a valid clip transition is available based on the current move network state
//            this will be replaced when the move network interface is extended to include this
bool CTaskOpenVehicleDoorFromOutside::IsMoveTransitionAvailable(s32 iNextState) const
{
    switch(iNextState)
    {
    case State_TryLockedDoor:
	case State_ForcedEntry:
        return (m_iLastCompletedState == -1) || (m_iLastCompletedState == State_TryLockedDoor);
    case State_OpenDoor:
	case State_OpenDoorBlend:
	case State_OpenDoorCombat:
	case State_OpenDoorWater:
        return (m_iLastCompletedState == -1);
    default:
        taskAssertf(0, "Unexpected state!");
        return true;
    }
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL

void CTaskOpenVehicleDoorFromOutside::Debug() const
{
#if __BANK
	if (m_pVehicle)
	{
		// Look up bone index of target entry point
		s32 iBoneIndex = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromEntryPoint(m_iTargetEntryPoint);

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

CClonedEnterVehicleOpenDoorInfo::CClonedEnterVehicleOpenDoorInfo()
{
}

CClonedEnterVehicleOpenDoorInfo::CClonedEnterVehicleOpenDoorInfo(s32 enterState, CVehicle* pVehicle, SeatRequestType iSeatRequest, s32 iSeat, s32 iTargetEntryPoint, VehicleEnterExitFlags iRunningFlags) 
: CClonedVehicleFSMInfo(enterState, pVehicle, iSeatRequest, iSeat, iTargetEntryPoint, iRunningFlags)
{

}

//////////////////////////////////////////////////////////////////////////

const fwMvBooleanId CTaskCloseVehicleDoorFromInside::ms_CloseDoorClipFinishedId("CloseDoorAnimFinished",0xA23C50F3);
const fwMvBooleanId CTaskCloseVehicleDoorFromInside::ms_CloseDoorOnEnterId("CloseDoor_OnEnter",0x40C4545);
const fwMvClipId CTaskCloseVehicleDoorFromInside::ms_CloseDoorClipId("CloseDoorClip",0xD9C00C84);
const fwMvFloatId CTaskCloseVehicleDoorFromInside::ms_CloseDoorPhaseId("CloseDoorPhase",0x3F197B37);
const fwMvFloatId CTaskCloseVehicleDoorFromInside::ms_CloseDoorRateId("CloseDoorRate",0xDCC67D79);
const fwMvRequestId CTaskCloseVehicleDoorFromInside::ms_CloseDoorRequestId("CloseDoor",0xECD1883E);
const fwMvStateId CTaskCloseVehicleDoorFromInside::ms_CloseDoorStateId("Close Door From Inside",0x78897DD7);
const fwMvBooleanId CTaskCloseVehicleDoorFromInside::ms_CloseDoorShuffleInterruptId("CloseDoorShuffleInterrupt",0x5D49CE77);
const fwMvBooleanId CTaskCloseVehicleDoorFromInside::ms_CloseDoorStartEngineInterruptId("CloseDoorStartEngineInterrupt",0x77682801);
const fwMvFlagId CTaskCloseVehicleDoorFromInside::ms_UseCloseDoorBlendId("UseCloseDoorBlend",0xE1D494ED);
const fwMvFlagId CTaskCloseVehicleDoorFromInside::ms_FirstPersonModeId("FirstPersonMode",0x8BB6FFFA);
const fwMvFilterId CTaskCloseVehicleDoorFromInside::ms_CloseDoorFirstPersonFilterId("CloseDoorFirstPersonFilter",0xFCD2DED9);
const fwMvFloatId CTaskCloseVehicleDoorFromInside::ms_CloseDoorLengthId("CloseDoorLength",0x666513C2);
const fwMvFloatId CTaskCloseVehicleDoorFromInside::ms_CloseDoorBlendId("CloseDoorBlend",0xF787F528);
const fwMvBooleanId	CTaskCloseVehicleDoorFromInside::ms_CloseDoorNearAnimFinishedId("CloseDoorNearAnimFinished",0x8E667C3F);
const fwMvClipId CTaskCloseVehicleDoorFromInside::ms_CloseDoorNearClipId("CloseDoorNearClip",0x236E13F8);
const fwMvFloatId CTaskCloseVehicleDoorFromInside::ms_CloseDoorNearPhaseId("CloseDoorNearPhase",0xFCF56D3E);

////////////////////////////////////////////////////////////////////////////////

// Statics
CTaskCloseVehicleDoorFromInside::Tunables CTaskCloseVehicleDoorFromInside::ms_Tunables;

IMPLEMENT_VEHICLE_TASK_TUNABLES(CTaskCloseVehicleDoorFromInside, 0xbb64f88a);

////////////////////////////////////////////////////////////////////////////////

CTaskCloseVehicleDoorFromInside::CTaskCloseVehicleDoorFromInside(CVehicle* pVehicle, s32 iEntryPointIndex, CMoveNetworkHelper& moveNetworkHelper)
: CTaskVehicleFSM(pVehicle, SR_Specific, -1, m_iRunningFlags, iEntryPointIndex)
, m_MoveNetworkHelper(moveNetworkHelper)
, m_bMoveNetworkFinished(false)
, m_bTurnedOnDoorHandleIk(false)
, m_bTurnedOffDoorHandleIk(false)
, m_bDontUseArmIk(false)
{
	SetInternalTaskType(CTaskTypes::TASK_CLOSE_VEHICLE_DOOR_FROM_INSIDE);
}

////////////////////////////////////////////////////////////////////////////////

CTaskCloseVehicleDoorFromInside::~CTaskCloseVehicleDoorFromInside()
{

}

////////////////////////////////////////////////////////////////////////////////

void CTaskCloseVehicleDoorFromInside::CleanUp()
{
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCloseVehicleDoorFromInside::ProcessPreFSM()
{
	if (!taskVerifyf(m_pVehicle, "NULL vehicle pointer in CTaskOpenVehicleDoorFromOutside::ProcessPreFSM"))
	{
		return FSM_Quit;
	}

	if (!taskVerifyf(GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_ENTER_VEHICLE, "Invalid parent task"))
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}	

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCloseVehicleDoorFromInside::UpdateFSM(const s32 iState, const FSM_Event iEvent)
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
			FSM_OnExit
				return CloseDoor_OnExit();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCloseVehicleDoorFromInside::UpdateClonedFSM( s32 iState, CTask::FSM_Event iEvent )
{
    return UpdateFSM(iState, iEvent); 
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskCloseVehicleDoorFromInside::ProcessMoveSignals()
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

CTaskInfo* CTaskCloseVehicleDoorFromInside::CreateQueriableState() const
{
    return rage_new CTaskInfo(); 
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCloseVehicleDoorFromInside::Start_OnUpdate()
{
	if (!taskVerifyf(m_iTargetEntryPoint != -1 && m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint), "Invalid entry point index"))
	{
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	bool bNetworkExists = m_MoveNetworkHelper.IsNetworkAttached();
	// We may not have started the network if we warped
	if (!bNetworkExists)
	{
		bNetworkExists = CTaskEnterVehicle::StartMoveNetwork(*GetPed(), *m_pVehicle, m_MoveNetworkHelper, m_iTargetEntryPoint, CTaskEnterVehicle::ms_Tunables.m_NetworkBlendDuration, NULL);
	}

	if (bNetworkExists)
	{
		SetTaskState(State_CloseDoor);
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCloseVehicleDoorFromInside::CloseDoor_OnEnter()
{
	m_bDontUseArmIk = false;

	CPed* pPed = GetPed();
	// We might have possibly shuffled seats, so might need to change clipsets
	if (!m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint))
	{
		m_iTargetEntryPoint = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed), m_pVehicle);
	}
	CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(m_pVehicle, m_iTargetEntryPoint);
	if (pDoor && pDoor->GetDoorRatio() < ms_Tunables.m_MinOpenDoorRatioToUseArmIk)
	{
		m_bDontUseArmIk = true;
	}
	const CVehicleEntryPointAnimInfo* pClipInfo = m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint);
	taskFatalAssertf(pClipInfo, "NULL Clip Info for entry index %i", m_iTargetEntryPoint);
	m_MoveNetworkHelper.SetClipSet(pClipInfo->GetEntryPointClipSetId());

	const CVehicleSeatAnimInfo* pSeatClipInfo = m_pVehicle->GetSeatAnimationInfo(pPed);
	if (pDoor && pSeatClipInfo && pSeatClipInfo->GetUseCloseDoorBlendAnims())
	{
		m_MoveNetworkHelper.SetFlag(true, ms_UseCloseDoorBlendId);
		m_MoveNetworkHelper.SetFloat(ms_CloseDoorLengthId, pDoor->GetDoorRatio());
	}

	const bool bFirstPersonMode = FPS_MODE_SUPPORTED_ONLY( pPed->IsFirstPersonShooterModeEnabledForPlayer(false) ? true : ) false;
	m_MoveNetworkHelper.SetFlag(bFirstPersonMode, ms_FirstPersonModeId);

#if FPS_MODE_SUPPORTED
	if (bFirstPersonMode)
	{
		const CVehicleEntryPointInfo* pEntryInfo = m_pVehicle->GetEntryInfo(m_iTargetEntryPoint);
		const bool bRightSide = pEntryInfo && (pEntryInfo->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_RIGHT);
		const fwMvFilterId firstPersonFilterId = bRightSide ? BONEMASK_HEAD_NECK_AND_R_ARM : BONEMASK_HEAD_NECK_AND_L_ARM;
		crFrameFilter* pFirstPersonFilter = g_FrameFilterDictionaryStore.FindFrameFilter(firstPersonFilterId);

		if (pFirstPersonFilter)
		{
			m_MoveNetworkHelper.SetFilter(pFirstPersonFilter, ms_CloseDoorFirstPersonFilterId);
		}
	}
#endif // FPS_MODE_SUPPORTED

	m_bTurnedOnDoorHandleIk = false;
	m_bTurnedOffDoorHandleIk = false;
	SetMoveNetworkState(ms_CloseDoorRequestId, ms_CloseDoorOnEnterId, ms_CloseDoorStateId);
	SetClipPlaybackRate();

	// Let ProcessMoveSignals() handle the communication with the Move network.
	m_bMoveNetworkFinished = false;
	RequestProcessMoveSignalCalls();

	// Start player ambient lookAt.
	if(pPed->IsAPlayerPed())
	{
		if(!GetSubTask())
		{
			// Use "EMPTY" ambient context. We don't want any ambient anims to be played. Only lookAts.
			CTaskAmbientClips* pAmbientTask	= rage_new CTaskAmbientClips(0, CONDITIONALANIMSMGR.GetConditionalAnimsGroup(emptyAmbientContext.GetHash()));
			SetNewTask( pAmbientTask );
		}
	}

	CTaskVehicleFSM::SetRemotePedUsingDoorsTrue(*pPed, m_pVehicle, m_iTargetEntryPoint);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCloseVehicleDoorFromInside::CloseDoor_OnUpdate()
{
	CPed* pPed = GetPed();
	pPed->SetPedResetFlag(CPED_RESET_FLAG_IsClosingVehicleDoor, true);

	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	const s32 iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
	if (m_pVehicle->IsSeatIndexValid(iSeatIndex))
	{
		const s32 iEntryIndex = m_pVehicle->GetDirectEntryPointIndexForSeat(iSeatIndex);
		if (m_pVehicle->IsEntryIndexValid(iEntryIndex))
		{
			CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(m_pVehicle, iEntryIndex);
			if (pDoor)
			{
				m_MoveNetworkHelper.SetFloat(ms_CloseDoorLengthId, pDoor->GetDoorRatio());
			}
		}
	}

	CCarDoor* pDoor = GetDoorForEntryPointIndex(m_iTargetEntryPoint);
	if (!pDoor ||!pDoor->GetIsIntact(m_pVehicle))
	{
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	bool bAbortCloseDoor = pPed->GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle) || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsDoingDriveby);
	const CVehicleSeatAnimInfo* pSeatClipInfo = m_pVehicle->GetSeatAnimationInfo(pPed);
	if (pSeatClipInfo && pSeatClipInfo->GetUseCloseDoorBlendAnims())
	{
		bAbortCloseDoor = !ProcessCloseDoorBlend(m_MoveNetworkHelper, *m_pVehicle, *pPed, m_iTargetEntryPoint, m_bTurnedOnDoorHandleIk, m_bTurnedOffDoorHandleIk, m_bDontUseArmIk);
	}
	else
	{
		bAbortCloseDoor = !ProcessCloseDoor(m_MoveNetworkHelper, *m_pVehicle, *pPed, m_iTargetEntryPoint, m_bTurnedOnDoorHandleIk, m_bTurnedOffDoorHandleIk, m_bDontUseArmIk);
	}

	// Abort if the door ratio is smaller than what we expect it to be
	if (bAbortCloseDoor)
	{
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	// Wait until its closed unless the anim is finished
	if (!pDoor->GetIsClosed() && !m_bMoveNetworkFinished)
		return FSM_Continue;


	// If going to the opposite seat, interrupt for the shuffle
	if (pPed->IsLocalPlayer() && m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint))
	{
		const CEntryExitPoint* pEntryPoint = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint);
		if (pEntryPoint)
		{
			if (m_MoveNetworkHelper.GetBoolean(ms_CloseDoorShuffleInterruptId))
			{
				CTaskEnterVehicle* pParent = static_cast<CTaskEnterVehicle*>(GetParent());
				// Not sure if the specific request is valid
				const bool bCanFinishEarly = (pParent && pParent->GetSeatRequestType() == SR_Specific) || (pPed->IsAPlayerPed() && NetworkInterface::IsGameInProgress());
				if (bCanFinishEarly && pParent->GetTargetSeat() != pEntryPoint->GetSeat(SA_directAccessSeat))
				{
					pDoor->SetTargetDoorOpenRatio(0.0f, CCarDoor::DRIVEN_AUTORESET|CCarDoor::WILL_LOCK_DRIVEN);
					SetTaskState(State_Finish);
					return FSM_Continue;
				}
			}
			else if (m_MoveNetworkHelper.GetBoolean(ms_CloseDoorStartEngineInterruptId) && CTaskEnterVehicle::CheckForStartEngineInterrupt(*pPed, *m_pVehicle))
			{
				CCarDoor* pDoor = GetDoorForEntryPointIndex(m_iTargetEntryPoint);
				if (pDoor)
				{
					pDoor->SetTargetDoorOpenRatio(0.0f, CCarDoor::DRIVEN_AUTORESET|CCarDoor::WILL_LOCK_DRIVEN);
				}
				SetTaskState(State_Finish);
				return FSM_Continue;
			}

		}
	}

	if(m_bMoveNetworkFinished)
	{
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCloseVehicleDoorFromInside::CloseDoor_OnExit()
{
	if (m_pVehicle)
	{
		CTaskVehicleFSM::ClearDoorDrivingFlags(m_pVehicle, m_iTargetEntryPoint);
		CCarDoor* pDoor = GetDoorForEntryPointIndex(m_iTargetEntryPoint);
		if (pDoor)
		{
			if (m_bMoveNetworkFinished)
			{
				pDoor->SetTargetDoorOpenRatio(0.0f, CCarDoor::DRIVEN_AUTORESET|CCarDoor::DRIVEN_SHUT|CCarDoor::WILL_LOCK_DRIVEN);
			}
			else
			{
				if (!pDoor->GetIsLatched(m_pVehicle))
				{
					pDoor->SetSwingingFree(m_pVehicle);
				}
			}
		}

		CPed* pPed = GetPed();

		CTaskVehicleFSM::SetRemotePedUsingDoorsFalse(*pPed, m_pVehicle, m_iTargetEntryPoint);
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskCloseVehicleDoorFromInside::SetClipPlaybackRate()
{
	const CAnimRateSet& rAnimRateSet = m_pVehicle->GetLayoutInfo()->GetAnimRateSet();
	if (rAnimRateSet.m_UseInVehicleCombatRates)
	{
		float fClipRate = 1.0f;

		// Otherwise determine if we should speed up the entry anims and which tuning values to use (anim vs non anim)
		const CPed& rPed = *GetPed();
		const bool bShouldUseCombatEntryRates = static_cast<CTaskEnterVehicle*>(GetParent())->IsFlagSet(CVehicleEnterExitFlags::CombatEntry) || rPed.GetPedResetFlag(CPED_RESET_FLAG_UseFastEnterExitVehicleRates);

		// Set the clip rate based on our conditions
		if (bShouldUseCombatEntryRates)
		{
			fClipRate = rAnimRateSet.m_NoAnimCombatInVehicle.GetRate();
		}
		else
		{
			fClipRate = rAnimRateSet.m_NormalInVehicle.GetRate();
		}

		m_MoveNetworkHelper.SetFloat(ms_CloseDoorRateId, fClipRate);
	}
	else
	{
		float fClipRate = 1.0f;
		float fScaleMultiplier = 1.0f;

		// Never increase the jacking rate base on the use fast clip rate as it won't sync
		if (static_cast<CTaskEnterVehicle*>(GetParent())->IsFlagSet(CVehicleEnterExitFlags::CombatEntry))
		{
			fScaleMultiplier = rage::Max(fScaleMultiplier, CTaskEnterVehicle::ms_Tunables.m_FastEnterExitRate);
		}

		if (CAnimSpeedUps::ShouldUseMPAnimRates() || GetPed()->GetPedResetFlag(CPED_RESET_FLAG_UseFastEnterExitVehicleRates))
		{
			fScaleMultiplier = rage::Max(fScaleMultiplier, CAnimSpeedUps::ms_Tunables.m_MultiplayerEnterExitJackVehicleRateModifier);
		}

		fClipRate *= fScaleMultiplier;
		m_MoveNetworkHelper.SetFloat(ms_CloseDoorRateId, fClipRate);
	}
}

////////////////////////////////////////////////////////////////////////////////

const crClip* CTaskCloseVehicleDoorFromInside::GetClipAndPhaseForState(float& fPhase)
{
	switch (GetState())
	{
		case State_CloseDoor: 
		{
			fPhase = m_MoveNetworkHelper.GetFloat( ms_CloseDoorPhaseId);
			fPhase = rage::Clamp(fPhase, 0.0f, 1.0f);
			return m_MoveNetworkHelper.GetClip( ms_CloseDoorClipId);
		}
		default: taskAssertf(0, "Unhandled State");
	}

	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskCloseVehicleDoorFromInside::ProcessCloseDoor(CMoveNetworkHelper& moveNetworkHelper, CVehicle& vehicle, CPed& ped, s32 iTargetEntryPointIndex, bool& bTurnedOnDoorHandleIk, bool& bTurnedOffDoorHandleIk, bool bDisableArmIk)
{
	float fCloseDoorPhase = moveNetworkHelper.GetFloat(ms_CloseDoorPhaseId);
	const crClip* pCloseDoorClip = moveNetworkHelper.GetClip(ms_CloseDoorClipId);

	if (pCloseDoorClip)
	{
		CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(&vehicle, iTargetEntryPointIndex);
		if (pDoor)
		{
			float fPhaseToBeginCloseDoor = ms_Tunables.m_DefaultCloseDoorStartPhase;
			float fPhaseToEndCloseDoor = ms_Tunables.m_DefaultCloseDoorEndPhase;

			CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pCloseDoorClip, CClipEventTags::Door, CClipEventTags::Start, true, fPhaseToBeginCloseDoor);
			CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pCloseDoorClip, CClipEventTags::Door, CClipEventTags::Start, false, fPhaseToEndCloseDoor);

			CTaskVehicleFSM::DriveDoorFromClip(vehicle, pDoor, CTaskVehicleFSM::VF_CloseDoor, fCloseDoorPhase, fPhaseToBeginCloseDoor, fPhaseToEndCloseDoor);

			CCarDoor* pDoor2 = vehicle.GetSecondDoorFromEntryPointIndex(iTargetEntryPointIndex);
			if (pDoor2)
			{
				CTaskVehicleFSM::DriveDoorFromClip(vehicle, pDoor2, CTaskVehicleFSM::VF_CloseDoor, fCloseDoorPhase, fPhaseToBeginCloseDoor, fPhaseToEndCloseDoor);
			}

			TUNE_GROUP_FLOAT(IN_VEHICLE_TUNE, ABORT_CLOSE_DOOR_SPEED_TOL_1, 2.0f, 0.0f, 5.0f, 0.001f);
			if (!ped.GetPedResetFlag(CPED_RESET_FLAG_IgnoreVelocityWhenClosingVehicleDoor))
			{
				if (pDoor->GetCurrentSpeed() >= ABORT_CLOSE_DOOR_SPEED_TOL_1)
				{
					const float fTargetRatio = rage::Min(1.0f, 1.0f - (fCloseDoorPhase - fPhaseToBeginCloseDoor) / (fPhaseToEndCloseDoor - fPhaseToBeginCloseDoor));
					const float fCurrentRatio = pDoor->GetDoorRatio();
					TUNE_GROUP_FLOAT(IN_VEHICLE_TUNE, ABORT_CLOSE_DOOR_RATIO_DIFF_TOL_1, 0.05f, 0.0f, 1.0f, 0.001f);
					if (fCurrentRatio < fTargetRatio - ABORT_CLOSE_DOOR_RATIO_DIFF_TOL_1)
					{
						return false;
					}
				}
			}

			if (!bDisableArmIk && vehicle.GetLayoutInfo() && !vehicle.GetLayoutInfo()->GetNoArmIkOnInsideCloseDoor() &&ms_Tunables.m_EnableCloseDoorHandIk)
			{
				ProcessDoorHandleArmIkHelper(vehicle, ped, iTargetEntryPointIndex, pCloseDoorClip, fCloseDoorPhase, bTurnedOnDoorHandleIk, bTurnedOffDoorHandleIk);
			}
		}
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskCloseVehicleDoorFromInside::ProcessCloseDoorBlend(CMoveNetworkHelper& moveNetworkHelper, CVehicle& vehicle, CPed& ped, s32 iTargetEntryPointIndex, bool& bTurnedOnDoorHandleIk, bool& bTurnedOffDoorHandleIk, bool bDisableArmIk)
{
	float fCloseDoorBlendWeight = moveNetworkHelper.GetFloat(ms_CloseDoorBlendId);
	const crClip* pCloseDoorNearClip = moveNetworkHelper.GetClip(ms_CloseDoorNearClipId);
	const crClip* pCloseDoorFarClip = moveNetworkHelper.GetClip(ms_CloseDoorClipId);

	if (pCloseDoorNearClip && pCloseDoorFarClip)
	{
		CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(&vehicle, iTargetEntryPointIndex);
		if (pDoor)
		{
			float fPhaseToBeginCloseDoor = ms_Tunables.m_DefaultCloseDoorStartPhase;
			float fPhaseToEndCloseDoor = ms_Tunables.m_DefaultCloseDoorEndPhase;

			const bool bUseFarClipEvents = fCloseDoorBlendWeight > ms_Tunables.m_MinBlendWeightToUseFarClipEvents ? true : false;
			const crClip* pCloseDoorClip = bUseFarClipEvents ? pCloseDoorFarClip : pCloseDoorNearClip;
			const float fCloseDoorPhase = bUseFarClipEvents ? moveNetworkHelper.GetFloat(ms_CloseDoorPhaseId) : moveNetworkHelper.GetFloat(ms_CloseDoorNearPhaseId);

			CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pCloseDoorClip, CClipEventTags::Door, CClipEventTags::Start, true, fPhaseToBeginCloseDoor);
			CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pCloseDoorClip, CClipEventTags::Door, CClipEventTags::Start, false, fPhaseToEndCloseDoor);

			TUNE_GROUP_FLOAT(IN_VEHICLE_TUNE, ABORT_CLOSE_DOOR_SPEED_TOL, 2.0f, 0.0f, 5.0f, 0.001f);
			if (!ped.GetPedResetFlag(CPED_RESET_FLAG_IgnoreVelocityWhenClosingVehicleDoor))
			{
				if (pDoor->GetCurrentSpeed() >= ABORT_CLOSE_DOOR_SPEED_TOL)
				{
					const float fTargetRatio = rage::Min(1.0f, 1.0f - (fCloseDoorPhase - fPhaseToBeginCloseDoor) / (fPhaseToEndCloseDoor - fPhaseToBeginCloseDoor));
					const float fCurrentRatio = pDoor->GetDoorRatio();
					TUNE_GROUP_FLOAT(IN_VEHICLE_TUNE, ABORT_CLOSE_DOOR_RATIO_DIFF_TOL, 0.05f, 0.0f, 1.0f, 0.001f);
					if (fCurrentRatio < fTargetRatio - ABORT_CLOSE_DOOR_RATIO_DIFF_TOL)
					{
						return false;
					}
				}
			}

			CTaskVehicleFSM::DriveDoorFromClip(vehicle, pDoor, CTaskVehicleFSM::VF_CloseDoor, fCloseDoorPhase, fPhaseToBeginCloseDoor, fPhaseToEndCloseDoor);

			if (!bDisableArmIk && vehicle.GetLayoutInfo() && !vehicle.GetLayoutInfo()->GetNoArmIkOnInsideCloseDoor())
			{
				ProcessDoorHandleArmIkHelper(vehicle, ped, iTargetEntryPointIndex, pCloseDoorClip, fCloseDoorPhase, bTurnedOnDoorHandleIk, bTurnedOffDoorHandleIk);
			}
		}
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskCloseVehicleDoorFromInside::IsDoorClearOfPeds(const CVehicle& vehicle, const CPed& ped, s32 iEntry)
{
	// If we haven't passed in an entry index, calculate it from the the ped's seat
	if (iEntry == -1)
	{
		const s32 iSeatIndex = vehicle.GetSeatManager()->GetPedsSeatIndex(&ped);
		if (!vehicle.IsSeatIndexValid(iSeatIndex))
		{
			return true;
		}

		iEntry = vehicle.GetDirectEntryPointIndexForSeat(iSeatIndex);
	}

	const s32 iEntryIndex = iEntry;
	if (vehicle.IsEntryIndexValid(iEntryIndex))
	{
		const CVehicleEntryPointInfo* pEntryInfo = vehicle.GetEntryInfo(iEntryIndex);
		if (pEntryInfo)
		{
			// Get the entry position
			Vector3 vEntryPosition;
			Quaternion qEntryOrientation;
			CModelSeatInfo::CalculateEntrySituation(&vehicle, &ped, vEntryPosition, qEntryOrientation, iEntryIndex);

			Vector3 vStart;
			Vector3 vEnd;

			if(vehicle.GetLayoutInfo()->GetClimbUpAfterOpenDoor() || pEntryInfo->GetIsPlaneHatchEntry())
			{
				vEntryPosition += (VEC3V_TO_VECTOR3(vehicle.GetTransform().GetRight()) * 0.5f);

				vStart = vEntryPosition - Vector3(0.0f, 0.0f, ms_Tunables.m_PedTestZStartOffset);
				vEnd = vEntryPosition + Vector3(0.0f, 0.0f, ms_Tunables.m_PedTestZOffset);
			}
			else
			{
				// Get the ped matrix (should also be the same as the seat position)
				Matrix34 testMatrix = MAT34V_TO_MATRIX34(ped.GetTransform().GetMatrix());
				Vector3 vSeatPosition = testMatrix.d;

				// Work out the direction to the entry point
				Vector3 vToEntryPoint = vEntryPosition - vSeatPosition;
				vToEntryPoint.Normalize();

				// We want our offset to go in the same direction to the entry point, at a 90 degree angle to the seat direction
				Vector3 vSideOffset = testMatrix.a;
				if (vToEntryPoint.Dot(vSideOffset) < 0.0f)
				{
					vSideOffset *= -1.0f;
				}
				
				// Add the world space offsets to the original position
				Vector3 vOffset = vSideOffset * ms_Tunables.m_PedTestXOffset + testMatrix.b * ms_Tunables.m_PedTestYOffset;
				testMatrix.d += vOffset;

				vStart = testMatrix.d - Vector3(0.0f, 0.0f, ms_Tunables.m_PedTestZStartOffset);
				vEnd = vStart + Vector3(0.0f, 0.0f, ms_Tunables.m_PedTestZOffset);
			}

			WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
			capsuleDesc.SetCapsule(vStart, vEnd, ms_Tunables.m_PedTestRadius);
			capsuleDesc.SetIncludeFlags(ArchetypeFlags::GTA_PED_TYPE|ArchetypeFlags::GTA_RAGDOLL_TYPE);
			capsuleDesc.SetExcludeEntity(&ped, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
			capsuleDesc.SetContext(WorldProbe::LOS_GeneralAI);

			WorldProbe::CShapeTestFixedResults<> capsuleResult;
			capsuleDesc.SetResultsStructure(&capsuleResult);
			if (!WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc))
			{
#if DEBUG_DRAW
				CTask::ms_debugDraw.AddCapsule(RCC_VEC3V(vStart), RCC_VEC3V(vEnd), ms_Tunables.m_PedTestRadius, Color_green, 1000, 0, false);
#endif // DEBUG_DRAW
				return true;
			}
#if DEBUG_DRAW
			CTask::ms_debugDraw.AddCapsule(RCC_VEC3V(vStart), RCC_VEC3V(vEnd), ms_Tunables.m_PedTestRadius, Color_red, 1000, 0, false);
#endif // DEBUG_DRAW
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskCloseVehicleDoorFromInside::GetStaticStateName( s32 iState )
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

void CTaskCloseVehicleDoorFromInside::Debug() const
{
#if __BANK
	if (m_pVehicle)
	{
		// Look up bone index of target entry point
		s32 iBoneIndex = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromEntryPoint(m_iTargetEntryPoint);

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

const fwMvBooleanId CTaskEnterVehicleSeat::ms_GetInShuffleInterruptId("GetInShuffleInterrupt",0xED897ECF);
const fwMvBooleanId CTaskEnterVehicleSeat::ms_CloseDoorInterruptId("CloseDoorInterrupt",0x8644CD73);
const fwMvBooleanId CTaskEnterVehicleSeat::ms_EnterSeatClipFinishedId("EnterSeatAnimFinished",0x40F24436);
const fwMvBooleanId CTaskEnterVehicleSeat::ms_EnterSeatOnEnterId("EnterSeat_OnEnter",0x5FE9A198);
const fwMvClipId CTaskEnterVehicleSeat::ms_EnterSeatClipId("EnterSeatClip",0x6981AB9F);
const fwMvFlagId CTaskEnterVehicleSeat::ms_IsDeadId("IsDead",0xE2625733);
const fwMvFlagId CTaskEnterVehicleSeat::ms_IsFatId("IsFat",0xDBEDC678);
const fwMvFloatId CTaskEnterVehicleSeat::ms_EnterSeatPhaseId("EnterSeatPhase",0x17B70BB5);
const fwMvRequestId CTaskEnterVehicleSeat::ms_EnterSeatRequestId("EnterSeat",0xAE333226);
const fwMvStateId CTaskEnterVehicleSeat::ms_EnterSeatStateId("Enter Seat",0x4D578E86);

////////////////////////////////////////////////////////////////////////////////

// Statics
CTaskEnterVehicleSeat::Tunables CTaskEnterVehicleSeat::ms_Tunables;

IMPLEMENT_VEHICLE_TASK_TUNABLES(CTaskEnterVehicleSeat, 0xc31d98fe);

////////////////////////////////////////////////////////////////////////////////

CTaskEnterVehicleSeat::CTaskEnterVehicleSeat(CVehicle* pVehicle, s32 iEntryPointIndex, s32 iSeat, VehicleEnterExitFlags iRunningFlags, CMoveNetworkHelper& moveNetworkHelper, bool forceMoveState)
: CTaskVehicleFSM(pVehicle, SR_Specific, iSeat, iRunningFlags, iEntryPointIndex)
, m_pJackedPed(NULL)
, m_pDriver(NULL)
, m_MoveNetworkHelper(moveNetworkHelper)
, m_bMoveNetworkFinished(false)
, m_bWantsToExit(false)
, m_ForceMoveState(forceMoveState)
, m_bAppliedForce(false)
, m_fSpringTime(0.0f)
, m_fInitialRatio(0.0f)
, m_fLegIkBlendOutPhase(0.0f)
, m_bBikeWasOnSideAtBeginningOfGetOn(false)
{
	SetInternalTaskType(CTaskTypes::TASK_ENTER_VEHICLE_SEAT);
}

////////////////////////////////////////////////////////////////////////////////

CTaskEnterVehicleSeat::~CTaskEnterVehicleSeat()
{

}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicleSeat::CleanUp()
{
#if GTA_REPLAY
	//clear replay AI task flags
	CPed* pPed = GetPed();
	if(pPed && CReplayMgr::IsReplayInControlOfWorld() == false)
	{
		pPed->GetReplayRelevantAITaskInfo().ClearAITaskRunning(REPLAY_ENTER_VEHICLE_SEAT_TASK);
	}
#endif
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicleSeat::ProcessPreFSM()
{
	if (!taskVerifyf(m_pVehicle, "NULL vehicle pointer in CTaskEnterVehicleSeat::ProcessPreFSM"))
	{
		return FSM_Quit;
	}

	if (!taskVerifyf(GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_ENTER_VEHICLE, "Invalid parent task"))
	{
		return FSM_Quit;
	}
	
	// B*2197806: Update buoyancy info/flags when entering from water. 
	// Without this, the swimming flag was still set on the ped despite being in a vehicle, causing the boat blocker capsule to still be enabled.
	if (GetPed() && GetState() == State_GetInFromWater)
	{
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_ForceBuoyancyProcessingIfAsleep, true);
	}

	if (m_pVehicle->GetSeatManager()->GetDriver())
	{
		m_pDriver = m_pVehicle->GetSeatManager()->GetDriver();
	}

	return FSM_Continue;
}	

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicleSeat::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();	

		FSM_State(State_GetIn)
			FSM_OnEnter
				return EnterSeatCommon_OnEnter();
			FSM_OnUpdate
				return EnterSeatCommon_OnUpdate();
			FSM_OnExit
				return EnterSeatCommon_OnExit();

		FSM_State(State_QuickGetOn)
			FSM_OnEnter
				return EnterSeatCommon_OnEnter();
			FSM_OnUpdate
				return EnterSeatCommon_OnUpdate();
			FSM_OnExit
				return EnterSeatCommon_OnExit();

		FSM_State(State_GetInFromSeatClipSet)
			FSM_OnEnter
				return EnterSeatCommon_OnEnter();
			FSM_OnUpdate
				return EnterSeatCommon_OnUpdate();
			FSM_OnExit
				return EnterSeatCommon_OnExit();

		FSM_State(State_GetInFromWater)
			FSM_OnEnter
				return EnterSeatCommon_OnEnter();
			FSM_OnUpdate
				return EnterSeatCommon_OnUpdate();
			FSM_OnExit
				return EnterSeatCommon_OnExit();

		FSM_State(State_GetInFromStandOn)
			FSM_OnEnter
				return EnterSeatCommon_OnEnter();
			FSM_OnUpdate
				return EnterSeatCommon_OnUpdate();
			FSM_OnExit
				return EnterSeatCommon_OnExit();

		FSM_State(State_GetInCombat)
			FSM_OnEnter
				return EnterSeatCommon_OnEnter();
			FSM_OnUpdate
				return EnterSeatCommon_OnUpdate();
			FSM_OnExit
				return EnterSeatCommon_OnExit();

		FSM_State(State_Ragdoll)
			FSM_OnEnter
				return Ragdoll_OnEnter();
			FSM_OnUpdate
				return Ragdoll_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return Finish_OnUpdate();

	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicleSeat::UpdateClonedFSM( s32 iState, CTask::FSM_Event iEvent )
{
    return UpdateFSM(iState, iEvent); 
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicleSeat::ProcessPostFSM()
{	
	const bool bIsDead = m_pJackedPed ? m_pJackedPed->IsInjured() : false;
	m_MoveNetworkHelper.SetFlag( bIsDead, ms_IsDeadId);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicleSeat::ProcessMoveSignals()
{
	if(m_bMoveNetworkFinished)
	{
		// Already finished, nothing to do.
		return false;
	}

	const int state = GetState();

	if(	state == State_GetIn || 
		state == State_GetInFromSeatClipSet || 
		state == State_GetInCombat || 
		state == State_GetInFromStandOn || 
		state == State_GetInFromWater || 
		state == State_QuickGetOn)
	{
		// Check if done running the enter seat Move state.
		if (IsMoveNetworkStateFinished(ms_EnterSeatClipFinishedId, ms_EnterSeatPhaseId))
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

CTaskInfo* CTaskEnterVehicleSeat::CreateQueriableState() const
{
    return rage_new CTaskInfo();
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicleSeat::Start_OnUpdate()
{
	if (!taskVerifyf(m_iTargetEntryPoint != -1 && m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint), "Invalid entry point index"))
	{
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	// Screwed if we don't have a valid seat or entry anim info
	s32 nSeatID = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat, m_iSeat);
	const CVehicleSeatAnimInfo* pSeatAnimInfo = m_pVehicle->GetSeatAnimationInfo(nSeatID);
	const CVehicleEntryPointAnimInfo* pEntryAnimInfo = m_pVehicle->GetEntryAnimInfo(m_iTargetEntryPoint);
	if (!taskVerifyf(pSeatAnimInfo && pEntryAnimInfo, "Couldn't find seat (%s) and/or entry anim (%s) info for vehicle %s, seat %i, entry index %i", pSeatAnimInfo ? "HAVE" : "NULL", pEntryAnimInfo ? "HAVE" : "NULL", m_pVehicle->GetModelName(), m_iSeat, m_iTargetEntryPoint))
	{
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

#if GTA_REPLAY
	//we need to know if this task was running so we can do some extra camera magic during playback
	CPed* pPed = GetPed();
	if(pPed && CReplayMgr::IsReplayInControlOfWorld() ==  false)
	{
		pPed->GetReplayRelevantAITaskInfo().SetAITaskRunning(REPLAY_ENTER_VEHICLE_SEAT_TASK);
	}
#endif

	const s32 iLastCompletedParentState = static_cast<CTaskEnterVehicle*>(GetParent())->GetLastCompletedState();
	if (iLastCompletedParentState == CTaskEnterVehicle::State_PickUpBike || iLastCompletedParentState == CTaskEnterVehicle::State_PullUpBike)
	{
		SetTaskState(State_QuickGetOn);
		return FSM_Continue;
	}
	else if (pEntryAnimInfo->GetUseSeatClipSetAnims())
	{
		m_MoveNetworkHelper.SetClipSet(pSeatAnimInfo->GetSeatClipSetId(), CTaskEnterVehicle::ms_SeatClipSetId);
		SetTaskState(State_GetInFromSeatClipSet);
		return FSM_Continue;
	}
	else if (IsFlagSet(CVehicleEnterExitFlags::EnteringOnVehicle) && pEntryAnimInfo->GetHasOnVehicleEntry() && pEntryAnimInfo->GetUseStandOnGetIn())
	{
		SetTaskState(State_GetInFromStandOn);
		return FSM_Continue;
	}
	else if (IsFlagSet(CVehicleEnterExitFlags::InWater) && pEntryAnimInfo->GetHasGetInFromWater())
	{
		SetTaskState(State_GetInFromWater);
		return FSM_Continue;
	}
	else if (static_cast<CTaskEnterVehicle*>(GetParent())->ShouldUseCombatEntryAnimLogic())
	{
		SetTaskState(State_GetInCombat);
		return FSM_Continue;
	}

	SetTaskState(State_GetIn);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicleSeat::EnterSeatCommon_OnEnter()
{
	CPed* pPed = GetPed();

	CTaskEnterVehicle::SetUpRagdollOnCollision(*pPed, *m_pVehicle);	
	SetClipsForState();
	SetClipPlaybackRate();
	SetMoveNetworkState(ms_EnterSeatRequestId, ms_EnterSeatOnEnterId, ms_EnterSeatStateId);
	
	//Add shocking event if it's a nice vehicle
	if (m_pVehicle->GetVehicleModelInfo() && m_pVehicle->GetVehicleModelInfo()->GetVehicleSwankness() > SWANKNESS_3 )
	{
		m_pVehicle->AddCommentOnVehicleEvent();
	}

	Assert( pPed );
	if( !pPed->IsPlayer() )
	{
		pPed->SetClothPinRadiusScale(0.0f);		
	}
	pPed->SetClothNegateForce(true);

	if (m_pVehicle->InheritsFromBike())
	{
		m_bBikeWasOnSideAtBeginningOfGetOn = false;

		const float fSide = m_pVehicle->GetTransform().GetA().GetZf();
		TUNE_GROUP_FLOAT(BIKE_ENTRY_TUNE, CONSIDER_ON_SIDE_MAG, 0.4f, 0.0f, 1.0f, 0.01f);
		if (Abs(fSide) > CONSIDER_ON_SIDE_MAG)
		{
			m_bBikeWasOnSideAtBeginningOfGetOn = true;
		}
	}
	
	// Let ProcessMoveSignals() handle the communication with the Move network.
	m_bMoveNetworkFinished = false;
	m_bAppliedForce = false;
	RequestProcessMoveSignalCalls();

	// Look up bone index of target entry point
	s32 iBoneIndex = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromEntryPoint(m_iTargetEntryPoint);

	if (iBoneIndex > -1)
	{
		CCarDoor* pDoor = m_pVehicle->GetDoorFromBoneIndex(iBoneIndex);
		if (pDoor)
		{
			m_fInitialRatio = pDoor->GetDoorRatio();
		}
	}

	// Start player ambient lookAt.
	if(GetPed()->IsAPlayerPed() && !IsFlagSet(CVehicleEnterExitFlags::FromCover))
	{
		if(!GetSubTask())
		{
			// Use "EMPTY" ambient context. We don't want any ambient anims to be played. Only lookAts.
			CTaskAmbientClips* pAmbientTask	= rage_new CTaskAmbientClips(0, CONDITIONALANIMSMGR.GetConditionalAnimsGroup(emptyAmbientContext.GetHash()));
			SetNewTask( pAmbientTask );		}
	}

	CTaskVehicleFSM::SetRemotePedUsingDoorsTrue(*pPed, m_pVehicle, m_iTargetEntryPoint);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicleSeat::EnterSeatCommon_OnUpdate()
{
	CPed& rPed = *GetPed();

	TUNE_GROUP_BOOL(VEHICLE_TUNE,ENABLE_KINEMATIC_MODE,true);
	if (ENABLE_KINEMATIC_MODE)
	{
		rPed.SetPedResetFlag(CPED_RESET_FLAG_TaskUseKinematicPhysics, true);
	}

	float fPhase = 0.0f;
	const crClip* pClip = GetClipAndPhaseForState(fPhase);
	CTaskVehicleFSM::ProcessLegIkBlendOut(rPed, m_fLegIkBlendOutPhase, fPhase);

	if (CheckForTransitionToRagdollState(rPed, *m_pVehicle))
	{
		SetTaskState(State_Ragdoll);
		return FSM_Continue;
	}

	bool bIsStandingTurretSeat = false;
#if ENABLE_MOTION_IN_TURRET_TASK	
	if (pClip)
	{
		const s32 iDirectSeatIndex = m_pVehicle->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat);
		if (m_pVehicle->IsTurretSeat(iDirectSeatIndex))
		{
			CTurret* pTurret = m_pVehicle->GetVehicleWeaponMgr() ? m_pVehicle->GetVehicleWeaponMgr()->GetFirstTurretForSeat(iDirectSeatIndex) : NULL;
			if (pTurret)
			{
				if (m_pVehicle->GetSeatAnimationInfo(iDirectSeatIndex)->IsStandingTurretSeat())
				{
					bIsStandingTurretSeat = true;
				}

				if (bIsStandingTurretSeat)
				{
					const crSkeletonData& rSkelData = m_pVehicle->GetSkeletonData();
					CTaskVehicleFSM::IkHandToTurret(rPed, *m_pVehicle, rSkelData, false, 0.25f, *pTurret);
					CTaskVehicleFSM::IkHandToTurret(rPed, *m_pVehicle, rSkelData, true, 0.25f, *pTurret);
				}
				else
				{
					CTaskVehicleFSM::ProcessTurretHandGripIk(*m_pVehicle, rPed, pClip, fPhase, *pTurret);
				}
			}
		}
	}
#endif // ENABLE_MOTION_IN_TURRET_TASK	


	const CVehicleEntryPointInfo* pEntryInfo = m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint) ? m_pVehicle->GetEntryInfo(m_iTargetEntryPoint) : NULL;
	CTaskEnterVehicle::eForceArmIk forceArmIk = CTaskEnterVehicle::ForceArmIk_None;

	if (m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint))
	{
		const bool bInheritsFromBike = m_pVehicle->InheritsFromBike();
		if (bInheritsFromBike || bIsStandingTurretSeat)
		{
			rPed.SetPedResetFlag(CPED_RESET_FLAG_RequiresLegIk, true);
		}

		if (bInheritsFromBike)
		{
			s32 iParentPreviousState = GetParent()->GetPreviousState();
			if (iParentPreviousState == CTaskEnterVehicle::State_PickUpBike || iParentPreviousState == CTaskEnterVehicle::State_PullUpBike)
			{
				if (pEntryInfo)
				{
					forceArmIk = (pEntryInfo->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT) ? CTaskEnterVehicle::ForceArmIk_Left : CTaskEnterVehicle::ForceArmIk_Right;
				}
			}
		}
	}

	if (m_pVehicle->GetIsJetSki() && pClip && m_pVehicle->IsEntryIndexValid(m_iTargetEntryPoint))
	{
		const CVehicleEntryPointInfo* pEntryInfo = m_pVehicle->GetEntryInfo(m_iTargetEntryPoint);
		if (pEntryInfo)
		{
			if (pEntryInfo->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT)
			{
				// Left hand
				CTaskVehicleFSM::ProcessBikeHandleArmIk(*m_pVehicle, rPed, pClip, fPhase, true, false, true);
			}
			else
			{
				//Right hand
				CTaskVehicleFSM::ProcessBikeHandleArmIk(*m_pVehicle, rPed, pClip, fPhase, true, true, true);
			}
		}
	}

	if (!m_pVehicle->InheritsFromBike())
	{
		float fTargetRatio = m_fInitialRatio;

		// If we're a front seat, find the door behind us and try to drive it shut if its not being used
		s32 iFrontEntryIndex = CTaskVehicleFSM::GetEntryPointInFrontIfBeingUsed(m_pVehicle, m_iTargetEntryPoint, &rPed);
		if (m_pVehicle->IsEntryIndexValid(iFrontEntryIndex))
		{
			fTargetRatio = CTaskEnterVehicle::ms_Tunables.m_TargetRearDoorOpenRatio;
		}

		// Look up bone index of target entry point
		s32 iBoneIndex = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromEntryPoint(m_iTargetEntryPoint);

		if (iBoneIndex > -1)
		{
			s32 iFrontEntryIndex = CTaskVehicleFSM::GetEntryPointInFrontIfBeingUsed(m_pVehicle, m_iTargetEntryPoint, &rPed);
			if (!m_pVehicle->IsEntryIndexValid(iFrontEntryIndex))
			{
				if (m_pVehicle->GetLayoutInfo() && m_pVehicle->GetLayoutInfo()->GetUseDoorOscillation())
				{
					CCarDoor* pDoor = m_pVehicle->GetDoorFromBoneIndex(iBoneIndex);
					if (pDoor && !(pDoor->GetFlag(CCarDoor::DRIVEN_NORESET) || pDoor->GetFlag(CCarDoor::DRIVEN_NORESET_NETWORK)))
					{
						TUNE_GROUP_FLOAT(VEHICLE_TUNE, OCSCILLATION_TERM, 25.0f, 0.0f, 500.0f, 0.1f);
						TUNE_GROUP_FLOAT(VEHICLE_TUNE, DAMPING_TERM, 7.5f, 0.0f, 50.0f, 0.1f);
						const float fMaxDisplacement = rage::Min(1.0f - CTaskEnterVehicle::ms_Tunables.m_MaxOpenRatioForOpenDoorOutside, CTaskEnterVehicle::ms_Tunables.m_MaxOscillationDisplacementOutside);
						const float fTimeStep		= GetTimeStep();
						m_fSpringTime += fTimeStep;
						const float fDisplacement	= expf(-DAMPING_TERM * fPhase) * fMaxDisplacement * Cosf(OCSCILLATION_TERM * fPhase);
						fTargetRatio = CTaskEnterVehicle::ms_Tunables.m_MaxOpenRatioForOpenDoorOutside + fDisplacement;	
						//Displayf("Target Ratio : %.4f", fTargetRatio);
						pDoor->SetTargetDoorOpenRatio(fTargetRatio, CCarDoor::DRIVEN_AUTORESET|CCarDoor::DRIVEN_SPECIAL);
					}
				}
				else
				{
					// Keep doors open when entering seat
					CTaskVehicleFSM::ProcessOpenDoor(*pClip, 1.0f, *m_pVehicle, m_iTargetEntryPoint, false, false, &rPed);
				}
			}
		}
	}

	if (!IsMoveNetworkStateValid())
	{
		if (forceArmIk != CTaskEnterVehicle::ForceArmIk_None)
		{
			CTaskVehicleFSM::ForceBikeHandleArmIk(*m_pVehicle, rPed, true, (forceArmIk == CTaskEnterVehicle::ForceArmIk_Right));
		}

		return FSM_Continue;
	}

	if (m_pVehicle->InheritsFromBike() && (!pEntryInfo || !pEntryInfo->GetIsPassengerOnlyEntry()))
	{
		// A bike has one hard coded door (CBike::InitDoors()) we use it to pick or pull it up!
		CCarDoor* pDoor = m_pVehicle->GetDoor(0); 

		if (pClip && pDoor)
		{
			TUNE_GROUP_FLOAT(BIKE_TUNE, PHASE_TO_BEGIN_BIKE_STABILISE, 0.95f, 0.0f, 1.0f, 0.01f);
			TUNE_GROUP_FLOAT(BIKE_TUNE, PHASE_TO_END_BIKE_STABILISE, 1.0f, 0.0f, 1.0f, 0.01f);
			TUNE_GROUP_FLOAT(BIKE_TUNE, PHASE_TO_BEGIN_BIKE_STABILISE_SIDE, 0.0f, 0.0f, 1.0f, 0.01f);
			TUNE_GROUP_FLOAT(BIKE_TUNE, PHASE_TO_END_BIKE_STABILISE_SIDE, 0.1f, 0.0f, 1.0f, 0.01f);
			TUNE_GROUP_FLOAT(BIKE_TUNE, PHASE_TO_ALLOW_ARM_IK_SIDE, 0.4f, 0.0f, 1.0f, 0.01f);

			float fPhaseToBeginOpenDoor = m_bBikeWasOnSideAtBeginningOfGetOn ? PHASE_TO_BEGIN_BIKE_STABILISE_SIDE : PHASE_TO_BEGIN_BIKE_STABILISE;
			float fPhaseToEndOpenDoor = m_bBikeWasOnSideAtBeginningOfGetOn ? PHASE_TO_END_BIKE_STABILISE_SIDE : PHASE_TO_END_BIKE_STABILISE;

			CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pClip, CClipEventTags::Door, CClipEventTags::Start, true, fPhaseToBeginOpenDoor);
			CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pClip, CClipEventTags::Door, CClipEventTags::Start, false, fPhaseToEndOpenDoor);

			if (fPhase >= fPhaseToBeginOpenDoor)
			{
				static_cast<CBike*>(m_pVehicle.Get())->m_nBikeFlags.bGettingPickedUp = true;
			}

			bool bDoArmIk = true;
			if (m_bBikeWasOnSideAtBeginningOfGetOn && fPhase < PHASE_TO_ALLOW_ARM_IK_SIDE)
			{
				bDoArmIk = false;
			}

			if (bDoArmIk)
			{
				CTaskEnterVehicle::ProcessBikeArmIk(*m_pVehicle, rPed, m_iTargetEntryPoint, *pClip, fPhase, false, forceArmIk);
			}

			CBike* pBike = static_cast<CBike*>(m_pVehicle.Get());

			float fPhaseRate = rage::Clamp(fPhase * CTaskEnterVehicle::ms_Tunables.m_BikeEnterLeanAngleOvershootRate, 0.0f, 1.0f);
			float fEndLeanAngle = rage::Lerp(fPhaseRate, pBike->GetLeanAngle(), m_pVehicle->GetLayoutInfo()->GetBikeLeansUnlessMoving() ? pBike->pHandling->GetBikeHandlingData()->m_fBikeOnStandLeanAngle : 0.0f);
				
			CTaskVehicleFSM::DriveBikeAngleFromClip(m_pVehicle, 0, fPhase, fPhaseToBeginOpenDoor, fPhaseToEndOpenDoor, pBike->GetLeanAngle(), fEndLeanAngle);
		}
		else
		{
			if (forceArmIk != CTaskEnterVehicle::ForceArmIk_None)
			{
				CTaskVehicleFSM::ForceBikeHandleArmIk(*m_pVehicle, rPed, true, (forceArmIk == CTaskEnterVehicle::ForceArmIk_Right));
			}
		}
	}

	if (CTaskVehicleFSM::ProcessApplyForce(m_MoveNetworkHelper, *m_pVehicle, rPed))
	{
		static_cast<CTaskEnterVehicle*>(GetParent())->SetRunningFlag(CVehicleEnterExitFlags::DontApplyForceWhenSettingPedIntoVehicle);
	}

	if (CTaskEnterVehicle::CanInterruptEnterSeat(rPed, *m_pVehicle, m_MoveNetworkHelper, m_iTargetEntryPoint, m_bWantsToExit, *static_cast<const CTaskEnterVehicle*>(GetParent())))
	{
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	if(m_bMoveNetworkFinished)
	{
		SetTaskState(State_Finish);
		return FSM_Continue;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskEnterVehicleSeat::EnterSeatCommon_OnExit()
{
	CTaskVehicleFSM::SetRemotePedUsingDoorsFalse(*GetPed(), m_pVehicle, m_iTargetEntryPoint);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicleSeat::Ragdoll_OnEnter()
{
	CTaskNMJumpRollFromRoadVehicle* pNMTask = rage_new CTaskNMJumpRollFromRoadVehicle(1000, 6000, true, false, atHashString::Null(), m_pVehicle);
	CEventSwitch2NM switchToNmEvent(6000, pNMTask);
	GetPed()->SwitchToRagdoll(switchToNmEvent);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicleSeat::Ragdoll_OnUpdate()
{
	// Should be getting a ragdoll event;
	if (!taskVerifyf(GetTimeInState() < 1.0f, "Ped 0x%p failed to complete ragdoll detach properly", GetPed()))
	{
		return FSM_Quit;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskEnterVehicleSeat::Finish_OnUpdate()
{
	Assert( m_pVehicle );
	if( m_pVehicle->GetVehicleModelInfo() && m_pVehicle->GetVehicleModelInfo()->GetIsAutomobile() )
	{
		CPed* pPed = GetPed();
		Assert( pPed );
		pPed->SetClothForceSkin(true);
	}

	return FSM_Quit;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicleSeat::CheckForTransitionToRagdollState(CPed& rPed, CVehicle& rVeh) const
{
	if (!CTaskNMBehaviour::CanUseRagdoll(&rPed, RAGDOLL_TRIGGER_IMPACT_CAR, &rVeh))
		return false;

	// If we had a driver on our bike that is no longer on it and is ragdolling, then put ourselfs into ragdoll
	if (!NetworkInterface::IsGameInProgress() && m_pVehicle->InheritsFromBike())
	{
		if (m_iSeat != m_pVehicle->GetDriverSeat())
		{
			if (m_pDriver && m_pDriver != &rPed && !m_pDriver->GetIsInVehicle() && 
				m_pDriver->GetUsingRagdoll() && m_pDriver->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_THROUGH_WINDSCREEN))
			{
				return true;
			}	
		}
	}	

	if (rPed.GetPedResetFlag(CPED_RESET_FLAG_BlockRagdollActivationInVehicle))
		return false;

	const CClipEventTags::CBlockRagdollTag* pProp = static_cast<const CClipEventTags::CBlockRagdollTag*>(m_MoveNetworkHelper.GetNetworkPlayer()->GetProperty(CClipEventTags::BlockRagdoll));
	if (pProp)
	{
		if (pProp->ShouldBlockFromThisVehicleMoving())
			return false;
	}

#if __DEV
	TUNE_GROUP_BOOL(BIKE_ENTER_TUNE, FORCE_RAGDOLL_WHEN_GETTING_ON, false);
	if (FORCE_RAGDOLL_WHEN_GETTING_ON)
		return true;
#endif // __DEV

	if (NetworkInterface::IsGameInProgress())
		return false;

	if (rVeh.IsUpsideDown())
		return true;

	if (!rVeh.InheritsFromBike())
		return false;

	// Need to check relative velocity in case ped is trying to get on the bike while on a train, for example...
	const float fBikePedRelativeVelocitySquared = rVeh.GetRelativeVelocity(rPed).Mag2();
	if (fBikePedRelativeVelocitySquared < rage::square(ms_Tunables.m_MinVelocityToRagdollPed))
		return false;

	// let the ped pick up the bike
	if (!rVeh.GetSeatManager()->GetDriver() && fBikePedRelativeVelocitySquared < rage::square(ms_Tunables.m_MaxVelocityToEnterBike))
		return false;

	return true;
}

////////////////////////////////////////////////////////////////////////////////

int CTaskEnterVehicleSeat::IntegerSort(const s32 *a, const s32 *b)
{
	if(*a < *b)
	{
		return -1;
	}
	else if(*a > *b)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

////////////////////////////////////////////////////////////////////////////////

float CTaskEnterVehicleSeat::GetMultipleEntryAnimClipRateModifier(CVehicle* pVeh, const CPed* pPed, bool bExitingVehicle)
{

	// Check if anyone else is using the same door behind us and speed up/slow them down locally
	const CSeatManager* pSeatManager = pVeh->GetSeatManager();
	const s32 nMaxSeats = pSeatManager ? pSeatManager->GetMaxSeats() : -1;
	float fMultipleEntryRateScale = 1.0f;
	atArray<s32> AlignStartTimes;

	CVehicleModelInfo* pVehModelInfo = pVeh->GetVehicleModelInfo();
	const CModelSeatInfo* pModelSeatInfo = pVehModelInfo ? pVehModelInfo->GetModelSeatInfo() : NULL;

	if (nMaxSeats > 0)
	{
		for (s32 iSeat=0; iSeat < nMaxSeats; iSeat++)
		{
			s32 iEntryPointIndex = pModelSeatInfo ? pModelSeatInfo->GetEntryPointIndexForSeat(iSeat, pVeh) : -1;
			const CVehicleEntryPointInfo* pEntryInfo = pVeh->IsEntryIndexValid(iEntryPointIndex) ? pVeh->GetEntryInfo(iEntryPointIndex) : NULL;
			if (pEntryInfo && (pEntryInfo->GetSPEntryAllowedAlso() || (!pEntryInfo->GetSPEntryAllowedAlso() && !pEntryInfo->GetMPWarpInOut()) || pVeh->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_USE_FULL_ANIMS_FOR_MP_WARP_ENTRY_POINTS)))
			{
				CPed* pPedEntering = CTaskVehicleFSM::GetPedInOrUsingSeat(*pVeh, iSeat);
				bool bIsPedAttached = pPedEntering ? pPedEntering->GetIsAttached() : false;
				if (pPedEntering && bIsPedAttached)
				{
					s32 iTimeStartedAlign = -1;
					if (bExitingVehicle)
					{
						CTaskExitVehicle* pExitVehTask = static_cast<CTaskExitVehicle*>(pPedEntering->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE));
						iTimeStartedAlign = pExitVehTask ? pExitVehTask->GetTimeStartedExitTask() : -1;
					}
					else
					{
						CTaskEnterVehicle* pEnterVehTask = static_cast<CTaskEnterVehicle*>(pPedEntering->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
						iTimeStartedAlign = pEnterVehTask ? pEnterVehTask->GetTimeStartedAlign() : -1;
					}

					if (iTimeStartedAlign > 0)
					{
						AlignStartTimes.PushAndGrow(iTimeStartedAlign);
					}
				}
			}
		}

		u32 iTotalPeds = AlignStartTimes.GetCount();
		TUNE_GROUP_FLOAT(VEHICLE_ENTRY_TUNE, fMultipleBusEntrySpeedOffset, 0.2f,0.0f,1.0f,0.1f);
		if (iTotalPeds > 1)
		{
			AlignStartTimes.QSort(0, -1, CTaskEnterVehicleSeat::IntegerSort);
			CTaskEnterVehicle* pEnterVehTask = static_cast<CTaskEnterVehicle*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
			CTaskExitVehicle* pExitVehTask = static_cast<CTaskExitVehicle*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE));
			float fTopRateOffset = fMultipleEntryRateScale + iTotalPeds / 2 * fMultipleBusEntrySpeedOffset;
			fTopRateOffset = Clamp(fTopRateOffset,0.6f,1.6f);

			for (u32 i=0; i<iTotalPeds; i++)
			{
				if ((!bExitingVehicle && pEnterVehTask && AlignStartTimes[i] == pEnterVehTask->GetTimeStartedAlign()) ||
					(bExitingVehicle && pExitVehTask  && AlignStartTimes[i] == pExitVehTask->GetTimeStartedExitTask()))
				{
					fMultipleEntryRateScale = fTopRateOffset - fMultipleBusEntrySpeedOffset * i;
					AI_LOG_WITH_ARGS("[EnterExitVehicle][SetClipPlaybackRate] Setting anim rate modifier (%.2f) for %s with start time of %i\n", fMultipleEntryRateScale, AILogging::GetDynamicEntityNameSafe(pPed), bExitingVehicle ? pExitVehTask->GetTimeStartedExitTask() : pEnterVehTask->GetTimeStartedAlign());
				}
			}
		}
		else if (bExitingVehicle && pVeh->GetNumberOfPassenger() > 1)
		{
			fMultipleEntryRateScale = fMultipleEntryRateScale + pVeh->GetNumberOfPassenger() / 2 * fMultipleBusEntrySpeedOffset;
			fMultipleEntryRateScale = Clamp(fMultipleEntryRateScale,0.6f,1.6f);
			AI_LOG_WITH_ARGS("[EnterExitVehicle][SetClipPlaybackRate] Setting anim rate modifier (%.2f) for %s because he's first to leave the vehicle while there's other passengers\n", fMultipleEntryRateScale, AILogging::GetDynamicEntityNameSafe(pPed));
		}
	}

	return fMultipleEntryRateScale;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskEnterVehicleSeat::SetClipPlaybackRate()
{
	const s32 iState = GetState();
	float fClipRate = 1.0f;
	const CAnimRateSet& rAnimRateSet = m_pVehicle->GetLayoutInfo()->GetAnimRateSet();

	// Otherwise determine if we should speed up the entry anims and which tuning values to use (anim vs non anim)
	const CPed& rPed = *GetPed();
	const bool bShouldUseCombatEntryRates = static_cast<CTaskEnterVehicle*>(GetParent())->IsFlagSet(CVehicleEnterExitFlags::CombatEntry) || static_cast<CTaskEnterVehicle*>(GetParent())->IsFlagSet(CVehicleEnterExitFlags::UseFastClipRate) || rPed.GetPedResetFlag(CPED_RESET_FLAG_UseFastEnterExitVehicleRates);
	bool bUseAnimCombatEntryRates = false;
	bool bUseNonAnimCombatEntryRates = false;

	if (bShouldUseCombatEntryRates)
	{
		if (iState == State_GetInCombat)
		{
			bUseAnimCombatEntryRates = true;
		}
		else
		{
			bUseNonAnimCombatEntryRates = true;
		}
	}

#if FPS_MODE_SUPPORTED
	TUNE_GROUP_FLOAT(FIRST_PERSON_VEHICLE_ENTRY_TUNE, COMBAT_ENTRY_RATE_SCALE, 0.60f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_VEHICLE_ENTRY_TUNE, NON_ANIM_COMBAT_ENTRY_RATE_SCALE, 0.60f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_VEHICLE_ENTRY_TUNE, NORMAL_ENTRY_RATE_SCALE, 0.60f, 0.0f, 2.0f, 0.01f);
	const bool bInFirstPersonMode = rPed.IsFirstPersonShooterModeEnabledForPlayer(false);
	const float fCombatEntryRateScale = bInFirstPersonMode ? COMBAT_ENTRY_RATE_SCALE : 1.0f;
	const float fNonAnimCombatEntryRateScale = bInFirstPersonMode ? NON_ANIM_COMBAT_ENTRY_RATE_SCALE : 1.0f;
	const float fNormalEntryRateScale = bInFirstPersonMode ? NORMAL_ENTRY_RATE_SCALE : 1.0f;
#else // FPS_MODE_SUPPORTED
	const float fCombatEntryRateScale = 1.0f;
	const float fNonAnimCombatEntryRateScale = 1.0f;
	const float fNormalEntryRateScale = 1.0f;
#endif // FPS_MODE_SUPPORTED

	float fFatPedEntryRateScale = 1.0f;

	const fwMvClipSetId clipSetId = rPed.GetPedModelInfo()->GetMovementClipSet();
	if(rPed.IsLocalPlayer() && CTaskHumanLocomotion::CheckClipSetForMoveFlag(clipSetId, CTaskEnterVehicleSeat::ms_IsFatId))
	{
		TUNE_GROUP_FLOAT(VEHICLE_ENTRY_TUNE, FAT_PED_SEAT_ENTRY_RATE_SCALE, 0.8f, 0.0f, 2.0f, 0.01f);
		fFatPedEntryRateScale = FAT_PED_SEAT_ENTRY_RATE_SCALE;
	}

	// Set the clip rate based on our conditions
	if (bUseAnimCombatEntryRates)
	{
		fClipRate = rAnimRateSet.m_AnimCombatEntry.GetRate() * fCombatEntryRateScale;
	}
	else if (bUseNonAnimCombatEntryRates)
	{
		fClipRate = rAnimRateSet.m_NoAnimCombatEntry.GetRate() * fNonAnimCombatEntryRateScale * fFatPedEntryRateScale;
	}
	else
	{
		fClipRate = rAnimRateSet.m_NormalEntry.GetRate() * fNormalEntryRateScale;
	}

	static u32 LAYOUT_BUS_HASH =  atStringHash("LAYOUT_BUS");
	static u32 LAYOUT_PRISON_BUS_HASH = atStringHash("LAYOUT_PRISON_BUS");
	static u32 LAYOUT_COACH_HASH = atStringHash("LAYOUT_COACH");
	static u32 LAYOUT_PLANE_LUXOR2 = atStringHash("LAYOUT_PLANE_LUXOR2");
	static u32 LAYOUT_PLANE_LUXOR = atStringHash("LAYOUT_PLANE_JET");
	static u32 LAYOUT_PLANE_NIMBUS = atStringHash("LAYOUT_PLANE_NIMBUS");
	static u32 LAYOUT_PLANE_BOMBUSHKA = atStringHash("LAYOUT_PLANE_BOMBUSHKA");
	const bool bIsBusLayout = m_pVehicle->GetLayoutInfo()->GetName() == LAYOUT_BUS_HASH;
	const bool bIsPrisonBusLayout = m_pVehicle->GetLayoutInfo()->GetName() == LAYOUT_PRISON_BUS_HASH;
	const bool bIsCoachLayout = m_pVehicle->GetLayoutInfo()->GetName() == LAYOUT_COACH_HASH;
	const bool bIsPassengerJet = m_pVehicle->GetLayoutInfo()->GetName() == LAYOUT_PLANE_LUXOR2 || m_pVehicle->GetLayoutInfo()->GetName() == LAYOUT_PLANE_LUXOR || 
								 m_pVehicle->GetLayoutInfo()->GetName() == LAYOUT_PLANE_NIMBUS || m_pVehicle->GetLayoutInfo()->GetName() == LAYOUT_PLANE_BOMBUSHKA;

	if (bIsBusLayout || bIsPrisonBusLayout || bIsCoachLayout || bIsPassengerJet)
		fClipRate *= CTaskEnterVehicleSeat::GetMultipleEntryAnimClipRateModifier(m_pVehicle, &rPed);

#if __BANK
	if (rPed.IsLocalPlayer())
	{
		aiDisplayf("Ped %s setting enter seat playback rate to %.2f in state %s", AILogging::GetDynamicEntityNameSafe(&rPed), fClipRate, GetStaticStateName(iState));
	}
#endif // __BANK
	m_MoveNetworkHelper.SetFloat(CTaskEnterVehicle::ms_GetInRateId, fClipRate);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicleSeat::SetClipsForState()
{
	const s32 iState = GetState();
	const fwMvClipSetId clipSetId = m_MoveNetworkHelper.GetClipSetId(iState == State_GetInFromSeatClipSet ? CTaskEnterVehicle::ms_SeatClipSetId : CLIP_SET_VAR_ID_DEFAULT);

	fwMvClipId clipId = ms_Tunables.m_DefaultGetInClipId; 

	switch (iState)
	{
		case State_QuickGetOn:			clipId = ms_Tunables.m_GetOnQuickClipId;		break;
		case State_GetInFromWater:		clipId = ms_Tunables.m_GetInFromWaterClipId;	break;
		case State_GetInFromStandOn:	clipId = ms_Tunables.m_GetInStandOnClipId;		break;
		case State_GetInCombat:			clipId = ms_Tunables.m_GetInCombatClipId;		break;
		default: break;
	}

	const crClip* pClip = fwClipSetManager::GetClip(clipSetId, clipId);
	if (!taskVerifyf(pClip, "Couldn't find clip %s from clipset %s", clipId.GetCStr(), clipSetId.GetCStr()))
	{
		return false;
	}

	m_fLegIkBlendOutPhase = 0.0f;

	if (iState == State_GetIn || iState == State_GetInCombat || iState == State_GetInFromStandOn || iState == State_QuickGetOn)
	{
		float fUnused;
		CClipEventTags::GetMoveEventStartAndEndPhases(pClip, ms_LegIkBlendOutId, m_fLegIkBlendOutPhase, fUnused);
	}

	m_MoveNetworkHelper.SetClip(pClip, ms_EnterSeatClipId);
	return true;
}

////////////////////////////////////////////////////////////////////////////////

const crClip* CTaskEnterVehicleSeat::GetClipAndPhaseForState(float& fPhase)
{
	switch (GetState())
	{
		case State_Start:
			return NULL;
		case State_GetIn: 
		case State_GetInFromSeatClipSet:
		case State_GetInFromStandOn:
		case State_GetInFromWater:
		case State_GetInCombat:
		case State_QuickGetOn:
		{
			fPhase = m_MoveNetworkHelper.GetFloat( ms_EnterSeatPhaseId);
			fPhase = rage::Clamp(fPhase, 0.0f, 1.0f);
			return m_MoveNetworkHelper.GetClip( ms_EnterSeatClipId);
		}
		case State_Finish: break;
		default: taskAssertf(0, "Unhandled State");
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskEnterVehicleSeat::IsMoveTransitionAvailable(s32 UNUSED_PARAM(iNextState)) const
{
	return !m_ForceMoveState;
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL

void CTaskEnterVehicleSeat::Debug() const
{
#if __BANK
	Matrix34 mPedMatrix;
	GetPed()->GetMatrixCopy(mPedMatrix);
	grcDebugDraw::Axis(mPedMatrix, 0.1f, true);
	
	if (m_pVehicle)
	{
		const CCarDoor* pDoor = GetDoorForEntryPointIndex(m_iTargetEntryPoint);
		if (pDoor)
		{
			grcDebugDraw::AddDebugOutput(Color_green, "Door Ratio : %.4f", pDoor->GetDoorRatio());
			grcDebugDraw::AddDebugOutput(Color_green, "Target Door Ratio : %.4f", pDoor->GetTargetDoorRatio());
		}
	}
#endif
}
#endif // !__FINAL

////////////////////////////////////////////////////////////////////////////////

const fwMvBooleanId CTaskInVehicleSeatShuffle::ms_ShuffleInterruptId("ShuffleInterrupt",0x3A199B8A);
const fwMvBooleanId CTaskInVehicleSeatShuffle::ms_SeatShuffleClipFinishedId("SeatShuffleAnimFinished",0xD09C405);
const fwMvBooleanId CTaskInVehicleSeatShuffle::ms_ShuffleOnEnterId("Shuffle_OnEnter",0x57B1DF3E);
const fwMvBooleanId CTaskInVehicleSeatShuffle::ms_ShuffleToEmptySeatOnEnterId("ShuffleToEmptySeat_OnEnter",0x6B2FA76A);
const fwMvBooleanId CTaskInVehicleSeatShuffle::ms_ShuffleAndJackPedOnEnterId("ShuffleAndJackPed_OnEnter",0x5A710E21);
const fwMvBooleanId CTaskInVehicleSeatShuffle::ms_ShuffleCustomClipOnEnterId("ShuffleCustomClip_OnEnter",0x23686d54);
const fwMvClipId CTaskInVehicleSeatShuffle::ms_SeatShuffleClipId("SeatShuffleClip",0x63D681F1);
const fwMvFlagId CTaskInVehicleSeatShuffle::ms_FromInsideId("FromInside",0xB75486AB);
const fwMvFlagId CTaskInVehicleSeatShuffle::ms_IsDeadId("IsDead",0xE2625733);
const fwMvFloatId CTaskInVehicleSeatShuffle::ms_ShuffleRateId("ShuffleRate",0x7D6A0A3A);
const fwMvFloatId CTaskInVehicleSeatShuffle::ms_SeatShufflePhaseId("SeatShufflePhase",0xC1216D5C);
const fwMvRequestId CTaskInVehicleSeatShuffle::ms_SeatShuffleRequestId("SeatShuffle",0x1AAEFB35);
const fwMvRequestId CTaskInVehicleSeatShuffle::ms_ShuffleToEmptySeatRequestId("ShuffleToEmptySeat",0xEF283A6C);
const fwMvRequestId CTaskInVehicleSeatShuffle::ms_ShuffleAndJackPedRequestId("ShuffleAndJackPed",0xB113ED2F);
const fwMvRequestId CTaskInVehicleSeatShuffle::ms_ShuffleCustomClipRequestId("ShuffleCustomClip",0x8de7700c);
const fwMvStateId CTaskInVehicleSeatShuffle::ms_ShuffleStateId("Shuffle",0x29315D2B);
const fwMvStateId CTaskInVehicleSeatShuffle::ms_ShuffleToEmptySeatStateId("ShuffleToEmptySeat",0xEF283A6C);
const fwMvStateId CTaskInVehicleSeatShuffle::ms_ShuffleAndJackPedStateId("ShuffleAndJackPed",0xB113ED2F);
const fwMvClipId CTaskInVehicleSeatShuffle::ms_Clip0Id("Clip0",0xf416c5e4);

////////////////////////////////////////////////////////////////////////////////

bool CTaskInVehicleSeatShuffle::IsTaskValid(const CPed* pPed, const CVehicle* pVehicle)
{
	if (!pVehicle || !pPed || !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle)
		|| !taskVerifyf(pVehicle == pPed->GetMyVehicle(), "Ped has been told to shuffle in a vehicle they are not in")
		|| !taskVerifyf(pPed->GetIsAttached(), "Ped was not attached"))
	{
		return false;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

CTaskInVehicleSeatShuffle::CTaskInVehicleSeatShuffle(CVehicle* pVehicle, CMoveNetworkHelper* pParentNetwork, bool bJackAnyOne, s32 iTargetSeatIndex)
: m_pVehicle(pVehicle)
, m_pJackedPed(NULL)
, m_iCurrentSeatIndex(-1)
, m_iTargetSeatIndex(iTargetSeatIndex)
, m_pMoveNetworkHelper(pParentNetwork)
, m_iWaitForPedTimer(0)
, m_bPedEnterExit(false)
, m_bWaitForSeatToBeEmpty(false)
, m_bWaitForSeatToBeOccupied(false)
, m_bMoveNetworkFinished(false)
, m_bMoveShuffleInterrupt(false)
, m_bJackAnyOne(bJackAnyOne)
{
	SetInternalTaskType(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE);
}

////////////////////////////////////////////////////////////////////////////////

CTaskInVehicleSeatShuffle::~CTaskInVehicleSeatShuffle()
{

}

////////////////////////////////////////////////////////////////////////////////

bool CTaskInVehicleSeatShuffle::ShouldAbort(const AbortPriority UNUSED_PARAM(priority), const aiEvent* pEvent)
{
	if (pEvent && pEvent->GetEventType() == EVENT_DAMAGE)
	{
		return false;
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskInVehicleSeatShuffle::CleanUp()
{
	if (m_MoveNetworkHelper.IsNetworkActive())
	{
		CPed* pPed = GetPed();
		ASSERT_ONLY(taskDebugf2("Frame : %u - %s%s stopped shuffle Move Network : CTaskInVehicleSeatShuffle::CleanUp()", fwTimer::GetFrameCount(), pPed->IsNetworkClone() ? "Clone ped : " : "Local ped : ", pPed->GetDebugName()));
		pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_InVehicle);
		TUNE_GROUP_FLOAT(SHUFFLE_TUNE, VEHICLE_HANDLE_BAR_BLENDOUT_DURATION, 0.2f, 0.0f, 1.0f, 0.01f);
		float fBlendOutDuration = m_pVehicle && CTaskMotionInVehicle::VehicleHasHandleBars(*m_pVehicle) ? VEHICLE_HANDLE_BAR_BLENDOUT_DURATION : FAST_BLEND_DURATION;
		pPed->GetMovePed().ClearTaskNetwork( m_MoveNetworkHelper, CTaskMotionInAutomobile::WantsToDuck(*pPed) ? CTaskEnterVehicle::ms_Tunables.m_DuckShuffleBlendDuration : fBlendOutDuration);
	}
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskInVehicleSeatShuffle::ProcessPreFSM()
{
	CPed* pPed = GetPed();

	// Check if we should quit out of the task
	if (!IsTaskValid(pPed, m_pVehicle))
	{
		return FSM_Quit;
	}

	// Force the do nothing motion state
	if (ShouldMotionStateBeForcedToDoNothing() && !CTaskMotionInAutomobile::WantsToDuck(*pPed))
	{
		pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_DoNothing);
	}

	// If this is a clone ped jacking a local player, we need to make sure they know when to get jacked
    EnsureLocallyJackedPedsExitVehicle();

    // peds shuffling inside a vehicle must be synced in the network game, but peds in cars are not
    // synchronised by default, in order to save bandwidth as they don't do much
    if(pPed->GetNetworkObject() && !pPed->IsNetworkClone())
    {
        NetworkInterface::ForceSynchronisation(pPed);
    }

	// Try to stream the invehicle clipset for the target shuffle seat
	if (GetState() > State_Start)
	{
		fwMvClipSetId inVehicleClipsetId = CTaskMotionInAutomobile::GetInVehicleClipsetId(*pPed, m_pVehicle, m_iTargetSeatIndex);
		CTaskVehicleFSM::RequestVehicleClipSet(inVehicleClipsetId, SP_High);
	}

	if (pPed->IsLocalPlayer() && MI_CAR_APC.IsValid() && m_pVehicle->GetModelIndex() == MI_CAR_APC)
		camInterface::GetCinematicDirector().DisableFirstPersonInVehicleThisUpdate();

	// Need to set this to get the processphysics call to update the ped attachment
	pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_IsSeatShuffling, true );
	return FSM_Continue;
}	

////////////////////////////////////////////////////////////////////////////////

void CTaskInVehicleSeatShuffle::EnsureLocallyJackedPedsExitVehicle()
{
#if __BANK
    if(!m_cloneTaskInScope)
    {
        BANK_ONLY(taskDebugf2("Frame : %u - %s%s : 0x%p in %s:%s : 0x%p EnsureLocallyJackedPedsExitVehicle() - running for out of scope clone task!", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone ped : " : "Local ped : ", GetPed()->GetDebugName(), GetPed(), GetTaskName(), GetStateName(GetState()), this));
    }
#endif // __BANK

	if (!m_pVehicle)
	{
		AI_LOG_WITH_ARGS("Ped %s earlying out from CTaskInVehicleSeatShuffle::EnsureLocallyJackedPedsExitVehicle as m_pVehicle is NULL", AILogging::GetDynamicEntityNameSafe(GetPed()));
		return;
	}

    CPed* pPed = GetPed();
    if(pPed->IsNetworkClone() && m_iTargetSeatIndex != -1)
    {
        s32 stateToCheck = GetStateFromNetwork();

        if(m_cloneTaskInScope && GetState() != State_Start)
        {
            stateToCheck = GetState();
        }

#if __BANK
        if(!m_cloneTaskInScope)
        {
            BANK_ONLY(taskDebugf2("Frame : %u - %s%s : 0x%p in %s:%s : 0x%p EnsureLocallyJackedPedsExitVehicle() - Checked state %s", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone ped : " : "Local ped : ", GetPed()->GetDebugName(), GetPed(), GetTaskName(), GetStateName(GetState()), this, GetStateName(stateToCheck)));
        }
#endif // __BANK

        switch(stateToCheck)
        {
        case State_JackPed:
        case State_WaitForPedToLeaveSeat:
            {
                CPed *pPedInTargetSeat = m_pVehicle->GetSeatManager()->GetPedInSeat(m_iTargetSeatIndex);

#if __BANK
                if(!m_cloneTaskInScope)
                {
                    BANK_ONLY(taskDebugf2("Frame : %u - %s%s : 0x%p in %s:%s : 0x%p EnsureLocallyJackedPedsExitVehicle() - Ped in target seat %s", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone ped : " : "Local ped : ", GetPed()->GetDebugName(), GetPed(), GetTaskName(), GetStateName(GetState()), this, pPedInTargetSeat ? pPedInTargetSeat->GetDebugName() : "None"));
                }
#endif // __BANK

                if(pPedInTargetSeat)
                {
                    if(pPedInTargetSeat != pPed &&
                       !pPedInTargetSeat->IsNetworkClone() &&
                       !pPedInTargetSeat->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE))
                    {
                        if(CTaskVehicleFSM::CanJackPed(pPed, pPedInTargetSeat))
                        {
						    s32 iDirectEntryPointIndex = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(m_iTargetSeatIndex, m_pVehicle);
						    if (taskVerifyf(m_pVehicle->IsEntryPointIndexValid(iDirectEntryPointIndex), "Entry index was invalid %u", iDirectEntryPointIndex))
						    {
							    VehicleEnterExitFlags vehicleFlags;
							    vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::BeJacked);
							    vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::JackedFromInside);
							    const s32 iEventPriority = CTaskEnterVehicle::GetEventPriorityForJackedPed(*pPedInTargetSeat);
							    CEventGivePedTask givePedTask(PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP, rage_new CTaskExitVehicle(m_pVehicle, vehicleFlags, 0.0f, pPed, iDirectEntryPointIndex), true, iEventPriority );
							    pPedInTargetSeat->GetPedIntelligence()->AddEvent(givePedTask);
#if __BANK
							    CGivePedJackedEventDebugInfo instance(pPedInTargetSeat, this, __FUNCTION__);
							    instance.Print();
#endif // __BANK
						    }
                        }
                    }
                    else
                    {
#if __BANK
                        if(!m_cloneTaskInScope)
                        {
                            BANK_ONLY(taskDebugf2("Frame : %u - %s%s : 0x%p in %s:%s : 0x%p EnsureLocallyJackedPedsExitVehicle() - Can't jack ped in target seat: same ped:%s, is clone:%s, running enter vehicle task:%s",
                                fwTimer::GetFrameCount(),
                                GetPed()->IsNetworkClone() ? "Clone ped : " : "Local ped : ",
                                GetPed()->GetDebugName(),
                                GetPed(),
                                GetTaskName(),
                                GetStateName(GetState()),
                                this,
                                (pPedInTargetSeat == pPed) ? "true" : "false",
                                pPedInTargetSeat->IsNetworkClone() ? "true" : "false",
                                pPedInTargetSeat->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE)  ? "true" : "false"));
                        }
#endif // __BANK
                    }
                }
            }
            break;
        default:
            break;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskInVehicleSeatShuffle::ShouldMotionStateBeForcedToDoNothing()
{
	switch(GetState())
	{
	case State_Start:
	case State_StreamAssets:
	case State_ReserveShuffleSeat:
	case State_WaitForNetwork:
	case State_WaitForInsert:
	case State_Shuffle:
	case State_SetPedInSeat:
	case State_Finish:
		return false;
	default:
		return true;
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskInVehicleSeatShuffle::SetClipPlaybackRate()
{
	if (!m_pMoveNetworkHelper)
		return;

	float fClipRate = 1.0f;
	const CAnimRateSet& rAnimRateSet = m_pVehicle->GetLayoutInfo()->GetAnimRateSet();

	// Otherwise determine if we should speed up the entry anims and which tuning values to use (anim vs non anim)
	const CPed& rPed = *GetPed();
	const bool bShouldUseCombatEntryRates = GetParent() && (static_cast<CTaskEnterVehicle*>(GetParent())->IsFlagSet(CVehicleEnterExitFlags::CombatEntry) || static_cast<CTaskEnterVehicle*>(GetParent())->IsFlagSet(CVehicleEnterExitFlags::UseFastClipRate) || rPed.GetPedResetFlag(CPED_RESET_FLAG_UseFastEnterExitVehicleRates));

	// Set the clip rate based on our conditions
	if (bShouldUseCombatEntryRates)
	{
		fClipRate = rAnimRateSet.m_NoAnimCombatEntry.GetRate();
	}
	else
	{
		fClipRate = rAnimRateSet.m_NormalEntry.GetRate();
	}

	m_pMoveNetworkHelper->SetFloat(ms_ShuffleRateId, fClipRate);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskInVehicleSeatShuffle::SetClipsForState()
{
	fwMvClipId clipId = CLIP_ID_INVALID;
	const crClip* pClip = NULL;
	const s32 iState = GetState();
	switch (iState)
	{
	case State_Shuffle:
		{
			const bool bFromTurretSeat = m_pVehicle->IsTurretSeat(m_iCurrentSeatIndex);
			const bool bToTurretSeat = m_pVehicle->IsTurretSeat(m_iTargetSeatIndex);
			const bool bDefaultToSecondClip = (bFromTurretSeat && !bToTurretSeat);
			const bool bHas180Clips = m_pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_DIRECTIONAL_SHUFFLES);
			const CTurret* pTurret = NULL;
			if (bHas180Clips && m_pVehicle->GetVehicleWeaponMgr())
			{
				pTurret = bFromTurretSeat ? m_pVehicle->GetVehicleWeaponMgr()->GetFirstTurretForSeat(m_iCurrentSeatIndex) : m_pVehicle->GetVehicleWeaponMgr()->GetFirstTurretForSeat(m_iTargetSeatIndex);
			}

			clipId = bDefaultToSecondClip ? GetShuffleClip2(pTurret) : GetShuffleClip1(pTurret);

			// Detect if this is the second shuffle link, if so use the second shuffle clip
			const CVehicleSeatInfo* pCurrentSeatInfo = m_pVehicle->GetSeatInfo(m_iCurrentSeatIndex);
			const CVehicleSeatInfo* pTargetSeatInfo = m_pVehicle->GetSeatInfo(m_iTargetSeatIndex);
			if (pCurrentSeatInfo->GetShuffleLink2() == pTargetSeatInfo)
			{
				clipId = bDefaultToSecondClip ? GetShuffleClip1(pTurret) : GetShuffleClip2(pTurret);
			}
			break;
		}
	case State_JackPed:
		{
			if (!m_pVehicle->IsTurretSeat(m_iTargetSeatIndex))
			{
				// Don't set custom clip if shuffle jacking a non turret seat, use the old anim names
				return;
			}
			const bool bHas180Clips = m_pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_DIRECTIONAL_SHUFFLES);
			const CTurret* pTurret = NULL;
			if (bHas180Clips && m_pVehicle->GetVehicleWeaponMgr())
			{
				pTurret = m_pVehicle->GetVehicleWeaponMgr()->GetFirstTurretForSeat(m_iTargetSeatIndex);
			}

			clipId = (m_pJackedPed && m_pJackedPed->ShouldBeDead()) ? GetShuffleDeadJackClip(pTurret) : GetShuffleJackClip(pTurret);			
			break;
		}
		default: taskAssertf(0, "Unhandled state %s", GetStaticStateName(iState));
		break;
	}

	const fwMvClipSetId clipSetId = m_pMoveNetworkHelper->GetClipSetId();
	if (taskVerifyf(clipId != CLIP_ID_INVALID, "Clip Id was invalid in state %s", GetStaticStateName(iState)) && 
		taskVerifyf(clipSetId != CLIP_SET_ID_INVALID, "ClipSet Id was invalid in state %s", GetStaticStateName(iState)))
	{
		pClip = fwClipSetManager::GetClip(clipSetId, clipId);
	}

	taskAssertf(pClip, "Couldn't find clip %s in clipset %s", clipId.GetCStr(), clipSetId.GetCStr());
	m_pMoveNetworkHelper->SendRequest(ms_ShuffleCustomClipRequestId);
	m_pMoveNetworkHelper->WaitForTargetState(ms_ShuffleCustomClipOnEnterId);
	m_pMoveNetworkHelper->SetClip(pClip, ms_Clip0Id);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskInVehicleSeatShuffle::ShouldUse180Clip(const CTurret* pTurret) const
{
	if (!m_pVehicle)
		return false;

	if (!pTurret)
		return false;

	const float fTurretHeading = pTurret->GetCurrentHeading(m_pVehicle);
	const float fVehicleHeading = m_pVehicle->GetTransform().GetHeading(); // Need to change this if seats aren't pointing in the same direction as the vehicle
	const float fDelta = fwAngle::LimitRadianAngle(fTurretHeading - fVehicleHeading);
	TUNE_GROUP_FLOAT(TURRET_TUNE, HEADING_DELTA_TO_USE_180_CLIPS, HALF_PI, 0, TWO_PI, 0.01f);
	if (Abs(fDelta) > HEADING_DELTA_TO_USE_180_CLIPS)
	{
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipId CTaskInVehicleSeatShuffle::GetShuffleClip1(const CTurret* pTurret) const
{
	if (!pTurret)
		return CTaskEnterVehicle::ms_Tunables.m_ShuffleClipId;

	// Determine the direction of the turret relative to our seat orientation
	bool bShouldUse180Clip = ShouldUse180Clip(pTurret);
	return bShouldUse180Clip ? CTaskEnterVehicle::ms_Tunables.m_Shuffle_180ClipId : CTaskEnterVehicle::ms_Tunables.m_ShuffleClipId;
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipId CTaskInVehicleSeatShuffle::GetShuffleClip2(const CTurret* pTurret) const
{
	if (!pTurret)
		return CTaskEnterVehicle::ms_Tunables.m_Shuffle2ClipId;

	// Determine the direction of the turret relative to our seat orientation
	bool bShouldUse180Clip = ShouldUse180Clip(pTurret);
	return bShouldUse180Clip ? CTaskEnterVehicle::ms_Tunables.m_Shuffle2_180ClipId : CTaskEnterVehicle::ms_Tunables.m_Shuffle2ClipId;
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipId CTaskInVehicleSeatShuffle::GetShuffleJackClip(const CTurret* pTurret) const
{
	if (!pTurret)
		return CTaskEnterVehicle::ms_Tunables.m_CustomShuffleJackClipId;

	// Determine the direction of the turret relative to our seat orientation
	bool bShouldUse180Clip = ShouldUse180Clip(pTurret);
	return bShouldUse180Clip ?  CTaskEnterVehicle::ms_Tunables.m_CustomShuffleJack_180ClipId : CTaskEnterVehicle::ms_Tunables.m_CustomShuffleJackClipId;
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipId CTaskInVehicleSeatShuffle::GetShuffleDeadJackClip(const CTurret* pTurret) const
{
	if (!pTurret)
		return CTaskEnterVehicle::ms_Tunables.m_CustomShuffleJackDeadClipId;

	// Determine the direction of the turret relative to our seat orientation
	bool bShouldUse180Clip = ShouldUse180Clip(pTurret);
	return bShouldUse180Clip ? CTaskEnterVehicle::ms_Tunables.m_CustomShuffleJackDead_180ClipId : CTaskEnterVehicle::ms_Tunables.m_CustomShuffleJackDeadClipId;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskInVehicleSeatShuffle::KeepComponentReservation()
{
	CPed* pPed = GetPed();

	if(!pPed->IsNetworkClone())
	{
		// If the task should currently have the shuffle seat reserved, check that it does
		if (GetState() > State_ReserveShuffleSeat && GetState() < State_UnReserveShuffleSeat)
		{
			CComponentReservation* pComponentReservation = m_pVehicle->GetComponentReservationMgr()->GetSeatReservationFromSeat(m_iTargetSeatIndex);
			if (pComponentReservation)
			{
#if __DEV
				taskDebugf2("Frame : %u - %s%s : 0x%p : Ped Trying To Still Use Component With Reservation 0x%p : Bone Index %i", fwTimer::GetFrameCount(), pPed->IsNetworkClone() ? "Clone ped : " : "Local ped : ", pPed->GetDebugName(), pPed, pComponentReservation, pComponentReservation->GetBoneIndex());
				taskDebugf2("Ped %s is in seat %i", CTaskVehicleFSM::GetPedInOrUsingSeat(*m_pVehicle, m_iTargetSeatIndex) ? CTaskVehicleFSM::GetPedInOrUsingSeat(*m_pVehicle, m_iTargetSeatIndex)->GetDebugName() : "NULL", m_iTargetSeatIndex);
#endif
				pComponentReservation->SetPedStillUsingComponent(pPed);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CTaskInVehicleSeatShuffle::GetShuffleClipsetId(const CVehicle* pVehicle, s32 iSeatIndex)
{
	const CVehicleSeatAnimInfo* pClipInfo = pVehicle->GetSeatAnimationInfo(iSeatIndex);
	if (pVehicle->InheritsFromBike() || pVehicle->GetIsJetSki() || (pVehicle->InheritsFromHeli() && pVehicle->IsTurretSeat(iSeatIndex)) || pVehicle->InheritsFromQuadBike())
	{
		// The shuffle anims are in the seat clipset for bikes
		taskFatalAssertf(pClipInfo, "NULL Seat clip Info for seat index %i", iSeatIndex);
		return pClipInfo->GetSeatClipSetId();
	}
	else
	{
		const s32 iEntryPointIndex = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iSeatIndex, pVehicle);
		const CVehicleEntryPointAnimInfo* pClipInfo = pVehicle->GetEntryAnimInfo(iEntryPointIndex);
		taskFatalAssertf(pClipInfo, "NULL Entry clip Info for entry index %i", iEntryPointIndex);
		return pClipInfo->GetEntryPointClipSetId();
	}
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskInVehicleSeatShuffle::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();	

		FSM_State(State_StreamAssets)
			FSM_OnUpdate
				return StreamAssets_OnUpdate();

		FSM_State(State_ReserveShuffleSeat)
			FSM_OnUpdate
				return ReserveShuffleSeat_OnUpdate();

		FSM_State(State_WaitForInsert)
			FSM_OnEnter
				WaitForInsert_OnEnter();
			FSM_OnUpdate
				return WaitForInsert_OnUpdate();

        FSM_State(State_WaitForNetwork)
			FSM_OnUpdate
				return WaitForNetwork_OnUpdate();

		FSM_State(State_Shuffle)
			FSM_OnEnter
				Shuffle_OnEnter();
			FSM_OnUpdate
				return Shuffle_OnUpdate();

		FSM_State(State_JackPed)
			FSM_OnEnter
				JackPed_OnEnter();
			FSM_OnUpdate
				return JackPed_OnUpdate();

		FSM_State(State_UnReserveShuffleSeat)
			FSM_OnUpdate
				return UnReserveShuffleSeat_OnUpdate();

        FSM_State(State_WaitForPedToLeaveSeat)
            FSM_OnEnter
                WaitForPedToLeaveSeat_OnEnter();
            FSM_OnUpdate
                return WaitForPedToLeaveSeat_OnUpdate();

		FSM_State(State_SetPedInSeat)
			FSM_OnEnter
				SetPedInSeat_OnEnter();
			FSM_OnUpdate
				return SetPedInSeat_OnUpdate();

		FSM_State(State_Finish)
            FSM_OnEnter
				Finish_OnEnter();
			FSM_OnUpdate
				return Finish_OnUpdate();

	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskInVehicleSeatShuffle::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
    return UpdateFSM(iState, iEvent);
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskInVehicleSeatShuffle::ProcessPostFSM()
{
	KeepComponentReservation();

    if(m_pMoveNetworkHelper && m_pMoveNetworkHelper->IsNetworkActive())
    {
	    const bool bFromInside = (m_pVehicle->GetSeatManager()->GetPedsSeatIndex(GetPed()) > -1) ? true : false;
	    m_pMoveNetworkHelper->SetFlag( bFromInside, ms_FromInsideId);
	    const bool bIsDead = m_pJackedPed ? m_pJackedPed->IsInjured() : false;
	    m_pMoveNetworkHelper->SetFlag( bIsDead, ms_IsDeadId);
    }

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskInVehicleSeatShuffle::IsInScope(const CPed* pPed)
{
	bool isInScope = CTaskFSMClone::IsInScope(pPed);

	if(!isInScope)
	{
		// even if the clone task is out of scope we need to make sure any locally
		// controlled peds exit the vehicle if they are being jacked
		EnsureLocallyJackedPedsExitVehicle();
	}

	return isInScope;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskInVehicleSeatShuffle::ProcessPhysics(float fTimeStep, int nTimeSlice)
{
	if (m_pVehicle && GetState() > State_Start && GetState() < State_SetPedInSeat && GetPed()->GetIsAttached())
	{
		// Update the target situation
		Vector3 vNewTargetPosition(Vector3::ZeroType);
		Quaternion qNewTargetOrientation(0.0f,0.0f,0.0f,1.0f);
		CModelSeatInfo::CalculateSeatSituation(m_pVehicle, vNewTargetPosition, qNewTargetOrientation, m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(m_iTargetSeatIndex, m_pVehicle));

		// Set the target situation on the helper
		m_PlayAttachedClipHelper.SetTarget(vNewTargetPosition, qNewTargetOrientation);

		// Compute initial situation (We update the clip relative to this)
		Vector3 vNewPedPosition(Vector3::ZeroType);
		Quaternion qNewPedOrientation(0.0f,0.0f,0.0f,1.0f);
		CModelSeatInfo::CalculateSeatSituation(m_pVehicle, vNewPedPosition, qNewPedOrientation,m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(m_iCurrentSeatIndex, m_pVehicle));

		if (m_pMoveNetworkHelper && m_pMoveNetworkHelper->IsNetworkActive())
		{
			float fClipPhase = 0.0f;
			const crClip* pClip = GetClipAndPhaseForState(fClipPhase);
			if (pClip)
			{
				// Update the situation from the current phase in the clip
				m_PlayAttachedClipHelper.ComputeSituationFromClip(pClip, 0.0f, fClipPhase, vNewPedPosition, qNewPedOrientation);

				// Compute the offsets we'll need to fix up
				Vector3 vOffsetPosition(Vector3::ZeroType);
				Quaternion qOffsetOrientation(0.0f,0.0f,0.0f,1.0f);
				m_PlayAttachedClipHelper.ComputeOffsets(pClip, fClipPhase, 1.0f, vNewPedPosition, qNewPedOrientation, vOffsetPosition, qOffsetOrientation);

				// Apply the fixup
				m_PlayAttachedClipHelper.ApplyVehicleShuffleOffsets(vNewPedPosition, qNewPedOrientation, pClip, vOffsetPosition, qOffsetOrientation, fClipPhase);
			}
		}

		qNewPedOrientation.Normalize();

		CTaskVehicleFSM::UpdatePedVehicleAttachment(GetPed(), VECTOR3_TO_VEC3V(vNewPedPosition), QUATERNION_TO_QUATV(qNewPedOrientation));
	}

	// Base class
	return CTask::ProcessPhysics(fTimeStep, nTimeSlice);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskInVehicleSeatShuffle::ProcessMoveSignals()
{
	if(!CPedAILodManager::ShouldDoFullUpdate(*GetPed()))
	{
		KeepComponentReservation();
	}

	if(GetState() == State_Shuffle)
	{
		if(m_pMoveNetworkHelper && m_pMoveNetworkHelper->IsNetworkActive())
		{
			m_bMoveShuffleInterrupt |= m_pMoveNetworkHelper->GetBoolean(ms_ShuffleInterruptId);
		}

		// Check if done running the shuffle Move state.
		if (!m_bMoveNetworkFinished && IsMoveNetworkStateFinished(ms_SeatShuffleClipFinishedId, ms_SeatShufflePhaseId))
		{
			m_bMoveNetworkFinished = true;
		}
	}
	else if (GetState() == State_JackPed)
	{
        if(m_pVehicle)
        {
		    m_pVehicle->m_nVehicleFlags.bForceOtherVehiclesToStopForThisVehicle = true;
        }
	}

	//We have things that need to be done every frame, so never turn this callback off.
	return true;
}

////////////////////////////////////////////////////////////////////////////////

CTaskInfo* CTaskInVehicleSeatShuffle::CreateQueriableState() const
{
    bool enteringVehicle = GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_ENTER_VEHICLE;
	return rage_new CClonedSeatShuffleInfo(GetState(), m_pVehicle, enteringVehicle, m_iTargetSeatIndex);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskInVehicleSeatShuffle::ReadQueriableState( CClonedFSMTaskInfo* pTaskInfo )
{
	Assert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_SHUFFLE_SEAT_CLONED );

    m_bPedEnterExit = static_cast<CClonedSeatShuffleInfo *>(pTaskInfo)->IsPedEnterExit();
	m_iTargetSeatIndex = static_cast<CClonedSeatShuffleInfo *>(pTaskInfo)->GetTargetSeatIndex();

	// Call the base implementation - syncs the state
	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskInVehicleSeatShuffle::CanShuffleSeat(const CVehicle* pVehicle, s32 iSeatIndex, s32 iEntryPoint, s32* piShuffleSeatIndex, s32* piShuffleSeatEntryIndex, bool bOnlyUseSecondShuffleLink)
{
	if (!pVehicle->IsSeatIndexValid(iSeatIndex))
		return false;

	// Some seats don't have any entry points (rear seats of low vehicles for instance, bail out if we get tasked to shuffle)
	if (!pVehicle->IsEntryIndexValid(iEntryPoint))
		return false;

	const CModelSeatInfo& rModelInfo = *pVehicle->GetVehicleModelInfo()->GetModelSeatInfo();

	// Check first shuffle link
	s32 iTargetSeatIndex = bOnlyUseSecondShuffleLink ? -1 : rModelInfo.GetShuffleSeatForSeat(iEntryPoint, iSeatIndex, false);
	if (pVehicle->IsSeatIndexValid(iTargetSeatIndex))
	{
		if (CanShuffleToTargetSeat(rModelInfo, iTargetSeatIndex, pVehicle, piShuffleSeatIndex, piShuffleSeatEntryIndex))
		{
			return true;
		}
	}

	// Check second shuffle link
	iTargetSeatIndex = rModelInfo.GetShuffleSeatForSeat(iEntryPoint, iSeatIndex, true);
	if (pVehicle->IsSeatIndexValid(iTargetSeatIndex))
	{
		if (CanShuffleToTargetSeat(rModelInfo, iTargetSeatIndex, pVehicle, piShuffleSeatIndex, piShuffleSeatEntryIndex))
		{
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskInVehicleSeatShuffle::CanShuffleToTargetSeat(const CModelSeatInfo &rModelInfo, s32 iTargetSeatIndex, const CVehicle* pVehicle, s32* &piShuffleSeatIndex, s32* &piShuffleSeatEntryIndex)
{
	const s32 iTargetEntryPointIndex = rModelInfo.GetEntryPointIndexForSeat(iTargetSeatIndex, pVehicle);
	// Prevent shuffling to a seat which doesn't have a direct access entrypoint (crashes when computing the seat situation, probably not needed anyway)
	if (!pVehicle->IsEntryIndexValid(iTargetEntryPointIndex))
		return false;

	if (piShuffleSeatIndex)
		*piShuffleSeatIndex = iTargetSeatIndex;

	if (piShuffleSeatEntryIndex)
		*piShuffleSeatEntryIndex = iTargetEntryPointIndex;

	CPed* pJackedPed = pVehicle->GetSeatManager()->GetPedInSeat(iTargetSeatIndex);

	const CVehicleSeatAnimInfo* pTargetSeatClipInfo = pVehicle->GetSeatAnimationInfo(iTargetSeatIndex);
	taskFatalAssertf(pTargetSeatClipInfo, "NULL Seat clip Info for seat index %i", iTargetSeatIndex);
	if (pJackedPed && pTargetSeatClipInfo->GetPreventShuffleJack())
		return false;

	if (pJackedPed && !pJackedPed->ShouldBeDead() && pTargetSeatClipInfo->IsTurretSeat())
		return false; 

	return true;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskInVehicleSeatShuffle::Start_OnUpdate()
{
	CPed* pPed = GetPed();
	m_iCurrentSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
	const s32 iEntryPointIndex = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(m_iCurrentSeatIndex, m_pVehicle);

	if (!CanShuffleSeat(m_pVehicle, m_iCurrentSeatIndex, iEntryPointIndex))
	{
		SetTaskState(State_Finish);
		return FSM_Continue;
	}
	
	// If we're going to use the alternate shuffle seat, we assume this is passed in
	if (m_iTargetSeatIndex == -1)
	{
		m_iTargetSeatIndex = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetShuffleSeatForSeat(iEntryPointIndex, m_iCurrentSeatIndex);
	}

	const fwMvClipSetId shuffleClipsetId = GetShuffleClipsetId(m_pVehicle, m_iCurrentSeatIndex);
	if (m_pMoveNetworkHelper && m_pMoveNetworkHelper->IsNetworkActive() && shuffleClipsetId == m_pMoveNetworkHelper->GetClipSetId())
	{
		// This task is being run as part of the enter/exit task
		m_bPedEnterExit = true;
	}

	m_pJackedPed = m_pVehicle->GetSeatManager()->GetPedInSeat(m_iTargetSeatIndex);

	if(pPed->IsNetworkClone())
    {
        if(!pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE))
        {
            if(!pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE))
            {
                SetTaskState(State_Finish);
		        return FSM_Continue;
            }
            else
            {
                // wait for the shuffle task to start on the parent machine, if we are running ahead this
                // can lead to glitches
                return FSM_Continue;
            }
        }
    }

	if (m_iCurrentSeatIndex != -1 && m_pVehicle->IsTurretSeat(m_iCurrentSeatIndex))
	{
		CTaskVehicleFSM::PointTurretInNeturalPosition(*m_pVehicle, m_iCurrentSeatIndex);
	}

	RequestProcessMoveSignalCalls();

	SetTaskState(State_StreamAssets);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskInVehicleSeatShuffle::StreamAssets_OnUpdate()
{
	const float fTimeBeforeWarp = CTaskEnterVehicle::ms_Tunables.m_MaxTimeStreamShuffleClipSetInBeforeWarp;

	if (taskVerifyf(m_pVehicle->IsSeatIndexValid(m_iCurrentSeatIndex), "Seat index (%i) for vehicle (%s) was invalid", m_iCurrentSeatIndex, m_pVehicle->GetModelName() ? m_pVehicle->GetModelName() : "NULL"))
	{
        CPed* pPed = GetPed();

		// Stream in the clipset containing the shuffle anim for this seat, the helper handles 
		// add refing the clipset so it won't get streamed out
		const fwMvClipSetId shuffleClipsetId = GetShuffleClipsetId(m_pVehicle, m_iCurrentSeatIndex);
		if (CTaskVehicleFSM::RequestVehicleClipSetReturnIsLoaded(shuffleClipsetId, SP_High))
		{	
			if (pPed->IsNetworkClone())
			{
				if(m_pMoveNetworkHelper)
				{
					SetTaskState(State_WaitForInsert);
					return FSM_Continue;
				}
				else
				{
					SetTaskState(State_WaitForNetwork);
					return FSM_Continue;
				}
			}
			else
			{
				SetTaskState(State_ReserveShuffleSeat);
				return FSM_Continue;
			}
		}
        else
        {
#if __BANK
			CTaskVehicleFSM::SpewStreamingDebugToTTY(*pPed, *m_pVehicle, m_iCurrentSeatIndex, shuffleClipsetId, GetTimeInState());
#endif // __BANK

            if(pPed->IsNetworkClone())
            {
                // if we don't manage to stream the anims before the clone ped has finished shuffling just quit the task (the
                // ped will be popped to it's new position by the network code)
                if(!pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE))
                {
                    SetTaskState(State_Finish);
				    return FSM_Continue;
                }
            }
			else if (GetTimeInState() > fTimeBeforeWarp)
			{
				SetTaskState(State_WaitForPedToLeaveSeat);
				return FSM_Continue;
			}
        }
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskInVehicleSeatShuffle::ReserveShuffleSeat_OnUpdate()
{
	CPed* pPed = GetPed();

	TUNE_GROUP_FLOAT(VEHICLE_ENTRY, MAX_TIME_TO_ALLOW_EXIT, 0.5f, 0.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_ENTRY, MAX_TIME_TO_WAIT_FOR_RESERVATION, 2.5f, 0.0f, 10.0f, 0.01f);
	if (NetworkInterface::IsGameInProgress())
	{
		bool bIsTryingToExit = false;
		if (pPed->IsLocalPlayer() && GetTimeInState() > MAX_TIME_TO_ALLOW_EXIT)
		{
			const CControl* pControl = pPed->GetControlFromPlayer();
			if (pControl && pControl->GetPedEnter().IsPressed())
			{
				bIsTryingToExit = true;	
				SetFlag(aiTaskFlags::ProcessNextActiveTaskImmediately);
			}
		}

		if (bIsTryingToExit || (m_bPedEnterExit && GetTimeInState() > MAX_TIME_TO_WAIT_FOR_RESERVATION))
		{
			SetTaskState(State_Finish);
			return FSM_Continue;
		}
	}

	CComponentReservation* pComponentReservation = m_pVehicle->GetComponentReservationMgr()->GetSeatReservationFromSeat(m_iTargetSeatIndex);
	if (CComponentReservationManager::ReserveVehicleComponent(pPed, m_pVehicle, pComponentReservation))
	{
		s32 iSeat = m_iTargetSeatIndex;

		CPed* pSeatOccupier = m_pVehicle->GetSeatManager()->GetPedInSeat(iSeat);

		// We can sometimes get into a state where m_bWaitForSeatToBeOccupied is set to true, but no one is getting in
		// could maybe sync the ped who we think is in the seat and query their tasks?
		bool bShouldWait = true;
		TUNE_GROUP_FLOAT(SEAT_SHUFFLE_TUNE, MAX_TIME_TO_WAIT_FOR_OCCUPIER, 3.0f, 0.0f, 5.0f, 0.01f);
		if (m_bWaitForSeatToBeOccupied && GetTimeInState() > MAX_TIME_TO_WAIT_FOR_OCCUPIER)
		{
			bShouldWait = false;
			taskDisplayf("Waiting for seat to be occupied for longer than %.2f seconds, assuming it's free", MAX_TIME_TO_WAIT_FOR_OCCUPIER);
		}

		bool bShouldWaitForSeatToBeEmpty = (m_bWaitForSeatToBeEmpty && pSeatOccupier) ? true : false;
		if (bShouldWaitForSeatToBeEmpty && m_pVehicle->InheritsFromBoat() && NetworkInterface::IsGameInProgress() && pSeatOccupier->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE))
		{
			bShouldWait = false;
			AI_LOG_WITH_ARGS("[ComponentReservation] - Allowing ped %s to bypass wait for seat to be empty because ped %s is shuffling\n", AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetDynamicEntityNameSafe(pSeatOccupier));
		}

		if(bShouldWait &&
			((m_bWaitForSeatToBeOccupied &&  !pSeatOccupier) ||
			bShouldWaitForSeatToBeEmpty))
		{
			// Try again next frame
            taskDisplayf("Waiting for seat to be %s", m_bWaitForSeatToBeEmpty ? "empty" : "occupied");
		}
		else
		{
			m_bWaitForSeatToBeOccupied = false;
			m_bWaitForSeatToBeEmpty    = false;

#if __DEV
		    taskDebugf2("Frame : %u - %s%s : 0x%p : Ped Reserved Use Of Component With Reservation 0x%p : Bone Index %i", fwTimer::GetFrameCount(), pPed->IsNetworkClone() ? "Clone ped : " : "Local ped : ", pPed->GetDebugName(), pPed, pComponentReservation, pComponentReservation->GetBoneIndex());
#endif
		    if (DecideShuffleState(false) && m_bPedEnterExit)
		    {
			    SetTaskState(State_WaitForInsert);
			    return FSM_Continue;
		    }
        }

	}
	else if (m_pVehicle->IsTurretSeat(m_iTargetSeatIndex) && pComponentReservation && pComponentReservation->GetPedUsingComponent() && !pComponentReservation->GetPedUsingComponent()->ShouldBeDead())
	{
#if __DEV
		taskDebugf2("Frame : %u - %s%s : 0x%p : Ped aborting shuffle because turret occupied by alive ped %s%s : 0x%p", fwTimer::GetFrameCount(), pPed->IsNetworkClone() ? "Clone ped : " : "Local ped : ", pPed->GetDebugName(), pPed, pComponentReservation->GetPedUsingComponent()->IsNetworkClone() ? "Clone ped : " : "Local ped : ", pComponentReservation->GetPedUsingComponent()->GetDebugName(), pComponentReservation->GetPedUsingComponent());
#endif
		SetTaskState(State_Finish);
		return FSM_Continue;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskInVehicleSeatShuffle::WaitForInsert_OnEnter()
{
	taskAssertf(!m_MoveNetworkHelper.IsNetworkActive(), "Shuffle network active when waiting for parent network insert");
	
	// We need to start the move network if not being run as part of enter/exit
	StartMoveNetwork();

	// Send the request to move from the current enter/exit vehicle state to the shuffle (or the initial shuffle state)
	SetMoveNetworkState(ms_SeatShuffleRequestId, ms_ShuffleOnEnterId, ms_ShuffleStateId);
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskInVehicleSeatShuffle::WaitForInsert_OnUpdate()
{
	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

    CPed *pPed = GetPed();

    if(pPed->IsNetworkClone())
    {
        SetTaskState(State_WaitForNetwork);
    }
    else
    {
	    DecideShuffleState();
    }

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskInVehicleSeatShuffle::WaitForNetwork_OnUpdate()
{
    CPed       *pPed      = GetPed();
    CNetObjPed *netObjPed = static_cast<CNetObjPed *>(pPed->GetNetworkObject());

    if(!taskVerifyf(pPed->IsNetworkClone(), "Waiting for a network update from a locally controlled ped!") ||
       !taskVerifyf(netObjPed, "Waiting for a network update from a ped with no network object!"))
    {
        SetTaskState(State_Finish);
    }
    else
    {
        if(GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_EXIT_VEHICLE)
        {
            SetTaskState(State_Shuffle);
        }
        else
        {
			const bool bRemotePedRunningShuffleTask = pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE);
			const s32 iEntryPointIndex = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(m_iCurrentSeatIndex, m_pVehicle);
			s32 iShuffleSeat = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetShuffleSeatForSeat(iEntryPointIndex, m_iCurrentSeatIndex);
            const CEntryExitPoint* pEntryExitPoint = m_pVehicle->GetEntryExitPoint(iEntryPointIndex);
	        if(!taskVerifyf(pEntryExitPoint, "Entry point doesn't exist!"))
            {
                SetTaskState(State_Finish);
            }
            else if(!GetParent() && !bRemotePedRunningShuffleTask)
            {
                // if we think the ped is still running an enter vehicle task we may be waiting for the shuffle task update
                if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE))
                {
                    return FSM_Continue;
                }

                m_iTargetSeatIndex = m_iCurrentSeatIndex;

                SetTaskState(State_Finish);
            }
            else
            {
                CVehicle *targetVehicle    = netObjPed->GetPedsTargetVehicle();
                s32       targetSeat       = netObjPed->GetPedsTargetSeat();
				s32       remoteState      = bRemotePedRunningShuffleTask ? pPed->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE) : -1;
                bool      bPedInTargetSeat = (targetSeat == iShuffleSeat) && (m_pVehicle == targetVehicle);

				// Need to quit out if we're no longer running the shuffle task / been set out of the vehicle
				if(remoteState == -1 || !NetworkInterface::IsPedInVehicleOnOwnerMachine(pPed))
				{
					taskDisplayf("Aborting shuffle because remote ped not in vehicle or shuffle finished");
					SetTaskState(State_Finish);
					return FSM_Continue;
				}

                if(remoteState == State_Shuffle || remoteState == State_JackPed || bPedInTargetSeat)
                {
                    DecideShuffleState();
                }
            }
        }
    }

    return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskInVehicleSeatShuffle::Shuffle_OnEnter()
{
	StoreInitialOffsets();
	StartMoveNetwork();
	SetClipPlaybackRate();
	SetMoveNetworkState(ms_ShuffleToEmptySeatRequestId, ms_ShuffleToEmptySeatOnEnterId, ms_ShuffleToEmptySeatStateId);
	SetClipsForState();
	m_bMoveNetworkFinished = false;
	m_bMoveShuffleInterrupt = false;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskInVehicleSeatShuffle::Shuffle_OnUpdate()
{
	CPed& ped = *GetPed();
	if (!CTaskMotionInAutomobile::WantsToDuck(ped))
	ped.ForceMotionStateThisFrame(CPedMotionStates::MotionState_DoNothing);

	float fHairScale = 0.0f;
	if (CTaskMotionInVehicle::ShouldPerformInVehicleHairScale(ped, fHairScale))
	{
		ped.SetPedResetFlag(CPED_RESET_FLAG_OverrideHairScale, true);
		ped.SetTargetHairScale(fHairScale);
	}

	if (GetTimeInState() > 7.0f)
	{
#if __ASSERT
		taskWarningf("Ped stuck in shuffle state for more than 5 seconds, aborting in correct state ? %s, anim finished ? %s", IsMoveNetworkStateValid() ? "TRUE" : "FALSE", m_bMoveNetworkFinished ? "TRUE" : "FALSE");
		if (GetParent())
		{
			taskWarningf("Aborted shuffle task, parent task type : %i, State %i, Previous State %i", GetParent()->GetTaskType(), GetParent()->GetState(), GetParent()->GetPreviousState());
		}
#endif // __ASSERT
		SetTaskState(State_WaitForPedToLeaveSeat);
		return FSM_Continue;
	}

#if __ASSERT
	const CPed* pPedInOrUsingSeat = CTaskVehicleFSM::GetPedInOrUsingSeat(*m_pVehicle, m_iTargetSeatIndex);
	if (pPedInOrUsingSeat && pPedInOrUsingSeat != &ped)
	{
		taskWarningf("Frame : %i, Poptype = %i Ped %s(%p) has entered the seat ped %s(%p) is shuffling to", fwTimer::GetFrameCount(), pPedInOrUsingSeat ? (s32)pPedInOrUsingSeat->PopTypeGet() : -1, pPedInOrUsingSeat ? pPedInOrUsingSeat->GetDebugName() : "NULL", pPedInOrUsingSeat, ped.GetDebugName(), &ped);
		pPedInOrUsingSeat->GetPedIntelligence()->GetTaskManager()->SpewAllTasks(aiTaskSpew::SPEW_STD);
		if (pPedInOrUsingSeat->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_VEHICLE))
		{
			aiDisplayf("Ped %p was in motion in vehicle task for %.2f seconds", pPedInOrUsingSeat, pPedInOrUsingSeat->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_VEHICLE)->GetTimeRunning());
		}
	}
#endif // __ASSERT

	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	if (m_pVehicle->IsTurretSeat(m_iCurrentSeatIndex) && m_pVehicle->IsTurretSeat(m_iTargetSeatIndex))
	{
		CTaskVehicleFSM::PointTurretInNeturalPosition(*m_pVehicle, m_iTargetSeatIndex);
	}

	float fPhase = 0.0f;
	const crClip* pClip = GetClipAndPhaseForState(fPhase);
	const CVehicleSeatAnimInfo* pSeatClipInfo = m_pVehicle->GetSeatAnimationInfo(m_iTargetSeatIndex);
	if (!taskVerifyf(pSeatClipInfo, "Seat Clip Info was null for %s, Target Seat Index: %i", m_pVehicle->GetModelName(), m_iTargetSeatIndex) && pSeatClipInfo->IsTurretSeat())
	{
		if (pClip)
		{
			if (CTaskMotionInTurret::IsSeatWithHatchProtection(m_iTargetSeatIndex, *m_pVehicle))
			{
				s32 iDirectEntryPointIndex = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(m_iTargetSeatIndex, m_pVehicle);
				CTaskVehicleFSM::ProcessOpenDoor(*pClip, fPhase, *m_pVehicle, iDirectEntryPointIndex, false);
			}

			CTurret* pTurret = m_pVehicle->GetVehicleWeaponMgr() ? m_pVehicle->GetVehicleWeaponMgr()->GetFirstTurretForSeat(m_iTargetSeatIndex) : NULL;
			if (pTurret)
			{
				CTaskVehicleFSM::ProcessTurretHandGripIk(*m_pVehicle, ped, pClip, fPhase, *pTurret);
			}
		}
	}
	else
	{
		// TODO: CTaskVehicleFSM::ProcessCloseDoor
		const CVehicleSeatAnimInfo* pSeatClipInfo = m_pVehicle->GetSeatAnimationInfo(m_iCurrentSeatIndex);
		if (!taskVerifyf(pSeatClipInfo, "Seat Clip Info was null for %s, Current Seat Index: %i", m_pVehicle->GetModelName(), m_iCurrentSeatIndex) && pSeatClipInfo->IsTurretSeat())
		{
			if (pClip)
			{
				s32 iDirectEntryPointIndex = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(m_iCurrentSeatIndex, m_pVehicle);
				TUNE_GROUP_FLOAT(TURRET_TUNE, START_CLOSE_DOOR_PHASE, 0.0f, 0.0f, 1.0f, 0.01f);
				TUNE_GROUP_FLOAT(TURRET_TUNE, END_CLOSE_DOOR_PHASE, 0.4f, 0.0f, 1.0f, 0.01f);
				float fPhaseToBeginCloseDoor = START_CLOSE_DOOR_PHASE;
				float fPhaseToEndCloseDoor = END_CLOSE_DOOR_PHASE;
				CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pClip, CClipEventTags::Door, CClipEventTags::Start, true, fPhaseToBeginCloseDoor);
				CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pClip, CClipEventTags::Door, CClipEventTags::Start, false, fPhaseToEndCloseDoor);

				CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(m_pVehicle, iDirectEntryPointIndex);
				if (pDoor)
				{
					CTaskVehicleFSM::DriveDoorFromClip(*m_pVehicle, pDoor, CTaskVehicleFSM::VF_CloseDoor, fPhase, fPhaseToBeginCloseDoor, fPhaseToEndCloseDoor);
				}
			}
		}
	}

	if (CTaskMotionInVehicle::VehicleHasHandleBars(*m_pVehicle) && pClip)
	{
		s32 iDirectEntryPointIndex = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(m_iTargetSeatIndex, m_pVehicle);
		CTaskEnterVehicle::ProcessBikeArmIk(*m_pVehicle, ped, iDirectEntryPointIndex, *pClip, fPhase, false);
	}
	CTaskVehicleFSM::ProcessApplyForce(m_MoveNetworkHelper, *m_pVehicle, ped);

	// When running the shuffle task as part of entry/exit we blend into the exit/idle as soon as possible
	// Or wanting to duck
	if (GetParent() || CTaskMotionInAutomobile::WantsToDuck(ped))
	{
		if (m_bMoveShuffleInterrupt)
		{
#if __ASSERT
			const CPed* pPedInOrUsingSeat = CTaskVehicleFSM::GetPedInOrUsingSeat(*m_pVehicle, m_iTargetSeatIndex);
			if (pPedInOrUsingSeat && pPedInOrUsingSeat != &ped)
			{
				taskWarningf("Frame : %i, %s ped %s finished shuffle due to interrupt but %s %s", fwTimer::GetFrameCount(), ped.IsNetworkClone() ? "CLONE" : "LOCAL", ped.GetDebugName(), pPedInOrUsingSeat ? "should have jacked ped " : "did not have seat reserved ", pPedInOrUsingSeat ? pPedInOrUsingSeat->GetDebugName() : "NULL");
			}
#endif // __ASSERT			
			SetTaskState(State_WaitForPedToLeaveSeat);
			return FSM_Continue;
		}
	}

	if (m_bMoveNetworkFinished)
	{
#if __ASSERT
		const CPed* pPedInOrUsingSeat = CTaskVehicleFSM::GetPedInOrUsingSeat(*m_pVehicle, m_iTargetSeatIndex);
		if (pPedInOrUsingSeat && pPedInOrUsingSeat != &ped)
		{
			taskWarningf("Frame : %i, %s ped %s finished shuffle but %s %s", fwTimer::GetFrameCount(), ped.IsNetworkClone() ? "CLONE" : "LOCAL", ped.GetDebugName(), pPedInOrUsingSeat ? "should have jacked ped " : "did not have seat reserved ", pPedInOrUsingSeat ? pPedInOrUsingSeat->GetDebugName() : "NULL");
		}
#endif // __ASSERT	
		SetTaskState(State_WaitForPedToLeaveSeat);
		return FSM_Continue;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskInVehicleSeatShuffle::JackPed_OnEnter()
{
	StoreInitialOffsets();
	StartMoveNetwork();
	SetClipPlaybackRate();
	SetMoveNetworkState(ms_ShuffleAndJackPedRequestId, ms_ShuffleAndJackPedOnEnterId, ms_ShuffleAndJackPedStateId);
	SetClipsForState();
	CTaskEnterVehicle::PossiblyReportStolentVehicle(*GetPed(), *m_pVehicle, true);
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskInVehicleSeatShuffle::JackPed_OnUpdate()
{
	CPed* pPed = GetPed();

	// Super special code to switch places with a ped we're shuffle jacking if the victims entry point is blocked
	if (!NetworkInterface::IsGameInProgress() && m_pJackedPed && !m_pJackedPed->IsPlayer() && pPed->IsLocalPlayer())
	{
		const s32 iOtherSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(m_pJackedPed);
		const s32 iMySeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
		s32 iDirectEntryPoint = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iOtherSeatIndex, m_pVehicle);
		if (m_pVehicle->IsSeatIndexValid(iMySeatIndex) && m_pVehicle->IsEntryIndexValid(iDirectEntryPoint) && !CVehicle::IsEntryPointClearForPed(*m_pVehicle, *pPed, iDirectEntryPoint))
		{
			// Set ourself out so the other ped can be put into our seat
			pPed->SetPedOutOfVehicle();
			m_pJackedPed->SetPedOutOfVehicle();
			m_pJackedPed->GetPedIntelligence()->FlushImmediately();
			// Put the other ped in our seat
			m_pJackedPed->SetPedInVehicle(m_pVehicle, iMySeatIndex, CPed::PVF_Warp);
			// Put ourself in the other peds seat
			pPed->SetPedInVehicle(m_pVehicle, iOtherSeatIndex, CPed::PVF_Warp);
			// Make the other ped get out
			VehicleEnterExitFlags vehicleFlags;
			vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::BeJacked);
			vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::SwitchedPlacesWithJacker);
			s32 iMyDirectEntryPoint = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iMySeatIndex, m_pVehicle);
			const s32 iEventPriority = CTaskEnterVehicle::GetEventPriorityForJackedPed(*m_pJackedPed);
			CEventGivePedTask givePedTask(PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP, rage_new CTaskExitVehicle(m_pVehicle, vehicleFlags, 0.0f, GetPed(), iMyDirectEntryPoint), true, iEventPriority );
			m_pJackedPed->GetPedIntelligence()->AddEvent(givePedTask);
#if __BANK
			CGivePedJackedEventDebugInfo instance(m_pJackedPed, this, __FUNCTION__);
			instance.Print();
#endif // __BANK
			return FSM_Quit;
		}
	}

	static dev_float JACK_TIME_OUT = 7.0f;
	if (GetTimeInState() > JACK_TIME_OUT)
	{
		aiAssertf(0, "Couldn't complete shuffle jack anim for vehicle %s, target seat %i", m_pVehicle->GetModelName(), m_iTargetSeatIndex);
		GetPed()->SetPedOutOfVehicle();
		return FSM_Quit;
	}

#if __ASSERT
	taskDisplayf("Frame : %u - %s%s %s anim target state and in %s", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone ped : " : "Local ped : ", GetPed()->GetDebugName(), IsMoveNetworkStateValid() ? "IS IN" : "IS NOT IN", GetStateName(GetState()));
#endif // __ASSERT 

	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	if (IsMoveNetworkStateFinished(ms_SeatShuffleClipFinishedId, ms_SeatShufflePhaseId))
	{
		SetTaskState(State_WaitForPedToLeaveSeat);
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskInVehicleSeatShuffle::WaitForPedToLeaveSeat_OnEnter()
{
    m_iWaitForPedTimer = 0;

	// Wait in sp aswell in case the ped is in the process of exiting (tag not early enough)
    //if(NetworkInterface::IsGameInProgress())
    {
        bool pedAlreadyInSeat = (m_iTargetSeatIndex != -1) && (m_pVehicle->GetSeatManager()->GetPedInSeat(m_iTargetSeatIndex) != 0);

        if(pedAlreadyInSeat)
        {
            const unsigned WAIT_FOR_PED_DURATION_MS = 3000; // This was 500ms, but has had to be increased due to poor framerate
            m_iWaitForPedTimer = WAIT_FOR_PED_DURATION_MS;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskInVehicleSeatShuffle::WaitForPedToLeaveSeat_OnUpdate()
{
    CPed *pPed = GetPed();

    // locally controlled peds must wait for clone peds to leave the seat they
    // are being set in. Any local peds in the target seat should have already left, if not it
    // is an error. Clone peds do not wait for the seat to become free, this is handled elsewhere.
    bool pedAlreadyInSeat = (m_iTargetSeatIndex != -1) && (m_pVehicle->GetSeatManager()->GetPedInSeat(m_iTargetSeatIndex) != 0);

    if(pedAlreadyInSeat /*&& NetworkInterface::IsGameInProgress()*/ && !pPed->IsNetworkClone())
    {
        if(m_iWaitForPedTimer == 0)
        {
            // timed out waiting for the ped to leave the seat we are shuffling to, leave the ped in his current seat
            CPed *pPedInSeat = m_pVehicle->GetSeatManager()->GetPedInSeat(m_iTargetSeatIndex);

            if(CTaskVehicleFSM::CanJackPed(pPed, pPedInSeat))
            {
                if(pPed->GetMyVehicle() == m_pVehicle && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
                {
                    pPed->SetPedOutOfVehicle();
                }
            }

#if !__FINAL
			if (pPedInSeat)
			{
				pPedInSeat->GetPedIntelligence()->GetTaskManager()->SpewAllTasks(aiTaskSpew::SPEW_STD);
			}
#endif // !__FINAL
			aiWarningf("%s ped %s failed to shuffle to seat %i because it was still occupied by %s ped %s", 
				pPed->IsNetworkClone() ? "CLONE" : "LOCAL", pPed->GetDebugName(), m_iTargetSeatIndex, 
				pPedInSeat ? (pPedInSeat->IsNetworkClone() ? "CLONE" : "LOCAL") : "NULL", pPedInSeat ? pPedInSeat->GetDebugName() : "NULL");
            SetTaskState(State_Finish);
        }

        m_iWaitForPedTimer -= static_cast<s16>(GetTimeStepInMilliseconds());
        m_iWaitForPedTimer = MAX(0, m_iWaitForPedTimer);
    }
    else
    {
		m_iWaitForPedTimer = 0;
		SetTaskState(State_SetPedInSeat);
    }

    return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskInVehicleSeatShuffle::SetPedInSeat_OnEnter()
{
	CPed *pPed = GetPed();

	// if we get to this state before we have a valid seat index or there is still a ped in the seat
	// we have to quit the task and rely on the network syncing code to place the ped in the vehicle
	CPed* pPedInSeat = m_pVehicle->GetSeatManager()->GetPedInSeat(m_iTargetSeatIndex);
	bool pedAlreadyInSeat = pPedInSeat != 0;

	if(pPed && pPed->IsNetworkClone() && (m_iTargetSeatIndex == -1 || pedAlreadyInSeat))
	{
		// Do nothing
		AI_LOG_WITH_ARGS("[VehicleEntryExit][CTaskInVehicleSeatShuffle::SetPedInSeat_OnEnter] - Clone ped %s not being set in target seat %i, ped in seat %s\n", AILogging::GetDynamicEntityNameSafe(pPed), m_iTargetSeatIndex, AILogging::GetDynamicEntityNameSafe(pPedInSeat));
	}
	else
	{
		if (pedAlreadyInSeat)
		{
			// Reset the ped back into the seat they're shuffling from to reset the attach offset
			AI_LOG_WITH_ARGS("[VehicleEntryExit][CTaskInVehicleSeatShuffle::SetPedInSeat_OnEnter] - Failed to add %s ped %s to seat %i because it was occupied by ped %s\n", AILogging::GetDynamicEntityIsCloneStringSafe(pPed), AILogging::GetDynamicEntityNameSafe(pPed), m_iTargetSeatIndex, AILogging::GetDynamicEntityNameSafe(pPedInSeat));
			m_iTargetSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
			taskAssertf(m_pVehicle->IsSeatIndexValid(m_iTargetSeatIndex), "Invalid seat index %u", m_iTargetSeatIndex);
		}

		// Keep the in vehicle move network
		SetNewTask(rage_new CTaskSetPedInVehicle(m_pVehicle, m_iTargetSeatIndex, CPed::PVF_KeepInVehicleMoveBlender));
	}
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskInVehicleSeatShuffle::SetPedInSeat_OnUpdate()
{
	if (!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetTaskState(State_UnReserveShuffleSeat);
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskInVehicleSeatShuffle::Finish_OnEnter()
{
    CPed* pPed = GetPed();

	if (pPed->IsNetworkClone())
	{
        const unsigned WAIT_FOR_SEAT_DURATION_MS = 500;
        m_iWaitForPedTimer = WAIT_FOR_SEAT_DURATION_MS;
    }
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskInVehicleSeatShuffle::Finish_OnUpdate()
{
	CPed* pPed = GetPed();

	if (pPed->IsNetworkClone())
	{
        // update the cached seat for the ped
        CNetObjPed *netObjPed = static_cast<CNetObjPed *>(pPed->GetNetworkObject());

        s32 networkTargetSeat = netObjPed->GetPedsTargetSeat();
		AI_FUNCTION_LOG_WITH_ARGS("Clone waiting to be set in seat, networkTarget = %i, m_iTargetSeatIndex = %i", networkTargetSeat, m_iTargetSeatIndex);

        if(networkTargetSeat != m_iTargetSeatIndex)
        {
            if(m_iWaitForPedTimer > 0)
            {
                m_iWaitForPedTimer -= static_cast<s16>(GetTimeStepInMilliseconds());
                m_iWaitForPedTimer = MAX(0, m_iWaitForPedTimer);
                return FSM_Continue;
            }
        }
	}
	return FSM_Quit;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskInVehicleSeatShuffle::UnReserveShuffleSeat_OnUpdate()
{
    // Get the direct entry point index for our current seat
    CPed *pPed = GetPed();

    if(!pPed->IsNetworkClone())
    {
	    CComponentReservation* pComponentReservation = m_pVehicle->GetComponentReservationMgr()->GetSeatReservationFromSeat(m_iTargetSeatIndex);
	    CComponentReservationManager::UnreserveVehicleComponent( pPed, pComponentReservation);
		CTaskVehicleFSM::ProcessAutoCloseDoor( *pPed, *static_cast<CVehicle*>(m_pVehicle.Get()), m_iCurrentSeatIndex );
    }

	SetTaskState(State_Finish);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskInVehicleSeatShuffle::StoreInitialOffsets()
{
	Vector3 vTargetPosition(Vector3::ZeroType);
	Quaternion qTargetOrientation(0.0f,0.0f,0.0f,1.0f);

	CModelSeatInfo::CalculateSeatSituation(m_pVehicle, vTargetPosition, qTargetOrientation, m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(m_iTargetSeatIndex, m_pVehicle));
	m_PlayAttachedClipHelper.SetTarget(vTargetPosition, qTargetOrientation);
	m_PlayAttachedClipHelper.SetInitialOffsets(*GetPed());
}

////////////////////////////////////////////////////////////////////////////////

void CTaskInVehicleSeatShuffle::StartMoveNetwork()
{
	const fwMvClipSetId shuffleClipsetId = GetShuffleClipsetId(m_pVehicle, m_iCurrentSeatIndex);
	if (!m_pMoveNetworkHelper)
	{
		CPed* pPed = GetPed();
		taskAssertf(!m_bPedEnterExit, "Deciding shuffle state with a NULL parent network helper when not running as part of an enter exit task!");

		m_MoveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskInVehicleSeatShuffle);
		TUNE_GROUP_FLOAT(SHUFFLE_TUNE, NORMAL_SHUFFLE_BLEND_IN_DURATION, 0.0f, 0.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(SHUFFLE_TUNE, TURRET_SHUFFLE_BLEND_IN_DURATION, 0.25f, 0.0f, 1.0f, 0.01f);
		const bool bIsMovingToOrFromTurretSeat = m_pVehicle->IsTurretSeat(m_iCurrentSeatIndex) || m_pVehicle->IsTurretSeat(m_iTargetSeatIndex);
		float fBlendDuration = bIsMovingToOrFromTurretSeat ? TURRET_SHUFFLE_BLEND_IN_DURATION : NORMAL_SHUFFLE_BLEND_IN_DURATION;
		if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsDuckingInVehicle))
		{
			fBlendDuration = CTaskEnterVehicle::ms_Tunables.m_DuckShuffleBlendDuration;
		}
		pPed->GetMovePed().SetTaskNetwork(m_MoveNetworkHelper, fBlendDuration);
		m_MoveNetworkHelper.SetClipSet(shuffleClipsetId);
		m_pMoveNetworkHelper = &m_MoveNetworkHelper;

		ASSERT_ONLY(taskDebugf2("Frame : %u - %s%s started Shuffle Network", fwTimer::GetFrameCount(), pPed->IsNetworkClone() ? "Clone ped : " : "Local ped : ", pPed->GetDebugName()));
	}
	else
	{
		m_pMoveNetworkHelper->SetClipSet(shuffleClipsetId);
		m_pMoveNetworkHelper->SendRequest(ms_SeatShuffleRequestId);
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskInVehicleSeatShuffle::DecideShuffleState(bool bGiveJackedPedEvent)
{
    CPed* pPed = GetPed();

	const s32 iDirectEntryIndex = m_pVehicle->GetDirectEntryPointIndexForPed(pPed);
	if (m_pVehicle->IsEntryIndexValid(iDirectEntryIndex))
	{
		s32 iTargetSeat = m_iTargetSeatIndex;

		if (m_pVehicle->IsSeatIndexValid(iTargetSeat))
		{
			m_pJackedPed = m_pVehicle->GetSeatManager()->GetPedInSeat(iTargetSeat);
			if (m_pJackedPed)
			{
				VehicleEnterExitFlags flags;
				if (GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_ENTER_VEHICLE)
				{
					// Maybe should just take all flags?
					if (static_cast<CTaskEnterVehicle*>(GetParent())->IsFlagSet(CVehicleEnterExitFlags::WantsToJackFriendlyPed))
					{
						flags.BitSet().Set(CVehicleEnterExitFlags::WantsToJackFriendlyPed);
						flags.BitSet().Set(CVehicleEnterExitFlags::ConsiderJackingFriendlyPeds);
						AI_FUNCTION_LOG("Local player setting CVehicleEnterExitFlags::ConsiderJackingFriendlyPeds");
					}
				}

				// Re-check for PreventShuffleJack flag
				const CVehicleSeatAnimInfo* pTargetSeatClipInfo = m_pVehicle->GetSeatAnimationInfo(iTargetSeat);
				taskFatalAssertf(pTargetSeatClipInfo, "NULL Seat clip Info for seat index %i", iTargetSeat);
				bool bPreventShuffleJack = pTargetSeatClipInfo->GetPreventShuffleJack();

                if (m_pVehicle->GetDriver() != pPed && !bPreventShuffleJack && (m_bJackAnyOne || (CTaskVehicleFSM::CanJackPed(pPed, m_pJackedPed, flags) && !m_pVehicle->InheritsFromBike())))
				{
					// Don't try to jack the ped until they've finished the enter vehicle task as they won't abort it for the exit vehicle task
					// until its finished.
					if (m_pJackedPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE))
					{
						return false;
					}

					// If the ped has just been killed, they will receive a damage event which will be higher priority than any give ped task event and
					// prevent a jack from completing successfully
					if (!m_pJackedPed->IsNetworkClone() && m_pJackedPed->ShouldBeDead())
					{
						const CEventDamage* pDamageEvent = static_cast<const CEventDamage*>(m_pJackedPed->GetPedIntelligence()->GetEventOfType(EVENT_DAMAGE));
						if (pDamageEvent)
						{
							ASSERT_ONLY(taskDisplayf("Frame : %u - %s%s : 0x%p : Preventing jack because ped %s ped %s hasn't responded to damage event", fwTimer::GetFrameCount(), pPed->IsNetworkClone() ? "Clone ped : " : "Local ped : ", pPed->GetDebugName(), pPed, m_pJackedPed->IsNetworkClone() ? "Clone" : "Local", m_pJackedPed->GetDebugName()));
							return false;
						}
					}

					if (!m_pJackedPed->IsNetworkClone() && (bGiveJackedPedEvent || !m_bPedEnterExit))
					{
						s32 iDirectEntryPointIndex = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iTargetSeat, m_pVehicle);
						if (taskVerifyf(m_pVehicle->IsEntryPointIndexValid(iDirectEntryPointIndex), "Entry index was invalid %u", iDirectEntryPointIndex))
						{
							const s32 iEventPriority = CTaskEnterVehicle::GetEventPriorityForJackedPed(*m_pJackedPed);
							VehicleEnterExitFlags vehicleFlags;
							vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::BeJacked);
							vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::JackedFromInside);
							CEventGivePedTask givePedTask(PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP, rage_new CTaskExitVehicle(m_pVehicle,vehicleFlags, 0.0f, GetPed(), iDirectEntryPointIndex), true, iEventPriority );
							m_pJackedPed->GetPedIntelligence()->AddEvent(givePedTask);
#if __BANK
							CGivePedJackedEventDebugInfo instance(m_pJackedPed, this, __FUNCTION__);
							instance.Print();
							CTasksFullDebugInfo jackedPedTaskFull(m_pJackedPed);
							jackedPedTaskFull.Print();
#endif // __BANK
						}
					}
					SetTaskState(State_JackPed);
				}
				else
				{
#if __DEV
					taskDisplayf("Frame : %u - %s%s : 0x%p : Couldn't shuffle jack %s ped %s", fwTimer::GetFrameCount(), pPed->IsNetworkClone() ? "Clone ped : " : "Local ped : ", pPed->GetDebugName(), pPed, m_pJackedPed->IsNetworkClone() ? "Clone" : "Local", m_pJackedPed->GetDebugName());
#endif
					m_iTargetSeatIndex = m_iCurrentSeatIndex;
					SetTaskState(State_UnReserveShuffleSeat);
					return false;
				}
			}
			else
			{
#if __ASSERT
				// Only assert for local peds. Clones will receive seat info over network.
				if (!pPed->IsNetworkClone())
				{
					const CPed* pPedInOrUsingSeat = CTaskVehicleFSM::GetPedInOrUsingSeat(*m_pVehicle, m_iTargetSeatIndex);
					taskAssertf(!pPedInOrUsingSeat || pPedInOrUsingSeat == pPed, "Frame : %i, %s ped %s starting shuffle but %s %s", fwTimer::GetFrameCount(), pPed->IsNetworkClone() ? "CLONE" : "LOCAL", pPed->GetDebugName(), pPedInOrUsingSeat ? "should have jacked ped " : "did not have seat reserved ", pPedInOrUsingSeat ? pPedInOrUsingSeat->GetDebugName() : "NULL");
				}
#endif // __ASSERT
				SetTaskState(State_Shuffle);
			}
		}
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////

const crClip* CTaskInVehicleSeatShuffle::GetClipAndPhaseForState(float& fPhase)
{
	if (m_pMoveNetworkHelper)
	{
		switch (GetState())
		{
			case State_Shuffle: 
			case State_JackPed:
			case State_WaitForPedToLeaveSeat:
			// Intentional fall-through
			{
				fPhase = m_pMoveNetworkHelper->GetFloat( ms_SeatShufflePhaseId);
				return m_pMoveNetworkHelper->GetClip( ms_SeatShuffleClipId);
			}
			case State_Start:
			case State_StreamAssets:
			case State_WaitForInsert: 
            case State_WaitForNetwork:
			case State_ReserveShuffleSeat:
			case State_UnReserveShuffleSeat:
			case State_SetPedInSeat:
			case State_Finish:
				break;

			default: taskAssertf(0, "Unhandled State");
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
void CTaskInVehicleSeatShuffle::Debug() const
{
#if __BANK
	Vector3 vTargetPos(Vector3::ZeroType);
	Quaternion qTargetOrientation(0.0f,0.0f,0.0f,1.0f);

	m_PlayAttachedClipHelper.GetTarget(vTargetPos, qTargetOrientation);

	Matrix34 mTemp(Matrix34::IdentityType);
	mTemp.FromQuaternion(qTargetOrientation);
	mTemp.d = vTargetPos;

	static float sfScale = 0.5f;
	grcDebugDraw::Axis(mTemp, sfScale);
#endif
}
#endif // !__FINAL

////////////////////////////////////////////////////////////////////////////////

CClonedSeatShuffleInfo::CClonedSeatShuffleInfo(s32 shuffleState, CVehicle *pVehicle, bool bEnteringVehicle, s32 iTargetSeatIndex) :
m_pVehicle(pVehicle),
m_bPedEnterExit(bEnteringVehicle),
m_iTargetSeatIndex(iTargetSeatIndex)
{
    SetStatusFromMainTaskState(shuffleState);
}

CTaskFSMClone *CClonedSeatShuffleInfo::CreateCloneFSMTask()
{
    if(!m_bPedEnterExit && m_pVehicle.GetVehicle())
    {
        return rage_new CTaskInVehicleSeatShuffle(m_pVehicle.GetVehicle(), NULL, false, m_iTargetSeatIndex);
    }
    else
    {
        return 0;
    }
}
