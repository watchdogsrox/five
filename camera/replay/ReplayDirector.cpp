//
// camera/replay/ReplayDirector.cpp
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#include "camera/replay/ReplayDirector.h"

#if GTA_REPLAY

#include "camera/CamInterface.h"
#include "camera/helpers/FrameInterpolator.h"
#include "camera/helpers/BaseFrameShaker.h"
#include "camera/replay/ReplayFreeCamera.h"
#include "camera/replay/ReplayPresetCamera.h"
#include "camera/system/CameraFactory.h"
#include "camera/system/CameraMetadata.h"
#include "camera/viewports/ViewportManager.h"
#include "control/replay/IReplayMarkerStorage.h"
#include "control/replay/Replay.h"
#include "control/replay/ReplayMarkerInfo.h"
#include "control/replay/ReplayMarkerContext.h"
#include "frontend/VideoEditor/VideoEditorInterface.h"
#include "scene/EntityIterator.h"
#include "scene/portals/PortalTracker.h"
#include "system/controlMgr.h"
#include "peds/PedCapsule.h"

#include "input/mouse.h"

CAMERA_OPTIMISATIONS();

INSTANTIATE_RTTI_CLASS(camReplayDirector,0xC251A33B)

#if __BANK
float camReplayDirector::ms_DebugSmoothSpringConstant = 0.0f;
bool camReplayDirector::ms_DebugWasReplayPlaybackSetupFinished = false;
bool camReplayDirector::ms_DebugShouldSmooth = false;
#endif //  __BANK
bool camReplayDirector::ms_IsReplayPlaybackSetupFinished = false;
bool camReplayDirector::ms_WasReplayPlaybackSetupFinished = false;

//this will allow to filter the custom DOF marker index on the last frame only for project of version 3 or above. 
//Older projects will keep the old behavior that will keep the dof off on the last marker. 
static const u32 s_ReplayProjectVersionToUseDofMarkerIndex = 3;

//-----------------------------------------------------------------------------
int CompareTargetDistanceSqCB(const void* pFirst, const void* pSecond)
{
	ReplayTargetInfo* pFirstTarget  = (ReplayTargetInfo*)pFirst;
	ReplayTargetInfo* pSecondTarget = (ReplayTargetInfo*)pSecond;

	// Multiply by 1000, so we take into account first three decimal places before converting to integer for comparison.
	return (int)(pFirstTarget->TargetDistanceSq * 1000.0f - pSecondTarget->TargetDistanceSq * 1000.0f);
}

camReplayDirector::camReplayDirector(const camBaseObjectMetadata& metadata)
: camBaseDirector(metadata)
, m_Metadata(static_cast<const camReplayDirectorMetadata&>(metadata))
{
	Reset();
}

camReplayDirector::~camReplayDirector()
{
	DestroyAllCameras();
}

bool camReplayDirector::Update()
{
	m_UpdatedCamera = NULL;
	if (CReplayMgr::IsEditModeActive())
	{
		const IReplayMarkerStorage* markerStorage = CReplayMarkerContext::GetMarkerStorage();
		if (markerStorage)
		{
			ms_WasReplayPlaybackSetupFinished	= ms_IsReplayPlaybackSetupFinished;
			ms_IsReplayPlaybackSetupFinished	= CReplayMgr::AreEntitiesSetup();

			const CPed* playerPed = CReplayMgr::GetMainPlayerPtr();
			if (playerPed)
			{
				const Vector3 playerPedPosition = VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition());

				GenerateReplayCameraTargetList(playerPedPosition);
			}

			m_PreviousMarkerIndex	= m_CurrentMarkerIndex;
			m_CurrentMarkerIndex	= GetCurrentMarkerIndex();

			if (m_CurrentMarkerIndex == -1)	
			{
				return false;
			}

			const sReplayMarkerInfo* currentMarkerInfo = markerStorage->TryGetMarker(m_CurrentMarkerIndex);

			//if we're blending, the camera we care about is the camera we're blending from (it's not the active camera)
			const u32 activeCameraHash			= IsCurrentlyBlending() ? static_cast<const camReplayFreeCamera*>(m_ActiveCamera.Get())->GetCameraBlendingFromNameHash() :
													m_ActiveCamera ? m_ActiveCamera->GetNameHash() : 0;
			const u32 currentMarkerCameraHash	= CalculateCurrentCameraHash(currentMarkerInfo);

			const bool isNewCameraHash			= (activeCameraHash != currentMarkerCameraHash);
			const bool hasChangedMarker			= (m_CurrentMarkerIndex != m_PreviousMarkerIndex);

			 //there is an active free camera that has a camera to blend from, but it's marker is set to no blend
			const bool hasRemovedBlend			= (m_ActiveCamera && m_ActiveCamera->GetIsClassId(camReplayFreeCamera::GetStaticClassId()) &&
													(static_cast<const camReplayFreeCamera*>(m_ActiveCamera.Get())->GetCameraBlendingFromNameHash() != 0) &&
													currentMarkerInfo && currentMarkerInfo->GetBlendType() == (u8)MARKER_CAMERA_BLEND_NONE);

			if(hasChangedMarker)
			{
				SetLastCamFrameValid(false); 
				SetLastRelativeSettingsValid(false); 
			}
			
			//this needs to be called before we decide to create new cameras
			const bool hasBlendHaltedOnPreviousUpdate = m_ActiveCamera && m_ActiveCamera->GetIsClassId(camReplayFreeCamera::GetStaticClassId()) &&
														(static_cast<const camReplayFreeCamera*>(m_ActiveCamera.Get())->GetMaxPhaseToHaltBlendMovement() < 1.0f - SMALL_FLOAT);
			if (hasBlendHaltedOnPreviousUpdate)
			{
				m_ShouldResetSmoothing = true;
			}

			if (isNewCameraHash || hasChangedMarker || hasRemovedBlend)
			{
				CreateActiveCamera(currentMarkerCameraHash);
				cameraAssertf(m_ActiveCamera != NULL, "Failed to create replay camera (hash: %u)", currentMarkerCameraHash);

				if (m_ActiveCamera->GetIsClassId(camReplayBaseCamera::GetStaticClassId()))
				{
					//Force the default frame if we've created a different camera, but not because we changed marker
					//NOTE: This should only be when the player is switching camera choice in edit mode, and never during playback
					//We check the previous marker's validity to cover the case when the clip is first loaded

					const u32 newActiveCameraHash		= m_ActiveCamera ? m_ActiveCamera->GetNameHash() : 0;
					const bool isNewCamera				= (activeCameraHash != newActiveCameraHash);
					const bool shouldForceDefaultFrame	= isNewCamera && !hasChangedMarker && (m_PreviousMarkerIndex != -1) && (IsReplayPlaybackSetupFinished() || CReplayMgr::IsFineScrubbing()) && !hasRemovedBlend;

					//we tell the new camera what its marker index is (this may not be the same as CReplayMarkerContext::GetCurrentMarkerIndex())
					static_cast<camReplayBaseCamera*>(m_ActiveCamera.Get())->Init(m_CurrentMarkerIndex, shouldForceDefaultFrame);
				}

				CreateFallbackCamera();
				cameraAssertf(m_FallbackCamera != NULL, "Failed to create fallback camera (hash: %u)", m_Metadata.m_ReplayRecordedCameraRef.GetHash());

				m_FallbackCamera->Init(m_CurrentMarkerIndex);
			}

			//if this marker is marked to blend to the next marker (and we're not already blending), make the next marker the active camera
			const s32 nextMarkerIndex = GetNextMarkerIndex();
			const sReplayMarkerInfo* nextMarkerInfo	= markerStorage->TryGetMarker(nextMarkerIndex);
            bool isBlending = false;
			if (m_ActiveCamera && m_ActiveCamera->GetIsClassId(camReplayFreeCamera::GetStaticClassId()) &&
				nextMarkerInfo && nextMarkerInfo->GetCameraType() == (u8)RPLYCAM_FREE_CAMERA &&
				currentMarkerInfo && currentMarkerInfo->GetBlendType() != (u8)MARKER_CAMERA_BLEND_NONE && !IsCurrentlyBlending() &&
				(IsReplayPlaybackSetupFinished() || CReplayMgr::IsFineScrubbing()))
			{
				//create the next camera, so this camera becomes m_PreviousCamera
				const u32 nextCameraHash = CalculateCurrentCameraHash(nextMarkerInfo);
				if(cameraVerifyf(CreateActiveCamera(nextCameraHash), "Failed to create replay camera (hash: %u)", nextCameraHash))
				{
					static_cast<camReplayBaseCamera*>(m_ActiveCamera.Get())->Init(nextMarkerIndex);
					static_cast<camReplayFreeCamera*>(m_ActiveCamera.Get())->BlendFromCamera(*m_PreviousCamera, m_CurrentMarkerIndex);
                    isBlending = true;
				}
				if (cameraVerifyf(CreateFallbackCamera(), "Failed to create fallback camera (hash: %u)", m_Metadata.m_ReplayRecordedCameraRef.GetHash()))
				{
					m_FallbackCamera->Init(nextMarkerIndex);
				}
			}

            const u32 replayProjectVersion = CVideoEditorInterface::GetActiveProject() ? CVideoEditorInterface::GetActiveProject()->GetCachedProjectVersion() : 0;
            if(replayProjectVersion >= s_ReplayProjectVersionToUseDofMarkerIndex && m_ActiveCamera && (IsReplayPlaybackSetupFinished() || CReplayMgr::IsFineScrubbing()))
            {
                const bool isNextMarkerTheLastMarker    = markerStorage->GetLastMarker() == nextMarkerInfo;
                const bool isCurrentMarkerTheLastMarker = markerStorage->GetLastMarker() == currentMarkerInfo;
                if(isBlending && isNextMarkerTheLastMarker)
                {
                    static_cast<camReplayBaseCamera*>(m_ActiveCamera.Get())->SetDofMarkerIndex(m_CurrentMarkerIndex);
                }
                else if(isCurrentMarkerTheLastMarker)
                {
                    const s32 markerIndex = GetPreviousMarkerIndex(m_CurrentMarkerIndex);
                    static_cast<camReplayBaseCamera*>(m_ActiveCamera.Get())->SetDofMarkerIndex(markerIndex);
                }
            }

			camFrame recordedFrame;
			if (m_FallbackCamera)
			{
				m_FallbackCamera->BaseUpdate(recordedFrame);
			}

			if(m_PreviousCamera)
			{
				if (m_CurrentMarkerIndex < m_PreviousMarkerIndex)
				{
					m_PreviousCameraWasBlending	= currentMarkerInfo && (currentMarkerInfo->GetBlendType() != MARKER_CAMERA_BLEND_NONE);
				}
				else
				{
					const sReplayMarkerInfo* previousMarkerInfo	= markerStorage->TryGetMarker(m_PreviousMarkerIndex);
					m_PreviousCameraWasBlending	= previousMarkerInfo && (previousMarkerInfo->GetBlendType() != MARKER_CAMERA_BLEND_NONE);
				}

				bool shouldUpdatePreviousCamera = IsCurrentlyBlending();
				if(shouldUpdatePreviousCamera)
				{
					//We are interpolating from the previous camera, so we need to update it.
					camFrame dummyFrame;
					const bool hasSucceeded = m_PreviousCamera->BaseUpdate(dummyFrame);

					if(!hasSucceeded)
					{
						shouldUpdatePreviousCamera = false;
					}
				}

				if (!shouldUpdatePreviousCamera)
				{
					if (!IsCurrentlyBlending() && m_ActiveCamera && m_ActiveCamera->GetIsClassId(camReplayFreeCamera::GetStaticClassId()) &&
						static_cast<const camReplayFreeCamera*>(m_ActiveCamera.Get())->GetCameraBlendingFromNameHash() != 0)
					{
						//if the camera's marker info says no blend (IsCurrentlyBlending()) but it has a camera to blend from, we've just changed the menu option
						//instead of deleting the previous camera, we want to make it the active camera.
						delete m_ActiveCamera;
						m_ActiveCamera = m_PreviousCamera;
					}

					//If we no longer need to update the previous camera, we can safely delete it.
					delete m_PreviousCamera;
				}
			}

			camFrame activeFrame;
			if (m_ActiveCamera && m_ActiveCamera->BaseUpdate(activeFrame))
			{
				const camReplayBaseCamera* activeReplayCamera = m_ActiveCamera->GetIsClassId(camReplayBaseCamera::GetStaticClassId()) ? static_cast<const camReplayBaseCamera*>(m_ActiveCamera.Get()) : NULL;

				//NOTE: m_ActiveCamera && m_ActiveCamera->GetIsClassId(camReplayFreeCamera::GetStaticClassId()) has been verified in IsCurrentlyBlending()

                const camReplayFreeCamera* freeCamera = static_cast<const camReplayFreeCamera*>(m_ActiveCamera.Get());
                const bool freeCameraBlendingInvalid = freeCamera->IsBlendingPositionInvalid();
				m_IsFallingBackToRecordedCamera = ( ( IsCurrentlyBlending() && (freeCameraBlendingInvalid || freeCamera->IsPositionOutsidePlayerLimits(activeFrame.GetPosition()))) ||
													(!IsCurrentlyBlending() && activeReplayCamera && activeReplayCamera->ShouldFallbackToGameplayCamera())) &&
													m_FallbackCamera; //if IsReplayPlaybackSetupFinished() is false, m_ActiveCamera->BaseUpdate() would fail, so we don't need to check it here
				
				if (m_IsFallingBackToRecordedCamera)
				{					
					if(!m_OutOfRangeWarningIssued && m_FallbackCamera == m_RenderedCamera)
					{	
                        //if the blending position is invalid don't fire this warning as specific ones have already been fired in the ReplayFreeCamera code.
                        if(!freeCameraBlendingInvalid)
                        {
						    // Flag the warning state so the UI can display some helpful text to the player						
						    CReplayMarkerContext::SetWarning(CReplayMarkerContext::EDIT_WARNING_OUT_OF_RANGE);
                        }
						m_OutOfRangeWarningIssued = true;
					}

					m_UpdatedCamera = m_FallbackCamera;
					m_Frame.CloneFrom(recordedFrame);
				}
				else
				{
					m_OutOfRangeWarningIssued = false;

					m_UpdatedCamera = m_ActiveCamera;
					m_Frame.CloneFrom(activeFrame);
				}

				m_ActiveRenderState	= RS_FULLY_RENDERING;
			}
			else if (m_ActiveCamera && m_ActiveCamera->GetIsClassId(camReplayBaseCamera::GetStaticClassId()) && static_cast<const camReplayBaseCamera*>(m_ActiveCamera.Get())->IsWaitingToInitialize())
			{
				//no warning?
				if (m_ActiveRenderState != RS_NOT_RENDERING)
				{
					m_UpdatedCamera = m_ActiveCamera;
					m_Frame.CloneFrom(m_LastFrame);
				}
				else if (m_FallbackCamera)
				{
					m_UpdatedCamera = m_FallbackCamera;
					m_Frame.CloneFrom(recordedFrame);
				}

				m_ActiveRenderState	= RS_FULLY_RENDERING;
			}

			if (IsShaking())
			{
				SetShakeAmplitude(m_FrameShakerAmplitudeSpring.Update(m_TargetFrameShakerAmplitude, m_Metadata.m_ShakeAmplitudeSpringConstant, m_Metadata.m_ShakeAmplitudeSpringDampingRatio, true));
			}

			return (m_UpdatedCamera.Get() != NULL);
		}
	}

	DestroyAllCameras();
	m_ActiveRenderState = RS_NOT_RENDERING;

	return false;
}

