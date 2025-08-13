//
// camera/replay/ReplayFreeCamera.cpp
// 
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

#include "camera/replay/ReplayFreeCamera.h"

#if GTA_REPLAY

#include "camera/CamInterface.h"
#include "camera/helpers/Collision.h"
#include "camera/helpers/ControlHelper.h"
#include "camera/replay/ReplayDirector.h"
#include "camera/replay/ReplayRecordedCamera.h"
#include "camera/replay/ReplayPresetCamera.h"
#include "camera/system/CameraMetadata.h"

#include "fwmaths/angle.h"

#include "camera/viewports/ViewportManager.h"
#include "control/replay/ReplayMarkerContext.h"
#include "control/replay/IReplayMarkerStorage.h"
#include "frontend/VideoEditor/VideoEditorInterface.h"
#include "modelInfo/MLOmodelInfo.h"
#include "objects/Door.h"
#include "output/constantlighteffect.h"
#include "Peds/Ped.h"
#include "scene/loader/MapData_Interiors.h"
#include "system/controlMgr.h"
#include "system/param.h"
#include "vfx/Misc/GameGlows.h"

#include "entity/entity.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camReplayFreeCamera,0x3388F839)

PF_PAGE(camReplayFreeCameraPage, "Camera: Replay Free Camera");

PF_GROUP(camReplayFreeCameraMetrics);
PF_LINK(camReplayFreeCameraPage, camReplayFreeCameraMetrics);

PF_VALUE_FLOAT(blendPhase, camReplayFreeCameraMetrics);
PF_VALUE_FLOAT(smoothBlendRatio, camReplayFreeCameraMetrics);

extern const float g_InvalidWaterHeight;

static sDisableReprobingInteriors sm_DisableReprobingInteriors[] = {
	{BANK_ONLY("v_janitor",) 0xFCDBE127, -1},
	{BANK_ONLY("v_recycle",) 0x7329ADEF, 10},
	{BANK_ONLY("v_hanger",) 0x6E23F273, 4},
	{BANK_ONLY("bt1_04_carpark",) 0xC7AB3CD0, -1},
	{BANK_ONLY("v_foundry",) 0x5314B05, -1},
	{BANK_ONLY("v_franklinshouse",) 0xd5520ef, 2},
	{BANK_ONLY("v_trevors",) 0x66ce0ac8, 3},
	{BANK_ONLY("",) 0,-1} //End terminator 
};


static sAdditionalCollisionInteriorChecks sm_AdditionalCollisionInteriorChecks[] = {
	{BANK_ONLY("v_abattoir",) 0x91C11002, -1, 34.4f},
	{BANK_ONLY("v_factory1",) 0xA16750EA, -1, 37.0f},
	{BANK_ONLY("v_trevors",) 0x66CE0AC8, -1, 12.2f},
	{BANK_ONLY("v_factory2",) 0x9329B46F, 1, 33.4f},
	{BANK_ONLY("v_franklinshouse",) 0xD5520EF, 2, 177.1f},
	{BANK_ONLY("v_lab",) 0xa6091244, 14, 26.0},
	{BANK_ONLY("",) 0,-1, 0.0f} //End terminator 
};

static sAdditionalObjectsChecks sm_AdditionalObjectChecks[] = {
    {BANK_ONLY("apa_mp_apa_yacht_jacuzzi_cam",) 0x218E9DFC, 0.2f}
};


camReplayFreeCamera::camReplayFreeCamera(const camBaseObjectMetadata& metadata)
: camReplayBaseCamera(metadata)
, m_Metadata(static_cast<const camReplayFreeCameraMetadata&>(metadata))
, m_AttachEntityMatrix(MATRIX34_IDENTITY)
, m_PreviousAttachEntityMatrix(MATRIX34_IDENTITY)
, m_CollisionOrigin(g_InvalidPosition)
, m_CollisionRootPosition(g_InvalidPosition)
, m_DesiredPosition(VEC3_ZERO)
, m_TranslationInputThisUpdate(VEC3_ZERO)
, m_LookAtOffset(0.0f, 0.0f)
, m_CachedNormalisedHorizontalTranslationInput(0.0f, 0.0f)
, m_CachedNormalisedHeadingPitchTranslationInput(0.0f, 0.0f)
, m_LastComputedEntityForBoundRadius(-1)
, m_LastEditingMarkerIndex(-1)
, m_BlendSourceMarkerIndex(-1)
#if KEYBOARD_MOUSE_SUPPORT
, m_MouseInputMode(eMouseInputModes::NO_INPUT)
#endif
, m_DurationOfMaxStickInputForRollCorrection(0)
, m_NumCollisionUpdatesPerformedWithAttachEntity(0)
, m_LastConstrainedTime(0)
, m_PreviousAttachTime(0)
, m_CachedVerticalTranslationInputSign(0.0f)
, m_CachedHorizontalLookAtOffsetInputSign(0.0f)
, m_CachedVerticalLookAtOffsetInputSign(0.0f)
, m_CachedRollRotationInputSign(0.0f)
, m_HorizontalSpeed(0.0f)
, m_VerticalSpeed(0.0f)
, m_HorizontalLookAtOffsetSpeed(0.0f)
, m_VerticalLookAtOffsetSpeed(0.0f)
, m_LookAtRoll(0.0f)
, m_HeadingPitchSpeed(0.0f)
, m_RollSpeed(0.0f)
, m_ZoomSpeed(0.0f)
, m_SmoothedWaterHeight(g_InvalidWaterHeight)
, m_SmoothedWaterHeightPostUpdate(g_InvalidWaterHeight)
, m_AttachEntityRelativeHeading(0.0f)
, m_AttachEntityRelativePitch(0.0f)
, m_AttachEntityRelativeRoll(0.0f)
, m_MaxPhaseToHaltBlendMovement(1.0f)
, m_LastValidPhase(0.0f)
, m_AttachEntityIsInVehicle(false)
, m_BlendHalted(false)
, m_IsANewMarker(false)
, m_ShouldUpdateMaxPhaseToHaltBlend(false)
, m_ResetForNewAttachEntityThisUpdate(false)
, m_IsBlendingPositionInvalid(false)
{
}

bool camReplayFreeCamera::Init(const s32 markerIndex, const bool shouldForceDefaultFrame)
{
	IReplayMarkerStorage* markerStorage	= CReplayMarkerContext::GetMarkerStorage();
	sReplayMarkerInfo* markerInfo		= markerStorage ? markerStorage->TryGetMarker(markerIndex) : NULL;
	m_IsANewMarker						= (markerInfo && !markerInfo->IsCamMatrixValid()) || shouldForceDefaultFrame; //check matrix validity before camReplayBaseCamera::Init()

	const bool succeeded = camReplayBaseCamera::Init(markerIndex, shouldForceDefaultFrame);
	if (!succeeded)
	{
		return false;
	}

	m_HorizontalSpeed	= 0.0f;
	m_VerticalSpeed		= 0.0f;
	m_HorizontalLookAtOffsetSpeed	= 0.0f;
	m_VerticalLookAtOffsetSpeed		= 0.0f;
	m_HeadingPitchSpeed	= 0.0f;
	m_RollSpeed			= 0.0f;
	m_ZoomSpeed			= 0.0f;

	m_CameraToBlendFrom				= NULL;
	m_BlendSourceMarkerIndex		= -1;
	m_MaxPhaseToHaltBlendMovement	= 1.0f;
    m_IsBlendingPositionInvalid = false;

	m_LastComputedEntityForBoundRadius = (s32)NULL;

	m_NumCollisionUpdatesPerformedWithAttachEntity = 0;

	m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation);
	
	//these checks happen when the camera has just been created - we make sure it is within the player safety bubble and isn't colliding with anything.
	if (m_IsANewMarker)
	{
        //use the player entity only if we are jumping from a replay recorded camera,
        //otherwise is not safe and can make the new free camera warping too far away from the 
        //previous frame.
        const CEntity* entityToUseInSafeTest = NULL;
        const camBaseCamera* previousCamera = camInterface::GetReplayDirector().GetPreviousCamera();
        if(previousCamera && (previousCamera->GetClassId() == camReplayRecordedCamera::GetStaticClassId() || previousCamera->GetClassId() == camReplayPresetCamera::GetStaticClassId()))
        {
            entityToUseInSafeTest = CReplayMgr::GetMainPlayerPtr();
        }

		Vector3 cameraPosition = m_Frame.GetPosition();
        SetToSafePosition(cameraPosition, true, false, entityToUseInSafeTest);
		m_Frame.SetPosition(cameraPosition);

	}

	//LoadEntities (from LoadMarkerCameraInfo) sets m_DesiredPosition
	if (!m_AttachEntity)
	{
		m_DesiredPosition = m_Frame.GetPosition();
	}

	//save out any changes initializing may have caused
	if (m_IsANewMarker)
	{
		SaveMarkerCameraInfo(*markerInfo, m_Frame);
	}

	m_Frame.SetDofFullScreenBlurBlendLevel(0.0f);

    //initialize phase interpolators
    m_PhaseInterpolatorIn.UpdateParameters(m_Metadata.m_PhaseInterpolatorIn);
    m_PhaseInterpolatorOut.UpdateParameters(m_Metadata.m_PhaseInterpolatorOut);
    m_PhaseInterpolatorInOut.UpdateParameters(m_Metadata.m_PhaseInterpolatorInOut);

	return true;
}

bool camReplayFreeCamera::SetToSafePosition(Vector3& cameraPosition, bool shouldFlagWarning /* = false*/, bool shouldFlagBlendingWarning /* = false */, const CEntity* entity /* = NULL */) const
{
	bool hasChangedPosition = false;

	const bool isPositionOutsidePlayerLimits = IsPositionOutsidePlayerLimits(cameraPosition);
	if (isPositionOutsidePlayerLimits)
	{
		//NOTE: GetMainPlayerPtr() has already been verified
		const Vector3& playerPedPosition	= VEC3V_TO_VECTOR3(CReplayMgr::GetMainPlayerPtr()->GetTransform().GetPosition());
		const Vector3 playerPedToCamera		= cameraPosition - playerPedPosition;

		Vector3 maxPlayerPedToCamera(playerPedToCamera);
		maxPlayerPedToCamera.NormalizeSafe(YAXIS);
		const float maxDistance = camInterface::GetReplayDirector().GetMaxDistanceAllowedFromPlayer(true);
		maxPlayerPedToCamera.Scale(maxDistance - 0.001f);	// - a little, so next frame it's not constrained unless we're still pushing out

		cameraPosition = playerPedPosition + maxPlayerPedToCamera;

		hasChangedPosition = true;

		if (shouldFlagWarning)
		{
			CReplayMarkerContext::SetWarning(CReplayMarkerContext::EDIT_WARNING_OUT_OF_RANGE);
		}
	}

	//do a capsule test from tracked entity to camera, reposition if it hits
	if (m_AttachEntity || entity)
	{
        const CEntity* entityToTest = m_AttachEntity ? m_AttachEntity : entity;
		hasChangedPosition |= ComputeSafePosition(*entityToTest, cameraPosition);

		if (hasChangedPosition && shouldFlagWarning)
		{
            if( shouldFlagBlendingWarning )
            {
                CReplayMarkerContext::SetWarning(CReplayMarkerContext::EDIT_WARNING_BLEND_COLLISION);
            }
            else
            {
                CReplayMarkerContext::SetWarning(CReplayMarkerContext::EDIT_WARNING_COLLISION);
            }
			
		}
	}

	return hasChangedPosition;
}

void camReplayFreeCamera::GetDefaultFrame(camFrame& frame) const
{
	if(camInterface::GetReplayDirector().IsLastFreeCamFrameValid())
	{
		frame.CloneFrom(camInterface::GetReplayDirector().GetLastValidFreeCamFrame()); 
	}
	else
	{
		camReplayBaseCamera::GetDefaultFrame(frame); 
	}

    //make sure we don't retain any motionblur data from the recorded gameplay camera
    frame.SetMotionBlurStrength(0.0f);
}

bool camReplayFreeCamera::ShouldFallbackToGameplayCamera() const
{
	if (camReplayBaseCamera::ShouldFallbackToGameplayCamera())
	{
		return true;
	}

	const bool isCurrentlyEditingThisMarker = (CReplayMarkerContext::GetEditingMarkerIndex() == m_MarkerIndex);
	if (!isCurrentlyEditingThisMarker)
	{
		//NOTE: makerStorage and markerInfo have already been verified.
		const sReplayMarkerInfo* markerInfo	= CReplayMarkerContext::GetMarkerStorage()->GetMarker(m_MarkerIndex);
		const bool hasLostAttachEntity		= (markerInfo->GetAttachTargetId() != DEFAULT_ATTACH_TARGET_ID) && !m_AttachEntity;

		if (hasLostAttachEntity)
		{
			return true;
		}

		Vector3 desiredPosition = m_DesiredPosition;
		if (m_AttachEntity)
		{
			m_AttachEntityMatrix.Transform(desiredPosition);
		}

		const bool isPositionOutsidePlayerLimits = IsPositionOutsidePlayerLimits(desiredPosition);
		if (isPositionOutsidePlayerLimits)
		{
			return true;
		}

        //if the free camera is flagged to be invalid return that.
        return m_IsBlendingPositionInvalid;
	}

	return false;
}

bool camReplayFreeCamera::Update()
{
	const bool hasSucceeded = camReplayBaseCamera::Update();
	if (!hasSucceeded)
	{
		return false;
	}

	//NOTE: makerStorage and markerInfo have already been verified.
	const sReplayMarkerInfo* markerInfo = CReplayMarkerContext::GetMarkerStorage()->GetMarker(m_MarkerIndex);
	LoadEntities(*markerInfo, m_Frame); //search for the attach and look at entities every frame

    if( (CReplayMgr::IsJumping() || CReplayMgr::WasJumping()) && ! CReplayMgr::IsFineScrubbing() )
    {
        //disable the smoothing if the replay cursor is jumping
        camInterface::GetReplayDirector().DisableForceSmoothing();

        //reset this if we just jumped.
        m_IsBlendingPositionInvalid = false;
    }

	const bool isCurrentlyEditingThisMarker	= (CReplayMarkerContext::GetEditingMarkerIndex() == m_MarkerIndex);
	if(!isCurrentlyEditingThisMarker)
	{
		m_LastEditingMarkerIndex = -1;
	}

	const bool isCurrentlyEditingBlendSourceMarker	= (CReplayMarkerContext::GetEditingMarkerIndex() == m_BlendSourceMarkerIndex);
	if (isCurrentlyEditingThisMarker || isCurrentlyEditingBlendSourceMarker)
	{
		m_ShouldUpdateMaxPhaseToHaltBlend = true;
	}

	UpdateMaxPhaseToHaltBlend();

	Vector3 desiredTranslation = VEC3_ZERO;
	Vector2 desiredLookAtOffsetTranslation(0.0f, 0.0f);
	Vector3 desiredRotation = VEC3_ZERO;

	const CControl& control	= CControlMgr::GetMainFrontendControl(false);

	const bool shouldResetCamera = GetShouldResetCamera(control);
	if (shouldResetCamera)
	{
		ResetCamera();
	}

	if (isCurrentlyEditingThisMarker)
	{
		float desiredZoom;
		UpdateControl(control, desiredTranslation, desiredLookAtOffsetTranslation, desiredRotation, desiredZoom);

		UpdateZoom(desiredZoom);
	}

	if (m_LookAtEntity)
	{
		//we calculate the position first, then rotation if we're locked onto a look at target
		UpdatePosition(desiredTranslation);
		UpdateRotation(desiredRotation, desiredLookAtOffsetTranslation);
	}
	else
	{
		UpdateRotation(desiredRotation, desiredLookAtOffsetTranslation);
		UpdatePosition(desiredTranslation);
	}

	m_Frame.SetNearClip(m_Metadata.m_NearClip);

	//We want to update the desired position when we're dragging a marker
	const bool isShiftingThisMarker = CReplayMarkerContext::GetCurrentMarkerIndex() == m_MarkerIndex && CReplayMarkerContext::IsShiftingCurrentMarker();

	if (isCurrentlyEditingThisMarker || isShiftingThisMarker)
	{
		if (m_AttachEntity)
		{
			const Vector3& desiredPositionWS = m_Frame.GetPosition();
			m_AttachEntityMatrix.UnTransform(desiredPositionWS, m_DesiredPosition);

			Vector3 localFront;
			Vector3 localUp;
			m_AttachEntityMatrix.UnTransform3x3(m_Frame.GetFront(), localFront);
			m_AttachEntityMatrix.UnTransform3x3(m_Frame.GetUp(), localUp);

			Matrix34 localMatrix;
			camFrame::ComputeWorldMatrixFromFrontAndUp(localFront, localUp, localMatrix);

			camFrame::ComputeHeadingPitchAndRollFromMatrixUsingEulers(localMatrix, m_AttachEntityRelativeHeading, m_AttachEntityRelativePitch, m_AttachEntityRelativeRoll);
		}
		else
		{
			m_DesiredPosition = m_Frame.GetPosition();
		}
	}

	// Update the free cameras portal tracker to probe every frame
	UpdatePortalTracker();

	m_IsANewMarker = false;

	if(!CVideoEditorInterface::ShouldShowLoadingScreen())
	{
		const replayCamFrame& replayFrame = CReplayMgr::GetReplayFrameData().m_cameraData;
		if(replayFrame.m_Fader.IsFadedOut())
		{	// Faded out completely...probably hiding some nasties so leave it.
			camInterface::SetFader(replayFrame.m_Fader);
		}
		else
		{	
			if(!camInterface::GetFader().IsFadedOut())
			{
				// Not recorded camera so remove the fade.
				CViewportFader fader;
				fader.SetFixedFade(0.0f);
				fader.Process();	// Force the values to update so 'IsFadedOut' returns correctly.
				camInterface::SetFader(fader);
			}
		}
	}

	return true;
}

