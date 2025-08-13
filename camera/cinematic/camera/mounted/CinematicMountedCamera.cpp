//
// camera/cinematic/CinematicMountedCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"

#include "camera/CamInterface.h"
#include "camera/cinematic/cinematicdirector.h"
#include "camera/cinematic/camera/mounted/CinematicMountedPartCamera.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/follow/FollowVehicleCamera.h"
#include "camera/gameplay/aim/FirstPersonShooterCamera.h"
#include "camera/gameplay/aim/ThirdPersonPedAimCamera.h"
#include "camera/helpers/BaseFrameShaker.h"
#include "camera/helpers/ControlHelper.h"
#include "camera/helpers/Envelope.h"
#include "camera/helpers/Collision.h"
#include "camera/helpers/FrameInterpolator.h"
#include "camera/helpers/LookAheadHelper.h"
#include "camera/helpers/LookAtDampingHelper.h"
#include "camera/helpers/SpringMount.h"
#include "camera/system/CameraFactory.h"
#include "camera/cinematic/camera/tracking/CinematicVehicleTrackingCamera.h"
#include "camera/cinematic/camera/tracking/CinematicCamManCamera.h"
#include "frontend/MobilePhone.h"
#include "frontend/ProfileSettings.h"
#include "peds/ped.h"
#include "peds/PedIntelligence.h"
#include "profile/group.h"
#include "profile/page.h"
#include "task/Movement/Climbing/TaskRappel.h"
#include "Task/Service/Police/TaskPolicePatrol.h"
#include "task/vehicle/TaskEnterVehicle.h"
#include "task/vehicle/TaskInVehicle.h"
#include "task/Vehicle/TaskMountAnimalWeapon.h"
#include "Task/Vehicle/TaskVehicleWeapon.h"
#include "task/Weapons/Gun/TaskVehicleDriveBy.h"
#include "vehicles/metadata/VehicleLayoutInfo.h"
#include "vehicles/VehicleGadgets.h"

#if __BANK
	#include "Vehicles/Metadata/VehicleMetadataManager.h"
#endif

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camCinematicMountedCamera,0x2F96EBCD)

PF_PAGE(camCinematicMountedCameraPage, "Camera: Cinematic Mounted Camera");

PF_GROUP(camCinematicMountedCameraMetrics);
PF_LINK(camCinematicMountedCameraPage, camCinematicMountedCameraMetrics);

PF_VALUE_FLOAT(springConstantConsideringSpeed, camCinematicMountedCameraMetrics);
PF_VALUE_FLOAT(minSpringConstant, camCinematicMountedCameraMetrics);
PF_VALUE_FLOAT(maxSpringConstant, camCinematicMountedCameraMetrics);
PF_VALUE_FLOAT(attachParentSpeed, camCinematicMountedCameraMetrics);
PF_VALUE_FLOAT(minVehicleSpeed, camCinematicMountedCameraMetrics);
PF_VALUE_FLOAT(maxVehicleSpeed, camCinematicMountedCameraMetrics);

REGISTER_TUNE_GROUP_FLOAT(SHUFFLE_FROM_TURRET_VISIBLE_THRESHOLD, 0.50f);
#define SHUFFLE_FROM_TURRET_USE_HEAD_BONE_THRESHOLD						(SMALL_FLOAT)

const u32 g_MaxShapeTestResultsForClippingTests = 5;
const float g_HeadingLimitNextToTurretSeatBlendLevelSpringConstant = 30.0f;
const float g_HeadingLimitNextToTurretSeatBlendLevelSpringDampingRatio = 1.0f;

static dev_float s_fPutOnHelmentRate = 0.1f;

