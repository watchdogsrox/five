//
// camera/gameplay/aim/ThirdPersonVehicleAimCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/gameplay/aim/ThirdPersonVehicleAimCamera.h"

#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/system/CameraMetadata.h"
#include "peds/Ped.h"
#include "peds/PedWeapons/PedTargetEvaluator.h"
#include "vehicles/Metadata/VehicleLayoutInfo.h"
#include "vehicles/Metadata/VehicleSeatInfo.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camThirdPersonVehicleAimCamera,0x953F1771)


camThirdPersonVehicleAimCamera::camThirdPersonVehicleAimCamera(const camBaseObjectMetadata& metadata)
: camThirdPersonAimCamera(metadata)
, m_Metadata(static_cast<const camThirdPersonVehicleAimCameraMetadata&>(metadata))
{
}

bool camThirdPersonVehicleAimCamera::Update()
{
	//Determine the attach vehicle.
	const CVehicle* attachVehicle = camInterface::GetGameplayDirector().GetFollowVehicle();
	if(!attachVehicle)
	{
		return false;
	}

	if(m_AttachParent.Get() != attachVehicle)
	{
		//We don't have a attach vehicle or the player was warped into another vehicle, so change the attach vehicle.
		AttachTo(attachVehicle);
	}

	const bool hasSucceeded = camThirdPersonAimCamera::Update();

	return hasSucceeded;
}

void camThirdPersonVehicleAimCamera::ComputeOrbitRelativePivotOffsetInternal(Vector3& orbitRelativeOffset) const
{
	camThirdPersonAimCamera::ComputeOrbitRelativePivotOffsetInternal(orbitRelativeOffset);

	//If the ped is holding on to the sides of a vehicle e.g. firetruck or swat vehicles we use additional side offsets

	int seatIndex = camInterface::FindFollowPed()->GetAttachCarSeatIndex(); 
	if(seatIndex < 0)
	{
		return;
	}

	//NOTE: GetFollowVehicle() has already been verified.
	const CVehicle* attachVehicle					= camInterface::GetGameplayDirector().GetFollowVehicle();
	const CVehicleLayoutInfo* attachVehicleLayout	= attachVehicle->GetLayoutInfo();
	const CVehicleSeatAnimInfo* animSeatInfo		= attachVehicleLayout->GetSeatAnimationInfo(seatIndex);
	if (!animSeatInfo || !animSeatInfo->GetKeepCollisionOnWhenInVehicle())
	{
		return;
	}

	const CVehicleModelInfo* vehicleModelInfo	= attachVehicle->GetVehicleModelInfo();
	const CModelSeatInfo* seatInfo				= vehicleModelInfo ? vehicleModelInfo->GetModelSeatInfo() : NULL;
	if (!seatInfo)
	{
		return;
	}
	
	const s32 boneIndex = seatInfo->GetBoneIndexFromSeat(seatIndex);
	if (boneIndex < 0)
	{
		return;
	}

	const crBoneData* boneData = attachVehicle->GetSkeletonData().GetBoneData(boneIndex);
	if (!boneData)
	{
		return;
	}

	const bool isHangingOnLeftSide	= boneData->GetDefaultTranslation().GetXf() < 0.0f;
	orbitRelativeOffset.x			+= isHangingOnLeftSide ? m_Metadata.m_ExtraSideOffsetForHangingOnLeftSide : m_Metadata.m_ExtraSideOffsetForHangingOnRightSide;
}

bool camThirdPersonVehicleAimCamera::ComputeShouldUseLockOnAiming() const
{
	bool shouldUseLockOnAiming = camThirdPersonAimCamera::ComputeShouldUseLockOnAiming();
	if(shouldUseLockOnAiming)
	{
		const CPed* followPed = camInterface::FindFollowPed();
		if(followPed)
		{
			const bool isFollowPedDriver	= followPed->GetIsDrivingVehicle();
			shouldUseLockOnAiming			= isFollowPedDriver ? m_Metadata.m_ShouldUseLockOnAimingForDriver :
												m_Metadata.m_ShouldUseLockOnAimingForPassenger;

			u32 nTargetMode = CPedTargetEvaluator::GetTargetMode(followPed);
			if (isFollowPedDriver && nTargetMode != CPedTargetEvaluator::TARGETING_OPTION_FREEAIM)
			{
				shouldUseLockOnAiming = false;
			}
		}
		else
		{
			shouldUseLockOnAiming = false;
		}
	}

	return shouldUseLockOnAiming;
}
