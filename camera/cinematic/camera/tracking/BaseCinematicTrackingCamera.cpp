//
// camera/cinematic/BaseCinematicTrackingCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "BaseCinematicTrackingCamera.h"

#include "camera/system/CameraMetadata.h"
#include "fwsys/timer.h"
#include "scene/Entity.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camBaseCinematicTrackingCamera,0x5EE61948)

camBaseCinematicTrackingCamera::camBaseCinematicTrackingCamera(const camBaseObjectMetadata& metadata)
: camBaseCamera(metadata)
, m_Metadata(static_cast<const camBaseCinematicTrackingCameraMetadata&>(metadata))
, m_DesiredLookAtPosition(VEC3_ZERO)
, m_ShotStartTime(fwTimer::GetCamTimeInMilliseconds())
{
}

bool camBaseCinematicTrackingCamera::ComputeDesiredLookAtPosition()
{
	if(m_LookAtTarget)
	{	
		m_DesiredLookAtPosition = VEC3V_TO_VECTOR3(m_LookAtTarget->GetTransform().GetPosition()); 

		return true;
	}
	return false; 
}

bool camBaseCinematicTrackingCamera::Update()
{
	return UpdateOrientation(); 
}

bool camBaseCinematicTrackingCamera::UpdateOrientation()
{
	if(ComputeDesiredLookAtPosition())
	{
		Vector3 Look = m_DesiredLookAtPosition - m_Frame.GetPosition();

		m_Frame.SetWorldMatrixFromFront(Look); 

		return true; 
	}
	return false; 
}

