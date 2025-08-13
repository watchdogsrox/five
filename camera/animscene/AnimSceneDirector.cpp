//
// camera/SynceSceneDirector.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/animscene/AnimSceneDirector.h"


#include "camera/base/baseCamera.h"
#include "camera/cutscene/AnimatedCamera.h"
#include "camera/system/CameraMetadata.h"
#include "camera/system/CameraFactory.h"
#include "fwAnimation/animhelpers.h"
#include "fwAnimation/animdirector.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camAnimSceneDirector,0xd4d361d2)

camAnimSceneDirector::camAnimSceneDirector(const camBaseObjectMetadata& metadata)
: camBaseDirector(metadata)
, m_Metadata(static_cast<const camAnimSceneDirectorMetadata&>(metadata))
{
	m_cleanUpCameraAfterBlendout = false;
}

camAnimSceneDirector::~camAnimSceneDirector()
{
}

bool camAnimSceneDirector::Update()
{
	bool hasSucceeded = false;

	if(m_cleanUpCameraAfterBlendout)
	{
		camAnimatedCamera* cam = GetCamera();
		if( cam )
		{
			cam->SetCurrentTime( cam->GetCurrentTime() + fwTimer::GetTimeStep() );
		}
		
		if((m_ActiveRenderState != RS_INTERPOLATING_OUT) || (m_ActiveRenderState == RS_NOT_RENDERING))
		{
			StopAnimatingCamera();
			m_cleanUpCameraAfterBlendout = false;
		}
	}

	if(m_UpdatedCamera)
	{
		m_UpdatedCamera->BaseUpdate(m_Frame);
		hasSucceeded = true;
	}

	return hasSucceeded; 
}

void camAnimSceneDirector::CreateCamera()
{
	m_UpdatedCamera = camFactory::CreateObject<camBaseCamera>(m_Metadata.m_DefaultAnimatedCameraRef);
}

void camAnimSceneDirector::BlendOutCameraAnimation(u32 interpolationDuration)
{
	StopRendering(interpolationDuration, false); 
}

void camAnimSceneDirector::SetCleanUpCameraAfterBlendOut(bool val /*= true*/)
{
	m_cleanUpCameraAfterBlendout = val;
}

void camAnimSceneDirector::StopAnimatingCamera()
{
	StopRendering(); 

	if(m_UpdatedCamera)
	{
		delete m_UpdatedCamera; 
		m_UpdatedCamera = NULL;
	}
}

bool camAnimSceneDirector::AnimateCamera(const strStreamingObjectName animDictName, const crClip& clip, const Matrix34& sceneOrigin, float startTime, float currentTime, u32 iFlags, u32 BlendInTime)
{
	bool hasSucceeded = false;
	
	//Delete the current camera
	if(m_UpdatedCamera)
	{
		delete m_UpdatedCamera; 
		m_UpdatedCamera = NULL;
	}

	//reset blend out cleanups
	SetCleanUpCameraAfterBlendOut(false);

	//create a new instance of the camera
	CreateCamera(); 
	
	if(m_UpdatedCamera)
	{
		//Tell the director to start rendering 
		Render(BlendInTime); 

		camAnimatedCamera* pAnimatedCamera = static_cast<camAnimatedCamera*>(m_UpdatedCamera.Get());

		if(pAnimatedCamera)
		{
			pAnimatedCamera->Init();
			Matrix34 finalSceneOrigin(sceneOrigin);
			fwAnimHelpers::ApplyInitialOffsetFromClip(clip, finalSceneOrigin);
			pAnimatedCamera->SetSceneMatrix(finalSceneOrigin);

			if(cameraVerifyf(pAnimatedCamera->SetCameraClip(VEC3_ZERO, animDictName, &clip, startTime, iFlags),
				"Failed to initialise the animation for a script animated camera"))
			{
				pAnimatedCamera->SetCurrentTime(currentTime);
				hasSucceeded = true;
			}
		}
	}
	return hasSucceeded;
}

camAnimatedCamera* camAnimSceneDirector::GetCamera()
{
	if(m_UpdatedCamera)
	{
		camAnimatedCamera* pAnimatedCamera = static_cast<camAnimatedCamera*>(m_UpdatedCamera.Get());
		return pAnimatedCamera; 
	}
	return NULL; 
}


#if __BANK
void camAnimSceneDirector::DebugStopRender()
{
	if (IsRendering())
	{
		StopRendering(); 
	}
}

void camAnimSceneDirector::DebugStartRender()
{
	if (!IsRendering())
	{
		Render(); 
	}
}
#endif
