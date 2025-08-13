//
// camera/cinematic/shot/camCinematicOnFootIdleShot.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/shot/CinematicOnFootIdleShot.h"
#include "camera/cinematic/camera/tracking/CinematicIdleCamera.h"
#include "camera/cinematic/CinematicDirector.h"

INSTANTIATE_RTTI_CLASS(camCinematicOnFootIdleShot,0x12BC1DAF)

CAMERA_OPTIMISATIONS()

camCinematicOnFootIdleShot::camCinematicOnFootIdleShot(const camBaseObjectMetadata& metadata)
: camBaseCinematicShot(metadata)
, m_Metadata(static_cast<const camCinematicOnFootIdleShotMetadata&>(metadata))
, m_cameraCreationsForThisShot(0)
{
}

void camCinematicOnFootIdleShot::InitCamera()
{
	if(m_LookAtEntity)	
	{
		if(m_UpdatedCamera && m_UpdatedCamera->GetIsClassId(camCinematicIdleCamera::GetStaticClassId()))
		{
			camCinematicIdleCamera* pIdleCinematic = static_cast<camCinematicIdleCamera*>(m_UpdatedCamera.Get()); 
			
			pIdleCinematic->Init(camInterface::GetGameplayDirector().GetFollowCamera()->GetFrame(), m_LookAtEntity);

			m_cameraCreationsForThisShot++; 
		}
	}
}

void camCinematicOnFootIdleShot::SetUpCamera(camBaseCamera* camera)
{
	if(m_LookAtEntity)	
	{
		if(camera && camera->GetIsClassId(camCinematicIdleCamera::GetStaticClassId()))
		{
			camCinematicIdleCamera* pIdleCinematic = static_cast<camCinematicIdleCamera*>(camera); 

			pIdleCinematic->Init(camera->GetFrame(), m_LookAtEntity);
			
			m_cameraCreationsForThisShot++; 
		}
	}
}


bool camCinematicOnFootIdleShot::CreateAndTestCamera(u32 CameraRef)
{
	if(!m_UpdatedCamera)
	{
		if(camBaseCinematicShot::CreateAndTestCamera(CameraRef))
		{
			return true; 
		}
		return false; 
	}
	else
	{
		if(CanCreate())
		{
			camBaseCamera* Camera = camFactory::CreateObject<camBaseCamera>(CameraRef);
			
			Camera->SetFrame(m_UpdatedCamera->GetFrame()); 

			SetUpCamera(Camera);
			bool isValid = Camera->BaseUpdate(camInterface::GetCinematicDirector().GetFrameNonConst());

			if(isValid)
			{
				delete m_UpdatedCamera; 
				m_UpdatedCamera = Camera; 
			}
			else
			{
				delete Camera; 
			}
			return true;
		}
	}
	return false; 
}

void camCinematicOnFootIdleShot::ClearShot(bool ResetCameraEndTime)
{
	camBaseCinematicShot::ClearShot(ResetCameraEndTime); 
	m_cameraCreationsForThisShot =0; 

}