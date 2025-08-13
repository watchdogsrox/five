//
// camera/helpers/switch/ShortTranslationSwitchHelper.cpp
// 
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#include "camera/helpers/switch/ShortTranslationSwitchHelper.h"

#include "fwmaths/angle.h"
#include "math/simplemath.h"

#include "camera/system/CameraFactory.h"
#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/system/CameraMetadata.h"
#include "peds/ped.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camShortTranslationSwitchHelper,0x5B73611F)


camShortTranslationSwitchHelper::camShortTranslationSwitchHelper(const camBaseObjectMetadata& metadata)
: camBaseSwitchHelper(metadata)
, m_Metadata(static_cast<const camShortTranslationSwitchHelperMetadata&>(metadata))
, m_DesiredExtraOrbitRelativeOffset(VEC3_ZERO)
{
}

void camShortTranslationSwitchHelper::ComputeOrbitDistance(float& orbitDistance) const
{
	if (IsFirstPersonSwitch())
	{
		float maxOrbitDistance = 100.0f;

		const camGameplayDirectorMetadata& gameplayDirectorMetadata = static_cast<const camGameplayDirectorMetadata&>(camInterface::GetGameplayDirector().GetMetadata());

		const camLongSwoopSwitchHelperMetadata* metadata = camFactory::FindObjectMetadata<camLongSwoopSwitchHelperMetadata>(gameplayDirectorMetadata.m_FirstPersonLongInSwitchHelperRef);
		if(metadata)
		{
			maxOrbitDistance = metadata->m_MaxOrbitDistance;
		}

		orbitDistance = MIN(orbitDistance, maxOrbitDistance);
	}
}

void camShortTranslationSwitchHelper::ComputeOrbitOrientation(float UNUSED_PARAM(attachParentHeading), float& orbitHeading, float& orbitPitch) const
{
	if(m_DesiredExtraOrbitRelativeOffset.IsZero() && m_OldPed && m_NewPed && !IsFirstPersonSwitch())
	{
		//Initialise the desired extra orbit-relative offset, based upon the position of the peds relative to the camera orbit orientation.

		const Vector3 oldPedPosition	= VEC3V_TO_VECTOR3(m_OldPed->GetTransform().GetPosition());
		const Vector3 newPedPosition	= VEC3V_TO_VECTOR3(m_NewPed->GetTransform().GetPosition());
		const Vector3 pedDelta			= m_IsIntro ? (newPedPosition - oldPedPosition) : (oldPedPosition - newPedPosition);
		const float pedDistance2		= pedDelta.Mag2();
		if(pedDistance2 >= VERY_SMALL_FLOAT)
		{
			const float pedDistance			= Sqrtf(pedDistance2);
			const Vector3 pedDirection		= pedDelta / pedDistance;

			float pedDeltaHeading;
			float pedDeltaPitch;
			camFrame::ComputeHeadingAndPitchFromFront(pedDirection, pedDeltaHeading, pedDeltaPitch);

			float relativeHeading			= orbitHeading - pedDeltaHeading;
			relativeHeading					= fwAngle::LimitRadianAngle(relativeHeading);
			const float sinRelativeHeading	= Sinf(relativeHeading);

			float relativePitch				= orbitPitch - pedDeltaPitch;
			relativePitch					= fwAngle::LimitRadianAngle(relativePitch);
			const float sinRelativePitch	= Sinf(relativePitch);

			//Limit the desired offsets to half of the distance between the peds. This does not take into account the positions
			//of the third-person cameras attached to each ped, but it's better than nothing.
			const float maxDistanceToConsider	= Min(pedDistance / 2.0f, m_Metadata.m_MaxDistanceToTranslate);
			const float desiredExtraOffsetX		= sinRelativeHeading * maxDistanceToConsider;
			const float desiredExtraOffsetZ		= -sinRelativePitch * maxDistanceToConsider;

			m_DesiredExtraOrbitRelativeOffset.Set(desiredExtraOffsetX, 0.0f, desiredExtraOffsetZ);
		}
	}
}

void camShortTranslationSwitchHelper::ComputeOrbitRelativePivotOffset(Vector3& orbitRelativeOffset) const
{
	orbitRelativeOffset.AddScaled(m_DesiredExtraOrbitRelativeOffset, m_BlendLevel);
}
