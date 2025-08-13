//
// camera/replay/ReplayPresetCamera.cpp
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#include "camera/replay/ReplayPresetCamera.h"

#if GTA_REPLAY

#include "control/replay/IReplayMarkerStorage.h"
#include "control/replay/replay.h"
#include "control/replay/ReplayMarkerContext.h"
#include "control/replay/ReplayMarkerInfo.h"

#include "camera/CamInterface.h"
#include "camera/helpers/Collision.h"
#include "camera/helpers/ControlHelper.h"
#include "camera/replay/ReplayDirector.h"
#include "camera/system/CameraMetadata.h"
#include "camera/system/CameraFactory.h"

#include "Frontend/VideoEditor/VideoEditorInterface.h"
#include "Peds/ped.h"
#include "system/controlMgr.h"
#include "vehicles/Trailer.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camReplayPresetCamera,0xCA60B2BF)

extern const float g_InvalidWaterHeight;

camReplayPresetCamera::camReplayPresetCamera(const camBaseObjectMetadata& metadata)
: camReplayBaseCamera(metadata)
, m_Metadata(static_cast<const camReplayPresetCameraMetadata&>(metadata))
, m_TrackingEntityMatrix(MATRIX34_IDENTITY)
, m_CollisionOrigin(g_InvalidPosition)
, m_CollisionRootPosition(g_InvalidPosition)
, m_DesiredDistanceScale(0.5f)
, m_DefaultDistanceScalar(0.0f)
, m_CachedDistanceInputSign(0.0f)
, m_CachedRollRotationInputSign(0.0f)
, m_DistanceSpeed(0.0f)
, m_RollSpeed(0.0f)
, m_ZoomSpeed(0.0f)
, m_SmoothedWaterHeight(g_InvalidWaterHeight)
, m_RelativeRoll(0.0f)
, m_TrackingEntityIsInVehicle(false)
{
}

bool camReplayPresetCamera::Init(const s32 markerIndex, const bool shouldForceDefaultFrame)
{	
	m_ForcingDefaultFrame	= shouldForceDefaultFrame;
	m_MarkerIndex			= markerIndex;

	//it is not safe to continue if playback setup hasn't finished or if we do not have a tracking entity
	if (( !camInterface::GetReplayDirector().IsReplayPlaybackSetupFinished() && ! CReplayMgr::IsFineScrubbing() ) || !UpdateTrackingEntity())
	{
		return false;
	}

	IReplayMarkerStorage* markerStorage	= CReplayMarkerContext::GetMarkerStorage();
	sReplayMarkerInfo* markerInfo		= markerStorage ? markerStorage->TryGetMarker(m_MarkerIndex) : NULL;

	//we need the attach entity matrix from the marker before UpdateTrackingEntityMatrix is called
	if (!shouldForceDefaultFrame)
	{
		LoadMarkerCameraInfo(*markerInfo, m_Frame);
	}

	//we need to call this before base Init because that calls GetDefaultFrame()
	//which gets the default attach and look at positions, which rely on the tracking entity matrix
	UpdateTrackingEntityMatrix();

	m_AttachParent = m_TrackingEntity; //for external use
	
	m_DesiredDistanceScale = 0.5f;

	const bool succeeded = camReplayBaseCamera::Init(markerIndex, shouldForceDefaultFrame);
	
	if (!succeeded)
	{
		return false;
	}

	m_DistanceSpeed		= 0.0f;
	m_RollSpeed			= 0.0f;
	m_ZoomSpeed			= 0.0f;

	Vector3 lookAtPosition, defaultAttachPosition;
	ComputeLookAtAndDefaultAttachPositions(lookAtPosition, defaultAttachPosition);

	const CPed* playerPed				= CReplayMgr::GetMainPlayerPtr();

	const float defaultDistance			= lookAtPosition.Dist(defaultAttachPosition);
	const float onFootBoundRadius		= GetBoundRadius(playerPed);

	if (onFootBoundRadius >= SMALL_FLOAT)
	{
		m_DefaultDistanceScalar			= defaultDistance / onFootBoundRadius;
	}

	//remove any roll we may have inherited from a previous camera/marker if roll control is disabled
	if (!m_Metadata.m_ShouldAllowRollControl)
	{
		const Matrix34& worldMatrix = m_Frame.GetWorldMatrix();
		float heading, pitch, roll;
		camFrame::ComputeHeadingPitchAndRollFromMatrixUsingEulers(worldMatrix, heading, pitch, roll);
		roll = 0.0f;
		m_Frame.SetWorldMatrixFromHeadingPitchAndRoll(heading, pitch, roll);
	}

	//because we've made changes to the frame, we want to set up our relative orientations and save out the marker info again if we forced the default frame
	if (shouldForceDefaultFrame)
	{
		GetDefaultFrame(m_Frame);

		const float distance			= m_Frame.GetPosition().Dist(lookAtPosition);

		const float boundRadius			= GetBoundRadius(m_TrackingEntity);
		const float minCameraDistance	= m_DefaultDistanceScalar * m_Metadata.m_MinDistanceScalar * boundRadius;
		const float maxCameraDistance	= m_DefaultDistanceScalar * m_Metadata.m_MaxDistanceScalar * boundRadius;

		m_DesiredDistanceScale			= RampValueSafe(distance, minCameraDistance, maxCameraDistance, 0.0f, 1.0f);

		if (markerInfo)
		{
			SaveMarkerCameraInfo(*markerInfo, m_Frame);
		}
	}

	m_Frame.SetDofFullScreenBlurBlendLevel(0.0f);

	return true;
}

