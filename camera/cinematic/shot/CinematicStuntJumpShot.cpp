//
// camera/cinematic/shot/camCinematicStuntJumpShot.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/shot/CinematicStuntJumpShot.h"
#include "camera/cinematic/camera/tracking/CinematicStuntCamera.h"
#include "control/stuntjump.h"

INSTANTIATE_RTTI_CLASS(camCinematicStuntJumpShot,0x72A31038)

CAMERA_OPTIMISATIONS()

camCinematicStuntJumpShot::camCinematicStuntJumpShot(const camBaseObjectMetadata& metadata)
: camBaseCinematicShot(metadata)
, m_Metadata(static_cast<const camCinematicStuntJumpShotMetadata&>(metadata))
{
}

void camCinematicStuntJumpShot::InitCamera()
{
	if(m_LookAtEntity)	
	{
		if(m_UpdatedCamera->GetIsClassId(camCinematicStuntCamera::GetStaticClassId()))
		{
			m_UpdatedCamera->LookAt(m_LookAtEntity); 
			Vector3 camPos(VEC3_ZERO); 
			SStuntJumpManager::GetInstance().GetCameraPositionForStuntJump(camPos); 
			m_UpdatedCamera->GetFrameNonConst().SetPosition(camPos); 
		}
	}
}


