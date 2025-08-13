//
// camera/helpers/switch/ShortRotationSwitchHelper.cpp
// 
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#include "camera/helpers/switch/ShortRotationSwitchHelper.h"

#include "fwmaths/angle.h"
#include "math/simplemath.h"

#include "camera/system/CameraFactory.h"
#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/system/CameraMetadata.h"
#include "peds/ped.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camShortRotationSwitchHelper,0xA1145BC4)


camShortRotationSwitchHelper::camShortRotationSwitchHelper(const camBaseObjectMetadata& metadata)
: camBaseSwitchHelper(metadata)
, m_Metadata(static_cast<const camShortRotationSwitchHelperMetadata&>(metadata))
, m_DesiredOrbitHeadingOffset(0.0f)
{
}

void camShortRotationSwitchHelper::ComputeOrbitDistance(float& orbitDistance) const
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

void camShortRotationSwitchHelper::ComputeOrbitOrientation(float UNUSED_PARAM(attachParentHeading), float& orbitHeading, float& UNUSED_PARAM(orbitPitch)) const
{
	if(IsNearZero(m_DesiredOrbitHeadingOffset) && m_OldPed && m_NewPed)
	{
		//Initialise the desired extra orbit heading offset, based upon the position of the peds relative to the camera orbit heading.

		const Vector3 oldPedPosition	= VEC3V_TO_VECTOR3(m_OldPed->GetTransform().GetPosition());
		const Vector3 newPedPosition	= VEC3V_TO_VECTOR3(m_NewPed->GetTransform().GetPosition());
		const Vector3 pedDelta			= m_IsIntro ? (newPedPosition - oldPedPosition) : (oldPedPosition - newPedPosition);
		const float pedDistance2		= pedDelta.Mag2();
		if(pedDistance2 >= VERY_SMALL_FLOAT)
		{
			const float pedDistance			= Sqrtf(pedDistance2);
			const Vector3 pedDirection		= pedDelta / pedDistance;
			const float pedDeltaHeading		= camFrame::ComputeHeadingFromFront(pedDirection);

			float relativeHeading			= orbitHeading - pedDeltaHeading;
			relativeHeading					= fwAngle::LimitRadianAngle(relativeHeading);
			float cosRelativeHeading		= Cosf(relativeHeading);
			float signScaling				= (relativeHeading >= 0.0f) ? 1.0f : -1.0f;

			//Ensure that we rotate in the direction that was determined upon initiating the Switch, as we require consistency between intro and outro.
			const float directionSignToConsider = m_IsIntro ? m_DirectionSign : -m_DirectionSign;
			if(!IsClose(signScaling, directionSignToConsider))
			{
				//We were going to rotate the wrong way, so reverse the direction and use the maximum angle.
				signScaling			*= -1.0f;
				cosRelativeHeading	= 1.0f;
			}

			m_DesiredOrbitHeadingOffset		= cosRelativeHeading * signScaling * m_Metadata.m_MaxAngleToRotate * DtoR;
		}
	}

	const float headingOffsetToApply	= m_DesiredOrbitHeadingOffset * m_BlendLevel;
	orbitHeading						+= headingOffsetToApply;
	orbitHeading						= fwAngle::LimitDegreeAngle(orbitHeading);
}