void camReplayPresetCamera::GetDefaultFrame(camFrame& frame) const
{
	camFrame newFrame;

	Vector3 lookAtPosition, defaultAttachPosition;
	ComputeLookAtAndDefaultAttachPositions(lookAtPosition, defaultAttachPosition);

	Matrix34 worldMatrix;
	ComputeLookAtOrientation(worldMatrix, m_TrackingEntityMatrix, lookAtPosition, defaultAttachPosition, 0.0f);
	newFrame.SetWorldMatrix(worldMatrix);

	const float boundRadius				= GetBoundRadius(m_TrackingEntity);
	const float minCameraDistance		= m_DefaultDistanceScalar * m_Metadata.m_MinDistanceScalar * boundRadius;
	const float maxCameraDistance		= m_DefaultDistanceScalar * m_Metadata.m_MaxDistanceScalar * boundRadius;
	const float baseCameraDistance		= RampValueSafe(m_DesiredDistanceScale, 0.0f, 1.0f, minCameraDistance, maxCameraDistance);	
	Vector3 cameraFront					= lookAtPosition - defaultAttachPosition;
	cameraFront.Normalize();
	Vector3 positionToApply				= lookAtPosition - (cameraFront * baseCameraDistance);
	newFrame.SetPosition(positionToApply);

	newFrame.SetFov(m_Metadata.m_DefaultFov);

	frame.CloneFrom(newFrame);

    //make sure we don't retain any motionblur data from the recorded gameplay camera
    frame.SetMotionBlurStrength(0.0f);
}

