//
// camera/helpers/switch/LongSwoopSwitchHelper.cpp
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//
#include "camera/helpers/switch/LongSwoopSwitchHelper.h"

#include "fwmaths/angle.h"

#include "camera\CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/helpers/ControlHelper.h"
#include "camera/system/CameraMetadata.h"
#include "Peds/ped.h"
#include "vehicles/vehicle.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camLongSwoopSwitchHelper,0xC6D5AEFA)

camLongSwoopSwitchHelper::camLongSwoopSwitchHelper(const camBaseObjectMetadata& metadata)
: camBaseSwitchHelper(metadata)
, m_Metadata(static_cast<const camLongSwoopSwitchHelperMetadata&>(metadata))
{
}

void camLongSwoopSwitchHelper::ComputeOrbitDistance(float& orbitDistance) const
{
	orbitDistance = MIN(orbitDistance, m_Metadata.m_MaxOrbitDistance);
}

void camLongSwoopSwitchHelper::ComputeOrbitOrientation(float attachParentHeading, float& orbitHeading, float& orbitPitch) const
{
	const float desiredOrbitPitch	= m_Metadata.m_DesiredOrbitPitch * DtoR;
	orbitPitch						= Lerp(m_BlendLevel, orbitPitch, desiredOrbitPitch);

	if(m_Metadata.m_ShouldAttainAttachParentHeading)
	{
		//Tend towards the heading of the attach parent at maximum blend.
		orbitHeading = fwAngle::LerpTowards(orbitHeading, attachParentHeading, m_BlendLevel);
	}
}

void camLongSwoopSwitchHelper::ComputeOrbitRelativePivotOffset(Vector3& orbitRelativeOffset) const
{
	//Tend towards zero pivot offset at maximum blend.
	bool bLeaveZComponent = false;

	const CPed* pFollowPed= camInterface::GetGameplayDirector().GetFollowPed();
	if (pFollowPed && pFollowPed->GetIsInVehicle())
	{
		const CVehicle* pVeh = pFollowPed->GetVehiclePedInside();
		if (pVeh && pVeh->GetSeatManager())
		{
			const s32 iSeatindex = pVeh->GetSeatManager()->GetPedsSeatIndex(pFollowPed); 
			if (camGameplayDirector::IsTurretSeat(pVeh, iSeatindex))
			{
				bLeaveZComponent = true;
			}
		}
	}

	const float scalingToApply = (1.0f - m_BlendLevel);
	if (bLeaveZComponent)
	{
		orbitRelativeOffset.x*=scalingToApply;
		orbitRelativeOffset.y*=scalingToApply;
	}
	else
	{
		orbitRelativeOffset.Scale(scalingToApply);
	}
	
}

void camLongSwoopSwitchHelper::ComputeDesiredFov(float& desiredFov) const
{
	//Zoom out slowly at the end of the swoop.

	if(m_Metadata.m_ZoomOutDuration == 0)
	{
		return;
	}

	float zoomPhase	= m_PhaseNonClipped * static_cast<float>(m_Metadata.m_Duration) / static_cast<float>(m_Metadata.m_ZoomOutDuration);
	eCurveType zoomCurveType;
	float desiredZoomFactor;
	float initialZoomFactor;

	if(zoomPhase>1.0f && m_Metadata.m_ZoomOut2Duration>0.0f)
	{
		zoomPhase = ((m_PhaseNonClipped * static_cast<float>(m_Metadata.m_Duration))-static_cast<float>(m_Metadata.m_ZoomOutDuration))/static_cast<float>(m_Metadata.m_ZoomOut2Duration);
		zoomCurveType =  m_Metadata.m_ZoomOut2BlendCurveType;
		desiredZoomFactor = m_Metadata.m_DesiredZoomOut2Factor;
		initialZoomFactor = m_Metadata.m_DesiredZoomOutFactor;
	}
	else
	{
		zoomCurveType = m_Metadata.m_ZoomOutBlendCurveType;
		desiredZoomFactor = m_Metadata.m_DesiredZoomOutFactor;
		initialZoomFactor = m_Metadata.m_InitialZoomOutFactor;
	}
	
	zoomPhase		= Clamp(zoomPhase, 0.0f, 1.0f);

	const float zoomBlendLevel	= ComputeBlendLevel(zoomPhase, zoomCurveType);
	float zoomFactor = Lerp(zoomBlendLevel, initialZoomFactor, desiredZoomFactor);

	if(zoomFactor >= SMALL_FLOAT)
	{
		desiredFov /= zoomFactor;
	}

	desiredFov = Clamp(desiredFov, g_MinFov, g_MaxFov);
}
