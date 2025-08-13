//
// camera/SynceSceneDirector.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/syncedscene/SyncedSceneDirector.h"

#include "camera/base/baseCamera.h"
#include "camera/cutscene/AnimatedCamera.h"
#include "camera/system/CameraMetadata.h"
#include "camera/system/CameraFactory.h"
#include "fwAnimation/animhelpers.h"
#include "fwAnimation/animdirector.h"
#include "fwanimation/directorcomponentsyncedscene.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camSyncedSceneDirector,0xD92813B6)

#if __BANK
const char* ClientName[camSyncedSceneDirector::MAX_CLIENTS] = {"ANIM_PLACEMENT_TOOL","TASK_ARREST","TASK_SHARK_ATTACK","NETWORK_SYNCHRONISED_SCENES"};
#endif

camSyncedSceneDirector::camSyncedSceneDirector(const camBaseObjectMetadata& metadata)
: camBaseDirector(metadata)
, m_Metadata(static_cast<const camSyncedSceneDirectorMetadata&>(metadata))
, m_activeClient(MAX_CLIENTS)
, m_StartRenderingNextUpdate(false)
{

}

camSyncedSceneDirector::~camSyncedSceneDirector()
{
}

bool camSyncedSceneDirector::Update()
{
	bool hasSucceeded = false;

	if (m_StartRenderingNextUpdate)
	{
		Render();
		m_StartRenderingNextUpdate = false;
	}

	if(m_UpdatedCamera)
	{
		m_UpdatedCamera->BaseUpdate(m_Frame);
		hasSucceeded = true;
	}

	return hasSucceeded; 
}

void camSyncedSceneDirector::CreateCamera()
{
	m_UpdatedCamera = camFactory::CreateObject<camBaseCamera>(m_Metadata.m_DefaultAnimatedCameraRef);
	
}

void camSyncedSceneDirector::StopAnimatingCamera(SyncedSceneAnimatedCameraClients Client, u32 interpolationDuration)
{
	if(m_UpdatedCamera && Client == m_activeClient)
	{
		delete m_UpdatedCamera; 
		m_UpdatedCamera = NULL; 
		m_activeClient = MAX_CLIENTS; 
		StopRendering(interpolationDuration, true); 
	}
}

bool camSyncedSceneDirector::AnimateCamera(const strStreamingObjectName animDictName, const crClip& clip, const Matrix34& sceneOrigin, int sceneId /*=-1*/, u32 iFlags /*= 0*/, SyncedSceneAnimatedCameraClients client)
{
	bool hasSucceeded = false;
	
	//only change the camera is the calling client is higher priority
	if(client <= m_activeClient)
	{
		//Delete the current camera
		if(m_UpdatedCamera)
		{
			delete m_UpdatedCamera; 
			m_UpdatedCamera = NULL;
		}

		m_activeClient = client; 
	}
	else
	{
#if __ASSERT		
		char animName[256] = "NO CLIP";
		clip.GetDebugName(animName, 256);
		cameraAssertf(0, "%s (Dict: %s, Clip: %s, SceneId: %d ) synced scene camera will not render as higher priorty %s is already rendering a camera", ClientName[client], animDictName.GetCStr(), animName, sceneId, ClientName[m_activeClient]); 
#endif		
		return hasSucceeded; 
	}

	//create a new instance of the camera
	CreateCamera(); 
	
	if(m_UpdatedCamera)
	{
		//Tell the director to start rendering
		m_StartRenderingNextUpdate = true;

		camAnimatedCamera* pAnimatedCamera = static_cast<camAnimatedCamera*>(m_UpdatedCamera.Get());

		if(pAnimatedCamera)
		{
			pAnimatedCamera->Init();
			pAnimatedCamera->SetShouldUseLocalCameraTime(true);
			//If the clip contains a scene origin/offset, apply it to the script-specified origin.
			Matrix34 finalSceneOrigin(sceneOrigin);
			fwAnimHelpers::ApplyInitialOffsetFromClip(clip, finalSceneOrigin);
			pAnimatedCamera->SetSceneMatrix(finalSceneOrigin);

			float currentTimeInSeconds = fwAnimDirectorComponentSyncedScene::GetSyncedSceneTime((fwSyncedSceneId)sceneId);
			if(cameraVerifyf(pAnimatedCamera->SetCameraClip(VEC3_ZERO, animDictName, &clip, currentTimeInSeconds, iFlags),
				"Failed to initialise the animation for a script animated camera"))
			{
				pAnimatedCamera->SetCurrentTime(currentTimeInSeconds);
				pAnimatedCamera->UpdateFrame(0.0f);	//Set initial phase (for translation, rotation etc.)

				if (fwAnimDirectorComponentSyncedScene::IsValidSceneId((fwSyncedSceneId)sceneId))
				{
					pAnimatedCamera->SetSyncedScene((fwSyncedSceneId)sceneId);
				}

				pAnimatedCamera->GetFrameNonConst().GetFlags().SetFlag(camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation);

				hasSucceeded = true;
			}
		}
	}
	return hasSucceeded;
}


#if __BANK
void camSyncedSceneDirector::DebugStopRender(SyncedSceneAnimatedCameraClients client)
{
	if(client == m_activeClient)
	{
		if (IsRendering())
		{
			StopRendering(); 
		}
	}
}

void camSyncedSceneDirector::DebugStartRender(SyncedSceneAnimatedCameraClients client)
{
	if(client == m_activeClient)
	{
		if (!IsRendering())
		{
			Render(); 
		}
	}
}
#endif