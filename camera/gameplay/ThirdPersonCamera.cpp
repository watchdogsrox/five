//
// camera/gameplay/ThirdPersonCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/gameplay/ThirdPersonCamera.h"

#include "profile/group.h"
#include "profile/page.h"

#include "fwmaths/angle.h"

#include "camera/CamInterface.h"
#include "camera/cinematic/CinematicDirector.h"
#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"
#include "camera/cutscene/CutsceneDirector.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/aim/FirstPersonAimCamera.h"
#include "camera/gameplay/aim/FirstPersonShooterCamera.h"
#include "camera/gameplay/aim/ThirdPersonVehicleAimCamera.h"
#include "camera/gameplay/follow/FollowVehicleCamera.h"
#include "camera/helpers/switch/BaseSwitchHelper.h"
#include "camera/helpers/CatchUpHelper.h"
#include "camera/helpers/Collision.h"
#include "camera/helpers/ControlHelper.h"
#include "camera/helpers/Envelope.h"
#include "camera/helpers/HintHelper.h"
#include "camera/helpers/ThirdPersonFrameInterpolator.h"
#include "camera/system/CameraFactory.h"
#include "camera/system/CameraManager.h"
#include "camera/system/CameraMetadata.h"
#include "control/record.h"
#include "frontend/PauseMenu.h"
#include "network/objects/entities/NetObjPlayer.h"
#include "peds/PedIntelligence.h"
#include "Task/Animation/TaskScriptedAnimation.h"
#include "task/Motion/Locomotion/TaskMotionPed.h"
#include "task/Movement/Climbing/TaskVault.h"
#include "task/Movement/TaskParachute.h"
#include "task/Vehicle/TaskEnterVehicle.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "vehicles/Planes.h"
#include "vehicles/Trailer.h"
#include "vehicles/Wheel.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camThirdPersonCamera,0xE8287878)

PF_PAGE(camThirdPersonCameraPage, "Camera: Third-Person Camera");

PF_GROUP(camThirdPersonCameraMetrics);
PF_LINK(camThirdPersonCameraPage, camThirdPersonCameraMetrics);

PF_VALUE_FLOAT(desiredAttachPedPelvisOffset, camThirdPersonCameraMetrics);
PF_VALUE_FLOAT(dampedAttachPedPelvisOffset, camThirdPersonCameraMetrics);

PF_VALUE_FLOAT(desiredAttachParentRoll, camThirdPersonCameraMetrics);
PF_VALUE_FLOAT(dampedAttachParentRoll, camThirdPersonCameraMetrics);

PF_VALUE_FLOAT(nearThirdPersonViewModeBlendLevel, camThirdPersonCameraMetrics);
PF_VALUE_FLOAT(mediumThirdPersonViewModeBlendLevel, camThirdPersonCameraMetrics);
PF_VALUE_FLOAT(farThirdPersonViewModeBlendLevel, camThirdPersonCameraMetrics);

PF_VALUE_FLOAT(desiredRollAttachParentPitch, camThirdPersonCameraMetrics);
PF_VALUE_FLOAT(desiredRollDampingLimitBlendLevel, camThirdPersonCameraMetrics);
PF_VALUE_FLOAT(desiredRollAttachParentRoll, camThirdPersonCameraMetrics);
PF_VALUE_FLOAT(desiredRollDampedAttachParentRoll, camThirdPersonCameraMetrics);

const float		g_MaxErrorForOverriddenOrbitOrientation	= (1.0f * DtoR);
const float		g_MaxAbsSafePitchLimit					= (89.5f * DtoR);
const float		g_MinDistanceFromCameraToLookAtPosition	= 0.01f;			//Ensure a safe minimum distance between the camera and look-at positions.

const float		g_MaxAttachParentSubmergedLevelToApplyFullAttachParentMatrixForRelativeOrbitForPlanes = 0.5f;

extern const u32 g_CatchUpHelperRefForFirstPerson = ATSTRINGHASH("FIRST_PERSON_SHOOTER_CATCH_UP_HELPER", 0x92c7d98d);


camThirdPersonCamera::camThirdPersonCamera(const camBaseObjectMetadata& metadata)
: camBaseCamera(metadata)
, m_Metadata(static_cast<const camThirdPersonCameraMetadata&>(metadata))
, m_AttachParentMatrix(M34_IDENTITY)
, m_AttachParentMatrixToConsiderForRelativeOrbit(M34_IDENTITY)
, m_PreviousAttachParentOrientationDeltaForRelativeOrbit(Quaternion::sm_I)
, m_AttachParentOrientationOnPreviousUpdate(Quaternion::sm_I)
, m_AttachParentBoundingBoxMin(VEC3_ZERO)
, m_AttachParentBoundingBoxMax(VEC3_ZERO)
, m_BaseAttachPositionOnPreviousUpdate(g_InvalidPosition)
, m_BaseAttachPosition(g_InvalidPosition)
, m_BaseAttachVelocityToConsider(VEC3_ZERO)
, m_BaseAttachVelocityToIgnoreForMostRecentEntity(VEC3_ZERO)
, m_BaseAttachVelocityToIgnore(VEC3_ZERO)
, m_BasePivotPosition(g_InvalidPosition)
, m_BaseFront(YAXIS)
, m_BasePosition(VEC3_ZERO)
, m_PivotPosition(g_InvalidPosition)
, m_PreCollisionCameraPosition(g_InvalidPosition)
, m_CollisionOrigin(g_InvalidPosition)
, m_CollisionRootPosition(g_InvalidPosition)
, m_LookAtPosition(g_InvalidPosition)
, m_RelativeOrbitHeadingLimits(m_Metadata.m_RelativeOrbitHeadingLimits * DtoR)
, m_OrbitPitchLimits(m_Metadata.m_OrbitPitchLimits * DtoR)
, m_ControlHelper(NULL)
, m_HintHelper(NULL)
, m_CatchUpHelper(NULL)
, m_BaseAttachVelocityToIgnoreEnvelope(NULL)
, m_CustomVehicleSettings(NULL)
, m_MostRecentTimeFullAttachParentMatrixBlockedForRelativeOrbit(0)
, m_DesiredBuoyancyStateTransitionTime(0)
, m_BuoyancyStateTransitionTime(0)
, m_MotionBlurForAttachParentDamageStartTime(0)
, m_BoundingBoxExtensionForOrbitDistance(0.0f)
, m_OrbitDistanceExtensionForTowedVehicle(0.0f)
, m_RelativeHeadingToApply(0.0f)		//Start with the camera directly behind the attach parent.
, m_RelativePitchToApply(0.0f)			// ...
, m_RelativeHeadingSmoothRate(1.0f)
, m_RelativePitchSmoothRate(1.0f)
, m_RelativeHeadingForLimitingOnPreviousUpdate(THREE_PI) //Invalidate.
, m_MotionBlurPreviousAttachParentHealth(0.0f)
, m_MotionBlurPeakAttachParentDamage(0.0f)
, m_BlendFovDelta(0.0f)
, m_BlendFovDeltaStartTime(0)
, m_BlendFovDeltaDuration(1000)
, m_DistanceTravelled(0.0f)
, m_AfterAimingActionModeBlendStartTime(0)
, m_ShouldApplyFullAttachParentMatrixForRelativeOrbit(true)
, m_ShouldApplyRelativeHeading(true)	//Start with the camera directly behind the attach parent.
, m_ShouldApplyRelativePitch(true)		// ...
, m_ShouldResetFullAttachParentMatrixForRelativeOrbitTimer(false)
, m_ShouldIgnoreInputForRelativeHeading(true)
, m_ShouldIgnoreInputForRelativePitch(true)
, m_ShouldApplyRelativeHeadingWrtIdeal(false)
, m_ShouldFlagRelativeHeadingChangeAsCut(true)
, m_ShouldFlagRelativePitchChangeAsCut(true)
, m_DesiredBuoyancyState(true)
, m_IsBuoyant(true)
, m_ShouldBypassBuoyancyStateThisUpdate(false)
, m_WasBypassingBuoyancyStateOnPreviousUpdate(false)
{
	m_ControlHelper = camFactory::CreateObject<camControlHelper>(m_Metadata.m_ControlHelperRef.GetHash());
	cameraAssertf(m_ControlHelper, "A third-person camera (name: %s, hash: %u) failed to create a control helper (name: %s, hash: %u)",
		GetName(), GetNameHash(), SAFE_CSTRING(m_Metadata.m_ControlHelperRef.GetCStr()), m_Metadata.m_ControlHelperRef.GetHash());

	const camEnvelopeMetadata* envelopeMetadata =
		camFactory::FindObjectMetadata<camEnvelopeMetadata>(m_Metadata.m_BaseAttachVelocityToIgnoreEnvelopeRef.GetHash());
	if(envelopeMetadata)
	{
		m_BaseAttachVelocityToIgnoreEnvelope = camFactory::CreateObject<camEnvelope>(*envelopeMetadata);
		if(cameraVerifyf(m_BaseAttachVelocityToIgnoreEnvelope, "A third-person camera (name: %s, hash: %u) failed to create an envelope (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(envelopeMetadata->m_Name.GetCStr()), envelopeMetadata->m_Name.GetHash()))
		{
			m_BaseAttachVelocityToIgnoreEnvelope->SetUseGameTime(true);
		}
	}

	if( !camInterface::IsDominantRenderedDirector(camInterface::GetGameplayDirector()) )
	{
		m_DistanceTravelled = m_Metadata.m_PivotPosition.m_maxDistanceAfterAimingToApplyAlternateScalingMax+1.0f;
	}
}

camThirdPersonCamera::~camThirdPersonCamera()
{
	m_VehiclesOnTopOfAttachVehicleList.Reset();

	if(m_ControlHelper)
	{
		delete m_ControlHelper;
	}

	if(m_BaseAttachVelocityToIgnoreEnvelope)
	{
		delete m_BaseAttachVelocityToIgnoreEnvelope;
	}

	if(m_HintHelper)
	{
		delete m_HintHelper; 
	}

	if(m_CatchUpHelper)
	{
		delete m_CatchUpHelper;
	}
}

bool camThirdPersonCamera::Update()
{
	if(!m_AttachParent || !m_ControlHelper)
	{
		return false;
	}
	
	InitVehicleCustomSettings(); 

	//Flag any cuts that will result from orbit overrides at the start of the update, as this allows any subsequent damping to be bypassed.
	FlagCutsForOrbitOverrides();

	UpdateFirstPersonCoverSettings();
	UpdateControlHelper();

	CreateOrDestroyHintHelper();

	UpdateAttachParentMatrix();
	UpdateAttachParentMatrixToConsiderForRelativeOrbit();

	const bool isBoundingBoxValid = UpdateAttachParentBoundingBox();
	if(!isBoundingBoxValid)
	{
		return false;
	}

	UpdateAttachPedPelvisOffsetSpring();

	UpdateBaseAttachPosition();
	UpdateBaseAttachVelocities();

	const float gameTimeStep		= fwTimer::GetTimeStep();
	const Vector3 movementToIgnore	= m_BaseAttachVelocityToIgnore * gameTimeStep;
	m_BasePosition					+= movementToIgnore;

	UpdateTightSpaceSpring();

	UpdateStealthZoom();

	UpdateBasePivotPosition();

    UpdateBasePivotSpring();

	UpdateOrbitPitchLimits();

	UpdateLockOn();

	const float baseOrbitDistance = UpdateOrbitDistance();

	float orbitDistanceToApply = baseOrbitDistance;
	if(m_HintHelper)
	{
		float distanceScalar;
		m_HintHelper->ComputeHintFollowDistanceScalar(distanceScalar);

		orbitDistanceToApply *= distanceScalar;
	}

	const camBaseSwitchHelper* switchHelper = camInterface::GetGameplayDirector().GetSwitchHelper();
	if(switchHelper)
	{
		switchHelper->ComputeOrbitDistance(orbitDistanceToApply);
	}
	else
	{
		//NOTE: We do not generally scale the orbit distance during custom Switch behaviours. Any orbit distance change is handled by the Switch helper in such cases.
		const bool isFallingBackToThirdPersonForDirectorBlendCatchUpOrSwitch = camInterface::GetGameplayDirector().IsFallingBackToThirdPersonForDirectorBlendCatchUpOrSwitch();
		if(isFallingBackToThirdPersonForDirectorBlendCatchUpOrSwitch && !camInterface::GetCutsceneDirector().IsPerformingFirstPersonBlendOut())
		{
			orbitDistanceToApply *= m_Metadata.m_OrbitDistanceScalingForCustomFirstPersonFallBack;
		}
	}

	float orbitHeading;
	float orbitPitch;
	ComputeOrbitOrientation(orbitHeading, orbitPitch);

	if(m_HintHelper)
	{
		const float lookAroundInputEnvelopeLevel = m_ControlHelper->GetLookAroundInputEnvelopeLevel();
		
		const float orbitPitchOffset = ComputeOrbitPitchOffset();
		
		Vector3 orbitRelativePivotOffset;

		Vector3 orbitFront;
		camFrame::ComputeFrontFromHeadingAndPitch(orbitHeading, orbitPitch, orbitFront);

		ComputeOrbitRelativePivotOffset(orbitFront, orbitRelativePivotOffset);

		m_HintHelper->ComputeHintOrientation(m_BasePivotPosition, m_OrbitPitchLimits, lookAroundInputEnvelopeLevel, orbitHeading, orbitPitch,  m_Frame.GetFov(), orbitPitchOffset, orbitRelativePivotOffset);
	}

	if(ShouldOrbitRelativeToAttachParentOrientation())
	{
		//Convert the orbit orientation to be parent-relative.
		Vector3 orbitFront;
		camFrame::ComputeFrontFromHeadingAndPitch(orbitHeading, orbitPitch, orbitFront);

		m_AttachParentMatrixToConsiderForRelativeOrbit.UnTransform3x3(orbitFront);

		camFrame::ComputeHeadingAndPitchFromFront(orbitFront, orbitHeading, orbitPitch);
	}

	ApplyOverriddenOrbitOrientation(orbitHeading, orbitPitch);

	const bool isLookingBehind = m_ControlHelper->IsLookingBehind();
	if(isLookingBehind)
	{
		//Flip the basic orbit orientation when looking behind, prior to any offsets being applied.
		orbitHeading	+= PI;
		orbitPitch		= -orbitPitch;
	}

	float lookAroundHeadingOffset;
	float lookAroundPitchOffset;
	ComputeLookAroundOrientationOffsets(lookAroundHeadingOffset, lookAroundPitchOffset);

	orbitHeading	+= lookAroundHeadingOffset;
	orbitPitch		+= lookAroundPitchOffset;

	LimitOrbitHeading(orbitHeading);
	LimitOrbitPitch(orbitPitch);

	//Apply any orientation offsets that are not propagated to the next update.

	float orbitPitchOffset	= ComputeOrbitPitchOffset();

	orbitPitch += orbitPitchOffset;
	LimitOrbitPitch(orbitPitch);

	float switchHeadingDelta	= 0.0f;
	float switchPitchDelta		= 0.0f;

	if(switchHelper)
	{
		const float originalOrbitHeading	= orbitHeading;
		const float originalOrbitPitch		= orbitPitch;

		const Matrix34 attachParentMatrix	= MAT34V_TO_MATRIX34(m_AttachParent->GetMatrix());
		const float attachParentHeading		= camFrame::ComputeHeadingFromMatrix(attachParentMatrix);

		switchHelper->ComputeOrbitOrientation(attachParentHeading, orbitHeading, orbitPitch);

		switchHeadingDelta	= orbitHeading - originalOrbitHeading;
		switchPitchDelta	= orbitPitch - originalOrbitPitch;
	}

	Matrix34 orbitMatrix;
	camFrame::ComputeWorldMatrixFromHeadingPitchAndRoll(orbitHeading, orbitPitch, 0.0f, orbitMatrix);

	orbitMatrix.d = m_BasePivotPosition;

	if(ShouldOrbitRelativeToAttachParentOrientation())
	{
		orbitMatrix.Dot3x3(m_AttachParentMatrixToConsiderForRelativeOrbit);
	}

	UpdatePivotPosition(orbitMatrix);

	Vector3 orbitFront(orbitMatrix.b);

	Vector3 cameraPosition = m_PivotPosition - (orbitFront * orbitDistanceToApply);

	if(m_CatchUpHelper)
	{
		const float maxOrbitDistance	= m_MaxOrbitDistanceSpring.GetResult();
		const bool hasCut				= m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
											m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation);
		const bool hasSucceeded			= m_CatchUpHelper->Update(*this, maxOrbitDistance, hasCut, m_BaseAttachVelocityToConsider);
		if(hasSucceeded)
		{
			m_CatchUpHelper->UpdateCameraPositionDelta(orbitFront, cameraPosition);
		}
		else
		{
			delete m_CatchUpHelper;
			m_CatchUpHelper = NULL;
		}
	}

	m_PreCollisionCameraPosition = cameraPosition;

	float orbitHeadingOffsetDueToCollision	= 0.0f;
	float orbitPitchOffsetDueToCollision	= 0.0f;

	UpdateCollision(cameraPosition, orbitHeadingOffsetDueToCollision, orbitPitchOffsetDueToCollision);

	//Apply the camera position.
	m_Frame.SetPosition(cameraPosition);

	camFrame::ComputeHeadingAndPitchFromFront(orbitFront, orbitHeading, orbitPitch);

	//Apply any orientation offset that must be propagated due to collision.
	orbitHeading	+= orbitHeadingOffsetDueToCollision;
	orbitPitch		+= orbitPitchOffsetDueToCollision;

	//Now remove all orientation offsets that are not propagated, after limiting, as this prevents overshoot and bouncing.

	if(ShouldOrbitRelativeToAttachParentOrientation())
	{
		//Convert the orbit orientation to be parent-relative.
		Vector3 tempOrbitFront;
		camFrame::ComputeFrontFromHeadingAndPitch(orbitHeading, orbitPitch, tempOrbitFront);

		m_AttachParentMatrixToConsiderForRelativeOrbit.UnTransform3x3(tempOrbitFront);

		camFrame::ComputeHeadingAndPitchFromFront(tempOrbitFront, orbitHeading, orbitPitch);
	}

	orbitHeading	-= switchHeadingDelta;
	orbitPitch		-= switchPitchDelta;

	orbitPitch		-= orbitPitchOffset;

	LimitOrbitHeading(orbitHeading);
	LimitOrbitPitch(orbitPitch);

	camFrame::ComputeFrontFromHeadingAndPitch(orbitHeading, orbitPitch, m_BaseFront);

	if(ShouldOrbitRelativeToAttachParentOrientation())
	{
		//Convert the orbit orientation to world-space.
		m_AttachParentMatrixToConsiderForRelativeOrbit.Transform3x3(m_BaseFront);
	}

	if(isLookingBehind)
	{
		//Finally, flip the orbit orientation again when looking behind, after all non-propagating offsets and limiting have been processed.
		m_BaseFront	= -m_BaseFront;
	}

	Vector2 orbitDistanceLimitsForBasePosition;
	ComputeOrbitDistanceLimitsForBasePosition(orbitDistanceLimitsForBasePosition);
	const float baseOrbitDistanceToApply	= Clamp(baseOrbitDistance, orbitDistanceLimitsForBasePosition.x, orbitDistanceLimitsForBasePosition.y);
	m_BasePosition							= m_BasePivotPosition - (m_BaseFront * baseOrbitDistanceToApply);

	UpdateLookAt(orbitFront);

	UpdateRoll(orbitMatrix); 

	UpdateLensParameters();

	CacheOrbitHeadingLimitSettings(orbitHeading);

	UpdateDistanceTravelled();

	if(m_CatchUpHelper)
	{
		m_CatchUpHelper->PostUpdate();
	}

	return true;
}

void camThirdPersonCamera::UpdateControlHelper()
{
	//NOTE: m_ControlHelper has already been validated.

	camGameplayDirector& gameplayDirector	= camInterface::GetGameplayDirector();
	const bool isActiveGameplayCamera		= gameplayDirector.IsActiveCamera(this);
	if(isActiveGameplayCamera)
	{
		//NOTE: Only the active gameplay camera is allowed to update the view mode.
		m_ControlHelper->SetShouldUpdateViewModeThisUpdate();

		const bool isInterpolating = IsInterpolating();
		if(isInterpolating)
		{
			//Inhibit view-mode changes to prevent bouncy interpolation behaviour.
			m_ControlHelper->IgnoreViewModeInputThisUpdate();
		}
	}

	if(gameplayDirector.GetSwitchHelper() || (m_HintHelper && (gameplayDirector.GetHintBlend() >= SMALL_FLOAT)))
	{
		//Inhibit look-behind during Switch behaviour and whilst a hint is influencing the orientation.
		m_ControlHelper->SetLookBehindState(false);
		m_ControlHelper->IgnoreLookBehindInputThisUpdate();
	}

	const bool isRenderingCinematicCamera = camInterface::GetCinematicDirector().IsRendering();
	if(isRenderingCinematicCamera)
	{
		//Ignore look-around input whenever a cinematic camera is rendering to avoid confusing the user.
		m_ControlHelper->IgnoreLookAroundInputThisUpdate();
	}

	const bool wasRenderingThisCameraOnPreviousUpdate	= camInterface::IsRenderingCamera(*this);
	const bool shouldSkipViewModeBlend					= (!wasRenderingThisCameraOnPreviousUpdate || (m_NumUpdatesPerformed == 0) ||
															m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
															m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation));
	if(shouldSkipViewModeBlend)
	{
		m_ControlHelper->SkipViewModeBlendThisUpdate();
	}

	//NOTE: m_AttachParent has already been validated.
	m_ControlHelper->Update(*m_AttachParent);

	const bool wasLookingBehind	= m_ControlHelper->WasLookingBehind();
	const bool isLookingBehind	= m_ControlHelper->IsLookingBehind();
	if(isLookingBehind != wasLookingBehind && m_NumUpdatesPerformed > 0)
	{
		//Ensure that any interpolation is stopped immediately if the look-behind state is flipped, preventing flipping within the interpolation.
		StopInterpolating();

		m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation);
	}

	if(isLookingBehind)
	{
		//Inhibit catch-up when looking behind.
		AbortCatchUp();
	}

	STATS_ONLY(const float* viewModeBlendLevels = m_ControlHelper->GetViewModeBlendLevels();)
	PF_SET(nearThirdPersonViewModeBlendLevel, viewModeBlendLevels[camControlHelperMetadataViewMode::THIRD_PERSON_NEAR]);
	PF_SET(mediumThirdPersonViewModeBlendLevel, viewModeBlendLevels[camControlHelperMetadataViewMode::THIRD_PERSON_MEDIUM]);
	PF_SET(farThirdPersonViewModeBlendLevel, viewModeBlendLevels[camControlHelperMetadataViewMode::THIRD_PERSON_FAR]);
}

void camThirdPersonCamera::FlagCutsForOrbitOverrides()
{
	//NOTE: We do not report a cut for a relative orbit orientation change on the first update, as this interface is used to clone base orientation.
	if(m_NumUpdatesPerformed == 0)
	{
		return;
	}

	//NOTE: m_ControlHelper has already been validated.
	const bool isLookAroundInputPresent	= m_ControlHelper->IsLookAroundInputPresent();
	const bool isCuttingOrbitHeading	= (m_ShouldApplyRelativeHeading && m_ShouldFlagRelativeHeadingChangeAsCut &&
											(m_ShouldIgnoreInputForRelativeHeading || !isLookAroundInputPresent) &&
											(m_RelativeHeadingSmoothRate >= (1.0f - SMALL_FLOAT)));
	const bool isCuttingOrbitPitch		= (m_ShouldApplyRelativePitch && m_ShouldFlagRelativePitchChangeAsCut &&
											(m_ShouldIgnoreInputForRelativePitch || !isLookAroundInputPresent) &&
											(m_RelativePitchSmoothRate >= (1.0f - SMALL_FLOAT)));
	if(isCuttingOrbitHeading || isCuttingOrbitPitch)
	{
		m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation);
	}
}

void camThirdPersonCamera::CreateOrDestroyHintHelper()
{
	if(camInterface::GetGameplayDirector().IsHintActive())
	{
		if(!m_HintHelper)
		{
			const camHintHelperMetadata* hintHelperMetadata =
				camFactory::FindObjectMetadata<camHintHelperMetadata>(m_Metadata.m_HintHelperRef.GetHash());

			if(hintHelperMetadata)
			{
				//NOTE: We only allow the hint helper to be overridden if we already have a valid hint helper defined for this camera.
				const atHashWithStringNotFinal& overriddenHintHelperRef = camInterface::GetGameplayDirector().GetHintHelperToCreate();
				if(overriddenHintHelperRef.GetHash() > 0)
				{
					const camHintHelperMetadata* overriddenHintHelperMetadata =
						camFactory::FindObjectMetadata<camHintHelperMetadata>(overriddenHintHelperRef.GetHash());

					if(cameraVerifyf(overriddenHintHelperMetadata,
						"A third-person camera (name: %s, hash: %u) failed to find the metadata for an overridden hint helper (name: %s, hash: %u)",
						GetName(), GetNameHash(), SAFE_CSTRING(overriddenHintHelperRef.GetCStr()), overriddenHintHelperRef.GetHash()))
					{
						hintHelperMetadata = overriddenHintHelperMetadata;
					}
				}

				m_HintHelper = camFactory::CreateObject<camHintHelper>(*hintHelperMetadata);
				cameraAssertf(m_HintHelper, "A third-person camera (name: %s, hash: %u) failed to create a hint helper (name: %s, hash: %u)",
					GetName(), GetNameHash(), SAFE_CSTRING(hintHelperMetadata->m_Name.GetCStr()), hintHelperMetadata->m_Name.GetHash());
			}
		}
	}
	else if(m_HintHelper)
	{
		delete m_HintHelper; 
		m_HintHelper = NULL; 
	}
}