void camReplayDirector::PostUpdate()
{
	camBaseDirector::PostUpdate();

	m_LastFrame.CloneFrom(m_Frame);

	UpdateShake();

	UpdateSmoothing();
}

void camReplayDirector::UpdateShake()
{
	const bool isCurrentlyEditingThisMarker	= (CReplayMarkerContext::GetEditingMarkerIndex() == m_CurrentMarkerIndex);
	if (isCurrentlyEditingThisMarker || !IsReplayPlaybackSetupFinished())
	{
		m_ShakeHash				= 0;
		m_ShakeAmplitude		= 0.5f;
		m_ShakeSpeed			= 0.0f;
		m_Speed					= 0.0f;
		m_IsForcingCameraShake	= false;

		StopShaking();

		return;
	}

	const IReplayMarkerStorage* markerStorage	= CReplayMarkerContext::GetMarkerStorage();
	const sReplayMarkerInfo* currentMarkerInfo	= markerStorage ? markerStorage->TryGetMarker(m_CurrentMarkerIndex) : NULL;
	if (!currentMarkerInfo)
	{
		return;
	}

	const u32 previousShakeHash			= m_ShakeHash;
	const bool previouslyForcedShake	= m_IsForcingCameraShake;

	GetShakeSelection(*currentMarkerInfo, m_ShakeHash, m_ShakeAmplitude, m_ShakeSpeed);
	m_IsForcingCameraShake				= CReplayMarkerContext::ShouldForceCameraShake();
	m_Speed								= currentMarkerInfo->GetSpeedValueFloat();

	//if we're blending we see if the two shakes are the same type and lerp the amplitudes, otherwise we don't do anything
	if (IsCurrentlyBlending())
	{
		//NOTE: m_ActiveCamera has already been verified in IsCurrentlyBlending()
		const camReplayFreeCamera* replayFreeCamera	= static_cast<const camReplayFreeCamera*>(m_ActiveCamera.Get());

		//NOTE: CReplayMarkerContext::GetMarkerStorage()->GetMarker(m_BlendSourceMarkerIndex) is already verified
		const s32 blendMarkerIndex					= replayFreeCamera->GetMarkerIndex();
		const sReplayMarkerInfo* blendMarkerInfo	= CReplayMarkerContext::GetMarkerStorage()->GetMarker(blendMarkerIndex);

		if (blendMarkerInfo)
		{
			u32 blendShakeHash = 0;
			float blendShakeAmplitude = 0.0f;
			float blendShakeSpeed = 0.0f;

			GetShakeSelection(*blendMarkerInfo, blendShakeHash, blendShakeAmplitude, blendShakeSpeed);

			if (blendShakeHash == m_ShakeHash)
			{
				const float currentTime = CReplayMgr::GetCurrentTimeRelativeMsFloat();
				const float phase		= replayFreeCamera->GetCurrentPhaseForBlend(currentTime);

				m_ShakeAmplitude		= Lerp(phase, m_ShakeAmplitude, blendShakeAmplitude);
				m_ShakeSpeed			= Lerp(phase, m_ShakeSpeed, blendShakeSpeed);
			}
		}
	}

	// Set the target amplitude
	m_TargetFrameShakerAmplitude = m_ShakeAmplitude;
	if(m_IsForcingCameraShake)
	{
		m_FrameShakerAmplitudeSpring.Reset(m_ShakeAmplitude); // TODO: Look at smoothing this out, but not important as the pop communicates that something has changed.
	}

	// Trigger the shake if... 
	if(m_ShakeHash != previousShakeHash ||					// the shake hash has changed,
		m_IsForcingCameraShake != previouslyForcedShake ||  // or we're forcing the shake (via menu preview),
		(m_IsForcingCameraShake && !m_FrameShaker))			// or we're forcing the shake but have stopped for some reason (affects one-shots)
	{
		//Try to kick-off the specified shake.
		StopShaking();
		Shake(m_ShakeHash, m_ShakeAmplitude);
	}

	// Update the shaker time source, scaling and offset (applying time warping when not previewing)
	if(m_FrameShaker)
	{
		m_FrameShaker->SetUseNonPausedCameraTime(m_IsForcingCameraShake); // if we're forcing shakes (previewing), we want to use non-paused camera time
		m_FrameShaker->SetTimeScaleFactor(m_IsForcingCameraShake ? m_ShakeSpeed : 1.0f); // and apply time scaling to reflect the chosen shake speed

		if(m_IsForcingCameraShake)
		{
			if(m_IsForcingCameraShake != previouslyForcedShake)
			{
				// Force the envelope start time to the current NonPausedCameraTime, once, when the user starts previewing
				m_FrameShaker->OverrideStartTime(fwTimer::GetNonPausableCamTimeInMilliseconds());
			}

			// And apply no time offset during preview
			m_FrameShaker->SetTimeOffset(0);
		}
		else
		{
			UpdateShakeTimeWarp();
		}
	}
}

