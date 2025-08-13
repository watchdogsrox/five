//
// camera/cinematic/shot/camCinematicOnFootSpectatingShot.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/shot/CinematicOnFootSpectatingShot.h"
#include "camera/cinematic/camera/tracking/CinematicPedCloseUpCamera.h"

INSTANTIATE_RTTI_CLASS(camCinematicOnFootSpectatingShot,0xC292C9BF)

CAMERA_OPTIMISATIONS()

camCinematicOnFootSpectatingShot::camCinematicOnFootSpectatingShot(const camBaseObjectMetadata& metadata)
: camBaseCinematicShot(metadata)
, m_Metadata(static_cast<const camCinematicOnFootSpectatingShotMetadata&>(metadata))
{
}

void camCinematicOnFootSpectatingShot::InitCamera()
{
	if(m_LookAtEntity)	
	{
		if(m_UpdatedCamera->GetIsClassId(camCinematicPedCloseUpCamera::GetStaticClassId()))
		{
			m_UpdatedCamera->LookAt(m_LookAtEntity); 
		}
	}
}