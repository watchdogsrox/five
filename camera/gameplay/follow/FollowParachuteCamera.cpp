//
// camera/gameplay/follow/FollowParachuteCamera.cpp
// 
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#include "camera/gameplay/follow/FollowParachuteCamera.h"

#include "camera/gameplay/follow/FollowPedCamera.h"
#include "camera/helpers/Envelope.h"
#include "camera/helpers/FrameInterpolator.h"
#include "camera/system/CameraFactory.h"
#include "camera/system/CameraMetadata.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camFollowParachuteCamera,0x3E99A560)


camFollowParachuteCamera::camFollowParachuteCamera(const camBaseObjectMetadata& metadata)
: camFollowObjectCamera(metadata)
, m_Metadata(static_cast<const camFollowParachuteCameraMetadata&>(metadata))
, m_VerticalMoveSpeedEnvelope(NULL)
{
	const camEnvelopeMetadata* envelopeMetadata	= camFactory::FindObjectMetadata<camEnvelopeMetadata>(
													m_Metadata.m_CustomSettings.m_VerticalMoveSpeedEnvelopeRef.GetHash());
	if(envelopeMetadata)
	{
		m_VerticalMoveSpeedEnvelope = camFactory::CreateObject<camEnvelope>(*envelopeMetadata);
		if(cameraVerifyf(m_VerticalMoveSpeedEnvelope,
			"A follow parachute camera (name: %s, hash: %u) failed to create an envelope (name: %s, hash: %u)", GetName(), GetNameHash(),
			SAFE_CSTRING(envelopeMetadata->m_Name.GetCStr()), envelopeMetadata->m_Name.GetHash()))
		{
			m_VerticalMoveSpeedEnvelope->SetUseGameTime(true);
		}
	}
}

camFollowParachuteCamera::~camFollowParachuteCamera()
{
	if(m_VerticalMoveSpeedEnvelope)
	{
		delete m_VerticalMoveSpeedEnvelope;
	}
}

bool camFollowParachuteCamera::Update()
{
	//NOTE: We must only start the deploy behaviour if we are interpolating from the sky diving camera, which happens to be a follow-ped camera.
	//TODO: Determine a cleaner approach.
	bool shouldStartDeployBehaviour = false;
	if((m_NumUpdatesPerformed == 0) && m_FrameInterpolator)
	{
		const camBaseCamera* sourceCamera = m_FrameInterpolator->GetSourceCamera();
		if(sourceCamera && sourceCamera->GetIsClassId(camFollowPedCamera::GetStaticClassId()))
		{
			shouldStartDeployBehaviour = true;
		}
	}

	if(m_VerticalMoveSpeedEnvelope)
	{
		if(shouldStartDeployBehaviour)
		{
			m_VerticalMoveSpeedEnvelope->Start();
		}

		m_VerticalMoveSpeedEnvelope->Update();
	}

	if(shouldStartDeployBehaviour && (m_Metadata.m_CustomSettings.m_DeployShakeRef != 0))
	{
		Shake(m_Metadata.m_CustomSettings.m_DeployShakeRef, 1.0f);
	}

	const bool hasSucceeded = camFollowObjectCamera::Update();

	return hasSucceeded;
}

float camFollowParachuteCamera::ComputeVerticalMoveSpeedScaling(const Vector3& attachParentVelocity) const
{
	float verticalMoveSpeedScaling = camFollowObjectCamera::ComputeVerticalMoveSpeedScaling(attachParentVelocity);

	if (m_VerticalMoveSpeedEnvelope && m_VerticalMoveSpeedEnvelope->IsActive())
	{
		float blendLevel			= m_VerticalMoveSpeedEnvelope->GetLevel();
		blendLevel					= SlowInOut(blendLevel);
		verticalMoveSpeedScaling	= Lerp(blendLevel, verticalMoveSpeedScaling, m_Metadata.m_CustomSettings.m_BaseVerticalMoveSpeedScale);
	}

	return verticalMoveSpeedScaling;
}
