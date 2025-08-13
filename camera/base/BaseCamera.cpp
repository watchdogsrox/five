//
// camera/base/BaseCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/base/BaseCamera.h"

#include "fwsys/timer.h"
#include "grcore/debugdraw.h"
#include "grcore/font.h"
#include "input/headtracking.h"

#include "camera/CamInterface.h"
#include "camera/gameplay/ThirdPersonCamera.h"
#include "camera/helpers/Collision.h"
#include "camera/helpers/ControlHelper.h"
#include "camera/helpers/FrameInterpolator.h"
#include "camera/helpers/AnimatedFrameShaker.h"
#include "camera/helpers/ThirdPersonFrameInterpolator.h"
#include "camera/system/CameraMetadata.h"
#include "camera/system/CameraFactory.h"
#include "camera/system/CameraManager.h"
#include "peds/ped.h"

#if __BANK
#include "bank/bank.h"
#include "grcore/device.h"
#endif

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camBaseCamera,0xFE12CE88)

const u32 g_DefaultDofSettingsNameHash = ATSTRINGHASH("DEFAULT_DOF_SETTINGS", 0x4aaf5eef);

camFrame	camBaseCamera::ms_ScratchFrame;					//A general purpose scratch camera frame.
bank_float	camBaseCamera::ms_MotionBlurDecayRate = 0.5f;