void camReplayFreeCamera::UpdateAttachEntityMatrix()
{
	if (!m_AttachEntity)
	{
		return;
	}

	// Smooth the heading 
	const CPed* ped								= m_AttachEntity->GetIsTypePed() ? static_cast<const CPed*>(m_AttachEntity.Get()) : NULL;
	const bool isTaskAimGunOnFootRunningForPed	= ped && const_cast<CPed*>(ped)->GetReplayRelevantAITaskInfo().IsAITaskRunning(REPLAY_TASK_AIM_GUN_ON_FOOT) && ped->GetBoneIndexFromBoneTag(BONETAG_FACING_DIR) != -1;

	//We want to force the default frame when we're dragging a marker
	const bool isShiftingThisMarker = CReplayMarkerContext::GetCurrentMarkerIndex() == m_MarkerIndex && CReplayMarkerContext::IsShiftingCurrentMarker();

	//NOTE: makerStorage and markerInfo have already been verified.
	sReplayMarkerInfo* markerInfo = CReplayMarkerContext::GetMarkerStorage()->GetMarker(m_MarkerIndex);

	bool attachEntityInRagdoll = (m_AttachEntity->GetIsTypePed() && static_cast<const CPed*>(m_AttachEntity.Get())->GetReplayUsingRagdoll());
	float smoothBlending = 1.0f;

    TUNE_GROUP_BOOL(REPLAY_FREE_CAMERA, bUseRootBonePositionForRagdoll, true);
	if (!markerInfo->IsAttachEntityMatrixValid() || m_IsANewMarker || isShiftingThisMarker)
	{
		//if matrix isn't valid for whatever reason, grab the whole thing
		m_AttachEntityMatrix = MAT34V_TO_MATRIX34(m_AttachEntity->GetTransform().GetMatrix());
        m_PreviousAttachEntityMatrix = m_AttachEntityMatrix;
        m_PreviousAttachTime = CReplayMgr::GetCurrentTimeRelativeMs();

        if(!markerInfo->IsAttachTypeFull() && attachEntityInRagdoll)
        {
            if (bUseRootBonePositionForRagdoll)
            {
                const CPed* pPed = static_cast<const CPed*>(m_AttachEntity.Get());
                Matrix34 rootMat;
                pPed->GetGlobalMtx(BONETAG_ROOT, rootMat);
                m_AttachEntityMatrix.d = rootMat.d;
            }
            else
            {
                m_AttachEntityMatrix.d = VEC3V_TO_VECTOR3(m_AttachEntity->GetTransform().GetPosition());
            }
        }
		
		float heading, pitch, roll;
		camFrame::ComputeHeadingPitchAndRollFromMatrix(m_AttachEntityMatrix, heading, pitch, roll);
		if(isTaskAimGunOnFootRunningForPed)
		{
			heading = ped->GetFacingDirectionHeading(); //special heading calculation for peds
		}
		camFrame::ComputeWorldMatrixFromHeadingPitchAndRoll(heading, pitch, roll, m_AttachEntityMatrix);
	}
	else
	{
		if (markerInfo->IsAttachTypeFull()) //full attach matrix regardless of ragdoll here
		{
			//grab the whole thing
			m_AttachEntityMatrix = MAT34V_TO_MATRIX34(m_AttachEntity->GetTransform().GetMatrix());
            m_PreviousAttachEntityMatrix = m_AttachEntityMatrix;
            m_PreviousAttachTime = CReplayMgr::GetCurrentTimeRelativeMs();

			float heading, pitch, roll;
			camFrame::ComputeHeadingPitchAndRollFromMatrix(m_AttachEntityMatrix, heading, pitch, roll);
			if(isTaskAimGunOnFootRunningForPed)
			{
				heading = ped->GetFacingDirectionHeading(); //special heading calculation for peds
			}
			camFrame::ComputeWorldMatrixFromHeadingPitchAndRoll(heading, pitch, roll, m_AttachEntityMatrix);
		}
		else if (attachEntityInRagdoll) //ragdoll, position only attach type, position and heading only attach type, or having a look at entity mean we grab position only
        {            
			if (bUseRootBonePositionForRagdoll)
			{
				const CPed* pPed = static_cast<const CPed*>(m_AttachEntity.Get());
				Matrix34 rootMat;
				pPed->GetGlobalMtx(BONETAG_ROOT, rootMat);
				m_AttachEntityMatrix.d = rootMat.d;
			}
			else
			{
				m_AttachEntityMatrix.d = VEC3V_TO_VECTOR3(m_AttachEntity->GetTransform().GetPosition());
			}

            const u32 currentTimeMs = CReplayMgr::GetCurrentTimeRelativeMs();

            TUNE_GROUP_BOOL(REPLAY_FREE_CAMERA, bUseSmoothingInRagdoll, true);
            if (bUseSmoothingInRagdoll)
            {
                //is replay actually moving forward?
                const u32 deltaTimeMs = currentTimeMs - m_PreviousAttachTime;
                if( deltaTimeMs > 0 )
                {
                    //compute speed in meters per seconds
                    const Vector3 deltaPosition = m_AttachEntityMatrix.d - m_PreviousAttachEntityMatrix.d;
                    const float speed = (deltaPosition.Mag() / static_cast<float>(deltaTimeMs)) * 1000.0f; //put this back in meters/seconds
                    if( speed > SMALL_FLOAT )
                    {
                        TUNE_GROUP_FLOAT(REPLAY_FREE_CAMERA, fMinSpeedForSmoothBlending, 1.0f,  0.0f, 50.0f, 0.1f );
                        TUNE_GROUP_FLOAT(REPLAY_FREE_CAMERA, fMaxSpeedForSmoothBlending, 2.0f, 0.0f, 50.0f, 0.1f );

                        const float speedNorm  = speed - fMinSpeedForSmoothBlending;
                        const float speedRange = fMaxSpeedForSmoothBlending - fMinSpeedForSmoothBlending;

                        //compute the smoothBlending based on the attached entity speed.
                        smoothBlending = speedRange > SMALL_FLOAT ? (1.0f - Clamp(speedNorm / speedRange, 0.0f, 1.0f)) : 1.0f;
                    }
                }

                //sorry for the cast-fest, I couldn't find an integer interpolation function...groan...
                const float duration = Lerp(smoothBlending, 0.0f, static_cast<float>(m_Metadata.m_RagdollForceSmoothingDurationMs));
                const float blendIn  = Lerp(smoothBlending, 0.0f, static_cast<float>(m_Metadata.m_RagdollForceSmoothingBlendOutDurationMs));
                const float blendOut = Lerp(smoothBlending, 0.0f, static_cast<float>(m_Metadata.m_RagdollForceSmoothingBlendOutDurationMs));

                //get the blended smoothing values to the director
                camInterface::GetReplayDirector().ForceSmoothing(static_cast<u32>(duration), static_cast<u32>(blendIn), static_cast<u32>(blendOut));
            }

            m_PreviousAttachEntityMatrix = m_AttachEntityMatrix;
            m_PreviousAttachTime         = currentTimeMs;
		}
		else if (markerInfo->IsAttachTypePositionOnly() || markerInfo->IsAttachTypePositionAndHeading() || m_LookAtEntity)
		{
			m_AttachEntityMatrix.d = VEC3V_TO_VECTOR3(m_AttachEntity->GetTransform().GetPosition());
            m_PreviousAttachEntityMatrix = m_AttachEntityMatrix;
            m_PreviousAttachTime = CReplayMgr::GetCurrentTimeRelativeMs();
		}

        PF_SET(smoothBlendRatio, smoothBlending);
		
		//position and heading only, we grab heading separately
		if (markerInfo->IsAttachTypePositionAndHeading())
		{
			float heading, pitch, roll;
			camFrame::ComputeHeadingPitchAndRollFromMatrix(m_AttachEntityMatrix, heading, pitch, roll);
			if(isTaskAimGunOnFootRunningForPed)
			{
				heading = ped->GetFacingDirectionHeading(); //special heading calculation for peds
			}
			else
			{
				heading = camFrame::ComputeHeadingFromMatrix(MAT34V_TO_MATRIX34(m_AttachEntity->GetTransform().GetMatrix()));
			}
			camFrame::ComputeWorldMatrixFromHeadingPitchAndRoll(heading, pitch, roll, m_AttachEntityMatrix);
		}
	}
}

void camReplayFreeCamera::UpdatePortalTracker()
{
	CViewport* gameViewport = gVpMan.GetGameViewport();
	if(gameViewport)
	{
		//Get the portal tracker from the viewport and make sure it is in an interior.
		int count = RENDERPHASEMGR.GetRenderPhaseCount();
		for(s32 i=0; i<count; i++)
		{
			CRenderPhase &renderPhase = (CRenderPhase &) RENDERPHASEMGR.GetRenderPhase(i);

			if (renderPhase.IsScanRenderPhase())
			{
				CRenderPhaseScanned &scanPhase = (CRenderPhaseScanned &) renderPhase;

				if (scanPhase.IsUpdatePortalVisTrackerPhase())
				{
					CPortalTracker* portalTracker = scanPhase.GetPortalVisTracker();
					if(portalTracker)
					{
						// Set the portal tracker to force update this frame
						portalTracker->SetIsReplayControlled( true );

						CInteriorInst* pIntInst = portalTracker->GetInteriorInst();

						if(pIntInst)
						{
							//Hack - has wrong collision, don't update location
							CInteriorProxy* pProxy = pIntInst->GetProxy();
							const s32 roomIdx = pIntInst->FindRoomByHashcode( portalTracker->GetCurrRoomHash() );
							if (pProxy && pProxy->GetNameHash() == ATSTRINGHASH("v_bank", 0x4326634d))
							{		
								Vector3 pos = m_Frame.GetPosition();
								static spdAABB door(Vec3V(265.1f, 216.9f, 109.3f), Vec3V(266.4f, 218.6f, 111.6f));
								if (door.ContainsPoint(VECTOR3_TO_VEC3V(pos)))
								{
									portalTracker->SetIsReplayControlled( false );
								}
							}

							//Hack - has invalid occlusion setup, don't probe every frame
							if (pProxy)
							{
								const sDisableReprobingInteriors* interior = sm_DisableReprobingInteriors;
								while(interior->interiorHash != 0)
								{
									u32 interiorHash = interior->interiorHash;
									BANK_ONLY( interiorHash = ATSTRINGHASH(interior->interiorName, interior->interiorHash); )

									if(interiorHash == pProxy->GetNameHash() && 
									   (interior->interiorRoom == -1 || interior->interiorRoom == roomIdx))
									{
										portalTracker->SetIsReplayControlled( false );
										break;
									}
									++interior;
								}
								
								if (pProxy->GetNameHash() == ATSTRINGHASH("v_fib02", 0x62504C8D))
								{
									// Hack - Probe won't touch the external ground going out of the fib building. 
									// Don't allow a fallback for this interior
									return;
								}
							}

							const fwInteriorLocation	intLoc = CInteriorInst::CreateLocation( pIntInst, roomIdx );
							// Set the current interior as a fall back
							portalTracker->EnableProbeFallback(intLoc);
						}
					}
				}
			}
		}
	}
}

bool camReplayFreeCamera::IsPositionOutsidePlayerLimits(const Vector3& position) const
{
	const CPed* playerPed = CReplayMgr::GetMainPlayerPtr();
	if (!playerPed)
	{
		return false;
	}

	const Vector3& playerPedPosition	= VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition());
	const Vector3 playerPedToCamera		= position - playerPedPosition;
	const float distance2FromPlayerPed	= playerPedToCamera.Mag2();

	float maxDistance					= camInterface::GetReplayDirector().GetMaxDistanceAllowedFromPlayer(true);

	return (distance2FromPlayerPed > (maxDistance * maxDistance));
}

float camReplayFreeCamera::ComputeBoundRadius(const CEntity* entity)
{
	if(!entity)
	{
		m_LastComputedEntityForBoundRadius = -1;
		return 0.0f;
	}
	else if(entity->GetIsTypePed())
	{
		const CPed* lookAtPed = static_cast<const CPed*>(entity);
		const CVehicle* pedVehicle = const_cast<const CVehicle*>(lookAtPed->GetVehiclePedInside());
		if(!pedVehicle) // lookAtPed is cast to non-const because GetReplayRelevantAITaskInfo is non-const and returns non-const ref
		{
			if(const_cast<CPed*>(lookAtPed)->GetReplayRelevantAITaskInfo().IsAITaskRunning(REPLAY_ENTER_VEHICLE_SEAT_TASK))
			{
				const fwEntity* fwEntityPtr = const_cast<const fwEntity*>(lookAtPed->GetAttachParent());
				if(fwEntityPtr)
				{
					const CEntity* attachParent = static_cast<const CEntity*>(fwEntityPtr);
					if(attachParent->GetIsTypeVehicle())
					{
						pedVehicle = static_cast<const CVehicle*>(attachParent);
						m_LastComputedEntityForBoundRadius = pedVehicle->GetReplayID();
					}
				}
			}
			else if(m_LastComputedEntityForBoundRadius >= 0)
			{
				//this small hack is needed because when the REPLAY_ENTER_VEHICLE_SEAT_TASK ends there's one frame of gap
				//where the ped is still "not in the vehicle" and "not entering the vehicle". This inconsistent state cause
				//a glitch with the bound radius and finally with the camera position.
				const CEntity* pLastEntity = CReplayMgr::GetEntity(m_LastComputedEntityForBoundRadius);
				if(pLastEntity && pLastEntity->GetIsTypeVehicle())
				{
					pedVehicle = static_cast<const CVehicle*>(pLastEntity);
					m_LastComputedEntityForBoundRadius = -1;
				}
			}
		}

		if(lookAtPed && !pedVehicle)
		{
			m_LastComputedEntityForBoundRadius = -1;
		}

		return pedVehicle ? pedVehicle->GetBoundRadius() : lookAtPed->GetBoundRadius(); 
	}
	else
	{
		m_LastComputedEntityForBoundRadius = -1;
		return entity->GetBoundRadius();
	}
}

//Compute the camera framing using a screen space lookAt offset point.
void camReplayFreeCamera::ComputeLookAtFrameWithOffset( const Vector3& lookAtPoint, 
                                                        const Vector3& cameraPosition, 
                                                        const Vector2& lookAtOffsetScreenSpace, 
                                                        const float fov, 
                                                        const float roll, 
                                                        Matrix34& resultWorldMatrix)
{
    //we need to re-compute the rotation using the same screen space computation that we do in the normal update.
    // Get the camera and entity position, and camera to entity vector
    Vector3 cameraFront = lookAtPoint - cameraPosition;

    // Compute a lookAt matrix (camera to entity), positioned at the entity origin
    Matrix34 orbitMatrix;
    camFrame::ComputeWorldMatrixFromFront(cameraFront, orbitMatrix);
    orbitMatrix.d = lookAtPoint;

    // UnTransform the orbit position (camera) into orbit matrix space
    Vector3 orbitMatrixRelativeCameraPosition(cameraPosition);
    orbitMatrix.UnTransform(orbitMatrixRelativeCameraPosition);

    // Calculate the new lookAt vector
    const CViewport* viewport	= gVpMan.GetGameViewport();
    const float aspectRatio		= viewport ? viewport->GetGrcViewport().GetAspectRatio() : (16.0f / 9.0f);
    const float verticalFov		= fov;
    const float nearZ			= m_Frame.GetNearClip();
    const float tanVFOV			= tanf((verticalFov * DtoR) / 2);
    const float tanHFOV			= tanVFOV * aspectRatio;

    Vector3 screenPoint			= Vector3(tanHFOV * -lookAtOffsetScreenSpace.x, 1.0f, tanVFOV * lookAtOffsetScreenSpace.y) * nearZ;

    Vector3 screenPointNormal(screenPoint);
    screenPointNormal.Normalize();

    const spdPlane orbitPlane(Vector3::ZeroType, YAXIS);
    Vec3V intersection;
    orbitPlane.IntersectRayV(RCC_VEC3V(orbitMatrixRelativeCameraPosition), RCC_VEC3V(screenPointNormal), intersection);
    Vector3 newLookAtOffset(VEC3V_TO_VECTOR3(intersection));

    Vector3 orbitRelativeCameraToLookAtOffset(newLookAtOffset - orbitMatrixRelativeCameraPosition);

    // Construct a matrix (still in orbitMatrix space) looking at the offset position
    Matrix34 orbitLocalLookAtMatrix;
    camFrame::ComputeWorldMatrixFromFront(orbitRelativeCameraToLookAtOffset, orbitLocalLookAtMatrix);
    orbitLocalLookAtMatrix.d = orbitMatrixRelativeCameraPosition;

    // Apply roll which pivots around the camera front axis
    orbitMatrix.RotateLocalY(roll);

    // Now transform back into world space
    Matrix34 cameraMatrix(orbitLocalLookAtMatrix);
    cameraMatrix.Dot(orbitMatrix);
    cameraMatrix.d = cameraPosition;

    resultWorldMatrix = cameraMatrix;
}