void camReplayDirector::UpdateShakeTimeWarp()
{
	if(!m_FrameShaker)
	{
		return;
	}

	const IReplayMarkerStorage* markerStorage = CReplayMarkerContext::GetMarkerStorage();
	if(!markerStorage)
	{
		return;
	}

	const sReplayMarkerInfo* currentMarkerInfo	= markerStorage->TryGetMarker(m_CurrentMarkerIndex);
	const sReplayMarkerInfo* previousMarkerInfo	= markerStorage->TryGetMarker(GetPreviousMarkerIndex(m_CurrentMarkerIndex));

	if(!currentMarkerInfo)
	{
		return;
	}
		
	s32 startingShakeMarkerIndex;
	if(!previousMarkerInfo)
	{
		startingShakeMarkerIndex = 0; // We know we're at the start if there's no previous marker.
	}
	else if(m_FrameShaker->IsOneShot() &&
		GetMarkerShakeHash((eMarkerShakeType)previousMarkerInfo->GetShakeType()).GetHash() == m_ShakeHash && 
		currentMarkerInfo->GetShakeSpeedValue() == previousMarkerInfo->GetShakeSpeedValue())
	{
		// If this is a one-shot shake with a previous marker of the same shake type and speed, we do not treat it as contiguous...
		startingShakeMarkerIndex = m_CurrentMarkerIndex; // so we started shaking at this marker.
	}
	else
	{	
		// Find the first shake of the same hash to compute the correct OverrideStartTime value
		for(startingShakeMarkerIndex = m_CurrentMarkerIndex; startingShakeMarkerIndex > -1; --startingShakeMarkerIndex)
		{
			const sReplayMarkerInfo* markerInfo = markerStorage->TryGetMarker(startingShakeMarkerIndex);
			if(markerInfo && !markerInfo->IsAnchor())
			{
				const u32 shakeHash = GetMarkerShakeHash((eMarkerShakeType)markerInfo->GetShakeType()).GetHash();
				if(shakeHash != m_ShakeHash)
				{
					startingShakeMarkerIndex++;
					break;
				}
			}
		}
	}

	float shakeStartTime = 0;
	{
		const sReplayMarkerInfo* markerInfo = markerStorage->TryGetMarker(startingShakeMarkerIndex);
		if(markerInfo)
		{
			shakeStartTime = markerInfo->GetNonDilatedTimeMs();
		}
	}

	// Compute the accumulated time since the start of the shake both scaled and unscaled
	float sumScaledTime	= 0.0f;
	float sumTime		= 0.0f;
	for(s32 shakeMarkerIndex = startingShakeMarkerIndex; shakeMarkerIndex < m_CurrentMarkerIndex; ++shakeMarkerIndex)
	{
		const sReplayMarkerInfo* markerInfo		= markerStorage->TryGetMarker(shakeMarkerIndex);
		const sReplayMarkerInfo* nextMarkerInfo	= markerStorage->TryGetMarker(GetNextMarkerIndex(shakeMarkerIndex));
		if(markerInfo && nextMarkerInfo)
		{
			const u32 shakeHash = GetMarkerShakeHash((eMarkerShakeType)markerInfo->GetShakeType()).GetHash();
			if(shakeHash != m_ShakeHash)
			{
				break;
			}

			const float timeScaleFactor	= markerInfo->GetShakeSpeedValueFloat() / markerInfo->GetSpeedValueFloat();
			const float markerDuration	= nextMarkerInfo->GetNonDilatedTimeMs() - markerInfo->GetNonDilatedTimeMs();

			sumScaledTime	+= (float)markerDuration * timeScaleFactor;
			sumTime			+= (float)markerDuration;
		}
	}

	// Include the time we're at on the current marker	
	const u32 shakeHash = GetMarkerShakeHash((eMarkerShakeType)currentMarkerInfo->GetShakeType()).GetHash();
	if(shakeHash == m_ShakeHash)
	{
		const float timeScaleFactor = currentMarkerInfo->GetShakeSpeedValueFloat() / currentMarkerInfo->GetSpeedValueFloat();

		const float markerStartTime		= currentMarkerInfo->GetNonDilatedTimeMs();
		const float timeInCurrentMarker	= CReplayMgr::GetCurrentTimeRelativeMsFloat() - markerStartTime;

		sumScaledTime	+= (float)timeInCurrentMarker * timeScaleFactor;
		sumTime			+= (float)timeInCurrentMarker;
	}

	m_FrameShaker->OverrideStartTime(CReplayMgr::GetPlayBackStartTimeAbsoluteMs() + (u32)shakeStartTime);
	m_FrameShaker->SetTimeOffset((s32)(sumScaledTime - sumTime));
}

void camReplayDirector::GetShakeSelection(const sReplayMarkerInfo& markerInfo, u32& shakeHash, float& shakeAmplitude, float& shakeSpeed) const
{
	shakeHash = 0;

	float shakeIntensity = DEFAULT_MARKER_SHAKE_INTENSITY;

	if (markerInfo.GetShakeType() != MARKER_SHAKE_NONE)
	{
		shakeIntensity = (float)markerInfo.GetShakeIntensity();
		shakeHash = GetMarkerShakeHash((eMarkerShakeType)markerInfo.GetShakeType());
	}

	//calculate the shake amplitude from shake intensity using a curve
	shakeAmplitude = (m_Metadata.m_ShakeIntensityToAmplitudeCurve.x * shakeIntensity * shakeIntensity) + (m_Metadata.m_ShakeIntensityToAmplitudeCurve.y * shakeIntensity) + m_Metadata.m_ShakeIntensityToAmplitudeCurve.z;

	//calculate the shake timescale factor, to keep shakes independent of clip speed and allow user control
	shakeSpeed = markerInfo.GetShakeSpeedValueFloat();
}

atHashString camReplayDirector::GetMarkerShakeHash(eMarkerShakeType shakeType) const
{
	switch (shakeType)
	{
	case MARKER_SHAKE_AIR_TURBULANCE:
		return m_Metadata.m_ReplayAirTurbulenceShakeRef;
	case MARKER_SHAKE_GROUND_VIBRATION:
		return m_Metadata.m_ReplayGroundVibrationShakeRef;
	case MARKER_SHAKE_DRUNK:
		return m_Metadata.m_ReplayDrunkShakeRef;
	case MARKER_SHAKE_HAND:
		return m_Metadata.m_ReplayHandheldShakeRef;
	case MARKER_SHAKE_EXPLOSION:
		return m_Metadata.m_ReplayExplosionShakeRef;
	default:
		return atHashString(u32(0));
	}
}

camBaseFrameShaker* camReplayDirector::Shake(u32 shakeHash, float amplitude)
{
	if (shakeHash == 0)
	{
		return NULL;
	}

	m_FrameShaker = camFactory::CreateObject<camBaseFrameShaker>(shakeHash);

	if (m_FrameShaker)
	{
		m_FrameShaker->SetAmplitude(amplitude);
		m_TargetFrameShakerAmplitude = amplitude;
		m_FrameShakerAmplitudeSpring.Reset(amplitude);
	}

	return m_FrameShaker;
}