camBaseCamera::camBaseCamera(const camBaseObjectMetadata& metadata)
: camBaseObject(metadata)
, m_FrameInterpolator(NULL)
, m_DofSettings(NULL)
, m_Metadata(static_cast<const camBaseCameraMetadata&>(metadata))
, m_NumUpdatesPerformed(0)
, m_WasUpdated(false)
, m_ShouldPreventNextCameraCloningOrientation(false)
, m_ShouldPreventInterpolationToNextCamera(false)
, m_AllowMotionBlurDecay(true)
{
	//Create a collision helper if specified in metadata.
	const camCollisionMetadata* collisionMetadata = camFactory::FindObjectMetadata<camCollisionMetadata>(m_Metadata.m_CollisionRef.GetHash());
	if(collisionMetadata)
	{
		m_Collision = camFactory::CreateObject<camCollision>(*collisionMetadata);
		cameraAssertf(m_Collision, "A camera (name: %s, hash: %u) failed to create a collision helper (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(collisionMetadata->m_Name.GetCStr()), collisionMetadata->m_Name.GetHash());
	}

	//Apply any shake specified in metadata.
	const camShakeMetadata* shakeMetadata = camFactory::FindObjectMetadata<camShakeMetadata>(m_Metadata.m_ShakeRef.GetHash());
	if(shakeMetadata)
	{
		Shake(*shakeMetadata);
	}

	m_DofSettings = camFactory::FindObjectMetadata<camDepthOfFieldSettingsMetadata>(m_Metadata.m_DofSettings.GetHash());
	if(!m_DofSettings)
	{
		//Fall back to the default if we don't have custom settings.
		m_DofSettings = camFactory::FindObjectMetadata<camDepthOfFieldSettingsMetadata>(g_DefaultDofSettingsNameHash);
	}

	if(m_DofSettings)
	{
		//Assign the basic DOF settings to both camera frames so that we don't apply the default values if this camera is not updated
		//(which can occur with scripted cameras in particular.)
		m_Frame.SetDofSettings(*m_DofSettings);
		m_PostEffectFrame.SetDofSettings(*m_DofSettings);
	}
}

camBaseCamera::~camBaseCamera()
{
	StopShaking();

	StopInterpolating();

	if(m_Collision)
	{
		delete m_Collision;
	}
}

void camBaseCamera::PreUpdate()
{
	m_WasUpdated = false;

	if(m_Collision)
	{
		m_Collision->PreUpdate();
	}
}

bool camBaseCamera::BaseUpdate(camFrame& destinationFrame)
{
	const bool shouldPause = m_Metadata.m_CanBePaused && camInterface::ComputeShouldPauseCameras();
	if(!shouldPause)
	{
		float originalMotionBlur = m_Frame.GetMotionBlurStrength();

		//Update the derived camera.
		if(!Update())
		{
			return false;
		}

		//Apply a decay to the motion blur.
		float newMotionBlur = m_Frame.GetMotionBlurStrength();
		if(m_AllowMotionBlurDecay && newMotionBlur < originalMotionBlur)
		{
			float motionBlur = originalMotionBlur - ((originalMotionBlur - newMotionBlur) * ms_MotionBlurDecayRate);
			m_Frame.SetMotionBlurStrength(motionBlur);
		}

		//Apply any post-effects, i.e. anything that affects the post-effects frame, but not the regular update frame.

		m_PostEffectFrame.CloneFrom(m_Frame);

		//NOTE: We have now 'updated' this camera and propagated the result to the post-effect frame in readiness for the post-effects and post-update.
		m_WasUpdated = true;
		m_NumUpdatesPerformed++;

		//Update and apply any frame shakers.
		UpdateFrameShakers(m_PostEffectFrame);

		//Clean up any (NULL) references to expired frame shakers.
		const int numFrameShakers = m_FrameShakers.GetCount();
		for(int frameShakerIndex=numFrameShakers - 1; frameShakerIndex>=0; frameShakerIndex--)
		{
			if(m_FrameShakers[frameShakerIndex].Get() == NULL)
			{
				m_FrameShakers.Delete(frameShakerIndex);
			}
		}

		UpdateDof();

		if(m_FrameInterpolator != NULL)
		{
			//Interpolate to our post-effect frame from another camera's frame.
			const camFrame& interpolatedFrame = m_FrameInterpolator->Update(*this);
			m_PostEffectFrame.CloneFrom(interpolatedFrame);

			if(!m_FrameInterpolator->IsInterpolating())
			{
				StopInterpolating();
			}
		}

		//Post-update the derived camera.
		PostUpdate();
	}

	//Propagate the post-effect frame.
	destinationFrame.CloneFrom(m_PostEffectFrame);

	return true;
}

void camBaseCamera::UpdateFrameShakers(camFrame& frameToShake, float amplitudeScalingFactor)
{
	if (!rage::ioHeadTracking::IsMotionTrackingEnabled())
	{
		const int numFrameShakers = m_FrameShakers.GetCount();
		for(int frameShakerIndex=0; frameShakerIndex<numFrameShakers; frameShakerIndex++)
		{
			RegdCamBaseFrameShaker& frameShaker = m_FrameShakers[frameShakerIndex];
			if(frameShaker.Get() != NULL)
			{
				//Update and apply the shake, which will automatically clean up if it's finished.
				frameShaker->Update(frameToShake, amplitudeScalingFactor);
			}
		}
	}
}

void camBaseCamera::UpdateDof()
{
	Vector3 overriddenFocusPosition;
	float overriddenFocusDistanceBlendLevel;
	ComputeDofOverriddenFocusSettings(overriddenFocusPosition, overriddenFocusDistanceBlendLevel);

	const Vector3& cameraPosition	= m_PostEffectFrame.GetPosition();
	float overriddenFocusDistance	= (overriddenFocusDistanceBlendLevel >= SMALL_FLOAT) ? cameraPosition.Dist(overriddenFocusPosition) : 0.0f;
	m_PostEffectFrame.SetDofOverriddenFocusDistance(overriddenFocusDistance);
	m_PostEffectFrame.SetDofOverriddenFocusDistanceBlendLevel(overriddenFocusDistanceBlendLevel);

	camDepthOfFieldSettingsMetadata dofSettings;
	ComputeDofSettings(overriddenFocusDistance, overriddenFocusDistanceBlendLevel, dofSettings);

	ApplyCustomFocusBehavioursToDofSettings(dofSettings);

	m_PostEffectFrame.SetDofSettings(dofSettings);
}

void camBaseCamera::ComputeDofOverriddenFocusSettings(Vector3& focusPosition, float& blendLevel) const
{
	const CEntity* focusEntity = ComputeDofOverriddenFocusEntity();
	if(!focusEntity)
	{
		blendLevel = 0.0f;

		return;
	}

	blendLevel = 1.0f;

	const fwTransform& lookAtTargetTransform	= focusEntity->GetTransform();
	focusPosition								= VEC3V_TO_VECTOR3(lookAtTargetTransform.GetPosition());

	if(focusEntity->GetIsTypePed())
	{
		//Focus on the ped's head position at the end of the previous update. It would be preferable to focus on the plane of the eyes that will be rendered this update,
		//however the update order of the game can result in a ped's head position being resolved after the camera update, which makes it impractical to query the 'live'
		//eye positions here. Any such queries can give results based upon an un-posed or non-final skeleton.

		Matrix34 focusPedHeadBoneMatrix;
		const CPed* focusPed		= static_cast<const CPed*>(focusEntity);
		const bool isHeadBoneValid	= focusPed->GetBoneMatrixCached(focusPedHeadBoneMatrix, BONETAG_HEAD);
		if(isHeadBoneValid)
		{
			focusPosition.Set(focusPedHeadBoneMatrix.d);
		}
	}
}

const CEntity* camBaseCamera::ComputeDofOverriddenFocusEntity() const
{
	const CEntity* focusEntity = NULL;
	if(m_DofSettings)
	{
		if(m_DofSettings->m_ShouldFocusOnLookAtTarget)
		{
			focusEntity = m_LookAtTarget;
		}
		else if(m_DofSettings->m_ShouldFocusOnAttachParent)
		{
			focusEntity = m_AttachParent;
		}
	}

	return focusEntity;
}

void camBaseCamera::ComputeDofSettings(float UNUSED_PARAM(overriddenFocusDistance), float UNUSED_PARAM(overriddenFocusDistanceBlendLevel), camDepthOfFieldSettingsMetadata& settings) const
{
	if(m_DofSettings)
	{
		settings = *m_DofSettings;
	}
}

void camBaseCamera::BlendDofSettings(float blendLevel, camDepthOfFieldSettingsMetadata& sourceSettings, camDepthOfFieldSettingsMetadata& destinationSettings,
	camDepthOfFieldSettingsMetadata& blendedSettings) const
{
	blendedSettings.m_FocusDistanceGridScaling.Lerp(blendLevel, sourceSettings.m_FocusDistanceGridScaling, destinationSettings.m_FocusDistanceGridScaling);
	blendedSettings.m_SubjectMagnificationPowerFactorNearFar.Lerp(blendLevel, sourceSettings.m_SubjectMagnificationPowerFactorNearFar, destinationSettings.m_SubjectMagnificationPowerFactorNearFar);

	//NOTE: We must avoid blending to/from an invalid (blended out) maximum pixel depth.
	if(destinationSettings.m_MaxPixelDepthBlendLevel >= SMALL_FLOAT)
	{
		if(sourceSettings.m_MaxPixelDepthBlendLevel >= SMALL_FLOAT)
		{
			blendedSettings.m_MaxPixelDepth = Lerp(blendLevel, sourceSettings.m_MaxPixelDepth, destinationSettings.m_MaxPixelDepth);
		}
		else
		{
			blendedSettings.m_MaxPixelDepth = destinationSettings.m_MaxPixelDepth;
		}
	}
	else
	{
		blendedSettings.m_MaxPixelDepth = sourceSettings.m_MaxPixelDepth;
	}

	blendedSettings.m_MaxPixelDepthBlendLevel	= Lerp(blendLevel, sourceSettings.m_MaxPixelDepthBlendLevel, destinationSettings.m_MaxPixelDepthBlendLevel);
	blendedSettings.m_PixelDepthPowerFactor		= Lerp(blendLevel, sourceSettings.m_PixelDepthPowerFactor, destinationSettings.m_PixelDepthPowerFactor);
	blendedSettings.m_FNumberOfLens				= camFrame::InterpolateFNumberOfLens(blendLevel, sourceSettings.m_FNumberOfLens, destinationSettings.m_FNumberOfLens);
	blendedSettings.m_FocalLengthMultiplier		= Lerp(blendLevel, sourceSettings.m_FocalLengthMultiplier, destinationSettings.m_FocalLengthMultiplier);
	blendedSettings.m_FStopsAtMaxExposure		= Lerp(blendLevel, sourceSettings.m_FStopsAtMaxExposure, destinationSettings.m_FStopsAtMaxExposure);
	blendedSettings.m_FocusDistanceBias			= Lerp(blendLevel, sourceSettings.m_FocusDistanceBias, destinationSettings.m_FocusDistanceBias);

	//NOTE: We must resolve the max near in-focus distances for keeping an entity in focus prior to blending, as we cannot blend the flags.

	ApplyCustomFocusBehavioursToDofSettings(sourceSettings);
	ApplyCustomFocusBehavioursToDofSettings(destinationSettings);

	blendedSettings.m_ShouldKeepLookAtTargetInFocus = sourceSettings.m_ShouldKeepLookAtTargetInFocus = destinationSettings.m_ShouldKeepLookAtTargetInFocus = false;
	blendedSettings.m_ShouldKeepAttachParentInFocus = sourceSettings.m_ShouldKeepAttachParentInFocus = destinationSettings.m_ShouldKeepAttachParentInFocus = false;

	//NOTE: We must avoid blending to/from an invalid (blended out) maximum near in-focus distance.
	if(destinationSettings.m_MaxNearInFocusDistanceBlendLevel >= SMALL_FLOAT)
	{
		if(sourceSettings.m_MaxNearInFocusDistanceBlendLevel >= SMALL_FLOAT)
		{
			blendedSettings.m_MaxNearInFocusDistance = Lerp(blendLevel, sourceSettings.m_MaxNearInFocusDistance, destinationSettings.m_MaxNearInFocusDistance);
		}
		else
		{
			blendedSettings.m_MaxNearInFocusDistance = destinationSettings.m_MaxNearInFocusDistance;
		}
	}
	else
	{
		blendedSettings.m_MaxNearInFocusDistance = sourceSettings.m_MaxNearInFocusDistanceBlendLevel;
	}

	blendedSettings.m_MaxNearInFocusDistanceBlendLevel		= Lerp(blendLevel, sourceSettings.m_MaxNearInFocusDistanceBlendLevel, destinationSettings.m_MaxNearInFocusDistanceBlendLevel);
	blendedSettings.m_MaxBlurRadiusAtNearInFocusLimit		= Lerp(blendLevel, sourceSettings.m_MaxBlurRadiusAtNearInFocusLimit, destinationSettings.m_MaxBlurRadiusAtNearInFocusLimit);
	blendedSettings.m_BlurDiskRadiusPowerFactorNear			= Lerp(blendLevel, sourceSettings.m_BlurDiskRadiusPowerFactorNear, destinationSettings.m_BlurDiskRadiusPowerFactorNear);
	blendedSettings.m_FocusDistanceIncreaseSpringConstant	= Lerp(blendLevel, sourceSettings.m_FocusDistanceIncreaseSpringConstant, destinationSettings.m_FocusDistanceIncreaseSpringConstant);
	blendedSettings.m_FocusDistanceDecreaseSpringConstant	= Lerp(blendLevel, sourceSettings.m_FocusDistanceDecreaseSpringConstant, destinationSettings.m_FocusDistanceDecreaseSpringConstant);

	blendedSettings.m_ShouldFocusOnLookAtTarget				= sourceSettings.m_ShouldFocusOnLookAtTarget;
	blendedSettings.m_ShouldFocusOnAttachParent				= sourceSettings.m_ShouldFocusOnAttachParent;
	blendedSettings.m_ShouldMeasurePostAlphaPixelDepth		= sourceSettings.m_ShouldMeasurePostAlphaPixelDepth;
	if(blendLevel >= SMALL_FLOAT)
	{
		blendedSettings.m_ShouldFocusOnLookAtTarget			|= destinationSettings.m_ShouldFocusOnLookAtTarget;
		blendedSettings.m_ShouldFocusOnAttachParent			|= destinationSettings.m_ShouldFocusOnAttachParent;
		blendedSettings.m_ShouldMeasurePostAlphaPixelDepth	|= destinationSettings.m_ShouldMeasurePostAlphaPixelDepth;
	}
}

void camBaseCamera::ApplyCustomFocusBehavioursToDofSettings(camDepthOfFieldSettingsMetadata& settings) const
{
	//NOTE: We purely constrain the foreground blur when keeping an entity 'in focus' at present.

	const Vector3& cameraPosition			= m_PostEffectFrame.GetPosition();
	float maxNearInFocusDistanceForEntity	= LARGE_FLOAT;
	bool hasValidInFocusEntity				= false;

	//NOTE: We only keep a look-at target in focus if we are focused upon it.
	if(settings.m_ShouldKeepLookAtTargetInFocus && settings.m_ShouldFocusOnLookAtTarget && m_LookAtTarget)
	{
		spdAABB boundingBox(VECTOR3_TO_VEC3V(m_LookAtTarget->GetBoundingBoxMin()), VECTOR3_TO_VEC3V(m_LookAtTarget->GetBoundingBoxMax()));
		Vec3V relativeCameraPosition	= m_LookAtTarget->GetTransform().UnTransform(VECTOR3_TO_VEC3V(cameraPosition));
		maxNearInFocusDistanceForEntity	= boundingBox.DistanceToPoint(relativeCameraPosition).Getf();
		hasValidInFocusEntity			= true;
	}

	if(settings.m_ShouldKeepAttachParentInFocus && m_AttachParent)
	{
		spdAABB boundingBox(VECTOR3_TO_VEC3V(m_AttachParent->GetBoundingBoxMin()), VECTOR3_TO_VEC3V(m_AttachParent->GetBoundingBoxMax()));
		Vec3V relativeCameraPosition	= m_AttachParent->GetTransform().UnTransform(VECTOR3_TO_VEC3V(cameraPosition));
		maxNearInFocusDistanceForEntity	= Min(maxNearInFocusDistanceForEntity, boundingBox.DistanceToPoint(relativeCameraPosition).Getf());
		hasValidInFocusEntity			= true;
	}

	if(!hasValidInFocusEntity)
	{
		return;
	}

	if(settings.m_MaxNearInFocusDistanceBlendLevel >= SMALL_FLOAT)
	{
		settings.m_MaxNearInFocusDistance = Min(settings.m_MaxNearInFocusDistance, maxNearInFocusDistanceForEntity);
	}
	else
	{
		settings.m_MaxNearInFocusDistance = maxNearInFocusDistanceForEntity;
	}

	settings.m_MaxNearInFocusDistanceBlendLevel = 1.0f;
}

void camBaseCamera::InterpolateFromCamera(camBaseCamera& sourceCamera, u32 duration, bool shouldDeleteSourceCamera)
{
	if(duration == 0)
	{
		return;
	}

	if(sourceCamera.ShouldPreventInterpolationToNextCamera() || sourceCamera.ShouldPreventAnyInterpolation() || ShouldPreventAnyInterpolation())
	{
		return;
	}

	//Terminate any existing interpolation.
	StopInterpolating();

	m_FrameInterpolator	= camera_verified_pool_new(camFrameInterpolator) (sourceCamera, *this, duration, shouldDeleteSourceCamera);
	cameraAssertf(m_FrameInterpolator, "A camera (name: %s, hash: %u) failed to create a frame interpolator", GetName(), GetNameHash());
}

void camBaseCamera::InterpolateFromFrame(const camFrame& sourceFrame, u32 duration)
{
	if(duration == 0)
	{
		return;
	}

	//Terminate any existing interpolation.
	StopInterpolating();

	m_FrameInterpolator = camera_verified_pool_new(camFrameInterpolator) (sourceFrame, duration);
	cameraAssertf(m_FrameInterpolator, "A camera (name: %s, hash: %u) failed to create a frame interpolator", GetName(), GetNameHash());
}

bool camBaseCamera::IsInterpolating(const camBaseCamera* sourceCamera) const
{
	bool isInterpolating = (m_FrameInterpolator != NULL) ? m_FrameInterpolator->IsInterpolating() : false;
	if(isInterpolating && (sourceCamera != NULL) && (sourceCamera != m_FrameInterpolator->GetSourceCamera()))
	{
		isInterpolating = false; //Interpolating, but from a different source camera.
	}

	return isInterpolating;
}

void camBaseCamera::StopInterpolating()
{
	if(m_FrameInterpolator)
	{
		delete m_FrameInterpolator;
		m_FrameInterpolator = NULL;
	}
}

camBaseFrameShaker*	camBaseCamera::Shake(const char* name, float amplitude, u32 maxInstances, u32 minTimeBetweenInstances)
{
	camBaseFrameShaker* frameShaker	= NULL;

	const camBaseShakeMetadata* metadata = camFactory::FindObjectMetadata<camBaseShakeMetadata>(name);

	if(cameraVerifyf(metadata, "A camera (name: %s, hash: %u) failed to create a frame shaker (%s)", GetName(), GetNameHash(), SAFE_CSTRING(name)))
	{
		frameShaker = Shake(*metadata, amplitude, maxInstances, minTimeBetweenInstances);
	}

	return frameShaker;
}

camBaseFrameShaker*	camBaseCamera::Shake(u32 nameHash, float amplitude, u32 maxInstances, u32 minTimeBetweenInstances)
{
	camBaseFrameShaker* frameShaker	= NULL;

	const camBaseShakeMetadata* metadata = camFactory::FindObjectMetadata<camBaseShakeMetadata>(nameHash);

	if(cameraVerifyf(metadata, "A camera (name: %s, hash: %u) failed to create a frame shaker (%u)", GetName(), GetNameHash(), nameHash))
	{
		frameShaker = Shake(*metadata, amplitude, maxInstances, minTimeBetweenInstances);
	}

	return frameShaker;
}

camBaseFrameShaker*	camBaseCamera::Shake(const camBaseShakeMetadata& metadata, float amplitude, u32 maxInstances, u32 minTimeBetweenInstances)
{
	//Limit the number of concurrent instances of this specific shake.
	const int maxExistingInstancesToPersist	= Max(static_cast<int>(maxInstances) - 1, 0);
	const int numFrameShakers				= m_FrameShakers.GetCount();
	int numExistingInstancesFound			= 0;

	//NOTE: The most recently added instances are persisted.
	for(int i=numFrameShakers - 1; i>=0; i--)
	{
		camBaseFrameShaker* frameShaker = m_FrameShakers[i];
		if(frameShaker && (frameShaker->GetNameHash() == metadata.m_Name.GetHash()))
		{
			if((minTimeBetweenInstances > 0) && (numExistingInstancesFound == 0))
			{
				const u32 currentTime		= frameShaker->GetCurrentTimeMilliseconds();
				const u32 shakeStartTime	= frameShaker->GetStartTime();
				if(currentTime < (shakeStartTime + minTimeBetweenInstances))
				{
					return NULL;
				}
			}

			numExistingInstancesFound++;

			if(numExistingInstancesFound > maxExistingInstancesToPersist)
			{
				frameShaker->RemoveRef(m_FrameShakers[i]);

				m_FrameShakers.Delete(i);
			}
		}
	}

	//Now create and add the new shake.

	camBaseFrameShaker* frameShaker	= camFactory::CreateObject<camBaseFrameShaker>(metadata);

	if(cameraVerifyf(frameShaker, "A camera (name: %s, hash: %u) failed to create a frame shaker (name: %s, hash: %u)", GetName(), GetNameHash(),
		SAFE_CSTRING(metadata.m_Name.GetCStr()), metadata.m_Name.GetHash()))
	{
		frameShaker->SetAmplitude(amplitude);

		m_FrameShakers.Grow() = frameShaker;
	}

	return frameShaker;
}

camBaseFrameShaker*	camBaseCamera::Shake(const char* animDictionary, const char* animClip, const char* metadataName, float amplitude)
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

	if (metadata)
	{
		// TODO: for now, only allow one instance of each animated frame shaker to run.
		const int numFrameShakers = m_FrameShakers.GetCount();
		for(int i=numFrameShakers - 1; i>=0; i--)
		{
			camBaseFrameShaker* baseFrameShaker = m_FrameShakers[i];
			if(baseFrameShaker && (baseFrameShaker->GetNameHash() == metadata->m_Name.GetHash()))
			{
				return NULL;
			}
		}

		camAnimatedFrameShaker* frameShaker = camFactory::CreateObject<camAnimatedFrameShaker>(*metadata);
		if ( cameraVerifyf(frameShaker,
							"A camera (name: %s, hash: %u) failed to create an animated frame shaker",
							GetName(), GetNameHash()) )
		{
			if (!frameShaker->SetAnimation(animDictionary, animClip))
			{
				// Animation not found, so remove shaker.
				delete frameShaker;
				frameShaker = NULL;
			}
			else
			{
				frameShaker->SetAmplitude(amplitude);
				m_FrameShakers.Grow() = frameShaker;
				return frameShaker;
			}
		}
	}

	return NULL;
}