camCinematicMountedCamera::camCinematicMountedCamera(const camBaseObjectMetadata& metadata)
: camBaseCamera(metadata)
, m_Metadata(static_cast<const camCinematicMountedCameraMetadata&>(metadata))
, m_ControlHelper(NULL)
, m_MobilePhoneCameraControlHelper(NULL)
, m_RelativeAttachSpringConstantEnvelope(NULL)
, m_RagdollBlendEnvelope(NULL)
, m_UnarmedDrivebyBlendEnvelope(NULL)
, m_DuckingBlendEnvelope(NULL)
, m_DrivebyBlendEnvelope(NULL)
, m_SmashWindowBlendEnvelope(NULL)
, m_LowriderAltAnimsBlendEnvelope(NULL)
, m_LookBehindEnvelope(NULL)
, m_ResetLookEnvelope(NULL)
, m_SpringMount(NULL)
, m_InVehicleLookAtDamper(NULL)
, m_OnFootLookAtDamper(NULL)
, m_InVehicleLookAheadHelper(NULL)
, m_LocalExitEnterOffset(VEC3_ZERO)
, m_BoneRelativeAttachPosition(m_Metadata.m_BoneRelativeAttachOffset)
, m_SeatSpecificCameraOffset(VEC3_ZERO)
, m_BaseFront(YAXIS)
, m_InitialRelativePitchLimits(m_Metadata.m_InitialRelativePitchLimits)
, m_InitialRelativeHeadingLimits(m_Metadata.m_InitialRelativeHeadingLimits)
, m_AttachParentRelativePitchLimits(m_Metadata.m_AttachParentRelativePitch)
, m_AttachParentRelativeHeadingLimits(m_Metadata.m_AttachParentRelativeHeading)
, m_TimeSpentClippingIntoDynamicCollision(m_Metadata.m_MaxTimeToClipIntoDynamicCollision)
, m_BlendAnimatedStartTime(0)
, m_LookBehindBlendLevel(0.0f)
, m_ResetLookBlendLevel(0.0f)
, m_fHeadingLimitForNextToTurretSeats(0.0f)
, m_RelativeHeading(0.0f)
, m_RelativeHeadingWithLookBehind(0.0f)
, m_RelativePitch(m_Metadata.m_DefaultRelativePitch*DtoR)
, m_ShotTimeSpentOccluded(0.0f)
, m_RagdollBlendLevel(0.0f)
, m_UnarmedDrivebyBlendLevel(0.0f)
, m_DuckingBlendLevel(0.0f)
, m_DriveByBlendLevel(0.0f)
, m_SmashWindowBlendLevel(0.0f)
, m_DesiredSideOffset(0.0f)
, m_HeadBoneHeading(FLT_MAX)
, m_HeadBonePitch(FLT_MAX)
, m_DestSeatHeading(FLT_MAX)
, m_DestSeatPitch(FLT_MAX)
, m_PreviousVehicleHeading(0.0f)
, m_PreviousVehiclePitch(0.0f)
, m_PitchOffset(VEC3_ZERO)
, m_vPutOnHelmetOffset(VEC3_ZERO)
, m_Fov(m_Metadata.m_BaseFov)
, m_CamFrontRelativeAttachSpeedOnPreviousUpdate(0.0f)
#if KEYBOARD_MOUSE_SUPPORT
, m_PreviousLookAroundInputEnvelopeLevel(0.0f)
#endif // KEYBOARD_MOUSE_SUPPORT
, m_ShakeAmplitudeScalar(0.0f)
, m_BlendLookAroundTime(0.0f)
, m_BlendFovDelta(0.0f)
, m_BlendFovDeltaStartTime(0)
, m_BlendFovDeltaDuration(1000)
, m_BaseHeadingRadians(m_Metadata.m_BaseHeading * DtoR)
, m_DesiredRelativeHeading(0.0f)
, m_SeatShufflePhase(0.0f)
, m_SeatShuffleTargetSeat(-1)
, m_AttachPart(m_Metadata.m_VehicleAttachPart)
, m_canUpdate(true)
, m_DisableControlHelper(false)
, m_ShouldOverrideLookAtBehaviour(false)
, m_ShouldLimitAttachParentRelativePitchAndHeading(m_Metadata.m_LimitAttachParentRelativePitchAndHeading)
, m_ShouldTerminateForPitchAndHeading(m_Metadata.m_ShouldTerminateForPitchAndHeading)
, m_shouldTerminateForOcclusion(m_Metadata.m_ShouldTerminateForOcclusion)
, m_shouldTerminateForDistanceToTarget(m_Metadata.m_ShouldTerminateForDistanceToTarget)
, m_CanUseDampedLookAtHelpers(false)
, m_IsSeatPositionOnLeftHandSide(false)
, m_ShouldAdjustRelativeAttachPositionForRollCage(false)
, m_CanUseLookAheadHelper(false)
, m_ShouldRotateRightForLookBehind(true)
, m_ShouldApplySideOffsetToLeft(false)
, m_HaveCachedSideForSideOffset(false)
, m_bShouldBlendRelativeAttachOffsetWhenLookingAround(false)
, m_bAllowSpringCutOnFirstUpdate(true)
, m_bResettingLook(false)
, m_bResetLookBehindForDriveby(false)
, m_PreviousWasInVehicleCinematicCamera(false)
, m_AttachedToTurret(false)
, m_SeatShuffleChangedCameras(false)
, m_IsShuffleTaskFirstUpdate(true)
, m_UseScriptRelativeHeading(false)
, m_UseScriptRelativePitch(false)
, m_CachedScriptHeading(0.0f)
, m_CachedScriptPitch(0.0f)
{
	const camControlHelperMetadata* controlHelperMetadata = camFactory::FindObjectMetadata<camControlHelperMetadata>(m_Metadata.m_ControlHelperRef.GetHash());
	if(controlHelperMetadata)
	{
		m_ControlHelper = camFactory::CreateObject<camControlHelper>(*controlHelperMetadata);
		cameraAssertf(m_ControlHelper, "A cinematic mounted camera (name: %s, hash: %u) failed to create a control helper (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(controlHelperMetadata->m_Name.GetCStr()), controlHelperMetadata->m_Name.GetHash());
	}

	controlHelperMetadata = camFactory::FindObjectMetadata<camControlHelperMetadata>(m_Metadata.m_MobilePhoneCameraControlHelperRef.GetHash());
	if(controlHelperMetadata)
	{
		m_MobilePhoneCameraControlHelper = camFactory::CreateObject<camControlHelper>(*controlHelperMetadata);
		cameraAssertf(m_MobilePhoneCameraControlHelper,
			"A cinematic mounted camera (name: %s, hash: %u) failed to create a mobile phone camera control helper (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(controlHelperMetadata->m_Name.GetCStr()), controlHelperMetadata->m_Name.GetHash());
	}
	const camEnvelopeMetadata* envelopeMetadata = NULL; 

	envelopeMetadata	= camFactory::FindObjectMetadata<camEnvelopeMetadata>(
													m_Metadata.m_RelativeAttachSpringConstantEnvelopeRef.GetHash());
	if(envelopeMetadata)
	{
		m_RelativeAttachSpringConstantEnvelope = camFactory::CreateObject<camEnvelope>(*envelopeMetadata);
		if(cameraVerifyf(m_RelativeAttachSpringConstantEnvelope,
			"A cinematic mounted camera (name: %s, hash: %u) failed to create an envelope (name: %s, hash: %u)", GetName(), GetNameHash(),
			SAFE_CSTRING(envelopeMetadata->m_Name.GetCStr()), envelopeMetadata->m_Name.GetHash()))
		{
			m_RelativeAttachSpringConstantEnvelope->SetUseGameTime(true);
		}
	}

	const camSpringMountMetadata* springMountMetadata = camFactory::FindObjectMetadata<camSpringMountMetadata>(m_Metadata.m_SpringMountRef.GetHash());
	if(springMountMetadata)
	{
		m_SpringMount = camFactory::CreateObject<camSpringMount>(*springMountMetadata);
		cameraAssertf(m_SpringMount, "A cinematic mounted camera (name: %s, hash: %u) failed to create a spring mount (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(springMountMetadata->m_Name.GetCStr()), springMountMetadata->m_Name.GetHash());
	}

	envelopeMetadata = camFactory::FindObjectMetadata<camEnvelopeMetadata>(m_Metadata.m_RagdollBlendEnvelopeRef.GetHash());
	if(envelopeMetadata)
	{
		m_RagdollBlendEnvelope = camFactory::CreateObject<camEnvelope>(*envelopeMetadata);
		if(cameraVerifyf(m_RagdollBlendEnvelope, "A cinematic mounted camera (name: %s, hash: %u) failed to create an envelope (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(envelopeMetadata->m_Name.GetCStr()), envelopeMetadata->m_Name.GetHash()))
		{
			m_RagdollBlendEnvelope->SetUseGameTime(true);
		}
	}

	envelopeMetadata = camFactory::FindObjectMetadata<camEnvelopeMetadata>(m_Metadata.m_UnarmedDrivebyBlendEnvelopeRef.GetHash());
	if(envelopeMetadata)
	{
		m_UnarmedDrivebyBlendEnvelope = camFactory::CreateObject<camEnvelope>(*envelopeMetadata);
		if(cameraVerifyf(m_UnarmedDrivebyBlendEnvelope, "A cinematic mounted camera (name: %s, hash: %u) failed to create an envelope (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(envelopeMetadata->m_Name.GetCStr()), envelopeMetadata->m_Name.GetHash()))
		{
			m_UnarmedDrivebyBlendEnvelope->SetUseGameTime(true);
		}
	}

	envelopeMetadata = camFactory::FindObjectMetadata<camEnvelopeMetadata>(m_Metadata.m_DuckingBlendEnvelopeRef.GetHash());
	if(envelopeMetadata)
	{
		m_DuckingBlendEnvelope = camFactory::CreateObject<camEnvelope>(*envelopeMetadata);
		if(cameraVerifyf(m_DuckingBlendEnvelope, "A cinematic mounted camera (name: %s, hash: %u) failed to create an envelope (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(envelopeMetadata->m_Name.GetCStr()), envelopeMetadata->m_Name.GetHash()))
		{
			m_DuckingBlendEnvelope->SetUseGameTime(true);
		}
	}

	envelopeMetadata = camFactory::FindObjectMetadata<camEnvelopeMetadata>(m_Metadata.m_DrivebyBlendEnvelopeRef.GetHash()); 
	if(envelopeMetadata)
	{
		m_DrivebyBlendEnvelope = camFactory::CreateObject<camEnvelope>(*envelopeMetadata);
		if(cameraVerifyf(m_DrivebyBlendEnvelope, "A cinematic mounted camera (name: %s, hash: %u) failed to create an envelope (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(envelopeMetadata->m_Name.GetCStr()), envelopeMetadata->m_Name.GetHash()))
		{
			m_DrivebyBlendEnvelope->SetUseGameTime(true);
		}
	}

	envelopeMetadata = camFactory::FindObjectMetadata<camEnvelopeMetadata>(m_Metadata.m_SmashWindowBlendEnvelopeRef.GetHash()); 
	if(envelopeMetadata)
	{
		m_SmashWindowBlendEnvelope = camFactory::CreateObject<camEnvelope>(*envelopeMetadata);
		if(cameraVerifyf(m_SmashWindowBlendEnvelope, "A cinematic mounted camera (name: %s, hash: %u) failed to create an envelope (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(envelopeMetadata->m_Name.GetCStr()), envelopeMetadata->m_Name.GetHash()))
		{
			m_SmashWindowBlendEnvelope->SetUseGameTime(true);
		}
	}


	envelopeMetadata = camFactory::FindObjectMetadata<camEnvelopeMetadata>(m_Metadata.m_LowriderAltAnimsEnvelopeRef.GetHash()); 
	if(envelopeMetadata)
	{
		m_LowriderAltAnimsBlendEnvelope = camFactory::CreateObject<camEnvelope>(*envelopeMetadata);
		if(cameraVerifyf(m_LowriderAltAnimsBlendEnvelope, "A cinematic mounted camera (name: %s, hash: %u) failed to create an envelope (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(envelopeMetadata->m_Name.GetCStr()), envelopeMetadata->m_Name.GetHash()))
		{
			m_LowriderAltAnimsBlendEnvelope->SetUseGameTime(true);
		}
	}

	envelopeMetadata = camFactory::FindObjectMetadata<camEnvelopeMetadata>(m_Metadata.m_LookBehindEnvelopeRef.GetHash());
	if(envelopeMetadata)
	{
		m_LookBehindEnvelope = camFactory::CreateObject<camEnvelope>(*envelopeMetadata);
		cameraAssertf(m_LookBehindEnvelope, "A cinematic mounted camera (name: %s, hash: %u) failed to create an envelope (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(envelopeMetadata->m_Name.GetCStr()), envelopeMetadata->m_Name.GetHash());
	}

	envelopeMetadata = camFactory::FindObjectMetadata<camEnvelopeMetadata>(m_Metadata.m_ResetLookEnvelopeRef.GetHash());
	if(envelopeMetadata)
	{
		m_ResetLookEnvelope = camFactory::CreateObject<camEnvelope>(*envelopeMetadata);
		cameraAssertf(m_ResetLookEnvelope, "A cinematic mounted camera (name: %s, hash: %u) failed to create an envelope (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(envelopeMetadata->m_Name.GetCStr()), envelopeMetadata->m_Name.GetHash());
	}

	CreateDampedLookAtHelpers(m_Metadata.m_InVehicleLookAtDampingRef, m_Metadata.m_OnFootLookAtDampingRef); 

	const camLookAheadHelperMetadata* pLookAheadHelperMetaData = camFactory::FindObjectMetadata<camLookAheadHelperMetadata>(m_Metadata.m_InVehicleLookAheadRef.GetHash());

	if(pLookAheadHelperMetaData)
	{
		m_InVehicleLookAheadHelper = camFactory::CreateObject<camLookAheadHelper>(*pLookAheadHelperMetaData);
		cameraAssertf(m_InVehicleLookAheadHelper, "Cinematic Mounted Camera (name: %s, hash: %u) cannot create a look ahead helper (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(pLookAheadHelperMetaData->m_Name.GetCStr()), pLookAheadHelperMetaData->m_Name.GetHash());
	}

	// Treat forcing first person bonnet camera as if previous camera was a cinematic one so we don't use gameplay orientation to set up initial orientation for this camera.
	m_PreviousWasInVehicleCinematicCamera = (camInterface::GetCinematicDirector().HaveUpdatedACinematicCamera() && camInterface::GetCinematicDirector().IsRenderedCameraInsideVehicle()) ||
											camInterface::GetGameplayDirector().IsForcingFirstPersonBonnet();

#if FPS_MODE_SUPPORTED && 0
	if(camInterface::GetDominantRenderedCamera())
	{
		if(camInterface::GetDominantRenderedCamera()->GetIsClassId( camFirstPersonShooterCamera::GetStaticClassId() ))
		{
			m_bAllowSpringCutOnFirstUpdate = false;
		}
		else if(camInterface::GetDominantRenderedCamera()->GetIsClassId( camCinematicMountedCamera::GetStaticClassId() ))
		{
			const camCinematicMountedCamera* pPreviousMountedCamera = static_cast<const camCinematicMountedCamera*>(camInterface::GetDominantRenderedCamera());
			if(pPreviousMountedCamera->IsFirstPersonCamera())
			{
				m_bAllowSpringCutOnFirstUpdate = false;
			}
		}
	}
#endif

#if FPS_MODE_SUPPORTED
	bool bApplyCustomFovBlend = false;

	m_UsingFirstPersonAnimatedData = false;
	const CPed* pFollowPed = camInterface::FindFollowPed();
	if(pFollowPed)
	{
		Vec3V vTrans;
		QuatV qRot;
		float fMinX, fMaxX, fMinY, fMaxY, fMinZ, fMaxZ, fov;
		float fWeight = 0.0f;
		bool bFirstPersonAnimData = pFollowPed->GetFirstPersonCameraDOFs(vTrans, qRot, fMinX, fMaxX, fMinY, fMaxY, fMinZ, fMaxZ, fWeight, fov);

		m_UsingFirstPersonAnimatedData = (bFirstPersonAnimData && fWeight != 0.0f);
	}
	m_WasUsingFirstPersonAnimatedData = m_UsingFirstPersonAnimatedData;

#if 0
	// Disable fov blend when switching from third person camera due to concerns with seeing too much or possible clipping.
	if( IsFirstPersonCamera() && camInterface::GetGameplayDirectorSafe() &&
		((camInterface::GetGameplayDirector().GetVehicleEntryExitState() == camGameplayDirector::INSIDE_VEHICLE &&
		  camInterface::GetGameplayDirector().GetVehicleEntryExitStateOnPreviousUpdate() != camGameplayDirector::INSIDE_VEHICLE) ||
		 (camInterface::GetGameplayDirector().GetFollowVehicleCamera())) )
	{
		bApplyCustomFovBlend = true;
	}
#endif

	if( m_Metadata.m_ShouldAttachToVehicleTurret && camInterface::GetGameplayDirectorSafe() &&
		!camInterface::GetGameplayDirector().GetFirstPersonShooterCamera() &&
		(camInterface::GetGameplayDirector().GetVehicleEntryExitState() == camGameplayDirector::INSIDE_VEHICLE &&
		 camInterface::GetGameplayDirector().GetVehicleEntryExitStateOnPreviousUpdate() == camGameplayDirector::INSIDE_VEHICLE) )
	{
		// Trigger custom fov blend if we switched to first person turret camera from third person turret camera.
		// (if we switched for vehicle entry, first person camera should be active)
		bApplyCustomFovBlend = true;
	}

	if(bApplyCustomFovBlend)
	{
		float fovDelta;
		u32 fovDuration;
		camInterface::GetGameplayDirector().GetThirdPersonToFirstPersonFovParameters(fovDelta, fovDuration);
		StartCustomFovBlend(fovDelta, fovDuration);
	}
#endif
}

camCinematicMountedCamera::~camCinematicMountedCamera()
{
	if(m_ControlHelper)
	{
		delete m_ControlHelper;
	}

	if(m_MobilePhoneCameraControlHelper)
	{
		delete m_MobilePhoneCameraControlHelper;
	}

	if(m_RelativeAttachSpringConstantEnvelope)
	{
		delete m_RelativeAttachSpringConstantEnvelope;
	}

	if(m_SpringMount)
	{
		delete m_SpringMount;
	}

	if(m_RagdollBlendEnvelope)
	{
		delete m_RagdollBlendEnvelope; 
	}

	if(m_UnarmedDrivebyBlendEnvelope)
	{
		delete m_UnarmedDrivebyBlendEnvelope; 
	}

	if(m_DuckingBlendEnvelope)
	{
		delete m_DuckingBlendEnvelope;
	}

	if(m_DrivebyBlendEnvelope)
	{
		delete m_DrivebyBlendEnvelope;
	}

	if(m_SmashWindowBlendEnvelope)
	{
		delete m_SmashWindowBlendEnvelope;
	}

	if(m_LowriderAltAnimsBlendEnvelope)
	{
		delete m_LowriderAltAnimsBlendEnvelope;
	}

	if(m_LookBehindEnvelope)
	{
		delete m_LookBehindEnvelope;
	}

	if(m_ResetLookEnvelope)
	{
		delete m_ResetLookEnvelope;
	}

	if(m_InVehicleLookAtDamper)
	{
		delete m_InVehicleLookAtDamper; 
	}

	if(m_OnFootLookAtDamper)
	{
		delete m_OnFootLookAtDamper; 
	}

	if(m_InVehicleLookAheadHelper)
	{
		delete m_InVehicleLookAheadHelper; 
	}
}

bool camCinematicMountedCamera::IsValid()
{
	return m_canUpdate; 
}

bool camCinematicMountedCamera::IsCameraInsideVehicle() const
{
	return m_Metadata.m_IsBehindVehicleGlass;
}

bool camCinematicMountedCamera::IsCameraForcingMotionBlur() const
{
	return m_Metadata.m_IsForcingMotionBlur;
}

bool camCinematicMountedCamera::ShouldDisplayReticule() const
{
	//NOTE: The reticule is explicitly disabled when the mobile phone camera is rendering.
	const bool isRenderingMobilePhoneCamera	= CPhoneMgr::CamGetState();
	bool shouldDisplayReticule		= !isRenderingMobilePhoneCamera && m_Metadata.m_ShouldDisplayReticule;

	// show reticule in drive by mode.
	if( m_DriveByBlendLevel > 0.0f)
	{
		shouldDisplayReticule = true;
	}

	return shouldDisplayReticule;
}

void camCinematicMountedCamera::SetShouldLimitAndTerminateForPitchAndHeadingLimits(bool ShouldLimitHeadingAndPitch, bool ShouldTerminateForHeadingAndPitch,
																				   const Vector2& InitialPitchLimits, const Vector2& InitialHeadingLimits,
																				   const Vector2& RelativeParentPitchLimits, const Vector2& RelativeParentHeadingLimits)
{
	m_ShouldLimitAttachParentRelativePitchAndHeading = ShouldLimitHeadingAndPitch; 
	m_ShouldTerminateForPitchAndHeading = ShouldTerminateForHeadingAndPitch; 
	m_InitialRelativePitchLimits = InitialPitchLimits; 
	m_InitialRelativeHeadingLimits = InitialHeadingLimits; 
	m_AttachParentRelativePitchLimits = RelativeParentPitchLimits; 
	m_AttachParentRelativeHeadingLimits = RelativeParentHeadingLimits;
}

bool camCinematicMountedCamera::ShouldShakeOnImpact() const
{
	return m_Metadata.m_ShouldShakeOnImpact;
}

bool camCinematicMountedCamera::ShouldLookBehindInGameplayCamera() const
{
	return m_Metadata.m_ShouldLookBehindInGameplayCamera;
}

bool camCinematicMountedCamera::ShouldAffectAiming() const
{
	return m_Metadata.m_ShouldAffectAiming;
}

bool camCinematicMountedCamera::IsLookingLeft() const
{
	return m_RelativeHeadingWithLookBehind > 0.0f; 
}


void camCinematicMountedCamera::OverrideLookAtDampingHelpers(atHashString InVehicleLookAtDamperRef, atHashString OnFootLookAtDamperRef)
{
	if(m_InVehicleLookAtDamper)
	{
		delete m_InVehicleLookAtDamper; 
		m_InVehicleLookAtDamper = NULL;
	}
	
	if(m_OnFootLookAtDamper)
	{
		delete m_OnFootLookAtDamper; 
		m_OnFootLookAtDamper = NULL; 
	}
	
	CreateDampedLookAtHelpers(InVehicleLookAtDamperRef, OnFootLookAtDamperRef); 
}

const camControlHelper* camCinematicMountedCamera::GetControlHelper() const
{
	if(m_DisableControlHelper)
	{
		return NULL;
	}

	const bool isRenderingMobilePhoneCamera	= CPhoneMgr::CamGetState();
	const camControlHelper* controlHelper	= isRenderingMobilePhoneCamera ? m_MobilePhoneCameraControlHelper : m_ControlHelper;

	return controlHelper;
}

void camCinematicMountedCamera::CreateDampedLookAtHelpers(atHashString InVehicleLookAtDamperRef, atHashString OnFootLookAtDamperRef)
{
	const camLookAtDampingHelperMetadata* pLookAtHelperMetaData = camFactory::FindObjectMetadata<camLookAtDampingHelperMetadata>(InVehicleLookAtDamperRef);

	if(pLookAtHelperMetaData)
	{
		m_InVehicleLookAtDamper = camFactory::CreateObject<camLookAtDampingHelper>(*pLookAtHelperMetaData);
		cameraAssertf(m_InVehicleLookAtDamper, "Mounted Camera (name: %s, hash: %u) cannot create a look at damping helper (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(pLookAtHelperMetaData->m_Name.GetCStr()), pLookAtHelperMetaData->m_Name.GetHash());
	}


	pLookAtHelperMetaData = camFactory::FindObjectMetadata<camLookAtDampingHelperMetadata>(OnFootLookAtDamperRef);

	if(pLookAtHelperMetaData)
	{
		m_OnFootLookAtDamper = camFactory::CreateObject<camLookAtDampingHelper>(*pLookAtHelperMetaData);
		cameraAssertf(m_OnFootLookAtDamper, "Mounted Camera (name: %s, hash: %u) cannot create a look at damping helper (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(pLookAtHelperMetaData->m_Name.GetCStr()), pLookAtHelperMetaData->m_Name.GetHash());
	}
};

void camCinematicMountedCamera::UpdateRagdollBlendLevel()
{
	if(m_LookAtTarget && m_LookAtTarget->GetIsTypePed())
	{
		//NOTE: m_AttachParent && m_AttachParent->GetIsTypePed() have already been validated.
		const bool isAttachPedUsingRagdoll = static_cast<const CPed*>(m_LookAtTarget.Get())->GetUsingRagdoll();
		if(m_RagdollBlendEnvelope)
		{
			m_RagdollBlendEnvelope->AutoStartStop(isAttachPedUsingRagdoll);

			m_RagdollBlendLevel	= m_RagdollBlendEnvelope->Update();
			m_RagdollBlendLevel	= Clamp(m_RagdollBlendLevel, 0.0f, 1.0f);
			m_RagdollBlendLevel	= SlowInOut(m_RagdollBlendLevel);
		}
		else
		{
			//We don't have an envelope, so just use the state directly.
			m_RagdollBlendLevel = isAttachPedUsingRagdoll ? 1.0f : 0.0f;
		}
	}
	
}

void camCinematicMountedCamera::UpdateDrivebyBlendLevel()
{
	if(!m_DrivebyBlendEnvelope)
	{
		m_DriveByBlendLevel = 0.0f;
		return;
	}

	const bool isDrivebyAiming = ComputeIsPedDrivebyAiming(false);

	m_DrivebyBlendEnvelope->AutoStartStop(isDrivebyAiming);
	m_DriveByBlendLevel	= m_DrivebyBlendEnvelope->Update();
}

void camCinematicMountedCamera::UpdateRappelBlendLevel()
{
	const bool bRappelCreatingRope = ComputeIsCreatingRopeForRappel();

	const bool shouldCutSpring		= (m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation);
	const float springConstant		= shouldCutSpring ? 0.0f : m_Metadata.m_RappellingBlendLevelSpringConstant;

	m_RappellingBlendLevelSpring.Update(bRappelCreatingRope ? 1.0f : 0.0f, springConstant, m_Metadata.m_RappellingBlendLevelSpringDampingRatio);
}

void camCinematicMountedCamera::UpdateHeadingLimitNextToTurretSeatBlendLevel()
{
	const bool bPedInTurretSeat = ComputeHeadingLimitNextToTurretSeat();

	const bool shouldCutSpring		= (m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation);
	const float springConstant		= shouldCutSpring ? 0.0f : g_HeadingLimitNextToTurretSeatBlendLevelSpringConstant;

	m_HeadingLimitNextToTurretSeatBlendLevelSpring.Update(bPedInTurretSeat ? 1.0f : 0.0f, springConstant, g_HeadingLimitNextToTurretSeatBlendLevelSpringDampingRatio);
}

void camCinematicMountedCamera::UpdateOpenHeliSideDoorsBlendLevel()
{
	const bool bIsOpeningHeliRearSideDoor = ComputeIsOpeningSlidingDoorFromInside();

	const bool shouldCutSpring		= (m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation);
	const float springConstant		= shouldCutSpring ? 0.0f : m_Metadata.m_OpenHeliSideDoorBlendLevelSpringConstant;

	m_OpenHeliSideDoorBlendLevelSpring.Update(bIsOpeningHeliRearSideDoor ? 1.0f : 0.0f, springConstant, m_Metadata.m_OpenHeliSideDoorBlendLevelSpringDampingRatio);
}

void camCinematicMountedCamera::UpdateSwitchHelmetVisorBlendLevel()
{
	const bool bIsSwitchingHelmetVisor = ComputeIsSwitchingHelmetVisor();

	TUNE_GROUP_FLOAT(HELMET_VISOR, cameraBlendSpringConstant, 30.0f, 0.0f, 1000.0f, 1.0f);
	TUNE_GROUP_FLOAT(HELMET_VISOR, cameraBlendSpringDampingRatio, 1.0f, 0.0f, 10.0f, 0.1f);
	const bool shouldCutSpring		= (m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation);
	const float springConstant		= shouldCutSpring ? 0.0f : cameraBlendSpringConstant;

	m_SwitchHelmetVisorBlendLevelSpring.Update(bIsSwitchingHelmetVisor ? 1.0f : 0.0f, springConstant, cameraBlendSpringDampingRatio);
}

void camCinematicMountedCamera::UpdateScriptedVehAnimBlendLevel()
{
	const bool bScriptedVehAnimPlaying = ComputeIsScriptedVehAnimPlaying();

	const bool shouldCutSpring		= (m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation);
	const float springConstant		= shouldCutSpring ? 0.0f : m_Metadata.m_ScriptedVehAnimBlendLevelSpringConstant;

	m_ScriptedVehAnimBlendLevelSpring.Update(bScriptedVehAnimPlaying ? 1.0f : 0.0f, springConstant, m_Metadata.m_ScriptedVehAnimBlendLevelSpringDampingRatio);
}

void camCinematicMountedCamera::UpdateSmashWindowBlendLevel()
{
	if(!m_SmashWindowBlendEnvelope)
	{
		m_SmashWindowBlendLevel = 0.0f;
		return;
	}

	const bool needsToSmashWindow = ComputeNeedsToSmashWindow();
	
	m_SmashWindowBlendEnvelope->AutoStartStop(needsToSmashWindow);
	m_SmashWindowBlendLevel	= m_SmashWindowBlendEnvelope->Update();
}

void camCinematicMountedCamera::UpdateLowriderAltAnimsBlendLevel()
{
	if(!m_LowriderAltAnimsBlendEnvelope)
	{
		m_LowriderAltAnimsBlendLevel = 0.0f;
		return;
	}

	const bool isUsingLowriderAltAnims = ComputeIsPedUsingLowriderAltAnims();

	m_LowriderAltAnimsBlendEnvelope->AutoStartStop(isUsingLowriderAltAnims);

	const bool shouldCutBlendIn = (m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation);
	if(shouldCutBlendIn)
	{
		//cut the blendin.
		m_LowriderAltAnimsBlendEnvelope->OverrideAttackDuration(0);
	}

	m_LowriderAltAnimsBlendLevel	= m_LowriderAltAnimsBlendEnvelope->Update();
	m_LowriderAltAnimsBlendLevel	= Clamp(m_LowriderAltAnimsBlendLevel, 0.0f, 1.0f);
}

void camCinematicMountedCamera::UpdateUnarmedDrivebyBlendLevel()
{
	//NOTE: We only track unarmed drive-by status if we have a valid envelope for blending.
	if(!m_UnarmedDrivebyBlendEnvelope)
	{
		m_UnarmedDrivebyBlendLevel = 0.0f;
		return;
	}

	const bool isUnarmedDrivebyAiming = ComputeIsPedDrivebyAiming(true);

	m_UnarmedDrivebyBlendEnvelope->AutoStartStop(isUnarmedDrivebyAiming);

	m_UnarmedDrivebyBlendLevel	= m_UnarmedDrivebyBlendEnvelope->Update();
	m_UnarmedDrivebyBlendLevel	= Clamp(m_UnarmedDrivebyBlendLevel, 0.0f, 1.0f);
}

void camCinematicMountedCamera::UpdateVehicleMeleeBlendLevel()
{
	const bool isDoingVehMelee = ComputeIsPedDoingVehicleMelee();

	const bool shouldCutSpring		= (m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation);
	const float springConstant		= shouldCutSpring ? 0.0f : m_Metadata.m_VehMeleeBlendLevelSpringConstant;

	m_VehMeleeBlendLevelSpring.Update(isDoingVehMelee ? 1.0f : 0.0f, springConstant, m_Metadata.m_VehMeleeBlendLevelSpringDampingRatio);

	m_VehicleMeleeBlendLevel = m_VehMeleeBlendLevelSpring.GetResult();
}

bool camCinematicMountedCamera::ComputeNeedsToSmashWindow() const
{
	const CPed* pedToConsider = NULL;
	if(m_Metadata.m_ShouldAttachToFollowPedHead || m_Metadata.m_ShouldAttachToFollowPed)
	{
		if(m_LookAtTarget && m_LookAtTarget->GetIsTypePed())
		{
			pedToConsider = static_cast<const CPed*>(m_LookAtTarget.Get()); 
		}
		else
		{
			pedToConsider = camInterface::FindFollowPed();
		}
	}
	else if(m_AttachParent->GetIsTypePed())
	{
		pedToConsider = static_cast <const CPed*>(m_AttachParent.Get());
	}

	if(!pedToConsider)
	{
		return false;
	}

	const CPedIntelligence* pedIntelligence = pedToConsider->GetPedIntelligence();
	if(!pedIntelligence)
	{
		return false;
	}

	const CQueriableInterface* pedQueriableInterface = pedIntelligence->GetQueriableInterface();
	if(!pedQueriableInterface)
	{
		return false;
	}

	bool isSmashingWindow = pedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_SMASH_CAR_WINDOW, true);

	bool isDrivebyAiming = pedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY, true) || 
		pedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_MOUNT_THROW_PROJECTILE, true) || 
		pedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_SMASH_CAR_WINDOW, true);

	Vector3 vCamPos = m_Frame.GetPosition();
	Vector3 vCamFwd = m_Frame.GetFront();
	Vector3 vTargetPosWorldOut = vCamPos + (vCamFwd * 30.0f);
	return isSmashingWindow || (isDrivebyAiming && CTaskVehicleGun::NeedsToSmashWindowFromTargetPos(pedToConsider, vTargetPosWorldOut));
}

bool camCinematicMountedCamera::ComputeIsScriptedVehAnimPlaying() const
{
	const CPed* pedToConsider = NULL;
	if(m_Metadata.m_ShouldAttachToFollowPedHead || m_Metadata.m_ShouldAttachToFollowPed)
	{
		if(m_LookAtTarget && m_LookAtTarget->GetIsTypePed())
		{
			pedToConsider = static_cast<const CPed*>(m_LookAtTarget.Get()); 
		}
		else
		{
			pedToConsider = camInterface::FindFollowPed();
		}
	}
	else if(m_AttachParent->GetIsTypePed())
	{
		pedToConsider = static_cast <const CPed*>(m_AttachParent.Get());
	}

	if(!pedToConsider)
	{
		return false;
	}

	const CPedIntelligence* pedIntelligence = pedToConsider->GetPedIntelligence();
	if(!pedIntelligence)
	{
		return false;
	}

	const CTask* pTaskScriptedAnim = static_cast<const CTask*>(pedIntelligence->GetTaskSecondaryActive());
	if( pTaskScriptedAnim )
	{
		if (pTaskScriptedAnim->GetTaskType()==CTaskTypes::TASK_SCRIPTED_ANIMATION)
		{
			return true;
		}
	}

	return false;
}	

bool camCinematicMountedCamera::ComputeIsCreatingRopeForRappel() const
{
	const CPed* pedToConsider = NULL;
	if(m_Metadata.m_ShouldAttachToFollowPedHead || m_Metadata.m_ShouldAttachToFollowPed)
	{
		if(m_LookAtTarget && m_LookAtTarget->GetIsTypePed())
		{
			pedToConsider = static_cast<const CPed*>(m_LookAtTarget.Get()); 
		}
		else
		{
			pedToConsider = camInterface::FindFollowPed();
		}
	}
	else if(m_AttachParent->GetIsTypePed())
	{
		pedToConsider = static_cast <const CPed*>(m_AttachParent.Get());
	}

	if(!pedToConsider)
	{
		return false;
	}

	const CPedIntelligence* pedIntelligence = pedToConsider->GetPedIntelligence();
	if(!pedIntelligence)
	{
		return false;
	}

	const CTaskHeliPassengerRappel* pRappelTask = static_cast<const CTaskHeliPassengerRappel*>(pedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_HELI_PASSENGER_RAPPEL));

	if (pRappelTask && pRappelTask->GetState() == CTaskHeliPassengerRappel::State_CreateRope)
	{
		return true;
	}

	return false;
}

bool camCinematicMountedCamera::ComputeHeadingLimitNextToTurretSeat()
{
	m_fHeadingLimitForNextToTurretSeats = 0.0f;

	const CPed* pFollowPed = camInterface::FindFollowPed();
	if (!pFollowPed)
	{
		return false;
	}

	const CVehicle* attachVehicle = static_cast<const CVehicle*>(m_AttachParent.Get());
	if (!attachVehicle)
	{
		return false;
	}

	bool bIsInsurgentWithTurret = (MI_CAR_INSURGENT.IsValid() && attachVehicle->GetModelIndex() == MI_CAR_INSURGENT) ||
								  (MI_CAR_INSURGENT3.IsValid() && attachVehicle->GetModelIndex() == MI_CAR_INSURGENT3);
	bool bIsBarrage = MI_CAR_BARRAGE.IsValid() && attachVehicle->GetModelIndex() == MI_CAR_BARRAGE;
	bool bIsMenacer = MI_CAR_MENACER.IsValid() && attachVehicle->GetModelIndex() == MI_CAR_MENACER;

	int iSeatIndex = pFollowPed->GetAttachCarSeatIndex();
	int iLeftSeatIndex = (bIsInsurgentWithTurret || bIsMenacer) ? 2 : 0;
	int iRightSeatIndex = iLeftSeatIndex + 1;

	if ((bIsInsurgentWithTurret || bIsBarrage || bIsMenacer ) && (iSeatIndex == iLeftSeatIndex || iSeatIndex == iRightSeatIndex))
	{
		s32 iTurretSeatIndex = attachVehicle->GetFirstTurretSeat();
		CPed* pPedInTurret = attachVehicle->IsSeatIndexValid(iTurretSeatIndex) ? attachVehicle->GetPedInSeat(iTurretSeatIndex) : NULL;
		if (!pPedInTurret)
		{
			return false;
		}

		if (bIsInsurgentWithTurret || bIsMenacer)
		{
			// For the INSURGENT, we only want to limit heading when the turret ped is facing at certain angles

			float fPedHeading = pPedInTurret->GetCurrentHeading();
			float fVehicleHeading = attachVehicle->GetTransform().GetHeading();
			float fPedToVehicleHeadingDiff = Abs(fwAngle::LimitRadianAngle(fPedHeading - fVehicleHeading));

			TUNE_GROUP_FLOAT(FIRST_PERSON_TURRET_REAR_SEAT_TUNE, fHeadingDiffMin, 0.75f, 0.0f, 10.0f, 0.1f);
			TUNE_GROUP_FLOAT(FIRST_PERSON_TURRET_REAR_SEAT_TUNE, fHeadingDiffMax, 2.5f, 0.0f, 10.0f, 0.1f);
			TUNE_GROUP_FLOAT(FIRST_PERSON_TURRET_REAR_SEAT_TUNE, fHeadingLimitMin, -20.0f, -180.0f, 180.0f, 0.1f);
			if ((fPedToVehicleHeadingDiff > fHeadingDiffMin) && (fPedToVehicleHeadingDiff < fHeadingDiffMax))
			{
				Vector3 vPedForwardDir = VEC3V_TO_VECTOR3(pPedInTurret->GetTransform().GetB());
				Vector3 vVehicleRightDir = VEC3V_TO_VECTOR3(attachVehicle->GetTransform().GetA());
				bool bTurretAimingRight = vPedForwardDir.Dot(vVehicleRightDir) > 0;

				if ((bTurretAimingRight && iSeatIndex == iLeftSeatIndex) || (!bTurretAimingRight && iSeatIndex == iRightSeatIndex))
				{
					//Associating the heading limit to heading of the player in the turret (relative to the car heading)
					float fHeadingDiffRange = fHeadingDiffMax / 2.0f - fHeadingDiffMin;
					float fOffsetRange = -fHeadingLimitMin;
					m_fHeadingLimitForNextToTurretSeats = (((fPedToVehicleHeadingDiff - fHeadingDiffMin) * fOffsetRange) / fHeadingDiffRange) + fHeadingLimitMin;
					m_fHeadingLimitForNextToTurretSeats = Clamp(m_fHeadingLimitForNextToTurretSeats,fHeadingLimitMin, 0.0f);
					return true;
				}
			}
		}
		else
		{
			// For everything else, we want to limit heading all the time when turret is occupied
			TUNE_GROUP_FLOAT(FIRST_PERSON_TURRET_REAR_SEAT_TUNE, fHeadingLimitFixed, -5.0f, -180.0f, 180.0f, 0.1f);
			m_fHeadingLimitForNextToTurretSeats = fHeadingLimitFixed;
			return true;
		}
	}

	return false;
}

bool camCinematicMountedCamera::ComputeIsOpeningSlidingDoorFromInside() const
{
	const CPed* pedToConsider = NULL;
	if(m_Metadata.m_ShouldAttachToFollowPedHead || m_Metadata.m_ShouldAttachToFollowPed)
	{
		if(m_LookAtTarget && m_LookAtTarget->GetIsTypePed())
		{
			pedToConsider = static_cast<const CPed*>(m_LookAtTarget.Get()); 
		}
		else
		{
			pedToConsider = camInterface::FindFollowPed();
		}
	}
	else if(m_AttachParent->GetIsTypePed())
	{
		pedToConsider = static_cast <const CPed*>(m_AttachParent.Get());
	}

	if(!pedToConsider)
	{
		return false;
	}

	CVehicle* pVeh = pedToConsider->GetVehiclePedInside();
	if (!pVeh)
	{
		return false;
	}

	const CPedIntelligence* pedIntelligence = pedToConsider->GetPedIntelligence();
	if(!pedIntelligence)
	{
		return false;
	}

	// Check for valid task
	bool bIsOpeningOrClosingDoor = false;
	const CTaskVehicleGun* pVehicleGunTask = static_cast<const CTaskVehicleGun*>(pedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_VEHICLE_GUN));
	if (pVehicleGunTask && pVehicleGunTask->GetState() == CTaskVehicleGun::State_OpeningDoor)
	{
		bIsOpeningOrClosingDoor = true;
	}
	else
	{
		const CTaskMotionInAutomobile* pInVehicleTask = static_cast<const CTaskMotionInAutomobile*>(pedIntelligence->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_AUTOMOBILE));
		if (pInVehicleTask && pInVehicleTask->GetState() == CTaskMotionInAutomobile::State_CloseDoor)
		{
			bIsOpeningOrClosingDoor = true;
		}
		else
		{
			const CTaskMountThrowProjectile* pVehicleProjectileTask = static_cast<const CTaskMountThrowProjectile*>(pedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_MOUNT_THROW_PROJECTILE));
			if (pVehicleProjectileTask && pVehicleProjectileTask->GetState() == CTaskMountThrowProjectile::State_OpenDoor)
			{
				bIsOpeningOrClosingDoor = true;
			}
		}
	}

	if (bIsOpeningOrClosingDoor)
	{
		// Check if door is sliding
		const CSeatManager* pSeatManager = pVeh->GetSeatManager();
		s32 iSeatIndex = pSeatManager ? pSeatManager->GetPedsSeatIndex(pedToConsider) : -1;
		if (pVeh->IsSeatIndexValid(iSeatIndex))
		{
			s32 iEntryPointIndex = pVeh->IsSeatIndexValid(iSeatIndex) ? pVeh->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iSeatIndex, pVeh) : -1;
			if (pVeh->IsEntryIndexValid(iEntryPointIndex))
			{
				CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(pVeh, iEntryPointIndex);
				if (pDoor && (pDoor->GetFlag(CCarDoor::AXIS_SLIDE_X) || pDoor->GetFlag(CCarDoor::AXIS_SLIDE_Y) || pDoor->GetFlag(CCarDoor::AXIS_SLIDE_Z)))
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool camCinematicMountedCamera::ComputeIsSwitchingHelmetVisor() const
{
	const CPed* pedToConsider = NULL;
	if(m_Metadata.m_ShouldAttachToFollowPedHead || m_Metadata.m_ShouldAttachToFollowPed)
	{
		if(m_LookAtTarget && m_LookAtTarget->GetIsTypePed())
		{
			pedToConsider = static_cast<const CPed*>(m_LookAtTarget.Get()); 
		}
		else
		{
			pedToConsider = camInterface::FindFollowPed();
		}
	}
	else if(m_AttachParent->GetIsTypePed())
	{
		pedToConsider = static_cast <const CPed*>(m_AttachParent.Get());
	}

	if(!pedToConsider)
	{
		return false;
	}

	if (pedToConsider->GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingHelmetVisor))
	{
		return true;
	}

	return false;
}

bool camCinematicMountedCamera::ComputeIsPedDrivebyAiming(bool bCheckForUnarmed) const
{
	TUNE_BOOL(shouldConsiderAimingInPovCamera, false)
	if(shouldConsiderAimingInPovCamera)
	{
		return true;
	}

	const CPed* pedToConsider = NULL;
	if(m_Metadata.m_ShouldAttachToFollowPedHead || m_Metadata.m_ShouldAttachToFollowPed)
	{
		if(m_LookAtTarget && m_LookAtTarget->GetIsTypePed())
		{
			pedToConsider = static_cast<const CPed*>(m_LookAtTarget.Get()); 
		}
		else
		{
			pedToConsider = camInterface::FindFollowPed();
		}
	}
	else if(m_AttachParent->GetIsTypePed())
	{
		pedToConsider = static_cast<const CPed*>(m_AttachParent.Get());
	}
	else if(!IsFirstPersonCamera() && m_AttachParent->GetIsTypeVehicle())
	{
		pedToConsider = camInterface::FindFollowPed();
	}

	if(!pedToConsider)
	{
		return false;
	}

	const CPedWeaponManager* pedWeaponManager = pedToConsider->GetWeaponManager();
	if(!pedWeaponManager)
	{
		return false;
	}

	if(bCheckForUnarmed && !pedWeaponManager->GetIsArmedMelee())
	{
		return false;
	}

	if(!bCheckForUnarmed && pedWeaponManager->GetIsArmedMelee())
	{
		return false;
	}

	const CPedIntelligence* pedIntelligence = pedToConsider->GetPedIntelligence();
	if(!pedIntelligence)
	{
		return false;
	}

	const CQueriableInterface* pedQueriableInterface = pedIntelligence->GetQueriableInterface();
	if(!pedQueriableInterface)
	{
		return false;
	}
	
	bool isDrivebyAiming = pedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY, true) || 
			(pedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_MOUNT_THROW_PROJECTILE, true) && !pedToConsider->GetPedConfigFlag(CPED_CONFIG_FLAG_IsPerformingVehicleMelee)) || 
			pedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_SMASH_CAR_WINDOW, true);

	return isDrivebyAiming;
}

bool camCinematicMountedCamera::ComputeIsPedDoingVehicleMelee() const
{
	const CPed* pedToConsider = NULL;
	if(m_Metadata.m_ShouldAttachToFollowPedHead || m_Metadata.m_ShouldAttachToFollowPed)
	{
		if(m_LookAtTarget && m_LookAtTarget->GetIsTypePed())
		{
			pedToConsider = static_cast<const CPed*>(m_LookAtTarget.Get()); 
		}
		else
		{
			pedToConsider = camInterface::FindFollowPed();
		}
	}
	else if(m_AttachParent->GetIsTypePed())
	{
		pedToConsider = static_cast<const CPed*>(m_AttachParent.Get());
	}
	else if(!IsFirstPersonCamera() && m_AttachParent->GetIsTypeVehicle())
	{
		pedToConsider = camInterface::FindFollowPed();
	}

	if(!pedToConsider)
	{
		return false;
	}

	return pedToConsider->GetPedConfigFlag(CPED_CONFIG_FLAG_IsPerformingVehicleMelee);
}

bool camCinematicMountedCamera::ComputeIsPedUsingLowriderAltAnims() const
{
	const CPed* pedToConsider = NULL;
	if(m_Metadata.m_ShouldAttachToFollowPedHead || m_Metadata.m_ShouldAttachToFollowPed)
	{
		if(m_LookAtTarget && m_LookAtTarget->GetIsTypePed())
		{
			pedToConsider = static_cast<const CPed*>(m_LookAtTarget.Get()); 
		}
		else
		{
			pedToConsider = camInterface::FindFollowPed();
		}
	}
	else if(m_AttachParent->GetIsTypePed())
	{
		pedToConsider = static_cast<const CPed*>(m_AttachParent.Get());
	}
	else if(!IsFirstPersonCamera() && m_AttachParent->GetIsTypeVehicle())
	{
		pedToConsider = camInterface::FindFollowPed();
	}

	if(!pedToConsider)
	{
		return false;
	}

	return pedToConsider->GetPedConfigFlag(CPED_CONFIG_FLAG_UsingAlternateLowriderLeans);
}

void camCinematicMountedCamera::UpdateDuckingBlendLevel()
{
	//NOTE: We only track ducking status if we have a valid envelope for blending.
	if(!m_DuckingBlendEnvelope)
	{
		m_DuckingBlendLevel = 0.0f;
		return;
	}

	const bool isDucking = ComputeIsPedDucking();

	m_DuckingBlendEnvelope->AutoStartStop(isDucking);

	m_DuckingBlendLevel	= m_DuckingBlendEnvelope->Update();
	m_DuckingBlendLevel	= Clamp(m_DuckingBlendLevel, 0.0f, 1.0f);
}

bool camCinematicMountedCamera::ComputeIsPedDucking() const
{
	const CPed* pedToConsider = NULL;
	if(m_Metadata.m_ShouldAttachToFollowPedHead || m_Metadata.m_ShouldAttachToFollowPed || m_AttachedToTurret)
	{
		if(m_LookAtTarget && m_LookAtTarget->GetIsTypePed())
		{
			pedToConsider = static_cast<const CPed*>(m_LookAtTarget.Get()); 
		}
		else
		{
			pedToConsider = camInterface::FindFollowPed();
		}
	}
	else if(m_AttachParent->GetIsTypePed())
	{
		pedToConsider = static_cast <const CPed*>(m_AttachParent.Get());
	}

	if(!pedToConsider)
	{
		return false;
	}

	const CPedIntelligence* pedIntelligence = pedToConsider->GetPedIntelligence();
	if(!pedIntelligence)
	{
		return false;
	}

	bool isPedDucking = false;
	if (!m_AttachedToTurret)
	{
		const CTaskMotionInAutomobile* motionTask	= static_cast<const CTaskMotionInAutomobile*>(pedIntelligence->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_AUTOMOBILE));
		isPedDucking								= motionTask && motionTask->IsDucking();
	}
	else
	{
		const CTaskMotionInTurret* motionTask	= static_cast<const CTaskMotionInTurret*>(pedIntelligence->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_TURRET));
		isPedDucking							= motionTask && motionTask->IsDucking();
	}

	return isPedDucking;
}

void camCinematicMountedCamera::UpdateSeatShufflingBlendLevel()
{
	if (m_Metadata.m_LookAtBehaviour != LOOK_FORWARD_RELATIVE_TO_ATTACH)
	{
		m_SeatSwitchingLimitSpring.Reset();

		return;
	}

	const CPed* followPed					= camInterface::FindFollowPed();
	const CPedIntelligence* pedIntelligence	= followPed ? followPed->GetPedIntelligence() : NULL;
	CTaskInVehicleSeatShuffle* shuffleTask	= pedIntelligence ? static_cast<CTaskInVehicleSeatShuffle*>(pedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE)) : NULL;
	m_SeatShufflePhase = 0.0f;
	if (shuffleTask)
	{
		shuffleTask->GetClipAndPhaseForState(m_SeatShufflePhase);
		m_SeatShuffleTargetSeat = shuffleTask->GetTargetSeatIndex();

		const CVehicle* pPedsVehicle = followPed->GetVehiclePedInside();
		if (pPedsVehicle)
		{
			bool bIsMogul = MI_PLANE_MOGUL.IsValid() && pPedsVehicle->GetModelIndex() == MI_PLANE_MOGUL;
			bool bIsTula = MI_PLANE_TULA.IsValid() && pPedsVehicle->GetModelIndex() == MI_PLANE_TULA;
			bool bIsBombushka = MI_PLANE_BOMBUSHKA.IsValid() && pPedsVehicle->GetModelIndex() == MI_PLANE_BOMBUSHKA;
			bool bIsVolatol = MI_PLANE_VOLATOL.IsValid() && pPedsVehicle->GetModelIndex() == MI_PLANE_VOLATOL;
			if (m_IsShuffleTaskFirstUpdate && (bIsMogul || bIsTula || bIsBombushka || bIsVolatol))
			{
				m_SeatShuffleChangedCameras = false;
			}

			m_IsShuffleTaskFirstUpdate = false;
		}
	}
	else
	{
		m_SeatShuffleTargetSeat = -1;
		if(m_SeatSwitchingLimitSpring.GetResult() >= SMALL_FLOAT && followPed)
		{
			m_SeatShuffleTargetSeat = followPed->GetAttachCarSeatIndex();
		}
		m_IsShuffleTaskFirstUpdate = true;
	}

	const bool shouldCutSpring				= (m_NumUpdatesPerformed == 0 && m_bAllowSpringCutOnFirstUpdate) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
												m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation) || !IsFirstPersonCamera();
	const float seatShufflingSpringConstant	= shouldCutSpring ? 0.0f : m_Metadata.m_SeatShufflingSpringConstant;
	const float desiredShufflingBlendLevel	= (m_SeatShufflePhase >= SMALL_FLOAT) && (m_SeatShufflePhase <= m_Metadata.m_SeatShufflingPhaseThreshold) ? 1.0f : 0.0f;
	m_SeatSwitchingLimitSpring.Update(desiredShufflingBlendLevel, seatShufflingSpringConstant, m_Metadata.m_SeatShufflingSpringDampingRatio);

	m_SeatShuffleChangedCameras |= (m_NumUpdatesPerformed == 0 && m_SeatSwitchingLimitSpring.GetResult() >= SMALL_FLOAT);
	m_SeatShuffleChangedCameras &= (m_SeatSwitchingLimitSpring.GetResult() >= SMALL_FLOAT);
}

void camCinematicMountedCamera::UpdateAccelerationMovement()
{
	if (!m_AttachParent || !m_AttachParent->GetIsTypeVehicle() || !m_Metadata.m_AccelerationMovementSettings.m_ShouldMoveOnVehicleAcceleration)
	{
		return;
	}

	const CVehicle* attachVehicle			= static_cast<const CVehicle*>(m_AttachParent.Get());
	const Vector3& attachVehicleVelocity	= attachVehicle->GetVelocity();
	const Vector3& cameraFront				= m_Frame.GetFront();
	float attachVehicleSpeed				= attachVehicleVelocity.Dot(cameraFront);

	if (!m_Metadata.m_AccelerationMovementSettings.m_ShouldApplyWhenReversing)
	{
		attachVehicleSpeed = Max(attachVehicleSpeed, 0.0f);
	}

	if (m_NumUpdatesPerformed == 0)
	{
		m_CamFrontRelativeAttachSpeedOnPreviousUpdate = attachVehicleSpeed;
	}

	//we don't update the spring unless we're moving
	if (attachVehicleVelocity.Mag2() < VERY_SMALL_FLOAT)
	{
		return;
	}

	const float invTimeStep = fwTimer::GetInvTimeStep();

	const float attachVehicleForwardAcceleration = (attachVehicleSpeed - m_CamFrontRelativeAttachSpeedOnPreviousUpdate) * invTimeStep;

	float desiredBlendLevel = RampValueSafe(attachVehicleForwardAcceleration,
								m_Metadata.m_AccelerationMovementSettings.m_MinAbsForwardAcceleration, m_Metadata.m_AccelerationMovementSettings.m_MaxAbsForwardAcceleration,
								0.0f, 1.0f);

	if (m_Metadata.m_AccelerationMovementSettings.m_ShouldApplyWhenReversing)
	{
		desiredBlendLevel	*= Sign(attachVehicleSpeed);
	}

	const float blendLevelOnPreviousUpdate = m_AccelerationMovementSpring.GetResult();

	const bool shouldCutSpring		= (m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation);
	const float springConstant		= shouldCutSpring ? 0.0f :
										(desiredBlendLevel > blendLevelOnPreviousUpdate) ?
										m_Metadata.m_AccelerationMovementSettings.m_BlendInSpringConstant : m_Metadata.m_AccelerationMovementSettings.m_BlendOutSpringConstant;

	m_AccelerationMovementSpring.Update(desiredBlendLevel, springConstant, m_Metadata.m_AccelerationMovementSettings.m_SpringDampingRatio);
	
	m_CamFrontRelativeAttachSpeedOnPreviousUpdate = attachVehicleSpeed;
}

bool camCinematicMountedCamera::Update()
{
	if(!m_AttachParent.Get() || !m_AttachParent->GetIsPhysical() || m_canUpdate == false) //Safety check.
	{
		m_canUpdate = false;
		return m_canUpdate;
	}

#if RSG_PC
	const CViewport* viewport	= gVpMan.GetGameViewport();
	const float aspectRatio		= viewport ? viewport->GetGrcViewport().GetAspectRatio() : (16.0f / 9.0f);
	const bool bUsingTripleHead = camInterface::IsTripleHeadDisplay();
#else
	const float aspectRatio		= (16.0f / 9.0f);		// doesn't matter, not used for consoles anyway.
	const bool bUsingTripleHead = false;
#endif

	UpdateLowriderAltAnimsBlendLevel();
	UpdateVehicleMeleeBlendLevel();
	UpdateUnarmedDrivebyBlendLevel();
	UpdateDrivebyBlendLevel();
	UpdateSmashWindowBlendLevel();
	UpdateRappelBlendLevel();
	UpdateHeadingLimitNextToTurretSeatBlendLevel();
	UpdateOpenHeliSideDoorsBlendLevel();
	UpdateSwitchHelmetVisorBlendLevel();
	UpdateScriptedVehAnimBlendLevel();
	UpdateDuckingBlendLevel();
	UpdateSeatShufflingBlendLevel();
	UpdateAccelerationMovement();

	const bool isRenderingMobilePhoneCamera	= CPhoneMgr::CamGetState();

	if(m_ControlHelper)
	{
		const float seatShufflingBlendLevel = m_SeatSwitchingLimitSpring.GetResult();

		if((m_UnarmedDrivebyBlendLevel >= SMALL_FLOAT) || (m_DuckingBlendLevel >= SMALL_FLOAT) || (seatShufflingBlendLevel >= SMALL_FLOAT) || (m_VehicleMeleeBlendLevel >= SMALL_FLOAT))
		{
			//Inhibit look-behind during unarmed drive-by aiming and ducking and seat shuffling.
			m_ControlHelper->SetLookBehindState(false);
			m_ControlHelper->IgnoreLookBehindInputThisUpdate();
		}

		//Ignore look behind input during seat shuffling
		if (seatShufflingBlendLevel >= SMALL_FLOAT)
		{
			m_ControlHelper->IgnoreLookBehindInputThisUpdate();
		}

		if ((m_DriveByBlendLevel >= SMALL_FLOAT) || (m_UnarmedDrivebyBlendLevel >= SMALL_FLOAT) || (m_VehicleMeleeBlendLevel >= SMALL_FLOAT))
		{
			m_ControlHelper->SetForceAimSensitivityThisUpdate();
		}
		else
		{
			if (!isRenderingMobilePhoneCamera && !IsVehicleTurretCamera())
			{
				m_ControlHelper->SetForceDefaultLookAroundSensitivityThisUpdate();
			}
		}

#if RSG_PC
		if(camInterface::GetGameplayDirector().IsForcingFirstPersonBonnet() && !IsFirstPersonCamera())
		{
			// Disable input if we are forcing bonnet camera.
			////m_ControlHelper->SetLookBehindState(false);
			////m_ControlHelper->IgnoreLookBehindInputThisUpdate();
			m_ControlHelper->IgnoreLookAroundInputThisUpdate();
			m_RelativeHeading = 0.0f;
			m_RelativePitch = 0.0f;
		}
#endif

		m_ControlHelper->Update(*m_AttachParent);
	}

	if(isRenderingMobilePhoneCamera && m_MobilePhoneCameraControlHelper)
	{
		m_MobilePhoneCameraControlHelper->Update(*m_AttachParent);
	}

	UpdateRagdollBlendLevel();

	const CPhysical& attachParentPhysical = static_cast<const CPhysical&>(*m_AttachParent.Get());

	Matrix34 attachMatrix;
	ComputeAttachMatrix(attachParentPhysical, attachMatrix); 

	m_DesiredRelativeHeading = 0.0f;
	m_BaseHeadingRadians = m_Metadata.m_BaseHeading * DtoR;
	attachMatrix.RotateLocalZ(m_BaseHeadingRadians);

	m_AttachMatrix = attachMatrix;

	Vector3 cameraPosition;
	
	Vector3 AttachPosition = m_Metadata.m_RelativeAttachPosition;
	if(bUsingTripleHead && !m_Metadata.m_TripleHeadRelativeAttachPosition.IsZero())
	{
		AttachPosition = m_Metadata.m_TripleHeadRelativeAttachPosition;
	}

	const bool bAllowedRearSeatLookback = ShouldAllowRearSeatLookback();	

	const camControlHelper* controlHelper = GetControlHelper();
	if(controlHelper)
	{
		if(CanLookBehind() && !ShouldLookBehindInGameplayCamera() && bAllowedRearSeatLookback && m_Metadata.m_ShouldUseLookBehindCustomPosition)
		{
			AttachPosition = m_Metadata.m_LookBehindRelativeAttachPosition; 

			if(!m_IsSeatPositionOnLeftHandSide)
			{
				AttachPosition.x *= -1.0f; 
			}
		}
	}
	
	if(m_AttachParent->GetIsTypeVehicle())
	{
		const CVehicle* attachVehicle				= static_cast<const CVehicle*>(m_AttachParent.Get()); 
		const CVehicleModelInfo* modelInfo			= attachVehicle->GetVehicleModelInfo();

		if (modelInfo)
		{
			//Add offsets specified by the vehicle model
			if (m_Metadata.m_ShouldAttachToFollowPed && !m_Metadata.m_ShouldIgnoreVehicleSpecificAttachPositionOffset)
			{
				const Vector3 vehicleModelCameraOffset	= modelInfo->GetPovCameraOffset();
				AttachPosition							+= vehicleModelCameraOffset;

				if (m_ShouldAdjustRelativeAttachPositionForRollCage)
				{
					const float vehicleModelAdjustment	= modelInfo->GetPovCameraVerticalAdjustmentForRollCage();
					AttachPosition.z					+= vehicleModelAdjustment;
				}
			}

			const CFirstPersonDriveByLookAroundData *pDrivebyLookData = GetFirstPersonDrivebyData(attachVehicle);

			Vector3 vLookAroundDrivebyOffset(Vector3::ZeroType);

			if(pDrivebyLookData && m_DriveByBlendLevel > 0.0f)
			{
				bool bLeftSide = m_RelativeHeadingWithLookBehind > 0.0f;
				float fRelativeHeadingAbs = abs(m_RelativeHeadingWithLookBehind);

				const CFirstPersonDriveByLookAroundSideData & SideData = bLeftSide ? pDrivebyLookData->GetDataLeft() : pDrivebyLookData->GetDataRight();

				//! If we are trying to smash a window, blend out offset as otherwise we may go through it.
				float fSmashWindowScale = (1.0f - m_SmashWindowBlendLevel);

				Vector3 vDrivebyOffset(Vector3::ZeroType);
				for(int i = 0; i < 3; ++i)
				{
					const CFirstPersonDriveByLookAroundSideData::CAxisData *pAxisData = SideData.GetAxis(i);
					if(pAxisData)
					{
						float fLookAroundOffset = pAxisData->m_Offset;
						float fMinAngle = pAxisData->m_AngleToBlendInOffset.x * DtoR;
						float fMaxAngle = pAxisData->m_AngleToBlendInOffset.y * DtoR;

						//Start blending in the vertical and forwards offsets when we're past the angular threshold from the centered position
						if ( (fRelativeHeadingAbs >= fMinAngle) && (fMaxAngle - fMinAngle) >= SMALL_FLOAT)
						{
							float fOffsetRatio = Clamp( (fRelativeHeadingAbs - fMinAngle) / (fMaxAngle - fMinAngle), 0.0f, 1.0f);
							if (i == 0)
							{
								const float fSign = bLeftSide ? 1.0f : -1.0f; // Only flip axis for X value
								fOffsetRatio *= fSign;
							}
							vDrivebyOffset[i] = fOffsetRatio * fLookAroundOffset * fSmashWindowScale;
						}
					}
				}

				vLookAroundDrivebyOffset = vDrivebyOffset;
			}

			Vector3 vLookAroundOffset(Vector3::ZeroType);
			const CPed* pFollowPed = camInterface::FindFollowPed();

			if (m_Metadata.m_ShouldUseAdditionalRelativeAttachOffsetWhenLookingAround || (m_Metadata.m_ShouldUseAdditionalRelativeAttachOffsetForDriverOnlyWhenLookingAround && pFollowPed->GetIsDrivingVehicle()))
			{
				float fDrivebyLevel = pDrivebyLookData ? 0.0f : m_DriveByBlendLevel; 
				float fLookAroundblendLevel = m_VehicleMeleeBlendLevel > SMALL_FLOAT ? 1.0f - m_VehicleMeleeBlendLevel : 1.0f - fDrivebyLevel;

				//Blend in additional camera offsets when looking around to prevent the camera
				//clipping into the peds body
				Vector3 vVehFwd = m_Metadata.m_UseAttachMatrixForAdditionalOffsetWhenLookingAround ?  attachMatrix.b : VEC3V_TO_VECTOR3(attachVehicle->GetTransform().GetB());
				vVehFwd.Normalize();
				Vector3 vCamFwd = m_Frame.GetFront();

				if(m_UseScriptRelativeHeading || m_UseScriptRelativePitch)
				{
					float vehicleHeading, vehiclePitch;
					camFrame::ComputeHeadingAndPitchFromFront(vVehFwd, vehicleHeading, vehiclePitch);

					float heading, pitch;
					camFrame::ComputeHeadingAndPitchFromFront(vCamFwd, heading, pitch);
					if(m_UseScriptRelativeHeading)
					{
						heading = vehicleHeading + m_CachedScriptHeading;
					}

					if(m_UseScriptRelativePitch)
					{
						pitch = vehiclePitch + m_CachedScriptPitch;
					}

					camFrame::ComputeFrontFromHeadingAndPitch(heading, pitch, vCamFwd);
				}

				//When we manually toggle to first person, we may be pointing the third person camera to the side
				//we want to just cut to the correct offset in this case, otherwise we'll end up intersecting the ped
				bool bJustSwitchedToFirstPerson = false;
				if (pFollowPed && pFollowPed->GetPlayerInfo()) 
				{
					bJustSwitchedToFirstPerson = pFollowPed->GetPlayerInfo()->GetSwitchedToFirstPersonCount() > 0 ? true : false;
					// HACK - if we've switched back to first person from cinematic we need to manually flag 
					// us as having switched to first person so we don't do the side offset blending
					if (!bJustSwitchedToFirstPerson && m_NumUpdatesPerformed == 0 && pFollowPed->GetMyVehicle() 
						&& (pFollowPed->GetMyVehicle()->InheritsFromBike() || pFollowPed->GetMyVehicle()->InheritsFromQuadBike() || pFollowPed->GetMyVehicle()->InheritsFromAmphibiousQuadBike())&& camInterface::GetGameplayDirector().GetFollowCamera())
					{
						bJustSwitchedToFirstPerson = true;
					}

					if (bJustSwitchedToFirstPerson)
					{
						vCamFwd = camInterface::GetGameplayDirector().GetFrame().GetFront();
					}
				}

				if( IsInterpolating() )
				{
					vCamFwd = GetFrameInterpolator()->GetFrame().GetFront();
				}
				else if (!bJustSwitchedToFirstPerson)
				{
					//if this camera is cut too, blend in this behavior else there is a pop because the frame front is invalid of the first update.
					if(m_NumUpdatesPerformed == 0)
					{	
						vCamFwd = attachMatrix.b;

						float relativeHeading, relativePitch;
						if(RestoreCameraOrientation(attachMatrix, relativeHeading, relativePitch))
						{
							//apply relative rotations
							vCamFwd.RotateZ(relativeHeading);
							vCamFwd.RotateX(relativePitch);
						}
						else
						{
							m_bShouldBlendRelativeAttachOffsetWhenLookingAround = true; 
						}
					}
					else if(m_bShouldBlendRelativeAttachOffsetWhenLookingAround)
					{
						float camHeading = rage::Atan2f(-vCamFwd.x, vCamFwd.y);
						float attachHeading = rage::Atan2f(-attachMatrix.b.x, attachMatrix.b.y);
						float blendDuration = 1.0f; 

						float blendlevel = m_BlendLookAroundTime / blendDuration; 
						
						blendlevel = Clamp(blendlevel, 0.0f, 1.0f); 

						camHeading = fwAngle::LerpTowards(attachHeading, camHeading, blendlevel);
						vCamFwd = Vector3(0.0f,1.0f,0.0f);
						vCamFwd.RotateZ(camHeading);

						m_BlendLookAroundTime += fwTimer::GetCamTimeStep();

						if(blendlevel == 1.0f)
						{
							m_bShouldBlendRelativeAttachOffsetWhenLookingAround = false; 
						}
					}
				}

				vCamFwd.Normalize();

				float fMinLookAroundAngleToBlendInHeightOffsetNotDriveBy = m_Metadata.m_LookAroundSettings.m_MinLookAroundAngleToBlendInHeightOffset;
				float fMaxLookAroundAngleToBlendInHeightOffsetNotDriveBy = m_Metadata.m_LookAroundSettings.m_MaxLookAroundAngleToBlendInHeightOffset;

				TUNE_GROUP_BOOL(FIRST_PERSON_BIKE_TUNE, USE_LOOK_BEHIND_BLEND_IN_ANGLES, true);
				if (USE_LOOK_BEHIND_BLEND_IN_ANGLES)
				{
					TUNE_GROUP_FLOAT(FIRST_PERSON_BIKE_TUNE, MIN_LOOK_AROUND_ANGLE_LOOK_BEHIND, 0.0f, 0.0f, 90.0f, 1.0f);
					TUNE_GROUP_FLOAT(FIRST_PERSON_BIKE_TUNE, MAX_LOOK_AROUND_ANGLE_LOOK_BEHIND, 30.0f, 0.0f, 90.0f, 1.0f);
					const bool bIsLookingBehind = controlHelper && CanLookBehind() && !ShouldLookBehindInGameplayCamera() && bAllowedRearSeatLookback;
					if (bIsLookingBehind)
					{
						fMinLookAroundAngleToBlendInHeightOffsetNotDriveBy = MIN_LOOK_AROUND_ANGLE_LOOK_BEHIND;
						fMaxLookAroundAngleToBlendInHeightOffsetNotDriveBy = MAX_LOOK_AROUND_ANGLE_LOOK_BEHIND;
					}
				}

				const float fMinLookAroundAngleToBlendInHeightOffset =	fLookAroundblendLevel * fMinLookAroundAngleToBlendInHeightOffsetNotDriveBy + 
													fDrivebyLevel * m_Metadata.m_DriveByLookAroundSettings.m_MinLookAroundAngleToBlendInHeightOffset + 
									m_VehicleMeleeBlendLevel * m_Metadata.m_VehicleMeleeLookAroundSettings.m_MinLookAroundAngleToBlendInHeightOffset;

				const float fMaxLookAroundAngleToBlendInHeightOffset = fLookAroundblendLevel * fMaxLookAroundAngleToBlendInHeightOffsetNotDriveBy + 
													fDrivebyLevel * m_Metadata.m_DriveByLookAroundSettings.m_MaxLookAroundAngleToBlendInHeightOffset + 
									m_VehicleMeleeBlendLevel * m_Metadata.m_VehicleMeleeLookAroundSettings.m_MaxLookAroundAngleToBlendInHeightOffset;

				const float fVehCamDot = vVehFwd.Dot(vCamFwd);
				const float fVehCamDotInv = 1.0f - fVehCamDot;
				const float fMinCosAngleInv = 1.0f - Cosf(fMinLookAroundAngleToBlendInHeightOffset * DtoR);
				const float fMaxCosAngleInv = 1.0f - Cosf(fMaxLookAroundAngleToBlendInHeightOffset * DtoR);
				
				//Start blending in the vertical and forwards offsets when we're past the angular threshold from the centered position
				if (fVehCamDotInv >= fMinCosAngleInv && (fMaxCosAngleInv - fMinCosAngleInv) >= SMALL_FLOAT)
				{
					float fOffsetRatio = Clamp((fVehCamDotInv - fMinCosAngleInv) / (fMaxCosAngleInv - fMinCosAngleInv), 0.0f, 1.0f);

					//Scale the blend in rate based on the lean angle of the bike
					const float fLeanAngleSignal = CTaskMotionInAutomobile::ComputeLeanAngleSignal(*attachVehicle);

					//Renormalise the lean angle signal (0 / 1 are lean extents) so 0 is no lean and 1 is full lean
					const float fLeanNorm = Abs(0.5f - fLeanAngleSignal) * 2.0f;
					const float fLeanScale =  fLookAroundblendLevel * m_Metadata.m_LookAroundSettings.m_LeanScale + 
												fDrivebyLevel * m_Metadata.m_DriveByLookAroundSettings.m_LeanScale + 
								m_VehicleMeleeBlendLevel * m_Metadata.m_VehicleMeleeLookAroundSettings.m_LeanScale;

					fOffsetRatio *= (1.0f + fLeanNorm) * fLeanScale;
					fOffsetRatio = Clamp(fOffsetRatio, 0.0f, 1.0f);
					
					//Add each scaled component offset
					const float fMinLookAroundForwardOffset = fLookAroundblendLevel * m_Metadata.m_LookAroundSettings.m_MinLookAroundForwardOffset + 
																fDrivebyLevel * m_Metadata.m_DriveByLookAroundSettings.m_MinLookAroundForwardOffset + 
												m_VehicleMeleeBlendLevel * m_Metadata.m_VehicleMeleeLookAroundSettings.m_MinLookAroundForwardOffset;

					const float fMaxLookAroundForwardOffset = fLookAroundblendLevel * m_Metadata.m_LookAroundSettings.m_MaxLookAroundForwardOffset + 
																fDrivebyLevel * m_Metadata.m_DriveByLookAroundSettings.m_MaxLookAroundForwardOffset + 
												m_VehicleMeleeBlendLevel * m_Metadata.m_VehicleMeleeLookAroundSettings.m_MaxLookAroundForwardOffset;

					const float fFwdOffset = (1.0f - fOffsetRatio) * fMinLookAroundForwardOffset + fOffsetRatio * fMaxLookAroundForwardOffset;
					vLookAroundOffset.y += fOffsetRatio * fFwdOffset;
					
					const float fMaxLookAroundHeightOffset =  fLookAroundblendLevel * m_Metadata.m_LookAroundSettings.m_MaxLookAroundHeightOffset + 
																fDrivebyLevel * m_Metadata.m_DriveByLookAroundSettings.m_MaxLookAroundHeightOffset +
												m_VehicleMeleeBlendLevel * m_Metadata.m_VehicleMeleeLookAroundSettings.m_MaxLookAroundHeightOffset;

					vLookAroundOffset.z += fOffsetRatio * fMaxLookAroundHeightOffset;
				}
				//Blend the side offset in, taking into account the side we're looking at
				float fSideOffsetRatio = Clamp(fVehCamDotInv / fMaxCosAngleInv, 0.0f, 1.0f);
				bool bLeftSide = vVehFwd.CrossZ(vCamFwd) > 0.0f ? true : false;
				if (fVehCamDot < 0.5f)
				{
					if (!m_HaveCachedSideForSideOffset)
					{
						m_ShouldApplySideOffsetToLeft = bLeftSide;
						m_HaveCachedSideForSideOffset = true;
					}
				}
				else
				{
					m_HaveCachedSideForSideOffset = false;
				}

				if (m_HaveCachedSideForSideOffset)
				{
					bLeftSide = m_ShouldApplySideOffsetToLeft;
				}

				const float fMaxLookAroundSideOffsetNormal = bLeftSide ?  m_Metadata.m_LookAroundSettings.m_MaxLookAroundSideOffsetLeft :  m_Metadata.m_LookAroundSettings.m_MaxLookAroundSideOffsetRight;
				const float fMaxLookAroundSideOffsetDriveBy = bLeftSide ?  m_Metadata.m_DriveByLookAroundSettings.m_MaxLookAroundSideOffsetLeft :  m_Metadata.m_DriveByLookAroundSettings.m_MaxLookAroundSideOffsetRight;
				const float fMaxLookAroundSideOffsetVehMelee = bLeftSide ?  m_Metadata.m_VehicleMeleeLookAroundSettings.m_MaxLookAroundSideOffsetLeft :  m_Metadata.m_VehicleMeleeLookAroundSettings.m_MaxLookAroundSideOffsetRight;
				const float fMaxLookAroundSideOffset = fLookAroundblendLevel * fMaxLookAroundSideOffsetNormal + m_DriveByBlendLevel * fMaxLookAroundSideOffsetDriveBy + m_VehicleMeleeBlendLevel * fMaxLookAroundSideOffsetVehMelee;
				const float fSideOffset = fSideOffsetRatio * fMaxLookAroundSideOffset;
				const float fSign = bLeftSide ? 1.0f : -1.0f;

				m_DesiredSideOffset = fSign * fSideOffsetRatio * fSideOffset;

				const bool shouldCutSpring	= (m_NumUpdatesPerformed == 0 && m_bAllowSpringCutOnFirstUpdate) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
												m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation) || !IsFirstPersonCamera();

				const float springConstant = shouldCutSpring ? 0.0f : m_Metadata.m_SideOffsetSpringConstant;
				vLookAroundOffset.x += m_SideOffsetSpring.Update(m_DesiredSideOffset, springConstant, m_Metadata.m_SideOffsetDampingRatio, true);
			}

			Vector3 vFinalLookAroundOffset;
			if(pDrivebyLookData)
			{
				vFinalLookAroundOffset = (vLookAroundDrivebyOffset * m_DriveByBlendLevel) + vLookAroundOffset * (1.0f - m_DriveByBlendLevel);
			}
			else
			{
				vFinalLookAroundOffset = vLookAroundOffset;
			}

			float fForcedPitch = 0.0f;
			bool bUseForcedPitch = false;
			bool bUseAdditionalOffset = m_Metadata.m_FirstPersonPitchOffsetSettings.m_ShouldUseAdditionalOffsetForPitch && attachVehicle;
			if (bUseAdditionalOffset && pFollowPed && attachVehicle->InheritsFromBicycle())
			{
				const CTaskMotionOnBicycle* pMotionOnBicycleTask = static_cast<const CTaskMotionOnBicycle*>(pFollowPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_ON_BICYCLE));
				if (pMotionOnBicycleTask)
				{
					bUseForcedPitch = pMotionOnBicycleTask->ShouldApplyForcedCamPitchOffset(fForcedPitch);
				}
			}

			if (bUseAdditionalOffset)
			{
				const float fVehPitch = Abs(attachVehicle->GetTransform().GetPitch()); 
				const float fPitch = bUseForcedPitch ? Max(fForcedPitch, fVehPitch) : fVehPitch;
				const float fRatio = Clamp((fPitch - m_Metadata.m_FirstPersonPitchOffsetSettings.m_MinAbsPitchToBlendInOffset) / (m_Metadata.m_FirstPersonPitchOffsetSettings.m_MaxAbsPitchToBlendInOffset - m_Metadata.m_FirstPersonPitchOffsetSettings.m_MinAbsPitchToBlendInOffset), 0.0f, 1.0f);
				const Vector3 vDesiredFwdOffset = Lerp(fRatio, VEC3_ZERO, m_Metadata.m_FirstPersonPitchOffsetSettings.m_MaxOffset);
				Approach(m_PitchOffset.x, vDesiredFwdOffset.x, m_Metadata.m_FirstPersonPitchOffsetSettings.m_OffsetBlendRate, fwTimer::GetCamTimeStep());
				Approach(m_PitchOffset.y, vDesiredFwdOffset.y, m_Metadata.m_FirstPersonPitchOffsetSettings.m_OffsetBlendRate, fwTimer::GetCamTimeStep());
				Approach(m_PitchOffset.z, vDesiredFwdOffset.z, m_Metadata.m_FirstPersonPitchOffsetSettings.m_OffsetBlendRate, fwTimer::GetCamTimeStep());
				AttachPosition += m_PitchOffset;
			}

			AttachPosition += vFinalLookAroundOffset;
		}
	}

	//Add offsets specified by the custom seat settings
	AttachPosition += m_SeatSpecificCameraOffset;

	//Blend in an additional offset for unarmed drive-by aiming.
	Vector3 relativeAttachOffsetForUnarmedDriveby;
	relativeAttachOffsetForUnarmedDriveby.SetScaled(m_Metadata.m_RelativeAttachOffsetForUnarmedDriveby, m_UnarmedDrivebyBlendLevel);
	AttachPosition += relativeAttachOffsetForUnarmedDriveby;

	//Blend in an additional offset for ducking.
	Vector3 relativeAttachOffsetForDucking;
	relativeAttachOffsetForDucking.SetScaled(m_Metadata.m_RelativeAttachOffsetForDucking, m_DuckingBlendLevel);
	AttachPosition += relativeAttachOffsetForDucking;

	//Blend in an additional offset for lowrider alternate animations
	Vector3 relativeAttachOffsetForLowriderAltAnims;
	relativeAttachOffsetForLowriderAltAnims.SetScaled(m_Metadata.m_RelativeAttachOffsetForLowriderAltAnims, m_LowriderAltAnimsBlendLevel);
	AttachPosition += relativeAttachOffsetForLowriderAltAnims;

	//Blend in an additional offset for acceleration movement.
	const float accelerationBlendLevel = m_AccelerationMovementSpring.GetResult();
	
	Vector3 maxRelativeOffsetForAcceleration;
	maxRelativeOffsetForAcceleration.Set(0.0f, 0.0f, -m_Metadata.m_AccelerationMovementSettings.m_MaxDownwardOffset);
	
	Vector3 scaledRelativeAttachOffsetForAcceleration;
	scaledRelativeAttachOffsetForAcceleration.SetScaled(maxRelativeOffsetForAcceleration, accelerationBlendLevel);
	
	AttachPosition += scaledRelativeAttachOffsetForAcceleration;

	//Blend in an additional offset for drive by aiming.
	Vector3 relativeAttachOffsetForDriveby;
	relativeAttachOffsetForDriveby.SetScaled(m_Metadata.m_RelativeAttachOffsetForDriveby, m_DriveByBlendLevel);
	AttachPosition += relativeAttachOffsetForDriveby;

	//Blend in an additional offset when smashing window.
	const CPed* followPed = camInterface::FindFollowPed();
	Vector3 relativeAttachOffsetForSmashWindow;

	if ((m_Metadata.m_ShouldUseAdditionalRelativeAttachOffsetForDriverOnlyWhenLookingAround && followPed->GetIsDrivingVehicle()) || m_Metadata.m_ShouldUseAdditionalRelativeAttachOffsetWhenLookingAround)
	{
		relativeAttachOffsetForSmashWindow.SetScaled(m_Metadata.m_RelativeAttachOffsetForSmashingWindow + Vector3(0.0f,0.0f,m_Metadata.m_LookAroundSettings.m_MaxLookAroundHeightOffset), m_SmashWindowBlendLevel);
	}
	else
	{
		relativeAttachOffsetForSmashWindow.SetScaled(m_Metadata.m_RelativeAttachOffsetForSmashingWindow, m_SmashWindowBlendLevel);
	}

	AttachPosition += relativeAttachOffsetForSmashWindow;

	//Blend in an additional offset when rappelling down from the helicopter
	Vector3 relativeAttachOffsetForRappel;
	const float fRappelBlendLevel = m_RappellingBlendLevelSpring.GetResult();
	relativeAttachOffsetForRappel.SetScaled(m_Metadata.m_RelativeAttachOffsetForRappelling, fRappelBlendLevel);
	AttachPosition += relativeAttachOffsetForRappel;

	Vector3 relativeAttachOfsetForOpeningHeliSideDoors;
	const float fOpenHeliSideDoorsBlendLevel = m_OpenHeliSideDoorBlendLevelSpring.GetResult();
	relativeAttachOfsetForOpeningHeliSideDoors.SetScaled(m_Metadata.m_RelativeAttachOffsetForOpeningRearHeliDoors, fOpenHeliSideDoorsBlendLevel);
	AttachPosition += relativeAttachOfsetForOpeningHeliSideDoors;

	Vector3 vecPutOnHelmetCameraOffset = m_Metadata.m_PutOnHelmetCameraOffset;
	if(bUsingTripleHead && m_Metadata.m_TripleHeadPutOnHelmetCameraOffset.IsNonZero())
	{
		vecPutOnHelmetCameraOffset = m_Metadata.m_TripleHeadPutOnHelmetCameraOffset;
	}

	//B*2066868: Blend in/out an additional offset for putting on helmet if specified in the metadata (currently just the HAKUCHOU bike).
	if (vecPutOnHelmetCameraOffset.IsNonZero() && followPed && followPed->GetIsInVehicle() && (followPed->GetPedResetFlag(CPED_RESET_FLAG_PuttingOnHelmet) || m_vPutOnHelmetOffset.IsNonZero()))
	{
		CVehicle *pVehicle = static_cast<CVehicle*>(followPed->GetVehiclePedInside());
		if (pVehicle)
		{
			if (followPed->GetPedResetFlag(CPED_RESET_FLAG_PuttingOnHelmet))
			{
				m_vPutOnHelmetOffset = Lerp(s_fPutOnHelmentRate, m_vPutOnHelmetOffset, vecPutOnHelmetCameraOffset);
			}
			else
			{
				// We've finished putting on the helmet, lerp the position back to 0
				m_vPutOnHelmetOffset = Lerp(s_fPutOnHelmentRate, m_vPutOnHelmetOffset, VEC3_ZERO);
				if (m_vPutOnHelmetOffset.Mag() < 0.01f)
				{
					// Reset back to zero.
					m_vPutOnHelmetOffset = VEC3_ZERO;
				}
			}
			AttachPosition += m_vPutOnHelmetOffset;
		}
	}

	//There is a minimum height if unarmed drive-by aiming
	if (m_UnarmedDrivebyBlendLevel >= SMALL_FLOAT)
	{
		const float minHeight = Max(AttachPosition.z, m_Metadata.m_MinHeightWhenUnarmedDriveByAiming);
		AttachPosition.z = Lerp(m_UnarmedDrivebyBlendLevel, AttachPosition.z, minHeight);
	}

	//We limit the relative attach position offset when running TASK_IN_VEHICLE_SEAT_SHUFFLE
	// No longer needed as we now blend to player's head position for shuffle... need the attach position unclamped so we can blend back properly.
	const float seatShufflingBlendLevel = m_SeatSwitchingLimitSpring.GetResult();
	TUNE_GROUP_BOOL(VEHICLE_POV_CAMERA, c_ShuffleClampRelativeAttachPosition, false);
	if(c_ShuffleClampRelativeAttachPosition && seatShufflingBlendLevel >= SMALL_FLOAT && !IsVehicleTurretCamera() && m_SeatShuffleTargetSeat > 1)
	{
		const Vector3 maxRelativePositionWhenSeatShuffling = Vector3(Clamp(AttachPosition.x, -m_Metadata.m_AbsMaxRelativePositionWhenSeatShuffling.x, m_Metadata.m_AbsMaxRelativePositionWhenSeatShuffling.x),
																Clamp(AttachPosition.y, -m_Metadata.m_AbsMaxRelativePositionWhenSeatShuffling.y, m_Metadata.m_AbsMaxRelativePositionWhenSeatShuffling.y),
																Clamp(AttachPosition.z, -m_Metadata.m_AbsMaxRelativePositionWhenSeatShuffling.z, m_Metadata.m_AbsMaxRelativePositionWhenSeatShuffling.z));
		AttachPosition = Lerp(seatShufflingBlendLevel, AttachPosition, maxRelativePositionWhenSeatShuffling);
	}

	if(m_Metadata.m_ShouldAttachToFollowPed)
	{
		if(!m_IsSeatPositionOnLeftHandSide)
		{
			AttachPosition.x *= -1.0f;
		}
	}

	m_HeadBoneHeading = FLT_MAX;
	m_HeadBonePitch   = FLT_MAX;
	m_DestSeatHeading = FLT_MAX;
	m_DestSeatPitch   = FLT_MAX;

	const CVehicle* attachVehicle = (m_AttachParent && m_AttachParent->GetIsTypeVehicle()) ? static_cast<const CVehicle*>(m_AttachParent.Get()) : nullptr;

	if(seatShufflingBlendLevel >= SMALL_FLOAT)
	{
		// Blend to head bone position for shuffle. (even when relative to head, need to blend out the offset)
		////if(!m_Metadata.m_ShouldAttachToFollowPedHead)
		{
			Matrix34 boneMatrix;
			const bool isBoneValid = m_Metadata.m_ShouldUseCachedHeadBoneMatrix ? followPed->GetBoneMatrixCached(boneMatrix, BONETAG_HEAD) :
																				  followPed->GetBoneMatrix(boneMatrix, BONETAG_HEAD);
			if(isBoneValid)
			{
				Vector3 desiredAttachPosition;
				attachMatrix.UnTransform(boneMatrix.d, desiredAttachPosition);

				if(m_SeatShufflePhase > 0.50f || m_SeatShuffleChangedCameras)
				{
					if(attachVehicle && attachVehicle->GetVehicleModelInfo())
					{
						m_DesiredRelativeHeading = (!m_SeatShuffleChangedCameras && m_SeatShufflePhase > 0.50f) ? m_Metadata.m_RelativeHeadingWhenSeatShuffling * DtoR : m_DesiredRelativeHeading;
						if(attachVehicle->IsSeatIndexValid(m_SeatShuffleTargetSeat))
						{
							if(m_Metadata.m_ShouldAttachToFollowPed && !m_Metadata.m_ShouldAttachToFollowPedHead)
							{
								// Use destination seat matrix as approximation of final mover position.
								// Convert attach position to world space using that, then convert back using attach matrix
								// to get final position relative to current mover matrix.
								const CModelSeatInfo* pModelSeatInfo = attachVehicle->GetVehicleModelInfo()->GetModelSeatInfo();
								if(pModelSeatInfo)
								{
									float fBaseHeading = m_BaseHeadingRadians;
									if(!m_SeatShuffleChangedCameras)
									{
										// Negate attach position x component, assuming we are shuffling to the other side.
										// Assumes that we stay in current camera for shuffle, until the end when we attach to the new seat.
										AttachPosition.x *= -1.0f;
										fBaseHeading *= -1.0f;
									}

									Matrix34 seatMtx;
									const s32 iSeatBoneIndex = pModelSeatInfo->GetBoneIndexFromSeat( m_SeatShuffleTargetSeat );
									attachVehicle->GetGlobalMtx(iSeatBoneIndex, seatMtx);
									seatMtx.RotateLocalZ(fBaseHeading);							//Match rotation done to attach matrix.

									seatMtx.Transform(AttachPosition, cameraPosition);			//Transform to world space.
									attachMatrix.UnTransform(cameraPosition, AttachPosition);	//Make final camera position relative to current attach matrix.

									camFrame::ComputeHeadingAndPitchFromMatrix(seatMtx, m_DestSeatHeading, m_DestSeatPitch);
								}
							}
							else if(IsVehicleTurretCamera() && camGameplayDirector::IsTurretSeat(attachVehicle, m_SeatShuffleTargetSeat))
							{
								// Shuffling from turret camera, convert head bone position to be relative to attach matrix. (turret bone)
								const CVehicleModelInfo* vehicleModelInfo	= (attachVehicle) ? attachVehicle->GetVehicleModelInfo() : NULL;
								if(vehicleModelInfo && attachVehicle->GetVehicleWeaponMgr())
								{
									const CTurret* pVehicleTurret = attachVehicle->GetVehicleWeaponMgr()->GetFirstTurretForSeat(m_SeatShuffleTargetSeat);
									if(pVehicleTurret && pVehicleTurret->GetBaseBoneId() > -1)
									{
										crSkeleton* pSkeleton = attachVehicle->GetSkeleton();
										if(pSkeleton)
										{
											if(!m_SeatShuffleChangedCameras)
											{
												// Negate attach position x component, assuming we are shuffling to the other side.
												// Assumes that we stay in current camera for shuffle, until the end when we attach to the new seat.
												AttachPosition.x *= -1.0f;
											}

											Mat34V boneMat;
											int boneIndex = vehicleModelInfo->GetBoneIndex(pVehicleTurret->GetBarrelBoneId());
											pSkeleton->GetGlobalMtx(boneIndex, boneMat);
											Vector3 barrelPosition;

											Matrix34 relativeAttachPosTransform = MAT34V_TO_MATRIX34(boneMat);
											relativeAttachPosTransform.Transform(m_BoneRelativeAttachPosition, barrelPosition);

											boneIndex = vehicleModelInfo->GetBoneIndex(pVehicleTurret->GetBaseBoneId());
											pSkeleton->GetGlobalMtx(boneIndex, boneMat);

											relativeAttachPosTransform = MAT34V_TO_MATRIX34(boneMat);
											relativeAttachPosTransform.d = barrelPosition;
											relativeAttachPosTransform.Transform(AttachPosition, cameraPosition);		// Transform to world space using barrel position and base bone orientation.

											// Untransform to get a relative attach offset to the current attach matrix.
											attachMatrix.UnTransform(cameraPosition, AttachPosition);
										}
									}
								}
							}
							else if(IsVehicleTurretCamera())
							{
								AttachPosition = desiredAttachPosition;
							}
						}
					}
				}
				else if(IsVehicleTurretCamera())
				{
					AttachPosition = desiredAttachPosition;
				}

				////camFrame::ComputeHeadingAndPitchFromMatrix(boneMatrix, m_HeadBoneHeading, m_HeadBonePitch);

				// Head matrix is rotated -90 degrees (so right is up and up is left), undo that rotation so heading calc is correct.
				boneMatrix.RotateLocalY( 90.0f * DtoR );

				const float c_fTolerance = 0.125f;
				Vector3 vecFront = boneMatrix.b;
				if( vecFront.z < -1.0f + c_fTolerance )
				{
					vecFront = boneMatrix.c;
				}
				else if( vecFront.z > 1.0f - c_fTolerance )
				{
					vecFront = -boneMatrix.c;
				}

				if(attachVehicle && attachVehicle->GetIsRotaryAircraft() && attachVehicle->IsSeatIndexValid(m_SeatShuffleTargetSeat) && m_SeatShuffleTargetSeat > 1)
				{
					if( followPed->GetBoneMatrix(boneMatrix, BONETAG_ROOT) )
					{
						// For rear helicopter seats, use root matrix for heading/pitch instead.
						//vecFront.x = boneMatrix.b.x;
						//vecFront.y = boneMatrix.b.y;
						vecFront = boneMatrix.b;
					}
				}
				m_HeadBoneHeading = rage::Atan2f(-vecFront.x, vecFront.y);
				m_HeadBonePitch   = AsinfSafe(vecFront.z);

				AttachPosition.Lerp(seatShufflingBlendLevel, desiredAttachPosition);
			}
		}
	}

	if(!m_AttachedToTurret || seatShufflingBlendLevel >= SHUFFLE_FROM_TURRET_USE_HEAD_BONE_THRESHOLD)
	{
		attachMatrix.Transform(AttachPosition, cameraPosition); //Transform to world space.
	}
	else
	{
		bool bTransformed = false;
		const CVehicle* attachVehicle				= (m_AttachParent && m_AttachParent->GetIsTypeVehicle()) ? static_cast<const CVehicle*>(m_AttachParent.Get()) : NULL; 
		const CVehicleModelInfo* vehicleModelInfo	= (attachVehicle) ? attachVehicle->GetVehicleModelInfo() : NULL;
		if(vehicleModelInfo)
		{
			const CTurret* pVehicleTurret = camInterface::GetGameplayDirector().GetTurretForPed(attachVehicle, *followPed);
			if(pVehicleTurret && pVehicleTurret->GetBaseBoneId() > -1)
			{
				crSkeleton* pSkeleton = attachVehicle->GetSkeleton();
				if(pSkeleton)
				{
					Mat34V boneMat;
					int boneIndex = vehicleModelInfo->GetBoneIndex(pVehicleTurret->GetBaseBoneId());
					pSkeleton->GetGlobalMtx(boneIndex, boneMat);

					Matrix34 relativeAttachPosTransform = MAT34V_TO_MATRIX34(boneMat);
					relativeAttachPosTransform.d = attachMatrix.d;
					relativeAttachPosTransform.Transform(AttachPosition, cameraPosition);		// Transform to world space using barrel position and base bone orientation.
					bTransformed = true;
				}
			}
		}

		if(!bTransformed)
		{
			attachMatrix.Transform(AttachPosition, cameraPosition); //Transform to world space.
		}
	}

	if(m_NumUpdatesPerformed == 0)
	{	
		if(!camInterface::GetCinematicDirector().IsCameraPositionValidWithRespectToTheLine(m_LookAtTarget, cameraPosition))
		{
			m_canUpdate = false; 
			return m_canUpdate;
		}
	}

 	m_Frame.SetPosition(cameraPosition);

	if(m_Metadata.m_ShouldAttachOrientationToVehicleBone && m_AttachParent->GetIsTypeVehicle())
	{
		Quaternion orientationQuat;
		eHierarchyId  boneId = camCinematicMountedPartCamera::ComputeVehicleHieracahyId(m_AttachPart); 
		if (camCinematicVehicleTrackingCamera::ComputeWorldSpaceOrientationRelativeToVehicleBone(static_cast<const CVehicle*>(m_AttachParent.Get()), boneId, orientationQuat, 
			m_Metadata.m_VehicleBoneOrientationFlags.IsSet(VEHICLE_BONE_USE_X_AXIS_ROTATION), m_Metadata.m_VehicleBoneOrientationFlags.IsSet(VEHICLE_BONE_USE_Y_AXIS_ROTATION), m_Metadata.m_VehicleBoneOrientationFlags.IsSet(VEHICLE_BONE_USE_Z_AXIS_ROTATION)))
		{
			float fBlendOrientation = 1.0f - m_DriveByBlendLevel;

			Quaternion defaultQuat;
			attachMatrix.ToQuaternion(defaultQuat);

			defaultQuat.Slerp(fBlendOrientation, orientationQuat);

			attachMatrix.FromQuaternion(defaultQuat);
		}
	}
	else if(attachVehicle && followPed && camInterface::GetGameplayDirector().IsAttachVehicleMiniTank())
	{
		TUNE_GROUP_FLOAT(VEHICLE_POV_CAMERA, fMiniTankJumpingSpringConstantIn, 50.0f, 0.0f, 9999.0f, 0.1f);
		TUNE_GROUP_FLOAT(VEHICLE_POV_CAMERA, fMiniTankJumpingSpringConstantOut, 4.0f, 0.0f, 9999.0f, 0.1f);

		const bool isMinitankInTheAir = !IsAttachVehicleOnTheGround(*attachVehicle);

		const float springConstant = isMinitankInTheAir ? fMiniTankJumpingSpringConstantIn : fMiniTankJumpingSpringConstantOut;
		const float miniTankJumpingBlendRatio = m_MiniTankJumpingSpring.Update(isMinitankInTheAir ? 1.0f : 0.0f, springConstant);

		if (miniTankJumpingBlendRatio > SMALL_FLOAT)
		{
			//special case when we jump with the minitank, fully attach the rotation to the barrel bone
			const CVehicleWeaponMgr* vehicleWeaponManager = attachVehicle->GetVehicleWeaponMgr();
			if (vehicleWeaponManager)
			{
				const s32 attachSeatIndex = camInterface::GetGameplayDirector().ComputeSeatIndexPedIsIn(*attachVehicle, *followPed, false);
				const CTurret* vehicleTurret = vehicleWeaponManager->GetFirstTurretForSeat(attachSeatIndex);
				if (vehicleTurret)
				{
					const eHierarchyId barrelBoneId = vehicleTurret->GetBarrelBoneId();
					const crSkeleton* skeleton = attachVehicle->GetSkeleton();
					const CVehicleModelInfo* vehicleModelInfo = attachVehicle->GetVehicleModelInfo();
					if (skeleton && vehicleModelInfo && barrelBoneId != VEH_INVALID_ID)
					{
						Mat34V boneMat;
						const int boneIndex = vehicleModelInfo->GetBoneIndex(barrelBoneId);
						skeleton->GetGlobalMtx(boneIndex, boneMat);
						const Matrix34 mtxBarrelBone = MAT34V_TO_MATRIX34(boneMat);

						Quaternion quatBarrelBone;
						mtxBarrelBone.ToQuaternion(quatBarrelBone);

						Quaternion quatDefault;
						attachMatrix.ToQuaternion(quatDefault);

						quatDefault.PrepareSlerp(quatBarrelBone);
						quatDefault.Slerp(miniTankJumpingBlendRatio, quatBarrelBone);

						attachMatrix.FromQuaternion(quatDefault);
					}
				}
			}
		}
	}

	//NOTE: We apply damping to a copy of the attach matrix that is used purely for orientation calculations, so as to avoid translating the camera too close to geometry.
	Matrix34 attachMatrixToConsiderForOrientation(attachMatrix);
	ApplyDampingToAttachMatrix(attachMatrixToConsiderForOrientation);

	const float relativeHeading = UpdateOrientation(attachParentPhysical, attachMatrixToConsiderForOrientation);
	
	//NOTE: We optionally offset the camera position in the x axis of the vehicle based upon the relative heading of the camera. Because this is dependent upon
	//the camera orientation, and the orientation is dependent upon the camera position, we have a nasty circular dependency. To break this down we apply this
	//special offset after the initial position and orientation have been computed.

	if((m_Metadata.m_ShouldForceUseofSideOffset || m_Metadata.m_ShouldAttachToFollowPed) && m_AttachParent->GetIsTypeVehicle())
	{
		const CVehicle* AttachParent	= static_cast<const CVehicle*>(m_AttachParent.Get());

		float sideOffsetBlendLevel = 0.0f;
		if(AttachParent->GetVehicleType() == VEHICLE_TYPE_CAR || AttachParent->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE || AttachParent->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR)
		{
			//Only offset for front seats.
			const s32 seatIndex = followPed->GetAttachCarSeatIndex();
			if((seatIndex == Seat_driver) || (seatIndex == Seat_frontPassenger))
			{
				if((m_IsSeatPositionOnLeftHandSide && (relativeHeading < 0.0f)) ||
					(!m_IsSeatPositionOnLeftHandSide && (relativeHeading > 0.0f)))
				{
					sideOffsetBlendLevel = -relativeHeading / PI;
				}
			}
		}
		else if(AttachParent->InheritsFromBike() || AttachParent->GetIsJetSki())
		{
			//Only offset for the driver when there's a passenger behind them.
			const bool isFollowPedDriver = followPed && followPed->GetIsDrivingVehicle();
			if(isFollowPedDriver)
			{
				const s32 numPassengers = AttachParent->GetNumberOfPassenger();
				if(numPassengers > 0 || m_Metadata.m_ShouldForceUseofSideOffset)
				{
					sideOffsetBlendLevel = -relativeHeading / PI;
					sideOffsetBlendLevel *= (1.0f - m_DriveByBlendLevel);
				}
			}
		}

		sideOffsetBlendLevel	= Clamp(sideOffsetBlendLevel, -1.0f, 1.0f); 
		AttachPosition.x		+= (sideOffsetBlendLevel * m_Metadata.m_InVehicleLookBehindSideOffset); 

		attachMatrix.Transform(AttachPosition, cameraPosition); //Transform to world space.

		TUNE_GROUP_BOOL(FIRST_PERSON_BIKE_TUNE, ENABLE_ADDITIONAL_ATTACH_COLLISION_TEST, true);
		if (ENABLE_ADDITIONAL_ATTACH_COLLISION_TEST && followPed && followPed->GetCapsuleInfo() && AttachParent->GetIsSmallDoubleSidedAccessVehicle() &&
			(m_Metadata.m_ShouldUseAdditionalRelativeAttachOffsetWhenLookingAround || (m_Metadata.m_ShouldUseAdditionalRelativeAttachOffsetForDriverOnlyWhenLookingAround && followPed->GetIsDrivingVehicle())))
		{
			u32 includeFlags = ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_VEHICLE_TYPE;

			// Constrain the camera against the world
			WorldProbe::CShapeTestCapsuleDesc constrainCapsuleTest;

			constrainCapsuleTest.SetIncludeFlags(includeFlags);
			constrainCapsuleTest.SetIsDirected(true);
			constrainCapsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST); 
			constrainCapsuleTest.SetContext(WorldProbe::LOS_Camera);
			constrainCapsuleTest.SetOptions(WorldProbe::LOS_IGNORE_NO_CAM_COLLISION);
			constrainCapsuleTest.SetExcludeEntity(AttachParent);

			Matrix34 initialAttachMatrix;
			ComputeAttachMatrix(attachParentPhysical, initialAttachMatrix); 
			Vector3 vFallbackPosition = initialAttachMatrix.d + Vector3(0.0f,0.0f,m_Metadata.m_FallBackHeightDeltaFromAttachPositionForCollision);	// TODO:: Another capsule test to determine if valid
			Vector3 startPosition = initialAttachMatrix.d;
			Vector3 endPosition   = cameraPosition;

			float radius			= followPed->GetCapsuleInfo()->GetBipedCapsuleInfo() ? followPed->GetCapsuleInfo()->GetBipedCapsuleInfo()->GetRadius() : 0.25f;
			radius					+= m_Metadata.m_CollisionTestRadiusOffset;
			constrainCapsuleTest.SetCapsule(startPosition, endPosition, radius);

			if (WorldProbe::GetShapeTestManager()->SubmitTest(constrainCapsuleTest))
			{
				cameraPosition = vFallbackPosition;
				//! Avoid a const_cast by setting on local player.
				if(followPed == CGameWorld::FindLocalPlayer())
					CGameWorld::FindLocalPlayer()->SetPedResetFlag(CPED_RESET_FLAG_PoVCameraConstrained, true);
			}
		}

		m_Frame.SetPosition(cameraPosition);
	}

	UpdatePedVisibility(relativeHeading);

	if (ComputeShouldTerminateForWaterClipping(cameraPosition))
	{
        if(m_Metadata.m_RestoreHeadingAndPitchAfterClippingWithWater)
        {
            camInterface::GetCinematicDirector().CacheHeadingAndPitchForVehiclePovCameraDuringWaterClip(m_RelativeHeading, m_RelativePitch);
        }

		m_canUpdate = false;
		return m_canUpdate;
	}

	if(m_Metadata.m_ShouldTestForClipping && ComputeShouldTerminateForClipping(cameraPosition, attachMatrix.d))
	{
		m_canUpdate = false; 
		return m_canUpdate;
	}

	//apply damped look at 
	if(m_CanUseDampedLookAtHelpers)
	{
		if(m_LookAtTarget)
		{
			if(m_LookAtTarget->GetIsTypeVehicle())
			{
				if(m_InVehicleLookAtDamper)
				{
					Matrix34 WorldMat(m_Frame.GetWorldMatrix()); 

					m_InVehicleLookAtDamper->Update(WorldMat, m_Frame.GetFov(), m_NumUpdatesPerformed == 0); 

					m_Frame.SetWorldMatrix(WorldMat);
				}
			}
			else
			{
				if(m_OnFootLookAtDamper)
				{
					Matrix34 WorldMat(m_Frame.GetWorldMatrix()); 

					m_OnFootLookAtDamper->Update(WorldMat, m_Frame.GetFov(), m_NumUpdatesPerformed == 0); 

					m_Frame.SetWorldMatrix(WorldMat);
				}
			}
		}
	}

	UpdateFov();

	UpdateNearClip(bUsingTripleHead, aspectRatio);

	UpdateFirstPersonAnimatedData();

	UpdateShakeAmplitudeScaling();

	UpdateIdleShake();

	if(m_AttachParent->GetIsPhysical())
	{
		const CPhysical* attachParentPhysical = static_cast<const CPhysical*>(m_AttachParent.Get());
		UpdateSpeedRelativeShake(*attachParentPhysical, m_Metadata.m_HighSpeedShakeSettings, m_HighSpeedShakeSpring);
		UpdateSpeedRelativeShake(*attachParentPhysical, m_Metadata.m_HighSpeedVibrationShakeSettings, m_HighSpeedVibrationShakeSpring);
	}

	UpdateRocketSettings();

	if(m_canUpdate && m_Metadata.m_ShouldByPassNearClip)
	{
		m_Frame.GetFlags().SetFlag(camFrame::Flag_BypassNearClipScanner);
	}

	m_BaseFront = m_Frame.GetFront();

#if FPS_MODE_SUPPORTED
	if(m_canUpdate && m_AttachedToTurret)
	{
		camBaseCamera* renderedGameplayCamera = camInterface::GetGameplayDirector().GetRenderedCamera();
		if( renderedGameplayCamera &&
			renderedGameplayCamera->GetIsClassId(camThirdPersonPedAimCamera::GetStaticClassId()) )
		{
			camThirdPersonPedAimCamera* pTurretCamera = static_cast<camThirdPersonPedAimCamera*>(renderedGameplayCamera);
			if(pTurretCamera->GetIsTurretCam())
			{
				pTurretCamera->CacheCinematicCameraOrientation(m_PostEffectFrame);
			}
		}
	}
#endif

	return m_canUpdate;
}

void camCinematicMountedCamera::PostUpdate()
{
	// Camera shake can potentially break camera's pitch limits, which is bad for pov camera.
	// Not done if running an animated camera or blending out from animated camera.
	const int numFrameShakers = m_FrameShakers.GetCount();
	if( IsFirstPersonCamera() && numFrameShakers > 0 && m_FinalPitchLimits.IsNotEqual(Vector2(PI, PI)) &&
		!m_UsingFirstPersonAnimatedData && !m_WasUsingFirstPersonAnimatedData )
	{
		// Validate pitch from post effect frame and clamp if necessary.
		float heading, pitch, roll;
		Matrix34 localCameraMatrix = m_PostEffectFrame.GetWorldMatrix();
		localCameraMatrix.DotTranspose(m_AttachMatrix);			// transform to local space.

		// Calculate heading, pitch and roll relative to attach matrix.
		camFrame::ComputeHeadingPitchAndRollFromMatrix(localCameraMatrix, heading, pitch, roll);

		// Clamp to relative pitch limits.
		const float c_ShakeLimitMargin = 1.5f*DtoR;
		float clampedPitch = Clamp(pitch, m_FinalPitchLimits.x - c_ShakeLimitMargin, m_FinalPitchLimits.y + c_ShakeLimitMargin);
		if(clampedPitch != pitch)
		{
			Matrix34 matClamped;
			camFrame::ComputeWorldMatrixFromHeadingPitchAndRoll(heading, clampedPitch, roll, matClamped);

			matClamped.Dot(m_AttachMatrix);						// transform back to world space.

			m_PostEffectFrame.SetWorldMatrix(matClamped, true);
		}
	}
}

void camCinematicMountedCamera::UpdatePedVisibility(float relativeHeading)
{
	m_PedToMakeInvisible = m_PedToMakeHeadInvisible = NULL;

	const CPed* pedToConsider = NULL;
	if(m_Metadata.m_ShouldAttachToFollowPedHead || m_Metadata.m_ShouldAttachToFollowPed || m_Metadata.m_ShouldAttachToVehicleTurret)
	{
		if(m_LookAtTarget && m_LookAtTarget->GetIsTypePed())
		{
			pedToConsider = static_cast<const CPed*>(m_LookAtTarget.Get()); 
		}
		else
		{
			pedToConsider = camInterface::FindFollowPed();
		}
	}
	else if(m_AttachParent->GetIsTypePed())
	{
		pedToConsider = static_cast <const CPed*>(m_AttachParent.Get());
	}
	else if(!IsFirstPersonCamera() && m_SeatSwitchingLimitSpring.GetResult() >= 1.0f - SMALL_FLOAT)
	{
		m_PedToMakeInvisible = camInterface::FindFollowPed();
		return;
	}

	if(!pedToConsider)
	{
		return;
	}

	const bool canPedBeVisibleInVehicle = ComputeCanPedBeVisibleInVehicle(*pedToConsider, relativeHeading);

	INSTANTIATE_TUNE_GROUP_FLOAT(VEHICLE_POV_CAMERA, SHUFFLE_FROM_TURRET_VISIBLE_THRESHOLD, 0.0f, 1.0f, 0.001f);
	const bool isShufflingFromTurretSeat = IsVehicleTurretCamera() && m_SeatSwitchingLimitSpring.GetResult() > SHUFFLE_FROM_TURRET_VISIBLE_THRESHOLD;

	//NOTE: We always hide the ped when when the mobile phone camera is rendering,
	//as we do not want to see the ped's body or mobile phone prop.
	const bool isRenderingMobilePhoneCamera = CPhoneMgr::CamGetState();
	if(isRenderingMobilePhoneCamera || isShufflingFromTurretSeat)
	{
		m_PedToMakeInvisible = pedToConsider;
	}
	else if(m_Metadata.m_ShouldMakePedInAttachSeatInvisible)
	{
		//if we're in a sub, always allow the ped to be invisible
		const CVehicle* followVehicle	= camInterface::GetGameplayDirector().GetFollowVehicle();
		const bool isFollowVehicleASub	= followVehicle && followVehicle->InheritsFromSubmarine();

		TUNE_GROUP_BOOL(VEHICLE_POV_CAMERA, MAKE_PED_VISIBLE_IN_VEHICLES, false);
		if(!MAKE_PED_VISIBLE_IN_VEHICLES && (!canPedBeVisibleInVehicle || isFollowVehicleASub))
		{
			m_PedToMakeInvisible = pedToConsider;
		}
	}

	TUNE_GROUP_BOOL(VEHICLE_POV_CAMERA, MAKE_PED_HEADS_INVISIBLE_IN_VEHICLES, false);
	if( MAKE_PED_HEADS_INVISIBLE_IN_VEHICLES ||
		canPedBeVisibleInVehicle ||
		m_Metadata.m_ShouldMakeFollowPedHeadInvisible ||
		(m_AttachedToTurret && m_Metadata.m_ShouldAttachToVehicleTurret) )
	{
		m_PedToMakeHeadInvisible = pedToConsider;
	}
}

bool camCinematicMountedCamera::ComputeCanPedBeVisibleInVehicle(const CPed& ped, float relativeHeading) const
{
	const CTaskMotionBase* currentMotionTask = ped.GetCurrentMotionTask();
	if(!currentMotionTask || (currentMotionTask->GetTaskType() != CTaskTypes::TASK_MOTION_IN_AUTOMOBILE))
	{
		return false;
	}

	const CTaskMotionInAutomobile* autoMobileTask	= static_cast<const CTaskMotionInAutomobile*>(currentMotionTask);

	bool canPedBeVisibleInVehicle					= autoMobileTask->CanPedBeVisibleInPoVCamera(false);
	if(canPedBeVisibleInVehicle && m_DriveByBlendLevel <= 0.0f)
	{
		//NOTE: We ensure that the ped is invisible at extreme relative headings, to avoid clipping into the shoulder and collar.
		const float relativeHeadingDeg	= relativeHeading * RtoD;
		canPedBeVisibleInVehicle		= (relativeHeadingDeg >= m_Metadata.m_AttachParentRelativeHeadingForVisiblePed.x) &&
											(relativeHeadingDeg <= m_Metadata.m_AttachParentRelativeHeadingForVisiblePed.y);
	}

	return canPedBeVisibleInVehicle;
}

bool camCinematicMountedCamera::IsFollowPedSittingInTheLeftOfTheVehicle(const CVehicle* attachVehicle, const CPed* followPed) const
{
	if(attachVehicle && followPed)
	{
		const CVehicleModelInfo* vehicleModelInfo	= attachVehicle->GetVehicleModelInfo();
		const CModelSeatInfo* seatInfo				= vehicleModelInfo ? vehicleModelInfo->GetModelSeatInfo() : NULL;

		if(!seatInfo)
		{
			return false;
		}

		s32 attachSeatIndex = seatInfo->GetDriverSeat(); 

		if(followPed->GetMyVehicle() == attachVehicle)
		{
			attachSeatIndex = followPed->GetAttachCarSeatIndex();

			if(attachSeatIndex >= 0)
			{
				s32 iBoneIndex = seatInfo->GetBoneIndexFromSeat(attachSeatIndex);

				if(iBoneIndex > -1) 
				{ 
					const crBoneData* pBoneData = attachVehicle->GetSkeletonData().GetBoneData(iBoneIndex);  

					if(pBoneData)
					{
						return pBoneData->GetDefaultTranslation().GetXf() < 0.0f;
					}
				}
			}
		}
	}
	return false; 
}

bool camCinematicMountedCamera::IsFollowPedSittingInVehicleFacingForward(const CVehicle* attachVehicle, const CPed* followPed) const
{
	if(attachVehicle && followPed)
	{
		const CVehicleModelInfo* vehicleModelInfo	= attachVehicle->GetVehicleModelInfo();
		const CModelSeatInfo* seatInfo				= vehicleModelInfo ? vehicleModelInfo->GetModelSeatInfo() : NULL;

		if(!seatInfo)
		{
			return false;
		}

		s32 attachSeatIndex = seatInfo->GetDriverSeat(); 

		if(followPed->GetMyVehicle() == attachVehicle)
		{
			attachSeatIndex = followPed->GetAttachCarSeatIndex();

			if(attachSeatIndex >= 0)
			{
				s32 iBoneIndex = seatInfo->GetBoneIndexFromSeat(attachSeatIndex);

				if(iBoneIndex > -1) 
				{ 
					const crBoneData* pBoneData = attachVehicle->GetSkeletonData().GetBoneData(iBoneIndex);  

					if(pBoneData)
					{
						const float seatAngle = pBoneData->GetDefaultRotation().GetZf();
						const float maxAngleToConsiderFacingForward = m_Metadata.m_LeadingLookSettings.m_MaxAngleToConsiderFacingForward * DtoR;
						return Abs(seatAngle) < maxAngleToConsiderFacingForward;
					}
				}
			}
		}
	}
	return false; 
}

bool camCinematicMountedCamera::ShouldAllowRearSeatLookback()
{
	if (m_AttachParent && m_AttachParent->GetIsTypeVehicle())
	{
		const CPed* pFollowPed								= camInterface::FindFollowPed();
		const CPedIntelligence* pedIntelligence				= pFollowPed ? pFollowPed->GetPedIntelligence() : NULL;
		const CTaskAimGunVehicleDriveBy* pVehicleGunTask	= pedIntelligence ? static_cast<CTaskAimGunVehicleDriveBy*>(pedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY)) : NULL;
		const CTaskMountThrowProjectile* pProjectileTask	= pedIntelligence ? static_cast<CTaskMountThrowProjectile*>(pedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_MOUNT_THROW_PROJECTILE)) : NULL;

		if (pVehicleGunTask || pProjectileTask)
		{
			const CVehicle* attachVehicle								= static_cast<const CVehicle*>(m_AttachParent.Get());
			const CFirstPersonDriveByLookAroundData *pDrivebyLookData	= GetFirstPersonDrivebyData(attachVehicle);

			return pDrivebyLookData ? pDrivebyLookData->GetAllowLookback() : true;
		}
	}

	return true;
}

void camCinematicMountedCamera::ComputeAttachMatrix(const CPhysical& attachPhysical, Matrix34& attachMatrix)
{
	//Attach the camera the vehicle offset position that corresponds to the requested shot.

	attachMatrix = MAT34V_TO_MATRIX34(m_AttachParent->GetMatrix());

	const CPed* followPed = camInterface::FindFollowPed();
	
	if(m_LookAtTarget && m_LookAtTarget->GetIsTypePed())
	{
		followPed = static_cast<const CPed*>(m_LookAtTarget.Get()); 
	}

	if(!followPed)
	{
		return;
	}

	if(m_Metadata.m_ShouldAttachToFollowPedHead)
	{
		Matrix34 boneMatrix;
		const bool isBoneValid = m_Metadata.m_ShouldUseCachedHeadBoneMatrix ? followPed->GetBoneMatrixCached(boneMatrix, BONETAG_HEAD) :
									followPed->GetBoneMatrix(boneMatrix, BONETAG_HEAD);
		
		if(isBoneValid)
		{
			Vector3 desiredAttachPosition;
			boneMatrix.Transform(m_BoneRelativeAttachPosition, desiredAttachPosition);

			//Damp the desired position relative to the attach parent matrix.

			Vector3 desiredRelativeAttachPosition;
			attachMatrix.UnTransform(desiredAttachPosition, desiredRelativeAttachPosition);

			desiredRelativeAttachPosition.Min(desiredRelativeAttachPosition, m_Metadata.m_MaxRelativeAttachOffsets);

			const Vector3& attachParentVelocity		= attachPhysical.GetVelocity();
			const float attachParentSpeed			= attachParentVelocity.Mag();
			const bool shouldApplyMaxSpringConstant	= (attachParentSpeed >= m_Metadata.m_MinSpeedForMaxRelativeAttachSpringConstant);

			float springConstantBlendLevel;
			if(m_RelativeAttachSpringConstantEnvelope)
			{
				if(m_NumUpdatesPerformed == 0)
				{
					if(shouldApplyMaxSpringConstant)
					{
						m_RelativeAttachSpringConstantEnvelope->Start(1.0f);
					}
					else
					{
						m_RelativeAttachSpringConstantEnvelope->Stop();
					}
				}
				else
				{
					m_RelativeAttachSpringConstantEnvelope->AutoStartStop(shouldApplyMaxSpringConstant);
				}

				springConstantBlendLevel = m_RelativeAttachSpringConstantEnvelope->Update();
			}
			else
			{
				//We don't have an envelope, so just use the input state directly.
				springConstantBlendLevel = shouldApplyMaxSpringConstant ? 1.0f : 0.0f;
			}

			const bool isFollowPedDriver		= followPed && followPed->GetIsDrivingVehicle() && m_DriveByBlendLevel <= 0.0f;
			const Vector2& springConstantLimits	= isFollowPedDriver ? m_Metadata.m_RelativeAttachSpringConstantLimits :
													m_Metadata.m_RelativeAttachSpringConstantLimitsForPassengers;

			const bool isRenderedCamera = camInterface::IsRenderingCamera(*this);
			const bool shouldCutSpring	= ((m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
											m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation) || (!isRenderedCamera));
			const float springConstant	= shouldCutSpring ? 0.0f : Lerp(springConstantBlendLevel, springConstantLimits.x,
											springConstantLimits.y);

			Vector3 dampedRelativeAttachPosition;
			dampedRelativeAttachPosition.x	= m_RelativeAttachPositionSprings[0].Update(desiredRelativeAttachPosition.x, springConstant,
												m_Metadata.m_RelativeAttachSpringDampingRatio);
			dampedRelativeAttachPosition.y	= m_RelativeAttachPositionSprings[1].Update(desiredRelativeAttachPosition.y, springConstant,
												m_Metadata.m_RelativeAttachSpringDampingRatio);
			dampedRelativeAttachPosition.z	= m_RelativeAttachPositionSprings[2].Update(desiredRelativeAttachPosition.z, springConstant,
												m_Metadata.m_RelativeAttachSpringDampingRatio);

			attachMatrix.Transform(dampedRelativeAttachPosition, attachMatrix.d);

			return;
		}
	}
	
	if(m_Metadata.m_ShouldAttachToFollowPed)
	{
		attachMatrix = MAT34V_TO_MATRIX34(followPed->GetMatrix());

		const CVehicle* attachVehicle = m_AttachParent->GetIsTypeVehicle() ? static_cast<const CVehicle*>(m_AttachParent.Get()) : NULL;
		
		m_IsSeatPositionOnLeftHandSide = IsFollowPedSittingInTheLeftOfTheVehicle(attachVehicle, followPed); 

		return; 
	}

	const CVehicle* attachVehicle = m_AttachParent->GetIsTypeVehicle() ? static_cast<const CVehicle*>(m_AttachParent.Get()) : NULL;
	if(!attachVehicle)
	{
		return;
	}

	const CVehicleModelInfo* vehicleModelInfo = attachVehicle->GetVehicleModelInfo();

	const bool isPedInTurretSeat = camInterface::GetGameplayDirector().IsPedInOrEnteringTurretSeat(attachVehicle, *followPed, false);
	s32 attachSeatIndex = camInterface::GetGameplayDirector().ComputeSeatIndexPedIsIn(*attachVehicle, *followPed, false);
	const bool isPedInMiniTank = camInterface::GetGameplayDirector().IsAttachVehicleMiniTank();

	if(m_Metadata.m_ShouldAttachToFollowPedSeat)
	{
		const CModelSeatInfo* seatInfo = vehicleModelInfo ? vehicleModelInfo->GetModelSeatInfo() : NULL;

		if(!seatInfo)
		{
			return;
		}

		attachSeatIndex = seatInfo->GetDriverSeat(); 

		if(followPed->GetMyVehicle() == attachVehicle)
		{
			attachSeatIndex = followPed->GetAttachCarSeatIndex();
			if(!IsFirstPersonCamera() && m_SeatShuffleTargetSeat >= 0)
			{
				// If seat shuffling for bonnet camera, switch to target seat.
				attachSeatIndex = m_SeatShuffleTargetSeat;
			}

			if(attachSeatIndex >= 0)
			{
				m_IsSeatPositionOnLeftHandSide = IsFollowPedSittingInTheLeftOfTheVehicle(attachVehicle, followPed); 

				if(m_Metadata.m_ShouldRestictToFrontSeat)
				{	
					if(attachSeatIndex > 0)
					{
						//NOTE: We do not attempt to attach to the front passenger seats of buses, prison buses or coaches (rental/tour buses are handled normally),
						//as this seat is set back, which causes undesirable camera positioning and clipping in some cases.

						static const u32 LAYOUT_BUS_HASH		= ATSTRINGHASH("LAYOUT_BUS", 0xa1cdf1da);
						static const u32 LAYOUT_PRISON_BUS_HASH	= ATSTRINGHASH("LAYOUT_PRISON_BUS", 0x1b9367f);
						static const u32 LAYOUT_COACH_HASH		= ATSTRINGHASH("LAYOUT_COACH", 0xb050699b);
                        static const u32 LAYOUT_PBUS2_HASH      = ATSTRINGHASH("LAYOUT_PBUS2", 0xA3252E33);

						const CVehicleLayoutInfo* layoutInfo	= attachVehicle->GetLayoutInfo();
						const u32 vehicleLayoutNameHash			= layoutInfo ? layoutInfo->GetName().GetHash() : 0;
						const bool isAttachedToBusOrCoach		= ((vehicleLayoutNameHash == LAYOUT_BUS_HASH) || (vehicleLayoutNameHash == LAYOUT_PRISON_BUS_HASH) ||
																	(vehicleLayoutNameHash == LAYOUT_COACH_HASH) || (vehicleLayoutNameHash == LAYOUT_PBUS2_HASH));

						if(!isAttachedToBusOrCoach && !m_IsSeatPositionOnLeftHandSide && seatInfo->GetLayoutInfo() && seatInfo->GetLayoutInfo()->GetSeatInfo(Seat_frontPassenger))
						{
							attachSeatIndex = Seat_frontPassenger; 
						}
						else
						{
							attachSeatIndex = seatInfo->GetDriverSeat(); 
						}
					}
				}

				const s32 attachSeatBoneIndex = seatInfo->GetBoneIndexFromSeat(attachSeatIndex);
				if(cameraVerifyf(attachSeatBoneIndex != -1, "A cinematic mounted camera could not obtain a valid bone index for a seat (index %d)",
					attachSeatIndex))
				{
					Matrix34 boneMatrix;
					attachVehicle->GetSkeleton()->GetGlobalMtx(attachSeatBoneIndex, RC_MAT34V(boneMatrix));

					boneMatrix.Transform(m_BoneRelativeAttachPosition, attachMatrix.d);
				}
			}
		}
	}
	else if(m_Metadata.m_ShouldAttachToVehicleExitEntryPoint)
	{
		const fwTransform& attachVehicleTransform = attachVehicle->GetTransform();

		Quaternion entryExitOrientation;
		bool hasClimbDown;
		const bool hasValidEntryExitPoint	= camInterface::ComputeVehicleEntryExitPointPositionAndOrientation(*followPed, *attachVehicle, attachMatrix.d,
												entryExitOrientation, hasClimbDown);
		if(hasValidEntryExitPoint)
		{
			m_LocalExitEnterOffset = VEC3V_TO_VECTOR3(attachVehicleTransform.UnTransform(VECTOR3_TO_VEC3V(attachMatrix.d)));
		}
		else
		{
			attachMatrix.d = VEC3V_TO_VECTOR3(attachVehicleTransform.Transform(VECTOR3_TO_VEC3V(m_LocalExitEnterOffset)));
		}

	}
	else if(m_Metadata.m_ShouldAttachToVehicleBone)
	{
		eHierarchyId  boneId = camCinematicMountedPartCamera::ComputeVehicleHieracahyId(m_AttachPart); 
		
		Vector3 VehiclePos(VEC3_ZERO); 
		
		camCinematicVehicleTrackingCamera::ComputeWorldSpacePositionRelativeToVehicleBoneWithOffset(attachVehicle, boneId, m_BoneRelativeAttachPosition, VehiclePos);
		
		attachMatrix.d = VehiclePos;
	}
	else if(m_Metadata.m_ShouldAttachToVehicleTurret && (isPedInTurretSeat || isPedInMiniTank))
	{
		Vector3 turretPos(VEC3_ZERO);
		m_AttachedToTurret = true;

		const CTurret* pVehicleTurret = camInterface::GetGameplayDirector().GetTurretForPed(attachVehicle, *followPed);

		if (pVehicleTurret == nullptr && isPedInMiniTank)
		{
			const CVehicleWeaponMgr* vehicleWeaponManager = attachVehicle->GetVehicleWeaponMgr();
			if (vehicleWeaponManager)
			{
				pVehicleTurret = vehicleWeaponManager->GetFirstTurretForSeat(attachSeatIndex);
			}
		}

		if (vehicleModelInfo && pVehicleTurret && pVehicleTurret->GetBarrelBoneId() > -1)
		{
			crSkeleton* pSkeleton = attachVehicle->GetSkeleton();
			if (pSkeleton)
			{
				Mat34V boneMat;
				int boneIndex = vehicleModelInfo->GetBoneIndex(pVehicleTurret->GetBarrelBoneId());
				pSkeleton->GetGlobalMtx(boneIndex, boneMat);

				const Matrix34 mtxBarrelBone = MAT34V_TO_MATRIX34(boneMat);

				mtxBarrelBone.Transform(m_BoneRelativeAttachPosition, turretPos);

				if (followPed && followPed->GetMyVehicle() == attachVehicle)
				{
					m_IsSeatPositionOnLeftHandSide = IsFollowPedSittingInTheLeftOfTheVehicle(attachVehicle, followPed);
				}
			}
		}
		
		attachMatrix.d = turretPos;
	}
	else
	{
		//Attach relative to the turret bone for tanks.
		// - We would ideally data-drive this, but we want to avoid changing the camera metadata schema in DLC.
		const bool isAttachedToTank = attachVehicle->IsTank();
		if(isAttachedToTank)
		{
			const s32 turretBoneIndex = vehicleModelInfo->GetBoneIndex(VEH_TURRET_1_BASE);
			if(turretBoneIndex != -1)
			{
				Matrix34 boneMatrix;
				attachVehicle->GetSkeleton()->GetGlobalMtx(turretBoneIndex, RC_MAT34V(boneMatrix));

				boneMatrix.Transform(m_BoneRelativeAttachPosition, attachMatrix.d);
			}
		}
	}
}

void camCinematicMountedCamera::ApplyDampingToAttachMatrix(Matrix34& attachMatrix)
{
	if( m_Metadata.m_AttachMatrixPitchSpringConstant < SMALL_FLOAT && 
	    m_Metadata.m_AttachMatrixRollSpringConstant < SMALL_FLOAT &&
	    m_Metadata.m_FirstPersonRollSettings.m_OrientateUpLimit < SMALL_FLOAT &&
	    m_Metadata.m_AttachMatrixHeadingSpringConstant < SMALL_FLOAT)
	{
		return;
	}

	float attachHeading;
	float attachPitch;
	float attachRoll;
	camFrame::ComputeHeadingPitchAndRollFromMatrixUsingEulers(attachMatrix, attachHeading, attachPitch, attachRoll);

	TUNE_GROUP_BOOL(VEHICLE_POV_CAMERA, USE_FAKE_ROLL, false);
	TUNE_GROUP_FLOAT(VEHICLE_POV_CAMERA, FAKE_ROLL, 0.0f, -3.14f, 3.14f, 0.1f);
	if(USE_FAKE_ROLL)
	{
		attachRoll = FAKE_ROLL;
	}

	if( m_Metadata.m_FirstPersonRollSettings.m_OrientateUpLimit > SMALL_FLOAT)
	{
		const CVehicle* attachVehicle = (m_AttachParent && m_AttachParent->GetIsTypeVehicle()) ? static_cast<const CVehicle*>(m_AttachParent.Get()) : NULL;
		if(attachVehicle && attachVehicle->InheritsFromBike())
		{
			//use different blend amount depending on driveby blend
			float fBlendLevel = Lerp(m_DriveByBlendLevel, m_Metadata.m_FirstPersonRollSettings.m_RollDampingRatio, m_Metadata.m_FirstPersonRollSettings.m_RollDampingRatioDriveBy);

			if( m_Metadata.m_FirstPersonRollSettings.m_OrientateUpLimit > fabsf(attachRoll) )
			{
				//blend towards world up when within limits
				attachRoll = fwAngle::LerpTowards(attachRoll, 0.0f, fBlendLevel);
			}
			else if( m_Metadata.m_FirstPersonRollSettings.m_OrientateWithVehicleLimit > fabsf(attachRoll) )
			{
				//vehicle is orientated outside angle in which we blend to world up, so lerp back to vehicle roll angle
				float fMaxLeanAngle = (1.0f - fBlendLevel) * m_Metadata.m_FirstPersonRollSettings.m_OrientateUpLimit;

				float sign = Sign(attachRoll);
				attachRoll = RampValueSafe(fabsf(attachRoll), m_Metadata.m_FirstPersonRollSettings.m_OrientateUpLimit, m_Metadata.m_FirstPersonRollSettings.m_OrientateWithVehicleLimit,
											fMaxLeanAngle,	m_Metadata.m_FirstPersonRollSettings.m_OrientateWithVehicleLimit);
				attachRoll *= sign;
			}
		}
	}
		
	//Blend in a hard limit on the damping as the desired pitch approaches world up/down. This allows us to work-around an inherent discontinuity in the roll.

	float dampingLimitBlendLevel = 0.0f;
	if(attachPitch < (m_Metadata.m_AttachMatrixDampingPitchSoftLimits.x * DtoR))
	{
		dampingLimitBlendLevel	= RampValueSafe(attachPitch, (m_Metadata.m_AttachMatrixDampingPitchHardLimits.x * DtoR),
									(m_Metadata.m_AttachMatrixDampingPitchSoftLimits.x * DtoR), 1.0f, 0.0f);
	}
	else if(attachPitch > (m_Metadata.m_AttachMatrixDampingPitchSoftLimits.y * DtoR))
	{
		dampingLimitBlendLevel	= RampValueSafe(attachPitch, (m_Metadata.m_AttachMatrixDampingPitchSoftLimits.y * DtoR),
									(m_Metadata.m_AttachMatrixDampingPitchHardLimits.y * DtoR), 0.0f, 1.0f);
	}

	dampingLimitBlendLevel = SlowInOut(dampingLimitBlendLevel);

	float dampedPitch;
	float dampedRoll;
	float dampedHeading;
	if(dampingLimitBlendLevel > (1.0f - SMALL_FLOAT))
	{
		dampedPitch = attachPitch;
		m_AttachMatrixPitchSpring.Reset(attachPitch);

		dampedRoll = attachRoll;
		m_AttachMatrixRollSpring.Reset(attachRoll);

		dampedHeading = attachHeading;
		m_AttachMatrixHeadingSpring.Reset(attachHeading);
	}
	else
	{
		const bool isRenderedCamera = camInterface::IsRenderingCamera(*this);
		const bool shouldCutSpring	= ((m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
										m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation) || (!isRenderedCamera));

		const float pitchSpringConstant	= shouldCutSpring ? 0.0f : m_Metadata.m_AttachMatrixPitchSpringConstant;
		dampedPitch						= m_AttachMatrixPitchSpring.Update(attachPitch, pitchSpringConstant, m_Metadata.m_AttachMatrixPitchSpringDampingRatio);
		dampedPitch						= fwAngle::LerpTowards(dampedPitch, attachPitch, dampingLimitBlendLevel);
		m_AttachMatrixPitchSpring.OverrideResult(dampedPitch);

		//Ensure that we blend to the desired orientation over the shortest angle.
		float unwrappedAttachRoll			= attachRoll;
		const float rollOnPreviousUpdate	= m_AttachMatrixRollSpring.GetResult();
		const float desiredRollDelta		= attachRoll - rollOnPreviousUpdate;
		if(desiredRollDelta > PI)
		{
			unwrappedAttachRoll -= TWO_PI;
		}
		else if(desiredRollDelta < -PI)
		{
			unwrappedAttachRoll += TWO_PI;
		}

		const float rollSpringConstant	= shouldCutSpring ? 0.0f : m_Metadata.m_AttachMatrixRollSpringConstant;
		dampedRoll						= m_AttachMatrixRollSpring.Update(unwrappedAttachRoll, rollSpringConstant, m_Metadata.m_AttachMatrixRollSpringDampingRatio);
		dampedRoll						= fwAngle::LimitRadianAngle(dampedRoll);
		dampedRoll						= fwAngle::LerpTowards(dampedRoll, attachRoll, dampingLimitBlendLevel);
		m_AttachMatrixRollSpring.OverrideResult(dampedRoll);

		//Ensure that we blend to the desired orientation over the shortest angle.
		float unwrappedAttachHeading		= attachHeading;
		const float headingOnPreviousUpdate	= m_AttachMatrixHeadingSpring.GetResult();
		const float desiredHeadingDelta		= attachHeading - headingOnPreviousUpdate;
		if(desiredHeadingDelta > PI)
		{
			unwrappedAttachHeading -= TWO_PI;
		}
		else if(desiredHeadingDelta < -PI)
		{
			unwrappedAttachHeading += TWO_PI;
		}

		if (!m_Metadata.m_UseAttachMatrixHeadingSpring)
		{
			dampedHeading = attachHeading;
		}
		else
		{
			const float headingSpringConstant	= shouldCutSpring ? 0.0f : m_Metadata.m_AttachMatrixHeadingSpringConstant;
			dampedHeading				= m_AttachMatrixHeadingSpring.Update(unwrappedAttachHeading, headingSpringConstant, m_Metadata.m_AttachMatrixHeadingSpringDampingRatio);
			dampedHeading				= fwAngle::LimitRadianAngle(dampedHeading);
			dampedHeading				= fwAngle::LerpTowards(dampedHeading, attachHeading, dampingLimitBlendLevel);
			m_AttachMatrixHeadingSpring.OverrideResult(dampedHeading);
		}
	}

	camFrame::ComputeWorldMatrixFromHeadingPitchAndRoll(dampedHeading, dampedPitch, dampedRoll, attachMatrix);
}

bool camCinematicMountedCamera::CanResetLook() const
{	
	if(m_LookBehindBlendLevel < SMALL_FLOAT)
	{
		const camControlHelper* controlHelper = GetControlHelper();

		bool bResetControls = !controlHelper->WasLookingBehind() && controlHelper->IsLookingBehind();

		float fResetLookThreshold = m_Metadata.m_ResetLookTheshold * DtoR;

		if( ((abs(m_RelativeHeading) > fResetLookThreshold ) && bResetControls) || m_bResettingLook )
		{
			return true;
		}
	}

	return false;
}

bool camCinematicMountedCamera::CanLookBehind() const
{
	if(m_ResetLookBlendLevel < SMALL_FLOAT && !m_bResettingLook && !m_bResetLookBehindForDriveby)
	{
		const camControlHelper* controlHelper = GetControlHelper();

		if(controlHelper->IsLookingBehind())
		{
			return true;
		}
	}

	return false;
}

float camCinematicMountedCamera::UpdateOrientation(const CPhysical& attachPhysical, const Matrix34& attachMatrix)
{
	Matrix34 lookAtTransformRelativeToAttachMatrix(Matrix34::IdentityType);

	eCinematicMountedCameraLookAtBehaviour LookAtBehaviour = m_Metadata.m_LookAtBehaviour; 
	
	Vector3 idealLookAtPosition;

	if(m_ShouldOverrideLookAtBehaviour)
	{
		LookAtBehaviour = m_OverriddenLookAtBehaviour;
	}

	const CPed* followPed = camInterface::FindFollowPed();
	

	if(m_LookAtTarget && m_LookAtTarget->GetIsTypePed())
	{
		followPed = static_cast<const CPed*>(m_LookAtTarget.Get()); 
	}

	m_FinalPitchLimits.Set(PI, PI);									// Init

	if(LookAtBehaviour != LOOK_FORWARD_RELATIVE_TO_ATTACH)
	{
		const Vector3& cameraPosition = m_Frame.GetPosition();
		Vector3 idealLookAtPosition;
		if(ComputeIdealLookAtPosition(attachMatrix, idealLookAtPosition))
		{
			Vector3 idealLookAtFront		= idealLookAtPosition - cameraPosition;
			idealLookAtFront.NormalizeSafe();
			camFrame::ComputeWorldMatrixFromFront(idealLookAtFront, lookAtTransformRelativeToAttachMatrix);
			lookAtTransformRelativeToAttachMatrix.Dot3x3Transpose(attachMatrix);


			if(m_shouldTerminateForOcclusion)
			{
				if (!UpdateOcclusion(cameraPosition, idealLookAtPosition))
				{
					m_canUpdate = false; 
				}
			}
		}
		
		if(LookAtBehaviour == LOOK_AT_FOLLOW_PED_RELATIVE_POSITION)
		{
			if(m_shouldTerminateForDistanceToTarget)
			{
				Vector3 CamPosition = cameraPosition; 
				Vector3 lookAtPosition = idealLookAtPosition; 
				if(m_Metadata.m_ShouldCalculateXYDistance)
				{
					CamPosition.z = 0.0f; 
					lookAtPosition.z = 0.0f;
				}
			
				float distanceToTarget = cameraPosition.Dist2(lookAtPosition); 
				
				if( distanceToTarget > m_Metadata.m_DistanceToTerminate * m_Metadata.m_DistanceToTerminate)
				{
					m_canUpdate = false; 
				}
			}
		}
		
		if(m_ShouldLimitAttachParentRelativePitchAndHeading)
		{
			float heading;
			float pitch; 
			float roll; 
			camFrame::ComputeHeadingPitchAndRollFromMatrix(lookAtTransformRelativeToAttachMatrix, heading, pitch, roll); 
			
			if(m_ShouldTerminateForPitchAndHeading)
			{
				float pitchDegrees = pitch * RtoD; 
				float headingDegrees = heading * RtoD; 
				
				if(m_NumUpdatesPerformed == 0 )
				{
					if(pitchDegrees < m_InitialRelativePitchLimits.x || pitchDegrees > m_InitialRelativePitchLimits.y )
					{
						m_canUpdate = false; 
					}

					if(headingDegrees  < m_InitialRelativeHeadingLimits.x  || headingDegrees > m_InitialRelativeHeadingLimits.y )
					{
						m_canUpdate = false; 
					}
				}
				else
				{
					if(pitchDegrees < m_AttachParentRelativePitchLimits.x || pitchDegrees > m_AttachParentRelativePitchLimits.y )
					{
						m_canUpdate = false; 
					}

					if(headingDegrees < m_AttachParentRelativeHeadingLimits.x || headingDegrees > m_AttachParentRelativeHeadingLimits.y )
					{
						m_canUpdate = false; 
					}
				}
			}

			m_FinalPitchLimits.Set(m_AttachParentRelativePitchLimits.x * DtoR, m_AttachParentRelativePitchLimits.y * DtoR);
			pitch = Clamp(pitch, m_FinalPitchLimits.x, m_FinalPitchLimits.y);
			heading = Clamp(heading, m_AttachParentRelativeHeadingLimits.x * DtoR, m_AttachParentRelativeHeadingLimits.y * DtoR);

			Vector3 camFront; 
			camFrame::ComputeFrontFromHeadingAndPitch(heading, pitch, camFront);

			camFrame::ComputeWorldMatrixFromFront(camFront, lookAtTransformRelativeToAttachMatrix);
		}
	}

	const bool isRenderingMobilePhoneCamera	= CPhoneMgr::CamGetState();

	const camGameplayDirector& gameplayDirector	= camInterface::GetGameplayDirector();
	const camBaseCamera* renderedGameplayCamera	= gameplayDirector.GetRenderedCamera();

	if ( m_Metadata.m_ShouldCopyVehicleCameraMotionBlur &&
		 renderedGameplayCamera &&
		 renderedGameplayCamera->GetIsClassId(camFollowVehicleCamera::GetStaticClassId()) )
	{
		// Inherit motion blur settings from follow vehicle camera.
		m_Frame.SetMotionBlurStrength( renderedGameplayCamera->GetFrame().GetMotionBlurStrength() );
	}

	//Unless the follow ped has just entered the vehicle or rendering of this camera had been delayed on entry (where we want a reset),
	//clone the orientation of the gameplay camera, as we want to point in roughly the same direction (subject to orientation limits.)
	const bool c_AttachedToTurret = (m_AttachedToTurret && m_Metadata.m_ShouldAttachToVehicleTurret);
	if(!m_PreviousWasInVehicleCinematicCamera && m_NumUpdatesPerformed <= 1)
	{
		const bool isGameplayTurretCamera = (renderedGameplayCamera &&
											 renderedGameplayCamera->GetIsClassId(camThirdPersonPedAimCamera::GetStaticClassId()) &&
											 static_cast<const camThirdPersonPedAimCamera*>(renderedGameplayCamera)->GetIsTurretCam());
		if(isGameplayTurretCamera && c_AttachedToTurret && renderedGameplayCamera && !camInterface::IsRenderingFirstPersonShooterCamera())
		{
			const Vector3& gameplayCameraFront = renderedGameplayCamera->GetFrameNoPostEffects().GetFront();//renderedGameplayCamera->GetBaseFront();
			const float maxProbeDistance = 50.0f;
			const u32 collisionIncludeFlags =	(u32)ArchetypeFlags::GTA_ALL_TYPES_WEAPON |
												(u32)ArchetypeFlags::GTA_RAGDOLL_TYPE |
												(u32)ArchetypeFlags::GTA_HORSE_RAGDOLL_TYPE;
			WorldProbe::CShapeTestProbeDesc lineTest;
			WorldProbe::CShapeTestFixedResults<1> shapeTestResults;
			lineTest.SetResultsStructure(&shapeTestResults);
			lineTest.SetIsDirected(true);
			lineTest.SetIncludeFlags(collisionIncludeFlags);
			lineTest.SetExcludeTypeFlags(TYPE_FLAGS_NONE);
			lineTest.SetTypeFlags(ArchetypeFlags::GTA_WEAPON_TEST);
			lineTest.SetContext(WorldProbe::LOS_Camera);
			lineTest.SetExcludeEntity(m_AttachParent, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
			lineTest.SetTreatPolyhedralBoundsAsPrimitives(true);

			const Vector3 previousGameplayPosition = renderedGameplayCamera->GetFrameNoPostEffects().GetPosition();
			const Vector3 playerPosition = attachMatrix.d;
			const Vector3 previousCameraPosToPlayer = playerPosition - previousGameplayPosition;
			float fDotProjection = previousCameraPosToPlayer.Dot(gameplayCameraFront);
			const Vector3 vProbeStartPosition = previousGameplayPosition + gameplayCameraFront*fDotProjection;
			Vector3 vTargetPoint = vProbeStartPosition+gameplayCameraFront*maxProbeDistance;

			lineTest.SetStartAndEnd(vProbeStartPosition, vTargetPoint);

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

			Vector3 vNewForward = vTargetPoint - m_Frame.GetPosition();
			vNewForward.NormalizeSafe();

			Matrix34 lookAtMatrix(attachMatrix);
			lookAtMatrix.Dot3x3FromLeft(lookAtTransformRelativeToAttachMatrix);

			Vector3 lookAtRelativeGameplayCameraFront;
			lookAtMatrix.UnTransform3x3(vNewForward, lookAtRelativeGameplayCameraFront);

			camFrame::ComputeHeadingAndPitchFromFront(lookAtRelativeGameplayCameraFront, m_RelativeHeading, m_RelativePitch);
		}
	#if FPS_MODE_SUPPORTED
		else if(c_AttachedToTurret && camInterface::IsRenderingFirstPersonShooterCamera())
		{
			Vector3 turretForward = attachMatrix.b; 
			if(m_AttachParent && m_AttachParent->GetIsTypeVehicle())
			{
				const CVehicle* attachVehicle				= static_cast<const CVehicle*>(m_AttachParent.Get()); 
				const CVehicleModelInfo* vehicleModelInfo	= attachVehicle->GetVehicleModelInfo();
				const CTurret* pVehicleTurret = camInterface::GetGameplayDirector().GetTurretForPed(attachVehicle, *followPed);
				if(vehicleModelInfo && pVehicleTurret && pVehicleTurret->GetBarrelBoneId() > -1)
				{
					crSkeleton* pSkeleton = attachVehicle->GetSkeleton();
					if(pSkeleton)
					{
						Mat34V boneMat;
						int boneIndex = vehicleModelInfo->GetBoneIndex(pVehicleTurret->GetBarrelBoneId());
						pSkeleton->GetGlobalMtx(boneIndex, boneMat);

						turretForward = VEC3V_TO_VECTOR3(boneMat.GetCol1());
					}
				}
			}

			Matrix34 lookAtMatrix(attachMatrix);
			lookAtMatrix.Dot3x3FromLeft(lookAtTransformRelativeToAttachMatrix);

			Vector3 lookAtRelativeGameplayCameraFront;
			lookAtMatrix.UnTransform3x3(turretForward, lookAtRelativeGameplayCameraFront);

			camFrame::ComputeHeadingAndPitchFromFront(lookAtRelativeGameplayCameraFront, m_RelativeHeading, m_RelativePitch);
		}
	#endif
	}

	const CVehicle* attachVehicle	= (m_AttachParent && m_AttachParent->GetIsTypeVehicle()) ? static_cast<const CVehicle*>(m_AttachParent.Get()) : NULL;

	{
		float relativeHeading, relativePitch;
		if(RestoreCameraOrientation(attachMatrix, relativeHeading, relativePitch))
		{
			m_RelativeHeading = relativeHeading;
			m_RelativePitch = relativePitch;
		}
	}

	float fCachedRelativeHeadingWithLookBehind = m_RelativeHeadingWithLookBehind;

	if(m_UseScriptRelativeHeading)
	{
		m_RelativeHeading = m_CachedScriptHeading;
		m_UseScriptRelativeHeading = false;
	}

    if(m_UseScriptRelativePitch)
    {
        m_RelativePitch = m_CachedScriptPitch;
        m_UseScriptRelativePitch = false;
    }

	m_RelativeHeadingWithLookBehind = m_RelativeHeading;

	//NOTE: We use a custom control helper when the mobile phone camera is rendering, as it uses this camera type to achieve reasonable first-person
	//framing in vehicles.
	const float seatShufflingBlendLevel = m_SeatSwitchingLimitSpring.GetResult();
	const camControlHelper* controlHelper = GetControlHelper();
	if(controlHelper)
	{
		const bool shouldCutSpring = ((m_NumUpdatesPerformed == 0) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) || m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation));

		const bool bResetLook = CanResetLook();
		float resetLookBlendLevelLastUpdate;
		if(m_ResetLookEnvelope)
		{
			if(shouldCutSpring)
			{
				if(bResetLook)
				{
					m_ResetLookEnvelope->Start(1.0f);
				}
				else
				{
					m_ResetLookEnvelope->Stop();
				}
			}
			else
			{
				m_ResetLookEnvelope->AutoStartStop(bResetLook);
			}

			resetLookBlendLevelLastUpdate = m_ResetLookEnvelope->GetLevel();
			m_ResetLookBlendLevel = m_ResetLookEnvelope->Update();
			m_ResetLookBlendLevel = Clamp(m_ResetLookBlendLevel, 0.0f, 1.0f);
			m_ResetLookBlendLevel = SlowInOut(m_ResetLookBlendLevel);

			if(bResetLook) 
			{
				m_bResettingLook = true;
			}
		}
		else
		{
			resetLookBlendLevelLastUpdate = m_ResetLookBlendLevel;
			//We don't have an envelope, so just use the state directly.
			m_ResetLookBlendLevel = bResetLook ? 1.0f : 0.0f;
		}

		if(m_bResettingLook)
		{
			//We force the relative heading to reset if we've just finished looking behind or just started leaving look behind
			if((m_ResetLookBlendLevel > 1.0f - SMALL_FLOAT && resetLookBlendLevelLastUpdate <= 1.0f - SMALL_FLOAT) ||
				(m_ResetLookBlendLevel <= 1.0f - SMALL_FLOAT && resetLookBlendLevelLastUpdate > 1.0f - SMALL_FLOAT))
			{
				m_RelativeHeading = 0.0f;
				m_RelativeHeadingWithLookBehind = 0.0f;
			}

			if( !controlHelper->IsLookingBehind() && (m_ResetLookBlendLevel > 1.0f - SMALL_FLOAT) )
			{
				m_bResettingLook = false;
			}
		}

		const bool bAllowedRearSeatLookback = ShouldAllowRearSeatLookback();
		const bool isLookingBehind = CanLookBehind() && !ShouldLookBehindInGameplayCamera() && bAllowedRearSeatLookback && !bResetLook;

		float lookBehindBlendLevelLastUpdate;
		if(m_LookBehindEnvelope)
		{
			if(shouldCutSpring)
			{
				if(isLookingBehind)
				{
					m_LookBehindEnvelope->Start(1.0f);
				}
				else
				{
					m_LookBehindEnvelope->Stop();
				}
			}
			else
			{
				m_LookBehindEnvelope->AutoStartStop(isLookingBehind);
			}

			lookBehindBlendLevelLastUpdate = m_LookBehindEnvelope->GetLevel();
			m_LookBehindBlendLevel = m_LookBehindEnvelope->Update();
			m_LookBehindBlendLevel = Clamp(m_LookBehindBlendLevel, 0.0f, 1.0f);
			m_LookBehindBlendLevel = SlowInOut(m_LookBehindBlendLevel);
		}
		else
		{
			//We don't have an envelope, so just use the state directly.
			lookBehindBlendLevelLastUpdate = (controlHelper->WasLookingBehind() && m_LookBehindBlendLevel > SMALL_FLOAT) ? 1.0f : 0.0f;
			m_LookBehindBlendLevel = isLookingBehind ? 1.0f : 0.0f;
		}

		//We force the relative heading to reset if we've just finished looking behind or just started leaving look behind
		if( !m_bResetLookBehindForDriveby && ((m_LookBehindBlendLevel > 1.0f - SMALL_FLOAT && lookBehindBlendLevelLastUpdate <= 1.0f - SMALL_FLOAT) ||
			(m_LookBehindBlendLevel <= 1.0f - SMALL_FLOAT && lookBehindBlendLevelLastUpdate > 1.0f - SMALL_FLOAT)))
		{
			m_RelativeHeading = 0.0f;
		}

		if(m_bResettingLook)
		{
			m_RelativeHeading *= (1.0f-m_ResetLookBlendLevel);
		}
		else
		{
			//! If we start to driveby, kill look behind blend level and force current relative heading.
			if( !m_bResetLookBehindForDriveby && (m_LookBehindBlendLevel > (1.0f - SMALL_FLOAT)) &&
				(m_DriveByBlendLevel > SMALL_FLOAT) )
			{
				m_RelativeHeading = fCachedRelativeHeadingWithLookBehind;
				m_bResetLookBehindForDriveby = true;
			}

			float fResetLookThreshold = m_Metadata.m_ResetLookTheshold * DtoR;
			bool bLookBehindValid = (abs(fCachedRelativeHeadingWithLookBehind) <= fResetLookThreshold ) && !controlHelper->WasLookingBehind() && (m_LookBehindBlendLevel < SMALL_FLOAT);
			if( (m_DriveByBlendLevel < SMALL_FLOAT) || bLookBehindValid)
			{
				m_bResetLookBehindForDriveby = false;
			}
		}

		float fLookBehindBlendForHeading = m_bResetLookBehindForDriveby ? 0.0f : m_LookBehindBlendLevel;

		//Update the persistent relative orientation.

		//Make the camera tend towards a particular look-at orientation with movement, but not as a passenger or when the mobile phone camera is
		//rendering.

		float fCurrentVehicleHeading		= 0.0f;
		float fCurrentVehiclePitch			= 0.0f;
		bool bEnableIgnoreVehHeadingPitchDriveby = !CPauseMenu::GetMenuPreference(PREF_FPS_RELATIVE_VEHICLE_CAMERA_DRIVEBY_AIMING);
		if((m_AttachedToTurret || (m_DriveByBlendLevel > 0.0f && bEnableIgnoreVehHeadingPitchDriveby)) && attachVehicle)
		{
			Matrix34 matVehicle = MAT34V_TO_MATRIX34(attachVehicle->GetMatrix());
			camFrame::ComputeHeadingAndPitchFromMatrix(matVehicle, fCurrentVehicleHeading, fCurrentVehiclePitch);

			// If m_PreviousVehicleHeading/m_PreviousVehiclePitch haven't been initialized, use current value for first frame.
			if (m_DriveByBlendLevel > 0.0f && m_PreviousVehicleHeading == 0.0f && m_PreviousVehiclePitch == 0.0f)
			{
				m_PreviousVehicleHeading = fCurrentVehicleHeading;
				m_PreviousVehiclePitch = fCurrentVehiclePitch;
			}
		}

		//we do this early, so we can flip heading limits and spring constants correctly
		const bool wasLookingBehind = controlHelper->WasLookingBehind();
		if((isLookingBehind && !wasLookingBehind) || (m_NumUpdatesPerformed == 0))
		{
			//Rotate towards the centre of the vehicle, unless it's a bike, quad, jet ski, or a single-seater, in which case just pick a side based upon the current relative heading.
			//Helis are included in this test, as they run no_reverse POV cameras with the exception of the side door.
			const bool shouldUseCurrentSide		= (attachVehicle && (attachVehicle->InheritsFromBike() || attachVehicle->InheritsFromQuadBike() || attachVehicle->InheritsFromAmphibiousQuadBike() ||
													attachVehicle->GetIsJetSki() || attachVehicle->InheritsFromHeli() || (attachVehicle->GetMaxPassengers() == 0)));
			m_ShouldRotateRightForLookBehind	= shouldUseCurrentSide ? (m_RelativeHeading <= 0.0f) : m_IsSeatPositionOnLeftHandSide;
		}

		float desiredRelativeHeading			= m_DesiredRelativeHeading;
		float orientationSpringBlendLevel		= 0.0f;
		float springConstantConsideringSpeed	= m_Metadata.m_OrientationSpring.m_SpringConstant;

		
		if (followPed->GetIsInVehicle() && followPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_BUSTED))
		{
			CTaskBusted *pBustedTask = static_cast<CTaskBusted*>(followPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_BUSTED));
			if (pBustedTask && !followPed->GetIsArrested() && pBustedTask->IsPlayingBustedClip() && pBustedTask->GetArrestingPed())
			{
				desiredRelativeHeading = camCinematicMountedCamera::LerpRelativeHeadingBetweenPeds(followPed, pBustedTask->GetArrestingPed(), m_Frame.ComputeHeading());
			}
		}

		const bool isSeatFacingForwards = IsFollowPedSittingInVehicleFacingForward(attachVehicle, followPed);

		if(isSeatFacingForwards && !isRenderingMobilePhoneCamera && 
			( ( IsFirstPersonCamera() && m_DriveByBlendLevel < SMALL_FLOAT && m_UnarmedDrivebyBlendLevel < SMALL_FLOAT) ||
			  (!IsFirstPersonCamera() && !ComputeIsPedDrivebyAiming(false)) ))
		{
			//NOTE: We consider the velocity along the parent front for everything but helicopters, where that is less meaningful.
			const Vector3& attachParentVelocity	= attachPhysical.GetVelocity();
			Vector3 attachParentFront			= VEC3V_TO_VECTOR3(m_AttachParent->GetTransform().GetForward());
			const bool inheritsFromHeli			= attachVehicle && attachVehicle->InheritsFromHeli();
			const bool inheritsFromSub			= attachVehicle && attachVehicle->InheritsFromSubmarine();
			const bool inheritsFromBicycleInAir	= attachVehicle && attachVehicle->InheritsFromBicycle() && attachVehicle->IsInAir();
			const float attachParentSpeedToConsider	= inheritsFromHeli || inheritsFromSub || inheritsFromBicycleInAir ? attachParentVelocity.Mag() : attachParentVelocity.Dot(attachParentFront);

			const CPedWeaponManager* pWeaponManager = followPed ? followPed->GetWeaponManager() : NULL;
			const CWeaponInfo* pEquippedWeaponInfo = pWeaponManager ? pWeaponManager->GetEquippedWeaponInfo() : NULL;
			const bool bDisablePullAroundCameraForWeapon = pEquippedWeaponInfo ? pEquippedWeaponInfo->GetDisableCameraPullAround() : false;

			bool bAllowAutoCenter = true;
			if(attachVehicle && (CPauseMenu::GetMenuPreference(PREF_FPS_VEH_AUTO_CENTER) == FALSE || bDisablePullAroundCameraForWeapon))
			{
				// Menu option is set to disable auto-center in vehicles, however...
				// auto-center can only be disabled if mouse control for that vehicle is set to camera (which == 0).
				if(inheritsFromSub)
					bAllowAutoCenter = CControl::IsMouseSteeringOn(PREF_MOUSE_SUB);
				else if (attachVehicle->GetIsAircraft())
					bAllowAutoCenter = CControl::IsMouseSteeringOn(PREF_MOUSE_FLY);
				else
					bAllowAutoCenter = CControl::IsMouseSteeringOn(PREF_MOUSE_DRIVE);
			}
			const float attachParentSpeedToConsiderForAllowingOrientation = (bAllowAutoCenter) ? 
											(m_Metadata.m_OrientationSpring.m_ShouldReversingOrientateForward ? Abs(attachParentSpeedToConsider) : attachParentSpeedToConsider) :
											0.0f;
			if (attachParentSpeedToConsiderForAllowingOrientation >= m_Metadata.m_OrientationSpring.m_MinVehicleMoveSpeedToOrientationForward)
			{
				orientationSpringBlendLevel		= 1.0f;

				const bool shouldConsiderSeatOnLeftSide = m_IsSeatPositionOnLeftHandSide || (attachVehicle && (attachVehicle->InheritsFromBike() ||
															attachVehicle->InheritsFromQuadBike() || attachVehicle->InheritsFromAmphibiousQuadBike() ||
															attachVehicle->GetIsJetSki() || (attachVehicle->GetMaxPassengers() == 0)));
			
				const Vector2 minSpringConstantsConsideringSide = shouldConsiderSeatOnLeftSide ? m_Metadata.m_OrientationSpring.m_MinSpeedSpringConstants :
																	Vector2(m_Metadata.m_OrientationSpring.m_MinSpeedSpringConstants.y, m_Metadata.m_OrientationSpring.m_MinSpeedSpringConstants.x);
				const Vector2 maxSpringConstantsConsideringSide = shouldConsiderSeatOnLeftSide ? m_Metadata.m_OrientationSpring.m_MaxSpeedSpringConstants :
																	Vector2(m_Metadata.m_OrientationSpring.m_MaxSpeedSpringConstants.y, m_Metadata.m_OrientationSpring.m_MaxSpeedSpringConstants.x);

				float minSpringConstant				= (m_RelativeHeading > 0.0f) ? minSpringConstantsConsideringSide.x : minSpringConstantsConsideringSide.y;
				float maxSpringConstant				= (m_RelativeHeading > 0.0f) ? maxSpringConstantsConsideringSide.x : maxSpringConstantsConsideringSide.y;

				const float lookAroundInputEnvelopeLevel = controlHelper->GetLookAroundInputEnvelopeLevel();
				if (isLookingBehind)
				{
					const Vector2 lookBehindSpringConstantsConsideringSide = m_ShouldRotateRightForLookBehind ? m_Metadata.m_OrientationSpring.m_LookBehindSpringConstants :
																				Vector2(m_Metadata.m_OrientationSpring.m_LookBehindSpringConstants.y, m_Metadata.m_OrientationSpring.m_LookBehindSpringConstants.x);
					const float lookBehindSpringConstant = (m_RelativeHeading > 0.0f) ? lookBehindSpringConstantsConsideringSide.x : lookBehindSpringConstantsConsideringSide.y;
				
					minSpringConstant				= Lerp(m_LookBehindBlendLevel, minSpringConstant, lookBehindSpringConstant);
					maxSpringConstant				= Lerp(m_LookBehindBlendLevel, maxSpringConstant, lookBehindSpringConstant);
				}
				else if (lookAroundInputEnvelopeLevel >= 1.0f - SMALL_FLOAT)
				{
					//we use a looser spring constant for max speed if look around input is 1.0f
					//so we have less resistance when the player is looking around, but a snappier look forward as soon as the player stops
					const Vector2 maxLookAroundSpringConstantsConsideringSide = shouldConsiderSeatOnLeftSide ? m_Metadata.m_OrientationSpring.m_MaxSpeedLookAroundSpringConstants :
																				Vector2(m_Metadata.m_OrientationSpring.m_MaxSpeedLookAroundSpringConstants.y, m_Metadata.m_OrientationSpring.m_MaxSpeedLookAroundSpringConstants.x);
					const float maxLookAroundSpringConstant = (m_RelativeHeading > 0.0f) ? maxLookAroundSpringConstantsConsideringSide.x : maxLookAroundSpringConstantsConsideringSide.y;

					maxSpringConstant				= maxLookAroundSpringConstant;
				}

				springConstantConsideringSpeed		= RampValueSafe(attachParentSpeedToConsiderForAllowingOrientation,
													m_Metadata.m_OrientationSpring.m_VehicleMoveSpeedForMinSpringConstant, m_Metadata.m_OrientationSpring.m_VehicleMoveSpeedForMaxSpringConstant,
													minSpringConstant, maxSpringConstant);

				PF_SET(springConstantConsideringSpeed, springConstantConsideringSpeed);
				PF_SET(minSpringConstant, minSpringConstant);
				PF_SET(maxSpringConstant, maxSpringConstant);
				PF_SET(attachParentSpeed, attachParentSpeedToConsiderForAllowingOrientation);
				PF_SET(minVehicleSpeed, m_Metadata.m_OrientationSpring.m_VehicleMoveSpeedForMinSpringConstant);
				PF_SET(maxVehicleSpeed, m_Metadata.m_OrientationSpring.m_VehicleMoveSpeedForMaxSpringConstant);

#if KEYBOARD_MOUSE_SUPPORT
				if (controlHelper->WasUsingKeyboardMouseLastInput())
				{
					const float lookAroundInputEnvelopeLevel = controlHelper->GetLookAroundInputEnvelopeLevel();
					//only blend out the spring blend level if we're increasing or maintaining the look around input envelope level
					if (lookAroundInputEnvelopeLevel >= m_PreviousLookAroundInputEnvelopeLevel - SMALL_FLOAT)
					{
						orientationSpringBlendLevel = Lerp(lookAroundInputEnvelopeLevel, orientationSpringBlendLevel, 0.0f);
					}
					m_PreviousLookAroundInputEnvelopeLevel = lookAroundInputEnvelopeLevel;
				}
#endif // KEYBOARD_MOUSE_SUPPORT
			}
		}

		float lookAroundScalingForZoom = 1.0f;
		if(controlHelper->IsUsingZoomInput())
		{
			//Scale the look-around offsets based up the zoom factor, as this allows the look-around input to be responsive at minimum zoom without
			//being too fast at maximum zoom.
			//TODO: Push this into the control helper.
			const float zoomFactor				= controlHelper->GetZoomFactor();
			const float zoomFactorToConsider	= Max(zoomFactor, 1.0f);
			lookAroundScalingForZoom			= 1.0f / zoomFactorToConsider;
		}

		//apply leading look - offset the desired heading when turning
		const camCinematicMountedCameraMetadataLeadingLookSettings& settings = m_Metadata.m_LeadingLookSettings;

		Vector3 attachParentVelocity		= attachPhysical.GetVelocity();
		attachParentVelocity.z				= 0.0f;
		const Vector3 attachParentFront		= VEC3V_TO_VECTOR3(m_AttachParent->GetTransform().GetForward());
		const float attachParentSpeed		= attachParentVelocity.Dot(attachParentFront);

		float desiredHeadingOffset			= 0.0f;

		if (attachVehicle && (Abs(attachParentSpeed) > settings.m_AbsMinVehicleSpeed) && !isLookingBehind)
		{
			const bool isFollowPedDriver	= followPed && followPed->GetIsDrivingVehicle();
			if (isFollowPedDriver)
			{
				CVehicle* nonConstAttachVehicle	= const_cast<CVehicle*>(attachVehicle); //GetSteerRatioForAnim() is non-const but behaves const
				const float steeringAngleRatio	= nonConstAttachVehicle->GetSteerRatioForAnim();

				if (Abs(steeringAngleRatio) >= SMALL_FLOAT)
				{
					desiredHeadingOffset		= RampValueSafe(Abs(steeringAngleRatio), 0.0f, 1.0f, 0.0f, settings.m_MaxHeadingOffsetDegrees * DtoR);
					desiredHeadingOffset		*= Sign(steeringAngleRatio);
				}
			}
			else if (isSeatFacingForwards)
			{
				const Vector3 attachParentRight		= VEC3V_TO_VECTOR3(m_AttachParent->GetTransform().GetRight());
				const float attachParentRightSpeed	= attachParentVelocity.Dot(attachParentRight);

				desiredHeadingOffset				= RampValueSafe(Abs(attachParentRightSpeed), 0.0f, settings.m_AbsMaxPassengerRightSpeed, 0.0f, settings.m_MaxPassengerHeadingOffsetDegrees * DtoR);
				desiredHeadingOffset				*= Sign(attachParentRightSpeed);
				desiredHeadingOffset				*= settings.m_ShouldReversePassengerLookDirection ? -1.0f : 1.0f;
			}
		}

		const float leadingLookHeadingSpringConstant	= shouldCutSpring ? 0.0f : settings.m_SpringConstant;
		const float headingOffsetToApply				= m_LeadingLookHeadingSpring.Update(desiredHeadingOffset, leadingLookHeadingSpringConstant, settings.m_SpringDampingRatio);

		if (Abs(attachParentSpeed) > settings.m_AbsMinVehicleSpeed)
		{
			if (attachParentSpeed < 0.0f)
			{
				if (settings.m_ShouldApplyWhenReversing)
				{
					desiredRelativeHeading -= headingOffsetToApply;
				}
			}
			else
			{
				desiredRelativeHeading += headingOffsetToApply;
			}
		}

		//Ensure that we blend to the desired orientation over the shortest angle.
		const float desiredRelativeHeadingDelta = desiredRelativeHeading - m_RelativeHeading;
		if(desiredRelativeHeadingDelta > PI)
		{
			desiredRelativeHeading -= TWO_PI;
		}
		else if(desiredRelativeHeadingDelta < -PI)
		{
			desiredRelativeHeading += TWO_PI;
		}

		//we tend the relative heading towards 0 if seat shuffling
		bool isShuffleTargetValid = false;
		Matrix34 matShuffleTarget;
		float fShuffleRelativeHeading = desiredRelativeHeading;
		if (seatShufflingBlendLevel >= SMALL_FLOAT)
		{
			float fHeadingRelativeTo = camFrame::ComputeHeadingFromMatrix( attachMatrix );
			if(m_SeatShuffleChangedCameras && m_NumUpdatesPerformed == 0)
			{
				const camFrame& cinematicDirectorFrame = camInterface::GetCinematicDirector().GetFrameNoPostEffects();
				cinematicDirectorFrame.ComputeHeadingAndPitch(m_RelativeHeading, m_RelativePitch);
				m_RelativeHeading = fwAngle::LimitRadianAngle(m_RelativeHeading - fHeadingRelativeTo);
			}

			if (attachVehicle && attachVehicle->IsSeatIndexValid(m_SeatShuffleTargetSeat))
			{
				const CVehicleModelInfo* vehicleModelInfo	= attachVehicle->GetVehicleModelInfo();
				if(vehicleModelInfo)
				{
					if(!IsVehicleTurretCamera())
					{
						if(m_Metadata.m_ShouldAttachToFollowPed && m_DestSeatHeading != FLT_MAX && (m_SeatShufflePhase > 0.50f || m_SeatShuffleChangedCameras))
						{
							desiredRelativeHeading = fwAngle::LimitRadianAngle(m_DestSeatHeading - fHeadingRelativeTo);
						}
					}
					else if(m_SeatShufflePhase > 0.50f || m_SeatShuffleChangedCameras)
					{
						const CTurret* pVehicleTurret = NULL;
						if(camGameplayDirector::IsTurretSeat(attachVehicle, m_SeatShuffleTargetSeat)) 
						{
							const CVehicleWeaponMgr* vehicleWeaponManager = attachVehicle->GetVehicleWeaponMgr();
							if(vehicleWeaponManager)
							{
								pVehicleTurret = vehicleWeaponManager->GetFirstTurretForSeat(m_SeatShuffleTargetSeat);
							}
						}

						if(pVehicleTurret && pVehicleTurret->GetBarrelBoneId() > -1)
						{
							crSkeleton* pSkeleton = attachVehicle->GetSkeleton();
							if(pSkeleton)
							{
								Mat34V boneMat;
								int boneIndex = vehicleModelInfo->GetBoneIndex(pVehicleTurret->GetBaseBoneId());
								pSkeleton->GetGlobalMtx(boneIndex, boneMat);

								matShuffleTarget = MAT34V_TO_MATRIX34(boneMat);
								float fHeadingRelativeTo = camFrame::ComputeHeadingFromMatrix( attachMatrix ); //MAT34V_TO_MATRIX34(attachPhysical.GetMatrix()) );
								desiredRelativeHeading = camFrame::ComputeHeadingFromMatrix( matShuffleTarget );
								desiredRelativeHeading = fwAngle::LimitRadianAngle( desiredRelativeHeading - fHeadingRelativeTo );
								if(m_SeatShuffleChangedCameras && m_NumUpdatesPerformed == 0)
								{
									m_RelativeHeading = desiredRelativeHeading;
								}
								isShuffleTargetValid = true;
							}
						}
					}
				}
			}

			// If camera is not relative to head bone, use head bone heading for shuffle.
			fShuffleRelativeHeading = m_Metadata.m_RelativeHeadingWhenSeatShuffling * DtoR;
			if(m_HeadBoneHeading != FLT_MAX)
			{
				TUNE_GROUP_FLOAT(VEHICLE_POV_CAMERA, c_ShuffleHeadingSpringConstant, 125.0f, 0.01f, 200.0f, 0.05f);
				fShuffleRelativeHeading = fwAngle::LimitRadianAngle( m_HeadBoneHeading - fHeadingRelativeTo );
				springConstantConsideringSpeed = c_ShuffleHeadingSpringConstant;					// TODO: add to metadata?
				if(m_bAllowSpringCutOnFirstUpdate)
				{
					orientationSpringBlendLevel = 1.0f;
				}
			}

			orientationSpringBlendLevel	= Lerp(seatShufflingBlendLevel, orientationSpringBlendLevel, 1.0f);
			desiredRelativeHeading		= fwAngle::LerpTowards(desiredRelativeHeading, fShuffleRelativeHeading, seatShufflingBlendLevel);
		}

		//blend out orientation spring with look around input, if not a driver and is not doing a seat shuffle
		const bool isFollowPedDriver = followPed && followPed->GetIsDrivingVehicle();
		if (!isFollowPedDriver && seatShufflingBlendLevel < SMALL_FLOAT)
		{
			const float lookAroundInputEnvelopeLevel	= controlHelper->GetLookAroundInputEnvelopeLevel();
			orientationSpringBlendLevel					*= (1.0f - lookAroundInputEnvelopeLevel);
		}

		// we tend the relative heading towards 0 if putting on a helmet
		bool bResetRelativeHeading = false;
		if (followPed->GetMyVehicle())
		{
			if (followPed->GetMyVehicle()->InheritsFromBicycle())
			{
				const CTaskMotionOnBicycle* pMotionTask = static_cast<const CTaskMotionOnBicycle*>(followPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_ON_BICYCLE));
				if (pMotionTask && pMotionTask->ShouldLimitCameraDueToJump())
				{
					bResetRelativeHeading = true;
				}
			}
			else if (followPed->GetMyVehicle()->InheritsFromBike() || followPed->GetMyVehicle()->InheritsFromQuadBike() || followPed->GetMyVehicle()->InheritsFromAmphibiousQuadBike())
			{
				const CTaskMotionInAutomobile* pMotionTask = static_cast<const CTaskMotionInAutomobile*>(followPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_AUTOMOBILE));
				if (pMotionTask && pMotionTask->GetState() == CTaskMotionInAutomobile::State_StartEngine)
				{
					bResetRelativeHeading = true;
				}
			}

			if(m_UsingFirstPersonAnimatedData || m_WasUsingFirstPersonAnimatedData)
			{
				bResetRelativeHeading = true;
			}
		}

		if(m_bResettingLook)
		{
			desiredRelativeHeading = 0.0f;
		}

		if (bResetRelativeHeading || followPed->GetPedResetFlag(CPED_RESET_FLAG_PuttingOnHelmet))
		{
			orientationSpringBlendLevel = 1.0f;
			desiredRelativeHeading = (!c_AttachedToTurret) ? 0.0f : m_RelativeHeading;
		}

		bool shouldCutHeadingSpring = shouldCutSpring && seatShufflingBlendLevel < SMALL_FLOAT;
		float fRelativeHeadingForLerp = m_RelativeHeading;
		if(seatShufflingBlendLevel >= SMALL_FLOAT && (m_SeatShufflePhase > 0.50f || m_SeatShuffleChangedCameras))
		{
			fRelativeHeadingForLerp = desiredRelativeHeading;
			desiredRelativeHeading  = fShuffleRelativeHeading;
		}

		if(m_RelativeHeading > PI)
		{
			fRelativeHeadingForLerp -= TWO_PI;
		}
		else if(m_RelativeHeading < -PI)
		{
			fRelativeHeadingForLerp += TWO_PI;
		}

		float desiredRelativeHeadingToSpring	= fwAngle::LerpTowards(fRelativeHeadingForLerp, desiredRelativeHeading, orientationSpringBlendLevel);
		const float relativeHeadingSpringConstant	= shouldCutHeadingSpring ? 0.0f : springConstantConsideringSpeed;

		if(m_RelativeHeading > PI)
		{
			desiredRelativeHeadingToSpring += TWO_PI;
		}
		else if(m_RelativeHeading < -PI)
		{
			desiredRelativeHeadingToSpring -= TWO_PI;
		}

		m_RelativeHeadingSpring.OverrideResult(m_RelativeHeading);
		m_RelativeHeading	= m_RelativeHeadingSpring.Update(desiredRelativeHeadingToSpring, relativeHeadingSpringConstant,
								m_Metadata.m_OrientationSpring.m_SpringDampingRatio);

		if((m_AttachedToTurret || (m_DriveByBlendLevel > 0.0f && bEnableIgnoreVehHeadingPitchDriveby)) && attachVehicle && m_NumUpdatesPerformed > 0)
		{
			float fHeadingDifference = fwAngle::LimitRadianAngle(fCurrentVehicleHeading - m_PreviousVehicleHeading);
			// Don't limit driveby angle here, we don't want to wrap around.
			if (m_AttachedToTurret)
			{
				m_RelativeHeading = fwAngle::LimitRadianAngle(m_RelativeHeading - fHeadingDifference);
			}
			else
			{
				m_RelativeHeading = (m_RelativeHeading - fHeadingDifference);
			}
		}

		const float lookAroundScalingForUnarmedDriveby = RampValue(m_UnarmedDrivebyBlendLevel, 0.0f, 1.0f, 1.0f, m_Metadata.m_LookAroundScalingForUnarmedDriveby);
		const float lookAroundScalingForArmedDriveby	= RampValue(m_DriveByBlendLevel, 0.0f, 1.0f, 1.0f, m_Metadata.m_LookAroundScalingForArmedDriveby);

		float lookAroundHeadingOffset	= controlHelper->GetLookAroundHeadingOffset();
		lookAroundHeadingOffset			*= lookAroundScalingForZoom;
		lookAroundHeadingOffset			*= lookAroundScalingForUnarmedDriveby;
		lookAroundHeadingOffset			*= lookAroundScalingForArmedDriveby;
		lookAroundHeadingOffset			*= (1.0f - seatShufflingBlendLevel);
		bool bUseLookAroundHeadingOffset = !followPed->GetPedResetFlag(CPED_RESET_FLAG_PuttingOnHelmet) && !bResetRelativeHeading;
		if (bUseLookAroundHeadingOffset)
		{
			m_RelativeHeading			+= lookAroundHeadingOffset;
		}

		const float fScriptedVehAnimBlendLevel = m_ScriptedVehAnimBlendLevelSpring.GetResult();

		if(m_ShouldLimitAttachParentRelativePitchAndHeading && LookAtBehaviour == LOOK_FORWARD_RELATIVE_TO_ATTACH && !m_AttachedToTurret)
		{
			Vector2 attachParentRelativeHeadingLimitsToConsider;
			attachParentRelativeHeadingLimitsToConsider.Lerp(m_DuckingBlendLevel, m_AttachParentRelativeHeadingLimits, m_Metadata.m_AttachParentRelativeHeadingForDucking);
			attachParentRelativeHeadingLimitsToConsider.Lerp(m_UnarmedDrivebyBlendLevel, attachParentRelativeHeadingLimitsToConsider, m_Metadata.m_AttachParentRelativeHeadingForUnarmedDriveby);

			Vector2 vVehicleMeleeLimits(m_Metadata.m_AttachParentRelativeHeadingForVehicleMelee);
			if (followPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsPerformingVehicleMelee) && followPed->GetPlayerInfo() && !followPed->GetPlayerInfo()->GetVehMeleePerformingRightHit())
			{
				vVehicleMeleeLimits.Set(Vector2(-vVehicleMeleeLimits.y, -vVehicleMeleeLimits.x));
			}
			attachParentRelativeHeadingLimitsToConsider.Lerp(m_VehicleMeleeBlendLevel, attachParentRelativeHeadingLimitsToConsider, vVehicleMeleeLimits);

			const float fOpenHeliSideDoorsBlendLevel = m_OpenHeliSideDoorBlendLevelSpring.GetResult();
			attachParentRelativeHeadingLimitsToConsider.Lerp(fOpenHeliSideDoorsBlendLevel, attachParentRelativeHeadingLimitsToConsider, m_Metadata.m_AttachParentRelativeHeadingForExitHeliSideDoor);

			TUNE_GROUP_FLOAT(HELMET_VISOR, cameraHeadingLimitMin, -40.0f, -180.0f, 180.0f, 1.0f);
			TUNE_GROUP_FLOAT(HELMET_VISOR, cameraHeadingLimitMax, 40.0f, -180.0f, 180.0f, 1.0f);
			Vector2 vSwitchVisorHeadingLimits(cameraHeadingLimitMin,cameraHeadingLimitMax);
			const float fSwitchHelmetVisorBlendLevel = m_SwitchHelmetVisorBlendLevelSpring.GetResult();
			attachParentRelativeHeadingLimitsToConsider.Lerp(fSwitchHelmetVisorBlendLevel, attachParentRelativeHeadingLimitsToConsider, vSwitchVisorHeadingLimits);

			Vector2 drivebyLimits = m_Metadata.m_AttachParentRelativeHeadingForDriveby;

			const CVehicle* attachVehicle = (m_AttachParent && m_AttachParent->GetIsTypeVehicle()) ? static_cast<const CVehicle*>(m_AttachParent.Get()) : NULL;
			if(attachVehicle && m_SmashWindowBlendLevel <= 0.01f)
			{
				const CFirstPersonDriveByLookAroundData *pDrivebyLookData = GetFirstPersonDrivebyData(attachVehicle);
				if(pDrivebyLookData)
				{
					drivebyLimits = pDrivebyLookData->m_HeadingLimits;
				}
			}

			attachParentRelativeHeadingLimitsToConsider.Lerp(m_DriveByBlendLevel, attachParentRelativeHeadingLimitsToConsider, drivebyLimits );

			if (attachVehicle)
			{
				Vector2 vScriptedVehAnimHeadingOffset = GetSeatSpecificScriptedAnimOffset(attachVehicle);
				attachParentRelativeHeadingLimitsToConsider.Lerp(fScriptedVehAnimBlendLevel, attachParentRelativeHeadingLimitsToConsider, m_Metadata.m_AttachParentRelativeHeadingForScriptedVehAnim + vScriptedVehAnimHeadingOffset);
			}
						
			if(m_bResetLookBehindForDriveby && (m_LookBehindBlendLevel > SMALL_FLOAT))
			{
				Vector2 lookBehindLimitsConsideringSide = m_ShouldRotateRightForLookBehind ? m_Metadata.m_AttachParentRelativeHeadingForLookBehind :
					Vector2(-m_Metadata.m_AttachParentRelativeHeadingForLookBehind.y, -m_Metadata.m_AttachParentRelativeHeadingForLookBehind.x);
				lookBehindLimitsConsideringSide += m_Metadata.m_RelativeHeadingOffsetsForLookBehind;

				attachParentRelativeHeadingLimitsToConsider.Lerp(m_LookBehindBlendLevel, attachParentRelativeHeadingLimitsToConsider, lookBehindLimitsConsideringSide);

				const float lookBehindDriveByBlendLevel = m_DriveByBlendLevel * m_LookBehindBlendLevel;
				Vector2 lookBehindDriveByLimitsConsideringSide = m_ShouldRotateRightForLookBehind ? m_Metadata.m_AttachParentRelativeHeadingForLookBehindDriveBy :
					Vector2(-m_Metadata.m_AttachParentRelativeHeadingForLookBehindDriveBy.y, -m_Metadata.m_AttachParentRelativeHeadingForLookBehindDriveBy.x);
				lookBehindDriveByLimitsConsideringSide += m_Metadata.m_RelativeHeadingOffsetsForLookBehind;

				attachParentRelativeHeadingLimitsToConsider.Lerp(lookBehindDriveByBlendLevel, attachParentRelativeHeadingLimitsToConsider, lookBehindDriveByLimitsConsideringSide);
			}
			//only apply look behind offsets when fully looking behind 
			else if (m_LookBehindBlendLevel >= 1.0f - SMALL_FLOAT)
			{
				const Vector2 lookBehindLimitsConsideringSide = m_ShouldRotateRightForLookBehind ? m_Metadata.m_AttachParentRelativeHeadingForLookBehind :
																Vector2(-m_Metadata.m_AttachParentRelativeHeadingForLookBehind.y, -m_Metadata.m_AttachParentRelativeHeadingForLookBehind.x);
			
				attachParentRelativeHeadingLimitsToConsider.Lerp(m_LookBehindBlendLevel, attachParentRelativeHeadingLimitsToConsider, lookBehindLimitsConsideringSide);

				const float lookBehindDriveByBlendLevel = m_DriveByBlendLevel * m_LookBehindBlendLevel;
				const Vector2 lookBehindDriveByLimitsConsideringSide = m_ShouldRotateRightForLookBehind ? m_Metadata.m_AttachParentRelativeHeadingForLookBehindDriveBy :
																		Vector2(-m_Metadata.m_AttachParentRelativeHeadingForLookBehindDriveBy.y, -m_Metadata.m_AttachParentRelativeHeadingForLookBehindDriveBy.x);

				attachParentRelativeHeadingLimitsToConsider.Lerp(lookBehindDriveByBlendLevel, attachParentRelativeHeadingLimitsToConsider, lookBehindDriveByLimitsConsideringSide);
			}

			float fTurretHeadingBlendLevel = m_HeadingLimitNextToTurretSeatBlendLevelSpring.GetResult();
			attachParentRelativeHeadingLimitsToConsider.Lerp(fTurretHeadingBlendLevel, attachParentRelativeHeadingLimitsToConsider, Vector2(m_fHeadingLimitForNextToTurretSeats,attachParentRelativeHeadingLimitsToConsider.y));

			const bool shouldConsiderSeatOnLeftSide = m_IsSeatPositionOnLeftHandSide || (attachVehicle && (attachVehicle->InheritsFromBike() ||
														attachVehicle->InheritsFromQuadBike() || attachVehicle->InheritsFromAmphibiousQuadBike() ||
														attachVehicle->GetIsJetSki() || (attachVehicle->GetMaxPassengers() == 0)));
			
			const Vector2 headingLimitsConsideringSide = (shouldConsiderSeatOnLeftSide || (m_LookBehindBlendLevel >= SMALL_FLOAT && !m_bResetLookBehindForDriveby)) ? attachParentRelativeHeadingLimitsToConsider :
															Vector2(-attachParentRelativeHeadingLimitsToConsider.y, -attachParentRelativeHeadingLimitsToConsider.x);

			// Only clamp relative heading if we are not moving to another seat.
			if(seatShufflingBlendLevel < SMALL_FLOAT || m_SeatShufflePhase < 0.50f || m_SeatShuffleChangedCameras)
			{
				m_RelativeHeading = Clamp(m_RelativeHeading, headingLimitsConsideringSide.x * DtoR, headingLimitsConsideringSide.y * DtoR);
			}
		}
		else if(m_AttachedToTurret && m_ShouldLimitAttachParentRelativePitchAndHeading && seatShufflingBlendLevel < SHUFFLE_FROM_TURRET_USE_HEAD_BONE_THRESHOLD)
		{
			LimitParentHeadingRelativeToAttachParent( m_RelativeHeading );
		}
		else
		{
			LimitRelativeHeading(m_RelativeHeading);
		}

		//Apply a post-effect heading rotation for look-behind.

		float desiredRelativeHeadingOffsetForLookBehind = DtoR * (m_ShouldRotateRightForLookBehind ? m_Metadata.m_RelativeHeadingOffsetsForLookBehind.x :
															m_Metadata.m_RelativeHeadingOffsetsForLookBehind.y);

		//Because we're not resetting the relative heading until look behind has finished, we amend the offset for look behind to compensate for the existing relative heading
		if(m_LookBehindBlendLevel <= 1.0f - SMALL_FLOAT)
		{
			desiredRelativeHeadingOffsetForLookBehind -= m_RelativeHeading;
		}

		float fClampedRelativeHeading = m_RelativeHeading;
		if(fClampedRelativeHeading > PI)
		{
			fClampedRelativeHeading -= (PI * 2.0f); 
		}
		else if(fClampedRelativeHeading < -PI)
		{
			fClampedRelativeHeading += (PI * 2.0f); 
		}

		if(m_bResettingLook)
		{
			fClampedRelativeHeading *= (1.0f-m_ResetLookBlendLevel);
		}

		//NOTE: We never want to rotate around the back, so we avoid a shortest angle Lerp.
		float fClampedrelativeHeadingWithLookBehind = fClampedRelativeHeading + (desiredRelativeHeadingOffsetForLookBehind * fLookBehindBlendForHeading);

		m_RelativeHeadingWithLookBehind = m_RelativeHeading + (desiredRelativeHeadingOffsetForLookBehind * fLookBehindBlendForHeading);
				
		lookAtTransformRelativeToAttachMatrix.RotateLocalZ(fClampedrelativeHeadingWithLookBehind);

		//Update pitch:

		float relativePitchAdjustedForThrottle = m_Metadata.m_DefaultRelativePitch*DtoR;

		const camCinematicMountedCameraMetadataRelativePitchScalingToThrottle& pitchScalingSettings = m_Metadata.m_RelativePitchScaling;
		if (pitchScalingSettings.m_ShouldScaleRelativePitchToThrottle)
		{
			const CVehicle* vehicleToConsider = NULL;
			if (attachPhysical.GetIsTypeVehicle())
			{
				vehicleToConsider = static_cast<const CVehicle*>(&attachPhysical);
			}
			else if (attachPhysical.GetIsTypePed())
			{
				const CPed& attachPed = static_cast<const CPed&>(attachPhysical);
				vehicleToConsider = attachPed.GetMyVehicle();
			}

			if (vehicleToConsider)
			{
				const float vehicleThrottle = vehicleToConsider->GetThrottle();
				if (vehicleThrottle >= SMALL_FLOAT)
				{
					relativePitchAdjustedForThrottle = pitchScalingSettings.m_RelativePitchAtMaxThrottle*DtoR;
				}
			}
		}

		bool shouldCutPitchSpring = shouldCutSpring && seatShufflingBlendLevel < SMALL_FLOAT;
		float desiredRelativePitchToSpring		= fwAngle::LerpTowards(m_RelativePitch, relativePitchAdjustedForThrottle, orientationSpringBlendLevel);
		float relativePitchSpringConstant		= shouldCutPitchSpring ? 0.0f : springConstantConsideringSpeed;
		if (seatShufflingBlendLevel >= SMALL_FLOAT && m_HeadBonePitch != FLT_MAX)
		{
			float pitchOffsetToApply = 0.0f;
			if(m_ShouldLimitAttachParentRelativePitchAndHeading && LookAtBehaviour == LOOK_FORWARD_RELATIVE_TO_ATTACH && !m_AttachedToTurret)
			{
				pitchOffsetToApply = m_Metadata.m_PitchOffsetToApply * DtoR;
			}

			float fPitchRelativeTo = camFrame::ComputePitchFromMatrix( attachMatrix );//MAT34V_TO_MATRIX34(attachPhysical.GetMatrix()) );
			if(m_SeatShuffleChangedCameras && m_NumUpdatesPerformed == 0)
			{
				// Already calculated above as a world relative pitch.
				m_RelativePitch = fwAngle::LimitRadianAngle(m_RelativePitch - pitchOffsetToApply - fPitchRelativeTo);
			}

			if(isShuffleTargetValid && (m_SeatShufflePhase > 0.50f || m_SeatShuffleChangedCameras))
			{
				desiredRelativePitchToSpring = camFrame::ComputePitchFromMatrix(matShuffleTarget);
				desiredRelativePitchToSpring = fwAngle::LimitRadianAngle( desiredRelativePitchToSpring - fPitchRelativeTo );
				if(m_SeatShuffleChangedCameras && m_NumUpdatesPerformed == 0)
				{
					m_RelativePitch = desiredRelativePitchToSpring;
				}
			}
			else if(m_SeatShufflePhase > 0.50f || m_SeatShuffleChangedCameras)
			{
				if(attachVehicle)
				{
					if(m_Metadata.m_ShouldAttachToFollowPed && m_DestSeatPitch != FLT_MAX && (m_SeatShufflePhase > 0.50f || m_SeatShuffleChangedCameras))
					{
						desiredRelativePitchToSpring = fwAngle::LimitRadianAngle(m_DestSeatPitch + pitchOffsetToApply - fPitchRelativeTo);
						desiredRelativePitchToSpring = Clamp(desiredRelativePitchToSpring, m_Metadata.m_MinPitch * DtoR, m_Metadata.m_MaxPitch * DtoR);
					}
					else
					{
						// Base pitch is relative to vehicle pitch, so convert to be relative to attach matrix.
						desiredRelativePitchToSpring = camFrame::ComputePitchFromMatrix( MAT34V_TO_MATRIX34(attachVehicle->GetMatrix()) );
						if(!m_SeatShuffleChangedCameras && m_BaseHeadingRadians != 0.0f)
						{
							// Negate fPitchRelativeTo to get the value of the destination seat.
							desiredRelativePitchToSpring = fwAngle::LimitRadianAngle( desiredRelativePitchToSpring + pitchOffsetToApply + fPitchRelativeTo );
						}
						else
						{
							desiredRelativePitchToSpring = fwAngle::LimitRadianAngle( desiredRelativePitchToSpring + pitchOffsetToApply - fPitchRelativeTo );
						}
					}
				}
				else
				{
					desiredRelativePitchToSpring = 0.0f;
				}
			}

			// If camera is not relative to head bone, use head bone heading for shuffle.
			TUNE_GROUP_FLOAT(VEHICLE_POV_CAMERA, c_ShufflePitchSpringConstant, 75.0f, 0.01f, 200.0f, 0.05f);
			float fShuffleRelativePitch = fwAngle::LimitRadianAngle( m_HeadBonePitch - fPitchRelativeTo );
			desiredRelativePitchToSpring = Lerp(seatShufflingBlendLevel, desiredRelativePitchToSpring, fShuffleRelativePitch);
			relativePitchSpringConstant = c_ShufflePitchSpringConstant;					// TODO: add to metadata?
		}

		m_RelativePitchSpring.OverrideResult(m_RelativePitch);
		m_RelativePitch	= m_RelativePitchSpring.Update(desiredRelativePitchToSpring, relativePitchSpringConstant, m_Metadata.m_OrientationSpring.m_SpringDampingRatio);

		if((m_AttachedToTurret || (m_DriveByBlendLevel > 0.0f && bEnableIgnoreVehHeadingPitchDriveby)) && attachVehicle && m_NumUpdatesPerformed > 0)
		{
			float fPitchDifference = fwAngle::LimitRadianAngle(fCurrentVehiclePitch - m_PreviousVehiclePitch);
			// Don't limit driveby angle here, we don't want to wrap around.
			if (m_AttachedToTurret)
			{
				m_RelativePitch = fwAngle::LimitRadianAngle(m_RelativePitch - fPitchDifference);
			}
			else
			{
				m_RelativePitch = (m_RelativePitch - fPitchDifference);
			}
		}

		float pitchOffsetToApply		= m_Metadata.m_PitchOffsetToApply * DtoR;

		float lookAroundPitchOffset		= controlHelper->GetLookAroundPitchOffset();
		lookAroundPitchOffset			*= lookAroundScalingForZoom;
		lookAroundPitchOffset			*= lookAroundScalingForArmedDriveby;
		bool bUseLookAroundPitchOffset = !followPed->GetPedResetFlag(CPED_RESET_FLAG_PuttingOnHelmet);
		if (bUseLookAroundPitchOffset)
		{
			m_RelativePitch				+= lookAroundPitchOffset;	
		}

		float fMaxPitch	= m_Metadata.m_MaxPitch;
		float minPitchScaling = 1.0f;
		float lookBehindDriveByBlendLevel = 0.0f;
		if(IsFirstPersonCamera() && !m_AttachedToTurret)
		{
			//Blend out the ability to look down as the camera looks backwards and when ducking or unarmed drive-by aiming.
			minPitchScaling					= 0.5f * (Cosf(m_RelativeHeadingWithLookBehind) + 1.0f);
			minPitchScaling					*= (1.0f - m_DuckingBlendLevel);
			minPitchScaling					*= (1.0f - m_UnarmedDrivebyBlendLevel);

			fMaxPitch						= Lerp(m_UnarmedDrivebyBlendLevel, m_Metadata.m_MaxPitch, m_Metadata.m_MaxPitchUnarmed);

			//we blend out this behaviour if drive-by aiming and looking behind
			lookBehindDriveByBlendLevel		= m_DriveByBlendLevel * m_LookBehindBlendLevel;
			minPitchScaling					= Lerp(lookBehindDriveByBlendLevel, minPitchScaling, 1.0f);
			minPitchScaling					= Lerp(m_DriveByBlendLevel, minPitchScaling, 1.0f);
		}

		m_FinalPitchLimits.Set(m_Metadata.m_MinPitch * minPitchScaling * DtoR - pitchOffsetToApply, fMaxPitch * DtoR - pitchOffsetToApply);
		m_RelativePitch	= Clamp(m_RelativePitch, m_FinalPitchLimits.x, m_FinalPitchLimits.y);

		if(m_ShouldLimitAttachParentRelativePitchAndHeading && LookAtBehaviour == LOOK_FORWARD_RELATIVE_TO_ATTACH && !m_AttachedToTurret)
		{	
			Vector2 attachParentRelativePitchLimitsToConsider = m_AttachParentRelativePitchLimits;

			const CFirstPersonDriveByLookAroundData *pDrivebyLookData = GetFirstPersonDrivebyData(attachVehicle);
			if(pDrivebyLookData)
			{
				//We know this is wrong, we should not use the m_AttachParentRelativePitchForLookBehindDriveBy but we should use the m_AttachParentRelativePitch.
				//However this has been broken too long now and fixing it will probably cause the issues with other existing cameras. If you have bugs related to
				//this setting, the safest thing is to try to tune the metadata itself and make it equal to m_AttachParentRelativePitch
				Vector2 vDrivebyPitchLimits = m_Metadata.m_AttachParentRelativePitchForLookBehindDriveBy;

				bool bLeftSide = m_RelativeHeadingWithLookBehind > 0.0f;
				float fRelativeHeadingAbs = abs(m_RelativeHeadingWithLookBehind);

				const CFirstPersonDriveByLookAroundSideData & SideData = bLeftSide ? pDrivebyLookData->GetDataLeft() : pDrivebyLookData->GetDataRight();

				float fMinAngle = SideData.m_AngleToBlendInExtraPitch.x * DtoR;
				float fMaxAngle = SideData.m_AngleToBlendInExtraPitch.y * DtoR;

				if ( (fRelativeHeadingAbs >= fMinAngle))
				{
					float fOffsetRatio;

					if( (fMaxAngle - fMinAngle) <= SMALL_FLOAT)
					{
						fOffsetRatio = 1.0f;
					}
					else
					{
						fOffsetRatio = Clamp( (fRelativeHeadingAbs - fMinAngle) / (fMaxAngle - fMinAngle), 0.0f, 1.0f);
					}

					vDrivebyPitchLimits.x += (fOffsetRatio * SideData.m_ExtraRelativePitch.x);
					vDrivebyPitchLimits.y += (fOffsetRatio * SideData.m_ExtraRelativePitch.y);
				}

				attachParentRelativePitchLimitsToConsider.Lerp(m_DriveByBlendLevel, attachParentRelativePitchLimitsToConsider, vDrivebyPitchLimits);
			}
			
			attachParentRelativePitchLimitsToConsider.Lerp(lookBehindDriveByBlendLevel, attachParentRelativePitchLimitsToConsider, m_Metadata.m_AttachParentRelativePitchForLookBehindDriveBy);

			const float fRappellingBlendLevel = m_RappellingBlendLevelSpring.GetResult();
			attachParentRelativePitchLimitsToConsider.Lerp(fRappellingBlendLevel, attachParentRelativePitchLimitsToConsider, m_Metadata.m_AttachParentRelativePitchForRappelling);

			attachParentRelativePitchLimitsToConsider.Lerp(m_VehicleMeleeBlendLevel, attachParentRelativePitchLimitsToConsider, m_Metadata.m_AttachParentRelativePitchForVehicleMelee);

			TUNE_GROUP_FLOAT(HELMET_VISOR, cameraPitchLimitsMin, -20.0f, -180.0f, 180.0f, 0.1f);
			TUNE_GROUP_FLOAT(HELMET_VISOR, cameraPitchLimitsMax, -5.0f, -180.0f, 180.0f, 0.1f);
			Vector2 vSwitchHelmetVisorPitchLimits(cameraPitchLimitsMin,cameraPitchLimitsMax);
			const float fSwitchHelmetVisorBlendLevel = m_SwitchHelmetVisorBlendLevelSpring.GetResult();
			attachParentRelativePitchLimitsToConsider.Lerp(fSwitchHelmetVisorBlendLevel, attachParentRelativePitchLimitsToConsider, vSwitchHelmetVisorPitchLimits);

			float fScriptedPitchMinClamped = Clamp(m_Metadata.m_AttachParentRelativePitchForScriptedVehAnim.x, m_Metadata.m_AttachParentRelativePitch.x, m_Metadata.m_AttachParentRelativePitch.y);
			float fScriptedPitchMaxClamped = Clamp(m_Metadata.m_AttachParentRelativePitchForScriptedVehAnim.y, m_Metadata.m_AttachParentRelativePitch.x, m_Metadata.m_AttachParentRelativePitch.y);
			attachParentRelativePitchLimitsToConsider.Lerp(fScriptedVehAnimBlendLevel, attachParentRelativePitchLimitsToConsider, Vector2(fScriptedPitchMinClamped,fScriptedPitchMaxClamped));

			//Blend out the ability to look down as the camera looks backwards.
			m_FinalPitchLimits.Set(attachParentRelativePitchLimitsToConsider.x * minPitchScaling * DtoR, attachParentRelativePitchLimitsToConsider.y * DtoR);

			m_RelativePitch			= Clamp(m_RelativePitch, m_FinalPitchLimits.x - pitchOffsetToApply, m_FinalPitchLimits.y - pitchOffsetToApply);

			pitchOffsetToApply		= Clamp(m_RelativePitch + pitchOffsetToApply, m_FinalPitchLimits.x, m_FinalPitchLimits.y) - m_RelativePitch;
		}

		//Blend in additional pitch offset for acceleration movement.
		const float accelerationBlendLevel				= m_AccelerationMovementSpring.GetResult();
		const float relativePitchOffsetForAcceleration	= Lerp(accelerationBlendLevel, 0.0f, m_Metadata.m_AccelerationMovementSettings.m_MaxPitchOffset * DtoR);

		lookAtTransformRelativeToAttachMatrix.RotateLocalX(m_RelativePitch + relativePitchOffsetForAcceleration + pitchOffsetToApply);

		//Apply roll when looking behind, blended in from +/-[90->m_Metadata.m_RelativeHeadingOffsetsForLookBehind] degrees (whether or not due to look-behind.)
		//Only done for pov cameras, but not turret seat cameras.
		if(IsFirstPersonCamera() && !m_AttachedToTurret && seatShufflingBlendLevel < SMALL_FLOAT)
		{
			float desiredRelativeHeadingOffsetForLookBehind = DtoR * (m_ShouldRotateRightForLookBehind ? m_Metadata.m_RelativeHeadingOffsetsForLookBehind.x :
																										 m_Metadata.m_RelativeHeadingOffsetsForLookBehind.y);
			float relativeRollToApply = RampValueSafe(Abs(m_RelativeHeadingWithLookBehind), HALF_PI, Abs(desiredRelativeHeadingOffsetForLookBehind),
																							0.0f, m_Metadata.m_MaxRollForLookBehind * DtoR);
			relativeRollToApply *= Sign(m_RelativeHeadingWithLookBehind);
			lookAtTransformRelativeToAttachMatrix.RotateLocalY(relativeRollToApply);
		}

		m_PreviousVehicleHeading	= fCurrentVehicleHeading;
		m_PreviousVehiclePitch		= fCurrentVehiclePitch;
	}

	Matrix34 lookAtMatrix(attachMatrix);
	if(m_SpringMount)
	{
		//Apply the spring mount.
		m_SpringMount->Update(lookAtMatrix, attachPhysical);
	}

	lookAtMatrix.Dot3x3FromLeft(lookAtTransformRelativeToAttachMatrix);

	if(m_Metadata.m_ShouldApplyAttachParentRoll)
	{
		//roll
		Matrix34 CameraRelativeAttachMat;
		CameraRelativeAttachMat.DotTranspose(attachMatrix, lookAtMatrix);

		const float desiredRoll	= camFrame::ComputeRollFromMatrix(CameraRelativeAttachMat);

		float heading; 
		float pitch; 
		camFrame::ComputeHeadingAndPitchFromMatrix(lookAtMatrix, heading, pitch); 

		camFrame::ComputeWorldMatrixFromHeadingPitchAndRoll(heading, pitch,desiredRoll, lookAtMatrix); 
	}
	
	if(m_Metadata.m_ShouldTerminateForWorldPitch)
	{
		float heading;
		float pitch; 
		float roll; 

		camFrame::ComputeHeadingPitchAndRollFromMatrix(lookAtMatrix, heading, pitch, roll); 

		float pitchDegrees = pitch * RtoD; 
		
		if(m_NumUpdatesPerformed == 0 )
		{
			if(pitchDegrees < m_Metadata.m_InitialWorldPitchLimits.x || pitchDegrees > m_Metadata.m_InitialWorldPitchLimits.y )
			{
				m_canUpdate = false; 
			}
		}
		else
		{
			if(pitchDegrees < m_Metadata.m_WorldPitchLimits.x || pitchDegrees > m_Metadata.m_WorldPitchLimits.y )
			{
				m_canUpdate = false; 
			}
		}
	}


	m_Frame.SetWorldMatrix(lookAtMatrix);

	//if we're unarmed aiming, turn off look behind
	if (m_UnarmedDrivebyBlendLevel >= SMALL_FLOAT)
	{
		if (m_ControlHelper)
		{
			m_ControlHelper->SetLookBehindState(false);
		}
		if (m_LookBehindEnvelope)
		{
			m_LookBehindEnvelope->Stop();
		}
	}

	return m_RelativeHeadingWithLookBehind;
}