void camReplayDirector::UpdateSmoothing()
{
	BANK_ONLY(ms_DebugSmoothSpringConstant = 0.0f;)

	if (!m_UpdatedCamera || m_UpdatedCamera->GetIsClassId(camReplayRecordedCamera::GetStaticClassId()))
	{
		return;
	}

	const IReplayMarkerStorage* markerStorage	= CReplayMarkerContext::GetMarkerStorage();
	const sReplayMarkerInfo* currentMarkerInfo	= markerStorage ? markerStorage->TryGetMarker(m_CurrentMarkerIndex) : NULL;
	if (!currentMarkerInfo)
	{
		return;
	}

	bool smoothingValid = false;

	// we should only smooth if the next two markers are valid, and smoothing is enabled on the middle one
	// OR the previous and next markers are valid, and smoothing is enabled on this one.

	// get the previous marker
	const sReplayMarkerInfo* prevMarkerInfo	= markerStorage ? markerStorage->TryGetMarker(GetPreviousMarkerIndex()) : NULL;
	// get the next marker
	const s32 nextMarkerIndex = GetNextMarkerIndex();
	const sReplayMarkerInfo* nextMarkerInfo	= markerStorage ? markerStorage->TryGetMarker(nextMarkerIndex) : NULL;
	// get the next-next marker
	const s32 nextNextMarkerIndex = nextMarkerIndex > -1 ? GetNextMarkerIndex(nextMarkerIndex) : -1;
	const sReplayMarkerInfo* nextNextMarkerInfo	= markerStorage ? markerStorage->TryGetMarker(nextNextMarkerIndex) : NULL;

	float smoothingBlend = 1.0f;

	const u32 currentTime	= CReplayMgr::GetCurrentTimeRelativeMs();
    float phase				= 0.0f;

	if (prevMarkerInfo && nextMarkerInfo && prevMarkerInfo->GetBlendType()!=(u8)MARKER_CAMERA_BLEND_NONE && currentMarkerInfo->GetBlendType()!=(u8)MARKER_CAMERA_BLEND_NONE)
	{
		smoothingValid = (currentMarkerInfo->HasSmoothBlend());

		if (smoothingValid && m_Metadata.m_SmoothingBlendOutPhase>0.0f && (nextMarkerInfo->GetBlendType()==MARKER_CAMERA_BLEND_NONE || !nextMarkerInfo->HasSmoothBlend() || nextNextMarkerInfo==NULL))
		{
			// We've passed the last marker with smoothing enabled on it.
			// Blend out over the second half of the distance between the two markers
			const float startTime = currentMarkerInfo->GetNonDilatedTimeMs();
			const float endTime = nextMarkerInfo->GetNonDilatedTimeMs();
			const float currentTime	= (float)CReplayMgr::GetCurrentTimeRelativeMs();

			if ((currentTime > startTime) && (endTime > startTime))
			{
				phase = (currentTime - startTime) / (endTime - startTime);
			}

			smoothingBlend = 1.0f - Clamp((phase/m_Metadata.m_SmoothingBlendInPhase)-((1.0f/m_Metadata.m_SmoothingBlendInPhase)-1.0f), 0.0f, 1.0f);
		}
	}
	else if (nextMarkerInfo && nextNextMarkerInfo && currentMarkerInfo->GetBlendType()!=(u8)MARKER_CAMERA_BLEND_NONE && nextMarkerInfo->GetBlendType()!=(u8)MARKER_CAMERA_BLEND_NONE)
	{
		smoothingValid = (nextMarkerInfo->HasSmoothBlend());

		if (smoothingValid && m_Metadata.m_SmoothingBlendInPhase>0.0f)
		{
			// We're approaching the first marker with smoothing on it.
			// Blend in over the first half of the distance between the two markers
			const float startTime = currentMarkerInfo->GetNonDilatedTimeMs();
			const float endTime = nextMarkerInfo->GetNonDilatedTimeMs();
			const float currentTime	= (float)CReplayMgr::GetCurrentTimeRelativeMs();

			if ((currentTime > startTime) && (endTime > startTime))
			{
				phase = (currentTime - startTime) / (endTime - startTime);
			}

			smoothingBlend =  Clamp(phase/m_Metadata.m_SmoothingBlendInPhase, 0.0f, 1.0f);
		}
	}
	else if (m_ForceSmoothingStartTime != 0 && (m_ForceSmoothingStartTime<=currentTime))
	{
		if (currentTime >= (m_ForceSmoothingStartTime+m_ForceSmoothingDuration))
		{
			// Force smoothing is complete. reset it.
			DisableForceSmoothing();
		}
		else
		{
			// work out the blend (if any) based on the blend in and out durations, and the current time.
			u32 blendOutStartTime = (m_ForceSmoothingStartTime+m_ForceSmoothingDuration-m_ForceSmoothingBlendOutDuration);
			if (m_ForceSmoothingBlendInDuration>0.0f && currentTime<(m_ForceSmoothingStartTime+m_ForceSmoothingBlendInDuration))
			{
				// forced blend in
				smoothingBlend = Clamp((((currentTime - (float)m_ForceSmoothingStartTime)) / (float)m_ForceSmoothingBlendInDuration), 0.0f, 1.0f);
			}
			else if (m_ForceSmoothingBlendOutDuration > 0.0f && currentTime > blendOutStartTime)
			{
				// forced blend out
				smoothingBlend = 1.0f - Clamp((((currentTime - (float)blendOutStartTime)) / (float)m_ForceSmoothingBlendOutDuration), 0.0f, 1.0f);
			}

			// Smoothing needs to be enabled.
			smoothingValid = true;
		}
	}

	if (!smoothingValid)
	{
		m_ShouldResetSmoothing = true;
	}

	if (!IsReplayPlaybackSetupFinished() || WasReplayPlaybackSetupFinishedThisUpdate())
	{
		m_ShouldResetSmoothing = true;
	}

	const bool isInEditMode = (CReplayMarkerContext::GetEditingMarkerIndex() != -1);
	if (isInEditMode)
	{
		m_ShouldResetSmoothing = true;
	}

	//we ignore cuts if we're blending between cameras
	const bool shouldSmooth			= (BANK_ONLY(ms_DebugShouldSmooth || )(smoothingValid)) && CReplayMgr::GetPlaybackFrameStepDeltaMsFloat() >= SMALL_FLOAT; //not rewinding
	const bool hasCutBeenFlagged	= (m_UpdatedCamera->GetNumUpdatesPerformed() == 0) || (m_PostEffectFrame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) || m_PostEffectFrame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation));
	const bool shouldCutSpring		= (hasCutBeenFlagged && !m_PreviousCameraWasBlending) || m_ShouldResetSmoothing;
	const float springConstant		= (shouldCutSpring || !shouldSmooth) ? 0.0f : Lerp(smoothingBlend, m_Metadata.m_MinBlendedSmoothingSpringConstant, m_Metadata.m_FullyBlendedSmoothingSpringConstant);
	BANK_ONLY(ms_DebugSmoothSpringConstant = springConstant;)

    //compute the fade smoothing in case we are getting in or out a smoothed section of the clip.
    float fadeSmoothingRatio = 1.0f;

    //are we smooth blending this marker? Jump this if we are already force smoothing the frame or if we shouldn't smooth at all
    if( (m_ForceSmoothingStartTime == 0 || (m_ForceSmoothingStartTime>currentTime)) && 
        currentMarkerInfo && nextMarkerInfo &&
        currentMarkerInfo->GetBlendType()==(u8)MARKER_CAMERA_BLEND_SMOOTH)
    {
        if((phase > m_Metadata.m_FadeSmoothingBlendOutBegin && nextMarkerInfo->GetBlendType()!=(u8)MARKER_CAMERA_BLEND_SMOOTH))
        {
            //blend out
            fadeSmoothingRatio = RampValueSafe(phase, m_Metadata.m_FadeSmoothingBlendOutBegin, 1.0f, 1.0f, 0.0f);
        }
    }

    const Matrix34 desiredMatrix = m_PostEffectFrame.GetWorldMatrix();
	Matrix34 dampedMatrix;
	ApplyRotationalDampingToMatrix(desiredMatrix, springConstant, m_Metadata.m_SmoothingSpringDampingRatio, dampedMatrix);
	
    Matrix34 resultMatrix;
    camFrame::SlerpOrientation(fadeSmoothingRatio, desiredMatrix, dampedMatrix, resultMatrix);

	m_PostEffectFrame.SetWorldMatrix(resultMatrix);

	const float desiredPositionX	= m_PostEffectFrame.GetPosition().x;
	const float desiredPositionY	= m_PostEffectFrame.GetPosition().y;
	const float desiredPositionZ	= m_PostEffectFrame.GetPosition().z;

	float dampedPositionX			= m_SmoothingPositionXSpring.Update(desiredPositionX, springConstant, m_Metadata.m_SmoothingSpringDampingRatio, true, true);
	float dampedPositionY			= m_SmoothingPositionYSpring.Update(desiredPositionY, springConstant, m_Metadata.m_SmoothingSpringDampingRatio, true, true);
	float dampedPositionZ			= m_SmoothingPositionZSpring.Update(desiredPositionZ, springConstant, m_Metadata.m_SmoothingSpringDampingRatio, true, true);

    dampedPositionX                 = Lerp(fadeSmoothingRatio, desiredPositionX, dampedPositionX);
    dampedPositionY                 = Lerp(fadeSmoothingRatio, desiredPositionY, dampedPositionY);
    dampedPositionZ                 = Lerp(fadeSmoothingRatio, desiredPositionZ, dampedPositionZ);

	m_PostEffectFrame.SetPosition(Vector3(dampedPositionX, dampedPositionY, dampedPositionZ));

	const float desiredFov	= m_PostEffectFrame.GetFov();
	float dampedFov	        = m_SmoothingFovSpring.Update(desiredFov, springConstant, m_Metadata.m_SmoothingSpringDampingRatio, true, true);
    dampedFov               = Lerp(fadeSmoothingRatio, desiredFov, dampedFov);

	m_PostEffectFrame.SetFov(dampedFov);

	m_ShouldResetSmoothing = false;
}