void camThirdPersonCamera::UpdateAttachParentMatrix()
{
	//Cache the orientation from the previous update, as this is used to persist relative orientation.
	m_AttachParentOrientationOnPreviousUpdate.FromMatrix34(m_AttachParentMatrix);

	//NOTE: m_AttachParent has already been validated.
	m_AttachParentMatrix = MAT34V_TO_MATRIX34(m_AttachParent->GetMatrix());

	if(m_AttachParent->GetIsTypePed())
	{
		if(NetworkInterface::IsGameInProgress())
		{
			const CPed* attachPed = static_cast<const CPed*>(m_AttachParent.Get());
			if( attachPed->IsPlayer() && attachPed->IsNetworkClone())
			{
				//Must be spectating a player
				CNetObjPlayer const* attachNetObjPlayer = static_cast<CNetObjPlayer*>(attachPed->GetNetworkObject());

				if(attachNetObjPlayer->WasHiddenForTutorial())
				{
					Vector3 vCloneExpectedPosition		= NetworkInterface::GetLastPosReceivedOverNetwork(attachPed);
					m_AttachParentMatrix.d = vCloneExpectedPosition;
				}
			}
		}

		//Apply any rotation that has been applied separately to the attach ped's bound.
		const CPed* attachPed	= (const CPed*)m_AttachParent.Get();
		const float boundPitch	= attachPed->GetBoundPitch();

		m_AttachParentMatrix.RotateLocalX(boundPitch);
	}
}

void camThirdPersonCamera::UpdateAttachParentMatrixToConsiderForRelativeOrbit()
{
	//Blend between the full matrix of the attach parent and an identity 3x3 part.

	const u32 gameTime = fwTimer::GetTimeInMilliseconds();

	const bool shouldApplyFullAttachParentMatrixOnPreviousUpdate	= m_ShouldApplyFullAttachParentMatrixForRelativeOrbit;
	m_ShouldApplyFullAttachParentMatrixForRelativeOrbit				= ComputeShouldApplyFullAttachParentMatrixForRelativeOrbit();
	if(!m_ShouldApplyFullAttachParentMatrixForRelativeOrbit)
	{
		m_MostRecentTimeFullAttachParentMatrixBlockedForRelativeOrbit = gameTime;
	}
	else if(m_ShouldResetFullAttachParentMatrixForRelativeOrbitTimer)
	{
		//script wants to reset the timer.
		m_MostRecentTimeFullAttachParentMatrixBlockedForRelativeOrbit = 0;
	}
	else if(gameTime < (m_MostRecentTimeFullAttachParentMatrixBlockedForRelativeOrbit + m_Metadata.m_MinHoldTimeToBlockFullAttachParentMatrixForRelativeOrbit) && !(m_ShouldApplyRelativeHeading || m_ShouldApplyRelativePitch))
	{
		//Continue blocking until the hold time has elapsed, for improved continuity.
		m_ShouldApplyFullAttachParentMatrixForRelativeOrbit = false;
	}

	m_ShouldResetFullAttachParentMatrixForRelativeOrbitTimer = false;

	const float desiredBlendLevel	= m_ShouldApplyFullAttachParentMatrixForRelativeOrbit ? 1.0f : 0.0f;
	const bool shouldCut			= ((m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
										m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation));
	const float springConstant		= shouldCut ? 0.0f : GetAttachParentMatrixForRelativeOrbitSpringConstant();

	float blendLevelToApply			= m_AttachParentMatrixForRelativeOrbitSpring.Update(desiredBlendLevel, springConstant,
										GetAttachParentMatrixForRelativeOrbitSpringDampingRatio());

	//Don't overshoot the target blend level.
	blendLevelToApply = Clamp(blendLevelToApply, 0.0f, 1.0f);
	m_AttachParentMatrixForRelativeOrbitSpring.OverrideResult(blendLevelToApply);

	const bool shouldMaintainDirectionOfBlend	= (m_NumUpdatesPerformed > 0) &&
													(m_ShouldApplyFullAttachParentMatrixForRelativeOrbit == shouldApplyFullAttachParentMatrixOnPreviousUpdate);

	camFrame::SlerpOrientation(blendLevelToApply, M34_IDENTITY, m_AttachParentMatrix, m_AttachParentMatrixToConsiderForRelativeOrbit,
		shouldMaintainDirectionOfBlend, &m_PreviousAttachParentOrientationDeltaForRelativeOrbit);

	m_AttachParentMatrixToConsiderForRelativeOrbit.d = m_AttachParentMatrix.d;
}

bool camThirdPersonCamera::ComputeShouldApplyFullAttachParentMatrixForRelativeOrbit() const
{
// 	TUNE_BOOL(shouldForceDisableFullAttachParentMatrixForRelativeOrbit, false)
// 	if(shouldForceDisableFullAttachParentMatrixForRelativeOrbit)
// 	{
// 		return false;
// 	}

	//NOTE: m_AttachParent has already been validated.
	if(m_AttachParent->GetIsTypeVehicle())
	{
		//Prevent the full attach parent matrix being applied if the attach vehicle is wrecked, crashing, submerged in water or missing a wing/tail.
		//Otherwise, we can observe very unstable camera movement.

		const CVehicle* attachVehicle	= static_cast<const CVehicle*>(m_AttachParent.Get());
		const s32 attachVehicleStatus	= attachVehicle->GetStatus();
		if(attachVehicleStatus == STATUS_WRECKED)
		{
			return false;
		}

		const CVehicleIntelligence* attachVehicleIntelligence = attachVehicle->GetIntelligence();
		if(attachVehicleIntelligence)
		{
			const CTaskManager* taskManager = attachVehicleIntelligence->GetTaskManager();
			if(taskManager)
			{
				if(taskManager->FindTaskByTypeActive(VEHICLE_TASK_TREE_PRIMARY, CTaskTypes::TASK_VEHICLE_CRASH) != NULL)
				{
					return false;
				}
			}
		}

		const bool attachVehicleInheritsFromPlane = attachVehicle->InheritsFromPlane();

		if(attachVehicle->m_nPhysicalFlags.bPossiblyTouchesWaterIsUpToDate)
		{
			const CBuoyancy& buoyancyHelper = attachVehicle->m_Buoyancy;
			if(buoyancyHelper.GetBuoyancyInfoUpdatedThisFrame())
			{
				const s32 buoyancyStatus = buoyancyHelper.GetStatus();
				if(buoyancyStatus != NOT_IN_WATER)
				{
					//NOTE: The submerged level is only updated if the buoyancy helper detects water.
					const float submergedLevel							= buoyancyHelper.GetSubmergedLevel();
					const float maxAttachParentSubmergedLevelToConsider	= attachVehicleInheritsFromPlane ? g_MaxAttachParentSubmergedLevelToApplyFullAttachParentMatrixForRelativeOrbitForPlanes :
																			m_Metadata.m_MaxAttachParentSubmergedLevelToApplyFullAttachParentMatrixForRelativeOrbit;
					if(submergedLevel > maxAttachParentSubmergedLevelToConsider)
					{
						return false;
					}
				}
			}
		}

		if(attachVehicleInheritsFromPlane && !(m_ShouldApplyRelativeHeading || m_ShouldApplyRelativePitch))
		{
			const CPlane* attachPlane					= static_cast<const CPlane *>(attachVehicle);
			const CAircraftDamage& attachPlaneDamage	= attachPlane->GetAircraftDamage();

			if(attachPlaneDamage.HasSectionBrokenOff(attachPlane, CAircraftDamage::WING_L) ||
				attachPlaneDamage.HasSectionBrokenOff(attachPlane, CAircraftDamage::WING_R) ||
				attachPlaneDamage.HasSectionBrokenOff(attachPlane, CAircraftDamage::TAIL))
			{
				return false;
			}

			const bool isVehicleRecording = CVehicleRecordingMgr::IsPlaybackGoingOnForCar(attachVehicle);
			if(attachPlane->IsAsleep() && !isVehicleRecording)
			{
				return false;
			}

			const Vector3& attachPlaneVelocity	= attachPlane->GetVelocity();
			const float attachPlaneSpeed2		= attachPlaneVelocity.Mag2();

			const bool isInAir = const_cast<CPlane*>(attachPlane)->IsInAir(false);
			if(isInAir)
			{
				//Check for the attach plane landing other than on its wheels, and hence reporting that it's in the air.

				if(attachPlaneSpeed2 < m_Metadata.m_MinAircraftContactSpeedToApplyFullAttachParentMatrixForRelativeOrbit *
					m_Metadata.m_MinAircraftContactSpeedToApplyFullAttachParentMatrixForRelativeOrbit)
				{
					const CCollisionHistory* collisionHistory = attachPlane->GetFrameCollisionHistory();
					if(collisionHistory && (collisionHistory->GetNumCollidedEntities() > 0))
					{
						return false;
					}
				}
			}
			else if(attachPlaneSpeed2 < m_Metadata.m_MinAircraftGroundSpeedToApplyFullAttachParentMatrixForRelativeOrbit *
				m_Metadata.m_MinAircraftGroundSpeedToApplyFullAttachParentMatrixForRelativeOrbit)
			{
				return false;
			}
		}
	}

	return true;
}

bool camThirdPersonCamera::UpdateAttachParentBoundingBox()
{
	m_BoundingBoxExtensionForOrbitDistance	= 0.0f;
	m_OrbitDistanceExtensionForTowedVehicle	= 0.0f;

	//NOTE: m_AttachParent has already been validated.
	const CBaseModelInfo* baseModelInfo = m_AttachParent->GetBaseModelInfo();
	if(baseModelInfo)
	{
		m_AttachParentBoundingBoxMin = baseModelInfo->GetBoundingBoxMin();
		m_AttachParentBoundingBoxMax = baseModelInfo->GetBoundingBoxMax();
		
		float HeightScaling = m_Metadata.m_CustomBoundingBoxSettings.m_HeightScaling; 
		
		if(m_CustomVehicleSettings)
		{
			if(m_CustomVehicleSettings->m_AdditionalBoundingScaling.m_ShouldConsiderData)
			{
				HeightScaling = HeightScaling * m_CustomVehicleSettings->m_AdditionalBoundingScaling.m_HeightScaling; 
			}
		}
		
		if(!IsClose(HeightScaling, 1.0f))
		{
			const float parentHeight		= m_AttachParentBoundingBoxMax.z - m_AttachParentBoundingBoxMin.z;
			const float heightOffsetToApply	= parentHeight * (HeightScaling - 1.0f);
			m_AttachParentBoundingBoxMax.z	= Max(m_AttachParentBoundingBoxMax.z + heightOffsetToApply, 0.0f);
		}

		//If an attach vehicle is pulling a trailer, we need to take account of its size.
		if(m_AttachParent->GetIsTypeVehicle())
		{
			const CVehicle* attachVehicle	= (const CVehicle*)m_AttachParent.Get();
			const CTrailer* trailer			= static_cast<const CTrailer*>(camInterface::GetGameplayDirector().GetAttachedTrailer());
			if(trailer)
			{
				s32 attachVehicleAttachBoneIndex	= attachVehicle->GetBoneIndex(VEH_ATTACH);
				s32 trailerAttachBoneIndex			= trailer->GetBoneIndex(TRAILER_ATTACH);

				if((attachVehicleAttachBoneIndex != -1) && (trailerAttachBoneIndex != -1))
				{
					const Matrix34& attachVehicleAttachBoneMatrix	= attachVehicle->GetLocalMtx(attachVehicleAttachBoneIndex);
					const Matrix34& trailerAttachBoneMatrix			= trailer->GetLocalMtx(trailerAttachBoneIndex);

					CBaseModelInfo* trailerBaseModelInfo = trailer->GetBaseModelInfo();
					if(trailerBaseModelInfo)
					{
						const Vector3& trailerBoundingBoxMin	= trailerBaseModelInfo->GetBoundingBoxMin();
						const Vector3& trailerBoundingBoxMax	= trailerBaseModelInfo->GetBoundingBoxMax();

						//Compute the additional length presented by the trailer, being careful to exclude any overlap in the attachment.
						//This value is used later in the update.
						m_BoundingBoxExtensionForOrbitDistance	= trailerAttachBoneMatrix.d.y - trailerBoundingBoxMin.y -
							(attachVehicleAttachBoneMatrix.d.y - m_AttachParentBoundingBoxMin.y);
						m_BoundingBoxExtensionForOrbitDistance *= camInterface::GetGameplayDirector().GetTrailerBlendLevel();

						//Now deal with with any additional height.
						const float attachVehicleHeightAboveAttachPoint	= m_AttachParentBoundingBoxMax.z - attachVehicleAttachBoneMatrix.d.z;
						float trailerHeightAboveAttachPoint				= trailerBoundingBoxMax.z - trailerAttachBoneMatrix.d.z;

						//Also factor in the height of the trailer's immediate child attachments.
						const fwEntity* childAttachment = trailer->GetChildAttachment();
						while(childAttachment)
						{
							Vector3 highestTrailerRelativePositionOnChild;
							const bool isChildValid	= ((!static_cast<const CEntity*>(childAttachment)->GetIsTypePed()) && 
														ComputeHighestPositionOnEntityRelativeToOtherEntity(static_cast<const CEntity&>(*childAttachment),
														*trailer, highestTrailerRelativePositionOnChild));
							if(isChildValid)
							{
								const float childHeightAboveTrailerAttachPoint = highestTrailerRelativePositionOnChild.z - trailerAttachBoneMatrix.d.z;
								if(childHeightAboveTrailerAttachPoint > trailerHeightAboveAttachPoint)
								{
									trailerHeightAboveAttachPoint = childHeightAboveTrailerAttachPoint;
								}
							}

							childAttachment = childAttachment->GetSiblingAttachment();
						}

						float extraHeight				= trailerHeightAboveAttachPoint - attachVehicleHeightAboveAttachPoint;
						extraHeight						= Clamp(extraHeight, 0.0f,
															m_Metadata.m_CustomBoundingBoxSettings.m_MaxExtraHeightForVehicleTrailers);
						m_AttachParentBoundingBoxMax.z	+= camInterface::GetGameplayDirector().GetTrailerBlendLevel() * extraHeight;
					}
				}
			}
			else
			{
				const CVehicle* towedVehicle = camInterface::GetGameplayDirector().GetTowedVehicle();
				if(towedVehicle)
				{
					const CBaseModelInfo* towedVehicleBaseModelInfo = towedVehicle->GetBaseModelInfo();
					if(towedVehicleBaseModelInfo)
					{
						const Vector3& towedVehicleBoundingBoxMin	= towedVehicleBaseModelInfo->GetBoundingBoxMin();
						const Vector3& towedVehicleBoundingBoxMax	= towedVehicleBaseModelInfo->GetBoundingBoxMax();

						const Vector3 attachVehiclePosition		= VEC3V_TO_VECTOR3(attachVehicle->GetTransform().GetPosition());
						const Vector3 towedVehiclePosition		= VEC3V_TO_VECTOR3(towedVehicle->GetTransform().GetPosition());
						const float distanceBetweenVehicles		= attachVehiclePosition.Dist(towedVehiclePosition);

						m_OrbitDistanceExtensionForTowedVehicle	= distanceBetweenVehicles + Max(-towedVehicleBoundingBoxMin.y, towedVehicleBoundingBoxMax.y) +
																	m_AttachParentBoundingBoxMin.y;
						const float towedVehicleBlendLevel		= camInterface::GetGameplayDirector().GetTowedVehicleBlendLevel();
						m_OrbitDistanceExtensionForTowedVehicle	*= towedVehicleBlendLevel;
						m_OrbitDistanceExtensionForTowedVehicle	= Max(m_OrbitDistanceExtensionForTowedVehicle, 0.0f);

						const float attachVehicleHeight	= m_AttachParentBoundingBoxMax.z - m_AttachParentBoundingBoxMin.z;
						const float towedVehicleHeight	= towedVehicleBoundingBoxMax.z - towedVehicleBoundingBoxMin.z;
						float extraHeight				= towedVehicleHeight - attachVehicleHeight;
						if(extraHeight >= SMALL_FLOAT)
						{
							extraHeight						= Min(extraHeight, m_Metadata.m_CustomBoundingBoxSettings.m_MaxExtraHeightForTowedVehicles);
							extraHeight						*= towedVehicleBlendLevel;
							m_AttachParentBoundingBoxMax.z	+= extraHeight;
						}
					}
				}
			}

			//Ensure that the top of the bounding box is a minimum distance above the driver's seat. This takes account of vehicles where the
			//driver's body penetrates the top of the vehicle bounding box.
			const CVehicleModelInfo* vehicleModelInfo	= attachVehicle->GetVehicleModelInfo();
			const CModelSeatInfo* seatInfo				= vehicleModelInfo ? vehicleModelInfo->GetModelSeatInfo() : NULL;
			const s32 driverSeatBone					= seatInfo ? seatInfo->GetBoneIndexFromSeat(seatInfo->GetDriverSeat()) : -1;
			if(driverSeatBone != -1 && attachVehicle->GetSkeleton())
			{
				const Matrix34& localBoneMatrix			= RCC_MATRIX34(attachVehicle->GetSkeleton()->GetLocalMtx(driverSeatBone));
				const float boundingBoxZForSeatOffset	= localBoneMatrix.d.z +
															m_Metadata.m_CustomBoundingBoxSettings.m_MinHeightAboveVehicleDriverSeat;
				m_AttachParentBoundingBoxMax.z			= Max(m_AttachParentBoundingBoxMax.z, boundingBoxZForSeatOffset);
			}
		}
	}

	return (baseModelInfo != NULL);
}

bool camThirdPersonCamera::ComputeHighestPositionOnEntityRelativeToOtherEntity(const CEntity& entity, const CEntity& otherEntity,
	Vector3& highestRelativePosition) const
{
	const CBaseModelInfo* baseModelInfo = entity.GetBaseModelInfo();
	if(!baseModelInfo)
	{
		return false;
	}

	const Vector3& boundingBoxMin = baseModelInfo->GetBoundingBoxMin();
	const Vector3& boundingBoxMax = baseModelInfo->GetBoundingBoxMax();

	//Extract the bounding box verts.
	const Vector3 boundingBoxVerts[8] =
	{
		Vector3(boundingBoxMax.x, boundingBoxMax.y, boundingBoxMax.z),
		Vector3(boundingBoxMin.x, boundingBoxMax.y, boundingBoxMax.z),
		Vector3(boundingBoxMax.x, boundingBoxMin.y, boundingBoxMax.z),
		Vector3(boundingBoxMin.x, boundingBoxMin.y, boundingBoxMax.z),
		Vector3(boundingBoxMax.x, boundingBoxMax.y, boundingBoxMin.z),
		Vector3(boundingBoxMin.x, boundingBoxMax.y, boundingBoxMin.z),
		Vector3(boundingBoxMax.x, boundingBoxMin.y, boundingBoxMin.z),
		Vector3(boundingBoxMin.x, boundingBoxMin.y, boundingBoxMin.z)
	};

	highestRelativePosition.SetScaled(ZAXIS, -LARGE_FLOAT);

	for(int i=0; i<8; i++)
	{
		//Transform the local-space verts into world-space and then back into the local-space of the other entity.
		const fwTransform& enityTransform		= entity.GetTransform();
		const Vec3V worldPosition				= enityTransform.Transform(VECTOR3_TO_VEC3V(boundingBoxVerts[i]));
		const fwTransform& otherEntityTransform	= otherEntity.GetTransform();
		const Vector3 relativePosition			= VEC3V_TO_VECTOR3(otherEntityTransform.UnTransform(worldPosition));

		if(relativePosition.z > highestRelativePosition.z)
		{
			highestRelativePosition = relativePosition;
		}
	}

	return true;
}

void camThirdPersonCamera::UpdateAttachPedPelvisOffsetSpring()
{
	//NOTE: m_AttachParent has already been validated.
	if(!m_AttachParent->GetIsTypePed() || !m_Metadata.m_ShouldApplyAttachPedPelvisOffset)
	{
		m_AttachPedPelvisOffsetSpring.Reset();
		return;
	}

	const bool shouldForceAttachPedPelvisOffsetToBeConsidered = ComputeShouldForceAttachPedPelvisOffsetToBeConsidered();

	const CPed* attachPed		= static_cast<const CPed*>(m_AttachParent.Get());
	const float desiredOffset	= attachPed->GetIkManager().GetPelvisDeltaZForCamera(shouldForceAttachPedPelvisOffsetToBeConsidered);
	const bool shouldCut		= ((m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
									m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation));

	m_AttachPedPelvisOffsetSpring.Update(desiredOffset, shouldCut ? 0.0f : m_Metadata.m_AttachPedPelvisOffsetSpringConstant,
		m_Metadata.m_AttachPedPelvisOffsetSpringDampingRatio);

	PF_SET(desiredAttachPedPelvisOffset, desiredOffset);
	PF_SET(dampedAttachPedPelvisOffset, m_AttachPedPelvisOffsetSpring.GetResult());
}

void camThirdPersonCamera::UpdateBaseAttachPosition()
{
	m_BaseAttachPositionOnPreviousUpdate = m_BaseAttachPosition;

	//Simply attach to the parent position by default.
	m_BaseAttachPosition = m_AttachParentMatrix.d;

	if(m_Metadata.m_ShouldAttachToParentCentreOfGravity)
	{
		if(m_Metadata.m_ShouldUseDynamicCentreOfGravity)
		{
			//NOTE: m_AttachParent has already been validated.
			phInst* inst = m_AttachParent->GetCurrentPhysicsInst();
			if(inst)
			{
				const phArchetype* archetype = inst->GetArchetype();
				if(archetype)
				{
					const phBound* bound = archetype->GetBound();
					if(bound)
					{
						const Vector3 relativeCentreOfGravity = VEC3V_TO_VECTOR3(bound->GetCGOffset());
						m_AttachParentMatrix.Transform(relativeCentreOfGravity, m_BaseAttachPosition); //Transform to world space.
					}
				}
			}
		}
		else
		{
			//Extract the static authored centre of gravity from the model info.

			//NOTE: m_AttachParent has already been validated.
			const CBaseModelInfo* modelInfo = m_AttachParent->GetBaseModelInfo();
			if(modelInfo)
			{
				const gtaFragType* fragType = modelInfo->GetFragType();
				if(fragType)
				{
					const Vector3 relativeCentreOfGravity = fragType->GetPhysics(0)->GetRootCGOffset();
					m_AttachParentMatrix.Transform(relativeCentreOfGravity, m_BaseAttachPosition); //Transform to world space.
				}
			}
		}
	}
}

void camThirdPersonCamera::GetAttachParentVelocity(Vector3& velocity) const
{
	Vector3 baseAttachVelocity(VEC3_ZERO);
	if(m_BaseAttachPositionOnPreviousUpdate != g_InvalidPosition)
	{
		const float gameInvTimeStep	= fwTimer::GetInvTimeStep();
		baseAttachVelocity			= (m_BaseAttachPosition - m_BaseAttachPositionOnPreviousUpdate) * gameInvTimeStep;
	}

	velocity = baseAttachVelocity;
}

void camThirdPersonCamera::UpdateBaseAttachVelocities()
{
	Vector3 baseAttachVelocity;
	GetAttachParentVelocity(baseAttachVelocity);

	//Ignore the movement of any entity the attach parent attached to or supported by, as we don't want this to affect the behavior of the camera.
	const CEntity* attachOrGroundEntity = NULL;
	if(m_Metadata.m_ShouldIgnoreVelocityOfAttachParentAttachEntity)
	{
		attachOrGroundEntity = ComputeAttachParentAttachEntity();
	}

	//If we haven't already identified an attach entity, use any 'ground' entity.
	bool isGroundEntity = false;
	if(!attachOrGroundEntity)
	{
		attachOrGroundEntity	= ComputeAttachParentGroundEntity();
		isGroundEntity			= (attachOrGroundEntity != NULL);
	}

	//Ignore the non-zero local velocity that is lowest in the attachment hierarchy.
	bool shouldIgnoreVelocityOfAttachOrGroundEntity = false;
	while(attachOrGroundEntity)
	{
		//NOTE: We only query the local velocity of vehicular ground physicals. Other types can report spurious local velocities that do
		//not translate into equivalent movement of our attach parent.
		if(attachOrGroundEntity->GetIsPhysical() && (!isGroundEntity || attachOrGroundEntity->GetIsTypeVehicle()))
		{
			const CPhysical* attachOrGroundPhysical		= static_cast<const CPhysical*>(attachOrGroundEntity);
			const Vector3 localVelocity					= attachOrGroundPhysical->GetLocalSpeed(m_BaseAttachPosition, true);
			shouldIgnoreVelocityOfAttachOrGroundEntity	= (localVelocity.Mag2() >= VERY_SMALL_FLOAT);
			if(shouldIgnoreVelocityOfAttachOrGroundEntity)
			{
				m_BaseAttachVelocityToIgnoreForMostRecentEntity = localVelocity;
				break;
			}
		}

		attachOrGroundEntity = static_cast<const CEntity*>(attachOrGroundEntity->GetAttachParent());
	}

	const bool bIsUsingJetpack = m_AttachParent->GetIsTypePed() && static_cast<const CPed*>(m_AttachParent.Get())->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack);
	if (bIsUsingJetpack)
	{
		shouldIgnoreVelocityOfAttachOrGroundEntity = false;
	}

	float blendLevel;
	if(m_BaseAttachVelocityToIgnoreEnvelope)
	{
		if(m_NumUpdatesPerformed == 0)
		{
			if(shouldIgnoreVelocityOfAttachOrGroundEntity)
			{
				m_BaseAttachVelocityToIgnoreEnvelope->Start(1.0f);
			}
			else
			{
				m_BaseAttachVelocityToIgnoreEnvelope->Stop();
			}
		}
		else
		{
			if (!shouldIgnoreVelocityOfAttachOrGroundEntity &&
				m_BaseAttachVelocityToIgnoreEnvelope->IsActive() &&
				m_AttachParent->GetIsTypePed() &&
				static_cast<const CPed*>(m_AttachParent.Get())->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInTheAir) &&
				!bIsUsingJetpack)
			{
				m_BaseAttachVelocityToIgnoreEnvelope->AutoStart();
			}
			else
			{
				m_BaseAttachVelocityToIgnoreEnvelope->AutoStartStop(shouldIgnoreVelocityOfAttachOrGroundEntity);
			}
		}

		blendLevel = m_BaseAttachVelocityToIgnoreEnvelope->Update();
		blendLevel = Clamp(blendLevel, 0.0f, 1.0f);
		blendLevel = SlowInOut(blendLevel);
	}
	else
	{
		//We don't have an envelope, so just use the input state directly.
		blendLevel = shouldIgnoreVelocityOfAttachOrGroundEntity ? 1.0f : 0.0f;
	}

	m_BaseAttachVelocityToIgnore.SetScaled(m_BaseAttachVelocityToIgnoreForMostRecentEntity, blendLevel);

	m_BaseAttachVelocityToConsider = baseAttachVelocity - m_BaseAttachVelocityToIgnore;
}

