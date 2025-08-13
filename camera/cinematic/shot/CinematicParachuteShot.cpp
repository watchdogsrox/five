//
// camera/cinematic/shot/camCinematicParachuteShot.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/shot/CinematicParachuteShot.h"

#include "camera/cinematic/camera/tracking/CinematicHeliChaseCamera.h"
#include "camera/cinematic/camera/tracking/CinematicCamManCamera.h"
#include "fwmaths/random.h"

INSTANTIATE_RTTI_CLASS(camCinematicParachuteHeliShot,0xE5B1C52C)
INSTANTIATE_RTTI_CLASS(camCinematicParachuteCameraManShot,0xDC3FD48B)

CAMERA_OPTIMISATIONS()

camCinematicParachuteHeliShot::camCinematicParachuteHeliShot(const camBaseObjectMetadata& metadata)
: camBaseCinematicShot(metadata)
, m_Metadata(static_cast<const camCinematicParachuteHeliShotMetadata&>(metadata))
, m_HeliTrackingAccuracyMode(0)
, m_DesiredTrackingAccuracyMode(0)
{
}

void camCinematicParachuteHeliShot::InitCamera()
{
	if(m_LookAtEntity)	
	{
		if(m_UpdatedCamera->GetIsClassId(camCinematicHeliChaseCamera::GetStaticClassId()))
		{
			m_UpdatedCamera->LookAt(m_LookAtEntity);
			camCinematicHeliChaseCamera* pCam = static_cast<camCinematicHeliChaseCamera*>(m_UpdatedCamera.Get()); 
			pCam->SetMode(m_CurrentMode); 

			const u32 shotTypeOffset = fwRandom::GetRandomNumberInRange( 1, camCinematicHeliChaseCamera::NUM_OF_ACCURACY_MODES);
			m_DesiredTrackingAccuracyMode = ( m_HeliTrackingAccuracyMode + shotTypeOffset ) % camCinematicHeliChaseCamera::NUM_OF_ACCURACY_MODES;

			pCam->SetTrackingAccuracyMode(m_DesiredTrackingAccuracyMode); 
		}
	}
}


void camCinematicParachuteHeliShot::Update()
{
	if(m_UpdatedCamera && m_UpdatedCamera->GetNumUpdatesPerformed() > 0)
	{
		m_HeliTrackingAccuracyMode = m_DesiredTrackingAccuracyMode; 
	}
}


camCinematicParachuteCameraManShot::camCinematicParachuteCameraManShot(const camBaseObjectMetadata& metadata)
: camBaseCinematicShot(metadata)
, m_Metadata(static_cast<const camCinematicParachuteCameraManShotMetadata&>(metadata))
{
}

void camCinematicParachuteCameraManShot::InitCamera()
{
	if(m_LookAtEntity)	
	{
		if(m_UpdatedCamera->GetIsClassId(camCinematicCamManCamera::GetStaticClassId()))
		{
			m_UpdatedCamera->LookAt(m_LookAtEntity);
			((camCinematicCamManCamera*)m_UpdatedCamera.Get())->SetMode(m_CurrentMode);
		}
	}
}