bool camReplayPresetCamera::Update()
{
	const bool hasSucceeded = camReplayBaseCamera::Update();
	if (!hasSucceeded)
	{
		return false;
	}

	if(!UpdateTrackingEntity())
	{
		return false;
	}

	m_AttachParent = m_TrackingEntity; //for external use

	UpdateTrackingEntityMatrix();

    if( CReplayMgr::IsJumping() || CReplayMgr::WasJumping() )
    {
        //disable the smoothing if the replay cursor is jumping
        camInterface::GetReplayDirector().DisableForceSmoothing();
    }

	const bool isCurrentlyEditingThisMarker	= (CReplayMarkerContext::GetEditingMarkerIndex() == m_MarkerIndex);

	float desiredDistanceChange = 0.0f;
	float desiredRoll = 0.0f;

	const CControl& control	= CControlMgr::GetMainFrontendControl(false);

	const bool shouldResetCamera = GetShouldResetCamera(control);
	if (shouldResetCamera)
	{
		ResetCamera();
	}

	if (isCurrentlyEditingThisMarker)
	{
		float desiredZoom;
		UpdateControl(control, desiredDistanceChange, desiredRoll, desiredZoom);

		UpdateZoom(desiredZoom);
	}

	UpdateRotation(desiredRoll);

	Vector3 preCollisionPosition(VEC3_ZERO);
	UpdatePosition(desiredDistanceChange, preCollisionPosition);

	if (isCurrentlyEditingThisMarker)
	{
		Vector3 lookAtPosition, defaultAttachPosition;
		ComputeLookAtAndDefaultAttachPositions(lookAtPosition, defaultAttachPosition);

		const float distance			= m_Frame.GetPosition().Dist(lookAtPosition);

		const float boundRadius			= GetBoundRadius(m_TrackingEntity);
		const float minCameraDistance	= m_DefaultDistanceScalar * m_Metadata.m_MinDistanceScalar * boundRadius;
		const float maxCameraDistance	= m_DefaultDistanceScalar * m_Metadata.m_MaxDistanceScalar * boundRadius;

		m_DesiredDistanceScale			= RampValueSafe(distance, minCameraDistance, maxCameraDistance, 0.0f, 1.0f);
	}

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

void camReplayPresetCamera::UpdateTrackingEntityMatrix()
{
	//NOTE: m_TrackingEntity has already been validated

	//We want to force the default frame when we're dragging a marker
	const bool isShiftingThisMarker = CReplayMarkerContext::GetCurrentMarkerIndex() == m_MarkerIndex && CReplayMarkerContext::IsShiftingCurrentMarker();

	//if the ped is in ragdoll, use the root bone to track movement, and activate smoothing
	Matrix34 identityMatrix;
	identityMatrix.Identity();
	const bool trackingEntityInRagdoll = (!m_TrackingEntityMatrix.IsClose(identityMatrix) && m_TrackingEntity->GetIsTypePed() && static_cast<const CPed*>(m_TrackingEntity.Get())->GetReplayUsingRagdoll());

	IReplayMarkerStorage* markerStorage		= CReplayMarkerContext::GetMarkerStorage();
	sReplayMarkerInfo* markerInfo			= markerStorage ? markerStorage->TryGetMarker(m_MarkerIndex) : NULL;
	const bool isAttachEntityMatrixValid	= markerInfo && markerInfo->IsAttachEntityMatrixValid();
	if(!isAttachEntityMatrixValid || m_ForcingDefaultFrame || isShiftingThisMarker || m_Metadata.m_ShouldUseRigidAttachType)
	{
		m_TrackingEntityMatrix = MAT34V_TO_MATRIX34(m_TrackingEntity->GetTransform().GetMatrix());
	}
	else if (trackingEntityInRagdoll)
	{
		TUNE_GROUP_BOOL(REPLAY_PRESET_CAMERA, bUseSmoothingInRagdoll, true);
		TUNE_GROUP_BOOL(REPLAY_PRESET_CAMERA, bUseRootBonePositionForRagdoll, true);

		if (bUseSmoothingInRagdoll)
		{
			camInterface::GetReplayDirector().ForceSmoothing(m_Metadata.m_RagdollForceSmoothingDurationMs, 0, m_Metadata.m_RagdollForceSmoothingBlendOutDurationMs);
		}	

		if (bUseRootBonePositionForRagdoll)
		{
			const CPed* pPed = static_cast<const CPed*>(m_TrackingEntity.Get());
			Matrix34 rootMat;
			pPed->GetGlobalMtx(BONETAG_ROOT, rootMat);
			m_TrackingEntityMatrix.d = rootMat.d;
		}
		else
		{
			m_TrackingEntityMatrix.d = VEC3V_TO_VECTOR3(m_TrackingEntity->GetTransform().GetPosition());
		}
	}	
	else
	{
		m_TrackingEntityMatrix.d = VEC3V_TO_VECTOR3(m_TrackingEntity->GetTransform().GetPosition());
	}
}

void camReplayPresetCamera::ComputeLookAtAndDefaultAttachPositions(Vector3& lookAtPosition, Vector3& defaultAttachPosition) const
{
	lookAtPosition = defaultAttachPosition = VEC3_ZERO;
	
	m_TrackingEntityMatrix.Transform(m_Metadata.m_DefaultRelativeAttachPosition, defaultAttachPosition);
	m_TrackingEntityMatrix.Transform(m_Metadata.m_RelativeLookAtPosition, lookAtPosition);
}

float camReplayPresetCamera::GetBoundRadius(const CEntity* entity) const
{
	if (!entity)
	{
		return 0.0f;
	}
	
	// Get the radius of the bound containing the vehicle the ped is entering, including trailers, towed vehicles and attachments
	if(entity->GetIsTypeVehicle())
	{			
		const CVehicle* trackingVehicle = static_cast<const CVehicle*>(entity);
		if(trackingVehicle)
		{				
			spdAABB vehicleBoundingBox;
			trackingVehicle->GetAABB(vehicleBoundingBox);

			// Include all child attachments of the tracked vehicle
			const CPhysical* childAttachment = static_cast<const CPhysical*>(trackingVehicle->GetChildAttachment());
			while(childAttachment)
			{
				if(childAttachment->GetIsTypeObject() && static_cast<const CObject*>(childAttachment)->GetIsParachute())
				{
					//ignore parachutes on vehicles when computing the bounding box.
					childAttachment = static_cast<const CPhysical*>(childAttachment->GetSiblingAttachment());
					continue;
				}

				spdAABB childAttachmentBoundingBox;
				childAttachment->GetAABB(childAttachmentBoundingBox);
				vehicleBoundingBox.GrowAABB(childAttachmentBoundingBox);

				childAttachment = static_cast<const CPhysical*>(childAttachment->GetSiblingAttachment());
			}
			
			// Include the bounding box of the trailer or towed vehicle...			
			const CVehicle* trailerOrTowedVehicle = const_cast<const CVehicle*>(static_cast<CVehicle*>(trackingVehicle->GetAttachedTrailer()));
			if(!trailerOrTowedVehicle)
			{							
				const CEntity* towedEntity = trackingVehicle->GetEntityBeingTowed();
				const bool isTowingVehicle = towedEntity && towedEntity->GetIsTypeVehicle();
				if(isTowingVehicle)
				{
					trailerOrTowedVehicle = static_cast<const CVehicle*>(towedEntity);
				}
			}

			if(trailerOrTowedVehicle)
			{	
				spdAABB trailerOrTowedVehicleBoundingBox;
				trailerOrTowedVehicle->GetAABB(trailerOrTowedVehicleBoundingBox);
				vehicleBoundingBox.GrowAABB(trailerOrTowedVehicleBoundingBox);

				// ... and all vehicle attachments of the towed vehicle / trailer.
				const fwEntity* childAttachment = trailerOrTowedVehicle->GetChildAttachment();
				while(childAttachment)
				{
					spdAABB childAttachmentBoundingBox;
					childAttachment->GetAABB(childAttachmentBoundingBox);
					vehicleBoundingBox.GrowAABB(childAttachmentBoundingBox);

					childAttachment = childAttachment->GetSiblingAttachment();
				}
			}
			
			return vehicleBoundingBox.GetBoundingSphere().GetRadiusf();
		}
	}
	
	const float boundRadius = entity->GetBaseModelInfo() ? entity->GetBaseModelInfo()->GetBoundingSphereRadius() : entity->GetBoundRadius();	

	return boundRadius;
}

void camReplayPresetCamera::ResetCamera()
{
	camFrame defaultFrame;
	GetDefaultFrame(defaultFrame);

	//reset zoom
	const float defaultFov = defaultFrame.GetFov();
	m_Frame.SetFov(defaultFov);

	//reset position
	m_DesiredDistanceScale = 0.5f; //we don't change the frame's position, UpdatePosition will deal with that

	m_DistanceSpeed = 0.0f;
	m_RollSpeed		= 0.0f;
	m_RelativeRoll	= 0.0f;
	m_ZoomSpeed		= 0.0f;

	m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation);
}