const CEntity* camThirdPersonCamera::ComputeAttachParentAttachEntity() const
{
	//Default to the game-level attach parent.
	//NOTE: m_AttachParent has already been validated.
	const CEntity* attachEntity = static_cast<const CEntity*>(m_AttachParent->GetAttachParent());

	//Treat any entity being climbed upon as the highest priority for an attach ped.
	if(m_AttachParent->GetIsTypePed())
	{
		const CPed* attachPed = static_cast<const CPed*>(m_AttachParent.Get());
		if(!attachPed->IsNetworkClone())
		{
			const CPedIntelligence* pedIntelligence = attachPed->GetPedIntelligence();
			if(pedIntelligence && pedIntelligence->IsPedClimbing())
			{
				const CTaskVault* climbTask = static_cast<const CTaskVault*>(pedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_VAULT));
				if(climbTask)
				{
					const CPhysical* handHoldPhysical = climbTask->GetHandHold().GetPhysical();
					if(handHoldPhysical)
					{
						attachEntity = handHoldPhysical;
					}
				}
			}
		}
	}

	return attachEntity;
}

const CEntity* camThirdPersonCamera::ComputeAttachParentGroundEntity() const
{
	const CEntity* groundEntity = NULL;

	//NOTE: m_AttachParent has already been validated.
	if(m_AttachParent->GetIsTypePed())
	{
		const CPed* attachPed	= static_cast<const CPed*>(m_AttachParent.Get());
		groundEntity			= attachPed->GetGroundPhysical();
	}
	else if(m_AttachParent->GetIsTypeVehicle())
	{
		//TODO: Deal with the wheels contacting more than one physical.
		const CVehicle* attachVehicle	= static_cast<const CVehicle*>(m_AttachParent.Get());
		const int numWheels				= attachVehicle->GetNumWheels();
		for(int i=0; i<numWheels; i++)
		{
			const CWheel* wheel = attachVehicle->GetWheel(i);
			if(wheel)
			{
				const CPhysical* hitPhysical = wheel->GetHitPhysical();
				if(hitPhysical)
				{
					groundEntity = hitPhysical;
					break;
				}
			}
		}
	}

	return groundEntity;
}

void camThirdPersonCamera::UpdateTightSpaceSpring()
{
	const bool wasRenderingThisCameraOnPreviousUpdate	= camInterface::IsRenderingCamera(*this);
	const bool shouldCut								= (!wasRenderingThisCameraOnPreviousUpdate || (m_NumUpdatesPerformed == 0) ||
															m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
															m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation));
	if(!shouldCut)
	{
		const float baseAttachSpeed2	= m_BaseAttachVelocityToConsider.Mag2();
		const bool shouldUpdate			= (baseAttachSpeed2 >= (m_Metadata.m_MinAttachSpeedToUpdateTightSpaceLevel *
											m_Metadata.m_MinAttachSpeedToUpdateTightSpaceLevel));
		if(!shouldUpdate)
		{
			return;
		}
	}

	const float desiredLevel	= camInterface::GetGameplayDirector().GetTightSpaceLevel();
	const float springConstant	= shouldCut ? 0.0f : m_Metadata.m_TightSpaceSpringConstant;

	m_TightSpaceSpring.Update(desiredLevel, springConstant, m_Metadata.m_TightSpaceSpringDampingRatio);
	m_TightSpaceHeightSpring.Update(camInterface::GetGameplayDirector().IsFallingBackToThirdPersonForDirectorBlendCatchUpOrSwitch() ? 0.0f : desiredLevel, springConstant, m_Metadata.m_TightSpaceSpringDampingRatio);
}

void camThirdPersonCamera::UpdateStealthZoom()
{
	const camThirdPersonCameraMetadataStealthZoomSettings& settings = m_Metadata.m_StealthZoomSettings;
	if(!settings.m_ShouldApply)
	{
		m_StealthZoomSpring.Reset(1.0f);

		return;
	}

	bool isAttachPedUsingStealth = false;

	if(m_AttachParent && m_AttachParent->GetIsTypePed())
	{
		const CPed* attachPed						= static_cast<const CPed*>(m_AttachParent.Get());
		const CPedMotionData* attachPedMotionData	= attachPed->GetMotionData();
		isAttachPedUsingStealth						= (attachPedMotionData && attachPedMotionData->GetUsingStealth());
	}

	const float desiredZoomFactor = isAttachPedUsingStealth ? settings.m_MaxZoomFactor : 1.0f;

	const bool wasRenderingThisCameraOnPreviousUpdate	= camInterface::IsRenderingCamera(*this);
	const bool shouldCut								= (!wasRenderingThisCameraOnPreviousUpdate || (m_NumUpdatesPerformed == 0) ||
															m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
															m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation));
	const float springConstant							= shouldCut ? 0.0f : settings.m_SpringConstant;

	m_StealthZoomSpring.Update(desiredZoomFactor, springConstant, settings.m_SpringDampingRatio, true);
}

void camThirdPersonCamera::UpdateBasePivotPosition()
{
	m_BasePivotPosition = m_BaseAttachPosition;

	Vector3 vLocalBasePivotPosition;
	m_AttachParentMatrix.UnTransform(m_BasePivotPosition, vLocalBasePivotPosition); // Get current m_BasePivotPosition in parent's local space
	vLocalBasePivotPosition += m_Metadata.m_BasePivotPosition.m_RelativeOffset; // Apply offset in the local space

	m_AttachParentMatrix.Transform(vLocalBasePivotPosition, m_BasePivotPosition); //Transform local offset back to world space.

	const float baseAttachPositionOffsetForBasePivotPosition = ComputeBaseAttachPositionOffsetForBasePivotPosition();
	if(IsNearZero(baseAttachPositionOffsetForBasePivotPosition))
	{
		return;
	}

	if(!m_Metadata.m_BasePivotPosition.m_ShouldApplyInAttachParentLocalSpace &&
		!m_Metadata.m_BasePivotPosition.m_RollSettings.m_ShouldApplyAttachParentRoll)
	{
		m_BasePivotPosition.z += baseAttachPositionOffsetForBasePivotPosition;

		return;
	}

	Vector3 attachParentUpToConsider;
	if(m_Metadata.m_BasePivotPosition.m_ShouldApplyInAttachParentLocalSpace)
	{
		attachParentUpToConsider.Set(m_AttachParentMatrix.c);
	}
	else //if(rollSettings.m_ShouldApplyAttachParentRoll)
	{
		const float attachParentRollToApply = ComputeAttachParentRollForBasePivotPosition();

		Quaternion orientationToApplyForDampedRoll;
		orientationToApplyForDampedRoll.FromRotation(m_AttachParentMatrix.b, attachParentRollToApply);

		orientationToApplyForDampedRoll.Transform(ZAXIS, attachParentUpToConsider);
	}

	if(m_Metadata.m_BasePivotPosition.m_ShouldLockVerticalOffset)
	{
		attachParentUpToConsider.z = 1.0f;
	}

	Vector3 offsetToApply;
	offsetToApply.SetScaled(attachParentUpToConsider, baseAttachPositionOffsetForBasePivotPosition);

	m_BasePivotPosition += offsetToApply;
}

void camThirdPersonCamera::UpdateBasePivotSpring()
{
    //fetch the settings and check if we should apply the behavior
    const camThirdPersonCameraMetadataQuadrupedalHeightSpring& settings = m_Metadata.m_QuadrupedalHeightSpringSetting;
    if(!settings.m_ShouldApply || m_NumUpdatesPerformed == 0 )
    {
        m_BasePivotSpring.Reset( m_BasePivotPosition.z );
        return;
    }

    //check if the PED entity valid and is a QUADRUPED
    const CPed* pPedEntity = static_cast< const CPed* >(m_AttachParent.Get());
    if( ! ( pPedEntity && m_AttachParent->GetIsTypePed() && pPedEntity->GetPedType() == PEDTYPE_ANIMAL ) )
    {
        return;
    }

    //check the camera frame for cuts and in case set the spring constant to zero (this will reset the spring).
    const bool shouldCut = ( m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) || 
                             m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation) );
    const float springConstant = shouldCut ? 0.0f : settings.m_SpringConstant;

    //update the damped spring and assign the result back to the base pivot position.
    m_BasePivotSpring.Update( m_BasePivotPosition.z, springConstant, settings.m_SpringDampingRatio, true );

    const float delta = m_BasePivotSpring.GetResult() - m_BasePivotPosition.z;
    const float ratio = RampValueSafe( fabs(delta), settings.m_MaxHeightDistance * 0.5f, settings.m_MaxHeightDistance, 1.0f, 0.0f );

    m_BasePivotPosition.z += ratio * delta;
}

float camThirdPersonCamera::ComputeBaseAttachPositionOffsetForBasePivotPosition() const
{
	if(m_Metadata.m_BasePivotPosition.m_ShouldUseBaseAttachPosition)
	{
		return 0.0f;
	}

	Vector3 relativeBaseAttachPosition;
	m_AttachParentMatrix.UnTransform(m_BaseAttachPosition, relativeBaseAttachPosition); //Transform to parent-local space.

	const float parentHeight = m_AttachParentBoundingBoxMax.z - m_AttachParentBoundingBoxMin.z;

	const float attachParentHeightRatioToAttain = ComputeAttachParentHeightRatioToAttainForBasePivotPosition();

	float extraOffset					= m_AttachPedPelvisOffsetSpring.GetResult();
	const float extraOffsetForHighHeels	= ComputeAttachPedPelvisOffsetForShoes();
	extraOffset							+= extraOffsetForHighHeels;

	const float basePivotOffset			= (parentHeight * attachParentHeightRatioToAttain) +
											m_AttachParentBoundingBoxMin.z - relativeBaseAttachPosition.z + extraOffset;

	return basePivotOffset;
}

float camThirdPersonCamera::ComputeAttachParentHeightRatioToAttainForBasePivotPosition() const
{
	float attachParentHeightRatioToAttain;

	if(ShouldUseTightSpaceCustomFraming())
	{
		//Apply any scaling for tight spaces.
		const float blendLevel			= m_TightSpaceHeightSpring.GetResult();
		attachParentHeightRatioToAttain	= Lerp(blendLevel, m_Metadata.m_BasePivotPosition.m_AttachParentHeightRatioToAttain,
											m_Metadata.m_BasePivotPosition.m_AttachParentHeightRatioToAttainInTightSpace);
	}
	else
	{
		attachParentHeightRatioToAttain = m_Metadata.m_BasePivotPosition.m_AttachParentHeightRatioToAttain;
	}

	return attachParentHeightRatioToAttain;
}

float camThirdPersonCamera::ComputeAttachParentRollForBasePivotPosition()
{
	const camThirdPersonCameraMetadataBasePivotPositionRollSettings& rollSettings = m_Metadata.m_BasePivotPosition.m_RollSettings;

	const float attachParentRoll = camFrame::ComputeRollFromMatrix(m_AttachParentMatrix);

	//Rescale using Sine, such that the angles considered will peak at +/-90 degrees and diminish beyond this, which helps to reduce
	//instability when the attach parent barrel rolls.
	const float rescaledRoll	= HALF_PI * Sinf(attachParentRoll);

	//Scale the angles based upon the speed of the attach parent.
	const float attachParentSpeed	= m_BaseAttachVelocityToConsider.Mag();
	float desiredSpeedFactor		= RampValueSafe(attachParentSpeed, rollSettings.m_MinForwardSpeed,
										rollSettings.m_MaxForwardSpeed, 0.0f, 1.0f);
	desiredSpeedFactor				= SlowInOut(desiredSpeedFactor);

	const float scalingToApply	= desiredSpeedFactor * rollSettings.m_AngleScalingFactor;
	float rollToConsider		= rescaledRoll * scalingToApply;
	const float maxAngle		= rollSettings.m_MaxAngle * DtoR;
	rollToConsider				= Clamp(rollToConsider, -maxAngle, maxAngle);

	const bool shouldCut		= ((m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
									m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation));
	const float springConstant	= shouldCut ? 0.0f : rollSettings.m_SpringConstant;
	const float rollToApply		= m_BasePivotPositionRollSpring.Update(rollToConsider, springConstant, rollSettings.m_SpringDampingRatio);

	PF_SET(desiredAttachParentRoll, attachParentRoll);
	PF_SET(dampedAttachParentRoll, rollToApply);

	return rollToApply;
}

void camThirdPersonCamera::UpdateOrbitPitchLimits()
{
	m_OrbitPitchLimits.SetScaled(m_Metadata.m_OrbitPitchLimits, DtoR);
}

float camThirdPersonCamera::UpdateOrbitDistance()
{
	float desiredOrbitDistance = m_BasePivotPosition.Dist(m_BasePosition);

	if(m_CatchUpHelper)
	{
		m_CatchUpHelper->ComputeDesiredOrbitDistance(*this, desiredOrbitDistance);
	}

	//When the orbit distance springs have been initialised, we maintain a consistent orbit distance ratio between the limits. This ensures that any
	//(damped) change in the limits is visible to the user.

	float desiredOrbitDistanceRatio = 1.0f;
	if(m_NumUpdatesPerformed > 0)
	{
		const float previousMinOrbitDistance	= m_MinOrbitDistanceSpring.GetResult();
		const float previousMaxOrbitDistance	= m_MaxOrbitDistanceSpring.GetResult();

		const float distanceRange = previousMaxOrbitDistance - previousMinOrbitDistance;
		if(distanceRange >= SMALL_FLOAT)
		{
			desiredOrbitDistanceRatio	= (desiredOrbitDistance - previousMinOrbitDistance) / distanceRange;
			desiredOrbitDistanceRatio	= Clamp(desiredOrbitDistanceRatio, 0.0f, 1.0f);
		}
	}

	UpdateOrbitDistanceLimits();

	float minOrbitDistance = m_MinOrbitDistanceSpring.GetResult();
	float maxOrbitDistance = m_MaxOrbitDistanceSpring.GetResult();

	//NOTE: m_ControlHelper has already been validated.
	const bool wasLookingBehind	= m_ControlHelper->WasLookingBehind();
	const bool isLookingBehind	= m_ControlHelper->IsLookingBehind();
	if(isLookingBehind || wasLookingBehind)
	{
		//Force the orbit distance out to the upper limit when looking behind (and when toggling back from looking-behind) to avoid
		//unintuitive behaviour.
		//NOTE: This must be done after the damping of the actual orbit distance limits.
		minOrbitDistance = maxOrbitDistance;
	}

	float orbitDistanceToApply;
	if(m_NumUpdatesPerformed > 0)
	{
		orbitDistanceToApply = Lerp(desiredOrbitDistanceRatio, minOrbitDistance, maxOrbitDistance);
	}
	else
	{
		orbitDistanceToApply = Clamp(desiredOrbitDistance, minOrbitDistance, maxOrbitDistance);
	}

	return orbitDistanceToApply;
}

void camThirdPersonCamera::UpdateOrbitDistanceLimits()
{
	float desiredMinOrbitDistance;
	float desiredMaxOrbitDistance;
	const float orbitPitchOffset = ComputeOrbitPitchOffset();

	ComputeDesiredOrbitDistanceLimits(desiredMinOrbitDistance, desiredMaxOrbitDistance, orbitPitchOffset,
		m_Metadata.m_ShouldIgnoreVerticalPivotOffsetForFootRoom);

	const float springConstant = ComputeOrbitDistanceLimitsSpringConstant();

	m_MinOrbitDistanceSpring.Update(desiredMinOrbitDistance, springConstant, m_Metadata.m_OrbitDistanceLimitSpringDampingRatio, true);
	m_MaxOrbitDistanceSpring.Update(desiredMaxOrbitDistance, springConstant, m_Metadata.m_OrbitDistanceLimitSpringDampingRatio, true);
}

void camThirdPersonCamera::ComputeDesiredOrbitDistanceLimits(float& minOrbitDistance, float& maxOrbitDistance, float orbitPitchOffset, bool ShouldIgnoreVerticalPivotOffsetForFootRoom,
	bool includeAttachments, s32 overriddenViewMode) const
{
	//Base the distance limits on the maximum delta between the the base attach position and the attach parent's XY bounding box limits.
	//This improves the consistency of setup across a wide variety of parent types.

	Vector3 relativeBaseAttachPosition;
	m_AttachParentMatrix.UnTransform(m_BaseAttachPosition, relativeBaseAttachPosition); //Transform to parent-local space.

	Vector3 maxDistancesToBoundingBox;
	maxDistancesToBoundingBox.Max(m_AttachParentBoundingBoxMax - relativeBaseAttachPosition, relativeBaseAttachPosition - m_AttachParentBoundingBoxMin);

	const float maxDistanceToBoundingBox = Max(maxDistancesToBoundingBox.x, maxDistancesToBoundingBox.y);

	//Now compute the additional distances required to achieve the desired min/max foot-room, taking into account any orbit pitch offset.

	const float baseAttachPositionOffsetForBasePivotPosition = ComputeBaseAttachPositionOffsetForBasePivotPosition();

	float basePivotHeight			= baseAttachPositionOffsetForBasePivotPosition + relativeBaseAttachPosition.z - m_AttachParentBoundingBoxMin.z;

	const float basePivotHeightScalingForFootRoom = ComputeBasePivotHeightScalingForFootRoom();

	basePivotHeight					*= basePivotHeightScalingForFootRoom;
	const float theta				= rage::Atan2f(basePivotHeight / maxDistanceToBoundingBox, 1.0f);
	const float hypot				= rage::Sqrtf((basePivotHeight * basePivotHeight) + (maxDistanceToBoundingBox * maxDistanceToBoundingBox));
	const float	phi					= theta - orbitPitchOffset;
	float baseOrbitDistance			= hypot * rage::Cosf(phi);
	float boxHeight					= hypot * rage::Sinf(phi);

	if(!ShouldIgnoreVerticalPivotOffsetForFootRoom)
	{
		//NOTE: The box height is essentially camera-relative, given parent movement along the XY plane, so add any camera relative vertical offset.
		Vector3 orbitRelativeOffset;
		ComputeOrbitRelativePivotOffset(orbitRelativeOffset);

		boxHeight += orbitRelativeOffset.z;
	}

	float screenRatioForMinFootRoom;
	float screenRatioForMaxFootRoom;
	ComputeScreenRatiosForFootRoomLimits(screenRatioForMinFootRoom, screenRatioForMaxFootRoom);

	const float baseFov				= ComputeBaseFov();

	const float minFootRoomAngle	= Min(baseFov * DtoR * (0.5f - screenRatioForMinFootRoom), HALF_PI);
	const float tanMinFootRoomAngle	= rage::Tanf(minFootRoomAngle);
	const float maxFootRoomAngle	= Min(baseFov * DtoR * (0.5f - screenRatioForMaxFootRoom), HALF_PI);
	const float tanMaxFootRoomAngle	= rage::Tanf(maxFootRoomAngle);

	const float minFootOrbitDistanceOffset	= boxHeight / tanMinFootRoomAngle;
	const float maxFootOrbitDistanceOffset	= boxHeight / tanMaxFootRoomAngle;

	baseOrbitDistance += m_Metadata.m_BaseOrbitDistanceOffset;

	minOrbitDistance = baseOrbitDistance + minFootOrbitDistanceOffset;
	maxOrbitDistance = baseOrbitDistance + maxFootOrbitDistanceOffset;

	float minSafeOrbitDistance = 0.0f;
	if(m_BoundingBoxExtensionForOrbitDistance > SMALL_FLOAT && includeAttachments)
	{
		//Ensure that both orbit distance limits are comfortably longer than this extension.
		const float extendedOrbitDistance = m_BoundingBoxExtensionForOrbitDistance + relativeBaseAttachPosition.y - m_AttachParentBoundingBoxMin.y;
	
		minSafeOrbitDistance				= extendedOrbitDistance * m_Metadata.m_MinSafeOrbitDistanceScalingForExtensions;
		minOrbitDistance					= Max(minOrbitDistance, minSafeOrbitDistance);
		maxOrbitDistance					= Max(maxOrbitDistance, minSafeOrbitDistance);

		if (minOrbitDistance == minSafeOrbitDistance || maxOrbitDistance == minSafeOrbitDistance)
		{
			const camControlHelperMetadataViewModeSettings* nearViewModeSettings	= m_ControlHelper ? m_ControlHelper->GetViewModeSettings(
																						(s32)camControlHelperMetadataViewMode::THIRD_PERSON_NEAR) : NULL;
			if (nearViewModeSettings)
			{
				if (minOrbitDistance == minSafeOrbitDistance && nearViewModeSettings->m_OrbitDistanceLimitScaling.x < 1.0f)
				{
					minOrbitDistance *= (1.0f/nearViewModeSettings->m_OrbitDistanceLimitScaling.x);
				}
				if (maxOrbitDistance == minSafeOrbitDistance && nearViewModeSettings->m_OrbitDistanceLimitScaling.y < 1.0f)
				{
					maxOrbitDistance *= (1.0f/nearViewModeSettings->m_OrbitDistanceLimitScaling.y);
				}
			}
		}
	}

	if(m_ControlHelper)
	{
		const camControlHelperMetadataViewModeSettings* viewModeSettings = m_ControlHelper->GetViewModeSettings(overriddenViewMode);
		if(viewModeSettings)
		{
			const camControlHelperMetadataViewModeSettings* mediumViewModeSettings	= m_ControlHelper ? m_ControlHelper->GetViewModeSettings(
																						(s32)camControlHelperMetadataViewMode::THIRD_PERSON_MEDIUM) : NULL;
			if((m_OrbitDistanceExtensionForTowedVehicle >= SMALL_FLOAT) && mediumViewModeSettings && includeAttachments)
			{
				minOrbitDistance *= mediumViewModeSettings->m_OrbitDistanceLimitScaling.x;
				maxOrbitDistance *= mediumViewModeSettings->m_OrbitDistanceLimitScaling.y;

				minOrbitDistance += m_OrbitDistanceExtensionForTowedVehicle * viewModeSettings->m_OrbitDistanceLimitScaling.x;
				maxOrbitDistance += m_OrbitDistanceExtensionForTowedVehicle * viewModeSettings->m_OrbitDistanceLimitScaling.y;
			}
			else
			{
				minOrbitDistance *= viewModeSettings->m_OrbitDistanceLimitScaling.x;
				maxOrbitDistance *= viewModeSettings->m_OrbitDistanceLimitScaling.y;
			}
		}
	}

	camInterface::GetGameplayDirector().GetScriptOverriddenThirdPersonCameraOrbitDistanceLimitsThisUpdate(minOrbitDistance, maxOrbitDistance);

	// To be safe, in case after scaling by m_OrbitDistanceLimitScaling, the orbit distance is below the safe minimum.
	minOrbitDistance	= Max(minOrbitDistance, minSafeOrbitDistance);
	maxOrbitDistance	= Max(maxOrbitDistance, minSafeOrbitDistance);

	if(m_Metadata.m_CustomOrbitDistanceLimitsToForce.x >= SMALL_FLOAT)
	{
		minOrbitDistance = m_Metadata.m_CustomOrbitDistanceLimitsToForce.x;
	}

	if(m_Metadata.m_CustomOrbitDistanceLimitsToForce.y >= SMALL_FLOAT)
	{
		maxOrbitDistance = m_Metadata.m_CustomOrbitDistanceLimitsToForce.y;
	}
}

void camThirdPersonCamera::ComputeScreenRatiosForFootRoomLimits(float& screenRatioForMinFootRoom, float& screenRatioForMaxFootRoom) const
{
	if(ShouldUseTightSpaceCustomFraming())
	{
		//Apply any scaling for tight spaces.
		const float blendLevel		= m_TightSpaceSpring.GetResult();
		screenRatioForMinFootRoom	= Lerp(blendLevel, m_Metadata.m_ScreenRatioForMinFootRoom, m_Metadata.m_ScreenRatioForMinFootRoomInTightSpace);
		screenRatioForMaxFootRoom	= Lerp(blendLevel, m_Metadata.m_ScreenRatioForMaxFootRoom, m_Metadata.m_ScreenRatioForMaxFootRoomInTightSpace);
	}
	else
	{
		screenRatioForMinFootRoom	= m_Metadata.m_ScreenRatioForMinFootRoom;
		screenRatioForMaxFootRoom	= m_Metadata.m_ScreenRatioForMaxFootRoom;
	}

	ComputeCustomScreenRatiosForFootRoomLimits(screenRatioForMinFootRoom, screenRatioForMaxFootRoom);
}

float camThirdPersonCamera::ComputeBasePivotHeightScalingForFootRoom() const
{
	return m_Metadata.m_BasePivotHeightScalingForFootRoom;
}