camBaseFrameShaker*	camBaseCamera::FindFrameShaker(u32 nameHash)
{
	camBaseFrameShaker* desiredFrameShaker = NULL;

	const int numFrameShakers = m_FrameShakers.GetCount();
	for(int i=0; i<numFrameShakers; i++)
	{
		camBaseFrameShaker* frameShaker = m_FrameShakers[i];
		if(frameShaker && (frameShaker->GetNameHash() == nameHash))
		{
			desiredFrameShaker = frameShaker;
			break;
		}
	}

	return desiredFrameShaker;
}

bool camBaseCamera::SetShakeAmplitude(float amplitude, int shakeIndex)
{
	bool hasSucceeded = false;

	const int numFrameShakers = m_FrameShakers.GetCount();
	if(shakeIndex < numFrameShakers)
	{
		camBaseFrameShaker* frameShaker = m_FrameShakers[shakeIndex];
		if(frameShaker)
		{
			frameShaker->SetAmplitude(amplitude);
			hasSucceeded = true;
		}
	}

	return hasSucceeded;
}

void camBaseCamera::StopShaking(bool shouldStopImmediately)
{
	//Release or stop the frame shakers in reverse order to simplify clean-up and deletion.
	const int numFrameShakers = m_FrameShakers.GetCount();
	for(int i=numFrameShakers - 1; i>=0; i--)
	{
		RegdCamBaseFrameShaker& frameShaker = m_FrameShakers[i];
		if(frameShaker.Get() != NULL)
		{
			if(shouldStopImmediately)
			{
				//Immediately remove our reference to the shake.
				frameShaker->RemoveRef(m_FrameShakers[i]);
			}
			else
			{
				//Smoothly release the shake.
				frameShaker->Release();
			}
		}

		if(frameShaker.Get() == NULL)
		{
			m_FrameShakers.Delete(i);
		}
	}
}