void camReplayPresetCamera::UpdateControl(const CControl& control, float& desiredDistanceOffset, float& desiredRollOffset, float& desiredZoom)
{
	UpdateDistanceControl(control, desiredDistanceOffset);

	UpdateRollControl(control, desiredRollOffset);

	UpdateZoomControl(control, desiredZoom);
}

void camReplayPresetCamera::UpdateDistanceControl(const CControl& control, float& desiredDistanceOffset)
{
	float distanceInput;
	ComputeDistanceInput(control, distanceInput);

	const float timeStep = fwTimer::GetNonPausableCamTimeStep();

#if KEYBOARD_MOUSE_SUPPORT	
	if(control.GetValue(INPUT_CURSOR_SCROLL_UP).GetSource().m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE ||
		control.GetValue(INPUT_CURSOR_SCROLL_DOWN).GetSource().m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE)
	{				
		desiredDistanceOffset = distanceInput * timeStep;
	}
	else
	{
#endif
		const float distanceInputMag = Abs(distanceInput);
		if(distanceInputMag >= SMALL_FLOAT)
		{
			//Cache the sign of the vertical translation input, for use in deceleration.
			m_CachedDistanceInputSign = Sign(distanceInput);
		}

		const camReplayBaseCameraMetadataInputResponse& distanceInputResponse = m_Metadata.m_DistanceInputResponse;

		//Apply the custom power factor scaling specified in metadata to change the control response across the range of stick input.
		const float scaledDistanceInputMag = rage::Powf(Min(distanceInputMag, 1.0f), distanceInputResponse.m_InputMagPowerFactor);	

		const float distanceOffset = camControlHelper::ComputeOffsetBasedOnAcceleratedInput(scaledDistanceInputMag,
										distanceInputResponse.m_MaxAcceleration, distanceInputResponse.m_MaxDeceleration,
										distanceInputResponse.m_MaxSpeed, m_DistanceSpeed, timeStep, false);
			
		desiredDistanceOffset = m_CachedDistanceInputSign * distanceOffset;
#if KEYBOARD_MOUSE_SUPPORT	
	}
#endif
}

void camReplayPresetCamera::ComputeDistanceInput(const CControl& control, float& distanceInput) const
{
	distanceInput = 0.0f;

#if KEYBOARD_MOUSE_SUPPORT
	if(control.GetValue(INPUT_CURSOR_SCROLL_UP).GetSource().m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE ||
		control.GetValue(INPUT_CURSOR_SCROLL_DOWN).GetSource().m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE)
	{
		distanceInput = control.GetValue(INPUT_CURSOR_SCROLL_DOWN).GetNorm() - control.GetValue(INPUT_CURSOR_SCROLL_UP).GetNorm();
		distanceInput *= m_Metadata.m_MouseInputScale;
	}
	else
	{
#endif
		distanceInput = control.GetScriptRightAxisY().GetNorm();
#if KEYBOARD_MOUSE_SUPPORT
	}
#endif
}

void camReplayPresetCamera::UpdateRollControl(const CControl& control, float& desiredRollOffset)
{
	float rollInput;
	ComputeRollInput(control, rollInput);

	const float rollInputMag = Abs(rollInput);
	if(rollInputMag >= SMALL_FLOAT)
	{
		//Cache the sign of the roll input, for use in deceleration.
		m_CachedRollRotationInputSign = Sign(rollInput);
	}

	const camReplayBaseCameraMetadataInputResponse& rollInputResponse = m_Metadata.m_RollInputResponse;

	//Apply the custom power factor scaling specified in metadata to change the control response across the range of stick input.
	const float scaledRollInputMag = rage::Powf(Min(rollInputMag, 1.0f), rollInputResponse.m_InputMagPowerFactor);

	const float timeStep = fwTimer::GetNonPausableCamTimeStep();

	const float rollOffset = camControlHelper::ComputeOffsetBasedOnAcceleratedInput(scaledRollInputMag,
								rollInputResponse.m_MaxAcceleration, rollInputResponse.m_MaxDeceleration,
								rollInputResponse.m_MaxSpeed, m_RollSpeed, timeStep, false);

	desiredRollOffset = m_CachedRollRotationInputSign * rollOffset;
}

void camReplayPresetCamera::ComputeRollInput(const CControl& control, float& rollInput) const
{
	rollInput = 0.0f;

	if (control.GetScriptPadLeft().IsDown())
	{
		rollInput -= 1.0f;
	}
	if (control.GetScriptPadRight().IsDown())
	{
		rollInput += 1.0f;
	}
}