bool camCinematicMountedCamera::ComputeIdealLookAtPosition(const Matrix34& attachMatrix, rage::Vector3& lookAtPosition) const
{
	const CPed* followPed = camInterface::FindFollowPed();
	if(m_LookAtTarget && m_LookAtTarget->GetIsTypePed())
	{
		followPed = static_cast<const CPed*>(m_LookAtTarget.Get()); 
	}

	eCinematicMountedCameraLookAtBehaviour LookAtBehaviour = m_Metadata.m_LookAtBehaviour; 
	lookAtPosition = Vector3(0.0f,0.0f,0.0f);
	bool validLookAtPosition = false;

	if(LookAtBehaviour != LOOK_FORWARD_RELATIVE_TO_ATTACH)
	{
		validLookAtPosition = true;

		if(followPed && (LookAtBehaviour == LOOK_AT_FOLLOW_PED_RELATIVE_POSITION))
		{
			//Orient the camera to look at an offset relative to the follow-ped.
			if(m_Metadata.m_FollowPedLookAtBoneTag >= 0) //A negative bone tag indicates that the root ped position should be used.
			{
				Matrix34 boneMatrix;
				followPed->GetBoneMatrix(boneMatrix, (eAnimBoneTag)m_Metadata.m_FollowPedLookAtBoneTag);
				lookAtPosition = boneMatrix.d;
			}
			else
			{
				lookAtPosition = VEC3V_TO_VECTOR3(followPed->GetTransform().GetPosition());
			}

			Vector3 baseLookAtPositionForRagdoll; 

			followPed->CalcCameraAttachPositionForRagdoll(baseLookAtPositionForRagdoll);

			lookAtPosition.Lerp(m_RagdollBlendLevel, baseLookAtPositionForRagdoll);

			//Transform to a world-space offset.
			lookAtPosition += VEC3V_TO_VECTOR3(followPed->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(m_Metadata.m_RelativeLookAtPosition)));
		}
		else
		{
			//Orient the camera to look at an offset relative to the attach parent.
			attachMatrix.Transform(m_Metadata.m_RelativeLookAtPosition, lookAtPosition); //Transform to world-space.
		}

		//push the look at position dependent on the direction of travel
		if (m_LookAtTarget && m_LookAtTarget->GetIsTypeVehicle() && m_CanUseLookAheadHelper && m_InVehicleLookAheadHelper)
		{
			Vector3 lookAheadPositionOffset = VEC3_ZERO;
			const CPhysical* lookAtTargetPhysical = static_cast<const CPhysical*>(m_LookAtTarget.Get());
			m_InVehicleLookAheadHelper->UpdateConsideringLookAtTarget(lookAtTargetPhysical, m_NumUpdatesPerformed == 0, lookAheadPositionOffset);
			lookAtPosition += lookAheadPositionOffset;
		}
	}

	return validLookAtPosition;
}

