//
// camera/base/BaseDirector.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/base/BaseDirector.h"

#include "input/headtracking.h"

#include "camera/CamInterface.h"
#include "camera/base/BaseCamera.h"
#include "camera/helpers/FrameInterpolator.h"
#include "camera/helpers/FrameShaker.h"
#include "camera/helpers/AnimatedFrameShaker.h"
#include "camera/system/CameraFactory.h"
#include "camera/system/CameraMetadata.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camBaseDirector,0x6BFB7FB2)


camBaseDirector::camBaseDirector(const camBaseObjectMetadata& metadata)
: camBaseObject(metadata)
, m_Metadata(static_cast<const camBaseDirectorMetadata&>(metadata))
, m_FrameInterpolator(NULL)
{
	BaseReset();
}

camBaseDirector::~camBaseDirector()
{
	StopShaking(true);
	
	if(m_FrameInterpolator)
	{
		delete m_FrameInterpolator;
	}
}

bool camBaseDirector::BaseUpdate(camFrame& destinationFrame)
{
	bool hasSucceeded = true;

	m_CachedInitialDestinationFrame.CloneFrom(destinationFrame);

	const bool shouldPause = m_Metadata.m_CanBePaused && camInterface::ComputeShouldPauseCameras();
	if(!shouldPause)
	{
		//Update the derived director.
		hasSucceeded = Update();

		//Apply post update effects e.g. shakes
		PostUpdate(); 

		//Now update the active render state.
		UpdateActiveRenderState();

		//Action and clear any request for rendering to be bypassed.
		m_IsRenderingBypassed = m_ShouldBypassRenderingThisUpdate;
		m_ShouldBypassRenderingThisUpdate = false;
	}

	const bool isRendering = IsRendering();
	if(isRendering)
	{
		destinationFrame.CloneFrom(m_PostEffectFrame);

		//NOTE: We need to overwrite the rendered camera here even if we don't have an updated camera, as we are outputting a frame.
		m_RenderedCamera = m_UpdatedCamera;
	}
	else
	{
		m_RenderedCamera = NULL;
	}

	UpdatePostRender();

	return hasSucceeded;
}

void camBaseDirector::PostUpdate()
{
	m_PostEffectFrame.CloneFrom(m_Frame); 
	
	if (!rage::ioHeadTracking::IsMotionTrackingEnabled())
	{
		if(m_FrameShaker)
		{
            const float shakeAmplitude = m_UpdatedCamera ? m_UpdatedCamera->GetFrameShakerScaleFactor() : 1.0f;
			//Apply any shake to our frame.
			m_FrameShaker->Update(m_PostEffectFrame, shakeAmplitude);
		}
	}
	if(m_ShouldForceHighQualityDofThisUpdate)
	{
		m_PostEffectFrame.SetDofPlanesBlendLevel(1.0f);

		m_ShouldForceHighQualityDofThisUpdate = false;
	}
}

void camBaseDirector::UpdateActiveRenderState()
{
	if(m_ActiveRenderState == RS_INTERPOLATING_IN)
	{
		if(m_FrameInterpolator)
		{
			//Interpolate from the initial destination frame to our desired output frame.

			if(!m_ShouldLockInterpolationSourceFrame)
			{
				//Update the interpolation source frame.
				m_FrameInterpolator->SetSourceFrame(m_CachedInitialDestinationFrame);
			}

#if FPS_MODE_SUPPORTED
			//Override the blend curves for an interpolation from an FPS camera or a custom fall back camera.
			const bool wasRenderingFpsCameraOnPreviousUpdate				= camInterface::IsRenderingFirstPersonShooterCamera();
			const bool wasRenderingFpsCustomFallBackCameraOnPreviousUpdate	= camInterface::IsRenderingFirstPersonShooterCustomFallBackCamera();
			if(wasRenderingFpsCameraOnPreviousUpdate || wasRenderingFpsCustomFallBackCameraOnPreviousUpdate)
			{
				for(int componentIndex=0; componentIndex<camFrame::CAM_FRAME_MAX_COMPONENTS; componentIndex++)
				{
					m_FrameInterpolator->SetCurveTypeForFrameComponent((camFrame::eComponentType)componentIndex, camFrameInterpolator::DECELERATION);
				}
			}
#endif // FPS_MODE_SUPPORTED

			const u32 timeInMilliseconds		= GetTimeInMilliseconds();
			const camFrame& interpolatedFrame	= m_FrameInterpolator->Update(m_PostEffectFrame, timeInMilliseconds);
			m_PostEffectFrame.CloneFrom(interpolatedFrame);

			if(!m_FrameInterpolator->IsInterpolating())
			{
				m_ActiveRenderState = RS_FULLY_RENDERING;
			}
		}
		else
		{
			m_ActiveRenderState = RS_FULLY_RENDERING;
		}
	}
	else if(m_ActiveRenderState == RS_INTERPOLATING_OUT)
	{
		if(m_FrameInterpolator)
		{
			//Interpolate from our desired output frame to the initial destination frame.

			if(!m_ShouldLockInterpolationSourceFrame)
			{
				//Update the interpolation source frame.
				m_FrameInterpolator->SetSourceFrame(m_PostEffectFrame);
			}

#if FPS_MODE_SUPPORTED
			//Override the blend curves for an interpolation to an FPS camera or a custom fall back camera.
			const bool wasRenderingFpsCameraOnPreviousUpdate				= camInterface::IsRenderingFirstPersonShooterCamera();
			const bool wasRenderingFpsCustomFallBackCameraOnPreviousUpdate	= camInterface::IsRenderingFirstPersonShooterCustomFallBackCamera();
			if(wasRenderingFpsCameraOnPreviousUpdate || wasRenderingFpsCustomFallBackCameraOnPreviousUpdate)
			{
				for(int componentIndex=0; componentIndex<camFrame::CAM_FRAME_MAX_COMPONENTS; componentIndex++)
				{
					m_FrameInterpolator->SetCurveTypeForFrameComponent((camFrame::eComponentType)componentIndex, camFrameInterpolator::ACCELERATION);
				}
			}
#endif // FPS_MODE_SUPPORTED

			const u32 timeInMilliseconds		= GetTimeInMilliseconds();
			const camFrame& interpolatedFrame	= m_FrameInterpolator->Update(m_CachedInitialDestinationFrame, timeInMilliseconds);
			m_PostEffectFrame.CloneFrom(interpolatedFrame);

			if(!m_FrameInterpolator->IsInterpolating())
			{
				m_ActiveRenderState = RS_NOT_RENDERING;
			}
		}
		else
		{
			m_ActiveRenderState = RS_NOT_RENDERING;
		}
	}
	else if(m_FrameInterpolator) //Clean-up our interpolator, if we still have one.
	{
		delete m_FrameInterpolator;
		m_FrameInterpolator = NULL;
	}
}