float camThirdPersonCamera::ComputeBaseFov() const
{
	TUNE_BOOL(CAM_THIRD_PERSON_HFOV, false);
	if(CAM_THIRD_PERSON_HFOV)
	{
		const float baseAspect		= 16.0f / 9.0f; // The reference aspect ratio

		const float baseHFov		= 2.0f * atan(Tanf(0.5f * m_Metadata.m_BaseFov * DtoR) * baseAspect) * RtoD; // Convert the BaseFov from vertical to horizontal
		const float baseMaxHFov		= 110.0f;	// Value from the unofficial mod

		// Get the desired hFov (at a base aspect of 16/9)
		const s32 profileSettingFov	= CPauseMenu::GetMenuPreference(PREF_FPS_FIELD_OF_VIEW); // NOTE: Hacked in for now until we get an extra setting for 3rd person, if we're going for that
		float fFovSlider			= Clamp((float)profileSettingFov / (float)DEFAULT_SLIDER_MAX, 0.0f, 1.0f);
		const float hFovSetting		= Lerp(fFovSlider, baseHFov, baseMaxHFov); // TODO: Third person camera needs a maxbasefov (what about the aim cameras, cinematic cameras, sniper scopes, etc.)

		// Compute a limit on the resulting vFov so that we don't exceed sensible capsule radius bounds
		const float sceneWidth		= static_cast<float>(VideoResManager::GetSceneWidth());
		const float sceneHeight		= static_cast<float>(VideoResManager::GetSceneHeight());
		const float viewportAspect	= sceneWidth / sceneHeight;

		const float nearZ						= m_Metadata.m_BaseNearClip/*m_Frame.GetNearClip()*/;
		TUNE_FLOAT(CAM_MAX_THIRD_PERSON_PED_CAM_CAPSULE_RADIUS_FOR_FOV, 0.272f, 0.0f, 1.0f, 0.001f); // Slightly larger than the ped's mover capsule, but small enough to not exceed the shoulders/arms
		TUNE_FLOAT(CAM_MAX_THIRD_PERSON_VEHICLE_CAM_CAPSULE_RADIUS_FOR_FOV, 0.288f, 0.0f, 1.0f, 0.001f);
		
		float maxCapsuleRadius = (GetIsClassId(camFollowVehicleCamera::GetStaticClassId()) || GetIsClassId(camThirdPersonVehicleAimCamera::GetStaticClassId())) ?
			CAM_MAX_THIRD_PERSON_VEHICLE_CAM_CAPSULE_RADIUS_FOR_FOV :
			CAM_MAX_THIRD_PERSON_PED_CAM_CAPSULE_RADIUS_FOR_FOV;

		const float viewportDiagonalAtNearZ		= sqrt(maxCapsuleRadius * maxCapsuleRadius - nearZ * nearZ);
		const float viewportDiagonalAngleRad	= atan(1 / viewportAspect); // This will handle portrait mode
		//const float horizontalMax				= cos(viewportDiagonalAngleRad) * viewportDiagonalAtNearZ;
		const float verticalMax					= sin(viewportDiagonalAngleRad) * viewportDiagonalAtNearZ;
		//const float maxThirdPersonHFov		= 2.0f * atan(horizontalMax / nearZ) * RtoD;
		const float maxThirdPersonVFov			= 2.0f * atan(verticalMax / nearZ) * RtoD;

		// Finally the vFov for the current aspect (assuming a baseFov of 16/9)
		const float hFov	= 2.0f * atan(Tanf(0.5f * hFovSetting * DtoR) * (viewportAspect / baseAspect)) * RtoD;
		float vFov			= 2.0f * atan(Tanf(0.5f * hFov * DtoR) / viewportAspect) * RtoD;	
		vFov				= Min(vFov, maxThirdPersonVFov);

		if(FPIsFinite(vFov)) // Make sure we don't propagate a NaN fov
		{
			return vFov;
		}
		else
		{
			return m_Metadata.m_BaseFov;
		}
	}
	else
	{
		return m_Metadata.m_BaseFov;
	}
}

void camThirdPersonCamera::ComputeCustomScreenRatiosForFootRoomLimits(float& screenRatioMin, float& screenRatioMax) const
{
	if(IsArrestingBehaviorActive())
	{
		screenRatioMin = screenRatioMax = m_Metadata.m_CustomOrbitDistanceBehaviors.m_Arresting.m_ScreenRatio;
	}
}

bool camThirdPersonCamera::ComputeShouldCutToDesiredOrbitDistanceLimits() const
{
	const bool wasRenderingOnPreviousUpdate	= camInterface::IsRenderingCamera(*this);
	const bool shouldCut					= (!wasRenderingOnPreviousUpdate || (m_NumUpdatesPerformed == 0) ||
												m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
												m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation));

	return shouldCut;
}

float camThirdPersonCamera::ComputeOrbitDistanceLimitsSpringConstant() const
{
	const bool shouldCutToDesiredLimits = ComputeShouldCutToDesiredOrbitDistanceLimits();
	if(shouldCutToDesiredLimits)
	{
		return 0.0f;
	}

	if(IsArrestingBehaviorActive())
	{
		return m_Metadata.m_CustomOrbitDistanceBehaviors.m_Arresting.m_LimitsSpringConstant;
	}

	return m_Metadata.m_OrbitDistanceLimitSpringConstant;
}

void camThirdPersonCamera::ComputeOrbitOrientation(float& orbitHeading, float& orbitPitch)
{
	Vector3 orbitFront(m_BaseFront);

	if(ShouldPersistOrbitOrientationRelativeToAttachParent() && (m_NumUpdatesPerformed > 0))
	{
		//Compensate for any rotation of the attach parent between frames. This avoids complicating the meaning of the persisted base front.
		Quaternion attachParentOrientation;
		attachParentOrientation.FromMatrix34(m_AttachParentMatrix);

		m_AttachParentOrientationOnPreviousUpdate.UnTransform(orbitFront);
		attachParentOrientation.Transform(orbitFront);
	}

	camFrame::ComputeHeadingAndPitchFromFront(orbitFront, orbitHeading, orbitPitch);
}

void camThirdPersonCamera::ComputeAttachParentLocalXYVelocity(Vector3& attachParentLocalXYVelocity) const
{
	Vector3 aCrossB;
	aCrossB.Cross(m_BaseAttachVelocityToConsider, m_AttachParentMatrix.c);
	attachParentLocalXYVelocity.Cross(m_AttachParentMatrix.c, aCrossB);
}

void camThirdPersonCamera::ComputeLookAroundOrientationOffsets(float& headingOffset, float& pitchOffset) const
{
	//NOTE: m_ControlHelper has already been validated.
	headingOffset	= m_ControlHelper->GetLookAroundHeadingOffset();
	pitchOffset		= m_ControlHelper->GetLookAroundPitchOffset();
}

void camThirdPersonCamera::ApplyOverriddenOrbitOrientation(float& orbitHeading, float& orbitPitch)
{
	if(m_CatchUpHelper)
	{
		//TODO: Fix catch-up for parent relative orbiting.
		const bool shouldOverrideOrientation = m_CatchUpHelper->ComputeOrbitOrientation(*this, orbitHeading, orbitPitch);
		if(shouldOverrideOrientation)
		{
			//Clear any existing orientation override requests.
			m_ShouldApplyRelativeHeading	= false;
			m_ShouldApplyRelativePitch		= false;

			return;
		}
	}

	const camBaseSwitchHelper* switchHelper = camInterface::GetGameplayDirector().GetSwitchHelper();
	if(switchHelper && (m_NumUpdatesPerformed > 0))
	{
		//Respect the behaviour of the Switch helper after the initial update.
		m_ShouldApplyRelativeHeading	= false;
		m_ShouldApplyRelativePitch		= false;
	}

	//NOTE: m_ControlHelper has already been validated.
	const bool wasLookingBehind	= m_ControlHelper->WasLookingBehind();
	const bool isLookingBehind	= m_ControlHelper->IsLookingBehind();
	if(isLookingBehind != wasLookingBehind)
	{
		//Override any existing orientation override requests and respect the user's look-behind toggle.
		m_RelativeHeadingToApply		= 0.0f;
		m_RelativeHeadingSmoothRate		= 1.0f;
		m_ShouldApplyRelativeHeading	= true;

		if(!m_Metadata.m_ShouldIgnoreAttachParentPitchForLookBehind)
		{
			m_RelativePitchToApply		= 0.0f;
			m_RelativePitchSmoothRate	= 1.0f;
			m_ShouldApplyRelativePitch	= true;
		}
	}

	if(!m_ShouldApplyRelativeHeading && !m_ShouldApplyRelativePitch)
	{
		return;
	}

	//NOTE: We use the simple matrix of the attach parent directly, not m_AttachParentMatrix, when overriding a relative orientation. This
	//reduces the scope for the parent matrix to be upside-down in world space, as this can flip the decomposed parent heading.
	Matrix34 attachParentMatrixToConsider(MAT34V_TO_MATRIX34(m_AttachParent->GetMatrix()));
	Vector3 previousOrbitFrontToConsider(m_BaseFront);

	if(ShouldOrbitRelativeToAttachParentOrientation())
	{
		attachParentMatrixToConsider.Dot3x3Transpose(m_AttachParentMatrixToConsiderForRelativeOrbit);

		m_AttachParentMatrixToConsiderForRelativeOrbit.UnTransform3x3(previousOrbitFrontToConsider);
	}

	float attachParentHeading;
	float attachParentPitch;
	camFrame::ComputeHeadingAndPitchFromMatrix(attachParentMatrixToConsider, attachParentHeading, attachParentPitch);

	float desiredOrbitHeading	= attachParentHeading + m_RelativeHeadingToApply;
	float desiredOrbitPitch		= attachParentPitch + m_RelativePitchToApply;

	if(m_ShouldApplyRelativeHeadingWrtIdeal)
	{
		const float headingOffsetToApply	= m_Metadata.m_IdealHeadingOffsetForLimiting * DtoR;
		desiredOrbitHeading					+= headingOffsetToApply;
	}

	//Ensure that the desired orientation does not exceed the limits.
	LimitOrbitHeading(desiredOrbitHeading);
	LimitOrbitPitch(desiredOrbitPitch);

	float previousOrbitHeading;
	float previousOrbitPitch;
	camFrame::ComputeHeadingAndPitchFromFront(previousOrbitFrontToConsider, previousOrbitHeading, previousOrbitPitch);

	//NOTE: m_ControlHelper has already been validated.
	bool isLookAroundInputPresent = m_ControlHelper->IsLookAroundInputPresent();

	if(m_ShouldApplyRelativeHeading)
	{
		if(m_ShouldIgnoreInputForRelativeHeading || !isLookAroundInputPresent)
		{
			//Cheaply make the smoothing frame-rate independent.
			//TODO: Replace the hacky 'smoothing rate' with a meaningful blend time.
			//NOTE: We must ensure that the inverse time-step does not exceed our hacky tuning time, as this would skew the results.
			const float invTimeStep	= Min(fwTimer::GetCamInvTimeStep(), 30.0f);
			const float smoothRate	= Clamp(m_RelativeHeadingSmoothRate * 30.0f / invTimeStep, 0.0f, 1.0f);
			//NOTE: We interpolate from the previous orientation in order to stop any other changes preventing the interpolation completing.
			orbitHeading			= fwAngle::LerpTowards(previousOrbitHeading, desiredOrbitHeading, smoothRate);
			const float error		= fwAngle::LimitRadianAngleSafe(desiredOrbitHeading - orbitHeading);
			if(Abs(error) <= g_MaxErrorForOverriddenOrbitOrientation)
			{
				//We've attained the requested relative heading.
				m_ShouldApplyRelativeHeading = false;
			}
		}
		else
		{
			//We have look-around input present that should not be ignored, so stop overriding the relative heading.
			m_ShouldApplyRelativeHeading = false;
		}
	}

	if(m_ShouldApplyRelativePitch)
	{
		if(m_ShouldIgnoreInputForRelativePitch || !isLookAroundInputPresent)
		{
			//Cheaply make the smoothing frame-rate independent.
			//TODO: Replace the hacky 'smoothing rate' with a meaningful blend time.
			//NOTE: We must ensure that the inverse time-step does not exceed our hacky tuning time, as this would skew the results.
			const float invTimeStep	= Min(fwTimer::GetCamInvTimeStep(), 30.0f);
			const float smoothRate	= Clamp(m_RelativePitchSmoothRate * 30.0f / invTimeStep, 0.0f, 1.0f);
			//NOTE: We interpolate from the previous orientation in order to stop any other changes preventing the interpolation completing.
			orbitPitch				= fwAngle::LerpTowards(previousOrbitPitch, desiredOrbitPitch, smoothRate);
			const float error		= fwAngle::LimitRadianAngleSafe(desiredOrbitPitch - orbitPitch);
			if(Abs(error) <= g_MaxErrorForOverriddenOrbitOrientation)
			{
				//We've attained the requested relative pitch.
				m_ShouldApplyRelativePitch = false;
			}
		}
		else
		{
			//We have look-around input present that should not be ignored, so stop overriding the relative pitch.
			m_ShouldApplyRelativePitch	= false;
		}
	}
}

void camThirdPersonCamera::LimitOrbitHeading(float& heading) const
{
	//NOTE: We always wrap the heading into a valid -PI to PI range.
	heading = fwAngle::LimitRadianAngleSafe(heading);

	Vector2 vCurrentHeadingLimits = m_RelativeOrbitHeadingLimits;
	camInterface::GetGameplayDirector().GetScriptOverriddenThirdPersonCameraRelativeHeadingLimitsThisUpdate(vCurrentHeadingLimits);

	const float thresholdForLimiting = PI - SMALL_FLOAT;
	if((vCurrentHeadingLimits.x >= -thresholdForLimiting) || (vCurrentHeadingLimits.y <= thresholdForLimiting))
	{
		float idealHeading = ComputeIdealOrbitHeadingForLimiting();

		if(ShouldOrbitRelativeToAttachParentOrientation())
		{
			//Convert the ideal heading to be relative to the attach parent, just as the heading is.
			const float attachParentHeading	= camFrame::ComputeHeadingFromMatrix(m_AttachParentMatrixToConsiderForRelativeOrbit);
			idealHeading					-= attachParentHeading;
			idealHeading					= fwAngle::LimitRadianAngleSafe(idealHeading);
		}

		float relativeHeading	= heading - idealHeading;
		relativeHeading			= fwAngle::LimitRadianAngle(relativeHeading);

		//NOTE: We invalidate this angle using THREE_PI.
		if((m_RelativeHeadingForLimitingOnPreviousUpdate > -TWO_PI) && (m_RelativeHeadingForLimitingOnPreviousUpdate < TWO_PI))
		{
			//Ensure that the relative angle used for limiting changes via the shortest angle, as this should prevent the heading popping across
			//the limits.
			const float relativeHeadingDelta = relativeHeading - m_RelativeHeadingForLimitingOnPreviousUpdate;
			if(relativeHeadingDelta > PI)
			{
				relativeHeading -= TWO_PI;
			}
			else if(relativeHeadingDelta < -PI)
			{
				relativeHeading += TWO_PI;
			}
		}

		relativeHeading = Clamp(relativeHeading, vCurrentHeadingLimits.x, vCurrentHeadingLimits.y);

		heading	= idealHeading + relativeHeading;
		heading	= fwAngle::LimitRadianAngle(heading);
	}
}

float camThirdPersonCamera::ComputeIdealOrbitHeadingForLimiting() const
{
	//Limit the orbit heading relative to the attach parent heading by default.
	const float attachParentHeading		= camFrame::ComputeHeadingFromMatrix(m_AttachParentMatrix);
	const float headingOffsetToApply	= m_Metadata.m_IdealHeadingOffsetForLimiting * DtoR;
	float idealHeading					= attachParentHeading + headingOffsetToApply;
	idealHeading						= fwAngle::LimitRadianAngle(idealHeading);

	return idealHeading;
}

void camThirdPersonCamera::LimitOrbitPitch(float& pitch) const
{
	Vector2 vCurrentPitchLimits = m_OrbitPitchLimits;
	if (camInterface::GetGameplayDirector().GetScriptOverriddenThirdPersonCameraRelativePitchLimitsThisUpdate(vCurrentPitchLimits))
	{
		if(!ShouldOrbitRelativeToAttachParentOrientation() && m_AttachParent.Get())
		{
			//NOTE: We use the simple matrix of the attach parent directly, not m_AttachParentMatrix, when overriding a relative orientation. This
			//reduces the scope for the parent matrix to be upside-down in world space, as this can flip the decomposed parent heading.
			const Matrix34 attachParentMatrix	= MAT34V_TO_MATRIX34(m_AttachParent->GetMatrix());
			const float attachParentPitch		= camFrame::ComputePitchFromMatrix(attachParentMatrix);

			vCurrentPitchLimits.x += attachParentPitch;
			vCurrentPitchLimits.x = Max(vCurrentPitchLimits.x, -g_MaxAbsSafePitchLimit);
			vCurrentPitchLimits.y += attachParentPitch;
			vCurrentPitchLimits.y = Min(vCurrentPitchLimits.y, g_MaxAbsSafePitchLimit);
		}
	}

	pitch = Clamp(pitch, vCurrentPitchLimits.x, vCurrentPitchLimits.y);
}

float camThirdPersonCamera::ComputeOrbitPitchOffset() const
{
	float pitchOffset = ComputeBaseOrbitPitchOffset();

	//Derive a pitch offset that helps the camera 'look over' the attach parent.
	const float lookOverHeight		= m_AttachParentBoundingBoxMax.z - m_AttachParentBoundingBoxMin.z;
	const float lookOverPitchOffset	= DtoR * RampValueSafe(lookOverHeight, m_Metadata.m_LookOverSettings.m_MinHeight,
										m_Metadata.m_LookOverSettings.m_MaxHeight, m_Metadata.m_LookOverSettings.m_PitchOffsetAtMinHeight,
										m_Metadata.m_LookOverSettings.m_PitchOffsetAtMaxHeight);
	pitchOffset						+= lookOverPitchOffset;

	if(m_HintHelper)
	{
		m_HintHelper->ComputeOrbitPitchOffset(pitchOffset);
	}

	return pitchOffset;
}

float camThirdPersonCamera::ComputeBaseOrbitPitchOffset() const
{
	const bool isFallingBackToThirdPersonForDirectorBlendCatchUpOrSwitch = camInterface::GetGameplayDirector().IsFallingBackToThirdPersonForDirectorBlendCatchUpOrSwitch();
	if(isFallingBackToThirdPersonForDirectorBlendCatchUpOrSwitch)
	{
		return 0.0f;
	}

	float pitchOffset;

	if(ShouldUseTightSpaceCustomFraming())
	{
		//Apply any scaling for tight spaces.
		const float blendLevel	= m_TightSpaceSpring.GetResult();
		pitchOffset				= Lerp(blendLevel, m_Metadata.m_BaseOrbitPitchOffset, m_Metadata.m_BaseOrbitPitchOffsetInTightSpace);
	}
	else
	{
		pitchOffset				= m_Metadata.m_BaseOrbitPitchOffset;
	}

	pitchOffset *= DtoR;

	return pitchOffset;
}

void camThirdPersonCamera::UpdatePivotPosition(const Matrix34& orbitMatrix)
{
	//Apply an orbit-relative offset about base pivot position.

	Vector3 orbitRelativeOffset;
	ComputeOrbitRelativePivotOffset(orbitMatrix.b, orbitRelativeOffset);

	orbitMatrix.Transform(orbitRelativeOffset, m_PivotPosition); //Transform to world space.

	if(m_HintHelper == NULL)
	{
		if( ReorientCameraToPreviousAimDirection() )
		{
			Vector3 orbitFront(m_BaseFront);
			if(ShouldOrbitRelativeToAttachParentOrientation())
			{
				m_AttachParentMatrixToConsiderForRelativeOrbit.UnTransform3x3(orbitFront);
			}

			float existingRoll = camFrame::ComputeRollFromMatrix(orbitMatrix);
			camFrame::ComputeWorldMatrixFromFront(orbitFront, const_cast<Matrix34&>(orbitMatrix));
			const_cast<Matrix34&>(orbitMatrix).RotateLocalY(existingRoll);

			orbitMatrix.Transform(orbitRelativeOffset, m_PivotPosition); //Transform to world space.

			if( GetIsTurretCam() && ReorientCameraToPreviousAimDirection() )
			{
				Vector3 orbitFront(m_BaseFront);
				if(ShouldOrbitRelativeToAttachParentOrientation())
				{
					m_AttachParentMatrixToConsiderForRelativeOrbit.UnTransform3x3(orbitFront);
				}

				float existingRoll = camFrame::ComputeRollFromMatrix(orbitMatrix);
				camFrame::ComputeWorldMatrixFromFront(orbitFront, const_cast<Matrix34&>(orbitMatrix));
				const_cast<Matrix34&>(orbitMatrix).RotateLocalY(existingRoll);

				orbitMatrix.Transform(orbitRelativeOffset, m_PivotPosition); //Transform to world space.
			}
		}
	}
}

bool camThirdPersonCamera::ReorientCameraToPreviousAimDirection()
{
	const Vector3& previousCameraForward  = camInterface::GetGameplayDirector().GetPreviousAimCameraForward();
	const Vector3& previousCameraPosition = camInterface::GetGameplayDirector().GetPreviousAimCameraPosition();

	if( m_NumUpdatesPerformed == 0 &&
		!ShouldOrbitRelativeToAttachParentOrientation() &&
		m_AttachParent.Get() && m_AttachParent->GetIsTypePed() &&
		!previousCameraPosition.IsClose(g_InvalidPosition, SMALL_FLOAT) )
	{
		const CPed* pAttachPed = (CPed*)m_AttachParent.Get();

		if(pAttachPed->GetPedIntelligence())
		{
			const CTaskScriptedAnimation* pTaskScriptedAnim = (const CTaskScriptedAnimation*)(pAttachPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SCRIPTED_ANIMATION));
			if(pTaskScriptedAnim)
			{
				// Don't reorient to previous camera's orientation if player is running a scripted animation.
				return false;
			}
		}

		// B* 2177002: use line test so forward vector does not change.
		WorldProbe::CShapeTestProbeDesc lineTest;
		WorldProbe::CShapeTestFixedResults<1> shapeTestResults;
		lineTest.SetResultsStructure(&shapeTestResults);
		lineTest.SetTypeFlags(ArchetypeFlags::GTA_WEAPON_TEST);
		lineTest.SetContext(WorldProbe::LOS_Camera);
		lineTest.SetIsDirected(true);
		const u32 collisionIncludeFlags =	(u32)ArchetypeFlags::GTA_ALL_TYPES_WEAPON |
											(u32)ArchetypeFlags::GTA_RAGDOLL_TYPE |
											(u32)ArchetypeFlags::GTA_HORSE_RAGDOLL_TYPE;
		lineTest.SetIncludeFlags(collisionIncludeFlags);
		lineTest.SetExcludeTypeFlags(TYPE_FLAGS_NONE);
		lineTest.SetTreatPolyhedralBoundsAsPrimitives(true);

		const Vector3 playerPosition = VEC3V_TO_VECTOR3(m_AttachParent->GetTransform().GetPosition());
		const Vector3 previousCameraPosToPlayer = playerPosition + (previousCameraForward * (pAttachPed->GetCurrentMainMoverCapsuleRadius() + FLT_EPSILON)) - previousCameraPosition;
		float fDotProjection = Max(previousCameraPosToPlayer.Dot(previousCameraForward), 0.0f);
		const Vector3 vProbeStartPosition = previousCameraPosition + previousCameraForward*fDotProjection;

		const float maxProbeDistance = 50.0f;
		Vector3 vTargetPoint = vProbeStartPosition+previousCameraForward*maxProbeDistance;
		lineTest.SetStartAndEnd(vProbeStartPosition, vTargetPoint);
		lineTest.SetExcludeEntity(m_AttachParent, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);

		DEV_ONLY( CEntity* pHitEntity = NULL; )
		const bool hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(lineTest);
		if( hasHit )
		{
			vTargetPoint = shapeTestResults[0].GetHitPosition();
		#if __DEV
			pHitEntity = shapeTestResults[0].GetHitEntity();
			if(pHitEntity)
			{
			}
		#endif
		}

		Vector3 vNewForward  = vTargetPoint - m_PivotPosition;
		vNewForward.NormalizeSafe();

		m_BaseFront = vNewForward;
		if(ShouldOrbitRelativeToAttachParentOrientation())
		{
			//Convert the orbit orientation to be parent-relative.
			m_AttachParentMatrixToConsiderForRelativeOrbit.Transform3x3(m_BaseFront);
		}

#if __BANK && 0
		float targetHeading, targetPitch;
		camFrame::ComputeHeadingAndPitchFromFront(vNewForward, targetHeading, targetPitch);
		const camBaseCamera* pPreviousCamera = camInterface::GetDominantRenderedCamera();
		if(pPreviousCamera && pPreviousCamera != this)
		{
			float currentHeading, currentPitch;
			camFrame::ComputeHeadingAndPitchFromMatrix(pPreviousCamera->GetFrame().GetWorldMatrix(), currentHeading, currentPitch);
		#if !__NO_OUTPUT
			cameraDebugf1("REORIENT -- changing heading for camera %s, heading//pitch: %f, %f  current heading//pitch: %f, %f", GetName(), targetHeading, targetPitch, currentHeading, currentPitch);
		#else
			cameraDebugf1("REORIENT -- changing heading for camera, heading//pitch: %f, %f  current heading//pitch: %f, %f", targetHeading, targetPitch, currentHeading, currentPitch);
		#endif
		}
#endif // __BANK

		return true;
	}

	return false;
}

bool camThirdPersonCamera::ShouldPreventAnyInterpolation() const
{
	return m_Metadata.m_ShouldPreventAnyInterpolation;
}

bool camThirdPersonCamera::ShouldUseTightSpaceCustomFraming() const
{
	return m_Metadata.m_ShouldUseCustomFramingInTightSpace || camInterface::GetGameplayDirector().IsScriptForcingUseTightSpaceCustomFramingThisUpdate();
}

float camThirdPersonCamera::GetAttachParentHeightScalingForCameraRelativeVerticalOffset() const
{
	return camInterface::GetGameplayDirector().IsVehicleCameraUsingStuntSettingsThisUpdate(this) ? m_Metadata.m_PivotPosition.m_AttachParentHeightScalingForCameraRelativeVerticalOffset_ForStunt : m_Metadata.m_PivotPosition.m_AttachParentHeightScalingForCameraRelativeVerticalOffset;
}