bool camCinematicMountedCamera::RestoreCameraOrientation(const Matrix34& attachMatrix, float& relativeHeading, float& relativePitch) const
{
	relativeHeading = 0.0f;
	relativePitch = 0.0f;

	const CPed* followPed = camInterface::FindFollowPed();
	if(m_LookAtTarget && m_LookAtTarget->GetIsTypePed())
	{
		followPed = static_cast<const CPed*>(m_LookAtTarget.Get()); 
	}

	const CVehicle* attachVehicle = (m_AttachParent && m_AttachParent->GetIsTypeVehicle()) ? static_cast<const CVehicle*>(m_AttachParent.Get()) : NULL;

	if(!m_PreviousWasInVehicleCinematicCamera && m_NumUpdatesPerformed == 0 &&
		followPed && attachVehicle && attachVehicle->GetVehicleModelInfo())
	{
		bool bIsInAircraftBackSeat = false;
		bool bIsFacingBackwards = false;
		s32 attachSeatIndex = followPed->GetAttachCarSeatIndex();
		const CModelSeatInfo* pModelSeatInfo = attachVehicle->GetVehicleModelInfo()->GetModelSeatInfo();
		if(attachSeatIndex >= 0 && pModelSeatInfo)
		{
			const s32 iSeatBoneIndex = pModelSeatInfo->GetBoneIndexFromSeat( attachSeatIndex );
			if(iSeatBoneIndex > -1) 
			{
				if(attachVehicle->GetIsAircraft() && iSeatBoneIndex > 1)
				{
					bIsInAircraftBackSeat = true;
				}
				else
				{
					const Matrix34& localSeatMtx = attachVehicle->GetLocalMtx(iSeatBoneIndex);

					float relativeSeatHeading = camFrame::ComputeHeadingFromMatrix(localSeatMtx);
					const float minAngleToConsiderFacingBackward = PI - (m_Metadata.m_LeadingLookSettings.m_MaxAngleToConsiderFacingForward * DtoR);
					bIsFacingBackwards = Abs(relativeSeatHeading) > minAngleToConsiderFacingBackward;
				}
			}
		}

		if(!bIsInAircraftBackSeat && !bIsFacingBackwards)
		{
			const camGameplayDirector& gameplayDirector	= camInterface::GetGameplayDirector();
			const camBaseCamera* renderedGameplayCamera	= gameplayDirector.GetRenderedCamera();

			const s32 vehicleEntryExitStateOnPreviousUpdate = gameplayDirector.GetVehicleEntryExitStateOnPreviousUpdate();
			if((vehicleEntryExitStateOnPreviousUpdate == camGameplayDirector::INSIDE_VEHICLE) && !gameplayDirector.ShouldDelayVehiclePovCameraOnPreviousUpdate())
			{
				const Vector3& gameplayCameraFront	= (renderedGameplayCamera != NULL) ? renderedGameplayCamera->GetBaseFront() :
					gameplayDirector.GetFrameNoPostEffects().GetFront();

				Matrix34 lookAtTransformRelativeToAttachMatrix(Matrix34::IdentityType);
				if(m_Metadata.m_LookAtBehaviour != LOOK_FORWARD_RELATIVE_TO_ATTACH)
				{
					Vector3 idealLookAtPosition;
					if(ComputeIdealLookAtPosition(attachMatrix, idealLookAtPosition))
					{
						const Vector3& cameraPosition	= m_Frame.GetPosition();
						Vector3 idealLookAtFront		= idealLookAtPosition - cameraPosition;
						idealLookAtFront.NormalizeSafe();

						camFrame::ComputeWorldMatrixFromFront(idealLookAtFront, lookAtTransformRelativeToAttachMatrix);
						lookAtTransformRelativeToAttachMatrix.Dot3x3Transpose(attachMatrix);
					}
				}

				Matrix34 lookAtMatrix(attachMatrix);
				lookAtMatrix.Dot3x3FromLeft(lookAtTransformRelativeToAttachMatrix);

				Vector3 lookAtRelativeGameplayCameraFront;
				lookAtMatrix.UnTransform3x3(gameplayCameraFront, lookAtRelativeGameplayCameraFront);

				camFrame::ComputeHeadingAndPitchFromFront(lookAtRelativeGameplayCameraFront, relativeHeading, relativePitch);
				return true;
			}
		}
	}

	return false;
}