void camBaseCamera::BypassFrameShakersThisUpdate()
{
	const int numFrameShakers = m_FrameShakers.GetCount();
	for(int frameShakerIndex=0; frameShakerIndex<numFrameShakers; frameShakerIndex++)
	{
		RegdCamBaseFrameShaker& frameShaker = m_FrameShakers[frameShakerIndex];
		if(frameShaker.Get() != NULL)
		{
			frameShaker->BypassThisUpdate();
		}
	}
}

//Clones the specified camera.
//TODO: must change to be a proper clone.
void camBaseCamera::CloneFrom(const camBaseCamera* const sourceCam)
{
	if(sourceCam != NULL)
	{
		if(cameraVerifyf(sourceCam->GetClassId() == GetClassId(), "Attempted to clone incompatible camera types"))
		{
			m_Frame.CloneFrom(sourceCam->GetFrameNoPostEffects());
			m_PostEffectFrame.CloneFrom(sourceCam->GetFrame());

			//Share any frame shakers being used by the source camera.

			StopShaking();

			const int numSourceFrameShakers = sourceCam->m_FrameShakers.GetCount();
			for(int i=0; i<numSourceFrameShakers; i++)
			{
				camBaseFrameShaker* sourceFrameShaker = sourceCam->m_FrameShakers[i];
				if(sourceFrameShaker)
				{
					m_FrameShakers.Grow() = sourceFrameShaker;
				}
			}
		}
	}
}