void camReplayDirector::ApplyRotationalDampingToMatrix(const Matrix34& sourceMatrix, const float springConstant, const float dampingRatio, Matrix34& destinationMatrix)
{
	float desiredHeading, desiredPitch, desiredRoll;
	camFrame::ComputeHeadingPitchAndRollFromMatrixUsingEulers(sourceMatrix, desiredHeading, desiredPitch, desiredRoll);

	if(springConstant < SMALL_FLOAT)
	{
		m_SmoothingHeadingSpring.Reset(desiredHeading);
		m_SmoothingPitchSpring.Reset(desiredPitch);
		m_SmoothingRollSpring.Reset(desiredRoll);

		destinationMatrix = sourceMatrix;
		return;
	}
	
	//Ensure that we blend to the desired orientation over the shortest angle.
	float unwrappedDesiredHeading		= desiredHeading;
	const float headingOnPreviousUpdate	= m_SmoothingHeadingSpring.GetResult();
	const float desiredHeadingDelta		= desiredHeading - headingOnPreviousUpdate;
	if(desiredHeadingDelta > PI)
	{
		unwrappedDesiredHeading -= TWO_PI;
	}
	else if(desiredHeadingDelta < -PI)
	{
		unwrappedDesiredHeading += TWO_PI;
	}

	float dampedHeading			= m_SmoothingHeadingSpring.Update(unwrappedDesiredHeading, springConstant, dampingRatio, true, true);
	dampedHeading				= fwAngle::LimitRadianAngle(dampedHeading);
	m_SmoothingHeadingSpring.OverrideResult(dampedHeading);

	float dampedPitch			= m_SmoothingPitchSpring.Update(desiredPitch, springConstant, dampingRatio, true, true);
	m_SmoothingPitchSpring.OverrideResult(dampedPitch);

	//Ensure that we blend to the desired orientation over the shortest angle.
	float unwrappedDesiredRoll			= desiredRoll;
	const float rollOnPreviousUpdate	= m_SmoothingRollSpring.GetResult();
	const float desiredRollDelta		= desiredRoll - rollOnPreviousUpdate;
	if(desiredRollDelta > PI)
	{
		unwrappedDesiredRoll -= TWO_PI;
	}
	else if(desiredRollDelta < -PI)
	{
		unwrappedDesiredRoll += TWO_PI;
	}

	float dampedRoll				= m_SmoothingRollSpring.Update(unwrappedDesiredRoll, springConstant, dampingRatio, true, true);
	dampedRoll						= fwAngle::LimitRadianAngle(dampedRoll);
	m_SmoothingRollSpring.OverrideResult(dampedRoll);

	camFrame::ComputeWorldMatrixFromHeadingPitchAndRoll(dampedHeading, dampedPitch, dampedRoll, destinationMatrix);
}

u32 camReplayDirector::CalculateCurrentCameraHash(const sReplayMarkerInfo* currentMarkerInfo) const
{
	// Use the default recorded camera if we're not allow to change. (e.g. player out of control/cut scenes)
	if(CReplayMgr::IsPlaybackFlagsSet(FRAME_PACKET_DISABLE_CAMERA_MOVEMENT))
	{
		return m_Metadata.m_ReplayRecordedCameraRef;
	}

	// Use current marker from replay editor to set the current camera type.
	const u32 mode = currentMarkerInfo ? currentMarkerInfo->GetCameraType() : RPLYCAM_RECORDED;

	u32 cameraHash = 0;
	switch (mode)
	{
		case RPLYCAM_FRONT_LOW:
			cameraHash = m_Metadata.m_PresetFrontRef;
			break;
		case RPLYCAM_REAR:
			cameraHash = m_Metadata.m_PresetRearRef;
			break;
		case RPLYCAM_RIGHT_SIDE:
			cameraHash = m_Metadata.m_PresetRightRef;
			break;
		case RPLYCAM_LEFT_SIDE:
			cameraHash = m_Metadata.m_PresetLeftRef;
			break;
		case RPLYCAM_OVERHEAD:
			cameraHash = m_Metadata.m_PresetOverheadRef;
			break;

		case RPLYCAM_FREE_CAMERA:
			cameraHash = m_Metadata.m_ReplayFreeCameraRef;
			break;

		case RPLYCAM_RECORDED:
			// fall through
		default:
			cameraHash = m_Metadata.m_ReplayRecordedCameraRef;
			break;
	}

	return (cameraHash);
}

rage::s32 camReplayDirector::GetCurrentMarkerIndex()
{
	//NOTE: markerStorage has already been verified.
	const IReplayMarkerStorage* markerStorage = CReplayMarkerContext::GetMarkerStorage();

	s32 currentMarkerIndex = CReplayMarkerContext::GetCurrentMarkerIndex();

	//if the previous marker isn't blending, we ignore the end marker
	const s32 numMarkers						= markerStorage->GetMarkerCount();
	const s32 secondFromEndMarkerIndex			= GetPreviousMarkerIndex(numMarkers - 1);
	const bool isSecondFromEndMarkerBlending	= markerStorage->TryGetMarker(secondFromEndMarkerIndex) && markerStorage->TryGetMarker(secondFromEndMarkerIndex)->HasBlend();

	if (currentMarkerIndex == numMarkers - 1 && !isSecondFromEndMarkerBlending)
	{
		--currentMarkerIndex; //we skip the end marker and pretend it's not there
	}

	//we start at what the replay system says our current marker is, and work backwards until we find a non-anchor marker
	for (s32 i = currentMarkerIndex; i >= 0; --i)
	{
		const sReplayMarkerInfo* nextMarkerInfo = markerStorage->TryGetMarker(i);
		if (nextMarkerInfo && !nextMarkerInfo->IsAnchor() )
		{
			return i;
		}
	}

	return -1;
}

rage::s32 camReplayDirector::GetNextMarkerIndex() const
{
	return GetNextMarkerIndex(m_CurrentMarkerIndex);
}

rage::s32 camReplayDirector::GetNextMarkerIndex(s32 markerIndex)
{
	//NOTE: markerStorage has already been verified.
	const IReplayMarkerStorage* markerStorage = CReplayMarkerContext::GetMarkerStorage();

	//if the second from the end marker isn't blending, we ignore the end marker
	s32 numMarkers								= markerStorage->GetMarkerCount();
	const s32 secondFromEndMarkerIndex			= GetPreviousMarkerIndex(numMarkers - 1);
	const bool isSecondFromEndMarkerBlending	= markerStorage->TryGetMarker(secondFromEndMarkerIndex) && markerStorage->TryGetMarker(secondFromEndMarkerIndex)->HasBlend();

	if (!isSecondFromEndMarkerBlending)
	{
		--numMarkers;
	}

	//we start at our next marker, and work forwards until we find a non-anchor marker (we ignore the end marker however)
	for (s32 i = markerIndex + 1; i < numMarkers; ++i)
	{
		const sReplayMarkerInfo* nextMarkerInfo = markerStorage->TryGetMarker(i);
		if (nextMarkerInfo && !nextMarkerInfo->IsAnchor() )
		{
			return i;
		}
	}

	return -1;
}

rage::s32 camReplayDirector::GetPreviousMarkerIndex() const
{
	return GetPreviousMarkerIndex(m_CurrentMarkerIndex);
}

rage::s32 camReplayDirector::GetPreviousMarkerIndex(s32 markerIndex)
{
	//NOTE: markerStorage has already been verified.
	const IReplayMarkerStorage* markerStorage = CReplayMarkerContext::GetMarkerStorage();	

	//we start at our next marker, and work forwards until we find a non-anchor marker
	for (s32 i = markerIndex - 1; i > -1; --i)
	{
		const sReplayMarkerInfo* nextMarkerInfo = markerStorage->TryGetMarker(i);
		if (nextMarkerInfo && !nextMarkerInfo->IsAnchor() )
		{
			return i;
		}
	}

	return -1;
}

void camReplayDirector::Reset()
{
	BaseReset();

	camFrame defaultFrame;

	m_LastFrame.CloneFrom(defaultFrame);

	m_LastValidFreeCamFrame.CloneFrom(defaultFrame);

	m_LastValidFreeCamDesiredPos					= g_InvalidPosition;

	m_FrameShakerAmplitudeSpring.Reset();

	m_ShouldResetSmoothing							= true;

	m_ShakeHash										= 0;
	m_CurrentMarkerIndex							= -1;
	m_PreviousMarkerIndex							= -1;

	m_ValidTargetCount								= 0;
	DisableForceSmoothing();

	m_TargetFrameShakerAmplitude					= 0.0f;
	m_LastValidFreeCamAttachEntityRelativeHeading	= 0.0f; 
	m_LastValidFreeCamAttachEntityRelativePitch		= 0.0f; 
	m_LastValidFreeCamAttachEntityRelativeRoll		= 0.0f; 
	m_ShakeAmplitude								= 0.5f;
	m_ShakeSpeed									= 0.0f;
	m_Speed											= 0.0f;

	m_OutOfRangeWarningIssued						= false;
	m_HaveLastValidFreeCamFrame						= false; 
	m_HaveLastValidFreeCamRelativeSettings			= false;
	m_PreviousCameraWasBlending						= false;
	m_IsForcingCameraShake							= false;
	m_IsFallingBackToRecordedCamera					= false;

#if __BANK
	ms_DebugShouldSmooth							= false;
#endif // __BANK

	DestroyAllCameras();
}

bool camReplayDirector::CreateActiveCamera(u32 cameraHash)
{
	if(m_PreviousCamera)
	{
		//The active camera must have been interpolating, so clean-up the interpolation source camera so we don't leak it.
		delete m_PreviousCamera;
	}

	camBaseCamera* newCamera = camFactory::CreateObject<camBaseCamera>(cameraHash);
	if (newCamera)
	{
		m_PreviousCamera	= m_ActiveCamera;
		m_ActiveCamera		= newCamera;
	}

	return (newCamera != NULL);
}