bool camCinematicMountedCamera::IsAttachVehicleOnTheGround(const CVehicle& attachVehicle) const
{
	const int numWheels = attachVehicle.GetNumWheels();
	bool anyWheelTouching = false;
	for (int i = 0; i < numWheels; i++)
	{
		const CWheel* wheel = attachVehicle.GetWheel(i);
		anyWheelTouching |= wheel && wheel->GetIsTouching();
	}
	return anyWheelTouching;
}

void camCinematicMountedCamera::UpdateShakeAmplitudeScaling()
{
	const bool allowHeadBobbing	= (CPauseMenu::GetMenuPreference(PREF_FPS_HEADBOB) == TRUE);

	m_ShakeAmplitudeScalar		= allowHeadBobbing ? (1.0f - m_DriveByBlendLevel) : 0.0f;
	
	//we turn off all shakes only if head bobbing is turned off, not if the scalar is 0
	if (!allowHeadBobbing)
	{
		StopShaking();
	}
}

void camCinematicMountedCamera::UpdateIdleShake()
{
	camBaseFrameShaker* frameShaker	= NULL;
	const u32 idleShakeHash			= m_Metadata.m_IdleShakeRef.GetHash();
	if (idleShakeHash)
	{
		frameShaker = FindFrameShaker(idleShakeHash);
		if(!frameShaker)
		{
			frameShaker = Shake(idleShakeHash, m_ShakeAmplitudeScalar);
		}
	}

	if (frameShaker)
	{
		frameShaker->SetAmplitude(m_ShakeAmplitudeScalar);
	}
}