void camReplayPresetCamera::UpdateZoomControl(const CControl& control, float& desiredZoom)
{
	float zoomInput;
	ComputeZoomInput(control, zoomInput);

	const camReplayBaseCameraMetadataInputResponse& zoomInputResponse = m_Metadata.m_ZoomInputResponse;

	const float timeStep = fwTimer::GetNonPausableCamTimeStep();

	desiredZoom = camControlHelper::ComputeOffsetBasedOnAcceleratedInput(zoomInput,
					zoomInputResponse.m_MaxAcceleration, zoomInputResponse.m_MaxDeceleration,
					zoomInputResponse.m_MaxSpeed, m_ZoomSpeed, timeStep, false);
}

void camReplayPresetCamera::ComputeZoomInput(const CControl& control, float& zoomInput) const
{
	zoomInput = 0.0f;
	if (control.GetScriptLT().IsDown())
	{
		zoomInput -= 1.0f;
	}
	if (control.GetScriptRT().IsDown())
	{
		zoomInput += 1.0f;
	}
}

void camReplayPresetCamera::UpdateZoom(const float desiredZoom)
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

void camReplayPresetCamera::ComputeLookAtOrientation(Matrix34& rMatrix, const Matrix34& entityMatrix, const Vector3& lookAtPosition, const Vector3& attachPosition, const float roll)
{
	Vector3 cameraFront = lookAtPosition - attachPosition;
	entityMatrix.UnTransform3x3(cameraFront);	
	camFrame::ComputeWorldMatrixFromFront(cameraFront, rMatrix);
	rMatrix.RotateLocalY(roll);
	rMatrix.Dot(entityMatrix);
}

void camReplayPresetCamera::UpdateRotation(const float desiredRollOffset)
{	
	if(!m_Metadata.m_ShouldAllowRollControl)
	{
		m_RelativeRoll	= 0.0f;
		m_RollSpeed		= 0.0f;
	}
	else
	{
		m_RelativeRoll += desiredRollOffset;
	}

	Vector3 lookAtPosition, defaultAttachPosition;
	ComputeLookAtAndDefaultAttachPositions(lookAtPosition, defaultAttachPosition);

	Matrix34 worldMatrix;
	ComputeLookAtOrientation(worldMatrix, m_TrackingEntityMatrix, lookAtPosition, defaultAttachPosition, m_RelativeRoll);

	m_Frame.SetWorldMatrix(worldMatrix);
}

void camReplayPresetCamera::UpdatePosition(const float desiredDistanceOffset, Vector3& preCollisionPosition)
{
	//NOTE: m_TrackingEntity has already been verified
	const float boundRadius				= GetBoundRadius(m_TrackingEntity);

	const float minCameraDistance		= m_DefaultDistanceScalar * m_Metadata.m_MinDistanceScalar * boundRadius;
	const float maxCameraDistance		= m_DefaultDistanceScalar * m_Metadata.m_MaxDistanceScalar * boundRadius;

	const float baseCameraDistance		= RampValueSafe(m_DesiredDistanceScale, 0.0f, 1.0f, minCameraDistance, maxCameraDistance);
	const float cameraDistanceToApply	= Clamp(baseCameraDistance + desiredDistanceOffset, minCameraDistance, maxCameraDistance);

	Vector3 lookAtPosition, defaultAttachPosition;
	ComputeLookAtAndDefaultAttachPositions(lookAtPosition, defaultAttachPosition);

	Vector3 cameraFront					= lookAtPosition - defaultAttachPosition;
	cameraFront.Normalize();
	Vector3 positionToApply				= lookAtPosition - (cameraFront * cameraDistanceToApply);

	preCollisionPosition = positionToApply;
	
	//Constrain against world collision.
	const bool wasConstrainedAgainstCollision = UpdateCollision(positionToApply);

	//Zero the cached speed when we constrain against collision.
	if(wasConstrainedAgainstCollision)
	{
		m_DistanceSpeed = 0.0f;
	}

	m_Frame.SetPosition(positionToApply);
}