void camReplayFreeCamera::ResetCamera()
{
	m_Frame = m_ResetFrame;

	//reset position
	const Vector3& defaultPosition		= m_Frame.GetWorldMatrix().d;
	//NOTE: We set the desired position and not the position on the frame, because UpdatePosition uses this as it's initial position
	if (m_AttachEntity)
	{
		const Vector3& desiredPositionWS = defaultPosition;
		m_AttachEntityMatrix.UnTransform(desiredPositionWS, m_DesiredPosition);

		Vector3 localFront;
		Vector3 localUp;
		m_AttachEntityMatrix.UnTransform3x3(m_Frame.GetFront(), localFront);
		m_AttachEntityMatrix.UnTransform3x3(m_Frame.GetUp(), localUp);

		Matrix34 localMatrix;
		camFrame::ComputeWorldMatrixFromFrontAndUp(localFront, localUp, localMatrix);

		camFrame::ComputeHeadingPitchAndRollFromMatrixUsingEulers(localMatrix, m_AttachEntityRelativeHeading, m_AttachEntityRelativePitch, m_AttachEntityRelativeRoll);
	}
	else
	{
		m_DesiredPosition = defaultPosition;
	}

	m_HorizontalSpeed	= 0.0f;
	m_VerticalSpeed		= 0.0f;
	m_HorizontalLookAtOffsetSpeed	= 0.0f;
	m_VerticalLookAtOffsetSpeed		= 0.0f;
	m_HeadingPitchSpeed = 0.0f;
	m_RollSpeed			= 0.0f;
	m_ZoomSpeed			= 0.0f;
	m_LookAtOffset		= Vector2(0.0f, 0.0f);

    m_IsBlendingPositionInvalid = false;

	//stop roll correction spring
	m_TranslationInputThisUpdate = VEC3_ZERO;
	m_RollCorrectionBlendLevelSpring.Reset();

	m_LastComputedEntityForBoundRadius = -1;
	m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation);
}

void camReplayFreeCamera::UpdateControl(const CControl& control, Vector3& desiredTranslation, Vector2& desiredLookAtOffsetTranslation, Vector3& desiredRotation, float& desiredZoom)
{
#if KEYBOARD_MOUSE_SUPPORT
	m_MouseInputMode = eMouseInputModes::NO_INPUT;
#endif

	UpdateTranslationControl(control, desiredTranslation, desiredLookAtOffsetTranslation);

	UpdateRotationControl(control, desiredRotation);

	UpdateZoomControl(control, desiredZoom);

#if KEYBOARD_MOUSE_SUPPORT
	// Hide the mouse cursor when the left mouse button is down and the mouse has moved
	if(control.GetScriptRDown().GetSource().m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE && control.GetScriptRDown().IsDown()) // TODO: Move to IsLeftMouseButtonDown			
	{		
		if(IsMouseLookInputActive(control))
		{
			CReplayMarkerContext::SetCameraIsUsingMouse(true);
		}
	}
	else
	{
		CReplayMarkerContext::SetCameraIsUsingMouse(false);
	}		
#endif	
}

#if KEYBOARD_MOUSE_SUPPORT
bool camReplayFreeCamera::IsMouseLookInputActive(const CControl& control)
{
	const bool bIsMouseInputActive = control.GetPedLookLeftRight().GetSource().m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE ||
									control.GetPedLookUpDown().GetSource().m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE;
	return bIsMouseInputActive;
}
#endif

void camReplayFreeCamera::UpdateTranslationControl(const CControl& control, Vector3& desiredTranslation, Vector2& desiredLookAtOffsetTranslation)
{
	Vector3 translationInput;
	Vector2 lookAtOffsetInput;
	ComputeTranslationInput(control, translationInput, lookAtOffsetInput KEYBOARD_MOUSE_ONLY(, m_MouseInputMode));

	m_TranslationInputThisUpdate = translationInput;

	const float horizontalInputMag = translationInput.XYMag();
	if(horizontalInputMag >= SMALL_FLOAT)
	{
		//Cache the normalised horizontal translation inputs, for use in deceleration.
		m_CachedNormalisedHorizontalTranslationInput.InvScale(Vector2(translationInput, Vector2::kXY), horizontalInputMag);
	}

	const float verticalInputMag = Abs(translationInput.z);
	if(verticalInputMag >= SMALL_FLOAT)
	{
		//Cache the sign of the vertical translation input, for use in deceleration.
		m_CachedVerticalTranslationInputSign = Sign(translationInput.z);
	}

	const float horizontalLookAtOffsetInputMag = Abs(lookAtOffsetInput.x);
	if(horizontalLookAtOffsetInputMag >= SMALL_FLOAT)
	{
		//Cache the sign of the horizontal translation input, for use in deceleration.
		m_CachedHorizontalLookAtOffsetInputSign = Sign(lookAtOffsetInput.x);
	}

	const float verticalLookAtOffsetInputMag = Abs(lookAtOffsetInput.y);
	if(verticalLookAtOffsetInputMag >= SMALL_FLOAT)
	{
		//Cache the sign of the vertical translation input, for use in deceleration.
		m_CachedVerticalLookAtOffsetInputSign = Sign(lookAtOffsetInput.y);
	}

	const camReplayBaseCameraMetadataInputResponse& horizontalInputResponse		= m_LookAtEntity ? m_Metadata.m_HorizontalTranslationWithLookAtInputResponse : m_Metadata.m_HorizontalTranslationInputResponse;
	const camReplayBaseCameraMetadataInputResponse& verticalInputResponse		= m_LookAtEntity ? m_Metadata.m_VerticalTranslationWithLookAtInputResponse : m_Metadata.m_VerticalTranslationInputResponse;
	const camReplayBaseCameraMetadataInputResponse& lookAtOffsetInputResponse	= m_Metadata.m_LookAtOffsetInputResponse;

#if KEYBOARD_MOUSE_SUPPORT
	float fTranslationWithLookAtOffsetMouseInputMultiplier = 1.0f;
	float fVerticalTranslationWithLookAtOffsetMouseInputMultiplier = 1.0f;
	float fLookAtOffsetMouseInputMultiplier = 1.0f;

	if(m_LookAtEntity)
	{
		if(IsMouseLookInputActive(control) &&
			control.GetScriptRS().GetSource().m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE && control.GetScriptRS().IsDown())
		{
			fTranslationWithLookAtOffsetMouseInputMultiplier	= m_Metadata.m_TranslationWithLookAtOffsetMouseInputMultiplier;
			fLookAtOffsetMouseInputMultiplier					= m_Metadata.m_LookAtOffsetMouseInputMultiplier;
		}

		if (m_MouseInputMode == LOOKAT_VERTICALTRANSLATION)
		{
			fVerticalTranslationWithLookAtOffsetMouseInputMultiplier = m_Metadata.m_MouseInputScaleForVerticalTranslation;
		}
	}

	float fVerticalTranslationOffsetMouseInputMultiplier = 1.0f;
	if(control.GetValue(INPUT_CURSOR_SCROLL_UP).GetSource().m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE ||
		control.GetValue(INPUT_CURSOR_SCROLL_DOWN).GetSource().m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE)
	{
		fVerticalTranslationOffsetMouseInputMultiplier = m_Metadata.m_MouseInputScaleForVerticalTranslation;
	}
#endif // KEYBOARD_MOUSE_SUPPORT

	//Apply the custom power factor scaling specified in metadata to change the control response across the range of stick input.
	const float scaledHorizontalInputMag	= rage::Powf(Min(horizontalInputMag, 1.0f), horizontalInputResponse.m_InputMagPowerFactor);
	const float scaledVerticalInputMag		= rage::Powf(Min(verticalInputMag, 1.0f), verticalInputResponse.m_InputMagPowerFactor);

	const float scaledHorizontalLookAtOffsetInputMag	= rage::Powf(Min(horizontalLookAtOffsetInputMag, 1.0f), lookAtOffsetInputResponse.m_InputMagPowerFactor);
	const float scaledVerticalLookAtOffsetInputMag		= rage::Powf(Min(verticalLookAtOffsetInputMag, 1.0f), lookAtOffsetInputResponse.m_InputMagPowerFactor);

	const float timeStep = fwTimer::GetNonPausableCamTimeStep();

	const float horizontalOffset	= camControlHelper::ComputeOffsetBasedOnAcceleratedInput(scaledHorizontalInputMag,
										horizontalInputResponse.m_MaxAcceleration, horizontalInputResponse.m_MaxDeceleration,
										horizontalInputResponse.m_MaxSpeed, m_HorizontalSpeed, timeStep, false);
	const float verticalOffset		= camControlHelper::ComputeOffsetBasedOnAcceleratedInput(scaledVerticalInputMag,
										verticalInputResponse.m_MaxAcceleration, verticalInputResponse.m_MaxDeceleration,
										verticalInputResponse.m_MaxSpeed, m_VerticalSpeed, timeStep, false);

	const float horizontalLookAtOffset	= camControlHelper::ComputeOffsetBasedOnAcceleratedInput(scaledHorizontalLookAtOffsetInputMag,
											lookAtOffsetInputResponse.m_MaxAcceleration, lookAtOffsetInputResponse.m_MaxDeceleration,
											lookAtOffsetInputResponse.m_MaxSpeed, m_HorizontalLookAtOffsetSpeed, timeStep, false);
	const float verticalLookAtOffset	= camControlHelper::ComputeOffsetBasedOnAcceleratedInput(scaledVerticalLookAtOffsetInputMag,
											lookAtOffsetInputResponse.m_MaxAcceleration, lookAtOffsetInputResponse.m_MaxDeceleration,
											lookAtOffsetInputResponse.m_MaxSpeed, m_VerticalLookAtOffsetSpeed, timeStep, false);

	Vector2 horizontalTranslation;
	const float verticalTranslation = m_CachedVerticalTranslationInputSign * verticalOffset KEYBOARD_MOUSE_ONLY(* fVerticalTranslationOffsetMouseInputMultiplier);

	// Translation and lookAt Orbit
#if KEYBOARD_MOUSE_SUPPORT	
	if(m_MouseInputMode == eMouseInputModes::LOOKAT_ORBIT)
	{				
		horizontalTranslation.Set(Vector2(translationInput, Vector2::kXY) * fTranslationWithLookAtOffsetMouseInputMultiplier);		
	}
	else
	{	
#endif		
		horizontalTranslation.Scale(m_CachedNormalisedHorizontalTranslationInput, horizontalOffset);
#if KEYBOARD_MOUSE_SUPPORT		
	}
#endif

	desiredTranslation.Set(horizontalTranslation.x, horizontalTranslation.y, verticalTranslation);

	// lookAt Offset
#if KEYBOARD_MOUSE_SUPPORT			
	if(m_MouseInputMode == eMouseInputModes::LOOKAT_OFFSET)
	{
		desiredLookAtOffsetTranslation.Set(lookAtOffsetInput * fLookAtOffsetMouseInputMultiplier);
	}
	else
	{	
#endif
		const float horizontalLookAtOffsetTranslation	    = m_CachedHorizontalLookAtOffsetInputSign * horizontalLookAtOffset;
		const float verticalLookAtOffsetTranslation		    = m_CachedVerticalLookAtOffsetInputSign * verticalLookAtOffset KEYBOARD_MOUSE_ONLY(* fVerticalTranslationWithLookAtOffsetMouseInputMultiplier);
        
        //B*2287473, solution 1 to avoid any camera movements if we reach the framing limits.
        TUNE_GROUP_BOOL(REPLAY_FREE_CAMERA, bBlockCameraHeightChangeWhenLimitReached, false);
        const float limitMovementOnVerticalOffsetMultiplier = (bBlockCameraHeightChangeWhenLimitReached && Abs(m_LookAtOffset.y + verticalLookAtOffsetTranslation) > 1.0f) ? 0.0f : 1.0f;
		desiredLookAtOffsetTranslation.Set(horizontalLookAtOffsetTranslation, verticalLookAtOffsetTranslation * limitMovementOnVerticalOffsetMultiplier);

		//if we're controlling vertical look at offset with the shoulder buttons or scroll wheel, we want to also apply translation so we also move in world space
		if (control.GetScriptLB().IsDown() || control.GetScriptRB().IsDown() KEYBOARD_MOUSE_ONLY(|| m_MouseInputMode == eMouseInputModes::LOOKAT_VERTICALTRANSLATION) )
		{
			desiredTranslation.z += 0.5f * verticalLookAtOffsetTranslation * limitMovementOnVerticalOffsetMultiplier;
		}
#if KEYBOARD_MOUSE_SUPPORT
	}
#endif
}

void camReplayFreeCamera::ComputeTranslationInput(const CControl& control, Vector3& translationInput, Vector2& lookAtOffsetInput KEYBOARD_MOUSE_ONLY(, s32& mouseInputMode)) const
{
	translationInput = VEC3_ZERO;
	lookAtOffsetInput = Vector2(0.0f, 0.0f);

	if(m_LookAtEntity)
	{					
		// Orbit distance increase/decrease via dpad and arrow key up/down
		if(control.GetScriptPadDown().IsDown())
		{
			translationInput.y -= 1.0f;
		}
		if(control.GetScriptPadUp().IsDown())
		{
			translationInput.y += 1.0f;
		}

		// LB/RB (E/Q) buttons for look at translation
		if(control.GetScriptLB().IsDown())
		{
			lookAtOffsetInput.y -= 1.0f;
		}
		if(control.GetScriptRB().IsDown())
		{
			lookAtOffsetInput.y += 1.0f;
		}

#if KEYBOARD_MOUSE_SUPPORT
		// Mouse scrollwheel for vertical movement
		if(control.GetValue(INPUT_CURSOR_SCROLL_UP).GetSource().m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE ||
			control.GetValue(INPUT_CURSOR_SCROLL_DOWN).GetSource().m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE)
		{
			float verticalLookAtOffset	= control.GetValue(INPUT_CURSOR_SCROLL_UP).GetNorm() - control.GetValue(INPUT_CURSOR_SCROLL_DOWN).GetNorm();
			lookAtOffsetInput.y			+= verticalLookAtOffset;

			mouseInputMode = eMouseInputModes::LOOKAT_VERTICALTRANSLATION;
		}

		if(IsMouseLookInputActive(control))
		{
			const bool leftMouseDown	= control.GetScriptRDown().GetSource().m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE && control.GetScriptRDown().IsDown();
		
			// mouse lookAt Orbit
			//if(leftMouseDown)
			//{
			//	translationInput.x += control.GetPedLookLeftRight().GetNorm() * m_Metadata.m_MouseInputScaleForTranslationWithLookAt;
			//	translationInput.z += -control.GetPedLookUpDown().GetNorm() * m_Metadata.m_MouseInputScaleForTranslationWithLookAt;

			//	mouseInputMode = eMouseInputModes::LOOKAT_ORBIT;
			//}

			// mouse lookAt Offset
			if(leftMouseDown)
			{
				lookAtOffsetInput.x -= control.GetPedLookLeftRight().GetNorm() * m_Metadata.m_MouseInputScaleForLookAtOffset; //reversed because moving target to the right of the screen by pushing right felt wrong
				lookAtOffsetInput.y -= control.GetPedLookUpDown().GetNorm() * m_Metadata.m_MouseInputScaleForLookAtOffset; //same usage as ComputeRotationInput

				mouseInputMode = eMouseInputModes::LOOKAT_OFFSET;
			}
		}
		else
		{		
#endif
			// gamepad lookAt orbit
			translationInput.x += control.GetPedWalkLeftRight().GetNorm();
			translationInput.z += -control.GetPedWalkUpDown().GetNorm();	

			// gamepad lookAt offset
			lookAtOffsetInput.x -= control.GetPedLookLeftRight().GetNorm(); //reversed because moving target to the right of the screen by pushing right felt wrong
			lookAtOffsetInput.y -= control.GetPedLookUpDown().GetNorm(); //same usage as ComputeRotationInput
#if KEYBOARD_MOUSE_SUPPORT
		}
#endif
	}
	else
	{
		// LB/RB (E/Q) buttons for vertical translation
		if(control.GetScriptLB().IsDown())
		{
			translationInput.z -= 1.0f;
		}
		if(control.GetScriptRB().IsDown())
		{
			translationInput.z += 1.0f;
		}

		// Left stick / WASD for movement
		translationInput.x += control.GetPedWalkLeftRight().GetNorm();
		translationInput.y += -control.GetPedWalkUpDown().GetNorm();		

#if KEYBOARD_MOUSE_SUPPORT
		// Mouse scrollwheel for vertical movement
		if(control.GetValue(INPUT_CURSOR_SCROLL_UP).GetSource().m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE ||
			control.GetValue(INPUT_CURSOR_SCROLL_DOWN).GetSource().m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE)
		{
			float verticalInput = control.GetValue(INPUT_CURSOR_SCROLL_UP).GetNorm() - control.GetValue(INPUT_CURSOR_SCROLL_DOWN).GetNorm();
			translationInput.z	+= verticalInput;
		}
#endif
	}
}

