//
// camera/cinematic/shot/camCinematicScriptShot.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/shot/CinematicScriptShot.h"
#include "camera/cinematic/camera/tracking/CinematicStuntCamera.h"
#include "camera/cinematic/camera/tracking/CinematicPositionCamera.h"


INSTANTIATE_RTTI_CLASS(camCinematicScriptRaceCheckPointShot, 0xdd041ebc)

CAMERA_OPTIMISATIONS()

camCinematicScriptRaceCheckPointShot::camCinematicScriptRaceCheckPointShot(const camBaseObjectMetadata& metadata)
: camBaseCinematicShot(metadata)
, m_Metadata(static_cast<const camCinematicScriptRaceCheckPointShotMetadata&>(metadata))
{
}

void camCinematicScriptRaceCheckPointShot::InitCamera()
{
	if(m_LookAtEntity)	
	{
		if(m_UpdatedCamera->GetIsClassId(camCinematicPositionCamera::GetStaticClassId()))
		{
			camCinematicPositionCamera* positionCamera = (camCinematicPositionCamera*)m_UpdatedCamera.Get();
			positionCamera->OverrideCamPosition(m_Position); 
			positionCamera->LookAt(m_LookAtEntity); 
		}
	}
}