bool camReplayPresetCamera::UpdateCollision(Vector3& cameraPosition)
{
	if(m_Collision == NULL)
	{
		return false;
	}

	const camReplayBaseCameraMetadataCollisionSettings& settings = m_Metadata.m_CollisionSettings;

	//NOTE: m_TrackingEntity has already been validated
	m_Collision->IgnoreEntityThisUpdate(*m_TrackingEntity);
	m_Collision->SetShouldIgnoreDynamicCollision(true);

	const bool shouldPushBeyondAttachParentIfClipping = settings.m_ShouldPushBeyondAttachParentIfClipping; //TODO: deal with ragdolls somehow
	if(shouldPushBeyondAttachParentIfClipping)
	{
		m_Collision->PushBeyondEntityIfClippingThisUpdate(*m_TrackingEntity);
	}

	//Deal with trailer or towed vehicles
	if(m_TrackingEntity->GetIsTypeVehicle())
	{
		const CVehicle* followVehicle = static_cast<const CVehicle*>(m_TrackingEntity.Get());
		if(followVehicle)
		{
			const CVehicle* trailerOrTowedVehicle = const_cast<const CVehicle*>(static_cast<CVehicle*>(followVehicle->GetAttachedTrailer()));
			if(!trailerOrTowedVehicle)
			{							
				const CEntity* towedEntity = followVehicle->GetEntityBeingTowed();
				const bool isTowingVehicle = towedEntity && towedEntity->GetIsTypeVehicle();
				if(isTowingVehicle)
				{
					trailerOrTowedVehicle = static_cast<const CVehicle*>(towedEntity);
				}
			}

			if(trailerOrTowedVehicle)
			{
				m_Collision->IgnoreEntityThisUpdate(*trailerOrTowedVehicle);
				m_Collision->PushBeyondEntityIfClippingThisUpdate(*trailerOrTowedVehicle);
			}
		}
	}
	
	//we don't worry about vehicles on top of vehicles

	float desiredCollisionTestRadius = m_Metadata.m_MaxCollisionTestRadius;

	TUNE_GROUP_BOOL(REPLAY_PRESET_CAMERA, bAvoidCollisionWithAllNearbyParachutes, true);
	if( !bAvoidCollisionWithAllNearbyParachutes )
	{
		const fwAttachmentEntityExtension *pedAttachmentExtension = m_TrackingEntity->GetAttachmentExtension();
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
		camReplayBaseCamera::AvoidCollisionWithParachutes(m_TrackingEntityMatrix.d, m_Collision.Get());
	}

	if(m_TrackingEntity->GetIsTypePed())
	{
		const CPed* trackingPed	= static_cast<const CPed*>(m_TrackingEntity.Get());

		float radius			= trackingPed->GetCurrentMainMoverCapsuleRadius();
		radius					-= settings.m_MinSafeRadiusReductionWithinPedMoverCapsule;
		if(radius >= SMALL_FLOAT)
		{
			desiredCollisionTestRadius = Min(radius, desiredCollisionTestRadius);
		}
	}

	const float collisionTestRadiusOnPreviousUpdate = m_CollisionTestRadiusSpring.GetResult();

	//NOTE: We must cut to a lesser radius in order to avoid compromising the shapetests.
	const bool shouldCutSpring		= ((m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
										m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation) ||
										(desiredCollisionTestRadius < collisionTestRadiusOnPreviousUpdate));
	const float springConstant		= shouldCutSpring ? 0.0f : settings.m_TestRadiusSpringConstant;

	float collisionTestRadius		= m_CollisionTestRadiusSpring.Update(desiredCollisionTestRadius, springConstant,
										settings.m_TestRadiusSpringDampingRatio, true);

	UpdateCollisionOrigin(collisionTestRadius);

	UpdateCollisionRootPosition(collisionTestRadius);

	if(m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation))
	{
		//Bypass any orbit distance damping and force pop-in behaviour.
		m_Collision->Reset();
	}

	//we don't behave differently during a networked game

	m_Collision->SetEntityToConsiderForTouchingChecksThisUpdate(*m_TrackingEntity);
	m_Collision->IgnoreCollisionWithTouchingPedsAndVehiclesThisUpdate(); //we ignore any ped or vehicle the player is touching

	const Vector3 preCollisionCameraPosition(cameraPosition);

	const bool isCurrentlyEditingThisMarker	= (CReplayMarkerContext::GetEditingMarkerIndex() == m_MarkerIndex);
	if(isCurrentlyEditingThisMarker)
	{
		m_Collision->Reset();
	}

	float orbitHeading = 0.0f;
	float orbitPitch = 0.0f;
	m_Collision->Update(m_CollisionRootPosition, collisionTestRadius, true, cameraPosition, orbitHeading, orbitPitch);

	if (cameraPosition.Dist2(preCollisionCameraPosition) >= SMALL_FLOAT) // SMALL_FLOAT instead of VERY_SMALL_FLOAT because precision.
	{
		return true;
	}
	else
	{
		return false;
	}
}

void camReplayPresetCamera::UpdateCollisionOrigin(float& UNUSED_PARAM(collisionTestRadius))
{
	m_CollisionOrigin = m_TrackingEntityMatrix.d;
}