void camReplayFreeCamera::UpdateRotationControl(const CControl& control, Vector3& desiredRotation)
{
	Vector3 rotationInput;
	ComputeRotationInput(control, rotationInput KEYBOARD_MOUSE_ONLY(, m_MouseInputMode));

	const float headingPitchInputMag = rotationInput.XZMag();
	if(headingPitchInputMag >= SMALL_FLOAT)
	{
		//Cache the normalised heading/pitch rotation inputs, for use in deceleration.
		m_CachedNormalisedHeadingPitchTranslationInput.InvScale(Vector2(rotationInput, Vector2::kXZ), headingPitchInputMag);
	}

	const float rollInputMag = Abs(rotationInput.y);
	if(rollInputMag >= SMALL_FLOAT)
	{
		//Cache the sign of the roll input, for use in deceleration.
		m_CachedRollRotationInputSign = Sign(rotationInput.y);
	}

	const camReplayBaseCameraMetadataInputResponse& headingPitchInputResponse	= m_Metadata.m_HeadingPitchInputResponse;
	const camReplayBaseCameraMetadataInputResponse& rollInputResponse			= m_Metadata.m_RollInputResponse;

	//Apply the custom power factor scaling specified in metadata to change the control response across the range of stick input.
	const float scaledHeadingPitchInputMag	= rage::Powf(Min(headingPitchInputMag, 1.0f), headingPitchInputResponse.m_InputMagPowerFactor);
	const float scaledRollInputMag			= rage::Powf(Min(rollInputMag, 1.0f), rollInputResponse.m_InputMagPowerFactor);

	const float timeStep = fwTimer::GetNonPausableCamTimeStep();

#if KEYBOARD_MOUSE_SUPPORT
	float fRotationMouseInputMultiplier = 1.0f;
	if(IsMouseLookInputActive(control) && 
		control.GetScriptRS().GetSource().m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE && control.GetScriptRS().IsDown())
	{
		fRotationMouseInputMultiplier = m_Metadata.m_RotationMouseInputMultiplier;		
	}
#endif // KEYBOARD_MOUSE_SUPPORT
	
	const float rollOffset = camControlHelper::ComputeOffsetBasedOnAcceleratedInput(scaledRollInputMag,
								rollInputResponse.m_MaxAcceleration, rollInputResponse.m_MaxDeceleration,
								rollInputResponse.m_MaxSpeed, m_RollSpeed, timeStep, false);

	Vector2 headingPitchRotation;

#if KEYBOARD_MOUSE_SUPPORT
	if(m_MouseInputMode == eMouseInputModes::ROTATION)
	{
		headingPitchRotation = Vector2(rotationInput, Vector2::eVector3Axes::kXZ) * fRotationMouseInputMultiplier;
	}
	else
	{	
#endif
		const float headingPitchOffset = camControlHelper::ComputeOffsetBasedOnAcceleratedInput(scaledHeadingPitchInputMag,
											headingPitchInputResponse.m_MaxAcceleration, headingPitchInputResponse.m_MaxDeceleration,
											headingPitchInputResponse.m_MaxSpeed, m_HeadingPitchSpeed, timeStep, false);
		
		headingPitchRotation.Scale(m_CachedNormalisedHeadingPitchTranslationInput, headingPitchOffset);
#if KEYBOARD_MOUSE_SUPPORT
	}	
#endif	
		
	const float rollRotation = m_CachedRollRotationInputSign * rollOffset;

	desiredRotation.Set(headingPitchRotation.x, rollRotation, headingPitchRotation.y);
}

void camReplayFreeCamera::ComputeRotationInput(const CControl& control, Vector3& rotationInput KEYBOARD_MOUSE_ONLY(, s32& mouseInputMode)) const
{
	rotationInput	= VEC3_ZERO;

	// D-pad left/right or keyboard left/right arrows (by default) for roll
	if(control.GetScriptPadLeft().IsDown())
	{
		rotationInput.y -= 1.0f;
	}
	if(control.GetScriptPadRight().IsDown())
	{		
		rotationInput.y += 1.0f;
	}
	
	// No camera heading/pitch rotation when we have a lookAt target, instead we have orbit and lookAtOffset controlled in update translation
	if(m_LookAtEntity)
	{		
		return;
	}

	// Right stick / Mouse move for rotation
	rotationInput.z	+= -control.GetPedLookLeftRight().GetNorm();
	rotationInput.x += -control.GetPedLookUpDown().GetNorm();

#if KEYBOARD_MOUSE_SUPPORT
	// Must click LMB for mouse rotation input to be active
	if(IsMouseLookInputActive(control))
	{
		if(control.GetScriptRDown().GetSource().m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE && control.GetScriptRDown().IsDown())
		{
			rotationInput.z *= m_Metadata.m_MouseInputScaleForRotation;
			rotationInput.x *= m_Metadata.m_MouseInputScaleForRotation;	

			mouseInputMode = eMouseInputModes::ROTATION;
		}
		else
		{
			rotationInput.z = 0.0f;
			rotationInput.x = 0.0f;
		}
	}
#endif	
}

void camReplayFreeCamera::UpdateZoomControl(const CControl& control, float& desiredZoom)
{
	float zoomInput;
	ComputeZoomInput(control, zoomInput);

	const camReplayBaseCameraMetadataInputResponse& zoomInputResponse = m_Metadata.m_ZoomInputResponse;

	const float timeStep = fwTimer::GetNonPausableCamTimeStep();

	desiredZoom = camControlHelper::ComputeOffsetBasedOnAcceleratedInput(zoomInput,
					zoomInputResponse.m_MaxAcceleration, zoomInputResponse.m_MaxDeceleration,
					zoomInputResponse.m_MaxSpeed, m_ZoomSpeed, timeStep, false);
}

void camReplayFreeCamera::ComputeZoomInput(const CControl& control, float& zoomInput) const
{
	zoomInput = 0.0f;
	if(control.GetScriptLT().IsDown())
	{
		zoomInput += -1.0f;
	}
	if(control.GetScriptRT().IsDown())
	{
		zoomInput += 1.0f;
	}
}

void camReplayFreeCamera::UpdateZoom(const float desiredZoom)
{
	const float maxZoomFactor	= m_Metadata.m_MaxFov / m_Metadata.m_MinFov;
	const float currentFov		= m_Frame.GetFov();

	//NOTE: We scale the zoom offset based upon the current zoom factor. This prevents zoom speed seeming faster towards the low end of the zoom range.
	float zoomFactor			= m_Metadata.m_MaxFov / currentFov;
	const float zoomOffset		= desiredZoom * zoomFactor;
	zoomFactor					+= zoomOffset;
	zoomFactor					= Clamp(zoomFactor, 1.0f, maxZoomFactor);
	const float newFov			= m_Metadata.m_MaxFov / zoomFactor;

	m_Frame.SetFov(newFov);
}

void camReplayFreeCamera::UpdateRotation(const Vector3& desiredRotation, const Vector2& desiredLookAtOffsetTranslation)
{	
	float heading, pitch, roll;
	m_Frame.ComputeHeadingPitchAndRoll(heading, pitch, roll);

	if (m_LookAtEntity)
	{
		// Get the camera and entity position, and camera to entity vector
		const Vector3& orbitPosition	= m_Frame.GetPosition();
		const Vector3 entityPosition	= VEC3V_TO_VECTOR3(m_LookAtEntity->GetTransform().GetPosition());

		// Update the lookAt offset, limiting to prevent offsets outside of the screen
		m_LookAtOffset					+= desiredLookAtOffsetTranslation;		
		m_LookAtOffset.x				= Clamp(m_LookAtOffset.x, -1.0f, 1.0f);
		m_LookAtOffset.y				= Clamp(m_LookAtOffset.y, -1.0f, 1.0f);

		// Increment the lookAt roll value
		m_LookAtRoll					+= desiredRotation.y;

        Matrix34 cameraMatrix;
		ComputeLookAtFrameWithOffset( entityPosition, orbitPosition, m_LookAtOffset, m_Frame.GetFov(), m_LookAtRoll, cameraMatrix );

		// Set the frame's new orientation
		m_Frame.SetWorldMatrix(cameraMatrix); // NOTE: For horizon correction we would need to reposition the camera
	}
	else
	{
		if (m_AttachEntity)
		{
			Matrix34 localMatrix;
			camFrame::ComputeWorldMatrixFromHeadingPitchAndRoll(m_AttachEntityRelativeHeading, m_AttachEntityRelativePitch, m_AttachEntityRelativeRoll, localMatrix);

			Vector3 cameraFront;
			Vector3 cameraUp;
			m_AttachEntityMatrix.Transform3x3(localMatrix.b, cameraFront);
			m_AttachEntityMatrix.Transform3x3(localMatrix.c, cameraUp);

			Matrix34 worldMatrix;
			camFrame::ComputeWorldMatrixFromFrontAndUp(cameraFront, cameraUp, worldMatrix);

			camFrame::ComputeHeadingPitchAndRollFromMatrixUsingEulers(worldMatrix, heading, pitch, roll);
		}	

		//NOTE: We scale the look-around offsets based upon the current FOV, as this allows the look-around input to be responsive at minimum zoom without
		//being too fast at maximum zoom.
		const float fov = m_Frame.GetFov();
		float fovOrientationScaling = fov / m_Metadata.m_DefaultFov;

		heading	+= desiredRotation.z * fovOrientationScaling;
		heading	= fwAngle::LimitRadianAngle(heading);

		pitch	+= desiredRotation.x * fovOrientationScaling;
		pitch	= Clamp(pitch, -m_Metadata.m_MaxPitch * DtoR, m_Metadata.m_MaxPitch * DtoR);

		roll	+= desiredRotation.y;
		roll	= fwAngle::LimitRadianAngle(roll);

		if (abs(roll) < SMALL_FLOAT || abs(desiredRotation.y) >= SMALL_FLOAT)
		{
			m_TranslationInputThisUpdate = VEC3_ZERO;
			m_RollCorrectionBlendLevelSpring.Reset();
		}

		if (m_TranslationInputThisUpdate.Mag2() >= (m_Metadata.m_RollCorrectionMinStickInput * m_Metadata.m_RollCorrectionMinStickInput))
		{
			m_DurationOfMaxStickInputForRollCorrection += fwTimer::GetNonPausedTimeStepInMilliseconds();
		}
		else
		{
			m_DurationOfMaxStickInputForRollCorrection = 0;
		}

		const bool isCurrentlyEditingThisMarker		= (CReplayMarkerContext::GetEditingMarkerIndex() == m_MarkerIndex);
		const bool wasRollAppliedToRecordedCamera	= (abs(CReplayMgr::GetReplayFrameData().m_cameraData.m_Frame.ComputeRoll()) >= m_Metadata.m_RecordedRollThresholdForRollCorrection * DtoR);

		if (isCurrentlyEditingThisMarker && wasRollAppliedToRecordedCamera &&
			(m_DurationOfMaxStickInputForRollCorrection >= m_Metadata.m_MaxStickInputTimeForRollCorrection || m_RollCorrectionBlendLevelSpring.GetVelocity() >= SMALL_FLOAT)) //if the correction has started, keep going until it's finished
		{
			//we're editing a camera, so there can't be any cuts
			const float springConstant				= (m_NumReplayUpdatesPerformed == 0) ? 0.0f : m_Metadata.m_RollCorrectionSpringConstant;
			const float rollCorrectionBlendLevel	= m_RollCorrectionBlendLevelSpring.Update(1.0f, springConstant, m_Metadata.m_RollCorrectionSpringDampingRatio, false, false, true);
			roll									= RampValueSafe(rollCorrectionBlendLevel, 0.0f, 1.0f, roll, 0.0f); //non-linear, but looks fine
		}

		m_Frame.SetWorldMatrixFromHeadingPitchAndRoll(heading, pitch, roll);
	}
}