bool camReplayDirector::CreateFallbackCamera()
{
	if (m_FallbackCamera)
	{
		delete m_FallbackCamera;
	}

	camReplayRecordedCamera* newCamera = camFactory::CreateObject<camReplayRecordedCamera>(m_Metadata.m_ReplayRecordedCameraRef.GetHash());
	if (newCamera)
	{
		m_FallbackCamera = newCamera;
	}

	return (newCamera != NULL);
}

void camReplayDirector::DestroyAllCameras()
{
	if (m_ActiveCamera)
	{
		delete m_ActiveCamera;
	}

	//NOTE: The previous camera must be destroyed last, in case an active interpolation was referencing it.
	if(m_PreviousCamera)
	{
		delete m_PreviousCamera;
	}

	if(m_FallbackCamera)
	{
		delete m_FallbackCamera;
	}
}

void camReplayDirector::GenerateReplayCameraTargetList(const Vector3& vCenterPos)
{
	float    fFurthestPedDistSq		= LARGE_FLOAT;
	s32      sValidTargetCount		= 0;
	s32      sFurthestPedIndex		= 0;

	Vec3V vIteratorPos = VECTOR3_TO_VEC3V(vCenterPos);
	CEntityIterator entityIterator(IteratePeds, NULL, &vIteratorPos, m_Metadata.m_MaxTargetDistanceFromCenter);

	const bool checkPedInsideStiltApartment = CPortalTracker::IsPlayerInsideStiltApartment();

	CEntity* nextEntity = entityIterator.GetNext();
	while (nextEntity)
	{
		// Ignore dead peds.
		if (nextEntity->GetUsedInReplay() && nextEntity->GetIsTypePed() && !((CPed*)nextEntity)->IsDead())
		{
			//if the player ped is inside the stilt apartment and the ped is inside skip it.
			const bool skipPedOutSideStiltPartment = checkPedInsideStiltApartment && !CPortalTracker::IsPedInsideStiltApartment(static_cast<CPed*>(nextEntity));

			// Only consider peds within a certain room distance from the player (vCenterPos)
			const Vector3& entityPosition = VEC3V_TO_VECTOR3(nextEntity->GetTransform().GetPosition());
			if ((IsReplayPlaybackSetupFinished() && CPortalTracker::ArePositionsWithinRoomDistance(vCenterPos, entityPosition)) && !skipPedOutSideStiltPartment)
			{
				// We don't allow fish to be on the list, unless it's the player
				const CBaseCapsuleInfo* pCapsule = ((CPed*)nextEntity)->GetCapsuleInfo();
				if (pCapsule && (!pCapsule->IsFish() || static_cast<CPed*>(nextEntity)->IsPlayer()))
				{
					const Vector3 entityPosition     = VEC3V_TO_VECTOR3(nextEntity->GetTransform().GetPosition());
					const float fSqDistance2ToEntity = vCenterPos.Dist2(entityPosition);

					if (sValidTargetCount < MAX_PEDS_TRACKED)
					{
						m_aoValidTarget[sValidTargetCount].TargetID		   = ((CPed*)nextEntity)->GetReplayID();
						m_aoValidTarget[sValidTargetCount].TargetDistanceSq = fSqDistance2ToEntity;

						sValidTargetCount++;
					}
					else // if (sValidTargetCounter < MAX_PEDS_TRACKED)
					{
						fFurthestPedDistSq  = fSqDistance2ToEntity;
						sFurthestPedIndex	= -1;

						// Find the valid ped in our target list that is the further away than the new valid ped.
						for (s32 j = 0; j < MAX_PEDS_TRACKED; j++)
						{
							if (m_aoValidTarget[j].TargetDistanceSq > fFurthestPedDistSq)
							{
								fFurthestPedDistSq = m_aoValidTarget[j].TargetDistanceSq;
								sFurthestPedIndex = j;
							}
						}

						// If new target is closer than the furthest target in our list, replace with the new one.
						if (sFurthestPedIndex >= 0)
						{
							m_aoValidTarget[sFurthestPedIndex].TargetID			= ((CPed*)nextEntity)->GetReplayID();
							m_aoValidTarget[sFurthestPedIndex].TargetDistanceSq	= fSqDistance2ToEntity;
						}
					}
				}
			}
		}

		nextEntity = entityIterator.GetNext();
	}

	// Clear the rest of the list if we did not fill it.
	for (s32 i = sValidTargetCount; i < MAX_PEDS_TRACKED; i++)
	{
		m_aoValidTarget[i].TargetID = -1;
		m_aoValidTarget[i].TargetDistanceSq = LARGE_FLOAT;
	}

	// Sort the list by distance.
	qsort(m_aoValidTarget, sValidTargetCount, sizeof(ReplayTargetInfo), CompareTargetDistanceSqCB);

	m_ValidTargetCount = sValidTargetCount;
}

void camReplayDirector::FindTargetInList(s32 TargetReplayId, s32& ListIndex, s32& LastValidIndex) const
{
	Vector3 vTargetBubbleCenter = m_Frame.GetWorldMatrix().d;
	if (CReplayMgr::GetMainPlayerPtr())
	{
		Matrix34 playerMatrix;
		CReplayMgr::GetMainPlayerPtr()->GetMatrixCopy(playerMatrix);
		vTargetBubbleCenter = playerMatrix.d;
	}

	// Find target within the target list (if we have one) and find the last valid target in the list. (in case the list is not full)
	LastValidIndex = m_ValidTargetCount-1;
	for (s32 i = 0; i <= LastValidIndex; i++)
	{
		if (TargetReplayId == m_aoValidTarget[i].TargetID)
		{
			ListIndex = i;
			break;
		}
	}
}

s32 camReplayDirector::IncrementTarget(const s32 TargetReplayId, bool shouldIncrease, s32& targetIndex, bool canSelectNone /*= true*/) const
{
	Assert(CReplayMgr::GetMainPlayerPtr());

	targetIndex					= -1;
	s32   sMatchingIndexInList  = MAX_PEDS_TRACKED;
	s32   sLastValidIndexInList = MAX_PEDS_TRACKED;
	s32   sCurrentTargetID      = TargetReplayId;
	//s32   prevTargetID          = sCurrentTargetID;

	// Update the target list.
	FindTargetInList(TargetReplayId, sMatchingIndexInList, sLastValidIndexInList);

	// If, at the time the user tries to change the target, we still cannot find the previous one in the list,
	// treat as if the target is the player and increment from that point in the list.
	if (sCurrentTargetID < 0 || sMatchingIndexInList == MAX_PEDS_TRACKED)
	{
		if (shouldIncrease)
			targetIndex = 0;
		else
			targetIndex = sLastValidIndexInList;
	}
	else
	{
		targetIndex = sMatchingIndexInList + (shouldIncrease ? 1 : -1);
		if (targetIndex < (canSelectNone ? -1 : 0))
			targetIndex = sLastValidIndexInList;
		else if (targetIndex > sLastValidIndexInList)
			targetIndex = (canSelectNone ? -1 : 0);

		if (targetIndex >= 0)
		{
			// Only allow peds that are faded in to be selectable. (array index will remain the same)
			CEntity* pEntity = (CEntity*)CReplayMgr::GetEntity(m_aoValidTarget[targetIndex].TargetID);

			/*TODO4FIVE
			while (pEntity && pEntity->GetAlphaFade() < 250)
			{
				targetIndex += 1;
				if (targetIndex < -1)
					targetIndex = sLastValidIndexInList;
				else if (targetIndex > sLastValidIndexInList)
					targetIndex = -1;

				if (targetIndex >= 0)
					pEntity = (CEntity*)CReplayMgr::GetEntity(REPLAYIDTYPE_PLAYBACK, m_aoValidTarget[targetIndex].TargetID);
				else
					break;
			}*/

			if (targetIndex != -1 && pEntity == NULL)
				targetIndex = -1;
		}
	}

	if (targetIndex >= MAX_PEDS_TRACKED ||
		targetIndex <  0                ||
		m_aoValidTarget[targetIndex].TargetID < 0)
	{
		sCurrentTargetID = -1;
	}
	else
	{
		sCurrentTargetID = m_aoValidTarget[targetIndex].TargetID;
		CPed* pCurrentTarget   = (CPed*)CReplayMgr::GetEntity(sCurrentTargetID );
		if (pCurrentTarget == NULL /*TODO4FIVE || (!m_bIsEditingMarker && pCurrentTarget->GetAlphaFade() < c_PedFadeThreshold)*/)
		{
			sCurrentTargetID = -1;
		}
	}

	/*TODO4FIVE
	if (prevTargetID != sCurrentTargetID)
	{
		OnTargetChange();
	}*/

	const s32 newTargetIndex = (sCurrentTargetID >= 0) ? m_aoValidTarget[targetIndex].TargetID.ToInt() : -1;

	return newTargetIndex;
}

s32 camReplayDirector::GetTargetIdFromIndex(const s32 targetIndex) const
{
	if (targetIndex >= MAX_PEDS_TRACKED || targetIndex < 0 || m_aoValidTarget[targetIndex].TargetID < 0)
	{
		return -1;
	}

	const s32 targetId = m_aoValidTarget[targetIndex].TargetID.ToInt();

	return targetId;
}