void camReplayPresetCamera::UpdateCollisionRootPosition(float& collisionTestRadius)
{
	m_CollisionRootPosition = m_CollisionOrigin;	

    if(! m_TrackingEntity)
    {
        return;
    }

    //B*2305145 copy the shapetest code from the ReplayFreeCamera::UpdateCollisionRootPosition
    Vector3 relativeCollisionOrigin;
    const Matrix34 trackingEntityTransform = m_TrackingEntityMatrix;
    trackingEntityTransform.UnTransform(m_CollisionOrigin, relativeCollisionOrigin); //Transform to parent-local space.

    const Vector3& trackingEntityBoundingBoxMin = m_TrackingEntity->GetBoundingBoxMin();
    const Vector3& trackingEntityBoundingBoxMax = m_TrackingEntity->GetBoundingBoxMax();
    const float parentHeight = trackingEntityBoundingBoxMax.z - trackingEntityBoundingBoxMin.z;

    const float trackingEntityHeightRatioToAttain = m_Metadata.m_CollisionSettings.m_TrackingEntityHeightRatioToAttain;

    //we don't bother springing this offset
    const CPed* trackingPed = m_TrackingEntity->GetIsTypePed() ? static_cast<const CPed*>(m_TrackingEntity.Get()) : NULL;
    const float extraOffset	= trackingPed ? trackingPed->GetIkManager().GetPelvisDeltaZForCamera(false) : 0.0f;

    const float collisionFallBackOffset	= (parentHeight * trackingEntityHeightRatioToAttain) + trackingEntityBoundingBoxMin.z - relativeCollisionOrigin.z + extraOffset;

	const CVehicle* trackingVehicle = m_TrackingEntity->GetIsTypeVehicle() ? static_cast<const CVehicle*>(m_TrackingEntity.Get()) : NULL;
	if(trackingVehicle && trackingVehicle->HasContactWheels())
	{
		//vehicle can be upside down and still on track, apply the offset in entity space
		m_CollisionRootPosition += VEC3V_TO_VECTOR3(m_TrackingEntity->GetMatrix().c()) * collisionFallBackOffset;
	}
	else
	{
		m_CollisionRootPosition.z	+= collisionFallBackOffset;
	}

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

    const float collisionOriginToCollisionRootPositionDistance2 = m_CollisionOrigin.Dist2(m_CollisionRootPosition);
    if(collisionOriginToCollisionRootPositionDistance2 > VERY_SMALL_FLOAT)
    {
        if(!cameraVerifyf(collisionTestRadius >= SMALL_FLOAT,
            "Unable to compute a valid collision fall back position due to an invalid test radius."))
        {
            return;
        }

        shapeTestResults.Reset();
        capsuleTest.SetCapsule(m_CollisionOrigin, m_CollisionRootPosition, collisionTestRadius);

        //Reduce the test radius in readiness for use in a consecutive test.
        collisionTestRadius -= camCollision::GetMinRadiusReductionBetweenInterativeShapetests();

        const bool hasHit				= WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);
        const float blendLevel			= hasHit ? shapeTestResults[0].GetHitTValue() : 1.0f;

        m_CollisionRootPosition.Lerp(blendLevel, m_CollisionOrigin, m_CollisionRootPosition);
    }
}

bool camReplayPresetCamera::GetShouldResetCamera(const CControl& control) const
{
	const bool isCurrentlyEditingThisMarker	= (CReplayMarkerContext::GetEditingMarkerIndex() == m_MarkerIndex);
	if (!isCurrentlyEditingThisMarker)
	{
		return false;
	}

	const bool shouldReset = control.GetScriptLS().IsPressed();

	return shouldReset;
}

void camReplayPresetCamera::SaveMarkerCameraInfo(sReplayMarkerInfo& markerInfo, const camFrame& frame) const
{
	camReplayBaseCamera::SaveMarkerCameraInfo(markerInfo, frame);

	float (&relativePosition)[ 3 ] = markerInfo.GetFreeCamPositionRelative();

	relativePosition[0]	= 0.0f;
	relativePosition[1]	= m_DesiredDistanceScale;
	relativePosition[2]	= 0.0f;
	
	markerInfo.SetFreeCamRelativeRoll(m_RelativeRoll);

	float (&trackingEntityPosition)[ 3 ] = markerInfo.GetAttachEntityPosition();

	trackingEntityPosition[0] = m_TrackingEntityMatrix.d.x;
	trackingEntityPosition[1] = m_TrackingEntityMatrix.d.y;
	trackingEntityPosition[2] = m_TrackingEntityMatrix.d.z;

	Quaternion quat;
	m_TrackingEntityMatrix.ToQuaternion(quat);
	markerInfo.SetAttachEntityQuaternion(QuantizeQuaternionS3_20(quat));

	markerInfo.SetAttachEntityMatrixValid( true );	
}

void camReplayPresetCamera::LoadMarkerCameraInfo(sReplayMarkerInfo& markerInfo, camFrame& frame)
{
	camReplayBaseCamera::LoadMarkerCameraInfo(markerInfo, frame);

	m_DesiredDistanceScale	= markerInfo.GetFreeCamPositionRelative()[1];
	m_RelativeRoll			= markerInfo.GetFreeCamRelativeRoll();

	Quaternion orientation;
	DeQuantizeQuaternionS3_20(orientation, markerInfo.GetAttachEntityQuaternion() );

	if (orientation.Mag2() < square(0.999f) || orientation.Mag2() > square(1.001f))
	{
		//bad data must have been stored to the marker info, we need to reset it
		markerInfo.SetAttachEntityMatrixValid(false);
	}

	if (markerInfo.IsAttachEntityMatrixValid())
	{
		m_TrackingEntityMatrix.FromQuaternion(orientation);

		Vector3 position;
		float const (&positionRaw)[ 3 ] = markerInfo.GetAttachEntityPosition();
		m_TrackingEntityMatrix.d.Set(positionRaw[0], positionRaw[1], positionRaw[2]);
	}
}

void camReplayPresetCamera::GetMinMaxFov(float& minFov, float& maxFov) const
{
	minFov = m_Metadata.m_MinFov;
	maxFov = m_Metadata.m_MaxFov;
}

