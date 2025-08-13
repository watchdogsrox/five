//
// camera/helpers/camLookAheadHelper.cpp
// 
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

#include "camera/helpers/LookAheadHelper.h"

#include "fwsys/timer.h"

#include "camera/system/CameraMetadata.h"

#include "grcore/debugdraw.h"
#include "scene/Physical.h"
#include "vehicles/vehicle.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camLookAheadHelper,0x74ab6150)

#if __BANK
bool camLookAheadHelper::ms_DebugEnableDebugDisplay = false;
#endif // __BANK

camLookAheadHelper::camLookAheadHelper(const camBaseObjectMetadata& metadata)
	: camBaseObject(metadata)
	, m_Metadata(static_cast<const camLookAheadHelperMetadata&>(metadata))
{
}

void camLookAheadHelper::UpdateConsideringLookAtTarget(const CPhysical* lookAtTarget, bool isFirstUpdate, Vector3& lookAheadPositionOffset)
{
	lookAheadPositionOffset.Zero();

	if (!lookAtTarget)
	{
		return;
	}

	const float targetRadius = lookAtTarget->GetBaseModelInfo() ? lookAtTarget->GetBaseModelInfo()->GetBoundingSphereRadius() : lookAtTarget->GetBoundRadius();
	if (IsNearZero(targetRadius))
	{
		return;
	}
	
	const Vector3& lookAtTargetVelocity				= lookAtTarget->GetVelocity();
	Vector3 lookAtTargetFront						= VEC3V_TO_VECTOR3(lookAtTarget->GetTransform().GetForward());
	const float lookAtTargetSpeedToConsider			= lookAtTargetVelocity.Dot(lookAtTargetFront);

	const CVehicle* lookAtVehicle					= lookAtTarget->GetIsTypeVehicle() ? static_cast<const CVehicle*>(lookAtTarget) : NULL;
	//NOTE: We are rolling submarines into the 'boats' category here.
	const bool isLookAtTargetAnAircraftOrBoat		= lookAtVehicle && (lookAtVehicle->GetIsAircraft() || lookAtVehicle->InheritsFromBoat() || lookAtVehicle->InheritsFromSubmarine());

	const float offsetMultiplierAtMinSpeed			= isLookAtTargetAnAircraftOrBoat ? m_Metadata.m_AircraftBoatOffsetMultiplierAtMinSpeed : m_Metadata.m_OffsetMultiplierAtMinSpeed;
	const float offsetMultiplierAtMaxForwardSpeed	= isLookAtTargetAnAircraftOrBoat ? m_Metadata.m_AircraftBoatOffsetMultiplierAtMaxForwardSpeed : m_Metadata.m_OffsetMultiplierAtMaxForwardSpeed;
	const float offsetMultiplierAtMaxReverseSpeed	= isLookAtTargetAnAircraftOrBoat ? m_Metadata.m_AircraftBoatOffsetMultiplierAtMaxReverseSpeed : m_Metadata.m_OffsetMultiplierAtMaxReverseSpeed;

	float desiredLookAheadMultiplier = 0.0f;
	if (m_Metadata.m_ShouldApplyLookAheadInReverse && lookAtTargetSpeedToConsider < -m_Metadata.m_MinSpeed)
	{
		desiredLookAheadMultiplier	= RampValueSafe(lookAtTargetSpeedToConsider,
			m_Metadata.m_MaxReverseSpeed, -m_Metadata.m_MinSpeed, offsetMultiplierAtMaxReverseSpeed, offsetMultiplierAtMinSpeed);
	}
	else
	{
		desiredLookAheadMultiplier	= RampValueSafe(lookAtTargetSpeedToConsider,
			m_Metadata.m_MinSpeed, m_Metadata.m_MaxForwardSpeed, offsetMultiplierAtMinSpeed, offsetMultiplierAtMaxForwardSpeed);
	}

	const float dampedLookAheadMultiplier	= m_Spring.Update(desiredLookAheadMultiplier, isFirstUpdate ? 0.0f : m_Metadata.m_SpringConstant, m_Metadata.m_SpringDampingRatio, true);

	lookAheadPositionOffset					= (lookAtTargetFront * targetRadius * dampedLookAheadMultiplier);

#if __BANK
	if (ms_DebugEnableDebugDisplay)
	{
		const Vector3 lookAtTargetPosition	= VEC3V_TO_VECTOR3(lookAtTarget->GetTransform().GetPosition());

		const Vector3 desiredOffset			= (lookAtTargetFront * targetRadius * desiredLookAheadMultiplier);
		const Vector3 desiredPositionOffset	= lookAtTargetPosition + desiredOffset;
		grcDebugDraw::Sphere(desiredPositionOffset, targetRadius / 2.0f, Color_cyan);

		const Vector3 finalPositionOffset	= lookAtTargetPosition + lookAheadPositionOffset;
		grcDebugDraw::Sphere(finalPositionOffset, targetRadius / 2.0f, Color_magenta);
	}
#endif // __BANK
}

#if __BANK
void camLookAheadHelper::AddWidgets(bkBank &bank)
{
	bank.PushGroup("Look Ahead helper");
	{
		bank.AddToggle("Render debug display", &ms_DebugEnableDebugDisplay);
	}
	bank.PopGroup(); //Look Ahead helper
}
#endif // __BANK