float camReplayDirector::GetMaxTargetDistance() const
{
	return m_Metadata.m_MaxTargetDistanceFromCenter;
}

float camReplayDirector::GetMaxDistanceAllowedFromPlayer(const bool considerEditModeForDistanceReduction /* = false */) const
{
	float maxDistance = m_Metadata.m_MaxCameraDistanceFromPlayer;
	if(CReplayMgr::IsEditModeActive() && considerEditModeForDistanceReduction)
	{
		const bool isCurrentlyEditingMarker = (m_CurrentMarkerIndex != -1) && (CReplayMarkerContext::GetEditingMarkerIndex() == m_CurrentMarkerIndex);
		if(isCurrentlyEditingMarker)
		{
			maxDistance -= m_Metadata.m_MaxCameraDistanceReductionForEditMode;
		}
	}

	return maxDistance;
}

float camReplayDirector::GetFovOfRenderedFrame() const
{
	return m_Frame.GetFov();
}

float camReplayDirector::GetFocalLengthOfRenderedFrameInMM() const
{
	const float fov 		= GetFovOfRenderedFrame();
	const float focalLength	= g_HeightOf35mmFullFrame / (2.0f * Tanf(0.5f * fov * DtoR));
	return (focalLength * 1000.0f); //in mm
}

void camReplayDirector::GetDofSettingsHash(u32& dofSettingsHash) const
{
	dofSettingsHash = m_Metadata.m_ReplayDofSettingsRef.GetHash();
}

void camReplayDirector::GetDofMinMaxFNumberAndFocalLengthMultiplier(float& minFNumber, float& maxFNumber, float& fNumberExponent, float& minMultiplier, float& maxMultiplier) const
{
	minFNumber		= m_Metadata.m_MinFNumberOfLens;
	maxFNumber		= m_Metadata.m_MaxFNumberOfLens;
	fNumberExponent	= m_Metadata.m_FNumberExponent;
	minMultiplier	= m_Metadata.m_MinFocalLengthMultiplier;
	maxMultiplier	= m_Metadata.m_MaxFocalLengthMultiplier;
}

void camReplayDirector::OnReplayClipChanged()
{
	Reset();
}

camReplayBaseCamera* camReplayDirector::GetReplayCameraWithIndex(const u32 markerIndex)
{
	camReplayBaseCamera* replayCamera = NULL;

	if (m_ActiveCamera && m_ActiveCamera->GetIsClassId(camReplayBaseCamera::GetStaticClassId()) &&
		static_cast<camReplayBaseCamera*>(m_ActiveCamera.Get())->GetMarkerIndex() == (s32)markerIndex)
	{
		replayCamera = static_cast<camReplayBaseCamera*>(m_ActiveCamera.Get());
	}
	else if (m_PreviousCamera && m_PreviousCamera->GetIsClassId(camReplayBaseCamera::GetStaticClassId()) &&
		static_cast<camReplayBaseCamera*>(m_PreviousCamera.Get())->GetMarkerIndex() == (s32)markerIndex)
	{
		replayCamera = static_cast<camReplayBaseCamera*>(m_PreviousCamera.Get());
	}

	return replayCamera;
}

void camReplayDirector::IncreaseAttachTargetSelection(const u32 markerIndex)
{
	//can't change target selection if playback hasn't finished
	if (!IsReplayPlaybackSetupFinished())
	{
		return;
	}

	camReplayBaseCamera* replayCamera = GetReplayCameraWithIndex(markerIndex);
	if (!replayCamera || !replayCamera->GetIsClassId(camReplayFreeCamera::GetStaticClassId()))
	{
		return;
	}

	IReplayMarkerStorage* markerStorage = CReplayMarkerContext::GetMarkerStorage();
	sReplayMarkerInfo* markerInfo = markerStorage ? markerStorage->TryGetMarker(markerIndex) : NULL;

	if (markerInfo)
	{
		s32 targetIndex = markerInfo->GetAttachTargetIndex();
		markerInfo->SetAttachTargetId( IncrementTarget(markerInfo->GetAttachTargetId(), true, targetIndex) );
		markerInfo->SetAttachTargetIndex( targetIndex );

		static_cast<camReplayFreeCamera*>(replayCamera)->ResetForNewAttachEntity();
	}
}

void camReplayDirector::DecreaseAttachTargetSelection(const u32 markerIndex)
{
	//can't change target selection if playback hasn't finished
	if (!IsReplayPlaybackSetupFinished())
	{
		return;
	}

	camReplayBaseCamera* replayCamera = GetReplayCameraWithIndex(markerIndex);
	if (!replayCamera || !replayCamera->GetIsClassId(camReplayFreeCamera::GetStaticClassId()))
	{
		return;
	}

	IReplayMarkerStorage* markerStorage = CReplayMarkerContext::GetMarkerStorage();
	sReplayMarkerInfo* markerInfo = markerStorage ? markerStorage->TryGetMarker(markerIndex) : NULL;

	if (markerInfo)
	{
		s32 attachTargetIndex = markerInfo->GetAttachTargetIndex();
		markerInfo->SetAttachTargetId( IncrementTarget( markerInfo->GetAttachTargetId(), false, attachTargetIndex ) );
		markerInfo->SetAttachTargetIndex( attachTargetIndex );

		static_cast<camReplayFreeCamera*>(replayCamera)->ResetForNewAttachEntity();
	}
}

void camReplayDirector::DeselectAttachTarget(const u32 markerIndex)
{
	//can't change target selection if playback hasn't finished
	if (!IsReplayPlaybackSetupFinished())
	{
		return;
	}

	camReplayBaseCamera* replayCamera = GetReplayCameraWithIndex(markerIndex);
	if (!replayCamera || !replayCamera->GetIsClassId(camReplayFreeCamera::GetStaticClassId()))
	{
		return;
	}

	IReplayMarkerStorage* markerStorage = CReplayMarkerContext::GetMarkerStorage();
	sReplayMarkerInfo* markerInfo = markerStorage ? markerStorage->TryGetMarker(markerIndex) : NULL;

	if (markerInfo)
	{
		markerInfo->SetAttachTargetIndex( -1 );
		markerInfo->SetAttachTargetId( -1 );

		static_cast<camReplayFreeCamera*>(replayCamera)->ResetForNewAttachEntity();
	}
}

void camReplayDirector::IncreaseLookAtTargetSelection(const u32 markerIndex)
{
	//can't change target selection if playback hasn't finished
	if (!IsReplayPlaybackSetupFinished())
	{
		return;
	}

	camReplayBaseCamera* replayCamera = GetReplayCameraWithIndex(markerIndex);
	if (!replayCamera || !replayCamera->GetIsClassId(camReplayFreeCamera::GetStaticClassId()))
	{
		return;
	}

	IReplayMarkerStorage* markerStorage = CReplayMarkerContext::GetMarkerStorage();
	sReplayMarkerInfo* markerInfo = markerStorage ? markerStorage->TryGetMarker(markerIndex) : NULL;

	if (markerInfo)
	{
		s32 lookatTargetIndex = markerInfo->GetLookAtTargetIndex();
		markerInfo->SetLookAtTargetId( IncrementTarget( markerInfo->GetLookAtTargetId(), true, lookatTargetIndex ) );
		markerInfo->SetLookAtTargetIndex( lookatTargetIndex );

		static_cast<camReplayFreeCamera*>(replayCamera)->ResetForNewLookAtEntity();
	}
}

void camReplayDirector::DecreaseLookAtTargetSelection(const u32 markerIndex)
{
	//can't change target selection if playback hasn't finished
	if (!IsReplayPlaybackSetupFinished())
	{
		return;
	}

	camReplayBaseCamera* replayCamera = GetReplayCameraWithIndex(markerIndex);
	if (!replayCamera || !replayCamera->GetIsClassId(camReplayFreeCamera::GetStaticClassId()))
	{
		return;
	}

	IReplayMarkerStorage* markerStorage = CReplayMarkerContext::GetMarkerStorage();
	sReplayMarkerInfo* markerInfo = markerStorage ? markerStorage->TryGetMarker(markerIndex) : NULL;

	if (markerInfo)
	{
		s32 lookatTargetIndex = markerInfo->GetLookAtTargetIndex();
		markerInfo->SetLookAtTargetId( IncrementTarget( markerInfo->GetLookAtTargetId(), false, lookatTargetIndex ) );
		markerInfo->SetLookAtTargetIndex( lookatTargetIndex );

		static_cast<camReplayFreeCamera*>(replayCamera)->ResetForNewLookAtEntity();
	}
}

void camReplayDirector::DeselectLookAtTarget(const u32 markerIndex)
{
	//can't change target selection if playback hasn't finished
	if (!IsReplayPlaybackSetupFinished())
	{
		return;
	}

	camReplayBaseCamera* replayCamera = GetReplayCameraWithIndex(markerIndex);
	if (!replayCamera || !replayCamera->GetIsClassId(camReplayFreeCamera::GetStaticClassId()))
	{
		return;
	}

	IReplayMarkerStorage* markerStorage = CReplayMarkerContext::GetMarkerStorage();
	sReplayMarkerInfo* markerInfo = markerStorage ? markerStorage->TryGetMarker(markerIndex) : NULL;

	if (markerInfo)
	{
		markerInfo->SetLookAtTargetId( -1 );
		markerInfo->SetLookAtTargetIndex( -1 );

		static_cast<camReplayFreeCamera*>(replayCamera)->ResetForNewLookAtEntity();
	}
}