bool camBaseDirector::Shake(const char* shakeName, float amplitude)
{
	m_FrameShaker = camFactory::CreateObject<camBaseFrameShaker>(shakeName);

	const bool isShaking = (m_FrameShaker.Get() != NULL);
	if(isShaking)
	{
		m_FrameShaker->SetAmplitude(amplitude);
	}

	return isShaking;
}

bool camBaseDirector::Shake(const char* animDictionary, const char* animClip, const char* metadataName, float amplitude)
{
	const camAnimatedShakeMetadata* metadata = NULL;
	if (metadataName)
	{
		metadata = camFactory::FindObjectMetadata<camAnimatedShakeMetadata>(metadataName);
	}

	u32 defaultNameHash = ATSTRINGHASH("DEFAULT_ANIMATED_SHAKE", 0xD79E1C63);
	if (!metadata)
	{
		metadata = camFactory::FindObjectMetadata<camAnimatedShakeMetadata>(defaultNameHash);
	}

	m_FrameShaker = metadata ? camFactory::CreateObject<camAnimatedFrameShaker>(*metadata) : NULL;

	const bool isShaking = (m_FrameShaker.Get() != NULL);
	if(isShaking)
	{
		camAnimatedFrameShaker* pAnimatedFrameShaker = static_cast<camAnimatedFrameShaker*>(m_FrameShaker.Get());
		if (!pAnimatedFrameShaker->SetAnimation(animDictionary, animClip))
		{
			// Animation not found, so remove shaker.
			delete m_FrameShaker.Get();
			m_FrameShaker.Reset(NULL);
		}
		else
		{
			pAnimatedFrameShaker->SetAmplitude(amplitude);
		}
	}

	return isShaking;
}

bool camBaseDirector::IsShaking() const
{
	const bool isShaking = (m_FrameShaker.Get() != NULL);

	return isShaking;
}

bool camBaseDirector::SetShakeAmplitude(float amplitude)
{
	const bool isShaking = (m_FrameShaker.Get() != NULL);
	if(isShaking)
	{
		m_FrameShaker->SetAmplitude(amplitude);
	}

	return isShaking;
}

void camBaseDirector::StopShaking(bool shouldStopImmediately)
{
	if(m_FrameShaker)
	{
		if(shouldStopImmediately)
		{
			//Immediately remove our reference to the shake.
			m_FrameShaker->RemoveRef(m_FrameShaker);
		}
		else
		{
			//Smoothly release the shake.
			m_FrameShaker->Release();
		}
	}
}

void camBaseDirector::BaseReset()
{
	//Reset the frames.
	camFrame defaultFrame;
	m_Frame.CloneFrom(defaultFrame);
	m_CachedInitialDestinationFrame.CloneFrom(defaultFrame);

	m_UpdatedCamera		= NULL;
	m_RenderedCamera	= NULL;

	StopShaking(true);
	
	if(m_FrameInterpolator)
	{
		delete m_FrameInterpolator;
		m_FrameInterpolator = NULL;
	}

	m_ActiveRenderState						= RS_NOT_RENDERING;
	m_ShouldLockInterpolationSourceFrame	= false;
	m_ShouldForceHighQualityDofThisUpdate	= false;
	m_ShouldBypassRenderingThisUpdate		= false;
	m_IsRenderingBypassed					= false;
}