void camReplayPresetCamera::GetDefaultFov( float& defaultFov ) const
{
	defaultFov = m_Metadata.m_DefaultFov;
}

const CEntity* camReplayPresetCamera::UpdateTrackingEntity()
{
	const CPed* playerPed = CReplayMgr::GetMainPlayerPtr();
	if (!playerPed)
	{
		return NULL;
	}

	const CVehicle* playerVehicle = const_cast<const CVehicle*>(playerPed->GetVehiclePedInside());
	if(!playerVehicle && const_cast<CPed*>(playerPed)->GetReplayRelevantAITaskInfo().IsAITaskRunning(REPLAY_ENTER_VEHICLE_SEAT_TASK)) // playerPed is cast to non-const because GetReplayRelevantAITaskInfo is non-const and returns non-const ref
	{		
		const fwEntity* fwEntityPtr = const_cast<const fwEntity*>(playerPed->GetAttachParent());
		if(fwEntityPtr)
		{
			const CEntity* attachParent = static_cast<const CEntity*>(fwEntityPtr);
			if(attachParent->GetIsTypeVehicle())
			{
				playerVehicle = static_cast<const CVehicle*>(attachParent);
			}
		}					
	}

	if(playerVehicle)
	{
		if (!m_TrackingEntityIsInVehicle && m_TrackingEntity)
		{
			//getting into a vehicle
			camInterface::GetReplayDirector().ForceSmoothing(m_Metadata.m_InVehicleForceSmoothingDurationMs, 0, m_Metadata.m_InVehicleForceSmoothingBlendOutDurationMs);
		}

		m_TrackingEntityIsInVehicle = true;
		m_TrackingEntity			= playerVehicle;
	}
	else
	{
		if (m_TrackingEntityIsInVehicle)
		{
			//getting out of a vehicle
			camInterface::GetReplayDirector().ForceSmoothing(m_Metadata.m_OutVehicleForceSmoothingDurationMs, 0, m_Metadata.m_OutVehicleForceSmoothingBlendOutDurationMs);
		}

		m_TrackingEntityIsInVehicle = false;
		m_TrackingEntity			= playerPed;
	}

	return m_TrackingEntity;
}

#if __BANK
void camReplayPresetCamera::DebugRender()
{
	camBaseCamera::DebugRender();

	Vector3 lookAtPosition, defaultAttachPosition;
	ComputeLookAtAndDefaultAttachPositions(lookAtPosition, defaultAttachPosition);

	const Vector3& cameraPosition = m_Frame.GetPosition();

	static float radius = 0.125f;

	// look at sphere
	TUNE_GROUP_BOOL(REPLAY_PRESET_CAMERA, shouldDrawLookAtSphere, true);
	if (shouldDrawLookAtSphere)
	{
		grcDebugDraw::Sphere(RCC_VEC3V(lookAtPosition), radius, Color_green);
	}

	// attach position sphere
	TUNE_GROUP_BOOL(REPLAY_PRESET_CAMERA, shouldDrawDefaultPositionSphere, true);
	if (shouldDrawDefaultPositionSphere)
	{
		grcDebugDraw::Sphere(RCC_VEC3V(defaultAttachPosition), radius, Color_blue);
	}

	//camera line
	TUNE_GROUP_BOOL(REPLAY_PRESET_CAMERA, shouldDrawCameraPositionLine, true);
	if (shouldDrawCameraPositionLine)
	{
		grcDebugDraw::Line(cameraPosition, lookAtPosition, Color_green, Color_green);
	}

	//default line
	TUNE_GROUP_BOOL(REPLAY_PRESET_CAMERA, shouldDrawDefaultPositionLine, false);
	if (shouldDrawDefaultPositionLine)
	{
		grcDebugDraw::Line(defaultAttachPosition, lookAtPosition, Color_blue, Color_blue);
	}

	if (m_TrackingEntity)
	{
		const float boundRadius			= GetBoundRadius(m_TrackingEntity);
		const float minCameraDistance	= m_DefaultDistanceScalar * m_Metadata.m_MinDistanceScalar * boundRadius;
		const float maxCameraDistance	= m_DefaultDistanceScalar * m_Metadata.m_MaxDistanceScalar * boundRadius;

		const Vector3 cameraFront		= m_Frame.GetFront();
		const Vector3 minPosition		= lookAtPosition - (cameraFront * minCameraDistance);
		const Vector3 maxPosition		= lookAtPosition - (cameraFront * maxCameraDistance);

		//min line
		TUNE_GROUP_BOOL(REPLAY_PRESET_CAMERA, shouldDrawMinPositionLine, false);
		if (shouldDrawMinPositionLine)
		{
			grcDebugDraw::Line(minPosition, lookAtPosition, Color_red, Color_red);
		}

		//max line
		TUNE_GROUP_BOOL(REPLAY_PRESET_CAMERA, shouldDrawMaxPositionLine, false);
		if (shouldDrawMaxPositionLine)
		{
			grcDebugDraw::Line(maxPosition, lookAtPosition, Color_magenta, Color_magenta);
		}
	}
}
#endif // __BANK

#endif // GTA_REPLAY