void camReplayDirector::IncreaseFocusTargetSelection(const u32 markerIndex)
{
	IReplayMarkerStorage* markerStorage = CReplayMarkerContext::GetMarkerStorage();
	sReplayMarkerInfo* markerInfo = markerStorage ? markerStorage->TryGetMarker(markerIndex) : NULL;

	if (markerInfo)
	{
		s32 focusTargetIndex = markerInfo->GetFocusTargetIndex();
		markerInfo->SetFocusTargetId( IncrementTarget( markerInfo->GetFocusTargetId(), true, focusTargetIndex, false ) );
		markerInfo->SetFocusTargetIndex( focusTargetIndex );
	}
}

void camReplayDirector::DecreaseFocusTargetSelection(const u32 markerIndex)
{
	IReplayMarkerStorage* markerStorage = CReplayMarkerContext::GetMarkerStorage();
	sReplayMarkerInfo* markerInfo = markerStorage ? markerStorage->TryGetMarker(markerIndex) : NULL;

	if (markerInfo)
	{
		s32 focusTargetIndex = markerInfo->GetFocusTargetIndex();
		markerInfo->SetFocusTargetId( IncrementTarget( markerInfo->GetFocusTargetId(), false, focusTargetIndex, false ) );
		markerInfo->SetFocusTargetIndex( focusTargetIndex );
	}
}

void camReplayDirector::DeselectFocusTarget(const u32 markerIndex)
{
	IReplayMarkerStorage* markerStorage = CReplayMarkerContext::GetMarkerStorage();
	sReplayMarkerInfo* markerInfo = markerStorage ? markerStorage->TryGetMarker(markerIndex) : NULL;

	if (markerInfo)
	{
		markerInfo->SetFocusTargetId( -1 );
		markerInfo->SetFocusTargetIndex( -1 );
	}
}

const CEntity* camReplayDirector::GetActiveCameraTarget() const
{
	if (m_ActiveCamera)
	{
		if (m_ActiveCamera->GetAttachParent())
		{
			return m_ActiveCamera->GetAttachParent();
		}
		else 
		{
			return m_ActiveCamera->GetLookAtTarget();
		}
	}
	return NULL;
}

void camReplayDirector::SetLastValidCamRelativeSettings(const Vector3& desiredPosition, float lastValidFreeCamAttachEntityRelativeHeading, float lastValidFreeCamAttachEntityRelativePitch, float lastValidFreeCameAttachEntityRelativeRoll)
{
	m_LastValidFreeCamDesiredPos = desiredPosition; 
	m_LastValidFreeCamAttachEntityRelativeHeading = lastValidFreeCamAttachEntityRelativeHeading; 
	m_LastValidFreeCamAttachEntityRelativePitch = lastValidFreeCamAttachEntityRelativePitch; 
	m_LastValidFreeCamAttachEntityRelativeRoll = lastValidFreeCameAttachEntityRelativeRoll; 
	m_HaveLastValidFreeCamRelativeSettings = true; 
}

void camReplayDirector::GetLastValidCamRelativeSettings( Vector3& desiredPosition, float& lastValidFreeCamAttachEntityRelativeHeading, float &lastValidFreeCamAttachEntityRelativePitch, float &lastValidFreeCameAttachEntityRelativeRoll)
{
	if(m_HaveLastValidFreeCamFrame)
	{
		desiredPosition = m_LastValidFreeCamDesiredPos; 
		lastValidFreeCamAttachEntityRelativeHeading =  m_LastValidFreeCamAttachEntityRelativeHeading; 
		lastValidFreeCamAttachEntityRelativePitch = m_LastValidFreeCamAttachEntityRelativePitch; 
		lastValidFreeCameAttachEntityRelativeRoll = m_LastValidFreeCamAttachEntityRelativeRoll; 
	}
}

void camReplayDirector::ForceSmoothing(u32 duration, u32 blendInDuration, u32 blendOutDuration)
{
	m_ForceSmoothingStartTime			= CReplayMgr::GetCurrentTimeRelativeMs();
	m_ForceSmoothingDuration			= duration;
	m_ForceSmoothingBlendInDuration		= blendInDuration;
	m_ForceSmoothingBlendOutDuration	= blendOutDuration;
}

void camReplayDirector::DisableForceSmoothing()
{
	m_ForceSmoothingStartTime			= 0;
	m_ForceSmoothingDuration			= 0;
	m_ForceSmoothingBlendInDuration		= 0;
	m_ForceSmoothingBlendOutDuration	= 0;
}

#if __BANK
void camReplayDirector::DebugGetCameras(atArray<camBaseCamera*> &cameraList) const
{
	if(m_PreviousCamera)
	{
		cameraList.PushAndGrow(m_PreviousCamera);
	}

	if(m_ActiveCamera)
	{
		cameraList.PushAndGrow(m_ActiveCamera);
	}

	if (m_FallbackCamera)
	{
		cameraList.PushAndGrow(m_FallbackCamera);
	}
}

void camReplayDirector::DebugRender() const
{
	if (!CReplayMgr::IsEditModeActive())
	{
		return;
	}

	if (ms_DebugSmoothSpringConstant >= SMALL_FLOAT)
	{
		grcDebugDraw::Axis(m_PostEffectFrame.GetWorldMatrix(), 0.3f);
		m_PostEffectFrame.DebugRenderFrustum(Color32(0, 255, 0, 32));
	}
}

void camReplayDirector::AddWidgetsForInstance()
{
	if(m_WidgetGroup == NULL)
	{
		bkBank* bank = BANKMGR.FindBank("Camera");
		if(bank != NULL)
		{
			m_WidgetGroup = bank->PushGroup("Replay director", false);
			{
				bank->AddToggle("Smooth!", &ms_DebugShouldSmooth);
			}
			bank->PopGroup(); //Replay director.
		}
	}
}

void camReplayDirector::GetDebugText(char* debugText) const
{
	Vector3 desiredPosition(VEC3_ZERO);
	float relativeHeading = 0.0f;
	float relativePitch = 0.0f;
	float relativeRoll = 0.0f;
	float phase = -1.0f;
	char phaseText[8];
	phaseText[0] = '\0';

	if (m_ActiveCamera && m_ActiveCamera->GetIsClassId(camReplayFreeCamera::GetStaticClassId()))
	{
		const camReplayFreeCamera* replayFreeCamera	= static_cast<const camReplayFreeCamera*>(m_ActiveCamera.Get());
		replayFreeCamera->DebugGetDesiredPosition(desiredPosition);
		replayFreeCamera->DebugGetRelativeAttachHeadingPitchAndRoll(relativeHeading, relativePitch, relativeRoll);

		const float currentTime = CReplayMgr::GetCurrentTimeRelativeMsFloat();
		phase = replayFreeCamera->GetCurrentPhaseForBlend(currentTime);
		replayFreeCamera->DebugGetBlendType(phaseText);
	}

	const bool renderPhasesPaused = CPauseMenu::GetPauseRenderPhasesStatus();

	sprintf(debugText, "%sSetup? %s Index = %d (Previous = %d) | Shake = %s (Target: %.2f Actual: %.2f) | Phase = %.2f (%s) | Desired Pos (%.2f, %.2f, %.2f) | Relative H (%.2f) P (%.2f) R (%.2f) | Smooth = %.2f",
		renderPhasesPaused ? "*RENDER_PHASES_PAUSED* " : "", IsReplayPlaybackSetupFinished() ? "Y" : "N", m_CurrentMarkerIndex, m_PreviousMarkerIndex,
		m_FrameShaker ? m_FrameShaker->GetName() : "NONE", m_TargetFrameShakerAmplitude, m_FrameShaker ? m_FrameShaker->GetAmplitude() : 0.0f,
		phase, phaseText,
		desiredPosition.x, desiredPosition.y, desiredPosition.z, relativeHeading, relativePitch, relativeRoll,
		ms_DebugSmoothSpringConstant);

	const u32 frameCounter				= fwTimer::GetCamFrameCount();
	const u32 numUpdatesPerformed		= camInterface::GetDominantRenderedCamera() ? camInterface::GetDominantRenderedCamera()->GetNumUpdatesPerformed() : 0;
	const u32 numReplayUpdatesPerformed = camInterface::GetDominantRenderedCamera() && camInterface::GetDominantRenderedCamera()->GetIsClassId(camReplayBaseCamera::GetStaticClassId()) ?
											static_cast<const camReplayBaseCamera*>(camInterface::GetDominantRenderedCamera())->GetNumReplayUpdatesPerformed() : 0;

	if (IsReplayPlaybackSetupFinished() && !ms_DebugWasReplayPlaybackSetupFinished)
	{
		cameraDisplayf("Playback set up finished [Frame %u] [Updates %u] [Replay Updates %u]", frameCounter, numUpdatesPerformed, numReplayUpdatesPerformed);
	}
	else if (ms_DebugWasReplayPlaybackSetupFinished && !IsReplayPlaybackSetupFinished())
	{
		cameraDisplayf("Playback set up restarted [Frame %u] [Updates %u] [Replay Updates %u]", frameCounter, numUpdatesPerformed, numReplayUpdatesPerformed);
	}

	ms_DebugWasReplayPlaybackSetupFinished = IsReplayPlaybackSetupFinished();
}

#endif // __BANK

#else // !GTA_REPLAY - Stub out everything.

INSTANTIATE_RTTI_CLASS(camReplayDirector,0xC251A33B)


camReplayDirector::camReplayDirector(const camBaseObjectMetadata& metadata)
: camBaseDirector(metadata)
{
}

#endif // GTA_REPLAY