float camBaseDirector::GetInterpolationBlendLevel() const
{
	float blendLevel;

	//NOTE: We must take into account any rendering bypass, so we don't query m_ActiveRenderState directly.
	const eRenderStates effectiveRenderState = GetRenderState();

	switch(effectiveRenderState)
	{
	case RS_INTERPOLATING_IN:
		blendLevel = m_FrameInterpolator ? m_FrameInterpolator->GetBlendLevel() : 1.0f;
		break;

	case RS_FULLY_RENDERING:
		blendLevel = 1.0f;
		break;

	case RS_INTERPOLATING_OUT:
		blendLevel = m_FrameInterpolator ? (1.0f - m_FrameInterpolator->GetBlendLevel()) : 0.0f;
		break;

// 	case RS_NOT_RENDERING:
// 	case RS_INVALID:
	default:
		blendLevel = 0.0f;
		break;
	}

	return blendLevel;
}

s32 camBaseDirector::GetInterpolationTime() const
{
	s32 interpolationTime = -1;

	if(m_FrameInterpolator)
	{
		const float blendLevel	= m_FrameInterpolator->GetBlendLevel();
		const u32 duration		= m_FrameInterpolator->GetDuration();
		interpolationTime		= static_cast<s32>(Floorf(blendLevel * static_cast<float>(duration)));
	}

	return interpolationTime;
}

void camBaseDirector::Render(u32 interpolationDuration, bool shouldLockInterpolationSourceFrame)
{
	if(interpolationDuration == 0)
	{
		m_ActiveRenderState = RS_FULLY_RENDERING;
	}
	//NOTE: It makes no sense to interpolate in if this is already in progress or we are already fully rendering.
	else if((m_ActiveRenderState != RS_INTERPOLATING_IN) && (m_ActiveRenderState != RS_FULLY_RENDERING))
	{
		//TODO: Deal with transitioning neatly from a partial interpolation out back to interpolating in.

		if(m_FrameInterpolator)
		{
			delete m_FrameInterpolator;
		}

		const u32 timeInMilliseconds	= GetTimeInMilliseconds();
		m_FrameInterpolator				= camera_verified_pool_new(camFrameInterpolator) (m_CachedInitialDestinationFrame, interpolationDuration, timeInMilliseconds);
		cameraAssertf(m_FrameInterpolator, "A director (name: %s, hash: %u) failed to create a frame interpolator", GetName(), GetNameHash());

		m_ActiveRenderState				= m_FrameInterpolator ? RS_INTERPOLATING_IN : RS_FULLY_RENDERING;

		m_ShouldLockInterpolationSourceFrame = shouldLockInterpolationSourceFrame;
	}
}

void camBaseDirector::StopRendering(u32 interpolationDuration, bool shouldLockInterpolationSourceFrame)
{
	if(interpolationDuration == 0)
	{
		m_ActiveRenderState = RS_NOT_RENDERING;
	}
	//NOTE: It makes no sense to interpolate out if this is already in progress or we are already not rendering.
	else if((m_ActiveRenderState != RS_INTERPOLATING_OUT) && (m_ActiveRenderState != RS_NOT_RENDERING))
	{
		//TODO: Deal with transitioning neatly from a partial interpolation in back to interpolating out.

		if(m_FrameInterpolator)
		{
			delete m_FrameInterpolator;
		}

		const u32 timeInMilliseconds	= GetTimeInMilliseconds();
		m_FrameInterpolator				= camera_verified_pool_new(camFrameInterpolator) (m_Frame, interpolationDuration, timeInMilliseconds);
		cameraAssertf(m_FrameInterpolator, "A director (name: %s, hash: %u) failed to create a frame interpolator", GetName(), GetNameHash());

		m_ActiveRenderState				= m_FrameInterpolator ? RS_INTERPOLATING_OUT : RS_NOT_RENDERING;

		m_ShouldLockInterpolationSourceFrame = shouldLockInterpolationSourceFrame;
	}
}

void camBaseDirector::RegisterWeaponFire(const CWeapon& weapon, const CEntity& firingEntity, const bool isNetworkClone)
{
	if(isNetworkClone)
	{
		RegisterWeaponFireRemote(weapon, firingEntity);
	}
	else
	{
		RegisterWeaponFireLocal(weapon, firingEntity);
	}
}

#if __BANK
void camBaseDirector::DebugGetCameras( atArray<camBaseCamera*>& cameraList ) const
{
	if(m_UpdatedCamera)
	{
		cameraList.PushAndGrow(m_UpdatedCamera);
	}
}
#endif // __BANK