void camReplayFreeCamera::UpdatePosition(const Vector3& desiredTranslation)
{
	//m_DesiredPosition is relative if we have an attach entity
	Vector3 initialPosition = m_DesiredPosition;
	if (m_AttachEntity)
	{
		m_AttachEntityMatrix.Transform(initialPosition);
	}

	const bool isCurrentlyEditingThisMarker	= (CReplayMarkerContext::GetEditingMarkerIndex() == m_MarkerIndex);
	if (isCurrentlyEditingThisMarker && m_LastEditingMarkerIndex == -1) //just entered edit mode
	{
		SetToSafePosition(initialPosition, true);
	}

	Vector3 positionToApply(initialPosition);

	if(m_LookAtEntity)
	{
		const Vector3 lookAtPosition	= VEC3V_TO_VECTOR3(m_LookAtEntity->GetTransform().GetPosition());
		const Vector3 lookAtToCamera	= initialPosition - lookAtPosition;

		Vector3 lookAtToCameraFront(lookAtToCamera);
		lookAtToCameraFront.NormalizeSafe(YAXIS);

		Matrix34 lookAtToCameraMatrix;
		camFrame::ComputeWorldMatrixFromFront(lookAtToCameraFront, lookAtToCameraMatrix);

		//scale heading speed to reduce speed close to the poles, sweeping out the same arc length per unit of time
		const float headingSpeedFactor = sqrtf(1.0f - lookAtToCameraFront.z * lookAtToCameraFront.z);

		lookAtToCameraMatrix.RotateLocalZ(desiredTranslation.x * headingSpeedFactor); //heading
		lookAtToCameraMatrix.RotateLocalX(desiredTranslation.z); //pitch

		//limit pitch
		float heading, pitch, roll;
		camFrame::ComputeHeadingPitchAndRollFromMatrix(lookAtToCameraMatrix, heading, pitch, roll);
		pitch = Clamp(pitch, -m_Metadata.m_MaxPitch * DtoR, m_Metadata.m_MaxPitch * DtoR);
		camFrame::ComputeWorldMatrixFromHeadingPitchAndRoll(heading, pitch, roll, lookAtToCameraMatrix);

		const float boundRadius			= ComputeBoundRadius(m_LookAtEntity);
		const float orbitDistance		= lookAtToCamera.Mag();
		const float newOrbitDistance	= Max(orbitDistance - desiredTranslation.y, boundRadius);

		const Vector3 newLookAtToCameraFront = lookAtToCameraMatrix.b;
		positionToApply = lookAtPosition + (newLookAtToCameraFront * newOrbitDistance);
	}
	else
	{
		Vector3 relativeTranslation;
		m_Frame.GetWorldMatrix().Transform3x3(desiredTranslation, relativeTranslation);

		positionToApply += relativeTranslation;
	}

    // Removing the world limits clamp as it is not representative of the size of the map and is causing issues when the replay is recorded near the edges [8/10/2015 carlo.mangani]

	//First constrain the camera to the world limits.
	//const Vector3& worldMin = g_WorldLimits_MapDataExtentsAABB.GetMinVector3();
	//Vector3 worldMax = g_WorldLimits_MapDataExtentsAABB.GetMaxVector3();

	////limit to 10m below because the player is placed at the camera origin, causing him to be out of the world bounds
	//worldMax.z = g_WorldLimits_AABB.GetMaxVector3().z - 10.0f; 

	//positionToApply.Min(positionToApply, worldMax);
	//positionToApply.Max(positionToApply, worldMin);

	const bool isPositionOutsidePlayerLimits = IsPositionOutsidePlayerLimits(positionToApply);

	RenderCameraConstraintMarker(isPositionOutsidePlayerLimits);

	if (isPositionOutsidePlayerLimits && isCurrentlyEditingThisMarker)
	{
		//NOTE: GetMainPlayerPtr() has already been verified
		const Vector3& playerPedPosition	= VEC3V_TO_VECTOR3(CReplayMgr::GetMainPlayerPtr()->GetTransform().GetPosition());
		const Vector3 playerPedToCamera		= positionToApply - playerPedPosition;

		Vector3 maxPlayerPedToCamera(playerPedToCamera);
		maxPlayerPedToCamera.NormalizeSafe(YAXIS);
		const float maxDistance = camInterface::GetReplayDirector().GetMaxDistanceAllowedFromPlayer(true); // Get the max distance allowed from the player,
		maxPlayerPedToCamera.Scale(maxDistance - 0.001f);	// subtract a little so next frame it's not constrained unless we're still pushing out.

		positionToApply						= playerPedPosition + maxPlayerPedToCamera;
	}

	//Vertically constrain against simulated water.
	//NOTE: River bounds are covered by the collision shapetests below.
	const float maxDistanceToTestForRiver = m_Metadata.m_CollisionSettings.m_MinHeightAboveOrBelowWater * 4.0f;
	float waterHeight			= 0.0f;
	const bool hasFoundWater	= camCollision::ComputeWaterHeightAtPosition(positionToApply, LARGE_FLOAT, waterHeight, maxDistanceToTestForRiver, 0.5f, false);

	if(!hasFoundWater)
	{
		m_SmoothedWaterHeight = g_InvalidWaterHeight;
	}
	else
	{
		if(IsClose(m_SmoothedWaterHeight, g_InvalidWaterHeight))
		{
			m_SmoothedWaterHeight = waterHeight;
		}
		else
		{
			//Cheaply make the smoothing frame-rate independent.
			float smoothRate		= m_Metadata.m_CollisionSettings.m_WaterHeightSmoothRate * 30.0f * fwTimer::GetNonPausableCamTimeStep();
			smoothRate				= Min(smoothRate, 1.0f);
			m_SmoothedWaterHeight	= Lerp(smoothRate, m_SmoothedWaterHeight, waterHeight);
		}

		const float heightDelta = positionToApply.z - m_SmoothedWaterHeight;
		const bool isBuoyant	= (heightDelta >= 0.0f);

		// Test if the water is too shallow for twice min distance
		WorldProbe::CShapeTestCapsuleDesc capsuleTest;
		WorldProbe::CShapeTestHitPoint hitPoint;
		WorldProbe::CShapeTestResults shapeTestResults(hitPoint);		
		capsuleTest.SetResultsStructure(&shapeTestResults);
		capsuleTest.SetContext(WorldProbe::LOS_Camera);
		capsuleTest.SetIsDirected(true);	
		const u32 staticIncludeFlags	= static_cast<u32>(ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_GLASS_TYPE | ArchetypeFlags::GTA_DEEP_SURFACE_TYPE);
		capsuleTest.SetIncludeFlags(staticIncludeFlags);

		const float twiceMinHeight = m_Metadata.m_CollisionSettings.m_MinHeightAboveOrBelowWater * 2;		
		const Vector3 probeStartPosition(positionToApply.x, positionToApply.y, waterHeight);
		const Vector3 probeEndPosition(probeStartPosition - Vector3(0.0f, 0.0f, twiceMinHeight));			
		capsuleTest.SetCapsule(probeStartPosition, probeEndPosition, m_Metadata.m_CapsuleRadius);

		const bool hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);
		const float hitTValue	= hasHit ? shapeTestResults[0].GetHitTValue() : 1.0f;
		const float hitDepth	= hitTValue * (twiceMinHeight + m_Metadata.m_CapsuleRadius); // We only need to include the capsule radius once because the other hemisphere is above the water

		const bool hitDepthBelowMinHeight		= hitDepth < m_Metadata.m_CollisionSettings.m_MinHeightAboveOrBelowWater;
		const bool hitDepthBelowTwiceMinHeight	= hitDepth < twiceMinHeight;		
		
		//NOTE: We apply a soft limit from twice the min delta, rather than a hard limit at the minimum.				
		const float buoyancySign		= (hitDepthBelowMinHeight || isBuoyant) ? 1.0f : -1.0f; // If the water depth is below m_MinHeightAboveOrBelowWater we force the player above the water
		const int minHeightMultiplier	= (!isBuoyant && hitDepthBelowTwiceMinHeight) ? 1 : 2; // We reduce the minHeightMultiplier to allow for shallow water
		const float heightDeltaRatio	= heightDelta / (m_Metadata.m_CollisionSettings.m_MinHeightAboveOrBelowWater * minHeightMultiplier);		

		if(isCurrentlyEditingThisMarker && (buoyancySign * heightDeltaRatio) >= SMALL_FLOAT)
		{
			const float pushAwayRatio		= SlowIn(1.0f - Min(Abs(heightDeltaRatio), 1.0f));
			const float distanceToPushAway	= pushAwayRatio * m_Metadata.m_CollisionSettings.m_MinHeightAboveOrBelowWater * 30.0f * fwTimer::GetNonPausableCamTimeStep();
			positionToApply.z				+= buoyancySign * distanceToPushAway;
		}
		else if(Abs(heightDelta) < twiceMinHeight)
		{
			positionToApply.z				= waterHeight + (buoyancySign * m_Metadata.m_CollisionSettings.m_MinHeightAboveOrBelowWater * minHeightMultiplier);
		}
	}

	//we always run the attach entity collision update if we have an attach entity and we're not in edit mode
	//otherwise we only run the regular collision update if we're in edit mode
	bool wasConstrainedAgainstCollision = false;
	if (m_AttachEntity && !isCurrentlyEditingThisMarker)
	{
		//Constrain towards the attach entity.
		wasConstrainedAgainstCollision = UpdateCollisionWithAttachEntity(*m_AttachEntity.Get(), positionToApply);
	}
	else
	{
		//Constrain against world collision.
		wasConstrainedAgainstCollision = UpdateCollision(initialPosition, positionToApply);
		m_NumCollisionUpdatesPerformedWithAttachEntity = 0;
	}

	//Zero the cached speeds when we constrain against collision.
	if(wasConstrainedAgainstCollision)
	{
		m_HorizontalSpeed = m_VerticalSpeed = m_HorizontalLookAtOffsetSpeed = m_VerticalLookAtOffsetSpeed = 0.0f;
	}

	m_Frame.SetPosition(positionToApply);
}

bool camReplayFreeCamera::UpdateCollisionWithAttachEntity(const CEntity& attachEntityToUse, Vector3& cameraPosition)
{
	if(m_Collision == NULL)
	{
		return false;
	}

	const camReplayBaseCameraMetadataCollisionSettings& settings = m_Metadata.m_CollisionSettings;

	m_Collision->IgnoreEntityThisUpdate(attachEntityToUse);
	m_Collision->IgnoreCollisionWithTouchingPedsAndVehiclesThisUpdate(); //we ignore any ped or vehicle the player is touching
	m_Collision->SetShouldIgnoreDynamicCollision(true);

	const bool shouldPushBeyondAttachParentIfClipping = settings.m_ShouldPushBeyondAttachParentIfClipping; //TODO: deal with ragdolls somehow
	if(shouldPushBeyondAttachParentIfClipping)
	{
		m_Collision->PushBeyondEntityIfClippingThisUpdate(attachEntityToUse);
	}

	//TODO: Deal with trailer or towed vehicles

	//we don't worry about vehicles on top of vehicles

	float desiredCollisionTestRadius = m_Metadata.m_MaxCollisionTestRadius;

	TUNE_GROUP_BOOL(REPLAY_FREE_CAMERA, bAvoidCollisionWithAllNearbyParachutes, true);
	if( !bAvoidCollisionWithAllNearbyParachutes )
	{
		//If the attach ped is using a parachute, ignore it.
		const fwAttachmentEntityExtension *pedAttachmentExtension = attachEntityToUse.GetAttachmentExtension();
		// Find parachute via attachments
		if(pedAttachmentExtension)
		{
			CPhysical* pNextChild = (CPhysical *) pedAttachmentExtension->GetChildAttachment();
			while(pNextChild)
			{
				if(pNextChild->GetIsTypeObject())
				{
					CObject* pObject = static_cast<CObject*>(pNextChild);
					if( pObject )
					{
						if(pObject->GetIsParachute())
						{
							m_Collision->IgnoreEntityThisUpdate(*pObject);
							break;
						}
					}
				}
				fwAttachmentEntityExtension *nextChildAttachExt = pNextChild->GetAttachmentExtension();
				pNextChild = (CPhysical *) nextChildAttachExt->GetSiblingAttachment();
			}
		}
	}
	else
	{
		camReplayBaseCamera::AvoidCollisionWithParachutes(m_AttachEntityMatrix.d, m_Collision.Get());
	}

	if(attachEntityToUse.GetIsTypePed())
	{
        const CPed* trackingPed	= static_cast<const CPed*>(&attachEntityToUse);

		const float moverCapsuleRadius	= trackingPed->GetCurrentMainMoverCapsuleRadius();
		float radius					= moverCapsuleRadius;

		//we have an overridden mover capsule radius for non-bipeds that return a radius of 0.0
		const CBaseCapsuleInfo* capsuleInfo = trackingPed->GetCapsuleInfo();
		if (capsuleInfo && (!capsuleInfo->IsBiped()) && (moverCapsuleRadius < SMALL_FLOAT))
		{
			radius						= m_Metadata.m_MoverCapsuleRadiusOverrideForNonBipeds;
		}

		radius							-= settings.m_MinSafeRadiusReductionWithinPedMoverCapsule;
		if(radius >= SMALL_FLOAT)
		{
			desiredCollisionTestRadius = Min(radius, desiredCollisionTestRadius);
		}
	}

	desiredCollisionTestRadius = Max(desiredCollisionTestRadius, m_Metadata.m_MinCollisionTestRadius);

	const float collisionTestRadiusOnPreviousUpdate = m_CollisionTestRadiusSpring.GetResult();

	//NOTE: We must cut to a lesser radius in order to avoid compromising the shapetests.
	const bool shouldCutSpring		= ((m_NumCollisionUpdatesPerformedWithAttachEntity == 0) ||
										m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
										m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation) ||
										(desiredCollisionTestRadius < collisionTestRadiusOnPreviousUpdate));
	const float springConstant		= shouldCutSpring ? 0.0f : settings.m_TestRadiusSpringConstant;

	float collisionTestRadius		= m_CollisionTestRadiusSpring.Update(desiredCollisionTestRadius, springConstant,
										settings.m_TestRadiusSpringDampingRatio, true);

	UpdateCollisionOrigin(attachEntityToUse, collisionTestRadius);

	UpdateCollisionRootPosition(attachEntityToUse, collisionTestRadius);

	if(m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation) || (m_NumCollisionUpdatesPerformedWithAttachEntity == 0)) 
	{
		//Bypass any orbit distance damping and force pop-in behaviour.
		m_Collision->Reset();
	}

	//we don't behave differently during a networked game

	m_Collision->SetEntityToConsiderForTouchingChecksThisUpdate(attachEntityToUse);

	const Vector3 preCollisionCameraPosition(cameraPosition);

	float orbitHeading = 0.0f;
	float orbitPitch = 0.0f;
	m_Collision->Update(m_CollisionRootPosition, collisionTestRadius, true, cameraPosition, orbitHeading, orbitPitch);

	++m_NumCollisionUpdatesPerformedWithAttachEntity;

	if (cameraPosition.Dist2(preCollisionCameraPosition) >= VERY_SMALL_FLOAT)
	{
        //B*2298646 only fire the warning if we are not jumping.
        const bool replayIsJumping   = (CReplayMgr::IsJumping()   || CReplayMgr::WasJumping());
        const bool replayIsScrubbing = (CReplayMgr::IsScrubbing() || CReplayMgr::IsFineScrubbing()); 
		if ( ! (replayIsJumping && ! replayIsScrubbing) && m_NumCollisionUpdatesPerformedWithAttachEntity == 1)
		{
			// Flag the warning state so the UI can display some helpful text to the player
			CReplayMarkerContext::SetWarning( CReplayMarkerContext::EDIT_WARNING_LOS_COLLISION );
		}
		return true;
	}
	else
	{
		return false;
	}
}

void camReplayFreeCamera::UpdateCollisionOrigin(const CEntity& attachEntityToUse, float& collisionTestRadius)
{
	ComputeCollisionOrigin(attachEntityToUse, collisionTestRadius, m_CollisionOrigin);
}

void camReplayFreeCamera::ComputeCollisionOrigin(const CEntity& attachEntityToUse, float& UNUSED_PARAM(collisionTestRadius), Vector3& collisionOrigin) const
{
	collisionOrigin = VEC3V_TO_VECTOR3(attachEntityToUse.GetTransform().GetPosition());

	//We want to do shape test here (from line 2510 third person camera)
}

void camReplayFreeCamera::UpdateCollisionRootPosition(const CEntity& attachEntityToUse, float& collisionTestRadius)
{
	ComputeCollisionRootPosition(attachEntityToUse, m_CollisionOrigin, collisionTestRadius, m_CollisionRootPosition);
}

void camReplayFreeCamera::ComputeCollisionRootPosition(const CEntity& attachEntityToUse, const Vector3& collisionOrigin, float& collisionTestRadius, Vector3& collisionRootPosition) const
{
	//Fall-back to an offset from the (safe) collision origin.

	collisionRootPosition = collisionOrigin;

	Vector3 relativeCollisionOrigin;
	const Matrix34 trackingEntityTransform = m_AttachEntity ? m_AttachEntityMatrix : MAT34V_TO_MATRIX34(attachEntityToUse.GetTransform().GetMatrix());
	trackingEntityTransform.UnTransform(collisionOrigin, relativeCollisionOrigin); //Transform to parent-local space.

	const Vector3& trackingEntityBoundingBoxMin = attachEntityToUse.GetBoundingBoxMin();
	const Vector3& trackingEntityBoundingBoxMax = attachEntityToUse.GetBoundingBoxMax();
	const float parentHeight = trackingEntityBoundingBoxMax.z - trackingEntityBoundingBoxMin.z;

	const float trackingEntityHeightRatioToAttain = m_Metadata.m_CollisionSettings.m_TrackingEntityHeightRatioToAttain;

	//we don't bother springing this offset
	const CPed* trackingPed = attachEntityToUse.GetIsTypePed() ? static_cast<const CPed*>(&attachEntityToUse) : NULL;
	const float extraOffset	= trackingPed ? trackingPed->GetIkManager().GetPelvisDeltaZForCamera(false) : 0.0f;

	const float collisionFallBackOffset	= (parentHeight * trackingEntityHeightRatioToAttain) + trackingEntityBoundingBoxMin.z - relativeCollisionOrigin.z + extraOffset;

	collisionRootPosition.z	+= collisionFallBackOffset;

	WorldProbe::CShapeTestCapsuleDesc capsuleTest;
	WorldProbe::CShapeTestFixedResults<> shapeTestResults;
	capsuleTest.SetResultsStructure(&shapeTestResults);
	capsuleTest.SetIsDirected(true);
	//capsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	capsuleTest.SetContext(WorldProbe::LOS_Camera);

	const u32 staticIncludeFlags = static_cast<u32>(ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_GLASS_TYPE | ArchetypeFlags::GTA_DEEP_SURFACE_TYPE);
	capsuleTest.SetIncludeFlags(staticIncludeFlags);

	//NOTE: m_Collision has already been validated.
	const CEntity** entitiesToIgnore	= const_cast<const CEntity**>(m_Collision->GetEntitiesToIgnoreThisUpdate());
	s32 numEntitiesToIgnore				= m_Collision->GetNumEntitiesToIgnoreThisUpdate();

	atArray<const CEntity*> entitiesToIgnoreList;
	if(numEntitiesToIgnore > 0)
	{
		capsuleTest.SetExcludeEntities(entitiesToIgnore, numEntitiesToIgnore, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
	}

	const float collisionOriginToCollisionRootPositionDistance2 = collisionOrigin.Dist2(collisionRootPosition);
	if(collisionOriginToCollisionRootPositionDistance2 > VERY_SMALL_FLOAT)
	{
		if(!cameraVerifyf(collisionTestRadius >= SMALL_FLOAT,
			"Unable to compute a valid collision fall back position due to an invalid test radius."))
		{
			return;
		}

		shapeTestResults.Reset();
		capsuleTest.SetCapsule(collisionOrigin, collisionRootPosition, collisionTestRadius);

		//Reduce the test radius in readiness for use in a consecutive test.
		collisionTestRadius -= camCollision::GetMinRadiusReductionBetweenInterativeShapetests();

		const bool hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);
		const float blendLevel = hasHit ? ComputeFilteredHitTValue(shapeTestResults, false) : 1.0f;

		collisionRootPosition.Lerp(blendLevel, collisionOrigin, collisionRootPosition);
	}
}