float camThirdPersonCamera::GetAttachParentMatrixForRelativeOrbitSpringConstant() const
{
	return camInterface::GetGameplayDirector().IsVehicleCameraUsingStuntSettingsThisUpdate(this) ? m_Metadata.m_AttachParentMatrixForRelativeOrbitSpringConstant_ForStunt : m_Metadata.m_AttachParentMatrixForRelativeOrbitSpringConstant;
}

float camThirdPersonCamera::GetAttachParentMatrixForRelativeOrbitSpringDampingRatio() const
{
	return camInterface::GetGameplayDirector().IsVehicleCameraUsingStuntSettingsThisUpdate(this) ? m_Metadata.m_AttachParentMatrixForRelativeOrbitSpringDampingRatio_ForStunt : m_Metadata.m_AttachParentMatrixForRelativeOrbitSpringDampingRatio;
}

bool camThirdPersonCamera::ShouldOrbitRelativeToAttachParentOrientation() const
{
	return m_Metadata.m_ShouldOrbitRelativeToAttachParentOrientation || camInterface::GetGameplayDirector().IsVehicleCameraUsingStuntSettingsThisUpdate(this);
}

bool camThirdPersonCamera::ShouldPersistOrbitOrientationRelativeToAttachParent() const
{
	return m_Metadata.m_ShouldPersistOrbitOrientationRelativeToAttachParent || camInterface::GetGameplayDirector().IsVehicleCameraUsingStuntSettingsThisUpdate(this);
}

bool camThirdPersonCamera::ShouldApplyInAttachParentLocalSpace() const
{
	return m_Metadata.m_CollisionFallBackPosition.m_ShouldApplyInAttachParentLocalSpace || camInterface::GetGameplayDirector().IsVehicleCameraUsingStuntSettingsThisUpdate(this);
}

bool camThirdPersonCamera::ShouldApplyCollisionSettingsForTripleHeadInInteriors() const
{
	return m_Metadata.m_CollisionFallBackPosition.m_ShouldApplyMinBlendLevelAfterPushAwayFromCollisionForTripleHeadWithinInteriorsOnly;
}

bool camThirdPersonCamera::IsArrestingBehaviorActive() const
{
	if(m_Metadata.m_CustomOrbitDistanceBehaviors.m_Arresting.m_Enabled && m_AttachParent->GetIsTypePed())
	{
		const CPed* attachPed = static_cast<const CPed*>(m_AttachParent.Get());
		const bool isArresting = attachPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedIsArresting);
		return isArresting;
	}
	return false;
}

void camThirdPersonCamera::ComputeOrbitRelativePivotOffset(const Vector3& orbitFront, Vector3& orbitRelativeOffset) const
{
	ComputeOrbitRelativePivotOffset(orbitRelativeOffset);

	//Blend in an additional vertical offset to help low angle shots look over the bounding box of the attach parent.

	camThirdPersonCameraMetadataPivotOverBoungingBoxSettings pivotOverBoundingBoxSettings(m_Metadata.m_PivotOverBoundingBoxSettings);

	if(m_HintHelper)
	{
		m_HintHelper->ComputeHintPivotOverBoundingBoxBlendLevel(pivotOverBoundingBoxSettings.m_BlendLevel);
	}

	if(pivotOverBoundingBoxSettings.m_BlendLevel < SMALL_FLOAT)
	{
		return;
	}

	Vector3 relativeBasePivotPosition;
	m_AttachParentMatrix.UnTransform(m_BasePivotPosition, relativeBasePivotPosition); //Transform to parent-local space.

	const float boundingBoxTopToBasePivotHeightDelta = Abs(m_AttachParentBoundingBoxMax.z - relativeBasePivotPosition.z);
	if(boundingBoxTopToBasePivotHeightDelta < SMALL_FLOAT)
	{
		return;
	}

	Vector3 maxBasePivotToBoundingBoxDistances;
	maxBasePivotToBoundingBoxDistances.Max(m_AttachParentBoundingBoxMax - relativeBasePivotPosition, relativeBasePivotPosition -
		m_AttachParentBoundingBoxMin);

	float maxBasePivotToBoundingBoxDistanceXY = Max(maxBasePivotToBoundingBoxDistances.x, maxBasePivotToBoundingBoxDistances.y);
	if(m_BoundingBoxExtensionForOrbitDistance > SMALL_FLOAT)
	{
		//Take into account any bounding box extension, for trailers etc.
		const float extendedOrbitDistance	= m_BoundingBoxExtensionForOrbitDistance + relativeBasePivotPosition.y - m_AttachParentBoundingBoxMin.y;
		maxBasePivotToBoundingBoxDistanceXY	= Max(maxBasePivotToBoundingBoxDistanceXY, extendedOrbitDistance);
	}

	const float hypot	= Sqrtf((boundingBoxTopToBasePivotHeightDelta * boundingBoxTopToBasePivotHeightDelta) + (maxBasePivotToBoundingBoxDistanceXY *
							maxBasePivotToBoundingBoxDistanceXY));
	if(hypot < SMALL_FLOAT)
	{
		return;
	}

	const float alpha		= AsinfSafe(maxBasePivotToBoundingBoxDistanceXY / hypot);
	const float orbitPitch	= camFrame::ComputePitchFromFront(orbitFront);
	const float theta		= alpha - orbitPitch;
	float minVerticalOffset	= hypot * Cosf(theta);

	const float attachParentHeight	= m_AttachParentBoundingBoxMax.z - m_AttachParentBoundingBoxMin.z;
	minVerticalOffset				+= pivotOverBoundingBoxSettings.m_ExtraCameraRelativeVerticalOffset + (attachParentHeight *
										pivotOverBoundingBoxSettings.m_AttachParentHeightScalingForExtraCameraRelativeVerticalOffset);

	const float verticalOffset		= Max(orbitRelativeOffset.z, minVerticalOffset);
	orbitRelativeOffset.z			= Lerp(pivotOverBoundingBoxSettings.m_BlendLevel, orbitRelativeOffset.z, verticalOffset);
}

void camThirdPersonCamera::ComputeOrbitRelativePivotOffset(Vector3& orbitRelativeOffset) const
{
	ComputeOrbitRelativePivotOffsetInternal(orbitRelativeOffset);

	if(m_CatchUpHelper)
	{
		m_CatchUpHelper->ComputeOrbitRelativePivotOffset(*this, orbitRelativeOffset);
	}

	const camBaseSwitchHelper* switchHelper = camInterface::GetGameplayDirector().GetSwitchHelper();
	if(switchHelper)
	{
		switchHelper->ComputeOrbitRelativePivotOffset(orbitRelativeOffset);
	}

	const bool isFallingBackToThirdPersonForDirectorBlendCatchUpOrSwitch = camInterface::GetGameplayDirector().IsFallingBackToThirdPersonForDirectorBlendCatchUpOrSwitch();
	if(isFallingBackToThirdPersonForDirectorBlendCatchUpOrSwitch)
	{
		orbitRelativeOffset.x = 0.0f;
	}
}

void camThirdPersonCamera::ComputeOrbitRelativePivotOffsetInternal(Vector3& orbitRelativeOffset) const
{
	float relativeSideOffset		= m_Metadata.m_PivotPosition.m_CameraRelativeSideOffset; 
	float relativeVerticalOffset	= m_Metadata.m_PivotPosition.m_CameraRelativeVerticalOffset; 

	if(camInterface::GetGameplayDirector().IsHintActive() && m_HintHelper)
	{
		float hintSideOffsetAdditive; 
		float hintVerticalOffsetAdditive; 

		m_HintHelper->ComputeHintPivotPositionAdditiveOffset(hintSideOffsetAdditive, hintVerticalOffsetAdditive); 

		relativeSideOffset		+= hintSideOffsetAdditive; 
		relativeVerticalOffset	+= hintVerticalOffsetAdditive; 
	}

	orbitRelativeOffset.Set(relativeSideOffset, 0.0f, relativeVerticalOffset);

	const float attachParentWidth	= m_AttachParentBoundingBoxMax.x - m_AttachParentBoundingBoxMin.x;
	const float attachParentHeight	= m_AttachParentBoundingBoxMax.z - m_AttachParentBoundingBoxMin.z;

	float parentRelativeWidthScalar		= m_Metadata.m_PivotPosition.m_AttachParentWidthScalingForCameraRelativeSideOffset; 
	float parentRelativeHeightScalar	= GetAttachParentHeightScalingForCameraRelativeVerticalOffset();
	float alternateAimTimeMin			= m_Metadata.m_PivotPosition.m_timeAfterAimingToApplyAlternateScalingMin;
	float alternateAimTimeMax			= m_Metadata.m_PivotPosition.m_timeAfterAimingToApplyAlternateScalingMax;

	// Blend between the alternate height scalar based on the aim state of the player if the target is a player
	const float alternateParentHeightScalar = m_Metadata.m_PivotPosition.m_AttachParentHeightScalingForCameraRelativeVerticalOffset_AfterAiming;
	if(m_Metadata.m_PivotPosition.m_maxDistanceAfterAimingToApplyAlternateScalingMax > 0.0f)
	{
		if(m_DistanceTravelled < m_Metadata.m_PivotPosition.m_maxDistanceAfterAimingToApplyAlternateScalingMax)
		{
			float aimBlend = ComputeAimBlendBasedOnDistanceTravelled();
			parentRelativeHeightScalar = Lerp(SlowInOut(aimBlend), alternateParentHeightScalar, parentRelativeHeightScalar);
		}
	}
	else if( alternateAimTimeMin > 0.0f )
	{
		if(m_AttachParent && m_AttachParent->GetIsTypePed())
		{
			const CPed* attachPed = static_cast<const CPed*>(m_AttachParent.Get());
			if( attachPed && attachPed->IsLocalPlayer() && attachPed->GetPlayerInfo() )
			{
				Assert( alternateAimTimeMax > alternateAimTimeMin);
				float timeSinceLastAimed = (attachPed->GetPlayerInfo()->GetTimeSinceLastAimedMS()/1000.0f);
				float heightScale = Clamp((timeSinceLastAimed - alternateAimTimeMin) / (alternateAimTimeMax - alternateAimTimeMin), 0.0f, 1.0f);
				parentRelativeHeightScalar = Lerp(SlowInOut(heightScale), alternateParentHeightScalar, parentRelativeHeightScalar);
			}
		}	
	}

	if(camInterface::GetGameplayDirector().IsHintActive() && m_HintHelper)
	{
		float hintParentWidthAdditive; 
		float hintParentHeightAdditive; 

		m_HintHelper->ComputeHintParentSizeScalingAdditiveOffset(hintParentWidthAdditive, hintParentHeightAdditive); 

		parentRelativeWidthScalar	+= hintParentWidthAdditive; 
		parentRelativeHeightScalar	+= hintParentHeightAdditive; 
	}

	Vector3 scaledCameraRelativeOffset(attachParentWidth * parentRelativeWidthScalar, 0.0f, attachParentHeight * parentRelativeHeightScalar);

	orbitRelativeOffset += scaledCameraRelativeOffset;
}

void camThirdPersonCamera::UpdateBuoyancyState(float orbitDistance)
{
	const camThirdPersonCameraMetadataBuoyancySettings& settings = m_Metadata.m_BuoyancySettings;
	if(!settings.m_ShouldApplyBuoyancy)
	{
		return;
	}

	m_WasBypassingBuoyancyStateOnPreviousUpdate = m_ShouldBypassBuoyancyStateThisUpdate;
	if(m_ShouldBypassBuoyancyStateThisUpdate)
	{
		m_ShouldBypassBuoyancyStateThisUpdate = false;
		return;
	}

	const bool desiredBuoyancyState = ComputeIsBuoyant(orbitDistance);

	const u32 time = fwTimer::GetGameTimer().GetTimeInMilliseconds();

	//NOTE: m_AttachParent has already been validated.
	const CPed* player				= FindPlayerPed();
	const CPlayerInfo* playerInfo	= player ? player->GetPlayerInfo() : NULL;
	const bool isRespawning			= playerInfo && playerInfo->GetPlayerState() == CPlayerInfo::PLAYERSTATE_RESPAWN;

	if(m_NumUpdatesPerformed == 0 || m_WasBypassingBuoyancyStateOnPreviousUpdate)
	{
		m_IsBuoyant								= desiredBuoyancyState;
		m_DesiredBuoyancyState					= desiredBuoyancyState;
		m_DesiredBuoyancyStateTransitionTime	= 0;
		m_BuoyancyStateTransitionTime			= time;
	}
	else if(m_IsBuoyant == desiredBuoyancyState || isRespawning)
	{
		m_DesiredBuoyancyState					= desiredBuoyancyState;
		m_DesiredBuoyancyStateTransitionTime	= 0;
	}
	else
	{
		if(desiredBuoyancyState != m_DesiredBuoyancyState)
		{
			m_DesiredBuoyancyState					= desiredBuoyancyState;
			m_DesiredBuoyancyStateTransitionTime	= time;
		}

		const bool isValidToChangeBuoyancyState = (time >= (m_BuoyancyStateTransitionTime + settings.m_MinDelayBetweenBuoyancyStateChanges));
		if(isValidToChangeBuoyancyState)
		{
			const u32 minDelayToApply			= m_DesiredBuoyancyState ? settings.m_MinDelayOnSurfacing : settings.m_MinDelayOnSubmerging;
			const bool shouldApplyDesiredState	= (time >= (m_DesiredBuoyancyStateTransitionTime + minDelayToApply));
			if(shouldApplyDesiredState)
			{
				m_IsBuoyant						= m_DesiredBuoyancyState;
				m_BuoyancyStateTransitionTime	= time;

				m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation);
			}
		}
	}
}

bool camThirdPersonCamera::ComputeIsBuoyant(float orbitDistance) const
{
	//NOTE: m_AttachParent has already been validated.
	if(m_AttachParent->GetIsTypePed())
	{
		//Force camera buoyancy off if the attach ped is swimming under water, as this should be reliable after a minimum time swimming.
		//(The swimming motion tasks can be unreliable when first entering the water.)
		const u32 followPedTimeSpentSwimming	= camInterface::GetGameplayDirector().GetFollowPedTimeSpentSwimming();
		const bool shouldRespectMotionTask		= (followPedTimeSpentSwimming >= m_Metadata.m_BuoyancySettings.m_MinTimeSpentSwimmingToRespectMotionTask);
		if(shouldRespectMotionTask)
		{
			const CPed* attachPed		= static_cast<const CPed*>(m_AttachParent.Get());
			CTaskMotionBase* motionTask	= attachPed->GetCurrentMotionTask();
			if(!attachPed->GetUsingRagdoll() && motionTask && motionTask->IsUnderWater())
			{
				return false;
			}
		}
	}

	bool isBuoyant = true;

	if(m_AttachParent->GetIsPhysical())
	{
		//Use the attach parent's buoyancy helper, if it is up to date.
		int buoyancyStatus					= NOT_IN_WATER;
		const CPhysical* attachPhysical		= static_cast<const CPhysical*>(m_AttachParent.Get());
		const bool isBuoyancyHelperUpToDate	= (attachPhysical->m_nPhysicalFlags.bPossiblyTouchesWaterIsUpToDate &&
												attachPhysical->m_Buoyancy.GetBuoyancyInfoUpdatedThisFrame());
		if(isBuoyancyHelperUpToDate)
		{
			buoyancyStatus = attachPhysical->m_Buoyancy.GetStatus();

			//If the (up to date) buoyancy helper reports that the attach physical is at least partially in water, check if we should
			//force the camera underwater due to low overhead collision.
			if(buoyancyStatus != NOT_IN_WATER)
			{
				const float blendLevel = m_CollisionFallBackSpring.GetResult();
				if(blendLevel <= m_Metadata.m_BuoyancySettings.m_MaxCollisionFallBackBlendLevelToForceUnderWater)
				{
					return false;
				}
			}

			//Force camera buoyancy on if the (up to date) buoyancy helper reports that the attach physical is not fully in water,
			//as this should be reliable.
			if(m_Metadata.m_BuoyancySettings.m_ShouldSetBuoyantWhenAttachParentNotFullySubmerged && (buoyancyStatus != FULLY_IN_WATER))
			{
				return true;
			}
		}

		//Fall back to a simple bounding box check against the local water surface. The camera is buoyant so long as the
		//highest point on the attach parent is above a threshold level beneath the local water surface.

		//Extract the parent bounding box verts.
		Vector3 boundingBoxVerts[8] =
		{
			Vector3(m_AttachParentBoundingBoxMax.x, m_AttachParentBoundingBoxMax.y, m_AttachParentBoundingBoxMax.z),
			Vector3(m_AttachParentBoundingBoxMin.x, m_AttachParentBoundingBoxMax.y, m_AttachParentBoundingBoxMax.z),
			Vector3(m_AttachParentBoundingBoxMax.x, m_AttachParentBoundingBoxMin.y, m_AttachParentBoundingBoxMax.z),
			Vector3(m_AttachParentBoundingBoxMin.x, m_AttachParentBoundingBoxMin.y, m_AttachParentBoundingBoxMax.z),
			Vector3(m_AttachParentBoundingBoxMax.x, m_AttachParentBoundingBoxMax.y, m_AttachParentBoundingBoxMin.z),
			Vector3(m_AttachParentBoundingBoxMin.x, m_AttachParentBoundingBoxMax.y, m_AttachParentBoundingBoxMin.z),
			Vector3(m_AttachParentBoundingBoxMax.x, m_AttachParentBoundingBoxMin.y, m_AttachParentBoundingBoxMin.z),
			Vector3(m_AttachParentBoundingBoxMin.x, m_AttachParentBoundingBoxMin.y, m_AttachParentBoundingBoxMin.z)
		};

		//Transform the local-space verts into world-space.
		for(int i=0; i<8; i++)
		{
			m_AttachParentMatrix.Transform(boundingBoxVerts[i]);
		}

		//Compute the highest point on the attach parent.
		float maxParentHeight = boundingBoxVerts[0].z;
		for(int i=1; i<8; i++)
		{
			if(boundingBoxVerts[i].z > maxParentHeight)
			{
				maxParentHeight = boundingBoxVerts[i].z;
			}
		}

		//Compute the local water height, using a buoyancy helper if possible.
		float localWaterHeight			= 0.0f;
		bool isLocalWaterHeightValid	= false;
		//NOTE: The average absolute water level is only updated if the buoyancy helper detects water.
		if(isBuoyancyHelperUpToDate && (buoyancyStatus != NOT_IN_WATER))
		{
			localWaterHeight		= attachPhysical->m_Buoyancy.GetAbsWaterLevel();
			isLocalWaterHeightValid	= true;
		}
		else
		{
			//Query the buoyancy helper of any physical the attach parent is attached to or standing on, if it is up to date.
			const CEntity* attachOrGroundEntity = ComputeAttachParentAttachEntity();
			if(!attachOrGroundEntity)
			{
				attachOrGroundEntity = ComputeAttachParentGroundEntity();
			}

			if(attachOrGroundEntity && attachOrGroundEntity->GetIsPhysical())
			{
				const CPhysical* attachOrGroundPhysical = static_cast<const CPhysical*>(attachOrGroundEntity);
				if(attachOrGroundPhysical->m_nPhysicalFlags.bPossiblyTouchesWaterIsUpToDate &&
					attachOrGroundPhysical->m_Buoyancy.GetBuoyancyInfoUpdatedThisFrame())
				{
					//NOTE: The average absolute water level is only updated if the buoyancy helper detects water.
					const int attachOrGroundPhysicalBuoyancyStatus = attachOrGroundPhysical->m_Buoyancy.GetStatus();
					if(attachOrGroundPhysicalBuoyancyStatus != NOT_IN_WATER)
					{
						localWaterHeight		= attachOrGroundPhysical->m_Buoyancy.GetAbsWaterLevel();
						isLocalWaterHeightValid	= true;
					}
				}
			}

			if(!isLocalWaterHeightValid)
			{
				//We were not able to utilise a buoyancy helper, so query the local water height directly.
				//Test to double the orbit distance, for safety.
				const float maxDistanceToTestForWater	= 2.0f * orbitDistance;
				isLocalWaterHeightValid					= camCollision::ComputeWaterHeightAtPosition(m_AttachParentMatrix.d, maxDistanceToTestForWater, localWaterHeight, maxDistanceToTestForWater);
			}
		}

		if(isLocalWaterHeightValid)
		{
			float fMinDepthToRemainBuoyant = m_IsBuoyant ? m_Metadata.m_BuoyancySettings.m_MaxAttachParentDepthUnderWaterToRemainBuoyant : m_Metadata.m_BuoyancySettings.m_MaxAttachParentDepthUnderWaterToRemainBuoyantOut;
			isBuoyant = (maxParentHeight > (localWaterHeight - fMinDepthToRemainBuoyant));			
		}
	}

	return isBuoyant;
}

void camThirdPersonCamera::UpdateCollision(Vector3& cameraPosition, float &orbitHeadingOffset, float &orbitPitchOffset)
{
	if(m_Collision == NULL)
	{
		return;
	}

	if(m_Metadata.m_ShouldIgnoreCollisionWithAttachParent)
	{
		m_Collision->IgnoreEntityThisUpdate(*m_AttachParent);

		const bool shouldPushBeyondAttachParentIfClipping = ComputeShouldPushBeyondAttachParentIfClipping();
		if(shouldPushBeyondAttachParentIfClipping)
		{
			m_Collision->PushBeyondEntityIfClippingThisUpdate(*m_AttachParent);
		}
	}

	const camGameplayDirector& gameplayDirector = camInterface::GetGameplayDirector();

	if(m_Metadata.m_ShouldIgnoreCollisionWithFollowVehicle)
	{
		const CVehicle* followVehicle	= gameplayDirector.GetFollowVehicle();
		const s32 vehicleEntryExitState	= gameplayDirector.GetVehicleEntryExitState();

		// Ensure we don't ignore collision during heli turret direct entry point entrance (or when exiting)
		const CPed* attachPed						= m_AttachParent->GetIsTypePed() ? static_cast<const CPed*>(m_AttachParent.Get()) : NULL;
		const bool isFollowPedEnteringHeliTurret	= attachPed && 
														followVehicle && followVehicle->InheritsFromHeli() && 
														camInterface::GetGameplayDirector().IsPedInOrEnteringTurretSeat(followVehicle, *attachPed);		

		bool isClimbingUpVehicle = false;
		if(attachPed && attachPed->GetPedIntelligence())
		{
			const CTask* pTask = attachPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE);
			if(pTask)
			{
				const s32 iCurrentTastState = pTask->GetState();
				isClimbingUpVehicle = (iCurrentTastState == CTaskEnterVehicle::State_ClimbUp || iCurrentTastState == CTaskEnterVehicle::State_VaultUp);
			}
		}

		if(followVehicle && (vehicleEntryExitState != camGameplayDirector::OUTSIDE_VEHICLE || isClimbingUpVehicle) && !isFollowPedEnteringHeliTurret)
		{
			//Ignore any vehicle that the attach ped is associated with when applying collision and ensure that the camera is pushed outwards
			//beyond its bounds when clipping.
			m_Collision->IgnoreEntityThisUpdate(*followVehicle);
			m_Collision->PushBeyondEntityIfClippingThisUpdate(*followVehicle);

			//Also deal with any trailers or towed vehicles that we are tracking that may already have been detached.
			const CVehicle* trailerOrTowedVehicle = camInterface::GetGameplayDirector().GetAttachedTrailer();
			if(!trailerOrTowedVehicle)
			{
				trailerOrTowedVehicle = camInterface::GetGameplayDirector().GetTowedVehicle();
			}

			if(trailerOrTowedVehicle)
			{
				m_Collision->IgnoreEntityThisUpdate(*trailerOrTowedVehicle);
				m_Collision->PushBeyondEntityIfClippingThisUpdate(*trailerOrTowedVehicle);
			}
		}
	}

	if(m_AttachParent->GetIsTypePed())
	{
		//If the attach ped is using a parachute, ignore it.
		const CPed* attachPed					= static_cast<const CPed*>(m_AttachParent.Get());
		const CPedIntelligence* pedIntelligence	= attachPed->GetPedIntelligence();
		if(pedIntelligence)
		{
			const CTaskParachute* parachuteTask = static_cast<const CTaskParachute*>(pedIntelligence->FindTaskByType(CTaskTypes::TASK_PARACHUTE));
			if(parachuteTask)
			{
				const CObject* parachute = parachuteTask->GetParachute();
				if(parachute)
				{
					m_Collision->IgnoreEntityThisUpdate(*parachute);
				}
			}
		}
	}

	UpdateVehicleOnTopOfVehicleCollisionBehaviour();

	const float desiredCollisionTestRadius = ComputeMaxSafeCollisionRadiusForAttachParent();

	const float collisionTestRadiusOnPreviousUpdate = m_CollisionTestRadiusSpring.GetResult();

	//NOTE: We must cut to a lesser radius in order to avoid compromising the shapetests.
	const bool shouldCutSpring	= ((m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
									m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation) ||
									(desiredCollisionTestRadius < collisionTestRadiusOnPreviousUpdate));
	const float springConstant	= shouldCutSpring ? 0.0f : m_Metadata.m_CollisionTestRadiusSpringConstant;

	float collisionTestRadius	= m_CollisionTestRadiusSpring.Update(desiredCollisionTestRadius, springConstant,
									m_Metadata.m_CollisionTestRadiusSpringDampingRatio, true);

	UpdateCollisionOrigin(collisionTestRadius);
	UpdateCollisionRootPosition(cameraPosition, collisionTestRadius);

	const float orbitDistance = m_PivotPosition.Dist(cameraPosition);

	UpdateBuoyancyState(orbitDistance);

	if(m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation))
	{
		//Bypass any orbit distance damping and force pop-in behaviour.
		m_Collision->Reset();
	}
	else
	{
		//Force the camera to pop-in and block any automatic reorientation if the user is looking-around, during Switch behaviour or if a
		//hint is modifying the camera orientation, to avoid conflict.
		const bool isLookAroundInputPresent = m_ControlHelper->IsLookAroundInputPresent();
		const bool isSwitchBehaviourActive	= (gameplayDirector.GetSwitchHelper() != NULL);
		const bool isReorientingForHint		= (m_HintHelper && (gameplayDirector.GetHintBlend() >= SMALL_FLOAT));
		if(isLookAroundInputPresent || isSwitchBehaviourActive || isReorientingForHint)
		{
			m_Collision->ForcePopInThisUpdate();
		}
	}

	//We must ignore collision with all 'touching' peds and vehicles if the attach vehicle is flagged to ignore collision with network objects in a
	//multiplayer session. This interface is used to temporarily disable collision when respotting a vehicle.
	//- If the implementation of vehicle respotting or the use of the NO_COLLISION_NETWORK_OBJECTS flag is changed, we'll need to update this
	//code accordingly.
	if(NetworkInterface::IsGameInProgress() && m_AttachParent->GetIsTypeVehicle())
	{
		const CVehicle* attachVehicle = static_cast<const CVehicle*>(m_AttachParent.Get());
		if(attachVehicle->GetNoCollisionEntity())
		{
			const int noCollisionFlags = attachVehicle->GetNoCollisionFlags();
			if(noCollisionFlags & NO_COLLISION_NETWORK_OBJECTS)
			{
				m_Collision->IgnoreCollisionWithTouchingPedsAndVehiclesThisUpdate();
			}
		}
	}

	m_Collision->SetEntityToConsiderForTouchingChecksThisUpdate(*m_AttachParent);

	m_Collision->Update(m_CollisionRootPosition, collisionTestRadius, m_IsBuoyant, cameraPosition, orbitHeadingOffset, orbitPitchOffset);
}