bool camBaseCamera::GetObjectSpaceCameraMatrix(const CEntity* pTargetEntity, Matrix34& mat) const
{
	// return the offset to the current entity
	mat = GetFrame().GetWorldMatrix();
	mat.DotTranspose(MAT34V_TO_MATRIX34(pTargetEntity->GetMatrix()));
	return true;
}

#if __BANK
void camBaseCamera::AddWidgets(bkBank &bank)
{
	bank.AddSlider("Universal camera motion blur decay rate", &ms_MotionBlurDecayRate, 0.0f, 1.0f, 0.01f);
}

//Render the camera so we can see it.
void camBaseCamera::DebugRender()
{
	//NOTE: Don't render debug info for the active/rendering camera.
	const camBaseCamera* renderingCamera = camInterface::GetDominantRenderedCamera();
	if(this != renderingCamera)
	{
		const camFrame& frame = GetFrame();
		frame.DebugRender(camManager::GetSelectedCamera() == this);

		if(camManager::ms_ShouldRenderCameraInfo)
		{
			DebugRenderInfoTextInWorld(frame.GetPosition()); 
		}
	}


}

const char* camBaseCamera::DebugGetNameText() const
{
	return SAFE_CSTRING(m_Metadata.m_Name.GetCStr());
}

void camBaseCamera::DebugRenderInfoTextInWorld(const Vector3& vPos)
{
	grcDebugDraw::Text(vPos, CRGBA(0,0,0,255),0, grcDebugDraw::GetScreenSpaceTextHeight(), SAFE_CSTRING(m_Metadata.m_Name.GetCStr()) );
}