bool camReplayFreeCamera::UpdateCollision(const Vector3& initialPosition, Vector3& cameraPosition) const
{
	Vector3 cameraPositionBeforeCollision(cameraPosition);

	//Default to persisting the initial position to maintain a safe distance from collision.
	cameraPosition = initialPosition;

	const Vector3 desiredTranslation	= cameraPositionBeforeCollision - initialPosition;
	const float desiredTranslationMag2	= desiredTranslation.Mag2();
	if(desiredTranslationMag2 < VERY_SMALL_FLOAT)
	{
		return false;
	}

	const u32 staticIncludeFlags = static_cast<u32>(ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_GLASS_TYPE | ArchetypeFlags::GTA_DEEP_SURFACE_TYPE);

	//Sweep a sphere to the desired camera position.
	WorldProbe::CShapeTestCapsuleDesc capsuleTest;
	WorldProbe::CShapeTestFixedResults<> shapeTestResults;
	capsuleTest.SetResultsStructure(&shapeTestResults);
	capsuleTest.SetIncludeFlags(staticIncludeFlags);
	//capsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	capsuleTest.SetContext(WorldProbe::LOS_Camera);
	capsuleTest.SetIsDirected(true);
	capsuleTest.SetCapsule(initialPosition, cameraPositionBeforeCollision, m_Metadata.m_CapsuleRadius);

	bool hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);
	float hitTValue = hasHit ? ComputeFilteredHitTValue(shapeTestResults, false) : 1.0f;
	cameraPosition.Lerp(hitTValue, initialPosition, cameraPositionBeforeCollision);

	//Now perform a sphere test at the prospective camera position, with a radius mid-way between that of the swept sphere and the non-directed
	//capsule (below). This ensures that the camera cannot be placed so close to collision that the non-directed test could always hit and leave
	//the camera permanently stuck to the collision surface.

	const float nonDirectedCapsuleRadius	= m_Metadata.m_CapsuleRadius - camCollision::GetMinRadiusReductionBetweenInterativeShapetests() * 2.0f;
	
	//Finally performed a slightly smaller directed capsule test from the initial position to the prospective camera position to apply.
	//This ensures that this path is completely clear and avoids problems associated with the initial position of the swept sphere test.
	shapeTestResults.Reset();
	capsuleTest.SetResultsStructure(&shapeTestResults);
	capsuleTest.SetIsDirected(true);
	capsuleTest.SetCapsule(initialPosition, cameraPosition, nonDirectedCapsuleRadius);

	hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);
	hitTValue = hasHit ? ComputeFilteredHitTValue(shapeTestResults, false) : 1.0f;
	if(hasHit && hitTValue < 1.0f)
	{
		//The path is not clear, so revert to persisting the initial position.
		cameraPosition	= initialPosition;
		return true;
	}

	//Don't allow passing through one way portals
	s32 currentRoom = CPortalVisTracker::GetPrimaryRoomIdx();
	CInteriorInst* pIntInst = CPortalVisTracker::GetPrimaryInteriorInst();
	if (pIntInst && currentRoom > -1)
	{
		CMloModelInfo *pModelInfo = pIntInst->GetMloModelInfo();
		u32 numPortalInRoom = pIntInst->GetNumPortalsInRoom(currentRoom);

		//Increase the endposition by the nonDirectedCapsuleRadius to avoid intersection
		Vector3 vDir = (cameraPositionBeforeCollision - initialPosition);
		vDir.NormalizeSafe();
		Vector3 endPosition = cameraPosition + (vDir * nonDirectedCapsuleRadius);
		fwGeom::fwLine traversalTestLine;
		fwPortalCorners::CreateExtendedLineForPortalTraversalCheck(initialPosition, endPosition, traversalTestLine);

		// check against all of the portals in this room
		for (int i=0;i<numPortalInRoom;i++)
		{
			u32	interiorPortalIdx = pIntInst->GetPortalIdxInRoom(currentRoom,i);
			const CMloPortalDef &portal = pModelInfo->GetPortals()[interiorPortalIdx];
			CPortalFlags portalFlags = CPortalFlags(portal.m_flags);

			// Only limit one way portals
			if(portalFlags.GetIsOneWayPortal())
			{
				fwPortalCorners portalCorners = pIntInst->GetPortal(interiorPortalIdx);				

				// intersected with any portals?
				if (portalCorners.HasPassedThrough(traversalTestLine, false))
				{
					//The path is not clear, so revert to persisting the initial position.
					cameraPosition	= initialPosition;
					return true;
				}
			}
		}
	}

	//Don't allow the camera to go through doors if they aren't attach to a portal.
	const u32 doorIncludeFlags = static_cast<u32>(ArchetypeFlags::GTA_DOOR_OBJECT_INCLUDE_TYPES);
	capsuleTest.Reset();
	shapeTestResults.Reset();
	capsuleTest.SetResultsStructure(&shapeTestResults);
	capsuleTest.SetIncludeFlags(doorIncludeFlags );
	capsuleTest.SetContext(WorldProbe::LOS_Camera);
	capsuleTest.SetIsDirected(true);
	capsuleTest.SetCapsule(initialPosition, cameraPositionBeforeCollision, m_Metadata.m_CapsuleRadius);
	hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);
	hitTValue = hasHit ? ComputeFilteredHitTValue(shapeTestResults, false) : 1.0f;
	if(hasHit && hitTValue < 1.0f)
	{
		for(WorldProbe::ResultIterator it = shapeTestResults.begin(); it < shapeTestResults.last_result(); ++it)
		{
			if(it->GetHitInst())
			{
				CEntity* pHitEntity = CPhysics::GetEntityFromInst(it->GetHitInst());
				if(pHitEntity && pHitEntity->GetIsTypeObject())
				{
					CObject* pObject = static_cast<CObject*>(pHitEntity);
					if(pObject && pObject->IsADoor() && pObject->IsBaseFlagSet(fwEntity::IS_FIXED))
					{
						CDoor* pDoor = static_cast<CDoor*>(pObject);
						fwInteriorLocation interiorLoc = pObject->GetInteriorLocation();
						if (interiorLoc.IsValid())
						{
							CInteriorProxy *pInteriorProxy = CInteriorProxy::GetFromLocation(interiorLoc);

							if(pInteriorProxy && pInteriorProxy->GetIsCappedAtPartial())
							{
								cameraPosition	= initialPosition;
								return true;
							}
						}
								
						fwBaseEntityContainer* pOwner = pDoor->GetOwnerEntityContainer();
						if (pOwner)
						{
							fwSceneGraphNode* pSceneNode = pOwner->GetOwnerSceneNode();
							if (pSceneNode)
							{
								if(!pSceneNode->IsTypePortal())								
								{
									cameraPosition	= initialPosition;
									return true;
								}
							}
						}
					}
				}
			}
		}
	}
	
	//Hack - has wrong collision, don't allow camera above Z position
	if(pIntInst)
	{
		CInteriorProxy* pProxy = pIntInst->GetProxy();
		if(pProxy)
		{
			const sAdditionalCollisionInteriorChecks* interior = sm_AdditionalCollisionInteriorChecks;
			while(interior->interiorHash != 0)
			{
				u32 interiorHash = interior->interiorHash;
				BANK_ONLY( interiorHash = ATSTRINGHASH(interior->interiorName, interior->interiorHash); )
				if(interiorHash == pProxy->GetNameHash() &&
					cameraPosition.GetZ() > interior->interiorMaxZHeight &&
					(interior->interiorRoom == -1 || interior->interiorRoom == currentRoom))
				{
					cameraPosition	= initialPosition;
					return true;
				}
				++interior;
			}

			// Don't want to be able to enter some rooms in police station
			if (pProxy->GetNameHash() == ATSTRINGHASH("v_policehub", 0xA50BA987) && currentRoom == 10)
			{		
				if(cameraPositionBeforeCollision.GetY() < -989.4f)
				{
					cameraPosition = initialPosition;
					return true;
				}
			}
		}
	}

    //Hack2 for Jacuzzi on Yacht. Detect the object apa_mp_apa_yacht_jacuzzi_ripple1 and clip the camera Z to its position plus a small threshold.
    //The distance is computed in 2D because we know we cannot go beneath the hot tub with the camera. Any object added to the test list which can be flight beneath will break this test.
    //The test is considering a static range of 2.84 meters which is the size of the object covering the water surface. This number needs to be tweaked if bigger objects
    //will be added to the additional test. Also, an additional radius test needs to be made if objects of different size are included.
    static const float testRange = 2.84f;
    static const s32 maxObjects = 32;
    const size_t additionalObjectsCount = sizeof(sm_AdditionalObjectChecks) / sizeof(sAdditionalObjectsChecks);
    
    CEntity* objectsList[maxObjects];
    memset(objectsList, 0, sizeof(CEntity*) * maxObjects);
    s32 objectsFound = 0;

    CGameWorld::FindObjectsInRange(cameraPosition, testRange, true, &objectsFound, maxObjects, objectsList, false, false, false, true, true);
    for(s32 i = 0; i < objectsFound; ++i)
    {
        CEntity* pEntity = objectsList[i];
        if(pEntity == nullptr)
        {
            continue;
        }

        for(size_t k = 0; k < additionalObjectsCount; ++k)
        {
            if(pEntity->GetModelNameHash() == sm_AdditionalObjectChecks[k].modelHash)
            {
                const Vec3V entityPosition = pEntity->GetMatrix().d();
                const float entityPositionZ = entityPosition.GetZf() + sm_AdditionalObjectChecks[k].zOffset;
                cameraPosition.z = Max(entityPositionZ, cameraPosition.z);
            }
        }
    }
	return false;
}

float camReplayFreeCamera::ComputeFilteredHitTValue(const WorldProbe::CShapeTestFixedResults<>& shapeTestResults, bool isSphereTest) const
{
	const phMaterialMgrGta::Id materialFlags = PGTAMATERIALMGR->GetPackedPolyFlagValue(POLYFLAG_NO_CAM_COLLISION_ALLOW_CLIPPING);
	const s32 unpackedPolyFlagsToIgnore = PGTAMATERIALMGR->UnpackPolyFlags(materialFlags);

	const u32 numberOfHits = shapeTestResults.GetNumHits();

	float hitTValue = 1.0f;
	for(int index = 0; index < numberOfHits; ++index)
	{
		const WorldProbe::CShapeTestHitPoint& hitPoint = shapeTestResults[index];
		phMaterialMgrGta::Id hitPointMaterial = hitPoint.GetHitMaterialId();
		const s32 unpackedPolyFlags = PGTAMATERIALMGR->UnpackPolyFlags(hitPointMaterial);
		if((unpackedPolyFlags & unpackedPolyFlagsToIgnore) == 0)
		{
			hitTValue = Min(hitTValue, isSphereTest ? 0.0f : hitPoint.GetHitTValue());
		}
	}
	return hitTValue;
}

bool camReplayFreeCamera::GetShouldResetCamera(const CControl& control) const
{
	const bool isCurrentlyEditingThisMarker	= (CReplayMarkerContext::GetEditingMarkerIndex() == m_MarkerIndex);
	if (!isCurrentlyEditingThisMarker)
	{
		return false;
	}

	const bool leftMouseDown = control.GetScriptRDown().GetSource().m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE && control.GetScriptRDown().IsDown();
	const bool leftStickOrCtrlPressed = control.GetScriptLS().IsPressed();

	return leftStickOrCtrlPressed && !leftMouseDown;
}

void camReplayFreeCamera::ResetForNewAttachEntity()
{
	//NOTE: makerStorage and markerInfo have already been verified.
	sReplayMarkerInfo* markerInfo = CReplayMarkerContext::GetMarkerStorage()->GetMarker(m_MarkerIndex);
	const CEntity* newAttachEntity = CReplayMgr::GetEntity(markerInfo->GetAttachTargetId());

	//NOTE: This forces a recalculation of the desired attach position in LoadEntities() and a call to SaveMarkerCameraInfo() in PostUpdate()
	//This is safe, because the only path into this function is by changing UI (but we're not in "edit mode" so are having to force it like this)
	m_ResetForNewAttachEntityThisUpdate = true;

	if (newAttachEntity && m_AttachEntity != newAttachEntity)
	{
		markerInfo->SetAttachEntityMatrixValid(false); //we need to recalculate the full attach entity matrix
	}
}

bool camReplayFreeCamera::ComputeSafePosition(const CEntity& entity, Vector3& cameraPosition) const
{
	//do sphere test at current position
	//if it hits, find the first safe position from the player

	WorldProbe::CShapeTestSphereDesc sphereTest;
	const u32 staticIncludeFlags = static_cast<u32>(ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_GLASS_TYPE | ArchetypeFlags::GTA_DEEP_SURFACE_TYPE);
	sphereTest.SetIncludeFlags(staticIncludeFlags);

	WorldProbe::CShapeTestFixedResults<> shapeTestResults;
	sphereTest.SetResultsStructure(&shapeTestResults);
	//sphereTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	sphereTest.SetContext(WorldProbe::LOS_Camera);
	sphereTest.SetSphere(cameraPosition, m_Metadata.m_CapsuleRadius);

	bool hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(sphereTest);
	const float collisionTValue = ComputeFilteredHitTValue(shapeTestResults, true);
	if (!hasHit || (1.0f - collisionTValue) < SMALL_FLOAT)
	{
		return false;
	}
	
	float collisionTestRadius = m_Metadata.m_CapsuleRadius + m_Metadata.m_CollisionRootPositionCapsuleRadiusIncrement;
	Vector3 collisionOrigin;
	ComputeCollisionOrigin(entity, collisionTestRadius, collisionOrigin);
	Vector3 collisionRootPosition;
	ComputeCollisionRootPosition(entity, collisionOrigin, collisionTestRadius, collisionRootPosition);

	WorldProbe::CShapeTestCapsuleDesc capsuleTest;
	shapeTestResults.Reset();
	capsuleTest.SetResultsStructure(&shapeTestResults);
	capsuleTest.SetIncludeFlags(staticIncludeFlags);
	capsuleTest.SetContext(WorldProbe::LOS_Camera);
	capsuleTest.SetIsDirected(true);
	capsuleTest.SetCapsule(collisionRootPosition, cameraPosition, collisionTestRadius);

	hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);
	const float hitTValue = ComputeFilteredHitTValue(shapeTestResults, false);
	if(hasHit || hitTValue < 1.0f)
	{
		//this is guaranteed to be > SMALL_FLOAT
		const float distanceOfTest = cameraPosition.Dist(collisionRootPosition);

		//we only want to reduce the test further if there was a hit
		const float capsuleRadiusRatio = collisionTestRadius / distanceOfTest;

		const float hitTValueReducedByCapsuleRadius = Clamp(hitTValue - capsuleRadiusRatio, 0.0f, 1.0f);
		cameraPosition.Lerp(hitTValueReducedByCapsuleRadius, collisionRootPosition, cameraPosition);
	}

	return true;
}

void camReplayFreeCamera::ResetForNewLookAtEntity()
{
	//NOTE: makerStorage and markerInfo have already been verified.
	sReplayMarkerInfo* markerInfo = CReplayMarkerContext::GetMarkerStorage()->GetMarker(m_MarkerIndex);

	const CEntity* lastLookAtEntity	= m_LookAtEntity;
	m_LookAtEntity					= CReplayMgr::GetEntity(markerInfo->GetLookAtTargetId());

	if (m_LookAtEntity != lastLookAtEntity)
	{
		m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutOrientation);
		m_LookAtOffset = Vector2(0.0f, 0.0f);
	}

	//look at entities affect save data, so save it now in case we're not in edit mode
	SaveMarkerCameraInfo(*markerInfo, m_Frame);
}

void camReplayFreeCamera::SaveMarkerCameraInfo(sReplayMarkerInfo& markerInfo, const camFrame& frame) const
{
	camReplayBaseCamera::SaveMarkerCameraInfo(markerInfo, frame);

	if (m_AttachEntity)
	{
		float (&relativePosition)[ 3 ] = markerInfo.GetFreeCamPositionRelative();

		relativePosition[0]	= m_DesiredPosition.x;
		relativePosition[1]	= m_DesiredPosition.y;
		relativePosition[2]	= m_DesiredPosition.z;

		markerInfo.SetFreeCamRelativeHeading( m_AttachEntityRelativeHeading );
		markerInfo.SetFreeCamRelativePitch( m_AttachEntityRelativePitch );
		markerInfo.SetFreeCamRelativeRoll( m_AttachEntityRelativeRoll );
		camInterface::GetReplayDirector().SetLastValidCamRelativeSettings(m_DesiredPosition, m_AttachEntityRelativeHeading, m_AttachEntityRelativePitch, m_AttachEntityRelativeRoll); 

		float (&attachEntityPosition)[ 3 ] = markerInfo.GetAttachEntityPosition();

		attachEntityPosition[0]	= m_AttachEntityMatrix.d.x;
		attachEntityPosition[1]	= m_AttachEntityMatrix.d.y;
		attachEntityPosition[2]	= m_AttachEntityMatrix.d.z;
		
		Quaternion quat;
		m_AttachEntityMatrix.ToQuaternion(quat);
		markerInfo.SetAttachEntityMatrixValid( true );

		markerInfo.SetAttachEntityQuaternion( QuantizeQuaternionS3_20(quat) );

		if (m_LookAtEntity)
		{
			markerInfo.SetHorizontalLookAtOffset(m_LookAtOffset.x);
			markerInfo.SetVerticalLookAtOffset(m_LookAtOffset.y);
			markerInfo.SetLookAtRoll(m_LookAtRoll);
		}
	} 
	else if(camInterface::GetReplayDirector().IsLastRelativeSettingValid())
	{
		//This applies the cached settings if the marker has not changed but the camera has, i.e. the user is selecting new cameras. 
		Vector3 pos; 
		float AttachEntityRelativeHeading; 
		float AttachEntityRelativePitch; 
		float AttachEntityRelativeRoll; 

		camInterface::GetReplayDirector().GetLastValidCamRelativeSettings(pos, AttachEntityRelativeHeading, AttachEntityRelativePitch, AttachEntityRelativeRoll); 

		float (&relativePosition)[ 3 ] = markerInfo.GetFreeCamPositionRelative();

		relativePosition[0]	= pos.x;
		relativePosition[1]	= pos.y;
		relativePosition[2]	= pos.z;

		markerInfo.SetFreeCamRelativeHeading( AttachEntityRelativeHeading );
		markerInfo.SetFreeCamRelativePitch( AttachEntityRelativePitch );
		markerInfo.SetFreeCamRelativeRoll( AttachEntityRelativeRoll );

		if (m_LookAtEntity)
		{
			markerInfo.SetHorizontalLookAtOffset(m_LookAtOffset.x);
			markerInfo.SetVerticalLookAtOffset(m_LookAtOffset.y);
			markerInfo.SetLookAtRoll(m_LookAtRoll);
		}
	}

	s32 editingMarkerIndex = CReplayMarkerContext::GetEditingMarkerIndex();
	if(editingMarkerIndex == m_MarkerIndex && m_LastEditingMarkerIndex != editingMarkerIndex)
	{
		m_ResetFrame = frame;
	}
	m_LastEditingMarkerIndex = editingMarkerIndex;
	camInterface::GetReplayDirector().SetLastValidFreeCamFrame(frame); 
}