bool camThirdPersonCamera::ComputeShouldPushBeyondAttachParentIfClipping() const
{
	return m_Metadata.m_ShouldPushBeyondAttachParentIfClipping;
}

void camThirdPersonCamera::UpdateVehicleOnTopOfVehicleCollisionBehaviour()
{
	const camThirdPersonCameraMetadataVehicleOnTopOfVehicleCollisionSettings& settings = m_Metadata.m_VehicleOnTopOfVehicleCollisionSettings;

	if(!settings.m_ShouldApply || !m_AttachParent->GetIsTypeVehicle())
	{
		return;
	}

	const CVehicle* attachVehicle = static_cast<const CVehicle*>(m_AttachParent.Get());

	//Ignore and push beyond any vehicles that are detected as being 'on top of' the attach vehicle.
	// - Track any such nearby vehicles for a set duration to avoid rapid changes in behaviour.

	const u32 gameTime = fwTimer::GetTimeInMilliseconds();

	//Validate all existing tracked vehicles, ensuring that they have not expired and are still 'touching' the follow vehicle.
	s32 vehicleIndex;
	for(vehicleIndex=0; vehicleIndex<m_VehiclesOnTopOfAttachVehicleList.GetCount(); )
	{
		tNearbyVehicleSettings& vehicleSettings = m_VehiclesOnTopOfAttachVehicleList[vehicleIndex];

		const bool isValid	= ((vehicleSettings.m_Vehicle != NULL) && (gameTime <= (vehicleSettings.m_DetectionTime + settings.m_MaxDurationToTrackVehicles)) &&
								attachVehicle->GetIsTouching(vehicleSettings.m_Vehicle));
		if(isValid)
		{
			vehicleIndex++;
		}
		else
		{
			//NOTE: We explicitly clear the registered reference for safety.
			vehicleSettings.m_Vehicle = NULL;
			m_VehiclesOnTopOfAttachVehicleList.Delete(vehicleIndex);
		}
	}

	atArray<const CVehicle*> vehiclesOnTopOfAttachVehicleList;
	ComputeVehiclesOnTopOfVehicle(*attachVehicle, vehiclesOnTopOfAttachVehicleList);

	const s32 numVehiclesOnTop = vehiclesOnTopOfAttachVehicleList.GetCount();
	for(s32 vehicleOnTopIndex=0; vehicleOnTopIndex<numVehiclesOnTop; vehicleOnTopIndex++)
	{
		const CVehicle* vehicleOnTop = static_cast<const CVehicle*>(vehiclesOnTopOfAttachVehicleList[vehicleOnTopIndex]);
		if(!vehicleOnTop)
		{
			continue;
		}

		//Add this vehicle to our list of tracked vehicles, or bump the detection time if we're already tracking it.

		bool isTrackingVehicle = false;

		const s32 numTrackedVehicles = m_VehiclesOnTopOfAttachVehicleList.GetCount();
		for(vehicleIndex=0; vehicleIndex<numTrackedVehicles; vehicleIndex++)
		{
			tNearbyVehicleSettings& vehicleSettings	= m_VehiclesOnTopOfAttachVehicleList[vehicleIndex];
			isTrackingVehicle						= (vehicleSettings.m_Vehicle == vehicleOnTop);
			if(isTrackingVehicle)
			{
				vehicleSettings.m_DetectionTime = gameTime;
				break;
			}
		}

		if(!isTrackingVehicle)
		{
			tNearbyVehicleSettings& vehicleSettings	= m_VehiclesOnTopOfAttachVehicleList.Grow();
			vehicleSettings.m_Vehicle				= vehicleOnTop;
			vehicleSettings.m_DetectionTime			= gameTime;
		}
	}

	//Finally, ignore and push beyond all tracked vehicles.
	const s32 numTrackedVehicles = m_VehiclesOnTopOfAttachVehicleList.GetCount();
	for(vehicleIndex=0; vehicleIndex<numTrackedVehicles; vehicleIndex++)
	{
		tNearbyVehicleSettings& vehicleSettings	= m_VehiclesOnTopOfAttachVehicleList[vehicleIndex];
		const CVehicle* vehicleToIgnore			= vehicleSettings.m_Vehicle;
		if(!vehicleToIgnore)
		{
			continue;
		}

		m_Collision->IgnoreEntityThisUpdate(*vehicleToIgnore);
		m_Collision->PushBeyondEntityIfClippingThisUpdate(*vehicleToIgnore);
	}
}

void camThirdPersonCamera::ComputeVehiclesOnTopOfVehicle(const CVehicle& vehicleToConsiderAsBottom, atArray<const CVehicle*>& vehiclesOnTopList) const
{
	const CVehicleIntelligence* vehicleIntelligence = vehicleToConsiderAsBottom.GetIntelligence();
	if(!vehicleIntelligence)
	{
		return;
	}

	const CEntityScannerIterator nearbyVehicles = vehicleIntelligence->GetVehicleScanner().GetIterator();

	for(const CEntity* nearbyEntity = nearbyVehicles.GetFirst(); nearbyEntity; nearbyEntity = nearbyVehicles.GetNext())
	{
		const CVehicle* nearbyVehicle = static_cast<const CVehicle*>(nearbyEntity);

		const bool isOnTop = ComputeIsVehicleOnTopOfVehicle(vehicleToConsiderAsBottom, *nearbyVehicle);
		if(!isOnTop)
		{
			continue;
		}

		vehiclesOnTopList.Grow() = nearbyVehicle;
	}
}

bool camThirdPersonCamera::ComputeIsVehicleOnTopOfVehicle(const CVehicle& vehicleToConsiderAsBottom, const CVehicle& vehicleToConsiderOnTop) const
{
	//Ignore aircraft, as it's pretty easy for them to end up on top and we don't want to push beyond them.
	if(vehicleToConsiderOnTop.GetIsAircraft())
	{
		return false;
	}

	//Sanity check that the bounding spheres overlap, before attempting more expensive checks.
	const bool isTouching = vehicleToConsiderAsBottom.GetIsTouching(&vehicleToConsiderOnTop);
	if(!isTouching)
	{
		return false;
	}

	const bool areVehicleWheelsInContact = vehicleToConsiderOnTop.HasContactWheels();
	if(areVehicleWheelsInContact)
	{
		const int numWheels = vehicleToConsiderOnTop.GetNumWheels();
		for(int i=0; i<numWheels; i++)
		{
			const CWheel* wheel = vehicleToConsiderOnTop.GetWheel(i);
			if(wheel)
			{
				const CPhysical* hitPhysical = wheel->GetHitPhysical();
				if(hitPhysical == &vehicleToConsiderAsBottom)
				{
					return true;
				}
			}
		}
	}

	//Fall back to considering the ground physical of the vehicle that was determined in ProcessPreComputeImpacts. This handles cases where the wheels
	//are not in contact.

	if(vehicleToConsiderOnTop.m_pGroundPhysical.Get() != &vehicleToConsiderAsBottom)
	{
		//NOTE: Boats do not set a ground physical, as they do not call the base implementation within their overridden ProcessPreComputeImpacts.
		//It is too late to deal with this in single player, but we can attempt to work around this in multiplayer, by checking the collision history.
		//NOTE: We are handling jet skis specially here, as they have the most scope to sit on the back of another boat and they are small enough that
		//we can safely push beyond their bounds without a significant risk of constraining against water and then visibly clipping into the 'top' boat.

		bool isVehicleOnTopOfVehicleInCollisionHistory = false;

		if(NetworkInterface::IsGameInProgress() && vehicleToConsiderOnTop.GetIsJetSki())
		{
			const CCollisionHistory* collisionHistory = vehicleToConsiderOnTop.GetFrameCollisionHistory();
			if(collisionHistory && (collisionHistory->GetNumCollidedEntities() > 0))
			{
				const CCollisionRecord* collisionRecord = collisionHistory->GetFirstVehicleCollisionRecord();
				while(collisionRecord)
				{
					const CVehicle* otherVehicle = static_cast<const CVehicle*>(collisionRecord->m_pRegdCollisionEntity.Get());
					if(otherVehicle == &vehicleToConsiderAsBottom)
					{
						//Check that the normal is generally pointing up.
						if(collisionRecord->m_MyCollisionNormal.z >= 0.5f)
						{
							//Check that this boat is generally above the collision point.
							//NOTE: We use the centre of the this boat's bounding sphere, rather than the position of its transform,
							//which can be authored strangely.

							Vector3 boundCentre;
							vehicleToConsiderOnTop.GetBoundCentre(boundCentre);

							if(boundCentre.z > collisionRecord->m_OtherCollisionPos.z)
							{
								isVehicleOnTopOfVehicleInCollisionHistory = true;
								break;
							}
						}
					}

					collisionRecord = collisionRecord->GetNext();
				}
			}
		}

		if(!isVehicleOnTopOfVehicleInCollisionHistory)
		{
			return false;
		}
	}

	//NOTE: The ground physical pointer isn't a 100% reliable source of information and it can report a physical if the impact happens to be inside
	//a BVH vehicle interior, for example. To mitigate against this issue, check below the bottom vehicle and see if we detect the prospective vehicle on top.

	//We can safely avoid this test for bikes and quad bikes, as we know that they are convex.
	if(vehicleToConsiderOnTop.InheritsFromBike() || vehicleToConsiderOnTop.InheritsFromQuadBike() || vehicleToConsiderOnTop.InheritsFromAmphibiousQuadBike())
	{
		return true;
	}

	WorldProbe::CShapeTestProbeDesc probeTest;
	WorldProbe::CShapeTestFixedResults<1> probeTestResults;
	probeTest.SetResultsStructure(&probeTestResults);
	probeTest.SetIncludeFlags(ArchetypeFlags::GTA_VEHICLE_TYPE);
	probeTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	probeTest.SetContext(WorldProbe::LOS_Camera);
	probeTest.SetIsDirected(true);

	probeTest.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
	probeTest.SetIncludeEntity(&vehicleToConsiderOnTop, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);

	const Vector3 startPosition = VEC3V_TO_VECTOR3(vehicleToConsiderAsBottom.GetTransform().GetPosition());

	Vector3 endPosition(startPosition);
	endPosition.AddScaled(ZAXIS, -m_Metadata.m_VehicleOnTopOfVehicleCollisionSettings.m_DistanceToTestDownForVehiclesToReject);

	probeTest.SetStartAndEnd(startPosition, endPosition);

	const bool hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(probeTest);

	return !hasHit;
}

float camThirdPersonCamera::ComputeMaxSafeCollisionRadiusForAttachParent() const
{
#if RSG_PC
	if(camInterface::IsTripleHeadDisplay())
	{
		const float fov = m_Frame.GetFov();
		const float nearZ = m_Metadata.m_NearClipForTripleHead;/*m_Frame.GetNearClip()*/
		
		const float sceneWidth	= static_cast<float>(VideoResManager::GetSceneWidth());
		const float sceneHeight	= static_cast<float>(VideoResManager::GetSceneHeight());
		const float viewportAspect = sceneWidth / sceneHeight;
		const float tanVFOV = Tanf(0.5f * fov * DtoR);
		const float tanHFOV = tanVFOV * viewportAspect;	
		const float z0      = nearZ;

		Vector3 nearPlaneCorners[4];
		nearPlaneCorners[0] = Vector3(-tanHFOV, 1.0f, +tanVFOV) * z0; // 0: LEFT  TOP NEAR
		nearPlaneCorners[1] = Vector3(+tanHFOV, 1.0f, +tanVFOV) * z0; // 1: RIGHT TOP NEAR
		nearPlaneCorners[2] = Vector3(+tanHFOV, 1.0f, -tanVFOV) * z0; // 2: RIGHT BOT NEAR
		nearPlaneCorners[3] = Vector3(-tanHFOV, 1.0f, -tanVFOV) * z0; // 3: LEFT  BOT NEAR

		Matrix34 worldMatrix = m_Frame.GetWorldMatrix();
		const Vector3 cameraPosition = worldMatrix.d;
		const Vector3 cameraPositionToCollisionOrigin = m_CollisionRootPosition - cameraPosition;
		const float cameraPositionToCollisionOriginMag = cameraPositionToCollisionOrigin.Mag();		

		float maxDistanceToFrustumCorner = 0.0f;
		for(int i = 0; i < 4; i++)
		{
			Vector3 frustumCorner = nearPlaneCorners[i];
			worldMatrix.Transform(frustumCorner);
			
			// http://mathworld.wolfram.com/Point-LineDistance3-Dimensional.html
			Vector3 triangularCross = (frustumCorner - cameraPosition);
			triangularCross.Cross(frustumCorner - m_CollisionRootPosition);
			const float pointLineDistance = cameraPositionToCollisionOriginMag >= SMALL_FLOAT ? (triangularCross.Mag() / cameraPositionToCollisionOriginMag) : 0.0f;

			maxDistanceToFrustumCorner = Max(maxDistanceToFrustumCorner, pointLineDistance);			
		}

		if(maxDistanceToFrustumCorner >= SMALL_FLOAT)
		{		
			return maxDistanceToFrustumCorner + m_Metadata.m_MinSafeRadiusReductionWithinPedMoverCapsule;
		}
	}
#endif	

	float maxSafeCollisionRadiusForAttachParent = m_Metadata.m_MaxCollisionTestRadius;

	if(m_AttachParent && m_AttachParent->GetIsTypePed())
	{
		const CPed* attachPed	= static_cast<const CPed*>(m_AttachParent.Get());
		float radius			= attachPed->GetCurrentMainMoverCapsuleRadius();
		radius					-= m_Metadata.m_MinSafeRadiusReductionWithinPedMoverCapsule;
		if(radius >= SMALL_FLOAT)
		{
			maxSafeCollisionRadiusForAttachParent = Min(radius, maxSafeCollisionRadiusForAttachParent);
		}
	}

	//TODO: Handle very narrow vehicle bounds, such as those associated with bikes.

	return maxSafeCollisionRadiusForAttachParent;
}

void camThirdPersonCamera::UpdateCollisionOrigin(float& collisionTestRadius)
{
	//Determine a safe (ultimate) origin for our collision tests.

	if(m_Metadata.m_ShouldUseCustomCollisionOrigin)
	{
		//NOTE: We cannot test that the custom collision origin is safe w.r.t. the fall back position, as the fall back position itself
		//may not be safe in the cases we are overriding. We instead assume that custom position is valid as specified.
		const Matrix34 attachParentMatrix = MAT34V_TO_MATRIX34(m_AttachParent->GetMatrix());
#if RSG_PC
		if(m_Metadata.m_ShouldUseCustomCollisionOriginForTripleHead)
		{			
			if(camInterface::IsTripleHeadDisplay())
			{
				if(m_Metadata.m_ShouldApplySideAdjustedCollisionOriginRelativePositionForTripleHead)
				{
					Vector3 sideAdjustedCollisionOriginRelativePositionForTripleHead(m_Metadata.m_CustomCollisionOriginRelativePositionForTripleHead);

					Vector3 collisionOrigin;
					attachParentMatrix.Transform(sideAdjustedCollisionOriginRelativePositionForTripleHead, collisionOrigin);

					Vector3 collisionOriginLeftExtent;
					attachParentMatrix.Transform(sideAdjustedCollisionOriginRelativePositionForTripleHead - m_Metadata.m_ExtentsForSideAdjustedCollisionOriginRelativePositionTripleHead, collisionOriginLeftExtent);

					Vector3 collisionOriginRightExtent;
					attachParentMatrix.Transform(sideAdjustedCollisionOriginRelativePositionForTripleHead + m_Metadata.m_ExtentsForSideAdjustedCollisionOriginRelativePositionTripleHead, collisionOriginRightExtent);

					// Test the left extent
					WorldProbe::CShapeTestCapsuleDesc capsuleTest;
					WorldProbe::CShapeTestFixedResults<> shapeTestResults;
					capsuleTest.SetResultsStructure(&shapeTestResults);
					capsuleTest.SetIsDirected(true);
					capsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
					capsuleTest.SetContext(WorldProbe::LOS_Camera);

					const u32 collisionIncludeFlags	= camCollision::GetCollisionIncludeFlags();
					capsuleTest.SetIncludeFlags(collisionIncludeFlags);

					capsuleTest.SetCapsule(collisionOrigin, collisionOriginLeftExtent, m_Metadata.m_SideAdjustedCollisionTestRadius);
					
					bool hasHit					= WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);
					const float hitTValueLeft	= hasHit ? shapeTestResults[0].GetHitTValue() : 1.0f;

					capsuleTest.Reset();
					shapeTestResults.Reset();
					capsuleTest.SetResultsStructure(&shapeTestResults);
					capsuleTest.SetIsDirected(true);
					capsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
					capsuleTest.SetContext(WorldProbe::LOS_Camera);

					// Test the right extent
					capsuleTest.SetCapsule(collisionOrigin, collisionOriginRightExtent, m_Metadata.m_SideAdjustedCollisionTestRadius);

					hasHit						= WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);
					const float hitTValueRight	= hasHit ? shapeTestResults[0].GetHitTValue() : 1.0f;

					const Vector3 adjustedCollisionOriginRelativePositionForTripleHead =	sideAdjustedCollisionOriginRelativePositionForTripleHead + 
																							m_Metadata.m_ExtentsForSideAdjustedCollisionOriginRelativePositionTripleHead * (1.0f - hitTValueLeft) - 
																							m_Metadata.m_ExtentsForSideAdjustedCollisionOriginRelativePositionTripleHead * (1.0f - hitTValueRight);

					attachParentMatrix.Transform(adjustedCollisionOriginRelativePositionForTripleHead, m_CollisionOrigin);
					return;
				}

				attachParentMatrix.Transform(m_Metadata.m_CustomCollisionOriginRelativePositionForTripleHead, m_CollisionOrigin);
				return;
			}
		}
#endif
		attachParentMatrix.Transform(m_Metadata.m_CustomCollisionOriginRelativePosition, m_CollisionOrigin);

		return;
	}

	ComputeFallBackForCollisionOrigin(m_CollisionOrigin);

	if(!m_Metadata.m_ShouldIgnoreCollisionWithAttachParent)
	{
		return;
	}

	const float attachParentPositionToBaseAttachDistance2 = m_CollisionOrigin.Dist2(m_BaseAttachPosition);
	if(attachParentPositionToBaseAttachDistance2 < VERY_SMALL_FLOAT)
	{
		return;
	}

	if(!cameraVerifyf(collisionTestRadius >= SMALL_FLOAT,
		"Unable to compute a valid collision origin due to an invalid test radius."))
	{
		return;
	}

	WorldProbe::CShapeTestCapsuleDesc capsuleTest;
	WorldProbe::CShapeTestFixedResults<> shapeTestResults;
	capsuleTest.SetResultsStructure(&shapeTestResults);
	capsuleTest.SetIsDirected(true);
	capsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	capsuleTest.SetContext(WorldProbe::LOS_Camera);

	const u32 collisionIncludeFlags	= camCollision::GetCollisionIncludeFlags();
	capsuleTest.SetIncludeFlags(collisionIncludeFlags);

	//NOTE: m_Collision has already been validated.
	const CEntity** entitiesToIgnore	= const_cast<const CEntity**>(m_Collision->GetEntitiesToIgnoreThisUpdate());
	s32 numEntitiesToIgnore				= m_Collision->GetNumEntitiesToIgnoreThisUpdate();

	atArray<const CEntity*> entitiesToIgnoreList;
	const CVehicle* followVehicle = camInterface::GetGameplayDirector().GetFollowVehicle();

	if(m_Metadata.m_ShouldIgnoreFollowVehicleForCollisionOrigin && followVehicle)
	{
		if(numEntitiesToIgnore > 0)
		{
			entitiesToIgnoreList.CopyFrom(entitiesToIgnore, (u16)numEntitiesToIgnore);
		}

		camCollision::AddEntityToList(*followVehicle, entitiesToIgnoreList);

		entitiesToIgnore	= entitiesToIgnoreList.GetElements();
		numEntitiesToIgnore	= entitiesToIgnoreList.GetCount();

		capsuleTest.SetExcludeEntities(entitiesToIgnore, numEntitiesToIgnore, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
	}
	else if(numEntitiesToIgnore > 0)
	{
		capsuleTest.SetExcludeEntities(entitiesToIgnore, numEntitiesToIgnore, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
	}

	capsuleTest.SetCapsule(m_CollisionOrigin, m_BaseAttachPosition, collisionTestRadius);

	//Reduce the test radius in readiness for use in a consecutive test.
	collisionTestRadius		-= camCollision::GetMinRadiusReductionBetweenInterativeShapetests();

	const bool hasHit		= WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);
	const float hitTValue	= hasHit ? shapeTestResults[0].GetHitTValue() : 1.0f;

	//TODO: Apply damping when the origin is moving away from the attach parent position.

	m_CollisionOrigin.Lerp(hitTValue, m_BaseAttachPosition);
}

void camThirdPersonCamera::ComputeFallBackForCollisionOrigin(Vector3& fallBackPosition) const
{
	//NOTE: We use the simple matrix of the attach parent directly, not m_AttachParentMatrix. This ensures we use the real entity position and
	//not the result of an interpolation.
	const Matrix34 attachParentMatrixToConsider(MAT34V_TO_MATRIX34(m_AttachParent->GetMatrix()));

	fallBackPosition.Set(attachParentMatrixToConsider.d);
}

