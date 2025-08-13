//
// camera/switch/SwitchDirector.cpp
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#include "camera/switch/SwitchDirector.h"

#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "grcore/debugdraw.h"

#include "camera/CamInterface.h"
#include "camera/switch/SwitchCamera.h"
#include "camera/system/CameraFactory.h"
#include "camera/system/CameraMetadata.h"
#include "peds/ped.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camSwitchDirector,0x192101D6)


camSwitchDirector::camSwitchDirector(const camBaseObjectMetadata& metadata)
: camBaseDirector(metadata)
, m_Metadata(static_cast<const camSwitchDirectorMetadata&>(metadata))
{
}

camSwitchDirector::~camSwitchDirector()
{
	if(m_ActiveCamera)
	{
		delete m_ActiveCamera;
	}
}

bool camSwitchDirector::Update()
{
	m_UpdatedCamera = m_ActiveCamera;

	if(m_UpdatedCamera)
	{
		const bool hasSucceeded = m_UpdatedCamera->BaseUpdate(m_Frame);
		if(hasSucceeded)
		{
			camSwitchCamera* switchCamera		= static_cast<camSwitchCamera*>(m_UpdatedCamera.Get());
			const u32 interpolateOutDuration	= switchCamera->GetInterpolateOutDurationToApply();
			if(interpolateOutDuration > 0)
			{
				//Ensure that we are interpolating out (or have already completed the interpolation.)
				StopRendering(interpolateOutDuration);
			}
			else
			{
				//Ensure that we are rendering.
				Render();
			}

			return true;
		}
	}

	//Stop rendering if we do not have a valid camera to render.
	StopRendering();

	return false;
}

bool camSwitchDirector::InitSwitch(u32 cameraNameHash, const CPed& sourcePed, const CPed& destinationPed, const CPlayerSwitchParams& switchParams)
{
	if(m_ActiveCamera)
	{
		delete m_ActiveCamera;
	}

	if(cameraNameHash == 0)
	{
		//Fall back to the default Switch camera if no camera was specified.
		cameraNameHash = m_Metadata.m_DefaultSwitchCameraRef;
	}

	m_ActiveCamera = camFactory::CreateObject<camSwitchCamera>(cameraNameHash);
	if(!cameraVerifyf(m_ActiveCamera, "The Switch director failed to create a camera (hash: %u)", cameraNameHash))
	{
		return false;
	}

	camSwitchCamera* switchCamera = static_cast<camSwitchCamera*>(m_ActiveCamera.Get());
	switchCamera->Init(sourcePed, destinationPed, switchParams);

	return true;
}

void camSwitchDirector::ShutdownSwitch()
{
	if(m_ActiveCamera)
	{
		delete m_ActiveCamera;
	}
}

camSwitchCamera* camSwitchDirector::GetSwitchCamera()
{
	camSwitchCamera* switchCamera	= (m_ActiveCamera && m_ActiveCamera->GetIsClassId(camSwitchCamera::GetStaticClassId())) ?
										static_cast<camSwitchCamera*>(m_ActiveCamera.Get()) : NULL;

	return switchCamera;
}
