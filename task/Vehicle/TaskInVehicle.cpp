//
// Task/Vehicle/TaskInVehicle.cpp
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

// File header
#include "Task/Vehicle/TaskInVehicle.h"

// Game Headers
#include "ai/debug/types/RelationshipDebugInfo.h"
#include "animation/AnimManager.h"
#include "animation/FacialData.h"
#include "animation/Move.h"
#include "animation/MoveVehicle.h"
#include "audio/radioaudioentity.h"
#include "Camera/CamInterface.h"
#include "Camera/Cinematic/CinematicDirector.h"
#include "Camera/Debug/DebugDirector.h"
#include "Camera/gameplay/GameplayDirector.h"
#include "camera/helpers/ControlHelper.h"
#include "Debug/DebugScene.h"
#include "Entity/archetype.h"
#include "Event/EventDamage.h"
#include "Event/EventWeapon.h"
#include "Event/Events.h"
#include "Frontend/NewHud.h"
#include "Frontend/MobilePhone.h"
#include "Game/ModelIndices.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Peds/Ped.h"
#include "Peds/PedIkSettings.h"
#include "Peds/PlayerInfo.h"
#include "Peds/PlayerSpecialAbility.h"
#include "Peds/rendering/PedProps.h"
#include "Scene/EntityIterator.h"
#include "Scene/world/GameWorld.h"
#include "Script/script_hud.h"
#include "Stats/StatsInterface.h"
#include "Streaming/PrioritizedClipSetStreamer.h"
#include "System/control.h"
#include "System/control_mapping.h"
#include "Task/Default/TaskCuffed.h"
#include "Task/default/TaskPlayer.h"
#include "Task/General/TaskSecondary.h"
#include "Task/General/Phone/TaskMobilePhone.h"
#include "Task/Motion/Vehicle/TaskMotionInTurret.h"
#include "Task/Physics/TaskNMBalance.h"
#include "Task/Physics/TaskNMJumpRollFromRoadVehicle.h"
#include "Task/Physics/TaskNMThroughWindscreen.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskCarAccessories.h"
#include "Task/Vehicle/TaskCarUtils.h"
#include "Task/Vehicle/TaskVehicleBase.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Task/Vehicle/TaskMountAnimalWeapon.h"
#include "Task/Weapons/Gun/TaskVehicleDriveBy.h"
#include "Vehicles/Bike.h"
#include "Vehicles/Heli.h"
#include "Vehicles/vehicle.h"
#include "Vehicles/Planes.h"
#include "Vehicles/Trailer.h"
#include "Vehicles/Boat.h"
#include "Vehicles/Submarine.h"
#include "Vehicles/Metadata/VehicleLayoutInfo.h"
#include "Vehicles/Metadata/VehicleMetadataManager.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "Vehicles/Metadata/VehicleEntryPointInfo.h"
#include "VehicleAi/vehicleintelligence.h"
#include "vehicleAi/task/TaskVehicleAnimation.h"
#include "vehicleAi/task/TaskVehiclePlayer.h"
#include "Peds/PedHelmetComponent.h"
#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

const float IDLE_TIMER_HELMET_PUT_ON = 0.5f;

const fwMvBooleanId CTaskMotionInVehicle::ms_StillOnEnterId("Still_OnEnter",0xDF36A2BC);
const fwMvBooleanId CTaskMotionInVehicle::ms_LowLodOnEnterId("LowLodIdle_OnEnter",0x8FDCAD33);
const fwMvBooleanId CTaskMotionInVehicle::ms_IdleOnEnterId("Idle_OnEnter",0xCA902721);
const fwMvBooleanId CTaskMotionInVehicle::ms_ReverseOnEnterId("Reverse_OnEnter",0x37156EED);
const fwMvBooleanId CTaskMotionInVehicle::ms_CloseDoorOnEnterId("CloseDoor_OnEnter",0x40C4545);
const fwMvBooleanId CTaskMotionInVehicle::ms_ShuntOnEnterId("Shunt_OnEnter",0x1B6094CA);
const fwMvBooleanId CTaskMotionInVehicle::ms_StartEngineOnEnterId("StartEngine_OnEnter",0xCC521651);
const fwMvBooleanId CTaskMotionInVehicle::ms_HeavyBrakeOnEnterId("HeavyBrake_OnEnter",0xF574EEAB);
const fwMvBooleanId CTaskMotionInVehicle::ms_HornOnEnterId("Horn_OnEnter",0x8A169BF7);
const fwMvBooleanId CTaskMotionInVehicle::ms_FirstPersonHornOnEnterId("FirstPersonHorn_OnEnter",0xcd5d91d3);
const fwMvBooleanId CTaskMotionInVehicle::ms_PutOnHelmetInterrupt("PutOnHelmetInterrupt",0x3E2011E6);
const fwMvBooleanId CTaskMotionInVehicle::ms_FirstPersonSitIdleOnEnterId("FirstPersonSitIdle_OnEnter",0x0703a16e);
const fwMvBooleanId CTaskMotionInVehicle::ms_FirstPersonReverseOnEnterId("FirstPersonReverse_OnEnter",0x24958b29);
const fwMvBooleanId CTaskMotionInVehicle::ms_IsDoingDriveby("IsDoingDriveby", 0xc8f149ff); 
const fwMvBooleanId CTaskMotionInVehicle::ms_LowriderArmTransitionFinished("ArmTransition_Finished", 0xc397bf3b);
const fwMvRequestId CTaskMotionInVehicle::ms_LowLodRequestId("LowLod",0x343FE5DC);
const fwMvRequestId CTaskMotionInVehicle::ms_StillRequestId("Still",0x48A35826);
const fwMvRequestId CTaskMotionInVehicle::ms_IdleRequestId("SitIdle",0x733CD73C);
const fwMvRequestId CTaskMotionInVehicle::ms_ReverseRequestId("Reverse",0xAE3930D7);
const fwMvRequestId CTaskMotionInVehicle::ms_CloseDoorRequestId("CloseDoor",0xECD1883E);
const fwMvRequestId CTaskMotionInVehicle::ms_ShuntRequestId("Shunt",0x462CED36);
const fwMvRequestId CTaskMotionInVehicle::ms_HeavyBrakeRequestId("HeavyBrake",0x161EB492);
const fwMvRequestId CTaskMotionInVehicle::ms_StartEngineRequestId("StartEngine",0x43377B8E);
const fwMvRequestId CTaskMotionInVehicle::ms_HornRequestId("Horn",0x3E34681F);
const fwMvRequestId CTaskMotionInVehicle::ms_FirstPersonHornRequestId("FirstPersonHorn",0xdd07834b);
const fwMvRequestId CTaskMotionInVehicle::ms_FirstPersonSitIdleRequestId("FirstPersonSitIdle",0xb8f00517);
const fwMvRequestId CTaskMotionInVehicle::ms_FirstPersonReverseRequestId("FirstPersonReverse",0x68ac3eaf);
const fwMvFloatId CTaskMotionInVehicle::ms_BodyMovementXId("BodyMovementX",0xA1F93FE1);
const fwMvFloatId CTaskMotionInVehicle::ms_BodyMovementYId("BodyMovementY",0xF3DFE3AD);
const fwMvFloatId CTaskMotionInVehicle::ms_BodyMovementZId("BodyMovementZ",0xC6958919);
const fwMvFloatId CTaskMotionInVehicle::ms_SpeedId("Speed",0xF997622B);
const fwMvFloatId CTaskMotionInVehicle::ms_SteeringId("Steering",0x74D5E091);
const fwMvFloatId CTaskMotionInVehicle::ms_StartEnginePhaseId("StartEnginePhase",0x52740191);
const fwMvFloatId CTaskMotionInVehicle::ms_StartEngineRateId("StartEngineRate",0x55F6D7E9);
const fwMvFloatId CTaskMotionInVehicle::ms_SuspensionMovementUDId("SuspensionMovementUD", 0x26446209);
const fwMvFloatId CTaskMotionInVehicle::ms_SuspensionMovementLRId("SuspensionMovementLR", 0xc62f5dc8);
const fwMvFloatId CTaskMotionInVehicle::ms_WindowHeight("WindowHeight", 0x3b7be142);
const fwMvFlagId CTaskMotionInVehicle::ms_IsDriverId("IsDriver",0xAEA9F351);
const fwMvFlagId CTaskMotionInVehicle::sm_PedShouldSwayInVehicleId("PedShouldSwayInVehicle", 0xd8ae14f0);
const fwMvFlagId CTaskMotionInVehicle::ms_FrontFlagId("Front",0x89F230A2);
const fwMvFlagId CTaskMotionInVehicle::ms_BackFlagId("Back",0x37418674);
const fwMvFlagId CTaskMotionInVehicle::ms_LeftFlagId("Left",0x6FA34840);
const fwMvFlagId CTaskMotionInVehicle::ms_RightFlagId("Right",0xB8CCC339);
const fwMvFlagId CTaskMotionInVehicle::ms_UseNewBikeAnimsId("UseNewBikeAnims",0x8839BB25);
const fwMvFlagId CTaskMotionInVehicle::ms_HornIsBeingUsedId("HornIsBeingUsed",0xb96aaaff);
const fwMvFlagId CTaskMotionInVehicle::ms_BikeHornIsBeingUsedId("BikeHornIsBeingUsed",0xcb1ba548);
const fwMvFlagId CTaskMotionInVehicle::ms_IsLowriderId("IsLowrider", 0xe3e76c33);
const fwMvFlagId CTaskMotionInVehicle::ms_LowriderTransitionToArm("LowriderTransitionToArm", 0x6916bef0);
const fwMvFlagId CTaskMotionInVehicle::ms_LowriderTransitionToSteer("LowriderTransitionToSteer", 0xb3ca82f5);
const fwMvFlagId CTaskMotionInVehicle::ms_UsingLowriderArmClipset("UsingLowriderArmClipset", 0xe574996f);
const fwMvClipSetVarId CTaskMotionInVehicle::ms_EntryVarClipSetId("EntryVarClipSet",0xF7E35115);
const fwMvClipId CTaskMotionInVehicle::ms_LowLodClipId("LowLodClip",0xFBFB9F15);
const fwMvStateId CTaskMotionInVehicle::ms_InvalidStateId("InvalidState",0xF24DA9C1);

////////////////////////////////////////////////////////////////////////////////

// Statics
CTaskMotionInVehicle::Tunables CTaskMotionInVehicle::ms_Tunables;

IMPLEMENT_VEHICLE_TASK_TUNABLES(CTaskMotionInVehicle, 0xcafb85ef);

#if __STATS
PF_PAGE(LowriderBounce, "Lowdrider Bounce");
PF_GROUP(LowriderBounceData);
PF_LINK(LowriderBounce, LowriderBounceData);
PF_VALUE_FLOAT(LowriderBounceData_fBounceX, LowriderBounceData); 
PF_VALUE_FLOAT(LowriderBounceData_fBounceY, LowriderBounceData); 
#endif	// __STATS

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInVehicle::ProcessSteeringWheelArmIk(const CVehicle& rVeh, CPed& ped)
{
	if ((rVeh.GetVehicleType() == VEHICLE_TYPE_CAR || rVeh.GetVehicleType() == VEHICLE_TYPE_BOAT || rVeh.GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR) && rVeh.GetLayoutInfo()->GetUseSteeringWheelIk())
	{
		TUNE_GROUP_BOOL(IN_VEHICLE_TUNE, FORCE_STEERING_WHEEL_IK, false);
		TUNE_GROUP_BOOL(IN_VEHICLE_TUNE, ENABLE_STEERING_WHEEL_IK, true);
		if (ENABLE_STEERING_WHEEL_IK && ((ped.GetPedModelInfo() && ped.GetPedModelInfo()->IsUsingFemaleSkeleton()) || FORCE_STEERING_WHEEL_IK))
		{
			if (!ped.GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle))
			{
				if (rVeh.GetSeatManager()->GetDriver() == &ped)
				{
					const CModelSeatInfo* pModelSeatInfo = rVeh.GetVehicleModelInfo()->GetModelSeatInfo();
					if (pModelSeatInfo)
					{
						s32 iSeatIndex = rVeh.GetSeatManager()->GetPedsSeatIndex(&ped);
						s32 iDirectAccessEntryPointIndex = rVeh.GetDirectEntryPointIndexForPed(&ped);
						if (rVeh.IsEntryIndexValid(iDirectAccessEntryPointIndex) && rVeh.IsSeatIndexValid(iSeatIndex))
						{
							const CEntryExitPoint* pEntryExitPoint = pModelSeatInfo->GetEntryExitPoint(iDirectAccessEntryPointIndex);
							int iBoneIndex = pModelSeatInfo->GetBoneIndexFromSeat(pEntryExitPoint->GetSeat(SA_directAccessSeat, iSeatIndex));
							if (iBoneIndex > -1)
							{
								CTaskAimGunVehicleDriveBy* pGunDriveByTask = static_cast<CTaskAimGunVehicleDriveBy*>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY));
								const bool isThrowingProjectile = ped.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_MOUNT_THROW_PROJECTILE, true);
								const bool isShooting = pGunDriveByTask || isThrowingProjectile;

								const float fBlendInDuration = ped.GetPedResetFlag(CPED_RESET_FLAG_ForceInstantSteeringWheelIkBlendIn) ? INSTANT_BLEND_DURATION : PEDIK_ARMS_FADEIN_TIME;
								ped.GetIkManager().SetRelativeArmIKTarget(crIKSolverArms::LEFT_ARM, &rVeh, iBoneIndex, rVeh.GetLayoutInfo()->GetSteeringWheelOffset(), AIK_TARGET_WRT_IKHELPER, fBlendInDuration);

								if (!isShooting)
								{
									ped.GetIkManager().SetRelativeArmIKTarget(crIKSolverArms::RIGHT_ARM, &rVeh, iBoneIndex, rVeh.GetLayoutInfo()->GetSteeringWheelOffset(), AIK_TARGET_WRT_IKHELPER, fBlendInDuration);
								}
							}
						}
					}
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInVehicle::CheckForStill(float fTimeStep, const CVehicle& vehicle, const CPed& ped, float UNUSED_PARAM(fPitchAngle), float& fStillDelayTimer, bool bIgnorePitchCheck, bool bCheckTimeInState, float fTimeInState)
{
	if (ped.GetPedResetFlag(CPED_RESET_FLAG_PreventGoingIntoStillInVehicleState))
	{
		return false;
	}
	// Don't consider us to be still until over the min speed threshold

	if (vehicle.InheritsFromBike())
	{
		if (!static_cast<const CBike&>(vehicle).IsConsideredStill(vehicle.GetVelocityIncludingReferenceFrame()))
		{
			if (fStillDelayTimer >= 0.0f)
			{
				fStillDelayTimer = 0.0f;
			}
			return false;
		}
	}
	else
	{
		if (vehicle.GetVelocityIncludingReferenceFrame().Mag2() > ms_Tunables.m_MinSpeedForVehicleToBeConsideredStillSqr * ms_Tunables.m_MinSpeedForVehicleToBeConsideredStillSqr)
		{
			if (fStillDelayTimer >= 0.0f)
			{
				fStillDelayTimer = 0.0f;
			}
			return false;
		}
	}

	// Prevent flicking between states
	if (bCheckTimeInState && fTimeInState < ms_Tunables.m_MinTimeInCurrentStateForStill)
	{
		if (fStillDelayTimer >= 0.0f)
		{
			fStillDelayTimer = 0.0f;
		}
		return false;
	}

	const float fMinBikePitch = CTaskMotionInVehicle::ms_Tunables.m_MinPitchDefault;
	const float fMaxBikePitch = CTaskMotionInVehicle::ms_Tunables.m_MaxPitchDefault;
	float fPitchAngle = ped.GetCurrentPitch() - fMinBikePitch;
	const float fPitchRange = Abs(fMaxBikePitch - fMinBikePitch);
	fPitchAngle = fPitchAngle / fPitchRange;
	if (fPitchAngle > 1.0f) fPitchAngle = 1.0f;
	else if (fPitchAngle < 0.0f) fPitchAngle = 0.0f;

	// Only consider us to be still if not on a slope
	if (bIgnorePitchCheck || (fPitchAngle > (0.5f - ms_Tunables.m_StillPitchAngleTol) && fPitchAngle < (0.5f + ms_Tunables.m_StillPitchAngleTol)))
	{
		// If not delaying we can go to still state
		if (fStillDelayTimer < 0.0f)
		{
			return true;
		}

		// Increase delay timer, pass when we've been considered still for the required delay amount
		fStillDelayTimer += fTimeStep;

		if (fStillDelayTimer >= ms_Tunables.m_StillDelayTime)
		{
			return true;
		}	
		else
		{
			// Don't reset timer
			return false;
		}
	}

	if (fStillDelayTimer >= 0.0f)
	{
		fStillDelayTimer = 0.0f;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInVehicle::CheckForMovingForwards(const CVehicle& vehicle, CPed& ped)
{
	if (vehicle.GetVelocityIncludingReferenceFrame().Dot(VEC3V_TO_VECTOR3(vehicle.GetTransform().GetB())) > 0.75f)
	{
		return true;
	}

	if (vehicle.GetDriver() == &ped && ped.IsLocalPlayer())
	{
		CControl* pControl = ped.GetControlFromPlayer();
		if (pControl && !CTaskVehiclePlayerDrive::IsThePlayerControlInactive(pControl))
		{
			return pControl->GetVehicleAccelerate().GetNorm() > pControl->GetVehicleBrake().GetNorm() ? true : false;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInVehicle::CheckForShunt(CVehicle& vehicle, CPed& ped, float fZAcceleration)
{
	TUNE_GROUP_BOOL(THROUGH_WINDSCREEN, DISABLE_SHUNT, false);
	if (DISABLE_SHUNT)
	{
		return false;
	}

	if (ped.IsInFirstPersonVehicleCamera())
	{
		return false;
	}

	if (ped.GetPedConfigFlag(CPED_CONFIG_FLAG_IsDuckingInVehicle))
	{
#if __BANK
		AI_LOG("CheckForShunt() - Shunt animation wont play due to CPED_CONFIG_FLAG_IsDuckingInVehicle being set on the ped \n");
#endif
		return false;
	}

	if(ped.GetPedResetFlag(CPED_RESET_FLAG_PreventGoingIntoShuntInVehicleState))
	{
#if __BANK
		AI_LOG("CheckForShunt() - Shunt animation wont play due to CPED_RESET_FLAG_PreventGoingIntoShuntInVehicleState being set on the ped \n");
#endif
		return false;
	}

	bool bIsUsingTurretSeat = false;
	if (ped.GetIsInVehicle())
	{
		const CVehicleSeatAnimInfo* pSeatClipInfo = vehicle.GetSeatAnimationInfo(&ped);
		if (pSeatClipInfo && pSeatClipInfo->GetNoShunts())
		{
#if __BANK
			AI_LOG("CheckForShunt() - Shunt animation wont play due to NoShunts flag set in the seat info \n");
#endif
			return false;
		}

		const s32 iSeatIndex = vehicle.GetSeatManager()->GetPedsSeatIndex(&ped);
		bIsUsingTurretSeat = vehicle.IsTurretSeat(iSeatIndex);
	}

	// Check whether the car has undergone an impact high enough to trigger the shunt animation
	const CCollisionHistory* pCollisionHistory = vehicle.GetFrameCollisionHistory();
	if (pCollisionHistory->GetMaxCollisionImpulseMagLastFrame() > 0.0f )
	{
		const CCollisionRecord* pRecord = pCollisionHistory->GetMostSignificantCollisionRecord();
		if (pRecord)
		{
			if( (vehicle.InheritsFromBoat() || vehicle.InheritsFromBike() || vehicle.InheritsFromQuadBike() || vehicle.InheritsFromAmphibiousQuadBike()) 
				&& pRecord->m_MyCollisionNormal.z > 0.8f)
			{
				//! reject near vertical normals for jetski. These are handled in CheckForShuntHighFall().
				//! prevents doing shunts on jumps/ramps.
			}
			else
			{
				float fShuntAccelMag = ms_Tunables.m_ShuntAccelerateMag;

				if (vehicle.InheritsFromBike())
				{
					fShuntAccelMag = ms_Tunables.m_ShuntAccelerateMagBike;
				}
				else if (bIsUsingTurretSeat)
				{
					TUNE_GROUP_FLOAT(TURRET_TUNE, SHUNT_ACCEL_MAG, 0.75f, 0.0f, 10.0f, 0.01f);
					fShuntAccelMag = SHUNT_ACCEL_MAG;
				}

				float fVelocityChange = pCollisionHistory->GetMaxCollisionImpulseMagLastFrame() * vehicle.GetInvMass();
				if (fVelocityChange > fShuntAccelMag)
				{
					return true;
				}
			}
		}
	}

	TUNE_GROUP_FLOAT(IN_VEHICLE_TUNE, HELI_Z_ACCELERATION_FOR_SHUNT, 35.0f, 0.0f, 100.0f, 0.1f);
	if (vehicle.InheritsFromHeli() && fZAcceleration > HELI_Z_ACCELERATION_FOR_SHUNT)
	{
		return true;
	}

	TUNE_GROUP_FLOAT(IN_VEHICLE_TUNE, JETSKI_Z_ACCELERATION_FOR_SHUNT, 60.0f, 0.0f, 100.0f, 0.1f);
	if (vehicle.GetIsJetSki() && fZAcceleration > JETSKI_Z_ACCELERATION_FOR_SHUNT)
	{
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInVehicle::CheckForHeavyBrake(const CPed& rPed, const CVehicle& rVeh, float fYAcceleration, u32 uLastTimeLargeZAcceleration)
{
#if __BANK
	TUNE_GROUP_BOOL(IN_VEHICLE_TUNE, bForceHeavyBrake, false)
	if(bForceHeavyBrake)
	{
		return true;
	}
#endif

	// Disable heavy brake state for certain vehicles
	bool bVehicleModelBlocksHeavyBrake = rVeh.GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_NO_HEAVY_BRAKE_ANIMATION);
	if (rVeh.GetVehicleType() == VEHICLE_TYPE_BIKE || rVeh.GetVehicleType() == VEHICLE_TYPE_QUADBIKE || rVeh.GetVehicleType() == VEHICLE_TYPE_PLANE || rVeh.GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE || bVehicleModelBlocksHeavyBrake)
	{
		return false;
	}

	if (CTaskMotionInAutomobile::IsFirstPersonDrivingJetski(rPed, rVeh))
	{
		return false;
	}

	const CVehicleSeatAnimInfo* pSeatClipInfo = rVeh.GetSeatAnimationInfo(&rPed);
	if (pSeatClipInfo && pSeatClipInfo->GetUseBasicAnims())
	{
		return false;
	}

	Vector3 vVehVelocity = rVeh.IsNetworkClone() ? NetworkInterface::GetLastVelReceivedOverNetwork(&rVeh) : rVeh.GetVelocityIncludingReferenceFrame();
	if(rVeh.InheritsFromBoat())
	{
		// No heavy brake if we a boat has received a large z acceleration recently (as this can impose a large -acceleration, which could trigger this).
		TUNE_GROUP_INT(IN_VEHICLE_TUNE, LARGE_BOAT_Z_ACCEL_NO_HEAVY_BREAK_TIME, 500, 0, 5000, 100);
		if(fwTimer::GetTimeInMilliseconds() < (uLastTimeLargeZAcceleration + LARGE_BOAT_Z_ACCEL_NO_HEAVY_BREAK_TIME))
		{
			return false;	
		}
		
		// No heavy brake if boat is travelling fast enough.
		TUNE_GROUP_FLOAT(IN_VEHICLE_TUNE, LARGE_BOAT_VELOCITY_NO_HEAVY_BREAK_TIME, 15.0f, 0.0f, 50.0f, 0.1f);
		if(vVehVelocity.Mag2() > (LARGE_BOAT_VELOCITY_NO_HEAVY_BREAK_TIME*LARGE_BOAT_VELOCITY_NO_HEAVY_BREAK_TIME))
		{
			return false;
		}
	}

	// Check whether we're decelerating quickly and still going forwards
	if (fYAcceleration < ms_Tunables.m_HeavyBrakeYAcceleration)
	{
		TUNE_GROUP_FLOAT(IN_VEHICLE_TUNE, MIN_DOT_FOR_HEAVY_BRAKE, 0.85f, -1.0f, 1.0f, 0.01f);
		vVehVelocity.Normalize();
		if (vVehVelocity.Dot(VEC3V_TO_VECTOR3(rVeh.GetTransform().GetB())) >= MIN_DOT_FOR_HEAVY_BRAKE)
		{
			TUNE_GROUP_FLOAT(IN_VEHICLE_TUNE, MAX_ANG_VEL_FOR_HEAVY_BRAKE, 1.0f, 0.0f, 10.0f, 0.01f);
			if (Abs(rVeh.GetAngVelocity().z) < MAX_ANG_VEL_FOR_HEAVY_BRAKE)
			{
				return true;
			}
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInVehicle::CheckForThroughWindow(CPed& ped, CVehicle& vehicle, const Vector3& vPreviousVelocity, Vector3& vThrownForce)
{
	TUNE_GROUP_BOOL(IN_VEHICLE_TUNE, DISABLE_THROUGH_WINDSCREEN, false);
	if (DISABLE_THROUGH_WINDSCREEN)
	{
		return false;
	}

    // don't allow peds to go through the windscreen in network games as we can't guarantee a
    // ragdoll will be available on all machines
    if(NetworkInterface::IsGameInProgress())
    {
        return false;
    }

    if (ped.GetPedConfigFlag(CPED_CONFIG_FLAG_WillFlyThroughWindscreen) 
		&& vehicle.GetVehicleType() == VEHICLE_TYPE_CAR
		&& !vehicle.IsSuperDummy())
    {
	    // When driving into locates, script turn off player control to make the vehicle stop, we block going through the windscreen in this case
	    bool bPassesPlayerDriverWithNoControlTest = true;

	    CPed* pDriver = vehicle.GetDriver();
	    if (pDriver && pDriver->IsLocalPlayer())
	    {
		    CControl* pControl = pDriver->GetControlFromPlayer();
		    if (pControl && CTaskVehiclePlayerDrive::IsThePlayerControlInactive(pControl))
		    {
			    bPassesPlayerDriverWithNoControlTest = false;
		    }
	    }
		
		const float fMinVehVelocityToGoThroughWindScreen = NetworkInterface::IsGameInProgress() ? ms_Tunables.m_MinVehVelocityToGoThroughWindscreenMP : ms_Tunables.m_MinVehVelocityToGoThroughWindscreen;
        bool bPassesMinVelocityCheck = (vPreviousVelocity.Mag2() > rage::square(fMinVehVelocityToGoThroughWindScreen)) || vehicle.IsNetworkClone();

		TUNE_GROUP_BOOL(THROUGH_WINDSCREEN, DEBUG_COLLISIONS, false);
		if (bPassesPlayerDriverWithNoControlTest && (DEBUG_COLLISIONS || bPassesMinVelocityCheck))
	    {
		    const CVehicleModelInfo* pModelInfo = vehicle.GetVehicleModelInfo();
		    if (pModelInfo && !pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_DISABLE_THROUGH_WINDSCREEN))
		    {
			    if (!ped.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE))
			    {
				    s32 iSeatIndex = vehicle.GetSeatManager()->GetPedsSeatIndex(&ped);

				    if (vehicle.IsSeatIndexValid(iSeatIndex))
				    {
						const s32 iEntryPointIndex = vehicle.GetDirectEntryPointIndexForSeat(iSeatIndex);
						if (!vehicle.IsEntryIndexValid(iEntryPointIndex))
						{
							return false;
						}

						VehicleEnterExitFlags vehicleFlags;
						const fwMvClipSetId exitClipsetId = CTaskExitVehicleSeat::GetExitClipsetId(&vehicle, iEntryPointIndex, vehicleFlags);
						if (exitClipsetId == CLIP_SET_ID_INVALID)
						{
							return false;
						}

						fwClipSet* pClipSet = fwClipSetManager::GetClipSet(exitClipsetId);
						if (!pClipSet || !pClipSet->IsStreamedIn_DEPRECATED())
						{
							return false;
						}

					    const CVehicleSeatInfo* pSeatInfo = vehicle.GetSeatInfo(iSeatIndex);

					    if (pSeatInfo && pSeatInfo->GetIsFrontSeat())
					    {
							if(!vehicle.IsNetworkClone())
							{
								const float fRequiredVelocityDelta = ped.IsAPlayerPed() ? (NetworkInterface::IsGameInProgress() ? ms_Tunables.m_VelocityDeltaThrownOutPlayerMP : ms_Tunables.m_VelocityDeltaThrownOutPlayerSP) : ms_Tunables.m_VelocityDeltaThrownOut;
								aiDebugf2("fRequiredVelocityDelta: %.4f", fRequiredVelocityDelta);

								const Vector3 vCurrentVelocity = vehicle.GetVelocity();

								aiDebugf2("Frame: %i, Velocity P: %.2f,%.2f,%.2f, C: %.2f,%.2f,%.2f, COLP: %.2f,%.2f,%.2f", fwTimer::GetFrameCount(), vPreviousVelocity.x, vPreviousVelocity.y, vPreviousVelocity.z, vCurrentVelocity.x, vCurrentVelocity.y, vCurrentVelocity.z, vehicle.GetCollider() ?VEC3V_ARGS(vehicle.GetCollider()->GetLastVelocity()) : VEC3V_ARGS(Vec3V(V_ZERO)));
								const Vector3 vImpactChangeInVel = vCurrentVelocity - vPreviousVelocity;
								const float fManualVelocityDelta = vImpactChangeInVel.Mag();
								aiDebugf2("Collision velocity delta: %.4f", fManualVelocityDelta);

								//Set the damage entity because it will be needed in CTaskExitVehicleSeat::ApplyDamageThroughWindscreen()
								// to set the correct damage entity.
								const CCollisionHistory* pCollisionHistory = vehicle.GetFrameCollisionHistory();
								const CCollisionRecord* pCollisionRecord = pCollisionHistory->GetMostSignificantCollisionRecord();
								if(pCollisionRecord && pCollisionHistory->GetMaxCollisionImpulseMagLastFrame() > 0.0f)
								{
#if DEBUG_DRAW
									static char szText[512];	
									Vec3V vVehPos = vehicle.GetTransform().GetPosition();
									vVehPos.SetZf(vVehPos.GetZf() + 1.0f);
									Vec3V vColNorm = vVehPos + VECTOR3_TO_VEC3V(pCollisionRecord->m_MyCollisionNormal);
									Vec3V vVehFwd = vVehPos + vehicle.GetTransform().GetB();
									ms_debugDraw.AddArrow(vVehPos, vVehFwd, 0.25f, Color_blue, 4000);

									TUNE_GROUP_BOOL(THROUGH_WINDSCREEN, RENDER_VELOCITY_DIRECTION, true);
									if (RENDER_VELOCITY_DIRECTION)
									{
										Vec3V vVehPrevVel = vVehPos + Normalize(RCC_VEC3V(vPreviousVelocity)) + Vec3V(0.0f, 0.0f, 1.0f); 
										Vec3V vVehCurrVel = vVehPos + Normalize(RCC_VEC3V(vCurrentVelocity)) + Vec3V(0.0f, 0.0f, 1.0f);
										ms_debugDraw.AddArrow(vVehPos, vVehPrevVel, 0.25f, Color_brown, 4000);
										ms_debugDraw.AddArrow(vVehPos, vVehCurrVel, 0.25f, Color_cyan, 4000);
									}
#endif // DEBUG_DRAW
									taskAssertf(pCollisionRecord->m_MyCollisionNormal.Mag() <= (1.0f+0.01f), "Collision normal isn't normalised %f/%f/%f", pCollisionRecord->m_MyCollisionNormal.x, pCollisionRecord->m_MyCollisionNormal.y, pCollisionRecord->m_MyCollisionNormal.z);

									const float fDot = -pCollisionRecord->m_MyCollisionNormal.Dot(MAT34V_TO_MATRIX34(vehicle.GetMatrix()).b);
									aiDebugf2("Angle of impact (fDot): %.4f", fDot);
									TUNE_GROUP_FLOAT(THROUGH_WINDSCREEN, MIN_ANGLE_TO_GO_THROUGH, 0.9f, 0.0f, 1.0f, 0.01f);
									if (fDot <= MIN_ANGLE_TO_GO_THROUGH)
									{
#if DEBUG_DRAW
										if (DEBUG_COLLISIONS)
										{
											formatf(szText, "Frame: %i, IMPACT ANGLE NOT WITHIN RANGE %.3f <= (R: %.3f)", fwTimer::GetFrameCount(), fDot, MIN_ANGLE_TO_GO_THROUGH);
											ms_debugDraw.AddArrow(vVehPos, vColNorm, 0.25f, Color_red, 6000);
											ms_debugDraw.AddText(vVehPos, 0, -20, szText, Color_red, 6000);
										}
#endif // DEBUG_DRAW
										return false;
									}

									// Seem to get weird deltas via manual computation, so we also test the delta from the collision at a lower threshold
									TUNE_GROUP_FLOAT(THROUGH_WINDSCREEN, LARGE_DECELERATION_FROM_COLLISION, 17.0f, 0.0f, 40.0f, 0.1f);
									TUNE_GROUP_FLOAT(THROUGH_WINDSCREEN, LARGE_DECELERATION_FROM_COLLISION_VEHICLE, 10.0f, 0.0f, 40.0f, 0.1f);

									float fDecelerationFromCollisionThreshold;

									CEntity* colEntity = pCollisionRecord->m_pRegdCollisionEntity.Get();
									if (colEntity && colEntity->GetIsTypeVehicle())
									{
										fDecelerationFromCollisionThreshold = LARGE_DECELERATION_FROM_COLLISION_VEHICLE;

										CPed* driver = static_cast< CVehicle* >(colEntity)->GetDriver();
										if (driver && driver->IsPlayer() && !ped.IsPlayer())
										{
											vehicle.SetWeaponDamageInfo(colEntity, WEAPONTYPE_RAMMEDBYVEHICLE, fwTimer::GetTimeInMilliseconds());
										}
									}
									else
									{
										fDecelerationFromCollisionThreshold = LARGE_DECELERATION_FROM_COLLISION;
									}

									aiDebugf2("Frame: %i, Collision Impulse Mag: %.4f, TimeStep %.4f, Scaled Result : %.4f", fwTimer::GetFrameCount(), pCollisionHistory->GetMaxCollisionImpulseMagLastFrame(), fwTimer::GetTimeStep(), pCollisionHistory->GetMaxCollisionImpulseMagLastFrame() / fwTimer::GetTimeStep());
									const float fVelocityChange = pCollisionHistory->GetMaxCollisionImpulseMagLastFrame() * vehicle.GetInvMass();

									TUNE_GROUP_BOOL(THROUGH_WINDSCREEN, USE_LARGEST_VELOCITY_DELTA, true);
									float fVelDelta = fVelocityChange;
									if (USE_LARGEST_VELOCITY_DELTA && fManualVelocityDelta > fVelDelta)
									{
										fVelDelta = fManualVelocityDelta;
									}

									TUNE_GROUP_FLOAT(THROUGH_WINDSCREEN, LARGE_COLLISION_IMPULSE_MAG, 2000.0f, 0.0f, 100000.0f, 10.0f);
									const bool bLargeCollision = pCollisionHistory->GetMaxCollisionImpulseMagLastFrame() > LARGE_COLLISION_IMPULSE_MAG;

									const bool bLargeDecelerationFromCollision = fVelocityChange > fDecelerationFromCollisionThreshold;
									const bool bLargeDecelerationLargest = fVelDelta > fRequiredVelocityDelta;
									const bool bPassedDecelerationTest = bLargeDecelerationFromCollision && bLargeDecelerationLargest; 

									if (DEBUG_COLLISIONS || (bLargeCollision && bPassedDecelerationTest))
									{
#if DEBUG_DRAW
										Color32 debugColor = Color_pink;
										if (!bPassesMinVelocityCheck)
										{
											formatf(szText, "NOT GOING FAST ENOUGH");
											debugColor = Color_orange;
										}
										else if (bLargeCollision && bPassedDecelerationTest)
										{
											formatf(szText, "SHOULD GO THROUGH WINDSCREEN");
											debugColor = Color_green;
										}
										else if (bLargeCollision)
										{
											formatf(szText, "TOO SMALL VELOCITY DELTA M:%s, C:%s", bLargeDecelerationLargest ? "TRUE" : "FALSE", bLargeDecelerationFromCollision ? "TRUE" : "FALSE");
											debugColor = Color_black;
										}
										else if (bLargeDecelerationLargest)
										{
											formatf(szText, "TOO SMALL COLLISION");
											debugColor = Color_yellow;
										}
		
										ms_debugDraw.AddArrow(vVehPos, vColNorm, 0.25f, debugColor, 6000);

										TUNE_GROUP_BOOL(THROUGH_WINDSCREEN, ONLY_RENDER_LARGE_COLLISIONS, true);
										TUNE_GROUP_BOOL(THROUGH_WINDSCREEN, RENDER_COLLISION_VERBOSE_TEXT, true);

										ms_debugDraw.AddText(vVehPos, 0, -20, szText, debugColor, 6000);

										if (DEBUG_COLLISIONS && RENDER_COLLISION_VERBOSE_TEXT)
										{
											if (!ONLY_RENDER_LARGE_COLLISIONS || bLargeCollision)
											{
												s32 iVerticalOffset = (fwTimer::GetFrameCount() % 10) * 10;		
												formatf(szText, "F:%i,I:%.2f,D:%.2f(A:%.2f),P:%.2f,C,%.2f", fwTimer::GetFrameCount(), pCollisionHistory->GetMaxCollisionImpulseMagLastFrame(), fVelDelta, fVelocityChange, vPreviousVelocity.Mag(), vCurrentVelocity.Mag());
												ms_debugDraw.AddText(vVehPos, 0, iVerticalOffset, szText, debugColor, 6000);
											}
										}

#endif // DEBUG_DRAW
										vThrownForce = -pCollisionRecord->m_MyCollisionNormal * fVelocityChange;

										Displayf("[%d] THROUGHWINDSCREEN - vThrownForce: %f:%f:%f * %f", 
											fwTimer::GetFrameCount(),
											pCollisionRecord->m_MyCollisionNormal.x,
											pCollisionRecord->m_MyCollisionNormal.y,
											pCollisionRecord->m_MyCollisionNormal.z,
											fVelocityChange);

										taskAssertf(vThrownForce.Mag2() <= (1000.0f*1000.0f), "Thrown Force is invalid. Normal: %f/%f/%f VelocityChange: %f", pCollisionRecord->m_MyCollisionNormal.x, pCollisionRecord->m_MyCollisionNormal.y, pCollisionRecord->m_MyCollisionNormal.z, fVelocityChange);
										
										TUNE_GROUP_BOOL(THROUGH_WINDSCREEN, ALLOW_THROUGH_WINDSCREEN, true);
										return ALLOW_THROUGH_WINDSCREEN && bPassesMinVelocityCheck && bLargeCollision && bPassedDecelerationTest;
									}
								}
							}
                            else
                            {
                                // if we are in the front seat of a vehicle owned by another machine we go through the windscreen along with the driver
                                if(pDriver && (pDriver != &ped))
                                {
                                    CTaskExitVehicle *pExitVehicleTask = static_cast<CTaskExitVehicle *>(pDriver->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE));

                                    if(pExitVehicleTask && pExitVehicleTask->IsFlagSet(CVehicleEnterExitFlags::ThroughWindscreen))
                                    {
                                        return true;
                                    }
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

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInVehicle::CheckForReverse(const CVehicle& vehicle, const CPed& ped, bool bFirstPerson, bool bIgnoreThrottleCheck, bool bIsPanicking)
{
	if (vehicle.GetVehicleModelInfo() && vehicle.GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_NO_REVERSING_ANIMATION))
	{
		return false;
	}
	
	if (ped.GetPedResetFlag(CPED_RESET_FLAG_DisableInVehicleActions))
	{
		return false;
	}

	if(bIsPanicking)
	{
		return false;
	}

	TUNE_GROUP_BOOL(FIRST_PERSON_STEERING, bEnableFirstPersonReverse, false);
	if(bFirstPerson && !bEnableFirstPersonReverse)
	{
		return false;
	}

	if (CTaskMotionInAutomobile::IsFirstPersonDrivingJetski(ped, vehicle) || CTaskMotionInAutomobile::IsFirstPersonDrivingQuadBike(ped, vehicle))
	{
		return false;
	}

	if (const_cast<CVehicle&>(vehicle).IsInAir())
	{
		return false;
	}

	if (MI_BIKE_OPPRESSOR2.IsValid() && vehicle.GetModelIndex() == MI_BIKE_OPPRESSOR2)
	{
		return false;
	}

	const bool bIsBike = vehicle.InheritsFromBike();
	const bool bIsTrike = vehicle.IsTrike();
	if (vehicle.IsDriver(&ped) || bIsBike || bIsTrike)
	{
		const Vector3 vVehVelocity = vehicle.GetVelocityIncludingReferenceFrame();
		dev_float MAX_SPEED_TO_PLAY_REVERSE_ANIM = 5.0f;
		if ((bIsBike || bIsTrike) && vVehVelocity.Mag2() > square(MAX_SPEED_TO_PLAY_REVERSE_ANIM))
		{
			return false;
		}

		dev_float SPEED_TO_PLAY_REVERSE_CLIP_NPC = -1.0f;
		dev_float SPEED_TO_PLAY_REVERSE_CLIP_PLAYER = -0.5f;
		dev_float SPEED_TO_PLAY_REVERSE_CLIP_PLAYER_BICYCLE = -0.5f;
		dev_float SPEED_TO_PLAY_REVERSE_CLIP_PLAYER_BICYCLE_THROTTLE = -0.1f;
		const float fForwardMoveSpeed = DotProduct(vVehVelocity, VEC3V_TO_VECTOR3(vehicle.GetTransform().GetB()));

		// block Steering clips if the bike is stationary		
		float fLookBehindSpeedThreshold = ped.IsAPlayerPed() ? SPEED_TO_PLAY_REVERSE_CLIP_PLAYER : SPEED_TO_PLAY_REVERSE_CLIP_NPC;
		if (vehicle.InheritsFromBicycle())
		{
			fLookBehindSpeedThreshold = vehicle.GetThrottle() < 0.0f ? SPEED_TO_PLAY_REVERSE_CLIP_PLAYER_BICYCLE_THROTTLE : SPEED_TO_PLAY_REVERSE_CLIP_PLAYER_BICYCLE;
		}

		if( (bIgnoreThrottleCheck || vehicle.GetThrottle() < 0.0f) && fForwardMoveSpeed < fLookBehindSpeedThreshold )
		{
			return true;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInVehicle::CheckForClosingDoor(const CVehicle& vehicle, const CPed& ped, bool bIgnoreTaskCheck, s32 iTargetSeatIndex, s32 iEntryPointEnteringFrom, bool bCloseDoorInterruptTagCheck)
{
	vehicledoorDebugf3("CTaskMotionInVehicle::CheckForClosingDoor--vehicle[%p][%s][%s] ped[%p][%s][%s] bIgnoreTaskCheck[%d] iTargetSeatIndex[%d] iEntryPointEnteringFrom[%d] bCloseDoorInterruptTagCheck[%d] ",&vehicle,vehicle.GetModelName(),vehicle.GetNetworkObject() ? vehicle.GetNetworkObject()->GetLogName() : "", &ped, ped.GetModelName(), ped.GetNetworkObject() ? ped.GetNetworkObject()->GetLogName() : "",bIgnoreTaskCheck,iTargetSeatIndex,iEntryPointEnteringFrom,bCloseDoorInterruptTagCheck);

	if (ms_Tunables.m_DisableCloseDoor)
	{
		vehicledoorDebugf3("CTaskMotionInVehicle::CheckForClosingDoor--(ms_Tunables.m_DisableCloseDoor)-->return false");
		return false;
	}

	if (ped.GetPedResetFlag(CPED_RESET_FLAG_DisableInVehicleActions))
	{
		vehicledoorDebugf3("CTaskMotionInVehicle::CheckForClosingDoor--(ped.GetPedResetFlag(CPED_RESET_FLAG_DisableInVehicleActions))-->return false");
		return false;
	}

	if (CTaskMotionInAutomobile::WantsToDuck(ped))
		return false;

	const s32 m_iTargetEntryPoint = vehicle.GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iTargetSeatIndex, &vehicle);
	const CVehicleEntryPointInfo* pEntryInfo =  vehicle.IsEntryIndexValid(m_iTargetEntryPoint) ? vehicle.GetEntryInfo(m_iTargetEntryPoint) : NULL;

	if ((pEntryInfo && pEntryInfo->GetIsPlaneHatchEntry()) || vehicle.GetLayoutInfo()->GetAutomaticCloseDoor())
	{
		vehicledoorDebugf3("CTaskMotionInVehicle::CheckForClosingDoor--((pEntryInfo && pEntryInfo->GetIsPlaneHatchEntry()) || vehicle.GetLayoutInfo()->GetAutomaticCloseDoor())-->return false");
		return false;
	}

	if (ped.GetPedResetFlag(CPED_RESET_FLAG_IsDoingDriveby))
	{
		vehicledoorDebugf3("CTaskMotionInVehicle::CheckForClosingDoor--(ped.GetPedResetFlag(CPED_RESET_FLAG_IsDoingDriveby))-->return false");
		return false;
	}

	if (ped.GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed))
	{
		vehicledoorDebugf3("CTaskMotionInVehicle::CheckForClosingDoor--(ped.GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed))-->return false");
		return false;
	}

	if (ped.GetPedResetFlag(CPED_RESET_FLAG_DontCloseVehicleDoor))
	{
		vehicledoorDebugf3("CTaskMotionInVehicle::CheckForClosingDoor--(ped.GetPedResetFlag(CPED_RESET_FLAG_DontCloseVehicleDoor))-->return false");
		return false;
	}

	if (CheckForAttachedPedClimbingStairs(vehicle))
	{
		vehicledoorDebugf3("CTaskMotionInVehicle::CheckForClosingDoor--(CheckForAttachedPedClimbingStairs(vehicle))-->return false");
		return false;
	}

	s32 iDirectEntryPoint = iEntryPointEnteringFrom > -1 ? iEntryPointEnteringFrom : vehicle.GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(vehicle.GetSeatManager()->GetPedsSeatIndex(&ped), &vehicle);
	if (!vehicle.IsEntryIndexValid(iDirectEntryPoint))
	{
		vehicledoorDebugf3("CTaskMotionInVehicle::CheckForClosingDoor--!IsEntryIndexValid-->return false");
		return false;
	}

	bool bIgnoreVelocityChecks = false;
	bool bInSeatOpenedByDriveby = false;

	// Special case exceptions to triggering door closing regardless of vehicle velocity
	const bool bIsDriver = vehicle.GetDriver() == &ped ? true : false;
	if (bIsDriver)
	{
		// Buses (two doors)
		if (vehicle.GetSecondDoorFromEntryPointIndex(iDirectEntryPoint))
		{
			AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED_REF(ped, "CTaskMotionInVehicle::CheckForClosingDoor ignoring velocity checks because ped %s is driving bus\n", AILogging::GetDynamicEntityNameSafe(&ped));
			bIgnoreVelocityChecks = true;
		}
	}
	else
	{
		const s32 iSeatIndex = vehicle.GetSeatManager()->GetPedsSeatIndex(&ped);
		if (vehicle.IsSeatIndexValid(iSeatIndex))
		{
			const CVehicleSeatInfo* pSeatInfo = vehicle.GetSeatInfo(iSeatIndex);
			if (pSeatInfo && !pSeatInfo->GetIsFrontSeat())
			{
				const CVehicleDriveByInfo* pDrivebyInfo = CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(&ped);
				if (pDrivebyInfo && pDrivebyInfo->GetNeedToOpenDoors())
				{
					AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED_REF(ped, "CTaskMotionInVehicle::CheckForClosingDoor ignoring velocity checks because ped %s is in vehicle requiring doors open for driveby (back of van, sliding doors, etc.)\n", AILogging::GetDynamicEntityNameSafe(&ped));
					bIgnoreVelocityChecks = true;
					bInSeatOpenedByDriveby = true;
				}
			}
		}
	}

	if (!ped.GetPedResetFlag(CPED_RESET_FLAG_IgnoreVelocityWhenClosingVehicleDoor) && !bIgnoreVelocityChecks)
	{
		if (!ped.GetIsDrivingVehicle() && vehicle.GetVelocityIncludingReferenceFrame().Mag2() > rage::square(ms_Tunables.m_MaxVehSpeedToConsiderClosingDoor))
		{
			vehicledoorDebugf3("CTaskMotionInVehicle::CheckForClosingDoor--!CPED_RESET_FLAG_IgnoreVelocityWhenClosingVehicleDoor-->return false");
			return false;
		}
	}

	const CVehicleEntryPointAnimInfo* pAnimInfo = vehicle.GetEntryAnimInfo(iDirectEntryPoint);	
	const bool bCanCloseDoor = pAnimInfo ? !pAnimInfo->GetDontCloseDoorInside() : false;
	if (!bCanCloseDoor)
	{
		vehicledoorDebugf3("CTaskMotionInVehicle::CheckForClosingDoor--!bCanCloseDoor-->return false");
		return false;
	}

	const bool bTwoDoorsOneSeat = vehicle.GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_TWO_DOORS_ONE_SEAT);
	const CCarDoor* pDoor = NULL;
	if (iEntryPointEnteringFrom != -1)
	{
		pDoor = GetDirectAccessEntryDoorFromEntryPoint(vehicle, iDirectEntryPoint);
	}
	else if (bTwoDoorsOneSeat)
	{
		pDoor = GetOpenDoorForDoubleDoorSeat(vehicle, iTargetSeatIndex > -1 ? iTargetSeatIndex : vehicle.GetSeatManager()->GetPedsSeatIndex(&ped), iDirectEntryPoint);
	}
	else
	{
		pDoor = GetDirectAccessEntryDoorFromPed(vehicle, ped);
	}

	if (!pDoor || !pDoor->GetIsIntact(&vehicle))
	{
		vehicledoorDebugf3("CTaskMotionInVehicle::CheckForClosingDoor--(!pDoor || !pDoor->GetIsIntact(&vehicle))-->return false");
		return false;
	}

	TUNE_GROUP_FLOAT(IN_VEHICLE_TUNE, DOOR_ALREADY_SWINGING_SHUT_SPEED, 2.0f, 0.0f, 5.0f, 0.001f);
	if (pDoor->GetCurrentSpeed() >= DOOR_ALREADY_SWINGING_SHUT_SPEED)
	{
		vehicledoorDebugf3("CTaskMotionInVehicle::CheckForClosingDoor--(pDoor->GetCurrentSpeed() >= DOOR_ALREADY_SWINGING_SHUT_SPEED)-->return false");
		return false;
	}

	TUNE_GROUP_BOOL(IN_VEHICLE_TUNE, DONT_ALLOW_CLOSE_DOOR_WHEN_GOING_BACK, true);
	if (DONT_ALLOW_CLOSE_DOOR_WHEN_GOING_BACK)
	{
		float fVehicleVelocity = vehicle.GetVelocityIncludingReferenceFrame().Dot(VEC3V_TO_VECTOR3(vehicle.GetTransform().GetB()));
		if (fVehicleVelocity < -0.01f)
		{
			vehicledoorDebugf3("CTaskMotionInVehicle::CheckForClosingDoor--(fVehicleVelocity %f < -0.01f)-->return false", fVehicleVelocity);
			return false;
		}
	}

	if (!CheckIfStairsConditionPasses(vehicle, ped))
	{
		vehicledoorDebugf3("CTaskMotionInVehicle::CheckForClosingDoor--(!CheckIfStairsConditionPasses(vehicle, ped))-->return false");
		return false;
	}

	if (bIgnoreTaskCheck || (!ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE) 
		&& !ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE)))
	{
		taskFatalAssertf(vehicle.GetVehicleModelInfo(), "Vehicle has no model info");
		if (!vehicle.GetVehicleModelInfo()->GetIsBike() && !vehicle.GetVehicleModelInfo()->GetIsQuadBike() && !vehicle.GetVehicleModelInfo()->GetIsAmphibiousQuad())	// Bikes have a door, but not a real one that we can close!
		{
			// Make sure the ped is the driver for these checks
			// AI conditions
			const float fAbsThrottle = Abs(rage::Clamp(vehicle.GetThrottle() - vehicle.GetBrake(), 0.0f, 1.0f));
			bool bNotAcceleratingReversingOrTurning = bIgnoreVelocityChecks || !bIsDriver || (fAbsThrottle <= ms_Tunables.m_MaxAbsThrottleForCloseDoor && vehicle.GetSteerAngle() <= 0.05f);

			// Player conditions
			if (!bIgnoreVelocityChecks)
			{
				if (bIsDriver && ped.IsLocalPlayer())
				{
					const CControl *pControl = ped.GetControlFromPlayer();
					bNotAcceleratingReversingOrTurning = !pControl || (pControl->GetVehicleAccelerate().IsUp(ioValue::ANALOG_BUTTON_DOWN_THRESHOLD) && 
						pControl->GetVehicleSteeringLeftRight().GetNorm() == 0.0f && 
						pControl->GetVehicleBrake().IsUp(ioValue::ANALOG_BUTTON_DOWN_THRESHOLD));
				}
				else if (NetworkInterface::IsGameInProgress() && vehicle.IsNetworkClone() && !bCloseDoorInterruptTagCheck)
				{
					bNotAcceleratingReversingOrTurning = vehicle.GetRemoteDriverDoorClosing();
					if (bNotAcceleratingReversingOrTurning)
					{
						vehicledoorDebugf3("CTaskMotionInVehicle::CheckForClosingDoor--bIsDriver[1]--IsNetworkClone--GetRemoteDriverDoorClosing[1]=bNotAcceleratingReveringOrTurning-->return true");
						return true;
					}
				}
			}

			vehicledoorDebugf3("CTaskMotionInVehicle::CheckForClosingDoor--bIsDriver[%d] fAbsThrottle[%f] bNotAcceleratingReversingOrTurning[%d] GetRemoteDriverDoorClosing[%d]",bIsDriver,fAbsThrottle,bNotAcceleratingReversingOrTurning,vehicle.GetRemoteDriverDoorClosing());

			bool bPyroPassenger = !bIsDriver && MI_PLANE_PYRO.IsValid() && vehicle.GetModelIndex() == MI_PLANE_PYRO;

			// if the driver isn't directly controlling the vehicle
			if (!bPyroPassenger && bNotAcceleratingReversingOrTurning)
			{
				// close driver door if it is open, and not been forced open via a script command, etc
				bool bDoorForcedOpen = pDoor->GetFlag(CCarDoor::DRIVEN_NORESET) || pDoor->GetFlag(CCarDoor::DRIVEN_NORESET_NETWORK);
				if(pDoor->GetDoorRatio() >= ms_Tunables.m_MinRatioForClosingDoor && (!bDoorForcedOpen || bCloseDoorInterruptTagCheck || bInSeatOpenedByDriveby))
				{
					// The IsDoorClearOfPeds check can get out of sync if the ped detected is not in the same position,
					// allow this ped to close the door if remote ped is using the door
					const bool bPassesDoorSpeedCheck = bCloseDoorInterruptTagCheck ? true : Abs(pDoor->GetCurrentSpeed()) < ms_Tunables.m_MaxDoorSpeedToConsiderClosingDoor;
					if (bPassesDoorSpeedCheck && (pDoor->GetPedRemoteUsingDoor() || CTaskCloseVehicleDoorFromInside::IsDoorClearOfPeds(vehicle, ped)))
					{
						vehicledoorDebugf3("CTaskMotionInVehicle::CheckForClosingDoor-->return true");
						return true;
					}
				}
			}
		}
	}

	vehicledoorDebugf3("CTaskMotionInVehicle::CheckForClosingDoor--bIgnoreTaskCheck[%d]--final-->return false",bIgnoreTaskCheck);
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInVehicle::CheckForStartingEngine(const CVehicle& vehicle, const CPed& ped)
{
	// Do not handcuffed peds to start engine.
	if(ped.GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed))
	{
		return false;
	}

	if (ped.GetPedConfigFlag(CPED_CONFIG_FLAG_DisableStartEngine))
	{
		return false;
	}

	// Do not start engine for trailers
	if(vehicle.InheritsFromTrailer())
	{
		return false;
	}

	// Engine of the OPPRESSOR2 is turned off when above height limit, so don't allow us to restart the engine until back under the limit
	if (MI_BIKE_OPPRESSOR2.IsValid() && vehicle.GetModelIndex() == MI_BIKE_OPPRESSOR2)
	{
		float heightAboveCeiling = vehicle.HeightAboveCeiling(vehicle.GetTransform().GetPosition().GetZf());
		if (heightAboveCeiling > 0.0f)
		{
			return false;
		}
	}

	// Do not start engine if EMP effect is active
	if (vehicle.GetExplosionEffectEMP())
	{
		return false;
	}

	if (vehicle.GetStatus() != STATUS_WRECKED && vehicle.GetDriver() == &ped)
	{
		const bool bPlayingCarRecording = vehicle.IsRunningCarRecording();
		bool bShouldTryAndStartCar = vehicle.GetVehicleDamage()->GetPetrolTankHealth() > 0.0f && !bPlayingCarRecording;

		if(vehicle.InheritsFromPlane() && ped.IsPlayer() && (((CPlane*)&vehicle)->IsEngineTurnedOffByPlayer() || ((CPlane*)&vehicle)->IsEngineTurnedOffByScript()))
		{
			bShouldTryAndStartCar = false;
		}

		// Turn the engine on if its not already
		const bool bEngineNeedsStarting = !(vehicle.m_nVehicleFlags.bEngineOn || vehicle.m_nVehicleFlags.bEngineStarting);

		float throttle = vehicle.GetThrottle();

		if(vehicle.GetIsRotaryAircraft())
		{
			throttle = ((CRotaryWingAircraft*)&vehicle)->GetThrottleControl();
		}
		else if(vehicle.InheritsFromPlane())
		{
			throttle = ((CPlane*)&vehicle)->GetThrottleControl();
		}

		// This flag is only relevant for player drivers
		if (vehicle.m_bOnlyStartVehicleEngineOnThrottle && ped.IsAPlayerPed())
		{
			if (vehicle.GetVehicleType() == VEHICLE_TYPE_CAR && ped.GetControlFromPlayer() && !ped.GetPlayerInfo()->AreControlsDisabled())
			{
				// Cars with the engine off have forced brakes applied, which means GetThrottle() returns 0, so use input directly
				throttle = ped.GetControlFromPlayer()->GetVehicleAccelerate().GetNorm01();
			}

			if (Abs(throttle) < 0.01f)
			{
				return false;
			}
		}

		if( bShouldTryAndStartCar &&
			bEngineNeedsStarting &&
			( throttle > 0.0f || // Only try to restart the car if the engine is screwed if wont start if the accelerator is held
			( ped.IsPlayer() && !ped.GetPlayerInfo()->AreControlsDisabled() ) ) )
		{
			if (!ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE))
			{
				return true;
			}
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInVehicle::CheckForHotwiring(const CVehicle& vehicle, const CPed& ped)
{
	if (ped.GetPedResetFlag(CPED_RESET_FLAG_DisableInVehicleActions))
		return false;

	// Do not handcuffed peds to hotwire.
	if(ped.GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed))
	{
		return false;
	}

	if (vehicle.GetDriver() == &ped)
	{
		if (!vehicle.m_nVehicleFlags.bEngineOn && vehicle.m_nVehicleFlags.bCarNeedsToBeHotwired && (vehicle.GetVehicleType() == VEHICLE_TYPE_CAR))
		{
			if (!vehicle.IsLawEnforcementVehicle() || !ped.GetPedConfigFlag(CPED_CONFIG_FLAG_WillNotHotwireLawEnforcementVehicle))
			{
				return true;
			}
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInVehicle::CheckForChangingStation(const CVehicle& NA_RADIO_ENABLED_ONLY(vehicle), const CPed& NA_RADIO_ENABLED_ONLY(ped))
{
#if NA_RADIO_ENABLED

	// Do not handcuffed peds to change radio station.
	if(ped.GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed))
	{
		return false;
	}
	
	// Block state in certain vehicles (to prevent head clipping through roof during animation)
	if (MI_CAR_INFERNUS2.IsValid() && vehicle.GetModelIndex() == MI_CAR_INFERNUS2)
	{
		return false;
	}

	// SP logic: only local player
	const bool bConsiderPedForAnimationSP = !NetworkInterface::IsGameInProgress() && ped.IsLocalPlayer();

	// MP logic: local or remote whenever they change the radio in local's vehicle
	const bool bConsiderPedForAnimationMP = NetworkInterface::IsGameInProgress() && ped.IsPlayer() && CGameWorld::FindLocalPlayerVehicle()== &vehicle && g_RadioAudioEntity.MPDriverHasTriggeredRadioChange();

	const bool bConsiderPedForAnimation = bConsiderPedForAnimationSP | bConsiderPedForAnimationMP;

	// We only have anims for the driver changing the station
	if (bConsiderPedForAnimation && vehicle.GetDriver() == &ped && g_RadioAudioEntity.IsRetuningVehicleRadio() && vehicle.GetVehicleType() != VEHICLE_TYPE_BIKE)
	{
		return true;
	}
#endif

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInVehicle::CheckForAttachedPedClimbingStairs(const CVehicle& rVeh)
{
	if (rVeh.GetVehicleType() == VEHICLE_TYPE_PLANE)
	{
		const fwEntity* pChildAttachment = rVeh.GetChildAttachment();
		while (pChildAttachment)
		{
			if(static_cast<const CEntity*>(pChildAttachment)->GetIsTypePed())
			{
				const CPed* pAttachedPed = static_cast<const CPed*>(pChildAttachment);
				if (pAttachedPed->GetAttachState() != ATTACH_STATE_PED_IN_CAR)
				{
					bool bClimbing = false;
					s32 iTargetEntry = -1;

					const CTaskEnterVehicle* pEnterTask = static_cast<const CTaskEnterVehicle*>(pAttachedPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
					if (pEnterTask && pEnterTask->GetState() == CTaskEnterVehicle::State_ClimbUp)
					{
						bClimbing = true;
						iTargetEntry = pEnterTask->GetTargetEntryPoint();
					}
					const CTaskExitVehicle* pExitTask = static_cast<const CTaskExitVehicle*>(pAttachedPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE));
					if (pExitTask && pExitTask->GetState() == CTaskExitVehicle::State_ClimbDown)
					{
						bClimbing = true;
						iTargetEntry = pExitTask->GetTargetEntryPoint();
					}

					if (bClimbing)
					{
						const CVehicleEntryPointInfo* pEntryInfo =  rVeh.IsEntryIndexValid(iTargetEntry) ? rVeh.GetEntryInfo(iTargetEntry) : NULL;
						if (rVeh.GetLayoutInfo()->GetClimbUpAfterOpenDoor() || (pEntryInfo && pEntryInfo->GetIsPlaneHatchEntry()))
						{
							return true;
						}
					}
				}
			}
			pChildAttachment = pChildAttachment->GetSiblingAttachment();
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInVehicle::CheckIfStairsConditionPasses(const CVehicle& rVeh, const CPed& rPed)
{
	s32 iDirectEntryIndex = rVeh.GetDirectEntryPointIndexForPed(&rPed);
	if (rVeh.IsEntryIndexValid(iDirectEntryIndex))
	{
		if (rVeh.GetLayoutInfo()->GetClimbUpAfterOpenDoor() || rVeh.GetEntryInfo(iDirectEntryIndex)->GetIsPlaneHatchEntry())
		{
			s32 iCurrentSeatIndex = rVeh.GetSeatManager()->GetPedsSeatIndex(&rPed);
			if (rVeh.IsSeatIndexValid(iCurrentSeatIndex))
			{
				s32 iIndirectSeatIndex = rVeh.GetVehicleModelInfo()->GetModelSeatInfo()->GetShuffleSeatForSeat(iDirectEntryIndex, iCurrentSeatIndex);
				const CPed* pPedInOrEnteringPassengerSeat = CTaskVehicleFSM::GetPedInOrUsingSeat(rVeh, iIndirectSeatIndex);
				if (pPedInOrEnteringPassengerSeat && pPedInOrEnteringPassengerSeat->GetPedResetFlag(CPED_RESET_FLAG_IsEnteringOrExitingVehicle))
				{
					vehicledoorDebugf3("CTaskMotionInVehicle::CheckIfStairsConditionPasses--CPED_RESET_FLAG_IsEnteringOrExitingVehicle-->return false");
					return false;
				}
			}

			if (NetworkInterface::IsGameInProgress())
			{
				if (!rVeh.IsInAir() && rVeh.GetVelocity().Mag2() < square(ms_Tunables.m_MaxVehVelocityToKeepStairsDown))
				{
					Vector3 vEntryPosition;
					Quaternion qEntryOrientation;
					CModelSeatInfo::CalculateEntrySituation(&rVeh, &rPed, vEntryPosition, qEntryOrientation, iDirectEntryIndex, CModelSeatInfo::EF_RoughPosition);
					Vec3V vEntryPos = RCC_VEC3V(vEntryPosition);

					TUNE_GROUP_FLOAT(IN_VEHICLE_TUNE, NEARBY_PED_TO_STAIRS_MAX_DIST, 3.0f, 0.0f, 30.0f, 0.01f);
					CEntityIterator pedIterator(IteratePeds, &rPed, &vEntryPos, NEARBY_PED_TO_STAIRS_MAX_DIST);
					for(CEntity* pNearbyEntity = pedIterator.GetNext(); pNearbyEntity; pNearbyEntity = pedIterator.GetNext())
					{
						CPed* pNearbyPed = static_cast<CPed*>(pNearbyEntity);
						if (pNearbyPed)
						{
							if (pNearbyPed->IsPlayer())
							{
								vehicledoorDebugf3("CTaskMotionInVehicle::CheckIfStairsConditionPasses--pNearbyPed->IsPlayer NEARBY_PED_TO_STAIRS_MAX_DIST[%f]-->return false",NEARBY_PED_TO_STAIRS_MAX_DIST);
								return false;
							}

							const bool bRunningEnterVehicleTask = pNearbyPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE);
							if (bRunningEnterVehicleTask)
							{
								CVehicle* pVehicle= static_cast<CVehicle*>(pNearbyPed->GetPedIntelligence()->GetQueriableInterface()->GetTargetForTaskType(CTaskTypes::TASK_ENTER_VEHICLE));
								if (pVehicle == &rVeh)
								{
									vehicledoorDebugf3("CTaskMotionInVehicle::CheckIfStairsConditionPasses--(pVehicle == &rVeh)-->return false");
									return false;
								}
							}
						}
					}
				}
			}
		}
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////

Vector3 CTaskMotionInVehicle::GetCurrentAccelerationSignalNonSmoothed(float fTimeStep, const CVehicle& vehicle, Vector3& vLastFramesVelocity)
{
	Vector3 vAcceleration = vehicle.GetVelocity() - vLastFramesVelocity;
	vAcceleration.Scale(1.0f / fTimeStep );
	vAcceleration = VEC3V_TO_VECTOR3(vehicle.GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vAcceleration)));
	return vAcceleration;
}

////////////////////////////////////////////////////////////////////////////////

float GetSteeringLerpScale()
{
	TUNE_GROUP_BOOL(VEHICLE_MOTION, bUseFPSScale, true);
	if(bUseFPSScale)
	{
		float fDesiredDt = 1.0f/30.0f; // assume 30Hz.
		return (fwTimer::GetTimeStep()/fDesiredDt);
	}
	return 1.0f;
}


////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInVehicle::ComputeSteeringSignal(const CVehicle& vehicle, float& fCurrentValue, float fSmoothing)
{
	// Turn the steering angle into a value between 0->1
	CVehicleModelInfo* pVehicleModelInfo = vehicle.GetVehicleModelInfo();

#if __DEV
	// Override the steering wheel multiplier
	TUNE_GROUP_FLOAT(VEHICLE_MOTION, STEER_WHEEL_MULT_OVERRIDE, 2.0f, 0.0f, 10.0f, 0.1f);
	TUNE_GROUP_BOOL(VEHICLE_MOTION, USE_STEER_WHEEL_MULT_OVERRIDE, false);
	if (USE_STEER_WHEEL_MULT_OVERRIDE)
	{
		pVehicleModelInfo->SetSteeringWheelMult(STEER_WHEEL_MULT_OVERRIDE);
		USE_STEER_WHEEL_MULT_OVERRIDE = false;
	}
#endif

	CHandlingData* pHandlingData = CHandlingDataMgr::GetHandlingData(pVehicleModelInfo->GetHandlingId());
	float fSteeringAngle = vehicle.GetSteerAngle();
	taskAssert(pHandlingData->m_fSteeringLock != 0.0f);
	fSteeringAngle /= pHandlingData->m_fSteeringLock;
	fSteeringAngle = (fSteeringAngle*0.5f)+0.5f;

	fSteeringAngle = rage::Clamp(fSteeringAngle, 0.0f, 1.0f);
	float fSteeringScale = GetSteeringLerpScale();
	fSmoothing *= fSteeringScale;
	fCurrentValue = rage::Lerp(fSmoothing, fCurrentValue, fSteeringAngle);

	TUNE_GROUP_FLOAT(VEHICLE_MOTION, STEER_OVERRIDE, 0.5f, 0.0f, 1.0f, 0.001f);
	TUNE_GROUP_BOOL(VEHICLE_MOTION, USE_STEER_OVERRIDE, false);
	if (USE_STEER_OVERRIDE)
		fCurrentValue = STEER_OVERRIDE;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInVehicle::ComputeSteeringSignalWithDeadZone(const CVehicle& vehicle, 
															 float& fCurrentValue, 
															 u32 &nCentreTime, 
															 u32 nLastNonCentredTime, 
															 u32 uTimeTillCentred,
															 u32 uDeadZoneTime, 
															 float fDeadZone, 
															 float fSmoothingIn, 
															 float fSmoothingOut, 
															 bool bInvert)
{
	float fOldSteering = fCurrentValue;

	// Turn the steering angle into a value between 0->1
	CVehicleModelInfo* pVehicleModelInfo = vehicle.GetVehicleModelInfo();
	CHandlingData* pHandlingData = CHandlingDataMgr::GetHandlingData(pVehicleModelInfo->GetHandlingId());
	float fSteeringAngle = vehicle.GetSteerAngle();
	taskAssert(pHandlingData->m_fSteeringLock != 0.0f);
	fSteeringAngle /= pHandlingData->m_fSteeringLock;
	fSteeringAngle = (fSteeringAngle*0.5f)+0.5f;

	fSteeringAngle = rage::Clamp(fSteeringAngle, 0.0f, 1.0f);

	if(bInvert)
	{
		fSteeringAngle = (1.0f - fSteeringAngle);
	}

	bool bCentred = false;
	if( (abs(fSteeringAngle - 0.5f) < 0.001f))
	{
		if( (fwTimer::GetTimeInMilliseconds() - nLastNonCentredTime) > uTimeTillCentred)
		{
			bCentred = true;
			nCentreTime = fwTimer::GetTimeInMilliseconds();
		}
	}

	bool bDeadZone = false;
	if(fwTimer::GetTimeInMilliseconds() - nCentreTime < uDeadZoneTime)
	{
		bDeadZone = true;
	}

	float fSteeringScale = GetSteeringLerpScale();

	const float fTimeWarpScaleSqd = square(fwTimer::GetTimeWarpActive());
	if(!bCentred)
	{
		fCurrentValue = rage::Lerp(fSmoothingIn * fTimeWarpScaleSqd * fSteeringScale, fCurrentValue, fSteeringAngle);

		if(bDeadZone && abs(fCurrentValue - 0.5f) < fDeadZone)
		{
			fCurrentValue = fOldSteering;
		}
	}
	else
	{
		float fCurrentToCentre = abs(fCurrentValue - 0.5f);
		float fTargetToCentre = abs(fSteeringAngle - 0.5f);

		if( (fCurrentToCentre < 0.001f) || (fTargetToCentre > fCurrentToCentre) )
		{
			fCurrentValue = rage::Lerp(fSmoothingOut * fTimeWarpScaleSqd * fSteeringScale, fCurrentValue, 0.5f);
		}
		else
		{
			fCurrentValue = rage::Lerp(fSmoothingOut * fTimeWarpScaleSqd * fSteeringScale, fCurrentValue, fSteeringAngle);
		}
	}
};

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInVehicle::ComputeAccelerationSignal(float fTimeStep, const CVehicle& vehicle, Vector3& vCurrentValue, const Vector3& vLastFramesVelocity, bool bLastFramesVelocityValid, float fAccelerationSmoothing, bool bIsBike)
{
	Vector3 vAcceleration(0.5f, 0.5f, 0.5f);
	Vector3 vCurrentVelocity = vehicle.GetVelocity();

	if (bLastFramesVelocityValid)
	{
        if(vehicle.IsNetworkClone())
        {
            vAcceleration = NetworkInterface::GetPredictedAccelerationVector(&vehicle);
        }
        else
        {
		    vAcceleration = vCurrentVelocity - vLastFramesVelocity;
		    vAcceleration.Scale(1.0f / fTimeStep );
        }

		vAcceleration = VEC3V_TO_VECTOR3(vehicle.GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vAcceleration)));


		if (!bIsBike)
		{
			const float fAccelerationMagSqd = vAcceleration.Mag2();
			const float fMaxAccForLeanSqd = rage::square(ms_Tunables.m_AccelerationToStartLeaning);
			if (fAccelerationMagSqd > fMaxAccForLeanSqd)
			{
				float fScale = Clamp(fAccelerationMagSqd / fMaxAccForLeanSqd, 0.0f, 1.0f);
				vAcceleration.Normalize();
				vAcceleration.Scale(fScale*0.5f);
				vAcceleration+=Vector3(0.5f, 0.5f, 0.5f);
			}
			else
			{
				vAcceleration=Vector3(0.5f, 0.5f, 0.5f);
			}
		}
		else
		{
			Vector2 vFlatAcc(vAcceleration.x, vAcceleration.y);
			float fAccZ = 0.0f;
			
			const float zAccelMag = Abs(vAcceleration.z);
			if (zAccelMag > ms_Tunables.m_ZAccelerationToStartLeaning)
			{
				float fZScale = Clamp(zAccelMag / ms_Tunables.m_MaxZAccelerationForLeanBike, 0.0f, 1.0f);
				fAccZ = Sign(vAcceleration.z) * 0.5f;
				fAccZ *= fZScale;
			}

			vFlatAcc.Normalize();
			vFlatAcc.Scale(ms_Tunables.m_AccelerationScaleBike*0.5f);
			vAcceleration=Vector3(vFlatAcc.x, vFlatAcc.y, fAccZ) + Vector3(0.5f, 0.5f, 0.5f);

		}
	}
	else
	{
		vAcceleration.Set(0.5f, 0.5f, 0.5f);
	}

	vCurrentValue.Lerp(fAccelerationSmoothing, vAcceleration);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInVehicle::ComputeAngAccelerationSignal(const CVehicle& vehicle, Vector3& vCurrentValue, Vector3& vLastFramesAngVelocity, bool& bLastFramesVelocityValid, float fTimeStep)
{
	Vector3 vAngularAcceleration(0.5f, 0.5f, 0.5f);

	if (bLastFramesVelocityValid )
	{
		Assert(fTimeStep > 0.0f);
		vAngularAcceleration =  vehicle.GetAngVelocity() - vLastFramesAngVelocity;
		vAngularAcceleration.Scale(1.0f / fTimeStep );
		vAngularAcceleration = VEC3V_TO_VECTOR3(vehicle.GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vCurrentValue)));

		float fAngAccelerationMag = vAngularAcceleration.Mag();

		TUNE_GROUP_FLOAT(VEHICLE_MOTION, ANG_ACCELERATION_TO_START_LEANING, 3.0f, 0.0f, 10.0f, 0.1f);
		TUNE_GROUP_FLOAT(VEHICLE_MOTION, ANG_ACCELERATION_SCALE, 0.1f, 0.0f, 1.0f, 0.001f);
		if( fAngAccelerationMag > ANG_ACCELERATION_TO_START_LEANING )
		{
			float fScale = Clamp((fAngAccelerationMag-ANG_ACCELERATION_TO_START_LEANING)*ANG_ACCELERATION_SCALE, 0.0f, 1.0f);
			vAngularAcceleration.Normalize();
			vAngularAcceleration.Scale(fScale*0.5f);
			vAngularAcceleration+=Vector3(0.5f, 0.5f, 0.5f);
		}
		else
		{
			vAngularAcceleration=Vector3(0.5f, 0.5f, 0.5f);
		}

	}
	else
	{
		vAngularAcceleration.Set(0.5f, 0.5f, 0.5f);
	}

	TUNE_GROUP_FLOAT(VEHICLE_MOTION, ANG_ACCELERATION_SMOOTING, 0.2f, 0.0f, 1.0f, 0.001f);
	vCurrentValue.Lerp(ANG_ACCELERATION_SMOOTING, vAngularAcceleration);
	vLastFramesAngVelocity = vehicle.GetAngVelocity();
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInVehicle::ComputeSpeedSignal(const CPed& rPed, const CVehicle& vehicle, float& fCurrentValue, float fSpeedSmoothing, float fPitch)
{
	// Turn the speed into a value between 0->1
	float fSpeed = vehicle.GetVelocityIncludingReferenceFrame().Mag();

	TUNE_GROUP_FLOAT(VEHICLE_MOTION, MAX_VELOCITY, 30.0f, 0.0f, 100.0f, 0.1f);
	fSpeed /= MAX_VELOCITY;

	if (fSpeedSmoothing == 1.0f)
	{
		fCurrentValue = rage::Clamp(fSpeed, 0.0f, 1.0f); 
		return;
	}

	static dev_float s_fSpeedIncSlope = 0.5f;
	// static dev_float s_fIncTol = 0.0f;
	static dev_float s_fSpeedIncFlat = 0.1f;

	float fSpeedInc = s_fSpeedIncFlat;

	TUNE_GROUP_FLOAT(VEHICLE_MOTION, PITCH_PLAYER_INC_TOL, 0.3f, 0.0f, 1.0f, 0.01f);
	if (fPitch < PITCH_PLAYER_INC_TOL && fPitch > (1.0f - PITCH_PLAYER_INC_TOL))
	{
		fSpeedInc = s_fSpeedIncSlope;
	}

	bool bUseRestrictedLean = false;

	CTaskAimGunVehicleDriveBy* pGunDriveByTask = static_cast<CTaskAimGunVehicleDriveBy*>(rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY));
	if (pGunDriveByTask || rPed.GetPedResetFlag(CPED_RESET_FLAG_IsDoingDriveby))
	{
		bUseRestrictedLean = true;

		if (pGunDriveByTask && pGunDriveByTask->GetState() == CTaskAimGunVehicleDriveBy::State_Outro)
		{
			bUseRestrictedLean = !pGunDriveByTask->ShouldBlendOutAimOutro();
		}
	}

	if (bUseRestrictedLean)
	{
		const CVehicleDriveByInfo* pVehicleDriveByInfo = CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(&rPed);
		if (pVehicleDriveByInfo && pVehicleDriveByInfo->GetUseSpineAdditive() && vehicle.GetLayoutInfo()->GetDisableFastPoseWhenDrivebying())
		{
			const float fMaxSpeedParam = pVehicleDriveByInfo->GetMaxSpeedParam();
			if (fSpeed > fMaxSpeedParam)
			{
				rage::Approach(fCurrentValue, fMaxSpeedParam, pVehicleDriveByInfo->GetApproachSpeedToWithinMaxBlendDelta(), fwTimer::GetTimeStep());
				return;
			}
		}
	}

	fCurrentValue = rage::Lerp(fSpeedSmoothing, fCurrentValue, fSpeed);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInVehicle::ComputePitchSignal(const CPed& ped, float& fCurrentValue, float fSmoothingRate, float fMinPitch, float fMaxPitch)
{
	float fPitchAngle = ped.GetCurrentPitch() - fMinPitch;
	const float fPitchRange = Abs(fMaxPitch - fMinPitch);
	fPitchAngle = fPitchAngle / fPitchRange;
	if (fPitchAngle > 1.0f) fPitchAngle = 1.0f;
	else if (fPitchAngle < 0.0f) fPitchAngle = 0.0f;

	if (ped.GetPedResetFlag(CPED_RESET_FLAG_IsDoingDriveby))
	{
		const CVehicleDriveByInfo* pVehicleDriveByInfo = CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(&ped);
		if (pVehicleDriveByInfo && pVehicleDriveByInfo->GetUseSpineAdditive())
		{
			const float fMaxLongitudinalDelta = pVehicleDriveByInfo->GetMaxLongitudinalLeanBlendWeightDelta();
			if (Abs(fPitchAngle - 0.5f) >= fMaxLongitudinalDelta)
			{
				fPitchAngle = fPitchAngle < 0.5f ? 0.5f - fPitchAngle : 0.5f + fMaxLongitudinalDelta;
				rage::Approach(fCurrentValue, fPitchAngle, pVehicleDriveByInfo->GetApproachSpeedToWithinMaxBlendDelta(), fwTimer::GetTimeStep());
				return;
			}
		}
	}

	fCurrentValue = rage::Lerp(fSmoothingRate, fCurrentValue, fPitchAngle);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInVehicle::ComputeLeanSignal(float& fCurrentValue, const Vec3V& vAcceleration)
{
	fCurrentValue = 1.0f - fCurrentValue + vAcceleration.GetXf();
	fCurrentValue *= 0.5f;

	// #if __BANK
	// 	grcDebugDraw::AddDebugOutput(Color_yellow, "Lean: %.4f", fLean);
	// #endif

	TUNE_GROUP_FLOAT(VEHICLE_MOTION, LEAN_OVERRIDE, 0.5f, 0.0f, 1.0f, 0.001f);
	TUNE_GROUP_BOOL(VEHICLE_MOTION, USE_LEAN_OVERRIDE, false);
	if (USE_LEAN_OVERRIDE)
		fCurrentValue = LEAN_OVERRIDE;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInVehicle::ComputeBikeLeanAngleSignal(const CPed& ped, CVehicle& vehicle, float& fCurrentValue, float fSmoothingRate, float fDefaultSmoothingRate)
{
	taskAssert(CTaskMotionInAutomobile::ShouldUseBikeInVehicleAnims(vehicle));

	float fDesiredLeanAngle = 0.5f;
	if (vehicle.InheritsFromBike())
	{
		fDesiredLeanAngle = static_cast<const CBike&>(vehicle).GetLeanAngle();
		const float fMaxLeanAngle = vehicle.pHandling->GetBikeHandlingData()->m_fMaxBankAngle;
		fDesiredLeanAngle /= fMaxLeanAngle;
		fDesiredLeanAngle += 1.0f;
		fDesiredLeanAngle *= 0.5f;

		TUNE_GROUP_BOOL(BICYCLE_TUNE, USE_EXTRA_PLAYER_INPUT_FOR_LEAN, true);
		if (USE_EXTRA_PLAYER_INPUT_FOR_LEAN && fSmoothingRate == fDefaultSmoothingRate && vehicle.GetVehicleType() == VEHICLE_TYPE_BICYCLE && ped.IsLocalPlayer())
		{
			const CControl* pControl = ped.GetControlFromPlayer();
			if (pControl)
			{
				TUNE_GROUP_FLOAT(BICYCLE_TUNE, EXTRA_LEAN_PERCENTAGE, 0.1f, 0.0f, 1.0f, 0.01f);
				TUNE_GROUP_FLOAT(BICYCLE_TUNE, EXTRA_LEAN_PERCENTAGE_FPS, 0.4f, 0.0f, 1.0f, 0.01f);
				float fCurrentFramesLeftRightInput = 0.0f; 
				float fStickY = 0.0f;
				static_cast<CBike&>(vehicle).GetBikeLeanInputs(fCurrentFramesLeftRightInput, fStickY);
				//Displayf("Stick Input : %.2f", fReturnCurrentFramesLeftRightInput);

				float fStickInput = -fCurrentFramesLeftRightInput;
				
				TUNE_GROUP_FLOAT(BICYCLE_TUNE, MAX_STICK_INPUT_TO_SQUARE, 0.7f, 0.0f, 1.0f, 0.01f);
				if (Abs(fStickInput) < MAX_STICK_INPUT_TO_SQUARE)
				{
					fStickInput = Sign(fCurrentFramesLeftRightInput) * rage::square(fStickInput);
				}

				TUNE_GROUP_BOOL(BICYCLE_TUNE, SMOOTH_DESIRED_LEAN_ANGLE_AT_SMALL_ANGLES, true);
				TUNE_GROUP_FLOAT(BICYCLE_TUNE, LEAN_THRESHOLD, 0.2f, 0.0f, 1.0f, 0.01f);

				if (SMOOTH_DESIRED_LEAN_ANGLE_AT_SMALL_ANGLES && 
					(fDesiredLeanAngle > 0.5f - LEAN_THRESHOLD || fDesiredLeanAngle < 0.5f + LEAN_THRESHOLD))
				{
					TUNE_GROUP_FLOAT(BICYCLE_TUNE, DESIRED_LEAN_RATE_SMOOTHING_LOW, 0.5f, 0.0f, 1.0f, 0.01f);
					fSmoothingRate = DESIRED_LEAN_RATE_SMOOTHING_LOW;
				}

				const float fExtraLeanPercentage = ped.IsInFirstPersonVehicleCamera() ? EXTRA_LEAN_PERCENTAGE_FPS : EXTRA_LEAN_PERCENTAGE;
				if (fStickInput < 0.0f)
				{
					fDesiredLeanAngle = rage::Clamp(fDesiredLeanAngle - Abs(fStickInput) * fExtraLeanPercentage, 0.0f, 1.0f);
				}
				else
				{
					fDesiredLeanAngle = rage::Clamp(fDesiredLeanAngle + Abs(fStickInput) * fExtraLeanPercentage, 0.0f, 1.0f);
				}
			}
		}
	}

	if (ped.GetPedResetFlag(CPED_RESET_FLAG_IsDoingDriveby))
	{
		const CVehicleDriveByInfo* pVehicleDriveByInfo = CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(&ped);
		if (pVehicleDriveByInfo && pVehicleDriveByInfo->GetUseSpineAdditive())
		{
			const float fMaxLateralDelta = pVehicleDriveByInfo->GetMaxLateralLeanBlendWeightDelta();
			if (Abs(fDesiredLeanAngle - 0.5f) >= fMaxLateralDelta)
			{
				fDesiredLeanAngle = fDesiredLeanAngle < 0.5f ? 0.5f - fMaxLateralDelta : 0.5f + fMaxLateralDelta;
			}
		}
	}
	fCurrentValue = rage::Lerp(fSmoothingRate, fCurrentValue, fDesiredLeanAngle);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInVehicle::ComputeBikeLeanAngleSignalNew(const CVehicle& ASSERT_ONLY(vehicle), float& fCurrentValue, float& fDesiredValue, float fSmoothingRate)
{
	taskAssert(CTaskMotionInAutomobile::ShouldUseBikeInVehicleAnims(vehicle));
	fCurrentValue = rage::Lerp(fSmoothingRate, fCurrentValue, fDesiredValue);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInVehicle::ComputeBikeDesiredLeanAngleSignal(const CVehicle& ASSERT_ONLY(vehicle), float& fCurrentValue, float& fDesiredValue, float fSmoothingRate)
{
	taskAssert(CTaskMotionInAutomobile::ShouldUseBikeInVehicleAnims(vehicle));
	fCurrentValue = rage::Lerp(fSmoothingRate, fCurrentValue, fDesiredValue);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInVehicle::IsVerticalProbeClear(const Vector3& vTestPos, float fProbeLength, const CVehicle& vehicle, const CPed& ped)
{
	const Vector3 vEndPos(vTestPos.x, vTestPos.y, vTestPos.z - fProbeLength);

	WorldProbe::CShapeTestFixedResults<> probeResults;
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetResultsStructure(&probeResults);
	probeDesc.SetStartAndEnd(vTestPos, vEndPos);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_MOVER);

	const int ENTITY_EXCLUSION_MAX = 2;
	const CEntity* exclusionList[ENTITY_EXCLUSION_MAX];
	exclusionList[0] = &ped;
	exclusionList[1] = &vehicle;
	probeDesc.SetExcludeEntities(exclusionList, ENTITY_EXCLUSION_MAX);

	if (!WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
#if __BANK
		ms_debugDraw.AddLine(RCC_VEC3V(vTestPos), RCC_VEC3V(vEndPos), Color_green, 1000);
#endif
		return true;
	}

#if __BANK
	ms_debugDraw.AddLine(RCC_VEC3V(vTestPos), RCC_VEC3V(vEndPos), Color_red, 1000);
#endif

	return false;
}

////////////////////////////////////////////////////////////////////////////////

const CCarDoor* CTaskMotionInVehicle::GetOpenDoorForDoubleDoorSeat(const CVehicle& vehicle, s32 iSeatIndex, s32& iEntryIndex)
{
	s32 iLeftEntryPoint = vehicle.GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iSeatIndex, &vehicle, SA_directAccessSeat, true, true);
	if (vehicle.IsEntryIndexValid(iLeftEntryPoint))
	{
		const CCarDoor* pDoor = GetDirectAccessEntryDoorFromEntryPoint(vehicle, iEntryIndex);
		if (pDoor && pDoor->GetIsIntact(&vehicle) && pDoor->GetDoorRatio() >= ms_Tunables.m_MinRatioForClosingDoor)
		{
			iEntryIndex = iLeftEntryPoint;
			return pDoor;
		}
	}
	s32 iRightEntryPoint = vehicle.GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iSeatIndex, &vehicle, SA_directAccessSeat, true, false);
	if (vehicle.IsEntryIndexValid(iRightEntryPoint))
	{
		const CCarDoor* pDoor = GetDirectAccessEntryDoorFromEntryPoint(vehicle, iRightEntryPoint);
		if (pDoor && pDoor->GetIsIntact(&vehicle) && pDoor->GetDoorRatio() >= ms_Tunables.m_MinRatioForClosingDoor)
		{
			iEntryIndex = iRightEntryPoint;
			return pDoor;
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

const CCarDoor* CTaskMotionInVehicle::GetDirectAccessEntryDoorFromPed(const CVehicle& vehicle, const CPed& ped)
{
	// Lookup the entry point from our seat
	s32 iDirectEntryPoint = vehicle.GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(vehicle.GetSeatManager()->GetPedsSeatIndex(&ped), &vehicle);
	if (vehicle.IsEntryIndexValid(iDirectEntryPoint))
	{
		taskAssert(vehicle.GetVehicleModelInfo());
		const CEntryExitPoint* pEntryPoint = vehicle.GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryExitPoint(iDirectEntryPoint);
		if (taskVerifyf(pEntryPoint, "Entry point invalid"))
		{
			// TODO: Make component reservation functions const-compliant
			CComponentReservation* pComponentReservation = const_cast<CVehicle&>(vehicle).GetComponentReservationMgr()->GetDoorReservation(iDirectEntryPoint);
			if (pComponentReservation && (!pComponentReservation->IsComponentInUse() || pComponentReservation->GetPedUsingComponent() == &ped))
			{
				return vehicle.GetDoorFromBoneIndex(pEntryPoint->GetDoorBoneIndex());
			}
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

const CCarDoor* CTaskMotionInVehicle::GetDirectAccessEntryDoorFromSeat(const CVehicle& vehicle, s32 iSeatIndex)
{
	// Lookup the entry point from our seat
	s32 iDirectEntryPoint = vehicle.GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iSeatIndex, &vehicle);
	if (vehicle.IsEntryIndexValid(iDirectEntryPoint))
	{
		taskAssert(vehicle.GetVehicleModelInfo());
		const CEntryExitPoint* pEntryPoint = vehicle.GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryExitPoint(iDirectEntryPoint);
		if (taskVerifyf(pEntryPoint, "Entry point invalid"))
		{
			return vehicle.GetDoorFromBoneIndex(pEntryPoint->GetDoorBoneIndex());
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

const CCarDoor* CTaskMotionInVehicle::GetDirectAccessEntryDoorFromEntryPoint(const CVehicle& vehicle, s32 iEntryPointIndex)
{
	if (vehicle.IsEntryIndexValid(iEntryPointIndex))
	{
		taskAssert(vehicle.GetVehicleModelInfo());
		const CEntryExitPoint* pEntryPoint = vehicle.GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryExitPoint(iEntryPointIndex);
		if (taskVerifyf(pEntryPoint, "Entry point invalid"))
		{
			return vehicle.GetDoorFromBoneIndex(pEntryPoint->GetDoorBoneIndex());
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

const CSeatOverrideAnimInfo* CTaskMotionInVehicle::GetSeatOverrideAnimInfoFromPed(const CPed& ped)
{
	if (ped.GetIsInVehicle())
	{
		const CVehicleSeatAnimInfo* pSeatClipInfo = ped.GetMyVehicle()->GetSeatAnimationInfo(&ped);
		if (pSeatClipInfo)
		{
			// Look for an overriden in vehicle context and find the clipset associated with our current seat
			CTaskMotionBase* pTask = ped.GetPrimaryMotionTask();
			const u32 uOverrideInVehicleContext = pTask ? pTask->GetOverrideInVehicleContextHash() : 0;
			if (uOverrideInVehicleContext != 0)
			{
				const CInVehicleOverrideInfo* pOverrideInfo = CVehicleMetadataMgr::GetInVehicleOverrideInfo(uOverrideInVehicleContext);
				if (taskVerifyf(pOverrideInfo, "Couldn't find override info for context with hash %u", uOverrideInVehicleContext))
				{
					return pOverrideInfo->FindSeatOverrideAnimInfoFromSeatAnimInfo(pSeatClipInfo);
				}
			}
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInVehicle::VehicleHasHandleBars(const CVehicle& rVeh)
{
	return rVeh.InheritsFromBike() || rVeh.InheritsFromQuadBike() || rVeh.InheritsFromAmphibiousQuadBike() || rVeh.GetIsJetSki() || rVeh.GetIsAircraft();
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInVehicle::ShouldPerformInVehicleHairScale(const CPed& ped, float& fHairScale)
{
	const CVehicle* pVehiclePedInside = ped.GetVehiclePedInside();
	if (pVehiclePedInside && pVehiclePedInside->HasRaisedRoof())
	{
		int iAttachCarSeatIndex = ped.GetAttachCarSeatIndex();
		if (pVehiclePedInside->IsSeatIndexValid(iAttachCarSeatIndex))
		{
			const CVehicleSeatInfo* pSeatInfo = pVehiclePedInside->GetSeatInfo(iAttachCarSeatIndex);
			if (Verifyf(pSeatInfo, "Could not find seat info for vehicle %s, seat index %i", pVehiclePedInside->GetModelName(), iAttachCarSeatIndex))
			{
				if (!ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE))
				{
					bool bDisableHairScale = false;
					const CVehicleModelInfo* pVehicleModelInfo = pVehiclePedInside->GetVehicleModelInfo();
					if (pVehicleModelInfo)
					{
						float fMinSeatHeight = pVehicleModelInfo->GetMinSeatHeight();
						float fHairHeight = ped.GetHairHeight();

						if (fHairHeight != -1.0f && fMinSeatHeight != -1.0f)
						{
							const float fSeatHeadHeight = 0.59f; /* This is a conservative approximate measurement */
							if ((fSeatHeadHeight + fHairHeight + CVehicle::sm_HairScaleDisableThreshold) < fMinSeatHeight)
							{
								bDisableHairScale = true;
							}
						}
					}

					fHairScale = pVehiclePedInside->GetIsHeli() ? 0.0f : pSeatInfo->GetHairScale();
					if (fHairScale < 0.0f && !bDisableHairScale)
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

CTaskMotionInVehicle::CTaskMotionInVehicle() 
: m_pVehicle(NULL)
, m_pDriver(NULL)
, m_uTimeDriverValid(0)
, m_bWasUsingPhone(false)
, m_bDriverWasPlayerLastUpdate(false)
{
	m_uTimeInVehicle = fwTimer::GetTimeInMilliseconds();
	SetInternalTaskType(CTaskTypes::TASK_MOTION_IN_VEHICLE);
}

////////////////////////////////////////////////////////////////////////////////

CTaskMotionInVehicle::~CTaskMotionInVehicle()
{

}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInVehicle::CleanUp()
{
	if (m_pVehicle.Get())
	{
		vehicledoorDebugf2("CTaskMotionInAutomobile::CleanUp-->SetPedRemoteUsingDoor(false)/SetRemoteDriverDoorClosing(false)");

		CPed* pPed = GetPed();
		const s32 iDirectEntryPoint = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed), m_pVehicle);
		CTaskVehicleFSM::SetRemotePedUsingDoorsFalse(*pPed, m_pVehicle, iDirectEntryPoint);
		
		if( pPed == m_pVehicle->GetDriver() )
		{
			m_pVehicle->SetRemoteDriverDoorClosing(false);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInVehicle::ProcessPreFSM()
{
	CPed* pPed = GetPed();

	// disable action mode.
	pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableActionMode, true);
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostMovement, true);

	// The peds vehicle may change so store the peds current vehicle (maybe warped from one vehicle to another)
	m_pVehicle = pPed->GetMyVehicle();

	// clone peds may not have cloned their vehicle on this machine when
	// the task starts, in this case the task will wait in the start state
	// until this has occurred. The peds vehicle can be removed from this machine
	// before the ped too, and we need to quit this task (without asserting) in this case
	if (pPed->IsNetworkClone() && !m_pVehicle)
	{
		if (GetState()==State_Start)
		{
			return FSM_Continue;
		}
		else
		{
			return FSM_Quit;
		}
	}

	// If we don't have a valid vehicle or we're not attached to it at this point, bail out
	if (!m_pVehicle	|| !pPed->GetIsAttached())
	{
		return FSM_Quit;
	}

	if (m_pVehicle->GetSeatManager()->GetDriver())
	{
		m_pDriver = m_pVehicle->GetSeatManager()->GetDriver();		
		m_uTimeDriverValid = fwTimer::GetTimeInMilliseconds();
	}

	// Must have a valid seat index to run vehicle motion task
	const s32 iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
	if (!m_pVehicle->IsSeatIndexValid(iSeatIndex))
	{
		return FSM_Quit;
	}

	// Need to also have valid seat clip info
	if (!m_pVehicle->GetSeatAnimationInfo(pPed))
	{
		return FSM_Quit;
	}
	
	// disable cellphone anims while on a bike, jetski, or in a turret seat
	if ( m_pVehicle->InheritsFromBike() 
		|| m_pVehicle->InheritsFromQuadBike() 
		|| m_pVehicle->GetIsJetSki()
		|| m_pVehicle->IsTurretSeat(iSeatIndex)
		|| m_pVehicle->InheritsFromAmphibiousQuadBike())
	{
		// we could just disable phone for moving bikes
		// we would need to disable right arm ik for that to look nice
		// if ( CheckForMovingForwards(*m_pVehicle, *pPed) )
		{
			pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableCellphoneAnimations, true );
		}
	}
	
	const fwMvClipSetId inVehicleClipsetId = CTaskMotionInAutomobile::GetInVehicleClipsetId(*pPed, m_pVehicle, iSeatIndex);

	eStreamingPriority priority = (pPed->IsLocalPlayer() || pPed->PopTypeIsMission()) ? SP_High : SP_Invalid;
	CTaskVehicleFSM::RequestVehicleClipSet(inVehicleClipsetId, priority);

	// If we're following a ped and we're in the rear seat of a car, see if the opposite entry is blocked and shuffle over if a ped is trying to get in
	ProcessShuffleForGroupMember();

	bool bPassengerShouldRagdoll = false;
	// If we had a driver on our bike that is no longer on it and is ragdolling, then put ourselfs into ragdoll
	if (!pPed->IsNetworkClone() && m_pVehicle->InheritsFromBike() && 
		m_pDriver && m_pDriver != pPed && !m_pDriver->GetIsInVehicle() && 
		m_pDriver->GetUsingRagdoll() && m_pDriver->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_THROUGH_WINDSCREEN) &&
		CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_VEHICLE_FALLOUT))
	{
		CEventSwitch2NM event(10000, rage_new CTaskNMThroughWindscreen(2000, 30000, false, m_pVehicle));
		if (pPed->GetPedIntelligence()->AddEvent(event) != NULL)
		{
			bPassengerShouldRagdoll = true;
		}
	}		

	// If driver was a player and now is not, then reest the timer to automatically get forced out of the vehicle.
	// Player probably left session and it's now ambient ped should be exiting the vehicle, so give time for us to possibly stay in vehicle.
	bool bDriverPlayer = m_pDriver && m_pDriver->IsPlayer() && m_pDriver != pPed;
	if (m_bDriverWasPlayerLastUpdate && !bDriverPlayer)
	{
		m_uTimeInVehicle = fwTimer::GetTimeInMilliseconds();
	}

	// If we are the driver, when vehicle is set to exclusive use by someone else, then exit.
	if(GetShouldExitVehicle(pPed))
	{
		VehicleEnterExitFlags vehicleFlags;
		CTaskExitVehicle* pExitTask = rage_new CTaskExitVehicle(m_pVehicle, vehicleFlags);
		pExitTask->SetRunningFlag(CVehicleEnterExitFlags::DontWaitForCarToStop);
		CEventGivePedTask givePedTask(PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP, pExitTask, true );
		pPed->GetPedIntelligence()->AddEvent(givePedTask);
	}
	// network game specific behaviour
	else if(!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PreventAutoShuffleToDriversSeat) &&
		(NetworkInterface::IsGameInProgress() || pPed->IsLawEnforcementPed() || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_AllowAutoShuffleToDriversSeat)))
	{
		// Block auto-shuffle to driver seat if we're in a turret seat (or an APC 'turret seat')
		bool bInFrontPassengerSeatInAPC = MI_CAR_APC.IsValid() && m_pVehicle->GetModelIndex() == MI_CAR_APC && iSeatIndex == 1;
		bool bIsUsingTurretSeat = bInFrontPassengerSeatInAPC || m_pVehicle->IsTurretSeat(m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed));

		// The vehicle lock check was added because we didn't want players who weren't qualified to ride a bike in c&c (B*249088) to become the driver
		// removed for B*1514735
		if(!bIsUsingTurretSeat && !pPed->IsNetworkClone() && !m_pVehicle->IsDriver(pPed) /*&& !CTaskVehicleFSM::IsVehicleLockedForPlayer(*m_pVehicle, *pPed)*/)
		{
			CPed *pDriver = m_pVehicle->GetDriver();

            bool bShuffleToDriverSeat = false;

			if(pDriver)
			{
				bool bAllowJackShuffle = true;
				s32 nDriverSeat = m_pVehicle->GetDriverSeat();
				if(nDriverSeat != -1)
				{
					const CVehicleSeatAnimInfo* pDriverSeatClipInfo = m_pVehicle->GetSeatAnimationInfo(nDriverSeat);
					if(pDriverSeatClipInfo && pDriverSeatClipInfo->GetPreventShuffleJack())
					{
						bAllowJackShuffle = false;
					}
				}

                bShuffleToDriverSeat = !pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableSeatShuffleDueToInjuredDriver) && pDriver->IsInjured() &&
					bAllowJackShuffle;

                // ensure if we are a passenger in this vehicle that we get out if the driver is not on our team
				if( !pPed->IsInjured() &&
					pPed->GetPedIntelligence()->IsThreatenedBy(*pDriver) &&
					!pDriver->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE) &&
                    !pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE) &&
					!pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE) &&
					(pPed->GetCustodian() != pDriver) && pDriver->IsAPlayerPed())
				{
#if __BANK
					AI_LOG_WITH_ARGS("[VehicleEntryExit][CTaskMotionInVehicle::ProcessPreFSM] - Ped %s being given TASK_EXIT_VEHICLE because driver %s is not on our team\n", AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetDynamicEntityNameSafe(pDriver));
					CRelationshipDebugInfo debugThisPed(pPed);
					debugThisPed.Print();
					CRelationshipDebugInfo debugDriverPed(pDriver);
					debugDriverPed.Print();
#endif // __BANK
					VehicleEnterExitFlags vehicleFlags;
					CEventGivePedTask givePedTask(PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP, rage_new CTaskExitVehicle(m_pVehicle, vehicleFlags), true );
					pPed->GetPedIntelligence()->AddEvent(givePedTask);
				}
			}
            else
            {
				bShuffleToDriverSeat = !bPassengerShouldRagdoll;

				if (bShuffleToDriverSeat)
				{
					//Check if the ped is law-enforcement.
					if(pPed->IsLawEnforcementPed())
					{
						// Don't shuffle if the driver is alive, we are already exiting or we've been told by combat to not shuffle
						const CPed* pLastDriver = m_pVehicle->GetSeatManager()->GetLastPedInSeat(Seat_driver);
						if( pLastDriver && (!pLastDriver->IsInjured() || pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableSeatShuffleDueToInjuredDriver)) )
						{
							bShuffleToDriverSeat = false;
						}
						else if(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE))
						{
							bShuffleToDriverSeat = false;
						}
					}

#if __BANK			// Don't auto-shuffle to the driver seat if we're trying to debug force a seat in RAG
					if (CVehicleDebug::ms_bForcePlayerToUseSpecificSeat)
					{
						bShuffleToDriverSeat = false;
					}
#endif // __BANK
				}
            }

            // if the driver is dead or has left the vehicle and we are in the front passenger seat shuffle over
            if(bShuffleToDriverSeat)
            {
				const CVehicleSeatAnimInfo* pSeatAnimInfo = m_pVehicle->GetSeatAnimationInfo(iSeatIndex);
				
				bool bWarpFromHeliTurretSeat = false;
				if (pSeatAnimInfo && m_pVehicle->InheritsFromHeli() && pSeatAnimInfo->IsTurretSeat() && m_pVehicle->IsInAir() && !CTaskExitVehicle::IsVehicleOnGround(*m_pVehicle))
				{
					if (!m_pVehicle->GetSeatManager()->GetDriver())
						bWarpFromHeliTurretSeat = true;
				}

				if ((pSeatAnimInfo && pSeatAnimInfo->GetCanWarpToDriverSeatIfNoDriver()) || bWarpFromHeliTurretSeat)
				{
                    bool bWarpToDriverSeat = true;

                    // check if any other peds in seats with a lower seat index can warp, give them priority if so
                    for(s32 iLowerSeatIndex = 0; iLowerSeatIndex < iSeatIndex && bWarpToDriverSeat; iLowerSeatIndex++)
                    {
                        if(m_pVehicle->GetSeatManager()->GetPedInSeat(iLowerSeatIndex))
                        {
                            const CVehicleSeatAnimInfo* pLowerSeatAnimInfo = m_pVehicle->GetSeatAnimationInfo(iLowerSeatIndex);

                            if(pLowerSeatAnimInfo && pLowerSeatAnimInfo->GetCanWarpToDriverSeatIfNoDriver())
                            {
                                bWarpToDriverSeat = false;
                            }
                        }
                    }

					if (bWarpToDriverSeat && CheckCanPedMoveToDriversSeat(*pPed, *m_pVehicle))
					{
						const CPed* pPedInOrEnteringDriversSeat = CTaskVehicleFSM::GetPedInOrUsingSeat(*m_pVehicle, m_pVehicle->GetDriverSeat());
						aiDisplayf("Frame : %i, ped %s (%p) attempting to warp to driver's seat from seat %i, occupied ? %s by %s %p", fwTimer::GetFrameCount(), pPed->GetModelName(), pPed, iSeatIndex, pPedInOrEnteringDriversSeat ? "TRUE":"FALSE", pPedInOrEnteringDriversSeat ? pPedInOrEnteringDriversSeat->GetModelName() : "NULL", pPedInOrEnteringDriversSeat);
						if (!pPedInOrEnteringDriversSeat)
						{
							CComponentReservation* pComponentReservation = m_pVehicle->GetComponentReservationMgr()->GetSeatReservationFromSeat(m_pVehicle->GetDriverSeat());
							if (CComponentReservationManager::ReserveVehicleComponent(pPed, m_pVehicle, pComponentReservation))
							{
								AI_LOG_WITH_ARGS("[VehicleEntryExit][CTaskMotionInVehicle::ProcessPreFSM] - Ped %s is warping to driver's seat from seat %i\n", AILogging::GetDynamicEntityNameSafe(pPed), iSeatIndex);
								VehicleEnterExitFlags vehFlags;
								vehFlags.BitSet().Set(CVehicleEnterExitFlags::Warp);
								CEventGivePedTask givePedTask(PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP, rage_new CTaskEnterVehicle(m_pVehicle, SR_Specific, m_pVehicle->GetDriverSeat(), vehFlags), true );
								pPed->GetPedIntelligence()->AddEvent(givePedTask);
							}
						}
					}
				}
				else
				{
					if (CheckCanPedMoveToDriversSeat(*pPed, *m_pVehicle))
					{
						s32 iDirectEntryIndex = m_pVehicle->GetDirectEntryPointIndexForPed(pPed);
						if (m_pVehicle->IsEntryIndexValid(iDirectEntryIndex))
						{
							s32 iCurrentSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
							if (m_pVehicle->IsSeatIndexValid(iCurrentSeatIndex))
							{
								s32 iIndirectSeatIndex = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetShuffleSeatForSeat(iDirectEntryIndex, iCurrentSeatIndex);
								if(m_pVehicle->GetDriverSeat() == iIndirectSeatIndex)
								{
									AI_LOG_WITH_ARGS("[VehicleEntryExit][CTaskMotionInVehicle::ProcessPreFSM] - Ped %s is shuffling to driver's seat from seat %i\n", AILogging::GetDynamicEntityNameSafe(pPed), iCurrentSeatIndex);
									CEventGivePedTask givePedTask(PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP, rage_new CTaskInVehicleSeatShuffle(m_pVehicle, NULL), true );
									pPed->GetPedIntelligence()->AddEvent(givePedTask);
								}
							}
						}
					}
				}
            }
		}
	}

	
	if (pPed->IsLocalPlayer())
	{
		CVehicle* pTrailerSmall2 = NULL;
		if (m_pVehicle->IsTrailerSmall2() && !m_pVehicle->IsNetworkClone())
		{
			pTrailerSmall2 = m_pVehicle;
		}
		else
		{
			CTrailer* pTrailer = m_pVehicle->GetAttachedTrailer();
			if (pTrailer && pTrailer->IsTrailerSmall2() && !pTrailer->IsNetworkClone())
			{
				pTrailerSmall2 = pTrailer;
			}
		}

		if (pTrailerSmall2)
		{
			const CPed* pPedInTrailer = CTaskVehicleFSM::GetPedInOrUsingSeat(*pTrailerSmall2, 0);
			if (pPedInTrailer)
			{
				CPed* pPedEnteringSeat = NULL;
				CComponentReservation* pComponentReservation = pTrailerSmall2->GetComponentReservationMgr()->GetSeatReservationFromSeat(0);
				if (pComponentReservation && pComponentReservation->GetPedUsingComponent())
				{
					pPedEnteringSeat = pComponentReservation->GetPedUsingComponent();
				}

				if (pPedEnteringSeat && pPedEnteringSeat != pPedInTrailer)
				{
					pTrailerSmall2->ActivatePhysics();
				}
			}
		}
	}

	m_bDriverWasPlayerLastUpdate = bDriverPlayer;

	// Needed for arm ik
	RequestProcessMoveSignalCalls();
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInVehicle::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnEnter
				return Start_OnEnter();
			FSM_OnUpdate
				return Start_OnUpdate();	

		FSM_State(State_StreamAssets)
			FSM_OnEnter
				return StreamAssets_OnEnter();
			FSM_OnUpdate
				return StreamAssets_OnUpdate();

		FSM_State(State_InAutomobile)
			FSM_OnEnter
				return InAutomobile_OnEnter();
			FSM_OnUpdate
				return InAutomobile_OnUpdate();

		FSM_State(State_InRemoteControlVehicle)
			FSM_OnEnter
				return InRemoteControlVehicle_OnEnter();
			FSM_OnUpdate
				return InRemoteControlVehicle_OnUpdate();

		FSM_State(State_OnBicycle)
			FSM_OnEnter
				return OnBicycle_OnEnter();
			FSM_OnUpdate
				return OnBicycle_OnUpdate();

		FSM_State(State_InTurret)
			FSM_OnEnter
				return InTurret_OnEnter();
			FSM_OnUpdate
				return InTurret_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInVehicle::ProcessPostFSM()
{
	CPed* pPed = GetPed();

	if (!m_pVehicle)
	{
		taskAssertf(pPed->IsNetworkClone(), "A local ped has an in vehicle motion task without being in a vehicle!");
	}
	else
	{
		// Must have a valid seat index to run vehicle motion task
		const s32 iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
		if (!m_pVehicle->IsSeatIndexValid(iSeatIndex))
		{
			return FSM_Quit;
		}
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInVehicle::ProcessPostMovement()
{
	// Stream clipsets for the vehicle (local player processed in CPlayerInfo::ProcessPostMovement())
	if (!GetPed()->IsLocalPlayer())
	{
		m_VehicleClipRequestHelper.Update(fwTimer::GetTimeStep());
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInVehicle::ProcessMoveSignals()
{
	CPed& rPed = *GetPed();
	const bool bVehicleHasHandleBars = m_pVehicle ? VehicleHasHandleBars(*m_pVehicle) : false;
	const bool bVehicleValidForTorsoIk = bVehicleHasHandleBars || (m_pVehicle && m_pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_TURRET_SEAT_ON_VEHICLE));
	if (m_pVehicle && bVehicleValidForTorsoIk)
	{
#if __BANK
		if (!CTaskEnterVehicle::ms_Tunables.m_DisableBikeHandleArmIk)
#endif
		{
			CTaskMotionInAutomobile* pInAutomobile = static_cast<CTaskMotionInAutomobile*>(FindSubTaskOfType(CTaskTypes::TASK_MOTION_IN_AUTOMOBILE));
			CTaskMotionOnBicycle* pOnBicycle = static_cast<CTaskMotionOnBicycle*>(FindSubTaskOfType(CTaskTypes::TASK_MOTION_ON_BICYCLE));
			CTask* pActiveTask = rPed.GetPedIntelligence()->GetTaskActive();

			// Cannot check reset flag since it hasn't been set yet this frame by the sub motion tasks (CTaskMotionOnBicycle::PutOnHelmet_OnProcessMoveSignals() and CTaskMotionInAutomobile::PutOnHelmet_OnUpdate())
			bool bPuttingOnHelmet = (pInAutomobile && (pInAutomobile->GetState() == CTaskMotionInAutomobile::State_PutOnHelmet)) || 
										  (pOnBicycle && (pOnBicycle->GetState() == CTaskMotionOnBicycle::State_PutOnHelmet));

			// Enable handlebar IK if we're putting on a helmet and the helmet is now attached to the ped's head
			// B*3048477 - Now do this all the time, not just during FPS mode.
			if (bPuttingOnHelmet)
			{
				bool bHelmetAttached = rPed.GetHelmetComponent()->IsAlreadyWearingAHelmetProp(m_pVehicle);
				bool bHelmetInHand = rPed.GetHelmetComponent()->IsHelmetInHand();
				if (bHelmetAttached && !bHelmetInHand)
				{
					bPuttingOnHelmet = false;
				}
			}

			const bool bIsBeingBusted = pActiveTask && pActiveTask->GetTaskType() == CTaskTypes::TASK_BUSTED;

			if (!rPed.GetIKSettings().IsIKFlagDisabled(IKManagerSolverTypes::ikSolverTypeArm) && !bPuttingOnHelmet && !bIsBeingBusted)
			{
				bool isDriver = (m_pVehicle->GetSeatManager()->GetDriver() == &rPed);
				bool isDriving = rPed.GetIsInVehicle() && !rPed.GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle);
				bool bDoingVehMelee = rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_IsPerformingVehicleMelee);

				// Need to query tasks rather than reset flags as this is run before the tasks have been processed
				CTaskAimGunVehicleDriveBy* pGunDriveByTask = static_cast<CTaskAimGunVehicleDriveBy*>(rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY));
				const bool isThrowingProjectile = rPed.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_MOUNT_THROW_PROJECTILE, true) && !bDoingVehMelee;
				bool isShooting = pGunDriveByTask || isThrowingProjectile;

				if (isShooting && pGunDriveByTask && pGunDriveByTask->GetState() == CTaskAimGunVehicleDriveBy::State_Outro)
				{		
					if (pGunDriveByTask)
					{
						isShooting = !pGunDriveByTask->ShouldBlendOutAimOutro();
					}
				}

				//! Aircraft can't do IK in certain states.
				if(m_pVehicle->GetIsAircraft() && pInAutomobile)
				{
					switch(pInAutomobile->GetState())
					{
					case CTaskMotionInAutomobile::State_StartEngine:
					case CTaskMotionInAutomobile::State_Hotwiring:
					case CTaskMotionInAutomobile::State_Start:
					case CTaskMotionInAutomobile::State_StreamAssets:
					case CTaskMotionInAutomobile::State_CloseDoor:
					case CTaskMotionInAutomobile::State_Horn:
					case CTaskMotionInAutomobile::State_PutOnHelmet:
						isDriving = false; 
						break;
					default:
						break;
					}
				}

				//! Don't process R hand IK.
				bool bUsingPhone = false;
				CTask *pPhoneTask = rPed.GetPedIntelligence()->FindTaskActiveByTreeAndType(PED_TASK_TREE_SECONDARY, CTaskTypes::TASK_MOBILE_PHONE);
				if(pPhoneTask && pPhoneTask->GetState() != CTaskMobilePhone::State_Paused)
				{
					bUsingPhone = true;
					m_bWasUsingPhone = true;
				}
				
				CTaskTakeOffHelmet* pTakeOffHelmetTask = static_cast<CTaskTakeOffHelmet*>(rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_TAKE_OFF_HELMET));

				if((isDriving || isShooting) && !pTakeOffHelmetTask)
				{
					CIkManager& ikManager = rPed.GetIkManager();
					bool bTorsoIK = false;

					const s32 controlFlags = AIK_TARGET_WRT_IKHELPER | AIK_USE_ORIENTATION | AIK_USE_FULL_REACH | AIK_USE_ANIM_BLOCK_TAGS;

					CTaskMountThrowProjectile* pProjTask = NULL;
					if (bDoingVehMelee)
					{
						pProjTask = static_cast<CTaskMountThrowProjectile*>(rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_MOUNT_THROW_PROJECTILE));
					}

					if (isDriver && bVehicleHasHandleBars)
					{
						int lBoneIdx = m_pVehicle->GetBoneIndex(VEH_HBGRIP_L);

						float fBlendInTime_LH_IK = PEDIK_ARMS_FADEIN_TIME;
						bool bBlockLeftArmIK = rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingHelmetVisor) || ShouldBlockArmIkDueToLowRider(pInAutomobile);
						if (bBlockLeftArmIK && pInAutomobile && rPed.IsLocalPlayer() && rPed.IsInFirstPersonVehicleCamera(true))
						{
							bBlockLeftArmIK = pInAutomobile->IsUsingFPVisorArmIK();
							if (!bBlockLeftArmIK)
								fBlendInTime_LH_IK = 0.0f;
						}

						if (bDoingVehMelee)
						{
							bBlockLeftArmIK = pProjTask && pProjTask->ShouldBlockHandIk(false);
							fBlendInTime_LH_IK = PEDIK_ARMS_FADEIN_TIME_QUICK;
						}

						if(lBoneIdx >= 0 && !bBlockLeftArmIK)
						{
							aiDebugf2("Frame : %i, Left Arm Ik On TRUE", fwTimer::GetFrameCount());
							ikManager.SetRelativeArmIKTarget(crIKSolverArms::LEFT_ARM, m_pVehicle, lBoneIdx, VEC3_ZERO, controlFlags, fBlendInTime_LH_IK, PEDIK_ARMS_FADEOUT_TIME);
							bTorsoIK = true;
						}

						int rBoneIdx = m_pVehicle->GetBoneIndex(VEH_HBGRIP_R);

						float fBlendInTime_RH_IK = PEDIK_ARMS_FADEIN_TIME;
						float fBlendOutTime_RH_IK = 0.1f;
						bool bBlockRightArmIK = (bDoingVehMelee && pProjTask && pProjTask->ShouldBlockHandIk(true));
						// B*2063410: Blend the IK in quickly if we're returning from using the mobile phone
						if (m_bWasUsingPhone || bDoingVehMelee)
						{
							fBlendInTime_RH_IK = PEDIK_ARMS_FADEIN_TIME_QUICK;
							m_bWasUsingPhone = false;
						}

						if(!isShooting && !bUsingPhone && rBoneIdx >= 0 && !bBlockRightArmIK)
						{
							aiDebugf2("Frame : %i, RIGHT Arm Ik On TRUE", fwTimer::GetFrameCount());
							ikManager.SetRelativeArmIKTarget(crIKSolverArms::RIGHT_ARM, m_pVehicle, rBoneIdx, VEC3_ZERO, controlFlags, fBlendInTime_RH_IK, fBlendOutTime_RH_IK);
							bTorsoIK = true;
						}
					}

					CIkRequestTorsoVehicle ikRequest;
					const CVehicleSeatAnimInfo* pSeatAnimInfo = m_pVehicle->GetSeatAnimationInfo(&rPed);
					float fAnimatedLean = 0.5f;
					u32 uFlags = 0;

					if (pSeatAnimInfo && !CTaskMotionInAutomobile::ShouldUseFemaleClipSet(rPed, *pSeatAnimInfo))
					{
						if (pSeatAnimInfo->GetUseTorsoLeanIK())
						{
							uFlags |= TORSOVEHICLEIK_APPLY_LEAN;
						}
						else if (pSeatAnimInfo->GetUseRestrictedTorsoLeanIK())
						{
							const ScalarV vFwdVelocity(Dot(VECTOR3_TO_VEC3V(m_pVehicle->GetVelocity()), m_pVehicle->GetTransform().GetB()));
							const bool bReversing = !m_pVehicle->IsInAir() && (m_pVehicle->GetThrottle() < 0.0f) && IsLessThanAll(vFwdVelocity, ScalarV(V_ZERO));
							const bool bDoingBurnout = (m_pVehicle->InheritsFromAutomobile() && static_cast<CAutomobile*>(m_pVehicle.Get())->IsInBurnout()) || 
													   (pInAutomobile && pInAutomobile->CheckIfVehicleIsDoingBurnout(*m_pVehicle, rPed));
							const bool bDoingDriveby = rPed.GetPedResetFlag(CPED_RESET_FLAG_IsDoingDriveby); // reset pre-task update so readable here OK
		
							if (bReversing || bDoingBurnout || bDoingDriveby)
							{
								uFlags |= TORSOVEHICLEIK_APPLY_LEAN_RESTRICTED;
							}
						}
					}

					if (uFlags & (TORSOVEHICLEIK_APPLY_LEAN | TORSOVEHICLEIK_APPLY_LEAN_RESTRICTED))
					{
						if (pInAutomobile)
						{
							fAnimatedLean = pInAutomobile->GetBodyMovementY();
						}
						else if (pOnBicycle)
						{
							fAnimatedLean = pOnBicycle->GetBodyLeanY();
						}

						if (!isDriver && !bDoingVehMelee)
						{
							int seatIdx = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(&rPed);
							if (seatIdx >= 0)
							{
								if (m_pVehicle->IsTurretSeat(seatIdx))
								{
									uFlags |= TORSOVEHICLEIK_APPLY_LIMITED_LEAN;
									// Turret seats can rotate 360 degrees, currently the ik solver only works wrt vehicle forward
									uFlags |= TORSOVEHICLEIK_APPLY_WRT_PED_ORIENTATION;
								}
								else
								{
									int seatBoneIdx = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromSeat(seatIdx);
									if (seatBoneIdx >= 0)
									{
										ikManager.SetRelativeArmIKTarget(crIKSolverArms::LEFT_ARM, m_pVehicle, seatBoneIdx, VEC3_ZERO, controlFlags);

										if (!isShooting)
										{
											ikManager.SetRelativeArmIKTarget(crIKSolverArms::RIGHT_ARM, m_pVehicle, seatBoneIdx, VEC3_ZERO, controlFlags);
										}
									}
								}
							}		

							if (m_pVehicle->GetIsJetSki())
							{
								TUNE_GROUP_FLOAT(IN_VEHICLE_TUNE, PASSENGER_JETSKI_IK_REACH_LIMIT_PERCENTAGE, 0.90f, 0.0f, 1.0f, 0.01f);
								ikRequest.SetReachLimitPercentage(PASSENGER_JETSKI_IK_REACH_LIMIT_PERCENTAGE);
							}
						}

						bTorsoIK = true;
					}

					TUNE_GROUP_BOOL(FIRST_PERSON_IN_VEHICLE_TUNE, ALLOW_TORSO_IK_IN_FIRST_PERSON, false);
					if (!ALLOW_TORSO_IK_IN_FIRST_PERSON && rPed.IsInFirstPersonVehicleCamera() && !m_pVehicle->GetIsAircraft())
					{
						if (!m_pVehicle->InheritsFromBike())
						{
							bTorsoIK = false;
						}
						else if (m_pVehicle->InheritsFromBicycle())
						{
							// Disable since bicycle POV cameras are set far enough back that it can cause clipping with the neck on steep slopes.
							bTorsoIK = false;
						}
					}

					TUNE_GROUP_BOOL(FIRST_PERSON_IN_VEHICLE_TUNE, ALLOW_TORSO_IK_FOR_QUAD_BIKE, true);
					if (ALLOW_TORSO_IK_FOR_QUAD_BIKE && CTaskMotionInAutomobile::IsFirstPersonDrivingQuadBike(rPed, *m_pVehicle))
					{
						bTorsoIK = true;
					}

					TUNE_GROUP_BOOL(FIRST_PERSON_IN_VEHICLE_TUNE, DISABLE_TORSO_IK_WHEN_DRIVEBYING, true);
					if (DISABLE_TORSO_IK_WHEN_DRIVEBYING && rPed.IsInFirstPersonVehicleCamera() && isShooting)
					{
						bTorsoIK = false;
					}

					TUNE_GROUP_BOOL(FIRST_PERSON_IN_VEHICLE_TUNE, DISABLE_TORSO_IK_WHEN_USING_PHONE, true);
					if (DISABLE_TORSO_IK_WHEN_USING_PHONE && rPed.IsInFirstPersonVehicleCamera() && bUsingPhone)
					{
						bTorsoIK = false;
					}

					if (bTorsoIK)
					{
						if (CTaskMotionInAutomobile::IsFirstPersonDrivingQuadBike(rPed, *m_pVehicle))
						{
							TUNE_GROUP_FLOAT(FIRST_PERSON_IN_VEHICLE_TUNE, FORCED_ANIMATED_LEAN_QUAD, 0.0f, 0.0f, 1.0f, 0.01f);
							TUNE_GROUP_FLOAT(FIRST_PERSON_IN_VEHICLE_TUNE, QUAD_BIKE_TORSO_IK_DELTA_SCALE, 0.2f, 0.0f, 1.0f, 0.01f);
							ikRequest.SetAnimatedLean(FORCED_ANIMATED_LEAN_QUAD);
							ikRequest.SetDeltaScale(QUAD_BIKE_TORSO_IK_DELTA_SCALE);
						}
						else if (CTaskMotionInAutomobile::IsFirstPersonDrivingBicycle(rPed, *m_pVehicle))
						{
							TUNE_GROUP_FLOAT(FIRST_PERSON_IN_VEHICLE_TUNE, FORCED_ANIMATED_LEAN_BICYCLE, 0.0f, 0.0f, 1.0f, 0.01f);
							ikRequest.SetAnimatedLean(FORCED_ANIMATED_LEAN_BICYCLE);
						}
						else
						{
							ikRequest.SetAnimatedLean(fAnimatedLean);
						}
						ikRequest.SetFlags(uFlags);
						ikManager.Request(ikRequest);
					}
				}
			}
		}
	}

	// Moved to per-frame from InRemoteControlVehicle_OnUpdate to prevent timeslicing
	if (m_pVehicle && GetState() == State_InRemoteControlVehicle)
	{
		rPed.SetPedResetFlag(CPED_RESET_FLAG_DontRenderThisFrame, true);
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInVehicle::GetNMBlendOutClip(fwMvClipSetId& outClipSet, fwMvClipId& outClip)
{
	if (m_pVehicle)
	{
		const CVehicleSeatAnimInfo* pSeatClipInfo = m_pVehicle->GetSeatAnimationInfo(GetPed());
		if (pSeatClipInfo)
		{
			outClipSet = pSeatClipInfo->GetSeatClipSetId();
			outClip = CLIP_IDLE;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

Vec3V_Out CTaskMotionInVehicle::CalcDesiredVelocity(Mat34V_ConstRef UNUSED_PARAM(updatedPedMatrix),float UNUSED_PARAM(fTimestep))
{
	return VECTOR3_TO_VEC3V(GetPed()->GetVelocity());
}

////////////////////////////////////////////////////////////////////////////////

const CEntryExitPoint *CTaskMotionInVehicle::GetEntryExitPointForPedsSeat(CPed *pPed) const
{
    const CEntryExitPoint *pEntryExitPoint = 0;

    if(pPed)
    {
        if( m_pVehicle )
	    {
		    s32 iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);

		    if (iSeatIndex > -1)
		    {
			    // Lookup the entry point from our seat
			    s32 iDirectEntryPoint = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iSeatIndex, m_pVehicle);

			    if( m_pVehicle->IsEntryIndexValid(iDirectEntryPoint) )
			    {
					if(taskVerifyf(m_pVehicle->GetVehicleModelInfo(), "Vehicle has no model info!"))
                    {
					    pEntryExitPoint = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryExitPoint(iDirectEntryPoint);
                    }
                }
            }
        }
    }

    return pEntryExitPoint;
}

////////////////////////////////////////////////////////////////////////////////

CTask* CTaskMotionInVehicle::CreatePlayerControlTask()
{
	if (GetPed()->GetIsInVehicle())
	{
		return rage_new CTaskPlayerDrive();
	}
	else
	{
		return rage_new CTaskPlayerOnFoot();
	}
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInVehicle::Start_OnEnter()
{
	m_VehicleClipRequestHelper.SetPed(GetPed());

	if(m_pVehicle && m_pVehicle->m_nVehicleFlags.bEngineOn)
	{
		m_pVehicle->m_nVehicleFlags.bCarNeedsToBeHotwired = false;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInVehicle::Start_OnUpdate()
{
	const CPed* pPed = GetPed();

	// Don't do anything if dead (clips handled in dead task) or ragdolling
	if (pPed->IsDead() || pPed->GetUsingRagdoll())
	{
		// Unless we need to reposition
		if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedsFullyInSeat))
		{
			return FSM_Continue;
		}
	}

	// we need to wait for the vehicle to be cloned on this
	// machine for network clone peds
	if (m_pVehicle)
	{
		if (m_pVehicle->pHandling->mFlags & MF_IS_RC)
		{
			SetState(State_InRemoteControlVehicle);
			return FSM_Continue;
		}
		else
		{
			SetState(State_StreamAssets);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInVehicle::StreamAssets_OnEnter()
{
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInVehicle::StreamAssets_OnUpdate()
{
	CPed* pPed = GetPed();
	taskAssert(m_pVehicle);
	
	const CVehicleSeatAnimInfo* pSeatClipInfo = m_pVehicle->GetSeatAnimationInfo(pPed);
	if (taskVerifyf(pSeatClipInfo, "NULL seat clip info"))
	{
		if (pSeatClipInfo->GetInVehicleMoveNetwork() == CVehicleSeatAnimInfo::VEHICLE_BICYCLE)
		{
			SetState(State_OnBicycle);
			return FSM_Continue;
		}
		else if (pSeatClipInfo->IsTurretSeat())
		{
			SetState(State_InTurret);
			return FSM_Continue;
		}
		else
		{
			SetState(State_InAutomobile);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInVehicle::InAutomobile_OnEnter()
{
	SetNewTask(rage_new CTaskMotionInAutomobile(m_pVehicle, m_moveNetworkHelper));
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInVehicle::InAutomobile_OnUpdate()
{
	CPed* pPed = GetPed();

	float fHairScale = 0.0f;
	if (ShouldPerformInVehicleHairScale(*pPed, fHairScale))
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_OverrideHairScale, true);
		pPed->SetTargetHairScale(fHairScale);
	}

	ProcessDamageDeath();

	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finish);
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInVehicle::InRemoteControlVehicle_OnEnter()
{
	if (taskVerifyf(m_pVehicle, "Vehicle was NULL in CTaskMotionInVehicle::InRemoteControlVehicle_OnEnter"))
	{
		m_pVehicle->SwitchEngineOn(true);
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInVehicle::InRemoteControlVehicle_OnUpdate()
{
	// Rendering flag was moved to ProcessMoveSignals as it needs to be called every frame

	if (taskVerifyf(m_pVehicle, "Vehicle was NULL in CTaskMotionInVehicle::InRemoteControlVehicle_OnUpdate"))
	{
		if (!m_pVehicle->m_nVehicleFlags.bEngineOn && !m_pVehicle->GetExplosionEffectEMP() && m_pVehicle->GetStatus() != STATUS_WRECKED)
		{
			m_pVehicle->SwitchEngineOn(true);
		}

		// if no longer in an RC vehicle got back to start state
		if ((m_pVehicle->pHandling->mFlags & MF_IS_RC) == 0)
		{
			SetState(State_Start);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInVehicle::OnBicycle_OnEnter()
{
	SetNewTask(rage_new CTaskMotionOnBicycle(m_pVehicle, m_moveNetworkHelper, CClipNetworkMoveInfo::ms_NetworkTaskMotionOnBicycle));
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInVehicle::OnBicycle_OnUpdate()
{
	ProcessDamageDeath();

	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finish);
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInVehicle::InTurret_OnEnter()
{
#if ENABLE_MOTION_IN_TURRET_TASK
	SetNewTask(rage_new CTaskMotionInTurret(m_pVehicle, m_moveNetworkHelper));
#endif // ENABLE_MOTION_IN_TURRET_TASK
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInVehicle::InTurret_OnUpdate()
{
	ProcessDamageDeath();

	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finish);
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInVehicle::ProcessDamageDeath()
{
	CPed* pPed = GetPed();
	
	// Unless we need to reposition
	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedsFullyInSeat))
	{
		if (pPed->IsDead() || pPed->GetUsingRagdoll())
		{
			SetState(State_Start);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInVehicle::ProcessShuffleForGroupMember()
{
	// Only do this for single player
	if (!NetworkInterface::IsGameInProgress())
	{
		// Only for cars
		if (m_pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR)
		{		
			// Only when ped is inside vehicle, not getting in or out
			CPed& ped = *GetPed();
			if (ped.GetIsInVehicle() && !ped.GetPedResetFlag(CPED_RESET_FLAG_IsEnteringOrExitingVehicle))
			{
				// Don't shuffle over if we've been told to use a specific seat and we're in it already
				if (ped.GetPedConfigFlag(CPED_CONFIG_FLAG_ForcedToUseSpecificGroupSeatIndex))
				{
					const s32 iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(&ped);
					if (iSeatIndex == ped.m_PedConfigFlags.GetPassengerIndexToUseInAGroup())
					{
						return;
					}
				}

				s32 iDirectEntryIndex = m_pVehicle->GetDirectEntryPointIndexForPed(&ped);
				if (m_pVehicle->IsEntryIndexValid(iDirectEntryIndex))
				{
					// If we're in the rear seat of a car
					const CEntryExitPoint* pEntryPoint = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryExitPoint(iDirectEntryIndex);
					SeatAccess thisSeatAccess = pEntryPoint->CanSeatBeReached(0);
					if (thisSeatAccess == SA_invalid)
					{
						// If we're a following ped in a group
						const CPedGroup* pGroup = ped.GetPedsGroup();
						if (pGroup && pGroup->GetGroupMembership()->IsFollower(&ped))
						{
							// Make sure there's more than one follower
							s32 iNumMembers = pGroup->GetGroupMembership()->CountMembersExcludingLeader();
							if (iNumMembers > 1)
							{
								// See if an extra follower is on foot still
								for (s32 i=0; i<iNumMembers; ++i)
								{
									CPed* pFollower = pGroup->GetGroupMembership()->GetNthMember(i);
									if (pFollower && !pFollower->GetIsInVehicle())
									{
										// If the indirect access entry is blocked shuffle over
										const s32 iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(&ped);
										s32 iInDirectEntryIndex = m_pVehicle->GetIndirectEntryPointIndexForSeat(iSeatIndex);
										if (m_pVehicle->IsEntryIndexValid(iInDirectEntryIndex))
										{
											if (!CVehicle::IsEntryPointClearForPed(*m_pVehicle, ped, iInDirectEntryIndex) &&
												CVehicle::IsEntryPointClearForPed(*m_pVehicle, ped, iDirectEntryIndex))
											{
												CEventGivePedTask givePedTask(PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP, rage_new CTaskInVehicleSeatShuffle(m_pVehicle, NULL), true );
												ped.GetPedIntelligence()->AddEvent(givePedTask);
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
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInVehicle::CheckCanPedMoveToDriversSeat(const CPed& ped, const CVehicle& vehicle)
{

#if AI_DEBUG_OUTPUT_ENABLED
	const CPed* pPed = &ped;
#endif
	
	TUNE_GROUP_FLOAT(IN_VEHICLE_TUNE, MIN_TIME_IN_MOTION_TASK_TO_WARP_TO_DRIVERS_SEAT, 1.0f, 0.0f, 2.0f, 0.01f);
	if (GetTimeRunning() < MIN_TIME_IN_MOTION_TASK_TO_WARP_TO_DRIVERS_SEAT)
	{
		AI_LOG_WITH_ARGS_IF_LOCAL_PLAYER(pPed, "[Shuffle] FAILED: Time Running %.2f is less than 1.0 sec for Ped [%s]\n", GetTimeRunning(), AILogging::GetDynamicEntityNameSafe(pPed));
		return false;
	}


	if (!vehicle.IsAnExclusiveDriverPedOrOpenSeat(&ped))
	{
		AI_LOG_WITH_ARGS_IF_LOCAL_PLAYER(pPed, "[Shuffle] FAILED: !vehicle.IsAnExclusiveDriverPedOrOpenSeat for Ped [%s] \n", AILogging::GetDynamicEntityNameSafe(pPed));
		return false;
	}

	if (NetworkInterface::IsGameInProgress() && ped.IsLocalPlayer())
	{
		const s32 iTimeDriverWasValid = fwTimer::GetTimeInMilliseconds() - m_uTimeDriverValid;
		if (iTimeDriverWasValid < ms_Tunables.m_MinTimeSinceDriverValidToShuffle)
		{
			AI_LOG_WITH_ARGS_IF_LOCAL_PLAYER(pPed, "[Shuffle] FAILED: TimeDriverWasValid: %i < MinTimeSinceDriverValidToShuffle: %i for Ped [%s] \n", iTimeDriverWasValid, ms_Tunables.m_MinTimeSinceDriverValidToShuffle, AILogging::GetDynamicEntityNameSafe(pPed));
			return false;
		}

		// We also need to check the player drive task in case we're stuck waiting to reserve the seat B*1625802
		aiTask* pDriveTask = ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_DRIVE);
		if (pDriveTask && pDriveTask->GetTimeInState() < MIN_TIME_IN_MOTION_TASK_TO_WARP_TO_DRIVERS_SEAT)
		{
			AI_LOG_WITH_ARGS_IF_LOCAL_PLAYER(pPed, "[Shuffle] FAILED: Waiting to reserve seat. DriveTaskTimeInState: %.2f < 1.0s for Ped [%s] \n", pDriveTask->GetTimeInState(), AILogging::GetDynamicEntityNameSafe(pPed));
			return false;
		}
	}

	// If somebody else has seat reserved, don't shuffle
	s32 iDriverSeat = vehicle.GetDriverSeat();
	if (vehicle.IsSeatIndexValid(iDriverSeat))
	{
		CComponentReservation* pComponentReservation = const_cast<CVehicle&>(vehicle).GetComponentReservationMgr()->GetSeatReservationFromSeat(iDriverSeat);
		const CPed* pPedReservingSeat = pComponentReservation ? pComponentReservation->GetPedUsingComponent() : NULL;
		if (pPedReservingSeat && pPedReservingSeat != &ped)
		{
			AI_LOG_WITH_ARGS_IF_LOCAL_PLAYER(pPed, "[Shuffle] FAILED: Ped Reserving Seat [%s] is not Ped [%s] \n", AILogging::GetDynamicEntityNameSafe(pPedReservingSeat), AILogging::GetDynamicEntityNameSafe(pPed));
			return false;
		}
	}

	if (!ped.IsInjured() && !ped.GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed) && 
		(ped.IsLocalPlayer() || ped.IsLawEnforcementPed() || ped.GetPedConfigFlag(CPED_CONFIG_FLAG_AllowAutoShuffleToDriversSeat)))
	{
		if (!CTaskVehicleFSM::IsAnyPlayerEnteringDirectlyAsDriver(&ped, &vehicle))
		{
			CTaskMotionInAutomobile* pMotionInAutoTask = (CTaskMotionInAutomobile*)ped.GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_AUTOMOBILE);

			if (pMotionInAutoTask && !pMotionInAutoTask->CheckCanPedMoveToDriversSeat())
			{
				AI_LOG_WITH_ARGS_IF_LOCAL_PLAYER(pPed, "[Shuffle] FAILED: !pMotionInAutoTask->CheckCanPedMoveToDriversSeat() for Ped [%s] \n", AILogging::GetDynamicEntityNameSafe(pPed));
				return false;
			}

			if (!ped.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE) &&
				!ped.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE) &&
				!ped.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE))
			{
				return true;
			}
		}
	}

	AI_LOG_WITH_ARGS_IF_LOCAL_PLAYER(pPed, "[Shuffle] FAILED: Returning CheckCanPedMoveToDriversSeat as false for Ped [%s], AllowAutoShuffleToDriversSeat [%s] \n", AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetBooleanAsString(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_AllowAutoShuffleToDriversSeat)));
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInVehicle::GetShouldExitVehicle(CPed* pPed)
{
	if (pPed)
	{
		if(m_pVehicle && m_pVehicle->IsDriver(pPed) && !m_pVehicle->IsAnExclusiveDriverPedOrOpenSeat(pPed))
		{
			AI_LOG_WITH_ARGS("[VehicleEntryExit][CTaskMotionInVehicle::ProcessPreFSM] - Ped %s being given TASK_EXIT_VEHICLE because someone else is exclusive driver\n", AILogging::GetDynamicEntityNameSafe(pPed));
			return true;
		}

	
		if(pPed->IsLocalPlayer())
		{
			if(m_pVehicle && m_pVehicle->GetCanEjectPassengersIfLocked() && !m_pVehicle->IsDriver(pPed))
			{
				int iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
				if(m_pVehicle->GetDoorLockStateFromSeatIndex(iSeatIndex) == CARLOCK_LOCKED_NO_PASSENGERS)
				{
					AI_LOG_WITH_ARGS("[VehicleEntryExit][CTaskMotionInVehicle::ProcessPreFSM] - Ped %s being given TASK_EXIT_VEHICLE because they are being ejected due to vehicle being locked\n", AILogging::GetDynamicEntityNameSafe(pPed));
					return true;
				}
			}

			// Exit the car if the driver is an ambient AI ped that doesnt have AIDriverAllowFriendlyPassengerSeatEntry set on him (like taxi drivers)
			// But stay if "RAG/Vehicle Ped Tasks/Task Debug/Force player to use seat index specified as target seat normally" is enabled for debug purposes
			TUNE_GROUP_INT(VEHICLE_ENTRY_TUNE, TIME_BEFORE_EXITING_AMBIENT_VEHICLE, 3000, 0, 5000, 100);
			if (BANK_ONLY(!CVehicleDebug::ms_bForcePlayerToUseSpecificSeat &&) m_uTimeInVehicle + TIME_BEFORE_EXITING_AMBIENT_VEHICLE < fwTimer::GetTimeInMilliseconds())
			{
				CPed* pDriverPed = m_pVehicle->GetDriver();
				if (NetworkInterface::IsGameInProgress() && pDriverPed && (pPed != pDriverPed) && !pDriverPed->IsPlayer() && !pDriverPed->PopTypeIsMission() && pDriverPed->PopTypeGetPrevious() != POPTYPE_MISSION && !pDriverPed->GetPedConfigFlag(CPED_CONFIG_FLAG_AIDriverAllowFriendlyPassengerSeatEntry))
				{
					AI_LOG_WITH_ARGS("[CTaskMotionInVehicle::GetShouldExitVehicle] Forcing %s out of the vehicle %s because driver (%s) is AI ped\n", AILogging::GetDynamicEntityNameSafe(pPed), m_pVehicle->GetVehicleModelInfo()->GetModelName(), AILogging::GetDynamicEntityNameSafe(pDriverPed));
					return true;
				}
			}

			// Force ped out of vehicle if it was flagged with "CPED_RESET_FLAG_UseNormalExplosionDamageWhenBlownUpInVehicle" and vehicle is now wrecked.
			if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UseNormalExplosionDamageWhenBlownUpInVehicle) && !pPed->IsDead() && m_pVehicle->GetStatus() == STATUS_WRECKED)
			{
				AI_LOG_WITH_ARGS("[CTaskMotionInVehicle::GetShouldExitVehicle] Forcing %s out of the vehicle %s because CPED_CONFIG_FLAG_UseNormalExplosionDamageWhenBlownUpInVehicle is set, is not dead and vehicle is wrecked.", AILogging::GetDynamicEntityNameSafe(pPed), m_pVehicle->GetVehicleModelInfo()->GetModelName());
				return true;
			}
		}
	}

	return false;
}

bool CTaskMotionInVehicle::ShouldBlockArmIkDueToLowRider(const CTaskMotionInAutomobile* pInAutomobile) const
{
	if (!pInAutomobile)
		return false;

	if (!pInAutomobile->IsUsingAlternateLowriderClipset())
		return false;

	if (pInAutomobile->GetState() == CTaskMotionInAutomobile::State_StillToSit || pInAutomobile->GetState() == CTaskMotionInAutomobile::State_SitToStill)
		return false;

	return true;
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
void CTaskMotionInVehicle::Debug() const
{
	if (GetSubTask())
		GetSubTask()->Debug();

#if __BANK
	m_VehicleClipRequestHelper.Debug();
#endif
}
#endif // __FINAL

////////////////////////////////////////////////////////////////////////////////

#if __DEV
void CTaskMotionInVehicle::VerifyNetworkStartUp(const CVehicle& vehicle, const CPed& ped, const fwMvClipSetId& seatClipSetId)
{
	if (vehicle.IsDriver(&ped))
	{
		int iSeatIndex = vehicle.GetSeatManager()->GetPedsSeatIndex(&ped);
		taskAssertf(iSeatIndex == 0, "Ped is driver but their seat index isn't 0, how can this happen? : Clipset used was %s", seatClipSetId.GetCStr());
	}

	if (taskVerifyf(seatClipSetId != CLIP_SET_ID_INVALID, "Check vehicle metadata for valid in vehicle seat clipset hash"))
	{
		fwClipSet* pClipSet = fwClipSetManager::GetClipSet(seatClipSetId);
		taskAssertf(pClipSet, "Clipset (%s) doesn't exist", seatClipSetId.GetCStr());
	}
}
#endif // __DEV

////////////////////////////////////////////////////////////////////////////////

// Statics
ioRumbleEffect CTaskMotionInAutomobile::m_RumbleEffect;
const fwMvBooleanId CTaskMotionInAutomobile::ms_CloseDoorClipFinishedId("CloseDoorAnimFinished",0xA23C50F3);
const fwMvBooleanId CTaskMotionInAutomobile::ms_EngineStartClipFinishedId("KeyStartAnimFinished",0x11F3036D);
const fwMvClipId CTaskMotionInAutomobile::ms_CloseDoorClipId("CloseDoorClip",0xD9C00C84);
const fwMvClipId CTaskMotionInAutomobile::ms_StartEngineClipId("StartEngineClip",0x5E5B3249);
const fwMvClipId CTaskMotionInAutomobile::ms_HotwireClipId("HotwireCLip",0x9A134EAC);
const fwMvClipId CTaskMotionInAutomobile::ms_InputHotwireClipId("InputHotwireClip",0xaf96eec2);
const fwMvFloatId CTaskMotionInAutomobile::ms_CloseDoorPhaseId("CloseDoorPhase",0x3F197B37);
const fwMvFloatId CTaskMotionInAutomobile::ms_StartEnginePhaseId("StartEnginePhase",0x52740191);
const fwMvFloatId CTaskMotionInAutomobile::ms_ChangeStationPhaseId("ChangeStationPhase",0x1D51F152);
const fwMvFloatId CTaskMotionInAutomobile::ms_HotwiringPhaseId("HotwiringPhase",0x1FEB6210);
const fwMvFloatId CTaskMotionInAutomobile::ms_PutOnHelmetPhaseId("PutOnHelmetPhase",0x6f3d6df0);
const fwMvFlagId CTaskMotionInAutomobile::ms_UseStandardInVehicleAnimsFlagId("UseStandardInVehicleAnims",0xB43B7549);
const fwMvFlagId CTaskMotionInAutomobile::ms_UseBikeInVehicleAnimsFlagId("UseBikeInVehicleAnims",0xCCFA5D70);
const fwMvFlagId CTaskMotionInAutomobile::ms_UseBasicAnimsFlagId("UseBasicAnims",0x82091648);
const fwMvFlagId CTaskMotionInAutomobile::ms_UseJetSkiInVehicleAnimsId("UseJetSkiInVehicleAnims",0x82C7DD2);
const fwMvFlagId CTaskMotionInAutomobile::ms_UseBoatInVehicleAnimsId("UseBoatInVehicleAnims",0xDECFAF79);
const fwMvFlagId CTaskMotionInAutomobile::ms_LowInAirFlagId("LowInAir",0x2D247805);
const fwMvFlagId CTaskMotionInAutomobile::ms_LowImpactFlagId("LowImpact",0xEA993182);
const fwMvFlagId CTaskMotionInAutomobile::ms_MedImpactFlagId("MedImpact",0x236FD74B);
const fwMvFlagId CTaskMotionInAutomobile::ms_HighImpactFlagId("HighImpact",0x49B36B0D);
const fwMvFlagId CTaskMotionInAutomobile::ms_UseLeanSteerAnimsFlagId("UseLeanSteerAnims",0x58B93905);
const fwMvFlagId CTaskMotionInAutomobile::ms_DontUsePassengerLeanAnimsFlagId("DontUsePassengerLeanAnims",0x5a11377d);
const fwMvFlagId CTaskMotionInAutomobile::ms_UseIdleSteerAnimsFlagId("UseIdleSteerAnims",0x3d9e5359);
const fwMvFlagId CTaskMotionInAutomobile::ms_FirstPersonCameraFlagId("FirstPersonCamera", 0xaff55091);
const fwMvFlagId CTaskMotionInAutomobile::ms_FirstPersonModeFlagId("FirstPersonMode", 0x8BB6FFFA);
const fwMvFlagId CTaskMotionInAutomobile::ms_UsePOVAnimsFlagId("UsePOVAnims", 0x7e133144);
const fwMvFlagId CTaskMotionInAutomobile::ms_SkipDuckIntroOutrosFlagId("SkipDuckIntroOutros", 0x68d143c0);
const fwMvRequestId CTaskMotionInAutomobile::ms_ChangeStationRequestId("ChangeStation",0x6B0C5949);
const fwMvBooleanId CTaskMotionInAutomobile::ms_ChangeStationOnEnterId("ChangeStation_OnEnter",0x5FE3C8AB);
const fwMvBooleanId CTaskMotionInAutomobile::ms_ChangeStationClipFinishedId("ChangeStationAnimFinished",0xB991C06B);
const fwMvBooleanId CTaskMotionInAutomobile::ms_ChangeStationShouldLoopId("ChangeStationShouldLoop",0x9085A04);
const fwMvRequestId CTaskMotionInAutomobile::ms_HotwiringRequestId("Hotwiring",0xE884B4CA);
const fwMvBooleanId CTaskMotionInAutomobile::ms_HotwiringOnEnterId("Hotwiring_OnEnter",0x77DEF073);
const fwMvBooleanId CTaskMotionInAutomobile::ms_HotwiringClipFinishedId("HotwireAnimFinished",0xA1785B45);
const fwMvBooleanId CTaskMotionInAutomobile::ms_HotwiringShouldLoopId("HotwiringShouldLoop",0xF2F356C2);
const fwMvBooleanId CTaskMotionInAutomobile::ms_ShuntAnimFinishedId("ShuntAnimFinished",0xF3CDE86D);
const fwMvBooleanId CTaskMotionInAutomobile::ms_HeavyBrakeClipFinishedId("HeavyBrakeClipFinished",0xF4D3FD1E);
const fwMvRequestId CTaskMotionInAutomobile::ms_StillToSitRequestId("StillToSit", 0x9d365807);
const fwMvRequestId CTaskMotionInAutomobile::ms_SitToStillRequestId("SitToStill", 0xcff11877);
const fwMvBooleanId CTaskMotionInAutomobile::ms_StillToSitOnEnterId("StillToSit_OnEnter", 0xa58f9d5e);
const fwMvBooleanId CTaskMotionInAutomobile::ms_SitToStillOnEnterId("SitToStill_OnEnter", 0x1917da91);
const fwMvBooleanId CTaskMotionInAutomobile::ms_PutOnHelmetOnEnterId("PutOnHelmet_OnEnter", 0xF2B6391B);
const fwMvBooleanId CTaskMotionInAutomobile::ms_StartEngineId("StartEngine",0x43377B8E);
const fwMvFloatId CTaskMotionInAutomobile::ms_HotwireRate("HotwireRate",0x524CE1BF);
const fwMvFloatId CTaskMotionInAutomobile::ms_ShuntPhaseId("ShuntPhase",0x6E0A42C5);
const fwMvFloatId CTaskMotionInAutomobile::ms_StillToSitRate("StillToSitRate", 0xd5f89671);
const fwMvFloatId CTaskMotionInAutomobile::ms_SitToStillRate("SitToStillRate", 0x7169f1a9);
const fwMvFloatId CTaskMotionInAutomobile::ms_StillToSitPhaseId("StillToSitPhase", 0x8c9fee84);
const fwMvFloatId CTaskMotionInAutomobile::ms_SitToStillPhaseId("SitToStillPhase", 0x79c4c04a);
const fwMvFloatId CTaskMotionInAutomobile::ms_HeavyBrakePhaseId("HeaveBrakePhase",0x33DB794A);
const fwMvFloatId CTaskMotionInAutomobile::ms_SitFirstPersonRateId("SitFirstPersonRate", 0x4c309d6f);
const fwMvFloatId CTaskMotionInAutomobile::ms_InVehicleAnimsRateId("InVehicleAnimRate",0x7C708B5);
const fwMvFloatId CTaskMotionInAutomobile::ms_InVehicleAnimsPhaseId("InVehicleAnimPhase",0x4bdcebaf);
const fwMvFloatId CTaskMotionInAutomobile::ms_InvalidPhaseId("InvalidPhase",0xB4E4374C);
const fwMvFloatId CTaskMotionInAutomobile::ms_StartEngineBlendDurationId("StartEngineBlendDuration",0x6BB9EF4D);
const fwMvFloatId CTaskMotionInAutomobile::ms_ShuntToIdleBlendDurationId("ShuntToIdleBlendDuration",0x429c973f);
const fwMvFloatId CTaskMotionInAutomobile::ms_IdleBlendDurationId("IdleBlendDuration",0x73FD56C0);
const fwMvFloatId CTaskMotionInAutomobile::ms_BurnOutBlendId("BurnOutBlend",0x2d507927);
const fwMvFloatId CTaskMotionInAutomobile::ms_FirstPersonBlendDurationId("FirstPersonBlendDuration",0xdfcadd55);
const fwMvFloatId CTaskMotionInAutomobile::ms_RestartIdleBlendDurationId("RestartIdleBlendDuration",0x20831b31);
const fwMvBooleanId CTaskMotionInAutomobile::ms_PutOnHelmetClipFinishedId("PutOnHelmetAnimFinished",0xDF68E618);
const fwMvBooleanId CTaskMotionInAutomobile::ms_PovHornExitClipFinishedId("PovHornExitClip_Ended",0x30634c73);
const fwMvRequestId CTaskMotionInAutomobile::ms_PutOnHelmetRequestId("PutOnHelmet",0x80B20F39);
const fwMvBooleanId CTaskMotionInAutomobile::ms_PutOnHelmetCreateId("CreateHelmet",0x6A8FE35C);
const fwMvBooleanId CTaskMotionInAutomobile::ms_AttachHelmetRequestId("AttachHelmet",0x5543b1f3);
const fwMvBooleanId CTaskMotionInAutomobile::ms_PutOnHelmetInterruptId("HelmetInterrupt",0x15220231);
const fwMvBooleanId CTaskMotionInAutomobile::ms_CanKickStartDSId("CanKickStartDS",0x58C20F4B);
const fwMvBooleanId CTaskMotionInAutomobile::ms_CanKickStartPSId("CanKickStartPS",0x4CEE0EF3);
const fwMvBooleanId CTaskMotionInAutomobile::ms_BurnOutInterruptId("BurnOutInterrupt",0x10ce8715);
const fwMvClipId CTaskMotionInAutomobile::ms_PutOnHelmetClipId("PutOnHelmetClip",0x51E3C230);
const fwMvClipId CTaskMotionInAutomobile::ms_ChangeStationClipId("ChangeStationClip",0x99790337);
const fwMvClipId CTaskMotionInAutomobile::ms_StillToSitClipId("StillToSitClip",0x09cf842a);
const fwMvClipId CTaskMotionInAutomobile::ms_SitToStillClipId("SitToStillClip",0x69082a75);
const fwMvClipId CTaskMotionInAutomobile::ms_BurnOutClipId("BurnOutClip", 0x446295ea);
const fwMvFlagId CTaskMotionInAutomobile::ms_Use2DBodyBlendFlagId("Use2DBodyBlend",0x1bd49b47);
const fwMvFlagId CTaskMotionInAutomobile::ms_IsDuckingFlagId("IsDucking",0x9d6e43b8);
const fwMvFlagId CTaskMotionInAutomobile::ms_PlayDuckOutroFlagId("PlayDuckOutro",0xadb8f0e5);
const fwMvClipSetVarId CTaskMotionInAutomobile::ms_DuckClipSetId("DuckClipSet", 0x98bab7ab);

const fwMvFlagId CTaskMotionInAutomobile::ms_SwitchVisor("SwitchVisor",0x4DD23266);
const fwMvClipId CTaskMotionInAutomobile::ms_SwitchVisorClipId("SwitchVisorClip",0x56886B43);
const fwMvFloatId CTaskMotionInAutomobile::ms_SwitchVisorPhaseId("SwitchVisorPhase",0x1C8FC42D);
const fwMvBooleanId CTaskMotionInAutomobile::ms_SwitchVisorAnimFinishedId("SwitchVisorAnimFinished",0xE7D2968C);
const fwMvBooleanId CTaskMotionInAutomobile::ms_SwitchVisorAnimTag("VisorSwitch",0xB65F4A2D);
const fwMvClipSetId CTaskMotionInAutomobile::ms_SwitchVisorPOVClipSet("SwitchVisorPOV",0xC6E30FB3);

const fwMvClipSetVarId CTaskMotionInAutomobile::ms_FirstPersonAdditiveClipSetId("FirstPersonAdditiveClipSet",0x48bd0727);
const fwMvClipSetVarId CTaskMotionInAutomobile::ms_FirstPersonSteeringClipSetId("FirstPersonSteeringClipSet",0x7b17f8ba);
const fwMvClipSetVarId CTaskMotionInAutomobile::ms_FirstPersonRoadRageClipSetId("FirstPersonRoadRageClipSet",0xe7a4ad1e);

const fwMvFlagId CTaskMotionInAutomobile::ms_PlayFirstPersonAdditive("PlayFirstPersonAdditive",0x92e647da);
const fwMvBooleanId CTaskMotionInAutomobile::ms_FirstPersonAdditiveIdleFinishedId("FirstPersonAdditiveIdleFinished",0x4fba56ca);
const fwMvFlagId CTaskMotionInAutomobile::ms_PlayFirstPersonShunt("PlayFirstPersonShunt",0xe19b1dd2);
const fwMvFlagId CTaskMotionInAutomobile::ms_PlayFirstPersonRoadRage("PlayFirstPersonRoadRage",0x8ca745fb);

const fwMvBooleanId CTaskMotionInAutomobile::ms_FirstPersonShuntFinishedId("FirstPersonShuntFinished",0xfca53708);
const fwMvBooleanId CTaskMotionInAutomobile::ms_FirstPersonRoadRageFinishedId("FirstPersonRoadRageFinished",0xdf987653);
const fwMvFlagId CTaskMotionInAutomobile::ms_FPSHelmetPutOnUseUpperBodyOnly("FPSHelmetPutOnUseUpperBodyOnly", 0x5fd1b0f7);

////////////////////////////////////////////////////////////////////////////////

// Statics
CTaskMotionInAutomobile::Tunables CTaskMotionInAutomobile::ms_Tunables;

IMPLEMENT_VEHICLE_TASK_TUNABLES(CTaskMotionInAutomobile, 0xb1e7d367);

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::IsTaskValid(const CVehicle* pVehicle, s32 iSeatIndex)
{
	if (!pVehicle || !pVehicle->IsSeatIndexValid(iSeatIndex))
		return false;

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::ShouldUseFemaleClipSet(const CPed& rPed, const CVehicleSeatAnimInfo& rClipInfo)
{
	return !NetworkInterface::IsGameInProgress() && !rPed.IsMale() && rClipInfo.GetFemaleClipSet() != CLIP_SET_ID_INVALID;
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CTaskMotionInAutomobile::GetInVehicleClipsetId(const CPed& rPed, const CVehicle* pVehicle, s32 iSeatIndex)
{
	if (!IsTaskValid(pVehicle, iSeatIndex))
		return CLIP_SET_ID_INVALID;

	const CVehicleSeatAnimInfo* pClipInfo = pVehicle->GetSeatAnimationInfo(iSeatIndex);
	taskFatalAssertf(pClipInfo, "NULL Seat clip Info for seat index %i", iSeatIndex);

	if (ShouldUseFemaleClipSet(rPed, *pClipInfo))
	{
		return pClipInfo->GetFemaleClipSet();
	}

	return pClipInfo->GetSeatClipSetId();
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::ShouldUseBikeInVehicleAnims(const CVehicle& vehicle)
{
	//! Note: Might want to rename this at some point. The jetski uses the bike anims as we can re-use seat displacement code, which is quit similar.
	if (vehicle.InheritsFromBike() || vehicle.InheritsFromQuadBike() || vehicle.InheritsFromBoat() || vehicle.InheritsFromAmphibiousQuadBike())
	{
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::WillPutOnHelmet(const CVehicle& vehicle, const CPed& ped)
{
	if (NetworkInterface::IsGameInProgress() && ped.GetIsKeepingCurrentHelmetInVehicle())
		return false;

	if (ped.GetPedConfigFlag(CPED_CONFIG_FLAG_IsPerformingVehicleMelee))
		return false;

	// B*2014266: Interaction menu option to block putting on helmets in bikes/aircraft
	if (ped.GetIsInVehicle() && ped.GetPedConfigFlag(CPED_CONFIG_FLAG_DisableAutoEquipHelmetsInBikes) && (vehicle.InheritsFromBike() || vehicle.InheritsFromQuadBike() || vehicle.InheritsFromAmphibiousQuadBike()))
	{
		return false;
	}

	if (ped.GetIsInVehicle() && ped.GetPedConfigFlag(CPED_CONFIG_FLAG_DisableAutoEquipHelmetsInAircraft) && !ped.GetPedResetFlag(CPED_RESET_FLAG_ForceAutoEquipHelmetsInAicraft) && vehicle.GetIsAircraft())
	{
		return false;
	}

	// Only on motor bikes and quad bikes, perhaps bicycles?
	//	Helicopters now too
	if (!vehicle.InheritsFromBike() && !vehicle.InheritsFromQuadBike() && !vehicle.InheritsFromAmphibiousQuadBike() && 
		vehicle.GetVehicleType() != VEHICLE_TYPE_HELI && vehicle.GetVehicleType() != VEHICLE_TYPE_PLANE)
		return false;

	if((vehicle.InheritsFromQuadBike() || vehicle.InheritsFromAmphibiousQuadBike()) && ped.GetPedModelInfo()->GetModelNameHash() == MI_PLAYERPED_PLAYER_TWO.GetName().GetHash())
		return false;

	if (!ped.CanPutOnHelmet())
		return false;

	if(!ped.IsAPlayerPed() && !ped.PopTypeIsMission())
		return false;

	// No helmet nothing to put on!
	if (!ped.GetHelmetComponent())
		return false;

	// Already attached to peds head or we have no helmet prop
	if (ped.GetHelmetComponent()->IsHelmetEnabled() || !ped.GetHelmetComponent()->HasHelmetForVehicle(&vehicle))
		return false;

	//Don't add a helmet if we have a prop on our head already.
	if ( CPedPropsMgr::GetPedPropIdx(&ped, ANCHOR_HEAD) != -1 && !ped.IsPlayer())
		return false;

	if(ped.GetHelmetComponent()->IsAlreadyWearingAHelmetProp(&vehicle))
		return false;

	//Heli/planes snap the helmet so the player input doesn't effect them
	if (ped.IsPlayer() && vehicle.GetVehicleType() != VEHICLE_TYPE_HELI && vehicle.GetVehicleType() != VEHICLE_TYPE_PLANE)
	{
		const CControl* pControl = ped.GetControlFromPlayer();
		if (pControl && 
			(pControl->GetPedEnter().IsPressed() || 
			pControl->GetVehicleAccelerate().IsDown(ioValue::ANALOG_BUTTON_DOWN_THRESHOLD) ||
			pControl->GetVehicleBrake().IsDown(ioValue::ANALOG_BUTTON_DOWN_THRESHOLD) ||
			pControl->GetVehicleSelectNextWeapon().IsPressed() || 
			pControl->GetVehicleSelectPrevWeapon().IsPressed() ||
			CNewHud::IsWeaponWheelActive()))
		{
			return false;
		}
	}

	if (ped.GetPedConfigFlag(CPED_CONFIG_FLAG_IsAimingGun) || ped.GetPedResetFlag(CPED_RESET_FLAG_IsAiming))
	{
		return false;
	}

	if(vehicle.GetVehicleType() == VEHICLE_TYPE_PLANE && vehicle.GetSeatManager()->GetPedsSeatIndex(&ped) > 2)
	{
		return false;
	}

	if(ped.IsNetworkClone())
	{
		CNetObjPed* netObjPed = ((CNetObjPed*)ped.GetNetworkObject());
		if( !netObjPed || !netObjPed->IsOwnerAttachingAHelmet() || ped.GetHelmetComponent()->GetNetworkTextureIndex() == -1 )
		{
			// owner isn't putting a helmet on or doesn't know which helmet to put on yet
			return false;
		}
	}

	if (NetworkInterface::IsGameInProgress())
	{
		// In MP we might wear a mask on the BERD slot...
		const CPedVariationData& pedVarData = ped.GetPedDrawHandler().GetVarData();
		u32 compIdx = pedVarData.GetPedCompIdx(PV_COMP_BERD);
		u32 texIdx = pedVarData.GetPedTexIdx(PV_COMP_BERD);
		if (compIdx != 0 || texIdx != 0)
			return false;

	}

	//if (ped.GetPedConfigFlag(CPED_CONFIG_FLAG_HasHelmet))
	//	return false;

	return true;
}

bool CTaskMotionInAutomobile::WillAbortPuttingOnHelmet(const CVehicle& vehicle, const CPed& ped)
{
	//Heli/planes snap the helmet so the player input doesn't effect them
	if(vehicle.GetVehicleType() != VEHICLE_TYPE_HELI && vehicle.GetVehicleType() != VEHICLE_TYPE_PLANE)
	{
		if(ped.IsPlayer())
		{
			if(!ped.IsNetworkClone())
			{
				const CControl* pControl = ped.GetControlFromPlayer();
				if ( pControl && (pControl->GetPedEnter().IsPressed() || pControl->GetVehicleAccelerate().IsDown(ioValue::ANALOG_BUTTON_DOWN_THRESHOLD) ||
					pControl->GetVehicleBrake().IsDown(ioValue::ANALOG_BUTTON_DOWN_THRESHOLD)) )
					return true;
			}
			else
			{
				CNetObjPed* netObj = static_cast<CNetObjPed*>(ped.GetNetworkObject());
				if(netObj)
				{
					if(!netObj->IsOwnerAttachingAHelmet() && !netObj->IsOwnerWearingAHelmet())
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}

bool CTaskMotionInAutomobile::AreFeetOffGround(float fTimeStep, const CVehicle& vehicle, const CPed& ped)
{
	if(!vehicle.InheritsFromBike())
	{
		return false;
	}

	if(ped.GetMyVehicle() != &vehicle)
	{
		return false;
	}

	float fUnused = -1.0f;;
	if(CTaskMotionInVehicle::CheckForStill(fTimeStep, vehicle, ped, 0.5f/*flat*/, fUnused) || CTaskMotionInVehicle::CheckForReverse(vehicle, ped))
	{
		return false;
	}
	
	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::IsVehicleDrivable(const CVehicle& rVeh)
{
	const bool bEngineCanBeStarted = (rVeh.GetVehicleDamage()->GetEngineHealth() > 0.0f) && (rVeh.GetVehicleDamage()->GetPetrolTankLevel() > 0.0f || rVeh.HasInfiniteFuel() );
	return !rVeh.m_nVehicleFlags.bNotDriveable && bEngineCanBeStarted;
}

////////////////////////////////////////////////////////////////////////////////



CTaskMotionInAutomobile::CTaskMotionInAutomobile(CVehicle* pVehicle, CMoveNetworkHelper& moveNetworkHelper) 
: m_pVehicle(pVehicle)
, m_moveNetworkHelper(moveNetworkHelper)
, m_vLastFramesVehicleVelocity(V_ZERO)
, m_vCurrentVehicleAcceleration(V_ZERO)
, m_vCurrentVehicleAccelerationNorm(V_HALF)
, m_bLastFramesVelocityValid(false)
, m_vPreviousFrameAcceleration(0.5f, 0.5f, 0.5f)
, m_fBodyLeanX(0.5f)
, m_fBodyLeanApproachRateX(-1.0f)
, m_fBodyLeanY(0.5f)
, m_fBodyLeanApproachRateY(-1.0f)
, m_fSpeed(0.0f)
, m_fSteering(0.5f)
, m_uTaskStartTimeMS(0)
, m_uLastSteeringCentredTime(0)
, m_uLastSteeringNonCentredTime(0)
, m_uLastTimeLargeZAcceleration(0)
, m_uStateSitIdle_OnEnterTimeMS(0)
, m_uLastDamageTakenTime(0)
, m_fPitchAngle(0.5f)
, m_fRemoteDesiredPitchAngle(0.5f)
, m_fEngineStartPhase(-1.0f)
, m_fStillTransitionRate(1.0f)
, m_bCloseDoorSucceeded(false)
, m_vThrownForce(Vector3::ZeroType)
, m_fIdleTimer(0.0f)
, m_pInVehicleOverrideInfo(NULL)
, m_pSeatOverrideAnimInfo(NULL)
, m_pBikeAnimParams(NULL)
, m_bCurrentAnimFinished(false)
, m_bTurnedOnDoorHandleIk(false)
, m_bTurnedOffDoorHandleIk(false)
, m_bDefaultClipSetChangedThisFrame(false)
, m_bUseLeanSteerAnims(false)
, m_bDontUsePassengerLeanAnims(false)
, m_bUseIdleSteerAnims(false)
, m_bHasFinishedBlendToSeatPosition(true)
, m_bHasFinishedBlendToSeatOrientation(true)
, m_bWantsToMove(false)
, m_bDontUseArmIk(false)
, m_fHoldLegTime(0.0f)
, m_fBurnOutBlend(0.0f)
, m_fTimeSinceBurnOut(0.0f)
, m_bStoppedWantingToMove(false)
, m_fBurnOutPhase(0.0f)
, m_fMinHeightInAir(0.0f)
, m_fMaxHeightInAir(0.0f)
, m_fTimeInAir(0.0f)
, m_fDuckTimer(-1.0f)
, m_fTimeDucked(0.0f)
, m_fDisplacementScale(ms_Tunables.m_MinDisplacementScale)
, m_FirstPersonSteeringWheelCurrentAngleApproachSpeed(ms_Tunables.m_FirstPersonMinSteeringRate)
, m_fDisplacementDueToPitch(0.0f)
, m_nDefaultClipSet(DCS_Base)
, m_bWasJustInAir(false)
, m_bHasTriedToStartUndriveableVehicle(false)
, m_fFrontWheelOffGroundScale(0.0f)
, m_bSetUpRagdollOnCollision(false)
, m_fApproachRateVariance(0.0f)
, m_uLastDriveByTime(0)
, m_bUseFirstPersonAnims(false)
, m_bWasInFirstPersonVehicleCamera(false)
, m_fSteeringWheelWeight(0.0f)
, m_bIsDucking(false)
, m_bWasDucking(false)
, m_bUseHelmetPutOnUpperBodyOnlyFPS(false)
, m_bWaitingForVisorPropToStreamIn(false)
, m_bPlayingVisorSwitchAnim(false)
, m_bWasPlayingVisorSwitchAnimLastUpdate(false)
, m_bPlayingVisorUpAnim(false)
, m_bUsingVisorArmIK(false)
, m_fSteeringWheelAngle(0.0f)
, m_fSteeringWheelAnglePrev(0.0f)
, m_fTimeTillNextAdditiveIdle(0.0f)
, m_bPlayingFirstPersonAdditiveIdle(false)
, m_bPlayingFirstPersonShunt(false)
, m_bPlayingFirstPersonRoadRage(false)
, m_bIsAltRidingToStillTransition(false)
, m_bForceFinishTransition(false)
, m_TimeControlLastDisabled(0)
, m_iLastTimeWheelWasInSlowZone(0)
, m_iLastTimeWheelWasInFastZone(0)
, m_fSteeringAngleDelta(-1.0f)
, m_fLowriderLeanX(0.5f)
, m_fLowriderLeanY(0.5f)
, m_fLowriderLeanXVelocity(0.0f)
, m_fLowriderLeanYVelocity(0.0f)
, m_fPreviousLowriderVelocityLateral(0.0f)
, m_fPreviousLowriderVelocityLongitudinal(0.0f)
, m_vPreviousLowriderVelocity(Vector3::ZeroType)
, m_mPreviousLowriderMat(V_IDENTITY)
, m_fTimeToTriggerAltLowriderClipset(-1.0f)
, m_fAltLowriderClipsetTimer(0.0f)
, m_iTimeLastModifiedHydraulics(0)
, m_bPlayingLowriderClipsetTransition(false)
, m_bInitializedLowriderSprings(false)
, m_fLateralLeanSpringK(0.0f)
, m_fLateralLeanMinDampingK(0.0f)
, m_fLateralLeanMaxExtraDampingK(0.0f)
, m_fLongitudinalLeanSpringK(0.0f)
, m_fLongitudinalLeanMinDampingK(0.0f)
, m_fLongitudinalLeanMaxExtraDampingK(0.0f)
, m_bDrivingAggressively(false)
, m_iTimeLastAcceleratedAggressively(0)
, m_iTimeStartedAcceleratingAggressively(0)
, m_iTimeStoppedDrivingAtAggressiveSpeed(0)
, m_fJetskiStandPoseBlendTime(0.0f)
, m_MinTimeInAltLowriderClipsetTimer(0.0f)
#if FPS_MODE_SUPPORTED
, m_pFirstPersonIkHelper(NULL)
#endif // FPS_MODE_SUPPORTED
{
	SetInternalTaskType(CTaskTypes::TASK_MOTION_IN_AUTOMOBILE);
}

////////////////////////////////////////////////////////////////////////////////

CTaskMotionInAutomobile::~CTaskMotionInAutomobile()
{
	if (m_pBikeAnimParams)
	{
		delete m_pBikeAnimParams;
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInAutomobile::CleanUp()
{	
	CPed& rPed = *GetPed();
	rPed.SetPedConfigFlag(CPED_CONFIG_FLAG_IsDuckingInVehicle, false);
	rPed.SetPedConfigFlag(CPED_CONFIG_FLAG_UsingLowriderLeans, false);
	rPed.SetPedConfigFlag(CPED_CONFIG_FLAG_UsingAlternateLowriderLeans, false);
	if (m_bSetUpRagdollOnCollision)
	{
		ResetRagdollOnCollision(rPed);
	}

#if FPS_MODE_SUPPORTED
	if (m_pFirstPersonIkHelper)
	{
		delete m_pFirstPersonIkHelper;
		m_pFirstPersonIkHelper = NULL;
	}
#endif // FPS_MODE_SUPPORTED

	rPed.SetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingHelmetVisor,false);
	rPed.SetPedConfigFlag(CPED_CONFIG_FLAG_ForceHelmetVisorSwitch,false);
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::ProcessPreFSM()
{
    // clone peds may not have cloned their vehicle on this machine when
	// the task starts, in this case the task will wait in the start state
	// until this has occurred. The peds vehicle can be removed from this machine
	// before the ped too, and we need to quit this task (without asserting) in this case
    CPed* pPed = GetPed();

	if (pPed->IsLocalPlayer() && pPed->GetPlayerInfo() && pPed->GetPlayerInfo()->AreControlsDisabled())
	{
		m_TimeControlLastDisabled = fwTimer::GetTimeInMilliseconds();
	}

	if (pPed->IsNetworkClone() && !m_pVehicle)
	{
		if (GetState()==State_Start)
		{
			return FSM_Continue;
		}
		else
		{
			return FSM_Quit;
		}
	}

	if (!taskVerifyf(m_pVehicle, "Vehicle was NULL in CTaskMotionInAutomobile::ProcessPreFSM()") || (m_pVehicle != pPed->GetMyVehicle()))
	{
		return FSM_Quit;
	}

	ProcessInVehicleOverride();

	//Process the default clip set.
	ProcessDefaultClipSet();

	//Process the facial.
	ProcessFacial();

	ProcessAutoCloseDoor();

	// Process if the ped needs to be lerped back into the seat
	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedsInVehiclePositionNeedsReset))
	{
		ProcessPositionReset(*pPed);
	}

	// Need this every frame
	RequestProcessMoveSignalCalls();

	ProcessRagdollDueToVehicleOrientation();

	ProcessHelmetVisorSwitching();

	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_IsDoingDriveby))
	{
		m_uLastDriveByTime = fwTimer::GetTimeInMilliseconds();
	}

	//! if local ped is in this vehicle, then we may want to play 1st person anims for this character.
	const CPed* pCamFollowPed = camInterface::FindFollowPed();
	if(pCamFollowPed && pCamFollowPed->GetVehiclePedInside() == m_pVehicle)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostCamera, true);

		if(m_moveNetworkHelper.IsNetworkAttached())
		{
			const bool bFirstPersonVehicleCamera = pPed->IsInFirstPersonVehicleCamera();
			const bool bFirstPersonCamera = pPed->IsFirstPersonShooterModeEnabledForPlayer(false);
			m_moveNetworkHelper.SetFlag(bFirstPersonVehicleCamera, ms_FirstPersonCameraFlagId);
			m_moveNetworkHelper.SetFlag(bFirstPersonVehicleCamera || bFirstPersonCamera, ms_FirstPersonModeFlagId);
			m_moveNetworkHelper.SetFlag(CanUsePoVAnims(), ms_UsePOVAnimsFlagId);
		}
	}

	//! Blend steering wheel weight depending on state. This drives the actual physical steering wheel (so we don't allow any turning
	//! if hotwiring etc).
	if(FPS_MODE_SUPPORTED_ONLY(GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false) ||) IsUsingFirstPersonSteeringAnims())
	{
		switch(GetState())
		{
		case(State_Hotwiring):
		case(State_StartEngine):
		case(State_PutOnHelmet):
			rage::Approach(m_fSteeringWheelWeight, 0.0f, ms_Tunables.m_SteeringWheelBlendOutApproachSpeed, fwTimer::GetTimeStep());
			break;
		}
	}
	else
	{
		if(GetState() > State_StreamAssets)
		{
			m_fSteeringWheelWeight = 1.0f;
		}
	}

	if(IsUsingFirstPersonAnims() || (pCamFollowPed && pCamFollowPed->IsInFirstPersonVehicleCamera()))
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_BlockAnimatedWeaponReactions, true);

		if (pPed->GetIsInVehicle() && pPed->GetVehiclePedInside()->InheritsFromBike())
			pPed->SetPedResetFlag(CPED_RESET_FLAG_BlockIkWeaponReactions, true);
	}

	// B*2014306: Camera/Pad shake when landing from a fall in cars/bikes
	if (pPed->IsInFirstPersonVehicleCamera() && pPed->IsLocalPlayer() && (GetState() == State_FirstPersonSitIdle || GetState() == State_SitIdle) && m_pVehicle && pPed)
	{
		if (CheckForShuntFromHighFall(*m_pVehicle, *pPed, true))
		{
			const float fJumpHeight = m_fMaxHeightInAir - m_fMinHeightInAir;
			StartFirstPersonCameraShake(fJumpHeight);
		}
	}

	UpdateHeadIK();

	if(CanBlockCameraViewModeTransitions())
	{
		pPed->SetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_CAMERA_VIEW_MODE_CYCLE);
	}

	// This is more noticeable in first person, pause the cellphone task
	if (GetState() == State_Hotwiring || GetState() == State_StartEngine)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableCellphoneAnimations, true);
	}

	// Lowrider: Set/Increment time player last modified hydraulics. 
	// Used in CTaskMotionInAutomobile::ShouldUseLowriderLean to determine if we should use panic/agitated anims or stick with lowrider anims.
	if (m_pVehicle->InheritsFromAutomobile())
	{
		const CAutomobile* pAutomobile = static_cast<const CAutomobile*>(m_pVehicle.Get());
		if(pAutomobile->HasHydraulicSuspension())
		{
			if (m_pVehicle->m_nVehicleFlags.bPlayerModifiedHydraulics)
			{
				m_iTimeLastModifiedHydraulics = fwTimer::GetTimeInMilliseconds();
			}			
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();	

		FSM_State(State_StreamAssets)
			FSM_OnEnter
				return StreamAssets_OnEnter();
			FSM_OnUpdate
				return StreamAssets_OnUpdate();

		FSM_State(State_Still)
			FSM_OnEnter
				return Still_OnEnter();
			FSM_OnUpdate
				return Still_OnUpdate();
			FSM_OnExit
				return Still_OnExit();

		FSM_State(State_StillToSit)
			FSM_OnEnter
				return StillToSit_OnEnter();
			FSM_OnUpdate
				return StillToSit_OnUpdate();

		FSM_State(State_PutOnHelmet)
			FSM_OnEnter
				return PutOnHelmet_OnEnter();
			FSM_OnUpdate
				return PutOnHelmet_OnUpdate();
			FSM_OnExit
				return PutOnHelmet_OnExit();

		FSM_State(State_SitIdle)
			FSM_OnEnter
				return SitIdle_OnEnter();
			FSM_OnUpdate
				return SitIdle_OnUpdate();

		FSM_State(State_SitToStill)
			FSM_OnEnter
				return SitToStill_OnEnter();
			FSM_OnUpdate
				return SitToStill_OnUpdate();
			FSM_OnExit
				return SitToStill_OnExit();

		FSM_State(State_Reverse)
			FSM_OnEnter
				return Reverse_OnEnter();
			FSM_OnUpdate
				return Reverse_OnUpdate();
			
        FSM_State(State_ReserveDoorForClose)
			FSM_OnUpdate
				return ReserveDoorForClose_OnUpdate();

		FSM_State(State_CloseDoor)
			FSM_OnEnter
				return CloseDoor_OnEnter();
			FSM_OnUpdate
				return CloseDoor_OnUpdate();
			FSM_OnExit
				return CloseDoor_OnExit();

		FSM_State(State_StartEngine)
			FSM_OnEnter
				return StartEngine_OnEnter();
			FSM_OnUpdate
				return StartEngine_OnUpdate();

		FSM_State(State_Shunt)
			FSM_OnEnter
				return Shunt_OnEnter();
			FSM_OnUpdate
				return Shunt_OnUpdate();

		FSM_State(State_ChangeStation)
			FSM_OnEnter
				return ChangeStation_OnEnter();
			FSM_OnUpdate
				return ChangeStation_OnUpdate();

		FSM_State(State_Hotwiring)
			FSM_OnEnter
				return Hotwiring_OnEnter();
			FSM_OnUpdate
				return Hotwiring_OnUpdate();

		FSM_State(State_HeavyBrake)
			FSM_OnEnter
				return HeavyBrake_OnEnter();
			FSM_OnUpdate
				return HeavyBrake_OnUpdate();

		FSM_State(State_Horn)
			FSM_OnEnter
				return Horn_OnEnter();
			FSM_OnUpdate
				return Horn_OnUpdate();

		FSM_State(State_FirstPersonSitIdle)
				FSM_OnEnter
				return FirstPersonSitIdle_OnEnter();
			FSM_OnUpdate
				return FirstPersonSitIdle_OnUpdate();

		FSM_State(State_FirstPersonReverse)
				FSM_OnEnter
				return FirstPersonReverse_OnEnter();
			FSM_OnUpdate
				return FirstPersonReverse_OnUpdate();
		
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
			
	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::ProcessPostFSM()
{
	//Clear the flag noting that the default clip set changed this frame.
	m_bDefaultClipSetChangedThisFrame = false;

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::ProcessPhysics(float fTimeStep, int UNUSED_PARAM(nTimeSlice))
{
	if (!m_pVehicle)
	{
		return false;
	}

	CPed& rPed = *GetPed();

	TUNE_GROUP_BOOL(THROUGH_WINDSCREEN, PROCESS_PHYSICS, false);
	if (PROCESS_PHYSICS && !rPed.IsNetworkClone() && m_bLastFramesVelocityValid)
	{
		ProcessImpactReactions(rPed, *m_pVehicle);
	}

	// Lerp peds in vehicles back to their seat position/orientation
	if (!m_bHasFinishedBlendToSeatPosition || !m_bHasFinishedBlendToSeatOrientation)
	{
		const CVehicle* pVeh = rPed.GetMyVehicle();
		if (!pVeh || !rPed.GetIsAttached())
		{
			m_bHasFinishedBlendToSeatPosition = true;
			m_bHasFinishedBlendToSeatOrientation = true;
			return false;
		}

		const s32 iSeatIndex = pVeh->GetSeatManager()->GetPedsSeatIndex(&rPed);
		if (!pVeh->IsSeatIndexValid(iSeatIndex) || rPed.GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle))
		{
			m_bHasFinishedBlendToSeatPosition = true;
			m_bHasFinishedBlendToSeatOrientation = true;
			return false;
		}

		taskAssert(pVeh->GetVehicleModelInfo());
		const CModelSeatInfo* pModelSeatInfo = pVeh->GetVehicleModelInfo()->GetModelSeatInfo();
		taskAssert(pModelSeatInfo);
		const s32 iSeatBoneIndex = pModelSeatInfo->GetBoneIndexFromSeat(iSeatIndex);
		Matrix34 seatMtx;
		pVeh->GetGlobalMtx(iSeatBoneIndex, seatMtx);

		Vec3V vGlobalPedPosition(V_ZERO);
		QuatV qUnused;
		Vec3V vPedAttachOffset = VECTOR3_TO_VEC3V(rPed.GetAttachOffset());
		CTaskVehicleFSM::ComputeGlobalTransformFromLocal(rPed, vPedAttachOffset, qUnused, vGlobalPedPosition, qUnused);

		Vector3 vPedPosition = VEC3V_TO_VECTOR3(vGlobalPedPosition);
		const Vector3 vTargetSeatPosition = seatMtx.d;
		Quaternion qTargetSeatRotation;
		qTargetSeatRotation.FromMatrix34(seatMtx);

		m_bHasFinishedBlendToSeatPosition = vPedPosition.ApproachStraight(vTargetSeatPosition, ms_Tunables.m_SeatBlendLinSpeed, fTimeStep);
		const float fLerpRatio = rage::Clamp(GetTimeRunning() * ms_Tunables.m_SeatBlendAngSpeed, 0.0f, 1.0f);
		Quaternion qPedOrientation;
		qPedOrientation.Lerp(fLerpRatio, m_qInitialPedRotation, qTargetSeatRotation);
		qPedOrientation.Normalize();

		if (fLerpRatio == 1.0f)
		{
			m_bHasFinishedBlendToSeatOrientation = true;
		}

		if (m_bHasFinishedBlendToSeatOrientation && m_bHasFinishedBlendToSeatPosition)
		{
			rPed.SetPedConfigFlag(CPED_CONFIG_FLAG_PedsFullyInSeat, true);
		}

		CTaskVehicleFSM::UpdatePedVehicleAttachment(&rPed, VECTOR3_TO_VEC3V(vPedPosition), QUATERNION_TO_QUATV(qPedOrientation));
	}
	else
	{
		rPed.SetPedConfigFlag(CPED_CONFIG_FLAG_PedsFullyInSeat, true);
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::ProcessPostMovement()
{
	CPed& rPed = *GetPed();

	// Bail on NULL vehicle
	if(!m_pVehicle)
	{
		return false;
	}

	TUNE_GROUP_BOOL(THROUGH_WINDSCREEN, PROCESS_POST_MOVEMENT, true);
	if (PROCESS_POST_MOVEMENT && !rPed.IsNetworkClone() && m_bLastFramesVelocityValid)
	{
		bool bGoingThroughWindscreen = ProcessImpactReactions(rPed, *m_pVehicle);

		// If we've been impacted heavily apply damage to the ped
		ProcessDamageFromImpacts(rPed, *m_pVehicle, bGoingThroughWindscreen);
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::ProcessMoveSignals()
{
	// Local handle to ped
	CPed* pPed = GetPed();

	// Bail on NULL ped or vehicle
	// NOTE: may be NULL if task is ending
	if(!pPed || !m_pVehicle)
	{
		return false;
	}

	// Check if the ped should sway in this vehicle, if so raise the
	// sm_PedShouldSwayInVehicleId flag
	m_moveNetworkHelper.SetFlag(CheckIfPedShouldSwayInVehicle(m_pVehicle), CTaskMotionInVehicle::sm_PedShouldSwayInVehicleId); 

	// Lowrider: flag if arm transition clip has finished.
	if (m_bPlayingLowriderClipsetTransition)
	{
		bool bArmTransitionFinished = m_moveNetworkHelper.GetBoolean(CTaskMotionInVehicle::ms_LowriderArmTransitionFinished);
		if (bArmTransitionFinished)
		{
			m_bPlayingLowriderClipsetTransition = false;
		}
	}

	///////////////////////////////
	// Formerly in ProcessPreFSM:
	///////////////////////////////

	// this flag is reset every frame
	pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasksMotion, true);
	pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPostMovement, true);

	// See if we impacted something during the last physics update that would cause us to go through the windscreen/play impact reaction anim
	// we force an anim update in this case to see that anim blend in this frame
	TUNE_GROUP_BOOL(THROUGH_WINDSCREEN, PROCESS_PRE_FSM, false);
	bool bGoingThroughWindscreen = false;
	if (PROCESS_PRE_FSM)
	{
		bGoingThroughWindscreen = ProcessImpactReactions(*pPed, *m_pVehicle);

		// If we've been impacted heavily apply damage to the ped
		ProcessDamageFromImpacts(*pPed, *m_pVehicle, bGoingThroughWindscreen);
	}

	// Use time since last frame, not aiTask::GetTimeStep() since last timesliced update
	const float fTimeStep = fwTimer::GetTimeStep();

	// Compute a normalized acceleration parameter from the last and current velocities, update the last velocity
	ComputeAccelerationParameter(*pPed, *m_pVehicle, fTimeStep);

	// Need to ik females hands to the steering wheel as they use the male anims, but their skeletons are smaller
	if (m_pVehicle && GetState() != State_CloseDoor)
	{
		CTaskMotionInVehicle::ProcessSteeringWheelArmIk(*m_pVehicle, *pPed);
	}

	///////////////////////////////
	// Formerly in ProcessPostFSM:
	///////////////////////////////

	ProcessInAirHeight(*pPed, *m_pVehicle, fTimeStep);

	const CVehicleSeatAnimInfo* pSeatClipInfo = m_pVehicle->GetSeatAnimationInfo(pPed);
	const bool bIsMoveNetworkActive = m_moveNetworkHelper.IsNetworkActive();
	if( pSeatClipInfo && bIsMoveNetworkActive )
	{
		ComputeSteeringParameter(*pSeatClipInfo, *m_pVehicle, fTimeStep);
		ComputePitchParameter(fTimeStep);

		if((m_pVehicle->InheritsFromBike() || m_pVehicle->InheritsFromQuadBike() || m_pVehicle->InheritsFromAmphibiousQuadBike()) && m_fTimeInAir > ms_Tunables.m_BikeInAirDriveToStandUpTimeMin)
		{
			TUNE_GROUP_FLOAT(VEHICLE_MOTION, FORCED_SPEED_RATE, 0.25f, 0.0f, 10.0f, 0.01f);
			Approach(m_fSpeed, 1.0f, FORCED_SPEED_RATE, fTimeStep);
		}
		else
		{
			TUNE_GROUP_FLOAT(VEHICLE_MOTION, SPEED_SMOOTHING, 0.0375f, 0.0f, 1.0f, 0.001f);
			CTaskMotionInVehicle::ComputeSpeedSignal(*pPed, *m_pVehicle, m_fSpeed, SPEED_SMOOTHING, m_fPitchAngle);
		}

		m_moveNetworkHelper.SetFloat(CTaskMotionInVehicle::ms_SteeringId, m_fSteering);
		float fSpeedSignal = CTaskMotionInAutomobile::IsFirstPersonDrivingJetski(*pPed, *m_pVehicle) ? 0.0f : m_fSpeed;
		
		if(m_pVehicle->GetIsJetSki() && m_fJetskiStandPoseBlendTime > 0.0f)
		{
			fSpeedSignal = Max(0.1f, fSpeedSignal);
		}

		m_moveNetworkHelper.SetFloat(CTaskMotionInVehicle::ms_SpeedId, fSpeedSignal);

		const bool bShouldUseBikeInVehicleAnims = ShouldUseBikeInVehicleAnims(*m_pVehicle);
		if( m_pBikeAnimParams && bShouldUseBikeInVehicleAnims )
		{
			bool bShouldComputeBikeParams = GetState() != State_StillToSit && GetState() != State_SitToStill;
			if( GetPreviousState() == State_StillToSit && GetState() == State_SitIdle )
			{
				TUNE_GROUP_INT(VEHICLE_TUNE, HOLD_CENTRAL_BLEND_WEIGHT_TIME_MS, 250, 0, 5000, 10);
				if(fwTimer::GetTimeInMilliseconds() < m_uStateSitIdle_OnEnterTimeMS + HOLD_CENTRAL_BLEND_WEIGHT_TIME_MS)
				{
					bShouldComputeBikeParams = false;
				}
			}

			if( bShouldComputeBikeParams && GetState() != State_StreamAssets)
			{
				ComputeBikeParameters(*pPed);
			}
			else
			{
				const float fCenteredAngle = 0.5f;
				m_pBikeAnimParams->bikeLeanAngleHelper.SetLeanAngle(fCenteredAngle);
				m_pBikeAnimParams->bikeLeanAngleHelper.SetDesiredLeanAngle(fCenteredAngle);
				m_moveNetworkHelper.SetFloat(CTaskMotionInVehicle::ms_BodyMovementZId, 0.5f);
			}

			m_moveNetworkHelper.SetFloat(CTaskMotionInVehicle::ms_BodyMovementXId, m_pBikeAnimParams->bikeLeanAngleHelper.GetLeanAngle());
		}
		else // not using on a bike anims
		{
			// B*2305888: If in a lowrider vehicle with hydraulics enabled, calculate lean parameters based on vehicle lateral/longitudinal lean acceleration.
			if (ShouldUseLowriderLean())
			{
				ComputeLowriderParameters();
			}
			else
			{
				m_bInitializedLowriderSprings = false;
			}

			if( m_pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_USE_STEERING_PARAM_FOR_LEAN) && m_pVehicle->GetDriver() == pPed)
			{
				m_moveNetworkHelper.SetFloat(CTaskMotionInVehicle::ms_BodyMovementXId, m_fSteering);
			}
			else
			{
				m_moveNetworkHelper.SetFloat(CTaskMotionInVehicle::ms_BodyMovementXId, m_fBodyLeanX);
			}
		}

		// Determine if we are allowing Lean IK to handle body Y
		bool bDoingBurnout = false;
		if (m_pVehicle->InheritsFromAutomobile() && !m_pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_IS_QUADBIKE_USING_BIKE_ANIMATIONS))
		{
			bDoingBurnout = static_cast<CAutomobile*>(m_pVehicle.Get())->IsInBurnout();
		}
		else
		{
			bDoingBurnout = CheckIfVehicleIsDoingBurnout(*m_pVehicle, *pPed);
		}
		const bool bReversing = GetState() == State_Reverse;
		const bool bDoingDriveby = pPed->GetPedResetFlag(CPED_RESET_FLAG_IsDoingDriveby); // reset pre-task update so readable here OK
		const bool bAllowLeanIK = pSeatClipInfo->GetUseTorsoLeanIK() || (pSeatClipInfo->GetUseRestrictedTorsoLeanIK() && (bDoingBurnout || bDoingDriveby || bReversing));
		const float fCenteredAngle = 0.5f;
		// Default to a neutral centered angle to let IK drive any leaning
		float fNewYValue = fCenteredAngle;
		// If we are not using IK, use member parameters to drive animation leaning
		if( !bAllowLeanIK || bDoingDriveby )
		{
			if( bShouldUseBikeInVehicleAnims )
			{
				fNewYValue = m_fPitchAngle;	
			}
			else
			{
				fNewYValue = m_fBodyLeanY;
			}
		}
		// apply desired animation lean Y
		m_moveNetworkHelper.SetFloat(CTaskMotionInVehicle::ms_BodyMovementYId, fNewYValue);
	}

	///////////////////////////////

	// Done, do no more processing in this method
	if (m_bCurrentAnimFinished)
	{
		// we always need processing
		return true;
	}

	bool bValidState = false;
	fwMvBooleanId clipFinishedId;
	fwMvFloatId clipPhaseId;

	const int state = GetState();
	if(state == State_Hotwiring)
	{
		bValidState = true;
		clipFinishedId = ms_HotwiringClipFinishedId;
		clipPhaseId = ms_HotwiringPhaseId;
	}
	else if(state == State_CloseDoor)
	{
		bValidState = true;
		clipFinishedId = ms_CloseDoorClipFinishedId;
		clipPhaseId = ms_CloseDoorPhaseId;
	}
	else if(state == State_PutOnHelmet)
	{
		bValidState = true;
		clipFinishedId = ms_PutOnHelmetClipFinishedId;
		clipPhaseId = ms_PutOnHelmetPhaseId;
		if (pPed->IsInFirstPersonVehicleCamera())
		{
			m_moveNetworkHelper.SetFlag(m_bUseHelmetPutOnUpperBodyOnlyFPS, ms_FPSHelmetPutOnUseUpperBodyOnly);
		}
	}
	else if(state == State_HeavyBrake)
	{
		bValidState = true;
		clipFinishedId = ms_HeavyBrakeClipFinishedId;
		clipPhaseId = ms_HeavyBrakePhaseId;
	}
	else if(state == State_ChangeStation)
	{
		bValidState = true;
		clipFinishedId = ms_ChangeStationClipFinishedId;
		clipPhaseId = ms_ChangeStationPhaseId;
	}
	else if(state == State_Shunt)
	{
		bValidState = true;
		clipFinishedId = ms_ShuntAnimFinishedId;
		clipPhaseId = ms_ShuntPhaseId;
	}
	else if(state == State_StartEngine)
	{
		bValidState = true;
		clipFinishedId = ms_EngineStartClipFinishedId;
		clipPhaseId = ms_StartEnginePhaseId;
	}
	else if(state == State_FirstPersonSitIdle)
	{
		bool bHornIsBeingUsed = CheckForHorn();
		m_moveNetworkHelper.SetFlag(bHornIsBeingUsed, CTaskMotionInVehicle::ms_HornIsBeingUsedId);
	}
	else if ((state == State_Still || state == State_SitIdle) && m_pVehicle && m_pVehicle->InheritsFromBike())
	{
		// Only play the bike horn anims if they exist in the current clipset
		const fwMvClipSetId clipsetId = m_moveNetworkHelper.GetClipSetId();
		if (clipsetId != CLIP_SET_ID_INVALID)
		{
			const crClip* pHornIntroClip = fwClipSetManager::GetClip(clipsetId, ms_Tunables.m_BikeHornIntroClipId);
			const crClip* pHornClip = fwClipSetManager::GetClip(clipsetId, ms_Tunables.m_BikeHornClipId);
			const crClip* pHornOutroClip = fwClipSetManager::GetClip(clipsetId, ms_Tunables.m_BikeHornOutroClipId);
			if (pHornIntroClip && pHornClip && pHornOutroClip)
			{
				bool bHornIsBeingUsed = CheckForHorn(true);
				m_moveNetworkHelper.SetFlag(bHornIsBeingUsed, CTaskMotionInVehicle::ms_BikeHornIsBeingUsedId); 
			}
		}
	}

	if (bValidState)
	{
		// Check if done running the clip for the current state
		if (IsMoveNetworkStateFinished(clipFinishedId, clipPhaseId))
		{	
			m_bCurrentAnimFinished = true;
		}
	}

	// we always need processing
	return true;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::Start_OnUpdate()
{
	CPed* pPed = GetPed();

	m_uTaskStartTimeMS = fwTimer::GetTimeInMilliseconds();

	m_fIdleTimer = 0.0f;	// Clear idle timer when we start

	if (pPed && m_pVehicle && m_pVehicle->IsNetworkClone() && (m_pVehicle->GetDriver() == pPed))
	{
		vehicledoorDebugf2("CTaskMotionInAutomobile::Start_OnUpdate--driver-->SetRemoteDriverDoorClosing(false)");
		m_pVehicle->SetRemoteDriverDoorClosing(false); //reset
	}

    // if this is a network ped it may not have been put in the seat yet, and we must wait
    if(pPed->IsNetworkClone() && (!m_pVehicle || !m_pVehicle->ContainsPed(pPed)))
    {
        return FSM_Continue;
    }

	const s32 iMySeat = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
	if (m_pVehicle->IsSeatIndexValid(iMySeat))
	{
		const CVehicleSeatAnimInfo* pSeatAnimInfo = m_pVehicle->GetSeatAnimationInfo(iMySeat);
		if (pSeatAnimInfo && pSeatAnimInfo->GetKeepCollisionOnWhenInVehicle())
		{
			SetUpRagdollOnCollision(*pPed, *m_pVehicle);
		}
	}

	if (m_pVehicle && ShouldUseBikeInVehicleAnims(*m_pVehicle) && !m_pBikeAnimParams)
	{
		m_pBikeAnimParams = rage_new sBikeAnimParams(m_pVehicle, pPed);
		m_pBikeAnimParams->seatDisplacementSpring.Reset(0.5f);
	}

	// Bicycles don't have engines
	if (m_pVehicle->GetVehicleType() == VEHICLE_TYPE_BICYCLE)
	{
		m_pVehicle->SwitchEngineOn();
	}

	// No hotwiring anims for non cars
	if (m_pVehicle->GetVehicleType() != VEHICLE_TYPE_CAR)
	{
		m_pVehicle->m_nVehicleFlags.bCarNeedsToBeHotwired = false;
	}

	//B*1853645: Randomly override Trevor's helmet type to use the army-style helmet
	if (m_pVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE || m_pVehicle->GetVehicleType() == VEHICLE_TYPE_QUADBIKE || m_pVehicle->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE)
	{
		if (WillPutOnHelmet(*m_pVehicle, *pPed))
		{
			const CPedModelInfo* pPedModelInfo = pPed->GetPedModelInfo();
			if(pPedModelInfo->GetModelNameHash() == MI_PLAYERPED_PLAYER_TWO.GetName().GetHash())
			{
				// There are 8 variants of the army-style helmet, 6 variants of the moto-x helmet, and 1 of the original helmet.
				// 0 = moped, 1-8 = army, 9-14 = moto-x
				int iRandom = fwRandom::GetRandomNumberInRange(0, 14);
				if (iRandom >= 1 && iRandom <= 8)
				{
					pPed->GetHelmetComponent()->SetHelmetPropIndex(ALTERNATIVE_HELMET_PLAYER_TWO_ARMY);
				}
				else if (iRandom > 8)
				{
					pPed->GetHelmetComponent()->SetHelmetPropIndex(ALTERNATIVE_HELMET_PLAYER_TWO_MOTOX);
				}
				else
				{
					pPed->GetHelmetComponent()->SetHelmetPropIndex(-1);
				}
			}
		}
	}

	//	Enable helmet/headset for helicopters and planes
	if (m_pVehicle->GetVehicleType() == VEHICLE_TYPE_HELI || m_pVehicle->GetVehicleType() == VEHICLE_TYPE_PLANE)
	{
		if (WillPutOnHelmet(*m_pVehicle, *pPed))
		{
			// B*2060539: We sometimes override Trevors helmet on bikes so we need to ensure we clear the override flag before equipping an aircraft helmet
			if (pPed->GetHelmetComponent())
			{
				const CPedModelInfo* pPedModelInfo = pPed->GetPedModelInfo();
				const CVehicleModelInfo* pVehicleModelInfo = m_pVehicle->GetVehicleModelInfo();
				s32 iHeadPropIndex = pPed->GetHelmetComponent()->GetHelmetPropIndexOverride();
				if(pVehicleModelInfo && iHeadPropIndex != -1)
				{
					//Reset the override indexes
					if( (pPedModelInfo->GetModelNameHash() == MI_PLAYERPED_PLAYER_TWO.GetName().GetHash() && iHeadPropIndex == ALTERNATIVE_HELMET_PLAYER_TWO_ARMY) ||
						(pPedModelInfo->GetModelNameHash() == MI_PLAYERPED_PLAYER_TWO.GetName().GetHash() && iHeadPropIndex == ALTERNATIVE_HELMET_PLAYER_TWO_MOTOX))
					{
						pPed->GetHelmetComponent()->SetHelmetPropIndex(-1);
					}
				}
			}

			//HACK: A different helmet is wanted for DUSTER and STUNT for single player characters only
			const CPedModelInfo* pPedModelInfo = pPed->GetPedModelInfo();
			const CVehicleModelInfo* pVehicleModelInfo = m_pVehicle->GetVehicleModelInfo();
			if(pVehicleModelInfo && pVehicleModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_PLANE_WEAR_ALTERNATIVE_HELMET))
			{
				//Override the helmet index to use the correct head prop for these two planes
				if(pPedModelInfo->GetModelNameHash() == MI_PLAYERPED_PLAYER_TWO.GetName().GetHash())
				{
					pPed->GetHelmetComponent()->SetHelmetPropIndex(ALTERNATIVE_FLIGHT_HELMET_PLAYER_TWO);
					pPed->GetHelmetComponent()->SetHelmetTexIndex(0);
				}
				else if (pPedModelInfo->GetModelNameHash() == MI_PLAYERPED_PLAYER_ONE.GetName().GetHash())
				{
					pPed->GetHelmetComponent()->SetHelmetPropIndex(ALTERNATIVE_FLIGHT_HELMET_PLAYER_ONE);
					pPed->GetHelmetComponent()->SetHelmetTexIndex(0);
				}
				else if (pPedModelInfo->GetModelNameHash() == MI_PLAYERPED_PLAYER_ZERO.GetName().GetHash())
				{
					pPed->GetHelmetComponent()->SetHelmetPropIndex(ALTERNATIVE_FLIGHT_HELMET_PLAYER_ZERO);
					pPed->GetHelmetComponent()->SetHelmetTexIndex(0);
				}
			}
			
			// Don't automatically enable helmet in FPS mode as we want to play put on anims 
			// (unless we're in multiplayer just pop it on as we have some anim clipping issues on some vehicles).
			bool bInFPSModeWithoutPilotHelmet = !CPedHelmetComponent::ShouldUsePilotFlightHelmet(pPed) && (pPed->IsInFirstPersonVehicleCamera() || pPed->IsFirstPersonShooterModeEnabledForPlayer(false));
			
			// Do not treat bonnet camera as a first person camera here, so we instantly put on helmet (which activates hud)
			bool bInFPSVehicleViewMode = false;
			if (m_pVehicle->GetVehicleType() == VEHICLE_TYPE_HELI)
			{
				const int viewModeHeli = camControlHelper::GetViewModeForContext(camControlHelperMetadataViewMode::IN_HELI);
				bInFPSVehicleViewMode= (viewModeHeli == camControlHelperMetadataViewMode::FIRST_PERSON) && !camCinematicDirector::IsBonnetCameraEnabled();
			}
			else if (m_pVehicle->GetVehicleType() == VEHICLE_TYPE_PLANE)
			{
				const int viewModeAir = camControlHelper::GetViewModeForContext(camControlHelperMetadataViewMode::IN_AIRCRAFT);
				bInFPSVehicleViewMode= (viewModeAir == camControlHelperMetadataViewMode::FIRST_PERSON) && !camCinematicDirector::IsBonnetCameraEnabled();
			}

			if ((!bInFPSVehicleViewMode) || NetworkInterface::IsGameInProgress() || bInFPSModeWithoutPilotHelmet)
			{
				pPed->GetHelmetComponent()->EnableHelmet();
			}
		}
	}

	// B*2216490 - Set helmet index from hash map (if defined by script for this particular vehicle).
	if (pPed->GetHelmetComponent())
	{
		pPed->GetHelmetComponent()->SetHelmetIndexFromHashMap();
	}

	if (m_pVehicle->GetDriver() != pPed)
	{
		TUNE_GROUP_FLOAT(VEHICLE_TUNE, X_PASSENGER_BODY_LEAN_VARIANCE, 1.0f, 0.0f, 10.0f, 0.01f); 
		m_fApproachRateVariance = fwRandom::GetRandomNumberInRange(-X_PASSENGER_BODY_LEAN_VARIANCE, X_PASSENGER_BODY_LEAN_VARIANCE);
	}

	// There shouldn't be any intermediate move transitions between the in vehicle state, then we can assume we 
	// insert correctly the first time
	s32 iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);

	const fwMvClipSetId inVehicleClipsetId = GetInVehicleClipsetId(*pPed, m_pVehicle, iSeatIndex);

	if (!ms_Tunables.m_TestLowLodIdle && CTaskVehicleFSM::RequestVehicleClipSetReturnIsLoaded(inVehicleClipsetId, pPed->IsLocalPlayer() ? SP_High : SP_Invalid) && !CTaskVehicleFSM::ShouldClipSetBeStreamedOut(inVehicleClipsetId))
	{
		const CVehicleSeatAnimInfo* pSeatClipInfo = m_pVehicle->GetSeatAnimationInfo(pPed);
		if (StartMoveNetwork(pSeatClipInfo) && SetSeatClipSet(pSeatClipInfo))
		{
			UseLeanSteerAnims(m_pVehicle->GetLayoutInfo()->GetUseLeanSteerAnims());

			m_bWasInFirstPersonVehicleCamera = pPed->IsInFirstPersonVehicleCamera() FPS_MODE_SUPPORTED_ONLY(|| pPed->IsFirstPersonShooterModeEnabledForPlayer(false));

			UpdateFirstPersonAnimationState();

			float fUnused = -1.0f;
			if (pSeatClipInfo->GetUseBikeInVehicleAnims() && !pPed->ShouldBeDead())
			{
				if (CheckForStartingEngine(*m_pVehicle, *pPed) && pPed->GetPedResetFlag(CPED_RESET_FLAG_InterruptedToQuickStartEngine))
				{
					SetTaskState(State_StartEngine);
					return FSM_Continue;
				}
				//! Never let bike passengers go into passenger side of MoVE network as we don't have required transitions.
				else if ( (m_pVehicle->GetDriver() != pPed) && (m_pVehicle->InheritsFromBike()))
				{
					SetState(State_Still);
					return FSM_Continue;
				}
				else if (CTaskMotionInVehicle::CheckForStill(GetTimeStep(), *m_pVehicle, *pPed, m_fPitchAngle, fUnused))
				{
					SetState(State_Still);
					return FSM_Continue;
				}
			}

			SetState(GetSitIdleState());
			return FSM_Continue;
		}
	}
	else
	{
		SetState(State_StreamAssets);
		return FSM_Continue;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::StreamAssets_OnEnter()
{
	const CPed* pPed = GetPed();
	const CVehicleSeatAnimInfo* pSeatClipInfo = m_pVehicle->GetSeatAnimationInfo(pPed);
	if( pSeatClipInfo && StartMoveNetwork(pSeatClipInfo) )
	{
		UseLeanSteerAnims(m_pVehicle->GetLayoutInfo()->GetUseLeanSteerAnims());

		const crClip* pLowLodClip = fwClipSetManager::GetClip(ms_Tunables.m_LowLodIdleClipSetId, pSeatClipInfo->GetLowLODIdleAnim());
		m_moveNetworkHelper.SetClip(pLowLodClip, CTaskMotionInVehicle::ms_LowLodClipId);
		SetMoveNetworkState(CTaskMotionInVehicle::ms_LowLodRequestId, CTaskMotionInVehicle::ms_LowLodOnEnterId, CTaskMotionInVehicle::ms_InvalidStateId);
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::StreamAssets_OnUpdate()
{
	if (!IsMoveNetworkStateValid())
		return FSM_Continue;


	CPed* pPed = GetPed();
	s32 iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);

	if (m_pVehicle->IsSeatIndexValid(iSeatIndex))
	{
		const fwMvClipSetId inVehicleClipsetId = GetInVehicleClipsetId(*pPed, m_pVehicle, iSeatIndex);

		//Check if we can sit idle.
		bool bCanSitIdle = !ms_Tunables.m_TestLowLodIdle;
		bool bIsStreamingRequired = ((m_nDefaultClipSet != DCS_Panic) && (m_nDefaultClipSet != DCS_Agitated));
		if(bCanSitIdle && bIsStreamingRequired)
		{
			const CVehicleSeatAnimInfo* pSeatClipInfo = m_pVehicle->GetSeatAnimationInfo(pPed);
			bCanSitIdle = CTaskVehicleFSM::RequestVehicleClipSetReturnIsLoaded(inVehicleClipsetId, pPed->IsLocalPlayer() ? SP_High : SP_Invalid) &&
				!CTaskVehicleFSM::ShouldClipSetBeStreamedOut(inVehicleClipsetId) && SetSeatClipSet(pSeatClipInfo);
		}
		if (bCanSitIdle)
		{
			SetState(State_SitIdle);
			return FSM_Continue;
		}
		else
		{
#if __ASSERT
			CTaskVehicleFSM::SpewStreamingDebugToTTY(*pPed, *m_pVehicle, iSeatIndex, inVehicleClipsetId, GetTimeInState());
#endif // __ASSERT

			if (GetTimeInState() > CTaskMotionInVehicle::ms_Tunables.m_MaxTimeStreamInVehicleClipSetBeforeStartingEngine)
			{
				m_pVehicle->SwitchEngineOn();
			}
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::Still_OnEnter()
{
	SetMoveNetworkState(CTaskMotionInVehicle::ms_StillRequestId, CTaskMotionInVehicle::ms_StillOnEnterId, CTaskMotionInVehicle::ms_InvalidStateId);
	m_bCurrentAnimFinished = false;
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::Still_OnUpdate()
{
	CPed* pPed = GetPed();
	pPed->SetPedResetFlag(CPED_RESET_FLAG_RequiresLegIk, true);

	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	if(CanDoInstantSwitchToFirstPersonAnims())
	{
		SetTaskState(State_FirstPersonSitIdle);
		return FSM_Continue;
	}
	
	if (CheckForHelmet(*pPed))
	{
		//Make sure we have the helmet loaded before starting putting it on
		bool bFinishedPreLoading = pPed->GetHelmetComponent()->HasPreLoadedProp();
		if(	!pPed->GetHelmetComponent()->IsPreLoadingProp() && !bFinishedPreLoading)
		{
			//Load the helmet
			pPed->GetHelmetComponent()->SetPropFlag(PV_FLAG_DEFAULT_HELMET);
			pPed->GetHelmetComponent()->RequestPreLoadProp();
			return FSM_Continue;
		}

		if(bFinishedPreLoading)
		{	
			SetTaskState(State_PutOnHelmet);
			return FSM_Continue;
		}
	}

	if (CheckForChangingStation(*m_pVehicle, *pPed))
	{
		SetTaskState(State_ChangeStation);
		return FSM_Continue;
	}

	const float fTimeStep = GetTimeStep();

	if (CTaskMotionInVehicle::CheckForShunt(*m_pVehicle, *pPed, m_vCurrentVehicleAcceleration.GetZf()))
	{
		SetTaskState(State_Shunt);
		return FSM_Continue;
	}

	if (CheckForStartingEngine(*m_pVehicle, *pPed))
	{
		SetTaskState(State_StartEngine);
		return FSM_Continue;
	}

	if (!m_pVehicle->InheritsFromBike() || !m_pVehicle->m_nVehicleFlags.bScriptSetHandbrake)
	{
		if (ShouldUseSitToStillTransition(*m_pVehicle, *pPed))
		{
			if (CheckForStillToSitTransition(*m_pVehicle, *pPed))
			{
				SetTaskState(State_StillToSit);
				return FSM_Continue;
			}
		}
		else
		{
			float fUnused = -1.0f;
			if (!CTaskMotionInVehicle::CheckForStill(fTimeStep, *m_pVehicle, *pPed, m_fPitchAngle, fUnused))
			{
				SetTaskState(GetSitIdleState());
				return FSM_Continue;
			}
		}
	}

	if (CTaskMotionInVehicle::CheckForReverse(*m_pVehicle, *pPed, IsUsingFirstPersonAnims(), false, IsPanicking()))
	{
		SetTaskState(State_Reverse);
		return FSM_Continue;
	}

	m_fIdleTimer += GetTimeStep();

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::Still_OnExit()
{
	//Make sure we have cleared the request helmet
	if(GetPed()->GetHelmetComponent() && GetState() != State_PutOnHelmet)
	{
		GetPed()->GetHelmetComponent()->ClearPreLoadedProp();
	}

	m_fIdleTimer = 0.0f;
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::StillToSit_OnEnter()
{
	SetMoveNetworkState(ms_StillToSitRequestId, ms_StillToSitOnEnterId, CTaskMotionInVehicle::ms_InvalidStateId);
	SetClipsForState();

	const fwMvClipSetId clipsetId = m_moveNetworkHelper.GetClipSetId();
	const crClip* pBurnOutClip = fwClipSetManager::GetClip(clipsetId, ms_Tunables.m_BurnOutClipId);
	if (pBurnOutClip)
	{
		m_moveNetworkHelper.SetClip(pBurnOutClip, ms_BurnOutClipId);
		m_fBurnOutPhase = 0.4f;
		float fUnused;
		CClipEventTags::GetMoveEventStartAndEndPhases(pBurnOutClip, ms_BurnOutInterruptId, m_fBurnOutPhase, fUnused);
	}

	m_bWantsToMove = true;
	m_bStoppedWantingToMove = false;
	m_fStillTransitionRate = 1.0f;
	m_fHoldLegTime = 0.0f;
	m_fBurnOutBlend = 0.0f;
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::StillToSit_OnUpdate()
{
	// TODO: Write this in a simpler, neater way
	CPed& rPed = *GetPed();
	rPed.SetPedResetFlag(CPED_RESET_FLAG_RequiresLegIk, true);

	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	if(CanDoInstantSwitchToFirstPersonAnims())
	{
		SetTaskState(State_FirstPersonSitIdle);
		return FSM_Continue;
	}

	const float fVehVelocitySqd = m_pVehicle->GetVelocityIncludingReferenceFrame().Mag2();

	const bool bDoingBurnout = CheckIfVehicleIsDoingBurnout(*m_pVehicle, rPed);
	float fForwardNess = CheckIfPedWantsToMove(rPed, *m_pVehicle);
	bool bWantsToMove = fForwardNess > 0.0f || bDoingBurnout || ( fVehVelocitySqd > ( 1.0f * 1.0f ) );
	bool bInHoverMode = ( m_pVehicle->InheritsFromBike() && !m_pVehicle->HasGlider() && m_pVehicle->GetSpecialFlightModeRatio() > 0.5f );

	TUNE_GROUP_FLOAT(VEHICLE_TUNE, MIN_FORWARDNESS_TOCONTINUE_FULL_RATE, 0.4f, 0.0f, 1.0f, 0.01f);
	if (!m_bStoppedWantingToMove && fForwardNess < MIN_FORWARDNESS_TOCONTINUE_FULL_RATE && !rPed.GetPedResetFlag(CPED_RESET_FLAG_PreventGoingIntoStillInVehicleState))
	{
		m_bStoppedWantingToMove = true;
	}

	const float fClipPhase = m_moveNetworkHelper.GetFloat(ms_StillToSitPhaseId);
	const float fMinVelStillSqd = square(ms_Tunables.m_MinVelStillStart);
	const float fMaxHoldLegVelSqd = square(ms_Tunables.m_HoldLegOutVelocity);
	const float fForcedLegUpVelSqd = square(ms_Tunables.m_ForcedLegUpVelocity);
	float fDesiredPhase = bWantsToMove ? 1.0f : 0.0f;
	bool bIsSlow = false;
	
	if (!bDoingBurnout)
	{
		if (fVehVelocitySqd <= fMaxHoldLegVelSqd && m_bStoppedWantingToMove && !bInHoverMode )
		{
			TUNE_GROUP_FLOAT(VEHICLE_TUNE, MAX_TIME_HOLD_LEG_OUT_STILL_TO_SIT, 0.75f, 0.0f, 2.0f, 0.01f);
			if (bWantsToMove)
			{
				// Keep the leg out if teetering on the still velocity
				if (fVehVelocitySqd <= fMinVelStillSqd)
				{
					m_fHoldLegTime = 0.0f;
				}

				if (m_fHoldLegTime < MAX_TIME_HOLD_LEG_OUT_STILL_TO_SIT)
				{
					bIsSlow = true;
					TUNE_GROUP_FLOAT(VEHICLE_TUNE, MIN_HOLD_LEG_PHASE, 0.4f, 0.0f, 1.0f, 0.01f);
					TUNE_GROUP_FLOAT(VEHICLE_TUNE, MAX_HOLD_LEG_PHASE, 0.6f, 0.0f, 1.0f, 0.01f);
					const float fMinPhase = bWantsToMove ? MIN_HOLD_LEG_PHASE : 0.0f;
					const float fHoldLegRatio = fVehVelocitySqd / fMaxHoldLegVelSqd;
					fDesiredPhase = fMinPhase * (1.0f - fHoldLegRatio) + MAX_HOLD_LEG_PHASE * fHoldLegRatio;
				}
				m_fHoldLegTime += GetTimeStep();
			}
			else
			{
				m_fHoldLegTime = 0.0f;
			}
		}
		else if (fVehVelocitySqd > fForcedLegUpVelSqd || !m_bStoppedWantingToMove || bInHoverMode)
		{
			fDesiredPhase = 1.0f;
		}
	}
	else
	{
		fDesiredPhase = m_fBurnOutPhase;
	}

	float fCloseness = Abs(fClipPhase - fDesiredPhase);
	float fDesiredPlayBackRate = 1.0f;
	if (fDesiredPhase < fClipPhase)
	{
		fDesiredPlayBackRate = -1.0f;
		if (bDoingBurnout)
		{
			TUNE_GROUP_FLOAT(VEHICLE_TUNE, BURNOUT_MOVE_RATE, 0.5f, 0.0f, 1.0f, 0.01f);
			m_fStillTransitionRate = fDesiredPlayBackRate * BURNOUT_MOVE_RATE;
		}
		else if (bWantsToMove)
		{
			m_fStillTransitionRate = 0.0f;
		}
	}

	TUNE_GROUP_FLOAT(VEHICLE_TUNE, CLOSE_PHASE_TOL, 0.1f, 0.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_TUNE, STILL_TO_SIT_STILL_APPROACH_RATE_SLOW, 1.0f, 0.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_TUNE, STILL_TO_SIT_STILL_APPROACH_RATE, 2.0f, 0.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_TUNE, STILL_TO_SIT_STILL_APPROACH_RATE_CLOSE, 0.2f, 0.0f, 10.0f, 0.01f);
	float fApproachRate = bIsSlow ? STILL_TO_SIT_STILL_APPROACH_RATE_SLOW : STILL_TO_SIT_STILL_APPROACH_RATE;
	if (fCloseness < CLOSE_PHASE_TOL)
	{
		fApproachRate = STILL_TO_SIT_STILL_APPROACH_RATE_CLOSE;
	}

	Approach(m_fStillTransitionRate, fDesiredPlayBackRate, fApproachRate, GetTimeStep());
	m_moveNetworkHelper.SetFloat(ms_StillToSitRate, m_fStillTransitionRate);

	if (bDoingBurnout)
	{
		if (Abs(fClipPhase - fDesiredPhase) < ms_Tunables.m_BurnOutBlendInTol)
		{
			Approach(m_fBurnOutBlend, 1.0f, ms_Tunables.m_BurnOutBlendInSpeed, GetTimeStep());
		}
	}
	else
	{
		Approach(m_fBurnOutBlend, 0.0f, ms_Tunables.m_BurnOutBlendOutSpeed, GetTimeStep());
	}
	m_moveNetworkHelper.SetFloat(ms_BurnOutBlendId, m_fBurnOutBlend);

	TUNE_GROUP_FLOAT(VEHICLE_TUNE, STILL_TO_SIT_STILL_BLEND_PHASE, 0.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_TUNE, STILL_TO_SIT_IDLE_BLEND_PHASE, 1.0f, 0.0f, 1.0f, 0.01f);
	if (!bDoingBurnout)
	{
		if (bWantsToMove && fClipPhase >= STILL_TO_SIT_IDLE_BLEND_PHASE)
		{
			SetTaskState(GetSitIdleState());
			return FSM_Continue;
		}
		else if (!bWantsToMove && fClipPhase <= STILL_TO_SIT_STILL_BLEND_PHASE)
		{
			SetTaskState(State_Still);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::PutOnHelmet_OnEnter()
{
	CPed* pPed = GetPed();

	//Check the helmet is loaded
	if (pPed->GetIsDrivingVehicle() && (m_pVehicle->GetVehicleType() == VEHICLE_TYPE_HELI || m_pVehicle->GetVehicleType() == VEHICLE_TYPE_PLANE))
	{
		// FPS: use upper body only anims as the anim set used for helis/planes is arms/head only (merged with the sitidle anims)
		if (pPed->IsInFirstPersonVehicleCamera())
		{
			m_moveNetworkHelper.SetFlag(true, ms_FPSHelmetPutOnUseUpperBodyOnly);
		}
		if (CPedHelmetComponent::ShouldUsePilotFlightHelmet(pPed))
		{
			pPed->GetHelmetComponent()->SetPropFlag(PV_FLAG_PILOT_HELMET);
		}
		else
		{
			pPed->GetHelmetComponent()->SetPropFlag(PV_FLAG_FLIGHT_HELMET);
		}
	}
	else
	{
		// FPS: bikes etc use full body helmet anims due to the different seat positions
		if (pPed->IsInFirstPersonVehicleCamera())
		{
			m_moveNetworkHelper.SetFlag(false, ms_FPSHelmetPutOnUseUpperBodyOnly);
		}
		pPed->GetHelmetComponent()->SetPropFlag(PV_FLAG_DEFAULT_HELMET);
	}

	if (pPed->IsLocalPlayer() && pPed->IsInFirstPersonVehicleCamera())
	{
		// Disables PV_FLAG_HIDE_IN_FIRST_PERSON flag check in CPedPropsMgr::RenderPropsInternal from culling the helmet 
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableHelmetCullFPS, true);
	}
	
	pPed->SetPedResetFlag(CPED_RESET_FLAG_PuttingOnHelmet, true);

	RequestProcessMoveSignalCalls();
	
	SetMoveNetworkState(ms_PutOnHelmetRequestId, ms_PutOnHelmetOnEnterId, CTaskMotionInVehicle::ms_InvalidStateId);
	SetClipsForState();

	m_bCurrentAnimFinished = false;

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::PutOnHelmet_OnUpdate()
{
	CPed* pPed = GetPed();

#if FPS_MODE_SUPPORTED
	// Block camera changing while putting on helmet (as we're either playing the TPS or FPS helmet anim).
	if( pPed->IsLocalPlayer() )
	{
		pPed->SetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_CAMERA_VIEW_MODE_CYCLE);

		if (pPed->IsInFirstPersonVehicleCamera() || pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
		{

			float fCullPhase = -1.0f;
			const crClip *pClip = GetClipForStateNoAssert(State_PutOnHelmet);
			if (pClip)
			{
				u32 uVehicleModelHash = 0;
				if (m_pVehicle && m_pVehicle->GetVehicleModelInfo())
				{
					uVehicleModelHash = m_pVehicle->GetVehicleModelInfo()->GetModelNameHash();
				}

				// Check if we have a vehicle-specific tag phase
				if (pClip->GetTags() && uVehicleModelHash != 0)
				{
					CClipEventTags::CTagIterator<CClipEventTags::CFirstPersonHelmetCullEventTag> it(*pClip->GetTags(), CClipEventTags::FirstPersonHelmetCull);
					while (*it)
					{
						const CClipEventTags::CFirstPersonHelmetCullEventTag* pTag = (*it)->GetData();
						const u32 uTagVehicleModelHash = pTag->GetVehicleNameHash();
						if (uTagVehicleModelHash != 0 && uVehicleModelHash == uTagVehicleModelHash)
						{
							fCullPhase = (*it)->GetStart();
							break;
						}
						else
						{
							++it;
						}
					}
				}

				// Else just use the first tag phase value
				if (fCullPhase == -1.0f)
				{
					CClipEventTags::FindEventPhase(pClip, CClipEventTags::FirstPersonHelmetCull, fCullPhase);
				}
			}

			// Fall back to debug cull phase if no tag has been set
			if (fCullPhase == -1.0f)
			{
				TUNE_GROUP_FLOAT(FIRST_PERSON_HELMET_ANIMS, fStartCullPhase, 0.6f, 0.0f, 1.0f, 0.01f);
				fCullPhase = fStartCullPhase;
			}

			// Set flag so we keep the helmet visible until we reach a set phase in the anim (despite having PV_FLAG_HIDE_IN_FIRST_PERSON set on the prop)
			float fPutOnPhase = m_moveNetworkHelper.GetFloat(ms_PutOnHelmetPhaseId);
			if (fPutOnPhase < fCullPhase)
			{
				// Disables PV_FLAG_HIDE_IN_FIRST_PERSON flag check in CPedPropsMgr::RenderPropsInternal from culling the helmet 
				pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableHelmetCullFPS, true);
			}
		}
	}
#endif	//FPS_MODE_SUPPORTED

	//It's possible that the ped is exiting the vehicle as the helmet won't have been 
	//created yet. This detects if exiting and stops the ped creating a helmet.
	CTaskExitVehicle *task = (CTaskExitVehicle*)pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE);
	if(task)
	{
		return FSM_Continue;
	}

	//Stop the ai from driving off whilst putting on the helmet
	if(m_pVehicle && !pPed->IsPlayer() && pPed->GetIsDrivingVehicle())
	{
		m_pVehicle->SetThrottle(0.0f);
		m_pVehicle->SetBrake(HIGHEST_BRAKING_WITHOUT_LIGHT);
	}

	//Block head Ik as it breaks the animation and head alignment
	pPed->SetHeadIkBlocked();

	CTaskVehicleFSM::ProcessApplyForce(m_moveNetworkHelper, *m_pVehicle, *pPed);

	pPed->SetPedResetFlag(CPED_RESET_FLAG_RequiresLegIk, true);
	pPed->SetPedResetFlag(CPED_RESET_FLAG_PuttingOnHelmet, true);

	if (pPed->IsLocalPlayer())
	{
		//Disable switching camera whilst putting on the helmet. 
		CControlMgr::GetMainPlayerControl().DisableInput(INPUT_NEXT_CAMERA);
	}

	//Shouldn't be any state changes above here otherwise we risk the move network getting out of sync
	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	if(m_pVehicle && WillAbortPuttingOnHelmet(*m_pVehicle, *pPed) && (pPed->IsNetworkClone() || (!pPed->GetHelmetComponent()->IsHelmetInHand() &&
		(m_moveNetworkHelper.GetBoolean(ms_PutOnHelmetInterruptId) || pPed->GetHelmetComponent()->IsPreLoadingProp()))))
	{
		if (IsUsingFirstPersonAnims())
		{
			SetTaskState(GetSitIdleState());
		}
		else
		{
			SetTaskState(State_Still);
		}
		return FSM_Continue;
	}

	//Handle first person camera
	TUNE_GROUP_BOOL(FIRST_PERSON_HELMET_ANIMS, bEnablePutOnAnims, true);
	if(camInterface::ComputeShouldMakePedHeadInvisible(*pPed) && !bEnablePutOnAnims)
	{
		if(pPed->GetHelmetComponent()->IsHelmetInHand())
		{
			pPed->GetHelmetComponent()->DisableHelmet(false);
		}
		pPed->GetHelmetComponent()->EnableHelmet();
		pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true);
		if (IsUsingFirstPersonAnims())
		{
			SetTaskState(GetSitIdleState());
		}
		else
		{
			SetTaskState(State_Still);
		}
		return FSM_Continue;
	}

	if (m_moveNetworkHelper.GetBoolean(ms_PutOnHelmetCreateId))
	{	
		pPed->GetHelmetComponent()->EnableHelmet(ANCHOR_PH_L_HAND);
		pPed->GetHelmetComponent()->ClearPreLoadedProp();
	}

	if (m_moveNetworkHelper.GetBoolean(ms_AttachHelmetRequestId))
	{	
		pPed->GetHelmetComponent()->EnableHelmet();
		pPed->GetHelmetComponent()->ClearPreLoadedProp();
	}

	if (m_bCurrentAnimFinished)	// And check so we are actually playing a clip
	{
		pPed->GetHelmetComponent()->EnableHelmet();
		pPed->GetHelmetComponent()->ClearPreLoadedProp();
		if (IsUsingFirstPersonAnims())
		{
			SetTaskState(GetSitIdleState());
		}
		else
		{
			SetTaskState(State_Still);
		}
		return FSM_Continue;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskMotionInAutomobile::PutOnHelmet_OnExit()
{
	CPed* pPed = GetPed();

	if(pPed->GetHelmetComponent()->IsHelmetInHand() || pPed->IsDead() || (m_Flags.IsFlagSet(aiTaskFlags::IsAborted) && !pPed->GetHelmetComponent()->IsAlreadyWearingAHelmetProp(m_pVehicle)))
	{
		// Get rid of the helmet.
		const Vector3 impulse(-8.0f,6.0f,-2.5f);
		pPed->GetHelmetComponent()->DisableHelmet(true, &impulse, true);

		// we have a stored hat that wasn't restored in the call to DisableHelmet above
		if(pPed->GetHelmetComponent()->GetStoredHatPropIndex() != -1 || pPed->GetHelmetComponent()->GetStoredHatTexIndex() != -1)
		{
			// because there was no spare slots as the player was using all three (watch, glasses, helmet) and we couldn't clear the helmet prop as it's a delayed delete...
			if(CPedPropsMgr::GetNumActiveProps(pPed) == MAX_PROPS_PER_PED && CPedPropsMgr::IsPedPropDelayedDelete(pPed, ANCHOR_HEAD))
			{
				// we should flag the hat so it restores later when the helmet has been cleared.
				if(!pPed->GetHelmetComponent()->IsPendingHatRestore())
				{
					pPed->GetHelmetComponent()->RegisterPendingHatRestore();
				}
			}
		}
	}

	// B*1933002: Clear the prop flag when aborting. Ensures we equip the appropriate helmet when entering another vehicle.
	if (m_Flags.IsFlagSet(aiTaskFlags::IsAborted) && !pPed->GetHelmetComponent()->IsHelmetEnabled())
	{
		pPed->GetHelmetComponent()->SetPropFlag(PV_FLAG_NONE);
	}

	pPed->GetHelmetComponent()->ClearPreLoadedProp();

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInAutomobile::UpdateFirstPersonAnimationState()
{
	CPed* pPed = GetPed();

	const CPed* pCamFollowPed = camInterface::FindFollowPed();
	if(pCamFollowPed && pCamFollowPed->GetVehiclePedInside() == m_pVehicle)
	{
		bool bCheckDominant = pCamFollowPed->GetPedResetFlag(CPED_RESET_FLAG_UseFirstPersonVehicleAnimsIfFPSCamNotDominant) ? false : true;
		bool bInFirstPersonVehicleCamera = pCamFollowPed->IsInFirstPersonVehicleCamera(bCheckDominant) FPS_MODE_SUPPORTED_ONLY(|| pCamFollowPed->IsFirstPersonShooterModeEnabledForPlayer(false));

		const fwMvClipSetId inVehicleClipSetId = GetDefaultClipSetId(DCS_Idle);

		if (CTaskVehicleFSM::IsVehicleClipSetLoaded(inVehicleClipSetId))
		{
			bool bInstantUpdate = false;

			if( !camInterface::GetDebugDirector().IsFreeCamActive() && m_moveNetworkHelper.IsNetworkActive() )
			{
				m_bUseFirstPersonAnims = CanTransitionToFirstPersonAnims();

				//! If going from 1st to 3rd (Or vice versa), do an instant camera cut, where appropriate.
				if( (bInFirstPersonVehicleCamera != m_bWasInFirstPersonVehicleCamera) && ((GetPed() ==camInterface::FindFollowPed()) || (GetPed() == CGameWorld::FindLocalPlayer())))
				{
					pPed->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAIUpdate, true );
					pPed->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true );
					m_moveNetworkHelper.SetFloat(ms_FirstPersonBlendDurationId, 0.0f);
					bInstantUpdate = true;
				}

				if(IsUsingFirstPersonSteeringAnims() && (m_moveNetworkHelper.GetClipSetId(ms_FirstPersonSteeringClipSetId) != inVehicleClipSetId))
				{
					m_moveNetworkHelper.SetClipSet(inVehicleClipSetId, ms_FirstPersonSteeringClipSetId);
				}

				const bool bFirstPersonVehicleCamera = pPed->IsInFirstPersonVehicleCamera();
				const bool bFirstPersonCamera = pPed->IsFirstPersonShooterModeEnabledForPlayer(false);
				m_moveNetworkHelper.SetFlag(bFirstPersonVehicleCamera, ms_FirstPersonCameraFlagId);
				m_moveNetworkHelper.SetFlag(bFirstPersonVehicleCamera || bFirstPersonCamera, ms_FirstPersonModeFlagId);
				m_moveNetworkHelper.SetFlag(CanUsePoVAnims(), ms_UsePOVAnimsFlagId);
			}

			//! If driver, do an instant snap to anim. If this is a ped that a local ped is looking at in 1st person view, do a normal blend.
			if(!bInstantUpdate)
			{
				m_moveNetworkHelper.SetFloat(ms_FirstPersonBlendDurationId, 0.25f);
			}
		}

		m_bWasInFirstPersonVehicleCamera = bInFirstPersonVehicleCamera;
	}
}

bool CTaskMotionInAutomobile::ProcessPostCamera()
{
	UpdateFirstPersonAnimationState();
	ProcessBlockHandIkForVisor();
	return false;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::FirstPersonSitIdle_OnEnter()
{
	SetMoveNetworkState(CTaskMotionInVehicle::ms_FirstPersonSitIdleRequestId, CTaskMotionInVehicle::ms_FirstPersonSitIdleOnEnterId, CTaskMotionInVehicle::ms_InvalidStateId);
	
	m_fTimeTillNextAdditiveIdle = fwRandom::GetRandomNumberInRange(ms_Tunables.m_MinTimeToNextFirstPersonAdditiveIdle, ms_Tunables.m_MaxTimeToNextFirstPersonAdditiveIdle);
	m_bPlayingFirstPersonAdditiveIdle = false;
	m_bPlayingFirstPersonShunt = false;
	m_bPlayingFirstPersonRoadRage = false;

	PickRoadRageAnim();

	m_moveNetworkHelper.SetFlag(false, CTaskMotionInVehicle::ms_HornIsBeingUsedId);
	m_bCurrentAnimFinished = false;

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::FirstPersonSitIdle_OnUpdate()
{
	if (m_pVehicle->InheritsFromBike())
	{
		m_moveNetworkHelper.SetFloat(ms_SitFirstPersonRateId, 0.0f);
	}

	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	CPed* pPed = GetPed();

	if (pPed->ShouldBeDead())
	{
		return FSM_Continue;
	}

	//	Enable helmet/headset put on anims for helicopters and planes (SP ONLY)
	if (!NetworkInterface::IsGameInProgress() && pPed->GetIsDrivingVehicle() && (m_pVehicle->GetVehicleType() == VEHICLE_TYPE_HELI || m_pVehicle->GetVehicleType() == VEHICLE_TYPE_PLANE))
	{
		if (CheckForHelmet(*pPed))
		{
			//Make sure we have the helmet loaded before starting putting it on
			bool bFinishedPreLoading = pPed->GetHelmetComponent()->HasPreLoadedProp();
			if(	!pPed->GetHelmetComponent()->IsPreLoadingProp() && !bFinishedPreLoading)
			{
				//Load the pilot helmet
				if (CPedHelmetComponent::ShouldUsePilotFlightHelmet(pPed))
				{
					pPed->GetHelmetComponent()->SetPropFlag(PV_FLAG_PILOT_HELMET);
				}
				else
				{
					pPed->GetHelmetComponent()->SetPropFlag(PV_FLAG_FLIGHT_HELMET);
				}
				pPed->GetHelmetComponent()->RequestPreLoadProp();
				return FSM_Continue;
			}

			if(bFinishedPreLoading)
			{	
				// If we're using the pilot helmet, play the put-on animation; else just pop it on instantly.
				if (CPedHelmetComponent::ShouldUsePilotFlightHelmet(pPed))
				{
					SetTaskState(State_PutOnHelmet);
					return FSM_Continue;
				}
				else
				{
					pPed->GetHelmetComponent()->EnableHelmet();
				}
			}
		}
		if(m_fIdleTimer <= IDLE_TIMER_HELMET_PUT_ON)
			m_fIdleTimer += GetTimeStep();
	}

	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostCamera, true);

	TUNE_GROUP_FLOAT(FIRST_PERSON_STEERING, fSteerAdditiveRate, 0.15f, 0.0f, 2.0f, 0.01f);
	m_moveNetworkHelper.SetFloat(ms_InVehicleAnimsRateId, IsUsingFirstPersonSteeringAnims() ? fSteerAdditiveRate : 1.0f);

	if(!IsUsingFirstPersonAnims())
	{
		SetTaskState(State_SitIdle);
		return FSM_Continue;
	}

	if (CTaskMotionInVehicle::CheckForHotwiring(*m_pVehicle, *pPed))
	{
		SetTaskState(State_Hotwiring);
		return FSM_Continue;
	}

	bool bTimeBetweenCloseDoorAttemptsConditionPassed = true;
	if (GetPreviousState() == State_CloseDoor && GetTimeInState() < ms_Tunables.m_MinTimeBetweenCloseDoorAttempts)
	{
		bTimeBetweenCloseDoorAttemptsConditionPassed = false;
	}

	bool bTimeSinceLastDriveByConditionPassed = true;
	const s32 iTimeSinceDriveBy = fwTimer::GetTimeInMilliseconds() - m_uLastDriveByTime;
	if (m_uLastDriveByTime > 0 && (iTimeSinceDriveBy < ms_Tunables.m_MinTimeSinceDriveByForCloseDoor))
	{
		bTimeSinceLastDriveByConditionPassed = false;
	}

	if (bTimeBetweenCloseDoorAttemptsConditionPassed && bTimeSinceLastDriveByConditionPassed && CTaskMotionInVehicle::CheckForClosingDoor(*m_pVehicle, *pPed))
	{
		// Check that door is not already being closed
		const bool bTwoDoorsOneSeat = m_pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_TWO_DOORS_ONE_SEAT);
		const CCarDoor* pDoor = NULL;

		if (bTwoDoorsOneSeat)
		{
			const s32 iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
			s32 iEntryPointIndex = m_pVehicle->GetDirectEntryPointIndexForSeat(iSeatIndex);

			pDoor = CTaskMotionInVehicle::GetOpenDoorForDoubleDoorSeat(*m_pVehicle, iSeatIndex, iEntryPointIndex);
		}
		else
		{
			pDoor = CTaskMotionInVehicle::GetDirectAccessEntryDoorFromPed(*m_pVehicle, *pPed);
		}

		if (pDoor && (pDoor->GetTargetDoorRatio() > 0.0f))
		{
			if (IsCloseDoorStateValid())
			{
				SetTaskState(State_ReserveDoorForClose);
			}

			// Wait to stream close door anims
			return FSM_Continue;
		}
	}

	if (CheckForStartingEngine(*m_pVehicle, *pPed))
	{
		SetTaskState(State_StartEngine);
		return FSM_Continue;
	}

	bool bCanTransition = GetPed() != camInterface::FindFollowPed();

	//! Don't allow these transitions in 1st person mode as they cause clipping issues.
	if(bCanTransition)
	{
		if (CTaskMotionInVehicle::CheckForShunt(*m_pVehicle, *pPed, m_vCurrentVehicleAcceleration.GetZf()) ||
			CheckForShuntFromHighFall(*m_pVehicle, *pPed))
		{
			SetTaskState(State_Shunt);
			return FSM_Continue;
		}

		if (CTaskMotionInAutomobile::CheckForHorn())
		{
			SetTaskState(State_Horn);
			return FSM_Continue;
		}

		if (CTaskMotionInVehicle::CheckForReverse(*m_pVehicle, *pPed, IsUsingFirstPersonAnims(), false, IsPanicking()))
		{
			SetTaskState(State_FirstPersonReverse);
			return FSM_Continue;
		}

		if (CheckForChangingStation(*m_pVehicle, *pPed))
		{
			SetTaskState(State_ChangeStation);
			return FSM_Continue;
		}

		if(ShouldBeDucked())
		{
			SetTaskState(State_SitIdle);
		}
	}
	else
	{
		if(GetPed()==m_pVehicle->GetDriver())
		{
			if(m_bPlayingFirstPersonShunt)
			{
				if(m_moveNetworkHelper.GetBoolean(ms_FirstPersonShuntFinishedId))
				{
					m_bPlayingFirstPersonShunt = false;
				}
			}
			else
			{
				if (CTaskMotionInVehicle::CheckForShunt(*m_pVehicle, *pPed, m_vCurrentVehicleAcceleration.GetZf()) ||
					CheckForShuntFromHighFall(*m_pVehicle, *pPed))
				{
					m_bPlayingFirstPersonShunt = true;
				}
			}
		}
	}

	if(pPed==m_pVehicle->GetDriver())
	{
		rage::Approach(m_fSteeringWheelWeight, 1.0f, ms_Tunables.m_SteeringWheelBlendInApproachSpeed, fwTimer::GetTimeStep());

		UpdateFirstPersonAdditiveIdles();

		m_moveNetworkHelper.SetFlag(m_bPlayingFirstPersonShunt, ms_PlayFirstPersonShunt);

		bool bFinishedRoadRage = false;
		if(m_bPlayingFirstPersonRoadRage)
		{
			if(m_moveNetworkHelper.GetBoolean(ms_FirstPersonRoadRageFinishedId))
			{
				m_bPlayingFirstPersonRoadRage = false;
				bFinishedRoadRage = true;
				PickRoadRageAnim();
			}
			
			// B*2068927: Apply pad shake if tag has been set in the anim.
			const CClipEventTags::CPadShakeTag* pPadShake = static_cast<const CClipEventTags::CPadShakeTag*>(m_moveNetworkHelper.GetNetworkPlayer()->GetProperty(CClipEventTags::PadShake));
			if (pPadShake)
			{
				u32 iRumbleDuration = pPadShake->GetPadShakeDuration();
				float fRumbleIntensity = Clamp(pPadShake->GetPadShakeIntensity(), 0.0f, 1.0f);
				CControlMgr::StartPlayerPadShakeByIntensity(iRumbleDuration, fRumbleIntensity);
			}

			if(camInterface::GetCinematicDirector().IsRenderingAnyInVehicleFirstPersonCinematicCamera() && pPed->IsLocalPlayer())
			{
				const CClipEventTags::CCameraShakeTag* pCamShake = static_cast<const CClipEventTags::CCameraShakeTag*>(m_moveNetworkHelper.GetNetworkPlayer()->GetProperty(CClipEventTags::CameraShake));
				if(pCamShake && pCamShake->GetShakeRefHash() != 0 && camInterface::GetCinematicDirector().GetRenderedCamera())
				{
					camInterface::GetCinematicDirector().GetRenderedCamera()->Shake( pCamShake->GetShakeRefHash() );
				}
			}
		}

#if __BANK
		TUNE_GROUP_BOOL(FIRST_PERSON_STEERING, bTestRoadRage, false);
		if(bTestRoadRage)
		{
			m_bPlayingFirstPersonRoadRage = true;
		}
#endif

		if(pPed->GetPedResetFlag(CPED_RESET_FLAG_TriggerRoadRageAnim))
		{
			m_bPlayingFirstPersonRoadRage = true;
		}

		//! only play road rage anim when clipset has loaded in.
		if(m_RoadRageClipSetRequestHelper.Request() && !bFinishedRoadRage)
		{
			m_moveNetworkHelper.SetClipSet(m_RoadRageClipSetRequestHelper.GetClipSetId(), ms_FirstPersonRoadRageClipSetId);
			m_moveNetworkHelper.SetFlag(m_bPlayingFirstPersonRoadRage, ms_PlayFirstPersonRoadRage);
		}
		else
		{
			m_moveNetworkHelper.SetFlag(false, ms_PlayFirstPersonRoadRage);
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::FirstPersonReverse_OnEnter()
{
	SetMoveNetworkState(CTaskMotionInVehicle::ms_FirstPersonReverseRequestId, CTaskMotionInVehicle::ms_FirstPersonReverseOnEnterId, CTaskMotionInVehicle::ms_InvalidStateId);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::FirstPersonReverse_OnUpdate()
{
	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	CPed* pPed = GetPed();

	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostCamera, true);

	TUNE_GROUP_FLOAT(FIRST_PERSON_STEERING, fReverseAdditiveRate, 1.0f, 0.0f, 2.0f, 0.01f);
	m_moveNetworkHelper.SetFloat(ms_InVehicleAnimsRateId, fReverseAdditiveRate);

	if(!IsUsingFirstPersonAnims())
	{
		SetTaskState(State_SitIdle);
		return FSM_Continue;
	}

	if (!CTaskMotionInVehicle::CheckForReverse(*m_pVehicle, *pPed, IsUsingFirstPersonAnims(), false, IsPanicking()))
	{
		SetTaskState(State_FirstPersonSitIdle);
		return FSM_Continue;
	}

	rage::Approach(m_fSteeringWheelWeight, 1.0f, ms_Tunables.m_SteeringWheelBlendInApproachSpeed, fwTimer::GetTimeStep());

	return FSM_Continue;
}

void CTaskMotionInAutomobile::SetSitIdleMoveNetworkState()
{
	fwMvRequestId requestId;
	fwMvBooleanId onEnterId;

	if (IsUsingFirstPersonAnims())
	{
		requestId = CTaskMotionInVehicle::ms_FirstPersonSitIdleRequestId;
		onEnterId = CTaskMotionInVehicle::ms_FirstPersonSitIdleOnEnterId;
	}
	else
	{
		requestId = CTaskMotionInVehicle::ms_IdleRequestId;
		onEnterId = CTaskMotionInVehicle::ms_IdleOnEnterId;
	}

	SetMoveNetworkState(requestId, onEnterId, CTaskMotionInVehicle::ms_InvalidStateId);
}

s32 CTaskMotionInAutomobile::GetSitIdleState() const
{
	return IsUsingFirstPersonAnims() ? State_FirstPersonSitIdle : State_SitIdle;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::ShouldUseCustomPOVSettings(const CPed& rPed, const CVehicle& rVeh)
{
	if (!rPed.IsLocalPlayer())
		return false;

	if (!rVeh.GetPOVTuningInfo())
		return false;
	
	return rPed.IsInFirstPersonVehicleCamera();
}

////////////////////////////////////////////////////////////////////////////////

float CTaskMotionInAutomobile::ComputeLeanAngleSignal(const CVehicle& rVeh)
{
	if (!rVeh.InheritsFromBike())
	{
		return 0.5f;
	}

	float fLeanAngleSignal = rVeh.InheritsFromBike() ? static_cast<const CBike&>(rVeh).GetLeanAngle() : 0.0f;
	const float fMaxLeanAngle = rVeh.pHandling->GetBikeHandlingData()->m_fMaxBankAngle;
	fLeanAngleSignal /= fMaxLeanAngle;
	fLeanAngleSignal += 1.0f;
	fLeanAngleSignal *= 0.5f;
	return fLeanAngleSignal;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::IsFirstPersonDrivingJetski(const CPed& rPed, const CVehicle& rVeh)
{
	if (!rVeh.GetIsJetSki())
		return false;

	return rPed.IsInFirstPersonVehicleCamera();
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::IsFirstPersonDrivingQuadBike(const CPed& rPed, const CVehicle& rVeh)
{
	if (!rVeh.InheritsFromQuadBike() && !rVeh.InheritsFromAmphibiousQuadBike())
		return false;

	return rPed.IsInFirstPersonVehicleCamera();
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::IsFirstPersonDrivingBicycle(const CPed& rPed, const CVehicle& rVeh)
{
	if (!rVeh.InheritsFromBicycle())
		return false;

	return rPed.IsInFirstPersonVehicleCamera();
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::IsFirstPersonDrivingBikeQuadBikeOrBicycle(const CPed& rPed, const CVehicle& rVeh)
{
	if (!rVeh.InheritsFromBike() && !rVeh.InheritsFromQuadBike() && !rVeh.InheritsFromAmphibiousQuadBike())
		return false;

	return rPed.IsInFirstPersonVehicleCamera();
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::IsUsingFirstPersonSteeringAnims() const 
{ 
	return IsUsingFirstPersonAnims() && m_pVehicle && m_pVehicle->GetDriver()==GetPed(); 
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::IsUsingFirstPersonAnims() const
{
	return m_bUseFirstPersonAnims;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::CanDoInstantSwitchToFirstPersonAnims() const
{
	//! Only do instant switch if cam is following this ped.
	return IsUsingFirstPersonAnims() && ((GetPed() == camInterface::FindFollowPed()) || (GetPed() == CGameWorld::FindLocalPlayer()));
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::CanPedBeVisibleInPoVCamera(bool bShouldTestCamera) const
{
	if(CanTransitionToFirstPersonAnims(bShouldTestCamera))
	{
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::CanUsePoVAnims() const
{
	if(m_pVehicle)
	{
		//! Note - we always use idle clipset.
		const fwMvClipSetId fpIdleClipsetId = GetDefaultClipSetId(DCS_Idle);
		if(fpIdleClipsetId != CLIP_SET_ID_INVALID)
		{
			static fwMvClipId fpClip("pov_steer",0x637a9ca8);
			const crClip* pClip = fwClipSetManager::GetClip( fpIdleClipsetId, fpClip );
			if(pClip)
			{
				return true;
			}
		}

		if(m_pVehicle->GetIsAircraft() || m_pVehicle->GetIsJetSki() || m_pVehicle->InheritsFromBike())
		{
			bool bMicrolight = MI_PLANE_MICROLIGHT.IsValid() && m_pVehicle->GetModelIndex() == MI_PLANE_MICROLIGHT;
			if (!bMicrolight)
				return false;
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::CanTransitionToFirstPersonAnims(bool bShouldTestCamera) const
{
	const camBaseCamera* pDominantCamera = camInterface::GetDominantRenderedCamera();
	bool bIsInBonnetCam = !camInterface::GetCinematicDirector().IsRenderingCinematicCameraInsideVehicle();
	const bool bIsCameraValid = !bIsInBonnetCam && (!bShouldTestCamera ||
		(FPS_MODE_SUPPORTED_ONLY(GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false) ||) (pDominantCamera && pDominantCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))));

	const CPed* pPed = GetPed();
	if(bIsCameraValid || pPed->GetPedResetFlag(CPED_RESET_FLAG_UseFirstPersonVehicleAnimsIfFPSCamNotDominant))
	{
		if(pPed->GetIsDrivingVehicle() && m_pVehicle)
		{
			if (NetworkInterface::IsGameInProgress() && !pPed->IsLocalPlayer() && (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UsingLowriderLeans) || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UsingAlternateLowriderLeans)))
			{
				return false;
			}

			if (!pPed->IsMale() && m_pVehicle->InheritsFromBoat() && !NetworkInterface::IsGameInProgress())
			{
				return false;
			}

			if(pPed != camInterface::FindFollowPed() && ShouldBeDucked())
			{
				return false;
			}

			bool bBikeDriveby = m_pVehicle->InheritsFromBike() && pPed->GetPedResetFlag(CPED_RESET_FLAG_IsDoingDriveby) && (GetState() != State_Still);

			if(m_pVehicle->GetIsAircraft() || m_pVehicle->GetIsJetSki() || bBikeDriveby)
			{
				//! yes - use existing sit anim for PoV steer so that we can go into 1st driving state.
				return true;
			}
			else
			{
				//! Note - we always use idle clipset.
				const fwMvClipSetId fpIdleClipsetId = GetDefaultClipSetId(DCS_Idle);
				if(fpIdleClipsetId != CLIP_SET_ID_INVALID)
				{
					static fwMvClipId fpClip("pov_steer",0x637a9ca8);
					const crClip* pClip = fwClipSetManager::GetClip( fpIdleClipsetId, fpClip );
					if(pClip)
					{
						return true;
					}
				}
			}
		}
		else
		{
			//! Passengers can always transition if they are follow ped.
			return pPed == camInterface::FindFollowPed();
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::CanDelayFirstPersonCamera() const
{
	return false;
}

////////////////////////////////////////////////////////////////////////////////

float CTaskMotionInAutomobile::GetSteeringWheelWeight() const
{
	return m_fSteeringWheelWeight;
}

////////////////////////////////////////////////////////////////////////////////

float CTaskMotionInAutomobile::GetSteeringWheelAngle() const
{
	return m_fSteeringWheelAngle;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInAutomobile::UpdateFirstPersonAdditiveIdles()
{
	CTaskAimGunVehicleDriveBy* pGunDriveByTask = static_cast<CTaskAimGunVehicleDriveBy*>(GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY));

	bool bSteeringCentred = abs(m_fSteering - 0.5f) < ms_Tunables.m_FirstPersonAdditiveIdleSteeringCentreThreshold;
	
	TUNE_GROUP_FLOAT(FIRST_PERSON_STEERING, fVelocityToBlendOutAdditiveIdles, 2.0f, 0.0f, 1000.0f, 0.01f);
	Vector3 vVelocity = m_pVehicle->GetVelocity();
	bool bVelOkForAdditive = vVelocity.Mag2() < (fVelocityToBlendOutAdditiveIdles * fVelocityToBlendOutAdditiveIdles);
	if(bVelOkForAdditive && !m_bPlayingFirstPersonShunt && !pGunDriveByTask && !m_bPlayingFirstPersonRoadRage 
					&& bSteeringCentred && !GetPed()->GetPedResetFlag(CPED_RESET_FLAG_UsingMobilePhone))
	{
		m_fTimeTillNextAdditiveIdle-= fwTimer::GetTimeStep();
		if(m_fTimeTillNextAdditiveIdle < 0.0f && m_AdditiveIdleClipSetRequestHelper.IsInvalid())
		{
			m_fTimeTillNextAdditiveIdle = 0.0f;

			const CVehicleLayoutInfo *pLayoutInfo = m_pVehicle->GetLayoutInfo();
			if(pLayoutInfo)
			{
				s32 iNumClipsets = pLayoutInfo->GetNumFirstPersonAddiveClipSets();
				if(iNumClipsets > 0)
				{
					u32 iIndex = (u32)fwRandom::GetRandomNumberInRange(0, pLayoutInfo->GetNumFirstPersonAddiveClipSets());
					fwMvClipSetId additiveClipSet = pLayoutInfo->GetFirstPersonAddiveClipSet(iIndex);
					if(additiveClipSet != CLIP_SET_ID_INVALID)
					{
						m_AdditiveIdleClipSetRequestHelper.Request(additiveClipSet);
					}
				}
			}
		}

		if(m_bPlayingFirstPersonAdditiveIdle)
		{
			if(m_moveNetworkHelper.GetBoolean(ms_FirstPersonAdditiveIdleFinishedId))
			{
				FinishFirstPersonIdleAdditive();
			}
		}
		else
		{
			if(m_AdditiveIdleClipSetRequestHelper.Request())
			{
				m_bPlayingFirstPersonAdditiveIdle = true;
				m_moveNetworkHelper.SetClipSet(m_AdditiveIdleClipSetRequestHelper.GetClipSetId(), ms_FirstPersonAdditiveClipSetId);
			}
		}
	}
	else
	{
		FinishFirstPersonIdleAdditive();
	}

	m_moveNetworkHelper.SetFlag(m_bPlayingFirstPersonAdditiveIdle, ms_PlayFirstPersonAdditive);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInAutomobile::FinishFirstPersonIdleAdditive()
{
	m_bPlayingFirstPersonAdditiveIdle = false;
	m_AdditiveIdleClipSetRequestHelper.Release();
	m_fTimeTillNextAdditiveIdle = fwRandom::GetRandomNumberInRange(ms_Tunables.m_MinTimeToNextFirstPersonAdditiveIdle, ms_Tunables.m_MaxTimeToNextFirstPersonAdditiveIdle);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInAutomobile::TriggerRoadRage()
{
	m_bPlayingFirstPersonRoadRage = true;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInAutomobile::PickRoadRageAnim()
{
	m_RoadRageClipSetRequestHelper.Release();

	const CVehicleLayoutInfo *pLayoutInfo = m_pVehicle->GetLayoutInfo();
	if(pLayoutInfo)
	{
		s32 iNumClipsets = pLayoutInfo->GetNumFirstPersonRoadRageClipSets();
		if(iNumClipsets > 0)
		{
			u32 iIndex = (u32)fwRandom::GetRandomNumberInRange(0, pLayoutInfo->GetNumFirstPersonRoadRageClipSets());
			fwMvClipSetId roadRageClipSet = pLayoutInfo->GetFirstPersonRoadRageClipSet(iIndex);
			if(roadRageClipSet != CLIP_SET_ID_INVALID)
			{
				m_RoadRageClipSetRequestHelper.Request(roadRageClipSet);
			}
		}
	}
}

bool CTaskMotionInAutomobile::CanBlockCameraViewModeTransitions() const 
{
	if(GetPed()->IsLocalPlayer())
	{
		if (GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingHelmetVisor))
			return true;

		//! We arent in a position to do an instant anim update, so don't transition.
		if (!IsMoveNetworkStateValid())
			return true;

		//! Don't do in certain states where we override anims. Note: in certain states like shunt, we allow transition to 1st person as we
		//! instantly snap to correct pose. This handles the states where we have no instant transition.
		switch(GetState())
		{
		case(State_StartEngine):
		case(State_Hotwiring):
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::SitIdle_OnEnter()
{
	//Generate a varied rate.
	float fRate = fwRandom::GetRandomNumberInRange(CTaskMotionInVehicle::ms_Tunables.m_MinRateForInVehicleAnims, CTaskMotionInVehicle::ms_Tunables.m_MaxRateForInVehicleAnims);
	m_moveNetworkHelper.SetFloat(ms_InVehicleAnimsRateId, fRate);
	SetMoveNetworkState(CTaskMotionInVehicle::ms_IdleRequestId, CTaskMotionInVehicle::ms_IdleOnEnterId, CTaskMotionInVehicle::ms_InvalidStateId);
	if (m_pBikeAnimParams && GetPreviousState() == State_Still)
	{
		m_pBikeAnimParams->bikeLeanAngleHelper.SetLeanAngle(0.5f);
	}

	//Generate the phase.
	float fPhase = (m_nDefaultClipSet != DCS_Panic) ? 0.0f : fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
	m_moveNetworkHelper.SetFloat(ms_InVehicleAnimsPhaseId, fPhase);

	m_uStateSitIdle_OnEnterTimeMS = fwTimer::GetTimeInMilliseconds();

	if (GetPreviousState() == State_Shunt)
	{
		if (ShouldPanic())
		{
			m_moveNetworkHelper.SetFloat(ms_ShuntToIdleBlendDurationId, ms_Tunables.m_PanicShuntToIdleBlendDuration);
		}
		else
		{
			m_moveNetworkHelper.SetFloat(ms_ShuntToIdleBlendDurationId, ms_Tunables.m_DefaultShuntToIdleBlendDuration);
		}
	}

	m_bCurrentAnimFinished = false;

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::SitIdle_OnUpdate()
{
	//! Allow 1st person to override transition from start state.
	if(GetPreviousState() == State_Start && IsUsingFirstPersonAnims())
	{
		SetTaskState(State_FirstPersonSitIdle);
		return FSM_Continue;
	}

	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	CPed* pPed = GetPed();

	if (pPed->ShouldBeDead())
	{
		return FSM_Continue;
	}

	if(IsUsingFirstPersonAnims())
	{
		SetTaskState(State_FirstPersonSitIdle);
		return FSM_Continue;
	}

	if( m_pVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE || m_pVehicle->GetIsJetSki() )
	{
		Assert( pPed );
		if( pPed->IsPlayer() )
		{
			float vVehicleSpeedSqr;
			if(m_pVehicle->IsCachedAiDataValid())
			{
				// If valid then used the cached data
				vVehicleSpeedSqr = m_pVehicle->GetAiXYSpeed() * m_pVehicle->GetAiXYSpeed();
			}
			else
			{
				// If the cached data is invalid then get the velocity directly
				Vec3V vehVelocity = VECTOR3_TO_VEC3V(m_pVehicle->GetVelocity());			
				vVehicleSpeedSqr = MagSquared(vehVelocity.GetXY()).Getf();
			}

			/*static*/ float minSpeedSqr = 10.0f;
			/*static*/ float maxSpeedSqr = 1000.0f;
			
			if( vVehicleSpeedSqr > minSpeedSqr )
			{
				float speedScale = Min(vVehicleSpeedSqr, maxSpeedSqr) * FPInvertFast(maxSpeedSqr);
				pPed->SetClothWindScale( 0.4f * ( 2.0f - speedScale) );
			}
			else
			{
				pPed->SetClothWindScale(1.0f);
			}
		}
	}


	// Prevent the animated attach from playing anything other than the idle (don't have the anims)
	s32 iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
	if (m_pVehicle->IsSeatIndexValid(iSeatIndex))
	{
		const CVehicleSeatInfo* pSeatInfo = m_pVehicle->GetSeatInfo(iSeatIndex);
		if (pSeatInfo && pSeatInfo->GetDefaultCarTask() == CVehicleSeatInfo::TASK_HANGING_ON_VEHICLE)
		{
			return FSM_Continue;
		}
	}

	if (CTaskMotionInVehicle::CheckForShunt(*m_pVehicle, *pPed, m_vCurrentVehicleAcceleration.GetZf()) ||
		CheckForShuntFromHighFall(*m_pVehicle, *pPed))
	{
		SetTaskState(State_Shunt);
		return FSM_Continue;
	}

	TUNE_GROUP_BOOL(IN_VEHICLE_TUNE, DISABLE_HEAVY_BRAKE_WHEN_DUCKING, true)
	if ( !pPed->GetPedResetFlag(CPED_RESET_FLAG_PreventGoingIntoShuntInVehicleState) 
		&& CTaskMotionInVehicle::CheckForHeavyBrake(*pPed, *m_pVehicle, m_vCurrentVehicleAccelerationNorm.GetYf(), m_uLastTimeLargeZAcceleration) 
		&& (!DISABLE_HEAVY_BRAKE_WHEN_DUCKING || !IsDucking()))
	{
		SetTaskState(State_HeavyBrake);
		return FSM_Continue;
	}
	else if (CTaskMotionInVehicle::CheckForHotwiring(*m_pVehicle, *pPed))
	{
		SetTaskState(State_Hotwiring);
		return FSM_Continue;
	}

	bool bTimeBetweenCloseDoorAttemptsConditionPassed = true;
	if (GetPreviousState() == State_CloseDoor && GetTimeInState() < ms_Tunables.m_MinTimeBetweenCloseDoorAttempts)
	{
		bTimeBetweenCloseDoorAttemptsConditionPassed = false;
	}

	bool bTimeSinceLastDriveByConditionPassed = true;
	const s32 iTimeSinceDriveBy = fwTimer::GetTimeInMilliseconds() - m_uLastDriveByTime;
	if (m_uLastDriveByTime > 0 && (iTimeSinceDriveBy < ms_Tunables.m_MinTimeSinceDriveByForCloseDoor))
	{
		bTimeSinceLastDriveByConditionPassed = false;
	}

	if (bTimeBetweenCloseDoorAttemptsConditionPassed && bTimeSinceLastDriveByConditionPassed && CTaskMotionInVehicle::CheckForClosingDoor(*m_pVehicle, *pPed, true))
	{
		if (IsCloseDoorStateValid())
		{
			vehicledoorDebugf2("CTaskMotionInAutomobile::SitIdle_OnUpdate--bTimeBetweenCloseDoorAttemptsConditionPassed && CheckForClosingDoor && IsCloseDoorStateValid-->Set State_ReserveDoorForClose");
			SetTaskState(State_ReserveDoorForClose);
		}

		// Wait to stream close door anims
		return FSM_Continue;
	}

	if (CheckForStartingEngine(*m_pVehicle, *pPed))
	{
		SetTaskState(State_StartEngine);
		return FSM_Continue;
	}

	if (CTaskMotionInAutomobile::CheckForHorn())
	{
		SetTaskState(State_Horn);
		return FSM_Continue;
	}

	if (CTaskMotionInVehicle::CheckForReverse(*m_pVehicle, *pPed, IsUsingFirstPersonAnims(), false, IsPanicking()))
	{
		TUNE_GROUP_FLOAT(IN_VEHICLE_TUNE, MIN_TIME_IN_IDLE_STATE_FOR_REVERSE, 0.15f, 0.0f, 1.0f, 0.01f);
		if (pPed->IsPlayer() || GetTimeInState() > MIN_TIME_IN_IDLE_STATE_FOR_REVERSE)
		{
			SetTaskState(State_Reverse);
			return FSM_Continue;
		}
	}

	if (CheckForChangingStation(*m_pVehicle, *pPed))
	{
		SetTaskState(State_ChangeStation);
		return FSM_Continue;
	}

	if (ShouldUseSitToStillTransition(*m_pVehicle, *pPed))
	{
		if (CheckForSitToStillTransition(*m_pVehicle, *pPed))
		{
			SetTaskState(State_SitToStill);
			return FSM_Continue;
		}
	}
	else
	{
		const CVehicleSeatAnimInfo* pSeatClipInfo = m_pVehicle->GetSeatAnimationInfo(pPed);
		const float fTimeStep = GetTimeStep();
		if (pSeatClipInfo && pSeatClipInfo->GetUseBikeInVehicleAnims() && CTaskMotionInVehicle::CheckForStill(fTimeStep, *m_pVehicle, *pPed, m_fPitchAngle, m_fIdleTimer, false, true, GetTimeInState()))
		{
			if (!CTaskMotionInVehicle::CheckForMovingForwards(*m_pVehicle, *pPed))
			{
				SetTaskState(State_Still);
				return FSM_Continue;
			}
		}
	}

	//Check if the default clip set changed this frame.
	if(m_bDefaultClipSetChangedThisFrame)
	{
		bool bAllowRestart = true;
		if (m_pVehicle->IsBikeOrQuad())
		{
			if (!m_MinTimeInAltLowriderClipsetTimer.IsFinished())
			{
				bAllowRestart = false;
			}
		}

		if (bAllowRestart)
		{
			//Clear the flag.
			m_bDefaultClipSetChangedThisFrame = false;

			float fBlendDuration = GetBlendDurationForClipSetChange();

			//Restart the state.
			m_moveNetworkHelper.SetFloat(ms_RestartIdleBlendDurationId, fBlendDuration);
			SetFlag(aiTaskFlags::RestartCurrentState);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

float CTaskMotionInAutomobile::GetBlendDurationForClipSetChange() const
{
	const CPed* pPed = GetPed();
	const bool bWasDucking = m_bWasDucking || (!pPed->GetIsDrivingVehicle() && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsDuckingInVehicle));
	if (bWasDucking)
		return ms_Tunables.m_RestartIdleBlendDurationpassenger;
		
	if (m_nDefaultClipSet == DCS_LowriderLean && m_pVehicle->IsBikeOrQuad())
		return ms_Tunables.m_AltLowRiderBlendDuration;
		
	return ms_Tunables.m_RestartIdleBlendDuration;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::SitToStill_OnEnter()
{
	SetMoveNetworkState(ms_SitToStillRequestId, ms_SitToStillOnEnterId, CTaskMotionInVehicle::ms_InvalidStateId);
	SetClipsForState();
	m_fStillTransitionRate = 1.0f;
	const fwMvClipSetId clipsetId = m_moveNetworkHelper.GetClipSetId();
	const crClip* pBurnOutClip = fwClipSetManager::GetClip(clipsetId, ms_Tunables.m_BurnOutClipId);
	if (pBurnOutClip)
	{
		m_fBurnOutPhase = 0.6f;
		m_moveNetworkHelper.SetClip(pBurnOutClip, ms_BurnOutClipId);
		float fUnused;
		CClipEventTags::GetMoveEventStartAndEndPhases(pBurnOutClip, ms_BurnOutInterruptId, m_fBurnOutPhase, fUnused);
	}
	m_fBurnOutBlend = 0.0f;
	m_fTimeSinceBurnOut = -1.0f;
	m_bIsAltRidingToStillTransition = m_nDefaultClipSet == DCS_LowriderLeanAlt;
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::SitToStill_OnUpdate()
{
	CPed& rPed = *GetPed();
	rPed.SetPedResetFlag(CPED_RESET_FLAG_RequiresLegIk, true);

	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	if(CanDoInstantSwitchToFirstPersonAnims())
	{
		SetTaskState(State_FirstPersonSitIdle);
		return FSM_Continue;
	}

	const bool bDoingBurnout = CheckIfVehicleIsDoingBurnout(*m_pVehicle, rPed);
	bool bWantsToMove = m_bForceFinishTransition || CheckIfPedWantsToMove(rPed, *m_pVehicle) > 0.0f || bDoingBurnout || rPed.GetPedResetFlag(CPED_RESET_FLAG_PreventGoingIntoStillInVehicleState);
	if (bWantsToMove && (m_bIsAltRidingToStillTransition || IsDoingAWheelie(*m_pVehicle)))
	{
		m_bForceFinishTransition = true;
	}

	if (bDoingBurnout)
	{
		m_fTimeSinceBurnOut = 0.0f;
	}
	else if (m_fTimeSinceBurnOut >= 0.0f)
	{
		m_fTimeSinceBurnOut += GetTimeStep();
	}

	bool bWantsToMoveAfterBurnout = false;
	TUNE_GROUP_FLOAT(VEHICLE_TUNE, TIME_AFTER_BURNOUT_TO_IGNORE_SPEED, 0.5f, 0.0f, 1.0f, 0.01f);
	if (!bDoingBurnout)
	{
		if (m_fTimeSinceBurnOut < TIME_AFTER_BURNOUT_TO_IGNORE_SPEED)
		{
			m_fStillTransitionRate = 1.0f;
		}

		bWantsToMoveAfterBurnout = bWantsToMove;
	}

	const float fClipPhase = m_moveNetworkHelper.GetFloat(ms_SitToStillPhaseId);
	const float fVehVelocitySqd = m_pVehicle->GetVelocityIncludingReferenceFrame().Mag2();
	const float fMaxHoldLegVelSqd = square(ms_Tunables.m_HoldLegOutVelocity);
	const float fMinVelStillSqd = square(ms_Tunables.m_MinVelStillStart);
	const float fForcedLegUpVelSqd = square(ms_Tunables.m_ForcedLegUpVelocity);
	float fDesiredPhase = (bWantsToMove && !m_bForceFinishTransition) ? 0.0f : 1.0f;
	bool bIsSlow = false;

	if (!m_bForceFinishTransition)
	{
	if (!bDoingBurnout)
	{
			if ((fVehVelocitySqd < fForcedLegUpVelSqd && !bWantsToMoveAfterBurnout))
		{
			if (bWantsToMove && (fVehVelocitySqd <= fMaxHoldLegVelSqd && fVehVelocitySqd >= fMinVelStillSqd))
			{
				bIsSlow = true;
				TUNE_GROUP_FLOAT(VEHICLE_TUNE, MIN_HOLD_LEG_PHASE_2, 0.5f, 0.0f, 1.0f, 0.01f);
				TUNE_GROUP_FLOAT(VEHICLE_TUNE, MAX_HOLD_LEG_PHASE_2, 0.525f, 0.0f, 1.0f, 0.01f);
				const float fMinPhase = bWantsToMove ? MIN_HOLD_LEG_PHASE_2 : 0.0f;
				const float fHoldLegRatio = fVehVelocitySqd / fMaxHoldLegVelSqd;
				fDesiredPhase = fMinPhase * (1.0f - fHoldLegRatio) + MAX_HOLD_LEG_PHASE_2 * fHoldLegRatio;
			}
		}
		else if (fVehVelocitySqd > fForcedLegUpVelSqd)
		{
			fDesiredPhase = 0.0f;
		}
	}
	else
	{
		fDesiredPhase = m_fBurnOutPhase;
	}
	}

	float fCloseness = Abs(fClipPhase - fDesiredPhase);
	float fDesiredPlayBackRate = 1.0f;
	if (fDesiredPhase < fClipPhase)
	{
		fDesiredPlayBackRate = -1.0f;
		if (!bWantsToMoveAfterBurnout)
		{
			m_fStillTransitionRate = 0.0f;
		}
		else
		{
			TUNE_GROUP_FLOAT(VEHICLE_TUNE, BURNOUT_MOVE_RATE, 0.5f, 0.0f, 1.0f, 0.01f);
			m_fStillTransitionRate = fDesiredPlayBackRate * BURNOUT_MOVE_RATE;
		}
	}

	TUNE_GROUP_FLOAT(VEHICLE_TUNE, SIT_CLOSE_PHASE_TOL, 0.1f, 0.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_TUNE, SIT_TO_STILL_APPROACH_RATE_CLOSE, 0.2f, 0.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_TUNE, SIT_TO_STILL_APPROACH_RATE_SLOW, 1.0f, 0.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_TUNE, SIT_TO_STILL_APPROACH_RATE, 2.0f, 0.0f, 10.0f, 0.01f);
	float fApproachRate = bIsSlow ? SIT_TO_STILL_APPROACH_RATE_SLOW : SIT_TO_STILL_APPROACH_RATE;
	if (fCloseness < SIT_CLOSE_PHASE_TOL)
	{
		fApproachRate = SIT_TO_STILL_APPROACH_RATE_CLOSE;
	}

	Approach(m_fStillTransitionRate, fDesiredPlayBackRate, fApproachRate, GetTimeStep());
	m_moveNetworkHelper.SetFloat(ms_SitToStillRate, m_fStillTransitionRate);

	if (bDoingBurnout)
	{
		if (Abs(fClipPhase - fDesiredPhase) < ms_Tunables.m_BurnOutBlendInTol)
		{
			Approach(m_fBurnOutBlend, 1.0f, ms_Tunables.m_BurnOutBlendInSpeed, GetTimeStep());
		}
	}
	else
	{
		Approach(m_fBurnOutBlend, 0.0f, ms_Tunables.m_BurnOutBlendOutSpeed, GetTimeStep());
	}
	m_moveNetworkHelper.SetFloat(ms_BurnOutBlendId, m_fBurnOutBlend);

	TUNE_GROUP_FLOAT(VEHICLE_TUNE, SIT_TO_STILL_BLEND_PHASE, 1.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_TUNE, SIT_TO_STILL_IDLE_BLEND_PHASE, 0.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_TUNE, STILL_BLEND_PHASE, 1.0f, 0.0f, 1.0f, 0.01f);
	if (!bDoingBurnout)
	{
		const bool bWantsToBeStill = !bWantsToMove || m_bForceFinishTransition;
		const bool bWantsToBeSitting = bWantsToMove && !m_bForceFinishTransition;
		if (bWantsToBeStill && fClipPhase >= STILL_BLEND_PHASE)
		{
			SetTaskState(State_Still);
			return FSM_Continue;
		}
		else if (bWantsToBeSitting && fClipPhase <= SIT_TO_STILL_IDLE_BLEND_PHASE)
		{
			SetTaskState(GetSitIdleState());
			return FSM_Continue;
		}
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskMotionInAutomobile::SitToStill_OnExit()
{
	m_bIsAltRidingToStillTransition = false;
	m_bForceFinishTransition = false;
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::Reverse_OnEnter()
{
	m_fIdleTimer = 0.0f;
	SetMoveNetworkState(CTaskMotionInVehicle::ms_ReverseRequestId, CTaskMotionInVehicle::ms_ReverseOnEnterId, CTaskMotionInVehicle::ms_InvalidStateId);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::Reverse_OnUpdate()
{
	CPed* pPed = GetPed();

	if (m_pVehicle->InheritsFromBike())
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_RequiresLegIk, true);
	}

	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	if(CanDoInstantSwitchToFirstPersonAnims())
	{
		SetTaskState(State_FirstPersonSitIdle);
		return FSM_Continue;
	}

	const float fTimeStep = GetTimeStep();
	const CVehicleSeatAnimInfo* pSeatClipInfo = m_pVehicle->GetSeatAnimationInfo(pPed);
	if (pSeatClipInfo && pSeatClipInfo->GetUseBikeInVehicleAnims() && CTaskMotionInVehicle::CheckForStill(fTimeStep, *m_pVehicle, *pPed, m_fPitchAngle, m_fIdleTimer, true))
	{
		SetTaskState(State_Still);
		return FSM_Continue;
	}
	else if (CTaskMotionInVehicle::CheckForShunt(*m_pVehicle, *pPed, m_vCurrentVehicleAcceleration.GetZf()))
	{
		SetTaskState(State_Shunt);
		return FSM_Continue;
	}

	TUNE_GROUP_FLOAT(IN_VEHICLE_TUNE, MIN_TIME_IN_REVERSE_STATE_FOR_IDLE, 0.15f, 0.0f, 1.0f, 0.01f);
	if (pPed->IsPlayer() || GetTimeInState() > MIN_TIME_IN_REVERSE_STATE_FOR_IDLE)
	{
		if (m_pVehicle->InheritsFromBike())
		{
			if (CTaskMotionInVehicle::CheckForMovingForwards(*m_pVehicle, *pPed))
			{
				SetTaskState(GetSitIdleState());
				return FSM_Continue;
			}
		}
		else
		{
			if (!CTaskMotionInVehicle::CheckForReverse(*m_pVehicle, *pPed, IsUsingFirstPersonAnims(), false, IsPanicking()))
			{
				SetTaskState(GetSitIdleState());
				return FSM_Continue;
			}
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::ReserveDoorForClose_OnUpdate()
{
	CPed* pPed = GetPed();

	if(pPed->IsNetworkClone())
	{
		// clone peds don't need to reserve components, as they will have
		// reserved them on their local machine

		// still need to know if the door is being manipulated
		if (m_pVehicle.Get())
		{
			vehicledoorDebugf2("CTaskMotionInAutomobile::ReserveDoorForClose_OnUpdate--IsNetworkClone-->SetPedRemoteUsingDoor(true)");
			const s32 iDirectEntryPoint = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed), m_pVehicle);
			CTaskVehicleFSM::SetRemotePedUsingDoorsTrue(*pPed, m_pVehicle, iDirectEntryPoint);
		}

		vehicledoorDebugf2("CTaskMotionInAutomobile::ReserveDoorForClose_OnUpdate--IsNetworkClone-->Set State_CloseDoor");
		SetTaskState(State_CloseDoor);
		return FSM_Continue;
	}
	else
	{
		CVehicle* pVehicle = m_pVehicle;
		s32 iSeatIndex = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
		if (iSeatIndex > -1)
		{
			// Lookup the entry point from our seat
			s32 iDirectEntryPoint = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iSeatIndex, pVehicle);

			if (taskVerifyf(iDirectEntryPoint>=0, "Entry point index invalid"))
			{
				// Unreserve the door
				CComponentReservation* pComponentReservation = pVehicle->GetComponentReservationMgr()->GetDoorReservation(iDirectEntryPoint);

				if(CComponentReservationManager::ReserveVehicleComponent(pPed, pVehicle, pComponentReservation))
				{
					vehicledoorDebugf2("CTaskMotionInAutomobile::ReserveDoorForClose_OnUpdate-->Set State_CloseDoor");
					SetTaskState(State_CloseDoor);
					return FSM_Continue;
				}
			}
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::CloseDoor_OnEnter()
{
	SetEntryClipSet();

	CPed* pPed = GetPed();
	const CVehicleSeatAnimInfo* pSeatClipInfo = m_pVehicle->GetSeatAnimationInfo(pPed);
	taskFatalAssertf(pSeatClipInfo, "NULL Seat clip Info");

	m_bDontUseArmIk = false;
	const s32 iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
	if (m_pVehicle->IsSeatIndexValid(iSeatIndex))
	{
		if( pPed == m_pVehicle->GetDriver() )
		{
			m_pVehicle->SetRemoteDriverDoorClosing(true);
		}

		s32 iEntryIndex = m_pVehicle->GetDirectEntryPointIndexForSeat(iSeatIndex);
		const bool bTwoDoorsOneSeat = m_pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_TWO_DOORS_ONE_SEAT);
		const CCarDoor* pDoor = NULL;
		if (bTwoDoorsOneSeat)
		{
			pDoor = CTaskMotionInVehicle::GetOpenDoorForDoubleDoorSeat(*m_pVehicle, iSeatIndex, iEntryIndex);
		}
		else
		{
			pDoor = CTaskMotionInVehicle::GetDirectAccessEntryDoorFromPed(*m_pVehicle, *pPed);
		}
		if (m_pVehicle->IsEntryIndexValid(iEntryIndex))
		{
			if (pDoor)
			{
				if (pSeatClipInfo->GetUseCloseDoorBlendAnims())
				{
					m_moveNetworkHelper.SetFlag(true, CTaskCloseVehicleDoorFromInside::ms_UseCloseDoorBlendId);
					m_moveNetworkHelper.SetFloat(CTaskCloseVehicleDoorFromInside::ms_CloseDoorLengthId, pDoor->GetDoorRatio());
				}
	
				if (pDoor->GetDoorRatio() < CTaskCloseVehicleDoorFromInside::ms_Tunables.m_MinOpenDoorRatioToUseArmIk)
				{
					m_bDontUseArmIk = true;
				}
			}
		}
	}

	m_bTurnedOnDoorHandleIk = false;
	m_bTurnedOffDoorHandleIk = false;
	m_bCurrentAnimFinished = false;
	RequestProcessMoveSignalCalls();
	SetMoveNetworkState(CTaskMotionInVehicle::ms_CloseDoorRequestId, CTaskMotionInVehicle::ms_CloseDoorOnEnterId, CTaskMotionInVehicle::ms_InvalidStateId);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::CloseDoor_OnUpdate()
{
	CPed* pPed = GetPed();
	pPed->SetPedResetFlag(CPED_RESET_FLAG_IsClosingVehicleDoor, true);

	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	const s32 iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
	s32 iEntryPointIndex = m_pVehicle->GetDirectEntryPointIndexForSeat(iSeatIndex);
	const bool bTwoDoorsOneSeat = m_pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_TWO_DOORS_ONE_SEAT);
	const CCarDoor* pDoor = NULL;
	if (bTwoDoorsOneSeat)
	{
		pDoor = CTaskMotionInVehicle::GetOpenDoorForDoubleDoorSeat(*m_pVehicle, iSeatIndex, iEntryPointIndex);
	}
	else
	{
		pDoor = CTaskMotionInVehicle::GetDirectAccessEntryDoorFromPed(*m_pVehicle, *pPed);
	}

	if (!m_pVehicle->IsEntryIndexValid(iEntryPointIndex))
	{
		SetTaskState(GetSitIdleState());
		return FSM_Continue;
	}

	if (!pDoor || !pDoor->GetIsIntact(m_pVehicle))
	{
		SetTaskState(GetSitIdleState());
		return FSM_Continue;
	}

	m_moveNetworkHelper.SetFloat(CTaskCloseVehicleDoorFromInside::ms_CloseDoorLengthId, pDoor->GetDoorRatio());

	if (CTaskMotionInVehicle::CheckForShunt(*m_pVehicle, *pPed, m_vCurrentVehicleAcceleration.GetZf()))
	{
		m_bCloseDoorSucceeded = false;
		SetTaskState(State_Shunt);
		return FSM_Continue;
	}

	bool bAbortCloseDoor = pPed->GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle) || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsDoingDriveby);

	// If any player stands on the luxury plane door, hold at current position until that player walks away
	const bool bMiljet = MI_PLANE_MILJET.IsValid() && (m_pVehicle->GetModelIndex() == MI_PLANE_MILJET);
	const bool bLuxor2 = MI_PLANE_LUXURY_JET3.IsValid() && (m_pVehicle->GetModelIndex() == MI_PLANE_LUXURY_JET3);
	const bool bNimbus = MI_PLANE_NIMBUS.IsValid() && (m_pVehicle->GetModelIndex() == MI_PLANE_NIMBUS);
	const bool bBombushka = MI_PLANE_BOMBUSHKA.IsValid() && (m_pVehicle->GetModelIndex() == MI_PLANE_BOMBUSHKA);
	const bool bVolatol = MI_PLANE_VOLATOL.IsValid() && (m_pVehicle->GetModelIndex() == MI_PLANE_VOLATOL);
	bAbortCloseDoor |= (m_pVehicle->GetModelIndex() == MI_PLANE_LUXURY_JET || m_pVehicle->GetModelIndex() == MI_PLANE_LUXURY_JET2 || bMiljet || bLuxor2 || bNimbus || bBombushka || bVolatol) && pDoor->GetDoesPlayerStandOnDoor();

	if (!bAbortCloseDoor)
	{
		const CVehicleSeatAnimInfo* pSeatClipInfo = m_pVehicle->GetSeatAnimationInfo(pPed);
		taskFatalAssertf(pSeatClipInfo, "NULL Seat clip Info");
		bool bTurnedOnDoorHandleIk = m_bTurnedOnDoorHandleIk;
		bool bTurnedOffDoorHandleIk = m_bTurnedOffDoorHandleIk;
		if (pSeatClipInfo->GetUseCloseDoorBlendAnims())
		{
			bAbortCloseDoor = !CTaskCloseVehicleDoorFromInside::ProcessCloseDoorBlend(m_moveNetworkHelper, *m_pVehicle, *pPed, iEntryPointIndex, bTurnedOnDoorHandleIk, bTurnedOffDoorHandleIk, m_bDontUseArmIk);
		}
		else
		{
			bAbortCloseDoor = !CTaskCloseVehicleDoorFromInside::ProcessCloseDoor(m_moveNetworkHelper, *m_pVehicle, *pPed, iEntryPointIndex, bTurnedOnDoorHandleIk, bTurnedOffDoorHandleIk, m_bDontUseArmIk);
		}
		m_bTurnedOnDoorHandleIk = bTurnedOnDoorHandleIk;
		m_bTurnedOffDoorHandleIk = bTurnedOffDoorHandleIk;
	}

	// Wait until the close door clip has finished
	if (m_bCurrentAnimFinished)
	{
		SetSitIdleMoveNetworkState();
		SetTaskState(GetSitIdleState());
		return FSM_Continue;
	}

	// Abort if the door ratio is smaller than what we expect it to be
	if (bAbortCloseDoor)
	{
		TUNE_GROUP_FLOAT(IN_VEHICLE_TUNE, ABORT_CLOSE_DOOR_BLEND_DURATION, 0.15f, 0.0f, 10.0f, 0.01f);
		m_moveNetworkHelper.SetFloat(ms_IdleBlendDurationId, ABORT_CLOSE_DOOR_BLEND_DURATION);
		SetSitIdleMoveNetworkState();
		SetTaskState(GetSitIdleState());
		return FSM_Continue;
	}

	// Wait until its closed
	if (!pDoor->GetIsClosed())
		return FSM_Continue;

	const float fAbsThrottle = Abs(rage::Clamp(m_pVehicle->GetThrottle() -  m_pVehicle->GetBrake(), 0.0f, 1.0f));
	if (fAbsThrottle > CTaskMotionInVehicle::ms_Tunables.m_MaxAbsThrottleForCloseDoor || (!pPed->GetIsDrivingVehicle() && m_pVehicle->GetVelocityIncludingReferenceFrame().Mag2() >= rage::square(CTaskCloseVehicleDoorFromInside::ms_Tunables.m_VehicleSpeedToAbortCloseDoor)))
	{
		SetTaskState(GetSitIdleState());
		return FSM_Continue;
	}

	if (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE, true))
	{
		SetSitIdleMoveNetworkState();
		SetTaskState(GetSitIdleState());
		return FSM_Continue;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::CloseDoor_OnExit()
{
	CPed* pPed = GetPed();

	if( m_pVehicle )
	{
		if( pPed == m_pVehicle->GetDriver() )
		{
			m_pVehicle->SetRemoteDriverDoorClosing(false);
		}

		// still need to know if the door is being manipulated
		if (pPed && pPed->IsNetworkClone())
		{
			vehicledoorDebugf2("CTaskMotionInAutomobile::CloseDoor_OnExit--IsNetworkClone-->SetPedRemoteUsingDoor(false)");
			const s32 iDirectEntryPoint = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed), m_pVehicle);
			CTaskVehicleFSM::SetRemotePedUsingDoorsFalse(*pPed, m_pVehicle, iDirectEntryPoint);

			if (m_pVehicle->IsNetworkClone() && (m_pVehicle->GetDriver() == pPed))
			{
				vehicledoorDebugf2("CTaskMotionInAutomobile::CloseDoor_OnExit--driver-->SetRemoteDriverDoorClosing(false)");
				m_pVehicle->SetRemoteDriverDoorClosing(false);
			}
		}

		s32 iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);

		if (m_pVehicle->IsSeatIndexValid(iSeatIndex))
		{
			// Lookup the entry point from our seat
			s32 iDirectEntryPoint = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iSeatIndex, m_pVehicle);

			if( taskVerifyf(iDirectEntryPoint>=0, "Entry point index invalid") )
			{
				CTaskVehicleFSM::ClearDoorDrivingFlags(m_pVehicle, iDirectEntryPoint);

				taskAssert(m_pVehicle->GetVehicleModelInfo());
				const CEntryExitPoint* pEntryPoint = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryExitPoint(iDirectEntryPoint);
				if( taskVerifyf(pEntryPoint, "Entry point invalid") )
				{
					CCarDoor* pDoor = m_pVehicle->GetDoorFromBoneIndex(pEntryPoint->GetDoorBoneIndex());

					if (pDoor)
					{
						if (m_bCurrentAnimFinished || m_bCloseDoorSucceeded)
						{
							// close driver door if it is open
							if (pDoor->GetIsIntact(m_pVehicle) && !pDoor->GetIsLatched(m_pVehicle))
							{
								pDoor->SetShutAndLatched(m_pVehicle);
							}
						}
					}
				}

				// Unreserve the door
				CComponentReservation* pComponentReservation = m_pVehicle->GetComponentReservationMgr()->GetDoorReservation(iDirectEntryPoint);
				if (pComponentReservation)
				{
					CComponentReservationManager::UnreserveVehicleComponent(pPed, pComponentReservation);
				}
			}
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::StartEngine_OnEnter()
{
	SetMoveNetworkState(CTaskMotionInVehicle::ms_StartEngineRequestId, CTaskMotionInVehicle::ms_StartEngineOnEnterId, CTaskMotionInVehicle::ms_InvalidStateId);
	SetClipPlaybackRate();

#if FPS_MODE_SUPPORTED
	fwMvClipSetId clipSetId;
	fwMvClipId clipId;
	crClip* pStartEngineClip;

	bool bFPContextSet = false;
	if(m_pVehicle)
	{
		s32 viewMode = camControlHelper::ComputeViewModeContextForVehicle(*m_pVehicle);
		if(viewMode > -1 ) 
		{
			if(camControlHelperMetadataViewMode::FIRST_PERSON == camControlHelper::GetViewModeForContext(viewMode))
			{
				bFPContextSet = true;
			}
		}
	}
	
	camFirstPersonShooterCamera* bFirstPersonView = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();
	if(GetPed()->IsLocalPlayer() && (bFirstPersonView || GetPed()->IsInFirstPersonVehicleCamera() || bFPContextSet))
	{
		// Get the first person version of the anim
		clipSetId = GetDefaultClipSetId(DCS_Idle);
		clipId = ms_Tunables.m_FirstPersonStartEngineClipId;
		pStartEngineClip = fwClipSetManager::GetClip(clipSetId, clipId);

		// If there is no clip for this anim, fallback to the 3rd person version
		if(pStartEngineClip == NULL)
		{
			clipSetId = m_moveNetworkHelper.GetClipSetId();
			clipId = ms_Tunables.m_StartEngineClipId;
			pStartEngineClip = fwClipSetManager::GetClip(clipSetId, clipId);
		}
	}
	else
	{
		// If we are not on first person view just use the 3rd person anims
		clipSetId = m_moveNetworkHelper.GetClipSetId();
		clipId = ms_Tunables.m_StartEngineClipId;
		pStartEngineClip = fwClipSetManager::GetClip(clipSetId, clipId);
	}
#else
	// Get the 3rd person version of the anims
	const fwMvClipSetId clipSetId = m_moveNetworkHelper.GetClipSetId();
	const fwMvClipId clipId = ms_Tunables.m_StartEngineClipId;
	const crClip* pStartEngineClip = fwClipSetManager::GetClip(clipSetId, clipId);
#endif

	if (taskVerifyf(pStartEngineClip, "Couldn't find %s clip in clipset %s", clipId.GetCStr(), clipSetId.GetCStr()))
	{
		CClipEventTags::FindEventPhase(pStartEngineClip, CClipEventTags::StartEngine, m_fEngineStartPhase);
		m_moveNetworkHelper.SetClip(pStartEngineClip, ms_StartEngineClipId);

		const CPed& rPed = *GetPed();
		if (m_pVehicle->InheritsFromBike() && rPed.GetPedResetFlag(CPED_RESET_FLAG_InterruptedToQuickStartEngine))
		{	
			fwMvBooleanId kickStartTagId = rPed.GetPedResetFlag(CPED_RESET_FLAG_PedEnteredFromLeftEntry) ? ms_CanKickStartDSId : ms_CanKickStartPSId;
			float fStartPhase = 0.0f;
			float fEndPhase = 1.0f;

			if (CClipEventTags::GetMoveEventStartAndEndPhases(pStartEngineClip, kickStartTagId, fStartPhase, fEndPhase))
			{		
				const float fBlendDuration = fEndPhase - fStartPhase;
				taskAssertf(fBlendDuration >= 0.0f, "Blend duration wasn't positive, check kickstart tags");
				// Set initial phase and blend in duration
				m_moveNetworkHelper.SetFloat(ms_StartEnginePhaseId, fStartPhase);
				m_moveNetworkHelper.SetFloat(ms_StartEngineBlendDurationId, fBlendDuration); 
			}
		}
	}

	// Let ProcessMoveSignals() handle the communication with the Move network.
	m_bCurrentAnimFinished = false;
	RequestProcessMoveSignalCalls();
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::StartEngine_OnUpdate()
{
	CPed& ped = *GetPed();
	
	if (m_pVehicle->InheritsFromBike())
	{
		ped.SetPedResetFlag(CPED_RESET_FLAG_RequiresLegIk, true);
	}

	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	if (ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE))
	{
		SetTaskState(GetSitIdleState());
		return FSM_Continue;
	}

	const bool bVehicleDriveable = IsVehicleDrivable(*m_pVehicle) && m_pVehicle->GetVehicleDamage()->GetEngineHealth() > ENGINE_DAMAGE_ONFIRE;

	if (m_bCurrentAnimFinished)
	{
		if (!m_pVehicle->m_nVehicleFlags.bEngineOn) 
		{
			const bool bNoDelay = CPlayerInfo::AreCNCResponsivenessChangesEnabled(&ped) ? true : false;
			m_pVehicle->SwitchEngineOn(bNoDelay, true, !bVehicleDriveable);
		}

		float fUnused = -1.0f;
		const CVehicleSeatAnimInfo* pSeatClipInfo = m_pVehicle->GetSeatAnimationInfo(&ped);
		if (pSeatClipInfo->GetUseBikeInVehicleAnims() && CTaskMotionInVehicle::CheckForStill(GetTimeStep(), *m_pVehicle, ped, m_fPitchAngle, fUnused, true))
		{
			SetState(State_Still);
			return FSM_Continue;
		}

		SetTaskState(GetSitIdleState());
		return FSM_Continue;
	}

	const float fPhase = m_moveNetworkHelper.GetFloat(ms_StartEnginePhaseId);
	if (fPhase >= 0.0f)
	{
		CTaskVehicleFSM::ProcessApplyForce(m_moveNetworkHelper, *m_pVehicle, ped);
	}

	if (!m_pVehicle->m_nVehicleFlags.bEngineStarting)
	{
		bool bStartEngine = false;

		// Wait until we pass the engine start event phase before starting the engine
		if (fPhase >= 0.0f)
		{
			dev_float s_fDefaultStartEngineTime = 0.6f;

			// Always use the event phase if we found it
			if (m_fEngineStartPhase > 0.0f && fPhase >= m_fEngineStartPhase)
			{
				bStartEngine = true;
			}
			else if (GetTimeInState() >= s_fDefaultStartEngineTime)
			{
				bStartEngine = true;
			}
		}

		if (bStartEngine)
		{
			TUNE_GROUP_BOOL(TASK_IN_VEHICLE_TUNE, NO_DELAY, false);
			const bool bNoDelay = CPlayerInfo::AreCNCResponsivenessChangesEnabled(&ped) ? true : NO_DELAY;
			m_pVehicle->SwitchEngineOn(bNoDelay, true, !bVehicleDriveable);
			if (m_pVehicle->InheritsFromAutomobile() && CTaskMotionInAutomobile::WantsToDuck(ped))
			{
				SetTaskState(GetSitIdleState());
				return FSM_Continue;
			}
		}
	}
	
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::Shunt_OnEnter()
{
	Vec3V vVehFor = m_pVehicle->GetTransform().GetB();
	Vec3V vCollNormal = -vVehFor;

	const CCollisionHistory* pCollisionHistory = m_pVehicle->GetFrameCollisionHistory();
	const CCollisionRecord* pRecord = pCollisionHistory->GetMostSignificantCollisionRecord();
	if (pRecord)
	{
		vCollNormal = RCC_VEC3V(pRecord->m_MyCollisionNormal);
#if DEBUG_DRAW
		Vec3V vVehPos = m_pVehicle->GetTransform().GetPosition() + Vec3V(V_Z_AXIS_WZERO);
		CTask::ms_debugDraw.AddArrow(vVehPos, vVehPos + vCollNormal, 0.025f, Color_red, 2000);
		CTask::ms_debugDraw.AddArrow(vVehPos, vVehPos + vVehFor, 0.025f, Color_blue, 2000);
#endif // DEBUG_DRAW
	}

 	const float fCollisionDot = Dot(vCollNormal, vVehFor).Getf(); 
	m_moveNetworkHelper.SetFlag(false, CTaskMotionInVehicle::ms_BackFlagId);
	m_moveNetworkHelper.SetFlag(false, CTaskMotionInVehicle::ms_RightFlagId);
	m_moveNetworkHelper.SetFlag(false, CTaskMotionInVehicle::ms_LeftFlagId);
	m_moveNetworkHelper.SetFlag(false, CTaskMotionInVehicle::ms_FrontFlagId);
 	
	//Displayf("Collision Dot: %.4f", fCollisionDot);
	if (fCollisionDot > 0.707f) // < 45o
	{
		m_moveNetworkHelper.SetFlag(true, CTaskMotionInVehicle::ms_BackFlagId);
	}
	else if (fCollisionDot > -0.707f) // < 135o
	{
		const float fCrossZ = Cross(vCollNormal, vVehFor).GetZf();
		//Displayf("Collision Cross Vehicle Forward Z: %.4f", fCrossZ);
		if (fCrossZ < 0.0f)
		{
			m_moveNetworkHelper.SetFlag(true, CTaskMotionInVehicle::ms_RightFlagId);
		}
		else
		{
			m_moveNetworkHelper.SetFlag(true, CTaskMotionInVehicle::ms_LeftFlagId);
		}
	}
	else // >= 135o
	{
		m_moveNetworkHelper.SetFlag(true, CTaskMotionInVehicle::ms_FrontFlagId);
	}

	//Generate a varied rate.
	float fRate = fwRandom::GetRandomNumberInRange(CTaskMotionInVehicle::ms_Tunables.m_MinRateForInVehicleAnims, CTaskMotionInVehicle::ms_Tunables.m_MaxRateForInVehicleAnims);
	m_moveNetworkHelper.SetFloat(ms_InVehicleAnimsRateId, fRate);

	SetMoveNetworkState(CTaskMotionInVehicle::ms_ShuntRequestId, CTaskMotionInVehicle::ms_ShuntOnEnterId, CTaskMotionInVehicle::ms_InvalidStateId);

	// Let ProcessMoveSignals() handle the communication with the Move network.
	m_bCurrentAnimFinished = false;
	RequestProcessMoveSignalCalls();

	m_MinTimeInAltLowriderClipsetTimer.ForceFinish();
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::Shunt_OnUpdate()
{
	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	if(CanDoInstantSwitchToFirstPersonAnims())
	{
		SetTaskState(State_FirstPersonSitIdle);
		return FSM_Continue;
	}

	if (GetTimeInState() > CTaskMotionInVehicle::ms_Tunables.m_MinTimeInShuntStateBeforeRestart)
	{
		if (CTaskMotionInVehicle::CheckForShunt(*m_pVehicle, *GetPed(), m_vCurrentVehicleAcceleration.GetZf()))
		{
			SetFlag(aiTaskFlags::RestartCurrentState);
			return FSM_Continue;
		}
		if (m_bCurrentAnimFinished)
		{
			SetTaskState(GetSitIdleState());
			return FSM_Continue;
		}
	}
	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::ChangeStation_OnEnter()
{
	SetMoveNetworkState(ms_ChangeStationRequestId, ms_ChangeStationOnEnterId, CTaskMotionInVehicle::ms_InvalidStateId);
	SetClipsForState();

	// Let ProcessMoveSignals() handle the communication with the Move network.
	m_bCurrentAnimFinished = false;
	RequestProcessMoveSignalCalls();
	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::ChangeStation_OnUpdate()
{
	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	CPed* pPed = GetPed();

	if(CanDoInstantSwitchToFirstPersonAnims())
	{
		SetTaskState(State_FirstPersonSitIdle);
		return FSM_Continue;
	}

	if (CTaskMotionInVehicle::CheckForShunt(*m_pVehicle, *pPed, m_vCurrentVehicleAcceleration.GetZf()))
	{
		SetTaskState(State_Shunt);
		return FSM_Continue;
	}

	const bool bShouldLoopChangeStation = CheckForChangingStation(*m_pVehicle, *pPed);
	m_moveNetworkHelper.SetBoolean(ms_ChangeStationShouldLoopId, bShouldLoopChangeStation);

	if (m_bCurrentAnimFinished)
	{
		SetTaskState(GetSitIdleState());
		return FSM_Continue;
	}

	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////

void CTaskMotionInAutomobile::SetHotwireClip() 
{
#if FPS_MODE_SUPPORTED
	fwMvClipSetId clipSetId;
	fwMvClipId clipId;
	crClip* pHotwireClip;

	bool bFPContextSet = false;
	if(m_pVehicle)
	{
		s32 viewMode = camControlHelper::ComputeViewModeContextForVehicle(*m_pVehicle);
		if(viewMode > -1 ) 
		{
			if(camControlHelperMetadataViewMode::FIRST_PERSON == camControlHelper::GetViewModeForContext(viewMode))
			{
				bFPContextSet = true;
			}
		}
	}

	camFirstPersonShooterCamera* bFirstPersonView = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();
	if(GetPed()->IsLocalPlayer() && (bFirstPersonView || GetPed()->IsInFirstPersonVehicleCamera() || bFPContextSet))
	{
		// Get the first person version of the anim
		clipSetId = GetDefaultClipSetId(DCS_Idle);
		clipId = ms_Tunables.m_FirstPersonHotwireClipId;
		pHotwireClip = fwClipSetManager::GetClip(clipSetId, clipId);

		// If there is no clip for this anim, fallback to the 3rd person version
		if(pHotwireClip == NULL)
		{
			clipSetId = m_moveNetworkHelper.GetClipSetId();
			clipId = ms_Tunables.m_HotwireClipId;
			pHotwireClip = fwClipSetManager::GetClip(clipSetId, clipId);
		}
	}
	else
	{
		// If we are not on first person view just use the 3rd person anims
		clipSetId = m_moveNetworkHelper.GetClipSetId();
		clipId = ms_Tunables.m_HotwireClipId;
		pHotwireClip = fwClipSetManager::GetClip(clipSetId, clipId);
	}
#else
	// Get the 3rd person version of the anims
	const fwMvClipSetId clipSetId = m_moveNetworkHelper.GetClipSetId();
	const fwMvClipId clipId = ms_Tunables.m_HotwireClipId;
	const crClip* pHotwireClip = fwClipSetManager::GetClip(clipSetId, clipId);
#endif

	// Set the clip
	m_moveNetworkHelper.SetClip(pHotwireClip, ms_InputHotwireClipId);
}

///////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::Hotwiring_OnEnter()
{
	SetMoveNetworkState(ms_HotwiringRequestId, ms_HotwiringOnEnterId, CTaskMotionInVehicle::ms_InvalidStateId);

	// Let ProcessMoveSignals() handle the communication with the Move network.
	m_bCurrentAnimFinished = false;
	RequestProcessMoveSignalCalls();
	SetHotwireClip();
	SetClipPlaybackRate();
	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::Hotwiring_OnUpdate()
{
	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	const crClip* pClip = m_moveNetworkHelper.GetClip(ms_InputHotwireClipId);
	if (m_fEngineStartPhase < 0.0f)
	{
		m_fEngineStartPhase = 1.0f; // Don't recheck
		if (!taskVerifyf(CClipEventTags::HasMoveEvent(pClip, ms_StartEngineId.GetHash()), "%s from clipset %s doesn't have StartEngine move event, please add a bug to Default anim ingame", m_moveNetworkHelper.GetClipSetId().GetCStr(), ms_HotwireClipId.GetCStr()))
		{
			m_pVehicle->SwitchEngineOn(true);
			m_pVehicle->m_nVehicleFlags.bCarNeedsToBeHotwired = false;
		}
	}

	if (!m_pVehicle->m_nVehicleFlags.bEngineOn && m_moveNetworkHelper.GetBoolean(ms_StartEngineId))
	{
		const bool bNoDelay = CPlayerInfo::AreCNCResponsivenessChangesEnabled(GetPed()) ? true : false;
		m_pVehicle->SwitchEngineOn(bNoDelay);
		m_pVehicle->m_nVehicleFlags.bCarNeedsToBeHotwired = false;
	}

	if (m_pVehicle->InheritsFromAutomobile() && (m_pVehicle->m_nVehicleFlags.bEngineOn || m_pVehicle->m_nVehicleFlags.bEngineStarting) && CTaskMotionInAutomobile::WantsToDuck(*GetPed()))
	{
		m_moveNetworkHelper.SetBoolean(ms_HotwiringClipFinishedId, true);	// Need to do this as transition isn't request based
		SetTaskState(GetSitIdleState());
		return FSM_Continue;
	}

	if (m_bCurrentAnimFinished)
	{
		m_pVehicle->SwitchEngineOn(true);
		m_pVehicle->m_nVehicleFlags.bCarNeedsToBeHotwired = false;
		
		SetTaskState(GetSitIdleState());

		return FSM_Continue;
	}
	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////

#if __ASSERT
static dev_float s_StuckTime = 5.0f;
#endif // __ASSERT

CTask::FSM_Return CTaskMotionInAutomobile::HeavyBrake_OnEnter()
{
	//Generate a varied rate.
	float fRate = fwRandom::GetRandomNumberInRange(CTaskMotionInVehicle::ms_Tunables.m_MinRateForInVehicleAnims, CTaskMotionInVehicle::ms_Tunables.m_MaxRateForInVehicleAnims);
	m_moveNetworkHelper.SetFloat(ms_InVehicleAnimsRateId, fRate);

	SetMoveNetworkState(CTaskMotionInVehicle::ms_HeavyBrakeRequestId, CTaskMotionInVehicle::ms_HeavyBrakeOnEnterId, CTaskMotionInVehicle::ms_InvalidStateId);

	// Let ProcessMoveSignals() handle the communication with the Move network.
	m_bCurrentAnimFinished = false;
	RequestProcessMoveSignalCalls();

#if __ASSERT
	m_TaskStateTimer.Reset(s_StuckTime);
#endif // __ASSERT
	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::HeavyBrake_OnUpdate()
{
#if __ASSERT
	if (m_TaskStateTimer.Tick())
	{
		taskWarningf("Frame : %i, Ped %s (%p) stuck in heavy brake for more than %f seconds (vehicle = %s seat ID = %d) network state valid ? %s, previous state %s", 
			fwTimer::GetFrameCount(), 
			GetPed()->GetModelName(), 
			GetPed(), 
			s_StuckTime,
			m_pVehicle->GetModelName(),
			m_pVehicle->GetSeatManager()->GetPedsSeatIndex(GetPed()),
			IsMoveNetworkStateValid() ? "TRUE" : "FALSE", 
			GetStaticStateName(GetPreviousState()));
	}
#endif // __ASSERT

	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	if(CanDoInstantSwitchToFirstPersonAnims())
	{
		SetTaskState(State_FirstPersonSitIdle);
		return FSM_Continue;
	}

	if (CTaskMotionInVehicle::CheckForShunt(*m_pVehicle, *GetPed(), m_vCurrentVehicleAcceleration.GetZf()))
	{
		SetTaskState(State_Shunt);
		return FSM_Continue;
	}

	if (m_bCurrentAnimFinished)
	{
		SetTaskState(GetSitIdleState());
		return FSM_Continue;
	}

	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::Horn_OnEnter()
{
	SetMoveNetworkState(CTaskMotionInVehicle::ms_HornRequestId, CTaskMotionInVehicle::ms_HornOnEnterId, CTaskMotionInVehicle::ms_InvalidStateId);
	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionInAutomobile::Horn_OnUpdate()
{
	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	if(CanDoInstantSwitchToFirstPersonAnims())
	{
		SetTaskState(State_FirstPersonSitIdle);
		return FSM_Continue;
	}

	if (GetTimeInState() >= ms_Tunables.m_MinTimeInHornState && !CheckForHorn())
	{
		SetTaskState(GetSitIdleState());
		return FSM_Continue;
	}

	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////

#if !__FINAL

void CTaskMotionInAutomobile::Debug() const
{
#if __BANK
	const CPed& rPed = *GetPed();
	if (!rPed.IsLocalPlayer())
		return;

	if (m_pVehicle && (m_pVehicle->InheritsFromBike() || m_pVehicle->InheritsFromQuadBike() || m_pVehicle->InheritsFromAmphibiousQuadBike()))
	{
		TUNE_GROUP_BOOL(BIKE_DEBUG, RENDER_POV_BIKE_DEBUG, true);
		TUNE_GROUP_INT(BIKE_DEBUG, XOFFSET, 150, -200, 200, 1);;

		if (RENDER_POV_BIKE_DEBUG && m_pBikeAnimParams)
		{
			s32 iNoTexts = 0;
			char szText[128];
			Vec3V vDefaultPos = rPed.GetTransform().GetPosition() + Vec3V(0.0f, 0.0f, 1.0f);
			formatf(szText, "State : %s", GetStaticStateName(GetState()));
			grcDebugDraw::Text(vDefaultPos, Color_green, XOFFSET, grcDebugDraw::GetScreenSpaceTextHeight() *iNoTexts++, szText);
			formatf(szText, "m_fBodyLeanX : %.2f", m_pBikeAnimParams->bikeLeanAngleHelper.GetLeanAngle());
			grcDebugDraw::Text(vDefaultPos, Color_green, XOFFSET, grcDebugDraw::GetScreenSpaceTextHeight() *iNoTexts++, szText);
			formatf(szText, "m_fBodyLeanY : %.2f", m_fBodyLeanY);
			grcDebugDraw::Text(vDefaultPos, Color_green, XOFFSET, grcDebugDraw::GetScreenSpaceTextHeight() *iNoTexts++, szText);
			formatf(szText, "m_fSpeed : %.2f", m_fSpeed);
			grcDebugDraw::Text(vDefaultPos, Color_green, XOFFSET, grcDebugDraw::GetScreenSpaceTextHeight() *iNoTexts++, szText);
			formatf(szText, "m_fDesiredLeanAngle : %.2f", m_pBikeAnimParams->bikeLeanAngleHelper.GetDesiredLeanAngle());
			grcDebugDraw::Text(vDefaultPos, Color_green, XOFFSET, grcDebugDraw::GetScreenSpaceTextHeight() *iNoTexts++, szText);
			formatf(szText, "m_fPitchAngle : %.2f", m_fPitchAngle);
			grcDebugDraw::Text(vDefaultPos, Color_green, XOFFSET, grcDebugDraw::GetScreenSpaceTextHeight() *iNoTexts++, szText);
			formatf(szText, "Pitch : %.2f", rPed.GetCurrentPitch());
			grcDebugDraw::Text(vDefaultPos, Color_green, XOFFSET, grcDebugDraw::GetScreenSpaceTextHeight() *iNoTexts++, szText);
			formatf(szText, "m_nDefaultClipSet : %s", GetDefaultClipSetString(m_nDefaultClipSet));
			grcDebugDraw::Text(vDefaultPos, m_nDefaultClipSet == DCS_LowriderLeanAlt ? Color_orange : Color_green, XOFFSET, grcDebugDraw::GetScreenSpaceTextHeight() *iNoTexts++, szText);
			formatf(szText, "m_bForceFinishTransition : %s", AILogging::GetBooleanAsString(m_bForceFinishTransition));
			grcDebugDraw::Text(vDefaultPos, Color_green, XOFFSET, grcDebugDraw::GetScreenSpaceTextHeight() *iNoTexts++, szText);
			formatf(szText, "m_bIsAltRidingToStillTransition : %s", AILogging::GetBooleanAsString(m_bIsAltRidingToStillTransition));
			grcDebugDraw::Text(vDefaultPos, Color_green, XOFFSET, grcDebugDraw::GetScreenSpaceTextHeight() *iNoTexts++, szText);	
			formatf(szText, "m_fTimeToTriggerAltLowriderClipset : %.2f", m_fTimeToTriggerAltLowriderClipset);
			grcDebugDraw::Text(vDefaultPos, Color_green, XOFFSET, grcDebugDraw::GetScreenSpaceTextHeight() *iNoTexts++, szText);	
			formatf(szText, "m_fAltLowriderClipsetTimer : %.2f", m_fAltLowriderClipsetTimer);
			grcDebugDraw::Text(vDefaultPos, Color_green, XOFFSET, grcDebugDraw::GetScreenSpaceTextHeight() *iNoTexts++, szText);	
			formatf(szText, "m_MinTimeInAltLowriderClipsetTimer : %.2f", m_MinTimeInAltLowriderClipsetTimer.GetCurrentTime());
			grcDebugDraw::Text(vDefaultPos, m_MinTimeInAltLowriderClipsetTimer.GetCurrentTime() > 0.0f ? Color_red : Color_green, XOFFSET, grcDebugDraw::GetScreenSpaceTextHeight() *iNoTexts++, szText);		
			formatf(szText, "Vehicle Velocity : %.2f (Min/Max Trigger for Alt : %.2f/%.2f (%s) ", m_pVehicle->GetVelocity().Mag(), ms_Tunables.m_MinVelocityToAllowAltClipSet, IsDoingAWheelie(*m_pVehicle) ? ms_Tunables.m_MaxVelocityToAllowAltClipSetWheelie : ms_Tunables.m_MaxVelocityToAllowAltClipSet, IsDoingAWheelie(*m_pVehicle) ? "W" : " ");
			grcDebugDraw::Text(vDefaultPos, Color_green, XOFFSET, grcDebugDraw::GetScreenSpaceTextHeight() *iNoTexts++, szText);	
		}	
	}
	
	grcDebugDraw::AddDebugOutput(Color_green, "Frame : %i, m_fFrontWheelOffGroundScale  %.2f", fwTimer::GetFrameCount(), m_fFrontWheelOffGroundScale);
	if (m_pBikeAnimParams)
		grcDebugDraw::AddDebugOutput(Color_green, "Frame : %i, m_pBikeAnimParams->fPreviousTargetSeatDisplacement  %.2f", fwTimer::GetFrameCount(), m_pBikeAnimParams->fPreviousTargetSeatDisplacement);
	grcDebugDraw::AddDebugOutput(Color_green, "Frame : %i, m_fDisplacementScale %.2f", fwTimer::GetFrameCount(), m_fDisplacementScale);
	grcDebugDraw::AddDebugOutput(Color_green, "Frame : %i, m_fTimeInAir %.2f", fwTimer::GetFrameCount(), m_fTimeInAir, m_fTimeInAir > 0.0f ? "TRUE" : "FALSE");
	grcDebugDraw::AddDebugOutput(Color_green, "Frame : %i, m_fMinHeightInAir %.2f, m_fMaxHeightInAir %.2f", fwTimer::GetFrameCount(), m_fMinHeightInAir, m_fMaxHeightInAir);

	TUNE_GROUP_BOOL(IN_VEHICLE_DEBUG, RENDER_DEBUG, false);
	if (RENDER_DEBUG)
	{

#if __DEV
		const bool bDoingBurnout = CheckIfVehicleIsDoingBurnout(*m_pVehicle, rPed);
		grcDebugDraw::AddDebugOutput(Color_green, "Frame : %i, bDoingBurnout ? %s", fwTimer::GetFrameCount(), bDoingBurnout ? "TRUE" : "FALSE");
		grcDebugDraw::AddDebugOutput(Color_green, "Frame : %i, m_fBurnOutBlend %.2f", fwTimer::GetFrameCount(), m_fBurnOutBlend);
		grcDebugDraw::AddDebugOutput(Color_green, "Frame : %i, Last Frames Velocity %.2f, %.2f, %.2f", fwTimer::GetFrameCount(), VEC3V_ARGS(m_vDebugLastVelocity));
		grcDebugDraw::AddDebugOutput(Color_green, "Frame : %i, Current Frames Velocity %.2f, %.2f, %.2f", fwTimer::GetFrameCount(), VEC3V_ARGS(m_vDebugCurrentVelocity));
		grcDebugDraw::AddDebugOutput(Color_green, "Frame : %i, Acceleration %.2f, %.2f, %.2f", fwTimer::GetFrameCount(), VEC3V_ARGS(m_vDebugCurrentVehicleAcceleration));
		grcDebugDraw::AddDebugOutput(Color_green, "Frame : %i, Acceleration Param %.2f, %.2f, %.2f", fwTimer::GetFrameCount(), VEC3V_ARGS(m_vCurrentVehicleAccelerationNorm));
#endif // __DEV

		Vector3 vPedPos = VEC3V_TO_VECTOR3(rPed.GetTransform().GetPosition());
		TUNE_GROUP_FLOAT(VEHICLE_DEBUG, LEAN_PARAMETER_WIDTH, 0.05f, 0.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(VEHICLE_DEBUG, LEAN_PARAMETER_HEIGHT, 0.01f, 0.0f, 1.0f, 0.01f);
		float fScreenX = 0.0f;
		float fScreenY = 0.0f;

		CScriptHud::GetScreenPosFromWorldCoord(vPedPos, fScreenX, fScreenY);

		TUNE_GROUP_FLOAT(VEHICLE_DEBUG, SCREEN_X_OFFSET, -0.01f, -1.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(VEHICLE_DEBUG, SCREEN_Y_OFFSET, -0.084f, -1.0f, 1.0f, 0.01f);
		fScreenX += SCREEN_X_OFFSET;
		fScreenY += SCREEN_Y_OFFSET;

		Vector2 vCurrentRenderStartPos(fScreenX, fScreenY);
		const float fLeanParameterBarEndXPos = vCurrentRenderStartPos.x + LEAN_PARAMETER_WIDTH;
		Vector2 vLeanParameterBarEndPos(fLeanParameterBarEndXPos, fScreenY);
		grcDebugDraw::Line(vCurrentRenderStartPos, vLeanParameterBarEndPos, Color_white);

		// Render lean parameter
		float fLeanXPos = fScreenX + m_vCurrentVehicleAcceleration.GetXf() * (fLeanParameterBarEndXPos - fScreenX);
		vCurrentRenderStartPos = Vector2(fLeanXPos, fScreenY - LEAN_PARAMETER_HEIGHT); 

		// Render current phase
		TUNE_GROUP_BOOL(VEHICLE_TUNE, RENDER_CURRENT_ACCX_PARAM, true);
		if (RENDER_CURRENT_ACCX_PARAM)
		{
			Vector2 vPhaseEndPos(fLeanXPos, fScreenY + LEAN_PARAMETER_HEIGHT);
			grcDebugDraw::Line(vCurrentRenderStartPos, vPhaseEndPos, Color_blue);
		}

		// Render curren accx parameter
		fLeanXPos = fScreenX + m_fBodyLeanX * (fLeanParameterBarEndXPos - fScreenX);
		vCurrentRenderStartPos = Vector2(fLeanXPos, fScreenY - LEAN_PARAMETER_HEIGHT); 

		// Render current phase
		TUNE_GROUP_BOOL(VEHICLE_TUNE, RENDER_CURRENT_ACCX, true);
		if (RENDER_CURRENT_ACCX)
		{
			Vector2 vPhaseEndPos(fLeanXPos, fScreenY + LEAN_PARAMETER_HEIGHT);
			grcDebugDraw::Line(vCurrentRenderStartPos, vPhaseEndPos, Color_green);
		}

		TUNE_GROUP_FLOAT(VEHICLE_DEBUG, SCREEN_TEXT_X_OFFSET, 0.012f, -1.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(VEHICLE_DEBUG, SCREEN_TEXT_Y_OFFSET, 0.036f, -1.0f, 1.0f, 0.01f);
		char szDebugText[128];
		formatf(szDebugText, "A: %.2f, AN: %.2f AZ: %.2f", m_vCurrentVehicleAcceleration.GetXf(), m_fBodyLeanX, m_vCurrentVehicleAcceleration.GetZf());
		float fCurrentYPos = fScreenY + SCREEN_TEXT_Y_OFFSET;
		vCurrentRenderStartPos = Vector2(fLeanXPos - SCREEN_TEXT_X_OFFSET, fCurrentYPos); 
		grcDebugDraw::Text(vCurrentRenderStartPos, Color_black, szDebugText);


		grcDebugDraw::AddDebugOutput(Color_yellow, "AccelerationX : %.4f", m_vCurrentVehicleAcceleration.GetXf());
		grcDebugDraw::AddDebugOutput(Color_yellow, "AccelerationY : %.4f", m_vCurrentVehicleAcceleration.GetYf());
		grcDebugDraw::AddDebugOutput(Color_yellow, "AccelerationZ : %.4f", m_vCurrentVehicleAcceleration.GetZf());

// 		grcDebugDraw::AddDebugOutput(Color_yellow, "AngVelX : %.4f", m_vAngularAcceleration.x);
// 		grcDebugDraw::AddDebugOutput(Color_yellow, "AngVelY : %.4f", m_vAngularAcceleration.y);
// 		grcDebugDraw::AddDebugOutput(Color_yellow, "AngVelZ : %.4f", m_vAngularAcceleration.z);

		grcDebugDraw::AddDebugOutput(Color_yellow, "Speed Abs : %.4f (%.4f), Normalised : %.4f", m_pVehicle->GetVelocityIncludingReferenceFrame().Mag(), m_pVehicle->GetVelocity().Mag(), m_fSpeed);
		grcDebugDraw::AddDebugOutput(Color_yellow, "Steering : %.4f", m_fSteering);
		grcDebugDraw::AddDebugOutput(Color_yellow, "Pitch Angle: %.4f", m_fPitchAngle);

		static float fScale = 0.2f;
		static float fEndWidth = 0.01f;

		if(m_pVehicle->InheritsFromBoat())
		{
			static Vector2 vAccNormYDebugPos(0.1f,0.15f);
			static Vector2 vAccNormZDebugPos(0.25f,0.15f);

			float fAccY = m_vCurrentVehicleAccelerationNorm.GetYf();
			grcDebugDraw::Meter(vAccNormYDebugPos,Vector2(0.0f,1.0f),fScale,fEndWidth,Color32(255,255,255),"AccY");
			grcDebugDraw::MeterValue(vAccNormYDebugPos, Vector2(0.0f,1.0f), fScale, fAccY, fEndWidth, Color32(255,0,0),true);	
			grcDebugDraw::AddDebugOutput(Color_green, "AccY (-1.0f, 1.0f) : %.4f", fAccY);

			float fAccZ = m_vCurrentVehicleAccelerationNorm.GetZf();
			grcDebugDraw::Meter(vAccNormZDebugPos,Vector2(0.0f,1.0f),fScale,fEndWidth,Color32(255,255,255),"AccZ");
			grcDebugDraw::MeterValue(vAccNormZDebugPos, Vector2(0.0f,1.0f), fScale, fAccZ, fEndWidth, Color32(255,0,0),true);	
			grcDebugDraw::AddDebugOutput(Color_green, "AccZ (-1.0f, 1.0f) : %.4f", fAccZ);
		}
		else if (m_pBikeAnimParams)
		{
			static Vector2 vTargetSeatDisplacementSpringDebugPos(0.8f,0.15f);
			static Vector2 vSeatDisplacementSpringDebugPos(0.6f,0.15f);
			static Vector2 vPitchDebugPos(0.4f,0.15f);

			float fDisplacement = m_pBikeAnimParams->fSeatDisplacement;
			fDisplacement += 1.0f;
			fDisplacement *= 0.5f;
			fDisplacement = 1.0f - fDisplacement;
			grcDebugDraw::Meter(vSeatDisplacementSpringDebugPos,Vector2(0.0f,1.0f),fScale,fEndWidth,Color32(255,255,255),"Seat Displacement");
			grcDebugDraw::MeterValue(vSeatDisplacementSpringDebugPos, Vector2(0.0f,1.0f), fScale, fDisplacement, fEndWidth, Color32(255,0,0),true);	
			grcDebugDraw::AddDebugOutput(Color_green, "Seat Displacement (-1.0f, 1.0f) : %.4f", m_pBikeAnimParams->fSeatDisplacement);

			float fTargetDisplacement = m_pBikeAnimParams->fPreviousTargetSeatDisplacement;
			fTargetDisplacement += 1.0f;
			fTargetDisplacement *= 0.5f;
			fTargetDisplacement = 1.0f - fTargetDisplacement;
			grcDebugDraw::AddDebugOutput(Color_green, "Target Seat Displacement (-1.0f, 1.0f) : %.4f", m_pBikeAnimParams->fPreviousTargetSeatDisplacement);
			grcDebugDraw::Meter(vTargetSeatDisplacementSpringDebugPos,Vector2(0.0f,1.0f),fScale,fEndWidth,Color32(255,255,255),"Target Seat Displacement");
			grcDebugDraw::MeterValue(vTargetSeatDisplacementSpringDebugPos, Vector2(0.0f,1.0f), fScale, fTargetDisplacement, fEndWidth, Color32(255,0,0),true);	

			grcDebugDraw::AddDebugOutput(Color_green, "Pitch Angle (-1.0f, 1.0f) : %.4f", m_fPitchAngle);
			grcDebugDraw::Meter(vPitchDebugPos,Vector2(0.0f,1.0f),fScale,fEndWidth,Color32(255,255,255),"Pitch Angle");
			grcDebugDraw::MeterValue(vPitchDebugPos, Vector2(0.0f,1.0f), fScale, m_fPitchAngle, fEndWidth, Color32(255,0,0),true);	

			//grcDebugDraw::AddDebugOutput(Color_green, "Wheelie Impulse : %.4f", m_fWheelieImpulse);
			grcDebugDraw::AddDebugOutput(Color_green, "Desired Lean Angle : %.4f", m_pBikeAnimParams->bikeLeanAngleHelper.GetDesiredLeanAngle());
			grcDebugDraw::AddDebugOutput(Color_green, "Lean Angle : %.4f", m_pBikeAnimParams->bikeLeanAngleHelper.GetLeanAngle());
			grcDebugDraw::AddDebugOutput(Color_green, "State : %i", m_pBikeAnimParams->bikeLeanAngleHelper.GetState());
			grcDebugDraw::AddDebugOutput(Color_green, "Return To Idle State : %i", m_pBikeAnimParams->bikeLeanAngleHelper.GetReturnToIdleState());
		}
	}
#endif
}
#endif // !__FINAL

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInAutomobile::GetNMBlendOutClip(fwMvClipSetId& outClipSet, fwMvClipId& outClip)
{
	if (taskVerifyf(m_pVehicle, "NULL vehicle in CTaskMotionInAutomobile::GetNMBlendOutClip"))
	{
		const CVehicleSeatAnimInfo* pSeatClipInfo = m_pVehicle->GetSeatAnimationInfo(GetPed());
		if (pSeatClipInfo)
		{
			outClipSet = pSeatClipInfo->GetSeatClipSetId();
			outClip = CLIP_IDLE;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::CheckIfPedShouldSwayInVehicle(const CVehicle* pVehicle)
{
	const CPed* pPed = GetPed();

	//B*2339662 Don't lean in rear seats of boats when ducking! In rear boat seats we use different anim clipsets, that dont have slow leans
	//If these seat clipsets are changed to include them in future, this block will need to be reverted to how it was
	bool bDuckingInBoatRear = false;
	s32 iSeatIndex = m_pVehicle->GetSeatManager() ? m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed) : -1;
	if(m_pVehicle->IsSeatIndexValid(iSeatIndex) && m_pVehicle->GetSeatInfo(iSeatIndex) && !m_pVehicle->GetSeatInfo(iSeatIndex)->GetIsFrontSeat()) 
	{
		if (m_pVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT && GetDefaultClipSetToUse() == DCS_Ducked)
		{
			bDuckingInBoatRear = true;
		}
	}

	return pVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT && !bDuckingInBoatRear &&
		   pVehicle->GetVehicleModelInfo()->GetModelNameHash() != ATSTRINGHASH("predator", 0xe2e7d4ab);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInAutomobile::DriveCloseDoorFromClip(CVehicle* pVehicle, s32 iEntryPointIndex, const crClip* pCloseDoorClip, const float fCloseDoorPhase)
{
	if (!GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE))
	{
		taskFatalAssertf(pCloseDoorClip, "NULL clip in CTaskEnterVehicleCloseDoor::DriveCloseDoorFromClip");

		// Look up bone index of target entry point
		s32 iBoneIndex = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromEntryPoint(iEntryPointIndex);

		if (iBoneIndex > -1)
		{
			CCarDoor* pDoor = pVehicle->GetDoorFromBoneIndex(iBoneIndex);
			if (pDoor)
			{
				static dev_float sfBeginCloseDoor = 0.1f;
				float fPhaseToBeginCloseDoor = sfBeginCloseDoor;
				static dev_float sfEndCloseDoor = 0.5f;
				float fPhaseToEndCloseDoor = sfEndCloseDoor;

				//taskVerifyf(CGtaClipManager::FindEventPhase(pCloseDoorClip, AEF_DOOR_START, fPhaseToBeginCloseDoor), "Clip %s has no door start flag", pCloseDoorClip->GetName());
				//taskVerifyf(CGtaClipManager::FindEventPhase(pCloseDoorClip, AEF_DOOR_END, fPhaseToEndCloseDoor), "Clip %s has no door end flag", pCloseDoorClip->GetName());
				CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pCloseDoorClip, CClipEventTags::Door, CClipEventTags::Start, true, fPhaseToBeginCloseDoor);
				CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pCloseDoorClip, CClipEventTags::Door, CClipEventTags::Start, false, fPhaseToEndCloseDoor);

				if (fCloseDoorPhase >= fPhaseToEndCloseDoor)
				{
					pDoor->SetTargetDoorOpenRatio(0.0f, CCarDoor::DRIVEN_AUTORESET|CCarDoor::WILL_LOCK_DRIVEN);
					m_bCloseDoorSucceeded = true;
				}
				else if (fCloseDoorPhase >= fPhaseToBeginCloseDoor)
				{
					float fRatio = rage::Min(1.0f, 1.0f - (fCloseDoorPhase - fPhaseToBeginCloseDoor) / (fPhaseToEndCloseDoor - fPhaseToBeginCloseDoor));
					fRatio = rage::Min(pDoor->GetDoorRatio(), fRatio);
					pDoor->SetTargetDoorOpenRatio(fRatio, CCarDoor::DRIVEN_AUTORESET|CCarDoor::WILL_LOCK_DRIVEN);
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::CheckForHelmet(const CPed& ped) const
{
	if (ped.GetPedResetFlag(CPED_RESET_FLAG_DisableInVehicleActions))
		return false;

	if (!WillPutOnHelmet(*m_pVehicle, ped))
		return false;

	if (m_fIdleTimer < IDLE_TIMER_HELMET_PUT_ON)
		return false;

	if (!GetClipForStateNoAssert(State_PutOnHelmet))
		return false;

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::CheckForHorn(bool bAllowBikes) const
{
	if (((m_pVehicle->GetVehicleType() != VEHICLE_TYPE_CAR && m_pVehicle->GetVehicleType() != VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE) && !bAllowBikes) || (bAllowBikes && m_pVehicle->GetVehicleType() != VEHICLE_TYPE_BIKE))
	{
		return false;
	}

	const CPed& ped = *GetPed();
	if (ped.GetPedResetFlag(CPED_RESET_FLAG_DisableInVehicleActions))
		return false;

	// Only driver can use the horn
	if (m_pVehicle->GetSeatManager()->GetDriver() != &ped)
	{
		return false;
	}

	// And not dead...
	if (ped.ShouldBeDead())
	{
		return false;
	}

	// If a vehicle special ability is suppressing the horn, also prevent the task state
	if (m_pVehicle->GetVehicleAudioEntity() && m_pVehicle->GetVehicleAudioEntity()->ShouldSuppressHorn())
	{
		return false;
	}

	if (ped.IsLocalPlayer())
	{
		const CControl* pControl = ped.GetControlFromPlayer();
		if (pControl)
		{
			return pControl->GetVehicleHorn().IsDown();
		}
	}
	return m_pVehicle->IsHornOn();
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::StartMoveNetwork(const CVehicleSeatAnimInfo* pSeatClipInfo)
{
	if (!taskVerifyf(pSeatClipInfo, "NULL Seat Clip Info"))
		return false;

	CPed* pPed = GetPed();

	fwMvNetworkDefId networkDefId = CClipNetworkMoveInfo::ms_NetworkTaskInVehicle;

	// need to check if the parent task is ready for us
	if (m_moveNetworkHelper.RequestNetworkDef(networkDefId))
	{
		if (!m_moveNetworkHelper.IsNetworkActive())
		{
			// Look for an overridden invehicle context 
			const CSeatOverrideAnimInfo* pSeatOverrideAnimInfo = NULL;
			CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();
			const u32 uOverrideInVehicleContext = pTask ? pTask->GetOverrideInVehicleContextHash() : 0;
			if (uOverrideInVehicleContext != 0)
			{
				const CInVehicleOverrideInfo* pOverrideInfo = CVehicleMetadataMgr::GetInVehicleOverrideInfo(uOverrideInVehicleContext);
				if (taskVerifyf(pOverrideInfo, "Couldn't find override info for context with hash %u", uOverrideInVehicleContext))
				{
					pSeatOverrideAnimInfo = pOverrideInfo->FindSeatOverrideAnimInfoFromSeatAnimInfo(pSeatClipInfo);
				}
			}
					
			m_moveNetworkHelper.CreateNetworkPlayer(pPed, networkDefId);
			m_moveNetworkHelper.SetFlag(pSeatClipInfo->GetUseStandardInVehicleAnims(), ms_UseStandardInVehicleAnimsFlagId);
			m_moveNetworkHelper.SetFlag(pSeatClipInfo->GetUseBikeInVehicleAnims(), ms_UseBikeInVehicleAnimsFlagId);
			m_moveNetworkHelper.SetFlag(pSeatOverrideAnimInfo!=NULL ? pSeatOverrideAnimInfo->GetUseBasicAnims() : pSeatClipInfo->GetUseBasicAnims(), ms_UseBasicAnimsFlagId);
			m_moveNetworkHelper.SetFlag(pSeatClipInfo->GetUseBoatInVehicleAnims(), ms_UseBoatInVehicleAnimsId);
			m_moveNetworkHelper.SetFlag(pSeatClipInfo->GetUseJetSkiInVehicleAnims(), ms_UseJetSkiInVehicleAnimsId);
			m_moveNetworkHelper.SetFlag(m_pVehicle->GetLayoutInfo()->GetUse2DBodyBlend(), ms_Use2DBodyBlendFlagId);

			// Need to call this once when starting in order to get our params in the buffer the first frame
			ProcessMoveSignals();
		}

		// We need to do the deferred insert a frame before we receive the ready event so we actually update the ped pose correctly
		// without this we could end up with a one frame tpose unless there are only clips hiding this delay
		if (!m_moveNetworkHelper.IsNetworkAttached())
		{
			m_moveNetworkHelper.DeferredInsert();
			// Set the flag to decide whether we use driver or passenger clips
			m_moveNetworkHelper.SetFlag(m_pVehicle->IsDriver(pPed), CTaskMotionInVehicle::ms_IsDriverId);

			if (pSeatClipInfo->GetUseBikeInVehicleAnims())
			{
				m_moveNetworkHelper.SetFlag(true, CTaskMotionInVehicle::ms_UseNewBikeAnimsId);
			}
			return true;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::SetSeatClipSet(const CVehicleSeatAnimInfo* pSeatClipInfo)
{
	if (!taskVerifyf(pSeatClipInfo, "NULL Seat Clip Info"))
		return false;

	fwMvClipSetId seatClipSetId = pSeatClipInfo->GetSeatClipSetId();

#if __DEV
	// Check the ped is in the correct seat and their clipset is valid
	CTaskMotionInVehicle::VerifyNetworkStartUp(*m_pVehicle, *GetPed(), seatClipSetId);
#endif

	if (m_moveNetworkHelper.GetClipSetId() == CLIP_SET_ID_INVALID)
	{
		m_moveNetworkHelper.SetClipSet(seatClipSetId);
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::SetEntryClipSet()
{
	CPed* pPed = GetPed();

	s32 iDirectEntryPointIndex = m_pVehicle->GetDirectEntryPointIndexForPed(pPed);
	const bool bTwoDoorsOneSeat = m_pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_TWO_DOORS_ONE_SEAT);
	if (bTwoDoorsOneSeat)
	{
		CTaskMotionInVehicle::GetOpenDoorForDoubleDoorSeat(*m_pVehicle, m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed), iDirectEntryPointIndex);
	}
	if (m_pVehicle->IsEntryPointIndexValid(iDirectEntryPointIndex))
	{
		const CVehicleEntryPointAnimInfo* pEntryAnimClipInfo = m_pVehicle->GetEntryAnimInfo(iDirectEntryPointIndex);
		if (pEntryAnimClipInfo)
		{
			fwMvClipSetId entryClipSetId = pEntryAnimClipInfo->GetEntryPointClipSetId();
		
			const s32 iDirectEntryPointIndex = m_pVehicle->GetDirectEntryPointIndexForPed(pPed);
			if (m_pVehicle->IsEntryPointIndexValid(iDirectEntryPointIndex))
			{
				const CVehicleEntryPointAnimInfo* pEntryAnimClipInfo = m_pVehicle->GetEntryAnimInfo(iDirectEntryPointIndex);
				if (pEntryAnimClipInfo)
				{
#if __BANK
					if (!CTaskVehicleFSM::IsVehicleClipSetLoaded(entryClipSetId))
					{
						AI_LOG_WITH_ARGS("[InVehicle] %s ped %s, clipset %s isn't loaded for entrypoint %i\n", AILogging::GetDynamicEntityIsCloneStringSafe(pPed), AILogging::GetDynamicEntityNameSafe(pPed), entryClipSetId.GetCStr(), iDirectEntryPointIndex);
					}
#endif // __BANK

					m_moveNetworkHelper.SetClipSet(entryClipSetId, CTaskVehicleFSM::ms_EntryVarClipSetId);
					return true;
				}
			}
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::IsCloseDoorStateValid()
{
	const CPed* pPed = GetPed();
	s32 iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
	if (m_pVehicle->IsSeatIndexValid(iSeatIndex))
	{
		const s32 iDirectEntryPointIndex = m_pVehicle->GetDirectEntryPointIndexForPed(pPed);
		const bool bClipSetLoaded = CTaskVehicleFSM::RequestClipSetFromEntryPoint(m_pVehicle, iDirectEntryPointIndex);
#if __BANK
		if (bClipSetLoaded)
		{
			AI_LOG_WITH_ARGS("[InVehicle] Close door state valid for %s ped %s, clipset %s loaded for entrypoint %i\n", AILogging::GetDynamicEntityIsCloneStringSafe(pPed), AILogging::GetDynamicEntityNameSafe(pPed), CTaskVehicleFSM::GetClipSetIdFromEntryPoint(m_pVehicle, iDirectEntryPointIndex).GetCStr(), iDirectEntryPointIndex);
		}
#endif // __BANK
		return bClipSetLoaded;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::IsWithinRange(float fTestValue, float fMin, float fMax)
{
	return fTestValue >= fMin && fTestValue <= fMax;
}

void CTaskMotionInAutomobile::ComputePitchParameter(float fTimeStep)
{
	const CPed& ped = *GetPed();
	if ((m_pVehicle->InheritsFromBike() || m_pVehicle->InheritsFromQuadBike() || m_pVehicle->GetIsJetSki() || m_pVehicle->InheritsFromAmphibiousQuadBike()) 
		&& GetState() != State_Still && GetState() != State_StillToSit && GetState() != State_SitToStill)
	{
		if (ped.IsLocalPlayer())
		{
			const CControl* pControl = ped.GetControlFromPlayer();
			if (pControl)
			{
				TUNE_GROUP_FLOAT(IN_VEHICLE_TUNE, KERS_STEERING_RANGE, 0.25f, 0.0f, 1.0f, 0.01f);
				float fSteeringAngle = Clamp((pControl->GetVehicleSteeringLeftRight().GetNorm() + 1.0f) * 0.5f, 0.0f, 1.0f);
				if (ped.GetIsDrivingVehicle() && m_pVehicle->m_nVehicleFlags.bIsKERSBoostActive && IsWithinRange(fSteeringAngle, 0.5f - KERS_STEERING_RANGE, 0.5f + KERS_STEERING_RANGE))
				{
					TUNE_GROUP_FLOAT(IN_VEHICLE_TUNE, KERS_PITCH_APPROACH_RATE, 1.5f, 0.0f, 10.0f, 0.01f);
					rage::Approach(m_fPitchAngle, 1.0f, KERS_PITCH_APPROACH_RATE, fTimeStep);
					return;
				}

				CTaskAimGunVehicleDriveBy* pGunDriveByTask = static_cast<CTaskAimGunVehicleDriveBy*>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY));
				CTaskMountThrowProjectile* pProjDriveByTask = static_cast<CTaskMountThrowProjectile*>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_MOUNT_THROW_PROJECTILE));
				if (pGunDriveByTask || pProjDriveByTask)
				{
					TUNE_GROUP_FLOAT(IN_VEHICLE_TUNE, DRIVEBY_PITCH_APPROACH_RATE, 0.75f, 0.0f, 10.0f, 0.01f);
					rage::Approach(m_fPitchAngle, 0.5f, DRIVEBY_PITCH_APPROACH_RATE, fTimeStep);
					return;
				}
				const float fMinBikePitch = CTaskMotionInVehicle::ms_Tunables.m_MinPitchDefault;
				const float fMaxBikePitch = CTaskMotionInVehicle::ms_Tunables.m_MaxPitchDefault;
				float fBikePitchAngle = ped.GetCurrentPitch() - fMinBikePitch;
				const float fPitchRange = Abs(fMaxBikePitch - fMinBikePitch);
				fBikePitchAngle = fBikePitchAngle / fPitchRange;
				if (fBikePitchAngle > 1.0f) fBikePitchAngle = ms_Tunables.m_MaxForwardsPitchSlope;
				else if (fBikePitchAngle < 0.0f) fBikePitchAngle = ms_Tunables.m_MinForwardsPitchSlope;

				bool bLimitPitchDueToWheelie = false;

				bool bDoSlopeCalculation = false;
				if(m_pBikeAnimParams && m_pVehicle->GetWheel(1))
				{
					m_pBikeAnimParams->bIsDoingWheelie = !m_pVehicle->GetWheel(1)->GetIsTouching();
					if (m_pBikeAnimParams->bIsDoingWheelie)
					{
						m_pBikeAnimParams->fTimeDoingWheelie += fTimeStep;
						bLimitPitchDueToWheelie = true;
					}
					else
					{
						bDoSlopeCalculation = true;
						m_pBikeAnimParams->fTimeDoingWheelie = 0.0f;
					}
				}	
				else if(m_pVehicle->GetIsJetSki())
				{
					bDoSlopeCalculation = ((CBoat*)m_pVehicle.Get())->m_BoatHandling.IsInWater();
					bLimitPitchDueToWheelie = true;
				}

				bool bBikeOnSlope = false;
				const float fBikeStillLowerBound = (0.5f - CTaskMotionInVehicle::ms_Tunables.m_StillPitchAngleTol);
				const float fBikeStillUpperBound = (0.5f + CTaskMotionInVehicle::ms_Tunables.m_StillPitchAngleTol);
				if (bDoSlopeCalculation)
				{
					if (fBikePitchAngle <= fBikeStillLowerBound || fBikePitchAngle >= fBikeStillUpperBound) 
					{
						bBikeOnSlope = true;
					}
				}

				const float fSpeedRatio = rage::Clamp(m_fSpeed / ms_Tunables.m_SlowFastSpeedThreshold, 0.0f, 1.0f);
				float fMinPitch = ms_Tunables.m_MinForwardsPitchSlowSpeed * (1.0f - fSpeedRatio) + ms_Tunables.m_MinForwardsPitchFastSpeed * fSpeedRatio;
				float fMaxPitch = ms_Tunables.m_MaxForwardsPitchSlowSpeed * (1.0f - fSpeedRatio) + ms_Tunables.m_MaxForwardsPitchFastSpeed * fSpeedRatio;

				if (bLimitPitchDueToWheelie)
				{
					taskAssert(m_pBikeAnimParams);
					if (m_pBikeAnimParams->fTimeDoingWheelie >= ms_Tunables.m_TimeInWheelieToEnforceMinPitch)
					{
						fMinPitch = rage::Clamp(fMinPitch, ms_Tunables.m_MinForwardsPitchWheelieBalance, ms_Tunables.m_MaxForwardsPitchWheelieBalance);
					}
					else
					{
						fMinPitch = rage::Clamp(fMinPitch, ms_Tunables.m_MinForwardsPitchWheelieBegin, ms_Tunables.m_MaxForwardsPitchWheelieBalance);			
					}
				}

				if (bBikeOnSlope)
				{
					fMinPitch = ms_Tunables.m_MinForwardsPitchSlopeBalance;
					fMaxPitch = ms_Tunables.m_MaxForwardsPitchSlopeBalance;
				}

				const bool bShouldUseCustomPOVSettings = ShouldUseCustomPOVSettings(ped, *m_pVehicle);
				if (bShouldUseCustomPOVSettings)
				{
					m_pVehicle->GetPOVTuningInfo()->ComputeMinMaxPitchAngle(fSpeedRatio, fMinPitch, fMaxPitch);
				}

				float fStickX = 0.0f; float fStickY = 0.0f;
				float fDesiredPitch = fStickY = pControl->GetVehicleSteeringUpDown().GetNorm(); 
				if(m_pBikeAnimParams)
				{
					if (m_pVehicle->InheritsFromBike())
					{
						static_cast<CBike*>(m_pVehicle.Get())->GetBikeLeanInputs(fStickX, fStickY);
					}

					if (bShouldUseCustomPOVSettings && Abs(fStickY) < 0.1f)
					{
						fDesiredPitch = m_pVehicle->GetPOVTuningInfo()->ComputeStickCenteredBodyLeanY(m_pBikeAnimParams->bikeLeanAngleHelper.GetLeanAngle(), m_fSpeed);
					}
					else
					{
						fDesiredPitch = fStickY;
					}
				}

				fDesiredPitch += 1.0f;
				fDesiredPitch *= 0.5f;
				fDesiredPitch = 1.0f - fDesiredPitch;
				fDesiredPitch = rage::Clamp(fDesiredPitch, fMinPitch, fMaxPitch);

				if (bBikeOnSlope)
				{
					if(m_pVehicle->GetIsJetSki())
					{
						static dev_float s_SlopeAngleWeightingNoStick = 0.5f;
						static dev_float s_SlopeAngleWeightingStick = 0.25f;
						float fWeighting = fStickY > 0.01f ? s_SlopeAngleWeightingStick : s_SlopeAngleWeightingNoStick;
						fDesiredPitch *= (1.0f-fWeighting);
						fBikePitchAngle *= fWeighting;
						fDesiredPitch += fBikePitchAngle;
						fDesiredPitch = rage::Clamp(fDesiredPitch, fMinPitch, fMaxPitch);
					}
					else
					{
						if (fDesiredPitch >= fBikeStillLowerBound && fDesiredPitch <= fBikeStillUpperBound)
						{
							fDesiredPitch = fBikePitchAngle;
						}
					}
				}

				// We slow the approach rate when flicking between sides to avoid the head bobbing up
				TUNE_GROUP_FLOAT(FIRST_PERSON_VEHICLE_TUNE, POV_PITCH_APPROACH_JUST_LEAN_EXTREME, 0.1f, 0.0f, 5.0f, 0.01f);
				TUNE_GROUP_FLOAT(FIRST_PERSON_VEHICLE_TUNE, POV_PITCH_APPROACH_NOT_LEAN_EXTREME, 2.0f, 0.0f, 5.0f, 0.01f);
				TUNE_GROUP_FLOAT(FIRST_PERSON_VEHICLE_TUNE, LEAN_EXTREME_BLEND_OUT_TIME, 0.4f, 0.0f, 5.0f, 0.01f);
				float fPOVPitchApproachRate = POV_PITCH_APPROACH_NOT_LEAN_EXTREME;
				if (bShouldUseCustomPOVSettings)
				{
					const float fPOVApproachRatio = Clamp(m_pBikeAnimParams->bikeLeanAngleHelper.GetTimeSinceLeaningExtreme() / LEAN_EXTREME_BLEND_OUT_TIME, 0.0f, 1.0f);
					fPOVPitchApproachRate = (1.0f - fPOVApproachRatio) * POV_PITCH_APPROACH_JUST_LEAN_EXTREME + fPOVApproachRatio * POV_PITCH_APPROACH_NOT_LEAN_EXTREME;
				}

				const float fApproachRate = bShouldUseCustomPOVSettings ? fPOVPitchApproachRate : (ms_Tunables.m_SlowApproachRate * (1.0f - fSpeedRatio) + ms_Tunables.m_FastApproachRate * fSpeedRatio);
				rage::Approach(m_fPitchAngle, fDesiredPitch, bLimitPitchDueToWheelie ? ms_Tunables.m_WheelieApproachRate : fApproachRate, fTimeStep);
				PDR_ONLY(debugPlayback::RecordTaggedFloatValue(*ped.GetCurrentPhysicsInst(), m_fPitchAngle, "m_fPitchAngle"));
				return;
			}
		}
		else if ( ped.IsNetworkClone() && (m_pVehicle->GetDriver() && m_pVehicle->GetDriver() == &ped) && (m_pVehicle->InheritsFromBike() || m_pVehicle->InheritsFromQuadBike() || m_pVehicle->InheritsFromAmphibiousQuadBike()) )
		{
			m_fPitchAngle = rage::Lerp(CTaskMotionInVehicle::ms_Tunables.m_BikePitchSmoothingPassengerRate, m_fPitchAngle, m_fRemoteDesiredPitchAngle);
			return;
		}
		else if (m_pVehicle->GetDriver() && m_pVehicle->GetDriver() != &ped)
		{
			CTaskMotionInAutomobile* pMotionInAutomobileTask = static_cast<CTaskMotionInAutomobile*>(m_pVehicle->GetDriver()->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_AUTOMOBILE));
			if (pMotionInAutomobileTask)
			{
				m_fPitchAngle = rage::Lerp(CTaskMotionInVehicle::ms_Tunables.m_BikePitchSmoothingPassengerRate, m_fPitchAngle, pMotionInAutomobileTask->GetPitchAngle());
				return;
			}
		}
	}

	CTaskMotionInVehicle::ComputePitchSignal(ped, m_fPitchAngle, CTaskMotionInVehicle::ms_Tunables.m_BikePitchSmoothingRate);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInAutomobile::ProcessInVehicleOverride()
{
	CPed* pPed = GetPed();
	const CVehicleSeatAnimInfo* pSeatAnimInfo = m_pVehicle->GetSeatAnimationInfo(pPed);
	if (!pSeatAnimInfo)
	{
		return;
	}

	// Look for an overridden invehicle context and find the clipset associated with our current seat
	const CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();
	const u32 uOverrideInVehicleContext = pTask ? pTask->GetOverrideInVehicleContextHash() : 0;
	if (uOverrideInVehicleContext != 0)
	{
		if (!m_pInVehicleOverrideInfo || m_pInVehicleOverrideInfo->GetName().GetHash() != uOverrideInVehicleContext)
		{
			m_pInVehicleOverrideInfo = CVehicleMetadataMgr::GetInVehicleOverrideInfo(uOverrideInVehicleContext);
			if (taskVerifyf(m_pInVehicleOverrideInfo, "Couldn't find override info for context with hash %u", uOverrideInVehicleContext))
			{
				m_pSeatOverrideAnimInfo = m_pInVehicleOverrideInfo->FindSeatOverrideAnimInfoFromSeatAnimInfo(pSeatAnimInfo);
				
				if (taskVerifyf(m_pSeatOverrideAnimInfo, "Couldn't find seat override anim info for %s, seat info %s", m_pInVehicleOverrideInfo->GetName().GetCStr(), pSeatAnimInfo->GetName().GetCStr()) 
					&& m_pSeatOverrideAnimInfo->GetWeaponVisibleAttachedToRightHand() && pPed->GetWeaponManager())
				{
					if(pPed->GetWeaponManager()->GetEquippedWeaponHash() != 0 && pPed->GetInventory()->GetIsStreamedIn(pPed->GetWeaponManager()->GetEquippedWeaponHash()))
					{
						pPed->GetWeaponManager()->CreateEquippedWeaponObject(CPedEquippedWeapon::AP_RightHand);
						CObject* pWeapon = pPed->GetWeaponManager()->GetEquippedWeaponObject();
						if (pWeapon && !pWeapon->GetIsVisible())
						{
							pWeapon->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, true, true);
						}
					}
				}
			}
		}
	}
	else
	{
		m_pInVehicleOverrideInfo = NULL;
		m_pSeatOverrideAnimInfo = NULL;
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInAutomobile::ProcessHelmetVisorSwitching()
{
	CPed& rPed = *GetPed();

	// Interrupt and clean up if interupted by exit vehicle
	if (rPed.IsExitingVehicle())
	{
		m_bPlayingVisorSwitchAnim = false;
		rPed.SetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingHelmetVisor,false);
		rPed.SetPedConfigFlag(CPED_CONFIG_FLAG_ForceHelmetVisorSwitch,false);
		if (m_moveNetworkHelper.IsNetworkActive())
			m_moveNetworkHelper.SetFlag(m_bPlayingVisorSwitchAnim, ms_SwitchVisor);
		return;
	}

	if (rPed.GetHelmetComponent()->GetCurrentHelmetSupportsVisors())
	{
		if (m_HelmetVisorFirsPersonClipSetRequestHelper.GetClipSetId() == CLIP_SET_ID_INVALID)
			m_HelmetVisorFirsPersonClipSetRequestHelper.Request(ms_SwitchVisorPOVClipSet);
	}
	else
	{
		m_HelmetVisorFirsPersonClipSetRequestHelper.Release();
	}

	if (m_moveNetworkHelper.IsNetworkActive() && CTaskMotionInAutomobile::CheckForHelmetVisorSwitch(rPed, m_bWaitingForVisorPropToStreamIn, m_bPlayingVisorSwitchAnim))
	{
		bool bIsVisorUpNow = rPed.GetHelmetComponent()->GetIsVisorUp();
		bool bFinishedPreLoading = rPed.GetHelmetComponent()->HasPreLoadedProp();

		if(!rPed.GetHelmetComponent()->IsPreLoadingProp() && !bFinishedPreLoading)
		{
			int nVisorVersionToPreLoad = bIsVisorUpNow ? rPed.GetHelmetComponent()->GetHelmetVisorDownPropIndex() : rPed.GetHelmetComponent()->GetHelmetVisorUpPropIndex();

			if (nVisorVersionToPreLoad == -1)
				return;

			m_bWaitingForVisorPropToStreamIn = true;
			rPed.GetHelmetComponent()->SetPropFlag(PV_FLAG_DEFAULT_HELMET);
			rPed.GetHelmetComponent()->SetHelmetPropIndex(nVisorVersionToPreLoad);
			rPed.GetHelmetComponent()->RequestPreLoadProp();
			return;
		}

		if (bFinishedPreLoading)
		{
			m_bWaitingForVisorPropToStreamIn = false;

			fwMvClipId clipId = CLIP_ID_INVALID;
			const crClip* pClip = NULL;
#if FPS_MODE_SUPPORTED
			if (rPed.IsInFirstPersonVehicleCamera())
			{
				 //Check clipset has loaded in
				if (m_HelmetVisorFirsPersonClipSetRequestHelper.Request())
				{
					clipId = bIsVisorUpNow ? ms_Tunables.m_PutHelmetVisorDownPOVClipId : ms_Tunables.m_PutHelmetVisorUpPOVClipId;
					fwMvClipSetId clipSetId = m_HelmetVisorFirsPersonClipSetRequestHelper.GetClipSetId();
					if (clipSetId != CLIP_SET_ID_INVALID)
						pClip = fwClipSetManager::GetClip(clipSetId, clipId);
				}
			}
			else
#endif //FPS_MODE_SUPPORTED
			{

				clipId = CTaskTakeOffHelmet::GetVisorClipId(rPed, bIsVisorUpNow, false);

				fwMvClipSetId clipSetId = m_moveNetworkHelper.GetClipSetId();
				if (clipSetId != CLIP_SET_ID_INVALID)
					pClip = fwClipSetManager::GetClip(clipSetId, clipId);
			}

			if (pClip)
			{
				m_bPlayingVisorSwitchAnim = true;
				rPed.SetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingHelmetVisor,true);
				m_bPlayingVisorUpAnim = !bIsVisorUpNow;
				m_moveNetworkHelper.SetClip(pClip, ms_SwitchVisorClipId);			
				m_moveNetworkHelper.SetFlag(true, ms_SwitchVisor);
			}
			else
			{
				m_moveNetworkHelper.SetFlag(m_bPlayingVisorSwitchAnim, ms_SwitchVisor);
			}
		}
	}
	
	if(m_bPlayingVisorSwitchAnim)
	{
		rPed.SetHeadIkBlocked(); //reset every frame

		if (m_moveNetworkHelper.GetBoolean(ms_SwitchVisorAnimTag))
		{	
#if FPS_MODE_SUPPORTED
			if (rPed.IsInFirstPersonVehicleCamera())
				rPed.SetPedResetFlag(CPED_RESET_FLAG_PutDownHelmetFX, true);
#endif //FPS_MODE_SUPPORTED

			rPed.GetHelmetComponent()->EnableHelmet();
			rPed.GetHelmetComponent()->ClearPreLoadedProp();
			// Reset the desired helmet flag, which was updated by EnableHelmet() internally
			rPed.GetHelmetComponent()->SetPropFlag(PV_FLAG_NONE);

			if (!rPed.IsNetworkClone()) // Owner will switch this for us
				rPed.GetHelmetComponent()->SetIsVisorUp(!rPed.GetHelmetComponent()->GetIsVisorUp());
		}

		if (m_moveNetworkHelper.GetBoolean(ms_SwitchVisorAnimFinishedId) || !m_moveNetworkHelper.IsNetworkActive())
		{
			m_bPlayingVisorSwitchAnim = false;
			rPed.SetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingHelmetVisor,false);
			rPed.SetPedConfigFlag(CPED_CONFIG_FLAG_ForceHelmetVisorSwitch,false);

			if (m_moveNetworkHelper.IsNetworkActive())
			{
				m_moveNetworkHelper.SetFlag(m_bPlayingVisorSwitchAnim, ms_SwitchVisor);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::CheckForHelmetVisorSwitch(CPed& ped, bool bWaitingForVisorPropToStreamIn, bool bPlayingVisorSwitchAnim)
{
#if __BANK
	TUNE_GROUP_BOOL(HELMET_VISOR, bEnableDebugTesting, false);
	TUNE_GROUP_BOOL(HELMET_VISOR, bDebugIgnoreHasHelmetCheck, false);
	TUNE_GROUP_INT(HELMET_VISOR, VISOR_UP_ID, 74, 0, 1000, 1);
	TUNE_GROUP_INT(HELMET_VISOR, VISOR_DOWN_ID, 73, 0, 1000, 1);
	if (bEnableDebugTesting)
	{
		bool bIsVisorUpNow = ped.GetHelmetComponent()->GetIsVisorUp();
		ped.GetHelmetComponent()->SetCurrentHelmetSupportsVisors(true);
		ped.GetHelmetComponent()->SetVisorPropIndices(true, bIsVisorUpNow, VISOR_UP_ID,VISOR_DOWN_ID);
	}
#endif //__BANK

	bool bQuit = false;
	if (BANK_ONLY(!bEnableDebugTesting &&) !NetworkInterface::IsGameInProgress())
		bQuit = true;

	if (ped.GetIsEnteringVehicle())
		bQuit = true;

	if (ped.GetPedConfigFlag(CPED_CONFIG_FLAG_IsPerformingVehicleMelee))
		bQuit = true;

	if (BANK_ONLY(!bDebugIgnoreHasHelmetCheck &&) !ped.GetPedConfigFlag(CPED_CONFIG_FLAG_HasHelmet))
		bQuit = true;

	if (ped.GetPedResetFlag(CPED_RESET_FLAG_PuttingOnHelmet))
		bQuit = true;

	if (!ped.GetHelmetComponent() || !ped.GetHelmetComponent()->GetCurrentHelmetSupportsVisors())
		bQuit = true;

	if (ped.GetPedResetFlag(CPED_RESET_FLAG_IsDoingDriveby))
		bQuit = true;

	if ( (ped.GetPedResetFlag(CPED_RESET_FLAG_IsAiming) || ped.GetPedResetFlag(CPED_RESET_FLAG_FiringWeapon)) ||
		(ped.IsFirstPersonShooterModeEnabledForPlayer() && ped.GetMotionData()->GetFPSState() != CPedMotionData::FPS_IDLE) )
	{
		bQuit = true;
	}

	CVehicle* pVehicle = ped.GetVehiclePedInside();
	if (pVehicle && !pVehicle->InheritsFromBike() && !pVehicle->InheritsFromQuadBike())
		bQuit = true;

	if (bQuit)
	{
		ped.SetPedConfigFlag(CPED_CONFIG_FLAG_ForceHelmetVisorSwitch, false);
		return false;
	}

	if (bPlayingVisorSwitchAnim)
		return false;

	if (bWaitingForVisorPropToStreamIn)
		return true;

	if (ped.GetPedConfigFlag(CPED_CONFIG_FLAG_ForceHelmetVisorSwitch))
		return true;

	if (ped.IsLocalPlayer())
	{
		const CControl* pControl = ped.GetControlFromPlayer();
		TUNE_GROUP_INT(HELMET_VISOR, TIME_TO_HOLD_MS, 500, 0, 10000, 100);
		if (pControl && pControl->GetSwitchVisor().HistoryPressed(TIME_TO_HOLD_MS) && pControl->GetSwitchVisor().HistoryHeldDown(TIME_TO_HOLD_MS))
		{
			return true;
		}
	}
	else if (ped.IsNetworkClone() && ped.GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingHelmetVisor))
	{
		return true;
	}

	ped.SetPedConfigFlag(CPED_CONFIG_FLAG_ForceHelmetVisorSwitch, false);
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInAutomobile::ProcessBlockHandIkForVisor()
{
	m_bUsingVisorArmIK = false;

	bool bQuitEarly = false;
	if (!GetPed())
		bQuitEarly = true;

	if (!m_bPlayingVisorSwitchAnim)
		bQuitEarly = true;

	if (GetPed() && !GetPed()->GetIsDrivingVehicle())
		return;

	if (bQuitEarly && !m_bWasPlayingVisorSwitchAnimLastUpdate)
	{
#if FPS_MODE_SUPPORTED
		if (m_pFirstPersonIkHelper)
			m_pFirstPersonIkHelper->Reset();
#endif //FPS_MODE_SUPPORTED

		m_bWasPlayingVisorSwitchAnimLastUpdate = m_bPlayingVisorSwitchAnim;
		return;
	}

	CPed& ped = *GetPed();

#if FPS_MODE_SUPPORTED
	if (ped.IsInFirstPersonVehicleCamera())
	{
		if (m_pFirstPersonIkHelper == NULL)
		{
			m_pFirstPersonIkHelper = rage_new CFirstPersonIkHelper(CFirstPersonIkHelper::FP_DriveBy);
			taskAssert(m_pFirstPersonIkHelper);

			m_pFirstPersonIkHelper->SetArm(CFirstPersonIkHelper::FP_ArmLeft);
		}

		TUNE_GROUP_FLOAT(HELMET_VISOR, fPhaseToStopFpDownIK, 0.85f, 0.0f, 1.0f, 0.1f);
		TUNE_GROUP_FLOAT(HELMET_VISOR, fPhaseToStopFpUpIK, 0.65f, 0.0f, 1.0f, 0.1f);
		float fPhaseToStopFpIK = m_bPlayingVisorUpAnim ? fPhaseToStopFpUpIK : fPhaseToStopFpDownIK;
		float fPhase = m_moveNetworkHelper.GetFloat(ms_SwitchVisorPhaseId);

		if (m_pFirstPersonIkHelper && fPhase < fPhaseToStopFpIK)
		{
			TUNE_GROUP_BOOL(HELMET_VISOR, bOverrideOffsetValues, false);
			TUNE_GROUP_FLOAT(HELMET_VISOR, fOverrideOffsetX, -0.025f, -10.0f, 10.0f, 0.01f);
			TUNE_GROUP_FLOAT(HELMET_VISOR, fOverrideOffsetY, 0.025f, -10.0f, 10.0f, 0.01f);
			TUNE_GROUP_FLOAT(HELMET_VISOR, fOverrideOffsetZ, -0.030f, -10.0f, 10.0f, 0.01f);
			Vector3 vTargetOffset(Vector3::ZeroType);

			CVehicle* pVehicle = ped.GetVehiclePedInside();
			CVehicleModelInfo* pModelInfo = pVehicle->GetVehicleModelInfo();
			if(pModelInfo)
			{
				vTargetOffset = pModelInfo->GetFirstPersonVisorSwitchIKOffset();
			}

			if (bOverrideOffsetValues)
				vTargetOffset = Vector3(fOverrideOffsetX, fOverrideOffsetY, fOverrideOffsetZ);

			m_pFirstPersonIkHelper->SetBlendOutRate(ARMIK_BLEND_RATE_INSTANT);
			m_pFirstPersonIkHelper->SetOffset(RCC_VEC3V(vTargetOffset));
			m_pFirstPersonIkHelper->Process(&ped);
			m_bUsingVisorArmIK = true;
		}
	}
	else
#endif //FPS_MODE_SUPPORTED
	{
#if FPS_MODE_SUPPORTED
		if (m_pFirstPersonIkHelper)
			m_pFirstPersonIkHelper->Reset();
#endif //FPS_MODE_SUPPORTED

		fwMvClipId clipId = CTaskTakeOffHelmet::GetVisorClipId(ped, !m_bPlayingVisorUpAnim, false);
		const crClip* pClip = fwClipSetManager::GetClip(m_moveNetworkHelper.GetClipSetId(), clipId);
		if (!pClip)
		{
			m_bWasPlayingVisorSwitchAnimLastUpdate = m_bPlayingVisorSwitchAnim;
			return;
		}

		float fPhase = m_moveNetworkHelper.GetFloat(ms_SwitchVisorPhaseId);

		float fStartIkBlendInPhase = 0.0f;
		float fStopIkBlendInPhase = 1.0f;
		float fStartIkBlendOutPhase = 0.0f;
		float fStopIkBlendOutPhase = 1.0f;

		bool bRight = false;

		float fOutBlendInTime = NORMAL_BLEND_DURATION;
		float fOutBlendOutTime = NORMAL_BLEND_DURATION; 

		const bool bFoundBlendOutTags = CClipEventTags::FindIkTags(pClip, fStartIkBlendOutPhase, fStopIkBlendOutPhase, bRight, false);
		if (bFoundBlendOutTags)
		{
			float fPhaseDifference = (fStopIkBlendOutPhase - fStartIkBlendOutPhase);
			taskAssert(fPhaseDifference >= 0);
			fOutBlendOutTime = pClip->ConvertPhaseToTime(fPhaseDifference);
		}

		const bool bFoundBlendInTags = CClipEventTags::FindIkTags(pClip, fStartIkBlendInPhase, fStopIkBlendInPhase, bRight, true);
		if (bFoundBlendInTags)
		{
			float fPhaseDifference = (fStopIkBlendInPhase - fStartIkBlendInPhase);
			taskAssert(fPhaseDifference >= 0);
			fOutBlendInTime = pClip->ConvertPhaseToTime(fPhaseDifference);
		}

		if (bFoundBlendOutTags && bFoundBlendInTags && (fPhase < fStartIkBlendOutPhase || fPhase >= fStartIkBlendInPhase))
		{
			CVehicle* pVehicle = ped.GetVehiclePedInside();
			if (pVehicle)
			{
				int lBoneIdx = pVehicle->GetBoneIndex(VEH_HBGRIP_L);
				if (lBoneIdx > -1)
				{
					const s32 controlFlags = AIK_TARGET_WRT_IKHELPER | AIK_USE_ORIENTATION | AIK_USE_FULL_REACH | AIK_USE_ANIM_BLOCK_TAGS;
					aiDebugf2("Frame : %i, Left Arm Ik On TRUE", fwTimer::GetFrameCount());
					ped.GetIkManager().SetRelativeArmIKTarget(crIKSolverArms::LEFT_ARM, pVehicle, lBoneIdx, VEC3_ZERO, controlFlags, fOutBlendInTime, fOutBlendOutTime);
				}
			}
		}
	}

	m_bWasPlayingVisorSwitchAnimLastUpdate = m_bPlayingVisorSwitchAnim;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInAutomobile::ProcessDrivingAggressively()
{
	CPed& rPed = *GetPed();

	bool bDrivingAggressivelyThisFrame = false;
	//! Don't allow lowrider alt anims if we are cruising above a certain speed. If already driving aggressively, we need to drop below a certain theshold.
	TUNE_GROUP_FLOAT(LOWRIDER_TUNE, fMaxVehicleVelocityForLeans, 17.5f, 0.0f, 200, 0.1f);
	TUNE_GROUP_FLOAT(LOWRIDER_TUNE, fMinVehicleVelocityForLeans, 12.5f, 0.0f, 200, 0.1f);
	float fVelForLeans = m_bDrivingAggressively ? fMinVehicleVelocityForLeans : fMaxVehicleVelocityForLeans;
	if(m_pVehicle->GetVelocityIncludingReferenceFrame().Mag2() > rage::square(fVelForLeans))
	{
		//! Flag that we have gone fast, to prevent going back to lowrider anims whilst still driving aggessively.
		bDrivingAggressivelyThisFrame = true;
		m_bDrivingAggressively = true;
		m_iTimeStoppedDrivingAtAggressiveSpeed = 0;
	}
	else
	{
		if(m_iTimeStoppedDrivingAtAggressiveSpeed == 0)
		{
			m_iTimeStoppedDrivingAtAggressiveSpeed = fwTimer::GetTimeInMilliseconds();
		}
	}

	//! only consider moving after a couple of secs, so that we don't re-enter lowrider anims too soon after doing handbrake turns etc.
	bool bConsiderPedWantsToMove = true;
	TUNE_GROUP_INT(LOWRIDER_TUNE, iTimeToConsiderBrakingForAggressiveDriving, 2000, 0, 10000, 100);
	if(!bDrivingAggressivelyThisFrame && (fwTimer::GetTimeInMilliseconds() > (m_iTimeStoppedDrivingAtAggressiveSpeed + iTimeToConsiderBrakingForAggressiveDriving)))
	{
		bConsiderPedWantsToMove = false;
	}

	TUNE_GROUP_FLOAT(LOWRIDER_TUNE, fAggressiveAccelerateThreshold, 0.9f, 0.0f, 1.0f, 0.1f);
	TUNE_GROUP_INT(LOWRIDER_TUNE, iAggressiveDrivingAccelerationTime, 500, 0, 10000, 100);
	if(bConsiderPedWantsToMove || CheckIfPedWantsToMove(rPed, *m_pVehicle) > fAggressiveAccelerateThreshold)
	{
		if(m_iTimeStartedAcceleratingAggressively == 0)
		{
			m_iTimeStartedAcceleratingAggressively = fwTimer::GetTimeInMilliseconds();
		}
		if( (fwTimer::GetTimeInMilliseconds() - m_iTimeStartedAcceleratingAggressively) > iAggressiveDrivingAccelerationTime)
		{
			m_iTimeLastAcceleratedAggressively = fwTimer::GetTimeInMilliseconds();
		}
	}
	else
	{
		m_iTimeStartedAcceleratingAggressively = 0;
	}

	TUNE_GROUP_INT(LOWRIDER_TUNE, fAggressiveAccelerationTimeout, 1000, 0, 10000, 100);
	if(!bDrivingAggressivelyThisFrame && (fwTimer::GetTimeInMilliseconds() > (m_iTimeLastAcceleratedAggressively + fAggressiveAccelerationTimeout)) )
	{
		m_bDrivingAggressively = false;
	}
}

void CTaskMotionInAutomobile::ProcessDefaultClipSet()
{
	//Ensure the move network is active.
	if(!m_moveNetworkHelper.IsNetworkActive())
	{
		return;
	}

	const bool bIsBikeOrQuad = m_pVehicle->IsBikeOrQuad();
	if (bIsBikeOrQuad)
	{
		if (m_nDefaultClipSet == DCS_LowriderLean || m_nDefaultClipSet == DCS_LowriderLeanAlt)
		{
			m_MinTimeInAltLowriderClipsetTimer.Tick();
		}
	}

	CPed& rPed = *GetPed();

	if (WantsToDuck(rPed))
	{
		rPed.SetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_CAMERA_VIEW_MODE_CYCLE);
		m_fDuckTimer = ms_Tunables.m_MinTimeDucked;	
	}
	else if (m_fDuckTimer >= 0.0f)
	{
		m_fDuckTimer -= fwTimer::GetTimeStep();
	}

	bool bSkipDuckIntroOutros = false;
	if (ShouldSkipDuckIntroOutros())
	{
		bSkipDuckIntroOutros = true;
	}
	m_moveNetworkHelper.SetFlag(bSkipDuckIntroOutros, ms_SkipDuckIntroOutrosFlagId);

	// Initialize alternate lowrider clipset timers.
	if (!ShouldUseLowriderLeanAltClipset() && m_fTimeToTriggerAltLowriderClipset == -1.0f && m_nDefaultClipSet != DCS_LowriderLeanAlt && !IsUsingFirstPersonAnims())
	{
		TUNE_GROUP_FLOAT(LOWRIDER_TUNE, fAltClipsetTimeMin, 0.5f, 0.0f, 60.0f, 0.01f);
		TUNE_GROUP_FLOAT(LOWRIDER_TUNE, fAltClipsetTimeMax, 5.0f, 0.0f, 60.0f, 0.01f);

		TUNE_GROUP_FLOAT(LOWRIDER_BIKE_TUNE, fAltBikeClipsetTimeMin, 1.0f, 0.0f, 60.0f, 0.01f);
		TUNE_GROUP_FLOAT(LOWRIDER_BIKE_TUNE, fAltBikeClipsetTimeMax, 3.0f, 0.0f, 60.0f, 0.01f);
		const float fMin = bIsBikeOrQuad ? fAltBikeClipsetTimeMin : fAltClipsetTimeMin;
		const float fMax = bIsBikeOrQuad ? fAltBikeClipsetTimeMax : fAltClipsetTimeMax;

		m_fTimeToTriggerAltLowriderClipset = fwRandom::GetRandomNumberInRange(fMin, fMax);
	}

	ProcessDrivingAggressively();

	//Get the default clip set to use.
	DefaultClipSet nDefaultClipSet = GetDefaultClipSetToUse();

	//Cache previous default clipset (used to toggle lowrider transitions).
	DefaultClipSet nPrevDefaultClipSet = m_nDefaultClipSet;

	// Increment lowrider alternate clipset timer.
	if (ShouldUseLowriderLeanAltClipset() && nDefaultClipSet == DCS_LowriderLean && m_fAltLowriderClipsetTimer <= m_fTimeToTriggerAltLowriderClipset && !IsUsingFirstPersonAnims())
	{
		m_fAltLowriderClipsetTimer += GetTimeStep();
	}
	else if (nDefaultClipSet == DCS_LowriderLeanAlt || IsUsingFirstPersonAnims())
	{
		// Reset alternate lowrider anim timer parameters once we've started to use them (or if we're using first person anims).
		m_fTimeToTriggerAltLowriderClipset = -1.0f;
		m_fAltLowriderClipsetTimer = 0.0f;
	}
	else if (bIsBikeOrQuad)
	{
		m_fAltLowriderClipsetTimer = 0.0f;
	}

	//Get the clip set.
	fwMvClipSetId clipSetId = GetDefaultClipSetId(nDefaultClipSet);
	taskAssertf(clipSetId != CLIP_SET_ID_INVALID, "Invalid clipset for default clipset %i, vehicle %s, seat clipset %s, m_pSeatOverrideAnimInfo ? %s, %s ped %s seat index %i", 
		nDefaultClipSet, m_pVehicle ? m_pVehicle->GetModelName() : "NULL", m_pVehicle->GetSeatAnimationInfo(&rPed) ? m_pVehicle->GetSeatAnimationInfo(&rPed)->GetSeatClipSetId().GetCStr() : "NULL", 
		m_pSeatOverrideAnimInfo ? "TRUE" : "FALSE", rPed.IsNetworkClone() ? "CLONE" : "LOCAL", rPed.GetDebugName(), m_pVehicle ? m_pVehicle->GetSeatManager()->GetPedsSeatIndex(&rPed) : -99);

	//Check if the clip set requires streaming.
	bool bRequiresStreaming = (nDefaultClipSet != DCS_Base);
	bool bIsStreamedIn = !bRequiresStreaming;
	if(!bIsStreamedIn)
	{
		//Calculate the priority.
		eStreamingPriority nStreamingPriority = SP_Invalid;

		//Check if we can use high priority.
		bool bCanUseHighPriority = ((nDefaultClipSet == DCS_Panic) || (nDefaultClipSet == DCS_Agitated) || (nDefaultClipSet == DCS_LowriderLean) || (nDefaultClipSet == DCS_LowriderLeanAlt));
		if(bCanUseHighPriority)
		{
			//Check if we are the player.
			if(rPed.IsAPlayerPed())
			{
				nStreamingPriority = SP_High;
			}
			else
			{
				//Check if we are close to the camera.
				ScalarV scDistSq = DistSquared(rPed.GetTransform().GetPosition(), VECTOR3_TO_VEC3V(camInterface::GetPos()));
				static dev_float s_fMaxDistance = 20.0f;
				ScalarV scMaxDistSq = ScalarVFromF32(square(s_fMaxDistance));
				if(IsLessThanAll(scDistSq, scMaxDistSq))
				{
					nStreamingPriority = SP_High;
				}
			}
		}

		//Stream in the clip set.
		bIsStreamedIn = CTaskVehicleFSM::RequestVehicleClipSetReturnIsLoaded(clipSetId, nStreamingPriority);

#if __ASSERT
		if (nDefaultClipSet == DCS_Ducked && bIsStreamedIn && rPed.GetIsDrivingVehicle())
		{
			static const u32 NUM_CLIPS = 6;
			static fwMvClipId CLIPS[NUM_CLIPS] = {
				fwMvClipId("sit"),
				fwMvClipId("sit_duck_enter"),
				fwMvClipId("sit_duck_exit"),
				fwMvClipId("steer_lean_left"),
				fwMvClipId("steer_lean_right"),
				fwMvClipId("steer_no_lean")
			};

			for (s32 i=0; i<NUM_CLIPS; ++i)
			{
				fwMvClipId clipId = CLIPS[i];
				if (!taskVerifyf(fwClipSetManager::GetClip(clipSetId, clipId), "Couldn't find clip %s in clipset %s", clipId.GetCStr(), clipSetId.GetCStr()))
				{
					bIsStreamedIn = false;	// Prevent duck anims from playing
				}
			}
		}
#endif // __ASSERT
	}

	bool bRearSeat = false;
	s32 iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(GetPed());
	if( m_pVehicle->IsSeatIndexValid(iSeatIndex) && m_pVehicle->GetSeatInfo(iSeatIndex)) 
	{
		if(!m_pVehicle->GetSeatInfo(iSeatIndex)->GetIsFrontSeat())
		{
			bRearSeat = true; 
		}
	}

	//Check if the default clip set is changing, and streamed in.
	if(bIsStreamedIn && (nDefaultClipSet != m_nDefaultClipSet) && (!m_bIsDucking || IsMoveNetworkStateValid()))
	{
		//Set the default clip set.
		m_nDefaultClipSet = nDefaultClipSet;

		if (bIsBikeOrQuad)
		{
			m_MinTimeInAltLowriderClipsetTimer.Reset(ms_Tunables.m_MinTimeInLowRiderClipset);
		}

		//Set the clip set.
		m_moveNetworkHelper.SetClipSet(clipSetId);

		const bool bIsAgitated = m_nDefaultClipSet == DCS_Agitated;
		const bool bIsDucking = m_nDefaultClipSet == DCS_Ducked;
		bool bAgitatedOrDuckedAnims = (bIsAgitated || bIsDucking);

		bool bDontUsePassengerLeanAnims = bAgitatedOrDuckedAnims && bRearSeat;
		DontUsePassengerLeanAnims(bDontUsePassengerLeanAnims);
		
		//Check if we should use lean steer anims.
		bool bCanUseLeanSteerAnims = (nDefaultClipSet == DCS_Base) || bIsDucking || (bIsAgitated && !ShouldUseBaseIdle());
		m_bUseLeanSteerAnims = (bCanUseLeanSteerAnims && m_pVehicle->GetLayoutInfo()->GetUseLeanSteerAnims());
		UseLeanSteerAnims(m_bUseLeanSteerAnims);

		bool bUseIdleSteerAnims = m_pVehicle->InheritsFromBoat() && !m_pVehicle->GetLayoutInfo()->GetUseLeanSteerAnims();
		UseIdleSteerAnims(bUseIdleSteerAnims);

		bool bUsingLowriderArmClipset = (m_nDefaultClipSet == DCS_LowriderLeanAlt);
		m_moveNetworkHelper.SetFlag(bUsingLowriderArmClipset, CTaskMotionInVehicle::ms_UsingLowriderArmClipset);

		//Note that the default clip set changed this frame.
		m_bDefaultClipSetChangedThisFrame = true;
	}
	else
	{
		//Check if we are agitated.
		const bool bIsDucking = m_nDefaultClipSet == DCS_Ducked;
		if(m_nDefaultClipSet == DCS_Agitated || bIsDucking)
		{
			//Check if we should use lean steer anims.
			bool bUseLeanSteerAnims = (bIsDucking || !ShouldUseBaseIdle()) && m_pVehicle->GetLayoutInfo()->GetUseLeanSteerAnims();
			if(bUseLeanSteerAnims != m_bUseLeanSteerAnims)
			{
				bool bOldLeanStearAnims = m_bUseLeanSteerAnims;
				//Use the lean steer anims.
				UseLeanSteerAnims(bUseLeanSteerAnims);

				if(bOldLeanStearAnims != m_bUseLeanSteerAnims)
				{
					//Note that the default clip set changed this frame.
					m_bDefaultClipSetChangedThisFrame = true;
				}
			}

			bool bDontUsePassengerLeanAnims = bRearSeat;
			if(bDontUsePassengerLeanAnims != m_bDontUsePassengerLeanAnims)
			{
				bool bOldDontUsePassengerLeanAnims = m_bDontUsePassengerLeanAnims;

				DontUsePassengerLeanAnims(bDontUsePassengerLeanAnims);

				if(bOldDontUsePassengerLeanAnims != m_bDontUsePassengerLeanAnims)
				{
					//Note that the default clip set changed this frame.
					m_bDefaultClipSetChangedThisFrame = true;
				}
			}
		}
	}

	// B*2484255 - Blend in/out of lowrider sit/lean steer anims.
	if (bIsStreamedIn && (m_nDefaultClipSet == DCS_LowriderLean || m_nDefaultClipSet == DCS_LowriderLeanAlt))
	{
		// Switch back to sit anims if we've stopped steering and haven't modified suspension for a short amount of time.
		TUNE_GROUP_INT(LOWRIDER_TUNE, iReturnToSitTimer, 2000, 0, 10000, 1);
		u32 iTimeInMilliseconds = fwTimer::GetTimeInMilliseconds();
		bool bUseLeanSteerAnims = (!ShouldUseBaseIdle() || ((m_iTimeLastModifiedHydraulics + iReturnToSitTimer) > iTimeInMilliseconds)) && m_pVehicle->GetLayoutInfo()->GetUseLeanSteerAnims();
		if (bUseLeanSteerAnims != m_bUseLeanSteerAnims)
		{
			// Only blend to sit once the lateral lean has reset back to middle.
			if ((!bUseLeanSteerAnims && IsClose(m_fLowriderLeanX, 0.5f, SMALL_FLOAT)) || bUseLeanSteerAnims)
			{
				m_bUseLeanSteerAnims = bUseLeanSteerAnims;
				UseLeanSteerAnims(bUseLeanSteerAnims);
				//Note that the default clip set changed this frame.
				m_bDefaultClipSetChangedThisFrame = true;
			}
		}
	}
	
	bool bUsingLowriderLean = (m_nDefaultClipSet == DCS_LowriderLean || m_nDefaultClipSet == DCS_LowriderLeanAlt) ? true : false;
	m_moveNetworkHelper.SetFlag(bUsingLowriderLean, CTaskMotionInVehicle::ms_IsLowriderId);

	// Trigger transitions to/from alternate lowrider lean clipset.
	bool bUseToArmTransition = m_nDefaultClipSet == DCS_LowriderLeanAlt && nPrevDefaultClipSet == DCS_LowriderLean;
	bool bUseToSteerTransition = m_nDefaultClipSet == DCS_LowriderLean && nPrevDefaultClipSet == DCS_LowriderLeanAlt;

	bool bUseActionTransition = ShouldUseLowriderAltClipsetActionTransition();
	m_moveNetworkHelper.SetFlag(bUseToArmTransition, CTaskMotionInVehicle::ms_LowriderTransitionToArm);
	m_moveNetworkHelper.SetFlag(bUseToSteerTransition, CTaskMotionInVehicle::ms_LowriderTransitionToSteer);
	m_moveNetworkHelper.SetBoolean(CTaskMotionInVehicle::ms_IsDoingDriveby, bUseActionTransition);

	if (bUseToArmTransition || bUseToSteerTransition)
	{
		m_bPlayingLowriderClipsetTransition = true;
	}

	const bool bWasDucking = m_bIsDucking;
	const bool bIsDucking = m_nDefaultClipSet == DCS_Ducked ? true : false;
	m_moveNetworkHelper.SetFlag(bIsDucking, ms_IsDuckingFlagId);
	m_moveNetworkHelper.SetFlag(!bIsDucking && bWasDucking, ms_PlayDuckOutroFlagId);
	if (bIsDucking)
	{
		clipSetId = GetDefaultClipSetId(m_nDefaultClipSet);
		m_moveNetworkHelper.SetClipSet(clipSetId, ms_DuckClipSetId);
		m_fTimeDucked += GetTimeStep();
	}
	else
	{
		m_fTimeDucked = 0.0f;
	}

	if (m_fTimeDucked < ms_Tunables.m_MaxDuckBreakLockOnTime && bIsDucking)
	{
		rPed.SetPedResetFlag(CPED_RESET_FLAG_BreakTargetLock, true);
	}

	m_bWasDucking = m_bIsDucking;
	m_bIsDucking = bIsDucking;
	
	if (rPed.IsLocalPlayer())
	{
		if (m_bIsDucking)
			rPed.SetHeadIkBlocked();
		rPed.SetPedConfigFlag(CPED_CONFIG_FLAG_IsDuckingInVehicle, m_bIsDucking);
		rPed.SetPedConfigFlag(CPED_CONFIG_FLAG_UsingLowriderLeans, bUsingLowriderLean);
		rPed.SetPedConfigFlag(CPED_CONFIG_FLAG_UsingAlternateLowriderLeans, (m_nDefaultClipSet == DCS_LowriderLeanAlt));
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::ShouldSkipDuckIntroOutros() const
{
	// Some vehicle should always use the anims, because blending on some of the super-low vehicles causes hair clipping
	if ((MI_CAR_PROTO.IsValid() && m_pVehicle->GetModelIndex() == MI_CAR_PROTO) || (MI_CAR_VISIONE.IsValid() && m_pVehicle->GetModelIndex() == MI_CAR_VISIONE))
	{
		return false;
	}

	TUNE_GROUP_FLOAT(IN_VEHICLE_TUNE, DO_STRAIGHT_BLEND_LEFT_ANGLE, 0.25f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(IN_VEHICLE_TUNE, DO_STRAIGHT_BLEND_RIGHT_ANGLE, 0.75f, 0.0f, 1.0f, 0.01f);
	return m_fSteering < DO_STRAIGHT_BLEND_LEFT_ANGLE || m_fSteering > DO_STRAIGHT_BLEND_RIGHT_ANGLE;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::WantsToDuck(const CPed& rPed, bool bIgnoreCustomVehicleChecks)
{
	if (rPed.IsLocalPlayer())
	{
		if(rPed.GetPlayerInfo()->CanPlayerPerformVehicleMelee())
		{
			return false;
		}

		const CControl* pControl = rPed.GetControlFromPlayer();
		const CVehicle* pVehicle = rPed.GetVehiclePedInside();

		if (MI_CAR_DELUXO.IsValid() && pVehicle && pVehicle->GetModelIndex() == MI_CAR_DELUXO)
		{
			return false;
		}

		// disabled ducking on vehicles which have the side shunt mod
		if( pVehicle && pVehicle->HasSideShunt() )
		{
			return false;
		}

		float duckValue = pControl->GetVehicleDuck().GetNorm();

		// B*2170339: Use handbrake input for ducking if ped is a passenger (since this is unused for passengers).
		if (!rPed.GetIsDrivingVehicle() && CPauseMenu::GetMenuPreference(PREF_ALTERNATE_HANDBRAKE))
		{
			duckValue = pControl->GetVehicleHandBrake().GetNorm();
		}

		if(!bIgnoreCustomVehicleChecks && pVehicle && (pVehicle->InheritsFromPlane() || pVehicle->InheritsFromHeli() || pVehicle->InheritsFromBlimp()))
		{
			duckValue = pControl->GetVehicleFlyDuck().GetNorm();
		}

		if (pControl && duckValue > ms_Tunables.m_DuckControlThreshold)
		{
			return true;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::CanPanic() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Ensure the ped is not a player.
	if(pPed->IsPlayer())
	{
		return false;
	}

	//Ensure the flag is not set.
	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisablePanicInVehicle))
	{
		return false;
	}
	
	//Ensure the panic clip set is valid.
	fwMvClipSetId panicClipSetId = GetPanicClipSet();
	if(panicClipSetId == CLIP_SET_ID_INVALID)
	{
		return false;
	}
	
	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::ShouldPanic() const
{
	//Ensure we can panic.
	if(!CanPanic())
	{
		return false;
	}

#if __BANK
	//Check if we are forcing panic in vehicle.
	TUNE_GROUP_BOOL(PANIC_IN_VEHICLE, bForce, false);
	if(bForce)
	{
		return true;
	}
#endif

	const CPed& rPed = *GetPed();
	//Check if the panic flag is set, or we're an ai ped being electrocuted (we don't want the panic to blend back in when the electrocute anim finishes as it will pop in)
	if(rPed.GetPedResetFlag(CPED_RESET_FLAG_PanicInVehicle) || rPed.GetPedResetFlag(CPED_RESET_FLAG_BeingElectrocuted))
	{
		return true;
	}

	//Check if we should panic due to the driver getting jacked.
	if(ShouldPanicDueToDriverBeingJacked())
	{
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::ShouldPanicDueToDriverBeingJacked() const
{
	//Ensure the vehicle is valid.
	if(!m_pVehicle)
	{
		return false;
	}
	
	//Ensure the driver is valid.
	CPed* pDriver = m_pVehicle->GetDriver();
	if(!pDriver)
	{
		return false;
	}
	
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Ensure we are not the driver.
	if(pPed == pDriver)
	{
		return false;
	}
	
	//Ensure the driver is exiting the vehicle.
	if(!pDriver->IsExitingVehicle())
	{
		return false;
	}
	
	//Ensure the driver is being jacked.
	if(!pDriver->IsBeingJackedFromVehicle())
	{
		return false;
	}
	
	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::CanBeAgitated() const
{
	//Ensure the clip set is valid.
	fwMvClipSetId clipSetId = GetAgitatedClipSet();
	if(clipSetId == CLIP_SET_ID_INVALID)
	{
		return false;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::ShouldBeAgitated() const
{
	//Ensure we can be agitated.
	if(!CanBeAgitated())
	{
		return false;
	}

#if __BANK
	//Check if we are forcing it.
	TUNE_GROUP_BOOL(AGITATED_IN_VEHICLE, bForce, false);
	if(bForce)
	{
		return true;
	}
#endif

	if(GetPed()->GetPedResetFlag(CPED_RESET_FLAG_PlayAgitatedAnimsInVehicle))
	{
		return true;
	}

	if (PlayerShouldBeAgitated())
	{
		return true;
	}

	return false;
}

bool CTaskMotionInAutomobile::PlayerShouldBeAgitated() const
{
	//Check if we are a wanted player.
	if(GetPed()->IsPlayer())
	{
		const CWanted* pPlayerWanted = GetPed()->GetPlayerWanted();
		if(pPlayerWanted && pPlayerWanted->GetWantedLevel() > WANTED_CLEAN && !pPlayerWanted->PoliceBackOff())
		{
			bool bShouldBeAgitatedFromWanted = true;

			// If we're in a convertible lowrider (ie should use alt clipset), stop being agitated if cops are in searching mode (so we can go back to putting arm on window).
			if (m_pVehicle->InheritsFromAutomobile())
			{
				const CAutomobile* pAutomobile = static_cast<const CAutomobile*>(m_pVehicle.Get());
				if(pAutomobile->HasHydraulicSuspension() && ShouldUseLowriderLeanAltClipset(false) && pPlayerWanted->CopsAreSearching())
				{
					bShouldBeAgitatedFromWanted = false;
				}
			}

			if (bShouldBeAgitatedFromWanted)
			{
				return true;
			}
		}
	}
	
	//Check if we have recently done a drive-by (AI and non-drivers only)
	if(!GetPed()->IsPlayer() || !GetPed()->GetIsDrivingVehicle())
	{
		const s32 iTimeSinceDriveBy = fwTimer::GetTimeInMilliseconds() - m_uLastDriveByTime;
		if (m_uLastDriveByTime > 0 && (iTimeSinceDriveBy < ms_Tunables.m_MinTimeSinceDriveByForAgitate))
		{
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::CanBeDucked(bool bIgnoreDrivebyCheck) const
{
	TUNE_GROUP_INT(DUCKING_TUNE, DELAY_AFTER_DISABLE_CONTROL_BEFORE_ALLOWING_DUCK, 500, 0, 2000, 100);
	if ((fwTimer::GetTimeInMilliseconds() - m_TimeControlLastDisabled) < DELAY_AFTER_DISABLE_CONTROL_BEFORE_ALLOWING_DUCK)
	{
		aiDebugf1("Disabling ducking because control was recently disabled");
		return false;
	}

	//! Check if we have anims to do ducking in 1st person.
	if (IsUsingFirstPersonSteeringAnims())
	{
		const fwMvClipSetId fpClipsetId = GetDefaultClipSetId(DCS_Idle);
		if(fpClipsetId != CLIP_SET_ID_INVALID)
		{
			static fwMvClipId fpClip("pov_steer_duck",0xf2a994e0);
			const crClip* pClip = fwClipSetManager::GetClip( fpClipsetId, fpClip );
			if(!pClip)
			{
				return false;
			}
		}
	}

	if (!bIgnoreDrivebyCheck && GetPed()->GetPedResetFlag(CPED_RESET_FLAG_IsDoingDriveby))
	{
		return false;
	}

	if (GetState() == State_StartEngine || GetState() == State_Hotwiring)
	{
		return false;
	}

	//Ensure the clip set is valid.
	fwMvClipSetId clipSetId = GetDuckedClipSet();
	if(clipSetId == CLIP_SET_ID_INVALID)
	{
		return false;
	}

	const CPed& rPed = *GetPed();
	if (rPed.GetIsInVehicle())
	{
		CVehicle* pVehicle = rPed.GetVehiclePedInside();
		if (pVehicle)
		{
			//! If we have just entered, don't duck if we are about to start engine/hotwire.
			if ( (CTaskMotionInVehicle::CheckForHotwiring(*pVehicle, rPed) || CTaskMotionInVehicle::CheckForStartingEngine(*pVehicle, rPed)) && (GetTimeRunning() < 0.1f) )
			{
				return false;
			}

			// B*1929070: Disable ducking in stromberg in submarine mode (as fire button mapping conflicts with ducking)
			if (pVehicle->IsInSubmarineMode())
			{
				return false;
			}
			// B*2003480, also for driver of planes
			else if ((pVehicle->InheritsFromPlane() || pVehicle->InheritsFromHeli()) && pVehicle->GetDriver() == &rPed)
			{
				return false;
			}
			else if(pVehicle->InheritsFromAutomobile() && static_cast<CAutomobile*>(pVehicle)->HasHydraulicSuspension() && pVehicle->GetDriver() == &rPed)		// Disable ducking when in vehicles that have hydraulics.
			{
				return false;
			}
			else if(pVehicle->InheritsFromAutomobile() && static_cast<CAutomobile*>(pVehicle)->CanDeployParachute())
			{
				return false;
			}
			else if (pVehicle->InheritsFromAutomobile() && static_cast<CAutomobile*>(pVehicle)->HasRocketBoost() && static_cast<CAutomobile*>(pVehicle)->HasJump()) // SCRAMJET
			{
				return false;
			}
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::ShouldBeDucked() const
{
	//Ensure we can be ducked.
	if(!CanBeDucked())
	{
		return false;
	}

	const CPed& rPed = *GetPed();
	if (rPed.IsLocalPlayer())
	{
		if (m_fDuckTimer > 0.0f ? true : false)
		{
			return true;
		}
	}
	else
	{
		return rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_IsDuckingInVehicle);
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::ShouldUseBaseIdle() const
{
	//Grab the ped.
	const CPed& rPed = *GetPed();

	bool bSteeringConditionPassed = true;
	bool bSpeedConditionPassed = true;

	// If the ped is driving the vehicle and turning the wheel too much, don't use the idles.
	// If the ped is not driving and is in a boat do the same check
	if( m_pVehicle->GetDriver() == &rPed || (m_pVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT && !m_pVehicle->GetIsJetSki()) )
	{
		float fSteeringAngle = m_pVehicle->GetSteerAngle();
		if (m_pVehicle->InheritsFromSubmarineCar() && m_pVehicle->IsInSubmarineMode()) 
			fSteeringAngle = m_pVehicle->GetSubHandling()->GetYawControl() * m_pVehicle->pHandling->m_fSteeringLock;

		if (Abs(fSteeringAngle) > ms_Tunables.m_MaxSteeringAngleForSitIdles)
		{
			bSteeringConditionPassed = false;
		}

		if (MI_PLANE_MICROLIGHT.IsValid() && m_pVehicle->GetModelIndex() == MI_PLANE_MICROLIGHT && !IsClose(m_fBodyLeanX,0.5f,0.001f))
		{
			bSteeringConditionPassed = false;
		}

		if (m_bUseLeanSteerAnims && !m_pVehicle->GetIsJetSki())
		{
			const u32 uCurrentTime = fwTimer::GetTimeInMilliseconds();
			if ((uCurrentTime - m_uLastSteeringNonCentredTime) <  ms_Tunables.m_MinCentredSteeringAngleTimeForSitIdles)
			{
				bSteeringConditionPassed = false;
			}
		}
	}

	//If vehicle is above the velocity threshold, don't use the idles.
	if (m_pVehicle->GetVelocityIncludingReferenceFrame().Mag2() > rage::square(ms_Tunables.m_MaxVelocityForSitIdles))
	{
		bSpeedConditionPassed = false;
	}

	// Don't use idles during agitated animations on boats, otherwise we get animation pops during speed changes.
	if (m_pVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT && !m_pVehicle->GetIsJetSki() && m_nDefaultClipSet == DCS_Agitated)
	{
		bSpeedConditionPassed = false;
	}

	if(m_pVehicle->GetIsJetSki() && m_fJetskiStandPoseBlendTime > 0.0f)
	{
		bSpeedConditionPassed = false;
	}

	return bSteeringConditionPassed && bSpeedConditionPassed;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInAutomobile::DontUsePassengerLeanAnims(bool bValue)
{
	//Set the value.
	m_bDontUsePassengerLeanAnims = bValue;

	//Set the flag.
	m_moveNetworkHelper.SetFlag(bValue, ms_DontUsePassengerLeanAnimsFlagId);
}

void CTaskMotionInAutomobile::UseLeanSteerAnims(bool bValue)
{
	const CPed& rPed = *GetPed();
	if( m_pVehicle->GetDriver() == &rPed || (m_pVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT && !m_pVehicle->GetIsJetSki()) )
	{
		//Set the value.
		m_bUseLeanSteerAnims = bValue;

		//Set the flag.
		m_moveNetworkHelper.SetFlag(bValue, ms_UseLeanSteerAnimsFlagId);
	}
}

void CTaskMotionInAutomobile::UseIdleSteerAnims(bool bValue)
{
	const CPed& rPed = *GetPed();
	if (m_pVehicle->GetDriver() == &rPed)
	{
		//Set the value.
		m_bUseIdleSteerAnims = bValue;

		//Set the flag.
		m_moveNetworkHelper.SetFlag(bValue, ms_UseIdleSteerAnimsFlagId);
	}
}

////////////////////////////////////////////////////////////////////////////////

float CTaskMotionInAutomobile::GetBodyMovementY()
{
	if (m_pVehicle && ShouldUseBikeInVehicleAnims(*m_pVehicle))
	{
		return m_fPitchAngle;
	}
	else
	{
		return m_fBodyLeanY;
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::ProcessImpactReactions(CPed& rPed, CVehicle& rVeh)
{
	bool bThroughWindscreen = CTaskMotionInVehicle::CheckForThroughWindow(rPed, rVeh, VEC3V_TO_VECTOR3(m_vLastFramesVehicleVelocity), m_vThrownForce);

#if __BANK
	TUNE_GROUP_BOOL(THROUGH_WINDSCREEN, bTestThroughWindow, false);
	TUNE_GROUP_FLOAT(THROUGH_WINDSCREEN, fTestThroughWindowImpulse, 20.0f, 0.0f, 100.0f, 0.1f);
	const CControl* pControl = rPed.GetControlFromPlayer();
	if(bTestThroughWindow && pControl && pControl->GetPedJump().IsPressed())
	{
		m_vThrownForce = MAT34V_TO_MATRIX34(rVeh.GetMatrix()).b;
		m_vThrownForce *= fTestThroughWindowImpulse;
		bThroughWindscreen = true;
	}
#endif

	if (bThroughWindscreen)
	{
		VehicleEnterExitFlags vehicleFlags;
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::ThroughWindscreen);
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontWaitForCarToStop);
		CEventGivePedTask givePedTask(PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP, rage_new CTaskExitVehicle(&rVeh, vehicleFlags), true );
		rPed.GetPedIntelligence()->AddEvent(givePedTask);
		TUNE_GROUP_BOOL(THROUGH_WINDSCREEN, DO_INSTANT_AI_UPDATE, true);
		if (DO_INSTANT_AI_UPDATE)
			rPed.SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAIUpdate, true);
		TUNE_GROUP_BOOL(THROUGH_WINDSCREEN, DO_INSTANT_ANIM_UPDATE, true);	// Causes large velocities at low frame rates if true
		if (DO_INSTANT_ANIM_UPDATE)
			rPed.SetPedResetFlag(CPED_RESET_FLAG_ePostCameraAnimUpdateUseZeroTimestep, true);

		return true;
	}
	else if (CTaskMotionInVehicle::CheckForShunt(rVeh, rPed, m_vCurrentVehicleAcceleration.GetZf()))
	{
		rPed.SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAIUpdate, true);
		rPed.SetPedResetFlag(CPED_RESET_FLAG_ePostCameraAnimUpdateUseZeroTimestep, true);
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInAutomobile::ProcessDamageFromImpacts(CPed& rPed, const CVehicle& rVeh, bool bGoingThroughWindscreen)
{
	if (!rPed.IsNetworkClone() && !rPed.ShouldBeDead() 
		&& !rVeh.InheritsFromBicycle() && !rVeh.InheritsFromBike()
		&& !rPed.GetPedResetFlag(CPED_RESET_FLAG_IsEnteringOrExitingVehicle) && !rPed.GetPedResetFlag(CPED_RESET_FLAG_IsInCombat)
		&& ((fwTimer::GetTimeInMilliseconds() - m_uTaskStartTimeMS)/1000.0f) > ms_Tunables.m_MinTimeInTaskToCheckForDamage)	// Want to avoid the collision when setting the ped on the vehicle
	{
		const CCollisionHistory* pCollisionHistory = rVeh.GetFrameCollisionHistory();
	//	const CCollisionRecord* pRecord = pCollisionHistory->GetMostSignificantCollisionRecord();
		if (pCollisionHistory->GetCollisionImpulseMagSum() > 1.f)
		{
			// Actual vel change should be divided by timestep

			// Don't divide by timestep, we are using an impulse
			// Use the sum of all impulses rather than just the most significant one
			float fImpulseMagSum = pCollisionHistory->GetCollisionImpulseMagSum();

			// Disregard any force that come from a building and above, we are probably stuck under something and we get crazy high forces downwards in that case
			bool bDisregardImpulse = false;
			int nRecords = 0;
			const CCollisionRecord* pBuildingRecord = pCollisionHistory->GetFirstBuildingCollisionRecord();
			while (pBuildingRecord)
			{
				// In physics the Z is upwards
				if (pBuildingRecord->m_MyCollisionNormal.Dot(ZAXIS) < -0.94f)
				{
					bDisregardImpulse = true;
					fImpulseMagSum -= pBuildingRecord->m_fCollisionImpulseMag;
				}

				pBuildingRecord = pBuildingRecord->GetNext();
				++nRecords;
			}

			if (nRecords == 1 && bDisregardImpulse)
				fImpulseMagSum = 0.f;

			const float fVelocityChange = fImpulseMagSum * rVeh.GetInvMass();// pRecord->m_fCollisionImpulseMag * rVeh.GetInvMass();

			float fStrengthStatDamageScale = 1.0f;

			if (rPed.IsLocalPlayer())
			{
				// High strength stat -> lower damage scale
				float fStrengthValue = 1.0f - rage::Clamp(static_cast<float>(StatsInterface::GetIntStat(STAT_STRENGTH.GetStatId())) / 100.0f, 0.0f, 1.0f);
				fStrengthStatDamageScale = CPlayerInfo::ms_Tunables.m_MinVehicleCollisionDamageScale + ((CPlayerInfo::ms_Tunables.m_MaxVehicleCollisionDamageScale - CPlayerInfo::ms_Tunables.m_MinVehicleCollisionDamageScale) * fStrengthValue);
			}

			const float fDamageScale = rPed.IsAPlayerPed() ? ms_Tunables.m_ShuntDamageMultiplierPlayer : ms_Tunables.m_ShuntDamageMultiplierAI;
			float fDamageTaken = fVelocityChange * fStrengthStatDamageScale * fDamageScale;
			const float fMinDamageTaken = rPed.IsAPlayerPed() ? ms_Tunables.m_MinDamageTakenToApplyDamagePlayer : ms_Tunables.m_MinDamageTakenToApplyDamageAI;
			aiDebugf1("Damage Taken : %.2f", fDamageTaken);
#if __DEV
			TUNE_GROUP_FLOAT(VEHICLE_DAMAGE_TUNE, LARGE_DAMAGE_VALUE, 5.0f, 0.0f, 5.0f, 0.01f);
			if (fDamageTaken > LARGE_DAMAGE_VALUE)
			{
				taskDisplayf("Frame : %u, LARGE DAMAGE to ped %p : %.2f (min: %.2f / max: %.2f)", fwTimer::GetFrameCount(), &rPed, fDamageTaken, ms_Tunables.m_MinDamageToCheckForRandomDeath, ms_Tunables.m_MaxDamageToCheckForRandomDeath);
			}
#endif // __DEV
			if (rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_WillTakeDamageWhenVehicleCrashes) && fDamageTaken > fMinDamageTaken)
			{
				bool bIsDamageValid = true;
				// We want to limit the damage to X amount per Y seconds for the player
				if (rPed.IsLocalPlayer() && !bGoingThroughWindscreen)
				{
					TUNE_GROUP_FLOAT(VEHICLE_DAMAGE_TUNE, TICK_DAMAGE_PLAYER, 10.f, 0.0f, 100.f, 0.1f);
					TUNE_GROUP_INT(VEHICLE_DAMAGE_TUNE, TICK_TIME_PLAYER, 2000, 0, 10000, 100);

					// Don't hurt too often and don't kill the player
					if ((fwTimer::GetTimeInMilliseconds() < m_uLastDamageTakenTime + TICK_TIME_PLAYER) || 
						(rPed.GetHealth() <= rPed.GetDyingHealthThreshold() + TICK_DAMAGE_PLAYER * 2.f))
					{
						bIsDamageValid = false;
					}
					else
					{
						fDamageTaken = Min(fDamageTaken, TICK_DAMAGE_PLAYER);
						m_uLastDamageTakenTime = fwTimer::GetTimeInMilliseconds();
					}
				}

				if(rPed.IsLocalPlayer() && ( rVeh.HasRammingScoop() || rVeh.HasRamp() ) && pCollisionHistory->GetFirstVehicleCollisionRecord() )
				{
					bIsDamageValid = false;
				}

				if (bIsDamageValid)
				{
					CPedDamageCalculator damageCalculator(&rVeh, fDamageTaken, WEAPONTYPE_RAMMEDBYVEHICLE, 0, false);
					CEventDamage damageEvent(m_pVehicle, fwTimer::GetTimeInMilliseconds(), WEAPONTYPE_RAMMEDBYVEHICLE);

					TUNE_GROUP_BOOL(VEHICLE_DAMAGE_TUNE, ALLOW_FORCE_KILL, false);
					u32 damageFlags = CPedDamageCalculator::DF_None;
					if (ALLOW_FORCE_KILL || ((rPed.PopTypeIsRandom() && !rPed.IsLawEnforcementPed()) && fDamageTaken > ms_Tunables.m_MinDamageToCheckForRandomDeath))
					{
						// Always force a kill when the damage is high enough
						if (fDamageTaken > ms_Tunables.m_MaxDamageToCheckForRandomDeath)
						{
							damageFlags |= CPedDamageCalculator::DF_ForceInstantKill;
						}
						else
						{
							// Scale the chance of getting an instant kill based on the damage taken, the higher the damage, the more likely the insta-kill
							const float fDamageRatio = Clamp((fDamageTaken - ms_Tunables.m_MinDamageToCheckForRandomDeath) / (ms_Tunables.m_MaxDamageToCheckForRandomDeath - ms_Tunables.m_MinDamageToCheckForRandomDeath), 0.0f, 1.0f);
							const float fMinRandNumberToForceInstantKill = (1.0f - fDamageRatio) * ms_Tunables.m_MinHeavyCrashDeathChance + fDamageRatio * ms_Tunables.m_MaxHeavyCrashDeathChance;
							const float fRand = fwRandom::GetRandomNumberInRange(0.01f, 100.0f);
							taskDisplayf("Frame : %u, ped %p random death chance : %.2f (min: %.2f)", fwTimer::GetFrameCount(), &rPed, fRand, fMinRandNumberToForceInstantKill);
							if (fRand <= fMinRandNumberToForceInstantKill)
							{
								damageFlags |= CPedDamageCalculator::DF_ForceInstantKill;
							}
						}
					}

					// Kill the front seat passenger if the driver is thrown out of the windscreen or killed.
					if(bGoingThroughWindscreen || (damageFlags & CPedDamageCalculator::DF_ForceInstantKill) != 0)
					{
						CPed* pDriver = rVeh.GetDriver();
						if(pDriver && (pDriver == &rPed))
						{
							if (m_pVehicle->IsSeatIndexValid(1))
							{
								const CVehicleSeatInfo* pSeatInfo = m_pVehicle->GetSeatInfo(1);

								if (pSeatInfo && pSeatInfo->GetIsFrontSeat())
								{
									CPed* pPassengerPed = m_pVehicle->GetPedInSeat(1);
									if(pPassengerPed && !pPassengerPed->IsPlayer())
									{
										damageFlags |= CPedDamageCalculator::DF_ForceInstantKill;
										damageCalculator.ApplyDamageAndComputeResponse(pPassengerPed, damageEvent.GetDamageResponseData(), damageFlags);
									}
								}
							}
						}
					}

					// All or nothing for ai peds, either die or don't take damage
					if (rPed.IsLocalPlayer() || (damageFlags & CPedDamageCalculator::DF_ForceInstantKill))
					{
						// Only allow non ambient peds to be damaged by player collisions
						if (!rPed.IsAPlayerPed())
						{
							bool bHitByPlayer = false;
							const CCollisionRecord* pVehicleRecord = pCollisionHistory->GetFirstVehicleCollisionRecord();
							while (pVehicleRecord)
							{
								if (pVehicleRecord->m_pRegdCollisionEntity && pVehicleRecord->m_pRegdCollisionEntity->GetIsTypeVehicle())
								{
									const CVehicle* pCollisionVehicle = static_cast<const CVehicle*>(pVehicleRecord->m_pRegdCollisionEntity.Get());
									if (pCollisionVehicle->GetDriver() && pCollisionVehicle->GetDriver()->IsAPlayerPed())
									{
										bHitByPlayer = true;
									}
								}
								pVehicleRecord = pVehicleRecord->GetNext();
							}

							if (!bHitByPlayer)
							{
								return;
							}
						}

						damageCalculator.ApplyDamageAndComputeResponse(&rPed, damageEvent.GetDamageResponseData(), damageFlags);
					}

					// Damage events are higher priority than give ped task, so we don't want to add it if we're going through, or we won't process the event
					if (!bGoingThroughWindscreen)
					{
						rPed.GetPedIntelligence()->AddEvent(damageEvent);
					}
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInAutomobile::ProcessRagdollDueToVehicleOrientation()
{
	CPed& rPed = *GetPed();
	if (rPed.GetIsInVehicle() && !rPed.IsNetworkClone())
	{
		const CVehicleSeatAnimInfo* pSeatClipInfo = m_pVehicle->GetSeatAnimationInfo(&rPed);
		if (pSeatClipInfo && pSeatClipInfo->GetRagdollAtExtremeVehicleOrientation())
		{
			if (CVehicle::GetVehicleOrientation(*m_pVehicle) != CVehicle::VO_Upright && CTaskNMBehaviour::CanUseRagdoll(&rPed, RAGDOLL_TRIGGER_VEHICLE_FALLOUT))
			{
				nmEntityDebugf(&rPed, "CTaskMotionInAutomobile::ProcessRagdollDueToVehicleOrientation - adding CTaskNMBalance");
				CEventSwitch2NM event(500, rage_new CTaskNMBalance(500, 10000, NULL, 0));
				rPed.GetPedIntelligence()->AddEvent(event);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInAutomobile::ProcessInAirHeight(CPed& rPed, CVehicle& rVeh, float fTimeStep)
{
	bool bInAir;
	if(rVeh.InheritsFromBoat())
	{
		bool bColliding = (m_pVehicle->GetFrameCollisionHistory()->GetMaxCollisionImpulseMagLastFrame() > 0.0f) || (fwTimer::GetTimeInMilliseconds() < (m_pVehicle->GetFrameCollisionHistory()->GetMostRecentCollisionTime() + 1000));
		float fOutOfWaterTime = ((CBoat*)m_pVehicle.Get())->m_BoatHandling.GetOutOfWaterTimeForAnims();
		bInAir = (fOutOfWaterTime > 0.0f) && !bColliding && !m_pVehicle->IsAsleep();
	}
	else
	{
		bInAir = rVeh.IsInAir();
	}

	// Probably do this for all vehicles later on
	if (rVeh.InheritsFromBike() || rVeh.InheritsFromQuadBike() || rVeh.InheritsFromAmphibiousQuadBike() || rVeh.InheritsFromBoat() || (GetPed()->IsInFirstPersonVehicleCamera() && rVeh.InheritsFromAutomobile()))
	{
		if (bInAir)
		{
			m_bWasJustInAir = false;

			const float fCurrentVehHeight = rVeh.GetTransform().GetPosition().GetZf();
			if (m_fTimeInAir <= 0.0f)
			{
				m_fMinHeightInAir = fCurrentVehHeight;
				m_fMaxHeightInAir = fCurrentVehHeight;
			}
			else
			{
				m_fMinHeightInAir = Min(m_fMinHeightInAir, fCurrentVehHeight);
				m_fMaxHeightInAir = Max(m_fMaxHeightInAir, fCurrentVehHeight);
			}
			m_fTimeInAir += fTimeStep;
		}
		else
		{
			if (m_fTimeInAir > 0.0f)
			{
				m_bWasJustInAir = true;
				
				// need intelligence update next frame now that we've landed
				rPed.GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
			}
			else
			{
				m_bWasJustInAir = false;
			}
			
			m_fTimeInAir = 0.0f;
		}
	}
	PDR_ONLY(debugPlayback::RecordTaggedFloatValue(*rPed.GetCurrentPhysicsInst(), m_fTimeInAir, "m_fTimeInAir"));
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInAutomobile::ProcessPositionReset(CPed& rPed)
{
	rPed.SetPedConfigFlag(CPED_CONFIG_FLAG_PedsInVehiclePositionNeedsReset, false);
	rPed.SetPedConfigFlag(CPED_CONFIG_FLAG_PedsFullyInSeat, false);
	m_bHasFinishedBlendToSeatPosition = false;
	m_bHasFinishedBlendToSeatOrientation = false;
	m_qInitialPedRotation = QUATV_TO_QUATERNION(rPed.GetTransform().GetOrientation());
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInAutomobile::ComputeAccelerationParameter(const CPed& rPed, const CVehicle& rVeh, float fTimeStep)
{
	const CVehicleSeatAnimInfo* pSeatClipInfo = m_pVehicle->GetSeatAnimationInfo(&rPed);
	const bool bSimulateBumpiness = pSeatClipInfo && pSeatClipInfo->GetSimulateBumpiness();
	const float fMinFrameTime = fwTimer::GetMinimumFrameTime() * 100.0f;
	const float fInvTimeStep = 1.0f / fTimeStep;

	// The acceleration becomes erratic at low frame rates, which leads to choppy anim blends,
	// counteract this by scaling down the acceleration at low frame rates
	TUNE_GROUP_FLOAT(IN_VEHICLE_TUNE, MIN_TIME_STEP_SCALE, 1.0f, 0.0f, 1.0f, 0.01f);
	const float fTimeStepScale = rage::Clamp(fInvTimeStep * fMinFrameTime, bSimulateBumpiness ? 0.0f : MIN_TIME_STEP_SCALE, 1.0f);

	// Compute absolute acceleration relative to the vehicle, if we haven't yet stored the last frames velocity, just use the current 
	// this should result in no acceleration
	const Vec3V vCurrentVehicleVelocity = rVeh.GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(rVeh.GetVelocityIncludingReferenceFrame()));
	if (!m_bLastFramesVelocityValid)
	{
		m_vLastFramesVehicleVelocity = vCurrentVehicleVelocity;
	}

	Vec3V vActualAcceleration;
	if(m_pVehicle->IsNetworkClone())
	{
		vActualAcceleration = VECTOR3_TO_VEC3V(NetworkInterface::GetPredictedAccelerationVector(m_pVehicle));
		vActualAcceleration = rVeh.GetTransform().UnTransform3x3(vActualAcceleration);

		// HACK: We're getting some weird inverted predicted acceleration values, causing peds to swing oddly on back of trash / firetruck...
		if (pSeatClipInfo && pSeatClipInfo->GetKeepCollisionOnWhenInVehicle())
			vActualAcceleration.SetX(-vActualAcceleration.GetX());
	}
	else
	{
		vActualAcceleration = vCurrentVehicleVelocity - m_vLastFramesVehicleVelocity;
		vActualAcceleration = Scale(vActualAcceleration, ScalarVFromF32(fTimeStepScale * fInvTimeStep));
	}

#if __DEV
	m_vDebugLastVelocity = m_vLastFramesVehicleVelocity;
	m_vDebugCurrentVelocity = vCurrentVehicleVelocity;
	m_vDebugCurrentVehicleAcceleration = vActualAcceleration;
#endif // __DEV

	// Cache the current velocity, which will become last frames velocity next frame
	m_vLastFramesVehicleVelocity = vCurrentVehicleVelocity;
	m_bLastFramesVelocityValid = true;

	// Unfortunately we cannot just normalize the acceleration computed here because
	// the values we compute can be quite noisy, so we need to try to filter out the noise
	// by smoothing the values out, we need to allow the large increase/decreases in acceleration to 
	// come through though
	TUNE_GROUP_BOOL(VEHICLE_DEBUG, USE_ACTUAL_ACCELERATION, false);	
	if (USE_ACTUAL_ACCELERATION)
	{
		m_vCurrentVehicleAcceleration = vActualAcceleration;
	}
	else
	{
		// TODO: Move to per seat anim tunables?
		TUNE_GROUP_FLOAT(VEHICLE_DEBUG, X_APPROACH_ACC, 350.0f, 0.0f, 1000.0f, 0.1f);
		TUNE_GROUP_FLOAT(VEHICLE_DEBUG, X_APPROACH_ACC_SMALL, 4.0f, 0.0f, 1000.0f, 0.1f);
		TUNE_GROUP_FLOAT(VEHICLE_DEBUG, X_SMALL_ACC_DELTA, 0.1f, 0.0f, 10.0f, 0.01f);

		TUNE_GROUP_FLOAT(VEHICLE_DEBUG, Y_APPROACH_ACC, 350.0f, 0.0f, 1000.0f, 0.1f);
		TUNE_GROUP_FLOAT(VEHICLE_DEBUG, Y_APPROACH_ACC_SMALL, 4.0f, 0.0f, 1000.0f, 0.1f);
		TUNE_GROUP_FLOAT(VEHICLE_DEBUG, Y_SMALL_ACC_DELTA, 0.1f, 0.0f, 10.0f, 0.01f);

		TUNE_GROUP_FLOAT(VEHICLE_DEBUG, Z_APPROACH_ACC, 350.0f, 0.0f, 1000.0f, 0.1f);
		TUNE_GROUP_FLOAT(VEHICLE_DEBUG, Z_APPROACH_ACC_SMALL, 4.0f, 0.0f, 1000.0f, 0.1f);
		TUNE_GROUP_FLOAT(VEHICLE_DEBUG, Z_SMALL_ACC_DELTA, 0.15f, 0.0f, 10.0f, 0.01f);

		float fCurrentAccX = m_vCurrentVehicleAcceleration.GetXf();
		float fCurrentAccY = m_vCurrentVehicleAcceleration.GetYf();
		float fCurrentAccZ = m_vCurrentVehicleAcceleration.GetZf();

		ProcessParameterSmoothingNoScale(fCurrentAccX, vActualAcceleration.GetXf(), X_APPROACH_ACC_SMALL, X_APPROACH_ACC, X_SMALL_ACC_DELTA, fTimeStep);
		ProcessParameterSmoothingNoScale(fCurrentAccY, vActualAcceleration.GetYf(), Y_APPROACH_ACC_SMALL, Y_APPROACH_ACC, Y_SMALL_ACC_DELTA, fTimeStep);
		ProcessParameterSmoothingNoScale(fCurrentAccZ, vActualAcceleration.GetZf(), Z_APPROACH_ACC_SMALL, Z_APPROACH_ACC, Z_SMALL_ACC_DELTA, fTimeStep);

		m_vCurrentVehicleAcceleration.SetXf(fCurrentAccX);
		m_vCurrentVehicleAcceleration.SetYf(fCurrentAccY);
		m_vCurrentVehicleAcceleration.SetZf(fCurrentAccZ);
	}
	
	float fXComponent = m_vCurrentVehicleAcceleration.GetXf();
	float fYComponent = m_vCurrentVehicleAcceleration.GetYf();
	float fZComponent = m_vCurrentVehicleAcceleration.GetZf();
	
	// Add some x and y component based on the z acceleration if we don't have an animated z axis solution
	if (bSimulateBumpiness)
	{
		TUNE_GROUP_FLOAT(VEHICLE_DEBUG, Z_PERCENTAGE_FOR_X, 0.4f, 0.0f, 10.0f, 0.01f);
		fXComponent += m_vCurrentVehicleAcceleration.GetZf() * Z_PERCENTAGE_FOR_X;
		TUNE_GROUP_FLOAT(VEHICLE_DEBUG, Z_PERCENTAGE_FOR_Y, 0.4f, 0.0f, 10.0f, 0.01f);
		fYComponent += m_vCurrentVehicleAcceleration.GetZf() * Z_PERCENTAGE_FOR_Y;
	}
	
	TUNE_GROUP_FLOAT(VEHICLE_DEBUG, Y_MAX_ACCELERATION, 25.0f, 0.0f, 100.0f, 0.1f);
	TUNE_GROUP_FLOAT(VEHICLE_DEBUG, Z_MAX_ACCELERATION, 20.0f, 0.0f, 100.0f, 0.1f);
	
	const CVehicleLayoutInfo* pLayoutInfo = m_pVehicle->GetLayoutInfo();
	const float fXMaxAcceleration = pLayoutInfo->GetMaxXAcceleration();
	fXComponent = (rage::Clamp(fXComponent / fXMaxAcceleration, -1.0f, 1.0f) + 1.0f) * 0.5f;
	m_vCurrentVehicleAccelerationNorm.SetXf(fXComponent);
	fYComponent = (rage::Clamp(fYComponent / Y_MAX_ACCELERATION, -1.0f, 1.0f) + 1.0f) * 0.5f;
	m_vCurrentVehicleAccelerationNorm.SetYf(fYComponent);

	if(m_pVehicle->InheritsFromBoat())
	{
		TUNE_GROUP_FLOAT(VEHICLE_DEBUG, Z_MAX_ACCELERATION_JETSKI_OUT_OF_WATER, 10.0f, 0.0f, 100.0f, 0.1f);
		TUNE_GROUP_FLOAT(VEHICLE_DEBUG, Z_MAX_ACCELERATION_JETSKI_IN_WATER, 20.0f, 0.0f, 100.0f, 0.1f);
		TUNE_GROUP_FLOAT(VEHICLE_DEBUG, Z_MAX_ACCELERATION_BOAT_OUT_OF_WATER, 25.0f, 0.0f, 100.0f, 0.1f);
		TUNE_GROUP_FLOAT(VEHICLE_DEBUG, Z_MAX_ACCELERATION_BOAT_IN_WATER, 50.0f, 0.0f, 100.0f, 0.1f);
		TUNE_GROUP_FLOAT(VEHICLE_DEBUG, JETSKI_CONSIDERED_OUT_OF_WATER_TIME, 0.5f, 0.0f, 100.0f, 0.1f);

		float fAccInWater;
		float fAccOutOfWater;
		if(m_pVehicle->GetIsJetSki())
		{
			fAccInWater = Z_MAX_ACCELERATION_JETSKI_IN_WATER;
			fAccOutOfWater = Z_MAX_ACCELERATION_JETSKI_OUT_OF_WATER;
		}
		else
		{
			fAccInWater = Z_MAX_ACCELERATION_BOAT_IN_WATER;
			fAccOutOfWater = Z_MAX_ACCELERATION_BOAT_OUT_OF_WATER;
		}

		float fOutOfWaterTime = ((CBoat*)m_pVehicle.Get())->m_BoatHandling.GetOutOfWaterTimeForAnims();
		bool bColliding = (m_pVehicle->GetFrameCollisionHistory()->GetMaxCollisionImpulseMagLastFrame() > 0.0f) || (fwTimer::GetTimeInMilliseconds() < (m_pVehicle->GetFrameCollisionHistory()->GetMostRecentCollisionTime() + 1000));
		if(bColliding)
		{
			fOutOfWaterTime = 0.0f;
		}
		float fScale = Min(fOutOfWaterTime/JETSKI_CONSIDERED_OUT_OF_WATER_TIME, 1.0f);
		float fMaxAccel = fAccInWater + ((fAccOutOfWater-fAccInWater)*fScale);
		fZComponent = (rage::Clamp(fZComponent / fMaxAccel, -1.0f, 1.0f) + 1.0f) * 0.5f;
	}
	else if(m_pVehicle->InheritsFromBike() || m_pVehicle->InheritsFromQuadBike() || m_pVehicle->InheritsFromAmphibiousQuadBike())
	{
		TUNE_GROUP_FLOAT(VEHICLE_DEBUG, Z_MIN_ACCELERATION_BIKE_IN_AIR, 10.0f, 0.0f, 100.0f, 0.1f);
		TUNE_GROUP_FLOAT(VEHICLE_DEBUG, Z_MAX_ACCELERATION_BIKE_ON_GROUND, 10.0f, 0.0f, 100.0f, 0.1f);
		TUNE_GROUP_FLOAT(VEHICLE_DEBUG, Z_MAX_ACCELERATION_BIKE_IN_AIR, 20.0f, 0.0f, 100.0f, 0.1f);
		TUNE_GROUP_FLOAT(VEHICLE_DEBUG, BIKE_CONSIDERED_IN_AIR_TIME, 0.5f, 0.0f, 100.0f, 0.1f);
		float fScale = Min(m_fTimeInAir/BIKE_CONSIDERED_IN_AIR_TIME, 1.0f);
		float fMaxAccel = Z_MAX_ACCELERATION_BIKE_ON_GROUND + ((Z_MAX_ACCELERATION_BIKE_IN_AIR-Z_MAX_ACCELERATION_BIKE_ON_GROUND)*fScale);
		float fMinAccel = fScale * Z_MIN_ACCELERATION_BIKE_IN_AIR;
		fZComponent = Max(fMinAccel, Abs(fZComponent));
		fZComponent = (rage::Clamp(fZComponent / fMaxAccel, -1.0f, 1.0f) + 1.0f) * 0.5f;
	}
	else
	{
		fZComponent = (rage::Clamp(fZComponent / Z_MAX_ACCELERATION, -1.0f, 1.0f) + 1.0f) * 0.5f;
	}

	m_vCurrentVehicleAccelerationNorm.SetZf(fZComponent);	

	TUNE_GROUP_FLOAT(VEHICLE_DEBUG, X_APPROACH_SPEED_SMALL, 0.05f, 0.0f, 50.0f, 0.1f);
	TUNE_GROUP_FLOAT(VEHICLE_DEBUG, X_SMALL_DELTA, 0.2f, 0.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_DEBUG, Y_APPROACH_SPEED, 8.0f, 0.0f, 30.0f, 0.1f);
	TUNE_GROUP_FLOAT(VEHICLE_DEBUG, Y_APPROACH_SPEED_SMALL, 2.0f, 0.0f, 10.0f, 0.1f);
	TUNE_GROUP_FLOAT(VEHICLE_DEBUG, Y_SMALL_DELTA, 0.1f, 0.0f, 10.0f, 0.01f);

	bool bUsingLookBack = false;
	if (rPed.IsLocalPlayer())
	{
		const CControl* pControl = rPed.GetControlFromPlayer();
		if (pControl && pControl->GetVehicleLookBehind().IsDown())
		{
			bUsingLookBack = true;
		}
	}
	const float fXApproachSpeedScale = bUsingLookBack ? pLayoutInfo->GetLookBackApproachSpeedScale() : 1.0f;
	const float fVariance = m_pVehicle->GetDriver() == &rPed ? 0.0f : m_fApproachRateVariance;
	const float fXApproachSpeed = (pLayoutInfo->GetBodyLeanXApproachSpeed() + fVariance) * fXApproachSpeedScale;
	const float fXSmallDelta = pLayoutInfo->GetBodyLeanXSmallDelta();

	ProcessParameterSmoothing(m_fBodyLeanX, m_vCurrentVehicleAccelerationNorm.GetXf(), m_fBodyLeanApproachRateX, X_APPROACH_SPEED_SMALL, fXApproachSpeed, fXSmallDelta, fTimeStep, fTimeStepScale);

	if(!rPed.GetPedResetFlag(CPED_RESET_FLAG_PreventGoingIntoShuntInVehicleState))
	{
		ProcessParameterSmoothing(m_fBodyLeanY, m_vCurrentVehicleAccelerationNorm.GetYf(), m_fBodyLeanApproachRateY, Y_APPROACH_SPEED_SMALL, Y_APPROACH_SPEED, Y_SMALL_DELTA, fTimeStep, fTimeStepScale);
	}

	//If we are panicking, ensure our sit animation is enabled fully.
	if(IsPanicking())
	{
		m_fBodyLeanX = 0.5f;
		m_fBodyLeanY = 0.5f;
	}

	TUNE_GROUP_FLOAT(VEHICLE_DEBUG, LARGE_Z_ACCELERATION, 0.8f, 0.0f, 10.0f, 0.01f);
	if(m_vCurrentVehicleAccelerationNorm.GetZf() > LARGE_Z_ACCELERATION)
	{
		m_uLastTimeLargeZAcceleration = fwTimer::GetTimeInMilliseconds();
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInAutomobile::ComputeSteeringParameter(const CVehicleSeatAnimInfo& rSeatClipInfo, const CVehicle& rVeh, float fTimeStep)
{
	bool bFirstPersonSteering = IsUsingFirstPersonSteeringAnims();

	//! As anim pose is 1 frame behind, we need to locally use up to date steering wheel angle, but other systems will still use this frames.
	//! Avoids steering wheel moving, when anims don't.
	m_fSteeringWheelAngle = m_fSteeringWheelAnglePrev;
	float fSteeringWheelAngle = m_fSteeringWheelAngle;

	float fScaleForAnim = 1.0f;

	if(bFirstPersonSteering)
	{
		CVehicleModelInfo* pVehicleModelInfo = rVeh.GetVehicleModelInfo();
		if(pVehicleModelInfo != NULL)
		{
			float fSteeringWheelLimit = pVehicleModelInfo->GetMaxSteeringWheelAngle();
			bool bIsMicrolight = MI_PLANE_MICROLIGHT.IsValid() && rVeh.GetModelIndex() == MI_PLANE_MICROLIGHT;
			float fNewSteerAngle = (rVeh.InheritsFromPlane() && !bIsMicrolight) ? static_cast<const CPlane*>(&rVeh)->GetRollControl() * -1.0f : rVeh.GetSteerAngle();
            fNewSteerAngle = rVeh.InheritsFromSubmarineCar() && rVeh.IsInSubmarineMode() ? rVeh.GetSubHandling()->GetYawControl() * rVeh.pHandling->m_fSteeringLock : fNewSteerAngle;

			CHandlingData* pHandlingData = CHandlingDataMgr::GetHandlingData(pVehicleModelInfo->GetHandlingId());

			float fMaxSteeringWheelRotation = (pHandlingData->m_fSteeringLock * (fSteeringWheelLimit * DtoR)) * RtoD;
			float fSteeringWheelAnimRotation = pVehicleModelInfo->GetMaxSteeringWheelAnimAngle();
			fScaleForAnim = Clamp(abs(fMaxSteeringWheelRotation / fSteeringWheelAnimRotation), 0.0f, 1.0f);

			// Turn on/off cubic splines
			TUNE_GROUP_BOOL( FIRST_PERSON_STEERING, bUseCubicSplines, true );
			if(bUseCubicSplines)
			{
				// Normalize the strength of the signal between 0 and 1, 0 being no steering at all
				// and 1 being the max steering value (doesn't matter if it is right or left) 
				float fNormalizedAngle = (Abs(fNewSteerAngle) / fSteeringWheelLimit);

				// Transform the steering signal using the Cubic Hermite spline
				// polynomial
				float fSteeringRate = CubicHermiteSpline(fNormalizedAngle, 
														 ms_Tunables.m_FirstPersonSteeringSplineStart, 
														 ms_Tunables.m_FirstPersonSteeringSplineEnd, 
														 ms_Tunables.m_FirstPersonSteeringTangentT0, 
														 ms_Tunables.m_FirstPersonSteeringTangentT1);

				// Speed up the lerp if we are completely turning the wheel, otherwise keep it slow
				// for small taps of the stick
				float fMaxSteeringRate = rSeatClipInfo.GetFPSMaxSteeringRateOverride() != -1.0f ? rSeatClipInfo.GetFPSMaxSteeringRateOverride() : ms_Tunables.m_FirstPersonMaxSteeringRate;
				float fMinSteeringRate = rSeatClipInfo.GetFPSMinSteeringRateOverride() != -1.0f ? rSeatClipInfo.GetFPSMinSteeringRateOverride() : ms_Tunables.m_FirstPersonMinSteeringRate;
				SmoothSteeringWheelSpeedBasedOnInput(rVeh, m_FirstPersonSteeringWheelCurrentAngleApproachSpeed, 
													 ms_Tunables.m_FirstPersonSteeringSafeZone,
													 ms_Tunables.m_FirstPersonTimeToAllowFastSteering,
													 ms_Tunables.m_FirstPersonTimeToAllowSlowSteering,
													 ms_Tunables.m_FirstPersonSmoothRateToFastSteering,
													 ms_Tunables.m_FirstPersonSmoothRateToSlowSteering,
													 fMaxSteeringRate,
													 fMinSteeringRate,
													 ms_Tunables.m_FirstPersonMinVelocityToModifyRate,
													 ms_Tunables.m_FirstPersonMinVelocityToModifyRate);

				rage::Approach(fSteeringWheelAngle, fNewSteerAngle, m_FirstPersonSteeringWheelCurrentAngleApproachSpeed * fSteeringRate, fwTimer::GetTimeStep());
			}
			else
			{
				rage::Approach(fSteeringWheelAngle, fNewSteerAngle, ms_Tunables.m_FirstPersonSteeringWheelAngleApproachSpeed, fwTimer::GetTimeStep());
			}

		}
		else
		{
			rage::Approach(fSteeringWheelAngle, rVeh.GetSteerAngle(), ms_Tunables.m_FirstPersonSteeringWheelAngleApproachSpeed, fwTimer::GetTimeStep());
		}

		m_fSteeringWheelAnglePrev = fSteeringWheelAngle;
	}
	else
	{
		fSteeringWheelAngle = rVeh.GetSteerAngle();
        fSteeringWheelAngle = rVeh.InheritsFromSubmarineCar() && rVeh.IsInSubmarineMode() ? rVeh.GetSubHandling()->GetYawControl() * rVeh.pHandling->m_fSteeringLock : fSteeringWheelAngle;
		m_fSteeringWheelAnglePrev = fSteeringWheelAngle;
	}

	bool bDoPanicCheck = true;

	// B*3837198: We want to use L3 input for body leans, and R1/L1 (e.g. handling data) for steer anims for Microlight
	bool bMicrolight = MI_PLANE_MICROLIGHT.IsValid() && rVeh.GetModelIndex() == MI_PLANE_MICROLIGHT;
	if (bMicrolight)
	{
		m_fBodyLeanX = Compute2DBodyBlendSteeringParam(rVeh, m_fBodyLeanX, fTimeStep);
	}

	if( rVeh.GetIsJetSki() || (rVeh.InheritsFromBoat() && m_bUseLeanSteerAnims && !bFirstPersonSteering) )
	{
		CTaskMotionInVehicle::ComputeSteeringSignalWithDeadZone(rVeh, 
			m_fSteering, 
			m_uLastSteeringCentredTime, 
			m_uLastSteeringNonCentredTime,
			100,
			ms_Tunables.m_SteeringDeadZoneCentreTimeMS,
			ms_Tunables.m_SteeringDeadZone,
			rSeatClipInfo.GetSteeringSmoothing(), 
			rSeatClipInfo.GetSteeringSmoothing(),
			!rVeh.GetIsJetSki());

		bDoPanicCheck = false;
	}
	else if (m_bUseLeanSteerAnims || bFirstPersonSteering)
	{
		// Turn the steering angle into a value between 0->1
		CVehicleModelInfo* pVehicleModelInfo = rVeh.GetVehicleModelInfo();
		CHandlingData* pHandlingData = CHandlingDataMgr::GetHandlingData(pVehicleModelInfo->GetHandlingId());
		float fSteeringAngle = fSteeringWheelAngle * m_fSteeringWheelWeight * fScaleForAnim;
		taskAssert(pHandlingData->m_fSteeringLock != 0.0f);
		fSteeringAngle /= pHandlingData->m_fSteeringLock;
		fSteeringAngle = (fSteeringAngle*0.5f)+0.5f;

		fSteeringAngle = rage::Clamp(fSteeringAngle, 0.0f, 1.0f);
		fSteeringAngle = (1.0f - fSteeringAngle);

		//! Hands need to match steering wheel exactly in 1st person - no extra damping.
		if(bFirstPersonSteering)
		{
			m_fSteering = fSteeringAngle;
		}
		else
		{
			float fSteeringScale = GetSteeringLerpScale();

			// Slow the lerp rate down if we are returning to centre
			u32 uTimeDeltaNonCentred = (fwTimer::GetTimeInMilliseconds() - m_uLastSteeringNonCentredTime);
			float fLerpRate = Clamp((float)uTimeDeltaNonCentred / (float)ms_Tunables.m_MinCentredSteeringAngleTimeForSitIdles, 0.0f, 1.0f);
			TUNE_GROUP_FLOAT(STEERING_TUNE, MIN_LERP_RATE_PERCENTAGE, 0.15f, 0.0f, 1.0f, 0.01f);	
			TUNE_GROUP_FLOAT(STEERING_TUNE, STEER_MULTIPLIER, 4.0f, 0.0f, 10.0f, 0.01f);	
			fLerpRate = Clamp(1.0f - fLerpRate, MIN_LERP_RATE_PERCENTAGE, 1.0f) * square(fwTimer::GetTimeWarpActive());
			fLerpRate *= fSteeringScale;
			m_fSteering = rage::Lerp(fLerpRate * rSeatClipInfo.GetSteeringSmoothing() * STEER_MULTIPLIER, m_fSteering, fSteeringAngle);
		}
	}
	else if (rVeh.GetLayoutInfo()->GetUse2DBodyBlend())
	{
		const CPed& rPed = *GetPed();
		if (rPed.IsLocalPlayer())
		{
			m_fSteering = Compute2DBodyBlendSteeringParam(rVeh, m_fSteering, fTimeStep);
		}
		else
		{
			// TODO: Need to figure out which way a ped is steering
			CTaskMotionInVehicle::ComputeSteeringSignal(rVeh, m_fSteering, rSeatClipInfo.GetSteeringSmoothing());
		}
	}
	else
	{
		CTaskMotionInVehicle::ComputeSteeringSignal(rVeh, m_fSteering, rSeatClipInfo.GetSteeringSmoothing());
	}

	//If we are panicking, ensure our sit animation is enabled fully.
	if(bDoPanicCheck && IsPanicking())
	{
		m_fSteering = 0.5f;
	}

	if ((abs(m_fSteering - 0.5f) > 0.01f))
	{
		m_uLastSteeringNonCentredTime = fwTimer::GetTimeInMilliseconds();
	}
}

float CTaskMotionInAutomobile::Compute2DBodyBlendSteeringParam(const CVehicle& rVeh, float fCurrentSteeringValue, float fTimeStep)
{
	const CPed& rPed = *GetPed();
	if (rPed.IsLocalPlayer())
	{
		const CControl* pControl = rPed.GetControlFromPlayer();
		float fSteeringAngle = Clamp((pControl->GetVehicleSteeringLeftRight().GetNorm() + 1.0f) * 0.5f, 0.0f, 1.0f);

		TUNE_GROUP_FLOAT(BODY_BLEND_TUNE, BODY_BLEND_MAX_STEERING_FOR_LOW_SPEED, 0.15f, 0.0f, 0.5f, 0.01f);	
		TUNE_GROUP_FLOAT(BODY_BLEND_TUNE, BODY_BLEND_MIN_SPEED_FOR_FULL_LEAN, 5.0f, 0.0f, 20.0f, 0.01f);	
		const float fVelocitySqd = rVeh.GetVelocityIncludingReferenceFrame().Mag2();
		float fNewSteeringValue = fCurrentSteeringValue;
		if (fVelocitySqd < square(BODY_BLEND_MIN_SPEED_FOR_FULL_LEAN))
		{
			fNewSteeringValue = Clamp(fNewSteeringValue, 0.5f - BODY_BLEND_MAX_STEERING_FOR_LOW_SPEED, 0.5f + BODY_BLEND_MAX_STEERING_FOR_LOW_SPEED);
		}

		TUNE_GROUP_FLOAT(BODY_BLEND_TUNE, BODY_BLEND_STEERING_APPROACH_SPEED, 1.0f, 0.0f, 10.0f, 0.01f);
		rage::Approach(fNewSteeringValue, fSteeringAngle, BODY_BLEND_STEERING_APPROACH_SPEED, fTimeStep);

		return fNewSteeringValue;
	}

	return fCurrentSteeringValue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInAutomobile::ComputeBikeParameters(const CPed& rPed)
{
	// Don't process spring height params on OPPRESSOR2, as the hovering system plays havok with the inputs
	bool bIsOppressor2 = MI_BIKE_OPPRESSOR2.IsValid() && m_pVehicle->GetModelIndex() == MI_BIKE_OPPRESSOR2;

	// Process Spring
	TUNE_GROUP_BOOL(VEHICLE_MOTION, USE_SPRING_MODEL, true);
	if (USE_SPRING_MODEL && !bIsOppressor2)
	{
		float fZAccel = m_vCurrentVehicleAccelerationNorm.GetZf();

		TUNE_GROUP_FLOAT(VEHICLE_MOTION, SPRING_DAMPING_SCALAR, 100.0f, 0.0f, 100.0f, 0.01f);
		float fTargetDisplacement = fZAccel * 2.0f;
		fTargetDisplacement -= 1.0f;
		fTargetDisplacement *= -1.0f;

		bool bInAir = false;

		TUNE_GROUP_BOOL(VEHICLE_MOTION, BIKE_DRIVE_TO_STAND_UP_IN_AIR, true);
		TUNE_GROUP_BOOL(VEHICLE_MOTION, JETSKI_DRIVE_TO_STAND_UP_IN_AIR, true);
		if(JETSKI_DRIVE_TO_STAND_UP_IN_AIR && m_pVehicle->InheritsFromBoat())
		{
			TUNE_GROUP_FLOAT(VEHICLE_DEBUG, IN_AIR_DRIVE_TO_STAND_UP_TIME_MIN, 0.5f, 0.0f, 100.0f, 0.1f);
			TUNE_GROUP_FLOAT(VEHICLE_DEBUG, IN_AIR_DRIVE_TO_STAND_UP_TIME_MAX, 1.5f, 0.0f, 100.0f, 0.1f);
			float fOutOfWaterTime = ((CBoat*)m_pVehicle.Get())->m_BoatHandling.GetOutOfWaterTimeForAnims();
			bool bColliding = (m_pVehicle->GetFrameCollisionHistory()->GetMaxCollisionImpulseMagLastFrame() > 0.0f) || (fwTimer::GetTimeInMilliseconds() < (m_pVehicle->GetFrameCollisionHistory()->GetMostRecentCollisionTime() + 1000));
			if( (fOutOfWaterTime > IN_AIR_DRIVE_TO_STAND_UP_TIME_MIN) && !bColliding && !m_pVehicle->IsAsleep())
			{
				float fScale = (fOutOfWaterTime - IN_AIR_DRIVE_TO_STAND_UP_TIME_MIN) / (IN_AIR_DRIVE_TO_STAND_UP_TIME_MAX-IN_AIR_DRIVE_TO_STAND_UP_TIME_MIN);
				fTargetDisplacement = Min(fTargetDisplacement + ((1.0f-fTargetDisplacement) * fScale), 1.0f);
				bInAir = true;
			}
		}
		else if (BIKE_DRIVE_TO_STAND_UP_IN_AIR && 
				( ( m_pVehicle->InheritsFromBike() && ( m_pVehicle->HasGlider() || m_pVehicle->GetSpecialFlightModeRatio() < 0.5f ) ) || 
				  m_pVehicle->InheritsFromQuadBike() || m_pVehicle->InheritsFromAmphibiousQuadBike()))
		{
			PDR_ONLY(debugPlayback::RecordTaggedFloatValue(*rPed.GetCurrentPhysicsInst(), m_vCurrentVehicleAcceleration.GetZf(), "m_vCurrentVehicleAcceleration.GetZf()"));
			TUNE_GROUP_FLOAT(VEHICLE_MOTION, MAX_Z_ACCEL_TO_ZERO_DISPLACEMENT, 0.0f, 0.0f, 5.0f, 0.01f);
			if (m_fTimeInAir > ms_Tunables.m_BikeInAirDriveToStandUpTimeMin)
			{
				TUNE_GROUP_FLOAT(VEHICLE_MOTION, MIN_Z_ACCEL_TO_ZERO_DISPLACEMENT_DUE_TO_PITCH, 5.0f, 0.0f, 25.0f, 0.01f);
				if (m_fTimeInAir >= ms_Tunables.m_BikeInAirDriveToStandUpTimeMax || m_vCurrentVehicleAcceleration.GetZf() < -MIN_Z_ACCEL_TO_ZERO_DISPLACEMENT_DUE_TO_PITCH)
				{
					m_fDisplacementDueToPitch = 0.0f;
				}
				float fScale = Clamp((m_fTimeInAir - ms_Tunables.m_BikeInAirDriveToStandUpTimeMin) / (ms_Tunables.m_BikeInAirDriveToStandUpTimeMax-ms_Tunables.m_BikeInAirDriveToStandUpTimeMin), 0.0f, 1.0f);
				float fDisplacementDueToInAir = Max(fTargetDisplacement + ((1.0f-fTargetDisplacement) * fScale), m_fDisplacementDueToPitch);
				if (fDisplacementDueToInAir > fTargetDisplacement)
				{
					fTargetDisplacement = fDisplacementDueToInAir;
				}
				PDR_ONLY(debugPlayback::RecordTaggedFloatValue(*rPed.GetCurrentPhysicsInst(), fDisplacementDueToInAir, "fDisplacementDueToInAir"));
				bInAir = true;
			}

			const bool bFrontWheelTouching =  m_pVehicle->GetWheel(1)->GetIsTouching(); 
			if (!bFrontWheelTouching)
			{
				TUNE_GROUP_FLOAT(VEHICLE_MOTION, FRONT_WHEEL_IN_SPEED, 7.0f, 0.0f, 15.0f, 0.01f);
				Approach(m_fFrontWheelOffGroundScale, 1.0f, FRONT_WHEEL_IN_SPEED, fwTimer::GetTimeStep());
			}
			else
			{
				TUNE_GROUP_FLOAT(VEHICLE_MOTION, FRONT_WHEEL_OUT_SPEED, 1.0f, 0.0f, 15.0f, 0.01f);
				TUNE_GROUP_FLOAT(VEHICLE_MOTION, FRONT_WHEEL_OUT_MIN, 0.0f, 0.0f, 1.0f, 0.01f);
				Approach(m_fFrontWheelOffGroundScale, FRONT_WHEEL_OUT_MIN, FRONT_WHEEL_OUT_SPEED, fwTimer::GetTimeStep());
			}

			TUNE_GROUP_FLOAT(VEHICLE_MOTION, MIN_PITCH_TO_START_DISPLACEMENT, 0.5f, 0.0f, 1.0f, 0.01f);
			TUNE_GROUP_FLOAT(VEHICLE_MOTION, MAX_PITCH_TO_START_DISPLACEMENT, 0.8f, 0.0f, 1.0f, 0.01f);
			if (m_fPitchAngle > MIN_PITCH_TO_START_DISPLACEMENT)
			{
				m_fDisplacementDueToPitch = (m_fPitchAngle - MIN_PITCH_TO_START_DISPLACEMENT) / (MAX_PITCH_TO_START_DISPLACEMENT - MIN_PITCH_TO_START_DISPLACEMENT);

				PDR_ONLY(debugPlayback::RecordTaggedFloatValue(*rPed.GetCurrentPhysicsInst(), m_fFrontWheelOffGroundScale, "m_fFrontWheelOffGroundScale"));
				if (!bFrontWheelTouching && m_fDisplacementDueToPitch > fTargetDisplacement)
				{
					fTargetDisplacement = m_fDisplacementDueToPitch * m_fFrontWheelOffGroundScale;
				}
				else
				{
					m_fDisplacementDueToPitch = 0.0f;
				}
				PDR_ONLY(debugPlayback::RecordTaggedFloatValue(*rPed.GetCurrentPhysicsInst(), m_fDisplacementDueToPitch, "m_fDisplacementDueToPitch"));
			}

			TUNE_GROUP_FLOAT(VEHICLE_MOTION, DISPLACEMENT_TOL, 0.05f, 0.0f, 1.0f, 0.001f);
			if (fTargetDisplacement > m_pBikeAnimParams->fPreviousTargetSeatDisplacement + DISPLACEMENT_TOL)
			{
				Approach(m_fDisplacementScale, 1.0f, ms_Tunables.m_DisplacementScaleApproachRateIn, fwTimer::GetTimeStep());
			}
			else
			{
				Approach(m_fDisplacementScale, ms_Tunables.m_MinDisplacementScale, ms_Tunables.m_DisplacementScaleApproachRateOut, fwTimer::GetTimeStep());
			}

			fTargetDisplacement *= m_fDisplacementScale;
			PDR_ONLY(debugPlayback::RecordTaggedFloatValue(*rPed.GetCurrentPhysicsInst(), m_fDisplacementScale, "m_fDisplacementScale"));
		}

		TUNE_GROUP_BOOL(VEHICLE_MOTION, USE_TARGET_DISPLACEMENT_SMOOTHING, true);
		if (USE_TARGET_DISPLACEMENT_SMOOTHING)
		{
			TUNE_GROUP_FLOAT(VEHICLE_MOTION, TARGET_DISPLACEMENT_SMOOTHING_RATE, 1.0f, 0.0f, 1.0f, 0.001f);
			TUNE_GROUP_FLOAT(VEHICLE_MOTION, TARGET_DISPLACEMENT_SMOOTHING_RATE_IN_AIR, 0.5f, 0.0f, 100.0f, 0.1f);
			float fTargetDisplacementSmoothing = bInAir ? TARGET_DISPLACEMENT_SMOOTHING_RATE_IN_AIR : TARGET_DISPLACEMENT_SMOOTHING_RATE;
			fTargetDisplacement = rage::Lerp(fTargetDisplacementSmoothing, m_pBikeAnimParams->fPreviousTargetSeatDisplacement, fTargetDisplacement);
			m_pBikeAnimParams->fPreviousTargetSeatDisplacement = fTargetDisplacement;
		}

		//Displayf("Target Displacement : %.4f", fTargetDisplacement);
		m_pBikeAnimParams->fSeatDisplacement = m_pBikeAnimParams->seatDisplacementSpring.Update(fTargetDisplacement, SPRING_DAMPING_SCALAR);
		//Displayf("Displacement Before Clamp: %.4f", m_fSeatDisplacement);
		m_pBikeAnimParams->fSeatDisplacement = rage::Clamp(m_pBikeAnimParams->fSeatDisplacement, -1.0f, 1.0f);
		//Displayf("Displacement After Clamp: %.4f", m_fSeatDisplacement);

		//NOTE: We must override the persistent spring displacment to a valid -1.0f to 1.0f range.
		m_pBikeAnimParams->seatDisplacementSpring.OverrideResult(m_pBikeAnimParams->fSeatDisplacement);

		float fDisplacementSignal = m_pBikeAnimParams->fSeatDisplacement;
		fDisplacementSignal += 1.0f;
		fDisplacementSignal *= 0.5f;
		//Displayf("Displacement Signal: %.4f", fDisplacementSignal);
		
		TUNE_GROUP_BOOL(VEHICLE_MOTION, USE_DISPLACEMENT_SMOOTHING, true);
		if (USE_DISPLACEMENT_SMOOTHING)
		{
			const float fDisplacementSmoothingRate = m_pVehicle->GetDriver() == &rPed ? ms_Tunables.m_SeatDisplacementSmoothingRateDriver : ms_Tunables.m_SeatDisplacementSmoothingRatePassenger;
			fDisplacementSignal = rage::Lerp(fDisplacementSmoothingRate, m_pBikeAnimParams->fPreviousSeatDisplacementSignal, fDisplacementSignal);
			m_pBikeAnimParams->fPreviousSeatDisplacementSignal = fDisplacementSignal;
		}

		if(m_fJetskiStandPoseBlendTime > 0.0f)
		{
			fDisplacementSignal = rage::Lerp(m_fJetskiStandPoseBlendTime, fDisplacementSignal, 1.0f);
		}

		// Convert to signal
		PDR_ONLY(debugPlayback::RecordTaggedFloatValue(*rPed.GetCurrentPhysicsInst(), fDisplacementSignal, "fDisplacementSignal"));
		m_moveNetworkHelper.SetFloat(CTaskMotionInVehicle::ms_BodyMovementZId, fDisplacementSignal);
	}
	else
	{
		m_moveNetworkHelper.SetFloat(CTaskMotionInVehicle::ms_BodyMovementZId, 0.5f);
	}

	if(m_pVehicle->GetIsJetSki() && rPed.GetPedResetFlag(CPED_RESET_FLAG_ForceIntoStandPoseOnJetski))
	{
		m_fJetskiStandPoseBlendTime += fwTimer::GetTimeStep() * ms_Tunables.m_SeatDisplacementSmoothingStandUpOnJetski;
		m_fJetskiStandPoseBlendTime = Min(m_fJetskiStandPoseBlendTime, 1.0f);
	}
	else
	{
		m_fJetskiStandPoseBlendTime -= fwTimer::GetTimeStep() * ms_Tunables.m_SeatDisplacementSmoothingStandUpOnJetski;
		m_fJetskiStandPoseBlendTime = Max(m_fJetskiStandPoseBlendTime, 0.0f);
	}

	m_pBikeAnimParams->bikeLeanAngleHelper.Update();
}

////////////////////////////////////////////////////////////////////////////////

// Returns true if we should use lowrider lean anims.
bool CTaskMotionInAutomobile::ShouldUseLowriderLean() const
{
	TUNE_GROUP_BOOL(LOWRIDER_TUNE, ENABLED, true);
	if (ENABLED && m_pVehicle->InheritsFromAutomobile())
	{
		//Ensure the lowrider lean clip set is valid.
		fwMvClipSetId lowriderLeanClipSet = GetLowriderLeanClipSet();
		if(lowriderLeanClipSet == CLIP_SET_ID_INVALID)
		{
			return false;
		}

		if (m_pVehicle->IsNetworkClone())
		{
			// Vehicle owner is using lean anims so we should too.
			CPed *pVehicleOwner = m_pVehicle->GetDriver();
			if (pVehicleOwner && pVehicleOwner->GetPedConfigFlag(CPED_CONFIG_FLAG_UsingLowriderLeans))
			{
				return true;
			}
			else
			{
				return false;
			} 
		}
		else
		{
			CAutomobile *pAutomobile = static_cast<CAutomobile*>(m_pVehicle.Get());
			if (!pAutomobile || !pAutomobile->HasHydraulicSuspension())
			{
				return false;
			}

			// Don't use lowrider anims if we have a seat override set by script 
			if (m_pSeatOverrideAnimInfo)
			{
				return false;
			}

			// Don't use lowrider anims if we're panicking/agitated, unless we're actively modifying suspension (or have modified them recently).
			// ...As long as we're no longer using the alt clipset and we've finished playing any transition anims.
			bool bPanickingOrAgitated = !m_bPlayingLowriderClipsetTransition && m_nDefaultClipSet != DCS_LowriderLeanAlt && (ShouldPanic() || ShouldBeAgitated());

			// Always use lowrider lean anims now, even when not modifying suspension. 
			TUNE_GROUP_BOOL(LOWRIDER_TUNE, bMustModifySuspensionToUseNewAnims, false);
			if (bMustModifySuspensionToUseNewAnims || bPanickingOrAgitated)
			{
				bool bSuspensionModified = m_pVehicle->m_nVehicleFlags.bAreHydraulicsAllRaised || HasModifiedLowriderSuspension() || m_pVehicle->m_nVehicleFlags.bPlayerModifiedHydraulics;
				
				// If ped should be panicking/agitated, break out of lowrider anims if we haven't actively modified the suspension recently.
				TUNE_GROUP_INT(LOWRIDER_TUNE, iPanicOrAgitatedTimer, 2000, 0, 10000, 1);
				u32 iTimeInMilliseconds = fwTimer::GetTimeInMilliseconds();
				if (bPanickingOrAgitated && bSuspensionModified && (m_iTimeLastModifiedHydraulics + iPanicOrAgitatedTimer) < iTimeInMilliseconds)
				{
					bSuspensionModified = false;
				}	

				if (!bSuspensionModified)
				{
					return false;
				}
			}

			return true;
		}
	}

	return false;
}

// Returns true if we should use the alternate lowrider lean anims (ie arm on window).
bool CTaskMotionInAutomobile::ShouldUseLowriderLeanAltClipset(bool bCheckPanicOrAgitated) const
{
	const CPed *pPed = GetPed();

#if __BANK
	TUNE_GROUP_BOOL(LOWRIDER_TUNE, bForceUseAltClipset, false);
	if (bForceUseAltClipset)
	{
		return true;
	}
#endif	//__BANK

	if (!m_pVehicle)
	{
		return false;
	}

	if (GetLowriderLeanAlternateClipSet() == CLIP_SET_ID_INVALID)
	{
		return false;
	}

	// Return false if we're playing an ambient idle and not using the alternate clipset yet.
	if (m_nDefaultClipSet != DCS_LowriderLeanAlt && pPed->GetPedIntelligence() && pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_AMBIENT_CLIPS))
	{
		CTaskAmbientClips* pAmbientTask = static_cast<CTaskAmbientClips*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AMBIENT_CLIPS));
		if (pAmbientTask && (!pAmbientTask->IsIdleFinished() || pAmbientTask->IsBaseClipPlaying()))
		{
			return false;
		}
	}

	if (pPed->IsNetworkClone())
	{
		return pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UsingAlternateLowriderLeans);
	}

	// Only do alternate clipset for driver.
	if (!pPed->GetIsDrivingVehicle())
	{
		return false;
	}

	// Need to have a hand on the steering wheel!
	if (pPed->GetPedResetFlag(CPED_RESET_FLAG_UsingMobilePhone))
	{
		return false;
	}

	// Don't use alt clipset if we should use first person anims.
	if (IsUsingFirstPersonAnims())
	{
		return false;
	}

	// Is engine on?
	if (!m_pVehicle->IsEngineOn())
	{
		return false;
	}

	// Is car upside down or on its side?
	if (m_pVehicle->IsUpsideDown() || m_pVehicle->IsOnItsSide())
	{
		return false;
	}

	// Use normal anims when doing driveby.
	if (pPed->GetPedResetFlag(CPED_RESET_FLAG_IsDoingDriveby))
	{
		return false;
	}

	// Do we want to exit the vehicle?
	if (pPed->GetPedIntelligence() && pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE))
	{
		return false;
	}

	// Is the vehicle submersed in water (ie submerged by at least 0.25m and don't have all wheels touching the floor)?
	if (m_pVehicle->GetIsInWater() && m_pVehicle->m_Buoyancy.GetSubmergedLevel() > 0.25f && m_pVehicle->GetNumContactWheels() < m_pVehicle->GetNumWheels())
	{
		return false;
	}

	// Running scripted animation in MP? (ie gesture anims)
	if (NetworkInterface::IsGameInProgress())
	{
		const CTask* pTaskScriptedAnim = pPed->GetPedIntelligence() ? static_cast<const CTask*>(pPed->GetPedIntelligence()->GetTaskSecondaryActive()) : NULL;
		if( pTaskScriptedAnim && pTaskScriptedAnim->GetTaskType()==CTaskTypes::TASK_SCRIPTED_ANIMATION)
		{
			return false;
		}
	}

	// Should we switch to the panic/agitated clipsets?
	if (bCheckPanicOrAgitated && (ShouldPanic() || ShouldBeAgitated()))
	{
		return false;
	}

	// Some checks only run on bikes
	if (m_pVehicle->IsBikeOrQuad())
	{
		if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingHelmetVisor))
		{
			return false;
		}

		if (!PassesPassengerConditionsForBikeAltAnims())
		{
			return false;
		}

		if (!PassesVelocityConditionForBikeAltAnims())
		{
			return false;
		}

		if (!PassesLeanConditionForBikeAltAnims())
		{
			return false;
		}

		if (!PassesActionConditionsForBikeAltAnims())
		{
			return false;
		}

		if (!PassesWheelConditionsForBikeAltAnims())
		{
			return false;
		}

		if (CheckIfVehicleIsDoingBurnout(*m_pVehicle, *pPed))
		{
			return false;
		}

		if (GetState() == State_Reverse || GetState() == State_FirstPersonReverse || GetState() == State_StillToSit || GetState() == State_SitToStill || GetState() == State_Shunt)
		{
			return false;
		}
	}
	// And some only run on cars
	else 
	{
		// Don't use alt clipset if vehicle has a raised roof.
		if (m_pVehicle->CarHasRoof() && (m_pVehicle->HasRaisedRoof() || (m_pVehicle->GetConvertibleRoofState() != CTaskVehicleConvertibleRoof::STATE_LOWERED && m_pVehicle->GetConvertibleRoofState() != CTaskVehicleConvertibleRoof::STATE_ROOF_STUCK_LOWERED)))
		{
			return false;
		}

		// Do we have a door? If so, is the door shut?
		const CCarDoor* pDoor = CTaskMotionInVehicle::GetDirectAccessEntryDoorFromPed(*m_pVehicle, *pPed);
		if (!pDoor || !pDoor->GetIsIntact(m_pVehicle) || (pDoor && (pDoor->GetDoorRatio() > 0.0f)))
		{
			return false;
		}

		//! Don't allow lowrider alt anims if we are cruising above a certain speed.
		if(m_bDrivingAggressively)
		{
			return false;
		}
	}

	return true;
}

bool CTaskMotionInAutomobile::PassesVelocityConditionForBikeAltAnims() const
{
	const float fVelSqd = m_pVehicle->GetVelocityIncludingReferenceFrame().Mag2();
	if (fVelSqd < square(ms_Tunables.m_MinVelocityToAllowAltClipSet))
		return false;

	const float fMaxVelocity = IsDoingAWheelie(*m_pVehicle) ? ms_Tunables.m_MaxVelocityToAllowAltClipSetWheelie : ms_Tunables.m_MaxVelocityToAllowAltClipSet;
	if (fVelSqd > square(fMaxVelocity))
		return false;

	const Vec3V vVehFwd = m_pVehicle->GetTransform().GetForward();
	const Vec3V vVel = Normalize(VECTOR3_TO_VEC3V(m_pVehicle->GetVelocity()));
	const float fDot = Dot(vVehFwd, vVel).Getf();
	if (fDot < 0.0f)
		return false;

	return true;
}

bool CTaskMotionInAutomobile::PassesLeanConditionForBikeAltAnims() const
{
	if (!m_pBikeAnimParams)
		return false;

	const float fBodyLeanX = m_pBikeAnimParams->bikeLeanAngleHelper.GetLeanAngle();
	const float fLeftLeanThreshold = 0.5f - ms_Tunables.m_LeanThresholdToAllowAltClipSet;
	if (fBodyLeanX < fLeftLeanThreshold)
		return false;

	const float fRightLeanThreshold = 0.5f + ms_Tunables.m_LeanThresholdToAllowAltClipSet;
	if (fBodyLeanX > fRightLeanThreshold)
		return false;

	return true;
}

bool CTaskMotionInAutomobile::PassesActionConditionsForBikeAltAnims() const
{
	if (PlayerShouldBeAgitated())
		return false;

	const CPed& rPed = *GetPed();
	if (rPed.GetPedIntelligence()->IsBattleAware())
		return false;

	const float fTimeSinceLastDamage = ((float)fwTimer::GetTimeInMilliseconds() - rPed.GetWeaponDamagedTime())/1000.0f;
	if (fTimeSinceLastDamage < ms_Tunables.m_TimeSinceLastDamageToAllowAltLowRiderPoseLowRiderPose)
		return false;

	if (rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_IsPerformingVehicleMelee))
		return false;

	return true;
}

bool CTaskMotionInAutomobile::PassesWheelConditionsForBikeAltAnims() const
{
	if (!m_pVehicle)
		return false;

	if (m_pVehicle->InheritsFromQuadBike() || m_pVehicle->InheritsFromAmphibiousQuadBike())
		return true;

	if (!IsDoingAWheelie(*m_pVehicle))
		return true;

	if (!m_pVehicle->InheritsFromBike())
		return false;

// 	const float fPitch = GetPed()->GetCurrentPitch();
// 	if (fPitch > ms_Tunables.m_MaxPitchForBikeAltAnims)
// 		return false;

	if (m_pVehicle->IsInAir())
		return false;

	const CBike& rBike = static_cast<const CBike&>(*m_pVehicle);
	if (rBike.GetIsOffRoad())
		return true;

	return false;
}

bool CTaskMotionInAutomobile::PassesPassengerConditionsForBikeAltAnims() const
{
	if (!m_pVehicle)
		return false;

	const CPed* pPedInOrUsingPassengerSeat = CTaskVehicleFSM::GetPedInOrUsingSeat(*m_pVehicle, 1);
	if (pPedInOrUsingPassengerSeat)
		return false;

	return true;
}

// Returns true if timer has expired so that we can use the alternate lowrider lean anims (ie arm on window).
bool CTaskMotionInAutomobile::CanUseLowriderLeanAltClipset() const
{
	if (m_fAltLowriderClipsetTimer > m_fTimeToTriggerAltLowriderClipset)
	{
		return true;
	}

	return false;
}


// Returns true if we should use the fast action transition to return to the normal anims (hands on steering wheel).
bool CTaskMotionInAutomobile::ShouldUseLowriderAltClipsetActionTransition() const
{
#if __BANK
	TUNE_GROUP_BOOL(LOWRIDER_TUNE, bForceToWheelActionTransition, false);
	if (bForceToWheelActionTransition)
	{
		return true;
	}
#endif	// __BANK

	const CPed *pPed = GetPed();

	// Are we doing a driveby?
	if (pPed->GetPedResetFlag(CPED_RESET_FLAG_IsDoingDriveby))
	{
		return true;
	}

	// Is car upside down or on its side?
	if (m_pVehicle->IsUpsideDown() || m_pVehicle->IsOnItsSide())
	{
		return true;
	}

	// Is the vehicle submersed in water (ie submerged by at least 0.25m and don't have all wheels touching the floor)?
	if (m_pVehicle->GetIsInWater() && m_pVehicle->m_Buoyancy.GetSubmergedLevel() > 0.25f && m_pVehicle->GetNumContactWheels() < m_pVehicle->GetNumWheels())
	{
		return true;
	}

	// Are we about to exit the vehicle?
	TUNE_GROUP_BOOL(LOWRIDER_TUNE, bUseActionTransitionForExitVehicle, true);
	if (bUseActionTransitionForExitVehicle && pPed->GetPedIntelligence() && 
		pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE))
	{
		return true;
	}
	
	// Should we be using the panic/agitated clipsets?
	if (ShouldPanic() || ShouldBeAgitated())
	{
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

// Returns true if we should use bike alternate anims.
bool CTaskMotionInAutomobile::ShouldUseBikeAltClipset() const
{
	const CPed *pPed = GetPed();

	TUNE_GROUP_BOOL(LOWRIDER_BIKE_TUNE, FORCE_ENABLED, false);
	if ((pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_AllowBikeAlternateAnimations) || FORCE_ENABLED) && m_pVehicle->IsBikeOrQuad())
	{
		//Essentially useless, as a duplicate of the main clipset, but allowing DCS_LowriderLean makes things easier for transitions and timers...
		fwMvClipSetId lowriderLeanClipSet = GetLowriderLeanClipSet();
		if(lowriderLeanClipSet == CLIP_SET_ID_INVALID)
		{
			return false;
		}

		if (m_pVehicle->IsNetworkClone())
		{
			// Vehicle owner is using alt anims so we should too.
			CPed *pVehicleOwner = m_pVehicle->GetDriver();
			if (pVehicleOwner && pVehicleOwner->GetPedConfigFlag(CPED_CONFIG_FLAG_UsingLowriderLeans))
			{
				return true;
			}
			else
			{
				return false;
			} 
		}
		else
		{
			// Don't use alt anims if we have a seat override set by script 
			if (m_pSeatOverrideAnimInfo)
			{
				return false;
			}

			// Don't use alt anims if we're panicking/agitated
			bool bPanickingOrAgitated = !m_bPlayingLowriderClipsetTransition && m_nDefaultClipSet != DCS_LowriderLeanAlt && (ShouldPanic() || ShouldBeAgitated());
			if (bPanickingOrAgitated)
			{
				return false;
			}

			return true;
		}
	}

	return false;
}

bool CTaskMotionInAutomobile::HasModifiedLowriderSuspension() const
{
	if (!m_pVehicle)
		return false;

	// Currently not syncing suspension raise amount data for clone vehicles. Just check if vehicle owner is flagged to use lowrider leans.
	if (m_pVehicle->IsNetworkClone() && GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_UsingLowriderLeans))
	{
		return true;
	}
	else
	{
		// Return true if vehicle has modified hydraulics flag set.
		if (m_pVehicle->m_nVehicleFlags.bPlayerModifiedHydraulics)
		{
			return true;
		}

		// Return true for a short amount of time after hydraulics were last modified to ensure we settle down nicely.
		if (m_pVehicle->InheritsFromAutomobile())
		{
			CAutomobile *pAutomobile = static_cast<CAutomobile*>(GetPed()->GetVehiclePedInside());
			u32 uTimeHydraulicsLastModified = pAutomobile ? pAutomobile->GetTimeHydraulicsModified() : 0;
			TUNE_GROUP_INT(LOWRIDER_TUNE, iTimeToSettleAfterHydraulicsModified, 1500, 0, 10000, 1);
			if (uTimeHydraulicsLastModified + iTimeToSettleAfterHydraulicsModified > fwTimer::GetTimeInMilliseconds())
			{
				return true;
			}
		}

		// If not, check wheels just in case!
		for(int i = 0; i < m_pVehicle->GetNumWheels(); i++)
		{
			CWheel *pWheel = m_pVehicle->GetWheel(i);
			if (pWheel && pWheel->GetSuspensionRaiseAmount() > 0.0f)
			{
				return true;
			}
		}
	}

	return false;
}

// B*2305888: Lowrider lean functions.
void CTaskMotionInAutomobile::ComputeLowriderParameters()
{
	if (!m_bInitializedLowriderSprings)
	{
		m_bInitializedLowriderSprings = true;
		SetupLowriderSpringParameters();
	}

	ComputeLowriderLateralRoll();
	ComputeLowriderLongitudinalRoll();

	// Set window height parameter to blend between high/low window arm anims.
	float fWindowHeight = m_pVehicle->GetVehicleModelInfo() ? m_pVehicle->GetVehicleModelInfo()->GetLowriderArmWindowHeight() : 1.0f;
#if __BANK
	TUNE_GROUP_BOOL(LOWRIDER_TUNE, bOverrideWindowHeight, false);
	TUNE_GROUP_FLOAT(LOWRIDER_TUNE, fWindowHeightOverride, 1.0f, 0.0f, 1.0f, 0.01f);
	if (bOverrideWindowHeight)
	{
		fWindowHeight = fWindowHeightOverride;
	}
#endif

	m_moveNetworkHelper.SetFloat(CTaskMotionInVehicle::ms_WindowHeight, fWindowHeight);

	// Store previous matrix for calculations next frame.
	m_mPreviousLowriderMat = GetPed()->GetMyVehicle()->GetMatrix();
}

void CTaskMotionInAutomobile::SetupLowriderSpringParameters()
{
	// Randomize spring parameters so we have slight variations in movement between the peds in the vehicle.
	// Initialize lateral spring params
	TUNE_GROUP_FLOAT(LOWRIDER_TUNE, fLateralLeanSpringKMinValue, 4.0f, 0.0f, 25.0f, 0.01f);
	TUNE_GROUP_FLOAT(LOWRIDER_TUNE, fLateralLeanSpringKMaxValue, 6.0f, 0.0f, 25.0f, 0.01f);
	TUNE_GROUP_FLOAT(LOWRIDER_TUNE, fLateralLeanMinDampingKMinValue, 12.0f, 0.0f, 25.0f, 0.01f);
	TUNE_GROUP_FLOAT(LOWRIDER_TUNE, fLateralLeanMinDampingKMaxValue, 17.0f, 0.0f, 25.0f, 0.01f);
	TUNE_GROUP_FLOAT(LOWRIDER_TUNE, fLateralLeanMaxExtraDampingKMinValue, 1.0f, 0.0f, 25.0f, 0.01f);
	TUNE_GROUP_FLOAT(LOWRIDER_TUNE, fLateralLeanMaxExtraDampingKMaxValue, 4.0f, 0.0f, 25.0f, 0.01f);
	m_fLateralLeanSpringK = fwRandom::GetRandomNumberInRange(fLateralLeanSpringKMinValue, fLateralLeanSpringKMaxValue);
	m_fLateralLeanMinDampingK = fwRandom::GetRandomNumberInRange(fLateralLeanMinDampingKMinValue, fLateralLeanMinDampingKMaxValue);
	m_fLateralLeanMaxExtraDampingK = fwRandom::GetRandomNumberInRange(fLateralLeanMaxExtraDampingKMinValue, fLateralLeanMaxExtraDampingKMaxValue);

	// Initialize longitudinal spring params
	TUNE_GROUP_FLOAT(LOWRIDER_TUNE, fLongitudinalLeanSpringKMinValue, 12.0f, 0.0f, 50.0f, 0.01f);
	TUNE_GROUP_FLOAT(LOWRIDER_TUNE, fLongitudinalLeanSpringKMaxValue, 14.0f, 0.0f, 50.0f, 0.01f);
	TUNE_GROUP_FLOAT(LOWRIDER_TUNE, fLongitudinalLeanMinDampingKMinValue, 6.0f, 0.0f, 25.0f, 0.01f);
	TUNE_GROUP_FLOAT(LOWRIDER_TUNE, fLongitudinalLeanMinDampingKMaxValue, 8.0f, 0.0f, 25.0f, 0.01f);
	TUNE_GROUP_FLOAT(LOWRIDER_TUNE, fLongitudinalLeanMaxExtraDampingKMinValue, 1.0f, 0.0f, 25.0f, 0.01f);
	TUNE_GROUP_FLOAT(LOWRIDER_TUNE, fLongitudinalLeanMaxExtraDampingKMaxValue, 4.0f, 0.0f, 25.0f, 0.01f);
	m_fLongitudinalLeanSpringK = fwRandom::GetRandomNumberInRange(fLongitudinalLeanSpringKMinValue, fLongitudinalLeanSpringKMaxValue);
	m_fLongitudinalLeanMinDampingK = fwRandom::GetRandomNumberInRange(fLongitudinalLeanMinDampingKMinValue, fLongitudinalLeanMinDampingKMaxValue);
	m_fLongitudinalLeanMaxExtraDampingK = fwRandom::GetRandomNumberInRange(fLongitudinalLeanMaxExtraDampingKMinValue, fLongitudinalLeanMaxExtraDampingKMaxValue);
}

void CTaskMotionInAutomobile::ComputeLowriderLateralRoll()
{	
	// Only calculate lateral lean if player has actively modified the car suspension.
	bool bUseLateralLean = false;
	TUNE_GROUP_BOOL(LOWRIDER_TUNE, bEnableLateralRoll, true);
	if (bEnableLateralRoll && !m_pVehicle->m_nVehicleFlags.bAreHydraulicsAllRaised && HasModifiedLowriderSuspension())
	{
		bUseLateralLean = true;
	}

	const float fDeltaTime = fwTimer::GetTimeStep();
	const float fPreviousLowriderLeanX = m_fLowriderLeanX;
	if (bUseLateralLean || !IsClose(m_fLowriderLeanX, 0.5f, 0.01f))
	{
		TUNE_GROUP_FLOAT(LOWRIDER_TUNE, fLerpRateLateral, 4.0f, 0.0f, 50.0f, 0.01f);
		Approach( m_fLowriderLeanX, ComputeLowriderLeanLateral(), fLerpRateLateral, fDeltaTime );
	}
	else
	{
		TUNE_GROUP_FLOAT(LOWRIDER_TUNE, fLerpRateReturnToMidLateral, 0.5f, 0.0f, 50.0f, 0.01f);
		Approach( m_fLowriderLeanX, 0.5f, fLerpRateReturnToMidLateral, fDeltaTime );
		m_fPreviousLowriderVelocityLateral = 0.0f;
	}

	m_fLowriderLeanXVelocity = (m_fLowriderLeanX - fPreviousLowriderLeanX) / fDeltaTime;

#if __STATS
	TUNE_GROUP_BOOL(LOWRIDER_TUNE, STATS_bRenderLeanX, false);
	if (STATS_bRenderLeanX && GetPed() && GetPed()->IsLocalPlayer())
	{
		PF_SET(LowriderBounceData_fBounceX, m_fLowriderLeanX);
	}
#endif	// __STATS

	m_moveNetworkHelper.SetFloat(CTaskMotionInVehicle::ms_SuspensionMovementLRId, m_fLowriderLeanX);
}

float CTaskMotionInAutomobile::ComputeLowriderLeanLateral()
{
	const float fDeltaTime = fwTimer::GetTimeStep();

	TUNE_GROUP_FLOAT(LOWRIDER_TUNE, fMaxAccelLateral, 50.0f, 0.0f, 200.0f, 0.01f);
	float fRollAcceleration = Clamp( CalculateLowriderAccelerationLateral(), -fMaxAccelLateral, fMaxAccelLateral );
	float fAccelModifier = m_pVehicle->GetVehicleModelInfo() ? m_pVehicle->GetVehicleModelInfo()->GetLowriderLeanAccelModifier() : 1.0f;
	fRollAcceleration *= fAccelModifier;

	const float fSpringForce = CalculateSpringForceLateral();
	const float fCombinedAccelerations = fRollAcceleration + fSpringForce;

	// The desired lean would be the displacement given by the accelerations and the resistance of the spring
	const float fDisplacement = (m_fLowriderLeanXVelocity * fDeltaTime) + (0.5f * fCombinedAccelerations * fDeltaTime * fDeltaTime);
	return Clamp(m_fLowriderLeanX + fDisplacement, 0.0f, 1.0f);
}

float CTaskMotionInAutomobile::CalculateLowriderAccelerationLateral()
{
	const float fDeltaTime = fwTimer::GetTimeStep();

	// Transform the current lowrider matrix by the previous lowrider matrix.
	Mat34V mCurrentLowriderMat = GetPed()->GetMyVehicle()->GetMatrix();
	Mat34V mLocalMat(V_IDENTITY);
	UnTransformFull(mLocalMat, mCurrentLowriderMat, m_mPreviousLowriderMat);
	
	// Get the up vector relative to the previous lowrider matrix.
	Vec3V vLowriderUpVector = mLocalMat.GetCol2();
	Vector2 vLowriderUpVectorOnXZPlane(vLowriderUpVector.GetXf(), vLowriderUpVector.GetZf());
	vLowriderUpVectorOnXZPlane.NormalizeSafe();

	// Calculate the lateral roll velocity.
	float fAngle = rage::Atan2f(-vLowriderUpVectorOnXZPlane.x, vLowriderUpVectorOnXZPlane.y);

	if (Abs(fAngle) < 0.001f)
		fAngle = 0.0f;

	TUNE_GROUP_FLOAT(LOWRIDER_TUNE, fAngVelStrengthModulatorLateral, 5.0f, 0.0f, 50.0f, 0.01f);
	float fRollVelocity = ( fAngle * fAngVelStrengthModulatorLateral ) / fDeltaTime;


	// Calculate the acceleration for this frame.
	const float fRollAcceleration = (fRollVelocity - m_fPreviousLowriderVelocityLateral) / fDeltaTime;

	// Keep track of previous velocity.
	m_fPreviousLowriderVelocityLateral = fRollVelocity;

	return fRollAcceleration;
}

float CTaskMotionInAutomobile::CalculateSpringForceLateral() const
{
	// If lateral velocity is below defined threshold, reduce damping constant. 
	// Ensures spring actually returns us to 0.5f when little/no lateral roll is being applied.
	float fSpringKModifier = 1.0f;
	float fDampingK = m_fLateralLeanMinDampingK;
	TUNE_GROUP_FLOAT(LOWRIDER_TUNE, fDampingVelocityThreshold, 0.25f, 0.0f, 1.0f, 0.01f);
	if (Abs(m_fPreviousLowriderVelocityLateral) < fDampingVelocityThreshold)
	{
		TUNE_GROUP_FLOAT(LOWRIDER_TUNE, fLowVelMinLateralDampingK, 0.1f, 0.0f, 50.0f, 0.01f);
		TUNE_GROUP_FLOAT(LOWRIDER_TUNE, fLowVelMaxLateralDampingK, 5.0f, 0.0f, 50.0f, 0.01f);
		float fLerp = Abs(m_fPreviousLowriderVelocityLateral) / fDampingVelocityThreshold;
		fDampingK = rage::Lerp(fLerp, fLowVelMinLateralDampingK, fLowVelMaxLateralDampingK);

		// Increase spring strength to return us back quicker.
		TUNE_GROUP_FLOAT(LOWRIDER_TUNE, fSpringLowVelocityModifier, 1.25f, 0.0f, 10.0f, 0.01f);
		fSpringKModifier = fSpringLowVelocityModifier;
	}

	// Get the resistance that the spring applies based on its constant K.
	const float fSpringForce = -(m_fLateralLeanSpringK * fSpringKModifier) * (m_fLowriderLeanX - 0.5f);

	// Get the amount of damping (to prevent the spring from swinging)
	float fDamping = -m_fLowriderLeanXVelocity * (fDampingK + m_fLateralLeanMaxExtraDampingK);

	// Resulting force of applying a given force to a spring
	return fSpringForce + fDamping;
}

void CTaskMotionInAutomobile::ComputeLowriderLongitudinalRoll()
{
	const float fDeltaTime = fwTimer::GetTimeStep();
	const float fPreviousLowriderLeanY = m_fLowriderLeanY;

	TUNE_GROUP_BOOL(LOWRIDER_TUNE, bEnableLongitudinalRoll, true);
	TUNE_GROUP_FLOAT(LOWRIDER_TUNE, fLerpRateLongitudinal, 2.0f, 0.0f, 50.0f, 0.01f);
	
	if (bEnableLongitudinalRoll)
	{
		Approach( m_fLowriderLeanY, ComputeLowriderLeanLongitudinal(), fLerpRateLongitudinal, fDeltaTime );
	}
	else
	{
		TUNE_GROUP_FLOAT(LOWRIDER_TUNE, fLerpRateReturnToMidLongitudinal, 2.0f, 0.0f, 50.0f, 0.01f);
		Approach( m_fLowriderLeanY, 0.5f, fLerpRateReturnToMidLongitudinal, fDeltaTime );
		m_fPreviousLowriderVelocityLongitudinal = 0.0f;
	}

	m_fLowriderLeanYVelocity = (m_fLowriderLeanY - fPreviousLowriderLeanY) / fDeltaTime;

#if __STATS
	TUNE_GROUP_BOOL(LOWRIDER_TUNE, STATS_bRenderLeanY, true);
	if (STATS_bRenderLeanY && GetPed() && GetPed()->IsLocalPlayer())
	{
		PF_SET(LowriderBounceData_fBounceY, m_fLowriderLeanY);
	}
#endif	// __STATS

	m_moveNetworkHelper.SetFloat(CTaskMotionInVehicle::ms_SuspensionMovementUDId, m_fLowriderLeanY);
}

float CTaskMotionInAutomobile::ComputeLowriderLeanLongitudinal()
{
	const float fDeltaTime = fwTimer::GetTimeStep();

	TUNE_GROUP_FLOAT(LOWRIDER_TUNE, fMaxAccelLongitudinal, 50.0f, 0.0f, 200.0f, 0.01f);
	const float fLongitudinalAcceleration = Clamp( CalculateLowriderAccelerationLongitudinal(), -fMaxAccelLongitudinal, fMaxAccelLongitudinal );

	TUNE_GROUP_FLOAT(LOWRIDER_TUNE, fMaxZAccelLongitudinal, 20.0f, 0.0f, 200.0f, 0.01f);
	const float fAccelerationZ = Clamp( CalculateLowriderAccelerationZ(), -fMaxZAccelLongitudinal, fMaxZAccelLongitudinal );

	// Only use vertical Z acceleration if it's larger than our angular acceleration (ie for popping all suspension up/down).
	float fAcceleration = (Abs(fAccelerationZ) > Abs(fLongitudinalAcceleration)) ? fAccelerationZ : fLongitudinalAcceleration;

	float fAccelModifier = m_pVehicle->GetVehicleModelInfo() ? m_pVehicle->GetVehicleModelInfo()->GetLowriderLeanAccelModifier() : 1.0f;
	fAcceleration *= fAccelModifier;

	const float fSpringForce = CalculateSpringForceLongitudinal();

	const float fCombinedAccelerations = fAcceleration + fSpringForce;

	// The desired lean would be the displacement given by the accelerations and the resistance of the spring
	const float fDisplacement = (m_fLowriderLeanYVelocity * fDeltaTime) + (0.5f * fCombinedAccelerations * fDeltaTime * fDeltaTime);
	return Clamp(m_fLowriderLeanY + fDisplacement, 0.0f, 1.0f);
}

float CTaskMotionInAutomobile::CalculateLowriderAccelerationLongitudinal()
{
	const float fDeltaTime = fwTimer::GetTimeStep();

	// Transform the current lowrider matrix by the previous lowrider matrix.
	Mat34V mCurrentLowriderMat = GetPed()->GetMyVehicle()->GetMatrix();
	Mat34V mLocalMat(V_IDENTITY);
	UnTransformFull(mLocalMat, mCurrentLowriderMat, m_mPreviousLowriderMat);

	// Get the up vector relative to the previous lowrider matrix.
	Vec3V vLowriderForwardVector = mLocalMat.GetCol1();
	Vector2 vLowriderForwardVectorOnZYPlane(vLowriderForwardVector.GetZf(), vLowriderForwardVector.GetYf());
	vLowriderForwardVectorOnZYPlane.NormalizeSafe();

	// Calculate the longitudinal roll velocity.
	float fAngle = (rage::Atan2f(-vLowriderForwardVectorOnZYPlane.x, vLowriderForwardVectorOnZYPlane.y));
	TUNE_GROUP_FLOAT(LOWRIDER_TUNE, fAngVelStrengthModulatorLongitudinal, 8.0f, 0.0f, 50.0f, 0.01f);
	const float fLowriderAngularVelocity = ( fAngle * fAngVelStrengthModulatorLongitudinal ) / fDeltaTime;

	// Calculate the acceleration for this frame.
	const float fLongitudinalAcceleration = (fLowriderAngularVelocity - m_fPreviousLowriderVelocityLongitudinal) / fDeltaTime;

	// Keep track of previous velocity.
	m_fPreviousLowriderVelocityLongitudinal = fLowriderAngularVelocity;

	return fLongitudinalAcceleration;
}

float CTaskMotionInAutomobile::CalculateLowriderAccelerationZ()
{
	const float fDeltaTime = fwTimer::GetTimeStep();

	const CPed& ped = *GetPed();
	const CVehicle& vehicle = *ped.GetMyVehicle();

	// Get the acceleration of the lowrider between frames
	const Vector3 vCurrentVelocity = vehicle.GetVelocity();
	const Vector3 vAcceleration = (vCurrentVelocity - m_vPreviousLowriderVelocity) /  fDeltaTime;
	m_vPreviousLowriderVelocity = vCurrentVelocity;

	// We are just interested in the swaying of the ped in the Z plane, for that reason we are going to transform the acceleration vector over the peds matrix to just subtract the acceleration on the Z plane of the ped
	const Vec3V vTransformedAccel = ped.GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vAcceleration));

	// Modulate the strength of the acceleration so we can make it more or less obvious
	TUNE_GROUP_FLOAT(LOWRIDER_TUNE, fZVelStrengthModulatorLongitudinal, 2.0f, 0.0f, 50.0f, 0.01f);
	return (vTransformedAccel.GetZf() * fZVelStrengthModulatorLongitudinal);
}

float CTaskMotionInAutomobile::CalculateSpringForceLongitudinal() const
{
	// Get the resistance that the spring applies based on its constant K
	const float fSpringForce = -m_fLongitudinalLeanSpringK * (m_fLowriderLeanY - 0.5f);

	// Get the amount of damping (to prevent the spring from swinging)
	const float fDamping = -m_fLowriderLeanYVelocity * (m_fLongitudinalLeanMinDampingK + m_fLongitudinalLeanMaxExtraDampingK);

	// Resulting force of applying a given force to a spring
	return fSpringForce + fDamping;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInAutomobile::SetClipPlaybackRate()
{
	const CAnimRateSet& rAnimRateSet = m_pVehicle->GetLayoutInfo()->GetAnimRateSet();
	if (rAnimRateSet.m_UseInVehicleCombatRates)
	{
		float fClipRate = 1.0f;

		// Otherwise determine if we should speed up the entry anims and which tuning values to use (anim vs non anim)
		const CPed& rPed = *GetPed();
		const bool bShouldUseCombatEntryRates = rPed.IsLocalPlayer() && 
			(rPed.WantsToUseActionMode() 
			|| rPed.IsUsingStealthMode() 
			|| rPed.GetPlayerWanted()->GetWantedLevel() > WANTED_CLEAN 
			|| CPlayerInfo::AreCNCResponsivenessChangesEnabled(&rPed));

		// Set the clip rate based on our conditions
		if (bShouldUseCombatEntryRates)
		{
			fClipRate = rAnimRateSet.m_NoAnimCombatInVehicle.GetRate();
		}
		else
		{
			fClipRate = rAnimRateSet.m_NormalInVehicle.GetRate();
		}

		switch (GetState())
		{
			case State_StartEngine:
				{
					m_moveNetworkHelper.SetFloat(CTaskMotionInVehicle::ms_StartEngineRateId, fClipRate);
					break;
				}
			case State_CloseDoor:
				{
					m_moveNetworkHelper.SetFloat(CTaskCloseVehicleDoorFromInside::ms_CloseDoorRateId, fClipRate);
					break;
				}
			case State_Hotwiring:
				{
					const CPedModelInfo* mi = rPed.GetPedModelInfo();
					Assert(mi);
					const CPedModelInfo::PersonalityData& pd = mi->GetPersonalitySettings();
					const float fHotwireRate = pd.GetHotwireRate();
					m_moveNetworkHelper.SetFloat(ms_HotwireRate, Max(fHotwireRate, fClipRate));
					break;
				}
			default: break;
		}
	}
	else
	{
		const CPed& rPed = *GetPed();
		if (GetState() == State_StartEngine)
		{
			float fClipRate = 1.0f;
			float fScaleMultiplier = 1.0f;
			if (CAnimSpeedUps::ShouldUseMPAnimRates() || rPed.GetPedResetFlag(CPED_RESET_FLAG_UseFastEnterExitVehicleRates))
			{
				fScaleMultiplier = CAnimSpeedUps::ms_Tunables.m_MultiplayerEnterExitJackVehicleRateModifier;
			}

			fClipRate *= fScaleMultiplier;
			m_moveNetworkHelper.SetFloat(CTaskMotionInVehicle::ms_StartEngineRateId, fClipRate);
		}

		if (GetState() == State_Hotwiring && rPed.IsPlayer())
		{
			const CPedModelInfo* mi = rPed.GetPedModelInfo();
			Assert(mi);
			const CPedModelInfo::PersonalityData& pd = mi->GetPersonalitySettings();

			m_moveNetworkHelper.SetFloat(ms_HotwireRate, pd.GetHotwireRate());
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::SetClipsForState()
{
	const s32 iState = GetState();
	fwMvClipId moveClipId = CLIP_ID_INVALID;
	const crClip* pClip = GetClipForStateWithAssert(iState, &moveClipId);
	if (pClip)
	{
		m_moveNetworkHelper.SetClip(pClip, moveClipId);
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::CheckForChangingStation(const CVehicle& rVeh, const CPed& rPed) const
{
	if (!CTaskMotionInVehicle::CheckForChangingStation(rVeh, rPed))
		return false;

	if (!GetClipForStateNoAssert(State_ChangeStation))
		return false;

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::CheckForStillToSitTransition(const CVehicle& rVeh, const CPed& rPed) const
{
	if (rPed.GetPedResetFlag(CPED_RESET_FLAG_PreventGoingIntoStillInVehicleState))
	{
		return true;
	}

	TUNE_GROUP_FLOAT(VEHICLE_TUNE, MIN_TIME_IN_STILL_STATE_FROM_REVERSE, 0.35f, 0.0f, 1.0f, 0.01f);
	if (GetPreviousState() == State_Reverse && GetTimeInState() < MIN_TIME_IN_STILL_STATE_FROM_REVERSE)
	{
		return false;
	}

	if (CheckIfPedWantsToMove(rPed, *m_pVehicle) > 0.0f)
	{
		return true;
	}

	if (CheckIfVehicleIsDoingBurnout(rVeh, rPed))
	{
		return true;
	}

	if (rVeh.GetVelocityIncludingReferenceFrame().Mag2() > ms_Tunables.m_ForcedLegUpVelocity)
	{
		return true;
	}

	if( rVeh.InheritsFromBike() && !rVeh.HasGlider() && rVeh.GetSpecialFlightModeRatio() > 0.5f )
	{
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::ShouldUseSitToStillTransition(const CVehicle& rVeh, const CPed& rPed) const
{
	// Bike always hovers, don't allow transition into still state
	if (MI_BIKE_OPPRESSOR2.IsValid() && rVeh.GetModelIndex() == MI_BIKE_OPPRESSOR2)
	{
		return false;
	}

	if (!rVeh.GetLayoutInfo()->GetUseStillToSitTransition())
	{
		return false;
	}

	if (rVeh.GetDriver() != &rPed)
	{
		return false;
	}
	
	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::CheckForSitToStillTransition(const CVehicle& rVeh, const CPed& rPed) const
{
	if (rPed.GetPedResetFlag(CPED_RESET_FLAG_PreventGoingIntoStillInVehicleState))
	{
		return false;
	}

	TUNE_GROUP_FLOAT(VEHICLE_TUNE, SIT_TO_STILL_MAX_TRIGGER_VELOCITY, 3.0f, 0.0f, 10.0f, 0.01f);
	if (rVeh.GetVelocityIncludingReferenceFrame().Mag2() > square(SIT_TO_STILL_MAX_TRIGGER_VELOCITY))
	{
		return false;
	}

	if (CheckIfPedWantsToMove(rPed, *m_pVehicle) > 0.0f)
	{
		return false;
	}

	if (IsDoingAWheelie(*m_pVehicle))
	{
		return false;
	}

	if (CheckIfVehicleIsDoingBurnout(rVeh, rPed))
	{
		return true;
	}

	if (rVeh.GetVelocityIncludingReferenceFrame().Mag2() <= square(ms_Tunables.m_SitToStillForceTriggerVelocity))
	{
		return true;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

float CTaskMotionInAutomobile::CheckIfPedWantsToMove(const CPed& rPed, const CVehicle& rVeh) const
{
	if (rPed.IsLocalPlayer())
	{
		const CControl* pControl = rPed.GetControlFromPlayer();
		if (pControl)
		{
			const float fAccelerate = pControl->GetVehicleAccelerate().GetNorm();
			const float fDeccelerate = Max(pControl->GetVehicleBrake().GetNorm(), pControl->GetVehicleHandBrake().GetNorm());
			const float fForwardNess = fAccelerate - fDeccelerate;
			return fForwardNess;
		}
	}
	else
	{
		TUNE_GROUP_FLOAT(IN_VEHICLE_TUNE, MIN_VELOCITY_FOR_CLONE_MOVING, 0.1f, 0.0f, 1.0f, 0.01f);
		if (rPed.IsNetworkClone() && rVeh.GetVelocity().Mag2() < square(MIN_VELOCITY_FOR_CLONE_MOVING))
		{
			return 0.0;
		}
		const float fAccelerate = rVeh.GetThrottle();
		const float fDeccelerate = rVeh.GetBrake();
		const float fForwardNess = fAccelerate - fDeccelerate;
		return fForwardNess;
	}
	return 0.0;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::CheckIfVehicleIsDoingBurnout(const CVehicle& rVeh, const CPed& rPed)
{
	if (rVeh.InheritsFromBike() || rVeh.GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_IS_QUADBIKE_USING_BIKE_ANIMATIONS))
	{
		TUNE_GROUP_FLOAT(VEHICLE_TUNE, MIN_LINEAR_VELOCITY_FOR_BURNOUT, 1.0f, 0.0f, 10.0f, 0.01f);
		if (rVeh.GetVelocityIncludingReferenceFrame().Mag2() > square(MIN_LINEAR_VELOCITY_FOR_BURNOUT))
		{
			return false;
		}

		if (MI_BIKE_OPPRESSOR2.IsValid() && rVeh.GetModelIndex() == MI_BIKE_OPPRESSOR2 && rVeh.IsInAir())
		{
			return false;
		}

		const float fBurnoutTorque = static_cast<const CBike&>(rVeh).m_fBurnoutTorque;
		if (Abs(fBurnoutTorque) > 0.0f)
		{
			return true;
		}

		TUNE_GROUP_FLOAT(VEHICLE_TUNE, BURNOUT_INPUT, 1.0f, 0.0f, 1.0f, 0.01f);
		if (rPed.IsLocalPlayer())
		{
			const CControl* pControl = rPed.GetControlFromPlayer();
			if (pControl)
			{
				const float fAccelerate = pControl->GetVehicleAccelerate().GetNorm();
				const float fBreak = pControl->GetVehicleBrake().GetNorm();
				const float fHandBreak = pControl->GetVehicleHandBrake().GetNorm();

				if (fHandBreak >= fBreak && fHandBreak >= fAccelerate)
				{
					return false;
				}
				return fAccelerate >= BURNOUT_INPUT && fBreak >= BURNOUT_INPUT;
			}
		}
		else
		{
			const float fAccelerate = rVeh.GetThrottle();
			const float fDeccelerate = rVeh.GetBrake();
			return fAccelerate >= BURNOUT_INPUT && fDeccelerate >= BURNOUT_INPUT;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::IsDoingAWheelie(const CVehicle& rVeh)
{
	if (!rVeh.InheritsFromBike())
		return false;
	
	// If we're wheeling don't consider as still (so we don't put our leg down)
	const CBike& rBike = static_cast<const CBike&>(rVeh);
	if (rBike.GetWheel(1) && !rBike.GetWheel(1)->GetIsTouching())
		return true;
	
	return false;
}

bool CTaskMotionInAutomobile::CheckForShuntFromHighFall(const CVehicle& rVeh, const CPed& rPed, bool bForFPSCameraShake)
{
	if (rPed.IsInFirstPersonVehicleCamera() && !bForFPSCameraShake)
	{
		return false;
	}

	if (rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_IsDuckingInVehicle))
	{
		return false;
	}

	if (rPed.GetPedResetFlag(CPED_RESET_FLAG_PreventGoingIntoShuntInVehicleState))
	{
		return false;
	}

	if (rPed.GetIsInVehicle())
	{
		const CVehicleSeatAnimInfo* pSeatClipInfo = rVeh.GetSeatAnimationInfo(&rPed);
		if (pSeatClipInfo && pSeatClipInfo->GetNoShunts())
		{
			return false;
		}
	}

	if (m_bWasJustInAir)
	{
		const float fJumpHeight = m_fMaxHeightInAir - m_fMinHeightInAir;

		if(rVeh.InheritsFromBoat())
		{
			TUNE_GROUP_FLOAT(VEHICLE_MOTION, JUMP_HEIGHT_FOR_FORCED_SHUNT_JETSKI, 2.0f, 0.0f, 30.0f, 0.1f);
			TUNE_GROUP_FLOAT(VEHICLE_MOTION, JUMP_HEIGHT_FOR_FORCED_SHUNT_BOAT, 3.0f, 0.0f, 30.0f, 0.1f);

			float fShuntHeight = rVeh.GetIsJetSki() ? JUMP_HEIGHT_FOR_FORCED_SHUNT_JETSKI : JUMP_HEIGHT_FOR_FORCED_SHUNT_BOAT;

			if (fJumpHeight > fShuntHeight)
			{
				return true;
			}
		}
		else if(rVeh.InheritsFromBike())
		{
			TUNE_GROUP_FLOAT(VEHICLE_MOTION, JUMP_HEIGHT_FOR_FORCED_SHUNT, 10.0f, 0.0f, 30.0f, 0.1f);
			TUNE_GROUP_FLOAT(VEHICLE_MOTION, JUMP_HEIGHT_FOR_FPS_CAM_SHAKE_BIKES, 1.0f, 0.0f, 30.0f, 0.1f);
			if (fJumpHeight > JUMP_HEIGHT_FOR_FORCED_SHUNT || (bForFPSCameraShake && fJumpHeight > JUMP_HEIGHT_FOR_FPS_CAM_SHAKE_BIKES))
			{
				return true;
			}
		}
		else if (bForFPSCameraShake && rVeh.InheritsFromPlane())
		{
			TUNE_GROUP_FLOAT(VEHICLE_MOTION, PLANE_DOWNWARD_VEL_FOR_FPS_CAM_SHAKE_PLANES, 3.0f, 0.0f, 100.0f, 0.1f);

			if(const phCollider* pCollider = rVeh.GetCollider())
			{
				if(pCollider->GetVelocity().GetZf() < -PLANE_DOWNWARD_VEL_FOR_FPS_CAM_SHAKE_PLANES
					|| pCollider->GetLastVelocity().GetZf() < -PLANE_DOWNWARD_VEL_FOR_FPS_CAM_SHAKE_PLANES)
				{
					return true;
				}
			}
		}
		else if (bForFPSCameraShake && rVeh.InheritsFromAutomobile() && !rVeh.InheritsFromHeli())
		{
			TUNE_GROUP_FLOAT(VEHICLE_MOTION, JUMP_HEIGHT_FOR_FPS_CAM_SHAKE_CARS, 1.0f, 0.0f, 30.0f, 0.1f);
			if (fJumpHeight > JUMP_HEIGHT_FOR_FPS_CAM_SHAKE_CARS)
			{
				return true;
			}
		}	
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionInAutomobile::CheckForStartingEngine(const CVehicle& vehicle, const CPed& ped)
{
	if (ped.GetPedResetFlag(CPED_RESET_FLAG_DisableInVehicleActions))
		return false;

	const bool bIsDriveable = IsVehicleDrivable(*m_pVehicle);
	if (bIsDriveable || !m_bHasTriedToStartUndriveableVehicle)
	{
		if (!bIsDriveable)
		{
			m_bHasTriedToStartUndriveableVehicle = true;
		}
		return CTaskMotionInVehicle::CheckForStartingEngine(vehicle, ped);
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInAutomobile::ProcessFacial()
{
	// Don't process when in these vehicles
	if (m_pVehicle->InheritsFromHeli() || m_pVehicle->InheritsFromPlane() || m_pVehicle->InheritsFromBlimp() || m_pVehicle->InheritsFromSubmarine())
	{
		return;
	}

	// Ensure the facial data is valid.
	CPed& rPed = *GetPed();
	CFacialDataComponent* pFacialData = rPed.GetFacialData();
	if (!pFacialData)
	{
		return;
	}

	// Blend in the drive fast facials when going fast
	if (m_pVehicle->GetVelocityIncludingReferenceFrame().Mag2() >= square(ms_Tunables.m_MinSpeedToBlendInDriveFastFacial))
	{
		pFacialData->RequestFacialIdleClip(CFacialDataComponent::FICT_DriveFast);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInAutomobile::ProcessAutoCloseDoor()
{
	const CPed& rPed = *GetPed();
	const s32 iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(&rPed);

	if (rPed.IsLocalPlayer() && (rPed.IsInFirstPersonVehicleCamera() FPS_MODE_SUPPORTED_ONLY(|| rPed.IsFirstPersonShooterModeEnabledForPlayer(false))))
	{
		const int state = GetState();

		// Allow State_FirstPersonSitIdle to run for at least one update in case it can handle the closing of the door
		if ((state == State_FirstPersonSitIdle) && (GetTimeInState() == 0.0f) && (GetPreviousState() != State_CloseDoor))
			return;

		// Door is already being driven closed
		if ((state == State_ReserveDoorForClose) || (state == State_CloseDoor))
			return;
	}

	CTaskVehicleFSM::ProcessAutoCloseDoor(rPed, *m_pVehicle, iSeatIndex);

	// So door closes when no one is inside the seat linked to it
	if (rPed.GetIsDrivingVehicle() && m_pVehicle->InheritsFromPlane() && !m_pVehicle->GetLayoutInfo()->GetClimbUpAfterOpenDoor())
	{
		const s32 iHatchSeatIndex = m_pVehicle->FindSeatIndexForFirstHatchEntryPoint();
		if (m_pVehicle->IsSeatIndexValid(iHatchSeatIndex))
		{
			CTaskVehicleFSM::ProcessAutoCloseDoor(rPed, *m_pVehicle, iHatchSeatIndex);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInAutomobile::SetUpRagdollOnCollision(CPed& rPed, CVehicle& rVeh)
{
	// If the vehicle is on moving ground then it makes things too complicated to allow the ragdoll to activate based on collision
	// The ped ends up getting attached to the bike which forces the ped capsule to deactivate which stops the capsule's position from being updated each physics frame
	// which stops the ragdoll bounds from being updated each physics frame which will cause problems on moving ground (large contact depths since the moving ground and
	// ragdoll bounds will be out-of-sync)
	// Best to just not allow activating the ragdoll based on collision
	if (!rPed.IsNetworkClone() && !rPed.GetActivateRagdollOnCollision())
	{
		CTask* pTaskNM = rage_new CTaskNMJumpRollFromRoadVehicle(1000, 10000, false, false, atHashString::Null(), &rVeh);
		CEventSwitch2NM event(10000, pTaskNM);
		rPed.SetActivateRagdollOnCollisionEvent(event);
		rPed.SetActivateRagdollOnCollision(true);
		rPed.SetRagdollOnCollisionIgnorePhysical(&rVeh);
		rPed.SetNoCollisionEntity(&rVeh);
		TUNE_GROUP_FLOAT(RAGDOLL_ON_SIDE_TUNE, ALLOWED_SLOPE, -0.55f, -1.5f, 1.5f, 0.01f);
		rPed.SetRagdollOnCollisionAllowedSlope(ALLOWED_SLOPE);
		TUNE_GROUP_FLOAT(RAGDOLL_ON_SIDE_TUNE, ALLOWED_PENTRATION, 0.1f, 0.0f, 1.0f, 0.01f);
		rPed.SetRagdollOnCollisionAllowedPenetration(ALLOWED_PENTRATION);
		rPed.SetRagdollOnCollisionAllowedPartsMask(0xFFFFFFFF & ~(BIT(RAGDOLL_HAND_LEFT) | BIT(RAGDOLL_HAND_RIGHT) | BIT(RAGDOLL_FOOT_LEFT) | BIT(RAGDOLL_FOOT_RIGHT)));
		rPed.GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_NEVER_ACTIVATE, false);

		if(rPed.IsLocalPlayer())
		{
			// This type of ejection can cause ragdoll instability, so instruct the inst
			// to try to prevent it vie temporary increased stiffness and increased solver iterations.
			rPed.GetRagdollInst()->GetCacheEntry()->ActivateInstabilityPrevention();
		}
		m_bSetUpRagdollOnCollision = true;
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInAutomobile::ResetRagdollOnCollision(CPed& rPed)
{
	rPed.SetNoCollisionEntity(NULL);
	rPed.SetActivateRagdollOnCollision(false);
	rPed.SetRagdollOnCollisionIgnorePhysical(NULL);
	rPed.ClearActivateRagdollOnCollisionEvent();
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInAutomobile::SmoothSteeringWheelSpeedBasedOnInput(const CVehicle& vehicle, 
																   float& fCurrentSteeringRate,
																   float fSteeringSafeZone,
																   u32 iTimeToAllowFastSteering,
																   u32 iTimeToAllowSlowSteering,
																   float fSmoothRateToFastSteering,
																   float fSmoothRateToSlowSteering,
																   float fMaxSteeringRate,
																   float fMinSteeringRate,
																   float fMinVelocityToModifyRate,
																   float fMaxVelocityToModifyRate)
{
	// Turn the steering angle into a value between 0->1
	CVehicleModelInfo* pVehicleModelInfo = vehicle.GetVehicleModelInfo();
	CHandlingData* pHandlingData = CHandlingDataMgr::GetHandlingData( pVehicleModelInfo->GetHandlingId() );
	float fSteeringAngle = vehicle.InheritsFromPlane() ? static_cast<const CPlane*>(&vehicle)->GetRollControl() * -1.0f : vehicle.GetSteerAngle();
	float fRawSteeringAngle = fSteeringAngle;

	taskAssert(pHandlingData->m_fSteeringLock != 0.0f);
	fSteeringAngle /= pHandlingData->m_fSteeringLock;
	fSteeringAngle = (fSteeringAngle * 0.5f) + 0.5f;

	// Clamp the steering value between 0.0f (no steering) and 0.5f (max steering)
	fSteeringAngle = abs( rage::Clamp(fSteeringAngle, 0.0f, 1.0f) - 0.5f );

	// B*2138934: Make steering less twitchy.
	TUNE_GROUP_BOOL(STEERING_TUNE, bENABLE_NEW_STEERING_WHEEL_SPEED_FPS, true);
	float fSteeringAngleDelta = Abs(m_fSteeringWheelAngle - fRawSteeringAngle);
	if (bENABLE_NEW_STEERING_WHEEL_SPEED_FPS)
	{
		bool bPlayerIsSteering = fSteeringAngle > fSteeringSafeZone;

		// We've had a big change in steering angle (between the steering wheel and the vehicle wheels). 
		// Cache the delta and use it to calculate an appropriate steering rate.
		// We need to do this otherwise the rate will slow down as the delta gets progressively smaller.
		static dev_float fAngleThreshold = pHandlingData->m_fSteeringLock * 0.5f;
		if (fSteeringAngleDelta > fAngleThreshold)
		{
			bool bShouldOverrideDelta = fSteeringAngleDelta > m_fSteeringAngleDelta;
			if (m_fSteeringAngleDelta == -1.0f || bShouldOverrideDelta)
			{
				m_fSteeringAngleDelta = fSteeringAngleDelta;
			}
		}

		// Clear the cached delta once the delta is close to zero or if the player is no longer steering.
		static dev_float fClearThreshold = 0.01f;
		if (!bPlayerIsSteering || (m_fSteeringAngleDelta != -1.0f && (fSteeringAngleDelta < fClearThreshold)))
		{
			m_fSteeringAngleDelta = -1.0f;
		}
	}

	// Get the time
	const float fTimeWarpScaleSqd = square( fwTimer::GetTimeWarpActive() );
	const u32 uCurrentTime = fwTimer::GetTimeInMilliseconds();

	if( (fSteeringAngle > fSteeringSafeZone) || (m_fSteeringAngleDelta != -1.0f))
	{
		// Lerp fast to the maximum steering rate value since the player
		// wants to make a hard turn
		if( uCurrentTime - m_iLastTimeWheelWasInSlowZone > iTimeToAllowFastSteering )
		{
			if (bENABLE_NEW_STEERING_WHEEL_SPEED_FPS)
			{
				// Scale the max rate based on vehicle speed.
				float fVehicleVelocity = vehicle.GetVelocityIncludingReferenceFrame().Mag();
				float fMaxSteeringRateModified = fMaxSteeringRate;
				if (fVehicleVelocity >= fMinVelocityToModifyRate && fVehicleVelocity <= fMaxVelocityToModifyRate)
				{
					float fSpeedRateMult = fVehicleVelocity / (fMinVelocityToModifyRate + fMaxVelocityToModifyRate);
					fMaxSteeringRateModified *= fSpeedRateMult;
				}
				else if (fVehicleVelocity < fMinVelocityToModifyRate)
				{
					fMaxSteeringRateModified = fMinSteeringRate;
				}

				// Calculate the rate multiplier based on the difference between the angle delta and one lock of the steering wheel.
				TUNE_GROUP_BOOL(STEERING_TUNE, bScaleBasedOnFullWheelRotation, false);
				const float fMaxDelta = bScaleBasedOnFullWheelRotation ? pHandlingData->m_fSteeringLock * 2.0f : pHandlingData->m_fSteeringLock;
				float fDelta = m_fSteeringAngleDelta != -1.0f ? m_fSteeringAngleDelta : fSteeringAngleDelta;
				float fRateMult = fDelta / fMaxDelta;

				// Apply the multiplier to the max steering rate parameter. Never go below the defined minimum rate.
				fMaxSteeringRateModified *= fRateMult;
				fMaxSteeringRateModified = rage::Max(fMinSteeringRate, fMaxSteeringRateModified);

				// Clamp the max rate modifier.
				fMaxSteeringRate = rage::Clamp(fMaxSteeringRateModified, 0.0f, fMaxSteeringRate);
			}

			fCurrentSteeringRate = rage::Lerp(fSmoothRateToFastSteering * fTimeWarpScaleSqd, fCurrentSteeringRate, fMaxSteeringRate);
		}

		m_iLastTimeWheelWasInFastZone = fwTimer::GetTimeInMilliseconds();
	}
	else
	{
		// Reset the timer
		m_iLastTimeWheelWasInSlowZone = fwTimer::GetTimeInMilliseconds();

		// Lerp slowly to the minimum steering rate (if we are not already there)
		// since the player is just tapping the stick
		if( uCurrentTime - m_iLastTimeWheelWasInFastZone > iTimeToAllowSlowSteering && fCurrentSteeringRate > fMinSteeringRate + 0.1f )
		{
			// Lerp slowly back to normal
			fCurrentSteeringRate = rage::Lerp(fSmoothRateToSlowSteering * fTimeWarpScaleSqd, fCurrentSteeringRate, fMinSteeringRate);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInAutomobile::StartFirstPersonCameraShake(const float fJumpHeight)
{
#if FPS_MODE_SUPPORTED
	static atHashWithStringNotFinal s_hShake("VEH_IMPACT_PITCH_HEADING_SHAKE_FPS");

	// Ensure this is the local player.
	if(!GetPed()->IsLocalPlayer())
	{
		return;
	}

	// Ensure first person is enabled.
	if(!GetPed()->IsInFirstPersonVehicleCamera())
	{
		return;
	}

	// Ensure the camera is valid.
	camCinematicMountedCamera* pCamera = camInterface::GetGameplayDirector().GetFirstPersonVehicleCamera();
	if(pCamera == NULL)
	{
		return;
	}

	// Ensure the jump height is valid.
	if (fJumpHeight < 0.0f)
	{
		return;
	}

	float fAmplitude = 1.0f;
	TUNE_GROUP_FLOAT(VEHICLE_MOTION, FPS_CAM_SHAKE_HEIGHT_FOR_FULL_SHAKE, 2.5f, 0.0f, 30.0f, 0.1f);
	fAmplitude = fJumpHeight / FPS_CAM_SHAKE_HEIGHT_FOR_FULL_SHAKE;
	TUNE_GROUP_FLOAT(VEHICLE_MOTION, FPS_CAM_SHAKE_HEIGHT_MAX_CLAMP, 12.5f, 0.0f, 30.0f, 0.1f);
	fAmplitude = Clamp(fAmplitude, 0.0f, FPS_CAM_SHAKE_HEIGHT_MAX_CLAMP);

	// Start the shake.
	pCamera->Shake(s_hShake.GetHash(), fAmplitude);

	// Do some pad rumble (use seperate rumble effect so it doesn't override/get overriden by other pad shakes; ie from CVehicle::ProcessRumble).
	CControl *pControl = GetPed()->GetControlFromPlayer();
	if (pControl)
	{		
		TUNE_GROUP_FLOAT(VEHICLE_MOTION, FPS_CAM_SHAKE_PAD_SHAKE_AMP_MULT, 1.0f, 0.0f, 30.0f, 0.1f);
		TUNE_GROUP_FLOAT(VEHICLE_MOTION, FPS_PAD_RUMBLE_DURATION_MS, 300.0f, 0.0f, 1000.0f, 0.1f);
		u32 iFPSRumbleDuration = (u32)FPS_PAD_RUMBLE_DURATION_MS;
		float fRumbleIntensity = Clamp(fAmplitude, 0.0f, 1.0f);
		m_RumbleEffect = ioRumbleEffect(iFPSRumbleDuration, fRumbleIntensity, fRumbleIntensity);
		// Don't need to pass in duration/intesity values as these have already been set in m_RumbleEffect
		pControl->ShakeController(0, 0, false, &m_RumbleEffect);
	}
#endif	//FPS_MODE_SUPPORTED
}

////////////////////////////////////////////////////////////////////////////////

const crClip* CTaskMotionInAutomobile::GetClipForState(s32 iState, fwMvClipId* pMvClipId, bool ASSERT_ONLY(bAssertIfClipDoesntExist)) const
{
	const fwMvClipSetId clipsetId = m_moveNetworkHelper.GetClipSetId();
	fwMvClipId clipId = CLIP_ID_INVALID;
	bool bValidState = false;

	switch (iState)
	{	
		case State_PutOnHelmet:
			{
				if (pMvClipId)
				{
					*pMvClipId = ms_PutOnHelmetClipId;
				}

				if (GetPed() && GetPed()->IsInFirstPersonVehicleCamera())
				{
					CVehicle *pVehicle = static_cast<CVehicle*>(GetPed()->GetVehiclePedInside());
					bool bIsAircraft = pVehicle && pVehicle->GetIsAircraft();
					crClip* pClip = NULL;

					// Use new generic anim set (upper body only) for aircraft
					if (bIsAircraft)
					{
						clipId = ms_Tunables.m_PutOnHelmetFPSClipId;
						const fwMvClipSetId sClipSetId = CPedHelmetComponent::GetAircraftPutOnTakeOffHelmetFPSClipSet(pVehicle);
						pClip = fwClipSetManager::GetClip(sClipSetId, clipId);
						if (pClip)
						{
							return pClip;
						}
					}
					
					// ! TODO: Strip this out if we don't need it; just polished the existing TPS anims for now.
					// Check to see if we have an "FP_" specific anim in the current clipset. If not, fall back and use the generic take off anim.
					//const fwMvClipId fpsClipId("FP_Put_on_helmet", 0x194796a1);
					 //pClip = fwClipSetManager::GetClip(clipsetId, clipId);
					//if (pClip)
					//{
					//	return pClip;
					//}
					
					// We don't have a specific anim (ie in helis/planes), try using the generic fallback
					// If we're in a plane/heli, use the new "generic" arm-only set of anims
					//taskAssertf(pClip, "CTaskMotionInAutomobile::GetClipForState: Couldn't find clip %s from clipset %s for state %s. Falling back to third person put_on_helmet clip!", clipId.GetCStr(), clipsetId.GetCStr(), GetStaticStateName(iState));
					clipId = ms_Tunables.m_PutOnHelmetClipId; 
				}
				else
				{
					clipId = ms_Tunables.m_PutOnHelmetClipId; 
				}

				bValidState = true;
				break;
			}
		case State_ChangeStation:
			{
				if (pMvClipId)
				{
					*pMvClipId = ms_ChangeStationClipId;
				}
				clipId = ms_Tunables.m_ChangeStationClipId; 
				bValidState = true;
				break;
			}
		case State_StillToSit:
			{
				if (pMvClipId)
				{
					*pMvClipId = ms_StillToSitClipId;
				}
				clipId = ms_Tunables.m_StillToSitClipId; 
				bValidState = true;
				break;
			}
		case State_SitToStill:
			{
				if (pMvClipId)
				{
					*pMvClipId = ms_SitToStillClipId;
				}
				clipId = ms_Tunables.m_SitToStillClipId; 
				bValidState = true;
				break;
			}
		default:
			{
				return NULL;
			}
	}

	if (bValidState)
	{
		const crClip* pClip = fwClipSetManager::GetClip(clipsetId, clipId);
#if __ASSERT
		if (bAssertIfClipDoesntExist)
		{
			taskAssertf(pClip, "Couldn't find clip %s from clipset %s for state %s", clipId.GetCStr(), clipsetId.GetCStr(), GetStaticStateName(iState));
		}
#endif // __ASSERT
		return pClip;
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInAutomobile::ProcessParameterSmoothingNoScale(float& fCurrentValue, float fDesiredValue, float fApproachSpeedMin, float fApproachSpeedMax, float fSmallDelta, float fTimeStep)
{
	const float fDelta = Abs(fDesiredValue - fCurrentValue);
	const bool bIsSmallDelta = fDelta < fSmallDelta;
	const float fApproachSpeed = bIsSmallDelta ? fApproachSpeedMin : fApproachSpeedMax;	// fApproachSpeed is recomputed here, do we need to store the values?
	rage::Approach(fCurrentValue, fDesiredValue, fApproachSpeed, fTimeStep);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionInAutomobile::ProcessParameterSmoothing(float& fCurrentValue, float fDesiredValue, float& fApproachSpeed, float fApproachSpeedMin, float fApproachSpeedMax, float fSmallDelta, float fTimeStep, float fTimeStepScale)
{
	const float fDelta = Abs(fDesiredValue - fCurrentValue);
	const bool bIsSmallDelta = fDelta < fSmallDelta;
	fApproachSpeed = bIsSmallDelta ? fApproachSpeedMin : fApproachSpeedMax;	// fApproachSpeed is recomputed here, do we need to store the values?
	fApproachSpeed *= fTimeStep * fTimeStepScale;
	rage::Approach(fCurrentValue, fDesiredValue, fApproachSpeed, fTimeStep);
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CTaskMotionInAutomobile::GetAgitatedClipSet() const
{
	//Ensure the vehicle is valid.
	if(!m_pVehicle)
	{
		return CLIP_SET_ID_INVALID;
	}

	//Ensure the seat anim info is valid.
	const CVehicleSeatAnimInfo* pSeatAnimInfo = m_pVehicle->GetSeatAnimationInfo(GetPed());
	if(!pSeatAnimInfo)
	{
		return CLIP_SET_ID_INVALID;
	}

	return pSeatAnimInfo->GetAgitatedClipSet();
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CTaskMotionInAutomobile::GetDuckedClipSet() const
{
	//Ensure the vehicle is valid.
	if(!m_pVehicle)
	{
		return CLIP_SET_ID_INVALID;
	}

	//Ensure the seat anim info is valid.
	const CPed* pPed = GetPed();
	const CVehicleSeatAnimInfo* pSeatAnimInfo = m_pVehicle->GetSeatAnimationInfo(pPed);
	if(!pSeatAnimInfo)
	{
		return CLIP_SET_ID_INVALID;
	}
	
	fwMvClipSetId duckedClipSet = pSeatAnimInfo->GetDuckedClipSet();

	//Force use the seat clipset instead of the ducked clipset when a first person passenger
	if (duckedClipSet != CLIP_SET_ID_INVALID && pPed->IsInFirstPersonVehicleCamera() && !m_pVehicle->IsDriver(pPed))
	{
		duckedClipSet = pSeatAnimInfo->GetSeatClipSetId();
	}

	return duckedClipSet;
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CTaskMotionInAutomobile::GetPanicClipSet() const
{
	//Ensure the vehicle is valid.
	if(!m_pVehicle)
	{
		return CLIP_SET_ID_INVALID;
	}
	
	//Ensure the seat anim info is valid.
	const CVehicleSeatAnimInfo* pSeatAnimInfo = m_pVehicle->GetSeatAnimationInfo(GetPed());
	if(!pSeatAnimInfo)
	{
		return CLIP_SET_ID_INVALID;
	}
	
	return pSeatAnimInfo->GetPanicClipSet();
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CTaskMotionInAutomobile::GetLowriderLeanClipSet() const
{
	//Ensure the vehicle is valid.
	if(!m_pVehicle)
	{
		return CLIP_SET_ID_INVALID;
	}

	//Ensure the seat anim info is valid.
	const CVehicleSeatAnimInfo* pSeatAnimInfo = m_pVehicle->GetSeatAnimationInfo(GetPed());
	if(!pSeatAnimInfo)
	{
		return CLIP_SET_ID_INVALID;
	}


	return pSeatAnimInfo->GetLowriderLeanClipSet();
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CTaskMotionInAutomobile::GetLowriderLeanAlternateClipSet() const
{
	//Ensure the vehicle is valid.
	if(!m_pVehicle || !GetPed())
	{
		return CLIP_SET_ID_INVALID;
	}

	//Ensure the seat anim info is valid.
	const CVehicleSeatAnimInfo* pSeatAnimInfo = m_pVehicle->GetSeatAnimationInfo(GetPed());
	if(!pSeatAnimInfo)
	{
		return CLIP_SET_ID_INVALID;
	}

	return pSeatAnimInfo->GetAltLowriderLeanClipSet();
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CTaskMotionInAutomobile::GetSeatClipSetId() const
{
	//Ensure the vehicle is valid.
	if(!m_pVehicle)
	{
		return CLIP_SET_ID_INVALID;
	}
	
	const CPed* pPed = GetPed();
	//Ensure the seat anim info is valid.
	const CVehicleSeatAnimInfo* pSeatAnimInfo = m_pVehicle->GetSeatAnimationInfo(pPed);
	if(!pSeatAnimInfo)
	{
		return CLIP_SET_ID_INVALID;
	}

	if (m_pSeatOverrideAnimInfo)
	{
		const fwMvClipSetId overrideClipsetId = m_pSeatOverrideAnimInfo->GetSeatClipSet();
		if (overrideClipsetId != CLIP_SET_ID_INVALID)
		{
			fwClipSet* pClipSet = fwClipSetManager::GetClipSet(overrideClipsetId);
			if (taskVerifyf(pClipSet, "Clipset (%s) doesn't exist", overrideClipsetId.GetCStr()))
			{
				if (CTaskVehicleFSM::RequestVehicleClipSetReturnIsLoaded(overrideClipsetId, SP_High))
				{
					return overrideClipsetId;
				}
			}
		}
	}

	if (ShouldUseFemaleClipSet(*pPed, *pSeatAnimInfo))
	{
		return pSeatAnimInfo->GetFemaleClipSet();
	}
	
	return pSeatAnimInfo->GetSeatClipSetId();
}

////////////////////////////////////////////////////////////////////////////////

CTaskMotionInAutomobile::DefaultClipSet CTaskMotionInAutomobile::GetDefaultClipSetToUse() const
{
	// Check if we should use lowrider lean anims.
	if (ShouldUseLowriderLean() || ShouldUseBikeAltClipset())
	{
		if (m_pVehicle->IsBikeOrQuad() && (m_nDefaultClipSet == DCS_LowriderLean || m_nDefaultClipSet == DCS_LowriderLeanAlt))
		{
			if (!m_MinTimeInAltLowriderClipsetTimer.IsFinished())
			{
				return m_nDefaultClipSet;
			}
		}

		if (CanUseLowriderLeanAltClipset() && ShouldUseLowriderLeanAltClipset())
		{
			return DCS_LowriderLeanAlt;
		}
		else
		{
			return DCS_LowriderLean;
		}
	}
	//Check if we should panic.
	else if(ShouldPanic())
	{
		return DCS_Panic;
	}
	//Check if we should be ducked.
	else if(ShouldBeDucked())
	{
		return DCS_Ducked;
	}
	//Check if we should be agitated.
	else if(ShouldBeAgitated())
	{
		return DCS_Agitated;
	}
	//Check if we should use the idle loop.
	else if(ShouldUseBaseIdle())
	{
		return DCS_Idle;
	}
	else
	{
		return DCS_Base;
	}
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CTaskMotionInAutomobile::GetDefaultClipSetId(DefaultClipSet nDefaultClipSet) const
{
	//Check the default clip set.
	switch(nDefaultClipSet)
	{
		case DCS_Base:
		case DCS_Idle:
		{
			return GetSeatClipSetId();
		}
		case DCS_Panic:
		{
			return GetPanicClipSet();
		}
		case DCS_Agitated:
		{
			return GetAgitatedClipSet();
		}
		case DCS_Ducked:
		{
			return GetDuckedClipSet();
		}
		case DCS_LowriderLean:
		{
			return GetLowriderLeanClipSet();
		}
		case DCS_LowriderLeanAlt:
		{
			return GetLowriderLeanAlternateClipSet();
		}
		default:
		{
			taskAssert(false);
			return CLIP_SET_ID_INVALID;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

CTask * CTaskMotionInAutomobile::CreatePlayerControlTask()
{
	taskFatalAssertf(GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_MOTION_IN_VEHICLE, "Invalid parent task");
	return static_cast<CTaskMotionInVehicle*>(GetParent())->CreatePlayerControlTask();
}

////////////////////////////////////////////////////////////////////////////////

const fwMvRequestId CTaskMotionOnBicycle::ms_StillRequestId("Still",0x48A35826);
const fwMvRequestId CTaskMotionOnBicycle::ms_StillToSitRequestId("StillToSit",0x9D365807);
const fwMvRequestId CTaskMotionOnBicycle::ms_SittingRequestId("Sitting",0x54D8244C);
const fwMvRequestId CTaskMotionOnBicycle::ms_StandingRequestId("Standing",0x1BBBAD68);
const fwMvRequestId CTaskMotionOnBicycle::ms_SitToStillRequestId("SitToStill",0xCFF11877);
const fwMvRequestId CTaskMotionOnBicycle::ms_ImpactRequestId("Impact",0x628CB55F);
const fwMvRequestId CTaskMotionOnBicycle::ms_IdleRequestId("Idle", 0x71c21326);

const fwMvBooleanId CTaskMotionOnBicycle::ms_StillOnEnterId("Still_OnEnter",0xDF36A2BC);
const fwMvBooleanId CTaskMotionOnBicycle::ms_StillToSitOnEnterId("StillToSit_OnEnter",0xA58F9D5E);
const fwMvBooleanId CTaskMotionOnBicycle::ms_SittingOnEnterId("Sitting_OnEnter",0x3050A11B);
const fwMvBooleanId CTaskMotionOnBicycle::ms_StandingOnEnterId("Standing_OnEnter",0x2488D32D);
const fwMvBooleanId CTaskMotionOnBicycle::ms_SitToStillOnEnterId("SitToStill_OnEnter",0x1917DA91);
const fwMvBooleanId CTaskMotionOnBicycle::ms_ImpactOnEnterId("Impact_OnEnter",0x80C6A64C);
const fwMvBooleanId CTaskMotionOnBicycle::ms_StillToSitLeanBlendInId("StillToSitLeanBlendIn",0x4a00b628);

const fwMvFloatId CTaskMotionOnBicycle::ms_BodyLeanXId("BodyLeanX",0x72683061);
const fwMvFloatId CTaskMotionOnBicycle::ms_BodyLeanYId("BodyLeanY",0x64B89502);
const fwMvFloatId CTaskMotionOnBicycle::ms_TuckBlendId("TuckBlend",0x5D86D1C2);
const fwMvFlagId CTaskMotionOnBicycle::ms_UseSmallImpactFlagId("UseSmallImpact",0x9A91F26E);
const fwMvFlagId CTaskMotionOnBicycle::ms_UseTuckFlagId("UseTuck",0x306F6489);
const fwMvFlagId CTaskMotionOnBicycle::ms_DoingWheelieFlagId("DoingWheelie",0xE346EEC9);
const fwMvFloatId CTaskMotionOnBicycle::ms_DownHillBlendId("DownHillBlend",0xD851468D);

const fwMvFloatId CTaskMotionOnBicycle::ms_StillPhaseId("StillPhase",0xB53502A1);
const fwMvFloatId CTaskMotionOnBicycle::ms_StillToSitPhase("StillToSitPhase",0x8C9FEE84);
const fwMvFloatId CTaskMotionOnBicycle::ms_SitToStillPhase("SitToStillPhase",0x79C4C04A);
const fwMvFloatId CTaskMotionOnBicycle::ms_ReversePhaseId("ReversePhase",0x51F9763B);
const fwMvFloatId CTaskMotionOnBicycle::ms_ImpactPhaseId("ImpactPhase",0x2E179F05);
const fwMvFloatId CTaskMotionOnBicycle::ms_InAirPhaseId("InAirPhase",0x9BD91DD1);

const fwMvFloatId CTaskMotionOnBicycle::ms_StillToSitRateId("StillToSitRate",0xD5F89671);
const fwMvFloatId CTaskMotionOnBicycle::ms_CruisePedalRateId("CruisePedalRate",0x94CE0B5);
const fwMvFloatId CTaskMotionOnBicycle::ms_FastPedalRateId("FastPedalRate",0xDBC7E2A4);

const fwMvFloatId CTaskMotionOnBicycle::ms_PedalToFreewheelBlendDuration("PedalToFreewheelBlendDuration",0x1074ACC);
const fwMvBooleanId CTaskMotionOnBicycle::ms_UseDefaultPedalRateId("UseDefaultPedalRate",0xBAE29A40);
const fwMvBooleanId CTaskMotionOnBicycle::ms_FreeWheelBreakoutId("freewheel_Breakout",0x3952F3F6);
const fwMvBooleanId CTaskMotionOnBicycle::ms_StillToSitFreeWheelBreakoutId("stillToSitfreewheel_Breakout", 0x7836EEF4);
const fwMvBooleanId CTaskMotionOnBicycle::ms_CanBlendOutImpactClipId("CanBlendOutImpactClip",0xB0D7DBEE);
const fwMvBooleanId CTaskMotionOnBicycle::ms_CanTransitionToJumpPrepId("CanTransitionToJumpPrep",0x2E9EFE8F);
const fwMvBooleanId CTaskMotionOnBicycle::ms_CanTransitionToJumpPrepRId("CanTransitionToJumpPrepR",0xA5BEA5F2);

const fwMvFloatId CTaskMotionOnBicycle::ms_SitTransitionBlendDuration("SitTransitionBlendDuration",0xBDDD8710);
const fwMvFloatId CTaskMotionOnBicycle::ms_SitToStandBlendDurationId("SitToStandBlendDuration",0xB6301D6D);


const fwMvFloatId CTaskMotionOnBicycle::ms_TimeSinceExitStandStateId("TimeSinceExitStandState",0xE27604A7);
const fwMvBooleanId CTaskMotionOnBicycle::ms_StillToSitStillInterruptId("StillToSitStillInterrupt",0x6B7318AC);

const fwMvBooleanId CTaskMotionOnBicycle::ms_StillToSitTransitionFinished("StillToSitTransitionFinished",0x7191F937);
const fwMvBooleanId CTaskMotionOnBicycle::ms_SitToStillTransitionFinished("SitToStillTransitionFinished",0x36B15C4F);

const fwMvClipId CTaskMotionOnBicycle::ms_ImpactClipId("ImpactClip",0x790D3C61);
const fwMvClipId CTaskMotionOnBicycle::ms_ImpactClip1Id("ImpactClip1",0x2DACBBC);
const fwMvClipId CTaskMotionOnBicycle::ms_StillToSitClipId("StillToSitClip",0x9CF842A);
const fwMvClipId CTaskMotionOnBicycle::ms_ToStillTransitionClipId("ToStillTransitionClip",0x7D72830);
const fwMvFlagId CTaskMotionOnBicycle::ms_LeanFlagId("Lean",0x3443D4C6);

const fwMvBooleanId CTaskMotionOnBicycle::ms_BikePutOnHelmetInterruptId("HelmetInterrupt",0x15220231);
const fwMvBooleanId CTaskMotionOnBicycle::ms_BikePutOnHelmetClipFinishedId("PutOnHelmetAnimFinished",0xDF68E618);
const fwMvRequestId CTaskMotionOnBicycle::ms_BikePutOnHelmetRequestId("PutOnHelmet",0x80B20F39);
const fwMvBooleanId CTaskMotionOnBicycle::ms_BikePutOnHelmetOnEnterId("PutOnHelmet_OnEnter",0xF2B6391B);
const fwMvBooleanId CTaskMotionOnBicycle::ms_BikePutOnHelmetCreateId("CreateHelmet",0x6A8FE35C);
const fwMvBooleanId CTaskMotionOnBicycle::ms_BikePutOnHelmetAttachId("AttachHelmet",0x5543b1f3);


bool CTaskMotionOnBicycle::ms_bUseStartStopTransitions = false;
float CTaskMotionOnBicycle::ms_fMinTimeInStandState = 0.8f;

////////////////////////////////////////////////////////////////////////////////

// Statics
CTaskMotionOnBicycle::Tunables CTaskMotionOnBicycle::ms_Tunables;

IMPLEMENT_VEHICLE_TASK_TUNABLES(CTaskMotionOnBicycle, 0x6a1a7bdf);

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::CheckForWantsToMove(const CVehicle& vehicle, const CPed& ped, bool bIgnoreVelocityCheck, bool bIsStill)
{
	if (vehicle.IsBaseFlagSet(fwEntity::IS_FIXED))
		return false;

	float fBrake = vehicle.GetBrake();
	float fThrottle = vehicle.GetThrottle();
	if (ped.IsLocalPlayer() && bIsStill)
	{
		const CControl* pControl = ped.GetControlFromPlayer();
		fBrake = pControl->GetVehicleBrake().GetNorm01();

		if (pControl->GetVehiclePushbikePedal().IsDown() && !pControl->GetVehiclePushbikeFrontBrake().IsDown())
		{
			fThrottle = 1.0f;
		}
	}

	if (!bIgnoreVelocityCheck)
	{
		Vector3 vVehForward = VEC3V_TO_VECTOR3(vehicle.GetTransform().GetB());
		if (vehicle.GetVelocityIncludingReferenceFrame().Dot(vVehForward) > 0.0f && vehicle.GetVelocityIncludingReferenceFrame().XYMag2() > rage::square(ms_Tunables.m_MinXYVelForWantsToMove))
		{
			return true;
		}
	}

	float fForwardness = fThrottle - fBrake;
	if (fForwardness > ms_Tunables.m_MaxThrottleForStill)
	{
		return true;
	}

	const bool bIgnoreInAirCheck = false;
	if (bIsStill && (WantsToJump(ped, vehicle, bIgnoreInAirCheck) || WantsToTrackStand(ped, vehicle)))
	{
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::CheckForSitting(const CPed* pPed, bool bIsPedaling, const CMoveNetworkHelper& pedMoveNetworkHelper)
{
	if (pPed->GetMyVehicle()->IsInAir())
	{
		return false;
	}

	if (ShouldStandDueToPitch(*pPed))
	{
		return false;
	}

	if (pPed->IsLocalPlayer())
	{
		float fSprintResult = pPed->GetPlayerInfo()->ControlButtonSprint(CPlayerInfo::SPRINT_ON_BICYCLE, true);

		// Script-specified disabling of sprinting gives a max movespeed of 1.0f (running)
		if(pPed->GetPlayerInfo()->GetPlayerDataPlayerSprintDisabled())
			fSprintResult = Min(fSprintResult, 1.0f);

		//Displayf("Sprint Result: %.2f", fSprintResult);

		if (fSprintResult > 1.0f)
		{
			return false;
		}

		if (!bIsPedaling || pedMoveNetworkHelper.GetBoolean(ms_FreeWheelBreakoutId))
		{
			return true;
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::CheckForStill(const CVehicle& veh, const CPed& ped)
{
	if (ped.GetPedResetFlag(CPED_RESET_FLAG_PreventGoingIntoStillInVehicleState))
	{
		return false;
	}

	const CBicycleInfo* pBicycleInfo = veh.GetLayoutInfo()->GetBicycleInfo();
	if (taskVerifyf(pBicycleInfo, "Couldn't find bicycle info for %s", veh.GetModelName()))
	{
		float fBrake = veh.GetBrake();

		if (ped.IsLocalPlayer())
		{
			const CControl* pControl = ped.GetControlFromPlayer();

			if (pBicycleInfo->GetCanTrackStand())
			{
				if (WantsToDoSpecialMove(ped, pBicycleInfo->GetIsFixieBike()))
				{
					return false;
				}
			}
			else if (WantsToJump(ped, veh, false))
			{
				return false;
			}

			if (veh.GetIsHandBrakePressed(pControl))
			{
				fBrake = veh.GetHandBrake();
			}
		}

		float fForwardness = veh.GetThrottle() - fBrake;
		if (fForwardness > ms_Tunables.m_MaxThrottleForStill)
		{
			return false;
		}

		const float fMaxSpeedForStillSqd = rage::square(pBicycleInfo->GetSpeedToTriggerStillTransition());
		if (veh.GetVelocityIncludingReferenceFrame().Mag2() >= fMaxSpeedForStillSqd)
		{
			return false;
		}
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::CheckForJumpPrepTransition(const CMoveNetworkHelper& rPedMoveNetworkHelper, const CVehicle& veh, const CPed& ped, bool bCachedJumpInput)
{
	TUNE_GROUP_BOOL(BICYCLE_TUNE, USE_CACHED_JUMP_INPUT, true);
	const bool bIgnoreInAirCheck = false;
	const bool bWantsToJump = bCachedJumpInput || WantsToJump(ped, veh, bIgnoreInAirCheck);
	if (bWantsToJump && 
		(rPedMoveNetworkHelper.GetBoolean(ms_CanTransitionToJumpPrepId) || rPedMoveNetworkHelper.GetBoolean(ms_CanTransitionToJumpPrepRId)))
	{
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::UseUpperBodyDriveby() const
{
	const CVehicleDriveByInfo* pVehicleDriveByInfo = CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(GetPed());
	if (pVehicleDriveByInfo && pVehicleDriveByInfo->GetUseSpineAdditive())
	{
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::ShouldLimitCameraDueToJump() const
{
	if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOTION_ON_BICYCLE_CONTROLLER)
	{
		if (static_cast<const CTaskMotionOnBicycleController*>(GetSubTask())->ShouldLimitCameraDueToJump())
		{
			return true;
		}
	}
	return m_bWasLaunchJump && GetState() == State_Impact;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::ShouldApplyForcedCamPitchOffset(float& fForcedPitchOffset) const
{
	if (!m_pBicycleInfo || !m_pBicycleInfo->GetCanTrackStand())
		return false;

	TUNE_GROUP_INT(TRIBIKE_TUNE, MAX_TIME_AFTER_REVERSE_TO_APPLY_OFFSET, 1500, 0, 3000, 10);
	if ((fwTimer::GetTimeInMilliseconds() - m_uTimeLastReversing) > MAX_TIME_AFTER_REVERSE_TO_APPLY_OFFSET)
		return false;

	TUNE_GROUP_FLOAT(TRIBIKE_TUNE, FORCED_PITCH_FOR_OFFSET, 0.5f, 0.0f, 1.0f, 0.01f);
	fForcedPitchOffset = FORCED_PITCH_FOR_OFFSET;
	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::CanTransitionToStill() const
{
	const bool bIgnoreInAirCheck = false;
	const CPed& rPed = *GetPed();
	const CVehicle& rVeh = *m_pVehicle;
	if (WantsToJump(rPed, rVeh, bIgnoreInAirCheck))
	{
		return false;
	}

	if (WantsToTrackStand(rPed, rVeh))
	{
		return false;
	}

	TUNE_GROUP_FLOAT(BICYCLE_TUNE, MIN_TIME_BETWEEN_SIT_TO_STILL_TRANSITS, 0.25f, 0.0f, 1.0f, 0.01f);
	if (GetPreviousState() == State_SitToStill && GetTimeInState() < MIN_TIME_BETWEEN_SIT_TO_STILL_TRANSITS)
	{
		return false;
	}

	if (m_pVehicle->GetLayoutInfo()->GetBicycleInfo()->GetCanTrackStand() && m_fTimeSinceWantingToTrackStand < ms_Tunables.m_TimeSinceNotWantingToTrackStandToAllowStillTransition)
	{
		return false;
	}

	if (!IsSubTaskInState(CTaskMotionOnBicycleController::State_PreparingToLaunch) &&
		!IsSubTaskInState(CTaskMotionOnBicycleController::State_PedalToPreparingToLaunch) &&
		!IsSubTaskInState(CTaskMotionOnBicycleController::State_Launching) &&
		!IsSubTaskInState(CTaskMotionOnBicycleController::State_InAir))
	{
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::IsSubTaskInState(s32 iState) const
{
	if (GetSubTask() && GetSubTask()->GetState() == iState)
	{
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::PedWantsToStand(const CPed& rPed, const CVehicle& rVeh, float& fSprintResult, bool bHaveShiftedWeight) const
{
	fSprintResult = GetSprintResultFromPed(rPed, rVeh);

	TUNE_GROUP_FLOAT(BICYCLE_TUNE, MIN_STILL_TO_SIT_SPRINT_TO_STAND, 1.75, 0.0f, 5.0f, 0.01f);
	const float fMinSprintResultToStandStillToSit = GetState() == State_StillToSit ? MIN_STILL_TO_SIT_SPRINT_TO_STAND : ms_Tunables.m_MinSprintResultToStand;
	if (fSprintResult > fMinSprintResultToStandStillToSit)
	{
		return true;
	}

	if (WantsToWheelie(rPed, bHaveShiftedWeight))
	{
		return true;
	}

	if (ShouldStandDueToPitch(rPed))
	{
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::WantsToWheelie(const CPed& rPed, bool bHaveShiftedWeight)
{
	if (bHaveShiftedWeight)
	{
		const CControl* pControl = rPed.GetControlFromPlayer();
		if (pControl)
		{
			float fStickDesiredYLeanIntention = pControl->GetVehicleSteeringUpDown().GetNorm();
			CVehicle *pVehicle = rPed.GetMyVehicle();

			if(pVehicle && pVehicle->InheritsFromBike())
			{
				float fStickX = 0.0f;
				static_cast<CBike*>(pVehicle)->GetBikeLeanInputs(fStickX, fStickDesiredYLeanIntention);
			}

			if (fStickDesiredYLeanIntention >= ms_Tunables.m_WheelieStickPullBackMinIntention)
			{
				return true;
			}
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::ShouldStandDueToPitch(const CPed& rPed)
{
	const float fPitch = rPed.GetTransform().GetPitch();
	if (fPitch > ms_Tunables.m_UpHillMinPitchToStandUp)
	{
		return true;
	}
	else if (-fPitch > ms_Tunables.m_DownHillMinPitchToStandUp)
	{
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::IsTaskValid(const CVehicle* pVehicle, s32 iSeatIndex)
{
	if (!pVehicle || !pVehicle->IsSeatIndexValid(iSeatIndex))
		return false;

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::StreamTaskAssets(const CVehicle* pVehicle, s32 iSeatIndex)
{
	if (!IsTaskValid(pVehicle, iSeatIndex))
		return false;

	return CTaskVehicleFSM::RequestClipSetFromSeat(pVehicle, iSeatIndex);
}

////////////////////////////////////////////////////////////////////////////////

CTaskMotionOnBicycle::CTaskMotionOnBicycle(CVehicle* pVehicle, CMoveNetworkHelper& moveNetworkHelper, const fwMvNetworkDefId& networkDefId) 
: m_pVehicle(pVehicle)
, m_pedMoveNetworkHelper(moveNetworkHelper)
, m_PedsNetworkDefId(networkDefId)
, m_pVehicleSeatAnimInfo(NULL)
, m_pBicycleInfo(NULL)
, m_fPreviousBodyLeanX(0.5f)
, m_fBodyLeanX(0.5f)
, m_fBodyLeanY(0.5f)
, m_fPitchAngle(0.5f)
, m_fPedalRate(1.0f)
, m_fDownHillBlend(0.0f)
, m_fTimeSinceShiftedWeightForward(0.0f)
, m_fTimeSinceWheelieStarted(0.0f)
, m_fTimeSinceBikeInAir(0.0f)
, m_fTimeInDeadZone(0.0f)
, m_fMaxJumpHeight(0.0f)
, m_fMinJumpHeight(0.0f)
, m_fLeanRate(ms_Tunables.m_LeanAngleSmoothingRate)
, m_fTimeStillAfterImpact(0.0f)
, m_fIdleTimer(0.0f)
, m_fTimeSinceWantingToTrackStand(0.0f)
, m_fTuckBlend(0.0f)
, m_bHaveShiftedWeightFwd(false)
, m_bWheelieTimerUp(false)
, m_bStartedBicycleNetwork(false)
, m_bPastFirstSprintPress(false)
, m_bWantedToDoSpecialMoveInStillTransition(false)
, m_bWantedToStandInStillTransition(false)
, m_bShouldStandBecauseOnSlope(false)
, m_bWasInAir(false)
, m_bUseStillTransitions(false)
, m_bBicyclePitched(false)
, m_bWasFreewheeling(false)
, m_bHaveBeenInSideZone(false)
, m_bWasInSlowZone(false)
, m_bShouldUseSlowLongitudinalRate(false)
, m_bSentAdditiveRequest(false)
, m_bUseSitTransitionJumpPrepBlendDuration(false)
, m_bCachedJumpInput(false)
, m_bIsTucking(false)
, m_bUseTrackStandTransitionToStill(false)
, m_bUseLeftFootTransition(false)
, m_bShouldSetCanWheelieFlag(false)
, m_bWasUsingFirstPersonClipSet(false)
, m_bNeedToRestart(false)
, m_bRestartedThisFrame(false)
, m_bMoVE_PedNetworkStarted(false)
, m_bMoVE_VehicleNetworkStarted(false)
, m_bMoVE_SeatClipHasBeenSet(false)
, m_bMoVE_AreMoveNetworkStatesValid(false)
, m_bMoVE_PutOnHelmetInterrupt(false)
, m_bMoVE_PutOnHelmetClipFinished(false)
, m_bMoVE_StillToSitTransitionFinished(false)
, m_bMoVE_SitToStillTransitionFinished(false)
, m_bCanBlendOutImpactClip(false)
, m_bShouldSkipPutOnHelmetAnim(false)
, m_bWasLaunchJump(false)
, m_bSwitchClipsThisFrame(false)
, m_uTimeBicycleLastSentRequest(0)
, m_uTimeLastReversing(0)
{
	SetInternalTaskType(CTaskTypes::TASK_MOTION_ON_BICYCLE);
}

////////////////////////////////////////////////////////////////////////////////

CTaskMotionOnBicycle::~CTaskMotionOnBicycle()
{

}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionOnBicycle::CleanUp()
{	
	m_bicycleMoveNetworkHelper.ReleaseNetworkPlayer();

	if (m_pVehicle)
	{
		m_pVehicle->m_nVehicleFlags.bForcePostCameraAnimUpdateUseZeroTimeStep = 0;
		m_pVehicle->GetMoveVehicle().SetSecondaryNetwork(NULL, 0.0f);
		m_pVehicle->m_nDEflags.bForcePrePhysicsAnimUpdate = false;
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::IsPedPedalling() const
{ 
	if (GetState() == State_StillToSit)
		return true;

	if (!GetSubTask())
		return false;

	return GetSubTask()->GetState() == CTaskMotionOnBicycleController::State_Pedaling; 
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::IsPedStandingAndPedalling() const
{ 
	if (GetState() != State_Standing)
		return false;

	if (!GetSubTask())
		return false;

	return GetSubTask()->GetState() == CTaskMotionOnBicycleController::State_Pedaling; 
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::IsPedFreewheeling() const
{ 
	if (GetState() == State_StillToSit)
		return false;

	if (!GetSubTask())
		return false;

	return GetSubTask()->GetState() == CTaskMotionOnBicycleController::State_Freewheeling; 
}


////////////////////////////////////////////////////////////////////////////////

float CTaskMotionOnBicycle::GetTimePedalling() const
{ 
	if (!GetSubTask())
		return 0.0f;

	return GetSubTask()->GetState() == CTaskMotionOnBicycleController::State_Pedaling ? GetSubTask()->GetTimeInState() : 0.0f; 
}

////////////////////////////////////////////////////////////////////////////////

float CTaskMotionOnBicycle::GetTimeFreewheeling() const
{ 
	if (!GetSubTask())
		return 0.0f;
	
	return GetSubTask()->GetState() == CTaskMotionOnBicycleController::State_Freewheeling ? GetSubTask()->GetTimeInState() : 0.0f; 
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycle::ProcessPreFSM()
{
    // clone peds may not have cloned their vehicle on this machine when
	// the task starts, in this case the task will wait in the start state
	// until this has occurred. The peds vehicle can be removed from this machine
	// before the ped too, and we need to quit this task (without asserting) in this case
    CPed& rPed = *GetPed();

	if (rPed.IsLocalPlayer())
	{
		m_bRestartedThisFrame = false;
		rPed.SetPedResetFlag(CPED_RESET_FLAG_ProcessPostCamera, true);
	}

	if (rPed.IsNetworkClone() && !m_pVehicle)
	{
		if (GetState()==State_Start)
		{
			return FSM_Continue;
		}
		else
		{
			return FSM_Quit;
		}
	}

	if (!taskVerifyf(m_pVehicle, "Vehicle was NULL in CTaskMotionOnBicycle::ProcessPreFSM()"))
		return FSM_Quit;

	if (m_pVehicle != rPed.GetMyVehicle())
	{
		aiDebugf1("Frame %i, CTaskMotionOnBicycle quitting because cached vehicle (%s) wasn't the vehicle (%s) the ped %s was in", fwTimer::GetFrameCount(), m_pVehicle->GetDebugName(), rPed.GetMyVehicle() ? rPed.GetMyVehicle()->GetDebugName() : "NULL", rPed.GetDebugName());
		return FSM_Quit;
	}

	if (!taskVerifyf(m_pVehicle->GetLayoutInfo(), "NULL layout info for %s bicycle", m_pVehicle->GetModelName()))
		return FSM_Quit;

	m_pBicycleInfo = m_pVehicle->GetLayoutInfo()->GetBicycleInfo();
	if (!taskVerifyf(m_pBicycleInfo, "NULL bicycle info for %s bicycle", m_pVehicle->GetModelName()))
		return FSM_Quit;

	m_pVehicleSeatAnimInfo = m_pVehicle->GetSeatAnimationInfo(&rPed);
	if (!taskVerifyf(m_pVehicleSeatAnimInfo, "NULL seat anim info for %s bicycle, seat %i, %s ped %s", m_pVehicle->GetModelName(), m_pVehicle->GetSeatManager()->GetPedsSeatIndex(&rPed), rPed.IsNetworkClone() ? "CLONE" : "LOCAL", rPed.GetDebugName()))
		return FSM_Quit;

#if __DEV
	if (m_pedMoveNetworkHelper.IsNetworkActive() && m_pedMoveNetworkHelper.GetBoolean(CTaskMotionInVehicle::ms_IdleOnEnterId))
	{
		aiDebugf1("Frame %i, ped entered idle state", fwTimer::GetFrameCount());
	}

	if (m_bicycleMoveNetworkHelper.IsNetworkActive() && m_bicycleMoveNetworkHelper.GetBoolean(CTaskMotionInVehicle::ms_IdleOnEnterId))
	{
		aiDisplayf("Frame %i, bicycle entered idle state", fwTimer::GetFrameCount());
	}
#endif //__DEV

	if (m_pVehicle->GetLayoutInfo()->GetBicycleInfo()->GetCanTrackStand())
	{
		if (GetState()==State_Standing)
			rPed.SetPedResetFlag(CPED_RESET_FLAG_IsStandingAndCycling, true);

		if (CTaskMotionOnBicycle::WantsToTrackStand(rPed, *m_pVehicle) || (m_bWantedToDoSpecialMoveInStillTransition && GetState() == State_StillToSit))
		{
			m_fTimeSinceWantingToTrackStand = 0.0f;
		}
		else
		{
			m_fTimeSinceWantingToTrackStand += GetTimeStep();

			float fFrontBrakeButton = 0.0f;
			if (rPed.IsLocalPlayer())
			{
				const CControl* pControl = rPed.GetControlFromPlayer();
				if (pControl)
				{
					// On fixie bike, both bumper buttons are front brakes
					if (m_pBicycleInfo->GetIsFixieBike())
					{
						fFrontBrakeButton = Max(pControl->GetVehicleBrake().GetNorm01(), pControl->GetVehiclePushbikeFrontBrake().GetNorm01());
					}
					else
					{
						fFrontBrakeButton = pControl->GetVehicleBrake().GetNorm01();
					}
				}
			}

			if (fFrontBrakeButton == 0.0f)
			{
				// Delay the transition to still if we have tried to pedal away
				TUNE_GROUP_FLOAT(TRACK_STAND_TUNE, MAX_TIME_TO_PREVENT_STILL_FROM_TRACK_STAND, 3.0f, 0.0f, 10.0f, 0.01f);
				if (m_fTimeSinceWantingToTrackStand < MAX_TIME_TO_PREVENT_STILL_FROM_TRACK_STAND && rPed.IsLocalPlayer())
				{
					const float fSprintResult = GetSprintResultFromPed(rPed, *m_pVehicle);
					if (fSprintResult >= 0.75f)
					{
						m_fTimeSinceWantingToTrackStand = 0.0f;
					}
				}
			}
		}
	}

	if (m_bShouldSetCanWheelieFlag)
	{
		rPed.SetPedResetFlag(CPED_RESET_FLAG_CanDoBicycleWheelie, true);
	}
	if (m_bIsTucking)
	{
		rPed.SetPedResetFlag(CPED_RESET_FLAG_IsTuckedOnBicycleThisFrame, true);
	}

	return FSM_Continue;
}	

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycle::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed& rPed = *GetPed();

	TUNE_GROUP_BOOL(BICYCLE_TUNE, CLEAR_NETWORK_ON_EXIT, true);
	TUNE_GROUP_FLOAT(BICYCLE_TUNE, MIN_TIME_IN_STILL_STATE_TO_CLEAR, 0.2f, 0.0f, 1.0f, 0.01f);
	if (CLEAR_NETWORK_ON_EXIT && m_pVehicle && m_pVehicle->GetMoveVehicle().IsAnimating() && rPed.GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle) 
		&& GetState() == State_Still && GetTimeInState() >= MIN_TIME_IN_STILL_STATE_TO_CLEAR)
	{
		// signal MoVE for exit
		m_pVehicle->GetMoveVehicle().ClearSecondaryNetwork(m_bicycleMoveNetworkHelper.GetNetworkPlayer(), INSTANT_BLEND_DURATION);
	}

	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate(rPed);	

		FSM_State(State_StreamAssets)
			FSM_OnEnter
				return StreamAssets_OnEnter();
			FSM_OnUpdate
				return StreamAssets_OnUpdate(rPed);

		FSM_State(State_Still)
			FSM_OnEnter
				return Still_OnEnter(rPed);
			FSM_OnUpdate
				return Still_OnUpdate(rPed, *m_pVehicle);
			FSM_OnExit
				return Still_OnExit();

		FSM_State(State_PutOnHelmet)
			FSM_OnEnter
				return PutOnHelmet_OnEnter();
			FSM_OnUpdate
				return PutOnHelmet_OnUpdate();
			FSM_OnExit
				return PutOnHelmet_OnExit();

		FSM_State(State_StillToSit)
			FSM_OnEnter
				return StillToSit_OnEnter();
			FSM_OnUpdate
				return StillToSit_OnUpdate(rPed);

		FSM_State(State_SitToStill)
			FSM_OnEnter
				return SitToStill_OnEnter();
			FSM_OnUpdate
				return SitToStill_OnUpdate(rPed);

		FSM_State(State_Sitting)
			FSM_OnEnter
				return Sitting_OnEnter();
			FSM_OnUpdate
				return Sitting_OnUpdate(rPed);

		FSM_State(State_Standing)
			FSM_OnEnter
				return Standing_OnEnter();
			FSM_OnUpdate
				return Standing_OnUpdate(rPed);

		FSM_State(State_Reversing)
			FSM_OnEnter
				return Reversing_OnEnter();
			FSM_OnUpdate
				return Reversing_OnUpdate(rPed);

		FSM_State(State_Impact)
			FSM_OnEnter
				return Impact_OnEnter();
			FSM_OnUpdate
				return Impact_OnUpdate();
			FSM_OnExit
				Impact_OnExit();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycle::ProcessPostFSM()
{
	CPed& rPed = *GetPed();
	if (CanBlockCameraViewModeTransitions())
	{
		rPed.SetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_CAMERA_VIEW_MODE_CYCLE);
	}
		
	if (!m_pedMoveNetworkHelper.IsNetworkActive())
		return FSM_Continue;

	// Process driveby additive blend in/out
	ProcessDriveBySpineAdditive(rPed);

	// Need to force another clip update for the bicycle so the phases we've just set 
	// are taken into account before rendering this frame
	if (GetState() > State_StreamAssets && m_bicycleMoveNetworkHelper.IsNetworkAttached())
	{
		m_pVehicle->m_nVehicleFlags.bForcePostCameraAnimUpdateUseZeroTimeStep = 1;
	}

	if (IsPedPedalling())
		rPed.SetPedResetFlag(CPED_RESET_FLAG_IsPedalling, true); // reset pre-tasks

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::ProcessMoveSignals()
{
	// Get a local reference to the ped
	CPed& rPed = *GetPed();
	m_bShouldSetCanWheelieFlag = false;

	// The vehicle may not be valid (especially for network clones) so just skip signal processing until its valid
	if (!m_pVehicle)
	{
		return true;
	}

	// If the ped move network has not been started
	if( !m_bMoVE_PedNetworkStarted )
	{
		if( StartPedMoveNetwork(m_pVehicleSeatAnimInfo) )
		{
#if __ASSERT
			aiDisplayf("Frame : %i, %s Ped %s (%p) started ped movenetwork", fwTimer::GetFrameCount(), rPed.IsNetworkClone() ? "CLONE" :"LOCAL", rPed.GetDebugName(), &rPed);
#endif // __ASSERT
			m_bMoVE_PedNetworkStarted = true;
		}
	}

	// Local bool for whether the vehicle clipset is loaded
	bool bClipSetLoaded = false;

	// If either vehicle network or seat clipset still needs setup
	if( !m_bMoVE_VehicleNetworkStarted || !m_bMoVE_SeatClipHasBeenSet )
	{
		// There shouldn't be any intermediate move transitions between the in vehicle state, then we can assume we 
		// insert correctly the first time
		s32 iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(&rPed);
		if( m_pVehicle->IsSeatIndexValid(iSeatIndex) )
		{
			const fwMvClipSetId inVehicleClipsetId = CTaskMotionInAutomobile::GetInVehicleClipsetId(rPed, m_pVehicle, iSeatIndex);

			// Request the vehicle clipset to be loaded and mark if it is loaded
			// NOTE: Need to make the request every frame until the seat clip is set
			bClipSetLoaded = !CTaskMotionInAutomobile::ms_Tunables.m_TestLowLodIdle && CTaskVehicleFSM::RequestVehicleClipSetReturnIsLoaded(inVehicleClipsetId, rPed.IsLocalPlayer() ? SP_High : SP_Invalid);

#if __DEV
			if (!bClipSetLoaded)
			{
				bool inVehicleInPriorityStreamer = CPrioritizedClipSetRequestManager::GetInstance().HasClipSetBeenRequested(CPrioritizedClipSetRequestManager::RC_Vehicle, inVehicleClipsetId);
				const CPrioritizedClipSetRequest* pInVehicleRequest = CPrioritizedClipSetStreamer::GetInstance().FindRequest(inVehicleClipsetId);
				eStreamingPriority nInVehiclePriority = pInVehicleRequest ? pInVehicleRequest->GetStreamingPriority() : SP_Invalid;
				aiDisplayf("Frame : %i, Ped %s (%p) waiting for clipset %s to be loaded, vehicle %s, seat %i", fwTimer::GetFrameCount(), rPed.GetDebugName(), &rPed, inVehicleClipsetId.GetCStr(), m_pVehicle->GetDebugName(), iSeatIndex);
				aiDisplayf("Frame : %i, StreamAssets - in vehicle clip set: %s (in priority streamer=%s, has req=%s, pri=%d)", fwTimer::GetFrameCount(), inVehicleClipsetId.GetCStr(), inVehicleInPriorityStreamer ? "true" : "false", pInVehicleRequest ? "true" : "false", nInVehiclePriority); 
			}
#endif // __DEV
		}
	}

	// If the vehicle move network has not been started
	if( !m_bMoVE_VehicleNetworkStarted )
	{
		// if the vehicle clipset is loaded
		if( bClipSetLoaded )
		{
			if( StartVehicleMoveNetwork() )
			{
#if __ASSERT
				aiDisplayf("Frame : %i, %s Ped %s (%p) started bicycle movenetwork", fwTimer::GetFrameCount(), rPed.IsNetworkClone() ? "CLONE" :"LOCAL",  rPed.GetDebugName(), &rPed);
#endif // __ASSERT
				m_bMoVE_VehicleNetworkStarted = true;
			}
		}
	}

	// Check if the vehicle move network is started
	// but the seat clip has not been set yet
	if( m_bMoVE_VehicleNetworkStarted && !m_bMoVE_SeatClipHasBeenSet )
	{
		if( SetSeatClipSet(m_pVehicleSeatAnimInfo) )
		{
			m_bMoVE_SeatClipHasBeenSet = true;
		}
	}

	// need ped MoVE network to be active to run code below
	if (!m_pedMoveNetworkHelper.IsNetworkActive())
	{
		// keep calling this method
		return true;
	}

	// Use fwTimer time step since we are called outside of task updates
	const float fTimeStep = fwTimer::GetTimeStep();

	// Compute ped move parameters for next frame
	const float fCurrentBodyLeanX = m_fBodyLeanX;
	ProcessLateralBodyLean(rPed, fTimeStep);
	ProcessLongitudinalBodyLean(rPed, fTimeStep);
	ProcessPedalRate(rPed, fTimeStep);
	ProcessAllowWheelie(rPed, fTimeStep);
	ProcessDownHillSlopeBlend(rPed, fTimeStep);
	ProcessTuckBlend(rPed, fTimeStep);

	// Set ped move parameters for the next frame
	m_pedMoveNetworkHelper.SetFloat(ms_BodyLeanXId, m_fBodyLeanX);

	// Forcing body lean Y to 0.5 to disable animated lean, so that it can be handled by the torso vehicle IK solver in CTaskMotionInVehicle::ProcessMoveSignals()
	const bool bReversing = GetState() == State_Reversing;
	const bool bDoingDriveby = rPed.GetPedResetFlag(CPED_RESET_FLAG_IsDoingDriveby);
	const bool bAllowLeanIK = m_pVehicleSeatAnimInfo && (m_pVehicleSeatAnimInfo->GetUseTorsoLeanIK() || (m_pVehicleSeatAnimInfo->GetUseRestrictedTorsoLeanIK() && (bDoingDriveby || bReversing)));
	m_pedMoveNetworkHelper.SetFloat(ms_BodyLeanYId, !bAllowLeanIK || bDoingDriveby ? m_fBodyLeanY : 0.5f);

	const bool bShouldReversePedalRate = GetSubTask() ? static_cast<CTaskMotionOnBicycleController*>(GetSubTask())->GetShouldReversePedalRate() : false;
	m_pedMoveNetworkHelper.SetFloat(CTaskMotionOnBicycleController::ms_PedalRateId, bShouldReversePedalRate ? -m_fPedalRate : m_fPedalRate);
	m_pedMoveNetworkHelper.SetFloat(ms_DownHillBlendId, m_fDownHillBlend);
	if (m_pBicycleInfo->GetCanTrackStand())
	{
		m_pedMoveNetworkHelper.SetFloat(ms_TuckBlendId, m_fTuckBlend);
	}

	if (m_bicycleMoveNetworkHelper.IsNetworkActive())
	{
		// Syncronize ped and vehicle anim phases
		// Cache the ped move parameters output from the last frame (to be rendered this frame)
		// Set bicycle move parameters (these parameters are used to update the pose this frame due to post cam vehicle anim update)
		SyncronizeFloatParameter(ms_StillPhaseId);
		SyncronizeFloatParameter(ms_StillToSitPhase);
		SyncronizeFloatParameter(ms_SitToStillPhase);
		SyncronizeFloatParameter(ms_ReversePhaseId);
		SyncronizeFloatParameter(ms_ImpactPhaseId);
		SyncronizeFloatParameter(ms_InAirPhaseId);
		m_bicycleMoveNetworkHelper.SetFloat(ms_BodyLeanXId, fCurrentBodyLeanX);
	}

	if (m_pedMoveNetworkHelper.GetBoolean(ms_StillToSitLeanBlendInId))
	{
		m_pedMoveNetworkHelper.SetFlag(true, ms_LeanFlagId);
	}

	switch(GetState())
	{
	case State_Start:
		// nil
		break;
	case State_StreamAssets:
		StreamAssets_OnProcessMoveSignals();
		break;
	case State_Still:
		Still_OnProcessMoveSignals();
		break;
	case State_PutOnHelmet:
		PutOnHelmet_OnProcessMoveSignals();
		break;
	case State_StillToSit:
		StillToSit_OnProcessMoveSignals();
		break;
	case State_SitToStill:
		SitToStill_OnProcessMoveSignals();
		break;
	case State_Sitting:
		Sitting_OnProcessMoveSignals();
		break;
	case State_Standing:
		Standing_OnProcessMoveSignals();
		break;
	case State_Reversing:
		Reversing_OnProcessMoveSignals();
		break;
	case State_Impact:
		Impact_OnProcessMoveSignals();
		break;
	case State_Finish:
		// nil
		break;
	default:
		taskAssertf(false,"Unhandled state(%s) may need OnProcessMoveSignals!", GetStateName(GetState()));
		break;
	}

	// we always have MoVE signaling
	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::ProcessPostCamera()
{
	CheckForRestartingStateDueToCameraSwitch();
	return false;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycle::Start_OnUpdate(CPed& rPed)
{
	m_fIdleTimer = 0.0f;	// Clear idle timer when we start

	// B*1930551: Reset sprint counter, ensures we don't immediately start pedalling once mounting the bicycle.
	if (rPed.IsLocalPlayer() && rPed.GetPlayerInfo())
	{
		rPed.GetPlayerInfo()->SetPlayerDataSprintControlCounter(0.0f);
	}

    // if this is a network ped it may not have been put in the seat yet, and we must wait
    if (rPed.IsNetworkClone() && (!m_pVehicle || !m_pVehicle->ContainsPed(&rPed)))
    {
        return FSM_Continue;
    }

	// Request MoVE signal update call every frame from now on.
	RequestProcessMoveSignalCalls();

	if (!m_pVehicle->GetAnimDirector())
	{
		m_pVehicle->InitAnimLazy();
		taskAssertf(m_pVehicle->GetAnimDirector(), "Failed to create clip director");
	}

	m_pVehicle->m_nDEflags.bForcePrePhysicsAnimUpdate = true;

	m_bUseStillTransitions = m_pVehicle->GetLayoutInfo()->GetUseStillToSitTransition();

	ProcessMoveSignals();

	// If the ped MoVE network has been started
	if (m_bMoVE_PedNetworkStarted && m_bMoVE_VehicleNetworkStarted)
	{
		if (SetSeatClipSet(m_pVehicleSeatAnimInfo))
		{
			if (rPed.GetPedResetFlag(CPED_RESET_FLAG_PreventGoingIntoStillInVehicleState))
			{
				SetTaskState(State_Sitting);
				return FSM_Continue;
			}
			else
			{
				SetTaskState(State_Still);
				return FSM_Continue;
			}
		}
	}

	SetTaskState(State_StreamAssets);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycle::StreamAssets_OnEnter()
{
	taskAssert(m_pedMoveNetworkHelper.IsNetworkActive());
	const crClip* pLowLodClip = fwClipSetManager::GetClip(CTaskMotionInAutomobile::ms_Tunables.m_LowLodIdleClipSetId, m_pVehicleSeatAnimInfo->GetLowLODIdleAnim());
	// MoVE signal OK here in OnEnter
	m_pedMoveNetworkHelper.SetClip(pLowLodClip, CTaskMotionInVehicle::ms_LowLodClipId);
	SetMoveNetworkStates(CTaskMotionInVehicle::ms_LowLodRequestId, CTaskMotionInVehicle::ms_LowLodOnEnterId, true);
	m_bMoVE_AreMoveNetworkStatesValid = false;
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycle::StreamAssets_OnUpdate(CPed& rPed)
{
#if __BANK
	s32 iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(&rPed);
	if (m_pVehicle->IsSeatIndexValid(iSeatIndex))
	{
		const fwMvClipSetId inVehicleClipsetId = CTaskMotionInAutomobile::GetInVehicleClipsetId(rPed, m_pVehicle, iSeatIndex);
		CTaskVehicleFSM::SpewStreamingDebugToTTY(rPed, *m_pVehicle, iSeatIndex, inVehicleClipsetId, GetTimeInState());
	}
#endif // __BANK

	if (!m_bMoVE_AreMoveNetworkStatesValid)
		return FSM_Continue;

	if (m_bMoVE_SeatClipHasBeenSet)
	{
		if (rPed.GetPedResetFlag(CPED_RESET_FLAG_PreventGoingIntoStillInVehicleState))
		{
			SetTaskState(State_Sitting);
			return FSM_Continue;
		}
		else
		{
			SetTaskState(State_Still);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionOnBicycle::StreamAssets_OnProcessMoveSignals()
{
	m_bMoVE_AreMoveNetworkStatesValid = m_pedMoveNetworkHelper.IsInTargetState();
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycle::Still_OnEnter(CPed& rPed)
{
	m_bSentAdditiveRequest = false;
	rPed.GetIkManager().SetFlag(PEDIK_LEGS_AND_PELVIS_FADE_ON);
	if (!m_bShouldSkipPutOnHelmetAnim)
	{
		SetMoveNetworkStates(ms_StillRequestId, ms_StillOnEnterId);
	}
	m_bShouldSkipPutOnHelmetAnim = false;
	if (m_bRestartedThisFrame)
	{
		m_pedMoveNetworkHelper.SendRequest(ms_IdleRequestId);
		aiDebugf1("Frame : %i, Ped Sending %s Request", fwTimer::GetFrameCount(), ms_IdleRequestId.TryGetCStr());
		m_bicycleMoveNetworkHelper.SendRequest(ms_IdleRequestId);
		aiDebugf1("Frame : %i, Bicycle Sending %s Request", fwTimer::GetFrameCount(), ms_IdleRequestId.TryGetCStr());
	}
	m_bMoVE_AreMoveNetworkStatesValid = false;
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycle::Still_OnUpdate(CPed& rPed, CVehicle& rVeh)
{
	rPed.SetPedResetFlag(CPED_RESET_FLAG_RequiresLegIk, true); // reset pre-task
	rPed.SetPedResetFlag(CPED_RESET_FLAG_IsStillOnBicycle, true);  // reset pre-task

	if (!m_bMoVE_AreMoveNetworkStatesValid)
	{
		if (rPed.IsLocalPlayer())
		{
			m_bMoVE_AreMoveNetworkStatesValid = AreMoveNetworkStatesValid(ms_StillRequestId);
			aiDebugf1("Frame : %i, Bicycle Sending %s Request", fwTimer::GetFrameCount(), ms_StillRequestId.TryGetCStr());
			m_bicycleMoveNetworkHelper.SendRequest(ms_StillRequestId);
		}

		if (!m_bMoVE_AreMoveNetworkStatesValid)
		{
			return FSM_Continue;
		}
	}

	if (CheckForRestartingStateDueToCameraSwitch())
	{
		return FSM_Continue;
	}

	if (CheckForHelmet(rPed))
	{
		//Make sure we have the helmet loaded before starting putting it on
		bool bFinishedPreLoading = rPed.GetHelmetComponent()->HasPreLoadedProp();
		if(	!rPed.GetHelmetComponent()->IsPreLoadingProp() && !bFinishedPreLoading)
		{
			//Load the helmet
			rPed.GetHelmetComponent()->SetPropFlag(PV_FLAG_RANDOM_HELMET);
			rPed.GetHelmetComponent()->RequestPreLoadProp();
			return FSM_Continue;
		}

		if(bFinishedPreLoading)
		{	
			m_bShouldSkipPutOnHelmetAnim = camInterface::ComputeShouldMakePedHeadInvisible(rPed);
			SetTaskState(State_PutOnHelmet);
		}
		return FSM_Continue;
	}

	// Check if the ped wants to reverse
	if (m_pBicycleInfo->GetCanTrackStand())
	{
		if (CheckForReversingTransition())
		{
			SetTaskState(State_Reversing);
			return FSM_Continue;	
		}
	}
	else if (CTaskMotionInVehicle::CheckForReverse(rVeh, rPed, false, true))
	{
		SetTaskState(State_Reversing);
		return FSM_Continue;
	}

	// Check if the ped wants to move
	if (CheckForWantsToMove(rVeh, rPed, false, true) && !m_pVehicle->m_nVehicleFlags.bScriptSetHandbrake)
	{
		if (!rPed.GetPedResetFlag(CPED_RESET_FLAG_IsDoingDriveby) || UseUpperBodyDriveby())
		{
			SetTaskState(m_bUseStillTransitions ? State_StillToSit : State_Sitting);
			return FSM_Continue;
		}
	}

	if(m_fIdleTimer <= IDLE_TIMER_HELMET_PUT_ON)
		m_fIdleTimer += GetTimeStep();

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycle::Still_OnExit()
{
	m_fIdleTimer = 0.0f;
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionOnBicycle::Still_OnProcessMoveSignals()
{
	m_bMoVE_AreMoveNetworkStatesValid = AreMoveNetworkStatesValid(ms_StillRequestId);
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycle::PutOnHelmet_OnEnter()
{
	CPed* pPed = GetPed();

	pPed->SetPedResetFlag(CPED_RESET_FLAG_PuttingOnHelmet, true);

	if (!m_bShouldSkipPutOnHelmetAnim)
	{
		SetMoveNetworkStates(ms_BikePutOnHelmetRequestId, ms_BikePutOnHelmetOnEnterId);
		m_bMoVE_AreMoveNetworkStatesValid = false;
	}

	m_bMoVE_PutOnHelmetClipFinished = false;
	m_bMoVE_PutOnHelmetInterrupt = false;
	//NOTE: we set m_bMoVE_AreMoveNetworkStatesValid to false in OnProcessMoveSignals
	
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycle::PutOnHelmet_OnUpdate()
{
	CPed* pPed = GetPed();

	if (!m_bMoVE_AreMoveNetworkStatesValid)
		return FSM_Continue;

	if(m_pVehicle && CTaskMotionInAutomobile::WillAbortPuttingOnHelmet(*m_pVehicle, *pPed) && !pPed->GetHelmetComponent()->IsHelmetInHand() &&
		(m_bMoVE_PutOnHelmetInterrupt || pPed->GetHelmetComponent()->IsPreLoadingProp()))
	{
		SetTaskState(State_Still);
		return FSM_Continue;
	}

	//It's possible that the ped is exiting the vehicle as the helmet won't have been 
	//created yet. This detects if exiting and stops the ped creating a helmet.
	CTaskExitVehicle *task = (CTaskExitVehicle*)pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE);
	if(task)
	{
		return FSM_Continue;
	}

	if (pPed->IsLocalPlayer())
	{
		//Disable switching camera whilst putting on the helmet. 
		CControlMgr::GetMainPlayerControl().DisableInput(INPUT_NEXT_CAMERA);
	}

	//Handle first person camera
	if(m_bShouldSkipPutOnHelmetAnim)
	{
		if(pPed->GetHelmetComponent()->IsHelmetInHand())
		{
			pPed->GetHelmetComponent()->DisableHelmet(false);
		}
		pPed->GetHelmetComponent()->SetPropFlag(PV_FLAG_RANDOM_HELMET);
		pPed->GetHelmetComponent()->EnableHelmet();
		pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true);
		SetTaskState(State_Still);
		return FSM_Continue;
	}

	pPed->SetPedResetFlag(CPED_RESET_FLAG_RequiresLegIk, true); // reset pre-task

	if ( m_bMoVE_PutOnHelmetClipFinished )
	{
		SetTaskState(State_Still);
		return FSM_Continue;
	}
	
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycle::PutOnHelmet_OnExit()
{
	CPed* pPed = GetPed();

	//Make sure we have cleared the request
	pPed->GetHelmetComponent()->ClearPreLoadedProp();

	if(pPed->GetHelmetComponent()->IsHelmetInHand())
	{
		pPed->GetHelmetComponent()->DisableHelmet(true);
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionOnBicycle::PutOnHelmet_OnProcessMoveSignals()
{
	CPed* pPed = GetPed();
	
	m_bMoVE_AreMoveNetworkStatesValid = AreMoveNetworkStatesValid(ms_BikePutOnHelmetRequestId);

	if( !m_bMoVE_AreMoveNetworkStatesValid )
	{
		// don't do further processing in this method until the MoVE network is in the desired states
		return;
	}

	//Block head Ik as it breaks the animation and head alignment
	pPed->SetHeadIkBlocked(); // reset every frame

	CTaskVehicleFSM::ProcessApplyForce(m_pedMoveNetworkHelper, *m_pVehicle, *pPed);

	//Needs calling every frame.
	//Stop the ai from driving off whilst putting on the helmet
	if(m_pVehicle && !pPed->IsPlayer() && pPed->GetIsDrivingVehicle())
	{
		m_pVehicle->SetForwardSpeed(0.0f);
	}

	pPed->SetPedResetFlag(CPED_RESET_FLAG_PuttingOnHelmet, true); // reset every frame

	if (m_pedMoveNetworkHelper.GetBoolean(ms_BikePutOnHelmetCreateId))
	{	
		pPed->GetHelmetComponent()->EnableHelmet(ANCHOR_PH_L_HAND);
		pPed->GetHelmetComponent()->ClearPreLoadedProp();
	}

	if (m_pedMoveNetworkHelper.GetBoolean(ms_BikePutOnHelmetAttachId))
	{	
		pPed->GetHelmetComponent()->EnableHelmet();
		pPed->GetHelmetComponent()->ClearPreLoadedProp();
	}

	if(m_pedMoveNetworkHelper.GetBoolean(ms_BikePutOnHelmetInterruptId))
	{
		m_bMoVE_PutOnHelmetInterrupt = true;
	}

	if (m_pedMoveNetworkHelper.GetBoolean(ms_BikePutOnHelmetClipFinishedId))	// And check so we are actually playing a clip
	{
		pPed->GetHelmetComponent()->EnableHelmet();
		pPed->GetHelmetComponent()->ClearPreLoadedProp();
		m_bMoVE_PutOnHelmetClipFinished = true;
	}
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycle::StillToSit_OnEnter()
{
	m_bSentAdditiveRequest = false;
	m_bWantedToStandInStillTransition = false;
	m_bWantedToDoSpecialMoveInStillTransition = false;
	m_fLeanRate = ms_Tunables.m_StillToSitLeanRate; 
	m_bUseSitTransitionJumpPrepBlendDuration = false;
	// MoVE signal OK OnEnter
	m_pedMoveNetworkHelper.SetFlag(false, ms_LeanFlagId);
	SetMoveNetworkStates(ms_StillToSitRequestId, ms_StillToSitOnEnterId);
	m_bMoVE_AreMoveNetworkStatesValid = false;
	m_bMoVE_StillToSitTransitionFinished = false;
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycle::StillToSit_OnUpdate(CPed& rPed)
{
	rPed.SetPedResetFlag(CPED_RESET_FLAG_RequiresLegIk, true); // reset pre-task

	if (!m_bWantedToStandInStillTransition)
	{
		m_bWantedToStandInStillTransition = PedWantsToStand(rPed, *m_pVehicle, m_bHaveShiftedWeightFwd);
	}

	const bool bIgnoreInAirCheck = true;
	if (!m_bWantedToDoSpecialMoveInStillTransition)
	{
		m_bWantedToDoSpecialMoveInStillTransition = WantsToJump(rPed, *m_pVehicle, bIgnoreInAirCheck) || WantsToTrackStand(rPed, *m_pVehicle);
	}

	if (!m_bMoVE_AreMoveNetworkStatesValid)
		return FSM_Continue;

	// Check for a transition to the still state
	if (CTaskMotionInVehicle::CheckForReverse(*m_pVehicle, rPed, false))
	{
		SetTaskState(State_Still);
		return FSM_Continue;
	}

	bool bCanTransitionToStill = true;

	if (rPed.IsPlayer())
	{
		// If we already be moving, allow instant movement
		if (m_pVehicle->GetVelocityIncludingReferenceFrame().Mag2() <= rage::square(ms_Tunables.m_MaxSpeedForStill))
		{
            if(rPed.IsLocalPlayer())
            {
			    const CControl* pControl = rPed.GetControlFromPlayer();
			    if (pControl->GetVehicleBrake().GetNorm01() == 0.0f)
			    {
				    bCanTransitionToStill = GetTimeInState() >= m_pBicycleInfo->GetMinForcedStillToSitTime();
			    }
			    if (pControl->GetVehiclePushbikePedal().IsDown())
			    {
				    bCanTransitionToStill = false;
			    }
            }
            else
            {
                CNetObjPlayer *netObjPlayer = SafeCast(CNetObjPlayer, rPed.GetNetworkObject());
                
                if(netObjPlayer && netObjPlayer->IsVehicleJumpDown())
                {
                    bCanTransitionToStill = GetTimeInState() >= m_pBicycleInfo->GetMinForcedStillToSitTime();
                }
            }
		}
	}

	if (bCanTransitionToStill && CheckForStill(*m_pVehicle, rPed))
	{
		// MoVE signal check here OK: StillToSitStillInterrupt
		if (m_pedMoveNetworkHelper.GetBoolean(ms_StillToSitStillInterruptId))
		{
			SetTaskState(State_SitToStill);
			return FSM_Continue;
		}
		else
		{
			SetTaskState(State_Still);
			return FSM_Continue;
		}
	}

	if (rPed.GetPedResetFlag(CPED_RESET_FLAG_IsDoingDriveby) && !UseUpperBodyDriveby())
	{
		SetTaskState(State_Sitting);
		return FSM_Continue;
	}

	// MoVE signal checks directly here are OK: CanJump, CanTrackStand
	if (m_bWantedToDoSpecialMoveInStillTransition && (CanJump(m_pedMoveNetworkHelper) || CanTrackStand(m_pedMoveNetworkHelper)))
	{
		m_bUseSitTransitionJumpPrepBlendDuration = true;
		SetTaskState(State_Sitting);
		return FSM_Continue;
	}

	// MoVE signal check here OK: FreeWheelBreakout
	if ( m_bMoVE_StillToSitTransitionFinished || m_pedMoveNetworkHelper.GetBoolean(ms_StillToSitFreeWheelBreakoutId) )
	{
		if (IsTransitioningToState(State_Standing))
			return FSM_Continue;

		SetTaskState(State_Sitting);
		return FSM_Continue;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionOnBicycle::StillToSit_OnProcessMoveSignals()
{
	m_bMoVE_AreMoveNetworkStatesValid = AreMoveNetworkStatesValid(ms_StillToSitRequestId);

	if( !m_bMoVE_StillToSitTransitionFinished && m_pedMoveNetworkHelper.GetBoolean(ms_StillToSitTransitionFinished) )
	{
		m_bMoVE_StillToSitTransitionFinished = true;
	}
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycle::SitToStill_OnEnter()
{
	m_bSentAdditiveRequest = false;
	m_bWantedToStandInStillTransition = false;
	SetMoveNetworkStates(ms_SitToStillRequestId, ms_SitToStillOnEnterId);
	m_bMoVE_AreMoveNetworkStatesValid = false;
	m_bMoVE_SitToStillTransitionFinished = false;
	SetClipsForState(false);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycle::SitToStill_OnUpdate(CPed& rPed)
{
	rPed.SetPedResetFlag(CPED_RESET_FLAG_RequiresLegIk, true); // reset pre-task

	const bool bIgnoreInAirCheck = true;
	if (!m_bWantedToDoSpecialMoveInStillTransition)
	{
		m_bWantedToDoSpecialMoveInStillTransition = WantsToJump(rPed, *m_pVehicle, bIgnoreInAirCheck) || WantsToTrackStand(rPed, *m_pVehicle);
	}

	if (!m_bMoVE_AreMoveNetworkStatesValid)
	{
		return FSM_Continue;
	}

	if (IsTransitioningToState(State_Reversing))
		return FSM_Continue;

	// Check if the ped wants to move
	if (CheckForWantsToMove(*m_pVehicle, rPed, true))
	{
		SetTaskState(State_Sitting);
		return FSM_Continue;
	}

	// MoVE signal checks directly here are OK: CanJump, CanTrackStand
	if (m_bWantedToDoSpecialMoveInStillTransition && (CanJump(m_pedMoveNetworkHelper) || CanTrackStand(m_pedMoveNetworkHelper)))
	{
		m_bUseSitTransitionJumpPrepBlendDuration = true;
		SetTaskState(State_Sitting);
		return FSM_Continue;
	}

	if (m_bMoVE_SitToStillTransitionFinished)
	{
		SetTaskState(State_Still);
		return FSM_Continue;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionOnBicycle::SitToStill_OnProcessMoveSignals()
{
	m_bMoVE_AreMoveNetworkStatesValid = AreMoveNetworkStatesValid(ms_SitToStillRequestId);
	
	if( !m_bMoVE_AreMoveNetworkStatesValid )
	{
		SetClipsForState(true);
	}

	if( !m_bMoVE_SitToStillTransitionFinished && m_pedMoveNetworkHelper.GetBoolean(ms_SitToStillTransitionFinished) )
	{
		m_bMoVE_SitToStillTransitionFinished = true;
	}
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycle::Sitting_OnEnter()
{
	m_bSentAdditiveRequest = false;
	m_bUseTrackStandTransitionToStill = false;
	m_bUseLeftFootTransition = false;

	const CPed& rPed = *GetPed();
	if (rPed.GetPedResetFlag(CPED_RESET_FLAG_IsDoingDriveby) && !UseUpperBodyDriveby())
	{
		m_bWasFreewheeling = true;
	}

	SetNewTask(rage_new CTaskMotionOnBicycleController(m_pedMoveNetworkHelper, m_bicycleMoveNetworkHelper, m_pVehicle, m_bWasFreewheeling));
	SetMoveNetworkStates(ms_SittingRequestId, ms_SittingOnEnterId);
	if (m_bRestartedThisFrame)
	{
		m_pedMoveNetworkHelper.SendRequest(ms_IdleRequestId);
		m_bicycleMoveNetworkHelper.SendRequest(ms_IdleRequestId);
	}
	m_bMoVE_AreMoveNetworkStatesValid = false;
	const bool bSetOnBicycleNetwork = false;
	SetBlendDurationForState(bSetOnBicycleNetwork);
	m_bWasFreewheeling = false;
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycle::Sitting_OnUpdate(CPed& rPed)
{
	rPed.SetPedResetFlag(CPED_RESET_FLAG_IsSittingAndCycling, true); // reset pre-task

	if (!m_bMoVE_AreMoveNetworkStatesValid)
	{
		return FSM_Continue;
	}

	if (CheckForRestartingStateDueToCameraSwitch())
	{
		return FSM_Continue;
	}

	if (IsTransitioningToState(State_Impact))
	{
		m_bWasLaunchJump = GetSubTask() ? (GetSubTask()->GetState() == CTaskMotionOnBicycleController::State_Launching || GetSubTask()->GetPreviousState() == CTaskMotionOnBicycleController::State_Launching) : false;
		return FSM_Continue;
	}

	if (IsTransitioningToState(State_Reversing))
	{
		if (ms_Tunables.m_PreventDirectTransitionToReverseFromSit && m_pBicycleInfo->GetCanTrackStand())
		{
			SetTaskState(m_bUseStillTransitions ? State_SitToStill : State_Still);
		}
		return FSM_Continue;
	}

	const bool bCanTransitionToStill = CanTransitionToStill();
	if (!bCanTransitionToStill)
	{
		rPed.SetPedResetFlag(CPED_RESET_FLAG_PreventBicycleFromLeaningOver, true); // reset pre-task
	}

	if (bCanTransitionToStill)
	{
		// Check for a transition to the still state
		if (CheckForStill(*m_pVehicle, rPed) || CTaskMotionInVehicle::CheckForReverse(*m_pVehicle, *GetPed()))
		{
			if (rPed.GetPedResetFlag(CPED_RESET_FLAG_IsDoingDriveby) && !UseUpperBodyDriveby())
			{
				SetTaskState(State_Still);
				return FSM_Continue;
			}
			else
			{
				if (IsSubTaskInState(CTaskMotionOnBicycleController::State_TrackStand))
				{
					m_bUseTrackStandTransitionToStill = true;
					m_bUseLeftFootTransition = static_cast<CTaskMotionOnBicycleController*>(GetSubTask())->GetUseLeftFoot();
				}
				SetTaskState(m_bUseStillTransitions ? State_SitToStill : State_Still);
				return FSM_Continue;
			}
		}
	}

	if (IsTransitioningToState(State_Standing))
	{
		m_bWasFreewheeling = IsPedFreewheeling();
		return FSM_Continue;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionOnBicycle::Sitting_OnProcessMoveSignals()
{
	m_bMoVE_AreMoveNetworkStatesValid = AreMoveNetworkStatesValid(ms_SittingRequestId);
	
	if( !m_bMoVE_AreMoveNetworkStatesValid )
	{
		const bool bSetOnBicycleNetwork = true;
		SetBlendDurationForState(bSetOnBicycleNetwork);
	}
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycle::Standing_OnEnter()
{
	m_bSentAdditiveRequest = false;
	m_bPastFirstSprintPress = false;
	m_bWantedToStandInStillTransition = false;

	SetNewTask(rage_new CTaskMotionOnBicycleController(m_pedMoveNetworkHelper, m_bicycleMoveNetworkHelper, m_pVehicle, m_bWasFreewheeling));
	SetMoveNetworkStates(ms_StandingRequestId, ms_StandingOnEnterId);
	if (m_bRestartedThisFrame)
	{
		m_pedMoveNetworkHelper.SendRequest(ms_IdleRequestId);
		m_bicycleMoveNetworkHelper.SendRequest(ms_IdleRequestId);
	}
	m_bMoVE_AreMoveNetworkStatesValid = false;
	SetBlendDurationForState(false);
	m_bWasFreewheeling = false;
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycle::Standing_OnUpdate(CPed& rPed)
{
	// Cache is we should be standing as we're on a slope
	m_bShouldStandBecauseOnSlope = ShouldStandDueToPitch(rPed);

	if (!m_bMoVE_AreMoveNetworkStatesValid)
	{
		return FSM_Continue;
	}

	if (CheckForRestartingStateDueToCameraSwitch())
	{
		return FSM_Continue;
	}

	rPed.SetPedResetFlag(CPED_RESET_FLAG_IsStandingAndCycling, true); // reset pre-task

	if (IsTransitioningToState(State_Impact))
	{
		m_bWasLaunchJump = GetSubTask() ? (GetSubTask()->GetState() == CTaskMotionOnBicycleController::State_Launching || GetSubTask()->GetPreviousState() == CTaskMotionOnBicycleController::State_Launching) : false;
		return FSM_Continue;
	}

	if (IsTransitioningToState(State_Reversing))
	{
		if (ms_Tunables.m_PreventDirectTransitionToReverseFromSit && m_pBicycleInfo->GetCanTrackStand())
		{
			SetTaskState(m_bUseStillTransitions ? State_SitToStill : State_Still);
		}
		return FSM_Continue;
	}

	// Check for a transition to the still state
	const bool bCanTransitionToStill = CanTransitionToStill();
	if (!bCanTransitionToStill)
	{
		rPed.SetPedResetFlag(CPED_RESET_FLAG_PreventBicycleFromLeaningOver, true); // reset pre-task
	}

	if (bCanTransitionToStill && CheckForStill(*m_pVehicle, rPed))
	{
		SetTaskState(m_bUseStillTransitions ? State_SitToStill : State_Still);
		return FSM_Continue;
	}

	if (IsTransitioningToState(State_Sitting))
	{
		m_bWasFreewheeling = IsPedFreewheeling();
		return FSM_Continue;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionOnBicycle::Standing_OnProcessMoveSignals()
{
	CPed* pPed = GetPed();
	
	// Head too bouncy
	pPed->SetCodeHeadIkBlocked(); // reset every frame

	m_bMoVE_AreMoveNetworkStatesValid = AreMoveNetworkStatesValid(ms_StandingRequestId);
	
	if( !m_bMoVE_AreMoveNetworkStatesValid )
	{
		SetBlendDurationForState(true);
		return;
	}
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycle::Reversing_OnEnter()
{
	m_bSentAdditiveRequest = false;
	SetMoveNetworkStates(CTaskMotionInVehicle::ms_ReverseRequestId, CTaskMotionInVehicle::ms_ReverseOnEnterId);
	m_bMoVE_AreMoveNetworkStatesValid = false;
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycle::Reversing_OnUpdate(CPed& rPed)
{
	rPed.SetPedResetFlag(CPED_RESET_FLAG_RequiresLegIk, true); // reset pre-task

	TUNE_GROUP_FLOAT(TRIBIKE_TUNE, TIME_REVERSING_TO_APPLY_OFFSET, 0.5f, 0.0f, 3.0f, 0.01f);
	if (GetTimeInState() > TIME_REVERSING_TO_APPLY_OFFSET)
	{
		m_uTimeLastReversing = fwTimer::GetTimeInMilliseconds();
	}

	if (!m_bMoVE_AreMoveNetworkStatesValid)
		return FSM_Continue;

	if (CheckForRestartingStateDueToCameraSwitch())
	{
		return FSM_Continue;
	}

	if ((!CTaskMotionInVehicle::CheckForReverse(*m_pVehicle, rPed) && 
		(m_pVehicle->GetVelocityIncludingReferenceFrame().Mag2() < rage::square(ms_Tunables.m_MaxSpeedForStillReverse))) || CheckForWantsToMove(*m_pVehicle, rPed))
	{
		SetTaskState(State_Still);
		return FSM_Continue;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionOnBicycle::Reversing_OnProcessMoveSignals()
{
	m_bMoVE_AreMoveNetworkStatesValid = AreMoveNetworkStatesValid(CTaskMotionInVehicle::ms_ReverseRequestId);
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycle::Impact_OnEnter()
{
	m_fTimeStillAfterImpact = 0.0f;
	m_bWasInAir = false;
	SetMoveNetworkStates(ms_ImpactRequestId, ms_ImpactOnEnterId);
	m_bMoVE_AreMoveNetworkStatesValid = false;
	m_bCanBlendOutImpactClip = false;
	SetClipsForState(false);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycle::Impact_OnUpdate()
{
	if (!m_bMoVE_AreMoveNetworkStatesValid)
	{
		return FSM_Continue;
	}

	CPed& rPed = *GetPed();
	// MoVE signal checks directly here are OK: CheckForJumpPrepTransition
	const bool bShouldPrepToJump = CheckForJumpPrepTransition(m_pedMoveNetworkHelper, *m_pVehicle, rPed, m_bCachedJumpInput);
	if (bShouldPrepToJump)
	{
		m_bWantedToDoSpecialMoveInStillTransition = true;
		m_bUseSitTransitionJumpPrepBlendDuration = true;
		SetTaskState(State_Sitting);
		return FSM_Continue;
	}

	if (m_bSwitchClipsThisFrame)
	{
		SetClipsForState(false);
		SetClipsForState(true);
		m_bSwitchClipsThisFrame = false;
	}

	if (m_bCanBlendOutImpactClip)
	{
		TUNE_GROUP_FLOAT(BICYCLE_TUNE, MIN_JUMP_HEIGHT_TO_FORCE_STANDING, 2.0f, 0.0f, 10.0f, 0.1f);
		const float fMaxJumpHeight = Abs(m_fMaxJumpHeight - m_fMinJumpHeight);
		if (fMaxJumpHeight > MIN_JUMP_HEIGHT_TO_FORCE_STANDING || PedWantsToStand(rPed, *m_pVehicle, m_bHaveShiftedWeightFwd))
		{
			SetTaskState(State_Standing);
			return FSM_Continue;
		}
		else
		{
			SetTaskState(State_Sitting);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionOnBicycle::Impact_OnExit()
{
	m_bWasLaunchJump = false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionOnBicycle::Impact_OnProcessMoveSignals()
{
	m_bMoVE_AreMoveNetworkStatesValid = AreMoveNetworkStatesValid(ms_ImpactRequestId);
	
	if( !m_bMoVE_AreMoveNetworkStatesValid )
	{
		SetClipsForState(true);
	}

	m_bCanBlendOutImpactClip = m_pedMoveNetworkHelper.GetBoolean(ms_CanBlendOutImpactClipId) || GetTimeInState() > 2.0f;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionOnBicycle::SetTaskState(eOnBicycleState eState)
{
#if !__FINAL
	BANK_ONLY(taskDebugf2("Frame : %u - %s%s : 0x%p : changed state from %s:%s:0x%p to %s", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone ped : " : "Local ped : ", GetPed()->GetDebugName(), GetPed() , GetTaskName(), GetStateName(GetState()), this, GetStateName((s32)eState)));
#endif // !__FINAL

	SetState((s32)eState);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionOnBicycle::SetMoveNetworkStates(const fwMvRequestId& rRequestId, const fwMvBooleanId& rOnEnterId, bool bIgnoreVehicle)
{
#if __BANK
	taskDebugf2("Frame : %u - %s%s sent request %s from %s waiting for on enter event %s", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone ped : " : "Local ped : ", GetPed()->GetDebugName(), rRequestId.GetCStr(), GetStateName(GetState()), rOnEnterId.GetCStr());
#endif // __BANK

	m_pedMoveNetworkHelper.SendRequest(rRequestId);
	m_pedMoveNetworkHelper.WaitForTargetState(rOnEnterId);

	if (!bIgnoreVehicle)
	{
		m_bicycleMoveNetworkHelper.WaitForTargetState(rOnEnterId);
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::AreMoveNetworkStatesValid(fwMvRequestId requestId)
{
	const bool bPedInTargetState = m_pedMoveNetworkHelper.IsInTargetState();
	const bool bBicycleInTargetState = m_bicycleMoveNetworkHelper.IsInTargetState();

#if __BANK
	const CPed& rPed = *GetPed();
	if (!bPedInTargetState)
	{
		taskDebugf2("Frame : %u - %s%s is not in anim target state and in %s", fwTimer::GetFrameCount(), rPed.IsNetworkClone() ? "Clone ped : " : "Local ped : ", rPed.GetDebugName(), GetStateName(GetState()));
	}
	if (!bBicycleInTargetState)
	{
		taskDebugf2("Frame : %u - bicycle is not in anim target state and in %s", fwTimer::GetFrameCount(), GetStateName(GetState()));
	}
#endif // __BANK

	// We do not send the bicycle request until the ped is in the target state because we will get the bicycle pose update that same frame due to
	// the instant anim update
	if (!bPedInTargetState)
	{
		return false;
	}

	if (!bBicycleInTargetState)
	{
		m_bicycleMoveNetworkHelper.SendRequest(requestId);
		m_uTimeBicycleLastSentRequest = fwTimer::GetTimeInMilliseconds();
		taskDebugf2("Frame : %u - Bicycle - m_bBicycleRequestSentThisFrame %u", fwTimer::GetFrameCount(), m_uTimeBicycleLastSentRequest);
	}

	return bPedInTargetState && bBicycleInTargetState;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionOnBicycle::SetBlendDurationForState(bool bSetOnBicycleNetwork)
{
	if (GetState() == State_Sitting)
	{
		if (m_bUseSitTransitionJumpPrepBlendDuration)
		{
			if (GetPreviousState() == State_StillToSit || GetPreviousState() == State_SitToStill)
			{
				if (bSetOnBicycleNetwork)
				{
					m_bicycleMoveNetworkHelper.SetFloat(ms_SitTransitionBlendDuration, m_pBicycleInfo->GetSitTransitionJumpPrepBlendDuration());
				}
				else
				{
					m_pedMoveNetworkHelper.SetFloat(ms_SitTransitionBlendDuration, m_pBicycleInfo->GetSitTransitionJumpPrepBlendDuration());
				}
			}
		}
		else if (GetPreviousState() == State_SitToStill)
		{
			if (bSetOnBicycleNetwork)
			{
				m_bicycleMoveNetworkHelper.SetFloat(ms_SitTransitionBlendDuration, m_pBicycleInfo->GetStillToSitToSitBlendOutDuration());
			}
			else
			{
				m_pedMoveNetworkHelper.SetFloat(ms_SitTransitionBlendDuration, m_pBicycleInfo->GetStillToSitToSitBlendOutDuration());
			}
		}
	}
	else if (GetState() == State_Standing)
	{
		if (WantsToWheelie(*GetPed(), m_bHaveShiftedWeightFwd))
		{
			if (bSetOnBicycleNetwork)
			{
				m_bicycleMoveNetworkHelper.SetFloat(ms_SitToStandBlendDurationId, ms_Tunables.m_WheelieSitToStandBlendDuration);
			}
			else
			{
				m_pedMoveNetworkHelper.SetFloat(ms_SitToStandBlendDurationId, ms_Tunables.m_WheelieSitToStandBlendDuration);
			}
		}
		else
		{
			if (bSetOnBicycleNetwork)
			{
				m_bicycleMoveNetworkHelper.SetFloat(ms_SitToStandBlendDurationId, ms_Tunables.m_DefaultSitToStandBlendDuration);
			}
			else
			{
				m_pedMoveNetworkHelper.SetFloat(ms_SitToStandBlendDurationId, ms_Tunables.m_DefaultSitToStandBlendDuration);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::CanBlockCameraViewModeTransitions() const 
{
	if(GetPed()->IsLocalPlayer())
	{
		//! We arent in a position to do an instant anim update, so don't transition.
		if (!m_bMoVE_AreMoveNetworkStatesValid)
			return true;

		//! Don't do in certain states where we override anims. Note: in certain states like shunt, we allow transition to 1st person as we
		//! instantly snap to correct pose. This handles the states where we have no instant transition.
		switch(GetState())
		{
			case State_PutOnHelmet:
			case State_SitToStill:
			case State_StillToSit:
			case State_Impact:
				return true;
			default:
				break;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::CheckForStateTransition(eOnBicycleState eState)
{
	switch (eState)
	{
		case State_Still:				return CheckForStillTransition();
		case State_StillToSit:			return CheckForStillToSitTransition();
		case State_Sitting:				return CheckForSittingTransition();
		case State_Standing:			return CheckForStandingTransition();
		case State_SitToStill:			return CheckForSitToStillTransition();
		case State_Impact:				return CheckForImpactTransition();
		case State_Reversing:			return CheckForReversingTransition();
		default: break;
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::CheckForStillTransition() const
{
	return false;
}

///////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::CheckForStillToSitTransition() const
{
	return false;
}

///////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::CheckForSittingTransition() const
{
	const CPed& rPed = *GetPed();
	if (rPed.GetPedResetFlag(CPED_RESET_FLAG_IsDoingDriveby))
	{
		if (m_pVehicle->GetLayoutInfo()->GetDisableFastPoseWhenDrivebying())
		{
			return true;
		}
	}

	// Have a minimum time in the standing state to prevent oscillation
	const float fStandTime = GetTimeInState();
	const float fMinTimeInStandState = rPed.IsInFirstPersonVehicleCamera() ? ms_Tunables.m_MinTimeInStandStateFPS : ms_Tunables.m_MinTimeInStandState;
	if (fStandTime < fMinTimeInStandState)
	{
		return false;
	}

	// If we're on a slope we want to stand
	if (m_bShouldStandBecauseOnSlope)
	{
		return false;
	}

	// We also don't want to oscillate between pedaling and freewheeling
	const bool bIsPedaling = IsPedPedalling();
	if (!bIsPedaling && GetTimeFreewheeling() < m_pBicycleInfo->GetMinTimeInStandFreewheelState() && !rPed.IsInFirstPersonVehicleCamera())
	{
		return false;
	}

	float fSprintResult = GetSprintResultFromPed(rPed, *m_pVehicle);
	TUNE_GROUP_FLOAT(FIRST_PERSON_BICYCLE_TUNE, MIN_SPRINT_RESULT_FPS_FOR_SITTING, 1.4f, 0.0f, 3.0f, 0.01f);
	const float fMinSprintResultForSitting = rPed.IsInFirstPersonVehicleCamera() ? MIN_SPRINT_RESULT_FPS_FOR_SITTING : 1.0f;
	if (fSprintResult > fMinSprintResultForSitting)
	{
		return false;
	}
	
	// MoVE signal check here OK: FreeWheelBreakout
	const bool bCanBlendToSitting = !bIsPedaling || (bIsPedaling && m_pedMoveNetworkHelper.GetBoolean(ms_FreeWheelBreakoutId));
	if (bCanBlendToSitting && (fSprintResult >= 1.0f || !IsPedPedalling()))
	{
		return true;
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::CheckForStandingTransition() const
{
	const CPed& rPed = *GetPed();
	if (rPed.GetPedResetFlag(CPED_RESET_FLAG_IsDoingDriveby))
	{
		if (m_pVehicle->GetLayoutInfo()->GetDisableFastPoseWhenDrivebying())
		{
			return false;
		}
	}

	if (GetSubTask())
	{
		if (GetSubTask()->GetState() == CTaskMotionOnBicycleController::State_InAir || 
			GetSubTask()->GetState() == CTaskMotionOnBicycleController::State_Launching ||
			GetSubTask()->GetState() == CTaskMotionOnBicycleController::State_PreparingToLaunch ||
			GetSubTask()->GetState() == CTaskMotionOnBicycleController::State_TrackStand)
		{
			return false;
		}
	}

	TUNE_GROUP_BOOL(BICYCLE_TUNE, ALLOW_INSTANT_STAND_TRANSITION_IF_WANTING_TO_WHEELIE, true);
	// MoVE signal check here OK: FreeWheelBreakout
	if (!IsPedPedalling() || (m_pedMoveNetworkHelper.GetBoolean(ms_FreeWheelBreakoutId) || (ALLOW_INSTANT_STAND_TRANSITION_IF_WANTING_TO_WHEELIE && WantsToWheelie(rPed, m_bHaveShiftedWeightFwd))))
	{
		if (m_bWantedToStandInStillTransition)
		{
			return true;
		}

		if (PedWantsToStand(rPed, *m_pVehicle, m_bHaveShiftedWeightFwd))
		{
			return true;
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::CheckForSitToStillTransition() const
{
	return false;
}

///////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::CheckForImpactTransition() const
{
	if (m_bWasInAir && !m_pVehicle->IsInAir() && m_pBicycleInfo->GetHasImpactAnims())
	{
		TUNE_GROUP_FLOAT(BICYCLE_TUNE, MIN_JUMP_HEIGHT_TO_PLAY_IMPACT_ANIM, 0.3f, 0.0f, 1.0f, 0.01f);
		const float fMaxJumpHeight = Abs(m_fMaxJumpHeight - m_fMinJumpHeight);
		if (fMaxJumpHeight > MIN_JUMP_HEIGHT_TO_PLAY_IMPACT_ANIM)
		{
			return true;
		}
		else if (!GetSubTask())
		{
			return true;
		}
		else
		{
			if (GetSubTask()->GetState() == CTaskMotionOnBicycleController::State_InAir)
			{
				return true;
			}
			else if (GetSubTask()->GetState() == CTaskMotionOnBicycleController::State_Launching)
			{
				TUNE_GROUP_FLOAT(BICYCLE_TUNE, MIN_TIME_IN_LAUNCH_STATE_FOR_IMPACT, 0.3f, 0.0f, 2.0f, 0.01f);
				if (GetSubTask()->GetTimeInState() > MIN_TIME_IN_LAUNCH_STATE_FOR_IMPACT)
				{
					return true;
				}
			}
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::CheckForReversingTransition() const
{
	const CPed& rPed = *GetPed();
	if (WantsToTrackStand(rPed, *m_pVehicle) || WantsToJump(rPed, *m_pVehicle, false))
	{
		return false;
	}
	
	if (IsSubTaskInState(CTaskMotionOnBicycleController::State_PedalToPreparingToLaunch) ||
		IsSubTaskInState(CTaskMotionOnBicycleController::State_PreparingToLaunch))
	{
		return false;
	}

	if (!m_pVehicle->GetWheel(1)->GetIsTouching() || !m_pVehicle->GetWheel(0)->GetIsTouching())
	{
		return false;
	}

	if (ms_Tunables.m_PreventDirectTransitionToReverseFromSit && GetState() == State_SitToStill && GetTimeInState() < ms_Tunables.m_MinTimeInSitToStillStateToReverse)
	{
		return false;
	}

	float fFrontBrakeButton = 0.0f;
	if (rPed.IsLocalPlayer())
	{
		const CControl* pControl = rPed.GetControlFromPlayer();
		if (pControl)
		{
			fFrontBrakeButton = pControl->GetVehiclePushbikeFrontBrake().GetNorm01();
		}
	}

	if (!IsSubTaskInState(CTaskMotionOnBicycleController::State_Pedaling))
	{
		if (m_pVehicle->GetLayoutInfo()->GetBicycleInfo()->GetCanTrackStand() && m_fTimeSinceWantingToTrackStand < ms_Tunables.m_TimeSinceNotWantingToTrackStandToAllowStillTransition)
		{
			if (fFrontBrakeButton == 0.0f)
			{
				return false;
			}
		}
	}

	if (rPed.IsLocalPlayer())
	{
		const CControl* pControl = rPed.GetControlFromPlayer();
		if (pControl)
		{
			float fRearBrakeButton = pControl->GetVehiclePushbikeRearBrake().GetNorm01();
			if (fRearBrakeButton > fFrontBrakeButton)
			{
				dev_float SPEED_TO_PLAY_REVERSE_CLIP_PLAYER_BICYCLE_REAR_BRAKE = 1.0f;
				const Vector3 vVehVelocity = m_pVehicle->GetVelocityIncludingReferenceFrame();
				if (vVehVelocity.Mag2() < square(SPEED_TO_PLAY_REVERSE_CLIP_PLAYER_BICYCLE_REAR_BRAKE))
				{
					return false;
				}
			}
		}
	}

	return CTaskMotionInVehicle::CheckForReverse(*m_pVehicle, rPed, false, true);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::CheckForHelmet(const CPed& ped) const
{
	// B*2214377: Disable bicycle helmets in MP.
	if (NetworkInterface::IsGameInProgress())
	{
		return false;
	}

	if(!m_pBicycleInfo->GetUseHelmet())
	{
		return false;
	}

	if (!CTaskMotionInAutomobile::WillPutOnHelmet(*m_pVehicle, ped))
		return false;

	if (ped.IsAPlayerPed() && m_fIdleTimer <= IDLE_TIMER_HELMET_PUT_ON)
		return false;

	return true;
}

///////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::CheckForRestartingStateDueToCameraSwitch()
{
	CPed& rPed = *GetPed();

	if (m_bRestartedThisFrame)
	{
		// Prevent new transitions for a frame to prevent move network from getting out of sync
		return true;
	}
	
	if (m_pedMoveNetworkHelper.IsNetworkAttached() && m_bicycleMoveNetworkHelper.IsNetworkAttached())
	{
		if (GetPreviousState() >= State_Still)
		{
			if ((fwTimer::GetTimeInMilliseconds() - m_uTimeBicycleLastSentRequest) == 0)
			{
				aiDebugf1("Frame %i, blocking transition bicycle network due to just sent bicycle request", fwTimer::GetFrameCount());
				return true;
			}

			if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOTION_ON_BICYCLE_CONTROLLER)
			{
				if (!static_cast<const CTaskMotionOnBicycleController*>(GetSubTask())->CanRestartNetwork())
				{
					aiDebugf1("Frame %i, blocking transition controller due to just sent bicycle request", fwTimer::GetFrameCount());
					return true;
				}
			}
		}

		if (GetState() == State_Impact)
		{
			if (ShouldChangeAnimsDueToCameraSwitch())
			{
				m_bSwitchClipsThisFrame = true;
				rPed.SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAIUpdate, true);	
				rPed.SetPedResetFlag(CPED_RESET_FLAG_ePostCameraAnimUpdateUseZeroTimestep, true);
				rPed.SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true);
			}
			return false;
		}

		if (m_bNeedToRestart)
		{
			if (GetState() != State_Still && GetState() != State_Sitting && GetState() != State_Standing)
			{
				m_bNeedToRestart = false;
				return false;
			}

			if (!m_bMoVE_AreMoveNetworkStatesValid)
				return true;

			aiDebugf1("Frame %i, can restart network", fwTimer::GetFrameCount());
			SetFlag(aiTaskFlags::RestartCurrentState);
			m_bNeedToRestart = false;
			m_bRestartedThisFrame = true;
			return true;
		}
		else if (ShouldChangeAnimsDueToCameraSwitch())
		{
			m_bNeedToRestart = true;
			rPed.SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAIUpdate, true);	
			rPed.SetPedResetFlag(CPED_RESET_FLAG_ePostCameraAnimUpdateUseZeroTimestep, true);
			rPed.SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true);
			if (m_bWasUsingFirstPersonClipSet)
			{
				// Forces the torso vehicle solver to blend out instantly
				rPed.SetPedResetFlag(CPED_RESET_FLAG_DisableTorsoVehicleSolver, true);
			}
			return true;
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////

void CTaskMotionOnBicycle::ProcessDriveBySpineAdditive(const CPed& rPed)
{
	if (rPed.GetPedResetFlag(CPED_RESET_FLAG_IsDoingDriveby) && UseUpperBodyDriveby() FPS_MODE_SUPPORTED_ONLY(&& !rPed.IsInFirstPersonVehicleCamera()))
	{
		if (!m_bSentAdditiveRequest)
		{
			CTaskAimGunVehicleDriveBy* pGunDriveByTask = static_cast<CTaskAimGunVehicleDriveBy*>(rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY));
			CTaskMountThrowProjectile* pProjDriveByTask = static_cast<CTaskMountThrowProjectile*>(rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_MOUNT_THROW_PROJECTILE));
			if (pGunDriveByTask || pProjDriveByTask)
			{
				const CVehicleDriveByInfo* pVehicleDriveByInfo = CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(&rPed);
				if (GetState() == State_Still || GetState() == State_SitToStill)
				{
					if (GetTimeInState() >= pVehicleDriveByInfo->GetSpineAdditiveBlendInDelay())
					{
						const float fSpineAdditiveBlendInDuration = GetState() == State_Still ? pVehicleDriveByInfo->GetSpineAdditiveBlendInDurationStill() : pVehicleDriveByInfo->GetSpineAdditiveBlendInDuration();
						if (pGunDriveByTask) 
						{
							pGunDriveByTask->RequestSpineAdditiveBlendIn(fSpineAdditiveBlendInDuration);
						}
						else
						{
							pProjDriveByTask->RequestSpineAdditiveBlendIn(fSpineAdditiveBlendInDuration);
						}
						m_bSentAdditiveRequest = true;
					}
				}
				else
				{
					if (GetTimeInState() >= pVehicleDriveByInfo->GetSpineAdditiveBlendOutDelay())
					{
						if (pGunDriveByTask) 
						{
							pGunDriveByTask->RequestSpineAdditiveBlendOut(pVehicleDriveByInfo->GetSpineAdditiveBlendOutDuration());
						}
						else
						{
							pProjDriveByTask->RequestSpineAdditiveBlendOut(pVehicleDriveByInfo->GetSpineAdditiveBlendOutDuration());
						}
						m_bSentAdditiveRequest = true;
					}
				}
			}
		}
	}
	else
	{
		m_bSentAdditiveRequest = false;
	}
}

///////////////////////////////////////////////////////////////////////////

void CTaskMotionOnBicycle::ProcessLateralBodyLean(const CPed& rPed, float fTimeStep)
{
	if (GetState() != State_StillToSit)
	{
		rage::Approach(m_fLeanRate, ms_Tunables.m_LeanAngleSmoothingRate, ms_Tunables.m_StillToSitApproachRate, fTimeStep);
	}

	CTaskMotionInVehicle::ComputePitchSignal(rPed, m_fPitchAngle, CTaskMotionInVehicle::ms_Tunables.m_DefaultPitchSmoothingRate);
	m_fPreviousBodyLeanX = m_fBodyLeanX;
	if (GetState() == State_StillToSit)
	{
		m_fBodyLeanX = 0.5f;
	}
	else if (m_bIsTucking)
	{
		TUNE_GROUP_FLOAT(BICYCLE_TUNE, TUCK_RETURN_TO_CENTRE_LEAN_SPEED, 1.2f, 0.0f, 10.0f, 0.01f);
		rage::Approach(m_fBodyLeanX, 0.5f, TUCK_RETURN_TO_CENTRE_LEAN_SPEED, fTimeStep);
	}
	else
	{
		CTaskMotionInVehicle::ComputeBikeLeanAngleSignal(rPed, *m_pVehicle, m_fBodyLeanX, m_fLeanRate, ms_Tunables.m_LeanAngleSmoothingRate);
	}
}

///////////////////////////////////////////////////////////////////////////

void CTaskMotionOnBicycle::ProcessLongitudinalBodyLean(const CPed& rPed, float fTimeStep)
{
	if (rPed.IsLocalPlayer())
	{		
		const CControl* pControl = rPed.GetControlFromPlayer();
		float fStickDesiredYLeanIntention = pControl->GetVehicleSteeringUpDown().GetNorm();
		float fStickDesiredXLeanIntention = pControl->GetVehicleSteeringLeftRight().GetNorm();
		if(m_pVehicle && m_pVehicle->InheritsFromBike())
		{
			static_cast<CBike*>(m_pVehicle.Get())->GetBikeLeanInputs(fStickDesiredXLeanIntention, fStickDesiredYLeanIntention);
		}

		bool bIsInReturnZone = -fStickDesiredYLeanIntention < ms_Tunables.m_ReturnZoneThreshold;
		bool bIsInSideZone = Abs(fStickDesiredXLeanIntention) > ms_Tunables.m_SideZoneThreshold;
		bool bIsInSlowZone = bIsInSideZone|| ! bIsInReturnZone;

		// If we previously were in the slow zone and we still are, use the slow rate
		bool bShouldUseSlowLongitudinalRate = false;
		if (m_bWasInSlowZone && bIsInSlowZone)
		{
			bShouldUseSlowLongitudinalRate = true;
		}

		// Cache whether we were in the slow zone this frame for comparision next frame
		// Store if we have ever been in the side zone
		if (!bIsInSideZone)
		{
			if (bIsInReturnZone)
			{
				m_bHaveBeenInSideZone = false;
			}
		}
		else
		{
			m_bHaveBeenInSideZone = true;
		}
		m_bWasInSlowZone = bIsInSlowZone;
		m_bShouldUseSlowLongitudinalRate = bIsInSideZone || Abs(fStickDesiredYLeanIntention) < ms_Tunables.m_MaxYIntentionToUseSlowApproach || (m_bHaveBeenInSideZone && bShouldUseSlowLongitudinalRate);

		float fDesiredLeanIntention = 0.0f;
		float fApproachRate = 0.0f;

		// Lean forward so the pose blend to the prep launch is better
		const bool bIgnoreInAirCheck = false;
		if (WantsToJump(rPed, *m_pVehicle, bIgnoreInAirCheck))
		{
			TUNE_GROUP_FLOAT(BICYCLE_TUNE, LONGITUDINAL_BODY_LEAN_JUMP_APPROACH_RATE, 1.2f, 0.0f, 10.0f, 0.01f);
			fDesiredLeanIntention = 1.0f;
			fApproachRate = LONGITUDINAL_BODY_LEAN_JUMP_APPROACH_RATE;
		}
		// Lean backward when we Endo to counter-balance
		else if (m_pVehicle->GetWheel(1)->GetIsTouching() && !m_pVehicle->GetWheel(0)->GetIsTouching())
		{
			TUNE_GROUP_FLOAT(BICYCLE_TUNE, LONGITUDINAL_BODY_LEAN_ENDO_APPROACH_RATE, 3.0f, 0.0f, 10.0f, 0.1f);
			fDesiredLeanIntention = 0.0f;
			fApproachRate = LONGITUDINAL_BODY_LEAN_ENDO_APPROACH_RATE;
		}
		// Lean forward when we Wheelie to counter-balance
		else if (!m_pVehicle->GetWheel(1)->GetIsTouching() && m_pVehicle->GetWheel(0)->GetIsTouching())
		{
			TUNE_GROUP_FLOAT(BICYCLE_TUNE, LONGITUDINAL_BODY_LEAN_WHEELIE_APPROACH_RATE, 0.5f, 0.0f, 10.0f, 0.1f);
			TUNE_GROUP_FLOAT(BICYCLE_TUNE, MAX_WHEELIE_PITCH_SITTING, 0.55f, -1.57f, 1.57f, 0.01f);
			TUNE_GROUP_FLOAT(BICYCLE_TUNE, MAX_WHEELIE_PITCH_STANDING, 0.55f, -1.57f, 1.57f, 0.01f);
			TUNE_GROUP_FLOAT(BICYCLE_TUNE, LONGITUDINAL_BODY_LEAN_WHEELIE_MIN_INTENTION_SITTING, 0.5f, 0.0f, 1.0f, 0.01f);
			TUNE_GROUP_FLOAT(BICYCLE_TUNE, LONGITUDINAL_BODY_LEAN_WHEELIE_MAX_INTENTION_SITTING, 0.75f, 0.0f, 1.0f, 0.01f);
			TUNE_GROUP_FLOAT(BICYCLE_TUNE, LONGITUDINAL_BODY_LEAN_WHEELIE_MIN_INTENTION_STANDING, 0.5f, 0.0f, 1.0f, 0.01f);
			TUNE_GROUP_FLOAT(BICYCLE_TUNE, LONGITUDINAL_BODY_LEAN_WHEELIE_MAX_INTENTION_STANDING, 0.75f, 0.0f, 1.0f, 0.01f);
			const float fMaxWheeliePitch = GetState() == State_Standing ? MAX_WHEELIE_PITCH_STANDING : MAX_WHEELIE_PITCH_SITTING;
			const float fMinIntention = GetState() == State_Standing ? LONGITUDINAL_BODY_LEAN_WHEELIE_MIN_INTENTION_STANDING : LONGITUDINAL_BODY_LEAN_WHEELIE_MIN_INTENTION_SITTING;
			const float fMaxIntention = GetState() == State_Standing ? LONGITUDINAL_BODY_LEAN_WHEELIE_MAX_INTENTION_STANDING : LONGITUDINAL_BODY_LEAN_WHEELIE_MAX_INTENTION_SITTING;
			const float fCurrentPitch = Max(rPed.GetCurrentPitch(), 0.0f);
			float fBikePitchAngleNorm = fCurrentPitch / fMaxWheeliePitch;
			fDesiredLeanIntention = fBikePitchAngleNorm * fMinIntention + (1.0f - fBikePitchAngleNorm) * fMaxIntention;
			fApproachRate = LONGITUDINAL_BODY_LEAN_WHEELIE_APPROACH_RATE;
		}
		else
		{
			const float fMinBikePitch = ms_Tunables.m_MinPitchDefault;
			float fBikePitchAngle = rPed.GetCurrentPitch() - fMinBikePitch;
			if (fBikePitchAngle > 1.0f) 
			{
				fBikePitchAngle = ms_Tunables.m_MaxForwardsPitchSlope;
			}
			else if (fBikePitchAngle < 0.0f) 
			{
				fBikePitchAngle = ms_Tunables.m_MinForwardsPitchSlope;
			}

#if __DEV
			grcDebugDraw::AddDebugOutput("Bike Pitch Angle : %.2f", fBikePitchAngle);
#endif // __DEV

			const float fBikeSlopeLowerBound = (0.5f - ms_Tunables.m_OnSlopeThreshold);
			const float fBikeSlopeUpperBound = (0.5f + ms_Tunables.m_OnSlopeThreshold);
			if (fBikePitchAngle <= fBikeSlopeLowerBound || fBikePitchAngle >= fBikeSlopeUpperBound)
			{
				m_bBicyclePitched = true;
			}
			else
			{
				m_bBicyclePitched = false;
			}

#if __DEV
			grcDebugDraw::AddDebugOutput(m_bBicyclePitched ? Color_green : Color_red, "Bike Pitched : %s", m_bBicyclePitched ? "TRUE" : "FALSE");
#endif // __DEV

			fStickDesiredYLeanIntention += 1.0f;
			fStickDesiredYLeanIntention *= 0.5f;
			fStickDesiredYLeanIntention = 1.0f - fStickDesiredYLeanIntention;

			if (m_bBicyclePitched)
			{
				fApproachRate = ms_Tunables.m_LongitudinalBodyLeanApproachRateSlope;
				fDesiredLeanIntention = fBikePitchAngle;

				if ((fStickDesiredYLeanIntention > fDesiredLeanIntention && fDesiredLeanIntention > 0.5f) || 
					(fStickDesiredYLeanIntention < fDesiredLeanIntention && fDesiredLeanIntention < 0.5f))
				{
					fDesiredLeanIntention = fStickDesiredYLeanIntention;
				}
			}
			else
			{
				TUNE_GROUP_FLOAT(BICYCLE_TUNE, SHIFTED_WEIGHT_BACK_BLEND_DELAY_DURATION, 1.0f, 0.0f, 10.0f, 0.1f);
				if (WantsToWheelie(rPed, m_bHaveShiftedWeightFwd) || (m_fTimeSinceShiftedWeightForward > 0.0f && m_fTimeSinceShiftedWeightForward < SHIFTED_WEIGHT_BACK_BLEND_DELAY_DURATION))
				{
					TUNE_GROUP_FLOAT(BICYCLE_TUNE, WANTS_TO_WHEELIE_UPPER_APPROACH_RATE_SHIFTED, 2.0f, 0.0f, 10.0f, 0.1f);
					TUNE_GROUP_FLOAT(BICYCLE_TUNE, WANTS_TO_WHEELIE_UPPER_APPROACH_RATE, 3.0f, 0.0f, 10.0f, 0.1f);
					TUNE_GROUP_FLOAT(BICYCLE_TUNE, WANTS_TO_WHEELIE_UPPER_INTENTION, 1.0f, 0.0f, 1.0f, 0.01f);
					fApproachRate = m_bHaveShiftedWeightFwd ? WANTS_TO_WHEELIE_UPPER_APPROACH_RATE_SHIFTED : WANTS_TO_WHEELIE_UPPER_APPROACH_RATE;
					fDesiredLeanIntention = WANTS_TO_WHEELIE_UPPER_INTENTION;
				}
				else
				{
					fApproachRate = ms_Tunables.m_LongitudinalBodyLeanApproachRate;
					fDesiredLeanIntention = fStickDesiredYLeanIntention;
				}
			}
			if (m_bShouldUseSlowLongitudinalRate)
			{
				fApproachRate = ms_Tunables.m_LongitudinalBodyLeanApproachRateSlow;
			}

#if __DEV
			grcDebugDraw::AddDebugOutput("Desired Lean Intention : %.2f", fDesiredLeanIntention);
			grcDebugDraw::AddDebugOutput(m_bShouldUseSlowLongitudinalRate ? Color_red : Color_green, "Approach Rate : %.2f", fApproachRate);
#endif // __DEV
		}

		if (rPed.GetPedResetFlag(CPED_RESET_FLAG_IsDoingDriveby))
		{
			const CVehicleDriveByInfo* pVehicleDriveByInfo = CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(&rPed);
			if (pVehicleDriveByInfo && pVehicleDriveByInfo->GetUseSpineAdditive())
			{
				const float fMaxLongitudinalDelta = pVehicleDriveByInfo->GetMaxLongitudinalLeanBlendWeightDelta();
				if (Abs(m_fBodyLeanY - 0.5f) >= fMaxLongitudinalDelta)
				{
					fDesiredLeanIntention = m_fBodyLeanY < 0.5f ? 0.5f - fMaxLongitudinalDelta : 0.5f + fMaxLongitudinalDelta;
					fApproachRate = pVehicleDriveByInfo->GetApproachSpeedToWithinMaxBlendDelta();
				}
			}
		}
		rage::Approach(m_fBodyLeanY, fDesiredLeanIntention, fApproachRate, fTimeStep);
	}
}

///////////////////////////////////////////////////////////////////////////

void CTaskMotionOnBicycle::ProcessPedalRate(const CPed& rPed, float fTimeStep)
{
	const bool bIgnoreInAirCheck = false;
	float fSprintResult = 0.0f;
	bool bShouldSpeedUpToReachCriticalPoint = PedWantsToStand(rPed, *m_pVehicle, fSprintResult, m_bHaveShiftedWeightFwd) || WantsToJump(rPed, *m_pVehicle, bIgnoreInAirCheck);	

	// Compute pedal rate
	if (GetState() == State_StillToSit)
	{
		// MoVE signal check here OK, this method is called from ProcessMoveSignals
		if (m_pedMoveNetworkHelper.GetBoolean(ms_UseDefaultPedalRateId))
		{
			m_fPedalRate = 1.0f;
		}
		else
		{
			if (!bShouldSpeedUpToReachCriticalPoint)
			{
				bShouldSpeedUpToReachCriticalPoint = m_bWantedToStandInStillTransition;
			}

			float fDesiredRate = ComputeDesiredClipRateFromVehicleSpeed(*m_pVehicle, *m_pBicycleInfo);

			if (bShouldSpeedUpToReachCriticalPoint)
			{
				// Make sure we don't pull the pedal rate down to zero or we'll never finish the anim!
				if (fSprintResult < 0.5f)
				{
					fSprintResult = 1.1f;
				}
				fDesiredRate *= m_pBicycleInfo->GetDesiredStandingRateMult() * fSprintResult;
			}

			rage::Approach(m_fPedalRate, fDesiredRate, m_pBicycleInfo->GetStillToSitPedalGearApproachRate(), fTimeStep);
		}
	}
	else
	{
		// Don't process pedal rate during these states as we override them
		if (GetSubTask())
		{
			if (GetSubTask()->GetState() == CTaskMotionOnBicycleController::State_Freewheeling ||
				GetSubTask()->GetState() == CTaskMotionOnBicycleController::State_PreparingToLaunch)
			{
				return;
			}

			if (GetSubTask()->GetState() == CTaskMotionOnBicycleController::State_PedalToInAir)
			{
				bShouldSpeedUpToReachCriticalPoint = true;
			}
		}

		float fDesiredPedalRate = 1.0f;
		float fPedalApproachRate = 1.0f;
		const bool bIsDoingDriveBy = rPed.GetPedResetFlag(CPED_RESET_FLAG_IsDoingDriveby);
		if (bIsDoingDriveBy)
		{
			TUNE_GROUP_FLOAT(BICYCLE_TUNE, DRIVE_BY_PEDAL_RATE, 1.0f, 0.0f, 5.0f, 0.01f);
			TUNE_GROUP_FLOAT(BICYCLE_TUNE, DRIVE_BY_PEDAL_APPROACH_RATE, 1.0f, 0.0f, 5.0f, 0.01f);
			fDesiredPedalRate = DRIVE_BY_PEDAL_RATE;
			fPedalApproachRate = DRIVE_BY_PEDAL_APPROACH_RATE;
		}
		else if (IsSubTaskInState(CTaskMotionOnBicycleController::State_FixieSkid) || IsSubTaskInState(CTaskMotionOnBicycleController::State_TrackStand))
		{
			TUNE_GROUP_FLOAT(BICYCLE_TUNE, TRACK_STAND_QUICK_START_PEDAL_RATE, 1.0f, 0.0f, 5.0f, 0.01f);
			TUNE_GROUP_FLOAT(BICYCLE_TUNE, TRACK_STAND_QUICK_START_SPEED, 1.0f, 0.0f, 5.0f, 0.01f);
			fDesiredPedalRate = TRACK_STAND_QUICK_START_PEDAL_RATE;
			fPedalApproachRate = TRACK_STAND_QUICK_START_SPEED;
		}
		else
		{
			fDesiredPedalRate = CTaskMotionOnBicycle::ComputeDesiredClipRateFromVehicleSpeed(*m_pVehicle, *m_pBicycleInfo, GetState() == State_Sitting);
			if (GetState() == State_Sitting)
			{
				// Speed up if we want to reach the critical phase
				if (bShouldSpeedUpToReachCriticalPoint)
				{
					fDesiredPedalRate += m_pBicycleInfo->GetSitToStandPedalAccelerationScalar() * fTimeStep;
					fDesiredPedalRate = Clamp(fDesiredPedalRate, 1.0f, m_pBicycleInfo->GetSitToStandPedalMaxRate());
				}
			}
			else if (GetState() == State_Standing)
			{
				// Don't pull the rate down too much if we decided to stop pedaling
				fSprintResult = Clamp(fSprintResult, 1.0f, 5.0f);
				fDesiredPedalRate *= m_pBicycleInfo->GetDesiredStandingRateMult() * fSprintResult;
			}
			fPedalApproachRate = bShouldSpeedUpToReachCriticalPoint ? m_pBicycleInfo->GetStandPedGearApproachRate() :  m_pBicycleInfo->GetSitPedalGearApproachRate();
		}

		TUNE_GROUP_FLOAT(BICYCLE_TUNE, MIN_PEDAL_RATE, 0.1f, 0.0f, 3.0f, 0.01f);
		TUNE_GROUP_FLOAT(BICYCLE_TUNE, MAX_PEDAL_RATE, 1.5f, 0.0f, 3.0f, 0.01f);
		fDesiredPedalRate = Clamp(fDesiredPedalRate, MIN_PEDAL_RATE, MAX_PEDAL_RATE);
		rage::Approach(m_fPedalRate, fDesiredPedalRate, fPedalApproachRate, fTimeStep);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionOnBicycle::ProcessAllowWheelie(CPed& rPed, float fTimeStep)
{
	if (!rPed.IsLocalPlayer())
	{
		return;
	}

	const CControl* pControl = rPed.GetControlFromPlayer();
	if (!pControl)
	{
		return;
	}

	float fStickDesiredXLeanIntention = pControl->GetVehicleSteeringLeftRight().GetNorm();
	float fStickDesiredYLeanIntention = pControl->GetVehicleSteeringUpDown().GetNorm();
	if(m_pVehicle && m_pVehicle->InheritsFromBike())
	{
		if (m_pVehicle->pHandling->GetBikeHandlingData() && m_pVehicle->pHandling->GetBikeHandlingData()->m_fJumpForce == 0.0f)
		{
			return;
		}

		static_cast<CBike*>(m_pVehicle.Get())->GetBikeLeanInputs(fStickDesiredXLeanIntention, fStickDesiredYLeanIntention);
	}

	TUNE_GROUP_FLOAT(BICYCLE_TUNE, DEAD_ZONE_THRESHOLD, 0.1f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_BOOL(BICYCLE_TUNE, ALLOW_NO_STICK_INPUT, false);
	if (!ALLOW_NO_STICK_INPUT && Abs(fStickDesiredYLeanIntention) < DEAD_ZONE_THRESHOLD)
	{
		m_fTimeInDeadZone += fTimeStep;
	}

	TUNE_GROUP_FLOAT(BICYCLE_TUNE, MAX_TIME_BEFORE_WEIGHT_SHIFT_RESET, 0.5f, 0.0f, 2.0f, 0.01f);
	if (!m_bHaveShiftedWeightFwd)
	{
		if (fStickDesiredYLeanIntention < 0.0f)
		{
			m_fTimeInDeadZone = 0.0f;
			m_bHaveShiftedWeightFwd = true;
			m_bWheelieTimerUp = false;
		}
		m_fTimeSinceShiftedWeightForward = 0.0f;
	}
	else if (m_fTimeSinceShiftedWeightForward >= ms_Tunables.m_MaxTimeSinceShiftedWeightForwardToAllowWheelie)
	{
		m_bHaveShiftedWeightFwd = false;
	}
	else if (m_fTimeSinceShiftedWeightForward >= MAX_TIME_BEFORE_WEIGHT_SHIFT_RESET)
	{
		if (fStickDesiredYLeanIntention < 0.0f)
		{
			m_bHaveShiftedWeightFwd = false;
		}
	}
	else if (!ALLOW_NO_STICK_INPUT)
	{
		TUNE_GROUP_FLOAT(BICYCLE_TUNE, LATERAL_STICK_WEIGHT_SHIFT_THRESHOLD, 0.8f, 0.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(BICYCLE_TUNE, DEAD_ZONE_TIME_TO_RESET_WEIGHT_SHIFT, 0.1f, 0.0f, 1.0f, 0.01f);
		if (m_fTimeInDeadZone > DEAD_ZONE_TIME_TO_RESET_WEIGHT_SHIFT)
		{
			m_bHaveShiftedWeightFwd = false;
		}
		else if (Abs(fStickDesiredXLeanIntention) >= LATERAL_STICK_WEIGHT_SHIFT_THRESHOLD)
		{
			m_bHaveShiftedWeightFwd = false;
		}
	}

	if (m_bHaveShiftedWeightFwd)
	{
		m_fTimeSinceShiftedWeightForward += fTimeStep;
	}

	const bool bFrontWheelTouching =  m_pVehicle->GetWheel(1)->GetIsTouching(); 
	const bool bRearWheelTouching =  m_pVehicle->GetWheel(0)->GetIsTouching(); 

	if(bFrontWheelTouching && bRearWheelTouching)
	{
		m_fTimeSinceBikeInAir = 0.0f;
	}
	else if(m_bWasInAir)
	{
		m_fTimeSinceBikeInAir += fTimeStep;
	}
	
	const bool bIsDoingWheelie = !bFrontWheelTouching && bRearWheelTouching;
	m_pedMoveNetworkHelper.SetFlag(bIsDoingWheelie, ms_DoingWheelieFlagId);

	const bool bWasInAirRecentlyConditionPassed = (m_fTimeSinceBikeInAir > 0.0f && m_fTimeSinceBikeInAir < ms_Tunables.m_MaxTimeSinceShiftedWeightForwardToAllowWheelie);
	const bool bWeightShiftConditionPassed = (m_fTimeSinceShiftedWeightForward > 0.0f && m_fTimeSinceShiftedWeightForward < ms_Tunables.m_MaxTimeSinceShiftedWeightForwardToAllowWheelie);
	bool bCanWheelie = (IsPedPedalling() || bWasInAirRecentlyConditionPassed) && !m_bWheelieTimerUp && (bIsDoingWheelie || bWeightShiftConditionPassed);


	// Wheelie's are now limited by time rather than angle
	if(rPed.IsLocalPlayer())
	{
		StatId stat = STAT_WHEELIE_ABILITY.GetStatId();
		float fWheelieStatValue = rage::Clamp(static_cast<float>(StatsInterface::GetIntStat(stat)) / 100.0f, 0.0f, 1.0f);
		float fMinWheelieAbility = CPlayerInfo::GetPlayerStatInfoForPed(rPed).m_MinWheelieAbility;
		float fMaxWheelieAbility = CPlayerInfo::GetPlayerStatInfoForPed(rPed).m_MaxWheelieAbility;

		float fWheelieAbilityMult = ((1.0f - fWheelieStatValue) * fMinWheelieAbility + fWheelieStatValue * fMaxWheelieAbility)/100.0f;

		// If we have a certain amount of wheelie ability then don't time out wheelies
		TUNE_GROUP_FLOAT(BICYCLE_TUNE, WHEELIE_ABILITY_LIMIT_TO_DISABLE_TIMEOUT, 1.15f, 0.0f, 2.0f, 0.01f);

		if(fWheelieAbilityMult < WHEELIE_ABILITY_LIMIT_TO_DISABLE_TIMEOUT)
		{	
			// we want the delta between wheelie stats to have a larger effect.
			TUNE_GROUP_FLOAT(BICYCLE_TUNE, WIDEN_WHEELIE_ABILITY_DELTA_MULT, 2.0f, 0.0f, 5.0f, 0.01f);
			fWheelieAbilityMult = (fMaxWheelieAbility/100.0f) - (((fMaxWheelieAbility/100.0f) - fWheelieAbilityMult) * WIDEN_WHEELIE_ABILITY_DELTA_MULT);
			fWheelieAbilityMult = rage::Clamp(fWheelieAbilityMult, 0.1f, 2.0f);

			TUNE_GROUP_FLOAT(BICYCLE_TUNE, MAX_WHEELIE_TIME, 2.5f, 0.0f, 120.0f, 0.01f);

			float fMaxWheelieTime(MAX_WHEELIE_TIME * fWheelieAbilityMult);

			fMaxWheelieTime *= fMaxWheelieTime;

			//keep track of how long the wheelie has been going on for.
			if(bIsDoingWheelie)
			{
				m_fTimeSinceWheelieStarted += fTimeStep;
			}
			else
			{
				m_fTimeSinceWheelieStarted = 0.0f;
			}

			if(m_fTimeSinceWheelieStarted > fMaxWheelieTime)
			{
				bCanWheelie = false;
				m_bWheelieTimerUp = true;
			}
		}
	}

	if (bCanWheelie)
	{
		m_bShouldSetCanWheelieFlag = true;
	}
	else
	{
		m_bHaveShiftedWeightFwd = false;

	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionOnBicycle::ProcessDownHillSlopeBlend(CPed& rPed, float fTimeStep)
{
	if (rPed.IsLocalPlayer())
	{	
		TUNE_GROUP_FLOAT(BICYCLE_TUNE, DOWNHILL_APPROACH_RATE, 6.0f, 0.0f, 10.0f, 0.1f);
		TUNE_GROUP_FLOAT(BICYCLE_TUNE, WHEELIE_DOWNHILL_TIME_SINCE_WEIGHT_SHIFT, 0.75f, 0.0f, 2.0f, 0.01f);
		float fDesiredBlend = 0.0f;
		float fApproachRate = DOWNHILL_APPROACH_RATE;

		if (WantsToWheelie(rPed, m_bHaveShiftedWeightFwd))
		{
			bool bProcessDownHillReturn = true;
			if (m_fTimeSinceShiftedWeightForward < WHEELIE_DOWNHILL_TIME_SINCE_WEIGHT_SHIFT)
			{
				TUNE_GROUP_FLOAT(BICYCLE_TUNE, WHEELIE_DOWNHILL_INITIAL_APPROACH_RATE_SITTING, 4.0f, 0.0f, 50.0f, 0.1f);
				TUNE_GROUP_FLOAT(BICYCLE_TUNE, WHEELIE_DOWNHILL_INITIAL_APPROACH_RATE, 4.0f, 0.0f, 10.0f, 0.1f);
				TUNE_GROUP_FLOAT(BICYCLE_TUNE, WHEELIE_DOWNHILL_MAX_BLEND, 1.0f, 0.0f, 1.0f, 0.01f);	
				TUNE_GROUP_FLOAT(BICYCLE_TUNE, MAX_WHEELIE_PITCH, 0.5f, -1.57f, 1.57f, 0.01f);
				fDesiredBlend = 1.0f - Clamp(Abs(rPed.GetCurrentPitch()) / MAX_WHEELIE_PITCH, 0.0f, 1.0f);
				fDesiredBlend *= WHEELIE_DOWNHILL_MAX_BLEND;
				if (GetState() == State_Sitting)
				{
					fDesiredBlend = 1.0f;
					fApproachRate = WHEELIE_DOWNHILL_INITIAL_APPROACH_RATE_SITTING;
				}
				else
				{
					fApproachRate = WHEELIE_DOWNHILL_INITIAL_APPROACH_RATE;
				}
				bProcessDownHillReturn = false;
			}

			if (bProcessDownHillReturn)
			{
				TUNE_GROUP_FLOAT(BICYCLE_TUNE, WHEELIE_DOWNHILL_RETURN_APPROACH_RATE, 1.0f, 0.0f, 10.0f, 0.1f);
				fApproachRate = WHEELIE_DOWNHILL_RETURN_APPROACH_RATE;
			}
		}
		else
		{
			TUNE_GROUP_FLOAT(BICYCLE_TUNE, MIN_STANDING_PITCH_TO_BLEND_DOWN_HILL_ANIMS, -0.05f, -1.57f, 1.57f, 0.01f);
			if (rPed.GetCurrentPitch() < MIN_STANDING_PITCH_TO_BLEND_DOWN_HILL_ANIMS)
			{
				TUNE_GROUP_FLOAT(BICYCLE_TUNE, MAX_DOWNHILL_PITCH, 0.5f, -1.57f, 1.57f, 0.01f);
				fDesiredBlend = Clamp(Abs(rPed.GetCurrentPitch()) / MAX_DOWNHILL_PITCH, 0.0f, 1.0f);
			}

			if (GetState() == State_Impact || IsSubTaskInState(CTaskMotionOnBicycleController::State_InAir))
			{
				TUNE_GROUP_FLOAT(BICYCLE_TUNE, DOWNHILL_BLEND_IN_AIR_APPROACH_RATE, 1.0f, 0.0f, 10.0f, 0.1f);
				fApproachRate = DOWNHILL_BLEND_IN_AIR_APPROACH_RATE;
			}
		}

		rage::Approach(m_fDownHillBlend, fDesiredBlend, fApproachRate, fTimeStep);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionOnBicycle::ProcessTuckBlend(CPed& rPed, float fTimeStep)
{
	if (rPed.IsLocalPlayer())
	{
		m_bIsTucking = false;
		
		if (m_pBicycleInfo->GetCanTrackStand())
		{
			TUNE_GROUP_FLOAT(BICYCLE_TUNE, TUCKING_BLEND_IN_SPEED, 1.5f, 0.0f, 5.0f, 0.01f);
			TUNE_GROUP_FLOAT(BICYCLE_TUNE, TUCKING_BLEND_OUT_SPEED, 1.5f, 0.0f, 5.0f, 0.01f);

			float fDesiredTuckBlend = 0.0f;
			float fBlendSpeed = TUCKING_BLEND_OUT_SPEED;
			
			if ((WantsToDoSpecialMove(rPed, m_pBicycleInfo->GetIsFixieBike()) || IsSubTaskInState(CTaskMotionOnBicycleController::State_TrackStand))
				&& !IsSubTaskInState(CTaskMotionOnBicycleController::State_InAir) 
				&& GetState() != State_Impact)
			{
				m_bIsTucking = true;
				fDesiredTuckBlend = 1.0f;
				fBlendSpeed = TUCKING_BLEND_IN_SPEED;
			}

			rage::Approach(m_fTuckBlend, fDesiredTuckBlend, fBlendSpeed, fTimeStep);
		}
	}
	
	// MoVE signal check here OK, this method is called from ProcessMoveSignals
	m_pedMoveNetworkHelper.SetFlag(m_bIsTucking, ms_UseTuckFlagId);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::StartPedMoveNetwork(const CVehicleSeatAnimInfo* pSeatClipInfo)
{
	if (!taskVerifyf(pSeatClipInfo, "NULL Seat Clip Info"))
		return false;

	CPed* pPed = GetPed();
#if __DEV
	fwMvClipSetId seatClipSetId = pSeatClipInfo->GetSeatClipSetId();
	// Check the ped is in the correct seat and their clipset is valid
	CTaskMotionInVehicle::VerifyNetworkStartUp(*m_pVehicle, *pPed, seatClipSetId);
#endif

	// need to check if the parent task is ready for us
	if (m_pedMoveNetworkHelper.RequestNetworkDef(m_PedsNetworkDefId))
	{
		if (!m_pedMoveNetworkHelper.IsNetworkActive())
		{
			m_pedMoveNetworkHelper.CreateNetworkPlayer(pPed, m_PedsNetworkDefId);
		}

		// Set has tuck flag when starting the network up
		const bool bHasTuckAnims = m_pBicycleInfo && !m_pBicycleInfo->GetIsFixieBike() && m_pBicycleInfo->GetCanTrackStand();
		m_pedMoveNetworkHelper.SetFlag(bHasTuckAnims, CTaskMotionOnBicycleController::ms_HasTuckId);
		m_pedMoveNetworkHelper.SetFlag(m_pBicycleInfo->GetCanTrackStand(), CTaskMotionOnBicycleController::ms_HasHandGripId);

		// We need to do the deferred insert a frame before we receive the ready event so we actually update the ped pose correctly
		// without this we could end up with a one frame tpose unless there are only clips hiding this delay
		if (!m_pedMoveNetworkHelper.IsNetworkAttached())
		{
			m_pedMoveNetworkHelper.DeferredInsert();
		}

		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::StartVehicleMoveNetwork()
{
	if( !m_bStartedBicycleNetwork )
	{
		if( !m_bicycleMoveNetworkHelper.IsNetworkActive() )
		{
			m_bicycleMoveNetworkHelper.CreateNetworkPlayer(m_pVehicle, CClipNetworkMoveInfo::ms_NetworkVehicleBicycle);
		}

		m_pVehicle->GetMoveVehicle().SetSecondaryNetwork(m_bicycleMoveNetworkHelper.GetNetworkPlayer(), 0.0f);
		m_bStartedBicycleNetwork = true;
		m_pVehicle->SwitchEngineOn();

		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::SetSeatClipSet(const CVehicleSeatAnimInfo* pSeatClipInfo)
{
	if (!taskVerifyf(pSeatClipInfo, "NULL Seat Clip Info"))
		return false;

	m_bWasUsingFirstPersonClipSet = ShouldUseFirstPersonAnims();
	aiDisplayf("Frame %i,m_bWasUsingFirstPersonClipSet = %s", fwTimer::GetFrameCount(), m_bWasUsingFirstPersonClipSet ? "TRUE" : "FALSE");

	const CPed& rPed = *GetPed();
	s32 iSeatIndex = m_pVehicle->GetSeatManager()->GetPedsSeatIndex(&rPed);
	fwMvClipSetId seatClipSetId = m_bWasUsingFirstPersonClipSet ? pSeatClipInfo->GetFirstPersonSeatClipSetId() : CTaskMotionInAutomobile::GetInVehicleClipsetId(rPed, m_pVehicle, iSeatIndex);

	// Look for an overriden invehicle context and find the clipset associated with our current seat
	CTaskMotionBase* pTask = GetPed()->GetPrimaryMotionTask();
	const u32 uOverrideInVehicleContext = pTask ? pTask->GetOverrideInVehicleContextHash() : 0;
	if (uOverrideInVehicleContext != 0)
	{
		const CInVehicleOverrideInfo* pOverrideInfo = CVehicleMetadataMgr::GetInVehicleOverrideInfo(uOverrideInVehicleContext);
		if (taskVerifyf(pOverrideInfo, "Couldn't find override info for context with hash %u", uOverrideInVehicleContext))
		{
			const CSeatOverrideAnimInfo* pSeatOverrideAnimInfo = pOverrideInfo->FindSeatOverrideAnimInfoFromSeatAnimInfo(pSeatClipInfo);
			if (pSeatOverrideAnimInfo)
			{
				const fwMvClipSetId overrideClipsetId = pSeatOverrideAnimInfo->GetSeatClipSet();
				if (overrideClipsetId != CLIP_SET_ID_INVALID)
				{
					fwClipSet* pClipSet = fwClipSetManager::GetClipSet(overrideClipsetId);
					if (taskVerifyf(pClipSet, "Clipset (%s) doesn't exist", overrideClipsetId.GetCStr()))
					{
						if (taskVerifyf(pClipSet->IsStreamedIn_DEPRECATED(), "Clipset %s wasn't streamed in", overrideClipsetId.GetCStr()))
						{
							seatClipSetId = overrideClipsetId;
						}
					}
				}
			}
		}
	}

#if __DEV
	// Check the ped is in the correct seat and their clipset is valid
	CTaskMotionInVehicle::VerifyNetworkStartUp(*m_pVehicle, *GetPed(), seatClipSetId);
#endif

	if (m_bicycleMoveNetworkHelper.IsNetworkActive())
	{
		m_bicycleMoveNetworkHelper.SetClipSet(seatClipSetId);
	}
	m_pedMoveNetworkHelper.SetClipSet(seatClipSetId);
	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::SetClipsForState(bool bForBicycle)
{
	const fwMvClipSetId clipsetId = m_pedMoveNetworkHelper.GetClipSetId();
	fwMvClipId clipId = CLIP_ID_INVALID;
	fwMvClipId clip1Id = CLIP_ID_INVALID;
	fwMvClipId moveClipId = CLIP_ID_INVALID;
	fwMvClipId moveClip1Id = CLIP_ID_INVALID;
	bool bValidState = false;

	switch (GetState())
	{	
	case State_SitToStill:
		{
			moveClipId = ms_ToStillTransitionClipId;

			if (!m_bUseTrackStandTransitionToStill)
			{
				clipId = bForBicycle ? ms_Tunables.m_SitToStillBikeClipId : ms_Tunables.m_SitToStillCharClipId; 		
			}
			else
			{
				if (m_bUseLeftFootTransition)
				{
					clipId = bForBicycle ? ms_Tunables.m_TrackStandToStillLeftBikeClipId : ms_Tunables.m_TrackStandToStillLeftCharClipId; 

				}
				else
				{
					clipId = bForBicycle ? ms_Tunables.m_TrackStandToStillRightBikeClipId : ms_Tunables.m_TrackStandToStillRightCharClipId; 
				}
			}

			bValidState = true;
			break;
		}
	case State_Impact:
		{
			moveClipId = ms_ImpactClipId;
			moveClip1Id = ms_ImpactClip1Id;

			const float fMaxJumpHeight = Abs(m_fMaxJumpHeight - m_fMinJumpHeight);
			aiDebugf1("Jump Height : %.2f", fMaxJumpHeight);
			const bool bUseSmallImpact = fMaxJumpHeight < ms_Tunables.m_MaxJumpHeightForSmallImpact ? true : false;
			if (bUseSmallImpact)
			{
				clipId = bForBicycle ? ms_Tunables.m_DefaultSmallImpactBikeClipId : ms_Tunables.m_DefaultSmallImpactCharClipId; 
				clip1Id = bForBicycle ? CLIP_ID_INVALID : ms_Tunables.m_DownHillSmallImpactCharClipId; 

			}
			else
			{
				clipId = bForBicycle ? ms_Tunables.m_DefaultImpactBikeClipId : ms_Tunables.m_DefaultImpactCharClipId; 
				clip1Id = bForBicycle ? CLIP_ID_INVALID : ms_Tunables.m_DownHillImpactCharClipId; 
			}

			bValidState = true;
			break;
		}
	default:
		{
			return false;
		}
	}

	if (bValidState)
	{
		const crClip* pClip = fwClipSetManager::GetClip(clipsetId, clipId);
		if (bForBicycle)
		{
			taskAssertf(pClip, "Couldn't find clip %s from clipset %s", clipId.GetCStr(), clipsetId.GetCStr());
			m_bicycleMoveNetworkHelper.SetClip(pClip, moveClipId);
			if (moveClip1Id != CLIP_ID_INVALID && clip1Id != CLIP_ID_INVALID)
			{
				const crClip* pClip1 = fwClipSetManager::GetClip(clipsetId, clip1Id);
				taskAssertf(pClip1, "Couldn't find clip %s from clipset %s", clip1Id.GetCStr(), clipsetId.GetCStr());
				m_bicycleMoveNetworkHelper.SetClip(pClip1, moveClip1Id);
			}
		}
		else
		{
			taskAssertf(pClip, "Couldn't find clip %s from clipset %s", clipId.GetCStr(), clipsetId.GetCStr());
			m_pedMoveNetworkHelper.SetClip(pClip, moveClipId);
			if (moveClip1Id != CLIP_ID_INVALID && clip1Id != CLIP_ID_INVALID)
			{
				const crClip* pClip1 = fwClipSetManager::GetClip(clipsetId, clip1Id);
				taskAssertf(pClip1, "Couldn't find clip %s from clipset %s", clip1Id.GetCStr(), clipsetId.GetCStr());
				m_pedMoveNetworkHelper.SetClip(pClip1, moveClip1Id);
			}
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::ShouldUseFirstPersonAnims() const
{
	const CPed& rPed = *GetPed();

	if (rPed.IsInFirstPersonVehicleCamera())
		return true;

	if (rPed.IsLocalPlayer() && m_pVehicle && m_pVehicle->InheritsFromBicycle() && !camInterface::GetCinematicDirector().IsAnyCinematicContextActive())
	{
		s32 iOnBikeViewMode = camControlHelper::GetViewModeForContext(camControlHelperMetadataViewMode::ON_BIKE);
		if (iOnBikeViewMode == camControlHelperMetadataViewMode::FIRST_PERSON)
		{
			return true;
		}
	}

	return false;
}

bool CTaskMotionOnBicycle::ShouldChangeAnimsDueToCameraSwitch() 
{
	return ((GetPed() == camInterface::FindFollowPed()) || (GetPed() == CGameWorld::FindLocalPlayer())) && (m_bWasUsingFirstPersonClipSet != ShouldUseFirstPersonAnims()) && SetSeatClipSet(m_pVehicleSeatAnimInfo);
}

////////////////////////////////////////////////////////////////////////////////

float CTaskMotionOnBicycle::ComputeDesiredClipRateFromVehicleSpeed(const CVehicle& rVeh, const CBicycleInfo& rBicycleInfo, bool bCruising)
{
	const atArray<CBicycleInfo::sGearClipSpeeds>& rGearClipSpeeds = bCruising ? rBicycleInfo.GetCruiseGearClipSpeeds() : rBicycleInfo.GetFastGearClipSpeeds();
	const float fSpeed = rVeh.GetVelocityIncludingReferenceFrame().Mag();
	for (s32 i=rGearClipSpeeds.GetCount()-1; i>=0; --i)
	{
		if (fSpeed >= rGearClipSpeeds[i].m_Speed)
		{
			return rGearClipSpeeds[i].m_ClipRate;
		}
	}
	taskAssertf(0,"Couldn't find valid gear rate");
	return 1.0f;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::WantsToDoSpecialMove(const CPed& rPed, bool bIsFixie)
{
	if (rPed.IsPlayer()) 
	{
		const CControl* pControl = rPed.GetControlFromPlayer();
		if (bIsFixie)
		{
            if(pControl)
            {
			    return pControl->GetVehiclePushbikeFrontBrake().IsDown() && pControl->GetVehicleBrake().IsDown();
            }
		}
		else
		{
			if (rPed.GetPedResetFlag(CPED_RESET_FLAG_IsDoingDriveby))
			{
				return false;
			}

            if(rPed.IsNetworkClone())
            {
                CNetObjPlayer *netObjPlayer = SafeCast(CNetObjPlayer, rPed.GetNetworkObject());
                
                if(netObjPlayer)
                {
                    return netObjPlayer->IsVehicleJumpDown();
                }
            }
            else
            {
			    if (pControl->GetVehicleJump().IsDown())
			    {
				    return true;
			    }
            }
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::WantsToTrackStand(const CPed& rPed, const CVehicle& rVeh)
{
	if (!rVeh.GetLayoutInfo()->GetBicycleInfo()->GetCanTrackStand())
	{
		return false;
	}

	const bool bIsLocalPlayer = rPed.IsLocalPlayer();
	const bool bIsFixie = rVeh.GetLayoutInfo()->GetBicycleInfo()->GetIsFixieBike();
	const float fSprintResult = GetSprintResultFromPed(rPed, rVeh);

	if (bIsLocalPlayer)
	{
		const CControl* pControl = rPed.GetControlFromPlayer();
		if (pControl)
		{
			const float fRearBrakeButton = pControl->GetVehiclePushbikeRearBrake().GetNorm01();
			if (fRearBrakeButton > 0.0f)
			{
				return WantsToDoSpecialMove(rPed, bIsFixie);
			}

			const float fFrontBrakeButton = pControl->GetVehiclePushbikeFrontBrake().GetNorm01();
			if (fFrontBrakeButton > 0.0f)
			{
				if (static_cast<const CBike&>(rVeh).IsConsideredStill(rVeh.GetVelocityIncludingReferenceFrame()))
				{
					return WantsToDoSpecialMove(rPed, bIsFixie);
				}
				else
				{
					return false;
				}
			}
		}
	}
	else
	{
		if (!bIsFixie)
		{
			if (fSprintResult > 0.8f)
			{
				return false;
			}
		}
	}

	if (!WantsToDoSpecialMove(rPed, bIsFixie))
	{
		return false;
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::WantsToJump(const CPed& rPed, const CVehicle& rVeh, bool bIgnoreInAirCheck)
{
	if (!WantsToDoSpecialMove(rPed, rVeh.GetLayoutInfo()->GetBicycleInfo()->GetIsFixieBike()))
	{
		return false;
	}

	// B*1863671 - if the player is using the phone camera don't bunny hop
	if( rPed.IsLocalPlayer() && CPhoneMgr::CamGetState() )
	{
		return false;
	}

	// Probably should get rid of that const_cast at some point, but I think theres some buoyancy code that needs changing
	if ((bIgnoreInAirCheck || !const_cast<CVehicle&>(rVeh).IsInAir()) && rVeh.pHandling->GetBikeHandlingData() 
		&& rVeh.pHandling->GetBikeHandlingData()->m_fJumpForce > 0.0f)
	{
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::CanJump(const CMoveNetworkHelper& rPedMoveNetworkHelper)
{
	if (rPedMoveNetworkHelper.GetBoolean(CTaskMotionOnBicycleController::ms_PrepareToLaunchId) ||
		rPedMoveNetworkHelper.GetBoolean(CTaskMotionOnBicycleController::ms_PrepareToLaunchRId) ||
		rPedMoveNetworkHelper.GetBoolean(ms_CanTransitionToJumpPrepId) ||
		rPedMoveNetworkHelper.GetBoolean(ms_CanTransitionToJumpPrepRId))
	{
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::CanTrackStand(const CMoveNetworkHelper& rPedMoveNetworkHelper)
{
	if (rPedMoveNetworkHelper.GetBoolean(CTaskMotionOnBicycleController::ms_PrepareToTrackStandId) ||
		rPedMoveNetworkHelper.GetBoolean(CTaskMotionOnBicycleController::ms_PrepareToTrackStandRId))
	{
		return true;
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycle::WasSprintingRecently(const CVehicle& rVeh)
{
	u32 uLastSprintTime = 0;

	const CTaskVehiclePlayerDriveBike* pPlayerDriveTask = static_cast<const CTaskVehiclePlayerDriveBike*>(rVeh.GetIntelligence()->GetTaskManager()->FindTaskByTypeActive(VEHICLE_TASK_TREE_PRIMARY, CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_BIKE));
	if (pPlayerDriveTask)
	{
		uLastSprintTime = pPlayerDriveTask->GetLastSprintTime();
	}

	const u32 uLastSprintDiff = fwTimer::GetTimeInMilliseconds() - uLastSprintTime;

	// Wait in the free wheel state for a while before transitioning to sitting
	static dev_u32 MIN_LAST_SPRINT_DIFF_TO_SIT = 500;
	if (uLastSprintTime > 0 && uLastSprintDiff <= MIN_LAST_SPRINT_DIFF_TO_SIT)
	{
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

float CTaskMotionOnBicycle::GetSprintResultFromPed(const CPed& rPed, const CVehicle& rVeh, bool bIgnoreVelocityTest)
{
	float fSprintResult = 0.0f;
	if (rPed.IsLocalPlayer() && !rPed.GetPlayerInfo()->AreControlsDisabled())
	{
		fSprintResult = rPed.GetPlayerInfo()->ControlButtonSprint(CPlayerInfo::SPRINT_ON_BICYCLE, true);

		const CControl* pControl = rPed.GetControlFromPlayer();
		float fFrontBrakeButton = pControl->GetVehiclePushbikeFrontBrake().GetNorm01();
		float fRearBrakeButton = pControl->GetVehiclePushbikeRearBrake().GetNorm01();
		if (fFrontBrakeButton > 0.0f || fRearBrakeButton > 0.0f)
		{
			if (bIgnoreVelocityTest || static_cast<const CBike&>(rVeh).IsConsideredStill(rVeh.GetVelocityIncludingReferenceFrame(), true))
			{
				fSprintResult = 0.0f;
			}
		}
	}
	else
	{
		if ((rVeh.GetThrottle() - rVeh.GetBrake()) > 0.0f)
		{
			fSprintResult = rVeh.GetVelocityIncludingReferenceFrame().Mag2() >= rage::square(CTaskMotionOnBicycleController::ms_Tunables.m_MinAiSpeedForStandingUp) ? 1.1f : 1.0f;
		}
	}
	return fSprintResult;
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL

void CTaskMotionOnBicycle::Debug() const
{
#if __BANK
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);

	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
	if (GetPed() == pPlayerPed)
	{
		grcDebugDraw::AddDebugOutput(Color_green, "m_fTuckBlend : %.2f", m_fTuckBlend);
		grcDebugDraw::AddDebugOutput(Color_green, "m_fFixieSkidBlend : %.2f", GetSubTask() ? ((CTaskMotionOnBicycleController*)(GetSubTask()))->GetFixieSkidBlend() : -1.0f);
		grcDebugDraw::AddDebugOutput(Color_green, "m_fTimeSinceWantingToTrackStand : %.2f", m_fTimeSinceWantingToTrackStand);
		grcDebugDraw::AddDebugOutput(Color_green, "m_fPedalRate : %.2f", m_fPedalRate);
		grcDebugDraw::AddDebugOutput(Color_green, "m_fTimeInDeadZone : %.2f", m_fTimeInDeadZone);
		grcDebugDraw::AddDebugOutput(Color_green, "m_bHaveShiftedWeightFwd : %s", m_bHaveShiftedWeightFwd ? "TRUE" : "FALSE");
		grcDebugDraw::AddDebugOutput(Color_green, "m_fTimeSinceShiftedWeightForward : %.2f", m_fTimeSinceShiftedWeightForward);
		grcDebugDraw::AddDebugOutput(Color_green, "m_fDownHillBlend : %.2f", m_fDownHillBlend);
		grcDebugDraw::AddDebugOutput(Color_green, "m_fBodyLeanY : %.2f", m_fBodyLeanY);
		grcDebugDraw::AddDebugOutput(Color_green, "Can Wheelie : %s", pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_CanDoBicycleWheelie) ? "TRUE" : "FALSE");
	}

	if (pEntity)
	{
		if (!pEntity->GetIsTypePed())
		{
			pEntity = pPlayerPed;
		}

		CPed* pPed = static_cast<CPed*>(pEntity);	

		if (pPed && pPed == GetPed() && m_pVehicle)
		{
			s32 iNumElements = 0;
			char szDebugText[128];
			Vector2 vCurrentRenderStartPos(SCREEN_START_X_POS, SCREEN_START_Y_POS + iNumElements++ * ELEMENT_HEIGHT);
			formatf(szDebugText, "Bicycle Speed = %.2f (%.2f)", m_pVehicle->GetVelocityIncludingReferenceFrame().Mag(), m_pVehicle->GetVelocity().Mag());
			grcDebugDraw::Text(vCurrentRenderStartPos, Color_blue, szDebugText);
			++iNumElements;

			vCurrentRenderStartPos = Vector2(SCREEN_START_X_POS, SCREEN_START_Y_POS + iNumElements++ * ELEMENT_HEIGHT);
			formatf(szDebugText, "Bicycle Task State = %s", GetStaticStateName(GetState()));
			grcDebugDraw::Text(vCurrentRenderStartPos, Color_blue, szDebugText);
			++iNumElements;

			if (GetSubTask())
			{
				vCurrentRenderStartPos = Vector2(SCREEN_START_X_POS, SCREEN_START_Y_POS + iNumElements++ * ELEMENT_HEIGHT);
				formatf(szDebugText, "Bicycle Controller Task State = %s", CTaskMotionOnBicycleController::GetStaticStateName(GetSubTask()->GetState()));
				grcDebugDraw::Text(vCurrentRenderStartPos, Color_blue, szDebugText);
				++iNumElements;
			}

			TUNE_GROUP_BOOL(BICYCLE_TUNE, VISUALIZE_CLIPS, true);
			if (VISUALIZE_CLIPS)
			{
				const s32 NUM_CLIPS_TO_VISUALIZE = 3;
				sClipDebugInfo clipsToVisualize[NUM_CLIPS_TO_VISUALIZE] =
				{
					{ ms_StillToSitClipId, ms_StillToSitPhase, ms_StillToSitRateId },
					{ CTaskMotionOnBicycleController::ms_CruisePedalClip, CTaskMotionOnBicycleController::ms_CruisePedalPhase, ms_CruisePedalRateId },
					{ CTaskMotionOnBicycleController::ms_FastPedalClip, CTaskMotionOnBicycleController::ms_FastPedalPhase, ms_FastPedalRateId }
				};

				for (u32 i=0; i<NUM_CLIPS_TO_VISUALIZE; ++i)
				{
					const crClip* pClip = m_pedMoveNetworkHelper.GetClip(clipsToVisualize[i].clipId);
					if (pClip)
					{
						const float fPhase = m_pedMoveNetworkHelper.GetFloat(clipsToVisualize[i].phaseId);
						const float fRate = m_pedMoveNetworkHelper.GetFloat(clipsToVisualize[i].rateId);
						VisualizeClip(*pClip, fPhase, fRate, iNumElements);
					}
				}
			}
		}
	}
#endif // __BANK
}

#if __BANK

dev_float CTaskMotionOnBicycle::SCREEN_START_X_POS = 0.65f;
dev_float CTaskMotionOnBicycle::SCREEN_START_Y_POS = 0.2f;
dev_float CTaskMotionOnBicycle::TIME_LINE_WIDTH = 0.2f;
dev_float CTaskMotionOnBicycle::ELEMENT_HEIGHT = 0.01f;

void CTaskMotionOnBicycle::VisualizeClip(const crClip& rClip, float fPhase, float fRate, s32& iNumElements) const
{
	TUNE_GROUP_FLOAT(BICYCLE_TUNE, PHASE_TEXT_X_OFFSET, 0.01f, -1.0f, 1.0f, 0.01f);
	
	char szDebugText[128];
	
	float fCurrentYPos = SCREEN_START_Y_POS + iNumElements++ * ELEMENT_HEIGHT;
	Vector2 vCurrentRenderStartPos(SCREEN_START_X_POS, fCurrentYPos);
	formatf(szDebugText, "%s, Rate : %.2f", rClip.GetName(), fRate);
	grcDebugDraw::Text(vCurrentRenderStartPos, Color_blue, szDebugText);

	++iNumElements;
	fCurrentYPos = SCREEN_START_Y_POS + iNumElements * ELEMENT_HEIGHT;

	++iNumElements;
	++iNumElements;
	vCurrentRenderStartPos = Vector2(SCREEN_START_X_POS, fCurrentYPos); 
	const float fTimeLineEndXPos = vCurrentRenderStartPos.x + TIME_LINE_WIDTH;
	Vector2 vTimeLineEndPos(fTimeLineEndXPos, fCurrentYPos);
	grcDebugDraw::Line(vCurrentRenderStartPos, vTimeLineEndPos, Color_white);

	// Render timeline tags
	const float fPhaseXPos = SCREEN_START_X_POS + fPhase * (fTimeLineEndXPos - SCREEN_START_X_POS);
	vCurrentRenderStartPos = Vector2(fPhaseXPos, fCurrentYPos - ELEMENT_HEIGHT); 

	// Render current phase
	TUNE_GROUP_BOOL(BICYCLE_TUNE, RENDER_CURRENT_PHASE, true);
	if (RENDER_CURRENT_PHASE)
	{
		Vector2 vPhaseEndPos(fPhaseXPos, fCurrentYPos + ELEMENT_HEIGHT);
		grcDebugDraw::Line(vCurrentRenderStartPos, vPhaseEndPos, Color_yellow);
	}
	
	// Render freewheel tags
	TUNE_GROUP_BOOL(BICYCLE_TUNE, RENDER_FREEWHEEL_BREAKOUT_TAGS, true);
	if (RENDER_FREEWHEEL_BREAKOUT_TAGS)
	{
		VisualizeFreewheelBreakOutTags(rClip, fTimeLineEndXPos, fCurrentYPos);
	}

	// Render freewheel reverse pedal tags
	TUNE_GROUP_BOOL(BICYCLE_TUNE, RENDER_FREEWHEEL_REVERSE_PEDAL_TAGS, true);
	if (RENDER_FREEWHEEL_REVERSE_PEDAL_TAGS)
	{
		VisualizeFreewheelReversePedalTags(rClip, fTimeLineEndXPos, fCurrentYPos);
	}

	// Render prep launch tags
	TUNE_GROUP_BOOL(BICYCLE_TUNE, RENDER_PREP_LAUNCH_TAGS, true);
	if (RENDER_PREP_LAUNCH_TAGS)
	{
		VisualizePrepLaunchTags(rClip, fTimeLineEndXPos, fCurrentYPos, true);
		VisualizePrepLaunchTags(rClip, fTimeLineEndXPos, fCurrentYPos, false);
	}

	TUNE_GROUP_BOOL(BICYCLE_TUNE, RENDER_TRACK_STAND_TAGS, true);
	if (RENDER_TRACK_STAND_TAGS)
	{
		VisualizeTrackStandTags(rClip, fTimeLineEndXPos, fCurrentYPos, true);
		VisualizeTrackStandTags(rClip, fTimeLineEndXPos, fCurrentYPos, false);
	}

	++iNumElements;

	formatf(szDebugText, "%.2f", fPhase);
	fCurrentYPos = SCREEN_START_Y_POS + iNumElements * ELEMENT_HEIGHT;
	vCurrentRenderStartPos = Vector2(fPhaseXPos - PHASE_TEXT_X_OFFSET, fCurrentYPos); 
	grcDebugDraw::Text(vCurrentRenderStartPos, Color_black, szDebugText);

	++iNumElements;
	++iNumElements;
	++iNumElements;

}

void CTaskMotionOnBicycle::VisualizeFreewheelBreakOutTags(const crClip& rClip, float fTimeLineEndXPos, float fCurrentYPos) const
{
	CClipEventTags::CTagIteratorAttributeValue<crTag, crPropertyAttributeHashString, atHashString> it(*rClip.GetTags(), 0.0f, 1.0f, CClipEventTags::MoveEvent, CClipEventTags::MoveEvent, ms_FreeWheelBreakoutId);

	while (*it)
	{
		TUNE_GROUP_FLOAT(BICYCLE_TUNE, FREEWHEEL_BREAKOUT_TAG_HEIGHT, 0.01f, -1.0f, 1.0f, 0.001f);
		const float fStartPhase = (*it)->GetStart();
		const float fStartPhaseXPos = SCREEN_START_X_POS + fStartPhase * (fTimeLineEndXPos - SCREEN_START_X_POS);
		const float fEndPhase = (*it)->GetEnd();
		const float fEndPhaseXPos = SCREEN_START_X_POS + fEndPhase * (fTimeLineEndXPos - SCREEN_START_X_POS);

		Vector2 vRenderStartPos(fStartPhaseXPos, fCurrentYPos - FREEWHEEL_BREAKOUT_TAG_HEIGHT * 0.5f); 
		Vector2 vRenderEndPos(fEndPhaseXPos, fCurrentYPos + FREEWHEEL_BREAKOUT_TAG_HEIGHT  * 0.5f);

		TUNE_GROUP_INT(BICYCLE_TUNE, FREEWHEEL_BREAKOUT_TAG_ALPHA, 200, 0, 255, 1);
		Color32 color = Color_red;
		color.SetAlpha((u8)FREEWHEEL_BREAKOUT_TAG_ALPHA);
		grcDebugDraw::Line(vRenderStartPos, vRenderEndPos, color);
		++it;
	}
}

void CTaskMotionOnBicycle::VisualizeFreewheelReversePedalTags(const crClip& rClip, float fTimeLineEndXPos, float fCurrentYPos) const
{
	CClipEventTags::CTagIteratorAttributeValue<crTag, crPropertyAttributeHashString, atHashString> it(*rClip.GetTags(), 0.0f, 1.0f, CClipEventTags::MoveEvent, CClipEventTags::MoveEvent, CTaskMotionOnBicycleController::ms_CanReverseCruisePedalToGetToFreeWheel);

	while (*it)
	{
		TUNE_GROUP_FLOAT(BICYCLE_TUNE, FREEWHEEL_REVERSE_PEDAL_TAG_HEIGHT, 0.01f, -1.0f, 1.0f, 0.001f);
		const float fStartPhase = (*it)->GetStart();
		const float fStartPhaseXPos = SCREEN_START_X_POS + fStartPhase * (fTimeLineEndXPos - SCREEN_START_X_POS);
		const float fEndPhase = (*it)->GetEnd();
		const float fEndPhaseXPos = SCREEN_START_X_POS + fEndPhase * (fTimeLineEndXPos - SCREEN_START_X_POS);

		Vector2 vRenderStartPos(fStartPhaseXPos, fCurrentYPos - FREEWHEEL_REVERSE_PEDAL_TAG_HEIGHT * 0.5f);
		Vector2 vRenderEndPos(fEndPhaseXPos, fCurrentYPos + FREEWHEEL_REVERSE_PEDAL_TAG_HEIGHT * 0.5f); 

		TUNE_GROUP_INT(BICYCLE_TUNE, FREEWHEEL_REVERSE_PEDAL_TAG_ALPHA, 100, 0, 255, 1);
		Color32 color = Color_red;
		color.SetAlpha((u8)FREEWHEEL_REVERSE_PEDAL_TAG_ALPHA);
		grcDebugDraw::RectAxisAligned(vRenderStartPos, vRenderEndPos, color);
		++it;
	}
}

void CTaskMotionOnBicycle::VisualizePrepLaunchTags(const crClip& rClip, float fTimeLineEndXPos, float fCurrentYPos, bool bLeftTags) const
{
	CClipEventTags::CTagIteratorAttributeValue<crTag, crPropertyAttributeHashString, atHashString> it(*rClip.GetTags(), 0.0f, 1.0f, CClipEventTags::MoveEvent, CClipEventTags::MoveEvent, bLeftTags ? CTaskMotionOnBicycleController::ms_PrepareToLaunchId : CTaskMotionOnBicycleController::ms_PrepareToLaunchRId);

	while (*it)
	{
		TUNE_GROUP_FLOAT(BICYCLE_TUNE, PREPARE_TO_LAUNCH_TAG_HEIGHT, 0.01f, -1.0f, 1.0f, 0.001f);
		const float fStartPhase = (*it)->GetStart();
		const float fStartPhaseXPos = SCREEN_START_X_POS + fStartPhase * (fTimeLineEndXPos - SCREEN_START_X_POS);
		const float fEndPhase = (*it)->GetEnd();
		const float fEndPhaseXPos = SCREEN_START_X_POS + fEndPhase * (fTimeLineEndXPos - SCREEN_START_X_POS);

		Vector2 vRenderStartPos(fStartPhaseXPos, fCurrentYPos - PREPARE_TO_LAUNCH_TAG_HEIGHT * 0.5f);
		Vector2 vRenderEndPos(fEndPhaseXPos, fCurrentYPos + PREPARE_TO_LAUNCH_TAG_HEIGHT * 0.5f); 

		TUNE_GROUP_INT(BICYCLE_TUNE, PREPARE_TO_LAUNCH_TAG_ALPHA, 200, 0, 255, 1);
		Color32 color = bLeftTags ? Color_orange : Color_blue;
		color.SetAlpha((u8)PREPARE_TO_LAUNCH_TAG_ALPHA);
		grcDebugDraw::Line(vRenderStartPos, vRenderEndPos, color);
		++it;
	}
}

void CTaskMotionOnBicycle::VisualizeTrackStandTags(const crClip& rClip, float fTimeLineEndXPos, float fCurrentYPos, bool bLeftTags) const
{
	CClipEventTags::CTagIteratorAttributeValue<crTag, crPropertyAttributeHashString, atHashString> it(*rClip.GetTags(), 0.0f, 1.0f, CClipEventTags::MoveEvent, CClipEventTags::MoveEvent, bLeftTags ? CTaskMotionOnBicycleController::ms_PrepareToTrackStandId : CTaskMotionOnBicycleController::ms_PrepareToTrackStandRId);

	while (*it)
	{
		TUNE_GROUP_FLOAT(BICYCLE_TUNE, TRACK_STAND_TAG_HEIGHT, 0.01f, -1.0f, 1.0f, 0.001f);
		const float fStartPhase = (*it)->GetStart();
		const float fStartPhaseXPos = SCREEN_START_X_POS + fStartPhase * (fTimeLineEndXPos - SCREEN_START_X_POS);
		const float fEndPhase = (*it)->GetEnd();
		const float fEndPhaseXPos = SCREEN_START_X_POS + fEndPhase * (fTimeLineEndXPos - SCREEN_START_X_POS);

		Vector2 vRenderStartPos(fStartPhaseXPos, fCurrentYPos - TRACK_STAND_TAG_HEIGHT * 0.5f);
		Vector2 vRenderEndPos(fEndPhaseXPos, fCurrentYPos + TRACK_STAND_TAG_HEIGHT * 0.5f); 

		TUNE_GROUP_INT(BICYCLE_TUNE, TRACK_STAND_TAG_ALPHA, 200, 0, 255, 1);
		Color32 color = bLeftTags ? Color_green : Color_cyan;
		color.SetAlpha((u8)TRACK_STAND_TAG_ALPHA);
		grcDebugDraw::Line(vRenderStartPos, vRenderEndPos, color);
		++it;
	}
}

#endif // __BANK

#endif // !__FINAL

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionOnBicycle::GetNMBlendOutClip(fwMvClipSetId& outClipSet, fwMvClipId& outClip)
{
	const CVehicleSeatAnimInfo* pSeatClipInfo = m_pVehicle->GetSeatAnimationInfo(GetPed());
	if (pSeatClipInfo)
	{
		outClipSet = pSeatClipInfo->GetSeatClipSetId();
		outClip = CLIP_IDLE;
	}
}

////////////////////////////////////////////////////////////////////////////////

Vec3V_Out CTaskMotionOnBicycle::CalcDesiredVelocity(Mat34V_ConstRef UNUSED_PARAM(updatedPedMatrix),float UNUSED_PARAM(fTimestep))
{
	return VECTOR3_TO_VEC3V(GetPed()->GetVelocity());
}

////////////////////////////////////////////////////////////////////////////////

CTask * CTaskMotionOnBicycle::CreatePlayerControlTask()
{
	taskFatalAssertf(GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_MOTION_IN_VEHICLE, "Invalid parent task");
	return static_cast<CTaskMotionInVehicle*>(GetParent())->CreatePlayerControlTask();
}

////////////////////////////////////////////////////////////////////////////////

// Statics
CTaskMotionOnBicycleController::Tunables CTaskMotionOnBicycleController::ms_Tunables;

IMPLEMENT_VEHICLE_TASK_TUNABLES(CTaskMotionOnBicycleController, 0xd6304b99);

////////////////////////////////////////////////////////////////////////////////

const fwMvRequestId CTaskMotionOnBicycleController::ms_FreeWheelRequestId("FreeWheeling",0x6259E7EF);
const fwMvRequestId CTaskMotionOnBicycleController::ms_PedalingRequestId("Pedaling",0xB2A40CAB);
const fwMvRequestId CTaskMotionOnBicycleController::ms_PreparingToLaunchRequestId("PreparingToLaunch",0x2A287E98);
const fwMvRequestId CTaskMotionOnBicycleController::ms_LaunchingRequestId("Launching",0x431069E5);
const fwMvRequestId CTaskMotionOnBicycleController::ms_InAirRequestId("InAir",0x121BB840);
const fwMvRequestId CTaskMotionOnBicycleController::ms_TrackStandRequestId("TrackStand",0x8EE322C7);
const fwMvRequestId CTaskMotionOnBicycleController::ms_FixieSkidRequestId("FixieSkid",0xE99FC4AB);
const fwMvRequestId CTaskMotionOnBicycleController::ms_ToTrackStandTransitionRequestId("ToTrackStandTransition",0x5C14595);

const fwMvBooleanId CTaskMotionOnBicycleController::ms_FreeWheelOnEnterId("FreeWheeling_OnEnter",0xBE59DC41);
const fwMvBooleanId CTaskMotionOnBicycleController::ms_PedalingOnEnterId("Pedaling_OnEnter",0xB5A99AC8);
const fwMvBooleanId CTaskMotionOnBicycleController::ms_PreparingToLaunchOnEnterId("PreparingToLaunch_OnEnter",0x47FE04C7);
const fwMvBooleanId CTaskMotionOnBicycleController::ms_LaunchingOnEnterId("Launching_OnEnter",0x1B08242C);
const fwMvBooleanId CTaskMotionOnBicycleController::ms_InAirOnEnterId("InAir_OnEnter",0x73C547B3);
const fwMvBooleanId CTaskMotionOnBicycleController::ms_TrackStandOnEnterId("TrackStand_OnEnter",0x1BE42A9F);
const fwMvBooleanId CTaskMotionOnBicycleController::ms_FixieSkidOnEnterId("FixieSkid_OnEnter",0xC980F9A4);
const fwMvBooleanId CTaskMotionOnBicycleController::ms_ToTrackStandTransitionOnEnterId("ToTrackStandTransition_OnEnter",0x2561F106);

const fwMvBooleanId CTaskMotionOnBicycleController::ms_FreeWheelSitOnEnterId("FreeWheelingSit_OnEnter",0xA1191982);
const fwMvBooleanId CTaskMotionOnBicycleController::ms_PedalingSitOnEnterId("PedalingSit_OnEnter",0xB9639011);
const fwMvBooleanId CTaskMotionOnBicycleController::ms_PreparingToLaunchSitOnEnterId("PreparingToLaunchSit_OnEnter",0xB6CA57A0);
const fwMvBooleanId CTaskMotionOnBicycleController::ms_LaunchingSitOnEnterId("LaunchingSit_OnEnter",0x48838980);
const fwMvBooleanId CTaskMotionOnBicycleController::ms_InAirOnSitEnterId("InAirSit_OnEnter",0xF7A17F91);
const fwMvBooleanId CTaskMotionOnBicycleController::ms_TrackStandSitOnEnterId("TrackStandSit_OnEnter",0xAF41AB3E);
const fwMvBooleanId CTaskMotionOnBicycleController::ms_FixieSkidSitOnEnterId("FixieSkidSit_OnEnter",0xFAC51582);

const fwMvBooleanId CTaskMotionOnBicycleController::ms_PrepareToLaunchId("PrepareToLaunch",0x1872F180);
const fwMvBooleanId CTaskMotionOnBicycleController::ms_PrepareToLaunchRId("PrepareToLaunchR",0xF29629E5);
const fwMvBooleanId CTaskMotionOnBicycleController::ms_PrepareToTrackStandId("PrepareToTrackStand",0x5B51B6B9);
const fwMvBooleanId CTaskMotionOnBicycleController::ms_PrepareToTrackStandRId("PrepareToTrackStandR",0xCE06038A);
const fwMvBooleanId CTaskMotionOnBicycleController::ms_ToTrackStandTransitionBlendOutId("ToTrackStandTransitionBlendOut",0xEC21023C);

const fwMvBooleanId CTaskMotionOnBicycleController::ms_FreeWheelBreakoutId("freewheel_Breakout",0x3952F3F6);
const fwMvBooleanId CTaskMotionOnBicycleController::ms_LaunchId("Launch",0x3C50EB34);
const fwMvBooleanId CTaskMotionOnBicycleController::ms_UseDefaultPedalRateId("UseDefaultPedalRate",0xBAE29A40);
const fwMvBooleanId CTaskMotionOnBicycleController::ms_CanReverseCruisePedalToGetToFreeWheel("CanReverseCruisePedalToGetToFreeWheel",0x15E1B80E);

const fwMvFloatId CTaskMotionOnBicycleController::ms_PedalRateId("PedalRate",0xE9CBE2A0);
const fwMvFloatId CTaskMotionOnBicycleController::ms_InvalidBlendDurationId("Invalid",0x8451F94F);
const fwMvFloatId CTaskMotionOnBicycleController::ms_PedalToFreewheelBlendDurationId("PedalToFreewheelBlendDuration",0x1074ACC);
const fwMvFloatId CTaskMotionOnBicycleController::ms_FreewheelToPedalBlendDurationId("FreewheelToPedalBlendDuration",0xA64B54D);
const fwMvClipId  CTaskMotionOnBicycleController::ms_CruisePedalClip("CruisePedalClip",0xDB6EE652);
const fwMvClipId  CTaskMotionOnBicycleController::ms_FastPedalClip("FastPedalClip",0xEF1D9849);
const fwMvClipId  CTaskMotionOnBicycleController::ms_DuckPrepClipId("DuckPrepClip",0x17B63354);
const fwMvClipId  CTaskMotionOnBicycleController::ms_LaunchClipId("LaunchClip",0x2BE8250A);
const fwMvClipId  CTaskMotionOnBicycleController::ms_TrackStandClipId("TrackStandClip",0x933322B7);
const fwMvClipId  CTaskMotionOnBicycleController::ms_FixieSkidClip0Id("FixieSkidClip0",0x94726FEF);
const fwMvClipId  CTaskMotionOnBicycleController::ms_FixieSkidClip1Id("FixieSkidClip1",0x22070B1A);
const fwMvClipId  CTaskMotionOnBicycleController::ms_ToTrackStandTransitionClipId("ToTrackStandTransitionClip",0x62BE1388);
const fwMvClipId  CTaskMotionOnBicycleController::ms_InAirFreeWheelClipId("InAirFreeWheelClip",0xD362C5D9);
const fwMvClipId  CTaskMotionOnBicycleController::ms_InAirFreeWheelClip1Id("InAirFreeWheelClip1",0xE919D22D);

const fwMvFloatId CTaskMotionOnBicycleController::ms_CruisePedalPhase("CruisePedalPhase",0x23A66849);
const fwMvFloatId CTaskMotionOnBicycleController::ms_CruisePedalLeftPhase("CruisePedalLeftPhase",0x96916F9B);
const fwMvFloatId CTaskMotionOnBicycleController::ms_CruisePedalRightPhase("CruisePedalRightPhase",0x52729598);
const fwMvFloatId CTaskMotionOnBicycleController::ms_CruiseFreeWheelPhaseId("CruiseFreeWheelPhase",0x3BCA9148);
const fwMvFloatId CTaskMotionOnBicycleController::ms_CruiseFreeWheelLeftPhaseId("CruiseFreeWheelLeftPhase",0x9AC071AB);
const fwMvFloatId CTaskMotionOnBicycleController::ms_CruiseFreeWheelRightPhaseId("CruiseFreeWheelRightPhase",0xB13A1559);
const fwMvFloatId CTaskMotionOnBicycleController::ms_CruisePrepLaunchPhaseId("CruisePrepLaunchPhase",0xB60DFE38);
const fwMvFloatId CTaskMotionOnBicycleController::ms_CruiseLaunchPhaseId("CruiseLaunchPhase",0x7B16D243);
const fwMvFloatId CTaskMotionOnBicycleController::ms_TuckFreeWheelPhaseId("TuckFreeWheelPhase",0xD4A91548);
const fwMvFloatId CTaskMotionOnBicycleController::ms_ToTrackStandTransitionPhaseId("ToTrackStandTransitionPhase",0xB1E224AF);

const fwMvFloatId CTaskMotionOnBicycleController::ms_FastPedalPhase("FastPedalPhase",0x1A205679);
const fwMvFloatId CTaskMotionOnBicycleController::ms_FastPedalPhaseLeft("FastPedalPhaseLeft",0xA0A84DF1);
const fwMvFloatId CTaskMotionOnBicycleController::ms_FastPedalPhaseRight("FastPedalPhaseRight",0x4ED7AC02);
const fwMvFloatId CTaskMotionOnBicycleController::ms_FastFreeWheelPhase("FastFreeWheelPhase",0x720AFBAA);
const fwMvFloatId CTaskMotionOnBicycleController::ms_FastFreeWheelPhaseLeft("FastFreeWheelPhaseLeft",0x39397227);
const fwMvFloatId CTaskMotionOnBicycleController::ms_FastFreeWheelPhaseRight("FastFreeWheelPhaseRight",0x1431CA5B);
const fwMvFloatId CTaskMotionOnBicycleController::ms_FastPrepLaunchPhaseId("FastPrepLaunchPhase",0xBE371124);
const fwMvFloatId CTaskMotionOnBicycleController::ms_FastLaunchPhaseId("FastLaunchPhase",0xED4715BE);

const fwMvFloatId CTaskMotionOnBicycleController::ms_FixieSkidClip0PhaseId("FixieSkidClip0Phase",0x45339817);
const fwMvFloatId CTaskMotionOnBicycleController::ms_FixieSkidClip1PhaseId("FixieSkidClip1Phase",0x39AF5503);
const fwMvFloatId CTaskMotionOnBicycleController::ms_FixieSkidBlendId("FixieSkidBlend",0x6F0F918D);
const fwMvFloatId CTaskMotionOnBicycleController::ms_InitialPedalPhaseId("InitialPedalPhase",0x26186CA2);
const fwMvFloatId CTaskMotionOnBicycleController::ms_FixieSkidToTrackStandPhaseId("FixieSkidToTrackStandPhase",0x1EE82AD3);

const fwMvFlagId CTaskMotionOnBicycleController::ms_HasTuckId("HasTuck",0x9483E458);
const fwMvFlagId CTaskMotionOnBicycleController::ms_HasHandGripId("HasHandGrip",0xe948fee7);

////////////////////////////////////////////////////////////////////////////////

CTaskMotionOnBicycleController::CTaskMotionOnBicycleController(CMoveNetworkHelper& rPedMoveNetworkHelper, CMoveNetworkHelper& rBicycleMoveNetworkHelper, CVehicle* pBicycle, bool bWasFreewheeling)
: m_rPedMoveNetworkHelper(rPedMoveNetworkHelper)
, m_rBicycleMoveNetworkHelper(rBicycleMoveNetworkHelper)
, m_pBicycle(pBicycle)
, m_fOldPedalRate(1.0f)
, m_fTimeSinceReachingLaunchPoint(0.0f)
, m_fCachedJumpInputValidityTime(0.0f)
, m_fFixieSkidBlend(-1.0f)
, m_fTimeBikeStill(0.0f)
, m_bIsFixieBicycle(false)
, m_bShouldReversePedalRate(false)
, m_bCheckForSprintTestPassed(false)
, m_bWasFreewheeling(bWasFreewheeling)
, m_bUseLeftFoot(false)
, m_bMoVE_AreMoveNetworkStatesValid(false)
, m_bMoVE_HaveReachedLaunchPoint(false)
, m_uTimeBicycleLastSentRequest(0)
{
	SetInternalTaskType(CTaskTypes::TASK_MOTION_ON_BICYCLE_CONTROLLER);
}

////////////////////////////////////////////////////////////////////////////////

CTaskMotionOnBicycleController::~CTaskMotionOnBicycleController()
{

}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycleController::ShouldLimitCameraDueToJump() const
{
	return (GetState() == State_PreparingToLaunch || GetState() == State_Launching);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionOnBicycleController::GetNMBlendOutClip(fwMvClipSetId& outClipSet, fwMvClipId& outClip)
{
	const CVehicleSeatAnimInfo* pSeatClipInfo = m_pBicycle->GetSeatAnimationInfo(GetPed());
	if (pSeatClipInfo)
	{
		outClipSet = pSeatClipInfo->GetSeatClipSetId();
		outClip = CLIP_IDLE;
	}
}

////////////////////////////////////////////////////////////////////////////////

CTask * CTaskMotionOnBicycleController::CreatePlayerControlTask()
{
	taskFatalAssertf(GetParent() && GetParent()->GetParent() && GetParent()->GetParent()->GetTaskType() == CTaskTypes::TASK_MOTION_IN_VEHICLE, "Invalid parent task");
	return static_cast<CTaskMotionInVehicle*>(GetParent())->CreatePlayerControlTask();
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycleController::ProcessPreFSM()
{
	if (!m_pBicycle)
		return FSM_Quit;

	if (!taskVerifyf(m_pBicycle->GetLayoutInfo(), "NULL layout info for %s bicycle", m_pBicycle->GetModelName()))
		return FSM_Quit;

	m_pBicycleInfo = m_pBicycle->GetLayoutInfo()->GetBicycleInfo();
	if (!taskVerifyf(m_pBicycleInfo, "NULL bicycle info for %s bicycle", m_pBicycle->GetModelName()))
		return FSM_Quit;

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycleController::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed& rPed = *GetPed();

	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();	

		FSM_State(State_Pedaling)
			FSM_OnEnter
				return Pedaling_OnEnter();
			FSM_OnUpdate
				return Pedaling_OnUpdate();

		FSM_State(State_PedalToFreewheeling)
			FSM_OnEnter
				return PedalToFreewheeling_OnEnter();
			FSM_OnUpdate
				return PedalToFreewheeling_OnUpdate();	

		FSM_State(State_Freewheeling)
			FSM_OnEnter
				return Freewheeling_OnEnter();
			FSM_OnUpdate
				return Freewheeling_OnUpdate();
			FSM_OnExit
				return Freewheeling_OnExit();

		FSM_State(State_PedalToPreparingToLaunch)
			FSM_OnEnter
				return PedalToPreparingToLaunch_OnEnter();
			FSM_OnUpdate
				return PedalToPreparingToLaunch_OnUpdate();	

		FSM_State(State_PedalToTrackStand)
			FSM_OnEnter
				return PedalToTrackStand_OnEnter();
			FSM_OnUpdate
				return PedalToTrackStand_OnUpdate();	

		FSM_State(State_FixieSkid)
			FSM_OnEnter
				return FixieSkid_OnEnter();
			FSM_OnUpdate
				return FixieSkid_OnUpdate();	

		FSM_State(State_ToTrackStandTransition)
			FSM_OnEnter
				return ToTrackStandTransition_OnEnter();
			FSM_OnUpdate
				return ToTrackStandTransition_OnUpdate();	

		FSM_State(State_TrackStand)
			FSM_OnEnter
				return TrackStand_OnEnter();
			FSM_OnUpdate
				return TrackStand_OnUpdate();	

		FSM_State(State_PreparingToLaunch)
			FSM_OnEnter
				return PreparingToLaunch_OnEnter();
			FSM_OnUpdate
				return PreparingToLaunch_OnUpdate();
			FSM_OnExit
				return PreparingToLaunch_OnExit();

		FSM_State(State_Launching)
			FSM_OnEnter
				return Launching_OnEnter();
			FSM_OnUpdate
				return Launching_OnUpdate();

		FSM_State(State_PedalToInAir)
			FSM_OnEnter
				return PedalToInAir_OnEnter();
			FSM_OnUpdate
				return PedalToInAir_OnUpdate();

		FSM_State(State_InAir)
			FSM_OnEnter
				return InAir_OnEnter();
			FSM_OnUpdate
				return InAir_OnUpdate(rPed);
			FSM_OnExit
				return InAir_OnExit();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycleController::ProcessPostFSM()
{
	if (!m_pBicycle)
		return FSM_Quit;

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycleController::ProcessMoveSignals()
{
	if (!m_pBicycle)
		return true;

	// Sync ped move network output phases to bicycle network, ideally this would be automated somehow, as is,
	// every unique anim that both the ped and bicycle need to play in sync needs an individual parameter to be synced
	// TODO: Could we keep a track of whether the pedaling/freewheel move states are completely blended out (via exit events)
	// to prevent the need for sending signals in when not needed?
	SyncronizeFloatParameter(ms_CruisePedalLeftPhase);
	SyncronizeFloatParameter(ms_CruisePedalPhase);
	SyncronizeFloatParameter(ms_CruisePedalRightPhase);
	SyncronizeFloatParameter(ms_CruiseFreeWheelPhaseId);
	SyncronizeFloatParameter(ms_CruiseFreeWheelLeftPhaseId);
	SyncronizeFloatParameter(ms_CruiseFreeWheelRightPhaseId);
	SyncronizeFloatParameter(ms_CruisePrepLaunchPhaseId);
	SyncronizeFloatParameter(ms_CruiseLaunchPhaseId);
	SyncronizeFloatParameter(ms_FastPedalPhase);
	SyncronizeFloatParameter(ms_FastPedalPhaseLeft);
	SyncronizeFloatParameter(ms_FastPedalPhaseRight);
	SyncronizeFloatParameter(ms_FastFreeWheelPhase);
	SyncronizeFloatParameter(ms_FastFreeWheelPhaseLeft);
	SyncronizeFloatParameter(ms_FastFreeWheelPhaseRight);
	SyncronizeFloatParameter(ms_FastPrepLaunchPhaseId);
	SyncronizeFloatParameter(ms_FastLaunchPhaseId);
	SyncronizeFloatParameter(ms_TuckFreeWheelPhaseId);

	if (GetState() == State_FixieSkid || GetState() == State_ToTrackStandTransition || GetState() == State_TrackStand)
	{
		// Send last frame's blend parameter to the bicycle network
		m_rBicycleMoveNetworkHelper.SetFloat(ms_FixieSkidBlendId, m_fFixieSkidBlend);
		// Update the current blend parameter
		ProcessFixieSkidBlend(*m_pBicycle, fwTimer::GetTimeStep());
		// Send new blend parameter to ped network
		m_rPedMoveNetworkHelper.SetFloat(ms_FixieSkidBlendId, m_fFixieSkidBlend);

		SyncronizeFloatParameter(ms_FixieSkidClip0PhaseId);
		SyncronizeFloatParameter(ms_FixieSkidClip1PhaseId);
		SyncronizeFloatParameter(ms_ToTrackStandTransitionPhaseId);
		SyncronizeFloatParameter(ms_FixieSkidToTrackStandPhaseId);
	}

	switch(GetState())
	{
	case State_Start:
		// nil
		break;
	case State_Pedaling:
		Pedaling_OnProcessMoveSignals();
		break;
	case State_PedalToFreewheeling:
		// nil
		break;
	case State_Freewheeling:
		Freewheeling_OnProcessMoveSignals();
		break;
	case State_PedalToPreparingToLaunch:
		// nil
		break;
	case State_PreparingToLaunch:
		PreparingToLaunch_OnProcessMoveSignals();
		break;
	case State_Launching:
		// nil
		break;
	case State_PedalToInAir:
		// nil
		break;
	case State_InAir:
		InAir_OnProcessMoveSignals();
		break;
	case State_PedalToTrackStand:
		// nil
		break;
	case State_FixieSkid:
		FixieSkid_OnProcessMoveSignals();
		break;
	case State_ToTrackStandTransition:
		// nil
		break;
	case State_TrackStand:
		TrackStand_OnProcessMoveSignals();
		break;
	case State_Finish:
		// nil
		break;
	default:
		taskAssertf(false, "Unhandled state (%s) may need OnProcessMoveSignals!", GetStateName(GetState()));
		break;
	}

	// we have MoVE signals to process at all times
	return true;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycleController::Start_OnUpdate()
{
	// Request MoVE signal update call every frame from now on.
	// NOTE: We can allow GetBoolean calls below because we will transition out immediately like an OnEnter case.
	RequestProcessMoveSignalCalls();

	m_bIsFixieBicycle = m_pBicycleInfo->GetIsFixieBike();

	CTaskMotionOnBicycle* pMotionOnBicycleTask = static_cast<CTaskMotionOnBicycle*>(GetParent());
	m_bIsCruising = pMotionOnBicycleTask->GetState() == CTaskMotionOnBicycle::State_Sitting;
	if (pMotionOnBicycleTask->GetWantedToDoSpecialMoveInStillTransition() && 
		(CTaskMotionOnBicycle::CanJump(m_rPedMoveNetworkHelper) || (CTaskMotionOnBicycle::CanTrackStand(m_rPedMoveNetworkHelper) && m_bIsCruising)))
	{
		pMotionOnBicycleTask->SetWantedToDoSpecialMoveInStillTransition(false);
		if (m_rPedMoveNetworkHelper.GetBoolean(ms_PrepareToLaunchId) || 
			m_rPedMoveNetworkHelper.GetBoolean(CTaskMotionOnBicycle::ms_CanTransitionToJumpPrepId) ||
			m_rPedMoveNetworkHelper.GetBoolean(ms_PrepareToTrackStandId))
		{
			m_bUseLeftFoot = true;
		}
		else if (	m_rPedMoveNetworkHelper.GetBoolean(ms_PrepareToLaunchRId) || 
					m_rPedMoveNetworkHelper.GetBoolean(CTaskMotionOnBicycle::ms_CanTransitionToJumpPrepRId) ||
					m_rPedMoveNetworkHelper.GetBoolean(ms_PrepareToTrackStandRId))
		{
			m_bUseLeftFoot = false;
		}
		if (m_pBicycleInfo->GetCanTrackStand())
		{
			SetTaskState(State_TrackStand);
			return FSM_Continue;
		}
		else
		{
			SetTaskState(State_PreparingToLaunch);
			return FSM_Continue;
		}
	}
	else
	{
		// MoVE signal check OK: FreeWheelBreakout inside WantsToFreeWheel() 
		if (m_bWasFreewheeling || WantsToFreeWheel())
		{
			SetTaskState(State_Freewheeling);
			return FSM_Continue;
		}
		else
		{
			SetTaskState(State_Pedaling);
			return FSM_Continue;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycleController::Pedaling_OnEnter()
{
	m_bMoVE_AreMoveNetworkStatesValid = false;
	m_bShouldReversePedalRate = false;

	if (GetPreviousState() != State_PedalToInAir && GetPreviousState() != State_PedalToTrackStand)
	{
		// MoVE signaling OK in OnEnter
		m_rPedMoveNetworkHelper.SetFloat(ms_FreewheelToPedalBlendDurationId, m_pBicycleInfo->GetFreewheelToPedalBlendDuration());
		SetMoveNetworkStates(ms_PedalingRequestId, m_bIsCruising ? ms_PedalingSitOnEnterId : ms_PedalingOnEnterId);
	}
	
	// MoVE signaling OK in OnEnter
	SetInitialPedalPhase();
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycleController::Pedaling_OnUpdate()
{
	if (!m_bMoVE_AreMoveNetworkStatesValid)
		return FSM_Continue;

	if (IsTransitioningToState(State_PedalToPreparingToLaunch)) // no MoVE signals
		return FSM_Continue;

	if (IsTransitioningToState(State_PedalToTrackStand)) // MoVE signal OK: FreeWheelBreakout
		return FSM_Continue;

	if (IsTransitioningToState(State_PedalToInAir)) // no MoVE signals
		return FSM_Continue;

	if (IsTransitioningToState(State_Freewheeling)) // MoVE signal OK: FreeWheelBreakout
		return FSM_Continue;

	if (IsTransitioningToState(State_PedalToFreewheeling)) // MoVE signal OK: CanReverseCruisePedalToGetToFreeWheel
		return FSM_Continue;

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionOnBicycleController::Pedaling_OnProcessMoveSignals()
{
	m_bMoVE_AreMoveNetworkStatesValid = AreMoveNetworkStatesValid(ms_PedalingRequestId, ms_FreewheelToPedalBlendDurationId, m_pBicycleInfo->GetFreewheelToPedalBlendDuration());
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycleController::PedalToFreewheeling_OnEnter()
{
	const float fPedPedalPhase = m_rPedMoveNetworkHelper.GetFloat(m_bIsCruising ? ms_CruisePedalPhase : ms_FastPedalPhase);
	const crClip* pPedPedalClip = m_rPedMoveNetworkHelper.GetClip(m_bIsCruising ? ms_CruisePedalClip : ms_FastPedalClip);
	float fTargetBlendPhase = 1.0f;
	if (pPedPedalClip)
	{
		fTargetBlendPhase = FindTargetBlendPhaseForCriticalTag(*pPedPedalClip, ms_FreeWheelBreakoutId, fPedPedalPhase);
	}
	m_bShouldReversePedalRate = fPedPedalPhase > fTargetBlendPhase;
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycleController::PedalToFreewheeling_OnUpdate()
{
	// we need updates every frame in this state
	GetPed()->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate | CPedAILod::AL_LodTimesliceAnimUpdate);

	if (IsTransitioningToState(State_ToTrackStandTransition)) // MoVE signal OK: FreeWheelBreakout
		return FSM_Continue;

	if (IsTransitioningToState(State_Freewheeling)) // MoVE signal OK: FreeWheelBreakout
		return FSM_Continue;

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycleController::Freewheeling_OnEnter()
{
	m_bMoVE_AreMoveNetworkStatesValid = false;
	m_bCheckForSprintTestPassed = false;
	// MoVE signal OK in OnEnter
	m_rPedMoveNetworkHelper.SetFloat(ms_PedalToFreewheelBlendDurationId, m_pBicycleInfo->GetPedalToFreewheelBlendDuration());
	SetMoveNetworkStates(ms_FreeWheelRequestId, m_bIsCruising ? ms_FreeWheelSitOnEnterId : ms_FreeWheelOnEnterId);
	m_fOldPedalRate = GetPedalRate();
	SetPedalRate(0.0f);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycleController::Freewheeling_OnUpdate()
{
	if (!m_bMoVE_AreMoveNetworkStatesValid)
		return FSM_Continue;

	if (IsTransitioningToState(State_Pedaling)) // no MoVE signals
		return FSM_Continue;

	if (IsTransitioningToState(State_ToTrackStandTransition)) // MoVE signal OK: FreeWheelBreakout
		return FSM_Continue;

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycleController::Freewheeling_OnExit()
{
	SetPedalRate(m_fOldPedalRate);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionOnBicycleController::Freewheeling_OnProcessMoveSignals()
{
	m_bMoVE_AreMoveNetworkStatesValid = AreMoveNetworkStatesValid(ms_FreeWheelRequestId, ms_PedalToFreewheelBlendDurationId, m_pBicycleInfo->GetPedalToFreewheelBlendDuration());
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycleController::PedalToPreparingToLaunch_OnEnter()
{
	ComputeSpecialMoveParams(ST_Jump);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycleController::PedalToPreparingToLaunch_OnUpdate()
{
	// we need updates every frame in this state
	GetPed()->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate | CPedAILod::AL_LodTimesliceAnimUpdate);
	GetPed()->SetPedResetFlag(CPED_RESET_FLAG_DisableSprintDamage, true); // reset pre-task

	if (IsTransitioningToState(State_PreparingToLaunch)) // MoVE signal OK: PrepareToLaunch, PrepareToLaunchR
		return FSM_Continue;

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycleController::PedalToTrackStand_OnEnter()
{
	ComputeSpecialMoveParams(ST_TrackStand);	
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycleController::PedalToTrackStand_OnUpdate()
{
	CPed& rPed = *GetPed();
	// we need updates every frame in this state
	rPed.GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate | CPedAILod::AL_LodTimesliceAnimUpdate);
	rPed.SetPedResetFlag(CPED_RESET_FLAG_DisableSprintDamage, true); // reset pre-task
	rPed.SetPedResetFlag(CPED_RESET_FLAG_PreventBikeFromLeaning, true); // reset pre-task

	if (IsTransitioningToState(State_FixieSkid)) // MoVE signal OK: PrepareToTrackStand, PrepareToTrackStandR
		return FSM_Continue;

	if (IsTransitioningToState(State_TrackStand)) // MoVE signal OK: PrepareToTrackStand, PrepareToTrackStandR, ToTrackStandTransitionBlendOut
		return FSM_Continue;

	if (IsTransitioningToState(State_Pedaling)) // no MoVE signals
		return FSM_Continue;

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycleController::FixieSkid_OnEnter()
{
	m_fFixieSkidBlend = -1.0f;
	m_fOldPedalRate = GetPedalRate();
	m_fTimeBikeStill = 0.0f;
	SetMoveNetworkStates(ms_FixieSkidRequestId, m_bIsCruising ? ms_FixieSkidSitOnEnterId : ms_FixieSkidOnEnterId);
	SetClipsForState(false);
	m_bMoVE_AreMoveNetworkStatesValid = false;
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycleController::FixieSkid_OnUpdate()
{
	CPed& rPed = *GetPed();
	rPed.SetPedResetFlag(CPED_RESET_FLAG_DisableSprintDamage, true); // reset pre-task
	rPed.SetPedResetFlag(CPED_RESET_FLAG_PreventBikeFromLeaning, true); // reset pre-task

	if (!m_bMoVE_AreMoveNetworkStatesValid)
	{
		return FSM_Continue;
	}

	if (IsTransitioningToState(State_Pedaling)) // no MoVE signals
		return FSM_Continue;

	if (IsTransitioningToState(State_ToTrackStandTransition)) // MoVE signal OK: FreeWheelBreakout
		return FSM_Continue;

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionOnBicycleController::FixieSkid_OnProcessMoveSignals()
{
	m_bMoVE_AreMoveNetworkStatesValid = AreMoveNetworkStatesValid(ms_FixieSkidRequestId);
	if( !m_bMoVE_AreMoveNetworkStatesValid )
	{
		SetClipsForState(true);
	}

	Vector3 vBikeVelocity = m_pBicycle->GetVelocityIncludingReferenceFrame();
	if (static_cast<const CBike*>(m_pBicycle.Get())->IsConsideredStill(vBikeVelocity, true))
	{
		m_fTimeBikeStill += fwTimer::GetTimeStep();
	}
	else
	{
		m_fTimeBikeStill = 0.0f;
	}
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycleController::ToTrackStandTransition_OnEnter()
{
	SetMoveNetworkStates(ms_ToTrackStandTransitionRequestId, ms_ToTrackStandTransitionOnEnterId);
	SetClipsForState(false);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycleController::ToTrackStandTransition_OnUpdate()
{
	CPed& rPed = *GetPed();
	// we need updates every frame in this state
	rPed.GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate | CPedAILod::AL_LodTimesliceAnimUpdate);
	rPed.SetPedResetFlag(CPED_RESET_FLAG_DisableSprintDamage, true); // reset pre-task
	rPed.SetPedResetFlag(CPED_RESET_FLAG_PreventBikeFromLeaning, true); // reset pre-task

	if (!AreMoveNetworkStatesValid(ms_ToTrackStandTransitionRequestId))
	{
		SetClipsForState(true);
		return FSM_Continue;
	}

	if (IsTransitioningToState(State_TrackStand)) // MoVE signals OK: ToTrackStandTransitionBlendOut, PrepareToTrackStand, PrepareToTrackStandR
		return FSM_Continue;

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycleController::TrackStand_OnEnter()
{
	m_fOldPedalRate = GetPedalRate();
	SetMoveNetworkStates(ms_TrackStandRequestId, m_bIsCruising ? ms_TrackStandSitOnEnterId : ms_TrackStandOnEnterId);

	// Need to compute the initial pedal phase so we can get ready to go!
	if (m_bIsCruising && GetPreviousState() != State_ToTrackStandTransition)
	{
		ComputeSpecialMoveParams(ST_TrackStand);
	}
	SetClipsForState(false);
	m_bMoVE_AreMoveNetworkStatesValid = false;
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycleController::TrackStand_OnUpdate()
{
	CPed& rPed = *GetPed();
	rPed.SetPedResetFlag(CPED_RESET_FLAG_DisableSprintDamage, true);
	rPed.SetPedResetFlag(CPED_RESET_FLAG_PreventBikeFromLeaning, true);

	if (!m_bMoVE_AreMoveNetworkStatesValid)
	{
		return FSM_Continue;
	}

	if (IsTransitioningToState(State_Pedaling)) // no MoVE signals
		return FSM_Continue;

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionOnBicycleController::TrackStand_OnProcessMoveSignals()
{
	m_bMoVE_AreMoveNetworkStatesValid = AreMoveNetworkStatesValid(ms_TrackStandRequestId);

	if( !m_bMoVE_AreMoveNetworkStatesValid )
	{
		SetClipsForState(true);
	}
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycleController::PreparingToLaunch_OnEnter()
{
	m_fOldPedalRate = GetPedalRate();
	SetPedalRate(0.0f);
	SetMoveNetworkStates(ms_PreparingToLaunchRequestId, m_bIsCruising ? ms_PreparingToLaunchSitOnEnterId : ms_PreparingToLaunchOnEnterId);
	SetClipsForState(false);
	m_bMoVE_AreMoveNetworkStatesValid = false;
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycleController::PreparingToLaunch_OnUpdate()
{
	GetPed()->SetPedResetFlag(CPED_RESET_FLAG_DisableSprintDamage, true);

	if (!m_bMoVE_AreMoveNetworkStatesValid)
		return FSM_Continue;

	if (IsTransitioningToState(State_Launching)) // no MoVE signals
		return FSM_Continue;

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycleController::PreparingToLaunch_OnExit()
{
	SetPedalRate(m_fOldPedalRate);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionOnBicycleController::PreparingToLaunch_OnProcessMoveSignals()
{
	m_bMoVE_AreMoveNetworkStatesValid = AreMoveNetworkStatesValid(ms_PreparingToLaunchRequestId);
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycleController::Launching_OnEnter()
{
	m_fTimeSinceReachingLaunchPoint = -1.0f;
	m_bMoVE_HaveReachedLaunchPoint = false;
	SetMoveNetworkStates(ms_LaunchingRequestId, m_bIsCruising ? ms_LaunchingSitOnEnterId : ms_LaunchingOnEnterId);
	SetClipsForState(false);
	static_cast<CTaskMotionOnBicycle*>(GetParent())->SetCachedJumpInput(false);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycleController::Launching_OnUpdate()
{
	CPed& rPed = *GetPed();
	// we need updates every frame in this state
	rPed.GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate | CPedAILod::AL_LodTimesliceAnimUpdate);
	rPed.SetPedResetFlag(CPED_RESET_FLAG_DisableSprintDamage, true); // reset pre-task

	if (!m_bMoVE_HaveReachedLaunchPoint && m_rPedMoveNetworkHelper.GetBoolean(ms_LaunchId))
	{
		m_bMoVE_HaveReachedLaunchPoint = true;
		m_fTimeSinceReachingLaunchPoint = 0.0f;
		const float fLaunchHeight = rPed.GetTransform().GetPosition().GetZf();
		SetJumpMinHeight(fLaunchHeight);
		SetJumpMaxHeight(fLaunchHeight);
		rPed.SetPedResetFlag(CPED_RESET_FLAG_ShouldLaunchBicycleThisFrame, true); // reset pre-task
		// Allow launch to be interrupted by impact after we've jumped
		static_cast<CTaskMotionOnBicycle*>(GetParent())->SetWasInAir(true);
	}

	if (!AreMoveNetworkStatesValid(ms_LaunchingRequestId))
		return FSM_Continue;

	if (!m_bMoVE_HaveReachedLaunchPoint)
		return FSM_Continue;

	m_fTimeSinceReachingLaunchPoint += GetTimeStep();

	const bool bIgnoreInAirCheck = true;
	if (CTaskMotionOnBicycle::WantsToJump(rPed, *m_pBicycle, bIgnoreInAirCheck))
	{
		static_cast<CTaskMotionOnBicycle*>(GetParent())->SetCachedJumpInput(true);
	}

	if (IsTransitioningToState(State_PreparingToLaunch)) // MoVE signals OK: PrepareToLaunch, PrepareToLaunchR
	{
		m_bUseLeftFoot = true;
		return FSM_Continue;
	}

	if (IsTransitioningToState(State_InAir)) // MoVE signals OK: PrepareToLaunch
		return FSM_Continue;

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycleController::PedalToInAir_OnEnter()
{
	const float fPedPedalPhase = m_rPedMoveNetworkHelper.GetFloat(m_bIsCruising ? ms_CruisePedalPhase : ms_FastPedalPhase);
	const crClip* pPedPedalClip = m_rPedMoveNetworkHelper.GetClip(m_bIsCruising ? ms_CruisePedalClip : ms_FastPedalClip);
	float fTargetBlendPhase = 1.0f;
	if (pPedPedalClip)
	{
		fTargetBlendPhase = FindTargetBlendPhaseForCriticalTag(*pPedPedalClip, ms_PrepareToLaunchId, fPedPedalPhase);
	}

	CPed& rPed = *GetPed();
	const float fLaunchHeight = rPed.GetTransform().GetPosition().GetZf();
	SetJumpMinHeight(fLaunchHeight);
	SetJumpMaxHeight(fLaunchHeight);
	//m_bShouldReversePedalRate = fPedPedalPhase > fTargetBlendPhase;
	static_cast<CTaskMotionOnBicycle*>(GetParent())->SetWasInAir(true);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycleController::PedalToInAir_OnUpdate()
{
	// we need updates every frame in this state
	GetPed()->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate | CPedAILod::AL_LodTimesliceAnimUpdate);
	if (IsTransitioningToState(State_InAir)) // MoVE signal OK: PrepareToLaunch
		return FSM_Continue;

	if (IsTransitioningToState(State_Pedaling)) // no MoVE signals
	{
		static_cast<CTaskMotionOnBicycle*>(GetParent())->SetWasInAir(false);
		return FSM_Continue;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycleController::InAir_OnEnter()
{
	m_bMoVE_AreMoveNetworkStatesValid = false;
	m_fOldPedalRate = GetPedalRate();
	SetPedalRate(0.0f);
	SetMoveNetworkStates(ms_InAirRequestId, m_bIsCruising ? ms_InAirOnSitEnterId : ms_InAirOnEnterId);
	SetClipsForState(false);
	static_cast<CTaskMotionOnBicycle*>(GetParent())->SetWasInAir(true);
	if (GetPreviousState() == State_Launching)
	{
		static_cast<CTaskMotionOnBicycle*>(GetParent())->SetDownHillBlend(0.0f);
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycleController::InAir_OnUpdate(const CPed& rPed)
{
	const bool bIgnoreInAirCheck = true;
	if (CTaskMotionOnBicycle::WantsToJump(rPed, *m_pBicycle, bIgnoreInAirCheck))
	{
		static_cast<CTaskMotionOnBicycle*>(GetParent())->SetCachedJumpInput(true);
		m_fCachedJumpInputValidityTime = 0.0f;
	}
	else
	{
		m_fCachedJumpInputValidityTime += GetTimeStep();
	}

	const float fCurrentHeight = rPed.GetTransform().GetPosition().GetZf();
	if (fCurrentHeight > GetJumpMaxHeight())
	{
		SetJumpMaxHeight(fCurrentHeight);
	}
	else if (fCurrentHeight < GetJumpMinHeight())
	{
		SetJumpMinHeight(fCurrentHeight);
	}

	if (!m_bMoVE_AreMoveNetworkStatesValid)
	{
		return FSM_Continue;
	}

	// We rely on the parent task playing the impact anim to take us to the standing pose and restart this task.
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskMotionOnBicycleController::InAir_OnExit()
{
	if (GetParent() && static_cast<CTaskMotionOnBicycle*>(GetParent())->GetCachedJumpInput())
	{
		TUNE_GROUP_FLOAT(BICYCLE_TUNE, MAX_IN_AIR_JUMP_INPUT_VALID_TIME, 0.25f, 0.0f, 2.0f, 0.01f);
		if (m_fCachedJumpInputValidityTime > MAX_IN_AIR_JUMP_INPUT_VALID_TIME)
		{
			static_cast<CTaskMotionOnBicycle*>(GetParent())->SetCachedJumpInput(false);
		}
	}
	SetPedalRate(m_fOldPedalRate);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionOnBicycleController::InAir_OnProcessMoveSignals()
{
	m_bMoVE_AreMoveNetworkStatesValid = AreMoveNetworkStatesValid(ms_InAirRequestId);
	
	if( !m_bMoVE_AreMoveNetworkStatesValid )
	{
		SetClipsForState(true);
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycleController::CheckForStateTransition(eOnBicycleControllerState eState)
{
	switch (eState)
	{
		case State_Pedaling:					return CheckForPedalingTransition();
		case State_Freewheeling:				return CheckForFreewheelingTransition();
		case State_PedalToFreewheeling: 		return CheckForPedalToFreewheelingTransition();
		case State_PreparingToLaunch:			return CheckForPreparingToLaunchTransition();
		case State_PedalToPreparingToLaunch:	return CheckForPedalToPreparingToLaunchTransition();
		case State_PedalToTrackStand:			return CheckForPedalToTrackStandTransition();
		case State_Launching:					return CheckForLaunchingTransition();
		case State_PedalToInAir:				return CheckForPedalToInAirTransition();
		case State_InAir:						return CheckForInAirTransition();
		case State_TrackStand:					return CheckForTrackStandTransition();
		case State_FixieSkid:					return CheckForFixieSkidTransition();
		case State_ToTrackStandTransition:		return CheckForToTrackStandTransition();
		default: break;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycleController::CheckForPedalingTransition()
{
	const bool bIsInAir = m_pBicycle->IsInAir();
	if (GetState() == State_PedalToInAir)
	{
		if (!bIsInAir)
		{
			return true;
		}
	}

	if (GetTimeInState() < ms_Tunables.m_MinTimeInFreewheelState)
		return false;

	if (bIsInAir)
		return true;

	// Need to pedal to get to the launch point
	if (CheckForPedalToPreparingToLaunchTransition())
		return true;

	if (GetState() == State_PedalToTrackStand && !m_bIsFixieBicycle)
	{
		if (!CheckForNonFixiePedalTransition())
		{
			return true;
		}
	}

	const CPed& rPed = *GetPed();

	float fSprintResult = CTaskMotionOnBicycle::GetSprintResultFromPed(rPed, *m_pBicycle);

	bool bWantsToPedal = fSprintResult >= 1.0f;

	if (CTaskMotionOnBicycle::WantsToTrackStand(rPed, *m_pBicycle) && !bWantsToPedal)
	{
		return false;
	}

	if (GetState() == State_FixieSkid && !CheckForFixieSkidTransition())
	{
		const float fMaxSpeedToTriggerTransition = ms_Tunables.m_MaxSpeedToTriggerFixieSkidTransition;
		if (m_pBicycle->GetVelocityIncludingReferenceFrame().XYMag2() > square(fMaxSpeedToTriggerTransition))
		{
			if (GetTimeInState() > ms_Tunables.m_MinTimeInStateToAllowTransitionFromFixieSkid)
			{
				return true;
			}
		}
	}

	// If we don't want to pedal as we're not trying to move then don't transition
	if (!bWantsToPedal)
	{
		m_bCheckForSprintTestPassed = false;
		return false;
	}
	
	// If we're cruising we must discount the first button press since we don't know if we want to sprint
	if (m_bIsCruising && GetState() != State_TrackStand && GetState() != State_FixieSkid && GetState() != State_ToTrackStandTransition)
	{
		if (!m_bCheckForSprintTestPassed)
		{
			m_bCheckForSprintTestPassed = true;
		}
		else
		{
			bWantsToPedal = false;
		}
	}

	return bWantsToPedal;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycleController::CheckForFreewheelingTransition() const
{
	if (m_pBicycleInfo->GetIsFixieBike())
		return false;

	const bool bAtFreeWheelBreakout = m_rPedMoveNetworkHelper.GetBoolean(ms_FreeWheelBreakoutId);

	switch (GetState())
	{
		case State_PedalToFreewheeling:
			{
				return bAtFreeWheelBreakout;
			}
		default:
			{
				return WantsToFreeWheel();
			}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycleController::CheckForPedalToFreewheelingTransition() const
{
	if (GetTimeInState() < ms_Tunables.m_MinTimeInPedalState)
		return false;

	// Fixie bikes can only pedal
	if (m_pBicycleInfo->GetIsFixieBike())
		return false;

	// We're already going at speed so let it cycle, looks odd and glitchy reversing
	if (!m_bIsCruising)
		return false;

	// Always try to freewheel when doing a driveby
	const CPed& rPed = *GetPed();
	const bool bCanCycleWhilstDrivebying = static_cast<const CTaskMotionOnBicycle*>(GetParent())->UseUpperBodyDriveby();
	if (!bCanCycleWhilstDrivebying && rPed.GetPedResetFlag(CPED_RESET_FLAG_IsDoingDriveby))
		return true;

	// Only allow reversing if within tag boundary
	if (!m_rPedMoveNetworkHelper.GetBoolean(ms_CanReverseCruisePedalToGetToFreeWheel))
		return false;

	// Don't transition if we were sprinting recently (maybe tapping sprint button)
	if (CTaskMotionOnBicycle::WasSprintingRecently(*m_pBicycle))
		return false;

	// Don't transition if we want to stand
	float fSprintResult = CTaskMotionOnBicycle::GetSprintResultFromPed(rPed, *m_pBicycle);
	if (fSprintResult >= 1.0f)
	{
		return false;
	}		

	if (GetState() == State_Pedaling)
	{
		const Vector3 vVehVelocity = m_pBicycle->GetVelocityIncludingReferenceFrame();
		if (m_pBicycleInfo->GetCanTrackStand())
		{
			if (vVehVelocity.XYMag2() <= square(ms_Tunables.m_MaxSpeedToTriggerTrackStandTransition))
			{
				return true;
			}
			return false;
		}
		else
		{
			if (vVehVelocity.Mag2() > 0.0f)
			{
				return false;
			}
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycleController::CheckForPreparingToLaunchTransition() const
{
	if (GetState() == State_Launching)
	{
		const bool bIgnoreInAirCheck = false;
		if (m_bMoVE_HaveReachedLaunchPoint && CTaskMotionOnBicycle::WantsToJump(*GetPed(), *m_pBicycle, bIgnoreInAirCheck))
		{
			TUNE_GROUP_FLOAT(BICYCLE_TUNE, MIN_TIME_SINCE_REACHING_LAUNCH_POINT_TO_ALLOW_JUMP_PREP, 0.6f, 0.0f, 5.0f, 0.01f);
			if (m_fTimeSinceReachingLaunchPoint > MIN_TIME_SINCE_REACHING_LAUNCH_POINT_TO_ALLOW_JUMP_PREP)
			{
				return true;
			}
		}
		return false;
	}

	if (m_bUseLeftFoot)
	{
		if (m_rPedMoveNetworkHelper.GetBoolean(ms_PrepareToLaunchId))
		{
			return true;
		}
	}
	else
	{
		if (m_rPedMoveNetworkHelper.GetBoolean(ms_PrepareToLaunchRId))
		{
			return true;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycleController::CheckForPedalToPreparingToLaunchTransition() const
{
	const CPed& rPed = *GetPed();
	const CVehicle& rVeh = *m_pBicycle;
	const bool bIgnoreInAirCheck = false;
	if (CTaskMotionOnBicycle::WantsToJump(rPed, rVeh, bIgnoreInAirCheck))
	{
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycleController::CheckForPedalToTrackStandTransition() const
{
	const CPed& rPed = *GetPed();
	const CVehicle& rVeh = *m_pBicycle;
	if (CTaskMotionOnBicycle::WantsToTrackStand(rPed, rVeh))
	{
		if (m_bIsFixieBicycle)
		{
			return true;
		}
		else if (GetState() == State_Pedaling) 
		{
			if (!WantsToFreeWheel(true, true))
			{
				return false;
			}

			if (m_pBicycle->GetVelocityIncludingReferenceFrame().XYMag2() > square(ms_Tunables.m_MaxSpeedToTriggerTrackStandTransition))
			{
				return false;
			}
			return true;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycleController::CheckForLaunchingTransition() const
{
	TUNE_GROUP_FLOAT(BICYCLE_TUNE, MIN_TIME_IN_PREP_STATE, 0.0f, 0.0f, 2.0f, 0.01f);
	if (GetTimeInState() > MIN_TIME_IN_PREP_STATE)
	{
		const CPed& rPed = *GetPed();
		// Don't allow bunny hop when exiting
		if (rPed.GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle))
		{
			return false;
		}

		const CVehicle& rVeh = *m_pBicycle;
		const bool bIgnoreInAirCheck = false;
		//B*1724498: Don't allow bunny hop when being jacked
		if (!CTaskMotionOnBicycle::WantsToJump(rPed, rVeh, bIgnoreInAirCheck) && !rPed.GetPedResetFlag(CPED_RESET_FLAG_BeingJacked))
		{
			return true;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycleController::CheckForPedalToInAirTransition() const
{
	if (m_pBicycle->IsInAir())
	{
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycleController::CheckForInAirTransition() const
{
	if (!m_pBicycleInfo->GetHasImpactAnims())
	{
		return false;
	}

	if (GetState() == State_PedalToInAir)
	{
		return m_rPedMoveNetworkHelper.GetBoolean(ms_PrepareToLaunchId);
	}
	else
	{
		TUNE_GROUP_FLOAT(BICYCLE_TUNE, MIN_TIME_IN_LAUNCH_STATE_FOR_IN_AIR, 0.7f, 0.0f, 2.0f, 0.01f);
		if (GetTimeInState() < MIN_TIME_IN_LAUNCH_STATE_FOR_IN_AIR)
		{
			return false;
		}

		TUNE_GROUP_FLOAT(BICYCLE_TUNE, MAX_TIME_IN_LAUNCH_STATE_FOR_IN_AIR, 0.9f, 0.0f, 2.0f, 0.01f);
		if (GetTimeInState() > MAX_TIME_IN_LAUNCH_STATE_FOR_IN_AIR)
		{
			return true;
		}
	}
	return m_pBicycle->IsInAir();
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycleController::CheckForTrackStandTransition() const
{
	if (!m_pBicycleInfo->GetCanTrackStand())
	{
		return false;
	}

	if (GetState() == State_ToTrackStandTransition)
	{
		return m_rPedMoveNetworkHelper.GetBoolean(ms_ToTrackStandTransitionBlendOutId);
	}

	if (!CTaskMotionOnBicycle::CanTrackStand(m_rPedMoveNetworkHelper))
	{
		return false;
	}

	if (m_pBicycle->IsInAir())
	{
		return false;
	}

	if (m_pBicycle->GetVelocityIncludingReferenceFrame().XYMag2() > square(ms_Tunables.m_MaxSpeedToTriggerTrackStandTransition))
	{
		return false;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycleController::CheckForFixieSkidTransition() const
{
	if (!m_bIsFixieBicycle)
	{
		return false;
	}

	if (!m_pBicycleInfo->GetCanTrackStand())
	{
		return false;
	}

	if (!CTaskMotionOnBicycle::CanTrackStand(m_rPedMoveNetworkHelper))
	{
		return false;
	}

	if (m_pBicycle->IsInAir())
	{
		return false;
	}

	TUNE_GROUP_FLOAT(TRACK_STAND_TUNE, MAX_SPEED_FOR_FIXIE_SKID, 3.0f, 0.0f, 10.0f, 0.1f);
	if (m_pBicycle->GetVelocityIncludingReferenceFrame().XYMag2() <= square(MAX_SPEED_FOR_FIXIE_SKID))
	{
		return false;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycleController::CheckForNonFixiePedalTransition() const
{
	if (m_bIsFixieBicycle)
	{
		return false;
	}

	if (!m_pBicycleInfo->GetCanTrackStand())
	{
		return false;
	}

	if (m_pBicycle->IsInAir())
	{
		return false;
	}

	if (m_pBicycle->GetVelocityIncludingReferenceFrame().XYMag2() > square(ms_Tunables.m_MaxSpeedToTriggerTrackStandTransition))
	{
		return false;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycleController::CheckForToTrackStandTransition() const
{
	if (!m_pBicycleInfo->GetCanTrackStand())
	{
		return false;
	}

	const bool bIsInPedalToFreeWheel = GetState() == State_PedalToFreewheeling;
	if (GetState() == State_Freewheeling || bIsInPedalToFreeWheel)
	{
		if (m_bIsFixieBicycle)
		{
			return false;
		}

		const CPed& rPed = *GetPed();
		if (!CTaskMotionOnBicycle::WantsToTrackStand(rPed, *m_pBicycle))
		{
			return false;
		}

		if (bIsInPedalToFreeWheel && !m_rPedMoveNetworkHelper.GetBoolean(ms_FreeWheelBreakoutId))
		{
			return false;
		}

		if (m_pBicycle->GetVelocityIncludingReferenceFrame().XYMag2() > square(ms_Tunables.m_MaxSpeedToTriggerTrackStandTransition))
		{
			return false;
		}
		return true;
	}
	else 
	{
		const float fMaxSpeedToTriggerTransition = m_bIsFixieBicycle ? ms_Tunables.m_MaxSpeedToTriggerFixieSkidTransition : ms_Tunables.m_MaxSpeedToTriggerTrackStandTransition;
		if (m_pBicycle->GetVelocityIncludingReferenceFrame().XYMag2() > square(fMaxSpeedToTriggerTransition))
		{
			return false;
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycleController::WantsToFreeWheel(bool bIgnoreVelocityTest, bool bIgnoreAtFreeWheelCheck) const
{
	if (m_bIsFixieBicycle)
	{
		return false;
	}

	const CPed& rPed = *GetPed();
	if (rPed.GetPedResetFlag(CPED_RESET_FLAG_IsDoingDriveby) && !static_cast<const CTaskMotionOnBicycle*>(GetParent())->UseUpperBodyDriveby())
	{
		return true;
	}

	const bool bAtFreeWheelBreakout = bIgnoreAtFreeWheelCheck || m_rPedMoveNetworkHelper.GetBoolean(ms_FreeWheelBreakoutId);

	if (m_pBicycle->IsInAir())
		return bAtFreeWheelBreakout;

	float fSprintResult = CTaskMotionOnBicycle::GetSprintResultFromPed(rPed, *m_pBicycle, bIgnoreVelocityTest);

	TUNE_GROUP_INT(BICYCLE_TUNE, MIN_TIME_SINCE_SPRINT_PRESSED_TO_FREE_WHEEL, 250, 0, 1000, 10);
	bool bPassedSprintTest = true;

	if (rPed.IsLocalPlayer())
	{
		const CControl* pControl = rPed.GetControlFromPlayer();
		if (pControl && pControl->GetVehiclePushbikePedal().HistoryReleased(MIN_TIME_SINCE_SPRINT_PRESSED_TO_FREE_WHEEL))
		{
			bPassedSprintTest = false;
		}
	}

	if (bPassedSprintTest && bAtFreeWheelBreakout && fSprintResult < 1.0f)
	{
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionOnBicycleController::ProcessFixieSkidBlend(const CVehicle& rVeh, float fTimeStep)
{
	TUNE_GROUP_BOOL(TRACK_STAND_TUNE, USE_OVERRIDE_SKID_BLEND, false);
	if (USE_OVERRIDE_SKID_BLEND)
	{
		TUNE_GROUP_FLOAT(TRACK_STAND_TUNE, FIXIE_SKID_BLEND_OVERRIDE, 1.0f, 0.0f, 1.0f, 0.01f);
		m_fFixieSkidBlend = FIXIE_SKID_BLEND_OVERRIDE;
		return;
	}

	TUNE_GROUP_FLOAT(TRACK_STAND_TUNE, FIXIE_SKID_BLEND_SPEED, 1.0f, 0.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(TRACK_STAND_TUNE, FIXIE_SKID_MIN_SPEED, 3.0f, 0.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(TRACK_STAND_TUNE, FIXIE_SKID_MAX_SPEED, 15.0f, 0.0f, 20.0f, 0.01f);
	const float fXYSpeed = rVeh.GetVelocityIncludingReferenceFrame().XYMag();
	float fDesiredBlendNorm = Clamp((fXYSpeed - FIXIE_SKID_MIN_SPEED) / (FIXIE_SKID_MAX_SPEED - FIXIE_SKID_MIN_SPEED), 0.0f, 1.0f);
	if (m_fFixieSkidBlend < 0.0f)
	{
		m_fFixieSkidBlend = fDesiredBlendNorm;
	}
	else
	{
		rage::Approach(m_fFixieSkidBlend, fDesiredBlendNorm, FIXIE_SKID_BLEND_SPEED, fTimeStep);	
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionOnBicycleController::ComputeSpecialMoveParams(eSpecialMoveType eMoveType)
{
	m_fTargetPedalPhase = -1.0f;
	const float fPedPedalPhase = Clamp(m_rPedMoveNetworkHelper.GetFloat(m_bIsCruising ? ms_CruisePedalPhase : ms_FastPedalPhase), 0.0f, 1.0f);
	const crClip* pPedPedalClip = m_rPedMoveNetworkHelper.GetClip(m_bIsCruising ? ms_CruisePedalClip : ms_FastPedalClip);

	// This is a bit hacky, when track standing from the still to sit transition we won't know the target pedal phase
	if (!pPedPedalClip && m_bIsCruising)
	{
		const fwMvClipSetId clipsetId = m_rPedMoveNetworkHelper.GetClipSetId();
		pPedPedalClip = fwClipSetManager::GetClip(clipsetId, ms_Tunables.m_CruisePedalCharClipId);
	}

	float fTargetBlendPhase = 1.0f;
	if (pPedPedalClip)
	{
		fwMvBooleanId leftSpecialMoveTag;
		fwMvBooleanId rightSpecialMoveTag;
		switch (eMoveType)
		{
			case ST_Jump: 
				{
					leftSpecialMoveTag = ms_PrepareToLaunchId; 
					rightSpecialMoveTag = ms_PrepareToLaunchRId;
				}
				break;
			case ST_TrackStand: 
				{
					leftSpecialMoveTag = ms_PrepareToTrackStandId; 
					rightSpecialMoveTag = ms_PrepareToTrackStandRId;
				}
				break;
			default:
				taskAssertf(0, "Unhandled special move type %i", (s32)eMoveType);
		}

		
		const float fClosestLeftSpecialMovePhase = FindTargetBlendPhaseForCriticalTag(*pPedPedalClip, leftSpecialMoveTag, fPedPedalPhase);
		const float fClosestRightSpecialMovePhase = FindTargetBlendPhaseForCriticalTag(*pPedPedalClip, rightSpecialMoveTag, fPedPedalPhase);
		
		// Fixie bikes only go forwards
		if (m_bIsFixieBicycle)
		{
			m_bUseLeftFoot = fClosestLeftSpecialMovePhase < fPedPedalPhase ? true : false;
			m_bShouldReversePedalRate = false;
			m_fTargetPedalPhase = m_bUseLeftFoot ? fClosestLeftSpecialMovePhase : fClosestRightSpecialMovePhase;
		}
		else
		{
			m_bUseLeftFoot = false;
			const float fLeftPhaseDelta = fClosestLeftSpecialMovePhase - fPedPedalPhase;
			const float fRightPhaseDelta = fClosestRightSpecialMovePhase - fPedPedalPhase;
			if (Abs(fLeftPhaseDelta) < Abs(fRightPhaseDelta))
			{
				m_bUseLeftFoot = true;
			}
			m_fTargetPedalPhase = m_bUseLeftFoot ? fClosestLeftSpecialMovePhase : fClosestRightSpecialMovePhase;
			m_bShouldReversePedalRate = fPedPedalPhase > fTargetBlendPhase;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

float CTaskMotionOnBicycleController::FindTargetBlendPhaseForCriticalTag(const crClip& rClip, fwMvBooleanId criticalMoveTagId, float fCurrentPhase)
{
	float fTargetPhase = 1.0f;
	float fClosestPhaseDelta = 1.0f;

	if (rClip.HasTags())
	{
		CClipEventTags::CTagIteratorAttributeValue<crTag, crPropertyAttributeHashString, atHashString> it(*rClip.GetTags(), 0.0f, 1.0f, CClipEventTags::MoveEvent, CClipEventTags::MoveEvent, criticalMoveTagId);

		while (*it)
		{
			const float fPhase = (*it)->GetMid();
			const float fPhaseDelta = fPhase - fCurrentPhase;
			if (Abs(fPhaseDelta) < Abs(fClosestPhaseDelta))
			{
				fClosestPhaseDelta = fPhaseDelta;
				fTargetPhase = fPhase;
			}
			++it;
		}
	}
	return fTargetPhase;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycleController::SetClipsForState(bool bForBicycle)
{
	const fwMvClipSetId clipsetId = m_rPedMoveNetworkHelper.GetClipSetId();
	fwMvClipId clipId = CLIP_ID_INVALID;
	fwMvClipId clip1Id = CLIP_ID_INVALID;
	fwMvClipId moveClipId = CLIP_ID_INVALID;
	fwMvClipId moveClip1Id = CLIP_ID_INVALID;
	bool bValidState = false;

	switch (GetState())
	{	
		case State_PreparingToLaunch:
		{
			moveClipId = ms_DuckPrepClipId;
			moveClip1Id = CLIP_ID_INVALID;

			if (m_bIsCruising)
			{
				if (m_bUseLeftFoot)
				{
					clipId = bForBicycle ? ms_Tunables.m_CruiseDuckPrepLeftBikeClipId : ms_Tunables.m_CruiseDuckPrepLeftCharClipId;
				}
				else
				{
					clipId = bForBicycle ? ms_Tunables.m_CruiseDuckPrepRightBikeClipId : ms_Tunables.m_CruiseDuckPrepRightCharClipId;
				}
			}
			else
			{
				if (m_bUseLeftFoot)
				{
					clipId = bForBicycle ? ms_Tunables.m_FastDuckPrepLeftBikeClipId : ms_Tunables.m_FastDuckPrepLeftCharClipId;
				}
				else
				{
					clipId = bForBicycle ? ms_Tunables.m_FastDuckPrepRightBikeClipId : ms_Tunables.m_FastDuckPrepRightCharClipId;
				}
			}
			bValidState = true;
			break;
		}
		case State_Launching:
		{
			moveClipId = ms_LaunchClipId;

			if (m_bUseLeftFoot)
			{
				clipId = bForBicycle ? ms_Tunables.m_LaunchLeftBikeClipId : ms_Tunables.m_LaunchLeftCharClipId;
			}
			else
			{
				clipId = bForBicycle ? ms_Tunables.m_LaunchRightBikeClipId : ms_Tunables.m_LaunchRightCharClipId;
			}

			bValidState = true;
			break;
		}
		case State_TrackStand:
			{
				moveClipId = ms_TrackStandClipId;

				if (m_bUseLeftFoot)
				{
					clipId = bForBicycle ? ms_Tunables.m_TrackStandLeftBikeClipId : ms_Tunables.m_TrackStandLeftCharClipId;
				}
				else
				{
					clipId = bForBicycle ? ms_Tunables.m_TrackStandRightBikeClipId : ms_Tunables.m_TrackStandRightCharClipId;
				}

				bValidState = true;
				break;
			}
		case State_FixieSkid:
			{
				moveClipId = ms_FixieSkidClip0Id;
				moveClip1Id = ms_FixieSkidClip1Id;

				if (m_bUseLeftFoot)
				{
					clipId	= bForBicycle ? ms_Tunables.m_FixieSkidLeftBikeClip0Id : ms_Tunables.m_FixieSkidLeftCharClip0Id;
					clip1Id = bForBicycle ? ms_Tunables.m_FixieSkidLeftBikeClip1Id : ms_Tunables.m_FixieSkidLeftCharClip1Id;
				}
				else
				{
					clipId	= bForBicycle ? ms_Tunables.m_FixieSkidRightBikeClip0Id : ms_Tunables.m_FixieSkidRightCharClip0Id;
					clip1Id = bForBicycle ? ms_Tunables.m_FixieSkidRightBikeClip1Id : ms_Tunables.m_FixieSkidRightCharClip1Id;
				}

				bValidState = true;
				break;
			}
		case State_ToTrackStandTransition:
			{
				moveClipId = ms_ToTrackStandTransitionClipId;

				if (!m_bIsFixieBicycle)
				{
					clipId = bForBicycle ? ms_Tunables.m_TuckFreeWheelToTrackStandRightBikeClipId : ms_Tunables.m_TuckFreeWheelToTrackStandRightCharClipId;
					// Temp hack as these anims still haven't made it into the clip and I want to check in
					const crClip* pClip = fwClipSetManager::GetClip(clipsetId, clipId);
					if (!pClip)
					{
						clipId = bForBicycle ? ms_Tunables.m_TrackStandRightBikeClipId : ms_Tunables.m_TrackStandRightCharClipId;
					}
				}
				else
				{
					if (m_bUseLeftFoot)
					{
						clipId = bForBicycle ? ms_Tunables.m_FixieSkidToBalanceLeftBikeClip1Id : ms_Tunables.m_FixieSkidToBalanceLeftCharClip1Id;
					}
					else
					{
						clipId = bForBicycle ? ms_Tunables.m_FixieSkidToBalanceRightBikeClip1Id : ms_Tunables.m_FixieSkidToBalanceRightCharClip1Id;
					}
				}

				bValidState = true;
				break;
			}
		case State_InAir:
			{
				moveClipId = ms_InAirFreeWheelClipId;
				moveClip1Id = ms_InAirFreeWheelClip1Id;
				clipId = bForBicycle ? ms_Tunables.m_InAirFreeWheelBikeClipId : ms_Tunables.m_InAirFreeWheelCharClipId;
				clip1Id = bForBicycle ? CLIP_ID_INVALID : ms_Tunables.m_DownHillInAirFreeWheelCharClipId;
				bValidState = true;
				break;
			}
		default:
		{
			return false;
		}
	}

	if (bValidState)
	{
		const crClip* pClip = fwClipSetManager::GetClip(clipsetId, clipId);
		if (bForBicycle)
		{
			taskAssertf(pClip, "Couldn't find clip %s from clipset %s", clipId.GetCStr(), clipsetId.GetCStr());
			m_rBicycleMoveNetworkHelper.SetClip(pClip, moveClipId);
			if (moveClip1Id != CLIP_ID_INVALID && clip1Id != CLIP_ID_INVALID)
			{
				const crClip* pClip1 = fwClipSetManager::GetClip(clipsetId, clip1Id);
				taskAssertf(pClip1, "Couldn't find clip %s from clipset %s", clip1Id.GetCStr(), clipsetId.GetCStr());
				m_rBicycleMoveNetworkHelper.SetClip(pClip1, moveClip1Id);
			}
		}
		else
		{
			taskAssertf(pClip, "Couldn't find clip %s from clipset %s", clipId.GetCStr(), clipsetId.GetCStr());
			m_rPedMoveNetworkHelper.SetClip(pClip, moveClipId);
			if (moveClip1Id != CLIP_ID_INVALID && clip1Id != CLIP_ID_INVALID)
			{
				const crClip* pClip1 = fwClipSetManager::GetClip(clipsetId, clip1Id);
				taskAssertf(pClip1, "Couldn't find clip %s from clipset %s", clip1Id.GetCStr(), clipsetId.GetCStr());
				m_rPedMoveNetworkHelper.SetClip(pClip1, moveClip1Id);
			}
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionOnBicycleController::SetInitialPedalPhase()
{
	if (GetPreviousState() == State_TrackStand || GetPreviousState() == State_FixieSkid)
	{
		if (m_fTargetPedalPhase >= 0.0f)
		{
			// MoVE SetFloat called once in OnEnter
			m_rPedMoveNetworkHelper.SetFloat(ms_InitialPedalPhaseId, m_fTargetPedalPhase);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionOnBicycleController::SetTaskState(eOnBicycleControllerState eState)
{
#if !__FINAL
	BANK_ONLY(taskDebugf2("Frame : %u - %s%s : 0x%p : changed state from %s:%s:0x%p to %s", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone ped : " : "Local ped : ", GetPed()->GetDebugName(), GetPed() , GetTaskName(), GetStateName(GetState()), this, GetStateName((s32)eState)));
#endif // !__FINAL
	
	SetState((s32)eState);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskMotionOnBicycleController::SetMoveNetworkStates(const fwMvRequestId& rRequestId, const fwMvBooleanId& rOnEnterId)
{
#if __BANK
	taskDebugf2("Frame : %u - %s%s sent request %s from %s waiting for on enter event %s", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone ped : " : "Local ped : ", GetPed()->GetDebugName(), rRequestId.GetCStr(), GetStateName(GetState()), rOnEnterId.GetCStr());
#endif // __BANK

	m_rPedMoveNetworkHelper.SendRequest(rRequestId);
	m_rPedMoveNetworkHelper.WaitForTargetState(rOnEnterId);
	m_rBicycleMoveNetworkHelper.WaitForTargetState(rOnEnterId);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskMotionOnBicycleController::AreMoveNetworkStatesValid(fwMvRequestId requestId, fwMvFloatId blendDurationId, float fBlendDuration)
{
	const bool bPedInTargetState = m_rPedMoveNetworkHelper.IsInTargetState();
	const bool bBicycleInTargetState = m_rBicycleMoveNetworkHelper.IsInTargetState();

#if __BANK
	const CPed& rPed = *GetPed();
	if (!bPedInTargetState)
	{
		taskDebugf2("Frame : %u - %s%s is not in anim target state and in %s", fwTimer::GetFrameCount(), rPed.IsNetworkClone() ? "Clone ped : " : "Local ped : ", rPed.GetDebugName(), GetStateName(GetState()));
	}
	if (!bBicycleInTargetState)
	{
		taskDebugf2("Frame : %u - bicycle is not in anim target state and in %s", fwTimer::GetFrameCount(), GetStateName(GetState()));
	}
#endif // __BANK

	// We do not send the bicycle request until the ped is in the target state because we will get the bicycle pose update that same frame due to
	// the instant anim update
	if (!bPedInTargetState)
	{
		return false;
	}

	if (!bBicycleInTargetState)
	{
		m_rBicycleMoveNetworkHelper.SetFloat(blendDurationId, fBlendDuration);
		m_rBicycleMoveNetworkHelper.SendRequest(requestId);
		m_uTimeBicycleLastSentRequest = fwTimer::GetTimeInMilliseconds();
		taskDebugf2("Frame : %u - BicycleController - m_bBicycleRequestSentThisFrame %u", fwTimer::GetFrameCount(), m_uTimeBicycleLastSentRequest);
		SetClipsForState(true);
	}

	return bPedInTargetState && bBicycleInTargetState;
}

////////////////////////////////////////////////////////////////////////////////

// Statics
CBikeLeanAngleHelper::Tunables CBikeLeanAngleHelper::ms_Tunables;

IMPLEMENT_VEHICLE_TASK_TUNABLES(CBikeLeanAngleHelper, 0x7bcbb781);

////////////////////////////////////////////////////////////////////////////////

CBikeLeanAngleHelper::CBikeLeanAngleHelper(CVehicle* pVehicle, CPed* pPed)
: m_pVehicle(pVehicle)
, m_pPed(pPed)
, m_fLeanAngle(0.5f)
, m_fLeanAngleRate(ms_Tunables.m_LeanAngleDefaultRate)
, m_fPreviousFramesLeanAngle(0.5f)
, m_iPreviousState(State_Start)
, m_fDesiredLeanAngle(0.5f)
, m_iReturnToIdleState(State_ReturnFinish)
, m_fTimeSinceLeaningExtreme(1.0f)
{

}

////////////////////////////////////////////////////////////////////////////////

CBikeLeanAngleHelper::~CBikeLeanAngleHelper()
{

}

////////////////////////////////////////////////////////////////////////////////

CTaskHelperFSM::FSM_Return CBikeLeanAngleHelper::ProcessPreFSM()
{
	if (!m_pPed || !m_pVehicle)
	{
		return FSM_Quit;
	}

	m_fPreviousFramesLeanAngle = m_fLeanAngle;

	const bool bShouldUseCustomPOVSettings = CTaskMotionInAutomobile::ShouldUseCustomPOVSettings(*m_pPed, *m_pVehicle);

	if (m_iReturnToIdleState == State_ReturnFinish)
	{
		float fDesiredLeanAngle = 0.5f;
		CVehicleModelInfo* pVehicleModelInfo = m_pVehicle->GetVehicleModelInfo();
		CHandlingData* pHandlingData = CHandlingDataMgr::GetHandlingData(pVehicleModelInfo->GetHandlingId());
		
		if (m_pVehicle->InheritsFromBike())
		{
			fDesiredLeanAngle = CTaskMotionInAutomobile::ComputeLeanAngleSignal(*m_pVehicle);

			if (bShouldUseCustomPOVSettings)
			{
				fDesiredLeanAngle = m_pVehicle->GetPOVTuningInfo()->ComputeRestrictedDesiredLeanAngle(fDesiredLeanAngle);
			}

			if (fDesiredLeanAngle < ms_Tunables.m_DesiredLeanAngleTolToBringLegIn || fDesiredLeanAngle > (1.0f - ms_Tunables.m_DesiredLeanAngleTolToBringLegIn))
			{
				float fSpeed = m_pVehicle->GetVelocityIncludingReferenceFrame().Mag();
				fSpeed /= pHandlingData->m_fEstimatedMaxFlatVel; 
				fSpeed *= 0.5f;
				fSpeed = rage::Clamp(fSpeed, 0.0f, 1.0f); 
				if (fSpeed < ms_Tunables.m_DesiredSpeedToBringLegIn)
				{
					fDesiredLeanAngle = 0.5f;
				}
			}
		}
		else
		{
			// Quads don't lean as much so just use the steering angle for the lean
			fDesiredLeanAngle = m_pVehicle->GetSteerAngle();
			taskAssert(pHandlingData->m_fSteeringLock != 0.0f);
			fDesiredLeanAngle /= pHandlingData->m_fSteeringLock;
			fDesiredLeanAngle = (fDesiredLeanAngle*0.5f)+0.5f;
			fDesiredLeanAngle = rage::Clamp(fDesiredLeanAngle, 0.0f, 1.0f);
			// Invert
			fDesiredLeanAngle = 1.0f - fDesiredLeanAngle;

			if (bShouldUseCustomPOVSettings)
			{
				fDesiredLeanAngle = m_pVehicle->GetPOVTuningInfo()->ComputeRestrictedDesiredLeanAngle(fDesiredLeanAngle);
			}
		}
		
		float fLerpRate = m_pVehicle->InheritsFromBike() ? ms_Tunables.m_DesiredLeanAngleRate : ms_Tunables.m_DesiredLeanAngleRateQuad;
		if (bShouldUseCustomPOVSettings)
		{
			TUNE_GROUP_FLOAT(FIRST_PERSON_VEHICLE_TUNE, BODY_LEAN_X_SIGNAL_LERP_RATE_SLOW, 0.2f, 0.0f, 2.0f, 0.01f);
			TUNE_GROUP_FLOAT(FIRST_PERSON_VEHICLE_TUNE, BODY_LEAN_X_SIGNAL_LERP_RATE_FAST, 1.5f, 0.0f, 2.0f, 0.01f);
			const float fSpeed = Clamp(m_pVehicle->GetVelocityIncludingReferenceFrame().Mag() / 30.0f, 0.0f, 1.0f);
			const float fSpeedRatio = rage::Clamp(fSpeed / CTaskMotionInAutomobile::ms_Tunables.m_SlowFastSpeedThreshold, 0.0f, 1.0f);
			fLerpRate = (1.0f - fSpeedRatio) * BODY_LEAN_X_SIGNAL_LERP_RATE_SLOW + fSpeedRatio * BODY_LEAN_X_SIGNAL_LERP_RATE_FAST;
		}
		CTaskMotionInVehicle::ComputeBikeDesiredLeanAngleSignal(*m_pVehicle,  m_fDesiredLeanAngle, fDesiredLeanAngle, fLerpRate);
	}

	if (GetState() != State_LeaningExtremeWithStickInput)
	{
		m_fTimeSinceLeaningExtreme += fwTimer::GetTimeStep();
	}
	else
	{
		m_fTimeSinceLeaningExtreme = 0.0f;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskHelperFSM::FSM_Return CBikeLeanAngleHelper::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();	

		FSM_State(State_NoStickInput)
			FSM_OnEnter
				return NoStickInput_OnEnter();
			FSM_OnUpdate
				return NoStickInput_OnUpdate();

		FSM_State(State_HasStickInput)
			FSM_OnUpdate
				return HasStickInput_OnUpdate();

		FSM_State(State_LeaningExtremeWithStickInput)
			FSM_OnUpdate
				return LeaningExtremeWithStickInput_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

CTaskHelperFSM::FSM_Return CBikeLeanAngleHelper::Start_OnUpdate()
{
	if (HasStickInput())
	{
		m_iPreviousState = GetState();
		SetState(State_HasStickInput);
		return FSM_Continue;
	}
	else
	{
		m_iPreviousState = GetState();
		SetState(State_NoStickInput);
		return FSM_Continue;
	}
}

////////////////////////////////////////////////////////////////////////////////

CTaskHelperFSM::FSM_Return CBikeLeanAngleHelper::NoStickInput_OnEnter()
{
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskHelperFSM::FSM_Return CBikeLeanAngleHelper::NoStickInput_OnUpdate()
{
	if (HasStickInput())
	{
		//Displayf("Switched To Has Stick Input State");
		m_iPreviousState = GetState();
		if (ms_Tunables.m_UseReturnOvershoot)
			m_iReturnToIdleState = State_ReturnFinish;
		SetState(State_HasStickInput);
		return FSM_Continue;
	}
	else
	{
		if (ms_Tunables.m_UseReturnOvershoot && m_iPreviousState == State_LeaningExtremeWithStickInput)
		{
			ProcessReturnToIdleLean();
		}
		else
		{
			ProcessLeanAngleDefault();
		}
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskHelperFSM::FSM_Return CBikeLeanAngleHelper::HasStickInput_OnUpdate()
{
	if (!HasStickInput())
	{
		//Displayf("Switched To Has No Stick Input State");
		m_iPreviousState = GetState();
		SetState(State_NoStickInput);
		return FSM_Continue;
	}
	else
	{
		if (IsLeaningExtreme())
		{
			//Displayf("Switched To Leaning Extreme State");
			m_iPreviousState = GetState();
			SetState(State_LeaningExtremeWithStickInput);
			return FSM_Continue;
		}
		else
		{
			ProcessLeanAngleDefault();
		}
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskHelperFSM::FSM_Return CBikeLeanAngleHelper::LeaningExtremeWithStickInput_OnUpdate()
{
	if (!HasStickInput())
	{
		//Displayf("Switched To Has No Stick Input State");
		if (ms_Tunables.m_UseReturnOvershoot)
			m_iReturnToIdleState = State_ReturnInitial;
		m_iPreviousState = GetState();
		SetState(State_NoStickInput);
		return FSM_Continue;
	}
	else
	{
		ProcessLeanAngleDefault();
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CBikeLeanAngleHelper::ProcessLeanAngleDefault()
{
	if (m_iReturnToIdleState != State_ReturnFinish)
	{
		m_fLeanAngleRate = ms_Tunables.m_LeanAngleReturnRate;
	}
	else
	{
		const bool bDriver = m_pVehicle->GetDriver() == m_pPed ? true : false;
		TUNE_GROUP_FLOAT(FIRST_PERSON_VEHICLE_TUNE, LEAN_ANGLE_DEFAULT_RATE, 1.5f, 0.0f, 2.0f, 0.01f);
		const float fDriverLeanAngleDefaultRate = CTaskMotionInAutomobile::ShouldUseCustomPOVSettings(*m_pPed, *m_pVehicle) ? LEAN_ANGLE_DEFAULT_RATE : ms_Tunables.m_LeanAngleDefaultRate;
		m_fLeanAngleRate = bDriver ? fDriverLeanAngleDefaultRate : ms_Tunables.m_LeanAngleDefaultRatePassenger;
	}

	CTaskMotionInVehicle::ComputeBikeLeanAngleSignalNew(*m_pVehicle, m_fLeanAngle, m_fDesiredLeanAngle, m_fLeanAngleRate);
}

////////////////////////////////////////////////////////////////////////////////

void CBikeLeanAngleHelper::ProcessReturnToIdleLean()
{
	switch (m_iReturnToIdleState)
	{
		// Intentional fall through so ProcessLeanAngleDefault gets called
		case State_ReturnInitial:
		{
			if (m_fLeanAngle < 0.5f)
			{
				m_fDesiredLeanAngle = 1.0f - ms_Tunables.m_DesiredOvershootLeanAngle;
			}
			else
			{
				m_fDesiredLeanAngle = ms_Tunables.m_DesiredOvershootLeanAngle;
			}
			//Displayf("State_ReturnInitial : DesiredLeanAngle %.2f", m_fDesiredLeanAngle);
			m_iReturnToIdleState = State_Returning;
		}
		case State_Returning:
			{
				if (m_fLeanAngle > (m_fDesiredLeanAngle - ms_Tunables.m_LeanAngleReturnedTol) && m_fLeanAngle < (m_fDesiredLeanAngle + ms_Tunables.m_LeanAngleReturnedTol))
				{
					m_iReturnToIdleState = State_ReturnFinish;
				}
				//Displayf("State_Returning : DesiredLeanAngle %.2f", m_fDesiredLeanAngle);
			}
		case State_ReturnFinish:
		{
			//Displayf("State_ReturnFinish : DesiredLeanAngle %.2f", m_fDesiredLeanAngle);
			ProcessLeanAngleDefault();
			break;
		}
		default:
			taskAssertf(0, "Unhandled State");
			break;
	}
}

////////////////////////////////////////////////////////////////////////////////

void CBikeLeanAngleHelper::ProcessInitialLean()
{
	m_fDesiredLeanAngle = 0.5f;
	ProcessLeanAngleDefault();
}

////////////////////////////////////////////////////////////////////////////////

bool CBikeLeanAngleHelper::HasStickInput() const
{
	if (m_pPed)
	{
		CPed* pDriver = m_pVehicle->GetDriver();
		if (pDriver)
		{
			if (pDriver->IsLocalPlayer() && m_pPed == pDriver)
			{
				const CControl* pControl = pDriver->GetControlFromPlayer();
				if (pControl)
				{
					float fLeftRight = pControl->GetVehicleSteeringLeftRight().GetNorm();
					if(m_pVehicle->InheritsFromBike())
					{
						float fStickY = 0.0f;
						static_cast<CBike*>(m_pVehicle.Get())->GetBikeLeanInputs(fLeftRight, fStickY);
					}
					//Displayf("LeftRight Val : %.2f", fLeftRight);
					if (Abs(fLeftRight) > ms_Tunables.m_HasStickInputThreshold)
					{
						return true;
					}
				}
			}
			else
			{
				CTaskMotionInAutomobile* pMotionTask = static_cast<CTaskMotionInAutomobile*>(pDriver->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_MOTION_IN_AUTOMOBILE));
				if (pMotionTask && pMotionTask->GetState() != CTaskMotionInAutomobile::State_Still)
				{
					return true;
				}
			}
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool  CBikeLeanAngleHelper::IsLeaningExtreme() const
{
	//Displayf("Lean Angle : %.2f", m_fLeanAngle);
	if (m_fLeanAngle < ms_Tunables.m_LeaningExtremeThreshold || m_fLeanAngle > (1.0f - ms_Tunables.m_LeaningExtremeThreshold))
	{
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