void camThirdPersonCamera::UpdateCollisionRootPosition(const Vector3& cameraPosition, float& collisionTestRadius)
{
	WorldProbe::CShapeTestCapsuleDesc capsuleTest;
	WorldProbe::CShapeTestFixedResults<> shapeTestResults;
	capsuleTest.SetResultsStructure(&shapeTestResults);
	capsuleTest.SetIsDirected(true);
	capsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	capsuleTest.SetContext(WorldProbe::LOS_Camera);

	u32 collisionIncludeFlags = camCollision::GetCollisionIncludeFlags();
	capsuleTest.SetIncludeFlags(collisionIncludeFlags);

	//NOTE: m_Collision has already been validated.
	const CEntity** entitiesToIgnore	= const_cast<const CEntity**>(m_Collision->GetEntitiesToIgnoreThisUpdate());
	s32 numEntitiesToIgnore				= m_Collision->GetNumEntitiesToIgnoreThisUpdate();

	atArray<const CEntity*> entitiesToIgnoreList;
	const CVehicle* followVehicle = camInterface::GetGameplayDirector().GetFollowVehicle();

	if(m_Metadata.m_ShouldIgnoreFollowVehicleForCollisionRoot && followVehicle)
	{
		if(numEntitiesToIgnore > 0)
		{
			entitiesToIgnoreList.CopyFrom(entitiesToIgnore, (u16)numEntitiesToIgnore);
		}

		camCollision::AddEntityToList(*followVehicle, entitiesToIgnoreList);

		entitiesToIgnore	= entitiesToIgnoreList.GetElements();
		numEntitiesToIgnore	= entitiesToIgnoreList.GetCount();

		capsuleTest.SetExcludeEntities(entitiesToIgnore, numEntitiesToIgnore, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
	}
	else if(numEntitiesToIgnore > 0)
	{
		capsuleTest.SetExcludeEntities(entitiesToIgnore, numEntitiesToIgnore, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
	}

	Vector3 collisionFallBackPosition;
	ComputeCollisionFallBackPosition(cameraPosition, collisionTestRadius, capsuleTest, shapeTestResults, collisionFallBackPosition);

	//Now determine the ideal root position between the fall back and pivot positions.

	const float collisionRootPositionFallBackToPivotBlendValue = ComputeCollisionRootPositionFallBackToPivotBlendValue();

	Vector3 idealCollisionRootPosition;
	idealCollisionRootPosition.Lerp(collisionRootPositionFallBackToPivotBlendValue, collisionFallBackPosition, m_PivotPosition);

	//Finally, use the farthest safe position from the fall back out towards the ideal collision root position.

	m_CollisionRootPosition = collisionFallBackPosition;

	const float fallBackToPivotDistance2 = collisionFallBackPosition.Dist2(idealCollisionRootPosition);
	if(fallBackToPivotDistance2 > VERY_SMALL_FLOAT)
	{
		if(!cameraVerifyf(collisionTestRadius >= SMALL_FLOAT,
			"Unable to compute a valid collision root position due to an invalid test radius."))
		{
			return;
		}

		if(m_Metadata.m_ShouldConstrainCollisionRootPositionAgainstClippingTypes)
		{
			collisionIncludeFlags = camCollision::GetCollisionIncludeFlagsForClippingTests();
			capsuleTest.SetIncludeFlags(collisionIncludeFlags);
		}

		shapeTestResults.Reset();
		capsuleTest.SetCapsule(collisionFallBackPosition, idealCollisionRootPosition, collisionTestRadius);

		//Reduce the test radius in readiness for use in a consecutive test.
		collisionTestRadius		-= camCollision::GetMinRadiusReductionBetweenInterativeShapetests();

		const bool hasHit				= WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);
		const float desiredBlendLevel	= hasHit ? shapeTestResults[0].GetHitTValue() : 1.0f;

		const float blendLevelOnPreviousUpdate = m_CollisionRootSpring.GetResult();

		//NOTE: We must cut to a lesser blend level in order to avoid clipping the collision root position.
		const bool shouldCutSpring		= ((m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
											m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation) ||
											(desiredBlendLevel < blendLevelOnPreviousUpdate));
		const float springConstant		= shouldCutSpring ? 0.0f : m_Metadata.m_CollisionRootPositionSpringConstant;

		float blendLevelToApply	= m_CollisionRootSpring.Update(desiredBlendLevel, springConstant,
											m_Metadata.m_CollisionRootPositionSpringDampingRatio, true);

		blendLevelToApply		= Max(blendLevelToApply, m_Metadata.m_CollisionFallBackPosition.m_MinBlendLevelToIdealPosition);

		m_CollisionRootPosition.Lerp(blendLevelToApply, idealCollisionRootPosition);
	}
}

void camThirdPersonCamera::ComputeCollisionFallBackPosition(const Vector3& UNUSED_PARAM(cameraPosition), float& collisionTestRadius,
	WorldProbe::CShapeTestCapsuleDesc& capsuleTest, WorldProbe::CShapeTestResults& shapeTestResults,
	Vector3& collisionFallBackPosition)
{
	//Fall-back to an offset from the (safe) collision origin.

	collisionFallBackPosition = m_CollisionOrigin;

	Vector3 relativeCollisionOrigin;
	m_AttachParentMatrix.UnTransform(m_CollisionOrigin, relativeCollisionOrigin); //Transform to parent-local space.

	const float parentHeight = m_AttachParentBoundingBoxMax.z - m_AttachParentBoundingBoxMin.z;

	const float attachParentHeightRatioToAttain = ComputeAttachParentHeightRatioToAttainForCollisionFallBackPosition();

	float extraOffset					= m_AttachPedPelvisOffsetSpring.GetResult();
	const float extraOffsetForHighHeels	= ComputeAttachPedPelvisOffsetForShoes();
	extraOffset							+= extraOffsetForHighHeels;

	extraOffset							+= ComputeExtraOffsetForHatsAndHelmets();

	const float collisionFallBackOffset	= (parentHeight * attachParentHeightRatioToAttain) +
											m_AttachParentBoundingBoxMin.z - relativeCollisionOrigin.z + extraOffset;

	if(ShouldApplyInAttachParentLocalSpace())
	{
		collisionFallBackPosition	+= collisionFallBackOffset * m_AttachParentMatrix.c;
	}
	else
	{
		collisionFallBackPosition.z	+= collisionFallBackOffset;
	}

	if(m_Metadata.m_ShouldIgnoreCollisionWithAttachParent)
	{
		//Ensure that our fall back position is actually clear of collision, pushing away as needed.

		const float collisionOriginToFallBackDistance2 = m_CollisionOrigin.Dist2(collisionFallBackPosition);
		if(collisionOriginToFallBackDistance2 > VERY_SMALL_FLOAT)
		{
			if(!cameraVerifyf(collisionTestRadius >= SMALL_FLOAT,
				"Unable to compute a valid collision fall back position due to an invalid test radius."))
			{
				return;
			}

			const float minDistanceToPushAwayFromCollision = (parentHeight * m_Metadata.m_CollisionFallBackPosition.m_MinAttachParentHeightRatioToPushAwayFromCollision);
			if(ShouldApplyInAttachParentLocalSpace())
			{
				collisionFallBackPosition	+= minDistanceToPushAwayFromCollision * m_AttachParentMatrix.c;
			}
			else
			{
				collisionFallBackPosition.z	+= minDistanceToPushAwayFromCollision;
			}

			shapeTestResults.Reset();
			capsuleTest.SetCapsule(m_CollisionOrigin, collisionFallBackPosition, collisionTestRadius);

			//Reduce the test radius in readiness for use in a consecutive test.
			collisionTestRadius -= camCollision::GetMinRadiusReductionBetweenInterativeShapetests();
			WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);

			bool hasHit	= false;
			float desiredBlendLevel = 1.0f;
			for(int index = 0; index < shapeTestResults.GetNumHits(); ++index)
			{
				const CEntity* hitEntity = shapeTestResults[index].GetHitEntity();
				if(hitEntity && hitEntity->GetModelNameHash() == ATSTRINGHASH("h4_prop_h4_chair_01a", 0x290AEB43))
				{
					cameraDebugf1("Fallback ignoring hit entity %s", hitEntity->GetModelName());
				}
				else
				{
					hasHit = true;
					desiredBlendLevel = shapeTestResults[index].GetHitTValue();
					cameraDebugf2("Fallback collision test hit entity %s with tval %f", hitEntity ? hitEntity->GetModelName() : "NULL", desiredBlendLevel);
					break;
				}
			}

			const float blendLevelOnPreviousUpdate = m_CollisionFallBackSpring.GetResult();

			//NOTE: We must cut to a lesser blend level in order to avoid clipping the collision fall back position.
			const bool shouldCutSpring		= ((m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
												m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation) ||
												(desiredBlendLevel < blendLevelOnPreviousUpdate));
			const float springConstant		= shouldCutSpring ? 0.0f : m_Metadata.m_CollisionFallBackPosition.m_SpringConstant;

			float blendLevelToApply			= m_CollisionFallBackSpring.Update(desiredBlendLevel, springConstant,
												m_Metadata.m_CollisionFallBackPosition.m_SpringDampingRatio, true);

			if(minDistanceToPushAwayFromCollision >= SMALL_FLOAT)
			{
				//Compensate for the distance we extended the test and push away if we hit something.
				const float capsuleLength	= m_CollisionOrigin.Dist(collisionFallBackPosition);
				if(capsuleLength >= SMALL_FLOAT)
				{
					const float pushAwayRatio	= minDistanceToPushAwayFromCollision / capsuleLength;
#if RSG_PC
					if(m_Metadata.m_CollisionFallBackPosition.m_ShouldApplyMinBlendLevelAfterPushAwayFromCollisionForTripleHead && 
						camInterface::IsTripleHeadDisplay() && 
						!camInterface::GetGameplayDirector().GetScriptPreventCollisionSettingsForTripleHeadInInteriorsThisUpdate())
					{
						if(ShouldApplyCollisionSettingsForTripleHeadInInteriors())
						{
							const float maxBlendLevel	= Lerp(m_TightSpaceSpring.GetResult(), m_Metadata.m_CollisionFallBackPosition.m_MinBlendLevelAfterPushAwayFromCollision, m_Metadata.m_CollisionFallBackPosition.m_MinBlendLevelAfterPushAwayFromCollisionForTripleHead);
							blendLevelToApply			= Max(blendLevelToApply - pushAwayRatio, maxBlendLevel);
						}
						else
						{
							blendLevelToApply			= Max(blendLevelToApply - pushAwayRatio, m_Metadata.m_CollisionFallBackPosition.m_MinBlendLevelAfterPushAwayFromCollisionForTripleHead);
						}
					}
					else
					{
#endif					
						blendLevelToApply			= Max(blendLevelToApply - pushAwayRatio, m_Metadata.m_CollisionFallBackPosition.m_MinBlendLevelAfterPushAwayFromCollision);
#if RSG_PC
					}
#endif
				}
			}

			collisionFallBackPosition.Lerp(blendLevelToApply, m_CollisionOrigin, collisionFallBackPosition);
		}
	}
}

float camThirdPersonCamera::ComputeAttachParentHeightRatioToAttainForCollisionFallBackPosition() const
{
	return m_Metadata.m_CollisionFallBackPosition.m_AttachParentHeightRatioToAttain;
}

float camThirdPersonCamera::ComputeCollisionRootPositionFallBackToPivotBlendValue() const
{
	return m_Metadata.m_CollisionRootPositionFallBackToPivotBlendValue;
}

void camThirdPersonCamera::ComputeOrbitDistanceLimitsForBasePosition( Vector2& orbitDistanceLimitsForBasePosition ) const
{
	orbitDistanceLimitsForBasePosition = m_Metadata.m_OrbitDistanceLimitsForBasePosition;
}

void camThirdPersonCamera::UpdateLookAt(const Vector3& orbitFront)
{
	const Vector3& postCollisionCameraPosition	= m_Frame.GetPosition();
	const float preToPostCollisionBlendValue	= ComputePreToPostCollisionLookAtOrientationBlendValue();
	const bool wasCameraConstrainedOffAxis		= (m_Collision && m_Collision->WasCameraConstrainedOffAxisOnPreviousUpdate());

	Vector3 lookAtFront;
	ComputeLookAtFront(m_CollisionRootPosition, m_BasePivotPosition, m_BaseFront, orbitFront, m_PivotPosition, m_PreCollisionCameraPosition,
		postCollisionCameraPosition, preToPostCollisionBlendValue, wasCameraConstrainedOffAxis, lookAtFront);

	m_Frame.SetWorldMatrixFromFront(lookAtFront);

	//Compute the effective look-at position.
	ComputeLookAtPositionFromFront(lookAtFront, m_BasePivotPosition, orbitFront, postCollisionCameraPosition, m_LookAtPosition);
}

float camThirdPersonCamera::ComputePreToPostCollisionLookAtOrientationBlendValue() const
{
	return m_Metadata.m_PreToPostCollisionLookAtOrientationBlendValue;
}

void camThirdPersonCamera::ComputeLookAtFront(const Vector3& collisionRootPosition, const Vector3& basePivotPosition,
	const Vector3& baseFront, const Vector3& orbitFront, const Vector3& pivotPosition, const Vector3& preCollisionCameraPosition,
	const Vector3& postCollisionCameraPosition, float preToPostCollisionBlendValue, bool wasCameraConstrainedOffAxis, Vector3& lookAtFront) const
{
	ComputeBaseLookAtFront(collisionRootPosition, basePivotPosition, baseFront, pivotPosition, preCollisionCameraPosition,
		postCollisionCameraPosition, preToPostCollisionBlendValue, wasCameraConstrainedOffAxis, lookAtFront);

	if(m_CatchUpHelper)
	{
		//NOTE: An explicit const_cast is used here to avoid a refactor. UpdateLookAtOrientationDelta() should only act in a non-const manner on the
		//initial update of the catch-up, where it must cache some data. This should only occur within the normal third-person camera update.
		//TODO: Refactor these look-at helpers to allow for both const and non-const APIs.
		const bool isCatchUpFrontValid	= const_cast<camCatchUpHelper*>(m_CatchUpHelper)->UpdateLookAtOrientationDelta(basePivotPosition, orbitFront,
											postCollisionCameraPosition, lookAtFront);
		if(!isCatchUpFrontValid)
		{
			//We have an invalid catch-up front, so fall-back to rendering the look-at orientation from the previous update, as this is likely to be
			//closer to the expected blended orientation than our raw desired look-at orientation.
			lookAtFront.Set(m_Frame.GetFront());
		}
	}

	const camBaseSwitchHelper* switchHelper = camInterface::GetGameplayDirector().GetSwitchHelper();
	if(switchHelper)
	{
		switchHelper->ComputeLookAtFront(postCollisionCameraPosition, lookAtFront);
	}
}

void camThirdPersonCamera::ComputeBaseLookAtFront(const Vector3& collisionRootPosition, const Vector3& UNUSED_PARAM(basePivotPosition),
	const Vector3& UNUSED_PARAM(baseFront), const Vector3& pivotPosition, const Vector3& preCollisionCameraPosition,
	const Vector3& postCollisionCameraPosition, float preToPostCollisionBlendValue, bool wasCameraConstrainedOffAxis, Vector3& lookAtFront) const
{
	const Vector3& lookAtFrontOnPreviousUpdate = m_Frame.GetFront();

	//Compute a look-at front prior to collision, camera buoyancy and any other post-collision behaviours.
	Vector3 preCollisionLookAtFront = pivotPosition - preCollisionCameraPosition;
	preCollisionLookAtFront.NormalizeSafe(lookAtFrontOnPreviousUpdate);

	if(!wasCameraConstrainedOffAxis || (preToPostCollisionBlendValue < SMALL_FLOAT))
	{
		lookAtFront.Set(preCollisionLookAtFront);

		return;
	}

	//The camera constrained off the normal collision axis (between the pre-collision and collision root positions), so we must
	//take account of this movement.

	//Compute a look-at front after collision, camera buoyancy and any other post-collision behaviours have been applied, taking account of
	//any movement of the camera towards the root as a result of collision.
	const float preCollisionRootToCameraDistance	= collisionRootPosition.Dist(preCollisionCameraPosition);
	const float postCollisionRootToCameraDistance	= collisionRootPosition.Dist(postCollisionCameraPosition);
	const float lookAtRootToPivotLerpValue			= (preCollisionRootToCameraDistance < SMALL_FLOAT) ? 0.0f :
														Clamp(postCollisionRootToCameraDistance / preCollisionRootToCameraDistance, 0.0f, 1.0f);
	Vector3 postCollisionLookAtPosition;
	postCollisionLookAtPosition.Lerp(lookAtRootToPivotLerpValue, collisionRootPosition, pivotPosition);

	Vector3 postCollisionLookAtFront = postCollisionLookAtPosition - postCollisionCameraPosition;
	postCollisionLookAtFront.NormalizeSafe(lookAtFrontOnPreviousUpdate);

	//Now blend the pre- and post-collision look-at fronts, thereby controlling the extent to which collision, camera buoyancy and any other
	//post-collision behaviours can affect the look-at orientation.
	camFrame::SlerpOrientation(preToPostCollisionBlendValue, preCollisionLookAtFront, postCollisionLookAtFront, lookAtFront);
}

void camThirdPersonCamera::ComputeLookAtPositionFromFront(const Vector3& lookAtFront, const Vector3& basePivotPosition, const Vector3& orbitFront,
	const Vector3& postCollisionCameraPosition, Vector3& lookAtPosition)
{
	//Project the post-collision camera position along the look-at front onto the plane containing the base pivot and pivot positions, the normal of
	//which is the orbit front.

	float distanceToPlane = 0.0f;

	const float denominator = lookAtFront.Dot(orbitFront);
	if(!IsNearZero(denominator, SMALL_FLOAT))
	{
		const float numerator	= (basePivotPosition - postCollisionCameraPosition).Dot(orbitFront);
		distanceToPlane			= numerator / denominator;
	}

	//Do not allow the look-at position to converge with the camera position.
	distanceToPlane = Max(distanceToPlane, g_MinDistanceFromCameraToLookAtPosition);

	lookAtPosition = postCollisionCameraPosition + (distanceToPlane * lookAtFront);
}

void camThirdPersonCamera::UpdateRoll(const Matrix34& orbitMatrix)
{
	float desiredRoll = ComputeDesiredRoll(orbitMatrix);

	if(m_CatchUpHelper)
	{
		m_CatchUpHelper->ComputeDesiredRoll(desiredRoll);
	}

	float heading;
	float pitch;
	float tempRoll;
	m_Frame.ComputeHeadingPitchAndRollFromMatrixUsingEulers(m_Frame.GetWorldMatrix(), heading, pitch, tempRoll);
	m_Frame.SetWorldMatrixFromHeadingPitchAndRoll(heading, pitch, desiredRoll);
}

float camThirdPersonCamera::ComputeDesiredRoll(const Matrix34& orbitMatrix)
{
	if(!ShouldOrbitRelativeToAttachParentOrientation())
	{
		return 0.0f;
	}

	//NOTE: We apply damping to the roll of the attach parent, rather than the orbit orientation, as we do not want to apply damping
	//to camera behaviours that affect the orbit orientation, such as look-around.

	Matrix34 relativeOrbitMatrix;
	relativeOrbitMatrix.Dot3x3Transpose(orbitMatrix, m_AttachParentMatrixToConsiderForRelativeOrbit);

	float attachParentHeading;
	float attachParentPitch;
	float attachParentRoll;
	camFrame::ComputeHeadingPitchAndRollFromMatrixUsingEulers(m_AttachParentMatrixToConsiderForRelativeOrbit, attachParentHeading, attachParentPitch, attachParentRoll);

	//Blend in a hard limit on the damping as the pitch of the attach parent approaches world up/down. This allows us to work-around an inherent discontinuity
	//in the roll of the attach parent.

	float dampingLimitBlendLevel = 0.0f;
	if(attachParentPitch < (m_Metadata.m_AttachParentRollDampingPitchSoftLimits.x * DtoR))
	{
		dampingLimitBlendLevel	= RampValueSafe(attachParentPitch, (m_Metadata.m_AttachParentRollDampingPitchHardLimits.x * DtoR),
									(m_Metadata.m_AttachParentRollDampingPitchSoftLimits.x * DtoR), 1.0f, 0.0f);
	}
	else if(attachParentPitch > (m_Metadata.m_AttachParentRollDampingPitchSoftLimits.y * DtoR))
	{
		dampingLimitBlendLevel	= RampValueSafe(attachParentPitch, (m_Metadata.m_AttachParentRollDampingPitchSoftLimits.y * DtoR),
									(m_Metadata.m_AttachParentRollDampingPitchHardLimits.y * DtoR), 0.0f, 1.0f);
	}

	dampingLimitBlendLevel = SlowInOut(dampingLimitBlendLevel);

	Matrix34 orbitMatrixWithDampedAttachParentRoll;
	if(dampingLimitBlendLevel > (1.0f - SMALL_FLOAT))
	{
		m_AttachParentRollSpring.Reset(attachParentRoll);
		orbitMatrixWithDampedAttachParentRoll = orbitMatrix;
	}
	else
	{
		//Ensure that we blend to the desired orientation over the shortest angle.
		float unwrappedAttachParentRoll		= attachParentRoll;
		const float rollOnPreviousUpdate	= m_AttachParentRollSpring.GetResult();
		const float desiredRollDelta		= attachParentRoll - rollOnPreviousUpdate;
		if(desiredRollDelta > PI)
		{
			unwrappedAttachParentRoll -= TWO_PI;
		}
		else if(desiredRollDelta < -PI)
		{
			unwrappedAttachParentRoll += TWO_PI;
		}

		float rollToApplyForAttachParent;
		const bool shouldCutSpring	= ((m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
										m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation));
		const float springConstant	= shouldCutSpring ? 0.0f : m_Metadata.m_AttachParentRollSpringConstant;
		rollToApplyForAttachParent	= m_AttachParentRollSpring.Update(unwrappedAttachParentRoll, springConstant,
										m_Metadata.m_AttachParentRollSpringDampingRatio);
		rollToApplyForAttachParent	= fwAngle::LimitRadianAngle(rollToApplyForAttachParent);

		rollToApplyForAttachParent	= fwAngle::LerpTowards(rollToApplyForAttachParent, attachParentRoll, dampingLimitBlendLevel);

		m_AttachParentRollSpring.OverrideResult(rollToApplyForAttachParent);
		PF_SET(desiredRollDampedAttachParentRoll, rollToApplyForAttachParent);

		Matrix34 attachParentMatrixWithDampedRoll;
		camFrame::ComputeWorldMatrixFromHeadingPitchAndRoll(attachParentHeading, attachParentPitch, rollToApplyForAttachParent, attachParentMatrixWithDampedRoll);

		orbitMatrixWithDampedAttachParentRoll.Dot3x3(relativeOrbitMatrix, attachParentMatrixWithDampedRoll);
	}

	float tempHeading;
	float tempPitch;
	float desiredRoll;
	camFrame::ComputeHeadingPitchAndRollFromMatrixUsingEulers(orbitMatrixWithDampedAttachParentRoll, tempHeading, tempPitch, desiredRoll);

	PF_SET(desiredRollAttachParentPitch,		attachParentPitch);
	PF_SET(desiredRollDampingLimitBlendLevel,	dampingLimitBlendLevel);
	PF_SET(desiredRollAttachParentRoll,			attachParentRoll);

	return desiredRoll;
}

void camThirdPersonCamera::UpdateLensParameters()
{
	UpdateFov();

	m_Frame.SetNearClip(m_Metadata.m_BaseNearClip);

#if RSG_PC
	if(camInterface::IsTripleHeadDisplay())
	{
		m_Frame.SetNearClip(m_Metadata.m_NearClipForTripleHead);
	}
#endif //RSG_PC

	m_Frame.ComputeDofFromFovAndClipRange();

	UpdateMotionBlur();

	if(m_CatchUpHelper)
	{
		m_CatchUpHelper->ComputeLensParameters(m_Frame);
	}
}

void camThirdPersonCamera::UpdateFov()
{
	float desiredFov = ComputeDesiredFov();

	//NOTE: We apply the hint and Switch zoom effect after all other effects.

	if(m_HintHelper)
	{
		m_HintHelper->ComputeHintZoomScalar(desiredFov);
	}

	const camBaseSwitchHelper* switchHelper = camInterface::GetGameplayDirector().GetSwitchHelper();
	if(switchHelper)
	{
		switchHelper->ComputeDesiredFov(desiredFov);
	}

	m_Frame.SetFov(desiredFov);

	UpdateCustomFovBlend(desiredFov);
}

float camThirdPersonCamera::ComputeDesiredFov()
{
	float desiredFov = ComputeBaseFov();

	//Zoom when an attach ped is using stealth.
	const float zoomFactor = m_StealthZoomSpring.GetResult();
	if(zoomFactor >= SMALL_FLOAT)
	{
		desiredFov /= zoomFactor;
	}

	return desiredFov;
}

float camThirdPersonCamera::ComputeMotionBlurStrength()
{
	const CPed* followPed = camInterface::FindFollowPed();
	if(followPed && (followPed->ShouldBeDead() || followPed->GetIsArrested()))
	{		
		return 0;
	}

	float moveStrength			= ComputeMotionBlurStrengthForMovement();
	float damageStrength		= ComputeMotionBlurStrengthForDamage();

	float motionBlurStrength	= moveStrength + damageStrength;	

	const camBaseSwitchHelper* switchHelper = camInterface::GetGameplayDirector().GetSwitchHelper();
	if(switchHelper)
	{
		switchHelper->ComputeDesiredMotionBlurStrength(motionBlurStrength);
	}

	return motionBlurStrength;
}

void camThirdPersonCamera::UpdateMotionBlur()
{		
	m_Frame.SetMotionBlurStrength(Clamp(ComputeMotionBlurStrength(), 0.0f, 1.0f));
}

float camThirdPersonCamera::ComputeMotionBlurStrengthForMovement()
{
	const camMotionBlurSettingsMetadata* motionBlurSettings = camFactory::FindObjectMetadata<camMotionBlurSettingsMetadata>(m_Metadata.m_MotionBlurSettings.GetHash());
	if(!motionBlurSettings)
	{
		return 0.0f;
	}

	if(motionBlurSettings->m_MovementMotionBlurMaxStrength < SMALL_FLOAT)
	{
		return 0.0f;
	}

	float moveSpeed = 0.0f;

	if(m_AttachParent->GetIsTypePed())
	{
		//Derive a motion blur strength for based upon fall speed for peds.
		moveSpeed = -Min(m_BaseAttachVelocityToConsider.z, 0.0f);
	}
	else if(m_AttachParent->GetIsTypeVehicle())
	{
        Vector3 velocityToConsider = m_BaseAttachVelocityToConsider;

        //Network clone vehicles can accelerate enough to catch up to their target positions
        //to kick in motion-blur, so we consider the last velocity received over the network
        const CVehicle *vehicle = static_cast<const CVehicle *>(m_AttachParent.Get());

        if(vehicle && vehicle->IsNetworkClone())
        {
            velocityToConsider = NetworkInterface::GetLastVelReceivedOverNetwork(vehicle);
        }

		//Derive a motion blur strength based upon the forward move speed for vehicles.
		const Vector3 vehicleFront	= VEC3V_TO_VECTOR3(m_AttachParent->GetTransform().GetB());
		moveSpeed					= DotProduct(velocityToConsider, vehicleFront);
	}
	else
	{
		moveSpeed = m_BaseAttachVelocityToConsider.Mag();
	}

	float motionBlurStrength	= RampValueSafe(moveSpeed, motionBlurSettings->m_MovementMotionBlurMinSpeed, motionBlurSettings->m_MovementMotionBlurMaxSpeed,
									0.0f, motionBlurSettings->m_MovementMotionBlurMaxStrength);

	return motionBlurStrength;
}

float camThirdPersonCamera::ComputeMotionBlurStrengthForDamage()
{
	const camMotionBlurSettingsMetadata* motionBlurSettings = camFactory::FindObjectMetadata<camMotionBlurSettingsMetadata>(m_Metadata.m_MotionBlurSettings.GetHash());
	if(!motionBlurSettings)
	{
		return 0.0f;
	}

	if(motionBlurSettings->m_DamageMotionBlurMaxStrength < SMALL_FLOAT)
	{
		return 0.0f;
	}

	float motionBlurStrength = 0.0f;

	//NOTE: m_AttachParent has already been validated.
	if(m_AttachParent->GetIsPhysical())
	{
		CPhysical* attachParentPhysical			= (CPhysical*)m_AttachParent.Get();		
		float health							= attachParentPhysical->GetHealth();
		float damage							= m_MotionBlurPreviousAttachParentHealth - health;
		m_MotionBlurPreviousAttachParentHealth	= health;
		u32 time								= fwTimer::GetCamTimeInMilliseconds();

		if(damage > m_MotionBlurPeakAttachParentDamage)
		{
			m_MotionBlurForAttachParentDamageStartTime	= time;
			m_MotionBlurPeakAttachParentDamage			= damage;
		}

		if(m_MotionBlurPeakAttachParentDamage > 0.0f)
		{
			u32 timeSinceStart = time - m_MotionBlurForAttachParentDamageStartTime;
			if((timeSinceStart < motionBlurSettings->m_DamageMotionBlurDuration))
			{
				motionBlurStrength	= RampValueSafe(m_MotionBlurPeakAttachParentDamage, motionBlurSettings->m_DamageMotionBlurMinDamage,
										motionBlurSettings->m_DamageMotionBlurMaxDamage, 0.0f, motionBlurSettings->m_DamageMotionBlurMaxStrength);
			}
			else
			{
				m_MotionBlurPeakAttachParentDamage = 0.0f;
			}
		}
	}
	else
	{
		m_MotionBlurPeakAttachParentDamage = 0.0f;
	}

	return motionBlurStrength;
}

