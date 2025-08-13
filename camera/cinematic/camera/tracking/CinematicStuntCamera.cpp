//
// camera/cinematic/CinematicStuntCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "CinematicStuntCamera.h"

#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/helpers/FrameShaker.h"
#include "camera/gameplay/follow/FollowVehicleCamera.h"
#include "camera/system/CameraMetadata.h"
#include "fwmaths/random.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camCinematicStuntCamera,0x72E965CC)

camCinematicStuntCamera::camCinematicStuntCamera(const camBaseObjectMetadata& metadata)
: camBaseCinematicTrackingCamera(metadata)
, m_Metadata(static_cast<const camCinematicStuntCameraMetadata&>(metadata))
, m_StartFov(45.0f)
, m_EndFov(45.0f)
, m_ZoomInDuration(0.0f)
, m_ZoomHoldDuration(0.0f)
, m_ZoomOutDuration(0.0f)
, m_IsZoomInComplete(false)
, m_IsZoomHoldComplete(false)
, m_ZoomStartTime(fwTimer::GetCamTimeInMilliseconds())
{
	Init(); 
}

bool camCinematicStuntCamera::IsValid()
{
	return true; 
}

bool camCinematicStuntCamera::Update()
{
	if(camBaseCinematicTrackingCamera::Update()) 
	{
		UpdateZoom(); 
		return true;
	}
	return false; 
}

bool camCinematicStuntCamera::ComputeDesiredLookAtPosition()
{
	camFollowVehicleCamera* followVehicleCamera = camInterface::GetGameplayDirector().GetFollowVehicleCamera(); 
	
	if(followVehicleCamera)
	{
		m_DesiredLookAtPosition = followVehicleCamera->GetBaseAttachPosition(); 
		return true;
	}	
	else
	{
		return camBaseCinematicTrackingCamera::ComputeDesiredLookAtPosition();
	}
}

void camCinematicStuntCamera::ComputeFovParams()
{
	float midFovPoint	= ((m_Metadata.m_FovLimits.y - m_Metadata.m_FovLimits.x) / 2.0f) + m_Metadata.m_FovLimits.x; 
	m_StartFov			= fwRandom::GetRandomNumberInRange(midFovPoint, m_Metadata.m_FovLimits.y);
	m_EndFov			= m_Metadata.m_FovLimits.x;

	m_ZoomInDuration	= fwRandom::GetRandomNumberInRange(m_Metadata.m_ZoomInTimeLimits.x, m_Metadata.m_ZoomInTimeLimits.y);
	m_ZoomHoldDuration	= fwRandom::GetRandomNumberInRange(m_Metadata.m_ZoomHoldTimeLimits.x, m_Metadata.m_ZoomHoldTimeLimits.y);
	m_ZoomOutDuration	= fwRandom::GetRandomNumberInRange(m_Metadata.m_ZoomOutTimeLimits.x, m_Metadata.m_ZoomOutTimeLimits.y);
}

void camCinematicStuntCamera::Init()
{
	ComputeFovParams();
}

void camCinematicStuntCamera::UpdateZoom()
{ 	
	float currentZoomTime = (float) (fwTimer::GetCamTimeInMilliseconds() - m_ZoomStartTime); 
	
	if(!m_IsZoomInComplete)
	{
		float zoomT = currentZoomTime / m_ZoomInDuration;
		zoomT = Clamp(zoomT, 0.0f, 1.0f);
		float newFov = Lerp(SlowInOut(zoomT), m_StartFov, m_EndFov);
		m_Frame.SetFov(newFov);
		
		if(IsClose(newFov, m_EndFov))
		{
			m_ZoomStartTime = fwTimer::GetCamTimeInMilliseconds();
			m_IsZoomInComplete = true; 
		}
	}
	else if(!m_IsZoomHoldComplete)
	{
		if(currentZoomTime >= m_ZoomHoldDuration)
		{
			m_ZoomStartTime = fwTimer::GetCamTimeInMilliseconds();
			m_IsZoomHoldComplete = true;
		}
	}
	else
	{
		float zoomT = currentZoomTime / m_ZoomOutDuration;
		zoomT = Clamp(zoomT, 0.0f, 1.0f);
		float newFov = Lerp(SlowInOut(zoomT), m_EndFov , m_StartFov);
		m_Frame.SetFov(newFov);
	}
}
