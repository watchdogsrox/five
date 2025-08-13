//
// camera/gameplay/GameplayDirector.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/gameplay/GameplayDirector.h"

#include "fwdebug/picker.h"
#include "input/mapper.h"
#include "profile/group.h"
#include "profile/page.h"

#include "camera/CamInterface.h"
#include "camera/cinematic/CinematicDirector.h"
#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"
#include "camera/debug/DebugDirector.h"
#include "camera/gameplay/aim/FirstPersonPedAimCamera.h"
#include "camera/gameplay/aim/FirstPersonShooterCamera.h"
#include "camera/gameplay/aim/FirstPersonHeadTrackingAimCamera.h"
#include "camera/gameplay/aim/ThirdPersonPedAimCamera.h"
#include "camera/gameplay/aim/ThirdPersonPedAimInCoverCamera.h"
#include "camera/gameplay/aim/ThirdPersonPedAssistedAimCamera.h"
#include "camera/gameplay/aim/ThirdPersonPedMeleeAimCamera.h"
#include "camera/gameplay/aim/ThirdPersonVehicleAimCamera.h"
#include "camera/gameplay/follow/FollowParachuteCamera.h"
#include "camera/gameplay/follow/FollowPedCamera.h"
#include "camera/gameplay/follow/FollowVehicleCamera.h"
#include "camera/gameplay/follow/TableGamesCamera.h"
#include "camera/helpers/BaseFrameShaker.h"
#include "camera/helpers/CatchUpHelper.h"
#include "camera/helpers/Collision.h"
#include "camera/helpers/ControlHelper.h"
#include "camera/helpers/Envelope.h"
#include "camera/helpers/FrameInterpolator.h"
#include "camera/helpers/ThirdPersonFrameInterpolator.h"
#include "camera/helpers/switch/BaseSwitchHelper.h"
#include "camera/scripted/ScriptDirector.h"
#include "camera/system/CameraFactory.h"
#include "camera/system/CameraManager.h"
#include "camera/system/CameraMetadata.h"
#include "camera/viewports/ViewportManager.h"
#include "control/garages.h"
#include "debug/DebugScene.h"
#include "frontend/MobilePhone.h"
#include "frontend/ProfileSettings.h"
#include "input/headtracking.h"
#include "peds/PedIntelligence.h"
#include "peds/PlayerInfo.h"
#include "phbound/boundcomposite.h"
#include "physics/collision.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "stats/StatsInterface.h"
#include "system/controlMgr.h"
#include "task/Combat/Cover/cover.h"
#include "task/Combat/Cover/TaskCover.h"
#include "task/General/Phone/TaskMobilePhone.h"
#include "task/Motion/TaskMotionBase.h"
#include "Task/Movement/TaskParachute.h"
#include "task/Motion/Vehicle/TaskMotionInTurret.h"
#include "task/Vehicle/TaskEnterVehicle.h"
#include "task/Vehicle/TaskExitVehicle.h"
#include "task/Vehicle/TaskInVehicle.h"
#include "task/Vehicle/TaskMountAnimalWeapon.h"
#include "task/Vehicle/TaskVehicleWeapon.h"
#include "Task/Weapons/Gun/TaskGun.h"
#include "task/Weapons/Gun/TaskVehicleDriveBy.h"
#include "timecycle/TimeCycleConfig.h"
#include "tools/SectorTools.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "vehicles/metadata/VehicleSeatInfo.h"
#include "Vehicles/Metadata/VehicleMetadataManager.h"
#include "vehicles/Automobile.h"
#include "vehicles/AmphibiousAutomobile.h"
#include "vehicles/Trailer.h"
#include "vehicles/wheel.h"
#include "weapons/projectiles/ProjectileManager.h"
#include "weapons/projectiles/ProjectileRocket.h"

#if ! __FINAL
//rage stuff (for the rage::scrThread::PrePrintStackTrace() call).
#include "script/script.h"
RAGE_DECLARE_CHANNEL(Script)
#endif


CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camGameplayDirector,0x3E9ED27F)

PF_PAGE(camGameplayDirectorPage, "Camera Gameplay Director");

PF_GROUP(camGameplayDirectorDetails);
PF_LINK(camGameplayDirectorPage, camGameplayDirectorDetails);

PF_VALUE_FLOAT(camGameplayDirectorAccuracyRecoilScaling, camGameplayDirectorDetails);
PF_VALUE_FLOAT(camGameplayDirectorFirstPersonShootingAbilityRecoilScaling, camGameplayDirectorDetails);
PF_VALUE_FLOAT(camGameplayDirectorAccuracyOffsetScaling, camGameplayDirectorDetails);
PF_VALUE_FLOAT(camGameplayDirectorAccuracyOffsetScalingDamped, camGameplayDirectorDetails);
PF_VALUE_FLOAT(camGameplayDirectorTotalExplosionAmplitude, camGameplayDirectorDetails);

const u32 g_SelfieAimCameraHash				= ATSTRINGHASH("CELL_PHONE_SELFIE_AIM_CAMERA", 0x01ab41687);
const u32 g_ValkyrieTurretCameraHash		= ATSTRINGHASH("VALKYRIE_TURRET_CAMERA", 0x55801250);
const u32 g_InsurgentTurretCameraHash		= ATSTRINGHASH("INSURGENT_TURRET_CAMERA", 0x46773897);
const u32 g_InsurgentTurretAimCameraHash	= ATSTRINGHASH("INSURGENT_TURRET_AIM_CAMERA", 0xd569e597);
const u32 g_TechnicalTurretCameraHash		= ATSTRINGHASH("TECHNICAL_TURRET_CAMERA", 0x3d7fd025);
const u32 g_TechnicalTurretAimCameraHash	= ATSTRINGHASH("TECHNICAL_TURRET_AIM_CAMERA", 0xc9b34c1d);

const float g_TurretRelativeHeadingThresholdForCamera = 1.0f; // We wait until the turret heading is within 1 degree of the camera heading before transitioning to the turret camera

const float g_TurretShakeWeaponPower		= 150.0f;
const float g_TurretShakeMaxAmplitude		= 1.0f;
const float g_TurretShakeThirdPersonScaling = 0.5f;
const float g_TurretShakeFirstPersonScaling = 0.05f;

EXTERN_PARSER_ENUM(camControlHelperMetadataViewMode__eViewModeContext);

extern const u32 g_CatchUpHelperRefForFirstPerson;

const float g_MaxDistanceForWaterClippingTestWhenInASub			= 99999.0f;
const float g_MaxDistanceForRiverWaterClippingTestWhenInASub	= 10.0f;
const float g_MinHeightAboveWaterForFirstPersonWhenInASub		= 0.6f;

camGameplayDirector::camGameplayDirector(const camBaseObjectMetadata& metadata)
: camBaseDirector(metadata)
, m_Metadata(static_cast<const camGameplayDirectorMetadata&>(metadata))
, m_PreviousAimCameraForward(VEC3_ZERO)
, m_PreviousAimCameraPosition(g_InvalidPosition)
, m_SwitchHelper(NULL)
, m_RagdollBlendEnvelope(NULL)
, m_HintEnvelope(NULL)
, m_TurretAligned(false)
, m_ShortRangePlayerSwitchLastUpdate(false)
, m_ShortRangePlayerSwitchFromFirstPerson(false)
, m_CustomFovBlendEnabled(true)
, m_UseSwitchCustomFovBlend(false)
, m_ForceAfterAimingBlend(false)
, m_DisableAfterAimingBlend(false)
, m_PreventVehicleEntryExit(false)
, m_ShouldPerformCustomTransitionAfterWarpFromVehicle(false)
, m_UseVehicleCamStuntSettings(false)
, m_PreviousUseVehicleCamStuntSettings(false)
, m_ScriptRequestedVehicleCamStuntSettingsThisUpdate(false)
, m_ScriptForcedVehicleCamStuntSettingsThisUpdate(false)
, m_ScriptRequestDedicatedStuntCameraThisUpdate(false)
, m_VehicleCamStuntSettingsTestTimer(0.0f)
, m_LastTimeAttachParentWasInWater(0)
, m_ScriptOverriddenFollowVehicleSeatThisUpdate(-1)
#if FPS_MODE_SUPPORTED
, m_FirstPersonAnimatedDataStartTime(0)
, m_JustSwitchedFromSniperScope(false)
, m_IsFirstPersonModeEnabled(true)
, m_IsFirstPersonModeAllowed(false)
, m_DisableFirstPersonThisUpdate(false)
, m_DisableFirstPersonThisUpdateFromScript(false)
, m_bFirstPersonAnimatedDataInVehicle(false)
, m_bPovCameraAnimatedDataBlendoutPending(false)
, m_IsFirstPersonShooterLeftStickControl(true)
, m_WasFirstPersonModeEnabledPreviousFrame(false)
, m_AllowThirdPersonCameraFallbacks(true)
, m_ForceThirdPersonCameraForRagdollAndGetUps(false)
, m_ForceThirdPersonCameraWhenInputDisabled(false)
, m_EnableFirstPersonUseAnimatedData(true)
, m_bIsUsingFirstPersonControls(false)
, m_FirstPersonAnimatedDataRelativeToMover(true)
, m_bApplyRelativeOffsetInCameraSpace(true)
, m_IsFallingBackToThirdPersonForDirectorBlendCatchUpOrSwitch(false)
, m_OverrideFirstPersonCameraThisUpdate(false)
, m_ForceFirstPersonBonnetCamera(false)
, m_HasSwitchCameraTerminatedToForceFPSCamera(false)
#if __BANK
, m_DebugWasFirstPersonDisabledThisUpdate(false)
, m_DebugWasFirstPersonDisabledThisUpdateFromScript(false)
, m_DebugHintActive(false)
#endif // __BANK
#endif // FPS_MODE_SUPPORTED
{
	Reset();

	//Insert the list of vehicle custom settings metadata into a hash map, using the vehicle model name as the key.
	const int numVehicleCustomSettings = m_Metadata.m_VehicleCustomSettingsList.GetCount();
	for(int i=0; i<numVehicleCustomSettings; i++)
	{
		const camGameplayDirectorMetadataVehicleCustomSettings& vehicleCustomSettings = m_Metadata.m_VehicleCustomSettingsList[i];

		//First, check that we have valid custom settings metadata for this vehicle
		const u32 metadataNameHash = vehicleCustomSettings.m_SettingsRef.GetHash();
		if(metadataNameHash == 0)
		{
			continue;
		}

		const camVehicleCustomSettingsMetadata* customSettingsMetadata = camFactory::FindObjectMetadata<camVehicleCustomSettingsMetadata>(metadataNameHash);
		if(!customSettingsMetadata)
		{
			continue;
		}

		const u32 modelNameHash = vehicleCustomSettings.m_ModelName.GetHash();

		if(cameraVerifyf(!m_VehicleCustomSettingsMap.Access(modelNameHash),
			"Found a duplicate vehicle model name hash in the vehicle custom settings map (name: %s, hash: %u)",
			SAFE_CSTRING(vehicleCustomSettings.m_ModelName.GetCStr()), modelNameHash))
		{
			m_VehicleCustomSettingsMap.Insert(modelNameHash, customSettingsMetadata);
		}
	}

	m_ExplosionSettings.Reserve(m_Metadata.m_ExplosionShakeSettings.m_MaxInstances);	
}

camGameplayDirector::~camGameplayDirector()
{
	DestroyAllCameras();

	StopSwitchBehaviour();

	if(m_HintEnvelope)
	{
		delete m_HintEnvelope;
	}
	
	if(m_RagdollBlendEnvelope)
	{
		delete m_RagdollBlendEnvelope;
	}

	m_VehicleCustomSettingsMap.Kill();

	StopExplosionShakes();

	if(m_DeathFailEffectFrameShaker)
	{
		delete m_DeathFailEffectFrameShaker;
	}
}

bool camGameplayDirector::Update()
{
	FPS_MODE_SUPPORTED_ONLY(m_JustSwitchedFromSniperScope = false;)

	const CPed* followPed = camInterface::FindFollowPed();
	if(!cameraVerifyf(followPed, "The gameplay camera director does not have a valid follow ped"))
	{
		return false;
	}

	if(followPed != m_FollowPed)
	{
		//The follow ped has changed this update, so fall-back to a safe camera state. This prevents undesirable interpolation behavior.
		m_FollowPed	= followPed;
		m_State		= DOING_NOTHING;
	}

#if GTA_REPLAY
	if (CReplayMgr::IsEditModeActive())
	{
		return false;
	}
#endif // GTA_REPLAY

#if __BANK
	if(m_DebugShouldOverrideFollowPedCamera)
	{
		const u32 cameraNameHash = atStringHash(m_DebugOverriddenFollowPedCameraName);
		tGameplayCameraSettings settings(NULL, cameraNameHash, m_DebugOverriddenFollowPedCameraInterpolationDuration);

		SetScriptOverriddenFollowPedCameraSettingsThisUpdate(settings);
	}

	if(m_DebugShouldOverrideFollowVehicleCamera)
	{
		const u32 cameraNameHash = atStringHash(m_DebugOverriddenFollowVehicleCameraName);
		tGameplayCameraSettings settings(NULL, cameraNameHash, m_DebugOverriddenFollowVehicleCameraInterpolationDuration);

		SetScriptOverriddenFollowVehicleCameraSettingsThisUpdate(settings);
	}

	if(m_DebugForceUsingStuntSettings)
	{
		ScriptRequestVehicleCamStuntSettingsThisUpdate();
	}

	TUNE_GROUP_BOOL(CAM_STUNT_SETTINGS, bForceRequestDedicatedStuntCamera, false);
	if(bForceRequestDedicatedStuntCamera)
	{
		ScriptRequestDedicatedStuntCameraThisUpdate("FOLLOW_VEHICLE_STUNT_CAMERA");
	}

	if(m_DebugForceUsingTightSpaceCustomFraming)
	{
		ScriptForceUseTightSpaceCustomFramingThisUpdate();
	}
#endif // __BANK

	m_PreviousUseVehicleCamStuntSettings = m_UseVehicleCamStuntSettings;

	UpdateVehicleEntryExitState(*followPed);

	UpdateTurretHatchCollision(*followPed);
	
	UpdateHeliTurretCollision(*followPed);
	
	//Track the time spent swimming.
	bool isFollowPedSwimming = (followPed->GetIsSwimming() && !followPed->GetUsingRagdoll());
	if(isFollowPedSwimming)
	{
		//Sanity check the motion task, as the camera buoyancy state is influenced by this.
		const CTaskMotionBase* followPedMotionTask = followPed->GetPrimaryMotionTask();
		if(followPedMotionTask)
		{
			 isFollowPedSwimming = (followPedMotionTask->IsSwimming() || followPedMotionTask->IsDiving());
		}
	}

	m_FollowPedTimeSpentSwimming = isFollowPedSwimming ? (m_FollowPedTimeSpentSwimming + fwTimer::GetTimeStepInMilliseconds()) : 0;

	const bool isResettingActiveCamera = (m_State == DOING_NOTHING);

	UpdateFollowPedWeaponSettings(*followPed);

	const bool isRenderingMobilePhoneCamera	= CPhoneMgr::CamGetState();

	if(isResettingActiveCamera || (!isRenderingMobilePhoneCamera && (m_FollowPedWeaponInfo.Get() != m_FollowPedWeaponInfoOnPreviousUpdate.Get())) ||
		(isRenderingMobilePhoneCamera != m_WasRenderingMobilePhoneCameraOnPreviousUpdate))
	{
		//Reset the persistent zoom control when resetting the active camera or when the follow ped switches weapons or toggles the mobile phone
		//camera.
		camControlHelper::SetZoomFactor(1.0f);
	}

	UpdateFollowPedCoverSettings(*followPed, isResettingActiveCamera);

	UpdateFollowPedBlendLevels(isResettingActiveCamera);

	UpdateTightSpaceSettings(*followPed);

	UpdateTrailerStatus();
	UpdateTowedVehicleStatus();

	UpdateAttachParentInWater();

	const bool c_bShortRangePlayerSwitchThisUpdate = g_PlayerSwitch.IsActive() && g_PlayerSwitch.GetSwitchType() == CPlayerSwitchInterface::SWITCH_TYPE_SHORT;
#if FPS_MODE_SUPPORTED
	bool bToggledForceBonnetCameraView = false;
	bool bDisableForceBonnetCameraView = false;
	m_IsFallingBackToThirdPersonForDirectorBlendCatchUpOrSwitch = false;
	m_IsFirstPersonModeAllowed = (IsFirstPersonModeEnabled()) ?
		ComputeIsFirstPersonModeAllowed(followPed, true, m_IsFallingBackToThirdPersonForDirectorBlendCatchUpOrSwitch, &bToggledForceBonnetCameraView, &bDisableForceBonnetCameraView) : 
		false;
	UpdateBonnetCameraViewToggle(bToggledForceBonnetCameraView, bDisableForceBonnetCameraView);

	bool bAllowFirstPersonRagdoll = (CPauseMenu::GetMenuPreference( PREF_FPS_RAGDOLL ) == TRUE);
	m_ForceThirdPersonCameraForRagdollAndGetUps = !bAllowFirstPersonRagdoll;
	UpdateFirstPersonAnimatedDataState(*followPed);
	RemapInputsForFirstPersonShooter();
	m_OverrideFirstPersonCameraThisUpdate = false;
	m_HasSwitchCameraTerminatedToForceFPSCamera = false; 

	m_ShortRangePlayerSwitchFromFirstPerson |= GetFirstPersonShooterCamera() && c_bShortRangePlayerSwitchThisUpdate;
	m_ShortRangePlayerSwitchFromFirstPerson &= (c_bShortRangePlayerSwitchThisUpdate || m_ShortRangePlayerSwitchLastUpdate);
#endif

	m_CustomFovBlendEnabled = !m_ShortRangePlayerSwitchFromFirstPerson;
	m_PreviousAimCameraForward = VEC3_ZERO;
	m_PreviousAimCameraPosition = g_InvalidPosition;

	//Handle the DOING_NOTHING state up-front as a special-case. This avoids wasting an update deciding what state to transition, which would leave
	//us with an invalid or out of date frame.
	if(m_State == DOING_NOTHING)
	{
		DestroyAllCameras(); //Ensure any existing cameras are cleaned-up.

		//Transition to START_FOLLOWING_PED or START_FOLLOWING_VEHICLE.
		m_State = (m_VehicleEntryExitState == INSIDE_VEHICLE) ? START_FOLLOWING_VEHICLE : START_FOLLOWING_PED;
		if (rage::ioHeadTracking::UseFPSCamera())
			m_State = START_HEAD_TRACKING;
	}

	//NOTE: The 'update' (as oppose to 'start') states can request that an additional iteration of state update be performed. This allows transitions
	//between states to be implemented within a single camera update. This process does not recurse for all state changes, as that could result in an
	//infinite loop.
	bool shouldContinueStateUpdate;

	m_PreviousState = m_State;

	do
	{
		shouldContinueStateUpdate = false;

		switch(m_State)
		{
		case START_FOLLOWING_PED:
			UpdateStartFollowingPed(m_FollowVehicle, *followPed);
			break;

		case FOLLOWING_PED:
			//NOTE: Most of the important state transitions occur within UpdateFollowingPed.
			shouldContinueStateUpdate = UpdateFollowingPed(*followPed);
			break;

		case START_FOLLOWING_VEHICLE:
			UpdateStartFollowingVehicle(m_FollowVehicle);
			break;

		case FOLLOWING_VEHICLE:
			shouldContinueStateUpdate = UpdateFollowingVehicle(m_FollowVehicle, *followPed);
			break;

		case START_VEHICLE_AIMING:
			UpdateStartVehicleAiming(m_FollowVehicle, *followPed);
			break;

		case VEHICLE_AIMING:
			shouldContinueStateUpdate = UpdateVehicleAiming(m_FollowVehicle, *followPed);
			break;

		case START_MOUNTING_TURRET:
			UpdateStartMountingTurret(m_FollowVehicle, *followPed);
			break;

		case TURRET_MOUNTED:
			shouldContinueStateUpdate = UpdateTurretMounted(m_FollowVehicle, *followPed);
			break;

		case START_TURRET_AIMING:
			UpdateStartTurretAiming(m_FollowVehicle, *followPed);
			break;

		case START_TURRET_FIRING:
			UpdateStartTurretFiring(m_FollowVehicle, *followPed);
			break;

		case TURRET_AIMING:
			shouldContinueStateUpdate = UpdateTurretAiming(m_FollowVehicle, *followPed);
			break;

		case TURRET_FIRING:
			shouldContinueStateUpdate = UpdateTurretFiring(m_FollowVehicle, *followPed);
			break;

		case START_HEAD_TRACKING:
			UpdateStartHeadTracking(m_FollowVehicle, *followPed);
			break;

		case HEAD_TRACKING:
			shouldContinueStateUpdate = UpdateHeadTracking(*followPed);
			break;

		//case DOING_NOTHING:
		default:
			//The DOING_NOTHING state is handled specially above, so we should never hit this case.
			cameraErrorf("The gameplay camera director has entered an invalid state (%d)", (s32)m_State); 
			m_State = DOING_NOTHING;
			break;
		}

	} while(shouldContinueStateUpdate);

	UpdateCatchUp();

	UpdateSwitchBehaviour();
	
	UpdateMissileHint(); 

	UpdateHint(); 

	UpdateAimAccuracyOffsetShake();

	ApplyCameraOverridesForScript();

	camFrameInterpolator* activeFrameInterpolator		= m_ActiveCamera ? m_ActiveCamera->GetFrameInterpolator() : NULL;
	const bool isThirdPersonInterpolatingBetweenCameras	= (activeFrameInterpolator &&
															activeFrameInterpolator->GetIsClassId(camThirdPersonFrameInterpolator::GetStaticClassId()));

	if(m_VehicleForCustomTransitionAfterWarp)
	{
		if(isThirdPersonInterpolatingBetweenCameras)
		{
			camCollision* activeCameraCollision = m_ActiveCamera ? m_ActiveCamera->GetCollision() : nullptr;
			camCollision* previousCameraCollision = m_PreviousCamera ? m_PreviousCamera->GetCollision() : nullptr;
			if(activeCameraCollision)
			{
				activeCameraCollision->IgnoreEntityThisUpdate(*m_VehicleForCustomTransitionAfterWarp.Get());
			}
			if(previousCameraCollision)
			{
				previousCameraCollision->IgnoreEntityThisUpdate(*m_VehicleForCustomTransitionAfterWarp.Get());
			}
		}
		else
		{
			m_VehicleForCustomTransitionAfterWarp = nullptr;
		}
	}

	if(m_PreviousCamera)
	{
		bool shouldUpdatePreviousCamera = (m_ActiveCamera && m_ActiveCamera->IsInterpolating(m_PreviousCamera));
		if(shouldUpdatePreviousCamera)
		{
			if(isThirdPersonInterpolatingBetweenCameras)
			{
				//NOTE: We bypass the collision update of this camera when interpolating between third-person cameras, as we need to perform this during
				//the interpolation instead.
				camCollision* previousCameraCollision = m_PreviousCamera->GetCollision();
				if(previousCameraCollision)
				{
					previousCameraCollision->BypassThisUpdate();
				}

				//NOTE: We bypass the frame shakers associated with the this camera when interpolating between third-person cameras, as we need to perform this during
				//the interpolation instead.
				m_PreviousCamera->BypassFrameShakersThisUpdate();
			}

			//We are interpolating from the previous camera, so we need to update it.
			camFrame dummyFrame;
			const bool hasSucceeded = m_PreviousCamera->BaseUpdate(dummyFrame);

			if(activeFrameInterpolator && activeFrameInterpolator->HasCachedSourceRelativeToDestination())
			{
				//NOTE: We stop updating the previous camera if it fails to update, so long as we have cached the interpolation source parameters relative
				//to the destination. We don't want to perform dynamic interpolation based upon an out of date source unless we have really have to.
				//NOTE: We terminate interpolation from a follow parachute camera into another type of camera as soon as the source parameters have been
				//cached, as the parachute can fly away from the follow ped, causing undesirable interpolation behaviour.
				if(!hasSucceeded || (m_PreviousCamera->GetIsClassId(camFollowParachuteCamera::GetStaticClassId()) &&
					!m_ActiveCamera->GetIsClassId(camFollowParachuteCamera::GetStaticClassId())))
				{
					shouldUpdatePreviousCamera = false;
				}
			}
		}

	#if FPS_MODE_SUPPORTED
		if( m_PreviousCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()) )
		{
			const camFirstPersonShooterCamera* pFpsCamera = (const camFirstPersonShooterCamera*)(m_PreviousCamera.Get());
			if(pFpsCamera->GetDeathShakeHash() != 0)
			{
				m_ActiveCamera.Get()->Shake( pFpsCamera->GetDeathShakeHash() );
			}
		}
	#endif

		if(!shouldUpdatePreviousCamera)
		{
			//If we no longer need to update the previous camera, we can safely delete it.
			delete m_PreviousCamera;
		}
	}

	if(m_ActiveCamera)
	{
		if(isThirdPersonInterpolatingBetweenCameras)
		{
			//NOTE: We bypass the collision update of this camera when interpolating between third-person cameras, as we need to perform this during
			//the interpolation instead.
			camCollision* activeCameraCollision = m_ActiveCamera->GetCollision();
			if(activeCameraCollision)
			{
				activeCameraCollision->BypassThisUpdate();
			}

			//NOTE: We bypass the frame shakers associated with the this camera when interpolating between third-person cameras, as we need to perform this during
			//the interpolation instead.
			m_ActiveCamera->BypassFrameShakersThisUpdate();

			camThirdPersonFrameInterpolator* activeThirdPersonFrameInterpolator = static_cast<camThirdPersonFrameInterpolator*>(activeFrameInterpolator);

			//Ensure that the active camera's interpolation helper performs a collision update, as this will be bypassed within the source and
			//destination third-person cameras.
			activeThirdPersonFrameInterpolator->ApplyCollisionThisUpdate();

			//Ensure that the active camera's interpolation helper updates the frame shakers of the source and destination third-person cameras,
			//as they have been bypassed within the individual camera updates.
			activeThirdPersonFrameInterpolator->ApplyFrameShakeThisUpdate();
		}

		if(m_ActiveCamera->GetIsClassId(camFollowVehicleCamera::GetStaticClassId()) || m_ActiveCamera->GetIsClassId(camThirdPersonVehicleAimCamera::GetStaticClassId()))
		{
			//Attempt to clone the orientation of a rendering cinematic first-person mounted camera, as we want to persist this orientation in the vehicle camera.
			//TODO: Convert the standard first-person bonnet camera back into a gameplay camera. This will allow us to remove a lot of special-case logic.
			const camCinematicDirector& cinematicDirector	= camInterface::GetCinematicDirector();
			const bool isRenderingCinematicMountedCamera		= cinematicDirector.IsRenderingCinematicMountedCamera(false);
			const bool isRenderingCinematicWaterCrashCamera	= cinematicDirector.IsRenderingCinematicWaterCrash(false);
			if(isRenderingCinematicMountedCamera || isRenderingCinematicWaterCrashCamera)
			{
				const camBaseCamera* renderedCinematicCamera = cinematicDirector.GetRenderedCamera();
				if(renderedCinematicCamera)
				{
					static_cast<camThirdPersonCamera*>(m_ActiveCamera.Get())->CloneBaseOrientation(*renderedCinematicCamera);
				}
			}
		}

		m_ActiveCamera->BaseUpdate(m_Frame);
	}

	m_UpdatedCamera = m_ActiveCamera;

#if FPS_MODE_SUPPORTED
	//if( m_IsFirstPersonModeEnabled && IsFirstPersonModeAllowed() && m_ShouldUseThirdPersonCameraForFirstPersonMode &&
	//	m_FollowPed && m_ActiveCamera && m_ActiveCamera->GetIsClassId(camThirdPersonCamera::GetStaticClassId()) )
	//{
	//	//Override the camera position to the follow ped's head, where appropriate.
	//	Matrix34 boneMatrix;
	//	const bool isBoneValid = m_FollowPed->GetBoneMatrixCached(boneMatrix, BONETAG_HEAD);
	//	if(isBoneValid)
	//	{
	//		m_Frame.SetPosition(boneMatrix.d);
	//	}
	//}

	if(followPed && followPed->GetPedResetFlag(CPED_RESET_FLAG_BlockCameraSwitching))
	{
		//Check if the first person shooter camera is valid.
		camFirstPersonShooterCamera* pFirstPersonShooterCamera = GetFirstPersonShooterCamera();
		if(pFirstPersonShooterCamera && pFirstPersonShooterCamera->GetControlHelper())
		{
			//Disable camera switching.
			pFirstPersonShooterCamera->GetControlHelper()->IgnoreViewModeInputThisUpdate();
		}

		//Check if the third person camera is valid.
		camThirdPersonCamera* pThirdPersonCamera = GetThirdPersonCamera();
		if(pThirdPersonCamera && pThirdPersonCamera->GetControlHelper())
		{
			//Disable camera switching.
			pThirdPersonCamera->GetControlHelper()->IgnoreViewModeInputThisUpdate();
		}
	}

#endif // FPS_MODE_SUPPORTED

	UpdateMotionBlur(*followPed);
	UpdateDepthOfField();

	// Update after gameplay camera is updated as first person camera is delaying pov camera.
	// If done before camera is updated, then pov camera is delayed for an extra update.
	UpdateVehiclePovCameraConditions();

	UpdateReticuleSettings(*followPed);

	m_ShortRangePlayerSwitchLastUpdate = c_bShortRangePlayerSwitchThisUpdate;

	m_WasRenderingMobilePhoneCameraOnPreviousUpdate	= isRenderingMobilePhoneCamera;

	m_IsHighAltitudeFovScalingDisabledThisUpdate = false;

	m_ScriptOverriddenFollowPedCameraSettingsOnPreviousUpdate = m_ScriptOverriddenFollowPedCameraSettingsThisUpdate;
	m_ScriptOverriddenFollowPedCameraSettingsThisUpdate.Reset();
	
    m_ScriptOverriddenFollowVehicleCameraSettingsOnPreviousUpdate = m_ScriptOverriddenFollowVehicleCameraSettingsThisUpdate;
	m_ScriptOverriddenFollowVehicleCameraSettingsThisUpdate.Reset();

    m_ScriptOverriddenTableGamesCameraSettingsOnPreviousUpdate = m_ScriptOverriddenTableGamesCameraSettingsThisUpdate;
    m_ScriptOverriddenTableGamesCameraSettingsThisUpdate.Reset();

	if (!m_PreviousCamera)
	{
		m_CachedPreviousCameraTableGamesCameraHash = 0;
	}

	m_ShouldScriptOverrideFirstPersonAimCameraRelativeHeadingLimitsThisUpdate	= false;
	m_ShouldScriptOverrideFirstPersonAimCameraRelativePitchLimitsThisUpdate		= false;
	m_ShouldScriptOverrideFirstPersonAimCameraNearClipThisUpdate				= false;
	m_ShouldScriptOverrideThirdPersonCameraRelativeHeadingLimitsThisUpdate		= false;
	m_ShouldScriptOverrideThirdPersonCameraRelativePitchLimitsThisUpdate		= false;
	m_ShouldScriptOverrideThirdPersonCameraOrbitDistanceLimitsThisUpdate		= false;
	m_ShouldScriptOverrideThirdPersonAimCameraNearClipThisUpdate				= false; 

	m_ScriptPreventCollisionSettingsForTripleHeadInInteriorsThisUpdate = false;

	m_ShouldScriptOverrideFollowVehicleCameraHighAngleModeThisUpdate = false;

	m_ShouldAllowCustomVehicleDriveByCamerasThisUpdate = false;

	m_ScriptForceUseTightSpaceCustomFramingThisUpdate = false;

	m_ScriptRequestedVehicleCamStuntSettingsThisUpdate = false;
	m_ScriptForcedVehicleCamStuntSettingsThisUpdate = false;
	m_ScriptRequestDedicatedStuntCameraThisUpdate = false;

	m_ForceAfterAimingBlend		= false;
	m_DisableAfterAimingBlend	= false;

    m_ScriptOverriddenFollowVehicleSeatThisUpdate = -1;

	m_ShouldFollowCameraIgnoreAttachParentMovementThisUpdateForScript = false;
	FPS_MODE_SUPPORTED_ONLY( BANK_ONLY(m_DebugWasFirstPersonDisabledThisUpdate = m_DisableFirstPersonThisUpdate;) )
	FPS_MODE_SUPPORTED_ONLY( BANK_ONLY(m_DebugWasFirstPersonDisabledThisUpdateFromScript = m_DisableFirstPersonThisUpdateFromScript;) )
	FPS_MODE_SUPPORTED_ONLY( m_DisableFirstPersonThisUpdate = false; )
	FPS_MODE_SUPPORTED_ONLY( m_DisableFirstPersonThisUpdateFromScript = false; )
	FPS_MODE_SUPPORTED_ONLY( m_WasFirstPersonModeEnabledPreviousFrame = m_IsFirstPersonModeEnabled; )

	camControlHelper::ClearOverriddenZoomFactorLimits();

#if __BANK
	if(m_DebugShouldLogActiveViewModeContext)
	{
		const s32 activeViewModeContext	= GetActiveViewModeContext();
		const char* contextName			= PARSER_ENUM(camControlHelperMetadataViewMode__eViewModeContext).m_Names[activeViewModeContext];

		cameraDisplayf("Active view-mode context: %s", (contextName != NULL) ? contextName : "Invalid");
	}
#endif // __BANK

	return true;
}

void camGameplayDirector::PostUpdate()
{
	camBaseDirector::PostUpdate();

	//Update any explosion shakes, as they are handled separately to the base director shakes at present.

	if(camInterface::IsFadedOut())
	{
		//NOTE: We stop any active explosion shakes when the screen is faded out in order to ensure they are cleaned up on mission retry/shit-skip.
		StopExplosionShakes();
	}
	else
	{
		const s32 numExplosions = m_ExplosionSettings.GetCount();
		s32 explosionIndex;

#if __STATS
		float totalExplositionAmplitude = 0.0f;
#endif // __STATS

		for(explosionIndex=0; explosionIndex<numExplosions; explosionIndex++)
		{
			tExplosionSettings& explosionSettings	= m_ExplosionSettings[explosionIndex];
			camBaseFrameShaker* frameShaker			= explosionSettings.m_FrameShaker;
			if(!frameShaker)
			{
				continue;
			}

            const float shakeScaleFactor = m_ActiveCamera ? m_ActiveCamera->GetFrameShakerScaleFactor() : 1.0f;

			const float amplitude = ComputeShakeAmplitudeForExplosion(explosionSettings.m_Strength, explosionSettings.m_RollOffScaling, explosionSettings.m_Position);
			frameShaker->SetAmplitude(amplitude);
			frameShaker->Update(m_PostEffectFrame, shakeScaleFactor);

#if __STATS
			totalExplositionAmplitude += amplitude;
#endif // __STATS
		}

		PF_SET(camGameplayDirectorTotalExplosionAmplitude, totalExplositionAmplitude);

		//Clean up any (NULL) references to expired explosion frame shakers.
		for(explosionIndex=numExplosions - 1; explosionIndex>=0; explosionIndex--)
		{
			if(m_ExplosionSettings[explosionIndex].m_FrameShaker.Get() == NULL)
			{
				m_ExplosionSettings.Delete(explosionIndex);
			}
		}
	}

	//Update any death/fail effect shake, as this is handled separately to the base director shakes at present.

	if(m_DeathFailEffectFrameShaker)
	{
		m_DeathFailEffectFrameShaker->Update(m_PostEffectFrame);
	}

	//Apply any scaling and limiting to the motion blur strength, now that any post-effects in the director have been applied.

	float motionBlurStrength = m_PostEffectFrame.GetMotionBlurStrength();

	//Apply any script-specified scaling and limiting to the motion blur strength.
	motionBlurStrength *= m_ScriptControlledMotionBlurScalingThisUpdate;
	motionBlurStrength = Min(motionBlurStrength, m_ScriptControlledMaxMotionBlurStrengthThisUpdate);

	//Reset the script-specified motion blur scaling and limiting. These parameters must be specified every update by script, for safety.
	m_ScriptControlledMotionBlurScalingThisUpdate		= 1.0f;
	m_ScriptControlledMaxMotionBlurStrengthThisUpdate	= 1.0f;

	//NOTE: We apply the primary motion blur strength limit last, in order to ensure that script scaling cannot exceed this.
	const bool canUseVisualSettings		= g_visualSettings.GetIsLoaded();
	const float maxMotionBlurStrength	= canUseVisualSettings ? g_visualSettings.Get("cam.game.blur.cap") : 1.0f;
	motionBlurStrength					= Min(motionBlurStrength, maxMotionBlurStrength);

#if __BANK
	if (m_DebugOverrideMotionBlurStrengthThisUpdate >= 0.0f)
	{
		motionBlurStrength = m_DebugOverrideMotionBlurStrengthThisUpdate;
		m_DebugOverrideMotionBlurStrengthThisUpdate = -1.0f;
	}
#endif // __BANK

	m_PostEffectFrame.SetMotionBlurStrength(motionBlurStrength);

	//Populate the frustum helper, which is used by IsSphereVisible().

	Matrix34 rageWorldMatrix;
	m_PostEffectFrame.ComputeRageWorldMatrix(rageWorldMatrix);

	const grcWindow& window			= gVpMan.GetGameViewport()->GetGrcViewport().GetWindow();
	const float aspectRatio			= static_cast<float>(window.GetWidth()) / static_cast<float>(window.GetHeight());
	const float postEffectFov		= m_PostEffectFrame.GetFov();
	const float postEffectNearClip	= m_PostEffectFrame.GetNearClip();
	const float postEffectFarClip	= m_PostEffectFrame.GetFarClip();

	const ScalarV tanVFov(Tanf(0.5f * DtoR * postEffectFov));
	const ScalarV tanHFov = tanVFov * ScalarV(aspectRatio);
	const ScalarV nearClip(postEffectNearClip);
	const ScalarV farClip(postEffectFarClip);

	//NOTE: Specifying the near and far clip plane distances here is pretty suspect, as they are subject to change before rendering,
	//by the near-clip scanner and the time cycle, respectively. However, we are continuing to utilise these planes for backwards compatibility
	//with the legacy camFrame::IsSphereVisible() helper.
	m_TransposedPlaneSet = GenerateFrustum8(RC_MAT34V(rageWorldMatrix), tanHFov, tanVFov, nearClip, farClip);

#if __BANK
	TUNE_BOOL(adaptiveDofShouldUseBoundingSphereOfDebugEntityForFocus, false)
	if(adaptiveDofShouldUseBoundingSphereOfDebugEntityForFocus)
	{
		m_ScriptEntityToLimitFocusOverBoundingSphereThisUpdate = CDebugScene::FocusEntities_Get(0);
	}
#endif // __BANK

	if(m_ScriptEntityToLimitFocusOverBoundingSphereThisUpdate)
	{
		//If the camera is pointing at the bounding sphere of this entity, limit the max pixel depth for focus distance measurement to the farthest point on its bounding sphere.

		Vector3 entityCentre;
		const float entityRadius				= m_ScriptEntityToLimitFocusOverBoundingSphereThisUpdate->GetBoundCentreAndRadius(entityCentre);
		const Vector3& cameraFront				= m_PostEffectFrame.GetFront();
		const Vector3& cameraPosition			= m_PostEffectFrame.GetPosition();
		const Vector3 cameraToSphereCentre		= entityCentre - cameraPosition;
		const float cameraToSphereCentreDistSqr	= cameraToSphereCentre.Mag2();
		const float distanceToSphereCentreSqr	= cameraToSphereCentreDistSqr - square(cameraFront.Dot(cameraToSphereCentre));
		if(distanceToSphereCentreSqr <= square(entityRadius))
		{
			float maxPixelDepth	= m_PostEffectFrame.GetDofMaxPixelDepth();
			maxPixelDepth		= Min(maxPixelDepth, Sqrtf(cameraToSphereCentreDistSqr) + entityRadius);

			m_PostEffectFrame.SetDofMaxPixelDepth(maxPixelDepth);

#if DEBUG_DRAW
			TUNE_BOOL(adaptiveDofShouldRenderBoundingSphereOfEntityForFocus, false)
			if(adaptiveDofShouldRenderBoundingSphereOfEntityForFocus)
			{
				grcDebugDraw::Sphere(entityCentre, entityRadius, Color_red, false);
			}
#endif // DEBUG_DRAW
		}

		m_ScriptEntityToLimitFocusOverBoundingSphereThisUpdate = NULL;
	}

#if RSG_PC
	const bool disableInGameDof = CPauseMenu::GetMenuPreference(PREF_GFX_DOF) == 0;
#else
	const bool disableInGameDof = (CProfileSettings::GetInstance().Exists(CProfileSettings::DISPLAY_DOF) && (CProfileSettings::GetInstance().GetInt(CProfileSettings::DISPLAY_DOF) == 0));
#endif // RSG_PC

	if((!CPhoneMgr::CamGetState() && disableInGameDof) || (CPhoneMgr::CamGetState() && !CPhoneMgr::GetDOFState()))
	{
		m_PostEffectFrame.SetDofSubjectMagPowerNearFar(Vector2(10.0f, 10.0f));
		m_PostEffectFrame.SetFNumberOfLens(256.0f);
		m_PostEffectFrame.SetDofMaxNearInFocusDistance(0.0f);
		m_PostEffectFrame.SetDofMaxNearInFocusDistanceBlendLevel(1.0f);
	}
}

bool camGameplayDirector::IsPedInOrEnteringTurretSeat(const CVehicle* followVehicle, const CPed& followPed, bool bSkipEnterVehicleTestForFirstPerson/*=true*/, bool bIsCameraCheckForEntering)
{	
	if(!followVehicle)
	{
		return false;
	}

    const s32 seatIndex = ComputeSeatIndexPedIsIn(*followVehicle, followPed, bSkipEnterVehicleTestForFirstPerson);
    return IsTurretSeat(followVehicle, seatIndex, bIsCameraCheckForEntering);
}

bool camGameplayDirector::IsTurretSeat(const CVehicle* followVehicle, int seatIndex, bool bIsCameraCheckForEntering)
{
	if(!followVehicle || seatIndex < 0)
	{
		return false;
	}

	if (bIsCameraCheckForEntering)
	{
		const bool bEnteringDune3TurretSeat = MI_CAR_APC.IsValid() && followVehicle->GetModelIndex() == MI_CAR_APC && seatIndex == 1;
		if (bEnteringDune3TurretSeat)
		{
			return false;
		}

		const bool bEnteringHalfTrackTurretSeat = MI_CAR_HALFTRACK.IsValid() && followVehicle->GetModelIndex() == MI_CAR_HALFTRACK && seatIndex == 2;
		if (bEnteringHalfTrackTurretSeat)
		{
			return false;
		}

		const bool bIsTrailerSmall2 = followVehicle->IsTrailerSmall2();
		if (bIsTrailerSmall2)
		{
			return false;
		}
	}

	const bool bAPCTurretSeat = !bIsCameraCheckForEntering && MI_CAR_APC.IsValid() && followVehicle->GetModelIndex() == MI_CAR_APC && seatIndex == 1;
    const bool bUltralightDriverSeat = !bIsCameraCheckForEntering && MI_PLANE_MICROLIGHT.IsValid() && followVehicle->GetModelIndex() == MI_PLANE_MICROLIGHT && seatIndex == 0;
	const bool bMulePassengerSeat = !bIsCameraCheckForEntering && MI_CAR_MULE4.IsValid() && followVehicle->GetModelIndex() == MI_CAR_MULE4 && seatIndex == 1;
	const bool bPounderFrontSeat = !bIsCameraCheckForEntering && MI_CAR_POUNDER2.IsValid() && followVehicle->GetModelIndex() == MI_CAR_POUNDER2 && (seatIndex == 0 || seatIndex == 1);

	// For some vehicles, we only want to accept this as a turret seat (and use turret cameras) if the turret weapon is selected
    if(bUltralightDriverSeat || bMulePassengerSeat || bPounderFrontSeat)
    {
        const CPed* followPed = camInterface::FindFollowPed();
        if(followPed && followPed->GetWeaponManager())
        {
            const CWeaponInfo* pTurretInSeatInfo = GetTurretWeaponInfoForSeat(*followVehicle, seatIndex);
            const CWeaponInfo* pSelectedWeaponInfo = followPed->GetWeaponManager()->GetSelectedVehicleWeaponInfo();
            return (pSelectedWeaponInfo != nullptr && pTurretInSeatInfo != nullptr && pTurretInSeatInfo == pSelectedWeaponInfo);
        }
    }

	return bAPCTurretSeat || followVehicle->IsTurretSeat(seatIndex);
}

const CTurret* camGameplayDirector::GetTurretForPed(const CVehicle* followVehicle, const CPed& followPed)
{
	if(!followVehicle)
	{
		return NULL;
	}

    const s32 seatIndex = ComputeSeatIndexPedIsIn(*followVehicle, followPed, false, true);
	if(seatIndex != -1 && IsTurretSeat(followVehicle, seatIndex)) 
	{
		const CVehicleWeaponMgr* vehicleWeaponManager = followVehicle->GetVehicleWeaponMgr();			
		if(vehicleWeaponManager)
		{					
			CTurret* turret = vehicleWeaponManager->GetFirstTurretForSeat(seatIndex);
			if(turret)
			{	
				return turret;
			}
		}
	}

	return NULL;
}

void camGameplayDirector::UpdateVehicleEntryExitState(const CPed& followPed)
{
	m_VehicleEntryExitStateOnPreviousUpdate = m_VehicleEntryExitState;

#if __BANK
	if(m_DebugShouldReportOutsideVehicle)
	{
		m_FollowVehicle			= NULL;
		m_VehicleEntryExitState	= OUTSIDE_VEHICLE;

		return;
	}
#endif // __BANK

	if(m_ScriptOverriddenVehicleEntryExitState != -1)
	{
		m_VehicleEntryExitState = m_ScriptOverriddenVehicleEntryExitState; 
		m_FollowVehicle = m_ScriptOverriddenFollowVehicle; 
		m_ScriptOverriddenFollowVehicle = NULL; 
		m_ScriptOverriddenVehicleEntryExitState = -1; 
		return; 
	}
	
	m_VehicleEntryExitState = followPed.GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) ? INSIDE_VEHICLE : OUTSIDE_VEHICLE;

	const CVehicle* followPedVehicle	= followPed.GetMyVehicle();
	m_FollowVehicle						= (m_VehicleEntryExitState == INSIDE_VEHICLE) ? followPedVehicle : NULL;

	//update the stunt settings based on the vehicle state
	TUNE_GROUP_BOOL(CAM_STUNT_SETTINGS, bForceStuntSettingsEnabled, false);
	if(m_ScriptRequestedVehicleCamStuntSettingsThisUpdate || m_ScriptForcedVehicleCamStuntSettingsThisUpdate || bForceStuntSettingsEnabled)
	{
		const bool shouldUseStuntSettings = bForceStuntSettingsEnabled || ComputeShouldUseVehicleStuntSettings();
		TUNE_GROUP_FLOAT(CAM_STUNT_SETTINGS, fTestTimer, 1.0f, 0.0f, 10.0f, 0.01f);
		if(shouldUseStuntSettings)
		{
			m_UseVehicleCamStuntSettings = true;
			m_VehicleCamStuntSettingsTestTimer = 0.0f;
		}
		else
		{
			if(m_UseVehicleCamStuntSettings)
			{
                const bool isVehicleInWater = m_FollowVehicle && m_FollowVehicle->GetIsInWater();
				m_VehicleCamStuntSettingsTestTimer += fwTimer::GetCamTimeStep();

				if(m_VehicleCamStuntSettingsTestTimer > fTestTimer || isVehicleInWater)
				{
					m_UseVehicleCamStuntSettings = false;
				}
			}
		}
	}
	else
	{
		m_UseVehicleCamStuntSettings = false;
	}

	const CPedIntelligence* pedIntelligence	= followPed.GetPedIntelligence();
	if(pedIntelligence)
	{
		CTaskExitVehicle* exitVehicleTask	= static_cast<CTaskExitVehicle*>(pedIntelligence->FindTaskActiveByType(
												CTaskTypes::TASK_EXIT_VEHICLE));

		if(exitVehicleTask)
		{
			const CTaskInVehicleSeatShuffle* seatShuffleTask = static_cast<const CTaskInVehicleSeatShuffle*>(pedIntelligence->FindTaskActiveByType(
																CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE));
			if(seatShuffleTask)
			{
				// If shuffling, ignore the exit task.
				m_VehicleEntryExitState = m_PreventVehicleEntryExit ? OUTSIDE_VEHICLE : EXITING_VEHICLE;
				return;
			}

			float minExitSeatPhaseForCameraExit = -1.0f;

			CVehicle* vehicleBeingExited = exitVehicleTask->GetVehicle();
			if(vehicleBeingExited)
			{
				const CVehicleModelInfo* vehicleModelInfo = vehicleBeingExited->GetVehicleModelInfo();
				if(vehicleModelInfo)
				{
					const u32 vehicleModelNameHash							= vehicleModelInfo->GetModelNameHash();
					const camVehicleCustomSettingsMetadata* customSettings	= FindVehicleCustomSettings(vehicleModelNameHash);
					if(customSettings && customSettings->m_ExitSeatPhaseForCameraExitSettings.m_ShouldConsiderData)
					{
						minExitSeatPhaseForCameraExit = customSettings->m_ExitSeatPhaseForCameraExitSettings.m_MinExitSeatPhaseForCameraExit;
					}
				}
			}

			if(exitVehicleTask->IsConsideredGettingOutOfVehicleForCamera(minExitSeatPhaseForCameraExit))
			{
				const CVehicleModelInfo* vehicleModelInfo = vehicleBeingExited ? vehicleBeingExited->GetVehicleModelInfo() : NULL;
				const bool shouldCameraIgnoreExiting = vehicleModelInfo && vehicleModelInfo->ShouldCameraIgnoreExiting();

				//Report that the ped is still inside the vehicle if we should ignore exiting.
				m_VehicleEntryExitState = shouldCameraIgnoreExiting ? INSIDE_VEHICLE : (m_PreventVehicleEntryExit ? OUTSIDE_VEHICLE : EXITING_VEHICLE);
			}

			m_FollowVehicle = vehicleBeingExited;
			return;
		}

		CTaskEnterVehicle* enterVehicleTask	= static_cast<CTaskEnterVehicle*>(pedIntelligence->FindTaskActiveByType(
												CTaskTypes::TASK_ENTER_VEHICLE));
		if(enterVehicleTask)
		{
			if(enterVehicleTask->IsConsideredGettingInVehicleForCamera())
			{			
				m_VehicleEntryExitState = m_PreventVehicleEntryExit ? INSIDE_VEHICLE : ENTERING_VEHICLE;		
			}

			m_FollowVehicle = enterVehicleTask->GetVehicle();						
		}
		else
		{
			//if we reach this point we are not entering or exiting anymore so we can reset the flag.
			m_PreventVehicleEntryExit = false;
		}

#if FPS_MODE_SUPPORTED
		//FIX: url:bugstar:3830359
		if(m_VehicleEntryExitState == ENTERING_VEHICLE && m_FollowVehicle && m_FollowVehicle->GetVehicleModelInfo() && GetFirstPersonShooterCamera())
		{
			if(m_FollowVehicle->GetVehicleModelInfo()->GetModelNameHash() == ATSTRINGHASH("MOGUL", 0xD35698EF))
			{
				//disable lookbehind while entering a vehicle
				CControl* control = GetActiveControl();
				if(control)
				{
					control->DisableInput(INPUT_LOOK_BEHIND);
				}
			}
		}
#endif

		if(!IsPedInOrEnteringTurretSeat(m_FollowVehicle, *m_FollowPed))
		{				
			// Reset m_TurretAligned when we are not in a turret seat
			m_TurretAligned = false;
		}		
		
		if(m_FollowPed && m_FollowVehicle)
		{
			const CTurret* turret = GetTurretForPed(m_FollowVehicle, *m_FollowPed);
			if(turret)
			{
				bool bSkipWaitingForTurretAlignment = false;
				s32 sTargetSeat = m_FollowVehicle->GetVehicleWeaponMgr()->GetSeatIndexForTurret(*turret);
				if (m_FollowVehicle->IsSeatIndexValid(sTargetSeat))
				{
					const CVehicleSeatInfo* pSeatInfo = m_FollowVehicle->GetSeatInfo(sTargetSeat);
					if (pSeatInfo && pSeatInfo->GetCameraSkipWaitingForTurretAlignment())
					{
						bSkipWaitingForTurretAlignment = true;
					}
				}

				// Don't wait for turret alignment in the heli vehicles, or if the player is looking behind, or in FPS cam during entry
				if(bSkipWaitingForTurretAlignment || IsLookingBehind() || m_FollowVehicle->InheritsFromHeli() || m_FollowVehicle->InheritsFromPlane() || camInterface::IsRenderingFirstPersonCamera())
				{
					// But wait until we're in the turret seat.
					const CTaskVehicleMountedWeapon* vehicleMountedWeaponTask = static_cast<const CTaskVehicleMountedWeapon*>(m_FollowPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_VEHICLE_MOUNTED_WEAPON));				
					if(vehicleMountedWeaponTask) 
					{					
						m_VehicleEntryExitState = INSIDE_VEHICLE;
						m_TurretAligned = true;
					}
				}
				// Wait for turret-camera alignment on all other vehicle types
				else if(!m_TurretAligned)
				{
					const float cameraHeading = camInterface::GetHeading();
					const float turretHeading = turret->GetCurrentHeading(m_FollowVehicle);

					const float relativeHeading = fwAngle::LimitRadianAngle(cameraHeading - turretHeading);

					if(fabs(relativeHeading) * RtoD <= g_TurretRelativeHeadingThresholdForCamera)
					{					
						m_VehicleEntryExitState = INSIDE_VEHICLE;
						m_TurretAligned = true;
					}
				}
			}

			return;
		}		
	}

	//Check if the follow ped is ragdolling off/out of a vehicle.
	if((m_VehicleEntryExitStateOnPreviousUpdate == INSIDE_VEHICLE) && (m_VehicleEntryExitState == OUTSIDE_VEHICLE) && followPedVehicle)
	{
		const bool isAttachPedUsingRagdoll = followPed.GetUsingRagdoll();
		if(isAttachPedUsingRagdoll)
		{
			//Check that the follow ped is still close to the vehicle and hasn't been teleported away.
			spdAABB vehicleAabb;
			followPedVehicle->GetAABB(vehicleAabb);

			Vec3V followPedPosition				= followPed.GetTransform().GetPosition();
			const float distanceToVehicleAabb	= vehicleAabb.DistanceToPoint(followPedPosition).Getf();
			if(distanceToVehicleAabb <= m_Metadata.m_MaxDistanceFromVehicleAabbForRagdollExit)
			{
				m_VehicleEntryExitState	= m_PreventVehicleEntryExit ? OUTSIDE_VEHICLE : EXITING_VEHICLE;
				m_FollowVehicle			= followPedVehicle;
			}
		}
		else
		{
			//if we reach this point we are not entering or exiting anymore so we can reset the flag.
			m_PreventVehicleEntryExit = false;
		}
	}
	else
	{
		//if we reach this point we are not entering or exiting anymore so we can reset the flag.
		m_PreventVehicleEntryExit = false;
	}
}

void camGameplayDirector::SetCarDoorCameraCollision(const CVehicle* vehicle, s32 seatIndex, bool enabled)
{
	if(!vehicle)
	{
		return;
	}

	const CCarDoor* carDoor = CTaskMotionInVehicle::GetDirectAccessEntryDoorFromSeat(*vehicle, seatIndex);
	if(carDoor)
	{		
		const phBound* vehicleBound = vehicle->GetVehicleFragInst()->GetArchetype()->GetBound();
		if(vehicleBound->GetType() == phBound::COMPOSITE)
		{
			const int iComponent = vehicle->GetVehicleFragInst()->GetComponentFromBoneIndex(carDoor->GetBoneIndex());
			if(iComponent > -1)
			{
				phBoundComposite* pBoundComp = const_cast<phBoundComposite*>(static_cast<const phBoundComposite*>(vehicleBound));
				const u32 includeFlags = enabled ? (pBoundComp->GetIncludeFlags(iComponent) | ArchetypeFlags::GTA_CAMERA_TEST) : (pBoundComp->GetIncludeFlags(iComponent) & ~ArchetypeFlags::GTA_CAMERA_TEST);
				pBoundComp->SetIncludeFlags(iComponent, includeFlags);
			}
		}
	}
}

void camGameplayDirector::UpdateTurretHatchCollision(const CPed& followPed) const
{
#if ENABLE_MOTION_IN_TURRET_TASK
	if(!m_FollowVehicle)
	{
		return;
	}

	const CSeatManager* seatManager = m_FollowVehicle->GetSeatManager();
	const s32 seatIndex = seatManager ? seatManager->GetPedsSeatIndex(&followPed) : -1;

	if(!CTaskMotionInTurret::IsSeatWithHatchProtection(seatIndex, *m_FollowVehicle))
	{
		return;
	}
	
	const CPedIntelligence* pedIntelligence	= followPed.GetPedIntelligence();
	const CTaskInVehicleSeatShuffle* shuffleTask = pedIntelligence ? static_cast<CTaskInVehicleSeatShuffle*>(pedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE)) : NULL;

	const bool shufflingToNonTurretSeat = shuffleTask && !IsTurretSeat(m_FollowVehicle, shuffleTask->GetTargetSeatIndex());
	
	if(m_VehicleEntryExitState == EXITING_VEHICLE || shufflingToNonTurretSeat)
	{		
		SetCarDoorCameraCollision(m_FollowVehicle, seatIndex, true);		
	}
	else if(m_VehicleEntryExitState == INSIDE_VEHICLE)
	{		
		SetCarDoorCameraCollision(m_FollowVehicle, seatIndex, false);		
	}
#endif // ENABLE_MOTION_IN_TURRET_TASK
}

void camGameplayDirector::SetCachedHeliTurretCameraCollision(bool enabled)
{
	if(!m_HeliVehicleForTurret)
	{
		return;
	}
	
	phBound* vehicleBound = m_HeliVehicleForTurret->GetVehicleFragInst()->GetArchetype()->GetBound();
	if(vehicleBound && vehicleBound->GetType() == phBound::COMPOSITE)
	{		
		phBoundComposite* pBoundComp = static_cast<phBoundComposite*>(vehicleBound);

		if(m_HeliTurretWeaponComponentIndex > INVALID_COMPONENT)
		{									
			u32 includeFlags = pBoundComp->GetIncludeFlags(m_HeliTurretWeaponComponentIndex);
			includeFlags = enabled ? (includeFlags | ArchetypeFlags::GTA_CAMERA_TEST) : (includeFlags & ~ArchetypeFlags::GTA_CAMERA_TEST);
			pBoundComp->SetIncludeFlags(m_HeliTurretWeaponComponentIndex, includeFlags);
		}

		if(m_HeliTurretBaseComponentIndex > INVALID_COMPONENT)
		{									
			u32 includeFlags = pBoundComp->GetIncludeFlags(m_HeliTurretBaseComponentIndex);
			includeFlags = enabled ? (includeFlags | ArchetypeFlags::GTA_CAMERA_TEST) : (includeFlags & ~ArchetypeFlags::GTA_CAMERA_TEST);
			pBoundComp->SetIncludeFlags(m_HeliTurretBaseComponentIndex, includeFlags);
		}

		if(m_HeliTurretBarrelComponentIndex > INVALID_COMPONENT)
		{									
			u32 includeFlags = pBoundComp->GetIncludeFlags(m_HeliTurretBarrelComponentIndex);
			includeFlags = enabled ? (includeFlags | ArchetypeFlags::GTA_CAMERA_TEST) : (includeFlags & ~ArchetypeFlags::GTA_CAMERA_TEST);
			pBoundComp->SetIncludeFlags(m_HeliTurretBarrelComponentIndex, includeFlags);
		}
	}
}

void camGameplayDirector::UpdateAttachParentInWater()
{
	if(!m_FollowVehicle && !m_FollowPed)
	{
		return;
	}

	const CPhysical* pPhysical = (m_FollowVehicle && m_FollowVehicle->GetIsPhysical()) ? static_cast<const CPhysical*>(m_FollowVehicle.Get()) : (m_FollowPed && m_FollowPed->GetIsPhysical()) ? static_cast<const CPhysical*>(m_FollowPed.Get()) : nullptr;

	if(pPhysical && pPhysical->GetIsInWater())
	{
		m_LastTimeAttachParentWasInWater = fwTimer::GetCamTimeInMilliseconds();
	}
}

void camGameplayDirector::UpdateHeliTurretCollision(const CPed& followPed)
{
	s32 seatIndex = -1;
	const CPedIntelligence* pedIntelligence	= followPed.GetPedIntelligence();

	const CVehicle* heliVehicle = m_FollowVehicle.Get();
	if(heliVehicle)
	{
        seatIndex = ComputeSeatIndexPedIsIn(*heliVehicle, followPed);
	}	
	
	CTaskExitVehicleSeat* pExitVehicleSeatTask = (CTaskExitVehicleSeat*)(pedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE_SEAT));

	if(m_HeliVehicleForTurret)
	{	
		// Are we no longer in the turreted vehicle or have switched seat	
		if(pExitVehicleSeatTask || m_HeliVehicleForTurret != m_FollowVehicle || m_HeliVehicleSeatIndexForTurret != seatIndex)
		{
			// Re-enable camera collision			
			SetCachedHeliTurretCameraCollision(true);

			// Clear our pointer to the vehicle and seat index			
			m_HeliVehicleForTurret				= NULL;
			m_HeliVehicleSeatIndexForTurret		= -1;				
			m_HeliTurretWeaponComponentIndex	= -1;
			m_HeliTurretBaseComponentIndex		= -1;
			m_HeliTurretBarrelComponentIndex	= -1;
		}
		else
		{
			// Early out if we're still in the same vehicle and seat
			return;
		}
	}

	if(!pExitVehicleSeatTask && seatIndex != -1 && heliVehicle && heliVehicle->InheritsFromHeli() && IsTurretSeat(heliVehicle, seatIndex))
	{	
		m_HeliVehicleForTurret = heliVehicle;
		m_HeliVehicleSeatIndexForTurret = seatIndex;				

		if(heliVehicle->GetVehicleWeaponMgr())
		{		
			const CFixedVehicleWeapon* turretFixedWeapon = const_cast<const CFixedVehicleWeapon*>(heliVehicle->GetVehicleWeaponMgr()->GetFirstFixedWeaponForSeat(seatIndex));
			m_HeliTurretWeaponComponentIndex = m_HeliVehicleForTurret->GetVehicleFragInst()->GetComponentFromBoneIndex(m_HeliVehicleForTurret->GetBoneIndex(turretFixedWeapon->GetWeaponBone()));

			const CTurret* turret = const_cast<const CTurret*>(heliVehicle->GetVehicleWeaponMgr()->GetFirstTurretForSeat(seatIndex));
			if(turret->IsPhysicalTurret())
			{
				const CTurretPhysical* turretPhysical = static_cast<const CTurretPhysical*>(turret);
				m_HeliTurretBaseComponentIndex = turretPhysical->GetBarrelFragChild();
				m_HeliTurretBarrelComponentIndex = turretPhysical->GetBaseFragChild();
			}
		}

		SetCachedHeliTurretCameraCollision(false);
	}
}

void camGameplayDirector::UpdateFollowPedWeaponSettings(const CPed& followPed)
{
	m_FollowPedWeaponInfoOnPreviousUpdate = m_FollowPedWeaponInfo;

	m_FollowPedWeaponInfo = ComputeWeaponInfoForPed(followPed);
}

void camGameplayDirector::UpdateFollowPedCoverSettings(const CPed& followPed, bool shouldResetBlendLevels)
{
	m_FollowPedCoverSettingsOnPreviousUpdate = m_FollowPedCoverSettings;

	const CCoverPoint* coverPoint				= followPed.GetCoverPoint();
	const CPedIntelligence* pedIntelligence		= followPed.GetPedIntelligence();
	const CTaskCover* parentCoverTask			= pedIntelligence ? static_cast<CTaskCover*> (pedIntelligence->FindTaskActiveByType(
													CTaskTypes::TASK_COVER)) : NULL;
	const CTaskInCover* useCoverTask			= pedIntelligence ? static_cast<CTaskInCover*> (pedIntelligence->FindTaskActiveByType(
													CTaskTypes::TASK_IN_COVER)) : NULL;
	const CTaskMotionInCover* coverMotionTask	= pedIntelligence ? static_cast<CTaskMotionInCover*> (pedIntelligence->FindTaskActiveByType(
													CTaskTypes::TASK_MOTION_IN_COVER)) : NULL;

	//NOTE: If we are following a network clone ped and we don't have a cover point, we can try to extract the necessary data from the clone
	//task info.
	const CClonedCoverInfo* clonedCoverInfo = NULL; 
	if(!coverPoint && followPed.IsNetworkClone())
	{
		const CQueriableInterface* pedQueriableInterface = pedIntelligence->GetQueriableInterface();
		if(pedQueriableInterface && pedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_COVER))
		{
			clonedCoverInfo	= static_cast<const CClonedCoverInfo*>(pedQueriableInterface->GetTaskInfoForTaskType(CTaskTypes::TASK_COVER,
								PED_TASK_PRIORITY_MAX));
			if(clonedCoverInfo && !clonedCoverInfo->HasCoverPoint())
			{
				//The cover point information was not serialised for this clone ped.
				clonedCoverInfo = NULL;
			}
		}
	}

	if(parentCoverTask && (coverPoint || clonedCoverInfo))
	{
		m_FollowPedCoverSettings.m_IsInCover					= true;
		m_FollowPedCoverSettings.m_IsInLowCover					= coverPoint ? (coverPoint->GetHeight() != CCoverPoint::COVHEIGHT_TOOHIGH) :
																	!(clonedCoverInfo->GetFlags() & CTaskCover::CF_TooHighCoverPoint);
		m_FollowPedCoverSettings.m_CoverUsage					= coverPoint ? coverPoint->GetUsage() : clonedCoverInfo->GetCoverUsage();

		const s32 coverMotionState								= coverMotionTask ? coverMotionTask->GetState() : -1;
		m_FollowPedCoverSettings.m_IsTurning					= (coverMotionState == CTaskMotionInCover::State_TurnEnter) ||
																	(coverMotionState == CTaskMotionInCover::State_Turning) ||
																	(coverMotionState == CTaskMotionInCover::State_TurnEnd);
		m_FollowPedCoverSettings.m_IsMovingAroundCornerCover	= (coverMotionState == CTaskMotionInCover::State_EdgeTurn);
		m_FollowPedCoverSettings.m_IsMovingFromCoverToCover		= (coverMotionState == CTaskMotionInCover::State_CoverToCover);
		//NOTE: We consider the at-edge move states as idle in order to prevent one-frame glitches when returning from aiming.
		m_FollowPedCoverSettings.m_IsIdle						= (coverMotionState == CTaskMotionInCover::State_Idle) ||
																	(coverMotionState == CTaskMotionInCover::State_PlayingIdleVariation) ||
																	(coverMotionState == CTaskMotionInCover::State_Settle) ||
																	(coverMotionState == CTaskMotionInCover::State_AtEdge) ||
																	(coverMotionState == CTaskMotionInCover::State_AtEdgeLowCover);

		const CMoveNetworkHelper* coverMoveNetworkHelper		= coverMotionTask ? coverMotionTask->GetMoveNetworkHelper() : NULL;
		m_FollowPedCoverSettings.m_CoverToCoverPhase			= (m_FollowPedCoverSettings.m_IsMovingFromCoverToCover && coverMoveNetworkHelper) ?
																	Clamp(coverMoveNetworkHelper->GetFloat(
																	CTaskMotionInCover::ms_CoverToCoverClipCurrentPhaseId), 0.0f, 1.0f) : 0.0f;

		//NOTE: We must query if the turning states are active, as the facing-left flag is not updated until the turn is partially complete.
		const bool isFacingLeftInCoverTemp	= (m_FollowPedCoverSettings.m_IsTurning && coverMotionTask) ? coverMotionTask->IsTurningLeft() :
												parentCoverTask->IsCoverFlagSet(CTaskCover::CF_FacingLeft);
		//NOTE: We lock the facing direction initially during cover-to-cover transitions to prevent a premature change in direction.
		if(!m_FollowPedCoverSettings.m_IsMovingFromCoverToCover ||
			(m_FollowPedCoverSettings.m_CoverToCoverPhase >= m_Metadata.m_FollowPedCoverToCoverMinPhaseForFacingDirectionBlend))
		{
			m_FollowPedCoverSettings.m_IsFacingLeftInCover = isFacingLeftInCoverTemp;
		}

		m_FollowPedCoverSettings.m_CanFireRoundCornerCover		= CTaskInCover::CanPedFireRoundCorner(followPed,
																	m_FollowPedCoverSettings.m_IsFacingLeftInCover,
																	m_FollowPedCoverSettings.m_IsInLowCover,
																	static_cast<CCoverPoint::eCoverUsage>(m_FollowPedCoverSettings.m_CoverUsage));
		const s32 coverState									= useCoverTask ? useCoverTask->GetState() : -1;
		m_FollowPedCoverSettings.m_IsPeeking					= (coverState == CTaskInCover::State_Peeking);
		m_FollowPedCoverSettings.m_IsTransitioningToAiming		= (coverState == CTaskInCover::State_AimIntro);
		m_FollowPedCoverSettings.m_IsAiming						= (coverState == CTaskInCover::State_Aim) ||
																	(coverState == CTaskInCover::State_ThrowingProjectile);
		m_FollowPedCoverSettings.m_IsTransitioningFromAiming	= (coverState == CTaskInCover::State_AimOutro);
		m_FollowPedCoverSettings.m_IsReloading					= (coverState == CTaskInCover::State_Reloading);
		m_FollowPedCoverSettings.m_IsBlindFiring				= (coverState == CTaskInCover::State_BlindFiring);
		if(m_FollowPedCoverSettings.m_IsBlindFiring && pedIntelligence)
		{
			//Do not report the blind-fire outro or cocking as blind-firing, so long as the user isn't still firing (as we need to handle
			//shotgun cocking.)
			const CTaskAimGunBlindFire* blindFireTask	= static_cast<CTaskAimGunBlindFire*>(pedIntelligence->FindTaskActiveByType(
															CTaskTypes::TASK_AIM_GUN_BLIND_FIRE));
			if(blindFireTask)
			{
				const s32 blindFireState = blindFireTask->GetState();
				if((blindFireState == CTaskAimGunBlindFire::State_OutroNew) || (blindFireState == CTaskAimGunBlindFire::State_CockGun))
				{
					const WeaponControllerState weaponControllerState = blindFireTask->GetWeaponControllerState(&followPed);
					if(weaponControllerState != WCS_Fire)
					{
						m_FollowPedCoverSettings.m_IsBlindFiring = false;
					}
				}
			}
		}

		//Apply a hold time to camera behaviour associated with aiming in high corner cover, for improved continuity,

		const u32 currentTime							= fwTimer::GetCamTimeInMilliseconds();
		m_FollowPedCoverSettings.m_IsAimingHoldActive	= false;

		if((m_FollowPedCoverSettings.m_IsTransitioningToAiming || m_FollowPedCoverSettings.m_IsAiming || m_FollowPedCoverSettings.m_IsTransitioningFromAiming) &&
			m_FollowPedCoverSettings.m_CanFireRoundCornerCover && !m_FollowPedCoverSettings.m_IsInLowCover)
		{
			m_FollowPedCoverAimingHoldStartTime = currentTime;

			//NOTE: We must extend the hold throughout the transition from aiming.
			if(m_FollowPedCoverSettings.m_IsTransitioningFromAiming)
			{
				m_FollowPedCoverSettings.m_IsAimingHoldActive = true;
			}
		}
		else if(!m_FollowPedCoverSettings.m_IsIdle)
		{
			m_FollowPedCoverAimingHoldStartTime = 0;
		}
		else
		{
			m_FollowPedCoverSettings.m_IsAimingHoldActive = (currentTime <= (m_FollowPedCoverAimingHoldStartTime + m_Metadata.m_FollowPedCoverAimingHoldDuration));
		}

		//Enable automatic alignment when the follow ped first reaches high corner cover, or when they switch sides on a pillar, so long as they
		//aren't aiming or firing.
		if(m_FollowPedCoverSettings.m_CanFireRoundCornerCover && !m_FollowPedCoverSettings.m_IsInLowCover &&
			!m_FollowPedCoverSettings.m_IsTransitioningToAiming && !m_FollowPedCoverSettings.m_IsAiming &&
			!m_FollowPedCoverSettings.m_IsTransitioningFromAiming && !m_FollowPedCoverSettings.m_IsBlindFiring)
		{
			if(!m_FollowPedCoverSettingsOnPreviousUpdate.m_CanFireRoundCornerCover ||
				(m_FollowPedCoverSettings.m_IsFacingLeftInCover != m_FollowPedCoverSettingsOnPreviousUpdate.m_IsFacingLeftInCover))
			{
				m_FollowPedCoverSettings.m_ShouldAlignToCornerCover = true;
			}
		}
		else
		{
			m_FollowPedCoverSettings.m_ShouldAlignToCornerCover = false;
		}

		//Ensure that we abort automatic alignment to corner cover if the user is providing look-around input or if an orientation change has been
		//explicitly requested.
		if(m_FollowPedCoverSettings.m_ShouldAlignToCornerCover)
		{
			if(m_UseScriptRelativeHeading || m_UseScriptRelativePitch)
			{
				m_FollowPedCoverSettings.m_ShouldAlignToCornerCover = false;
			}
			else
			{
				const CControl* control = GetActiveControl();
				if(control)
				{
					const ioValue& headingControl	= control->GetPedLookLeftRight();
					const ioValue& pitchControl		= control->GetPedLookUpDown();
					const bool isLookingAround		= !IsNearZero(headingControl.GetUnboundNorm(), SMALL_FLOAT) ||
														!IsNearZero(pitchControl.GetUnboundNorm(), SMALL_FLOAT);

					const CTaskMobilePhone* pMobilePhoneTask = static_cast<const CTaskMobilePhone*>(followPed.GetPedIntelligence()->FindTaskSecondaryByType(CTaskTypes::TASK_MOBILE_PHONE));
					const s32 iPhoneState = pMobilePhoneTask ? pMobilePhoneTask->GetState() : -1;
					const bool isPuttingUpOrDownPhone = iPhoneState == CTaskMobilePhone::State_PutUpText || iPhoneState == CTaskMobilePhone::State_PutDownFromText;
					const bool isSwitchingWeapons	= control->GetSelectWeapon().IsDown();
					if(isLookingAround || isSwitchingWeapons || isPuttingUpOrDownPhone)
					{
						m_FollowPedCoverSettings.m_ShouldAlignToCornerCover = false;
					}
				}
			}
		}

		//NOTE: We avoid updating the step out position:
		// - during cover to cover transitions, as this position snaps to the destination cover point.
		// - during turning on narrow/pillar-type high cover, as this position snaps when turning around.
		// - during weapon swapping, as it is possible that no valid step out position will be available.
		const bool isTurningOnNarrowCover	= m_FollowPedCoverSettings.m_IsTurning &&
												(m_FollowPedCoverSettings.m_CoverUsage == CCoverPoint::COVUSE_WALLTONEITHER);
		if(m_FollowPedCoverSettings.m_CanFireRoundCornerCover && !m_FollowPedCoverSettings.m_IsMovingFromCoverToCover &&
			!isTurningOnNarrowCover && (coverState != CTaskInCover::State_SwapWeapon))
		{
			Vector3 stepOutMoverPosition;
			float dummyHeading;
			bool shouldAimDirectly		= false;
			const bool isPositionValid	= useCoverTask && useCoverTask->ComputeCoverAimPositionAndHeading(stepOutMoverPosition, dummyHeading,
											shouldAimDirectly, true);
			if(isPositionValid)
			{
				//NOTE: We also avoid updating the step out position and flag it to be ignored when 'aiming directly', as the step out position can
				//snap in this case.
				m_FollowPedCoverSettings.m_ShouldIgnoreStepOut = shouldAimDirectly;
				if(!m_FollowPedCoverSettings.m_ShouldIgnoreStepOut)
				{
					m_FollowPedCoverSettings.m_StepOutPosition = stepOutMoverPosition;
				}
			}
			else
			{
				m_FollowPedCoverSettings.m_ShouldIgnoreStepOut = true;
			}
		}

		//Generate a matrix to describe the cover point.
		//NOTE: We lock the cover matrix for the duration of cover-to-cover, as it would otherwise snap to the destination cover point.
		if(!m_FollowPedCoverSettings.m_IsMovingFromCoverToCover)
		{
			const Vector3 pedPosition = VEC3V_TO_VECTOR3(followPed.GetTransform().GetPosition());

			Vector3 coverPosition;
			Vector3 coverNormal;

			if(coverPoint)
			{
				coverPoint->GetCoverPointPosition(coverPosition);

				Vector3 targetDirection	= coverPosition - pedPosition;
				targetDirection.NormalizeSafe(YAXIS);
				coverNormal = VEC3V_TO_VECTOR3(coverPoint->GetCoverDirectionVector(&RCC_VEC3V(targetDirection)));
			}
			else
			{
				coverPosition	= clonedCoverInfo->GetCoverPosition();
				coverNormal		= clonedCoverInfo->GetCoverDirection();
				coverNormal.NormalizeSafe(YAXIS); //Renormalise for safety.
			}

			float coverNormalHeading = camFrame::ComputeHeadingFromFront(coverNormal);

			//Use a damped spring to smooth any discontinuities in the cover normal.
			if(m_FollowPedCoverSettingsOnPreviousUpdate.m_IsInCover && !shouldResetBlendLevels)
			{
				//Ensure that we blend to the desired heading over the shortest angle.
				const float coverNormalHeadingOnPreviousUpdate	= m_FollowPedCoverNormalHeadingSpring.GetResult();
				const float coverNormalHeadingDelta				= coverNormalHeading - coverNormalHeadingOnPreviousUpdate;
				if(coverNormalHeadingDelta > PI)
				{
					coverNormalHeading -= TWO_PI;
				}
				else if(coverNormalHeadingDelta < -PI)
				{
					coverNormalHeading += TWO_PI;
				}

				coverNormalHeading	= m_FollowPedCoverNormalHeadingSpring.Update(coverNormalHeading,
										m_Metadata.m_FollowPedCoverNormalSpringConstant);
				coverNormalHeading	= fwAngle::LimitRadianAngle(coverNormalHeading);

				//NOTE: We must override the persistent spring heading to a valid -PI to PI range.
				m_FollowPedCoverNormalHeadingSpring.OverrideResult(coverNormalHeading);
			}
			else
			{
				m_FollowPedCoverNormalHeadingSpring.Reset(coverNormalHeading);
			}

			camFrame::ComputeFrontFromHeadingAndPitch(coverNormalHeading, 0.0f, coverNormal);

			camFrame::ComputeWorldMatrixFromFront(coverNormal, m_FollowPedCoverSettings.m_CoverMatrix);

			m_FollowPedCoverSettings.m_CoverMatrix.d	= coverPosition;
			m_FollowPedCoverSettings.m_CoverMatrix.d.z	= pedPosition.z; //Use the z of the ped for consistency.
		}

		m_FollowPedCoverSettingsForMostRecentCover	= m_FollowPedCoverSettings;
	}
	else
	{
		m_FollowPedCoverSettings.Reset();

		m_FollowPedCoverAimingHoldStartTime = 0;
	}
}

void camGameplayDirector::UpdateFollowPedBlendLevels(bool shouldResetBlendLevels)
{
	const float desiredFacingDirectionScaling	= m_FollowPedCoverSettings.m_IsInCover ?
													(m_FollowPedCoverSettings.m_IsFacingLeftInCover ? -1.0f : 1.0f) : 0.0f;
	if(shouldResetBlendLevels)
	{
		m_FollowPedCoverFacingDirectionScalingSpring.Reset(desiredFacingDirectionScaling);
	}
	else
	{
		m_FollowPedCoverFacingDirectionScalingSpring.Update(desiredFacingDirectionScaling,
			m_Metadata.m_FollowPedCoverFacingDirectionScalingSpringConstant);
	}
}

void camGameplayDirector::UpdateTightSpaceSettings(const CPed& followPed)
{
#if __TRACK_PEDS_IN_NAVMESH
	const CNavMeshTrackedObject& followPedNavMeshTracker = followPed.GetNavMeshTracker();
	if(followPedNavMeshTracker.GetIsValid())
	{
		float reverbSize	= 0.0f;
		float reverbWetness	= 0.0f;

		const u32 audioProperties = static_cast<u32>(followPedNavMeshTracker.GetNavPolyData().m_iAudioProperties);

		//NOTE: We must special-case zeroed audio environment properties, as this denotes an invalid size/no reverb.
		if(audioProperties == 0)
		{
			m_TightSpaceLevel = 0.0f;
		}
		else
		{
			const u32 quantisedReverbSize	= (audioProperties & 3);
			reverbSize						= static_cast<float>(quantisedReverbSize) / 3.0f;
			float reverbSizeTvalue			= RampValueSafe(reverbSize, m_Metadata.m_ReverbSizeLimitsForTightSpaceLevel.x,
												m_Metadata.m_ReverbSizeLimitsForTightSpaceLevel.y, 0.0f, 1.0f);
			reverbSizeTvalue				= SlowInOut(reverbSizeTvalue);
			const float reverbSizeLevel		= Lerp(reverbSizeTvalue, m_Metadata.m_TightSpaceLevelAtReverbSizeLimits.x,
												m_Metadata.m_TightSpaceLevelAtReverbSizeLimits.y);

			const u32 quantisedReverbWet	= (audioProperties & 12) >> 2;
			reverbWetness					= static_cast<float>(quantisedReverbWet) / 3.0f;
			float reverbWetnessTvalue		= RampValueSafe(reverbWetness, m_Metadata.m_ReverbWetLimitsForTightSpaceLevel.x,
												m_Metadata.m_ReverbWetLimitsForTightSpaceLevel.y, 0.0f, 1.0f);
			reverbWetnessTvalue				= SlowInOut(reverbWetnessTvalue);
			const float reverbWetnessScale	= Lerp(reverbWetnessTvalue, m_Metadata.m_TightSpaceLevelScalingAtReverbWetLimits.x,
												m_Metadata.m_TightSpaceLevelScalingAtReverbWetLimits.y);

			m_TightSpaceLevel = reverbSizeLevel * reverbWetnessScale;
		}

#if __BANK
		if(m_DebugShouldLogTightSpaceSettings)
		{
			cameraDebugf1("Tight-space level = %f (reverbSize = %f, reverbWetness = %f)", m_TightSpaceLevel, reverbSize, reverbWetness);
		}
#endif // __BANK
	}
	else if(!followPedNavMeshTracker.GetIsUpToDate())
	{
		//The nav-mesh tracker is not up-to-date, so rather than persist what might be a inappropriate tight-space level, make a guess at the
		//correct level, based upon whether or not the follow-ped is in an interior.
		const bool isfollowPedInInterior	= followPed.GetIsInInterior();
		m_TightSpaceLevel					= isfollowPedInInterior ? 1.0f : 0.0f;

#if __BANK
		if(m_DebugShouldLogTightSpaceSettings)
		{
			cameraDebugf1("Estimated tight-space level = %f", m_TightSpaceLevel);
		}
#endif // __BANK
	}
#if __BANK
	else if(m_DebugShouldLogTightSpaceSettings)
	{
		cameraDebugf1("Persisted tight-space level = %f", m_TightSpaceLevel);
	}
#endif // __BANK
#endif // __TRACK_PEDS_IN_NAVMESH

//////////////////////////////////////////////////////////////////////////
//	An alternate approach that uses the audio environment API:
//////////////////////////////////////////////////////////////////////////
//
// 	naEnvironmentGroup* environmentGroup = NULL;
// 
// 	if(followVehicle)
// 	{
// 		const audVehicleAudioEntity* followVehicleAudioEntity = followVehicle->GetVehicleAudioEntity();
// 		if(followVehicleAudioEntity)
// 		{
// 			environmentGroup = followVehicleAudioEntity->GetEnvironmentGroup();
// 		}
// 	}
// 	else
// 	{
// 		const audPedAudioEntity* followPedAudioEntity = const_cast<CPed&>(followPed).GetPedAudioEntity();
// 		if(followPedAudioEntity)
// 		{
// 			environmentGroup = followPedAudioEntity->GetEnvironmentGroup();
// 		}
// 	}
// 
// 	if(environmentGroup)
// 	{
// 		environmentGroup->ForceSourceEnvironmentUpdate(NULL);
// 
// 		reverbSize		= environmentGroup->GetSourceEnvironmentReverbSizeUnsmoothed();
// 		reverbWetness	= environmentGroup->GetSourceEnvironmentReverbWetUnsmoothed();
// 
// 		//NOTE: We must special-case a reverb size of 0.0, as this denotes an invalid size/no reverb.
// 		if(reverbSize > SMALL_FLOAT)
// 		{
// 			float reverbSizeTvalue		= RampValueSafe(reverbSize, m_Metadata.m_ReverbSizeLimitsForTightSpaceLevel.x,
// 											m_Metadata.m_ReverbSizeLimitsForTightSpaceLevel.y, 0.0f, 1.0f);
// 			reverbSizeTvalue			= SlowInOut(reverbSizeTvalue);
// 			const float reverbSizeLevel	= Lerp(reverbSizeTvalue, m_Metadata.m_TightSpaceLevelAtReverbSizeLimits.x,
// 											m_Metadata.m_TightSpaceLevelAtReverbSizeLimits.y);
// 
// 			//NOTE: We do not take the reverb wetness into account when interior-specific reverb settings are used, as these settings can specify
// 			//unusually low wetness, which would otherwise skew our blend level.
// 			float reverbWetnessScaling = 1.0f;
// 			const bool isUsingInteriorSpecificSettings = (environmentGroup->GetInteriorSettings() != NULL);
// 			if(!isUsingInteriorSpecificSettings)
// 			{
// 				float reverbWetnessTvalue	= RampValueSafe(reverbWetness, m_Metadata.m_ReverbWetLimitsForTightSpaceLevel.x,
// 												m_Metadata.m_ReverbWetLimitsForTightSpaceLevel.y, 0.0f, 1.0f);
// 				reverbWetnessTvalue			= SlowInOut(reverbWetnessTvalue);
// 				reverbWetnessScaling		= Lerp(reverbWetnessTvalue, m_Metadata.m_TightSpaceLevelScalingAtReverbWetLimits.x,
// 												m_Metadata.m_TightSpaceLevelScalingAtReverbWetLimits.y);
// 			}
// 
// 			m_TightSpaceLevel = reverbSizeLevel * reverbWetnessScaling;
// 		}
// 	}
}

void camGameplayDirector::UpdateStartFollowingPed(const CVehicle* followVehicle, const CPed& followPed)
{
	tGameplayCameraSettings cameraSettings;
	ComputeFollowPedCameraSettings(followPed, cameraSettings);

	const bool wasInterpolatingFromFollowVehicleToFollowPed	= m_ActiveCamera && m_ActiveCamera->GetIsClassId(camFollowPedCamera::GetStaticClassId()) &&
																m_PreviousCamera && m_PreviousCamera->GetIsClassId(camFollowVehicleCamera::GetStaticClassId());

#if FPS_MODE_SUPPORTED
	if( m_ActiveCamera && m_ActiveCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()) &&
		m_ActiveCamera->GetNameHash() == cameraSettings.m_CameraHash )
	{
		// We were forcing an first person camera in a vehicle (to play the animated camera data)
		// and now we are trying to create an on foot camera.  In this case, early out and use the same camera.
		// TODO: generalize and early out whenever the camera we are creating is the same type as the existing one?
		m_State = FOLLOWING_PED;
		return;
	}
#endif

	if(cameraVerifyf(CreateActiveCamera(cameraSettings.m_CameraHash), "Failed to create a camera for following a ped (%u)", cameraSettings.m_CameraHash))
	{
		m_ActiveCamera->AttachTo(cameraSettings.m_AttachEntity);

		if(!cameraSettings.m_Flags.IsFlagSet(Flag_ShouldAllowNextCameraToCloneOrientation))
		{
			m_ActiveCamera->PreventNextCameraCloningOrientation();
		}

		if(!cameraSettings.m_Flags.IsFlagSet(Flag_ShouldAllowInterpolationToNextCamera))
		{
			m_ActiveCamera->PreventInterpolationToNextCamera();
		}

		if(m_ActiveCamera->GetIsClassId(camFollowCamera::GetStaticClassId()))
		{
			camFollowCamera* followPedCamera = (camFollowCamera*)m_ActiveCamera.Get();

			if(m_PreviousCamera)
			{
			#if FPS_MODE_SUPPORTED
				if(GetFirstPersonVehicleCamera(NULL) && m_CustomFovBlendEnabled)
				{
					// Coming from the vehicle pov camera, need to do fov blend.
					((camThirdPersonCamera*)m_ActiveCamera.Get())->StartCustomFovBlend( m_Metadata.m_FirstToThirdPersonFovDelta, m_Metadata.m_FirstToThirdPersonDuration );
				}
			#endif

				if(m_PreviousCamera->GetIsClassId(camFollowVehicleCamera::GetStaticClassId()))
				{
					camFollowVehicleCamera* followVehicleCamera = (camFollowVehicleCamera*)m_PreviousCamera.Get();

					if(NetworkInterface::IsGameInProgress() && m_ShouldPerformCustomTransitionAfterWarpFromVehicle && m_ActiveCamera->GetNameHash() == ATSTRINGHASH("CUSTOM_TRANSITION_AFTER_WARP_SKY_DIVE_CAMERA", 0x73F590F4))
					{
						m_VehicleForCustomTransitionAfterWarp = followVehicleCamera->GetAttachParent();
					}

					Vector3 entryExitFront;
					bool shouldCutToOrientation	= ComputeFrontForVehicleEntryExit(followPed, followVehicle, *followVehicleCamera, *followVehicleCamera, entryExitFront);

					Vector3 pedRelativeEntryExitFront = VEC3V_TO_VECTOR3( followPed.GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(entryExitFront))); //Transform into ped-relative space.

					float relativeHeading;
					float relativePitch;
					camFrame::ComputeHeadingAndPitchFromFront(pedRelativeEntryExitFront, relativeHeading, relativePitch);

					followPedCamera->SetRelativeHeading(relativeHeading);
					followPedCamera->SetRelativePitch(relativePitch);

					//Don't interpolate if warped in or out of a vehicle.
					const bool shouldInterpolate	= ((m_VehicleEntryExitState == ENTERING_VEHICLE) ||
														(m_VehicleEntryExitState == EXITING_VEHICLE));
					if(shouldInterpolate)
					{
						if(shouldCutToOrientation)
						{
							//We need to cut to this orientation, so we must also override the orientation of the previous camera, as this is the
							//interpolation source.
							//Transform into vehicle-relative space.
							Vector3 vehicleRelativeEntryExitFront = VEC3V_TO_VECTOR3(followVehicle->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(entryExitFront)));

							camFrame::ComputeHeadingAndPitchFromFront(vehicleRelativeEntryExitFront, relativeHeading, relativePitch);

							followVehicleCamera->SetRelativeHeading(relativeHeading);
							followVehicleCamera->SetRelativePitch(relativePitch);
						}

						u32 interpolationDuration = m_Metadata.m_FollowVehicleInterpolateDuration;

						const bool isAttachPedUsingRagdoll = followPed.GetUsingRagdoll();
						if(isAttachPedUsingRagdoll)
						{
							interpolationDuration = m_Metadata.m_FollowVehicleInterpolateDurationForRagdollExit;
						}
						else
						{
							const bool isFollowPedBailingOutOfVehicle	= followVehicle && !followVehicle->InheritsFromBike() &&
																			(followVehicle->GetVelocity().Mag() > m_Metadata.m_MinMoveSpeedForBailOutOfVehicle);
							if(isFollowPedBailingOutOfVehicle)
							{
								interpolationDuration = m_Metadata.m_FollowVehicleInterpolateDurationForBailOut;
							}
						}

						m_ActiveCamera->InterpolateFromCamera(*m_PreviousCamera, interpolationDuration, true);

						//Ensure that the interpolation is not performed over too large a distance.
						camFrameInterpolator* frameInterpolator = m_ActiveCamera->GetFrameInterpolator();
						if(frameInterpolator)
						{
							frameInterpolator->SetMaxDistanceToTravel(m_Metadata.m_MaxDistanceToInterpolateOnVehicleEntryExit);
						}
					}
				}
				else
				{
					if(cameraSettings.m_Flags.IsFlagSet(Flag_ShouldClonePreviousOrientation))
					{
						followPedCamera->CloneBaseOrientation(*m_PreviousCamera);
						//Also clone the base position in order to preserve the desired orbit distance.
						followPedCamera->CloneBasePosition(*m_PreviousCamera);
					}
					else if(cameraSettings.m_Flags.IsFlagSet(Flag_ShouldFallBackToIdealHeading))
					{
						followPedCamera->SetRelativeHeading(0.0f, 1.0f, true, true);
					}

					if(cameraSettings.m_Flags.IsFlagSet(Flag_ShouldClonePreviousControlSpeeds))
					{
						followPedCamera->CloneControlSpeeds(*m_PreviousCamera);
					}

					if(m_PreviousCamera->GetIsClassId(camFollowCamera::GetStaticClassId()))
					{
						//NOTE: A negative duration indicates that the default defined in metadata should be applied.
						const u32 interpolateDuration	= (cameraSettings.m_InterpolationDuration >= 0) ? (u32)cameraSettings.m_InterpolationDuration :
															m_Metadata.m_InterpolateDurationForMoveBlendTransition;
						m_ActiveCamera->InterpolateFromCamera(*m_PreviousCamera, interpolateDuration, true);

						//Ensure that the interpolation is not performed over too large an orientation delta.
						camFrameInterpolator* frameInterpolator = m_ActiveCamera->GetFrameInterpolator();
						if(frameInterpolator)
						{
							//NOTE: A negative limit indicates that the default defined in metadata should be applied.
							const float maxOrientationDelta	= (cameraSettings.m_MaxOrientationDeltaToInterpolate >= 0.0f) ? 
																cameraSettings.m_MaxOrientationDeltaToInterpolate :
																(m_Metadata.m_MaxOrientationDeltaToInterpolateOnMoveBlendTransition * DtoR);
							frameInterpolator->SetMaxOrientationDelta(maxOrientationDelta);
						}
					}
					else if(m_PreviousCamera->GetIsClassId(camThirdPersonAimCamera::GetStaticClassId()))
					{
						//Always interpolate from third-person aim to follow-ped.
						const bool wasAssistedAiming	= m_PreviousCamera->GetIsClassId(camThirdPersonPedAssistedAimCamera::GetStaticClassId());
						const bool wasMeleeAiming		= m_PreviousCamera->GetIsClassId(camThirdPersonPedMeleeAimCamera::GetStaticClassId());
						const bool wasAimingUsingFreeAimImprovements = m_PreviousCamera->GetIsClassId(camThirdPersonPedAimCamera::GetStaticClassId()) && CPlayerPedTargeting::GetAreCNCFreeAimImprovementsEnabled(&followPed);

						u32 interpolationDuration = m_Metadata.m_AimWeaponInterpolateOutDuration;
						if (wasAssistedAiming)
						{
							interpolationDuration = m_Metadata.m_AssistedAimingInterpolateOutDuration;
						}
						else if (wasMeleeAiming)
						{
							interpolationDuration = m_Metadata.m_MeleeAimingInterpolateOutDuration;
						}
						else if (wasAimingUsingFreeAimImprovements)
						{
							interpolationDuration = m_Metadata.m_AimWeaponInterpolateOutDurationFreeAimNew;
						}

						CachePreviousCameraOrientation( m_PreviousCamera->GetFrameNoPostEffects() );
						m_ActiveCamera->InterpolateFromCamera(*m_PreviousCamera, interpolationDuration, true);

						if(wasMeleeAiming)
						{
							camFrameInterpolator* frameInterpolator = m_ActiveCamera->GetFrameInterpolator();
							if(frameInterpolator)
							{
								frameInterpolator->SetCurveTypeForFrameComponent(camFrame::CAM_FRAME_COMPONENT_DOF, camFrameInterpolator::ACCELERATION);
							}
						}

						followPedCamera->PersistAimBehaviour();
					}
					else if(m_PreviousCamera->GetIsClassId(camFirstPersonAimCamera::GetStaticClassId()))
					{
					#if FPS_MODE_SUPPORTED
						if( m_PreviousCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()) && m_CustomFovBlendEnabled )
						{
							((camThirdPersonCamera*)m_ActiveCamera.Get())->StartCustomFovBlend( m_Metadata.m_FirstToThirdPersonFovDelta, m_Metadata.m_FirstToThirdPersonDuration );
						}
					#endif

						CachePreviousCameraOrientation( m_PreviousCamera->GetFrameNoPostEffects() );
					}
					else if (m_PreviousCamera->GetIsClassId(camTableGamesCamera::GetStaticClassId()))
					{
						UpdateBlendOutTableGamesCamera(cameraSettings, *followPedCamera, static_cast<camTableGamesCamera&>(*m_PreviousCamera.Get()));
					}
				}
			}
		}
		else if(m_ActiveCamera->GetIsClassId(camFirstPersonAimCamera::GetStaticClassId()))
		{
#if FPS_MODE_SUPPORTED
#if ! __NO_OUTPUT
			{
				s32 viewModeContextToTest = camControlHelperMetadataViewMode::ON_FOOT;
				if(m_FollowVehicle != NULL)
				{
					viewModeContextToTest = camControlHelperMetadataViewMode::IN_VEHICLE;

					// When in a vehicle, check the vehicle context so we can allow running the first person camera
					// if the vehicle pov camera is active.  This is to allow running an animated camera on first person camera.
					// Assign a vehicle-class-specific view mode context where appropriate for our follow vehicle.
					if(m_FollowVehicle->InheritsFromBike() || m_FollowVehicle->InheritsFromQuadBike() || m_FollowVehicle->InheritsFromAmphibiousQuadBike())
					{
						viewModeContextToTest = camControlHelperMetadataViewMode::ON_BIKE;
					}
					else if(m_FollowVehicle->InheritsFromBoat() || m_FollowVehicle->GetIsJetSki() || m_FollowVehicle->InheritsFromAmphibiousAutomobile() || m_FollowVehicle->InheritsFromAmphibiousQuadBike())
					{
						viewModeContextToTest = camControlHelperMetadataViewMode::IN_BOAT;
					}
					else if(m_FollowVehicle->InheritsFromHeli())
					{
						viewModeContextToTest = camControlHelperMetadataViewMode::IN_HELI;
					}
					else if(m_FollowVehicle->GetIsAircraft() || m_FollowVehicle->InheritsFromBlimp())
					{
						viewModeContextToTest = camControlHelperMetadataViewMode::IN_AIRCRAFT;
					}
					else if(m_FollowVehicle->InheritsFromSubmarine())
					{
						viewModeContextToTest = camControlHelperMetadataViewMode::IN_SUBMARINE;
					}
				}

				s32 currentViewMode = GetActiveViewMode( viewModeContextToTest );

				cameraDebugf1("Created a FP camera %s(%u) of type %s. IsFirstPersonModeAllowed %d. ForceFPBonnetCam %d. OverrideFPCamThisUpdate %d. ViewModeContext %d. CurrentViewMode %d", 
					m_ActiveCamera->GetName(), 
					m_ActiveCamera->GetNameHash(), 
					m_ActiveCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()) ? "camFirstPersonShooterCamera" : "camFirstPersonAimCamera",
					m_IsFirstPersonModeAllowed,
					m_ForceFirstPersonBonnetCamera,
					m_OverrideFirstPersonCameraThisUpdate,
					viewModeContextToTest,
					currentViewMode);

			}
#endif //! _NO_OUTPUT
#endif //FPS_MODE_SUPPORTED

			if(m_PreviousCamera)
			{
				camFirstPersonAimCamera* firstPersonAimCamera = static_cast<camFirstPersonAimCamera*>(m_ActiveCamera.Get());

				if(cameraSettings.m_Flags.IsFlagSet(Flag_ShouldClonePreviousOrientation))
				{
				#if FPS_MODE_SUPPORTED 
					if( m_ActiveCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()) &&
						m_PreviousCamera->GetIsClassId(camFollowVehicleCamera::GetStaticClassId()) )
					{
						firstPersonAimCamera->CloneBaseOrientationFromSeat(*m_PreviousCamera);
					}
					else
				#endif
					{
						firstPersonAimCamera->CloneBaseOrientation(*m_PreviousCamera);
					}
				}

				if(cameraSettings.m_Flags.IsFlagSet(Flag_ShouldClonePreviousControlSpeeds))
				{
					firstPersonAimCamera->CloneControlSpeeds(*m_PreviousCamera);
				}

			#if FPS_MODE_SUPPORTED 
				if( m_ActiveCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()) )
				{
					if( m_PreviousCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()) )
					{
						// To be safe, if the first person camera is being recreated (i.e. for a weapon change)
						// clone the previous camera's settings.
						m_ActiveCamera->GetFrameNonConst().CloneFrom( m_PreviousCamera->GetFrame() );

						if (IsTableGamesCameraRunning())
						{
							UpdateBlendInTableGamesCamera(cameraSettings, *m_ActiveCamera.Get(), *m_PreviousCamera.Get());
						}
						else if (WasTableGamesCameraRunning())
						{
							UpdateBlendOutTableGamesCamera(cameraSettings, *m_ActiveCamera.Get(), *m_PreviousCamera.Get());
						}
					}
					else if( m_PreviousCamera->GetIsClassId(camThirdPersonCamera::GetStaticClassId()) &&
							!m_PreviousCamera->GetIsClassId(camFollowVehicleCamera::GetStaticClassId()) )
					{
						((camFirstPersonShooterCamera*)m_ActiveCamera.Get())->StartCustomFovBlend( m_UseSwitchCustomFovBlend ? m_Metadata.m_SwitchThirdToFirstPersonFovDelta : m_Metadata.m_ThirdToFirstPersonFovDelta,  m_UseSwitchCustomFovBlend ? m_Metadata.m_SwitchThirdToFirstPersonDuration :m_Metadata.m_ThirdToFirstPersonDuration );
						m_UseSwitchCustomFovBlend = false;

						if( m_PreviousCamera->GetIsClassId(camThirdPersonPedAimCamera::GetStaticClassId()) ||
							m_PreviousCamera->GetIsClassId(camFollowCamera::GetStaticClassId()) )
						{
							CachePreviousCameraOrientation( m_PreviousCamera->GetFrameNoPostEffects() );
						}
					}
					else if( m_PreviousCamera->GetIsClassId(camFirstPersonPedAimCamera::GetStaticClassId()) )
					{
						// Handle transition from sniper aim to first person shooter camera.
						m_JustSwitchedFromSniperScope = true;

						CachePreviousCameraOrientation( m_PreviousCamera->GetFrameNoPostEffects() );
					}
				}
				else if( m_ActiveCamera->GetIsClassId(camFirstPersonPedAimCamera::GetStaticClassId()) )
				{
					// Handle transition from first person shooter to sniper aim camera.
					CachePreviousCameraOrientation( m_PreviousCamera->GetFrameNoPostEffects() );
				}
			#endif // FPS_MODE_SUPPORTED

				//NOTE: We do not interpolate into first-person aim cameras.

				if (m_PreviousCamera->GetIsClassId(camTableGamesCamera::GetStaticClassId()))
				{
					UpdateBlendOutTableGamesCamera(cameraSettings, *m_ActiveCamera.Get(), static_cast<camTableGamesCamera&>(*m_PreviousCamera.Get()));
				}
			}
		}
		else if(m_ActiveCamera->GetIsClassId(camFirstPersonHeadTrackingAimCamera::GetStaticClassId()))
		{
			if(m_PreviousCamera)
			{
				camFirstPersonHeadTrackingAimCamera* firstPersonAimCamera = static_cast<camFirstPersonHeadTrackingAimCamera*>(m_ActiveCamera.Get());

				if(cameraSettings.m_Flags.IsFlagSet(Flag_ShouldClonePreviousOrientation))
				{
					firstPersonAimCamera->CloneBaseOrientation(*m_PreviousCamera);
				}

				if(cameraSettings.m_Flags.IsFlagSet(Flag_ShouldClonePreviousControlSpeeds))
				{
					firstPersonAimCamera->CloneControlSpeeds(*m_PreviousCamera);
				}

				//NOTE: We do not interpolate into first-person aim cameras.
			}
		}
		else if(m_ActiveCamera->GetIsClassId(camThirdPersonAimCamera::GetStaticClassId()))
		{
			if(m_PreviousCamera)
			{
				camThirdPersonAimCamera* thirdPersonAimCamera = static_cast<camThirdPersonAimCamera*>(m_ActiveCamera.Get());

				if(cameraSettings.m_Flags.IsFlagSet(Flag_ShouldClonePreviousOrientation))
				{
					thirdPersonAimCamera->CloneBaseOrientation(*m_PreviousCamera, true, wasInterpolatingFromFollowVehicleToFollowPed);
					//Also clone the base position in order to preserve the desired orbit distance.
					thirdPersonAimCamera->CloneBasePosition(*m_PreviousCamera);
				}
				else if(cameraSettings.m_Flags.IsFlagSet(Flag_ShouldFallBackToIdealHeading))
				{
					thirdPersonAimCamera->SetRelativeHeading(0.0f, 1.0f, true, true);
				}

				if(cameraSettings.m_Flags.IsFlagSet(Flag_ShouldClonePreviousControlSpeeds))
				{
					thirdPersonAimCamera->CloneControlSpeeds(*m_PreviousCamera);
				}

				if(m_PreviousCamera->GetIsClassId(camFollowPedCamera::GetStaticClassId()) ||
					m_PreviousCamera->GetIsClassId(camFollowParachuteCamera::GetStaticClassId()))
				{
					camFollowCamera* followCamera = (camFollowCamera*)m_PreviousCamera.Get();

					s32 interpolationDuration = cameraSettings.m_InterpolationDuration;

					const bool shouldInterpolate = ComputeShouldInterpolateIntoAiming(followPed, *followCamera, *thirdPersonAimCamera, interpolationDuration,
													wasInterpolatingFromFollowVehicleToFollowPed);
					if(shouldInterpolate)
					{
						m_ActiveCamera->InterpolateFromCamera(*m_PreviousCamera, static_cast<u32>(interpolationDuration), true);
// 							camFrameInterpolator* frameInterpolator = m_ActiveCamera->GetFrameInterpolator();
// 							if(frameInterpolator)
// 							{
// 								frameInterpolator->SetExtraMotionBlurStrength(m_Metadata.m_AimWeaponInterpolationMotionBlurStrength);
// 							}

						camControlHelper* followCameraControlHelper = followCamera->GetControlHelper();
						if(followCameraControlHelper)
						{
							//Don't allow the follow camera that we're interpolating from to flip in or out of look-behind anymore.
							followCameraControlHelper->DeactivateLookBehindInput();

							camControlHelper* aimCameraControlHelper = thirdPersonAimCamera->GetControlHelper();

							if(followCameraControlHelper->IsLookingBehind() && aimCameraControlHelper && !aimCameraControlHelper->IsUsingLookBehindInput())
							{
								//Flip the follow camera to take account of look-behind and then disable look-behind, so that the interpolation into
								//the aim camera is based upon coherent base front vectors.

								followCamera->CloneBaseOrientation(*followCamera, false);

								followCameraControlHelper->SetLookBehindState(false);
							}

							//Allow the source follow camera to decelerate more aggressively in order to reduce overshoot during the interpolation.
							followCameraControlHelper->SetLookAroundDecelerationScaling(m_Metadata.m_AimWeaponInterpolationSourceLookAroundDecelerationScaling);
						}

						followCamera->IgnoreAttachParentMovementForOrientation();

						if(thirdPersonAimCamera->GetIsClassId(camThirdPersonPedAimInCoverCamera::GetStaticClassId()))
						{
							if(!m_UseScriptRelativeHeading && !m_UseScriptRelativePitch)
							{
								float coverAlignmentHeading;
								const bool shouldAlignToCover = ComputeCoverEntryAlignmentHeading(*followCamera, coverAlignmentHeading);
								if(shouldAlignToCover)
								{
									Vector3 coverAlignmentFront;
									camFrame::ComputeFrontFromHeadingAndPitch(coverAlignmentHeading, 0.0f, coverAlignmentFront);

									const Vector3 pedRelativeCoverAlignmentFront = VEC3V_TO_VECTOR3(followPed.GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(coverAlignmentFront)));

									float relativeHeading;
									float relativePitch;
									camFrame::ComputeHeadingAndPitchFromFront(pedRelativeCoverAlignmentFront, relativeHeading, relativePitch);

									thirdPersonAimCamera->SetRelativeHeading(relativeHeading);
									thirdPersonAimCamera->SetRelativePitch(relativePitch);
								}
							}
						}

					#if FPS_MODE_SUPPORTED
						{
							CachePreviousCameraOrientation( followCamera->GetFrameNoPostEffects() );
						}
					#endif
					}
				}
				else if(m_PreviousCamera->GetIsClassId(camThirdPersonAimCamera::GetStaticClassId()))
				{
					//If the tasks have not requested a custom interpolation duration, use the minimum interpolation duration to mask any glitches.
					const u32 interpolationDuration	= (cameraSettings.m_InterpolationDuration >= 0) ?
														static_cast<u32>(cameraSettings.m_InterpolationDuration) :
														m_Metadata.m_MinAimWeaponInterpolateInDuration;
					m_ActiveCamera->InterpolateFromCamera(*m_PreviousCamera, interpolationDuration, true);

					//Clone the accurate mode state of the previous aim camera.
					const camControlHelper* previousControlHelper	= static_cast<camThirdPersonAimCamera*>(m_PreviousCamera.Get())->GetControlHelper();
					camControlHelper* activeControlHelper			= thirdPersonAimCamera->GetControlHelper();
					if(previousControlHelper && activeControlHelper)
					{
						const bool wasInAccurateMode = previousControlHelper->IsInAccurateMode();

						activeControlHelper->SetAccurateModeState(wasInAccurateMode);
					}
				}
			#if FPS_MODE_SUPPORTED
				////else if(GetFirstPersonVehicleCamera(NULL) && m_CustomFovBlendEnabled)
				////{
				////	// Coming from the vehicle pov camera, need to do fov blend.  (in case come out of vehicle camera and player is aiming)
				////	((camThirdPersonCamera*)m_ActiveCamera.Get())->StartCustomFovBlend( m_Metadata.m_VehPovToThirdPersonFovDelta, m_Metadata.m_VehPovToThirdPersonDuration );
				////}
				else if( m_PreviousCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()) && m_CustomFovBlendEnabled )
				{
					((camThirdPersonCamera*)m_ActiveCamera.Get())->StartCustomFovBlend( m_Metadata.m_FirstToThirdPersonFovDelta, m_Metadata.m_FirstToThirdPersonDuration );

					if( m_ActiveCamera->GetIsClassId(camThirdPersonPedAimCamera::GetStaticClassId()) )
					{
						CachePreviousCameraOrientation( m_PreviousCamera->GetFrameNoPostEffects() );
					}
				}
			#endif
			}
		}
		else if (m_ActiveCamera->GetIsClassId(camTableGamesCamera::GetStaticClassId()))
		{
			if (m_PreviousCamera)
			{
				UpdateBlendInTableGamesCamera(cameraSettings, *m_ActiveCamera.Get(), *m_PreviousCamera.Get());
			}
		}

		m_ShouldPerformCustomTransitionAfterWarpFromVehicle = false; //For safety, this is reset after any successful transition to following a ped

		m_State = FOLLOWING_PED;
	}
	else
	{
		//Fall-back to a safe state.
		m_State = DOING_NOTHING;
	}
}

void camGameplayDirector::UpdateBlendInTableGamesCamera(const tGameplayCameraSettings& cameraSettings, camBaseCamera& activeCamera, camBaseCamera& previousCamera)
{
	if (previousCamera.GetIsClassId(camThirdPersonCamera::GetStaticClassId()))
	{
		camThirdPersonCamera& previousThirdPersonCamera = static_cast<camThirdPersonCamera&>(previousCamera);
		if (activeCamera.GetIsClassId(camTableGamesCamera::GetStaticClassId()))
		{
			camTableGamesCamera& tableGamesCamera = static_cast<camTableGamesCamera&>(activeCamera);

			if (cameraSettings.m_Flags.IsFlagSet(Flag_ShouldClonePreviousOrientation))
			{
				tableGamesCamera.CloneBaseOrientation(previousThirdPersonCamera, true, false);
				//Also clone the base position in order to preserve the desired orbit distance.
				tableGamesCamera.CloneBasePosition(previousThirdPersonCamera);
			}

			camControlHelper* previousCameraControlHelper	= previousThirdPersonCamera.GetControlHelper();
			camControlHelper* tableGamesCameraControlHelper = tableGamesCamera.GetControlHelper();
			if (previousCameraControlHelper && tableGamesCameraControlHelper)
			{
				//Don't allow the previous third person camera that we're interpolating from to flip in or out of look-behind anymore.
				previousCameraControlHelper->DeactivateLookBehindInput();

				if (previousCameraControlHelper->IsLookingBehind() && tableGamesCameraControlHelper && !tableGamesCameraControlHelper->IsUsingLookBehindInput())
				{
					//Flip the previous camera to take account of look-behind and then disable look-behind, so that the interpolation into
					//the table games camera is based upon coherent base front vectors.

					previousThirdPersonCamera.CloneBaseOrientation(previousThirdPersonCamera, false);

					previousCameraControlHelper->SetLookBehindState(false);
				}
			}

			tableGamesCamera.InterpolateFromCamera(previousThirdPersonCamera, cameraSettings.m_InterpolationDuration, true);
		}
	}
#if FPS_MODE_SUPPORTED
	else if (previousCamera.GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()))
	{
		if (activeCamera.GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()))
		{
			camFirstPersonShooterCamera& firstPersonCamera = static_cast<camFirstPersonShooterCamera&>(activeCamera);

			if (cameraSettings.m_Flags.IsFlagSet(Flag_ShouldClonePreviousOrientation))
			{
				firstPersonCamera.CloneBaseOrientation(previousCamera);
			}

			if (cameraSettings.m_Flags.IsFlagSet(Flag_ShouldClonePreviousControlSpeeds))
			{
				firstPersonCamera.CloneControlSpeeds(previousCamera);
			}

			CachePreviousCameraOrientation(previousCamera.GetFrameNoPostEffects());

			TUNE_GROUP_INT(TABLE_GAMES_CAMERAS, iFirstPersonBlendInDuration, 500, 0, 9999, 1);
			activeCamera.InterpolateFromCamera(previousCamera, iFirstPersonBlendInDuration, true);
		}
	}
#endif
}

void camGameplayDirector::UpdateBlendOutTableGamesCamera(const tGameplayCameraSettings& cameraSettings, camBaseCamera& activeCamera, camBaseCamera& previousCamera)
{
	m_CachedPreviousCameraTableGamesCameraHash = m_ScriptOverriddenTableGamesCameraSettingsOnPreviousUpdate.m_CameraHash;

	if (activeCamera.GetIsClassId(camFollowCamera::GetStaticClassId()))
	{
		camFollowCamera& followCamera = static_cast<camFollowCamera&>(activeCamera);

		if (cameraSettings.m_Flags.IsFlagSet(Flag_ShouldClonePreviousOrientation))
		{
			followCamera.CloneBaseOrientation(previousCamera, true, false);
			//Also clone the base position in order to preserve the desired orbit distance.
			followCamera.CloneBasePosition(previousCamera);
		}

		followCamera.InterpolateFromCamera(previousCamera, cameraSettings.m_InterpolationDuration, true);
	}
#if FPS_MODE_SUPPORTED
	else if (activeCamera.GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()))
	{
		camFirstPersonShooterCamera& firstPersonCamera = static_cast<camFirstPersonShooterCamera&>(activeCamera);

		if (cameraSettings.m_Flags.IsFlagSet(Flag_ShouldClonePreviousOrientation))
		{
			firstPersonCamera.CloneBaseOrientation(previousCamera);
		}

		if (cameraSettings.m_Flags.IsFlagSet(Flag_ShouldClonePreviousControlSpeeds))
		{
			firstPersonCamera.CloneControlSpeeds(previousCamera);
		}

		CachePreviousCameraOrientation(previousCamera.GetFrameNoPostEffects());

		if (previousCamera.GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()))
		{
			TUNE_GROUP_INT(TABLE_GAMES_CAMERAS, iFirstPersonBlendOutDuration, 500, 0, 9999, 1);
			activeCamera.InterpolateFromCamera(previousCamera, iFirstPersonBlendOutDuration, true);
		}
	}
#endif
}

bool camGameplayDirector::UpdateFollowingPed(const CPed& followPed)
{
	const CPedIntelligence* followPedIntelligence = followPed.GetPedIntelligence();
	const CTaskInVehicleSeatShuffle* taskInVehicleSeatShuffle = NULL; 
	if(followPedIntelligence)
	{	
		taskInVehicleSeatShuffle = static_cast<CTaskInVehicleSeatShuffle*>(followPedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE));		
	}

#if FPS_MODE_SUPPORTED
	bool bDelayTurretEntryForFpsCamera = IsDelayTurretEntryForFpsCamera();
#endif // FPS_MODE_SUPPORTED

	//Check for turret entry
	if( m_TurretAligned &&  FPS_MODE_SUPPORTED_ONLY(!bDelayTurretEntryForFpsCamera &&)
		m_VehicleEntryExitState == INSIDE_VEHICLE && IsPedInOrEnteringTurretSeat(m_FollowVehicle, followPed) )
	{
		m_State = START_MOUNTING_TURRET;
	}
	//Check for vehicle entry.
	else if(((m_VehicleEntryExitState == ENTERING_VEHICLE || m_VehicleEntryExitState == INSIDE_VEHICLE) && // Entering or inside a vehicle...
		!IsPedInOrEnteringTurretSeat(m_FollowVehicle, followPed, false, true)) ||  // but not a turret seat.
		(taskInVehicleSeatShuffle && m_VehicleEntryExitState != EXITING_VEHICLE)) // Transition to FOLLOWING_VEHICLE immediately upon seat shuffle, unless exiting
	{				
		m_State = START_FOLLOWING_VEHICLE;	
	}	
	//Check if the active follow-ped camera needs to be recreated to reflect a change in the follow ped's state.
	else if(ComputeShouldRecreateFollowPedCamera(followPed))
	{
		m_State = START_FOLLOWING_PED;
	}
	//Default to a safe state if we don't have an active camera.
	else if(!m_ActiveCamera)
	{
		m_State = DOING_NOTHING;
	}

	if (rage::ioHeadTracking::UseFPSCamera())
	{
		//if( settings.m_CameraHash != ATSTRINGHASH("SNIPER_AIM_CAMERA", 0xEC22D348) &&
		//	settings.m_CameraHash != ATSTRINGHASH("SNIPER_LOW_ZOOM_AIM_CAMERA", 0x8C40FB35) )
		if (!m_ActiveCamera->GetIsClassId(camFirstPersonPedAimCamera::GetStaticClassId()))
		{
			m_State = START_HEAD_TRACKING;
		}
	}

	return (m_State != FOLLOWING_PED);
}

void camGameplayDirector::UpdateStartHeadTracking(const CVehicle* /*followVehicle*/, const CPed& followPed)
{
	tGameplayCameraSettings cameraSettings;
	cameraSettings.m_CameraHash = m_Metadata.m_FirstPersonHeadTrackingAimCameraRef;

	if(cameraVerifyf(CreateActiveCamera(cameraSettings.m_CameraHash), "Failed to create a head tracking camera (%u)",
		cameraSettings.m_CameraHash) && cameraVerifyf(m_ActiveCamera->GetIsClassId(camFirstPersonHeadTrackingAimCamera::GetStaticClassId()),
		"Created a camera of an incorrect type (%s) for following a vehicle", m_ActiveCamera->GetClassId().GetCStr()))
	{
		m_ActiveCamera->AttachTo(&followPed);
	
		m_State = HEAD_TRACKING;
	}
	else
	{
		//Fall-back to a safe state.
		m_State = DOING_NOTHING;
	}
}

bool camGameplayDirector::UpdateHeadTracking(const CPed& followPed)
{
	if (!rage::ioHeadTracking::UseFPSCamera())
	{
		m_State = DOING_NOTHING;
	}

	CControl* control = GetActiveControl();
	const bool bIsReallyAiming = control && control->GetPedTargetIsDown();
	const bool bWeaponHasFirstPersonScope = followPed.GetWeaponManager() ? followPed.GetWeaponManager()->GetEquippedWeaponHasFirstPersonScope() : false;
	if ( bWeaponHasFirstPersonScope && bIsReallyAiming )
	{
		m_State = START_FOLLOWING_PED;
	}

	//Default to a safe state if we don't have an active camera.
	if(!m_ActiveCamera)
	{
		m_State = DOING_NOTHING;
	}

	return (m_State != HEAD_TRACKING);
}


void camGameplayDirector::UpdateStartFollowingVehicle(const CVehicle* followVehicle)
{
	if(!followVehicle)
	{
		//Fall-back to a safe state.
		m_State = DOING_NOTHING;
		return;
	}

	tGameplayCameraSettings cameraSettings;
	ComputeFollowVehicleCameraSettings(*followVehicle, cameraSettings);

#if FPS_MODE_SUPPORTED
	if( m_ActiveCamera && m_ActiveCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()) &&
		(cameraSettings.m_CameraHash == m_ActiveCamera->GetNameHash()) )
	{
		// We are supposed to be in first person shooter camera and one already exists, so continue using the same one.
		// (fixes pop when new first person shooter camera resets orientation)
		m_State = FOLLOWING_VEHICLE;
		return;
	}
#endif

	if( cameraVerifyf(CreateActiveCamera(cameraSettings.m_CameraHash), "Failed to create a follow-vehicle camera (%u)", cameraSettings.m_CameraHash) &&
		cameraVerifyf(	m_ActiveCamera->GetIsClassId(camFollowVehicleCamera::GetStaticClassId())
			FPS_MODE_SUPPORTED_ONLY(|| m_ActiveCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId())),
		 "Created a camera of an incorrect type (%s) for following a vehicle", m_ActiveCamera->GetClassId().GetCStr()) )
	{
		m_ActiveCamera->AttachTo(cameraSettings.m_AttachEntity);

		if(!cameraSettings.m_Flags.IsFlagSet(Flag_ShouldAllowNextCameraToCloneOrientation))
		{
			m_ActiveCamera->PreventNextCameraCloningOrientation();
		}

		if(!cameraSettings.m_Flags.IsFlagSet(Flag_ShouldAllowInterpolationToNextCamera))
		{
			m_ActiveCamera->PreventInterpolationToNextCamera();
		}

		camFollowVehicleCamera* followVehicleCamera = (m_ActiveCamera->GetIsClassId(camFollowVehicleCamera::GetStaticClassId())) ?
														(camFollowVehicleCamera*)m_ActiveCamera.Get() : NULL;

		const CVehicleModelInfo* followVehicleModelInfo = followVehicle->GetVehicleModelInfo();
		if(followVehicleModelInfo && followVehicleCamera)
		{
			const u32 followVehicleModelNameHash					= followVehicleModelInfo->GetModelNameHash();
			const camVehicleCustomSettingsMetadata* customSettings	= FindVehicleCustomSettings(followVehicleModelNameHash);
			if(customSettings)
			{
				followVehicleCamera->SetVehicleCustomSettings(customSettings);
			}
		}

		const bool isUsingStuntSettings = m_UseVehicleCamStuntSettings || m_PreviousUseVehicleCamStuntSettings;
		if(followVehicleCamera && m_UseVehicleCamStuntSettings && !m_ScriptRequestDedicatedStuntCameraThisUpdate)
		{
			followVehicleCamera->SetShouldUseStuntSettings(true);
		}

		if(m_PreviousCamera && followVehicleCamera)
		{
			//B*1827739: Set cinematic camera if we're warping into vehicle.
			const CPed* followPed = camInterface::FindFollowPed();
			bool bIsWarpingToSeat = followPed ? followPed->GetPedResetFlag(CPED_RESET_FLAG_IsWarpingIntoVehicleMP) : false;
		
			if(cameraSettings.m_Flags.IsFlagSet(Flag_ShouldClonePreviousOrientation) && !bIsWarpingToSeat)
			{
				followVehicleCamera->CloneBaseOrientation(*m_PreviousCamera);
			}
			else if(cameraSettings.m_Flags.IsFlagSet(Flag_ShouldFallBackToIdealHeading))
			{
				followVehicleCamera->SetRelativeHeading(0.0f, 1.0f, true, true);
			}

			if(cameraSettings.m_Flags.IsFlagSet(Flag_ShouldClonePreviousControlSpeeds))
			{
				followVehicleCamera->CloneControlSpeeds(*m_PreviousCamera);
			}

			const bool wasTurretCamera = (m_PreviousCamera->GetIsClassId(camThirdPersonPedAimCamera::GetStaticClassId()) && static_cast<const camThirdPersonPedAimCamera*>(m_PreviousCamera.Get())->GetIsTurretCam());
			const bool isPedInTurretSeat = IsPedInOrEnteringTurretSeat(followVehicle, *followPed);
			
			const bool isBlendingStuntSettings = !camInterface::IsRenderingFirstPersonCamera() && isUsingStuntSettings;

			if(m_PreviousCamera->GetNameHash() == m_Metadata.m_FollowVehicleParachuteCameraRef)
			{
				//make sure we apply the same interpolation when we blend out of the parachute vehicle camera.
				cameraSettings.m_InterpolationDuration = m_Metadata.m_FollowVehicleParachuteCameraInterpolation;
			}

			if(m_PreviousCamera->GetIsClassId(camFollowPedCamera::GetStaticClassId()) || wasTurretCamera || isUsingStuntSettings)
			{
				//Don't interpolate if warped in or out of a vehicle.
				const bool shouldInterpolate =	m_VehicleEntryExitState == ENTERING_VEHICLE || 
												m_VehicleEntryExitState == EXITING_VEHICLE || 
												(m_VehicleEntryExitState == INSIDE_VEHICLE && (
													isPedInTurretSeat || // but do interpolate if we've climbed in a side door to a turret seat
													wasTurretCamera || // ... or shuffled from a turret seat...
													isBlendingStuntSettings) // .. or we are blending stunt settings
												);

				const bool wasOnTulaTurret = wasTurretCamera && m_FollowVehicle->GetVehicleModelInfo() && m_FollowVehicle->GetVehicleModelInfo()->GetModelNameHash() == ATSTRINGHASH("TULA", 0x3E2E4F8A);

				if(shouldInterpolate)
				{
					static const u32 s_InterpolateDurationFromTulaTurret = 500;

					u32 interpolationDuration = wasOnTulaTurret ? s_InterpolateDurationFromTulaTurret : m_Metadata.m_FollowVehicleInterpolateDuration;
					if(isBlendingStuntSettings && m_UseVehicleCamStuntSettings)
					{
						bool isPreviousCamLookingBehind = false;
						if(m_PreviousCamera->GetIsClassId(camFollowVehicleCamera::GetStaticClassId()))
						{
							const camFollowVehicleCamera* pVehicleCam = static_cast<const camFollowVehicleCamera*>(m_PreviousCamera.Get());
							isPreviousCamLookingBehind = pVehicleCam->GetControlHelper() && pVehicleCam->GetControlHelper()->IsLookingBehind();
						}
						if(isPreviousCamLookingBehind)
						{
							TUNE_GROUP_INT(CAM_STUNT_SETTINGS, iStuntSettingsInterpolationDurationForLookBehind, 500, 100, 10000, 1);
							interpolationDuration = iStuntSettingsInterpolationDurationForLookBehind;
						}
						else
						{
							TUNE_GROUP_INT(CAM_STUNT_SETTINGS, iStuntSettingsInterpolationDuration, 1000, 100, 10000, 1);
							interpolationDuration = iStuntSettingsInterpolationDuration;
						}
					}
					m_ActiveCamera->InterpolateFromCamera(*m_PreviousCamera, interpolationDuration, true);

					//Ensure that the interpolation is not performed over too large a distance.
					camFrameInterpolator* frameInterpolator = m_ActiveCamera->GetFrameInterpolator();
					if(frameInterpolator)
					{
						frameInterpolator->SetMaxDistanceToTravel(m_Metadata.m_MaxDistanceToInterpolateOnVehicleEntryExit);
					}
				}
			}
			else if(m_PreviousCamera->GetIsClassId(camThirdPersonVehicleAimCamera::GetStaticClassId()))
			{
				m_ActiveCamera->InterpolateFromCamera(*m_PreviousCamera, m_Metadata.m_VehicleAimInterpolateOutDuration, true);

				followVehicleCamera->PersistAimBehaviour();
			}
			else if(m_PreviousCamera->GetIsClassId(camFollowVehicleCamera::GetStaticClassId()) && (cameraSettings.m_InterpolationDuration > 0))
			{
				//Allow custom camera settings to request an interpolation between follow-vehicle cameras.
				m_ActiveCamera->InterpolateFromCamera(*m_PreviousCamera, cameraSettings.m_InterpolationDuration, true);
			}
		#if FPS_MODE_SUPPORTED
			else if( m_PreviousCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()) && m_CustomFovBlendEnabled )
			{
				((camThirdPersonCamera*)m_ActiveCamera.Get())->StartCustomFovBlend( m_Metadata.m_FirstToThirdPersonFovDelta, m_Metadata.m_FirstToThirdPersonDuration );
			}
		#endif
		}

		m_State = FOLLOWING_VEHICLE;
	}
	else
	{
		//Fall-back to a safe state.
		m_State = DOING_NOTHING;
	}
}

bool camGameplayDirector::UpdateFollowingVehicle(const CVehicle* followVehicle, const CPed& followPed)
{
	const CPedIntelligence* followPedIntelligence = followPed.GetPedIntelligence();
	const CTaskInVehicleSeatShuffle* taskInVehicleSeatShuffle = NULL; 
	if(followPedIntelligence)
	{	
		taskInVehicleSeatShuffle = static_cast<CTaskInVehicleSeatShuffle*>(followPedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE));		
	}

	bool bFirstPersonAnimatedCameraActive = false;
	bool bFirstPersonVehicleEntryActive = false;
	bool bFirstPersonCameraEnteringTurret = false;
#if FPS_MODE_SUPPORTED
	const camFirstPersonShooterCamera* fpsCamera = GetFirstPersonShooterCamera();
	if( fpsCamera && fpsCamera == m_ActiveCamera.Get() && 
		!camInterface::GetCinematicDirector().IsRenderingAnyInVehicleFirstPersonCinematicCamera() )
	{
		// Delay switching to turret camera if first person camera is running an animated camera, or until 
		// cinematic camera starts.  This allows cinematic camera to blend from the active first person shooter cam.
		bFirstPersonAnimatedCameraActive = fpsCamera->IsUsingAnimatedData() || fpsCamera->IsAnimatedCameraDataBlendingOut();
		bFirstPersonCameraEnteringTurret = fpsCamera->IsEnteringTurretSeat();
		bFirstPersonVehicleEntryActive   = fpsCamera->IsEnteringVehicle() || fpsCamera->IsBlendingForVehicle();
	}
#endif

	bool forceRecreateVehicleCam = false;
	if((!m_FrameInterpolator || !m_FrameInterpolator->IsInterpolating()) && m_ActiveCamera->GetIsClassId(camFollowVehicleCamera::GetStaticClassId()) && m_UseVehicleCamStuntSettings != m_PreviousUseVehicleCamStuntSettings)
	{
		forceRecreateVehicleCam = true;
	}

	//Check for vehicle exit.
	if((m_VehicleEntryExitState == EXITING_VEHICLE) || (m_VehicleEntryExitState == OUTSIDE_VEHICLE))
	{
		m_State = START_FOLLOWING_PED;
	}
	//Check if the active follow-vehicle camera needs to be recreated due to a warp into a new follow vehicle.
	else if(followVehicle && (ComputeShouldRecreateFollowVehicleCamera(*followVehicle) || forceRecreateVehicleCam))
	{
		m_State = START_FOLLOWING_VEHICLE;
		if(!forceRecreateVehicleCam)
		{
			m_UseVehicleCamStuntSettings = false;
			m_PreviousUseVehicleCamStuntSettings = false;
		}
	}
	//Check for turret seat (warped to)
	else if(IsPedInOrEnteringTurretSeat(followVehicle, followPed) && !taskInVehicleSeatShuffle && !bFirstPersonAnimatedCameraActive && !bFirstPersonVehicleEntryActive)
	{
		m_State = START_MOUNTING_TURRET;
	}
	//Check for aiming.
	else if(followVehicle && ComputeIsVehicleAiming(*followVehicle, followPed) && !bFirstPersonCameraEnteringTurret)
	{
		m_State = START_VEHICLE_AIMING;
	}
	//Default to a safe state if we don't have an active camera.
	else if(!m_ActiveCamera)
	{
		m_State = DOING_NOTHING;
	}
	if (rage::ioHeadTracking::UseFPSCamera())
		m_State = START_HEAD_TRACKING;

	if( m_State == FOLLOWING_VEHICLE && GetFirstPersonVehicleCamera(NULL) &&
		m_ActiveCamera && m_ActiveCamera->GetIsClassId(camThirdPersonCamera::GetStaticClassId()) &&
		m_CustomFovBlendEnabled )
	{
		// Since the vehicle pov camera will be deleted after the gameplay director is updated, continuously start the fov blend while the pov vehicle camera is active.
		((camThirdPersonCamera*)m_ActiveCamera.Get())->StartCustomFovBlend( m_Metadata.m_FirstToThirdPersonFovDelta, m_Metadata.m_FirstToThirdPersonDuration );
	}

	return (m_State != FOLLOWING_VEHICLE);
}

void camGameplayDirector::UpdateStartVehicleAiming(const CVehicle* followVehicle, const CPed& followPed)
{
	if(!followVehicle)
	{
		//Fall-back to a safe state.
		m_State = DOING_NOTHING;
		return;
	}

	tGameplayCameraSettings desiredSettings;
	ComputeVehicleAimCameraSettings(*followVehicle, followPed, desiredSettings);

	if(cameraVerifyf(CreateActiveCamera(desiredSettings.m_CameraHash), "Failed to create a vehicle aim camera (hash: %u)",
		desiredSettings.m_CameraHash) && cameraVerifyf(m_ActiveCamera->GetIsClassId(camThirdPersonVehicleAimCamera::GetStaticClassId()) ||
		m_ActiveCamera->GetIsClassId(camThirdPersonPedAimCamera::GetStaticClassId()) || m_ActiveCamera->GetIsClassId(camFirstPersonPedAimCamera::GetStaticClassId()),
		"Created a camera of an incorrect type (%s) for vehicle aiming", m_ActiveCamera->GetClassId().GetCStr()))
	{
		m_ActiveCamera->AttachTo(desiredSettings.m_AttachEntity);

		if(!desiredSettings.m_Flags.IsFlagSet(Flag_ShouldAllowNextCameraToCloneOrientation))
		{
			m_ActiveCamera->PreventNextCameraCloningOrientation();
		}

		if(!desiredSettings.m_Flags.IsFlagSet(Flag_ShouldAllowInterpolationToNextCamera))
		{
			m_ActiveCamera->PreventInterpolationToNextCamera();
		}

		if(m_PreviousCamera)
		{
			if(m_ActiveCamera->GetIsClassId(camFirstPersonPedAimCamera::GetStaticClassId()))
			{
				camFirstPersonPedAimCamera* aimCamera = static_cast<camFirstPersonPedAimCamera*>(m_ActiveCamera.Get());
				aimCamera->CloneBaseOrientation(*m_PreviousCamera);
				aimCamera->CloneControlSpeeds(*m_PreviousCamera);
			}
			else
			{
				camThirdPersonAimCamera* aimCamera = static_cast<camThirdPersonAimCamera*>(m_ActiveCamera.Get());
				aimCamera->CloneBaseOrientation(*m_PreviousCamera);
				aimCamera->CloneControlSpeeds(*m_PreviousCamera);

				// B*2223062: If we're in a car that needs to open rear doors to shoot, set the camera heading to look behind the vehicle.
				bool bInFirstPerson = false;
#if FPS_MODE_SUPPORTED
				bInFirstPerson = IsFirstPersonModeEnabled();
#endif	//FPS_MODE_SUPPORTED
				if (!bInFirstPerson && followVehicle && followVehicle->InheritsFromAutomobile())
				{
					const CVehicleDriveByInfo* pDrivebyInfo = CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(&followPed);
					if (pDrivebyInfo && pDrivebyInfo->GetNeedToOpenDoors())
					{
						static dev_float s_fFiringRangeBuffer = 10.0f * DtoR;
						float fMinYaw = (pDrivebyInfo->GetMinRestrictedAimSweepHeadingAngleDegs(&followPed) * DtoR) + s_fFiringRangeBuffer;
						float fMaxYaw = (pDrivebyInfo->GetMaxRestrictedAimSweepHeadingAngleDegs(&followPed) * DtoR) - s_fFiringRangeBuffer;

						Vector3 vStart = VEC3V_TO_VECTOR3(followPed.GetTransform().GetPosition());
						Vector3 vEnd = m_PreviousCamera->GetFrame().GetPosition() + (m_PreviousCamera->GetFrame().GetFront() * 20.0f);

						// Compute desired yaw angle value
						float fDesiredYaw = CTaskAimGun::CalculateDesiredYaw(&followPed, vStart, vEnd);

						// Only look behind if the player isn't already aiming within the allowed window.
						bool bShouldLookBehind = false; 
						if (fDesiredYaw < fMinYaw || fDesiredYaw > fMaxYaw)
						{
							bShouldLookBehind = true;
						}

						if (bShouldLookBehind)
						{
							float fVehicleAngle = followVehicle->GetTransform().GetHeading();
							float fDesiredAngle = fVehicleAngle - PI;
							fDesiredAngle = fwAngle::LimitRadianAngleFast(fDesiredAngle);
							float fRelativeAngle = fDesiredAngle - fVehicleAngle;
							fRelativeAngle = fwAngle::LimitRadianAngleFast(fRelativeAngle);
							aimCamera->SetRelativeHeading(fRelativeAngle);
						}

					}
				}

				if(m_PreviousCamera->GetIsClassId(camFollowVehicleCamera::GetStaticClassId()) &&
					m_ActiveCamera->GetIsClassId(camThirdPersonVehicleAimCamera::GetStaticClassId()))
				{
					//NOTE: We do not interpolate into vehicle aiming if we are presently rendering a cinematic camera, as we want to cut straight into
					//the gameplay aim camera.
					const camBaseCamera* renderedCinematicCamera = camInterface::GetCinematicDirector().GetRenderedCamera();
					if(renderedCinematicCamera == NULL)
					{
						m_ActiveCamera->InterpolateFromCamera(*m_PreviousCamera, m_Metadata.m_VehicleAimInterpolateInDuration, true);

						camFollowVehicleCamera* followVehicleCamera	= (camFollowVehicleCamera*)m_PreviousCamera.Get();
						camControlHelper* followCameraControlHelper	= followVehicleCamera->GetControlHelper();
						if(followCameraControlHelper)
						{
							//Don't allow the follow camera that we're interpolating from to flip in or out of look-behind anymore.
							followCameraControlHelper->DeactivateLookBehindInput();

							camControlHelper* aimCameraControlHelper = aimCamera->GetControlHelper();

							if(followCameraControlHelper->IsLookingBehind() && aimCameraControlHelper && !aimCameraControlHelper->IsUsingLookBehindInput())
							{
								//Flip the follow camera to take account of look-behind and then disable look-behind, so that the interpolation into
								//the aim camera is based upon coherent base front vectors.

								followVehicleCamera->CloneBaseOrientation(*followVehicleCamera, false);

								followCameraControlHelper->SetLookBehindState(false);
							}
						}

						followVehicleCamera->IgnoreAttachParentMovementForOrientation();
					}
				}
			}
		}

		m_State = VEHICLE_AIMING;
	}
	else
	{
		//Fall-back to a safe state.
		m_State = DOING_NOTHING;
	}
}

bool camGameplayDirector::UpdateVehicleAiming(const CVehicle* followVehicle, const CPed& followPed)
{	
	const CPedIntelligence* followPedIntelligence = followPed.GetPedIntelligence();
	const CTaskInVehicleSeatShuffle* taskInVehicleSeatShuffle = NULL; 
	if(followPedIntelligence)
	{	
		taskInVehicleSeatShuffle = static_cast<CTaskInVehicleSeatShuffle*>(followPedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE));		
	}

	// Check for seat warp to turret seat
	if(IsPedInOrEnteringTurretSeat(followVehicle, followPed) && !taskInVehicleSeatShuffle)
	{
		m_State = START_MOUNTING_TURRET;
	}
	else if(followVehicle && ComputeShouldRecreateVehicleAimCamera(*followVehicle, followPed))
	{
		m_State = START_VEHICLE_AIMING;
	}
	//Check for aim release.
	else if(!followVehicle || !ComputeIsVehicleAiming(*followVehicle, followPed))
	{
		m_State = START_FOLLOWING_VEHICLE;
	}
	//Default to a safe state if we don't have an active camera.
	else if(!m_ActiveCamera)
	{
		m_State = DOING_NOTHING;
	}
	if (rage::ioHeadTracking::UseFPSCamera())
		m_State = START_HEAD_TRACKING;

	return (m_State != VEHICLE_AIMING);
}

void camGameplayDirector::UpdateStartMountingTurret(const CVehicle* followVehicle, const CPed& followPed)
{
	if(!followVehicle)
	{
		//Fall-back to a safe state.
		m_State = DOING_NOTHING;
		return;
	}

	tGameplayCameraSettings cameraSettings;
	if(!ComputeTurretCameraSettings(*followVehicle, followPed, cameraSettings))
	{
		m_State = FOLLOWING_VEHICLE;
		return;
	}

	if (m_ActiveCamera && m_ActiveCamera->GetIsClassId(camThirdPersonPedAimCamera::GetStaticClassId()) && static_cast<camThirdPersonPedAimCamera*>(m_ActiveCamera.Get())->ShouldPreventBlendToItself() && cameraSettings.m_CameraHash == m_ActiveCamera->GetNameHash())
	{
		m_State = TURRET_MOUNTED;
		return;
	}

	if(
		cameraVerifyf(CreateActiveCamera(cameraSettings.m_CameraHash), "Failed to create a turret camera (hash: %u)", cameraSettings.m_CameraHash) &&
		cameraVerifyf(m_ActiveCamera->GetIsClassId(camThirdPersonPedAimCamera::GetStaticClassId()), "Created a camera of an incorrect type (%s) for turret mounting", m_ActiveCamera->GetClassId().GetCStr())
	  )
	{		
		m_ActiveCamera->AttachTo(cameraSettings.m_AttachEntity);
		static_cast<camThirdPersonPedAimCamera*>(m_ActiveCamera.Get())->SetIsTurretCam(true);

		if(!cameraSettings.m_Flags.IsFlagSet(Flag_ShouldAllowNextCameraToCloneOrientation))
		{
			m_ActiveCamera->PreventNextCameraCloningOrientation();
		}

		if(!cameraSettings.m_Flags.IsFlagSet(Flag_ShouldAllowInterpolationToNextCamera))
		{
			m_ActiveCamera->PreventInterpolationToNextCamera();
		}

		if(m_PreviousCamera)
		{	
			camThirdPersonAimCamera* turretCamera = static_cast<camThirdPersonAimCamera*>(m_ActiveCamera.Get());

			if(cameraSettings.m_Flags.IsFlagSet(Flag_ShouldClonePreviousOrientation))
			{
				turretCamera->CloneBaseOrientation(*m_PreviousCamera);
			}

			if(cameraSettings.m_Flags.IsFlagSet(Flag_ShouldClonePreviousControlSpeeds))
			{
				turretCamera->CloneControlSpeeds(*m_PreviousCamera);
			}

			if(m_PreviousCamera->GetIsClassId(camThirdPersonCamera::GetStaticClassId()))
			{
				camThirdPersonCamera* thirdPersonCamera = static_cast<camThirdPersonCamera*>(m_PreviousCamera.Get());
				
				static const u32 s_TurretStartAimingInterpolateInDuration = 500; // Ideally should be in metadata
				u32 interpolationDuration = (m_PreviousState == TURRET_AIMING) ? m_Metadata.m_AimWeaponInterpolateOutDuration : s_TurretStartAimingInterpolateInDuration;
				m_ActiveCamera->InterpolateFromCamera(*m_PreviousCamera, interpolationDuration, true);

				camControlHelper* previousCameraControlHelper = thirdPersonCamera->GetControlHelper();
				if(previousCameraControlHelper)
				{
					//Don't allow the follow camera that we're interpolating from to flip in or out of look-behind anymore.
					previousCameraControlHelper->DeactivateLookBehindInput();

					camControlHelper* turretCameraControlHelper = turretCamera->GetControlHelper();					
					if(previousCameraControlHelper->IsLookingBehind() && turretCameraControlHelper && !turretCameraControlHelper->IsUsingLookBehindInput())
					{
						//Flip the follow camera to take account of look-behind and then disable look-behind, so that the interpolation into
						//the aim camera is based upon coherent base front vectors.
						thirdPersonCamera->CloneBaseOrientation(*thirdPersonCamera, false);
						previousCameraControlHelper->SetLookBehindState(false);
					}

					//Allow the source follow camera to decelerate more aggressively in order to reduce overshoot during the interpolation.
					previousCameraControlHelper->SetLookAroundDecelerationScaling(m_Metadata.m_AimWeaponInterpolationSourceLookAroundDecelerationScaling);
				}
			}
			else if(m_PreviousCamera->GetIsClassId(camAimCamera::GetStaticClassId()))
			{
				camAimCamera* aimCamera = static_cast<camAimCamera*>(m_PreviousCamera.Get());

				camControlHelper* previousCameraControlHelper = aimCamera->GetControlHelper();
				if(previousCameraControlHelper)
				{
					//Don't allow the follow camera that we're interpolating from to flip in or out of look-behind anymore.
					previousCameraControlHelper->DeactivateLookBehindInput();

					camControlHelper* turretCameraControlHelper = turretCamera->GetControlHelper();					
					if(previousCameraControlHelper->IsLookingBehind() && turretCameraControlHelper && !turretCameraControlHelper->IsUsingLookBehindInput())
					{
						//Flip the follow camera to take account of look-behind and then disable look-behind, so that the interpolation into
						//the aim camera is based upon coherent base front vectors.
						aimCamera->CloneBaseOrientation(*aimCamera);
						previousCameraControlHelper->SetLookBehindState(false);
					}

					//Allow the source follow camera to decelerate more aggressively in order to reduce overshoot during the interpolation.
					previousCameraControlHelper->SetLookAroundDecelerationScaling(m_Metadata.m_AimWeaponInterpolationSourceLookAroundDecelerationScaling);
				}
			}
		}

		m_VehicleEntryExitState = INSIDE_VEHICLE;
		m_State = TURRET_MOUNTED;
	}
	else
	{
		//Fall-back to a safe state.
		m_State = DOING_NOTHING;
	}
}

bool camGameplayDirector::UpdateTurretMounted(const CVehicle* followVehicle, const CPed& followPed)
{
	const CPedIntelligence* followPedIntelligence = followPed.GetPedIntelligence();
	const CTaskVehicleMountedWeapon* taskVehicleMountedWeapon = NULL; 
	const CTaskInVehicleSeatShuffle* taskInVehicleSeatShuffle = NULL;
	if(followPedIntelligence)
	{	
		taskVehicleMountedWeapon = static_cast<CTaskVehicleMountedWeapon*>(followPedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_VEHICLE_MOUNTED_WEAPON));
		taskInVehicleSeatShuffle = static_cast<CTaskInVehicleSeatShuffle*>(followPedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE));
	}

	if((m_VehicleEntryExitState == EXITING_VEHICLE || m_VehicleEntryExitState == OUTSIDE_VEHICLE) && !taskInVehicleSeatShuffle)
	{
		m_State = START_FOLLOWING_PED;
	}
	// If we've warped to a non-turret seat or are shuffling to a non turret seat transition to FOLLOW_VEHICLE
	else if((m_VehicleEntryExitState == INSIDE_VEHICLE && !IsPedInOrEnteringTurretSeat(followVehicle, followPed)) || taskInVehicleSeatShuffle)
	{
		m_State = START_FOLLOWING_VEHICLE;
	}
	else if(taskVehicleMountedWeapon && taskVehicleMountedWeapon->WantsToAim(const_cast<CPed*>(&followPed)))
	{
		m_State = START_TURRET_AIMING;
	}	
	else if(taskVehicleMountedWeapon && taskVehicleMountedWeapon->WantsToFire(const_cast<CPed*>(&followPed)))
	{
		m_State = START_TURRET_FIRING;
	}
	else if(!m_ActiveCamera) //Default to a safe state if we don't have an active camera.
	{
		m_State = DOING_NOTHING;
	}

#if FPS_MODE_SUPPORTED
	if( m_State == TURRET_MOUNTED &&
		m_ActiveCamera && m_ActiveCamera->GetIsClassId(camThirdPersonCamera::GetStaticClassId()) &&
		camInterface::GetCinematicDirector().IsRendering() && camInterface::GetCinematicDirector().IsRenderedCameraVehicleTurret())
	{
		// If we are rendering a third person turret camera, and the first person turret camera is active,
		// then we need to trigger an fov blend if the next camera mode button is released.
		const CControl* control = GetActiveControl();
		if(control && control->GetNextCameraMode().IsReleased())
		{
			((camThirdPersonCamera*)m_ActiveCamera.Get())->StartCustomFovBlend( m_Metadata.m_FirstToThirdPersonFovDelta, m_Metadata.m_FirstToThirdPersonDuration );
		}
	}
#endif

	return (m_State != TURRET_MOUNTED);
}

void camGameplayDirector::UpdateStartTurretAiming(const CVehicle* followVehicle, const CPed& followPed)
{
	if(!followVehicle)
	{
		//Fall-back to a safe state.
		m_State = DOING_NOTHING;
		return;
	}

	tGameplayCameraSettings cameraSettings;
	if(!ComputeTurretAimCameraSettings(*followVehicle, followPed, cameraSettings))
	{
		m_State = TURRET_MOUNTED;
		return;
	}		

	if(
		cameraVerifyf(CreateActiveCamera(cameraSettings.m_CameraHash), "Failed to create a turret aim camera (hash: %u)", cameraSettings.m_CameraHash) &&
		cameraVerifyf(m_ActiveCamera->GetIsClassId(camThirdPersonPedAimCamera::GetStaticClassId()), "Created a camera of an incorrect type (%s) for turret aiming", m_ActiveCamera->GetClassId().GetCStr())
		)
	{
		m_ActiveCamera->AttachTo(cameraSettings.m_AttachEntity);
		static_cast<camThirdPersonPedAimCamera*>(m_ActiveCamera.Get())->SetIsTurretCam(true);

		if(!cameraSettings.m_Flags.IsFlagSet(Flag_ShouldAllowNextCameraToCloneOrientation))
		{
			m_ActiveCamera->PreventNextCameraCloningOrientation();
		}

		if(!cameraSettings.m_Flags.IsFlagSet(Flag_ShouldAllowInterpolationToNextCamera))
		{
			m_ActiveCamera->PreventInterpolationToNextCamera();
		}

		if(m_PreviousCamera)
		{			
			camThirdPersonAimCamera* turretAimCamera = static_cast<camThirdPersonAimCamera*>(m_ActiveCamera.Get());

			if(cameraSettings.m_Flags.IsFlagSet(Flag_ShouldClonePreviousOrientation))
			{
				turretAimCamera->CloneBaseOrientation(*m_PreviousCamera);
			}

			if(cameraSettings.m_Flags.IsFlagSet(Flag_ShouldClonePreviousControlSpeeds))
			{
				turretAimCamera->CloneControlSpeeds(*m_PreviousCamera);
			}


			if(m_PreviousCamera->GetIsClassId(camThirdPersonCamera::GetStaticClassId()))
			{
				m_ActiveCamera->InterpolateFromCamera(*m_PreviousCamera, m_Metadata.m_MinAimWeaponInterpolateInDuration, true);
				
				//Ensure that the interpolation is not performed over too large a distance.
				camFrameInterpolator* frameInterpolator = m_ActiveCamera->GetFrameInterpolator();
				if(frameInterpolator)
				{
					frameInterpolator->SetMaxDistanceToTravel(m_Metadata.m_MaxDistanceToInterpolateOnVehicleEntryExit);
				}				

				camThirdPersonCamera* thirdPersonCamera = static_cast<camThirdPersonCamera*>(m_PreviousCamera.Get());
				camControlHelper* previousCameraControlHelper = thirdPersonCamera->GetControlHelper();
				if(previousCameraControlHelper)
				{
					//Don't allow the follow camera that we're interpolating from to flip in or out of look-behind anymore.
					previousCameraControlHelper->DeactivateLookBehindInput();

					camControlHelper* turretAimCameraControlHelper = turretAimCamera->GetControlHelper();
					if(previousCameraControlHelper->IsLookingBehind() && turretAimCameraControlHelper && !turretAimCameraControlHelper->IsUsingLookBehindInput())
					{
						//Flip the follow camera to take account of look-behind and then disable look-behind, so that the interpolation into
						//the aim camera is based upon coherent base front vectors.
						thirdPersonCamera->CloneBaseOrientation(*thirdPersonCamera, false);
						previousCameraControlHelper->SetLookBehindState(false);
					}

					//Allow the source follow camera to decelerate more aggressively in order to reduce overshoot during the interpolation.
					previousCameraControlHelper->SetLookAroundDecelerationScaling(m_Metadata.m_AimWeaponInterpolationSourceLookAroundDecelerationScaling);
				}
			}
			else if(m_PreviousCamera->GetIsClassId(camAimCamera::GetStaticClassId()))
			{
				camAimCamera* aimCamera = static_cast<camAimCamera*>(m_PreviousCamera.Get());
				camControlHelper* previousCameraControlHelper = aimCamera->GetControlHelper();
				if(previousCameraControlHelper)
				{
					//Don't allow the follow camera that we're interpolating from to flip in or out of look-behind anymore.
					previousCameraControlHelper->DeactivateLookBehindInput();

					camControlHelper* turretAimCameraControlHelper = turretAimCamera->GetControlHelper();					
					if(previousCameraControlHelper->IsLookingBehind() && turretAimCameraControlHelper && !turretAimCameraControlHelper->IsUsingLookBehindInput())
					{
						//Flip the follow camera to take account of look-behind and then disable look-behind, so that the interpolation into
						//the aim camera is based upon coherent base front vectors.
						aimCamera->CloneBaseOrientation(*aimCamera);
						previousCameraControlHelper->SetLookBehindState(false);
					}

					//Allow the source follow camera to decelerate more aggressively in order to reduce overshoot during the interpolation.
					previousCameraControlHelper->SetLookAroundDecelerationScaling(m_Metadata.m_AimWeaponInterpolationSourceLookAroundDecelerationScaling);
				}
			}
		}

		m_State = TURRET_AIMING;
	}
	else
	{
		m_State = DOING_NOTHING;
	}
}

void camGameplayDirector::UpdateStartTurretFiring(const CVehicle* followVehicle, const CPed& followPed)
{
	if(!followVehicle)
	{
		//Fall-back to a safe state.
		m_State = DOING_NOTHING;
		return;
	}

	tGameplayCameraSettings cameraSettings;
	if(!ComputeTurretFireCameraSettings(*followVehicle, followPed, cameraSettings))
	{
		m_State = FOLLOWING_VEHICLE;
		return;
	}

	if (m_ActiveCamera && m_ActiveCamera->GetIsClassId(camThirdPersonPedAimCamera::GetStaticClassId()) && static_cast<camThirdPersonPedAimCamera*>(m_ActiveCamera.Get())->ShouldPreventBlendToItself() && cameraSettings.m_CameraHash == m_ActiveCamera->GetNameHash())
	{
		m_State = TURRET_FIRING;
		return;
	}

	if(
		cameraVerifyf(CreateActiveCamera(cameraSettings.m_CameraHash), "Failed to create a turret aim camera (hash: %u)", cameraSettings.m_CameraHash) &&
		cameraVerifyf(m_ActiveCamera->GetIsClassId(camThirdPersonPedAimCamera::GetStaticClassId()), "Created a camera of an incorrect type (%s) for turret aiming", m_ActiveCamera->GetClassId().GetCStr())
		)
	{
		m_ActiveCamera->AttachTo(cameraSettings.m_AttachEntity);
		static_cast<camThirdPersonPedAimCamera*>(m_ActiveCamera.Get())->SetIsTurretCam(true);

		if(!cameraSettings.m_Flags.IsFlagSet(Flag_ShouldAllowNextCameraToCloneOrientation))
		{
			m_ActiveCamera->PreventNextCameraCloningOrientation();
		}

		if(!cameraSettings.m_Flags.IsFlagSet(Flag_ShouldAllowInterpolationToNextCamera))
		{
			m_ActiveCamera->PreventInterpolationToNextCamera();
		}

		if(m_PreviousCamera)
		{			
			camThirdPersonAimCamera* fireCamera = static_cast<camThirdPersonAimCamera*>(m_ActiveCamera.Get());

			if(cameraSettings.m_Flags.IsFlagSet(Flag_ShouldClonePreviousOrientation))
			{
				fireCamera->CloneBaseOrientation(*m_PreviousCamera);
			}

			if(cameraSettings.m_Flags.IsFlagSet(Flag_ShouldClonePreviousControlSpeeds))
			{
				fireCamera->CloneControlSpeeds(*m_PreviousCamera);
			}

			if(m_ActiveCamera->GetIsClassId(camFirstPersonPedAimCamera::GetStaticClassId()))
			{
				camFirstPersonPedAimCamera* fireCamera = static_cast<camFirstPersonPedAimCamera*>(m_ActiveCamera.Get());
				fireCamera->CloneBaseOrientation(*m_PreviousCamera);
				fireCamera->CloneControlSpeeds(*m_PreviousCamera);
			}
			else
			{
				m_ActiveCamera->InterpolateFromCamera(*m_PreviousCamera, m_Metadata.m_MinAimWeaponInterpolateInDuration, true);

				//Ensure that the interpolation is not performed over too large a distance.
				camFrameInterpolator* frameInterpolator = m_ActiveCamera->GetFrameInterpolator();
				if(frameInterpolator)
				{
					frameInterpolator->SetMaxDistanceToTravel(m_Metadata.m_MaxDistanceToInterpolateOnVehicleEntryExit);
				}			

				camThirdPersonPedAimCamera* previousCamera = const_cast<camThirdPersonPedAimCamera*>(static_cast<const camThirdPersonPedAimCamera*>(m_PreviousCamera.Get()));
				camControlHelper* previousCameraControlHelper = previousCamera->GetControlHelper();
				if(previousCameraControlHelper)
				{
					//Don't allow the follow camera that we're interpolating from to flip in or out of look-behind anymore.
					previousCameraControlHelper->DeactivateLookBehindInput();

					camControlHelper* fireCameraControlHelper = fireCamera->GetControlHelper();
					if(previousCameraControlHelper->IsLookingBehind() && fireCameraControlHelper && !fireCameraControlHelper->IsUsingLookBehindInput())
					{
						//Flip the follow camera to take account of look-behind and then disable look-behind, so that the interpolation into
						//the aim camera is based upon coherent base front vectors.
						previousCamera->CloneBaseOrientation(*previousCamera, false);
						previousCameraControlHelper->SetLookBehindState(false);
					}

					//Allow the source follow camera to decelerate more aggressively in order to reduce overshoot during the interpolation.
					previousCameraControlHelper->SetLookAroundDecelerationScaling(m_Metadata.m_AimWeaponInterpolationSourceLookAroundDecelerationScaling);
				}
			}
		}

		m_State = TURRET_FIRING;
	}
	else
	{
		m_State = DOING_NOTHING;
	}
}

bool camGameplayDirector::UpdateTurretAiming(const CVehicle* followVehicle, const CPed& followPed)
{
	const CPedIntelligence* followPedIntelligence = followPed.GetPedIntelligence();

	CTaskVehicleMountedWeapon* taskVehicleMountedWeapon = NULL;
	CTaskInVehicleSeatShuffle* taskInVehicleSeatShuffle = NULL;

	if(followPedIntelligence)
	{	
		taskVehicleMountedWeapon = static_cast<CTaskVehicleMountedWeapon*>(followPedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_VEHICLE_MOUNTED_WEAPON));
		taskInVehicleSeatShuffle = static_cast<CTaskInVehicleSeatShuffle*>(followPedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE));
	}

	if((m_VehicleEntryExitState == INSIDE_VEHICLE && !IsPedInOrEnteringTurretSeat(followVehicle, followPed)) || taskInVehicleSeatShuffle)
	{
		m_State = START_FOLLOWING_VEHICLE;
	}
	else if(taskVehicleMountedWeapon && !taskVehicleMountedWeapon->WantsToAim(const_cast<CPed*>(&followPed)))
	{
		m_State = START_MOUNTING_TURRET;
	}
	else if(m_VehicleEntryExitState == EXITING_VEHICLE || m_VehicleEntryExitState == OUTSIDE_VEHICLE)
	{
		m_State = START_FOLLOWING_PED;
	}
	else if(!m_ActiveCamera) //Default to a safe state if we don't have an active camera.
	{
		m_State = DOING_NOTHING;
	}

	return (m_State != TURRET_AIMING);
}

bool camGameplayDirector::UpdateTurretFiring(const CVehicle* followVehicle, const CPed& followPed)
{
	const CPedIntelligence* followPedIntelligence = followPed.GetPedIntelligence();

	CTaskVehicleMountedWeapon* taskVehicleMountedWeapon = NULL;
	CTaskInVehicleSeatShuffle* taskInVehicleSeatShuffle = NULL;

	if(followPedIntelligence)
	{	
		taskVehicleMountedWeapon = static_cast<CTaskVehicleMountedWeapon*>(followPedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_VEHICLE_MOUNTED_WEAPON));
		taskInVehicleSeatShuffle = static_cast<CTaskInVehicleSeatShuffle*>(followPedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE));
	}

	if((m_VehicleEntryExitState == INSIDE_VEHICLE && !IsPedInOrEnteringTurretSeat(followVehicle, followPed)) || taskInVehicleSeatShuffle)
	{
		m_State = START_FOLLOWING_VEHICLE;
	}
	else if(taskVehicleMountedWeapon && taskVehicleMountedWeapon->WantsToAim(const_cast<CPed*>(&followPed)))
	{
		m_State = START_TURRET_AIMING;
	}
	else if(taskVehicleMountedWeapon && !taskVehicleMountedWeapon->WantsToFire(const_cast<CPed*>(&followPed)))
	{
		m_State = START_MOUNTING_TURRET;
	}
	else if(m_VehicleEntryExitState == EXITING_VEHICLE || m_VehicleEntryExitState == OUTSIDE_VEHICLE)
	{
		m_State = START_FOLLOWING_PED;
	}
	else if(!m_ActiveCamera) //Default to a safe state if we don't have an active camera.
	{
		m_State = DOING_NOTHING;
	}

	return (m_State != TURRET_FIRING);
}

bool camGameplayDirector::ComputeShouldUseVehicleStuntSettings()
{
	if(!m_FollowVehicle)
	{
		return false;
	}

	const Vector3 followVehicleUpVector = VEC3V_TO_VECTOR3(m_FollowVehicle->GetTransform().GetMatrix().c());
	const float dotVehicleUpWorldDown = followVehicleUpVector.Dot(Vector3(0.0f, 0.0f, -1.0f));

	TUNE_GROUP_FLOAT(CAM_STUNT_SETTINGS, fDotVehicleUpLimit, -0.60f, -1.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(CAM_STUNT_SETTINGS, fDotVehicleUpLimitForLookBehind, -0.90f, -1.0f, 1.0f, 0.01f);
	const bool isVehicleUpsideDown = ((IsLookingBehind() ? fDotVehicleUpLimitForLookBehind : fDotVehicleUpLimit) < dotVehicleUpWorldDown) || m_ScriptForcedVehicleCamStuntSettingsThisUpdate;

	return !m_FollowVehicle->GetIsInWater() && isVehicleUpsideDown && (m_FollowVehicle->HasContactWheels() || ComputeVehicleProximityToGround());
}

bool camGameplayDirector::ComputeVehicleProximityToGround()
{
	if(!m_FollowVehicle)
	{
		return false;
	}

	const Vector3 followVehicleUpVector = VEC3V_TO_VECTOR3(m_FollowVehicle->GetTransform().GetMatrix().c());
	const Vector3 followVehiclePosition = VEC3V_TO_VECTOR3(m_FollowVehicle->GetTransform().GetMatrix().d());

	Vector3 averageWheelPosition(0.0f, 0.0f, 0.0f);

	const int numWheels = m_FollowVehicle->GetNumWheels();
	int validWheels = 0;
	for(int i = 0; i < numWheels; i++)
	{
		const CWheel* pWheel = m_FollowVehicle->GetWheel(i);
		if(pWheel)
		{
			Vector3 wheelPos;
			const float wheelRadius = pWheel->GetWheelPosAndRadius(wheelPos);
			averageWheelPosition += (wheelPos - (followVehicleUpVector*wheelRadius));
			validWheels++;
		}
	}

	Vector3 probeStartPosition(followVehiclePosition);

	if(validWheels > 0)
	{
		averageWheelPosition /= (float)validWheels;
		probeStartPosition = averageWheelPosition;
	}

	TUNE_GROUP_FLOAT(CAM_STUNT_SETTINGS, fVehicleProximityToGroundTestLength, 1.2f, 0.001f, 10.0f, 0.01f);

	WorldProbe::CShapeTestProbeDesc probeTest;
	probeTest.SetStartAndEnd(probeStartPosition, probeStartPosition-(followVehicleUpVector*fVehicleProximityToGroundTestLength));
	probeTest.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_VEHICLE);
	probeTest.SetContext(WorldProbe::LOS_Camera);

	return WorldProbe::GetShapeTestManager()->SubmitTest(probeTest);
}

void camGameplayDirector::ComputeFollowPedCameraSettings(const CPed& followPed, tGameplayCameraSettings& settings) const
{
	settings.m_AttachEntity = &followPed;

	//Fall-back to a default follow-ped camera.
	settings.m_CameraHash = m_Metadata.m_FollowPedCameraRef;

	//Query the follow ped's active tasks to see if one needs to override the camera.
	bool isFollowPedCamera					= true;
	CPedIntelligence* followPedIntelligence	= followPed.GetPedIntelligence();
	if(followPedIntelligence)
	{
		tGameplayCameraSettings tempSettings(settings);
		tempSettings.m_CameraHash = 0; //Invalidate the camera hash.

		followPedIntelligence->GetOverriddenGameplayCameraSettings(tempSettings);

		//Verify that we have a valid attach entity and that this camera exists before attempting to use it.
		if(tempSettings.m_CameraHash && tempSettings.m_AttachEntity)
		{
			const camBaseCameraMetadata* cameraMetadata = camFactory::FindObjectMetadata<camBaseCameraMetadata>(tempSettings.m_CameraHash);
			if(cameraMetadata)
			{
				settings						= tempSettings;
				isFollowPedCamera				= camFactory::IsObjectMetadataOfClass<camFollowPedCameraMetadata>(*cameraMetadata);
			}
		}

#if FPS_MODE_SUPPORTED
		// Override follow ped camera with first person camera, if required and permitted.
		TUNE_GROUP_BOOL(CAM_FPS, bOverrideSniperCamera, false);
		const bool bSniperCamera = (settings.m_CameraHash == ATSTRINGHASH("SNIPER_AIM_CAMERA", 0xec22d348)) ||
								   (settings.m_CameraHash == ATSTRINGHASH("SNIPER_LOW_ZOOM_AIM_CAMERA", 0x8c40fb35)) ||
								   (settings.m_CameraHash == ATSTRINGHASH("SNIPER_FIXED_ZOOM_AIM_CAMERA", 0xD5F08A9E));

		if( IsFirstPersonModeEnabled() && IsFirstPersonModeAllowed() )
		{
			// Some existing gameplay cameras should NOT be overridden, in first person mode.
			if( (!bSniperCamera || bOverrideSniperCamera) &&
				settings.m_CameraHash != ATSTRINGHASH("CELL_PHONE_AIM_CAMERA", 0x64e68b46) &&
				settings.m_CameraHash != ATSTRINGHASH("CELL_PHONE_SELFIE_AIM_CAMERA", 0x1ab41687) )
 			{
				settings.m_CameraHash	= ComputeFirstPersonShooterCameraRef();
				settings.m_AttachEntity	= &followPed;

				return;
			}
		}
#endif // FPS_MODE_SUPPORTED
	}

	if(isFollowPedCamera)
	{
		//Now check if the script has requested a custom follow-ped camera.
		if(m_ScriptOverriddenFollowPedCameraSettingsThisUpdate.m_CameraHash)
		{
			//NOTE: This camera should already have been validated, so we don't need to check that it exists.
			settings = m_ScriptOverriddenFollowPedCameraSettingsThisUpdate;
			//NOTE: We don't allow the script to override the attach entity.
			settings.m_AttachEntity = &followPed;
		}
		else if(m_ScriptOverriddenFollowPedCameraSettingsOnPreviousUpdate.m_CameraHash &&
			m_ScriptOverriddenFollowPedCameraSettingsOnPreviousUpdate.m_InterpolationDuration >= 0)
		{
			//Allow the script to control the duration of the interpolation out of a custom follow-ped camera.
			settings.m_InterpolationDuration = m_ScriptOverriddenFollowPedCameraSettingsOnPreviousUpdate.m_InterpolationDuration;
		}

        //Now check if the script has requested a custom table games camera.
        if(m_ScriptOverriddenTableGamesCameraSettingsThisUpdate.m_CameraHash)
        {
            //NOTE: This camera should already have been validated, so we don't need to check that it exists.
            settings = m_ScriptOverriddenTableGamesCameraSettingsThisUpdate;
            //NOTE: We don't allow the script to override the attach entity.
            settings.m_AttachEntity = &followPed;
        }
        else if(m_ScriptOverriddenTableGamesCameraSettingsOnPreviousUpdate.m_CameraHash &&
            m_ScriptOverriddenTableGamesCameraSettingsOnPreviousUpdate.m_InterpolationDuration >= 0)
        {
            //Allow the script to control the duration of the interpolation out of a custom table games camera.
            settings.m_InterpolationDuration = m_ScriptOverriddenTableGamesCameraSettingsOnPreviousUpdate.m_InterpolationDuration;
        }

		if (NetworkInterface::IsGameInProgress() && m_ShouldPerformCustomTransitionAfterWarpFromVehicle)
		{
			settings.m_CameraHash = ATSTRINGHASH("CUSTOM_TRANSITION_AFTER_WARP_SKY_DIVE_CAMERA", 0x73F590F4);
		}
	}
}

bool camGameplayDirector::ComputeCoverEntryAlignmentHeading(const camBaseCamera& previousCamera, float& alignmentHeading) const
{
	const Vector3& previousFront	= previousCamera.GetBaseFront();
	const float previousHeading		= camFrame::ComputeHeadingFromFront(previousFront);
	const float coverHeading		= camFrame::ComputeHeadingFromMatrix(m_FollowPedCoverSettings.m_CoverMatrix);

	float headingDelta	= previousHeading - coverHeading;
	headingDelta		= fwAngle::LimitRadianAngle(headingDelta);

	const float maxHeadingDeltaForCoverEntryAlignment = m_Metadata.m_MaxHeadingDeltaForCoverEntryAlignment * DtoR;

	const bool shouldAlign = (Abs(headingDelta) >= (maxHeadingDeltaForCoverEntryAlignment + SMALL_FLOAT));
	if(shouldAlign)
	{
		headingDelta		= Clamp(headingDelta, -maxHeadingDeltaForCoverEntryAlignment, maxHeadingDeltaForCoverEntryAlignment);
		alignmentHeading	= coverHeading + headingDelta;
		alignmentHeading	= fwAngle::LimitRadianAngle(alignmentHeading);
	}

	return shouldAlign;
}

void camGameplayDirector::ComputeFollowVehicleCameraSettings(const CVehicle& followVehicle, tGameplayCameraSettings& settings) const
{
	settings.m_AttachEntity	= &followVehicle;

	//Fall-back to a default follow-vehicle camera.
	settings.m_CameraHash	= m_Metadata.m_FollowVehicleCameraRef;

	//First check if the script has requested a custom follow-vehicle camera.
	if(m_ScriptOverriddenFollowVehicleCameraSettingsThisUpdate.m_CameraHash)
	{
		//NOTE: This camera should already have been validated, so we don't need to check that it exists.
		settings = m_ScriptOverriddenFollowVehicleCameraSettingsThisUpdate;
		//NOTE: We don't allow the script to override the attach entity.
		settings.m_AttachEntity = &followVehicle;

		return;
	}
	else if(m_ScriptOverriddenFollowVehicleCameraSettingsOnPreviousUpdate.m_CameraHash &&
		m_ScriptOverriddenFollowVehicleCameraSettingsOnPreviousUpdate.m_InterpolationDuration >= 0)
	{
		//Allow the script to control the duration of the interpolation out of a custom follow-vehicle camera.
		settings.m_InterpolationDuration = m_ScriptOverriddenFollowVehicleCameraSettingsOnPreviousUpdate.m_InterpolationDuration;
	}

	//Query the follow vehicle's active tasks to see if one needs to override the camera.
	bool bFoundCameraUsingVehicleIntel = false;
	CVehicleIntelligence* followVehicleIntelligence = followVehicle.GetIntelligence();
	if(followVehicleIntelligence)
	{
		tGameplayCameraSettings tempSettings(settings);
		tempSettings.m_CameraHash = 0; //Invalidate the camera hash.

		followVehicleIntelligence->GetOverriddenGameplayCameraSettings(tempSettings);

		//Verify that we have a valid attach entity and that this camera exists before attempting to use it.
		if(tempSettings.m_CameraHash && tempSettings.m_AttachEntity && camFactory::FindObjectMetadata<camBaseCameraMetadata>(tempSettings.m_CameraHash))
		{
			settings = tempSettings;
			bFoundCameraUsingVehicleIntel = true;
		}
	}

	//Try to extract the hash of a follow-vehicle camera from the vehicle model info.
	CVehicleModelInfo* vehicleModelInfo = followVehicle.GetVehicleModelInfo();
	if(!bFoundCameraUsingVehicleIntel && vehicleModelInfo)
	{
		u32 vehicleCameraHash = vehicleModelInfo->GetCameraNameHash();

		if(followVehicle.InheritsFromAutomobile())
		{
			const CAutomobile& automobile = static_cast<const CAutomobile&>(followVehicle);
			if(automobile.IsParachuting())
			{
				vehicleCameraHash = m_Metadata.m_FollowVehicleParachuteCameraRef;
				settings.m_InterpolationDuration = m_Metadata.m_FollowVehicleParachuteCameraInterpolation;
			}
		}

		if(followVehicle.InheritsFromAmphibiousQuadBike())
		{
			const CAmphibiousQuadBike& amphibiousQuadBike = static_cast<const CAmphibiousQuadBike&>(followVehicle);
			if(amphibiousQuadBike.IsWheelsFullyIn())
			{
				//force the jetski camera
				vehicleCameraHash = ATSTRINGHASH("FOLLOW_AMPHIBIOUS_JETSKI_CAMERA", 0xAEF08CBC);
				settings.m_InterpolationDuration = m_Metadata.m_FollowVehicleInterpolateDuration;
			}
			else
			{
				settings.m_InterpolationDuration = m_Metadata.m_FollowVehicleInterpolateDuration;
			}
		}

		//See if this camera exists, as we may not have added a custom camera for this vehicle.
		if(camFactory::FindObjectMetadata<camFollowVehicleCameraMetadata>(vehicleCameraHash))
		{
			//We have a custom camera for this vehicle, so use it.
			settings.m_CameraHash = vehicleCameraHash;
		}
	}

	if(m_UseVehicleCamStuntSettings && m_ScriptRequestDedicatedStuntCameraThisUpdate)
	{
		if(m_DedicatedStuntCameraName.IsNotNull())
		{
			settings.m_CameraHash = m_DedicatedStuntCameraName.GetHash();
		}
		else
		{
			settings.m_CameraHash = ATSTRINGHASH("FOLLOW_VEHICLE_STUNT_CAMERA", 0x16685D28);
		}
	}

#if FPS_MODE_SUPPORTED
	// Override with a first-person ped camera, if required and permitted.
	const bool bIsRenderingFirstPersonCinematicCamera = camInterface::GetCinematicDirector().IsRenderingAnyInVehicleFirstPersonCinematicCamera();
	if( m_FollowPed && IsFirstPersonModeEnabled() && (IsFirstPersonModeAllowed() || bIsRenderingFirstPersonCinematicCamera))
	{
		//we don't allow the FPS camera to run if we're rendering an in-vehicle cinematic camera, or the death fail effect is active
		bool isRenderingCinematicCamera = camInterface::GetCinematicDirector().IsRenderingAnyInVehicleCinematicCamera() ||
										  bIsRenderingFirstPersonCinematicCamera;
		const bool isDeathFailEffectActive = camInterface::IsDeathFailEffectActive();

		// If player is playing an anim with first person animated data, while the in car cinematic camera is active,
		// force the first person camera to update underneath so we can blend to/from the animated data.
		if(m_bFirstPersonAnimatedDataInVehicle || m_bPovCameraAnimatedDataBlendoutPending)
			isRenderingCinematicCamera = false;

		if( !isRenderingCinematicCamera && !isDeathFailEffectActive )
		{
			//Use the FPS camera for vehicle entry/exit. The cinematic POV camera will take over when when ready.
			const s32 viewModeContextForVehicle	= camControlHelper::ComputeViewModeContextForVehicle(followVehicle);
			const s32 viewModeForVehicle		= camControlHelper::GetViewModeForContext(viewModeContextForVehicle);
			const bool bJustEnteredVehicle		= m_VehicleEntryExitState == INSIDE_VEHICLE && m_VehicleEntryExitStateOnPreviousUpdate != INSIDE_VEHICLE &&
													(viewModeForVehicle == camControlHelperMetadataViewMode::FIRST_PERSON);
		
			// If vehicle POV camera is delayed, then _stay_ in first person shooter camera.
			bool bDelayForVehiclePovCamera = (m_ActiveCamera.Get() && m_ActiveCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId())) &&
											 (m_ShouldDelayVehiclePovCameraOnPreviousUpdate || m_ShouldDelayVehiclePovCamera);

			//Is jacking covered by EXITING_VEHICLE?
			const bool bEnterExitVehicle	= (m_VehicleEntryExitState == ENTERING_VEHICLE) || (m_VehicleEntryExitState == EXITING_VEHICLE) ||
												m_FollowPed->IsBeingJackedFromVehicle();

			// Stay in first person camera when putting on a helmet to avoid clipping issues
			const bool bIsPuttingOnHelmet = m_FollowPed->GetPedResetFlag(CPED_RESET_FLAG_PuttingOnHelmet);

			if( m_bFirstPersonAnimatedDataInVehicle || m_bPovCameraAnimatedDataBlendoutPending ||
				bEnterExitVehicle || bJustEnteredVehicle || bDelayForVehiclePovCamera || bIsPuttingOnHelmet)
			{
				settings.m_CameraHash	= m_Metadata.m_FirstPersonShooterCameraRef;
				settings.m_AttachEntity	= m_FollowPed;
			}
		}
	}
#endif // FPS_MODE_SUPPORTED
}

void camGameplayDirector::ComputeVehicleAimCameraSettings(const CVehicle& followVehicle, const CPed& followPed, tGameplayCameraSettings& settings) const
{
	//Fall-back to a default vehicle-aim camera.
	settings.m_CameraHash	= m_Metadata.m_ThirdPersonVehicleAimCameraRef;
	settings.m_AttachEntity	= &followVehicle;	

	const CVehicleModelInfo* followVehicleModelInfo = followVehicle.GetVehicleModelInfo();
	const u32 vehicleModelHash	= followVehicleModelInfo ? followVehicleModelInfo->GetModelNameHash() : 0;
	const camVehicleCustomSettingsMetadata* vehicleCustomSettings = vehicleModelHash > 0 ? FindVehicleCustomSettings(vehicleModelHash) : NULL;

	if(m_Metadata.m_ShouldAlwaysAllowCustomVehicleDriveByCameras || m_ShouldAllowCustomVehicleDriveByCamerasThisUpdate || (vehicleCustomSettings && vehicleCustomSettings->m_ShouldAllowCustomVehicleDriveByCameras))
	{
		//Check if a custom drive-by camera has been associated with the follow ped's current seat.
		const CVehicleSeatAnimInfo* seatAnimInfo = followVehicle.GetSeatAnimationInfo(&followPed);
		if(seatAnimInfo)
		{
			const CVehicleDriveByInfo* driveByInfo = seatAnimInfo->GetDriveByInfo();
			if(driveByInfo)
			{
				const u32 driveByAimCameraHash = driveByInfo->GetDriveByCamera().GetHash();
				if(driveByAimCameraHash)
				{
					//Verify that this camera exists before attempting to use it.
					const camBaseCameraMetadata* cameraMetadata	= camFactory::FindObjectMetadata<camBaseCameraMetadata>(driveByAimCameraHash);
					if(cameraMetadata && (camFactory::IsObjectMetadataOfClass<camThirdPersonAimCameraMetadata>(*cameraMetadata) ||
						camFactory::IsObjectMetadataOfClass<camFirstPersonAimCameraMetadata>(*cameraMetadata)))
					{
						settings.m_CameraHash = driveByAimCameraHash;

						//Attach to the follow ped if this is a ped-aim camera.
						const bool isPedAimCamera	= (camFactory::IsObjectMetadataOfClass<camThirdPersonPedAimCameraMetadata>(*cameraMetadata) ||
							camFactory::IsObjectMetadataOfClass<camFirstPersonPedAimCameraMetadata>(*cameraMetadata));
						if(isPedAimCamera)
						{
							settings.m_AttachEntity = &followPed;
						}

						return;
					}
				}
			}
		}
	}	

	// In multiplayer we want to switch to sniper scope (ped aim instead of vehicle aim) when in the side seats	(which are not turret seats)
    const int seatIndex = ComputeSeatIndexPedIsIn(followVehicle, followPed);
    const bool isTurretSeat = IsTurretSeat(&followVehicle, seatIndex);
	if(NetworkInterface::IsGameInProgress() && followVehicle.InheritsFromHeli() && !followPed.GetIsDrivingVehicle() && followVehicle.GetSeatManager() && !isTurretSeat)
	{
		const CWeapon* pWeapon = followPed.GetWeaponManager() ? followPed.GetWeaponManager()->GetEquippedWeapon() : NULL;
		if (pWeapon && pWeapon->GetHasFirstPersonScope())
		{
			const u32 cameraHash = pWeapon->GetDefaultCameraHash();
			if(cameraHash)
			{
				settings.m_CameraHash	= cameraHash;
				settings.m_AttachEntity	= &followPed;
				return;
			}
		}
	}

	//Attempt to extract the aim camera hash from the vehicle model info.
	if(followVehicleModelInfo)
	{
		const u32 vehicleAimCameraHash = followVehicleModelInfo->GetAimCameraNameHash();
		if(vehicleAimCameraHash)
		{
			//Verify that this camera exists before attempting to use it.
			const camThirdPersonAimCameraMetadata* cameraMetadata	= camFactory::FindObjectMetadata<camThirdPersonAimCameraMetadata>(
																		vehicleAimCameraHash);
			if(cameraMetadata)
			{
				settings.m_CameraHash = vehicleAimCameraHash;

				//Attach to the follow ped if this is a ped-aim camera.
				const bool isPedAimCamera = camFactory::IsObjectMetadataOfClass<camThirdPersonPedAimCameraMetadata>(*cameraMetadata);
				if(isPedAimCamera)
				{
					settings.m_AttachEntity = &followPed;
				}
			}
		}
	}
}

const CWeaponInfo* camGameplayDirector::GetTurretWeaponInfoForSeat(const CVehicle& followVehicle, s32 seatIndex)
{
    if(seatIndex < 0)
    {
        return nullptr;
    }

    const CVehicleWeaponMgr* weaponManager = followVehicle.GetVehicleWeaponMgr();
    if(weaponManager)
    {				
        atArray<const CVehicleWeapon*> weaponArray;
        weaponManager->GetWeaponsForSeat(seatIndex, weaponArray);

        for(int i = 0; i < weaponArray.GetCount(); i++)
        {					
            const CWeaponInfo* weaponInfo = weaponArray[i]->GetWeaponInfo();
            if(weaponInfo && weaponInfo->GetIsTurret())
            {
                return weaponInfo;
            }
        }
    }

	return nullptr;
}

s32 camGameplayDirector::ComputeSeatIndexPedIsIn(const CVehicle& followVehicle, const CPed& followPed, bool bSkipEnterVehicleTestForFirstPerson /*= false*/, bool bOnlyUseEnterSeatIfIsDirectEntry /*=false*/)
{
    s32 seatIndex = -1;

    const CPedIntelligence* pedIntelligence	= followPed.GetPedIntelligence();
    if(pedIntelligence)
    {
        // We may either be entering the seat or already in the seat (e.g. holding aim prior to and through mounting the turret) TODO: Check there isn't a helper which already implements this.
        CTaskEnterVehicle* enterVehicleTask	= static_cast<CTaskEnterVehicle*>(pedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));	
        if(enterVehicleTask)
        {
            FPS_MODE_SUPPORTED_ONLY(if(!bSkipEnterVehicleTestForFirstPerson || !camInterface::GetGameplayDirector().IsFirstPersonModeAllowed()))
            {
                if(bOnlyUseEnterSeatIfIsDirectEntry)
                {
                    // Only accept this if the target seat is a direct entry from the target entry point
                    const s32 targetSeat = enterVehicleTask->GetTargetSeat();
                    const s32 targetEntryPoint = enterVehicleTask->GetTargetEntryPoint();

                    if (followVehicle.IsSeatIndexValid(targetSeat) && followVehicle.IsEntryIndexValid(targetEntryPoint))
                    {
                        const s32 directSeat = followVehicle.GetEntryExitPoint(targetEntryPoint)->GetSeat(SA_directAccessSeat);
                        if (targetSeat == directSeat)
                        {
                            seatIndex = targetSeat;

                        }
                    }
                }
                else
                {
                    seatIndex = enterVehicleTask->GetTargetSeat();
                }
            }		
        }
        else
        {
            const CSeatManager* seatManager = followVehicle.GetSeatManager();
            if(seatManager) 
            {				
                seatIndex = seatManager->GetPedsSeatIndex(&followPed);			
            }
        }

        seatIndex = camInterface::GetGameplayDirector().GetScriptOverriddenFollowVehicleCameraSeat(seatIndex);
    }

    return seatIndex;
}

bool camGameplayDirector::ComputeTurretCameraSettings(const CVehicle& followVehicle, const CPed& followPed, tGameplayCameraSettings& settings) const
{	
    const s32 seatIndex = ComputeSeatIndexPedIsIn(followVehicle, followPed);
	const CWeaponInfo* weaponInfo = GetTurretWeaponInfoForSeat(followVehicle, seatIndex);
	if(weaponInfo)
	{	
		u32 cameraHash = weaponInfo->GetDefaultCameraHash();				

		if(cameraVerifyf(cameraHash != 0, "Could not find valid default camera (DefaultCameraHash) for turret"))
		{	
			settings.m_CameraHash = cameraHash;
			settings.m_AttachEntity = &followPed;

			return true;
		}
	}

	return false;
}

bool camGameplayDirector::ComputeTurretAimCameraSettings(const CVehicle& followVehicle, const CPed& followPed, tGameplayCameraSettings& settings) const
{		
    const s32 seatIndex = ComputeSeatIndexPedIsIn(followVehicle, followPed);
	const CWeaponInfo* weaponInfo = GetTurretWeaponInfoForSeat(followVehicle, seatIndex);
	if(cameraVerifyf(weaponInfo, "Could not get weapon info for turret"))
	{	
		u32 cameraHash = weaponInfo->GetAimCameraHash();

		if(!cameraHash) // Try to fall back to the default camera hash
		{			
			cameraHash = weaponInfo->GetDefaultCameraHash();				
		}

		if(cameraVerifyf(cameraHash != 0, "Could not find valid aim camera (AimCameraHash) for turret"))
		{	
			settings.m_CameraHash = cameraHash;
			settings.m_AttachEntity = &followPed;
		
			return true;
		}
	}

	return false;
}

bool camGameplayDirector::ComputeTurretFireCameraSettings(const CVehicle& followVehicle, const CPed& followPed, tGameplayCameraSettings& settings) const
{
    const s32 seatIndex = ComputeSeatIndexPedIsIn(followVehicle, followPed);
	const CWeaponInfo* weaponInfo = GetTurretWeaponInfoForSeat(followVehicle, seatIndex);
	if(cameraVerifyf(weaponInfo, "Could not get weapon info for turret"))
	{	
		u32 cameraHash = weaponInfo->GetFireCameraHash();

		if(!cameraHash) // Try to fall back to the default camera hash
		{			
			cameraHash = weaponInfo->GetDefaultCameraHash();				
		}

		if(cameraVerifyf(cameraHash != 0, "Could not find valid fire camera (FireCameraHash) for turret"))
		{	
			settings.m_CameraHash = cameraHash;
			settings.m_AttachEntity = &followPed;

			return true;
		}
	}

	return false;
}


bool camGameplayDirector::ComputeFrontForVehicleEntryExit(const CPed& followPed, const CVehicle* followVehicle,
	const camFollowVehicleCamera& followVehicleCamera, const camFollowCamera& previousCamera, Vector3& front) const
{
	bool shouldCutToNewOrientation = false;

	const Vector3& previousFront = previousCamera.GetBaseFront();
	front = previousFront; //Default to cloning the orientation of the previous camera.

	if (NetworkInterface::IsGameInProgress() && m_ShouldPerformCustomTransitionAfterWarpFromVehicle)
	{
		const Matrix34 followPedMatrix = MAT34V_TO_MATRIX34(followPed.GetTransform().GetMatrix());
		front = -followPedMatrix.c; //Default to pointing along the follow ped down vector - this custom transition is skydiving
	}

	if(!followVehicle)
	{
		return false;
	}

	const camVehicleCustomSettingsMetadataDoorAlignmentSettings& doorAlignmentSettings = followVehicleCamera.GetVehicleEntryExitSettings();
	if(doorAlignmentSettings.m_ShouldConsiderData && doorAlignmentSettings.m_ShouldAlignOnVehicleExit)
	{
		Vector3 entryExitPointPosition;
		Quaternion entryExitOrientation;
		bool hasClimbDown;
		const bool hasValidEntryExitPoint	= camInterface::ComputeVehicleEntryExitPointPositionAndOrientation(followPed, *followVehicle, entryExitPointPosition,
												entryExitOrientation, hasClimbDown);
		if(hasValidEntryExitPoint)
		{
			const Matrix34 followVehicleMatrix = MAT34V_TO_MATRIX34(followVehicle->GetMatrix());

			Vector3 entryExitPointLocalOffset;
			followVehicleMatrix.UnTransform(entryExitPointPosition, entryExitPointLocalOffset);

			const float entryExitSideSign = (entryExitPointLocalOffset.x > 0.0f) ? 1.0f : -1.0f;

			if(hasClimbDown && doorAlignmentSettings.m_ShouldConsiderClimbDown)
			{
				//Pull the camera around to the behind the follow ped when climbing down from a vehicle,
				//but bias a little away from the vehicle to improve the framing.

				Matrix34 entryExitOrientationMatrix;
				entryExitOrientationMatrix.FromQuaternion(entryExitOrientation);
				entryExitOrientationMatrix.RotateLocalZ(PI);

				const float frontDotFollowVehicleFront	= entryExitOrientationMatrix.b.Dot(followVehicleMatrix.b);
				const float biasRotationToApply			= m_Metadata.m_AngleToRotateAwayFromVehicleForClimbDownExit * DtoR *
															frontDotFollowVehicleFront * entryExitSideSign;

				entryExitOrientationMatrix.RotateLocalZ(biasRotationToApply);

				front = entryExitOrientationMatrix.b;

				const float orientationDeltaToApply = previousFront.FastAngle(front);
				shouldCutToNewOrientation			= (orientationDeltaToApply >= (doorAlignmentSettings.m_MinOrientationDeltaToCut * DtoR));
			}
			else
			{
				//Pull the camera around to the side of vehicle that the follow ped is entering/exiting from/to.

				const float previousFrontDotFollowVehicleFront	= previousFront.Dot(followVehicleMatrix.b);
				const bool wasPreviousCameraBehindVehicle		= (previousFrontDotFollowVehicleFront >= 0.0f);

				//NOTE: We apply a custom settings when we are tracking an attached (or recently detached) trailer (or towed vehicle), but only
				//if the previous camera was behind the vehicle.
				const bool shouldApplyLimitsForTrailers = (wasPreviousCameraBehindVehicle && ((m_AttachedTrailer.Get() != NULL) || (m_TowedVehicle.Get() != NULL)));

				Vector3 entryExitFront = -followVehicleMatrix.a * entryExitSideSign;

				const float alignmentConeOffsetTowardsVehicleFrontAngle = doorAlignmentSettings.m_AlignmentConeOffsetTowardsVehicleFrontAngle * DtoR;
				if(!IsNearZero(alignmentConeOffsetTowardsVehicleFrontAngle))
				{
					const float biasRotationToApply = alignmentConeOffsetTowardsVehicleFrontAngle * entryExitSideSign;

					Matrix34 rotationMatrix;
					rotationMatrix.MakeRotateUnitAxis(followVehicleMatrix.c, biasRotationToApply);

					rotationMatrix.Transform3x3(entryExitFront);
				}

				Matrix34 previousWorldMatrix;
				camFrame::ComputeWorldMatrixFromFront(previousFront, previousWorldMatrix);
				Quaternion previousOrientation;
				previousOrientation.FromMatrix34(previousWorldMatrix);

				Matrix34 entryExitWorldMatrix;
				camFrame::ComputeWorldMatrixFromFront(entryExitFront, entryExitWorldMatrix);
				Quaternion entryExitOrientation;
				entryExitOrientation.FromMatrix34(entryExitWorldMatrix);

				float orientationDelta			= previousOrientation.RelAngle(entryExitOrientation);
				const float alignmentConeAngle	= ((shouldApplyLimitsForTrailers ? doorAlignmentSettings.m_AlignmentConeAngleWithTrailer :
													doorAlignmentSettings.m_AlignmentConeAngle) * DtoR);
				if(orientationDelta > alignmentConeAngle)
				{
					//Rotate to the edge of the cone that's aligned with the entry exit door.

					Quaternion orientationToApply;
					float interpolationFactor = alignmentConeAngle / orientationDelta;
					orientationToApply.SlerpNear(interpolationFactor, entryExitOrientation, previousOrientation);

					Matrix34 worldMatrixToApply;
					worldMatrixToApply.FromQuaternion(orientationToApply);

					front = worldMatrixToApply.b;

					//Only cut to the new orientation if the delta is beyond a limit.
					float orientationDeltaToApply			= previousOrientation.RelAngle(orientationToApply);
					const float minOrientationDeltaToCut	= ((shouldApplyLimitsForTrailers ? doorAlignmentSettings.m_MinOrientationDeltaToCutWithTrailer :
																(wasPreviousCameraBehindVehicle ? doorAlignmentSettings.m_MinOrientationDeltaToCut :
																doorAlignmentSettings.m_MinOrientationDeltaToCutForReverseAngle)) * DtoR);
					shouldCutToNewOrientation				= (orientationDeltaToApply >= minOrientationDeltaToCut);
				}
			}
		}
	}

	return shouldCutToNewOrientation;
}

bool camGameplayDirector::ComputeShouldInterpolateIntoAiming(const CPed& followPed, const camFollowCamera& followCamera, const camThirdPersonAimCamera& aimCamera,
	s32& interpolationDuration, bool isAimingDuringVehicleExit) const
{
	//Compute the heading delta, handling any special-cases.

	float headingDelta = 0.0f;

	if(followPed.IsPlayer())
	{
		const CPlayerInfo* playerInfo = followPed.GetPlayerInfo();
		if(playerInfo)
		{
			const CEntity* aimTarget = playerInfo->GetTargeting().GetLockOnTarget();
			if(aimTarget)
			{
				const Vector3& followCameraFront = followCamera.GetFrame().GetFront();

				Vector3 targetFront = VEC3V_TO_VECTOR3(aimTarget->GetTransform().GetPosition() - followPed.GetTransform().GetPosition());
				targetFront.NormalizeSafe();

				headingDelta = Abs(camFrame::ComputeHeadingDeltaBetweenFronts(followCameraFront, targetFront));
			}
		}
	}

	//NOTE: We tolerate any heading delta when activating a melee aim camera.
	const bool shouldInterpolate	= (aimCamera.GetIsClassId(camThirdPersonPedMeleeAimCamera::GetStaticClassId()) ||
										(headingDelta <= (m_Metadata.m_MaxHeadingDeltaToInterpolateIntoAim * DtoR)));
	if(shouldInterpolate && (interpolationDuration < 0))
	{
		//A specific interpolation duration was not specified, so compute one.
		float t	= (headingDelta - (m_Metadata.m_MinHeadingDeltaForAimWeaponInterpolationSmoothing * DtoR)) /
					(m_Metadata.m_MaxHeadingDeltaForAimWeaponInterpolationSmoothing * DtoR);
		t		= Clamp(t, 0.0f, 1.0f);

		u32 minDuration = m_Metadata.m_MinAimWeaponInterpolateInDuration;
		u32 maxDuration = m_Metadata.m_MaxAimWeaponInterpolateInDuration;
		if (isAimingDuringVehicleExit)
		{
			minDuration = m_Metadata.m_MinAimWeaponInterpolateInDurationDuringVehicleExit;
			maxDuration = m_Metadata.m_MaxAimWeaponInterpolateInDurationDuringVehicleExit;
		}
		else if (m_FollowPedCoverSettings.m_IsInCover)
		{
			minDuration = m_Metadata.m_MinAimWeaponInterpolateInDurationInCover;
			maxDuration = m_Metadata.m_MaxAimWeaponInterpolateInDurationInCover;
		}
		else if (CPlayerPedTargeting::GetAreCNCFreeAimImprovementsEnabled(&followPed))
		{
			minDuration = m_Metadata.m_MinAimWeaponInterpolateInDurationFreeAimNew;
			maxDuration = m_Metadata.m_MaxAimWeaponInterpolateInDurationFreeAimNew;
		}

		interpolationDuration = static_cast<s32>(Lerp(t, minDuration, maxDuration));
	}

	return shouldInterpolate;
}

bool camGameplayDirector::ComputeIsVehicleAiming(const CVehicle& followVehicle, const CPed& followPed) const
{
	//Only request aim cameras for the local player, as they require control input and free-aim input/orientation is not synced for remote players.
	if(!followPed.IsLocalPlayer())
	{
		return false;
	}

	if(followPed.GetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_AIM_CAMERA))
	{
		return false;
	}

	//Block vehicle aiming during long and medium Switch transitions.
	if(g_PlayerSwitch.IsActive() && (g_PlayerSwitch.GetSwitchType() != CPlayerSwitchInterface::SWITCH_TYPE_SHORT))
	{
		return false;
	}

	//Only allow vehicle aiming when the follow ped is fully inside the vehicle.
	const bool isFollowPedInVehicle	= (m_VehicleEntryExitState == INSIDE_VEHICLE) &&
										followPed.GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle);
	if(!isFollowPedInVehicle)
	{
		return false;
	}

	const bool isFollowPedDriver				= followPed.GetIsDrivingVehicle();
	const CVehicleModelInfo* vehicleModelInfo	= followVehicle.GetVehicleModelInfo();
	const bool shouldBlockDrivebyAiming			= isFollowPedDriver && vehicleModelInfo &&
													vehicleModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_DRIVER_NO_DRIVE_BY);
	if(shouldBlockDrivebyAiming)
	{
		return false;
	}

	if(followPed.GetWeaponManager() && followPed.GetWeaponManager()->GetIsArmedMelee())
	{
		return false;
	}

	//Don't go into aim cam when using certain vehicles with weapons
	bool bVehicleBlocksAim = followVehicle.InheritsFromSubmarineCar() || (MI_PLANE_BOMBUSHKA.IsValid() && followVehicle.GetModelIndex() == MI_PLANE_BOMBUSHKA) || (MI_PLANE_VOLATOL.IsValid() && followVehicle.GetModelIndex() == MI_PLANE_VOLATOL);
	if (bVehicleBlocksAim && followPed.GetEquippedWeaponInfo() && followPed.GetEquippedWeaponInfo()->GetIsVehicleWeapon())
	{
		return false;
	}

	if (followPed.GetPedConfigFlag(CPED_CONFIG_FLAG_IsPerformingVehicleMelee))
		return false;

	bool isDrivebyAiming = false;
	const CPedIntelligence* followPedIntelligence = followPed.GetPedIntelligence();
	if(followPedIntelligence)
	{
		const CQueriableInterface* followPedQueriableInterface = followPedIntelligence->GetQueriableInterface();	
		if(followPedQueriableInterface)
		{
			isDrivebyAiming = followPedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_VEHICLE_GUN, true);
			if(!isDrivebyAiming)
			{
				const CTaskMountThrowProjectile* mountThrowProjectileTask	= static_cast<const CTaskMountThrowProjectile*>(followPedIntelligence->FindTaskActiveByType(
																				CTaskTypes::TASK_MOUNT_THROW_PROJECTILE));
				if(mountThrowProjectileTask)
				{
					//NOTE: We block the aim camera when dropping, rather than aiming, a thrown weapon. This information is cached in the intro state.
					//We must also block the aim camera in the open door and smash window states, as they can occur when dropping.
					const s32 state = mountThrowProjectileTask->GetState();
					if((state >= CTaskMountThrowProjectile::State_Intro) && (state != CTaskMountThrowProjectile::State_OpenDoor) &&
						(state != CTaskMountThrowProjectile::State_SmashWindow))
					{
						isDrivebyAiming = !mountThrowProjectileTask->IsDropping() FPS_MODE_SUPPORTED_ONLY(|| followPed.IsInFirstPersonVehicleCamera(true));
					}
				}
			}

			bool bIsPedOnTurretSeat = followVehicle.GetVehicleModelInfo() && followVehicle.GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_TURRET_SEAT_ON_VEHICLE);

            const s32 iSeatIndex = ComputeSeatIndexPedIsIn(followVehicle, followPed);

			if (MI_CAR_APC.IsValid() && followVehicle.GetModelIndex() == MI_CAR_APC)
			{
				if(iSeatIndex == 1)
				{
					bIsPedOnTurretSeat = true;
				}
			}

            if(!IsTurretSeat(&followVehicle, iSeatIndex))
            {
                bIsPedOnTurretSeat = false;
            }

			bool bForceVehicleAiming = false;
			const CVehicleDriveByInfo* pDrivebyInfo = CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(&followPed);
			if (pDrivebyInfo && pDrivebyInfo->GetUseAimCameraInThisSeat())
			{
				bForceVehicleAiming = true;
			}

			if (MI_CAR_CHERNOBOG.IsValid() && followVehicle.GetModelIndex() == MI_CAR_CHERNOBOG)
			{
				if (!followVehicle.GetAreOutriggersFullyDeployed())
				{
					bForceVehicleAiming = false;
				}
			}

			if(!isDrivebyAiming && (bIsPedOnTurretSeat || bForceVehicleAiming))
			{
				isDrivebyAiming = followPedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_VEHICLE_MOUNTED_WEAPON, true);
			}
		}
	}

	if(!isDrivebyAiming)
	{
		//Check for lock-on aiming, if the vehicle supports it.
		const bool shouldLockOnAim = ComputeShouldVehicleLockOnAim(followVehicle, followPed);
		if(shouldLockOnAim)
		{
			const CPlayerInfo* playerInfo	= followPed.GetPlayerInfo();
			isDrivebyAiming					= playerInfo && (playerInfo->GetTargeting().GetLockOnTarget() != NULL);
		}
	}

	return isDrivebyAiming;
}

bool camGameplayDirector::ComputeShouldVehicleLockOnAim(const CVehicle& followVehicle, const CPed& followPed) const
{
	tGameplayCameraSettings desiredSettings;
	ComputeVehicleAimCameraSettings(followVehicle, followPed, desiredSettings);

	const camBaseCameraMetadata* metadata = camFactory::FindObjectMetadata<camBaseCameraMetadata>(desiredSettings.m_CameraHash);

	bool shouldLockOnAim	= metadata && camFactory::IsObjectMetadataOfClass<camThirdPersonAimCameraMetadata>(*metadata) &&
								static_cast<const camThirdPersonAimCameraMetadata*>(metadata)->m_ShouldUseLockOnAiming;
	if(shouldLockOnAim)
	{
		const bool isVehicleAimCamera = camFactory::IsObjectMetadataOfClass<camThirdPersonVehicleAimCameraMetadata>(*metadata);
		if(isVehicleAimCamera)
		{
			const camThirdPersonVehicleAimCameraMetadata* vehicleAimCameraMetadata = static_cast<const camThirdPersonVehicleAimCameraMetadata*>(metadata);

			const bool isFollowPedDriver	= followPed.GetIsDrivingVehicle();
			shouldLockOnAim					= isFollowPedDriver ? vehicleAimCameraMetadata->m_ShouldUseLockOnAimingForDriver :
												vehicleAimCameraMetadata->m_ShouldUseLockOnAimingForPassenger;
		}
	}

	return shouldLockOnAim;
}

bool camGameplayDirector::CreateActiveCamera(u32 cameraHash)
{
	if(m_PreviousCamera)
	{
		//The active camera must have been interpolating, so clean-up the interpolation source camera so we don't leak it.
		delete m_PreviousCamera;
	}

	camBaseCamera* newCamera = camFactory::CreateObject<camBaseCamera>(cameraHash);
	if(newCamera)
	{
		m_PreviousCamera	= m_ActiveCamera;
		m_ActiveCamera		= newCamera;
	}

	if((m_ActiveCamera && m_ActiveCamera->ShouldPreventAnyInterpolation()) || (m_PreviousCamera && m_PreviousCamera->ShouldPreventAnyInterpolation()))
	{
		m_PreventVehicleEntryExit = true;
	}

	return (newCamera != NULL);
}

bool camGameplayDirector::IsCameraInterpolating(const camBaseCamera* sourceCamera) const
{
	bool isInterpolating = m_ActiveCamera && m_ActiveCamera->IsInterpolating(sourceCamera);
	return isInterpolating;
}

bool camGameplayDirector::IsTableGamesCameraRunning() const
{
	return (m_ActiveCamera && m_ActiveCamera->GetIsClassId(camTableGamesCamera::GetStaticClassId())) || (m_ScriptOverriddenTableGamesCameraSettingsThisUpdate.m_CameraHash != 0);
}

bool camGameplayDirector::WasTableGamesCameraRunning() const
{
	return (m_ScriptOverriddenTableGamesCameraSettingsOnPreviousUpdate.m_CameraHash != 0);
}

u32 camGameplayDirector::GetTableGamesCameraName() const
{
	return m_ScriptOverriddenTableGamesCameraSettingsThisUpdate.m_CameraHash;
}

u32 camGameplayDirector::GetBlendingOutTableGamesCameraName() const
{
	if (m_PreviousCamera)
	{
		return m_CachedPreviousCameraTableGamesCameraHash;
	}

	return 0;
}

bool camGameplayDirector::ComputeShouldRecreateFollowPedCamera(const CPed& followPed) const
{
	if(!m_ActiveCamera)
	{
		return true;
	}

	tGameplayCameraSettings desiredSettings;
	ComputeFollowPedCameraSettings(followPed, desiredSettings);

	u32 currentCameraHash				= m_ActiveCamera->GetNameHash();
	const CEntity* currentAttachEntity	= m_ActiveCamera->GetAttachParent();

	//NOTE: We recreate the camera if the desired camera (name) hash or the desired attach entity have changed.
	bool shouldRecreate = (desiredSettings.m_CameraHash != currentCameraHash) || (desiredSettings.m_AttachEntity != currentAttachEntity);
	if( !shouldRecreate  FPS_MODE_SUPPORTED_ONLY(&& !m_ActiveCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId())) )
	{
		//We also recreate an active aim camera if the weapon info for the follow ped has changed.
		//For now, first person camera is same for all weapons, so no need to re-create first person camera for weapon change.
		const bool isAiming	= IsAiming();
		shouldRecreate		= isAiming && (m_FollowPedWeaponInfo.Get() != m_FollowPedWeaponInfoOnPreviousUpdate.Get());
	}

	return shouldRecreate;
}

bool camGameplayDirector::ComputeShouldRecreateFollowVehicleCamera(const CVehicle& followVehicle) const
{
	if(!m_ActiveCamera)
	{
		return true;
	}

	tGameplayCameraSettings desiredSettings;
	ComputeFollowVehicleCameraSettings(followVehicle, desiredSettings);

	u32 currentCameraHash				= m_ActiveCamera->GetNameHash();
	const CEntity* currentAttachEntity	= m_ActiveCamera->GetAttachParent();

	//NOTE: We recreate the camera if the desired camera (name) hash or the desired attach entity have changed.
	const bool shouldRecreate = (desiredSettings.m_CameraHash != currentCameraHash) || (desiredSettings.m_AttachEntity != currentAttachEntity);

	return shouldRecreate;
}

bool camGameplayDirector::ComputeShouldRecreateVehicleAimCamera(const CVehicle& followVehicle, const CPed& followPed) const
{
	if(!m_ActiveCamera)
	{
		return true;
	}
		
	tGameplayCameraSettings desiredSettings;
	ComputeVehicleAimCameraSettings(followVehicle, followPed, desiredSettings);

	u32 currentCameraHash				= m_ActiveCamera->GetNameHash();
	const CEntity* currentAttachEntity	= m_ActiveCamera->GetAttachParent();

	//NOTE: We recreate the camera if the desired camera (name) hash or the desired attach entity have changed.
	const bool shouldRecreate			= (desiredSettings.m_CameraHash != currentCameraHash) || (desiredSettings.m_AttachEntity != currentAttachEntity);

	return shouldRecreate;
}

void camGameplayDirector::UpdateCatchUp()
{
	if(m_ShouldInitCatchUpFromFrame)
	{
		camThirdPersonCamera* thirdPersonCamera = GetThirdPersonCamera();
		if(thirdPersonCamera)
		{
			thirdPersonCamera->CatchUpFromFrame(m_CatchUpSourceFrame, m_CatchUpMaxDistanceToBlend, m_CatchUpBlendType);

			//Ensure that any interpolation is stopped immediately so that we get a clean cut.
			thirdPersonCamera->StopInterpolating();
		}

		m_ShouldInitCatchUpFromFrame = false;
	}
}

void camGameplayDirector::ComputeMissileHintTarget()
{
	if(m_CodeHintEntity == NULL)
	{
		camGameplayDirector& gameplayDirector	= camInterface::GetGameplayDirector();
		const s32 vehicleEntryExitState	= gameplayDirector.GetVehicleEntryExitState();
		if(vehicleEntryExitState == INSIDE_VEHICLE )
		{
			const camFollowVehicleCamera* followVehicleCamera = camInterface::GetGameplayDirector().GetFollowVehicleCamera();

			if(followVehicleCamera)
			{
				//the follow vehicle is also not valid
				const CEntity* followVehicle = followVehicleCamera->GetAttachParent();

				if(!followVehicle || !followVehicle->GetIsTypeVehicle())
				{
					return;
				}

				const CVehicle* pVehicle = static_cast<const CVehicle*>(followVehicle); 

				if(pVehicle->InheritsFromPlane() || pVehicle->InheritsFromHeli() )
				{
					if(pVehicle->GetLockOnTarget())
					{
						CPhysical* pTarget = pVehicle->GetLockOnTarget(); 
						//CVehicle* TargetVehicle = NULL; 

						if(pTarget && pTarget->GetIsTypePed())
						{
							CPed* pTargetPed = static_cast<CPed*>(pTarget); 
							
							if(pTargetPed->GetIsInVehicle())
							{
								pTarget = pTargetPed->GetVehiclePedInside(); 
							}

						}

						if(pTarget)
						{
							atArray<RegdProjectile> Rockets; 
							
							CProjectileManager::GetProjectilesForOwner(pVehicle, Rockets); 

							for(int i =0; i < Rockets.GetCount(); i++)
							{
								CProjectileRocket* pRocket = Rockets[i]->GetAsProjectileRocket(); 
								
								if(pRocket && pRocket->GetTarget() == pTarget)
								{
									m_CodeHintEntity = pTarget; 
									m_CanActivateCodeHint = true; 
								}
							}	
						}
					}
				}
			}
		}
	}
}

void camGameplayDirector::ToggleMissileHint()
{
	if(m_HasUserSelectedCodeHint && !m_IsCodeHintActive && m_CanActivateCodeHint)
	{
		if(!IsHintActive())
		{
			StartHinting(m_CodeHintEntity, VEC3_ZERO, false , m_Metadata.m_MissileHintBlendInDuration, -1, m_Metadata.m_MissileHintBlendOutDuration,m_Metadata.m_MissileHintHelperRef);
			m_IsCodeHintActive = true;
		}
	}
	else if(m_IsCodeHintActive)	
	{
		if(!m_HasUserSelectedCodeHint || !m_CanActivateCodeHint)
		{
			StopHinting(false);
		}
	}
}

void camGameplayDirector::UpdateMissileHintSelection()
{
	UpdateInputForQuickToggleActivationMode(); 

	if(m_LatchedState || m_IsCodeHintButtonDown)
	{
		m_HasUserSelectedCodeHint = true;
	}
	else
	{
		m_HasUserSelectedCodeHint = false;
	}
}

bool camGameplayDirector::CanHintAtMissile()
{
	const camFollowVehicleCamera* followVehicleCamera = camInterface::GetGameplayDirector().GetFollowVehicleCamera();

	if(followVehicleCamera)
	{
		//the follow vehicle is also not valid
		const CEntity* followVehicle = followVehicleCamera->GetAttachParent();

		if(m_CodeHintEntity && m_CanActivateCodeHint && followVehicle && followVehicle->GetIsTypeVehicle())
		{
			bool IsValid = true; 	
			bool IsDead = false; 
			
			//Work out if the target entity has been hit
			if (m_CodeHintEntity.Get())
			{
				if (m_CodeHintEntity.Get()->GetIsTypePed())
				{
					const CPed* pPed = static_cast<const CPed*>(m_CodeHintEntity.Get());

					IsDead = pPed->IsFatallyInjured();
				}
				else if (m_CodeHintEntity.Get()->GetIsTypeVehicle())
				{
					const CVehicle* pVehicle = static_cast<const CVehicle*>(m_CodeHintEntity.Get());

					IsDead = (pVehicle->GetStatus() == STATUS_WRECKED) || pVehicle->m_nVehicleFlags.bIsDrowning;
				}
				else 
				{
					IsDead = m_CodeHintEntity.Get()->GetHealth() < 0.001f;
				}
			}
			
			const CVehicle* pPlayersVehicle = static_cast<const CVehicle*>(followVehicle); 

			if(!IsDead)
			{
				//check there are still rockets active for the target which is still alive
				atArray<RegdProjectile> Rockets; 

				CProjectileManager::GetProjectilesForOwner(pPlayersVehicle, Rockets); 

				u32 RocketCount = 0; 

				Vector3 TargetPos = VEC3V_TO_VECTOR3(m_CodeHintEntity->GetTransform().GetPosition()); 

				bool AreAllMissilesMissing = true; 

				for(int i =0; i < Rockets.GetCount(); i++)
				{
					CProjectileRocket* pRocket = Rockets[i]->GetAsProjectileRocket(); 

					if(pRocket && pRocket->GetTarget() == m_CodeHintEntity)
					{
						RocketCount++; 

						Vector3 currentPos = VEC3V_TO_VECTOR3(pRocket->GetTransform().GetPosition()); 
						Vector3 previousPos = pRocket->GetPreviousPosition(); 

						if(TargetPos.Dist2(currentPos) < TargetPos.Dist2(previousPos))
						{
							AreAllMissilesMissing = false; 
							break; 
						}
					}
				}

				if(RocketCount == 0 || (RocketCount > 0 && AreAllMissilesMissing))
				{
					m_CodeHintTimer+= fwTimer::GetTimeStepInMilliseconds(); 
				}

				if(m_CodeHintTimer > m_Metadata.m_MissileHintMissedDuration )
				{	
					IsValid = false; 
				}
			}
			else
			{
				//Stay looking at the target for a while after its dead
				m_CodeHintTimer+= fwTimer::GetTimeStepInMilliseconds(); 

				if(m_CodeHintTimer >  m_Metadata.m_MissileHintKillDuration )
				{
					IsValid = false; 
				}
			}

			if(!IsValid)
			{
				m_CanActivateCodeHint = false;
				m_CodeHintEntity = NULL; 
				m_CodeHintTimer = 0; 
			}
			
			return IsValid;
		}
	}

	m_CanActivateCodeHint = false;
	m_CodeHintEntity = NULL; 
	m_CodeHintTimer = 0; 

	return false; 
}

void camGameplayDirector::UpdateMissileHint()
{	
	//compute the hint target if there isn't one
	ComputeMissileHintTarget(); 

	//Check that the state is still valid to start a hint
	CanHintAtMissile();
	
	//update if the code hint is active or script haven't got a hint running and are not blocking 
	if(m_IsCodeHintActive || (!m_IsCodeHintActive && !m_IsScriptBlockingCodeHints && !IsHintActive()))
	{
		//Check the player is 
		UpdateMissileHintSelection(); 
		
		ToggleMissileHint(); 
	}
	
	if(m_IsCodeHintActive)
	{
		m_IsCodeHintActive = IsHintActive(); 
	}
	
	if(!m_IsCodeHintActive)
	{
		m_LatchedState = false; 
	}

	m_IsScriptBlockingCodeHints = false; 
}

void camGameplayDirector::UpdateInputForQuickToggleActivationMode()
{
	m_IsCodeHintButtonDown = false; 

	CControl* control = camInterface::GetGameplayDirector().GetActiveControl();
	if(control)
	{
		if(control->GetVehicleCinematicCam().IsPressed())
		{
			//store the time
			m_TimeHintButtonInitalPressed  = fwTimer::GetCamTimeInMilliseconds(); 
		}

		if(control->GetVehicleCinematicCam().IsReleased())
		{
			if(fwTimer::GetCamTimeInMilliseconds() -  m_TimeHintButtonInitalPressed  < m_Metadata.m_QuickToggleTimeThreshold)
			{
				m_LatchedState = !m_LatchedState; 
				m_TimeHintButtonInitalPressed = 0; 
	
			}
		}

		if(control->GetVehicleCinematicCam().IsDown())
		{
			if(fwTimer::GetCamTimeInMilliseconds() -  m_TimeHintButtonInitalPressed  > m_Metadata.m_QuickToggleTimeThreshold  )
			{
				m_IsCodeHintButtonDown = true;
				m_LatchedState = false; 
			}
		}
	}
}

bool camGameplayDirector::ComputeAttachPedRootBoneMatrix(Matrix34& matrix) const
{
	if(!m_HintEnt || !m_HintEnt->GetIsTypePed())
	{
		return false;
	}

	const CPed* attachPed				= static_cast<const CPed*>(m_HintEnt.Get());
	const crSkeleton* attachPedSkeleton	= attachPed->GetSkeleton();
	if(!attachPedSkeleton)
	{
		return false;
	}

	const s32 rootBoneIndex = attachPed->GetBoneIndexFromBoneTag(BONETAG_ROOT);
	if(rootBoneIndex == -1)
	{
		return false;
	}

	//NOTE: We manually transform the local bone matrix into world space here, as the attach ped's skeleton may
	//not yet have been updated.
	const Matrix34& localBoneMatrix	= RCC_MATRIX34(attachPedSkeleton->GetLocalMtx(rootBoneIndex));
	const Matrix34 attachPedMatrix	= MAT34V_TO_MATRIX34(attachPed->GetTransform().GetMatrix());

	matrix.Dot(localBoneMatrix, attachPedMatrix);

	return true;
}

void camGameplayDirector::UpdateRagdollBlendLevel()
{
	if(m_HintEnt && m_HintEnt->GetIsTypePed())
	{
		const bool shouldApplyRagdollBehaviour = camFollowPedCamera::ComputeShouldApplyRagdollBehavour(*m_HintEnt);
	
		if(m_RagdollBlendEnvelope)
		{
			if(m_IsHintFirstUpdate && shouldApplyRagdollBehaviour)
			{
				m_RagdollBlendEnvelope->Start(1.0f);
			}
			else 
			{
				m_RagdollBlendEnvelope->AutoStartStop(shouldApplyRagdollBehaviour);
			}

			m_RagdollBlendLevel	= m_RagdollBlendEnvelope->Update();
			m_RagdollBlendLevel	= Clamp(m_RagdollBlendLevel, 0.0f, 1.0f);
			m_RagdollBlendLevel = SlowInOut(m_RagdollBlendLevel); 
		}
		else
		{
			//We don't have an envelope, so just use the state directly.
			m_RagdollBlendLevel = shouldApplyRagdollBehaviour ? 1.0f : 0.0f;
		}
	}
}


void camGameplayDirector::UpdateHint()
{
	if(m_HintEnvelope)
	{
		if (m_HintEnvelope->IsActive())
		{
			if(m_HintEnt)
			{
				m_HintLookAtMat = MAT34V_TO_MATRIX34(m_HintEnt->GetTransform().GetMatrix()); 
				
				if(m_HintEnt->GetIsTypePed())
				{
					UpdateRagdollBlendLevel(); 

					if(m_RagdollBlendLevel >= SMALL_FLOAT)
					{
						Matrix34 rootBoneMatrix;
						const bool isRootBoneValid = ComputeAttachPedRootBoneMatrix(rootBoneMatrix);
						if(isRootBoneValid)
						{
							Matrix34 blendedMatrix;
							camFrame::SlerpOrientation(m_RagdollBlendLevel, m_HintLookAtMat, rootBoneMatrix, blendedMatrix);

							blendedMatrix.d.Lerp(m_RagdollBlendLevel, m_HintLookAtMat.d, rootBoneMatrix.d);

							m_HintLookAtMat.Set(blendedMatrix);
						}
					}
				}
			}
	
			m_HintEnvelopeLevel = m_HintEnvelope->Update(); 
			m_HintEnvelopeLevel = SlowInOut(m_HintEnvelopeLevel); 
			m_HintEnvelopeLevel = SlowInOut(m_HintEnvelopeLevel); 
		}
		else
		{
			delete m_HintEnvelope;
			m_HintEnvelope = NULL; 
			m_HintEnvelopeLevel = 0.0f; 
			m_IsScriptOverridingHintFovScalar = false; 
			m_IsScriptOverridingHintDistanceScalar = false; 
			m_IsScriptOverridingHintBaseOrbitPitchOffset = false; 
			m_IsScriptOverridingHintCameraRelativeSideOffsetAdditive = false; 
			m_IsScriptOverridingHintCameraRelativeVerticalOffsetAdditive = false;
			m_IsScriptForcingHintCameraBlendToFollowPedMediumViewMode = false;
			if(m_RagdollBlendEnvelope)
			{
				delete m_RagdollBlendEnvelope; 
				m_RagdollBlendEnvelope = NULL; 
			}
			m_RagdollBlendLevel = 0.0f; 
#if __BANK
			m_DebugHintActive = false;
#endif
		}

		m_IsHintFirstUpdate = false; 
	}
}

void camGameplayDirector::UpdateAimAccuracyOffsetShake()
{
	camBaseFrameShaker* accuracyOffsetShake	= NULL;
	if(m_FollowPedWeaponInfo)
	{
		const u32 accuracyOffsetShakeHash	= m_FollowPedWeaponInfo->GetAccuracyOffsetShakeHash();
		accuracyOffsetShake					= accuracyOffsetShakeHash ? m_ActiveCamera->FindFrameShaker(accuracyOffsetShakeHash) : NULL;
	}

	if(accuracyOffsetShake)
	{
		const float amplitudeOnPreviousUpdate = m_AimAccuracyOffsetAmplitudeSpring.GetResult();

		const float desiredAmplitude	= ComputeDesiredAimAccuracyOffsetAmplitude();
		const float springConstant		= (desiredAmplitude > amplitudeOnPreviousUpdate) ?
											m_Metadata.m_AccuracyOffsetAmplitudeBlendInSpringConstant :
											m_Metadata.m_AccuracyOffsetAmplitudeBlendOutSpringConstant;
		const float amplitudeToApply	= m_AimAccuracyOffsetAmplitudeSpring.Update(desiredAmplitude, springConstant,
											m_Metadata.m_AccuracyOffsetAmplitudeSpringDampingRatio);

		accuracyOffsetShake->SetAmplitude(amplitudeToApply);

		PF_SET(camGameplayDirectorAccuracyOffsetScaling, desiredAmplitude);
		PF_SET(camGameplayDirectorAccuracyOffsetScalingDamped, amplitudeToApply);
	}
	else
	{
		m_AimAccuracyOffsetAmplitudeSpring.Reset();
	}
}

float camGameplayDirector::ComputeDesiredAimAccuracyOffsetAmplitude() const
{
	if(!m_FollowPed || !m_FollowPedWeaponInfo)
	{
		return 0.0f;
	}

	const CWeapon* followPedWeapon			= NULL;
	const CPedWeaponManager* weaponManager	= m_FollowPed->GetWeaponManager();
	if(weaponManager)
	{
		followPedWeapon = weaponManager->GetEquippedWeapon();
	}

	if(!followPedWeapon)
	{
		return 0.0f;
	}

	const bool isAiming = IsAiming(m_FollowPed);
	if(!isAiming)
	{
		return 0.0f;
	}

	//Apply any weapon-specific amplitude scaling.
	float desiredAmplitude = followPedWeapon->GetRecoilShakeAmplitude();

	if(m_ActiveCamera->GetIsClassId(camThirdPersonAimCamera::GetStaticClassId()))
	{
		const float amplitudeScaling	= static_cast<camThirdPersonAimCamera*>(m_ActiveCamera.Get())->GetRecoilShakeAmplitudeScaling();
		desiredAmplitude				*= amplitudeScaling;
	}

	//Scale the amplitude based upon the current accuracy, which is based on stats and will degrade over time with sustained fire.
	sWeaponAccuracy weaponAccuracy;
	followPedWeapon->GetAccuracy(m_FollowPed, -1.0f, weaponAccuracy);

	const float amplitudeScaling	= RampValueSafe(weaponAccuracy.fAccuracyMin, m_Metadata.m_AccuracyOffsetShakeAccuracyLimits.x,
										m_Metadata.m_AccuracyOffsetShakeAccuracyLimits.y,
										m_Metadata.m_AccuracyOffsetShakeAmplitudeScalingAtAccuracyLimits.x,
										m_Metadata.m_AccuracyOffsetShakeAmplitudeScalingAtAccuracyLimits.y);

	desiredAmplitude *= amplitudeScaling;

	return desiredAmplitude;
}

void camGameplayDirector::ApplyCameraOverridesForScript()
{
	if(!m_ActiveCamera)
	{
		return;
	}

	if(m_UseScriptRelativeHeading)
	{
		SetRelativeCameraHeading(m_CachedScriptHeading);

		//Ensure that any interpolation is stopped immediately so that we get a clean cut.
		m_ActiveCamera->StopInterpolating();

		m_UseScriptRelativeHeading = false;
	}

	if(m_UseScriptRelativePitch)
	{
		SetRelativeCameraPitch(m_CachedScriptPitch, m_CachedScriptPitchRate);

		if(m_CachedScriptPitchRate >= (1.0f - SMALL_FLOAT))
		{
			//Ensure that any interpolation is stopped immediately so that we get a clean cut.
			m_ActiveCamera->StopInterpolating();
		}

		m_UseScriptRelativePitch = false;
	}

	if (IsFallingBackToThirdPersonForDirectorBlendCatchUpOrSwitch())
	{
		if (m_UseFirstPersonFallbackRelativeHeading)
		{
			SetRelativeCameraHeading(m_CachedFirstPersonFallbackHeading);
			m_UseFirstPersonFallbackRelativeHeading = false;
		}
		if(m_UseFirstPersonFallbackRelativePitch)
		{
			SetRelativeCameraPitch(m_CachedFirstPersonFallbackPitch, m_CachedFirstPersonFallbackPitch);

			if(m_CachedFirstPersonFallbackPitch >= (1.0f - SMALL_FLOAT))
			{
				//Ensure that any interpolation is stopped immediately so that we get a clean cut.
				m_ActiveCamera->StopInterpolating();
			}

			m_UseFirstPersonFallbackRelativePitch = false;
		}
	}

	camFirstPersonAimCamera* firstPersonAimCamera = GetFirstPersonAimCamera();
	if(firstPersonAimCamera)
	{
		if(m_ShouldScriptOverrideFirstPersonAimCameraRelativeHeadingLimitsThisUpdate)
		{
			firstPersonAimCamera->SetRelativeHeadingLimits(m_ScriptOverriddenFirstPersonAimCameraRelativeHeadingLimitsThisUpdate);
		}

		if(m_ShouldScriptOverrideFirstPersonAimCameraRelativePitchLimitsThisUpdate)
		{
			firstPersonAimCamera->SetRelativePitchLimits(m_ScriptOverriddenFirstPersonAimCameraRelativePitchLimitsThisUpdate);
		}

		if(m_ShouldScriptOverrideFirstPersonAimCameraNearClipThisUpdate)
		{
			firstPersonAimCamera->SetNearClipThisUpdate(m_ScriptOverriddenFirstPersonAimCameraNearClipThisUpdate); 
		}

		if( m_ActiveCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()) )
		{
			if(m_UseScriptFirstPersonShooterHeading)
			{
				SetRelativeCameraHeading( m_CachedScriptFirstPersonShooterHeading );
				m_ActiveCamera->StopInterpolating();
				m_UseScriptFirstPersonShooterHeading = false;
			}

			if(m_UseScriptFirstPersonShooterPitch)
			{
				SetRelativeCameraPitch( m_CachedScriptFirstPersonShooterPitch );
				m_ActiveCamera->StopInterpolating();
				m_UseScriptFirstPersonShooterPitch = false;
			}
		}
	}

	camThirdPersonAimCamera* ThirdPersonAimCamera =	GetThirdPersonAimCamera(); 
	
	if(ThirdPersonAimCamera)
	{
		if(m_ShouldScriptOverrideThirdPersonAimCameraNearClipThisUpdate)
		{
			ThirdPersonAimCamera->SetNearClipThisUpdate(m_ScriptOverriddenThirdPersonAimCameraNearClipThisUpdate); 
		}
	}

	if(m_ShouldFollowCameraIgnoreAttachParentMovementThisUpdateForScript)
	{
		camFollowCamera* followCamera = GetFollowCamera();
		if(followCamera)
		{
			followCamera->IgnoreAttachParentMovementForOrientationThisUpdate();
		}
	}

	//check the entity to ignore exists and tell the active camera to add to the collision helper

	if(m_ActiveCamera && m_ActiveCamera->GetCollision() && m_ScriptEntityToIgnoreCollisionThisUpdate)
	{
		m_ActiveCamera->GetCollision()->IgnoreEntityThisUpdate(*m_ScriptEntityToIgnoreCollisionThisUpdate); 
	}
	
	m_ScriptEntityToIgnoreCollisionThisUpdate = NULL; 
}

void camGameplayDirector::UpdateMotionBlur(const CPed& followPed)
{
	bool isValidToApplyMotionBlur = !NetworkInterface::IsInSpectatorMode();

	if(isValidToApplyMotionBlur)
	{
		float motionBlurStrength = m_Frame.GetMotionBlurStrength();

		const bool canUseVisualSettings = g_visualSettings.GetIsLoaded();

		if(followPed.ShouldBeDead())
		{
			const float wastedMotionBlurStrength = canUseVisualSettings ? g_visualSettings.Get("cam.game.blur.wasted") : 0.1f;
			motionBlurStrength += wastedMotionBlurStrength;
		}

		if(followPed.GetIsArrested())
		{
			const float bustedMotionBlurStrength = canUseVisualSettings ? g_visualSettings.Get("cam.game.blur.busted") : 0.1f;
			motionBlurStrength += bustedMotionBlurStrength;
		}

		m_Frame.SetMotionBlurStrength(motionBlurStrength);
	}
}

void camGameplayDirector::UpdateDepthOfField()
{
#if __BANK	// for now is debug only
	if (m_DebugOverridingDofThisUpdate)
	{
		m_Frame.SetNearInFocusDofPlane(m_DebugOverrideNearDofThisUpdate);
		m_Frame.SetFarInFocusDofPlane(m_DebugOverrideFarDofThisUpdate);
		m_DebugOverridingDofThisUpdate = false;
	}
#endif // __BANK
}

void camGameplayDirector::UpdateReticuleSettings(const CPed& followPed)
{
	//Hide the reticule during player Switch transitions.
	if(g_PlayerSwitch.IsActive())
	{
		m_ShouldDisplayReticule	= false;
		m_ShouldDimReticule		= false;

		return;
	}

	const camFirstPersonAimCamera* firstPersonAimCamera = GetFirstPersonAimCamera();
	const camThirdPersonAimCamera* thirdPersonAimCamera = GetThirdPersonAimCamera();
	m_ShouldDisplayReticule								= (firstPersonAimCamera && firstPersonAimCamera->ShouldDisplayReticule()) ||
															(thirdPersonAimCamera && thirdPersonAimCamera->ShouldDisplayReticule());

	// Ask 1st person camera too. 
	if(!m_ShouldDisplayReticule)
	{
		const camBaseCamera* pDominantCamera = camInterface::GetDominantRenderedCamera();
		if(pDominantCamera && pDominantCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
		{
			m_ShouldDisplayReticule = static_cast<const camCinematicMountedCamera*>(pDominantCamera)->ShouldDisplayReticule();
		}
	}

	m_ShouldDimReticule = followPed.GetPedResetFlag(CPED_RESET_FLAG_DimTargetReticule);
	if(!m_ShouldDimReticule)
	{
		if(m_FollowPedCoverSettings.m_IsInCover)
		{
			m_ShouldDimReticule = m_FollowPedCoverSettings.m_IsReloading;
		}
	}

	if(!m_ShouldDimReticule)
	{
		const CPedWeaponManager* pManager = followPed.GetWeaponManager();
		const CWeapon* pWeapon = pManager ? pManager->GetEquippedWeapon() : NULL;

		if(pWeapon && pWeapon->GetWeaponInfo() && pWeapon->GetWeaponInfo()->GetTimeBetweenShots() >= 0.5f)
		{
			m_ShouldDimReticule = pWeapon->GetState() == CWeapon::STATE_WAITING_TO_FIRE;
		}
	}

	if(!m_ShouldDimReticule && followPed.GetIsInVehicle() && !followPed.GetIsDrivingVehicle())
	{
		CPedIntelligence* pIntelligence = followPed.GetPedIntelligence();

		if(pIntelligence)
		{
			const CTaskAimGunVehicleDriveBy* pDriveByTask = static_cast<CTaskAimGunVehicleDriveBy*>(pIntelligence->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY));
			if(pDriveByTask)
			{
				m_ShouldDimReticule = !pDriveByTask->GetIsReadyToFire();
			}
			else
			{
				m_ShouldDimReticule =	pIntelligence->FindTaskActiveByType(CTaskTypes::TASK_MOUNT_THROW_PROJECTILE) == NULL && 
										pIntelligence->FindTaskActiveByType(CTaskTypes::TASK_VEHICLE_MOUNTED_WEAPON) == NULL;
			}
		}
	}

	//Test to see if the follow ped is aiming directly at nearby collision.
	if(!m_ShouldDimReticule && thirdPersonAimCamera && (m_Metadata.m_AimingLosProbeLengthForDimmingReticule >= SMALL_FLOAT))
	{
		const Vector3& startPosition	= thirdPersonAimCamera->GetLookAtPosition();
		const Vector3 endPosition		= startPosition + (m_Metadata.m_AimingLosProbeLengthForDimmingReticule *
											thirdPersonAimCamera->GetFrame().GetFront());

		WorldProbe::CShapeTestProbeDesc probeTest;
		probeTest.SetStartAndEnd(startPosition, endPosition);
		probeTest.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_WEAPON);
		probeTest.SetOptions(WorldProbe::LOS_IGNORE_SEE_THRU | WorldProbe::LOS_IGNORE_SHOOT_THRU | WorldProbe::LOS_IGNORE_NO_COLLISION);
		probeTest.SetContext(WorldProbe::LOS_Camera);

		m_ShouldDimReticule = WorldProbe::GetShapeTestManager()->SubmitTest(probeTest);
	}
}

void camGameplayDirector::UpdateTrailerStatus()
{
	if(!m_FollowVehicle.Get() || !ComputeCanVehiclePullTrailer(*m_FollowVehicle))
	{
		ResetAttachedTrailerStatus();

		return;
	}

	const CTrailer* attachedTrailer = m_FollowVehicle->GetAttachedTrailer();
	if(attachedTrailer)
	{
		m_AttachedTrailer	= attachedTrailer;
		m_TrailerBlendLevel	= 1.0f;
	}
	else if(m_AttachedTrailer.Get())
	{
		//Compute a blend level based upon the distance between the follow vehicle and the detached trailer.

		const float distanceToConsider	= ComputeDistanceBetweenVehicleAndTrailerAttachPoints(*m_FollowVehicle, *m_AttachedTrailer);
		m_TrailerBlendLevel				= RampValueSafe(distanceToConsider, m_Metadata.m_DetachTrailerBlendStartDistance,
											m_Metadata.m_DetachTrailerBlendEndDistance, 1.0f, 0.0f);
		m_TrailerBlendLevel				= SlowInOut(m_TrailerBlendLevel);

		if(m_TrailerBlendLevel < SMALL_FLOAT)
		{
			ResetAttachedTrailerStatus();
		}
	}
	else
	{
		//Compute a blend level based upon the distance between the follow vehicle and the nearest trailer attach point.

		m_TrailerBlendLevel = 0.0f;

		const CVehicleIntelligence* vehicleIntelligence = m_FollowVehicle->GetIntelligence();
		if(vehicleIntelligence)
		{
			const CVehicle* nearestTrailer		= NULL;
			float distanceToNearestAttachPoint	= LARGE_FLOAT;

			const CEntityScannerIterator nearbyVehicles = vehicleIntelligence->GetVehicleScanner().GetIterator();

			for(const CEntity* nearbyEntity = nearbyVehicles.GetFirst(); nearbyEntity; nearbyEntity = nearbyVehicles.GetNext())
			{
				const CVehicle* nearbyVehicle = static_cast<const CVehicle*>(nearbyEntity);
				if(!nearbyVehicle->InheritsFromTrailer())
				{
					continue;
				}

				const float distanceToConsider = ComputeDistanceBetweenVehicleAndTrailerAttachPoints(*m_FollowVehicle, *nearbyVehicle);
				if(distanceToConsider < distanceToNearestAttachPoint)
				{
					nearestTrailer					= nearbyVehicle;
					distanceToNearestAttachPoint	= distanceToConsider;
				}
			}

			if(nearestTrailer)
			{
				m_TrailerBlendLevel	= RampValueSafe(distanceToNearestAttachPoint, m_Metadata.m_DetachTrailerBlendStartDistance,
										m_Metadata.m_DetachTrailerBlendEndDistance, 1.0f, 0.0f);
				m_TrailerBlendLevel	= SlowInOut(m_TrailerBlendLevel);

				if(m_TrailerBlendLevel >= SMALL_FLOAT)
				{
					m_AttachedTrailer = nearestTrailer;
				}
			}
		}
	}
}

bool camGameplayDirector::ComputeCanVehiclePullTrailer(const CVehicle& vehicle) const
{
	const s32 attachBoneIndex = vehicle.GetBoneIndex(VEH_ATTACH);
	if(attachBoneIndex == -1)
	{
		return false;
	}

	const s32 numGadgets = vehicle.GetNumberOfVehicleGadgets();
	for(s32 i=0; i<numGadgets; i++)
	{
		const CVehicleGadget* gadget = vehicle.GetVehicleGadget(i);

		if(gadget && (gadget->GetType() == VGT_TRAILER_ATTACH_POINT))
		{
			return true;
		}
	}

	return false;
}

float camGameplayDirector::ComputeDistanceBetweenVehicleAndTrailerAttachPoints(const CVehicle& vehicle, const CVehicle& trailer) const
{
	float distance = LARGE_FLOAT;

	s32 attachVehicleAttachBoneIndex	= vehicle.GetBoneIndex(VEH_ATTACH);
	s32 trailerAttachBoneIndex			= trailer.GetBoneIndex(TRAILER_ATTACH);
	if((attachVehicleAttachBoneIndex != -1) && (trailerAttachBoneIndex != -1))
	{
		Matrix34 attachVehicleAttachBoneMatrix;
		vehicle.GetGlobalMtx(attachVehicleAttachBoneIndex, attachVehicleAttachBoneMatrix);

		Matrix34 trailerAttachBoneMatrix;
		trailer.GetGlobalMtx(trailerAttachBoneIndex, trailerAttachBoneMatrix);

		distance = attachVehicleAttachBoneMatrix.d.Dist(trailerAttachBoneMatrix.d);
	}

	return distance;
}

void camGameplayDirector::ResetAttachedTrailerStatus()
{
	m_AttachedTrailer	= NULL;
	m_TrailerBlendLevel	= 0.0f;
}

void camGameplayDirector::UpdateTowedVehicleStatus()
{
	if(!m_FollowVehicle.Get() || !m_FollowVehicle->HasTowingGadget())
	{
		ResetTowedVehicleStatus();

		return;
	}

	const CEntity* towedEntity		= m_FollowVehicle->GetEntityBeingTowed();
	const bool isTowingVehicle		= (towedEntity && towedEntity->GetIsTypeVehicle());
	if(isTowingVehicle)
	{
		//Blend in to avoid a discontinuity.
		m_TowedVehicle				= static_cast<const CVehicle*>(towedEntity);
		m_TowedVehicleBlendLevel	= m_TowedVehicleBlendSpring.Update(1.0f, m_Metadata.m_TowedVehicleBlendInSpringConstant,
										m_Metadata.m_TowedVehicleBlendInSpringDampingRatio);
	}
	else if(m_TowedVehicle.Get())
	{
		//Compute a blend level based upon the distance between the bounding spheres of the follow vehicle and the detached towed vehicle.

		Vector3 followVehicleBoundCentre;
		const float followVehicleBoundRadius	= m_FollowVehicle->GetBoundCentreAndRadius(followVehicleBoundCentre);

		Vector3 towedVehicleBoundCentre;
		const float towedVehicleBoundRadius		= m_TowedVehicle->GetBoundCentreAndRadius(towedVehicleBoundCentre);

		const float distanceBetweenBoundCentres	= followVehicleBoundCentre.Dist(towedVehicleBoundCentre);
		const float distanceBetweenBoundSpheres	= distanceBetweenBoundCentres - followVehicleBoundRadius - towedVehicleBoundRadius;

		m_TowedVehicleBlendLevel	= RampValueSafe(distanceBetweenBoundSpheres, m_Metadata.m_DetachTowedVehicleBlendStartDistance,
										m_Metadata.m_DetachTowedVehicleBlendEndDistance, 1.0f, 0.0f);
		m_TowedVehicleBlendLevel	= SlowInOut(m_TowedVehicleBlendLevel);

		if(m_TowedVehicleBlendLevel >= SMALL_FLOAT)
		{
			m_TowedVehicleBlendSpring.Reset(m_TowedVehicleBlendLevel);
		}
		else
		{
			ResetTowedVehicleStatus();
		}
	}
	else
	{
		ResetTowedVehicleStatus();
	}
}

void camGameplayDirector::ResetTowedVehicleStatus()
{
	m_TowedVehicle				= NULL;	
	m_TowedVehicleBlendLevel	= 0.0f;

	m_TowedVehicleBlendSpring.Reset();
}

void camGameplayDirector::UpdateVehiclePovCameraConditions()
{
	m_ShouldDelayVehiclePovCameraOnPreviousUpdate	= m_ShouldDelayVehiclePovCamera;
	m_ShouldDelayVehiclePovCamera					= ComputeShouldDelayVehiclePovCamera();
}

bool camGameplayDirector::ComputeShouldDelayVehiclePovCamera() const
{
#if FPS_MODE_SUPPORTED
	const camFirstPersonShooterCamera* fpsCamera = GetFirstPersonShooterCamera();
	if( fpsCamera && ((!m_TurretAligned && fpsCamera->IsBlendingForVehicle()) || IsDelayTurretEntryForFpsCamera()) )
	{
		return true;
	}
#endif

	//NOTE: We only delay the vehicle POV camera for drivers, not passengers.
	if(!m_FollowPed.Get() || !m_FollowPed->GetIsDrivingVehicle())
	{
		return false;
	}

	////NOTE: We delay by one update on vehicle entry if the engine is off, in order to bridge the gap into the engine start and hot-wiring states.
	//if(m_FollowVehicle && !m_FollowVehicle->m_nVehicleFlags.bEngineOn && (m_VehicleEntryExitState == INSIDE_VEHICLE) && (m_VehicleEntryExitStateOnPreviousUpdate != INSIDE_VEHICLE))
	//{
	//	return true;
	//}

	const CPedIntelligence* followPedIntelligence = m_FollowPed->GetPedIntelligence();
	if(!followPedIntelligence)
	{
		return false;
	}

	const CTaskMotionInAutomobile* motionTask = static_cast<const CTaskMotionInAutomobile*>(followPedIntelligence->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_AUTOMOBILE));
	if(!motionTask)
	{
		return false;
	}

	//NOTE: We only delay for starting the engine and hot-wiring if the ped can actually be visible in the POV camera, which is defined by the animation data.
	const bool canPedBeVisibleInPov = motionTask->CanTransitionToFirstPersonAnims(false);
	if( !canPedBeVisibleInPov  FPS_MODE_SUPPORTED_ONLY(&& !GetFirstPersonShooterCamera()) )
	{
		return false;
	}

	const bool shouldDelayVehiclePovCamera = motionTask->CanDelayFirstPersonCamera();
	return shouldDelayVehiclePovCamera;
}

float camGameplayDirector::GetRelativeCameraHeading() const
{
	float relativeHeading = 0.0f;

    const camBaseCamera* pCameraToUse = m_ActiveCamera;

    // If we're rendering a cinematic mounted camera (e.g. in vehicle POV) we want to compute the relative heading based on that camera.
    const camBaseDirector* renderedDirector = camInterface::GetDominantRenderedDirector();
    const bool isCinematicDirector = renderedDirector && renderedDirector->GetIsClassId(camCinematicDirector::GetStaticClassId());
    if(isCinematicDirector)
    {
        camCinematicMountedCamera* firstPersonVehicleCamera = camInterface::GetCinematicDirector().GetFirstPersonVehicleCamera();
        if(firstPersonVehicleCamera)
        {
            pCameraToUse = firstPersonVehicleCamera;
        }
    }

	if(pCameraToUse)
	{
		const float cameraHeading = pCameraToUse->GetFrame().ComputeHeading();

		const CEntity* attachParent = pCameraToUse->GetAttachParent();
		if(attachParent)
		{
			const float attachParentHeading	= attachParent->GetTransform().GetHeading();
			relativeHeading					= cameraHeading - attachParentHeading;
			relativeHeading					= fwAngle::LimitRadianAngle(relativeHeading);
		}
	}

	return relativeHeading;
}

void camGameplayDirector::SetRelativeCameraHeading(float heading, float smoothRate)
{
	camThirdPersonCamera* thirdPersonCamera = GetThirdPersonCamera();
	if(thirdPersonCamera)
	{
		thirdPersonCamera->SetRelativeHeading(heading, smoothRate);
		if(heading == 0.0f)
		{
			thirdPersonCamera->SetRelativePitch(0.0f); //Also match attach parent pitch.
		}
	}
	else
	{
		camFirstPersonAimCamera* firstPersonAimCamera = GetFirstPersonAimCamera();
		if(firstPersonAimCamera)
		{
			firstPersonAimCamera->SetRelativeHeading(heading);
			if(heading == 0.0f)
			{
				firstPersonAimCamera->SetRelativePitch(0.0f); //Also match attach parent pitch.
			}
		}
	}
}

float camGameplayDirector::GetRelativeCameraPitch() const
{
	float relativePitch = 0.0f;

    const camBaseCamera* pCameraToUse = m_ActiveCamera;

    // If we're rendering a cinematic mounted camera (e.g. in vehicle POV) we want to compute the relative pitch based on that camera.
    const camBaseDirector* renderedDirector = camInterface::GetDominantRenderedDirector();
    const bool isCinematicDirector = renderedDirector && renderedDirector->GetIsClassId(camCinematicDirector::GetStaticClassId());
    if(isCinematicDirector)
    {
        camCinematicMountedCamera* firstPersonVehicleCamera = camInterface::GetCinematicDirector().GetFirstPersonVehicleCamera();
        if(firstPersonVehicleCamera)
        {
            pCameraToUse = firstPersonVehicleCamera;
        }
    }

	if(pCameraToUse)
	{
		const float cameraPitch = pCameraToUse->GetFrame().ComputePitch();

		const CEntity* attachParent = pCameraToUse->GetAttachParent();
		if(attachParent)
		{
			const float attachParentPitch	= attachParent->GetTransform().GetPitch();
			relativePitch					= cameraPitch - attachParentPitch;
			relativePitch					= fwAngle::LimitRadianAngleForPitch(relativePitch);
		}
	}

	return relativePitch;
}

void camGameplayDirector::SetRelativeCameraPitch(float pitch, float smoothRate)
{
	camThirdPersonCamera* thirdPersonCamera = GetThirdPersonCamera();
	if(thirdPersonCamera)
	{
		thirdPersonCamera->SetRelativePitch(pitch, smoothRate);
	}
	else
	{
		camFirstPersonAimCamera* firstPersonAimCamera = GetFirstPersonAimCamera();
		if(firstPersonAimCamera)
		{
			firstPersonAimCamera->SetRelativePitch(pitch);
		}
	}
}

void camGameplayDirector::Reset()
{
	BaseReset();

	//NOTE: The gameplay director renders by default.
	m_ActiveRenderState = RS_FULLY_RENDERING;

	DestroyAllCameras();

	m_FollowPed		= NULL;
	m_FollowVehicle	= NULL;
	m_ScriptOverriddenFollowVehicle = NULL; 

	m_HeliVehicleForTurret				= NULL;	
	m_HeliVehicleSeatIndexForTurret		= -1;
	m_HeliTurretWeaponComponentIndex	= -1;
	m_HeliTurretBaseComponentIndex		= -1;
	m_HeliTurretBarrelComponentIndex	= -1;

	m_FollowPedWeaponInfo					= NULL;
	m_FollowPedWeaponInfoOnPreviousUpdate	= NULL;

	m_ScriptEntityToIgnoreCollisionThisUpdate				= NULL; 
	m_ScriptEntityToLimitFocusOverBoundingSphereThisUpdate	= NULL;

	m_State = DOING_NOTHING;

	m_FollowPedCoverSettings.Reset();
	m_FollowPedCoverSettingsOnPreviousUpdate.Reset();
	m_FollowPedCoverSettingsForMostRecentCover.Reset();

	m_ScriptOverriddenFollowPedCameraSettingsThisUpdate.Reset();
	m_ScriptOverriddenFollowPedCameraSettingsOnPreviousUpdate.Reset();
	m_ScriptOverriddenFollowVehicleCameraSettingsThisUpdate.Reset();
	m_ScriptOverriddenFollowVehicleCameraSettingsOnPreviousUpdate.Reset();
    m_ScriptOverriddenTableGamesCameraSettingsThisUpdate.Reset();
    m_ScriptOverriddenTableGamesCameraSettingsOnPreviousUpdate.Reset();

	m_FollowPedCoverAimingHoldStartTime	= 0;
	m_FollowPedTimeSpentSwimming		= 0;

	m_CachedPreviousCameraTableGamesCameraHash = 0;

	m_VehicleEntryExitState						= OUTSIDE_VEHICLE;
	m_VehicleEntryExitStateOnPreviousUpdate		= OUTSIDE_VEHICLE;
	m_ScriptOverriddenVehicleEntryExitState		 = -1; 

	m_TightSpaceLevel = 0.0f;

	m_ScriptControlledMotionBlurScalingThisUpdate		= 1.0f;
	m_ScriptControlledMaxMotionBlurStrengthThisUpdate	= 1.0f;

	m_CachedScriptHeading		= 0.0f; 
	m_CachedScriptPitch			= 0.0f;
	m_CachedScriptPitchRate		= 1.0f; 
	m_CachedScriptFirstPersonShooterHeading = 0.0f;
	m_CachedScriptFirstPersonShooterPitch = 0.0f;
	m_CachedFirstPersonFallbackHeading		= 0.0f; 
	m_CachedFirstPersonFallbackPitch		= 0.0f;
	m_CachedFirstPersonFallbackPitchRate	= 1.0f; 
	m_UseScriptRelativeHeading	= false;
	m_UseScriptRelativePitch	= false;
	m_UseScriptFirstPersonShooterHeading = false;
	m_UseScriptFirstPersonShooterPitch = false;
	m_UseFirstPersonFallbackRelativeHeading = false;
	m_UseFirstPersonFallbackRelativePitch = false;
	
	m_ScriptOverriddenHintFovScalar = 0.0f; 
	m_ScriptOverriddenHintDistanceScalar = 0.0f; 
	m_ScriptOverriddenHintBaseOrbitPitchOffset = 0.0f; 
	m_ScriptOverriddenHintCameraRelativeSideOffsetAdditive = 0.0f; 
	m_ScriptOverriddenHintCameraRelativeVerticalOffsetAdditive = 0.0f; 
	m_ScriptOverriddenFirstPersonAimCameraNearClipThisUpdate = 0.0f; 

	m_IsScriptOverridingHintFovScalar = false; 
	m_IsScriptOverridingHintDistanceScalar = false;  
	m_IsScriptOverridingHintBaseOrbitPitchOffset = false;  
	m_IsScriptOverridingHintCameraRelativeSideOffsetAdditive = false;  
	m_IsScriptOverridingHintCameraRelativeVerticalOffsetAdditive = false; 
	m_IsScriptForcingHintCameraBlendToFollowPedMediumViewMode = false;

	m_ScriptOverriddenFirstPersonAimCameraRelativeHeadingLimitsThisUpdate.Zero();
	m_ScriptOverriddenFirstPersonAimCameraRelativePitchLimitsThisUpdate.Zero();
	m_ShouldScriptOverrideFirstPersonAimCameraRelativeHeadingLimitsThisUpdate	= false;
	m_ShouldScriptOverrideFirstPersonAimCameraRelativePitchLimitsThisUpdate		= false;
	m_ShouldScriptOverrideFirstPersonAimCameraNearClipThisUpdate				= false; 
	m_ShouldScriptOverrideThirdPersonCameraRelativeHeadingLimitsThisUpdate		= false;
	m_ShouldScriptOverrideThirdPersonCameraRelativePitchLimitsThisUpdate		= false;
	m_ShouldScriptOverrideThirdPersonCameraOrbitDistanceLimitsThisUpdate		= false;
	m_ShouldScriptOverrideThirdPersonAimCameraNearClipThisUpdate				= false; 

	m_ScriptPreventCollisionSettingsForTripleHeadInInteriorsThisUpdate = false;

	m_ShouldScriptOverrideFollowVehicleCameraHighAngleModeThisUpdate = false;
	m_ShouldScriptOverrideFollowVehicleCameraHighAngleModeEveryUpdate = false; 

	m_ShouldAllowCustomVehicleDriveByCamerasThisUpdate = false;

	m_ScriptForcedVehicleCamStuntSettingsThisUpdate = false;

	m_ShouldFollowCameraIgnoreAttachParentMovementThisUpdateForScript = false;
	m_ShouldInitCatchUpFromFrame = false;

	m_UseVehicleCamStuntSettings = false;
	m_PreviousUseVehicleCamStuntSettings = false;
	m_ScriptRequestedVehicleCamStuntSettingsThisUpdate = false;
	m_ScriptForcedVehicleCamStuntSettingsThisUpdate = false;
	m_ScriptRequestDedicatedStuntCameraThisUpdate = false;
	m_VehicleCamStuntSettingsTestTimer = 0.0f;
	m_LastTimeAttachParentWasInWater = 0;

#if FPS_MODE_SUPPORTED
    m_FirstPersonAnimatedDataStartTime = 0;
#endif

#if __BANK
	m_DebugOverrideMotionBlurStrengthThisUpdate	= -1.0f;
	m_DebugOverrideNearDofThisUpdate			= 0.0f;
	m_DebugOverrideFarDofThisUpdate				= 100.0f;
	m_DebugOverridingDofThisUpdate				= false;

	m_DebugOverriddenRelativeOrbitHeadingDegrees	= 0.0f;
	m_DebugOverriddenRelativeOrbitPitchDegrees		= 0.0f;

	m_DebugHintOffset.Zero();
	m_DebugHintAttackDuration	= 2000;
	m_DebugHintReleaseDuration	= 2000;
	m_DebugHintSustainDuration	= -1;
	m_DebugIsHintOffsetRelative	= true;

	m_DebugCatchUpDistance = 0.0f;

	m_DebugOverriddenFollowPedCameraName[0]						= '\0';
	m_DebugOverriddenFollowVehicleCameraName[0]					= '\0';
	m_DebugOverriddenFollowPedCameraInterpolationDuration		= -1;
	m_DebugOverriddenFollowVehicleCameraInterpolationDuration	= -1;
	m_DebugShouldOverrideFollowPedCamera						= false;
	m_DebugShouldOverrideFollowVehicleCamera					= false;

	m_DebugShouldReportOutsideVehicle = false;

	m_DebugShouldLogTightSpaceSettings		= false;
	m_DebugShouldLogActiveViewModeContext	= false;
	m_DebugForceUsingStuntSettings = false;
	m_DebugForceUsingTightSpaceCustomFraming = false;
	m_DebugHintActive = false;
#endif // __BANK

	m_IsHighAltitudeFovScalingPersistentlyEnabled	= true;
	m_IsHighAltitudeFovScalingDisabledThisUpdate	= false;

	m_ShouldDisplayReticule	= false;
	m_ShouldDimReticule		= false;
	
	m_WasRenderingMobilePhoneCameraOnPreviousUpdate = false;

	m_ShouldDelayVehiclePovCamera					= false;
	m_ShouldDelayVehiclePovCameraOnPreviousUpdate	= false;

	StopSwitchBehaviour();

	m_HintPos = VEC3_ZERO; 
	m_HintEnvelopeLevel = 0.0f;
	m_IsRelativeHint = true; 
	m_HintOffset = VEC3_ZERO;
	
	m_IsScriptBlockingCodeHints = false; 
	m_HasUserSelectedCodeHint = false; 
	m_IsCodeHintActive = false; 
	m_CanActivateCodeHint = false; 
	m_CodeHintTimer = 0; 
	m_HintLookAtMat = Matrix34::IdentityType; 
	m_RagdollBlendLevel = 0.0f; 
	m_IsHintFirstUpdate = false; 
	//hint control
	m_IsCodeHintButtonDown = false; 
	m_LatchedState = false; 
	m_TimeHintButtonInitalPressed = 0; 
	
	m_PreventVehicleEntryExit = false;

	ResetAttachedTrailerStatus();
	ResetTowedVehicleStatus();

	StopExplosionShakes();

	if(m_DeathFailEffectFrameShaker)
	{
		delete m_DeathFailEffectFrameShaker;
	}
}

void camGameplayDirector::DestroyAllCameras()
{
	if(m_ActiveCamera)
	{
		delete m_ActiveCamera;
	}

	//NOTE: The previous camera must be destroyed last, in case an active interpolation was referencing it.
	if(m_PreviousCamera)
	{
		delete m_PreviousCamera;
	}
}

//Clear up all child camera and interpolations and fall-back to a follow-ped camera.
void camGameplayDirector::ResurrectPlayer()
{
	Reset();
}

camThirdPersonCamera* camGameplayDirector::GetThirdPersonCamera(const CEntity* attachEntity)
{
	camThirdPersonCamera* thirdPersonCamera	= NULL;

	if(m_ActiveCamera && m_ActiveCamera->GetIsClassId(camThirdPersonCamera::GetStaticClassId()))
	{
		thirdPersonCamera = static_cast<camThirdPersonCamera*>(m_ActiveCamera.Get());
		if(attachEntity && (thirdPersonCamera->GetAttachParent() != attachEntity))
		{
			thirdPersonCamera = NULL; //This camera is not attached to the specified entity.
		}
	}

	return thirdPersonCamera;
}

camFollowCamera* camGameplayDirector::GetFollowCamera(const CEntity* attachEntity)
{
	camFollowCamera* followCamera = NULL;

	camThirdPersonCamera* thirdPersonCamera = GetThirdPersonCamera(attachEntity);
	if(thirdPersonCamera && thirdPersonCamera->GetIsClassId(camFollowCamera::GetStaticClassId()))
	{
		followCamera = static_cast<camFollowCamera*>(thirdPersonCamera);
	}

	return followCamera;
}

camFollowPedCamera* camGameplayDirector::GetFollowPedCamera(const CEntity* attachEntity)
{
	camFollowPedCamera* followPedCamera = NULL;

	camFollowCamera* followCamera = GetFollowCamera(attachEntity);
	if(followCamera && followCamera->GetIsClassId(camFollowPedCamera::GetStaticClassId()))
	{
		followPedCamera = static_cast<camFollowPedCamera*>(followCamera);
	}

	return followPedCamera;
}

camFollowVehicleCamera* camGameplayDirector::GetFollowVehicleCamera(const CEntity* attachEntity)
{
	camFollowVehicleCamera* followVehicleCamera = NULL;

	camFollowCamera* followCamera = GetFollowCamera(attachEntity);
	if(followCamera && followCamera->GetIsClassId(camFollowVehicleCamera::GetStaticClassId()))
	{
		followVehicleCamera = static_cast<camFollowVehicleCamera*>(followCamera);
	}

	return followVehicleCamera;
}

camFirstPersonAimCamera* camGameplayDirector::GetFirstPersonAimCamera(const CEntity* attachEntity)
{
	camFirstPersonAimCamera* firstPersonAimCamera = NULL;

	if(m_ActiveCamera && m_ActiveCamera->GetIsClassId(camFirstPersonAimCamera::GetStaticClassId()))
	{
		firstPersonAimCamera = static_cast<camFirstPersonAimCamera*>(m_ActiveCamera.Get());
		if(attachEntity && (firstPersonAimCamera->GetAttachParent() != attachEntity))
		{
			firstPersonAimCamera = NULL; //This camera is not attached to the specified entity.
		}
	}

	return firstPersonAimCamera;
}

camFirstPersonPedAimCamera* camGameplayDirector::GetFirstPersonPedAimCamera(const CEntity* attachEntity)
{
	camFirstPersonPedAimCamera* firstPersonAimCamera = NULL;

	if(m_ActiveCamera && m_ActiveCamera->GetIsClassId(camFirstPersonPedAimCamera::GetStaticClassId()))
	{
		firstPersonAimCamera = static_cast<camFirstPersonPedAimCamera*>(m_ActiveCamera.Get());
		if(attachEntity && (firstPersonAimCamera->GetAttachParent() != attachEntity))
		{
			firstPersonAimCamera = NULL; //This camera is not attached to the specified entity.
		}
	}

	return firstPersonAimCamera;
}

#if FPS_MODE_SUPPORTED
bool camGameplayDirector::IsFirstPersonShooterAiming(const CEntity* attachEntity) const
{
	const camFirstPersonShooterCamera* firstPersonShooterCamera = GetFirstPersonShooterCamera(attachEntity);
	return firstPersonShooterCamera != NULL && firstPersonShooterCamera->IsAiming();
}

camFirstPersonShooterCamera* camGameplayDirector::GetFirstPersonShooterCamera(const CEntity* attachEntity)
{
	camFirstPersonShooterCamera* firstPersonShooterCamera = NULL;

	if(m_ActiveCamera && m_ActiveCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()))
	{
		firstPersonShooterCamera = static_cast<camFirstPersonShooterCamera*>(m_ActiveCamera.Get());
		if(attachEntity && (firstPersonShooterCamera->GetAttachParent() != attachEntity))
		{
			firstPersonShooterCamera = NULL; //This camera is not attached to the specified entity.
		}
	}

	return firstPersonShooterCamera;
}
#endif // FPS_MODE_SUPPORTED

camCinematicMountedCamera*	camGameplayDirector::GetFirstPersonVehicleCamera(const CEntity* UNUSED_PARAM(attachEntity))
{
	return camInterface::GetCinematicDirector().GetFirstPersonVehicleCamera();
}

camFirstPersonHeadTrackingAimCamera* camGameplayDirector::GetFirstPersonHeadTrackingAimCamera(const CEntity* attachEntity)
{
	camFirstPersonHeadTrackingAimCamera* firstPersonAimCamera = NULL;

	if(m_ActiveCamera && m_ActiveCamera->GetIsClassId(camFirstPersonHeadTrackingAimCamera::GetStaticClassId()))
	{
		firstPersonAimCamera = static_cast<camFirstPersonHeadTrackingAimCamera*>(m_ActiveCamera.Get());
		if(attachEntity && (firstPersonAimCamera->GetAttachParent() != attachEntity))
		{
			firstPersonAimCamera = NULL; //This camera is not attached to the specified entity.
		}
	}

	return firstPersonAimCamera;
}

camThirdPersonAimCamera* camGameplayDirector::GetThirdPersonAimCamera(const CEntity* attachEntity)
{
	camThirdPersonAimCamera* thirdPersonAimCamera = NULL;

	camThirdPersonCamera* thirdPersonCamera = GetThirdPersonCamera(attachEntity);
	if(thirdPersonCamera && thirdPersonCamera->GetIsClassId(camThirdPersonAimCamera::GetStaticClassId()))
	{
		thirdPersonAimCamera = static_cast<camThirdPersonAimCamera*>(thirdPersonCamera);
	}

	return thirdPersonAimCamera;
}

bool camGameplayDirector::IsUsingVehicleTurret(bool bCheckForValidThirdPersonAimCamera) const
{
	const CPed* followPed = GetFollowPed();
	if (followPed && followPed->GetIsInVehicle() && IsTurretSeat(followPed->GetVehiclePedInside(), followPed->GetAttachCarSeatIndex()))
	{
		const CPedIntelligence* pedIntelligence	= followPed->GetPedIntelligence();
		if(pedIntelligence)
		{
			if(pedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE) != nullptr)
			{
				return false;
			}
		}

		if (bCheckForValidThirdPersonAimCamera)
		{
			camThirdPersonAimCamera* thirdPersonAimCamera = camInterface::GetGameplayDirector().GetThirdPersonAimCamera();
			if (thirdPersonAimCamera)
			{
				return true;
			}
		}
		else
		{ 
			return true;
		}
	}
	return false;
}

bool camGameplayDirector::IsLookingBehind() const
{
	const camFollowCamera* followCamera		= GetFollowCamera();
	const camControlHelper* controlHelper	= followCamera ? followCamera->GetControlHelper() : NULL;
	const bool isLookingBehind				= controlHelper && controlHelper->IsLookingBehind();

	return isLookingBehind; 
}

void camGameplayDirector::SetScriptOverriddenVehicleExitEntryState(CVehicle* pVehicle, s32 ExitEntryState)
{
	if(ExitEntryState != camGameplayDirector::OUTSIDE_VEHICLE)
	{
		if(pVehicle)
		{
			m_ScriptOverriddenVehicleEntryExitState = ExitEntryState; 
			m_ScriptOverriddenFollowVehicle = pVehicle; 
		}
		else
		{
			m_ScriptOverriddenVehicleEntryExitState = -1;
		}
	}
	else
	{
		m_ScriptOverriddenVehicleEntryExitState = -1;
	}

}
CControl* camGameplayDirector::GetActiveControl() const
{
	CControl* activeControl;

	const CPed* followPed = camInterface::FindFollowPed();
	if(followPed && followPed->ShouldBeDead())
	{
		activeControl = NULL;
	}
	else
	{
		activeControl = &CControlMgr::GetMainCameraControl(true);
	}

	return activeControl;
}

s32 camGameplayDirector::GetActiveViewModeContext() const
{
	const camControlHelper* activeControlHelper = NULL;
	if(m_ActiveCamera)
	{
		if(m_ActiveCamera->GetIsClassId(camThirdPersonCamera::GetStaticClassId()))
		{
			activeControlHelper = static_cast<const camThirdPersonCamera*>(m_ActiveCamera.Get())->GetControlHelper();
		}
		else if(m_ActiveCamera->GetIsClassId(camFirstPersonAimCamera::GetStaticClassId()))
		{
			activeControlHelper = static_cast<const camFirstPersonAimCamera*>(m_ActiveCamera.Get())->GetControlHelper();
		}
	}

	//NOTE: We fall back to reporting the on-foot context in the absence of a valid context.
	s32 viewModeContext = camControlHelperMetadataViewMode::ON_FOOT;
	if(activeControlHelper)
	{
		 const s32 tempViewModeContext = activeControlHelper->GetViewModeContext();
		 if((tempViewModeContext >= camControlHelperMetadataViewMode::eViewModeContext_MIN_VALUE) &&
			 (tempViewModeContext <= camControlHelperMetadataViewMode::eViewModeContext_MAX_VALUE))
		 {
			 viewModeContext = tempViewModeContext;
		 }
	}

	return viewModeContext;
}

s32 camGameplayDirector::GetActiveViewMode(s32 context) const
{
	const camControlHelper* activeControlHelper = NULL;
	if(m_ActiveCamera)
	{
		if(m_ActiveCamera->GetIsClassId(camThirdPersonCamera::GetStaticClassId()))
		{
			activeControlHelper = static_cast<const camThirdPersonCamera*>(m_ActiveCamera.Get())->GetControlHelper();
		}
		else if(m_ActiveCamera->GetIsClassId(camFirstPersonAimCamera::GetStaticClassId()))
		{
			activeControlHelper = static_cast<const camFirstPersonAimCamera*>(m_ActiveCamera.Get())->GetControlHelper();
		}
	}

	//NOTE: We fall back to reporting the on-foot context in the absence of a valid context.
	s32 viewMode = -1;
	if(activeControlHelper)
	{
		if( (context >= camControlHelperMetadataViewMode::eViewModeContext_MIN_VALUE) &&
			(context <= camControlHelperMetadataViewMode::eViewModeContext_MAX_VALUE) )
		{
		 	const s32 tempViewMode = activeControlHelper->GetViewModeForContext( context );
		 	if( (tempViewMode >= camControlHelperMetadataViewMode::eViewMode_MIN_VALUE) &&
				(tempViewMode <= camControlHelperMetadataViewMode::eViewMode_MAX_VALUE) )
		 	{
				viewMode = tempViewMode;
			}
		}
	}

	return viewMode;
}

void camGameplayDirector::RegisterWeaponFireRemote(const CWeapon& weapon, const CEntity& firingEntity)
{
	const CWeaponInfo* weaponInfo = weapon.GetWeaponInfo();
	if(weaponInfo && weaponInfo->GetIsTurret() && firingEntity.GetIsTypePed())
	{		
		const CPed* localPed = CPedFactory::GetFactory()->GetLocalPlayer();
		const CPed* firingPed = static_cast<const CPed*>(&firingEntity);
		const CVehicle* firingPedVehicle = static_cast<CVehicle*>(firingPed->GetVehiclePedInside());

		// Don't shake if we're the ped firing the turret (as this would cause a duplicate shake), check that we're in the same vehicle
		if(firingPed != localPed && localPed->GetVehiclePedInside() == firingPedVehicle)
		{
			const u32 recoilShakeHash = weaponInfo->GetRecoilShakeHash();			

			camBaseCamera* renderedCamera = (camBaseCamera*)GetRenderedCamera();
			if(recoilShakeHash && renderedCamera)
			{
				float distanceSquared = (renderedCamera->GetFrame().GetWorldMatrix().d - VEC3V_TO_VECTOR3(firingPed->GetMatrix().d())).Mag2();
				distanceSquared = distanceSquared > 0 ? distanceSquared : SMALL_FLOAT;
				
				const float intensity = g_TurretShakeWeaponPower / (PI * distanceSquared * 4); 				
				const float shakeAmplitude = Clamp(g_TurretShakeThirdPersonScaling * intensity, 0.0f, g_TurretShakeMaxAmplitude);

				renderedCamera->Shake(recoilShakeHash, shakeAmplitude);
			}
		}
	}
}

void camGameplayDirector::RegisterWeaponFireLocal(const CWeapon& weapon, const CEntity& firingEntity)
{
	const CWeaponInfo* weaponInfo = weapon.GetWeaponInfo();	
	
	//NOTE: The tank is always aiming in third-person, whether or not we are rendering an aim camera.
	const bool isAimingInTank	= (firingEntity.GetIsTypeVehicle() && static_cast<const CVehicle&>(firingEntity).IsTank() && GetThirdPersonCamera(&firingEntity));
	const bool isAimingInTurret = (firingEntity.GetIsTypePed() && &firingEntity == m_FollowPed && weaponInfo && weaponInfo->GetIsTurret());
	const bool isAiming			= isAimingInTank || isAimingInTurret || IsAiming(&firingEntity)  FPS_MODE_SUPPORTED_ONLY( || GetFirstPersonShooterCamera(&firingEntity));
	if(isAiming)
	{
		if(weaponInfo && ((weaponInfo == m_FollowPedWeaponInfo) || isAimingInTank || isAimingInTurret))
		{
			//Kick-off a new recoil shake if specified in the weapon metadata.
			u32 recoilShakeHash							= weaponInfo->GetRecoilShakeHash();
		#if FPS_MODE_SUPPORTED
			if(m_ActiveCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()))
			{
				recoilShakeHash							= weapon.GetFirstPersonRecoilShakeHash();
			}
		#endif

			const camShakeMetadata* recoilShakeMetadata	= camFactory::FindObjectMetadata<camShakeMetadata>(recoilShakeHash);
			if(recoilShakeMetadata)
			{
				//Apply any weapon-specific amplitude scaling to the shakes.
				float amplitude = weapon.GetRecoilShakeAmplitude();

				if(m_ActiveCamera->GetIsClassId(camThirdPersonAimCamera::GetStaticClassId()))
				{
					const float amplitudeScaling = static_cast<camThirdPersonAimCamera*>(m_ActiveCamera.Get())->GetRecoilShakeAmplitudeScaling();

					amplitude *= amplitudeScaling;
				}
			#if FPS_MODE_SUPPORTED
				else if(m_ActiveCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()))
				{
					if(firingEntity.GetIsTypePed())
					{
						const CPed& parentPed = (CPed&)firingEntity;
						if(parentPed.GetMotionData() && parentPed.GetMotionData()->GetIsFPSScope())
						{
							const float amplitudeScaling = static_cast<camFirstPersonShooterCamera*>(m_ActiveCamera.Get())->GetRecoilShakeAmplitudeScaling();
							amplitude *= amplitudeScaling;
						}
					}
				}
			#endif

				//Scale the amplitude of the shake based upon the current accuracy, which is based on stats and will degrade over time with
				//sustained fire.
				if(firingEntity.GetIsTypePed())
				{
					const CPed& parentPed = static_cast<const CPed&>(firingEntity);
					if(parentPed.IsLocalPlayer() && m_ActiveCamera->GetIsClassId(camFirstPersonAimCamera::GetStaticClassId()))
					{
						const float shootingAbility		= StatsInterface::GetFloatStat(StatsInterface::GetStatsModelHashId("SHOOTING_ABILITY"));
						const float amplitudeScaling	= RampValueSafe(shootingAbility, m_Metadata.m_RecoilShakeFirstPersonShootingAbilityLimits.x,
															m_Metadata.m_RecoilShakeFirstPersonShootingAbilityLimits.y,
															m_Metadata.m_RecoilShakeAmplitudeScalingAtFirstPersonShootingAbilityLimits.x,
															m_Metadata.m_RecoilShakeAmplitudeScalingAtFirstPersonShootingAbilityLimits.y);

						PF_SET(camGameplayDirectorFirstPersonShootingAbilityRecoilScaling, amplitudeScaling);
						amplitude *= amplitudeScaling;
					}
					else
					{
						sWeaponAccuracy weaponAccuracy;
						weapon.GetAccuracy(&parentPed, -1.0f, weaponAccuracy);

						const float amplitudeScaling	= RampValueSafe(weaponAccuracy.fAccuracyMin, m_Metadata.m_RecoilShakeAccuracyLimits.x,
															m_Metadata.m_RecoilShakeAccuracyLimits.y,
															m_Metadata.m_RecoilShakeAmplitudeScalingAtAccuracyLimits.x,
															m_Metadata.m_RecoilShakeAmplitudeScalingAtAccuracyLimits.y);

						PF_SET(camGameplayDirectorAccuracyRecoilScaling, amplitudeScaling);
						amplitude *= amplitudeScaling;
					}
				}

				const u32 minTimeBetweenRecoilShakes = weaponInfo->GetMinTimeBetweenRecoilShakes();
				m_ActiveCamera->Shake(*recoilShakeMetadata, amplitude, m_Metadata.m_MaxRecoilShakeInstances, minTimeBetweenRecoilShakes);
			}

			//Kick-off or auto-start the accuracy offset shake if specified in the weapon metadata.
			const u32 accuracyOffsetShakeHash					= weaponInfo->GetAccuracyOffsetShakeHash();
			const camShakeMetadata* accuracyOffsetShakeMetadata	= camFactory::FindObjectMetadata<camShakeMetadata>(accuracyOffsetShakeHash);
			if(accuracyOffsetShakeMetadata)
			{
				camBaseFrameShaker* accuracyOffsetShake = m_ActiveCamera->FindFrameShaker(accuracyOffsetShakeHash);
				if(accuracyOffsetShake)
				{
					//Auto-start the shake, ensuring that we receive the full hold duration from this point on.
					accuracyOffsetShake->AutoStart();
				}
				else
				{
					//Start with zero amplitude, as this will be updated to an appropriate level in UpdateAimAccuracyOffsetShake().
					m_ActiveCamera->Shake(*accuracyOffsetShakeMetadata, 0.0f);
				}
			}
		}
	}
}

void camGameplayDirector::RegisterPedKill(const CPed& inflictor, const CPed& UNUSED_PARAM(victim), u32 weaponHash, bool UNUSED_PARAM(isHeadShot))
{
	if(!m_Metadata.m_KillEffectShakeRef)
	{
		return;
	}

	const bool isAiming = IsAiming(&inflictor);
	if(!isAiming)
	{
		return;
	}

	//NOTE: We only render the camera effect if the ped was killed with a lethal weapon that fires like a gun.
	const CWeaponInfo* weaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(weaponHash);
	if(weaponInfo && weaponInfo->GetIsGunOrCanBeFiredLikeGun() && !weaponInfo->GetIsNonLethal())
	{
		m_ActiveCamera->Shake(m_Metadata.m_KillEffectShakeRef, 1.0f, m_Metadata.m_MaxKillEffectShakeInstances);
	}
}

void camGameplayDirector::RegisterVehicleDamage(const CVehicle* pVehicle, float damage)
{
	if (!m_Metadata.m_VehicleImpactShakeRef)
	{
		return;
	}

	if (camInterface::IsFadedOut() || camInterface::GetDominantRenderedDirector() != this)
	{
		return;
	}
	
	//compute the velocity delta for impacts. 
	float VelocityDelta = damage * InvertSafe(pVehicle->pHandling->m_fMass, pVehicle->GetInvMass());		

	if (pVehicle &&
		(!pVehicle->InheritsFromPlane() || !const_cast<CVehicle*>(pVehicle)->IsInAir()) &&
		m_ActiveCamera->GetIsClassId(camFollowVehicleCamera::GetStaticClassId()) &&
		GetFollowVehicle() == pVehicle &&
		VelocityDelta > m_Metadata.m_VehicleImpactShakeMinDamage)
	{
		//Apply amplitude scaling based velocity change of the impact
		float amplitude = Min(VelocityDelta * (m_Metadata.m_VehicleImpactShakeMaxAmplitude/m_Metadata.m_VehicleImpactShakeMaxDamage),
								m_Metadata.m_VehicleImpactShakeMaxAmplitude);
	#if __DEV && 0
		Errorf("Vehicle shake amplitude is %f for damage %f", amplitude, VelocityDelta);
	#endif // __DEV
	
		m_ActiveCamera->Shake(m_Metadata.m_VehicleImpactShakeRef, amplitude, 2);
	}
}

void camGameplayDirector::RegisterVehiclePartBroken(const CVehicle& vehicle, eHierarchyId vehiclePartId, float amplitude /* = 1.0f */)
{
	if (
		camInterface::IsFadedOut() || 
		camInterface::GetDominantRenderedDirector() != this || 
		vehiclePartId == VEH_INVALID_ID || 
		!m_ActiveCamera ||
		!m_ActiveCamera->GetIsClassId(camThirdPersonCamera::GetStaticClassId()))
	{
		return;
	}

	const CVehicleModelInfo* modelInfo = vehicle.GetVehicleModelInfo(); 
	if(modelInfo)
	{
		const s32 boneIdx = modelInfo->GetBoneIndex(vehiclePartId);

		const crSkeleton* skeleton = vehicle.GetSkeleton();
		if(boneIdx >= 0 && skeleton)
		{
			Mat34V vehiclePartMatrix;
			skeleton->GetGlobalMtx(boneIdx, vehiclePartMatrix);

			const Matrix34 cameraMatrix = m_ActiveCamera->GetFrame().GetWorldMatrix();

			Vector3 vehiclePartPositionRelativeToCamera = VEC3V_TO_VECTOR3(vehiclePartMatrix.d());
			cameraMatrix.UnTransform(vehiclePartPositionRelativeToCamera);

			//is the target vehicle part in front of the camera?
			if(vehiclePartPositionRelativeToCamera.y > 0.0f)
			{
				const atHashString vehicleBrokenPartShakeCentralName("VEH_BROKEN_PART_SHAKE_CENTRAL", 0x266431E2);
				const atHashString vehicleBrokenPartShakeLeftName("VEH_BROKEN_PART_SHAKE_LEFT", 0x415138C7);
				const atHashString vehicleBrokenPartShakeRightName("VEH_BROKEN_PART_SHAKE_RIGHT", 0xF173E7DB);
				const float angleThreshold = 10.0f * DtoR;

				Vector3 vehiclePartToCamera = vehiclePartPositionRelativeToCamera;
				vehiclePartToCamera.Normalize();

				const float dotVehiclePartCameraForward = vehiclePartToCamera.Dot(Vector3(0.0f, 1.0f, 0.0f));
				const float angleVehiclePartCameraForward = Acosf(Clamp(dotVehiclePartCameraForward, 0.0f, 1.0f));

				if(angleVehiclePartCameraForward >= -angleThreshold && angleVehiclePartCameraForward < angleThreshold)
				{
					m_ActiveCamera->Shake(vehicleBrokenPartShakeCentralName, amplitude);
				}
				else
				{
					const bool isPartOnTheRightSideOfTheCamera = vehiclePartPositionRelativeToCamera.x > 0.0f;
					if(isPartOnTheRightSideOfTheCamera)
					{
						//if is on the right we shake to the left
						m_ActiveCamera->Shake(vehicleBrokenPartShakeLeftName, amplitude);
					}
					else
					{
						m_ActiveCamera->Shake(vehicleBrokenPartShakeRightName, amplitude);
					}
				}
			}
		}
	}
}

void camGameplayDirector::RegisterExplosion(u32 shakeNameHash, float explosionStrength, float rollOffScaling, const Vector3& positionOfExplosion)
{
	const float baseRumbleAmplitude		= ComputeBaseAmplitudeForExplosion(explosionStrength, rollOffScaling, positionOfExplosion);
	const bool isNetworkGameInProgress	= CNetwork::IsGameInProgress();
	const float rumbleAmplitudeScaling	= isNetworkGameInProgress ? m_Metadata.m_ExplosionShakeSettings.m_RumbleAmplitudeScalingForMp :
											m_Metadata.m_ExplosionShakeSettings.m_RumbleAmplitudeScaling;
	const float rumbleAmplitude			= Min(baseRumbleAmplitude * rumbleAmplitudeScaling, m_Metadata.m_ExplosionShakeSettings.m_MaxRumbleAmplitude);
	if(rumbleAmplitude >= SMALL_FLOAT)
	{
		const u32 rumbleDuration		= isNetworkGameInProgress ? m_Metadata.m_ExplosionShakeSettings.m_RumbleDurationForMp :
											m_Metadata.m_ExplosionShakeSettings.m_RumbleDuration;
		CControlMgr::StartPlayerPadShakeByIntensity(rumbleDuration, rumbleAmplitude);
	}

	const float newAmplitude = ComputeShakeAmplitudeForExplosion(explosionStrength, rollOffScaling, positionOfExplosion);
	if(newAmplitude < SMALL_FLOAT)
	{
		return;
	}

	if(shakeNameHash == 0)
	{
		return;
	}

	const camBaseShakeMetadata* shakeMetadata = camFactory::FindObjectMetadata<camBaseShakeMetadata>(shakeNameHash);
	if(!shakeMetadata)
	{
		return;
	}

	const s32 numExplosions = m_ExplosionSettings.GetCount();
	if(numExplosions == static_cast<s32>(m_Metadata.m_ExplosionShakeSettings.m_MaxInstances))
	{
		//We have exceeded the maximum number of explosion shake instances, so try to find an instance with a lower amplitude, so we can replace it.
		float minAmplitude				= newAmplitude;
		s32 minAmpltudeExplosionIndex	= -1;
		for(s32 explosionIndex=0; explosionIndex<numExplosions; explosionIndex++)
		{
			tExplosionSettings& explosionSettings	= m_ExplosionSettings[explosionIndex];
			camBaseFrameShaker* frameShaker			= explosionSettings.m_FrameShaker;
			if(!frameShaker)
			{
				//The frame shaker is no longer active, so we can reclaim this slot.
				minAmplitude				= -1.0f;
				minAmpltudeExplosionIndex	= explosionIndex;
				break;
			}

			float amplitude				= frameShaker->GetAmplitude();
			//Factor in the release level, so that we may replace shakes that have released to a lower effective amplitude than our prospective new shake.
			const float releaseLevel	= frameShaker->GetReleaseLevel();
			amplitude					*= releaseLevel;

			if(amplitude >= minAmplitude)
			{
				continue;
			}

			minAmplitude				= amplitude;
			minAmpltudeExplosionIndex	= explosionIndex;
		}

		if(minAmpltudeExplosionIndex < 0)
		{
			return;
		}

		camBaseFrameShaker* frameShaker = m_ExplosionSettings[minAmpltudeExplosionIndex].m_FrameShaker;
		if(frameShaker)
		{
			delete frameShaker;

			m_ExplosionSettings[minAmpltudeExplosionIndex].m_FrameShaker = NULL;
		}

		m_ExplosionSettings.Delete(minAmpltudeExplosionIndex);
	}

	camBaseFrameShaker* frameShaker = camFactory::CreateObject<camBaseFrameShaker>(*shakeMetadata);
	if(!frameShaker)
	{
		return;
	}

	frameShaker->SetAmplitude(newAmplitude);

	tExplosionSettings& newExplosionSettings	= m_ExplosionSettings.Grow();
	newExplosionSettings.m_FrameShaker			= frameShaker;
	newExplosionSettings.m_Strength				= explosionStrength;
	newExplosionSettings.m_RollOffScaling		= rollOffScaling;
	newExplosionSettings.m_Position.Set(positionOfExplosion);
}

void camGameplayDirector::RegisterEnduranceDamage(float damageAmount)
{
	if (
		camInterface::IsFadedOut() || 
		camInterface::GetDominantRenderedDirector() != this ||
		!m_ActiveCamera ||
		!m_ActiveCamera->GetIsClassId(camThirdPersonCamera::GetStaticClassId()))
	{
		return;
	}
	
	TUNE_GROUP_BOOL(CAMERA_GAMEPLAY_DIRECTOR, bAlwaysAllowEnduranceShake, false);
	if(!NetworkInterface::IsInCopsAndCrooks() && !bAlwaysAllowEnduranceShake)
	{
		//only allow this for Cops&Crooks for now.
		return;
	}

	const float shakeAmplitude = RampValueSafe(damageAmount, m_Metadata.m_EnduranceShakeMinDamage, m_Metadata.m_EnduranceShakeMaxDamage, 0.0f, m_Metadata.m_EnduranceShakeMaxAmplitude);
	if(shakeAmplitude > SMALL_FLOAT)
	{
		m_ActiveCamera->Shake(m_Metadata.m_EnduranceDamageShakeRef, shakeAmplitude);
	}
}

float camGameplayDirector::ComputeShakeAmplitudeForExplosion(float explosionStrength, float rollOffScaling, const Vector3& positionOfExplosion) const
{
	const float baseAmplitude = ComputeBaseAmplitudeForExplosion(explosionStrength, rollOffScaling, positionOfExplosion);

	//Also scale the shake amplitude based upon the distance of our (no post-effect) frame from the follow-ped.
	// - This compensates for the fact that the rotational shake feels stronger at a greater distance from the follow-ped.

	float followPedDistanceScaling = 1.0f;
	float followPedInteriorScaling = 1.0f;
	if(m_FollowPed)
	{
		const Vector3& cameraPositionToConsider	= m_Frame.GetPosition();
		const Vector3 followPedPosition			= VEC3V_TO_VECTOR3(m_FollowPed->GetTransform().GetPosition());
		const float distanceFromFrameToPed		= cameraPositionToConsider.Dist(followPedPosition);
		const float distanceRatio				= distanceFromFrameToPed / m_Metadata.m_ExplosionShakeSettings.m_IdealFollowPedDistance;
		if(distanceRatio >= 1.0f)
		{
			followPedDistanceScaling = InvSqrtf(distanceRatio);
		}

		TUNE_GROUP_FLOAT(CAM_EXPLOSIONS_SETTINGS, fInteriorScaling, 0.50f, 0.0f, 1.0f, 0.01f);
		followPedInteriorScaling = m_FollowPed->GetIsInInterior() ? fInteriorScaling : 1.0f;
	}

	const float amplitude = baseAmplitude * followPedDistanceScaling * followPedInteriorScaling * m_Metadata.m_ExplosionShakeSettings.m_ShakeAmplitudeScaling;

	return amplitude;
}

float camGameplayDirector::ComputeBaseAmplitudeForExplosion(float explosionStrength, float rollOffScaling, const Vector3& positionOfExplosion) const
{
	//Compute the amplitude based upon the explosion strength and the distance of the shake from our (no post-effect) frame.

	const Vector3& cameraPositionToConsider = m_Frame.GetPosition();

	const float distance	= positionOfExplosion.Dist(cameraPositionToConsider);
	const float maxDistance	= rollOffScaling * m_Metadata.m_ExplosionShakeSettings.m_DistanceLimits.y;
	float distanceScaling	= RampValueSafe(distance, m_Metadata.m_ExplosionShakeSettings.m_DistanceLimits.x, maxDistance, 1.0f, 0.0f);
	distanceScaling			= SlowIn(SlowInOut(distanceScaling));

	const float amplitude	= explosionStrength * distanceScaling;

	return amplitude;
}

void camGameplayDirector::StopExplosionShakes()
{
	const s32 numExplosions = m_ExplosionSettings.GetCount();
	for(s32 explosionIndex=0; explosionIndex<numExplosions; explosionIndex++)
	{
		camBaseFrameShaker* frameShaker = m_ExplosionSettings[explosionIndex].m_FrameShaker;
		if(frameShaker)
		{
			delete frameShaker;

			m_ExplosionSettings[explosionIndex].m_FrameShaker = NULL;
		}
	}

	m_ExplosionSettings.Reset();
}

bool camGameplayDirector::SetScriptOverriddenFollowPedCameraSettingsThisUpdate(const tGameplayCameraSettings& settings)
{
	//Ensure that the requested camera exists.
	const bool isCameraValid = (camFactory::FindObjectMetadata<camFollowPedCameraMetadata>(settings.m_CameraHash) != NULL);
	if(isCameraValid)
	{
		m_ScriptOverriddenFollowPedCameraSettingsThisUpdate = settings;
	}

	return isCameraValid;
}

bool camGameplayDirector::SetScriptOverriddenFollowVehicleCameraSettingsThisUpdate(const tGameplayCameraSettings& settings)
{
	//Ensure that the requested camera exists.
	const bool isCameraValid = (camFactory::FindObjectMetadata<camFollowVehicleCameraMetadata>(settings.m_CameraHash) != NULL);
	if(isCameraValid)
	{
		m_ScriptOverriddenFollowVehicleCameraSettingsThisUpdate = settings;
	}

	return isCameraValid;
}

bool camGameplayDirector::SetScriptOverriddenTableGamesCameraSettingsThisUpdate(const tGameplayCameraSettings& settings)
{
	//Ensure that the requested camera exists.
	const camTableGamesCameraMetadata* metadata = camFactory::FindObjectMetadata<camTableGamesCameraMetadata>(settings.m_CameraHash);
	const bool isCameraDataValid = (metadata != nullptr);
	if (isCameraDataValid)
	{
		m_ScriptOverriddenTableGamesCameraSettingsThisUpdate = settings;
		m_ScriptOverriddenTableGamesCameraSettingsThisUpdate.m_InterpolationDuration = metadata->m_TableGamesSettings.m_InterpolationDurationMS;
	}

	return isCameraDataValid;
}

bool camGameplayDirector::ShouldUseFirstPersonSwitch(const CPed& pedToConsider)
{
	bool IsInFirstPersonModeOnFoot = camControlHelperMetadataViewMode::FIRST_PERSON == camControlHelper::GetViewModeForContext(camControlHelperMetadataViewMode::ON_FOOT); 

	if (pedToConsider.GetIsInVehicle() && pedToConsider.GetMyVehicle())
	{
		CVehicle* pVeh = pedToConsider.GetMyVehicle();

		s32 viewMode = camControlHelper::ComputeViewModeContextForVehicle(*pVeh);

		cameraAssertf(viewMode != -1, "Failed to find the correct view mode for %s", 
			pVeh->GetModelName()); 

		if(viewMode > -1 ) 
		{
			if(camControlHelperMetadataViewMode::FIRST_PERSON == camControlHelper::GetViewModeForContext(viewMode))
			{
				return true; 
			}
		}

		if(IsInFirstPersonModeOnFoot && CPauseMenu::GetMenuPreference( PREF_FPS_PERSISTANT_VIEW ) == FALSE)
		{
			return true; 
		}
	}
	else
	{
		return IsInFirstPersonModeOnFoot;
	}

	return false;
}

void camGameplayDirector::ScriptRequestVehicleCamStuntSettingsThisUpdate()
{
	m_ScriptRequestedVehicleCamStuntSettingsThisUpdate = true;
}

void camGameplayDirector::ScriptForceVehicleCamStuntSettingsThisUpdate()
{
	m_ScriptForcedVehicleCamStuntSettingsThisUpdate = true;
}

void camGameplayDirector::ScriptRequestDedicatedStuntCameraThisUpdate(const char* stuntCameraName)
{
	if(!stuntCameraName)
	{
		return;
	}

	m_ScriptRequestedVehicleCamStuntSettingsThisUpdate = true;
	m_ScriptRequestDedicatedStuntCameraThisUpdate = true;
	m_DedicatedStuntCameraName = atHashString(stuntCameraName);
}

bool camGameplayDirector::IsVehicleCameraUsingStuntSettingsThisUpdate(const camBaseCamera* pTargetCamera) const
{
	if(pTargetCamera && pTargetCamera->GetIsClassId(camFollowVehicleCamera::GetStaticClassId()))
	{
		const camFollowVehicleCamera* pTargetVehicleCamera = static_cast<const camFollowVehicleCamera*>(pTargetCamera);
		return pTargetVehicleCamera->ShouldUseStuntSettings();
	}
	return false;
}

bool camGameplayDirector::IsAttachVehicleMiniTank() const
{
	if (!m_FollowVehicle || !m_FollowPed)
	{
		return false;
	}

	const s32 attachSeatIndex = ComputeSeatIndexPedIsIn(*m_FollowVehicle.Get(), *m_FollowPed.Get(), false);
	const bool isPedInMiniTank = MI_CAR_MINITANK.IsValid() && m_FollowVehicle->GetModelIndex() == MI_CAR_MINITANK && attachSeatIndex == 0;
	return isPedInMiniTank;
}

void camGameplayDirector::CatchUpFromFrame(const camFrame& sourceFrame, float maxDistanceToBlend /*= 0.0f*/, int blendType /*= -1*/)
{ 
    m_CatchUpSourceFrame.CloneFrom(sourceFrame); 
    m_CatchUpMaxDistanceToBlend = maxDistanceToBlend; 
    m_CatchUpBlendType = blendType, 
    m_ShouldInitCatchUpFromFrame = true;

#if !__FINAL && __DEV
    //Get the third person camera and run a preliminary check on the source frame.
    camThirdPersonCamera* thirdPersonCamera = GetThirdPersonCamera();
    if(thirdPersonCamera && ! thirdPersonCamera->PreCatchupFromFrameCheck( m_CatchUpSourceFrame ) )
    {
        ::rage::scrThread* vm = ::rage::scrThread::GetActiveThread();
        if( vm && vm->GetScriptName() && vm->GetCurrentCmdName() )
        {
            //cache the TtyLevel value in the Channel_Script before updating it so we can restore it after we finish.
            const u8 backupTtyLevel = Channel_Script.TtyLevel;
            Channel_Script.TtyLevel = DIAG_SEVERITY_DEBUG1;

            //print out the Script callstack.
            scrDisplayf("%s() called. Script Name = %s : Stack:", vm->GetCurrentCmdName(), vm->GetScriptName());
            ::rage::scrThread::PrePrintStackTrace();

            //restoring the previous Channel_Script.TtyLevel value.
            Channel_Script.TtyLevel = backupTtyLevel;
        }
    }
#endif //!__FINAL && __DEV
}

void camGameplayDirector::StartSwitchBehaviour(s32 switchType, bool isIntro, const CPed* oldPed, const CPed* newPed, s32 shortSwitchStyle, float directionSign)
{
	StopSwitchBehaviour();

	const CPed* pedToConsider = isIntro ? oldPed : newPed;
	if(!pedToConsider)
	{
		pedToConsider = m_FollowPed;
	}

	const bool isValidForPed = (pedToConsider && ComputeIsSwitchBehaviourValidForPed(switchType, *pedToConsider));
	const bool isFirstPersonCameraEnabled = pedToConsider && ShouldUseFirstPersonSwitch(*pedToConsider);
	const bool isPedInVehicle = pedToConsider && pedToConsider->GetIsInVehicle();

	bool isFullFirstPersonSwitch = isFirstPersonCameraEnabled && !isPedInVehicle;

	// Also use the full version of the first person switch if the ped is riding a bicycle
	CVehicle* pVeh = pedToConsider ? pedToConsider->GetMyVehicle() : NULL;
	if (pVeh && pVeh->GetVehicleType()==VEHICLE_TYPE_BICYCLE && isFirstPersonCameraEnabled)
	{
		isFullFirstPersonSwitch = true;
	}

	u32 switchHelperHash;

	switch(switchType)
	{
	case CPlayerSwitchInterface::SWITCH_TYPE_LONG:
		if(isValidForPed)
		{
			if (isFullFirstPersonSwitch)
			{
				switchHelperHash = isIntro ? m_Metadata.m_FirstPersonLongInSwitchHelperRef : m_Metadata.m_FirstPersonLongOutSwitchHelperRef;
			}
			else
			{
				switchHelperHash = isIntro ? m_Metadata.m_LongInSwitchHelperRef : m_Metadata.m_LongOutSwitchHelperRef;
			}
			
		}
		else
		{
			if (isFullFirstPersonSwitch)
			{
				switchHelperHash = isIntro ? m_Metadata.m_FirstPersonLongFallBackInSwitchHelperRef : m_Metadata.m_FirstPersonLongFallBackOutSwitchHelperRef;
			}
			else
			{
				switchHelperHash = isIntro ? m_Metadata.m_LongFallBackInSwitchHelperRef : m_Metadata.m_LongFallBackOutSwitchHelperRef;
			}
		}
		break;

	case CPlayerSwitchInterface::SWITCH_TYPE_MEDIUM:
		if(isValidForPed)
		{
			if (isFullFirstPersonSwitch)
			{
				switchHelperHash = isIntro ? m_Metadata.m_FirstPersonMediumInSwitchHelperRef : m_Metadata.m_FirstPersonMediumOutSwitchHelperRef;
			}
			else
			{
				switchHelperHash = isIntro ? m_Metadata.m_MediumInSwitchHelperRef : m_Metadata.m_MediumOutSwitchHelperRef;
			}			
		}
		else
		{
			if (isFullFirstPersonSwitch)
			{
				switchHelperHash = isIntro ? m_Metadata.m_FirstPersonMediumFallBackInSwitchHelperRef : m_Metadata.m_FirstPersonMediumFallBackOutSwitchHelperRef;
			}
			else
			{
				switchHelperHash = isIntro ? m_Metadata.m_MediumFallBackInSwitchHelperRef : m_Metadata.m_MediumFallBackOutSwitchHelperRef;
			}
		}
		break;

	case CPlayerSwitchInterface::SWITCH_TYPE_SHORT:
		{
			switch(shortSwitchStyle)
			{
			case CPlayerSwitchMgrShort::SHORT_SWITCH_STYLE_ROTATION:
				switchHelperHash = isIntro ? m_Metadata.m_ShortRotationInSwitchHelperRef : m_Metadata.m_ShortRotationOutSwitchHelperRef;
				break;

			case CPlayerSwitchMgrShort::SHORT_SWITCH_STYLE_TRANSLATION:
				switchHelperHash = isIntro ? m_Metadata.m_ShortTranslationInSwitchHelperRef : m_Metadata.m_ShortTranslationOutSwitchHelperRef;
				break;

			case CPlayerSwitchMgrShort::SHORT_SWITCH_STYLE_ZOOM_TO_HEAD:
				switchHelperHash = isIntro ? m_Metadata.m_ShortZoomToHeadInSwitchHelperRef : m_Metadata.m_ShortZoomToHeadOutSwitchHelperRef;
				break;

			//case CPlayerSwitchMgrShort::SHORT_SWITCH_STYLE_ZOOM_IN_OUT:
			default:
				// don't use the default short switch in first person mode, the camera movement is confusing
				if (isFullFirstPersonSwitch)
				{
					switchHelperHash = isIntro ? m_Metadata.m_ShortTranslationInSwitchHelperRef : m_Metadata.m_ShortTranslationOutSwitchHelperRef;
				}
				else
				{
					switchHelperHash = isIntro ? m_Metadata.m_ShortZoomInOutInSwitchHelperRef : m_Metadata.m_ShortZoomInOutOutSwitchHelperRef;
				}
				break;
			}
		}
		break;

	default:
		switchHelperHash = 0;
	}

	if(switchHelperHash)
	{
		m_SwitchHelper = camFactory::CreateObject<camBaseSwitchHelper>(switchHelperHash);
		if(m_SwitchHelper)
		{
			m_SwitchHelper->Init(isIntro, oldPed, newPed, directionSign, isFullFirstPersonSwitch, isFirstPersonCameraEnabled);
		}
	}
}

bool camGameplayDirector::ComputeIsSwitchBehaviourValidForPed(s32 switchType, const CPed& ped)
{
	bool isValid = true;

	if((switchType == CPlayerSwitchInterface::SWITCH_TYPE_LONG) || (switchType == CPlayerSwitchInterface::SWITCH_TYPE_MEDIUM))
	{
		//Always invalidate the medium/long-range Switch behaviours in interiors and very tight spaces.
		const bool isPedInInterior = ped.GetIsInInterior();
		if(isPedInInterior || (m_TightSpaceLevel >= (1.0f - SMALL_FLOAT)))
		{
			return false;
		}

		// block long and medium swoops when a close catchup is active.
		camThirdPersonCamera* thirdPersonCamera = GetThirdPersonCamera();
		if(thirdPersonCamera && thirdPersonCamera->GetCatchUpHelper()!=NULL)
		{			
			Vector3 pivotPosition = thirdPersonCamera->GetPivotPosition();
			Vector3 camPosition = thirdPersonCamera->GetFrame().GetPosition();
			Vector3 offset = pivotPosition - camPosition;

			TUNE_GROUP_FLOAT(PLAYER_SWITCH, minCatchupPivotOffsetMag, 1.0f, 0.0f, 100.0f, 0.001f);
			if (offset.XYMag()<=minCatchupPivotOffsetMag)
			{
				return false;
			}
		}

		//NOTE: We do not probe the space above vehicles at present.
		if(ped.GetIsInVehicle())
		{
			return true;
		}

		//Check for sufficient clearance above the ped.

		WorldProbe::CShapeTestCapsuleDesc capsuleTest;
		capsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
		capsuleTest.SetContext(WorldProbe::LOS_Camera);
		capsuleTest.SetIsDirected(false);

		const u32 collisionIncludeFlags = camCollision::GetCollisionIncludeFlagsForClippingTests();
		capsuleTest.SetIncludeFlags(collisionIncludeFlags);

		capsuleTest.SetExcludeEntity(&ped, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);

		float radius	= ped.GetCurrentMainMoverCapsuleRadius();
 		radius			-= m_Metadata.m_MinSafeRadiusReductionWithinPedMoverCapsuleForSwitchValidation;

		if (radius < SMALL_FLOAT)
		{
			return false;
		}

		const Vector3 capsuleStartPosition = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());

		Vector3 capsuleEndPosition;
		capsuleEndPosition.AddScaled(capsuleStartPosition, ZAXIS, m_Metadata.m_CapsuleLengthForSwitchValidation);

		capsuleTest.SetCapsule(capsuleStartPosition, capsuleEndPosition, radius);

		isValid = !WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);
	}

	return isValid;
}

s32 camGameplayDirector::ComputeShortSwitchStyle(const CPed& oldPed, const CPed& newPed, float& directionSign) const
{
	directionSign = 1.0f;

	//If the peds are in (or attached to) the same vehicle we must use the zoom-to-head style, as we are likely to transition between two
	//cameras that are attached to this same vehicle.
	const CVehicle* oldPedVehicle = oldPed.GetVehiclePedInside();
	if(!oldPedVehicle)
	{
		const fwEntity* attachParent = oldPed.GetAttachParent();
		if(attachParent && static_cast<const CEntity*>(attachParent)->GetIsTypeVehicle())
		{
			oldPedVehicle = static_cast<const CVehicle*>(attachParent);
		}
	}

	const CVehicle* newPedVehicle = newPed.GetVehiclePedInside();
	if(!newPedVehicle)
	{
		const fwEntity* attachParent = newPed.GetAttachParent();
		if(attachParent && static_cast<const CEntity*>(attachParent)->GetIsTypeVehicle())
		{
			newPedVehicle = static_cast<const CVehicle*>(attachParent);
		}
	}

	if(oldPedVehicle && (oldPedVehicle == newPedVehicle))
	{
		return CPlayerSwitchMgrShort::SHORT_SWITCH_STYLE_ZOOM_TO_HEAD;
	}

	float absDeltaOldPedToPedDeltaHeading = 0.0f;

	const fwTransform& oldPedTransform	= oldPed.GetTransform();
	const fwTransform& newPedTransform	= newPed.GetTransform();
	const Vector3 oldPedPosition		= VEC3V_TO_VECTOR3(oldPedTransform.GetPosition());
	const Vector3 newPedPosition		= VEC3V_TO_VECTOR3(newPedTransform.GetPosition());
	const Vector3 pedDelta				= newPedPosition - oldPedPosition;
	const float pedDistance2			= pedDelta.Mag2();
	if(pedDistance2 >= VERY_SMALL_FLOAT)
	{
		const float pedDistance			= Sqrtf(pedDistance2);
		const Vector3 pedDirection		= pedDelta / pedDistance;

		float pedDeltaHeading;
		float pedDeltaPitch;
		camFrame::ComputeHeadingAndPitchFromFront(pedDirection, pedDeltaHeading, pedDeltaPitch);

		//NOTE: We consider the heading and pitch of the gameplay frame when Switching from the active follow ped.
		float headingToConsiderForOldPed;
		float pitchToConsiderForOldPed;
		if(&oldPed == m_FollowPed)
		{
			m_Frame.ComputeHeadingAndPitch(headingToConsiderForOldPed, pitchToConsiderForOldPed);
		}
		else
		{
			camFrame::ComputeHeadingAndPitchFromMatrix(MAT34V_TO_MATRIX34(oldPedTransform.GetMatrix()), headingToConsiderForOldPed, pitchToConsiderForOldPed);
		}

		float headingToConsiderForNewPed = camFrame::ComputeHeadingFromMatrix(MAT34V_TO_MATRIX34(newPedTransform.GetMatrix()));

		//NOTE: We use the heading of the cover point if the new ped is considered to be in cover from a camera-perspective.
		const CCoverPoint* coverPoint = newPed.GetCoverPoint();
		if(coverPoint)
		{
			const CPedIntelligence* newPedIntelligence = newPed.GetPedIntelligence();
			if(newPedIntelligence)
			{
				const CTaskCover* newPedCoverTask = static_cast<CTaskCover*>(newPedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_COVER));
				if(newPedCoverTask)
				{
					const bool isNewPedConsideredInCoverForCamera = newPedCoverTask->IsConsideredInCoverForCamera();
					if(isNewPedConsideredInCoverForCamera)
					{
						Vector3 coverPosition;
						const bool isCoverPositionValid = coverPoint->GetCoverPointPosition(coverPosition);
						if(isCoverPositionValid)
						{
							Vector3 targetDirection	= coverPosition - newPedPosition;
							targetDirection.NormalizeSafe(YAXIS);

							const Vector3 coverNormal = VEC3V_TO_VECTOR3(coverPoint->GetCoverDirectionVector(&RCC_VEC3V(targetDirection)));

							headingToConsiderForNewPed = camFrame::ComputeHeadingFromFront(coverNormal);
						}
					}
				}
			}
		}

		const float absHeadingDeltaBetweenPeds		= Abs(fwAngle::LimitRadianAngle(headingToConsiderForNewPed - headingToConsiderForOldPed));
		const float deltaOldPedToPedDeltaHeading	= fwAngle::LimitRadianAngle(headingToConsiderForOldPed - pedDeltaHeading);
		absDeltaOldPedToPedDeltaHeading				= Abs(deltaOldPedToPedDeltaHeading);

		if(pedDistance <= m_Metadata.m_MaxDistanceBetweenPedsForShortRotation)
		{
			//Check that the peds are facing each other, taking into account the game camera for the old ped, if appropriate.
			if(absHeadingDeltaBetweenPeds >= (m_Metadata.m_MinAbsHeadingDeltaBetweenPedsForShortRotation * DtoR))
			{
				if(absDeltaOldPedToPedDeltaHeading <= (m_Metadata.m_MaxAbsDeltaOldPedToPedDeltaHeadingForShortRotation * DtoR))
				{
					//Compute the direction of rotation so that we can ensure this is consistent for both the intro and outro,
					//irrespective of the specific configuration of peds/cameras.
					directionSign = (deltaOldPedToPedDeltaHeading >= 0.0f) ? 1.0f : -1.0f;

					return CPlayerSwitchMgrShort::SHORT_SWITCH_STYLE_ROTATION;
				}
			}
		}

		//Check that the peds are side-to-side or up-and-down, taking into account the game camera for the old ped, if appropriate.
		if(absHeadingDeltaBetweenPeds <= (m_Metadata.m_MaxAbsHeadingDeltaBetweenPedsForShortTranslation * DtoR))
		{
			const float absDeltaOldPedToPedDeltaPerpendicularHeading = Abs(HALF_PI - absDeltaOldPedToPedDeltaHeading);
			if(absDeltaOldPedToPedDeltaPerpendicularHeading <= (m_Metadata.m_MaxAbsDeltaOldPedToPedDeltaPerpHeadingForShortTranslation * DtoR))
			{
				return CPlayerSwitchMgrShort::SHORT_SWITCH_STYLE_TRANSLATION;
			}

			const float absDeltaOldPedToPedDeltaPitch				= Abs(fwAngle::LimitRadianAngle(pitchToConsiderForOldPed - pedDeltaPitch));
			const float absDeltaOldPedToPedDeltaPerpendicularPitch	= Abs(HALF_PI - absDeltaOldPedToPedDeltaPitch);
			if(absDeltaOldPedToPedDeltaPerpendicularPitch <= (m_Metadata.m_MaxAbsDeltaOldPedToPedDeltaPerpPitchForShortTranslation * DtoR))
			{
				return CPlayerSwitchMgrShort::SHORT_SWITCH_STYLE_TRANSLATION;
			}
		}
	}

	//Default to the zoom-in-out style.

	directionSign = (absDeltaOldPedToPedDeltaHeading < HALF_PI) ? 1.0f : -1.0f;

	return CPlayerSwitchMgrShort::SHORT_SWITCH_STYLE_ZOOM_IN_OUT;
}

void camGameplayDirector::StopSwitchBehaviour()
{
	if(m_SwitchHelper)
	{
		delete m_SwitchHelper;
		m_SwitchHelper = NULL;
	}
}


void camGameplayDirector::SetSwitchBehaviourPauseState(bool state)
{
	if(m_SwitchHelper)
	{
		m_SwitchHelper->SetPauseState(state);
	}
}

bool camGameplayDirector::IsSwitchBehaviourFinished() const
{
	bool isFinished = true;

	if(m_SwitchHelper)
	{
		isFinished = m_SwitchHelper->IsFinished();
	}

	return isFinished;
}

void camGameplayDirector::UpdateSwitchBehaviour()
{
	if(!m_SwitchHelper)
	{
		return;
	}

	m_SwitchHelper->Update();
}

void camGameplayDirector::UpdateFirstPersonAnimatedDataState(const CPed& FPS_MODE_SUPPORTED_ONLY(followPed))
{
    const bool wasFirstPersonAnimatedDataInVehicle = m_bFirstPersonAnimatedDataInVehicle;
	m_bFirstPersonAnimatedDataInVehicle = false;
	m_bPovCameraAnimatedDataBlendoutPending = false;

#if FPS_MODE_SUPPORTED
	// If player is playing an anim with first person animated data, while the in car cinematic camera is active,
	// force the first person camera to update underneath so we can blend to/from the animated data.
	if( camInterface::GetCinematicDirector().IsRenderingAnyInVehicleFirstPersonCinematicCamera() ||
		(m_ActiveCamera && m_ActiveCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId())) )
	{
		Vec3V vTrans;
		QuatV qRot;
		float fMinX, fMaxX, fMinY, fMaxY, fMinZ, fMaxZ, fov;
		float fWeight = 0.0f;
		bool bFirstPersonAnimData = followPed.GetFirstPersonCameraDOFs(vTrans, qRot, fMinX, fMaxX, fMinY, fMaxY, fMinZ, fMaxZ, fWeight, fov);
		if(bFirstPersonAnimData && fWeight != 0.0f)
		{
			m_bFirstPersonAnimatedDataInVehicle = true;
            if( m_bFirstPersonAnimatedDataInVehicle != wasFirstPersonAnimatedDataInVehicle )
            {
                m_FirstPersonAnimatedDataStartTime = fwTimer::GetTimeInMilliseconds();
            }
		}
		else
		{
			const camBaseCamera* pDominantRenderedCamera = camInterface::GetDominantRenderedCamera();
			if(pDominantRenderedCamera && camInterface::GetDominantRenderedCamera()->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
			{
				const camCinematicMountedCamera* pMountedCamera	= static_cast<const camCinematicMountedCamera*>(pDominantRenderedCamera);
				if(pMountedCamera->WasUsingAnimatedData())
				{
					m_bPovCameraAnimatedDataBlendoutPending = true;
				}
			}
		}
	}

    if( wasFirstPersonAnimatedDataInVehicle && ! m_bFirstPersonAnimatedDataInVehicle )
    {
        m_FirstPersonAnimatedDataStartTime = 0;
    }
#endif // FPS_MODE_SUPPORTED
}

//Hint
void camGameplayDirector::CreateRagdollLookAtEnvelope()
{
	if(m_RagdollBlendEnvelope)
	{
		delete m_RagdollBlendEnvelope; 
		m_RagdollBlendEnvelope = NULL; 
	}

	camEnvelope* pEnvelope = camFactory::CreateObject<camEnvelope>(m_Metadata.m_RagdollLookAtEnvelope);
	if(cameraVerifyf(pEnvelope, "Failed to create a hint ragdoll look at envelope (name: %s, hash: %u)",
		SAFE_CSTRING(m_Metadata.m_RagdollLookAtEnvelope.GetCStr()), m_Metadata.m_RagdollLookAtEnvelope.GetHash()))
	{
		m_RagdollBlendEnvelope = pEnvelope;
	}
}


void camGameplayDirector::CreateHintHelperEnvelope(u32 Attack, s32 hold, u32 Release )
{
	//TODO: This looks like it leaks! Investigate.
	camEnvelopeMetadata* pEnvelope = rage_new camEnvelopeMetadata(); 
	if(cameraVerifyf(pEnvelope, "The gameplay director failed to create custom envelope metadata for a hint"))
	{
		pEnvelope->m_AttackDuration = Attack;
		pEnvelope->m_HoldDuration = hold;
		pEnvelope->m_ReleaseDuration = Release;
		pEnvelope->m_DecayDuration = 0;
		pEnvelope->m_SustainLevel = 1.0f;
		pEnvelope->m_ReleaseCurveType = CURVE_TYPE_LINEAR; 

		m_HintEnvelope = camFactory::CreateObject<camEnvelope>(*pEnvelope);
	}
}

void camGameplayDirector::InitHint()
{
	if(m_HintEnvelope)
	{
		m_HintEnvelopeLevel = 0.0f; 
		m_HintEnvelope->Start(); 
	}
	m_IsHintFirstUpdate = true; 
}

void camGameplayDirector::StartHinting(const Vector3& Target, u32 Attack, s32 Sustain, u32 Release, u32 HintType)
{
	if(m_HintEnvelope == NULL)
	{
		m_HintEnt = NULL;
		m_IsRelativeHint = false;
		m_HintOffset.Zero();
		m_HintPos = Target; 
		m_HintType.SetHash(HintType);
		CreateHintHelperEnvelope(Attack, Sustain, Release); 
		InitHint(); 
		
	}
}

void camGameplayDirector::StartHinting(const CEntity* pEnt, const Vector3& Offset, bool IsRelative , u32 Attack, s32 Sustain, u32 Release, u32 HintType)
{
	if(m_HintEnvelope == NULL)
	{
		if(cameraVerifyf(pEnt,"Trying to hint at an entity that doesn't exit"))
		{
			m_HintEnt = pEnt;
			m_IsRelativeHint = IsRelative; 
			m_HintOffset = Offset; 
			m_HintType.SetHash(HintType); 
			
			CreateHintHelperEnvelope(Attack, Sustain, Release); 

			InitHint();

			if(m_HintEnt && m_HintEnt->GetIsTypePed())
			{
				CreateRagdollLookAtEnvelope(); 
			}
		}
	}
}

void camGameplayDirector::StopHinting(bool StopImmediately)
{ 
	if(m_HintEnvelope)
	{
		if (!StopImmediately)
		{
			m_HintEnvelope->Release();
		}
		else
		{
			m_HintEnvelope->Stop();
		}
	}
}

const Vector3& camGameplayDirector::UpdateHintPosition()
{
	if(m_HintEnt)
	{
		if(m_IsRelativeHint)
		{
			m_HintLookAtMat.Transform(m_HintOffset, m_HintPos);
		}
		else
		{
			return m_HintPos = m_HintLookAtMat.d + m_HintOffset ; 
		}
	}
	
	return m_HintPos; 
}

bool camGameplayDirector::IsHintActive() const
{
	if(m_HintEnvelope)
	{
		return m_HintEnvelope->IsActive(); 
	}

	return false; 
}

bool camGameplayDirector::IsSphereVisible(const Vector3& centre, const float radius) const
{
	const spdSphere sphere(RCC_VEC3V(centre), ScalarV(radius));
	const bool isVisible = m_TransposedPlaneSet.IntersectsSphere(sphere);

	return isVisible;
}

const camVehicleCustomSettingsMetadata* camGameplayDirector::FindVehicleCustomSettings(u32 vehicleModelNameHash) const
{
	const camVehicleCustomSettingsMetadata* const* metadataRef	= m_VehicleCustomSettingsMap.Access(vehicleModelNameHash);
	const camVehicleCustomSettingsMetadata* metadata			= metadataRef ? *metadataRef : NULL;

	return metadata;
}

const CWeaponInfo* camGameplayDirector::ComputeWeaponInfoForPed(const CPed& ped)
{
	//NOTE: The actual equipped/held weapon object is prioritised. We fall-back to using the 'equipped' weapon, which may not actually be held by the ped.

	const CWeaponInfo* weaponInfo = NULL;

	const CPedWeaponManager* weaponManager = ped.GetWeaponManager();
	if(weaponManager)
	{
		// If the ped is doing a grenade throw from aiming, get the back-up weapon info (as equipped weapon has briefly changed to the grenade)
		if (ped.GetPedResetFlag(CPED_RESET_FLAG_IsThrowingProjectileWhileAiming))
		{
			u32 uBackupWeaponHash = weaponManager->GetBackupWeapon();
			if (uBackupWeaponHash != 0)
			{
				weaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uBackupWeaponHash);
			} 
		}

		const CWeapon* weapon = weaponManager->GetEquippedWeapon();
		if(weapon && !weaponInfo)
		{
			weaponInfo = weapon->GetWeaponInfo();
		}

		if(!weaponInfo)
		{
			weaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(weaponManager->GetEquippedWeaponHash());
		}
	}

	if(!weaponInfo)
	{
		weaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(ped.GetDefaultUnarmedWeaponHash());
	}

	return weaponInfo;
}

bool camGameplayDirector::SetDeathFailEffectState(s32 state)
{
	bool hasSucceeded = true;

	switch(state)
	{
	case camInterface::DEATH_FAIL_EFFECT_INACTIVE:
		if(m_DeathFailEffectFrameShaker)
		{
			delete m_DeathFailEffectFrameShaker;
		}
		break;

	case camInterface::DEATH_FAIL_EFFECT_INTRO:
		if(m_Metadata.m_DeathFailIntroEffectShakeRef)
		{
			if(m_DeathFailEffectFrameShaker)
			{
				delete m_DeathFailEffectFrameShaker;
			}

			m_DeathFailEffectFrameShaker = camFactory::CreateObject<camBaseFrameShaker>(m_Metadata.m_DeathFailIntroEffectShakeRef);
		}
		break;

	case camInterface::DEATH_FAIL_EFFECT_OUTRO:
		if(m_Metadata.m_DeathFailOutroEffectShakeRef)
		{
			if(m_DeathFailEffectFrameShaker)
			{
				delete m_DeathFailEffectFrameShaker;
			}

			m_DeathFailEffectFrameShaker = camFactory::CreateObject<camBaseFrameShaker>(m_Metadata.m_DeathFailOutroEffectShakeRef);
		}
		break;

	default:
		cameraAssertf(false, "Invalid camera death/fail effect state (%d)", state);
		hasSucceeded = false;
	}

	return hasSucceeded;
}

void camGameplayDirector::GetThirdPersonToFirstPersonFovParameters(float& fovDelta, u32& duration) const
{
	fovDelta = m_Metadata.m_ThirdToFirstPersonFovDelta;
	duration = m_Metadata.m_ThirdToFirstPersonDuration;
}

void camGameplayDirector::CachePreviousCameraOrientation(const camFrame& previousCamFrame)
{
	if(camInterface::GetDominantRenderedDirector() == this)
	{
		m_PreviousAimCameraForward		= previousCamFrame.GetFront();
		m_PreviousAimCameraPosition		= previousCamFrame.GetPosition();
	}
}

#if FPS_MODE_SUPPORTED
bool camGameplayDirector::ComputeIsFirstPersonModeAllowed(const CPed* followPed, const bool bCheckViewMode, bool& shouldFallBackForDirectorBlendCatchUpOrSwitch,
														  bool* WIN32PC_ONLY(pToggleForceBonnetCameraView /*= NULL*/), bool* WIN32PC_ONLY(pDisableForceBonnetCameraView /*= NULL*/)) const
{
#if GTA_REPLAY
	// During the window where we're cleaning up the scripts for replay, force the FP to be disallowed.
	// This should put the director back to a default 3rd person camera before Replay takes control of the world.
	if(CReplayMgr::ShouldScriptsCleanup())
	{
		return false;
	}
#endif // GTA_REPLAY

	bool bConsiderFollowPedInVehicle = m_VehicleEntryExitState == INSIDE_VEHICLE;
	const CPedIntelligence* pedIntelligence	= (followPed) ? followPed->GetPedIntelligence() : NULL;
	if(pedIntelligence)
	{
		// For first person test, treat exiting vehicle as not in vehicle.
		const CTask* exitVehicleTask = (const CTask*)(pedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE));
		bConsiderFollowPedInVehicle &= (exitVehicleTask == NULL);

		// For first person test, treat entering vehicle as not in vehicle.
		const CTask* enterVehicleTask = (const CTask*)(pedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
		bConsiderFollowPedInVehicle &= (enterVehicleTask == NULL);
	}

	const bool bCheckVehicleContext = (m_State == START_FOLLOWING_VEHICLE || m_State == FOLLOWING_VEHICLE) && bConsiderFollowPedInVehicle;

	shouldFallBackForDirectorBlendCatchUpOrSwitch = false;

	if(!m_FollowPed.Get())
	{
		return false;
	}

	s32 viewModeContextToTest = camControlHelperMetadataViewMode::ON_FOOT;
	if(bCheckVehicleContext && m_FollowVehicle != NULL)
	{
		viewModeContextToTest = camControlHelperMetadataViewMode::IN_VEHICLE;

		// When in a vehicle, check the vehicle context so we can allow running the first person camera
		// if the vehicle pov camera is active.  This is to allow running an animated camera on first person camera.
		// Assign a vehicle-class-specific view mode context where appropriate for our follow vehicle.
		if(m_FollowVehicle->InheritsFromBike() || m_FollowVehicle->InheritsFromQuadBike() || m_FollowVehicle->InheritsFromAmphibiousQuadBike())
		{
			viewModeContextToTest = camControlHelperMetadataViewMode::ON_BIKE;
		}
		else if(m_FollowVehicle->InheritsFromBoat() || m_FollowVehicle->GetIsJetSki() || m_FollowVehicle->InheritsFromAmphibiousAutomobile() || m_FollowVehicle->InheritsFromAmphibiousQuadBike())
		{
			viewModeContextToTest = camControlHelperMetadataViewMode::IN_BOAT;
		}
		else if(m_FollowVehicle->InheritsFromHeli())
		{
			viewModeContextToTest = camControlHelperMetadataViewMode::IN_HELI;
		}
		else if(m_FollowVehicle->GetIsAircraft() || m_FollowVehicle->InheritsFromBlimp())
		{
			viewModeContextToTest = camControlHelperMetadataViewMode::IN_AIRCRAFT;
		}
		else if(m_FollowVehicle->InheritsFromSubmarine())
		{
			viewModeContextToTest = camControlHelperMetadataViewMode::IN_SUBMARINE;
		}
	}

	s32 currentViewMode = GetActiveViewMode( viewModeContextToTest );

	// TODO: rename PREF_FPS_PERSISTANT_VIEW? maybe someday...
	const bool bAllowIndependentViewModes = (CPauseMenu::GetMenuPreference( PREF_FPS_PERSISTANT_VIEW ) == TRUE);
	if( !bAllowIndependentViewModes && camControlHelper::GetLastSetViewMode() >= 0 )
	{
		if( currentViewMode != camControlHelperMetadataViewMode::FIRST_PERSON && camControlHelper::GetLastSetViewMode() == camControlHelperMetadataViewMode::FIRST_PERSON)
		{
			// View mode will be properly updated on camera update, so have to check here if we need to
			// force the view to first person or the current camera will be incorrect.
			// (for a new camera, the update has not been done yet, so view mode is not up to date)
			currentViewMode = camControlHelper::GetLastSetViewMode();
		}
		else if( currentViewMode == camControlHelperMetadataViewMode::FIRST_PERSON && camControlHelper::GetLastSetViewMode() != camControlHelperMetadataViewMode::FIRST_PERSON)
		{
			// View mode will be properly updated on camera update, so have to check here if we need to
			// force the view to first person or the current camera will be incorrect.
			// (for a new camera, the update has not been done yet, so view mode is not up to date)
			currentViewMode = camControlHelper::GetLastSetViewMode();
		}
	}

#if RSG_PC
	if( pToggleForceBonnetCameraView && pDisableForceBonnetCameraView )
	{
		if( (viewModeContextToTest == camControlHelperMetadataViewMode::IN_HELI ||
			 viewModeContextToTest == camControlHelperMetadataViewMode::IN_AIRCRAFT) &&
			followPed && followPed->IsLocalPlayer() && followPed->GetIsDrivingVehicle() && !followPed->GetIsDeadOrDying() )
		{
			const CWeaponInfo *pCurrentWeaponInfo = followPed->GetEquippedWeaponInfo();
			const bool bIsVehicleArmed = pCurrentWeaponInfo && pCurrentWeaponInfo->GetIsVehicleWeapon();
			if(bIsVehicleArmed)
			{
				const CControl* control = GetActiveControl();
				if( control )
				{
					const bool bIsReleased = control->GetVehicleFlyAttackCam().IsReleased();
					const bool bWasPressedWithinThreshold = control->GetVehicleFlyAttackCam().HistoryPressed(camControlHelper::GetBonnetCameraToggleTime());
					////const bool bWasPressedWithinThreshold = !control->GetVehicleFlyAttackCam().IsReleasedAfterHistoryHeldDown(camControlHelper::GetBonnetCameraToggleTime());
					DEV_ONLY(const bool bWasHeldForLookBehind = control->GetVehicleLookBehind().HistoryHeldDown(camControlHelper::GetBonnetCameraToggleTime());)
					DEV_ONLY(           bWasHeldForLookBehind;)
					const bool bCheckForReleaseOnly = control->WasKeyboardMouseLastKnownSource(); // || m_ForceFirstPersonBonnetCamera;
					*pToggleForceBonnetCameraView =	(!bCheckForReleaseOnly && bIsReleased && bWasPressedWithinThreshold) ||
													( bCheckForReleaseOnly && bIsReleased);
				}
			}
		}
		else
		{
			*pDisableForceBonnetCameraView = true;
			*pToggleForceBonnetCameraView = false;
		}
	}
#endif

	if( bCheckViewMode && currentViewMode != camControlHelperMetadataViewMode::FIRST_PERSON &&
		!m_ForceFirstPersonBonnetCamera && !m_OverrideFirstPersonCameraThisUpdate )
	{
		return false;
	}

	if( m_DisableFirstPersonThisUpdate )
	{
		return false;
	}

	if(m_FollowPed->ShouldBeDead() || m_FollowPed->GetIsArrested())
	{
		return false;
	}

	const CPedIntelligence* followPedIntelligence			= m_FollowPed->GetPedIntelligence();
	const CQueriableInterface* followPedQueriableInterface	= followPedIntelligence ? followPedIntelligence->GetQueriableInterface() : NULL;
	if(!followPedQueriableInterface)
	{
		return false;
	}

	if( m_FollowPed->GetIsDeadOrDying() ||
		m_FollowPed->GetPedConfigFlag( CPED_CONFIG_FLAG_KilledByStealth ) ||
		m_FollowPed->GetPedConfigFlag( CPED_CONFIG_FLAG_KilledByTakedown ) ||
		m_FollowPed->GetPedConfigFlag( CPED_CONFIG_FLAG_Knockedout ) )
	{
		return false;
	}

	if( m_ForceThirdPersonCameraForRagdollAndGetUps )
	{
		if(m_FollowPed->GetUsingRagdoll() || followPedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_GET_UP))
		{
			return false;
		}

		const bool c_IsFollowVehicleInWater = m_FollowVehicle && m_FollowVehicle->GetIsInWater();
		CTaskExitVehicleSeat* pExitVehicleSeatTask = (CTaskExitVehicleSeat*)(followPedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE_SEAT));
		if(pExitVehicleSeatTask && pExitVehicleSeatTask->GetState() == CTaskExitVehicleSeat::State_JumpOutOfSeat)
		{
			if(!c_IsFollowVehicleInWater)
			{
				return false;
			}
		}
		else if(pExitVehicleSeatTask == NULL)
		{
			CTaskInfo* pExitVehicleSeatInfo = followPedQueriableInterface->FindTaskInfoIfCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE_SEAT);
			if(pExitVehicleSeatInfo && pExitVehicleSeatInfo->GetState() == CTaskExitVehicleSeat::State_JumpOutOfSeat)
			{
				if(!c_IsFollowVehicleInWater)
				{
					return false;
				}
			}
		}
	}

	if( (CPauseMenu::GetMenuPreference( PREF_FPS_COMBATROLL ) == FALSE) )
	{
		const CTask* pTaskCombatRoll = (const CTask*)(followPedIntelligence->FindTaskActiveMotionByType(CTaskTypes::TASK_COMBAT_ROLL));
		if( pTaskCombatRoll )
		{
			return false;
		}
	}

	if( m_ForceThirdPersonCameraWhenInputDisabled )
	{
		//Disable first person mode if player control is disabled and this director is rendering exclusively.
		//NOTE: Any interpolation in or out of other directors will handled by a suitable fall-back camera.
		const CPlayerInfo* followPedPlayerInfo	= m_FollowPed->GetPlayerInfo();
		const bool isPlayerInControl			= followPedPlayerInfo && !followPedPlayerInfo->AreControlsDisabled();
		if(!isPlayerInControl)
		{
			//NOTE: Checking the rendered directors/cameras is likely to result in a frame of delay here.
			const atArray<tRenderedCameraObjectSettings>& renderedDirectors	= camInterface::GetRenderedDirectors();
			const s32 numRenderedDirectors									= renderedDirectors.GetCount();
			if((numRenderedDirectors == 1) && (renderedDirectors[0].m_Object == this))
			{
				return false;
			}
		}
	}

	if( m_AllowThirdPersonCameraFallbacks )
	{
		if((CTaskCover::CanUseThirdPersonCoverInFirstPerson(*m_FollowPed)))
		{
			if(followPedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_COVER))
			{
				return false;
			}
		}

		CTaskExitVehicle* exitVehicleTask = static_cast<CTaskExitVehicle*>(followPedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE));
		if( exitVehicleTask && 
			(exitVehicleTask->IsRunningFlagSet(CVehicleEnterExitFlags::VehicleIsUpsideDown) || 
			 exitVehicleTask->IsRunningFlagSet(CVehicleEnterExitFlags::VehicleIsOnSide)) )
		{
			return false;
		}

		CTaskParachute* parachuteTask = static_cast<CTaskParachute*>(followPedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_PARACHUTE));
		if (parachuteTask)
		{
			const camFirstPersonShooterCamera* fpsCamera		= GetFirstPersonShooterCamera();
			const camThirdPersonCamera* thirdPersonCamera		= GetThirdPersonCamera();
			const camControlHelper* controlHelper				= fpsCamera ? fpsCamera->GetControlHelper() : thirdPersonCamera ? thirdPersonCamera->GetControlHelper() : NULL;

			const CControl* control								= GetActiveControl();
			const bool isLookingBehind							= controlHelper && controlHelper->WillBeLookingBehindThisUpdate(*control);
			if (isLookingBehind)
			{
				return false;
			}

			// If we are using the camera, disable first person camera while aiming.
			if (CPhoneMgr::CamGetState())
			{
				CTaskMobilePhone* pTaskMobilePhone = static_cast<CTaskMobilePhone*>(followPedIntelligence->FindTaskSecondaryByType(CTaskTypes::TASK_MOBILE_PHONE));
				if (pTaskMobilePhone)
				{
					const s32 iState = pTaskMobilePhone->GetState();
					if ((iState == CTaskMobilePhone::State_CameraLoop) || (iState == CTaskMobilePhone::State_Paused))
					{
						return false;
					}
				}
			}
		}

		//HACK while we are entering or exiting the APC, invalidate the first person.
		const CTaskEnterVehicle* enterVehicleTask = static_cast<CTaskEnterVehicle*>(followPedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
		if(m_FollowVehicle && (enterVehicleTask || exitVehicleTask) && MI_CAR_APC.IsValid() && m_FollowVehicle->GetModelIndex() == MI_CAR_APC)
		{
			return false;
		}
	}

	const CVehicleModelInfo* followVehicleModelInfo = m_FollowVehicle ? m_FollowVehicle->GetVehicleModelInfo() : NULL;
	if( followVehicleModelInfo )
	{
		const u32 cameraHash = followVehicleModelInfo->GetBonnetCameraNameHash();
		// Special case, camera looks bad getting into/out of tank
		// TODO: add field to vehicle metadata?
		if( cameraHash == ATSTRINGHASH("VEHICLE_BONNET_CAMERA_TANK", 0xD83171A9) && 
			(m_VehicleEntryExitState == ENTERING_VEHICLE || m_VehicleEntryExitState == EXITING_VEHICLE) )
		{
			return false;
		}

		const u32 turretCameraHash = camInterface::ComputePovTurretCameraHash(m_FollowVehicle);
		if(IsUsingVehicleTurret(false))
		{
			const camCinematicMountedCameraMetadata* metadata = camFactory::FindObjectMetadata<camCinematicMountedCameraMetadata>(turretCameraHash);
			if( metadata == NULL )
				return false;
		}
		else ////if( m_VehicleEntryExitState <= INSIDE_VEHICLE )
		{
			const camCinematicMountedCameraMetadata* metadata = camFactory::FindObjectMetadata<camCinematicMountedCameraMetadata>(cameraHash);
			if( metadata == NULL )
			{
				metadata = camFactory::FindObjectMetadata<camCinematicMountedCameraMetadata>(turretCameraHash);
				if( metadata == NULL )
					return false;
			}
		}
	}

	if( m_VehicleEntryExitState == INSIDE_VEHICLE &&
		camInterface::GetCinematicDirector().IsFirstPersonInVehicleDisabledThisUpdate() )
	{
		return false;
	}

	//block first person when in a sub if we're clipping with water
	if (m_FollowVehicle && m_FollowVehicle->InheritsFromSubmarine())
	{
		bool isEnteringVehicle = false;
		if(followPedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE))
		{
			CTaskEnterVehicle* enterVehicleTask	= static_cast<CTaskEnterVehicle*>(followPedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
			isEnteringVehicle					= enterVehicleTask && enterVehicleTask->IsConsideredGettingInVehicleForCamera();
		}

		//we only want to do these checks if the we're already in first person view mode
		if (currentViewMode == camControlHelperMetadataViewMode::FIRST_PERSON)
		{
			//if we're entering or inside, and first person has been blocked, continue to block it - m_IsFirstPersonModeAllowed was set last frame
			if ((isEnteringVehicle || m_VehicleEntryExitState == INSIDE_VEHICLE) && !m_IsFirstPersonModeAllowed)
			{
				return false;
			}

			//if we're entering or inside and weren't blocked, do water test. always do water test if exiting
			if (isEnteringVehicle || m_VehicleEntryExitState == INSIDE_VEHICLE || m_VehicleEntryExitState == EXITING_VEHICLE)
			{
				Vector3 headPosition;
				if(m_FollowPed && m_FollowPed->GetBonePosition(headPosition, BONETAG_HEAD))
				{
					float waterHeight = 0.0f;
					if(camCollision::ComputeWaterHeightAtPosition(headPosition, g_MaxDistanceForWaterClippingTestWhenInASub, waterHeight, g_MaxDistanceForRiverWaterClippingTestWhenInASub))
					{
						if (headPosition.z < waterHeight + g_MinHeightAboveWaterForFirstPersonWhenInASub)
						{
							return false;
						}
					}
				}
			}
		}
	}

	//We cannot cleanly handle camera hinting in first-person when parachuting, so we must fall back to third-person.
	const bool isHinting = IsHintActive();
	if(isHinting && followPedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_PARACHUTE))
	{
		return false;
	}

	// B* 1997259 - 1st Person: Disable when an animal
	if (!CPlayerInfo::IsFirstPersonModeSupportedForPed(*m_FollowPed))
	{
		return false;
	}

	//Fall back to third-person when interpolating in or out of another director, when performing a valid catch-up transition and whenever a player Switch is active.
	//NOTE: Special third-person behaviours are implemented in such cases.

	//NOTE: The gameplay director doesn't interpolate, so it will not be included in this number.
	const u32 numDirectorsInterpolating	= camManager::ComputeNumDirectorsInterpolating();
	bool isValidCatchUpTransitionActive	= m_ShouldInitCatchUpFromFrame;
	if(!isValidCatchUpTransitionActive)
	{
		const camThirdPersonCamera* thirdPersonCamera = GetThirdPersonCamera();
		if(thirdPersonCamera)
		{
			const camCatchUpHelper* catchUpHelper = thirdPersonCamera->GetCatchUpHelper();
			if(catchUpHelper && (catchUpHelper->GetNameHash() == g_CatchUpHelperRefForFirstPerson))
			{
				isValidCatchUpTransitionActive = !catchUpHelper->WillFinishThisUpdate();
			}
		}
	}

	const bool isSwitchActive						= g_PlayerSwitch.IsActive();
	shouldFallBackForDirectorBlendCatchUpOrSwitch	= ((numDirectorsInterpolating >= 1) || isValidCatchUpTransitionActive || (isSwitchActive && !m_HasSwitchCameraTerminatedToForceFPSCamera) );
	if(shouldFallBackForDirectorBlendCatchUpOrSwitch)
	{
		return false;
	}

	return true;
}

void camGameplayDirector::RemapInputsForFirstPersonShooter()
{
	// TODO: remove if no longer required
#if 0
	if( IsFirstPersonModeEnabled() != m_WasFirstPersonModeEnabledPreviousFrame)
	{
		if( !m_WasFirstPersonModeEnabledPreviousFrame && IsFirstPersonModeEnabled() )
		{
			// TODO: figure out how to do proper remapping.
			CControl& rControl = CControlMgr::GetMainPlayerControl(false);
			rage::ioMapper& mapper = rControl.GetMapper( ON_FOOT);
			rage::ioSource originalSource;
			rage::ioSource newSource;
			originalSource.m_Device = IOMS_PAD_DIGITALBUTTON;
			newSource.m_Device = IOMS_PAD_DIGITALBUTTON;
			originalSource.m_DeviceIndex = ioSource::IOMD_DEFAULT;
			newSource.m_DeviceIndex = ioSource::IOMD_DEFAULT;

			rage::ioValue& inputJump = const_cast<rage::ioValue&>(rControl.GetPedJump());
			originalSource.m_Parameter = rage::ioPad::RLEFT;
			newSource.m_Parameter = rage::ioPad::RDOWN;
			mapper.Change(originalSource, newSource, inputJump);

		#if __BANK
			if( g_FirstPersonInputSchemeOne )
		#endif
			{
				rage::ioValue& inputSprint = const_cast<rage::ioValue&>(rControl.GetValue(INPUT_SPRINT));
				originalSource.m_Parameter = rage::ioPad::RDOWN;
				newSource.m_Parameter = rage::ioPad::R1;
				mapper.Change(originalSource, newSource, inputSprint);

				rage::ioValue& inputCover = const_cast<rage::ioValue&>(rControl.GetPedCover());
				originalSource.m_Parameter = rage::ioPad::R1;
				newSource.m_Parameter = rage::ioPad::RLEFT;
				mapper.Change(originalSource, newSource, inputCover);

				rage::ioValue& inputDive = const_cast<rage::ioValue&>(rControl.GetValue(INPUT_DIVE));
				originalSource.m_Parameter = rage::ioPad::R1;
				newSource.m_Parameter = rage::ioPad::RDOWN;
				mapper.Change(originalSource, newSource, inputDive);
			}
		#if __BANK
			else
			{
				rage::ioValue& inputSprint = const_cast<rage::ioValue&>(rControl.GetValue(INPUT_SPRINT));
				originalSource.m_Parameter = rage::ioMapperParameter::RDOWN;
				newSource.m_Parameter = rage::ioMapperParameter::L3;
				mapper.Change(originalSource, newSource, inputSprint);

				rage::ioValue& inputDuck = const_cast<rage::ioValue&>(rControl.GetPedDuck());
				originalSource.m_Parameter = rage::ioMapperParameter::L3;
				newSource.m_Parameter = rage::ioMapperParameter::RLEFT;
				mapper.Change(originalSource, newSource, inputDuck);

				rage::ioValue& inputDive = const_cast<rage::ioValue&>(rControl.GetValue(INPUT_DIVE));
				originalSource.m_Parameter = rage::ioPad::RDOWN;
				newSource.m_Parameter = rage::ioPad::R1;
				mapper.Change(originalSource, newSource, inputDive);
			}
		#endif
		}
		else
		{
			// To be safe, restore controls.
			CControlMgr::ReInitControls();
		}
	}

	bool bIsUsingFirstPersonControls = ShouldUse1stPersonControls();
	if(bIsUsingFirstPersonControls != m_bIsUsingFirstPersonControls)
	{
		CControlMgr::ReInitControls();
	}

	m_bIsUsingFirstPersonControls = bIsUsingFirstPersonControls;
#else
	m_bIsUsingFirstPersonControls = ShouldUse1stPersonControls();
#endif
}

void camGameplayDirector::DisableFirstPersonThisUpdate(const CPed* ped, bool fromScript)
{
	const CPed* followPed = GetFollowPed();

	if ((ped == NULL) || (ped == followPed))
	{
		m_DisableFirstPersonThisUpdate = true;
		m_DisableFirstPersonThisUpdateFromScript = fromScript;
	}
}

bool camGameplayDirector::ShouldUse1stPersonControls() const
{
	return	( GetActiveViewMode(GetActiveViewModeContext()) == (s32)camControlHelperMetadataViewMode::FIRST_PERSON );
}

bool camGameplayDirector::ComputeShouldUseFirstPersonDriveBy() const
{
	TUNE_GROUP_BOOL(FIRST_PERSON_STEERING, bFirstPersonDriveBy, true);
	if(!bFirstPersonDriveBy)
	{
		return false;
	}

	if(!m_FollowPed)
	{
		return false;
	}

	if(m_FollowPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		return true;
	}

	const CTaskMotionBase* currentMotionTask = m_FollowPed->GetCurrentMotionTask();
	if(currentMotionTask)
	{
		switch(currentMotionTask->GetTaskType())
		{
		case CTaskTypes::TASK_MOTION_IN_AUTOMOBILE:
		case CTaskTypes::TASK_MOTION_ON_BICYCLE:
		case CTaskTypes::TASK_MOTION_ON_BICYCLE_CONTROLLER:
			return true;
		default:
			break;
		}
	}

	return false;
}

u32 camGameplayDirector::ComputeFirstPersonShooterCameraRef() const
{
	if (IsTableGamesCameraRunning())
	{
		return ATSTRINGHASH("CASINO_TABLE_GAMES_FPS_CAMERA", 0x2DFF9A0);
	}

	return m_Metadata.m_FirstPersonShooterCameraRef;
}

bool camGameplayDirector::IsDelayTurretEntryForFpsCamera() const
{
	if( m_TurretAligned && m_VehicleEntryExitState == INSIDE_VEHICLE && 
		m_ActiveCamera && m_ActiveCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()) )
	{
		camFirstPersonShooterCamera* pFpsCamera = static_cast<camFirstPersonShooterCamera*>(m_ActiveCamera.Get());
		return (pFpsCamera->IsEnteringVehicle());
	}

	return false;
}

void camGameplayDirector::UpdateBonnetCameraViewToggle(bool bToggledForceBonnetCameraView, bool bDisableForceBonnetCameraView)
{
	if(bToggledForceBonnetCameraView)
	{
		m_ForceFirstPersonBonnetCamera = !m_ForceFirstPersonBonnetCamera;

		// When toggling view, disable look behind input so we don't trigger that.
		// (assumes look behind is on same controller button as the bonnet camera toggle)
		////camBaseCamera* pDominantCamera = const_cast<camBaseCamera*>(camInterface::GetDominantRenderedCamera());
		////if(pDominantCamera && pDominantCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
		////{
		////	camCinematicMountedCamera* pMountedCamera = static_cast<camCinematicMountedCamera*>(pDominantCamera);
		////	camControlHelper* pMountedControlHelper   = const_cast<camControlHelper*>(pMountedCamera->GetControlHelper());
		////	if(pMountedControlHelper)
		////	{
		////		pMountedControlHelper->IgnoreLookBehindInputThisUpdate();
		////	}
		////}
	}
	else if (bDisableForceBonnetCameraView)
	{
		m_ForceFirstPersonBonnetCamera = false;
	}
}
#endif // FPS_MODE_SUPPORTED

#if __BANK
void camGameplayDirector::AddWidgetsForInstance()
{
	if(m_WidgetGroup == NULL)
	{
		bkBank* bank = BANKMGR.FindBank("Camera");
		if(bank != NULL)
		{
			m_WidgetGroup = bank->PushGroup("Gameplay director", false);
			{
			#if FPS_MODE_SUPPORTED
				////bank->AddToggle("Enable PROTOTYPE first person camera", &m_IsFirstPersonModeEnabled, rage::NullCallback, "Allows first person as a view mode option via Select button");
				////bank->AddToggle("Use third person camera for first person mode", &m_ShouldUseThirdPersonCameraForFirstPersonMode);
				bank->AddToggle("Allow first person to fallback to thirdperson", &m_AllowThirdPersonCameraFallbacks, rage::NullCallback, "For player getting out of upside-down vehicle");
				bank->AddToggle("Force first person to fallback to thirdperson for ragdoll and get up", &m_ForceThirdPersonCameraForRagdollAndGetUps);
				bank->AddToggle("Force first person to fallback to thirdperson when input is disabled", &m_ForceThirdPersonCameraWhenInputDisabled);
				bank->AddToggle("Use Animated Data for First Person Camera", &m_EnableFirstPersonUseAnimatedData);
				bank->AddToggle("Apply first person Animated Data relative to MOVER", &m_FirstPersonAnimatedDataRelativeToMover);
				bank->AddToggle("Apply first person relative offset in Camera space", &m_bApplyRelativeOffsetInCameraSpace);
				bank->AddToggle("First person steer sprint with left stick", &m_IsFirstPersonShooterLeftStickControl);
				bank->AddSeparator();
			#endif // FPS_MODE_SUPPORTED

				bank->AddButton("Set relative orbit orientation", datCallback(MFA(camGameplayDirector::DebugSetRelativeOrbitOrientation), this));
				bank->AddSlider("Relative orbit heading (deg)", &m_DebugOverriddenRelativeOrbitHeadingDegrees, -180.0f, 180.0f, 0.1f);
				bank->AddSlider("Relative orbit pitch (deg)", &m_DebugOverriddenRelativeOrbitPitchDegrees, -90.0f, 90.0f, 0.1f);
				bank->AddButton("Flip relative orbit orientation", datCallback(MFA(camGameplayDirector::DebugFlipRelativeOrbitOrientation), this));
				bank->AddButton("Spin relative orbit orientation", datCallback(MFA(camGameplayDirector::DebugSpinRelativeOrbitOrientation), this));

				bank->PushGroup("Hinting", false);
				{
					bank->AddVector("Hint offset", &m_DebugHintOffset, -100.0f, 100.0f, 0.01f);
					bank->AddToggle("Is offset relative to target", &m_DebugIsHintOffsetRelative);
					bank->AddSlider("Attack duration (ms)", &m_DebugHintAttackDuration, 0, 60000, 1);
					bank->AddSlider("Sustain duration (ms)", &m_DebugHintSustainDuration, -1, 60000, 1);
					bank->AddSlider("Release duration (ms)", &m_DebugHintReleaseDuration, 0, 60000, 1);
					bank->AddButton("Hint towards picker entity", datCallback(MFA(camGameplayDirector::DebugHintTowardsPickerSelection), this));
					bank->AddButton("Stop hinting", datCallback(MFA(camGameplayDirector::DebugStopHinting), this));
					bank->AddButton("Stop hinting immediately", datCallback(MFA(camGameplayDirector::DebugStopHintingImmediately), this));
				}
				bank->PopGroup(); //Hinting.

				bank->AddSlider("Catch-up distance", &m_DebugCatchUpDistance, 0.0f, 100.0f, 0.1f);
				bank->AddButton("Catch-up from rendered frame", datCallback(MFA(camGameplayDirector::DebugCatchUpFromRenderedFrame), this));
				bank->AddButton("Catch-up from gameplay frame", datCallback(MFA(camGameplayDirector::DebugCatchUpFromGameplayFrame), this));
				bank->AddButton("Abort catch-up", datCallback(MFA(camGameplayDirector::DebugAbortCatchUp), this));

				bank->PushGroup("Follow-ped camera override", false);
				{
					bank->AddToggle("Should override", &m_DebugShouldOverrideFollowPedCamera);
					bank->AddText("Overridden camera name", m_DebugOverriddenFollowPedCameraName, g_MaxDebugCameraNameLength);
					bank->AddSlider("Interpolation duration", &m_DebugOverriddenFollowPedCameraInterpolationDuration, -1, 10000, 100);
				}
				bank->PopGroup(); //Follow-ped camera override.

				bank->PushGroup("Follow-vehicle camera override", false);
				{
					bank->AddToggle("Should override", &m_DebugShouldOverrideFollowVehicleCamera);
					bank->AddText("Overridden camera name", m_DebugOverriddenFollowVehicleCameraName, g_MaxDebugCameraNameLength);
					bank->AddSlider("Interpolation duration", &m_DebugOverriddenFollowVehicleCameraInterpolationDuration, -1, 10000, 100);
					bank->AddToggle("Force using stunt settings", &m_DebugForceUsingStuntSettings);
				}
				bank->PopGroup(); //Follow-vehicle camera override.

				bank->AddToggle("Force tight space custom framing", &m_DebugForceUsingTightSpaceCustomFraming);

				bank->AddToggle("Report peds as outside vehicles", &m_DebugShouldReportOutsideVehicle);

				bank->AddToggle("Log tight-space settings", &m_DebugShouldLogTightSpaceSettings);

				bank->AddToggle("Log active view-mode context", &m_DebugShouldLogActiveViewModeContext);
			}
			bank->PopGroup(); //Gameplay director.
		}
	}
}

void camGameplayDirector::DebugSetRelativeOrbitOrientation()
{
	const float relativeHeading = m_DebugOverriddenRelativeOrbitHeadingDegrees * DtoR;
	SetUseScriptHeading(relativeHeading);

	const float relativePitch = m_DebugOverriddenRelativeOrbitPitchDegrees * DtoR;
	SetUseScriptPitch(relativePitch, 1.0f);
}

void camGameplayDirector::DebugFlipRelativeOrbitOrientation()
{
	if (m_DebugOverriddenRelativeOrbitHeadingDegrees <= 0.0f)
	{
		m_DebugOverriddenRelativeOrbitHeadingDegrees = 180.0f;
	}
	else
	{
		m_DebugOverriddenRelativeOrbitHeadingDegrees = 0.0f;
	}

	DebugSetRelativeOrbitOrientation();
}

void camGameplayDirector::DebugSpinRelativeOrbitOrientation()
{
	m_DebugOverriddenRelativeOrbitHeadingDegrees += 45.0f;

	if (m_DebugOverriddenRelativeOrbitHeadingDegrees > 180.0f)
	{
		m_DebugOverriddenRelativeOrbitHeadingDegrees -= 360.0f;
	}

	DebugSetRelativeOrbitOrientation();
}

void camGameplayDirector::DebugHintTowardsPickerSelection()
{
	const CEntity* pickerEntity = static_cast<const CEntity*>(g_PickerManager.GetSelectedEntity());
	StartHinting(pickerEntity, m_DebugHintOffset, m_DebugIsHintOffsetRelative, m_DebugHintAttackDuration,
		m_DebugHintSustainDuration, m_DebugHintReleaseDuration, 0);
	m_DebugHintActive = IsHintActive();
}

void camGameplayDirector::DebugStopHinting()
{
	StopHinting(false);
}

void camGameplayDirector::DebugStopHintingImmediately()
{
	StopHinting(true);
}

void camGameplayDirector::DebugCatchUpFromRenderedFrame()
{
	const camFrame& renderedFrame = camInterface::GetFrame();
	CatchUpFromFrame(renderedFrame, m_DebugCatchUpDistance);

	//Deactivate the debug free camera and debug pad, if we used this to generate the source frame for catch-up.
	camInterface::GetDebugDirector().DeactivateFreeCam();
	CControlMgr::SetDebugPadOn(false);
}

void camGameplayDirector::DebugCatchUpFromGameplayFrame()
{
	const camFrame& gameplayFrame = GetFrame();
	CatchUpFromFrame(gameplayFrame, m_DebugCatchUpDistance);
}

void camGameplayDirector::DebugAbortCatchUp()
{
	camThirdPersonCamera* thirdPersonCamera = GetThirdPersonCamera();
	if(thirdPersonCamera)
	{
		thirdPersonCamera->AbortCatchUp();
	}
}

void camGameplayDirector::DebugGetCameras(atArray<camBaseCamera*> &cameraList) const
{
	if(m_PreviousCamera)
	{
		cameraList.PushAndGrow(m_PreviousCamera);
	}

	if(m_ActiveCamera)
	{
		cameraList.PushAndGrow(m_ActiveCamera);
	}
}

void camGameplayDirector::DebugSetOverrideMotionBlurThisUpdate(const float blurStrength)
{
	m_DebugOverrideMotionBlurStrengthThisUpdate = blurStrength;
}

void camGameplayDirector::DebugSetOverrideDofThisUpdate(const float nearDof, const float farDof)
{
	m_DebugOverrideNearDofThisUpdate = nearDof;
	m_DebugOverrideFarDofThisUpdate = farDof;
	m_DebugOverridingDofThisUpdate = true;
}
#endif // __BANK