void camThirdPersonCamera::CacheOrbitHeadingLimitSettings(float heading)
{
	Vector2 vCurrentHeadingLimits = m_RelativeOrbitHeadingLimits;
	camInterface::GetGameplayDirector().GetScriptOverriddenThirdPersonCameraRelativeHeadingLimitsThisUpdate(vCurrentHeadingLimits);

	const float thresholdForLimiting = PI - SMALL_FLOAT;
	if((vCurrentHeadingLimits.x >= -thresholdForLimiting) || (vCurrentHeadingLimits.y <= thresholdForLimiting))
	{
		float idealHeading = ComputeIdealOrbitHeadingForLimiting();

		if(ShouldOrbitRelativeToAttachParentOrientation())
		{
			//Convert the ideal heading to be relative to the attach parent, just as the heading is.
			const float attachParentHeading	= camFrame::ComputeHeadingFromMatrix(m_AttachParentMatrixToConsiderForRelativeOrbit);
			idealHeading					-= attachParentHeading;
			idealHeading					= fwAngle::LimitRadianAngleSafe(idealHeading);
		}

		m_RelativeHeadingForLimitingOnPreviousUpdate	= heading - idealHeading;
		m_RelativeHeadingForLimitingOnPreviousUpdate	= fwAngle::LimitRadianAngle(m_RelativeHeadingForLimitingOnPreviousUpdate);
	}
	else
	{
		m_RelativeHeadingForLimitingOnPreviousUpdate	= THREE_PI; //Invalidate.
	}
}

void camThirdPersonCamera::UpdateFirstPersonCoverSettings()
{
	if(m_AttachParent && m_AttachParent->GetIsTypePed())
	{
		const CPed& rPed = static_cast<const CPed&>(*m_AttachParent);
		if (rPed.IsLocalPlayer() && rPed.GetIsInCover() && CTaskCover::CanUseThirdPersonCoverInFirstPerson(rPed))
		{
			if (m_ControlHelper)
			{
				m_ControlHelper->IgnoreViewModeInputThisUpdate();
			}
		}
	}
}

void camThirdPersonCamera::InitVehicleCustomSettings()
{
	if(m_NumUpdatesPerformed == 0)
	{
		if(m_AttachParent->GetIsTypeVehicle())
		{
			const CVehicle* pVehicle = static_cast<const CVehicle*>(m_AttachParent.Get()); 

			const CVehicleModelInfo* targetVehicleModelInfo	= pVehicle->GetVehicleModelInfo();

			if(targetVehicleModelInfo)
			{
				u32 vehicleModelHash = targetVehicleModelInfo->GetModelNameHash(); 

				const camVehicleCustomSettingsMetadata* pCustomVehicleSettings = camInterface::GetGameplayDirector().FindVehicleCustomSettings(vehicleModelHash); 

				if(pCustomVehicleSettings)
				{
					m_CustomVehicleSettings = pCustomVehicleSettings; 
				}
			}
		}
	}
}

void camThirdPersonCamera::BypassBuoyancyThisUpdate()
{
	m_ShouldBypassBuoyancyStateThisUpdate = true;
	if(m_Collision)
	{
		m_Collision->BypassBuoyancyThisUpdate();
	}
}

void camThirdPersonCamera::InterpolateFromCamera(camBaseCamera& sourceCamera, u32 duration, bool shouldDeleteSourceCamera)
{
	if(duration == 0)
	{
		return;
	}

	const bool isInterpolatingAnAimCamera = GetIsClassId(camThirdPersonAimCamera::GetStaticClassId()) || sourceCamera.GetIsClassId(camThirdPersonAimCamera::GetStaticClassId());
	if(sourceCamera.ShouldPreventInterpolationToNextCamera() || (!isInterpolatingAnAimCamera && (sourceCamera.ShouldPreventAnyInterpolation() || ShouldPreventAnyInterpolation())))
	{
		return;
	}

	//Terminate any existing interpolation.
	StopInterpolating();

	//Determine if we should perform a third-person interpolation, or fall-back to the default behavior.

	bool shouldPerformThirdPersonInterpolation = sourceCamera.GetIsClassId(camThirdPersonCamera::GetStaticClassId());
	if(shouldPerformThirdPersonInterpolation)
	{
		//Ensure that if the source camera is interpolating, it is also third-person interpolating, as we cannot mismatch interpolation types.
		const camFrameInterpolator* sourceFrameInterpolator = sourceCamera.GetFrameInterpolator();
		if(sourceFrameInterpolator && !sourceFrameInterpolator->GetIsClassId(camThirdPersonFrameInterpolator::GetStaticClassId()))
		{
			shouldPerformThirdPersonInterpolation = false;
		}
	}

	if(shouldPerformThirdPersonInterpolation)
	{
		m_FrameInterpolator	= camera_verified_pool_new(camThirdPersonFrameInterpolator) (static_cast<camThirdPersonCamera&>(sourceCamera), *this,
								duration, shouldDeleteSourceCamera);
		cameraAssertf(m_FrameInterpolator, "A camera (name: %s, hash: %u) failed to create a third-person frame interpolator", GetName(), GetNameHash());
	}
	else
	{
		m_FrameInterpolator	= camera_verified_pool_new(camFrameInterpolator) (sourceCamera, *this, duration, shouldDeleteSourceCamera);
		cameraAssertf(m_FrameInterpolator, "A camera (name: %s, hash: %u) failed to create a frame interpolator", GetName(), GetNameHash());
	}
}

void camThirdPersonCamera::CloneBaseOrientation(const camBaseCamera& sourceCamera, bool shouldFlagAsCut, bool shouldUseInterpolatedBaseFront, bool ignoreCurrentControlHelperLookBehindInput)
{
	if(!m_AttachParent || sourceCamera.ShouldPreventNextCameraCloningOrientation())
	{
		return;
	}

	//Clone the 'base' front of the source camera, which is independent of any non-propagating offsets.

	Vector3 baseFront = sourceCamera.GetBaseFront();

	if(shouldUseInterpolatedBaseFront)
	{
		const camFrameInterpolator* sourceCameraFrameInterpolator = sourceCamera.GetFrameInterpolator();
		if(sourceCameraFrameInterpolator && sourceCameraFrameInterpolator->GetIsClassId(camThirdPersonFrameInterpolator::GetStaticClassId()))
		{
			const camThirdPersonFrameInterpolator* sourceThirdPersonFrameInterpolator = static_cast<const camThirdPersonFrameInterpolator*>(sourceCameraFrameInterpolator);
			baseFront = sourceThirdPersonFrameInterpolator->GetInterpolatedBaseFront();
		}
	}

	bool shouldClonePitch = true;

	const camControlHelper* sourceControlHelper = NULL;
	if(sourceCamera.GetIsClassId(camThirdPersonCamera::GetStaticClassId()))
	{
		sourceControlHelper = static_cast<const camThirdPersonCamera&>(sourceCamera).GetControlHelper();
	}
	else if(sourceCamera.GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
	{
		sourceControlHelper = static_cast<const camCinematicMountedCamera&>(sourceCamera).GetControlHelper();
		if(sourceControlHelper && sourceControlHelper->IsLookingBehind())
		{
			//Compensate for the look-behind flip, as the base front should not contain this.
			baseFront = -sourceCamera.GetBaseFront();
		}

		//NOTE: We only clone the pitch of cinematic mounted cameras at low speeds, as this can cause undesirable camera movement at higher speeds.
		const float attachParentSpeedSqr	= m_AttachParent->GetIsPhysical() ? static_cast<const CPhysical*>(m_AttachParent.Get())->GetVelocity().Mag2() : 0.0f;
		shouldClonePitch					= (attachParentSpeedSqr <= square(m_Metadata.m_MaxAttachParentSpeedToClonePitchFromCinematicMountedCameras));
	}
	else if(sourceCamera.GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()))
	{
		sourceControlHelper = static_cast<const camFirstPersonShooterCamera&>(sourceCamera).GetControlHelper();
		if(sourceControlHelper && sourceControlHelper->IsLookingBehind())
		{
			//Compensate for the look-behind flip, as the base front should not contain this.
			baseFront = -sourceCamera.GetBaseFront();
			// The look behind input is not set in this frame, set the state manually and skip the update of this frame
			if(GetControlHelper())
			{
				GetControlHelper()->SetLookBehindState(true);
				GetControlHelper()->IgnoreLookBehindInputThisUpdate();
			}
		}
	}

	if(sourceControlHelper && sourceControlHelper->IsLookingBehind() && (ignoreCurrentControlHelperLookBehindInput || (m_ControlHelper && !m_ControlHelper->IsUsingLookBehindInput())))
	{
		//This camera does not support look-behind, so we need to manually apply the look-behind rotation that is active on the source camera
		// - the base front ignores this.
		baseFront = -sourceCamera.GetBaseFront();
	}

	float baseFrontHeading;
	float baseFrontPitch;
	camFrame::ComputeHeadingAndPitchFromFront(baseFront, baseFrontHeading, baseFrontPitch);

	//NOTE: We use the simple matrix of the attach parent directly, not m_AttachParentMatrix, when overriding a relative orientation. This
	//reduces the scope for the parent matrix to be upside-down in world space, as this can flip the decomposed parent heading.
	//NOTE: m_AttachParent has already been validated.
	const Matrix34 attachParentMatrix = MAT34V_TO_MATRIX34(m_AttachParent->GetMatrix());

	float attachParentHeading;
	float attachParentPitch;
	camFrame::ComputeHeadingAndPitchFromMatrix(attachParentMatrix, attachParentHeading, attachParentPitch);

	float relativeHeading	= baseFrontHeading - attachParentHeading;
	relativeHeading			= fwAngle::LimitRadianAngleSafe(relativeHeading);
	SetRelativeHeading(relativeHeading, 1.0f, true, false, shouldFlagAsCut);

	if(shouldClonePitch)
	{
		float relativePitch	= baseFrontPitch - attachParentPitch;
		relativePitch		= fwAngle::LimitRadianAngleSafe(relativePitch);
		SetRelativePitch(relativePitch, 1.0f, true, shouldFlagAsCut);
	}
}

void camThirdPersonCamera::CloneBasePosition(const camBaseCamera& sourceCamera)
{
	if(sourceCamera.GetIsClassId(camThirdPersonCamera::GetStaticClassId()))
	{
		m_BasePosition = static_cast<const camThirdPersonCamera&>(sourceCamera).m_BasePosition;

		//Overriding the base position can result in a change in both the rendered position and orientation.
		//NOTE: We do not report a cut on the first update, as this interface is used to clone the framing of a previous camera, which may not generate
		//a visible cut.
		if(m_NumUpdatesPerformed > 0)
		{
			m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation);
		}
	}
}

void camThirdPersonCamera::CloneControlSpeeds(const camBaseCamera& sourceCamera)
{
	if(!m_ControlHelper)
	{
		return;
	}

	const camControlHelper* sourceControlHelper	= NULL;

	if(sourceCamera.GetIsClassId(camThirdPersonCamera::GetStaticClassId()))
	{
		sourceControlHelper = static_cast<const camThirdPersonCamera&>(sourceCamera).GetControlHelper();
	}
	else if(sourceCamera.GetIsClassId(camFirstPersonAimCamera::GetStaticClassId()))
	{
		sourceControlHelper = static_cast<const camFirstPersonAimCamera&>(sourceCamera).GetControlHelper();
	}

	if(sourceControlHelper)
	{
		m_ControlHelper->CloneSpeeds(*sourceControlHelper);
	}
}

#if ! __FINAL
//this is defined in CatchUpHelper.cpp
extern const float g_MinimumCatchUpDistance;
bool camThirdPersonCamera::PreCatchupFromFrameCheck(const camFrame& sourceFrame) const
{
    if(!m_Metadata.m_CatchUpHelperRef)
        return true;

    //NOTE: We use a custom catch-up helper (globally) when falling back to third-person for a special transition.
    const bool isFallingBackToThirdPersonForDirectorBlendCatchUpOrSwitch = camInterface::GetGameplayDirector().IsFallingBackToThirdPersonForDirectorBlendCatchUpOrSwitch();
    const u32 catchUpHelperHash = isFallingBackToThirdPersonForDirectorBlendCatchUpOrSwitch ? g_CatchUpHelperRefForFirstPerson : m_Metadata.m_CatchUpHelperRef.GetHash();
    const camCatchUpHelperMetadata* catchUpHelperMetadata = camFactory::FindObjectMetadata<camCatchUpHelperMetadata>(catchUpHelperHash);

    if(!catchUpHelperMetadata)
        return true;

    const float maxOrbitDistanceSetting = catchUpHelperMetadata->m_MaxOrbitDistanceScalingForMaxValidCatchUpDistance;

    //Sanity-check the distance of the source frame from the attach parent is not too big.
    const Vector3& sourcePosition		= sourceFrame.GetPosition();
    const Vector3& basePivotPosition	= GetBasePivotPosition();
    const float sourceToBasePivotDist2	= sourcePosition.Dist2(basePivotPosition);
    const float maxOrbitDistance        = m_MaxOrbitDistanceSpring.GetResult(); //it might be the value of the previous frame.
    const float maxCatchUpDistance		= (maxOrbitDistance * Max(maxOrbitDistanceSetting, g_MinimumCatchUpDistance));

    if(!cameraVerifyf(sourceToBasePivotDist2 <= (maxCatchUpDistance * maxCatchUpDistance),
        "A camera catch-up helper (name: %s, hash: %u) aborted as the catch-up position was too far away (%fm, limit=%fm) from the base pivot position",
        GetName(), GetNameHash(), Sqrtf(sourceToBasePivotDist2), maxCatchUpDistance))
    {
        return false;
    }

    return true;
}
#endif //!__FINAL

bool camThirdPersonCamera::CatchUpFromFrame(const camFrame& sourceFrame, float maxDistanceToBlend, int blendType)
{
	if(m_AttachParent)
	{
		//Check that the source frame is pointing towards our attach parent.
		const Vector3& sourceFrameFront		= sourceFrame.GetFront();
		const Vector3& sourceFramePosition	= sourceFrame.GetPosition();
		const Vector3& attachParentPosition	= VEC3V_TO_VECTOR3(m_AttachParent->GetTransform().GetPosition());
		Vector3 sourceFrameToAttachParent	= attachParentPosition - sourceFramePosition;
		const float frontDotDelta			= sourceFrameFront.Dot(sourceFrameToAttachParent);
		if(!cameraVerifyf(frontDotDelta >= SMALL_FLOAT,
			"A third-person camera (name: %s, hash: %u) rejected a request to catch-up from a camera frame that points away from the attach parent",
			GetName(), GetNameHash()))
		{
			return false;
		}
	}

	camCatchUpHelper* catchUpHelper = NULL;
	if(m_Metadata.m_CatchUpHelperRef)
	{
		//NOTE: We use a custom catch-up helper (globally) when falling back to third-person for a special transition.
		const bool isFallingBackToThirdPersonForDirectorBlendCatchUpOrSwitch = camInterface::GetGameplayDirector().IsFallingBackToThirdPersonForDirectorBlendCatchUpOrSwitch();
		const u32 catchUpHelperHash = isFallingBackToThirdPersonForDirectorBlendCatchUpOrSwitch ? g_CatchUpHelperRefForFirstPerson : m_Metadata.m_CatchUpHelperRef.GetHash();

		const camCatchUpHelperMetadata* catchUpHelperMetadata = camFactory::FindObjectMetadata<camCatchUpHelperMetadata>(catchUpHelperHash);
		if(catchUpHelperMetadata)
		{
			catchUpHelper = camFactory::CreateObject<camCatchUpHelper>(*catchUpHelperMetadata);
			cameraAssertf(catchUpHelper, "A third-person camera (name: %s, hash: %u) failed to create a catch-up helper (name: %s, hash: %u)",
				GetName(), GetNameHash(), SAFE_CSTRING(catchUpHelperMetadata->m_Name.GetCStr()), catchUpHelperMetadata->m_Name.GetHash());
		}
	}

	if(!catchUpHelper)
	{
		return false;
	}

	//Abort any existing catch-up behaviour.
	if(m_CatchUpHelper)
	{
		delete m_CatchUpHelper;
	}

	m_CatchUpHelper = catchUpHelper;
	m_CatchUpHelper->Init(sourceFrame, maxDistanceToBlend, blendType);

	if(m_Collision)
	{
		//Bypass any orbit distance damping and force pop-in behaviour for the initial update of catch-up.
		m_Collision->Reset();
	}

	return true;
}

void camThirdPersonCamera::AbortCatchUp()
{
	if(m_CatchUpHelper)
	{
		delete m_CatchUpHelper;
		m_CatchUpHelper = NULL;
	}
}

void camThirdPersonCamera::UpdateDof()
{
	camBaseCamera::UpdateDof();

	if(m_CatchUpHelper)
	{
		m_CatchUpHelper->ComputeDofParameters(m_PostEffectFrame);
	}
}

void camThirdPersonCamera::ComputeDofSettings(float overriddenFocusDistance, float overriddenFocusDistanceBlendLevel, camDepthOfFieldSettingsMetadata& settings) const
{
	camBaseCamera::ComputeDofSettings(overriddenFocusDistance, overriddenFocusDistanceBlendLevel, settings);

	const camDepthOfFieldSettingsMetadata* tempCustomSettings = camFactory::FindObjectMetadata<camDepthOfFieldSettingsMetadata>(m_Metadata.m_DofSettingsInTightSpace.GetHash());
	if(!tempCustomSettings)
	{
		return;
	}

	//Blend to custom DOF settings in tight spaces.

	camDepthOfFieldSettingsMetadata customSettings(*tempCustomSettings);

	const float blendLevel = m_TightSpaceSpring.GetResult();

	BlendDofSettings(blendLevel, settings, customSettings, settings);
}

void camThirdPersonCamera::StartCustomFovBlend(float fovDelta, u32 duration)
{
	m_BlendFovDelta = fovDelta;
	m_BlendFovDeltaDuration = duration;
	m_BlendFovDeltaStartTime = fwTimer::GetCamTimeInMilliseconds();
}

void camThirdPersonCamera::UpdateCustomFovBlend(float fCurrentFov)
{
	if(m_BlendFovDeltaStartTime != 0)
	{
		if(m_BlendFovDeltaStartTime + m_BlendFovDeltaDuration >= fwTimer::GetCamTimeInMilliseconds())
		{
			float fInterp = (float)(m_BlendFovDeltaStartTime + m_BlendFovDeltaDuration - fwTimer::GetCamTimeInMilliseconds()) / (float)m_BlendFovDeltaDuration;

			fInterp = Clamp(fInterp, 0.0f, 1.0f);
			//fInterp = SlowIn(fInterp);
			fInterp = CircularEaseIn(fInterp);

			m_Frame.SetFov( fCurrentFov + m_BlendFovDelta * fInterp );
		}
		else
		{
			m_BlendFovDeltaStartTime = 0;
		}
	}
}

void camThirdPersonCamera::UpdateDistanceTravelled()
{
	DEV_ONLY( TUNE_GROUP_BOOL(HURTY_FEELINGS, c_EnableAfterAimingBlendDelay, true); )
	DEV_ONLY( TUNE_GROUP_BOOL(HURTY_FEELINGS, c_ForceAfterAimingOn, false); )
	if( camInterface::GetGameplayDirector().IsAfterAimingBlendDisabled()  DEV_ONLY(|| !c_EnableAfterAimingBlendDelay) )
	{
		m_DistanceTravelled = m_Metadata.m_PivotPosition.m_maxDistanceAfterAimingToApplyAlternateScalingMax+1.0f;
	}
	else
	{
		if( camInterface::GetGameplayDirector().IsAfterAimingBlendForced()  DEV_ONLY(|| c_ForceAfterAimingOn) )
		{
			m_DistanceTravelled = 0.0f;
			return;
		}

		const camThirdPersonCameraMetadataPivotPosition& settings = m_Metadata.m_PivotPosition;
		if(settings.m_maxDistanceAfterAimingToApplyAlternateScalingMax > 0.0f)
		{
			// Note: m_DistanceTravelled is used for the after aiming blend.
			const CPed* attachPed = NULL;
			if(m_AttachParent && m_AttachParent->GetIsTypePed())
			{
				attachPed = (const CPed*)m_AttachParent.Get();
			}

			const bool c_IsSprinting = attachPed && attachPed->GetMotionData() && attachPed->GetMotionData()->GetIsDesiredSprinting(false);
			bool bTimeDelayExpired = true;
			u32 timeRemaining = 0;
			if( attachPed && attachPed->IsLocalPlayer() && attachPed->GetPlayerInfo() && settings.m_timeAfterAimingToApplyDistanceBlend != 0 )
			{
				u32 timeSinceLastAimed = attachPed->GetPlayerInfo()->GetTimeSinceLastAimedMS();
				u32 distanceBlendDelay = (u32)(settings.m_timeAfterAimingToApplyDistanceBlend * 1000.0f);
				if( timeSinceLastAimed < distanceBlendDelay )
				{
					bTimeDelayExpired = false;
					timeRemaining = distanceBlendDelay - timeSinceLastAimed;
				}
			}

			if(!bTimeDelayExpired && !c_IsSprinting)
			{
				// Blend back to after aiming values.
				const u32 c_AfterAimingActionModeBlendDuration = 1000;		// TODO: metadata?
				const u32 c_MinTimeRemainingToBlendToAfterAiming = c_AfterAimingActionModeBlendDuration*2 + c_AfterAimingActionModeBlendDuration/2;
				if(timeRemaining > c_MinTimeRemainingToBlendToAfterAiming)
				{
					if(m_AfterAimingActionModeBlendStartTime == 0)
					{
						float percentage = Min(m_DistanceTravelled / settings.m_maxDistanceAfterAimingToApplyAlternateScalingMax, 1.0f);
						m_AfterAimingActionModeBlendStartTime = fwTimer::GetTimeInMilliseconds() - u32((1.0f - percentage)*(float)c_AfterAimingActionModeBlendDuration);
					}
					else
					{
						float fInterp = (float)(fwTimer::GetTimeInMilliseconds() - m_AfterAimingActionModeBlendStartTime) / (float)c_AfterAimingActionModeBlendDuration;
						fInterp = Clamp(1.0f - fInterp, 0.0f, 1.0f);
						m_DistanceTravelled = fInterp * settings.m_maxDistanceAfterAimingToApplyAlternateScalingMax;

						if (fInterp < SMALL_FLOAT)
						{
							m_AfterAimingActionModeBlendStartTime = 0;
							m_DistanceTravelled = 0.0f;
						}
					}
				}

				return;
			}

			m_AfterAimingActionModeBlendStartTime = 0;
			if(m_DistanceTravelled < settings.m_maxDistanceAfterAimingToApplyAlternateScalingMax)
			{
				if(bTimeDelayExpired || c_IsSprinting)
				{
					float baseAttachSpeed = 3.0f;
					if(attachPed)
					{
						const Vector3 baseAttachVelocity	= (attachPed) ? attachPed->GetVelocity() : Vector3(3.0f, 0.0f, 0.0f);
						baseAttachSpeed						= baseAttachVelocity.Mag();
					}

					const float timeStep					= fwTimer::GetTimeStep();
					const float distanceTravelledThisUpdate	= baseAttachSpeed * timeStep;
					m_DistanceTravelled						+= distanceTravelledThisUpdate;
				}
			}
		}
	}
}

float camThirdPersonCamera::ComputeAimBlendBasedOnDistanceTravelled() const
{
	const float fReturn = RampValueSafe(m_DistanceTravelled, 0.0f, m_Metadata.m_PivotPosition.m_maxDistanceAfterAimingToApplyAlternateScalingMax, 0.0f, 1.0f);
	return (fReturn);
}

#if __BANK
//Render the camera so we can see it.
void camThirdPersonCamera::DebugRender()
{
	camBaseCamera::DebugRender();

	if(camManager::GetSelectedCamera() == this)
 	{
		//NOTE: These elements are drawn even if this is the active/rendering camera. This is necessary in order to debug the camera when active.
		static float axisScale	= 0.5f;
		static float radius		= 0.125f;

		if(m_AttachParent)
		{
			const Matrix34 attachParentMatrix(MAT34V_TO_MATRIX34(m_AttachParent->GetMatrix()));
			grcDebugDraw::Cross(RCC_VEC3V(attachParentMatrix.d), 2.0f * radius, Color_purple);
			grcDebugDraw::Axis(RCC_MAT34V(m_AttachParentMatrix), axisScale);
		}

		grcDebugDraw::Sphere(RCC_VEC3V(m_BaseAttachPosition), radius, Color_cyan);
		grcDebugDraw::Sphere(RCC_VEC3V(m_BasePivotPosition), radius, Color_blue);
		grcDebugDraw::Sphere(RCC_VEC3V(m_PivotPosition), radius, Color_yellow);
		grcDebugDraw::Cross(RCC_VEC3V(m_CollisionOrigin), 2.0f * radius, Color_orange);
		grcDebugDraw::Sphere(RCC_VEC3V(m_CollisionRootPosition), radius, Color_red);
		grcDebugDraw::Sphere(RCC_VEC3V(m_LookAtPosition), radius, Color_green);

		grcDebugDraw::Line(RCC_VEC3V(m_BasePivotPosition), VECTOR3_TO_VEC3V(m_BasePivotPosition - m_BaseFront), Color_blue);
		grcDebugDraw::Line(RCC_VEC3V(m_PivotPosition), RCC_VEC3V(m_LookAtPosition), Color_green);
	}
}
#endif // __BANK