void camReplayFreeCamera::LoadMarkerCameraInfo(sReplayMarkerInfo& markerInfo, camFrame& frame)
{
	camReplayBaseCamera::LoadMarkerCameraInfo(markerInfo, frame);

	Quaternion orientation;
	DeQuantizeQuaternionS3_20(orientation, markerInfo.GetAttachEntityQuaternion() );

	if (orientation.Mag2() < square(0.999f) || orientation.Mag2() > square(1.001f))
	{
		//bad data must have been stored to the marker info, we need to reset it
		markerInfo.SetAttachEntityMatrixValid(false);
	}

	if (markerInfo.IsAttachEntityMatrixValid())
	{
		m_AttachEntityMatrix.FromQuaternion(orientation);

		Vector3 position;
		float const (&positionRaw)[ 3 ] = markerInfo.GetAttachEntityPosition();
		m_AttachEntityMatrix.d.Set(positionRaw[0], positionRaw[1], positionRaw[2]);
	}

	m_LookAtOffset	= Vector2(markerInfo.GetHorizontalLookAtOffset(), markerInfo.GetVerticalLookAtOffset());
	m_LookAtRoll	= markerInfo.GetLookAtRoll();

	LoadEntities(markerInfo, frame);

	//have crossed a marker lets cache the frame
	if(!camInterface::GetReplayDirector().IsLastFreeCamFrameValid())
	{
		camInterface::GetReplayDirector().SetLastValidFreeCamFrame(frame); 
		camInterface::GetReplayDirector().SetLastValidCamRelativeSettings(m_DesiredPosition, m_AttachEntityRelativeHeading, m_AttachEntityRelativePitch, m_AttachEntityRelativeRoll); 
	}
}

void camReplayFreeCamera::ValidateEntities(sReplayMarkerInfo& markerInfo)
{
	camReplayBaseCamera::ValidateEntities(markerInfo);

	const CEntity* attachEntity = CReplayMgr::GetEntity(markerInfo.GetAttachTargetId());
	if (attachEntity)
	{
		Vector3 entityPosition = VEC3V_TO_VECTOR3(attachEntity->GetTransform().GetPosition());
		const bool isPositionOutsidePlayerLimits = IsPositionOutsidePlayerLimits(entityPosition);
		if (isPositionOutsidePlayerLimits)
		{
			markerInfo.SetAttachTargetId( DEFAULT_ATTACH_TARGET_ID );
			markerInfo.SetAttachTargetIndex( DEFAULT_ATTACH_TARGET_INDEX );
		}
	}

	const CEntity* lookAtEntity = CReplayMgr::GetEntity(markerInfo.GetLookAtTargetId());
	if(lookAtEntity)
	{
		Vector3 entityPosition = VEC3V_TO_VECTOR3(lookAtEntity->GetTransform().GetPosition());
		const bool isPositionOutsidePlayerLimits = IsPositionOutsidePlayerLimits(entityPosition);
		if (isPositionOutsidePlayerLimits)
		{
			markerInfo.SetLookAtTargetId( DEFAULT_LOOKAT_TARGET_ID );
			markerInfo.SetLookAtTargetIndex( DEFAULT_LOOKAT_TARGET_INDEX );
		}
	}

	const CEntity* focusEntity = CReplayMgr::GetEntity(markerInfo.GetFocusTargetId());
	if(focusEntity)
	{
		Vector3 entityPosition = VEC3V_TO_VECTOR3(focusEntity->GetTransform().GetPosition());
		const bool isPositionOutsidePlayerLimits = IsPositionOutsidePlayerLimits(entityPosition);
		if (isPositionOutsidePlayerLimits)
		{
			const s32 focusTargetId = camInterface::GetReplayDirector().GetTargetIdFromIndex(DEFAULT_MARKER_DOF_TARGETINDEX);
			if (CReplayMgr::GetEntity(focusTargetId))
			{
				markerInfo.SetFocusTargetId( focusTargetId );
			}
			else
			{
				markerInfo.SetFocusTargetId( DEFAULT_DOF_FOCUS_ID );
			}

			markerInfo.SetFocusTargetIndex( DEFAULT_MARKER_DOF_TARGETINDEX );
		}
	}
}

void camReplayFreeCamera::LoadEntities(const sReplayMarkerInfo& markerInfo, camFrame& frame)
{
	const CEntity* lastAttachEntity	= m_AttachEntity;

	m_AttachEntity					= CReplayMgr::GetEntity(markerInfo.GetAttachTargetId());
	
	const CPed* attachPed			= m_AttachEntity && m_AttachEntity->GetIsTypePed() ? static_cast<const CPed*>(m_AttachEntity.Get()) : NULL;
	const CVehicle* pedVehicle		= NULL;
	if (attachPed)
	{
		pedVehicle					= attachPed->GetVehiclePedInside();
	}

	bool hasForcedSmoothingThisUpdate = false;
	if (pedVehicle)
	{
		if (!m_AttachEntityIsInVehicle && lastAttachEntity) //checking lastAttachEntity ensures we're not the first frame
		{
			//getting into a vehicle
			camInterface::GetReplayDirector().ForceSmoothing(m_Metadata.m_InVehicleForceSmoothingDurationMs, 0, m_Metadata.m_InVehicleForceSmoothingBlendOutDurationMs);
			hasForcedSmoothingThisUpdate = true;
		}

		m_AttachEntity				= pedVehicle;
		m_AttachEntityIsInVehicle	= true;
	}
	else if (m_AttachEntityIsInVehicle && lastAttachEntity && !m_AttachEntity && markerInfo.GetAttachTargetId() != DEFAULT_ATTACH_TARGET_ID)
	{
		//if we lose the attach entity (but didn't intend to), but the ped was in a vehicle, keep tracking the vehicle
		m_AttachEntity = lastAttachEntity;
	}
	else
	{
		if (m_AttachEntityIsInVehicle)
		{
			//getting out of a vehicle
			camInterface::GetReplayDirector().ForceSmoothing(m_Metadata.m_OutVehicleForceSmoothingDurationMs, 0, m_Metadata.m_OutVehicleForceSmoothingBlendOutDurationMs);
			hasForcedSmoothingThisUpdate = true;
		}

		m_AttachEntityIsInVehicle	= false;
	}

	UpdateAttachEntityMatrix();

	if (m_AttachEntity)
	{
		if (m_AttachEntity != lastAttachEntity || m_ResetForNewAttachEntityThisUpdate)
		{
			if (m_IsANewMarker || m_ResetForNewAttachEntityThisUpdate)
			{
				//either start of first update or during load marker info
				Vector3 cameraPosition = frame.GetPosition();
				SetToSafePosition(cameraPosition);
				frame.SetPosition(cameraPosition);

				m_AttachEntityMatrix.UnTransform(cameraPosition, m_DesiredPosition);

				Vector3 localFront;
				Vector3 localUp;
				m_AttachEntityMatrix.UnTransform3x3(frame.GetFront(), localFront);
				m_AttachEntityMatrix.UnTransform3x3(frame.GetUp(), localUp);

				Matrix34 localMatrix;
				camFrame::ComputeWorldMatrixFromFrontAndUp(localFront, localUp, localMatrix);

				camFrame::ComputeHeadingPitchAndRollFromMatrixUsingEulers(localMatrix, m_AttachEntityRelativeHeading, m_AttachEntityRelativePitch, m_AttachEntityRelativeRoll);
			}
			else
			{
				float const (&positionRelativeRaw)[ 3 ] = markerInfo.GetFreeCamPositionRelative();
				const Vector3 positionRelative(positionRelativeRaw[0], positionRelativeRaw[1], positionRelativeRaw[2]); 

				if (IsNanInMemory(&positionRelative.x) || IsNanInMemory(&positionRelative.y) || IsNanInMemory(&positionRelative.z))
				{
					//bad data must have been stored to the marker info, we need to fallback to the default attach position
					m_DesiredPosition				= m_Metadata.m_DefaultAttachPositionOffset;

					//scale the relative desired position if the attach entity is inside a vehicle
					const CEntity* attachEntityToUse = m_AttachEntity;
					if (m_AttachEntityIsInVehicle)
					{
						attachEntityToUse			= CReplayMgr::GetEntity(markerInfo.GetAttachTargetId());
					}
					float boundRadius				= attachEntityToUse->GetBaseModelInfo() ? attachEntityToUse->GetBaseModelInfo()->GetBoundingSphereRadius() : attachEntityToUse->GetBoundRadius();
					if (boundRadius >= SMALL_FLOAT)
					{
						const Vector3 scaledOffset	= m_Metadata.m_DefaultAttachPositionOffset / boundRadius;
						const CPed* attachPed		= attachEntityToUse->GetIsTypePed() ? static_cast<const CPed*>(attachEntityToUse) : NULL;
						const CVehicle* pedVehicle	= NULL;
						if (attachPed)
						{
							pedVehicle				= attachPed->GetVehiclePedInside();
						}
						if (pedVehicle)
						{
							boundRadius				= pedVehicle->GetBaseModelInfo() ? pedVehicle->GetBaseModelInfo()->GetBoundingSphereRadius() : pedVehicle->GetBoundRadius();
							m_DesiredPosition		= scaledOffset * boundRadius;
						}
					}

					//find safe new position
					Vector3 worldPosition = VEC3_ZERO;
					m_AttachEntityMatrix.Transform(m_DesiredPosition, worldPosition);
					SetToSafePosition(worldPosition);
					m_AttachEntityMatrix.UnTransform(worldPosition, m_DesiredPosition);

					m_AttachEntityRelativeHeading	= 0.0f;
					m_AttachEntityRelativePitch		= 0.0f;
					m_AttachEntityRelativeRoll		= 0.0f;
				}
				else
				{
					m_DesiredPosition.Set(positionRelative);

					m_AttachEntityRelativeHeading	= markerInfo.GetFreeCamRelativeHeading();
					m_AttachEntityRelativePitch		= markerInfo.GetFreeCamRelativePitch();
					m_AttachEntityRelativeRoll		= markerInfo.GetFreeCamRelativeRoll();
				}
			}

			if (!hasForcedSmoothingThisUpdate)
			{
				frame.GetFlags().SetFlag(camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation);
				camInterface::GetReplayDirector().DisableForceSmoothing();
			}
		}
	}
	else if (lastAttachEntity  || (markerInfo.GetAttachTargetId() != DEFAULT_ATTACH_TARGET_ID && !m_AttachEntity))
	{
		m_DesiredPosition = frame.GetPosition();
		if (!hasForcedSmoothingThisUpdate)
		{
			camInterface::GetReplayDirector().DisableForceSmoothing();
		}
	}

	const CEntity* lastLookAtEntity	= m_LookAtEntity;
	m_LookAtEntity					= CReplayMgr::GetEntity(markerInfo.GetLookAtTargetId());

	if (m_LookAtEntity != lastLookAtEntity)
	{
		frame.GetFlags().SetFlag(camFrame::Flag_HasCutOrientation);
	}

	m_AttachParent = m_AttachEntity; //for external use
	m_LookAtTarget = m_LookAtEntity; //for external use
}

void camReplayFreeCamera::PostUpdate()
{
	camReplayBaseCamera::PostUpdate();

	sReplayMarkerInfo* markerInfo = CReplayMarkerContext::GetMarkerStorage()->GetMarker(m_MarkerIndex);

	if (!markerInfo)
	{
        m_LastValidPhase = 0.0f;
		return;
	}

	if (m_ResetForNewAttachEntityThisUpdate)
	{
		//save again
		SaveMarkerCameraInfo(*markerInfo, m_Frame);
	}

	if (camInterface::GetReplayDirector().IsReplayPlaybackSetupFinished())
	{
		m_ResetForNewAttachEntityThisUpdate = false;
	}

	const bool isCurrentlyEditingThisMarker	= (CReplayMarkerContext::GetEditingMarkerIndex() == m_MarkerIndex);
	if (isCurrentlyEditingThisMarker)
	{
        m_LastValidPhase = 0.0f;
		return; //no blending while editing
	}

	if (!IsBlendingFromCamera())
	{
        m_LastValidPhase = 0.0f;
		return;
	}

	const float currentTime		= CReplayMgr::GetCurrentTimeRelativeMsFloat();
	const float unHaltedPhase	= GetCurrentPhaseForBlend(currentTime, false);

	if (unHaltedPhase > m_MaxPhaseToHaltBlendMovement)
	{
		if (!m_BlendHalted)
		{
			m_BlendHalted = true;
			// Flag the warning state so the UI can display some helpful text to the player
			CReplayMarkerContext::SetWarning( CReplayMarkerContext::EDIT_WARNING_BLEND_COLLISION );
		}
	}
	else
	{
		m_BlendHalted = false;
    }

#if __BANK
    TUNE_GROUP_BOOL(REPLAY_FREE_CAMERA, EnableInterpolatorsTuning, false);
    if( EnableInterpolatorsTuning )
    {
        m_PhaseInterpolatorIn.UpdateParameters(m_Metadata.m_PhaseInterpolatorIn);
        m_PhaseInterpolatorOut.UpdateParameters(m_Metadata.m_PhaseInterpolatorOut);
        m_PhaseInterpolatorInOut.UpdateParameters(m_Metadata.m_PhaseInterpolatorInOut);
    }
#endif

    const camFrame& cameraToBlendFromFrame = m_CameraToBlendFrom->GetFrame();

	float phase = GetCurrentPhaseForBlend(currentTime);
	const Vector3 desiredNewPosition = Lerp(phase, cameraToBlendFromFrame.GetPosition(), m_PostEffectFrame.GetPosition());

    //check the water surface distance during camera blending, this will update the phase value if necessary
    UpdateBlendPhaseConsideringWater(desiredNewPosition, phase);

    //recompute the position interpolation after we check for the water.
    const Vector3 newPosition = Lerp(phase, cameraToBlendFromFrame.GetPosition(), m_PostEffectFrame.GetPosition());

    //NOTE: CReplayMarkerContext::GetMarkerStorage()->GetMarker(m_BlendSourceMarkerIndex) is already verified
    const sReplayMarkerInfo* sourceMarkerInfo = CReplayMarkerContext::GetMarkerStorage()->GetMarker(m_BlendSourceMarkerIndex);

    TUNE_GROUP_BOOL(REPLAY_FREE_CAMERA, bCheckBlendedPositionSafety, false);
    //don't do the test if we are editing the camera position.
    if( bCheckBlendedPositionSafety && (CReplayMarkerContext::GetEditingMarkerIndex() != m_BlendSourceMarkerIndex) && ! m_IsBlendingPositionInvalid )
    {
        //check blending collisions with normal geometries. If we hit something stop the camera.
        const CEntity* entityToTest = m_AttachEntity;
        if(entityToTest == NULL && sourceMarkerInfo->GetAttachTargetId() != DEFAULT_ATTACH_TARGET_ID)
        {
            //fetch the correct entity to test.
            entityToTest = CReplayMgr::GetEntity(sourceMarkerInfo->GetAttachTargetId());
        }

        //we want to check the safety of the new blended position.
        Vector3 positionToTest = newPosition;
        //in case we will invalidate the blending by balling back to the rendered camera.
        m_IsBlendingPositionInvalid = SetToSafePosition(positionToTest, true, true, entityToTest);
    }

	//if we're blending and we have a look at entity, only allow orientation to blend if the look at targets are different
	bool shouldBlendOrientation	= true;

	if (m_LookAtEntity)
	{
		shouldBlendOrientation = (sourceMarkerInfo->GetLookAtTargetId() != markerInfo->GetLookAtTargetId());
	}


    const float newFov = Lerp(phase, cameraToBlendFromFrame.GetFov(), m_PostEffectFrame.GetFov());

	Matrix34 newWorldMatrix;
	if (shouldBlendOrientation)
	{
		camFrame::SlerpOrientation(phase, cameraToBlendFromFrame.GetWorldMatrix(), m_PostEffectFrame.GetWorldMatrix(), newWorldMatrix);
	}
	else
	{
        const Vector3 lookAtPosition	= VEC3V_TO_VECTOR3(m_LookAtEntity->GetTransform().GetPosition());

        Vector2 lookAtOffset;
        lookAtOffset.x					= Lerp(phase, sourceMarkerInfo->GetHorizontalLookAtOffset(), m_LookAtOffset.x);
        lookAtOffset.y					= Lerp(phase, sourceMarkerInfo->GetVerticalLookAtOffset(), m_LookAtOffset.y);
        float newRoll = 0.0f;

        if( m_CameraToBlendFrom->GetIsClassId( camReplayFreeCamera::GetStaticClassId() ) )
        {
            newRoll = Lerp(phase, static_cast< const camReplayFreeCamera* >( m_CameraToBlendFrom.Get() )->m_LookAtRoll, m_LookAtRoll);
        }
        else
        {
            newRoll = Lerp(phase, cameraToBlendFromFrame.ComputeRoll(), m_PostEffectFrame.ComputeRoll());
        }

        ComputeLookAtFrameWithOffset( lookAtPosition, newPosition, lookAtOffset, newFov, newRoll, newWorldMatrix );
	}

	const float newMotionBlur = Lerp(phase, cameraToBlendFromFrame.GetMotionBlurStrength(), m_PostEffectFrame.GetMotionBlurStrength());
	m_PostEffectFrame.SetPosition(newPosition);
	m_PostEffectFrame.SetWorldMatrix(newWorldMatrix);
	m_PostEffectFrame.SetFov(newFov);
	m_PostEffectFrame.SetMotionBlurStrength(newMotionBlur);

	m_PostEffectFrame.InterpolateDofSettings(phase, cameraToBlendFromFrame, m_PostEffectFrame);

}