void camCinematicMountedCamera::UpdateSpeedRelativeShake(const CPhysical& parentPhysical, const camSpeedRelativeShakeSettingsMetadata& settings, camDampedSpring& speedSpring)
{
	if (m_ShakeAmplitudeScalar < SMALL_FLOAT)
	{
		return;
	}

	const u32 shakeHash = settings.m_ShakeRef.GetHash();
	if(shakeHash == 0)
	{
		//No shake was specified in metadata.
		return;
	}

	camBaseFrameShaker* frameShaker = FindFrameShaker(shakeHash);
	if(!frameShaker)
	{
		//Try to kick-off the specified shake.
		frameShaker = Shake(shakeHash, 0.0f);
		if(!frameShaker)
		{
			return;
		}
	}

	//Consider the forward and backward speed only.
	const Vector3 parentFront		= VEC3V_TO_VECTOR3(parentPhysical.GetTransform().GetB());
	const Vector3& parentVelocity	= parentPhysical.GetVelocity();
	float forwardBackwardSpeed		= DotProduct(parentVelocity, parentFront);
	forwardBackwardSpeed			= Abs(forwardBackwardSpeed);
	const float desiredSpeedFactor	= SlowInOut(RampValueSafe(forwardBackwardSpeed, settings.m_MinForwardSpeed, settings.m_MaxForwardSpeed, 0.0f, settings.m_MaxShakeAmplitude));

	const float springConstant		= (m_NumUpdatesPerformed == 0) ? 0.0f : settings.m_SpringConstant;
	float speedFactorToApply		= speedSpring.Update(desiredSpeedFactor, springConstant);
	speedFactorToApply				= Clamp(speedFactorToApply, 0.0f, settings.m_MaxShakeAmplitude); //For safety.

	const float shakeAmplitude		= speedFactorToApply * m_ShakeAmplitudeScalar;

	frameShaker->SetAmplitude(shakeAmplitude);
}