void camBaseCamera::DebugRender2dTextBox(s32 minTextX, s32 minTextY, s32 maxTextX, s32 maxTextY, Color32 colour)
{
	static s32 charWidth	= 9;
	static s32 charHeight	= 16;

	const float minX = float(minTextX * charWidth) / (float)GRCDEVICE.GetWidth();
	const float minY = float(minTextY * charHeight) / (float)GRCDEVICE.GetHeight();
	const float maxX = float(maxTextX * charWidth) / (float)GRCDEVICE.GetWidth();
	const float maxY = float(maxTextY * charHeight) / (float)GRCDEVICE.GetHeight();

	grcDebugDraw::Quad(Vector2(minX, minY), Vector2(maxX, minY), Vector2(maxX, maxY), Vector2(minX, maxY), colour);
}

void camBaseCamera::DebugAppendDetailedText(char* string, u32 maxLength) const
{
	const char* name = GetName();
	safecatf(string, maxLength, ", %s", name);

	const bool isTheRenderedCamera = camInterface::IsDominantRenderedCamera(*this);
	safecatf(string, maxLength, ", Rendering=%s", isTheRenderedCamera ? "Y" : "N");

	const camFrame& frame	= GetFrame();
	const Vector3& position	= frame.GetPosition();
	safecatf(string, maxLength, ", Position=[%.1f, %.1f, %.1f]", position.x, position.y, position.z);
	safecatf(string, maxLength, ", Clip=[%.2f, %.2f]", frame.GetNearClip(), frame.GetFarClip());
	safecatf(string, maxLength, ", DOF=[%.2f, %.2f]", frame.GetNearInFocusDofPlane(), frame.GetFarInFocusDofPlane());
	safecatf(string, maxLength, ", FOV=%.1f", frame.GetFov());
	safecatf(string, maxLength, ", MB=%.2f", frame.GetMotionBlurStrength());
}
#endif // __BANK