float camReplayFreeCamera::GetCurrentPhaseForBlend(float currentTime, bool shouldClampToMaxPhase /* = true*/) const
{
	const sReplayMarkerInfo* markerInfo = CReplayMarkerContext::GetMarkerStorage()->GetMarker(m_MarkerIndex);

	if (!markerInfo || !IsBlendingFromCamera())
	{
		return 1.0f;
	}

	const float endTime = markerInfo->GetNonDilatedTimeMs();

	//NOTE: CReplayMarkerContext::GetMarkerStorage()->GetMarker(m_BlendSourceMarkerIndex) is already verified
	const sReplayMarkerInfo* sourceMarkerInfo	= CReplayMarkerContext::GetMarkerStorage()->GetMarker(m_BlendSourceMarkerIndex);
	const float startTime						= sourceMarkerInfo ? sourceMarkerInfo->GetNonDilatedTimeMs() : endTime;

	if (IsClose(startTime, endTime, SMALL_FLOAT) || !sourceMarkerInfo)
	{
		return 1.0f;
	}

	float phase = 0.0f;
	if (((currentTime - startTime) >= SMALL_FLOAT) && ((endTime - startTime) >= SMALL_FLOAT))
	{
		phase = (float)(currentTime - startTime) / (float)(endTime - startTime);
	}

	phase = Clamp(phase, 0.0f, shouldClampToMaxPhase ? m_MaxPhaseToHaltBlendMovement : 1.0f);

	const s32 previousMarkerIndex				= camReplayDirector::GetPreviousMarkerIndex(m_BlendSourceMarkerIndex);
	const sReplayMarkerInfo* previousMarkerInfo	= CReplayMarkerContext::GetMarkerStorage()->TryGetMarker(previousMarkerIndex);

	const s32 nextMarkerIndex					= camReplayDirector::GetNextMarkerIndex(m_BlendSourceMarkerIndex);
	const sReplayMarkerInfo* nextMarkerInfo		= CReplayMarkerContext::GetMarkerStorage()->TryGetMarker(nextMarkerIndex);

	const bool previousHasEaseBlend		= previousMarkerInfo && previousMarkerInfo->HasBlend() && previousMarkerInfo->HasBlendEase();
	const bool nextHasLinearBlend		= nextMarkerInfo && nextMarkerInfo->HasBlend() && !nextMarkerInfo->HasBlendEase();
	const bool nextHasEaseNoBlend		= nextMarkerInfo && !nextMarkerInfo->HasBlend() && nextMarkerInfo->HasBlendEase();

	if (sourceMarkerInfo->HasBlendEase())
	{
		if (previousHasEaseBlend)
		{
			if (nextHasLinearBlend || nextHasEaseNoBlend)
			{
				phase = m_PhaseInterpolatorOut.Interpolate( phase );
			}
		}
		else
		{
			if (nextHasEaseNoBlend)
			{
				phase = m_PhaseInterpolatorInOut.Interpolate( phase );
			}
			else
            {
                phase = m_PhaseInterpolatorIn.Interpolate( phase );
			}
		}
	}
	else 
	{
		if (nextHasEaseNoBlend)
		{
			phase = m_PhaseInterpolatorOut.Interpolate( phase );
		}
	}

	PF_SET(blendPhase, phase);

	return phase;
}

void camReplayFreeCamera::BlendFromCamera(const camBaseCamera& sourceCamera, const s32 sourceMarkerIndex)
{
	const sReplayMarkerInfo* sourceMarkerInfo = CReplayMarkerContext::GetMarkerStorage() ? CReplayMarkerContext::GetMarkerStorage()->TryGetMarker(sourceMarkerIndex) : NULL;
	if (!sourceMarkerInfo)
	{
		return;
	}

	m_CameraToBlendFrom				= &sourceCamera;
	m_BlendSourceMarkerIndex		= sourceMarkerIndex;
	m_BlendHalted					= false;

	m_ShouldUpdateMaxPhaseToHaltBlend = true;
}

void camReplayFreeCamera::UpdateMaxPhaseToHaltBlend()
{
	if (!m_ShouldUpdateMaxPhaseToHaltBlend)
	{
		return;
	}

	m_MaxPhaseToHaltBlendMovement = 1.0f;

	if (!camReplayDirector::IsReplayPlaybackSetupFinished())
	{
		return;
	}

	if (!IsBlendingFromCamera())
	{
		return;
	}

	const sReplayMarkerInfo* markerInfo			= CReplayMarkerContext::GetMarkerStorage() ? CReplayMarkerContext::GetMarkerStorage()->TryGetMarker(m_MarkerIndex) : NULL;
	//NOTE: CReplayMarkerContext::GetMarkerStorage()->GetMarker(m_BlendSourceMarkerIndex) is already verified
	const sReplayMarkerInfo* sourceMarkerInfo	= CReplayMarkerContext::GetMarkerStorage()->GetMarker(m_BlendSourceMarkerIndex);
	if (!markerInfo || !sourceMarkerInfo)
	{
		return;
	}

	//if either this marker or the marker we're blending from were attached to anything, we can't stop the camera going through geometry
	if (sourceMarkerInfo->GetAttachTargetId() != DEFAULT_ATTACH_TARGET_ID || markerInfo->GetAttachTargetId() != DEFAULT_ATTACH_TARGET_ID)
	{
		return;
	}

	//otherwise, we find the point (if any) along this line that would intersect static collision
	WorldProbe::CShapeTestCapsuleDesc capsuleTest;
	WorldProbe::CShapeTestFixedResults<> shapeTestResults;
	capsuleTest.SetResultsStructure(&shapeTestResults);
	capsuleTest.SetIsDirected(true);
	//capsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	capsuleTest.SetContext(WorldProbe::LOS_Camera);

	const u32 staticIncludeFlags	= static_cast<u32>(ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_GLASS_TYPE | ArchetypeFlags::GTA_DEEP_SURFACE_TYPE);
	capsuleTest.SetIncludeFlags(staticIncludeFlags);

	const Vector3& startPosition	= m_CameraToBlendFrom->GetFrame().GetPosition();
	const Vector3& endPosition		= m_DesiredPosition; //we can't have an attach entity, so this must be in world space

	const float nonDirectedCapsuleRadius	= m_Metadata.m_CapsuleRadius - camCollision::GetMinRadiusReductionBetweenInterativeShapetests() * 2.0f;

	capsuleTest.SetCapsule(startPosition, endPosition, nonDirectedCapsuleRadius);

	const bool hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);
	const float hitPhase = ComputeFilteredHitTValue(shapeTestResults, false);
	if (!hasHit || (1.0f - hitPhase) < SMALL_FLOAT)
	{
		return;
	}

	m_MaxPhaseToHaltBlendMovement = Clamp(hitPhase, 0.0f, 1.0f);
	m_ShouldUpdateMaxPhaseToHaltBlend = false;
}

bool camReplayFreeCamera::IsBlendingFromCamera() const
{
	if (m_CameraToBlendFrom)
	{
		//NOTE: If we have m_CameraToBlendFrom, CReplayMarkerContext::GetMarkerStorage()->GetMarker(m_BlendSourceMarkerIndex) has already been verified
		const sReplayMarkerInfo* sourceMarkerInfo = CReplayMarkerContext::GetMarkerStorage()->GetMarker(m_BlendSourceMarkerIndex);
		const u8 blendType = sourceMarkerInfo->GetBlendType();

		return (blendType != MARKER_CAMERA_BLEND_NONE);
	}

	return false;
}

void camReplayFreeCamera::RenderCameraConstraintMarker(bool constrainedThisFrame )
{
#define REPLAY_FREECAM_CONSTRAINT_WARN_TIME_MS	(500)

	// Only show constraint when editing this camera
	const bool isCurrentlyEditingThisMarker	= (CReplayMarkerContext::GetEditingMarkerIndex() == m_MarkerIndex);
	if( isCurrentlyEditingThisMarker )
	{
		const CPed* playerPed = CReplayMgr::GetMainPlayerPtr();
		if (!playerPed)
		{
			return;
		}

		if( constrainedThisFrame )
		{
			m_LastConstrainedTime = fwTimer::GetTimeInMilliseconds_NonPausedNonScaledClipped();
		}

		const Color32 resultCol = Color_red;
		LIGHT_EFFECTS_ONLY(static ioConstantLightEffect RED_LIGHT_EFFECT(resultCol));

		u32 delta = fwTimer::GetTimeInMilliseconds_NonPausedNonScaledClipped() - m_LastConstrainedTime;
		if( delta < REPLAY_FREECAM_CONSTRAINT_WARN_TIME_MS )
		{
			static const float overExpansionSize = 0.5f;

			// We have something to draw
			float scaler = (float)delta/REPLAY_FREECAM_CONSTRAINT_WARN_TIME_MS;

			float maxDistance = camInterface::GetReplayDirector().GetMaxDistanceAllowedFromPlayer(true) + overExpansionSize;	// expland so we don't clip through the camera
			GameGlows::AddFullScreenGameGlow(playerPed->GetTransform().GetPosition(), maxDistance, resultCol, 0.5f * (1.0f-scaler), true, false, false);

#if LIGHT_EFFECTS_SUPPORT
			RED_LIGHT_EFFECT = ioConstantLightEffect(Lerp(scaler, resultCol, Color_white));
			// NOTE: Use script light effect so we do not remove the code one.
			CControlMgr::GetPlayerMappingControl().SetLightEffect(&RED_LIGHT_EFFECT, CControl::SCRIPT_LIGHT_EFFECT);
#endif // LIGHT_EFFECTS_SUPPORT

			// Flag the warning state so the UI can display some helpful text to the player
			CReplayMarkerContext::SetWarning( CReplayMarkerContext::EDIT_WARNING_MAX_RANGE );
		}
#if LIGHT_EFFECTS_SUPPORT
		else
		{
			// NOTE: Use script light effect so we do not remove the code one.
			CControlMgr::GetPlayerMappingControl().ClearLightEffect(&RED_LIGHT_EFFECT, CControl::SCRIPT_LIGHT_EFFECT);
		}
#endif // LIGHT_EFFECTS_SUPPORT
	}
}

void camReplayFreeCamera::GetMinMaxFov(float& minFov, float& maxFov) const
{
	minFov = m_Metadata.m_MinFov;
	maxFov = m_Metadata.m_MaxFov;
}

void camReplayFreeCamera::GetDefaultFov( float& defaultFov ) const
{
	defaultFov = m_Metadata.m_DefaultFov;
}

void camReplayFreeCamera::UpdateBlendPhaseConsideringWater(const Vector3& positionToApply, float& phase)
{
    //Vertically constrain against simulated water.
    //NOTE: River bounds are covered by the collision shape tests below.
    const float maxDistanceToTestForRiver = m_Metadata.m_CollisionSettings.m_MinHeightAboveOrBelowWater * 4.0f;
    float waterHeight			= 0.0f;
    const bool hasFoundWater	= camCollision::ComputeWaterHeightAtPosition(positionToApply, LARGE_FLOAT, waterHeight, maxDistanceToTestForRiver, 0.5f, false);

    if(!hasFoundWater)
    {
        m_SmoothedWaterHeightPostUpdate = g_InvalidWaterHeight;
    }
    else
    {
        if(IsClose(m_SmoothedWaterHeightPostUpdate, g_InvalidWaterHeight))
        {
            m_SmoothedWaterHeightPostUpdate = waterHeight;
        }
        else
        {
            //Cheaply make the smoothing frame-rate independent.
            float smoothRate		= m_Metadata.m_CollisionSettings.m_WaterHeightSmoothRate * 30.0f * fwTimer::GetNonPausableCamTimeStep();
            smoothRate				= Min(smoothRate, 1.0f);
            m_SmoothedWaterHeightPostUpdate	= Lerp(smoothRate, m_SmoothedWaterHeightPostUpdate, waterHeight);
        }

        const float heightDelta = positionToApply.z - m_SmoothedWaterHeightPostUpdate;
        const float minHeight = m_Metadata.m_CollisionSettings.m_MinHeightAboveOrBelowWater;	

        const bool ignorePhase = Abs(heightDelta) < minHeight;
        phase            = ignorePhase ? m_LastValidPhase : phase;
        m_LastValidPhase = phase;
    }
}

#if __BANK

void camReplayFreeCamera::DebugRender()
{
	camBaseCamera::DebugRender();

	if (IsBlendingFromCamera())
	{
		m_Frame.DebugRender(false); //render destination camera
	}
}

void camReplayFreeCamera::DebugGetBlendType(char* phaseText) const
{
	phaseText[0] = '\0';

	const bool isCurrentlyEditingThisMarker	= (CReplayMarkerContext::GetEditingMarkerIndex() == m_MarkerIndex);
	if (isCurrentlyEditingThisMarker)
	{
		return;
	}

	if (!IsBlendingFromCamera())
	{
		return;
	}

	const sReplayMarkerInfo* sourceMarkerInfo	= CReplayMarkerContext::GetMarkerStorage()->GetMarker(m_BlendSourceMarkerIndex);

	if (!sourceMarkerInfo)
	{
		return;
	}

	const s32 previousMarkerIndex				= camReplayDirector::GetPreviousMarkerIndex(m_BlendSourceMarkerIndex);
	const sReplayMarkerInfo* previousMarkerInfo	= CReplayMarkerContext::GetMarkerStorage()->TryGetMarker(previousMarkerIndex);

	const s32 nextMarkerIndex					= camReplayDirector::GetNextMarkerIndex(m_BlendSourceMarkerIndex);
	const sReplayMarkerInfo* nextMarkerInfo		= CReplayMarkerContext::GetMarkerStorage()->TryGetMarker(nextMarkerIndex);

	const bool previousHasEaseBlend		= previousMarkerInfo && previousMarkerInfo->HasBlend() && previousMarkerInfo->HasBlendEase();
	const bool nextHasLinearBlend		= nextMarkerInfo && nextMarkerInfo->HasBlend() && !nextMarkerInfo->HasBlendEase();
	const bool nextHasEaseNoBlend		= nextMarkerInfo && !nextMarkerInfo->HasBlend() && nextMarkerInfo->HasBlendEase();

	sprintf(phaseText, "L");

	if (sourceMarkerInfo->HasBlendEase())
	{
		if (previousHasEaseBlend)
		{
			if (nextHasLinearBlend || nextHasEaseNoBlend)
			{
				sprintf(phaseText, "O");
			}
		}
		else
		{
			if (nextHasEaseNoBlend)
			{
				sprintf(phaseText, "IO");
			}
			else
			{
				sprintf(phaseText, "I");
			}
		}
	}
	else 
	{
		if (nextHasEaseNoBlend)
		{
			sprintf(phaseText, "O");
		}
	}
}

#endif // __BANK
#endif // GTA_REPLAY