void camCinematicMountedCamera::UpdateRocketSettings()
{
	if(m_AttachParent->GetIsTypeVehicle())
	{
		const CVehicle* attachParentVehicle = static_cast<const CVehicle*>(m_AttachParent.Get());

		const bool isRocketBoosting = attachParentVehicle->IsRocketBoosting();
		if(isRocketBoosting)
		{
			//frame shaker
			if(m_Metadata.m_RocketSettings.m_ShakeRef.IsNotNull())
			{
				const u32 shakeHash = m_Metadata.m_RocketSettings.m_ShakeRef.GetHash();
				camBaseFrameShaker* frameShaker = FindFrameShaker(shakeHash);
				if(!frameShaker)
				{
					//Try to kick-off the specified shake.
					frameShaker = Shake(shakeHash, m_Metadata.m_RocketSettings.m_ShakeAmplitude);
				}
			}		
		}
	}
}

void camCinematicMountedCamera::LimitParentHeadingRelativeToAttachParent(float& relativeHeading) const
{
	if(m_ShouldLimitAttachParentRelativePitchAndHeading)
	{
		Vector2 headingLimitsConsideringSide = (m_IsSeatPositionOnLeftHandSide) ? m_AttachParentRelativeHeadingLimits :
														Vector2(-m_AttachParentRelativeHeadingLimits.y, -m_AttachParentRelativeHeadingLimits.x);
		headingLimitsConsideringSide.Scale(DtoR);

		// If outside of range, have to clamp angle.
		if(relativeHeading < headingLimitsConsideringSide.x || relativeHeading > headingLimitsConsideringSide.y)
		{
			// Calculate difference from angle to both limits and clamp to limit with smallest difference.
			float lowerLimitDiff = Abs(fwAngle::LimitRadianAngle(relativeHeading - headingLimitsConsideringSide.x));
			float upperLimitDiff = Abs(fwAngle::LimitRadianAngle(relativeHeading - headingLimitsConsideringSide.y));
			relativeHeading = (lowerLimitDiff < upperLimitDiff) ? headingLimitsConsideringSide.x : headingLimitsConsideringSide.y;
		}
	}
	else
	{
		Assert(false);
	}
}

void camCinematicMountedCamera::LimitRelativeHeading(float& relativeHeading) const
{
	//NOTE: We always wrap the heading into a valid -PI to PI range.
	relativeHeading = fwAngle::LimitRadianAngleSafe(relativeHeading);

	//Apply custom relative heading limits if the mobile phone camera is rendering, as it uses this camera type to achieve reasonable first-person
	//framing in vehicles.
	const bool isRenderingMobilePhoneCamera = CPhoneMgr::CamGetState();
	if(!isRenderingMobilePhoneCamera)
	{
		return;
	}

	const float thresholdForLimiting = 180.0f - SMALL_FLOAT;
	if((m_Metadata.m_RelativeHeadingLimitsForMobilePhoneCamera.x >= -thresholdForLimiting) ||
		(m_Metadata.m_RelativeHeadingLimitsForMobilePhoneCamera.y <= thresholdForLimiting))
	{
		relativeHeading	= Clamp(relativeHeading, m_Metadata.m_RelativeHeadingLimitsForMobilePhoneCamera.x * DtoR,
							m_Metadata.m_RelativeHeadingLimitsForMobilePhoneCamera.y * DtoR);
	}
}

void camCinematicMountedCamera::UpdateFov()
{
	//Use the zoom control if the mobile phone camera is rendering, as it uses this camera type to achieve reasonable first-person framing
	//in vehicles.
	const bool isRenderingMobilePhoneCamera = CPhoneMgr::CamGetState();
	if(isRenderingMobilePhoneCamera)
	{
		if(m_MobilePhoneCameraControlHelper)
		{
			m_Fov = m_MobilePhoneCameraControlHelper->GetZoomFov();
		}
	}
	else
	{
		const s32 profileSettingFov = CPauseMenu::GetMenuPreference(PREF_FPS_FIELD_OF_VIEW);
		const float fFovSlider = Clamp((float)profileSettingFov / (float)DEFAULT_SLIDER_MAX, 0.0f, 1.0f);		
		TUNE_BOOL(CAM_FIRST_PERSON_VEHICLE_HFOV, false);
		if(CAM_FIRST_PERSON_VEHICLE_HFOV)
		{
			const float baseHFov = 2.0f * atan(Tanf(0.5f * m_Metadata.m_BaseFov * DtoR) * 16.0f/9.0f) * RtoD;
			const float hFovSetting = Lerp(fFovSlider, baseHFov, 110.0f);

			const float sceneWidth	= static_cast<float>(VideoResManager::GetSceneWidth());
			const float sceneHeight	= static_cast<float>(VideoResManager::GetSceneHeight());
			const float viewportAspect = sceneWidth / sceneHeight;

			const float baseAspect = 16.0f / 9.0f;

			const float hFov2	= atan(Tanf(0.5f * hFovSetting * DtoR) * (viewportAspect / baseAspect)); // removed unnecessary 2x and DtoR/RtoD
			const float vFov	= 2.0f * atan(Tanf(hFov2) / viewportAspect) * RtoD;

			m_Fov = vFov;
		}
		else
		{
			m_Fov = m_Metadata.m_BaseFov;
		}

		//Blend in an additional FoV offset for drive by aiming.
		float fOffsetForDriveby = m_Metadata.m_FoVOffsetForDriveby * m_DriveByBlendLevel;
		m_Fov += fOffsetForDriveby;
	}

	//Blend in additional fov scaling for acceleration movement.
	if (m_Metadata.m_AccelerationMovementSettings.m_ShouldMoveOnVehicleAcceleration)
	{
		const float accelerationBlendLevel	= Max(m_AccelerationMovementSpring.GetResult(), 0.0f); //don't apply this effect when reversing
		const float accelerationZoomFactor	= Powf(m_Metadata.m_AccelerationMovementSettings.m_MaxZoomFactor, (1.0f - accelerationBlendLevel));
		if (accelerationZoomFactor >= SMALL_FLOAT)
		{
			m_Fov /= accelerationZoomFactor;
		}
	}

	m_Frame.SetFov(m_Fov);

	UpdateCustomFovBlend(m_Fov);
}

void camCinematicMountedCamera::UpdateNearClip(bool WIN32PC_ONLY(bUsingTripleHead), float WIN32PC_ONLY(aspectRatio))
{
	// This should be called before UpdateFirstPersonAnimatedData, in case it needs to be overridden.
	float fNearClip = m_Metadata.m_BaseNearClip;
#if RSG_PC
	if(bUsingTripleHead)
	{
		if(m_Metadata.m_TripleHeadNearClip > SMALL_FLOAT)
		{
			fNearClip = m_Metadata.m_TripleHeadNearClip;
			const float c_TripleHead_16to9 = (16.0f / 3.0f) + SMALL_FLOAT;
			if(aspectRatio > c_TripleHead_16to9)
			{
				// If aspect ratio is bigger than 16:9 triple head, use minimum near clip. (i.e. 3 21:9 monitors)
				fNearClip = Min(m_Metadata.m_TripleHeadNearClip, 0.05f);
			}
		}

		const CPed* followPed = camInterface::FindFollowPed();
		if(followPed && followPed->GetPedResetFlag(CPED_RESET_FLAG_PuttingOnHelmet))
		{
			// Force minimum near clip when putting on a helmet in triplehead to avoid having the player's arms clip the camera.
			fNearClip = Min(fNearClip, 0.05f);
			fNearClip = Lerp(s_fPutOnHelmentRate, m_Frame.GetNearClip(), fNearClip);
		}
	}
	else
#endif
	{
		const CVehicle* attachVehicle = (m_AttachParent.Get() && m_AttachParent->GetIsTypeVehicle()) ? static_cast<const CVehicle*>(m_AttachParent.Get()) : NULL;
		if(attachVehicle && attachVehicle->GetIsAircraft())
		{
			const CPed* pFollowPed = camInterface::FindFollowPed();
			const CSeatManager* pSeatManager = attachVehicle->GetSeatManager();
			s32 iSeatIndex = (pSeatManager && pFollowPed) ? pSeatManager->GetPedsSeatIndex(pFollowPed) : -1;
			if(iSeatIndex > 1 && attachVehicle->IsSeatIndexValid(iSeatIndex))
			{
				// Reduce near clip when in the rear passenger seats, since the same pov camera is being used.
				fNearClip = Min(fNearClip, 0.10f);
			}
		}
	}

	const float seatShufflingBlendLevel = m_SeatSwitchingLimitSpring.GetResult();
	if(IsVehicleTurretCamera() && seatShufflingBlendLevel > SHUFFLE_FROM_TURRET_VISIBLE_THRESHOLD)
	{
		TUNE_GROUP_FLOAT(VEHICLE_POV_CAMERA, SHUFFLE_FROM_TURRET_NEAR_CLIP, 0.05f, 0.05f, 0.35f, 0.001f);
		fNearClip = Min(fNearClip, SHUFFLE_FROM_TURRET_NEAR_CLIP);
	}

	m_Frame.SetNearClip(fNearClip);
}

