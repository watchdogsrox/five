//
// camera/cinematic/shot/camCinematicOnFootFirstPersonIdleShot.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/shot/CinematicOnFootFirstPersonIdleShot.h"
#include "camera/cinematic/camera/tracking/CinematicFirstPersonIdleCamera.h"
#include "Peds/ped.h"

INSTANTIATE_RTTI_CLASS(camCinematicOnFootFirstPersonIdleShot,0xF06723EF)

CAMERA_OPTIMISATIONS()


camCinematicOnFootFirstPersonIdleShot::camCinematicOnFootFirstPersonIdleShot(const camBaseObjectMetadata& metadata)
: camBaseCinematicShot(metadata)
, m_Metadata(static_cast<const camCinematicOnFootFirstPersonIdleShotMetadata&>(metadata))
{
}

void camCinematicOnFootFirstPersonIdleShot::InitCamera()
{
	if(m_AttachEntity)	
	{
		if(m_UpdatedCamera->GetIsClassId(camCinematicFirstPersonIdleCamera::GetStaticClassId()))
		{
			camCinematicFirstPersonIdleCamera* pFirstPersonIdleCinematic = static_cast<camCinematicFirstPersonIdleCamera*>(m_UpdatedCamera.Get()); 

			camFrame initFrame = camInterface::GetGameplayDirector().GetFollowCamera() ?
									camInterface::GetGameplayDirector().GetFollowCamera()->GetFrame() :
									camInterface::GetFrame();
			pFirstPersonIdleCinematic->Init(initFrame, m_AttachEntity);
		}
	}
}

bool camCinematicOnFootFirstPersonIdleShot::CreateAndTestCamera(u32 hash)
{
	// Instead of using the camera hash (which is a dummy value anyway),
	// go through shot's list of valid cameras and select one based on the local player's ped type.
	const CPed* pAttachedPed = NULL;
	if (m_AttachEntity && m_AttachEntity->GetIsTypePed())
	{
		pAttachedPed = static_cast<const CPed*>(m_AttachEntity.Get());
	}

	if (pAttachedPed)
	{
		u32 newCamera = hash; //m_Metadata.m_CameraRef;		// set to default camera
		int cameraCount = m_Metadata.m_Cameras.GetCount();
		if (cameraCount > 0)
		{
			switch (pAttachedPed->GetPedType())
			{
				case PEDTYPE_PLAYER_0:
					newCamera = m_Metadata.m_Cameras[0];			// Michael's idle camera
					break;
				case PEDTYPE_PLAYER_1:
					if (cameraCount > 1)
					{
						newCamera = m_Metadata.m_Cameras[1];		// Franklin's idle camera
					}
					break;
				case PEDTYPE_PLAYER_2:
					if (cameraCount > 2)
					{
						newCamera = m_Metadata.m_Cameras[2];		// Trevor's idle camera
					}
					break;
				default:
					break;
			}
		}

		if (camBaseCinematicShot::CreateAndTestCamera(newCamera))
		{
			return true;
		}
	}

	return false;
}