bool camCinematicMountedCamera::UpdateOcclusion(const Vector3& cameraPosition, const Vector3& lookAtPosition)
{
	bool haveLineOfSightToLookAt = true; 
	bool hasHitAttachedParent = false; 

	const bool hasHit = ComputeOcclusion(cameraPosition, lookAtPosition, hasHitAttachedParent); 

	if(m_NumUpdatesPerformed == 0)
	{
		if(hasHit)
		{
			haveLineOfSightToLookAt = false; 
			return false; 
		}
	}

	if(hasHit)
	{
		m_ShotTimeSpentOccluded += fwTimer::GetCamTimeInMilliseconds() - fwTimer::GetCamPrevElapsedTimeInMilliseconds();  
	}
	else
	{
		m_ShotTimeSpentOccluded = 0;
	}

	if(m_Metadata.m_ShouldTerminateIfOccludedByAttachParent && hasHitAttachedParent)
	{
		if(m_ShotTimeSpentOccluded > m_Metadata.m_MaxTimeToSpendOccludedByAttachParent)
		{
			haveLineOfSightToLookAt = false; 
		}
	}

	if(m_ShotTimeSpentOccluded > m_Metadata.m_MaxTimeToSpendOccluded)
	{
		haveLineOfSightToLookAt = false;
	}

	return haveLineOfSightToLookAt; 
}



bool camCinematicMountedCamera::ComputeOcclusion(const Vector3& cameraPosition, const Vector3& lookAtPosition, bool &HasHitAttachedParent)
{
	bool hasHit = false; 

	WorldProbe::CShapeTestProbeDesc occluderCapsuleTest;
	WorldProbe::CShapeTestFixedResults<> occluderShapeTestResults;
	occluderCapsuleTest.SetResultsStructure(&occluderShapeTestResults);

	occluderCapsuleTest.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER |  ArchetypeFlags::GTA_VEHICLE_NON_BVH_TYPE );
	occluderCapsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST); 
	occluderCapsuleTest.SetContext(WorldProbe::LOS_Camera);
	occluderCapsuleTest.SetIsDirected(true); 

	const CPed* followPed = camInterface::FindFollowPed();

	if(m_LookAtTarget && m_LookAtTarget->GetIsTypePed())
	{
		followPed = static_cast<const CPed*>(m_LookAtTarget.Get()); 
	}

	eCinematicMountedCameraLookAtBehaviour LookAtBehaviour; 

	if(m_ShouldOverrideLookAtBehaviour)
	{
		LookAtBehaviour = m_OverriddenLookAtBehaviour;
	}
	else
	{
		LookAtBehaviour = m_Metadata.m_LookAtBehaviour; 
	}

	if(followPed && (LookAtBehaviour == LOOK_AT_FOLLOW_PED_RELATIVE_POSITION))
	{
		CEntity* pFollowPedsVehicle = NULL; 

		if(m_LookAtTarget && m_LookAtTarget->GetIsTypePed())
		{
			const CPed* lookAtPed = static_cast<const CPed*>(m_LookAtTarget.Get()); 

			pFollowPedsVehicle = camCinematicCamManCamera::GetVehiclePedIsExitingEnteringOrIn(lookAtPed);  
		}
		
		if(pFollowPedsVehicle)
		{
			static const s32 iNumExceptions = 2;
			const CEntity* ppExceptions[iNumExceptions] = {followPed, pFollowPedsVehicle};

			occluderCapsuleTest.SetExcludeEntities(ppExceptions, iNumExceptions, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
		}
		else
		{
			occluderCapsuleTest.SetExcludeEntity(followPed, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS); 
		}
	}



	occluderCapsuleTest.SetStartAndEnd(cameraPosition, lookAtPosition);
	hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(occluderCapsuleTest);
	
	if(occluderShapeTestResults.GetNumHits() > 0)
	{
		const CEntity* pEnt = occluderShapeTestResults[0].GetHitEntity(); 
		
		if(pEnt && pEnt == m_AttachParent)
		{
			HasHitAttachedParent = true; 
		}
	}
	
	return hasHit; 
}

bool camCinematicMountedCamera::ComputeShouldTerminateForWaterClipping(const Vector3& position)
{
	if(m_Metadata.m_DisableWaterClippingTest || camInterface::GetCinematicDirector().IsScriptDisablingWaterClippingTest())
	{
		return false;
	}

	float waterHeight;
	bool isInOcean = false;
	bool isInOceanOrRiver = camCollision::ComputeIsOverOceanOrRiver(position, m_Metadata.m_MaxDistanceForWaterClippingTest,  m_Metadata.m_MaxDistanceForRiverWaterClippingTest, waterHeight, isInOcean);

	if(m_Metadata.m_ForceInvalidateWhenInOcean)
	{
		TUNE_GROUP_INT(VEHICLE_POV_CAMERA, iTimerToSwitchOutOfWater, 1500, 0, 60000, 1);
		const u32 lastTimeInWater = camInterface::GetGameplayDirector().GetLastTimeAttachParentWasInWater();
		if(isInOcean || (fwTimer::GetCamTimeInMilliseconds() - lastTimeInWater) <= (u32)iTimerToSwitchOutOfWater)
		{
			return true;
		}
	}

	if(isInOceanOrRiver)
	{
		const float heightDelta			= m_Metadata.m_ShouldConsiderMinHeightAboveOrBelowWater ? Abs(position.z - waterHeight) : (position.z - waterHeight);
		const float minHeightAboveWater	= m_Metadata.m_ShouldUseDifferentMinHeightAboveWaterOnFirstUpdate && (m_NumUpdatesPerformed == 0) ?
			m_Metadata.m_MinHeightAboveWaterOnFirstUpdate : m_Metadata.m_MinHeightAboveWater;
		if (heightDelta < minHeightAboveWater)
		{
			//Ensure zero tolerance of clipping into dynamic collision now.
			m_TimeSpentClippingIntoDynamicCollision = m_Metadata.m_MaxTimeToClipIntoDynamicCollision;
			return true;
		}
	}

	return false;
}

bool camCinematicMountedCamera::ComputeShouldTerminateForClipping(const Vector3& position, const Vector3& attachPosition)
{
	const u32 fullIncludeFlags = ((ArchetypeFlags::GTA_ALL_TYPES_WEAPON & ~ArchetypeFlags::GTA_PED_TYPE) | ArchetypeFlags::GTA_RAGDOLL_TYPE);

	bool shouldTerminateForClipping;
	if(m_Metadata.m_MaxTimeToClipIntoDynamicCollision == 0)
	{
		//Test against our full list of collision types at once.
		shouldTerminateForClipping = IsClipping(position, attachPosition, fullIncludeFlags, TYPE_FLAGS_NONE);
	}
	else
	{
		//Test against static map/vehicle and dynamic collision separately.

		//First, test against static map collision and vehicles (excluding bikes and quad bikes.)
		const u32 staticIncludeFlags	= (ArchetypeFlags::GTA_MAP_TYPE_WEAPON | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_BOX_VEHICLE_TYPE | ArchetypeFlags::GTA_RAGDOLL_TYPE);
		//Ignore any collision bounds that are flagged as both map type weapon and object type, as they may be dynamic.
		const u32 staticExcludeFlags	= ArchetypeFlags::GTA_OBJECT_TYPE;
		shouldTerminateForClipping		= IsClipping(position, attachPosition, staticIncludeFlags, staticExcludeFlags, true);
		if(shouldTerminateForClipping)
		{
			//Ensure zero tolerance of clipping into dynamic collision now.
			m_TimeSpentClippingIntoDynamicCollision = m_Metadata.m_MaxTimeToClipIntoDynamicCollision;
		}
		else
		{
			//Now test against dynamic collision and see if we have been clipping for long enough to report it.
			const u32 dynamicIncludeFlags			= (fullIncludeFlags &~ ArchetypeFlags::GTA_MAP_TYPE_WEAPON);
			const bool isClippingDynamicCollision	= IsClipping(position, attachPosition, dynamicIncludeFlags, 0);
			if(isClippingDynamicCollision)
			{
				m_TimeSpentClippingIntoDynamicCollision += fwTimer::GetCamTimeStepInMilliseconds();

				shouldTerminateForClipping = (m_TimeSpentClippingIntoDynamicCollision > m_Metadata.m_MaxTimeToClipIntoDynamicCollision);
			}
			else
			{
				m_TimeSpentClippingIntoDynamicCollision = 0;
			}
		}
	}

	return shouldTerminateForClipping; 
}

bool camCinematicMountedCamera::IsClipping(const Vector3& position, const Vector3& attachPosition, u32 includeFlags, u32 excludeFlags, bool shouldExcludeBikes) const
{
	//First perform an undirected line test between the root position of the attach parent and the camera position, to ensure that the camera
	//is not completely penetrating a one-sided collision mesh.
	//NOTE: We must explicitly ignore river and 'deep surface' bounds for this test, as they can clip inside the collision bounds of the attach parent. Likewise we consider mover bounds,
	//rather than weapon bounds, as the attach parent can clip into deep surfaces that are also flagged as weapon-type collision.
	u32 includeFlagsForLineTest	= (includeFlags | ArchetypeFlags::GTA_MAP_TYPE_MOVER) & ~(ArchetypeFlags::GTA_RIVER_TYPE | ArchetypeFlags::GTA_PED_TYPE | ArchetypeFlags::GTA_RAGDOLL_TYPE |
									ArchetypeFlags::GTA_DEEP_SURFACE_TYPE | ArchetypeFlags::GTA_MAP_TYPE_WEAPON);
	// - WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS otherwise allows peds to be detected if their bounds are flagged to include
	// ArchetypeFlags::GTA_CAMERA_TEST, which is known to happen in some cases.

	WorldProbe::CShapeTestProbeDesc lineTest;
	WorldProbe::CShapeTestFixedResults<g_MaxShapeTestResultsForClippingTests> shapeTestResults;
	lineTest.SetResultsStructure(&shapeTestResults);
	lineTest.SetIsDirected(false);
	lineTest.SetIncludeFlags(includeFlagsForLineTest);
	lineTest.SetExcludeTypeFlags(excludeFlags);
	lineTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	lineTest.SetContext(WorldProbe::LOS_Camera);
	lineTest.SetExcludeEntity(m_AttachParent, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
	lineTest.SetTreatPolyhedralBoundsAsPrimitives(true);
	lineTest.SetOptions(WorldProbe::LOS_IGNORE_NO_CAM_COLLISION);

	Vector3 startPosition = m_Metadata.m_ShouldTestForMapPenetrationFromAttachPosition ? attachPosition : VEC3V_TO_VECTOR3(m_AttachParent->GetTransform().GetPosition());
	if(m_AttachParent->GetIsTypeVehicle())
	{
		const CBaseModelInfo* modelInfo = m_AttachParent->GetBaseModelInfo();
		if(modelInfo && modelInfo->GetModelNameHash() == ATSTRINGHASH("FELTZER3", 0xA29D6D10))
		{
			// HACK: vehicle position for Feltzer3 is at the bottom of the car, so easily get collision hits
			// when driving over obstacles/curbs. Raising start point slightly to avoid this.
			startPosition.z += 0.25f;
		}
	}
	lineTest.SetStartAndEnd(startPosition, position);

	bool hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(lineTest);
	
	if(hasHit)
	{
		hasHit = ComputeHasHitExcludingHeistCratesProp(shapeTestResults);
	}

	if(hasHit && shouldExcludeBikes)
	{
		hasHit = ComputeHasHitExcludingBikes(shapeTestResults);
	}

	if(!hasHit)
	{
		//Now perform a sphere test at the camera position.
		WorldProbe::CShapeTestSphereDesc sphereTest;
		sphereTest.SetResultsStructure(&shapeTestResults);
		sphereTest.SetIncludeFlags(includeFlags);
		sphereTest.SetExcludeTypeFlags(excludeFlags);
		sphereTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
		sphereTest.SetContext(WorldProbe::LOS_Camera);
		sphereTest.SetOptions(WorldProbe::LOS_IGNORE_NO_CAM_COLLISION);

		shapeTestResults.Reset();

		// - WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS otherwise allows this ped to be detected if their bounds are flagged to include
		// ArchetypeFlags::GTA_CAMERA_TEST, which is known to happen in some cases.
		const CPed* followPed = (m_LookAtTarget && m_LookAtTarget->GetIsTypePed()) ? static_cast<const CPed*>(m_LookAtTarget.Get()) : camInterface::FindFollowPed();
		if(followPed)
		{
			const CEntity* entitiesToExclude[2] = {m_AttachParent.Get(), followPed};
			sphereTest.SetExcludeEntities(entitiesToExclude, 2, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
		}
		else
		{
			sphereTest.SetExcludeEntity(m_AttachParent, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
			//sphereTest.SetExcludeEntity(m_AttachParent, WorldProbe::EIEO_DONT_ADD_VEHICLE);
		}

		sphereTest.SetSphere(position, m_Metadata.m_CollisionRadius * m_Metadata.m_RadiusScalingForClippingTest);
		sphereTest.SetTreatPolyhedralBoundsAsPrimitives(true);

		hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(sphereTest);

		if(hasHit)
		{
			hasHit = ComputeHasHitExcludingHeistCratesProp(shapeTestResults);
		}

		if(hasHit && shouldExcludeBikes)
		{
			hasHit = ComputeHasHitExcludingBikes(shapeTestResults);
		}
	}

	return hasHit;
}

bool camCinematicMountedCamera::ComputeHasHitExcludingHeistCratesProp(const WorldProbe::CShapeTestResults& shapeTestResults) const
{
	//Check if the shapetest results contain a hit against something other than the hei_prop_heist_tub_truck 
	bool hasHit = false;

	const u32 numHits = shapeTestResults.GetNumHits();
	for(int i=0; i<numHits; i++)
	{
		const phInst* hitInstance	= shapeTestResults[i].GetHitInst();
		const CEntity* hitEntity	= hitInstance ? CPhysics::GetEntityFromInst(hitInstance) : NULL;

		if(hitEntity && hitEntity->GetArchetype() && hitEntity->GetArchetype()->GetModelNameHash() == ATSTRINGHASH("hei_prop_heist_tub_truck", 0x5532a3e0))
		{
			continue;		
		}

		hasHit = true;
		break;
	}

	return hasHit;
}

bool camCinematicMountedCamera::ComputeHasHitExcludingBikes(const WorldProbe::CShapeTestResults& shapeTestResults) const
{
	//Check if the shapetest results contain a hit against something other than a bike (or quad bike.)

	bool hasHit = false;

	const u32 numHits = shapeTestResults.GetNumHits();
	for(int i=0; i<numHits; i++)
	{
		const phInst* hitInstance	= shapeTestResults[i].GetHitInst();
		const CEntity* hitEntity	= hitInstance ? CPhysics::GetEntityFromInst(hitInstance) : NULL;
		if(hitEntity && hitEntity->GetIsTypeVehicle())
		{
			const CVehicle* hitVehicle		= static_cast<const CVehicle*>(hitEntity);
			const bool shouldTreatAsBike	= (hitVehicle->InheritsFromBike() || hitVehicle->InheritsFromQuadBike() || hitVehicle->InheritsFromAmphibiousQuadBike());
			if(shouldTreatAsBike)
			{
				continue;
			}
		}

		hasHit = true;
		break;
	}

	return hasHit;
}

void camCinematicMountedCamera::UpdateDof()
{
	camBaseCamera::UpdateDof();

	const bool isRenderingMobilePhoneCamera	= CPhoneMgr::CamGetState();
	if(isRenderingMobilePhoneCamera)
	{
		const CControl* activeControl = camInterface::GetGameplayDirector().GetActiveControl();
		if(activeControl)
		{
			const bool isFocusLockActive = activeControl->GetCellphoneCameraFocusLock().IsDown();
			if(isFocusLockActive)
			{
				m_PostEffectFrame.GetFlags().SetFlag(camFrame::Flag_ShouldLockAdaptiveDofFocusDistance);
			}
		}
	}

#if RSG_PC
	const bool shouldDisableInGameDof = CPauseMenu::GetMenuPreference(PREF_GFX_DOF) == 0;
#else
	const bool shouldDisableInGameDof = (CProfileSettings::GetInstance().Exists(CProfileSettings::DISPLAY_DOF) && (CProfileSettings::GetInstance().GetInt(CProfileSettings::DISPLAY_DOF) == 0));
#endif // RSG_PC
	if((!isRenderingMobilePhoneCamera && shouldDisableInGameDof) || (isRenderingMobilePhoneCamera && !CPhoneMgr::GetDOFState()))
	{
		m_PostEffectFrame.SetDofSubjectMagPowerNearFar(Vector2(10.0f, 10.0f));
		m_PostEffectFrame.SetFNumberOfLens(256.0f);
		m_PostEffectFrame.SetDofMaxNearInFocusDistance(0.0f);
		m_PostEffectFrame.SetDofMaxNearInFocusDistanceBlendLevel(1.0f);
	}
}

void camCinematicMountedCamera::ComputeDofSettings(float overriddenFocusDistance, float overriddenFocusDistanceBlendLevel, camDepthOfFieldSettingsMetadata& settings) const
{
	camBaseCamera::ComputeDofSettings(overriddenFocusDistance, overriddenFocusDistanceBlendLevel, settings);

	if(!IsFirstPersonCamera() && overriddenFocusDistance == 0.0f)
	{
		// Modify dof behaviour for bonnet cameras.
		const CPed* pPed = camInterface::FindFollowPed();
		if(pPed)
		{
			Matrix34 boneMatrix;
			const bool isBoneValid = m_Metadata.m_ShouldUseCachedHeadBoneMatrix ? pPed->GetBoneMatrixCached(boneMatrix, BONETAG_HEAD) :
																				  pPed->GetBoneMatrix(boneMatrix, BONETAG_HEAD);
			if(isBoneValid)
			{
				const Vector3& cameraPosition			= m_PostEffectFrame.GetPosition();
				const Vector3& cameraForward			= m_PostEffectFrame.GetFront();
					  Vector3  cameraToTarget			= boneMatrix.d - cameraPosition;
				cameraToTarget.Normalize();				// bonnet camera will never be at player's head bone

				float fCosTargetToCamForward = cameraToTarget.Dot(cameraForward);
				if(fCosTargetToCamForward >= SMALL_FLOAT)
				{
					TUNE_GROUP_FLOAT(CAM_FPS, fSecondaryFocusParentToTargetBlendLevel, 0.375f, -1.0f, 1.0f, 0.001f);
					float revisedHyperfocalDistance = (fSecondaryFocusParentToTargetBlendLevel) / (1.0f - fSecondaryFocusParentToTargetBlendLevel);

					if(revisedHyperfocalDistance != 0.0f)
					{
						const float fov						= m_PostEffectFrame.GetFov();
						const float focalLengthOfLens		= settings.m_FocalLengthMultiplier * g_HeightOf35mmFullFrame / (2.0f * Tanf(0.5f * fov * DtoR));
						const float desiredFNumberOfLens	= square(focalLengthOfLens) / (revisedHyperfocalDistance * g_CircleOfConfusionFor35mm);

						// Use the cosine of the angle from camera to Target and camera forward as the interpolater.
						settings.m_FNumberOfLens			= camFrame::InterpolateFNumberOfLens(fCosTargetToCamForward, settings.m_FNumberOfLens, desiredFNumberOfLens);
					}
				}
			}
		}
	}
}

void camCinematicMountedCamera::BlendMatrix(float fInterp, const Vector3& vFinalPosition, const Quaternion& qFinalOrientation, float fFinalFov, float fFinalNearClip, bool bUseAnimatedCameraFov)
{
	Vector3 vCameraPosition = m_Frame.GetPosition();
	Quaternion qCameraOrientation;
	qCameraOrientation.FromMatrix34( m_Frame.GetWorldMatrix() );

	qCameraOrientation.PrepareSlerp(qFinalOrientation);
	vCameraPosition.Lerp(fInterp, vFinalPosition);
	qCameraOrientation.Slerp(fInterp, qFinalOrientation);

	Matrix34 worldMatrix;
	worldMatrix.FromQuaternion(qCameraOrientation);
	worldMatrix.d = vCameraPosition;

	float fNearClip = Lerp(fInterp, m_Frame.GetNearClip(), fFinalNearClip);

	m_Frame.SetWorldMatrix( worldMatrix, false );
	m_Frame.SetNearClip( fNearClip );

	if(bUseAnimatedCameraFov)
	{
		float fFov = Lerp(fInterp, m_Frame.GetFov(), fFinalFov);
		m_Frame.SetFov( fFov );
	}
}

void camCinematicMountedCamera::UpdateFirstPersonAnimatedData()
{
#if FPS_MODE_SUPPORTED
	TUNE_GROUP_BOOL(CAM_FPS, bPovCamAnimatedUseWeightForBlendIn, false);
	TUNE_GROUP_BOOL(CAM_FPS, bPovCamAnimatedUseWeightForBlendOut, false);
	TUNE_GROUP_BOOL(CAM_FPS, bPovCamAnimatedStartBlendOutWhenWeightLessThanOne, true);
	TUNE_GROUP_INT(CAM_FPS, nPovCamAnimatedDataBlendInTime, 250, 0, 10000, 100);
	TUNE_GROUP_INT(CAM_FPS, nPovCamAnimatedDataBlendOutTime, 750, 0, 10000, 100);

	if(!IsFirstPersonCamera())
	{
		m_UsingFirstPersonAnimatedData = false;
		m_WasUsingFirstPersonAnimatedData = false;
		m_BlendAnimatedStartTime = 0;
		return;
	}

	if(m_NumUpdatesPerformed > 0)
	{
		// This was set in constructor, so do not overwrite on first update.
		m_UsingFirstPersonAnimatedData = camInterface::GetGameplayDirector().IsFirstPersonUseAnimatedDataInVehicle();
	}

	camFirstPersonShooterCamera* pFpsCamera = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();
	if(pFpsCamera)
	{
		if(bPovCamAnimatedStartBlendOutWhenWeightLessThanOne && m_UsingFirstPersonAnimatedData)
		{
			m_UsingFirstPersonAnimatedData &= (pFpsCamera->GetAnimatedWeight() == 1.0f);
		}

		u32 uBlendTime = (m_UsingFirstPersonAnimatedData) ? nPovCamAnimatedDataBlendInTime : nPovCamAnimatedDataBlendOutTime;
		if(m_BlendAnimatedStartTime == 0)
		{
			if(m_UsingFirstPersonAnimatedData && !m_WasUsingFirstPersonAnimatedData)
			{
                //special case for the blend in, we use the start time given from the GameplayDirector 
                //which is the start time of the animated data playback.
				m_BlendAnimatedStartTime = Max<u32>(fwTimer::GetTimeInMilliseconds(), camInterface::GetGameplayDirector().GetFirstPersonAnimatedDataStartTime());
				if(pFpsCamera->GetAnimatedCameraBlendInDuration() >= 0)
				{
					uBlendTime = pFpsCamera->GetAnimatedCameraBlendInDuration();
				}

				if(fwTimer::GetTimeInMilliseconds() > (m_BlendAnimatedStartTime + uBlendTime))
				{
					m_BlendFovDeltaStartTime = fwTimer::GetTimeInMilliseconds();
				}
			}
			else if (!m_UsingFirstPersonAnimatedData && m_WasUsingFirstPersonAnimatedData)
			{
				m_BlendAnimatedStartTime = fwTimer::GetTimeInMilliseconds();
				if(pFpsCamera->GetAnimatedCameraBlendOutDuration() >= 0)
				{
					uBlendTime = pFpsCamera->GetAnimatedCameraBlendInDuration();
				}
			}
		}

		if(m_UsingFirstPersonAnimatedData)
		{
			if(!bPovCamAnimatedUseWeightForBlendIn && fwTimer::GetTimeInMilliseconds() > (m_BlendAnimatedStartTime + uBlendTime))
			{
				m_BlendAnimatedStartTime = 0;
			}

			if(m_BlendAnimatedStartTime == 0)
			{
				m_Frame.SetWorldMatrix( pFpsCamera->GetFrame().GetWorldMatrix(), false );
				m_Frame.SetNearClip( pFpsCamera->GetFrame().GetNearClip() );
				if(pFpsCamera->IsAnimatedCameraFovSet())
				{
					m_Frame.SetFov( pFpsCamera->GetFrame().GetFov() );
				}
			}
			else
			{
				float fInterp;
				if(!bPovCamAnimatedUseWeightForBlendIn)
				{
					fInterp = (float)(fwTimer::GetTimeInMilliseconds() - m_BlendAnimatedStartTime) / (float)uBlendTime;
					fInterp = rage::SlowInOut(fInterp);
				}
				else
				{
					fInterp = Clamp(pFpsCamera->GetAnimatedWeight(), 0.0f, 1.0f);
					if(fInterp >= 1.0f)
					{
						m_BlendAnimatedStartTime = 0;
					}
				}
				Vector3 vFinalPosition = pFpsCamera->GetFrame().GetPosition();
				Quaternion qFinalOrientation;
				qFinalOrientation.FromMatrix34( pFpsCamera->GetFrame().GetWorldMatrix() );

				BlendMatrix(fInterp, vFinalPosition, qFinalOrientation, pFpsCamera->GetFrame().GetFov(), pFpsCamera->GetFrame().GetNearClip(), pFpsCamera->IsAnimatedCameraFovSet());
			}
		}
		else if(m_WasUsingFirstPersonAnimatedData)
		{
			const u32 duration = uBlendTime;
			if(!bPovCamAnimatedUseWeightForBlendOut && fwTimer::GetTimeInMilliseconds() > (m_BlendAnimatedStartTime + duration))
			{
				m_WasUsingFirstPersonAnimatedData = false;
			}
			else
			{
				float fInterp;
				if(!bPovCamAnimatedUseWeightForBlendOut)
				{
					fInterp = ((float)(m_BlendAnimatedStartTime + duration - fwTimer::GetTimeInMilliseconds()) / (float)duration);
					fInterp = rage::SlowInOut(fInterp);
				}
				else
				{
					fInterp = Clamp(pFpsCamera->GetAnimatedWeight(), 0.0f, 1.0f);
					if(fInterp >= 1.0f)
					{
						m_WasUsingFirstPersonAnimatedData = false;
						m_BlendAnimatedStartTime = 0;
					}
				}
				Vector3 vFinalPosition = pFpsCamera->GetFrame().GetPosition();
				Quaternion qFinalOrientation;
				qFinalOrientation.FromMatrix34( pFpsCamera->GetFrame().GetWorldMatrix() );

				BlendMatrix(fInterp, vFinalPosition, qFinalOrientation, pFpsCamera->GetFrame().GetFov(), pFpsCamera->GetFrame().GetNearClip(), pFpsCamera->IsAnimatedCameraFovSet());
			}
		}

		if(m_UsingFirstPersonAnimatedData)
		{
			m_WasUsingFirstPersonAnimatedData = true;			// Reset when interpolation ends.
		}
	}
	else if(m_NumUpdatesPerformed == 0 && m_UsingFirstPersonAnimatedData)
	{
		// First update with animated camera running, but first person shooter camera is not setup yet.
		// Duplicate the gameplay director frame, to delay the first person vehicle camera by 1 frame.
		m_Frame.CloneFrom( camInterface::GetGameplayDirector().GetFrame() );
	}
	else
	{
		// This flag was meant for prostitute interaction, to keep first person shooter camera active while blending
		// out from animated camera data.  If the camera is destroyed, then can't blend out so clear the flag.
		m_WasUsingFirstPersonAnimatedData = false;
		m_BlendAnimatedStartTime = 0;
	}
#endif
}


float camCinematicMountedCamera::LerpRelativeHeadingBetweenPeds(const CPed *pPlayer, const CPed *pArrestingPed, float fCamHeading)
{
	float fRelativeHeading = 0.0f;

	if (pPlayer && pArrestingPed)
	{
		Vector3 vTargetPos = VEC3V_TO_VECTOR3(pArrestingPed->GetTransform().GetPosition());
		Vector3 vPedPos = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());

		float fDesiredHeading = fwAngle::GetRadianAngleBetweenPoints(vTargetPos.x, vTargetPos.y, vPedPos.x, vPedPos.y);

		static dev_float fBlendRate = 0.25f;
		float fRate = Min(fBlendRate * 30.0f * fwTimer::GetCamTimeStep(), 1.0f);

		// Lerp up using 2PI so we don't look around the wrong (long) way
		fCamHeading += TWO_PI;
		fDesiredHeading += TWO_PI;
		fRelativeHeading = Lerp(fRate, fCamHeading, fDesiredHeading);
		fRelativeHeading -= TWO_PI;
	}

	return fRelativeHeading;
}

void camCinematicMountedCamera::StartCustomFovBlend(float fovDelta, u32 duration)
{
	m_BlendFovDelta = fovDelta;
	m_BlendFovDeltaDuration = duration;
	m_BlendFovDeltaStartTime = fwTimer::GetCamTimeInMilliseconds();
}

void camCinematicMountedCamera::UpdateCustomFovBlend(float fCurrentFov)
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

#if __BANK
	static bool s_bUseDebugDriveData = false;
	static int s_iDebugDriveData = -1;
#endif

Vector2 camCinematicMountedCamera::GetSeatSpecificScriptedAnimOffset(const CVehicle* attachVehicle)
{
	const CVehicleModelInfo* vehicleModelInfo			= attachVehicle->GetVehicleModelInfo();
	const u32 vehicleModelHash							= vehicleModelInfo ? vehicleModelInfo->GetModelNameHash() : 0;
	const camVehicleCustomSettingsMetadata* settings	= vehicleModelHash > 0 ? camInterface::GetGameplayDirector().FindVehicleCustomSettings(vehicleModelHash) : NULL;

	int seatIndex = camInterface::FindFollowPed()->GetAttachCarSeatIndex(); 
	if (settings && seatIndex > -1)
	{
		if (settings->m_SeatSpecificCamerasSettings.m_ShouldConsiderData)
		{
			for (int i = 0; i < settings->m_SeatSpecificCamerasSettings.m_SeatSpecificCamera.GetCount(); ++i)
			{
				if ((u32)seatIndex == settings->m_SeatSpecificCamerasSettings.m_SeatSpecificCamera[i].m_SeatIndex )
				{
					float fScriptedAnimHeadingOffset = settings->m_SeatSpecificCamerasSettings.m_SeatSpecificCamera[i].m_ScriptedAnimHeadingOffset;
					return Vector2(fScriptedAnimHeadingOffset,fScriptedAnimHeadingOffset);
				}
			}
		}
	}

	return Vector2(0.0f,0.0f);
}

const CFirstPersonDriveByLookAroundData *camCinematicMountedCamera::GetFirstPersonDrivebyData(const CVehicle* attachVehicle)
{
	if(attachVehicle)
	{
		const CVehicleModelInfo* modelInfo = attachVehicle->GetVehicleModelInfo();

#if __BANK
		if(s_bUseDebugDriveData && s_iDebugDriveData >= 0)
		{
			return CVehicleMetadataMgr::GetFirstPersonDriveByLookAroundDataAtIndex(s_iDebugDriveData);
		}
#endif

		s32 iSeat = attachVehicle->GetSeatManager()->GetPedsSeatIndex(camInterface::FindFollowPed());

		if(modelInfo && iSeat != -1)
		{
			return modelInfo->GetFirstPersonLookAroundData(iSeat);
		}
	}

	return NULL;
}

#if __BANK

void camCinematicMountedCamera::AddWidgets()
{
	bkBank* pBank = BANKMGR.FindBank("A.I.");
	if(pBank)
	{
		pBank->PushGroup("FirstPersonDriveby", false);

		static const unsigned maxDrivebyNamesCount = 1070;
		static const char* szDrivebyNames[maxDrivebyNamesCount];
		s32 iCount = 0;
		cameraAssertf(CVehicleMetadataMgr::GetTotalNumFirstPersonDriveByLookAroundData() <= maxDrivebyNamesCount, "CVehicleMetadataMgr::GetTotalNumFirstPersonDriveByLookAroundData (%d) exceeds maxDrivebyNames (%d), please bump the static variable in code to get rid of the assert", CVehicleMetadataMgr::GetTotalNumFirstPersonDriveByLookAroundData(), maxDrivebyNamesCount);
		for( s32 i = 0; i < Min(maxDrivebyNamesCount, CVehicleMetadataMgr::GetTotalNumFirstPersonDriveByLookAroundData()); i++)
		{
			const CFirstPersonDriveByLookAroundData *pDrivebyData = CVehicleMetadataMgr::GetFirstPersonDriveByLookAroundDataAtIndex(i);
			if(pDrivebyData)
			{
				szDrivebyNames[i] = pDrivebyData->GetName().TryGetCStr();
				++iCount;
			}
		}

		pBank->AddCombo(
			"Data:",
			&s_iDebugDriveData,
			iCount,
			szDrivebyNames,
			NullCB,
			"Driveby Debug Data"
			);

		pBank->AddToggle("Use Debug Driveby Data", &s_bUseDebugDriveData);
	
		pBank->PopGroup();
	}
}

#endif
