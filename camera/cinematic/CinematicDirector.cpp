//
// camera/cinematic/CinematicDirector.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/CinematicDirector.h"

#include "fwsys/timer.h"

#include "camera/CamInterface.h"
#include "camera/cinematic/camera/animated/CinematicAnimatedCamera.h"
#include "camera/cinematic/camera/tracking/CinematicCamManCamera.h"
#include "camera/cinematic/camera/tracking/CinematicHeliChaseCamera.h"
#include "camera/cinematic/camera/tracking/CinematicIdleCamera.h"
#include "camera/cinematic/camera/tracking/CinematicFirstPersonIdleCamera.h"
#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"
#include "camera/cinematic/camera/mounted/CinematicMountedPartCamera.h"
#include "camera/cinematic/camera/tracking/CinematicPedCloseUpCamera.h"
#include "camera/cinematic/camera/tracking/CinematicPositionCamera.h"
#include "camera/cinematic/camera/tracking/CinematicStuntCamera.h"
#include "camera/cinematic/camera/tracking/CinematicTrainTrackCamera.h"
#include "camera/cinematic/camera/tracking/CinematicWaterCrashCamera.h"
#include "camera/cinematic/camera/orbit/CinematicVehicleOrbitCamera.h"
#include "camera/cinematic/context/CinematicInVehicleContext.h"
#include "camera/cinematic/context/CinematicInVehicleWantedContext.h"
#include "camera/cinematic/context/CinematicInVehicleOverriddenFirstPersonContext.h"
#include "camera/cinematic/context/CinematicInVehicleFirstPersonContext.h"
#include "camera/cinematic/context/CinematicOnFootIdleContext.h"
#include "camera/cinematic/context/CinematicWaterCrashContext.h"
#include "camera/cinematic/shot/CinematicInVehicleShot.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/aim/ThirdPersonPedAssistedAimCamera.h"
#include "camera/gameplay/follow/FollowVehicleCamera.h"
#include "camera/helpers/ControlHelper.h"
#include "camera/helpers/BaseFrameShaker.h"
#include "camera/replay/ReplayDirector.h"
#include "camera/scripted/ScriptDirector.h"
#include "camera/system/CameraFactory.h"
#include "camera/system/CameraMetadata.h"
#include "frontend/MobilePhone.h"
#include "game/ModelIndices.h"
#include "network/NetworkInterface.h"
#include "peds/PedIntelligence.h"
#include "peds/QueriableInterface.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "system/Control.h"
#include "task/Animation/TaskAnims.h"
#include "task/Animation/TaskScriptedAnimation.h"
#include "task/Weapons/Gun/TaskGun.h"
#include "task/combat/TaskCombatMelee.h"
#include "task/combat/Subtasks/TaskVehicleCombat.h"
#include "task/movement/TaskParachute.h"
#include "Task/Vehicle/TaskVehicleWeapon.h"
#include "tools/SectorTools.h"
#include "vehicles/Metadata/VehicleSeatInfo.h"
#include "vehicles/vehicle.h"
#include "vehicles/train.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camCinematicDirector,0xD9B9A56E)

//NOTE: We wish to avoid overriding the camera metadata in the title update, if at all possible. So rather than change
//camCinematicDirectorMetadata::m_DurationBeforeTriggeringNetworkCinematicVehicleCam, we are applying a scaling factor to
//the minimum idle duration in code.
const u32 g_DurationScalingForNetworkCinematicVehicleIdleMode = 2;

#if __BANK
#define MAX_NUMBER_BONNET_CAMERAS (35)

const char* BonnetCameraTypes[MAX_NUMBER_BONNET_CAMERAS] = {"Use Metadata Defined Camera",  
	"DEFAULT_VEHICLE_BONNET_CAMERA",
	"VEHICLE_BONNET_CAMERA_HIGH",
	"VEHICLE_BONNET_CAMERA_IGNUS",
	"VEHICLE_BONNET_CAMERA_LOW",
	"VEHICLE_BONNET_CAMERA_LOW_LOW",
	"VEHICLE_BONNET_CAMERA_LOW_CALICO",
	"VEHICLE_BONNET_CAMERA_MID",
	"VEHICLE_BONNET_CAMERA_MID_EXTRA_HIGH",
	"VEHICLE_BONNET_CAMERA_MID_HIGH",
	"VEHICLE_BONNET_CAMERA_MID_NEAR",
	"VEHICLE_BONNET_CAMERA_MONROE",
	"VEHICLE_BONNET_CAMERA_NEAR",
	"VEHICLE_BONNET_CAMERA_NEAR_EXTRA_HIGH",
	"VEHICLE_BONNET_CAMERA_NEAR_HIGH",
	"VEHICLE_BONNET_CAMERA_NEAR_LOW",
	"VEHICLE_BONNET_CAMERA_RAPIDGT",
	"VEHICLE_BONNET_CAMERA_STANDARD",
	"VEHICLE_BONNET_CAMERA_STANDARD_HIGH",
	"VEHICLE_BONNET_CAMERA_STANDARD_LONG",
	"VEHICLE_BONNET_CAMERA_STANDARD_LONG_EXTRA_LOW",
	"VEHICLE_BONNET_CAMERA_STANDARD_LONG_NO_PITCH",
	"VEHICLE_BONNET_CAMERA_STANDARD_LONG_REBLA",
	"VEHICLE_BONNET_CAMERA_STANDARD_LONG_DEVIANT",
	"VEHICLE_BONNET_CAMERA_STOCKADE",
	"VEHICLE_BONNET_CAMERA_TANK",
	"VEHICLE_BONNET_CAMERA_SCARAB",
	"VEHICLE_BONNET_CAMERA_SPEEDO4",
	"VEHICLE_BONNET_CAMERA_ZR3803",
	"VEHICLE_BONNET_CAMERA_FORMULA",
	"VEHICLE_BONNET_CAMERA_FORMULA2",
	"VEHICLE_BONNET_CAMERA_OPENWHEEL1",
	"BOAT_BONNET_CAMERA",
	"BOAT_LONGFIN_BONNET_CAMERA",
	"VEHICLE_SUB_KOSATKA_BONNET_CAMERA",
	};
#endif

bool camCinematicDirector::IsSpectatingCopsAndCrooksEscape()
{
	TUNE_GROUP_BOOL(CAMERA_CINEMATIC, bForceSpectatingCopsAndCroocksEscape, false);
	return bForceSpectatingCopsAndCroocksEscape || (NetworkInterface::IsInSpectatorMode() && CNetwork::GetNetworkGameModeState() == GameModeState_CnC_Escape);
}

bool camCinematicDirector::ms_UseBonnetCamera = false;

PARAM(onFootCinematic, "[camCinematicDirector] Allow on foot cinematic");

camCinematicDirector::camCinematicDirector(const camBaseObjectMetadata& metadata)
: camBaseDirector(metadata)
, m_Metadata(static_cast<const camCinematicDirectorMetadata&>(metadata))
, m_SlowMoEnvelope(NULL)
, m_TargetEntity(NULL)
, m_LastLookAtEntity(NULL)
, m_FollowPedPositionOnPreviousUpdate(VEC3_ZERO)
, m_PreviousKillShotStartTime(fwTimer::GetTimeInMilliseconds())
, m_PreviousReactionShotStartTime(fwTimer::GetTimeInMilliseconds())
, m_TimeCinematicButtonIsDown(0)
, m_currentContext(-1)
, m_TimeCinematicButtonInitalPressed(0)
, m_InputSelectedShotPreference(SELECTION_INPUT_NONE)
, m_TimeCinematicButtonWasHeldPressed(0)
, m_SlowMotionBlendOutDurationOnShotTermination(0)
, m_VehiclePovCameraWasTerminatedForWaterClippingTime(0)
#if __BANK
, m_DebugBonnetCameraIndex(0)
#endif
, m_SlowMotionScaling(1.0f)
, m_desiredSlowMotionScaling(1.0f)
, m_CachedHeadingForVehiclePovCameraDuringWaterClipping(0.0f)
, m_CachedPitchForVehiclePovCameraDuringWaterClipping(0.0f)
, m_IsCinematicButtonDisabledByScript(false)
, m_IsFirstPersonInVehicleDisabledThisUpdate(false)
, m_IsVehicleIdleModeDisabledThisUpdate(false)
, m_HasCinematicContextChangedThisUpdate(false)
, m_CanActivateMeleeCam(true)
, m_ShouldSlowMotion(false)				
, m_HasControlStickBeenReset(false)	
, m_IsCinematicCameraModeLocked(false)	
, m_ShouldUsePreferedShot(false)
, m_ShouldChangeShot(false)
, m_IsSlowMoActive(false)
, m_IsCinematicInputActive(false)
, m_HaveOverriddenSlowMoShot(false)
, m_LatchedState(false)
, m_IsCinematicButtonDown(false)
, m_HaveCancelledOverridenShot(false)
, m_IsUsingQuickToggleThisFrame(false)
, m_HaveJustChangedToToggleContext(false)
, m_IsRenderedCamInVehicle(false)
, m_IsRenderedCamPovTurret(false)
, m_IsRenderedCamFirstPerson(false)
, m_ShouldRespectTheLineOfAction(false)
, m_ShouldScriptOverrideCinematicInputThisUpdate(false)
, m_IsCinematicInputActiveForScript(false)
, m_CanActivateNewsChannelContextThisUpdate(false)
, m_CanActivateMissionCreatorFailContext(false)
, m_WasCinematicButtonAciveAfterScriptStoppedBlocking(false)
, m_HaveStoredPressedTime(false)
, m_CanBlendOutSlowMotionOnShotTermination(false)
, m_IsScriptDisablingSlowMoThisUpdate(false)
, m_HaveUpdatedACinematicCamera(false)
, m_DisableBonnetCameraForScriptedAnimTask(false)
, m_IsMenuPreferenceForBonnetCameraIgnoredThisUpdate(false)
, m_IsScriptDisablingWaterClippingTestThisUpdate(false)
, m_VehiclePovCameraWasTerminatedForWaterClipping(false)
#if __BANK
,m_WasCinematicInputBlockedByScriptForDebugOutput(false)
#endif
{	
	m_Random.SetFullSeed((u64)sysTimer::GetTicks());

	const int numContexts = m_Metadata.m_CinematicContexts.GetCount();
	for(int i=0; i<numContexts; i++)
	{
		if(m_Metadata.m_CinematicContexts[i])
		{
			camBaseCinematicContext* pContext = camFactory::CreateObject<camBaseCinematicContext>(*m_Metadata.m_CinematicContexts[i]);
			if(cameraVerifyf(pContext, "Failed to create a context (name: %s, hash: %u)",
				SAFE_CSTRING(m_Metadata.m_CinematicContexts[i]->m_Name.GetCStr()), m_Metadata.m_CinematicContexts[i]->m_Name.GetHash()))
			{
				m_Contexts.Grow() = pContext;
			}
		}
	}
	
	const camEnvelopeMetadata* envelopeMetadata = camFactory::FindObjectMetadata<camEnvelopeMetadata>(m_Metadata.m_SlowMoEnvelopeRef.GetHash());
		if(envelopeMetadata)
		{
			m_SlowMoEnvelope = camFactory::CreateObject<camEnvelope>(*envelopeMetadata);
			cameraAssertf(m_SlowMoEnvelope, "A cinematic context (name: %s, hash: %u) failed to create an envelope (name: %s, hash: %u)",
				GetName(), GetNameHash(), SAFE_CSTRING(envelopeMetadata->m_Name.GetCStr()), envelopeMetadata->m_Name.GetHash());
		}
}

camCinematicDirector::~camCinematicDirector()
{
	CleanUpCinematicContext();
}

void camCinematicDirector::RegisterVehicleDamage(const CVehicle* pVehicle, const Vector3& impactDirection, float damage)
{
	if (!m_Metadata.m_VehicleImpactHeadingShakeRef || !m_Metadata.m_VehicleImpactPitchShakeRef)
	{
		return;
	}

	if (camInterface::IsFadedOut() || camInterface::GetDominantRenderedDirector() != this)
	{
		return;
	}
	
	if(!m_UpdatedCamera)
	{
		return; 
	}

	if (!m_UpdatedCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
	{
		return;
	}

	const camCinematicMountedCamera* cinematicMountedCamera = static_cast<const camCinematicMountedCamera*>(m_UpdatedCamera.Get());

	if (!cinematicMountedCamera->ShouldShakeOnImpact())
	{
		return;
	}

	//compute the damage relative to the size of the vehicle
	const float relativeDamage = damage * InvertSafe(pVehicle->pHandling->m_fMass, pVehicle->GetInvMass());		

	if (pVehicle &&
		(!pVehicle->InheritsFromPlane() || !const_cast<CVehicle*>(pVehicle)->IsInAir()) &&
		camInterface::GetGameplayDirector().GetFollowVehicle() == pVehicle &&
		relativeDamage > m_Metadata.m_VehicleImpactShakeMinDamage)
	{
		//Apply amplitude scaling based on the relative damage done
		const float amplitude = Min(relativeDamage * (m_Metadata.m_VehicleImpactShakeMaxAmplitude / m_Metadata.m_VehicleImpactShakeMaxDamage),
			m_Metadata.m_VehicleImpactShakeMaxAmplitude);

		Vector3 relativeImpactDirection;
		const Matrix34& worldMatrix = m_UpdatedCamera->GetFrame().GetWorldMatrix();
		worldMatrix.UnTransform3x3(impactDirection, relativeImpactDirection);

		const float heading = camFrame::ComputeHeadingFromFront(relativeImpactDirection);

		const float headingAmplitude = amplitude * -sin(heading);
		const float pitchAmplitude = amplitude * cos(heading);

#if __DEV && 0
		Errorf("Vehicle shake heading amplitude is %f and pitch amplitude is %f for damage %f", headingAmplitude, pitchAmplitude, relativeDamage);
#endif // __DEV

		m_UpdatedCamera->Shake(m_Metadata.m_VehicleImpactHeadingShakeRef, headingAmplitude, 2);
		m_UpdatedCamera->Shake(m_Metadata.m_VehicleImpactPitchShakeRef, pitchAmplitude, 2);
	}
}


//This is for Alex (and decals for rendering bullet holes and stuff on glass.)
bool camCinematicDirector::IsRenderedCameraInsideVehicle() const
{
	return m_IsRenderedCamInVehicle; 	
}

bool camCinematicDirector::IsRenderedCameraVehicleTurret() const
{
	return m_IsRenderedCamPovTurret; 
}

bool camCinematicDirector::IsRenderedCameraFirstPerson() const
{
	return m_IsRenderedCamFirstPerson;
}

bool camCinematicDirector::IsRenderedCameraForcingMotionBlur() const
{	
	return m_UpdatedCamera && (
		(m_UpdatedCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()) && 
		static_cast<camCinematicMountedCamera*>(m_UpdatedCamera.Get())->IsCameraForcingMotionBlur()) ||

		(m_UpdatedCamera->GetIsClassId(camCinematicMountedPartCamera::GetStaticClassId()) && 
		static_cast<camCinematicMountedPartCamera*>(m_UpdatedCamera.Get())->IsCameraForcingMotionBlur())
	);
}

void camCinematicDirector::UpdateRenderedCameraVehicleFlags()
{
	m_IsRenderedCamInVehicle		= false;
	m_IsRenderedCamPovTurret		= false;
	m_IsRenderedCamFirstPerson		= false;
	if( m_UpdatedCamera && m_UpdatedCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()) )
	{
		const camCinematicMountedCamera* pMountedCamera = static_cast<const camCinematicMountedCamera*>(m_UpdatedCamera.Get());
		m_IsRenderedCamInVehicle	= pMountedCamera->IsCameraInsideVehicle();
		m_IsRenderedCamPovTurret	= pMountedCamera->IsVehicleTurretCamera();
		m_IsRenderedCamFirstPerson	= pMountedCamera->IsFirstPersonCamera();
	}
}

bool camCinematicDirector::ShouldDisplayReticule() const
{
	bool shouldDisplayReticule = false;

	if( m_RenderedCamera && m_RenderedCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
	{
		shouldDisplayReticule = static_cast<camCinematicMountedCamera*>(m_RenderedCamera.Get())->ShouldDisplayReticule();
	}

	return shouldDisplayReticule;
}

void camCinematicDirector::InvalidateCinematicVehicleIdleMode()
{
	//Invalidate the idle mode of the in-vehicle cinematic context.
	for(int i = 0; i < m_Contexts.GetCount(); i++)
	{
		camBaseCinematicContext* context = m_Contexts[i];
		if(context && context->GetIsClassId(camCinematicInVehicleContext::GetStaticClassId()))
		{
			static_cast<camCinematicInVehicleContext*>(context)->InvalidateIdleMode();
		}
	}
}

void camCinematicDirector::InvalidateIdleCamera()
{
	//Invalidate any idle contexts.
	for(int i = 0; i < m_Contexts.GetCount(); i++)
	{
		camBaseCinematicContext* context = m_Contexts[i];
		if(context && context->GetIsClassId(camCinematicOnFootIdleContext::GetStaticClassId()))
		{
			static_cast<camCinematicOnFootIdleContext*>(context)->Invalidate();
		}
	}
}

u32 camCinematicDirector::GetTimeBeforeCinematicVehicleIdleCam() const
{
	const u32 duration = (g_DurationScalingForNetworkCinematicVehicleIdleMode * m_Metadata.m_DurationBeforeTriggeringNetworkCinematicVehicleCam);

	return duration;
}

bool camCinematicDirector::IsRenderingCinematicMountedCamera(bool shouldTestValidity) const
{
	if(IsAnyCinematicContextActive())
	{
		if(m_Contexts[m_currentContext]->GetClassId()== camCinematicInVehicleFirstPersonContext::GetStaticClassId() || 
			m_Contexts[m_currentContext]->GetClassId()== camCinematicInVehicleOverriddenFirstPersonContext::GetStaticClassId() )
		{
			if(!shouldTestValidity || m_Contexts[m_currentContext]->IsValid())
			{
				const camBaseCamera* renderedCamera = camInterface::GetDominantRenderedCamera();
				if(renderedCamera && (renderedCamera->GetClassId() == camCinematicMountedCamera::GetStaticClassId()))
				{
					return true;
				}
			}
		}
	}
	return false;
}

bool camCinematicDirector::IsRenderingCinematicPointOfViewCamera(bool shouldTestValidity) const
{
	if(IsAnyCinematicContextActive())
	{
		if(m_Contexts[m_currentContext]->GetClassId()== camCinematicInVehicleFirstPersonContext::GetStaticClassId() || 
			m_Contexts[m_currentContext]->GetClassId()== camCinematicInVehicleOverriddenFirstPersonContext::GetStaticClassId() )
		{
			if(!shouldTestValidity || m_Contexts[m_currentContext]->IsValid())
			{
				const camBaseCamera* renderedCamera = camInterface::GetDominantRenderedCamera();
				if(renderedCamera && (renderedCamera->GetClassId() == camCinematicMountedCamera::GetStaticClassId()))
				{
					const bool c_IsFirstPersonCamera = static_cast<const camCinematicMountedCamera*>(renderedCamera)->IsFirstPersonCamera();
					return c_IsFirstPersonCamera;
				}
			}
		}
	}
	return false;
}

bool camCinematicDirector::IsRenderingCinematicCameraInsideVehicle(bool shouldTestValidity) const
{
	if(IsAnyCinematicContextActive())
	{
		if(m_Contexts[m_currentContext]->GetClassId()== camCinematicInVehicleFirstPersonContext::GetStaticClassId() || 
			m_Contexts[m_currentContext]->GetClassId()== camCinematicInVehicleOverriddenFirstPersonContext::GetStaticClassId() )
		{
			if(!shouldTestValidity || m_Contexts[m_currentContext]->IsValid())
			{
				const camBaseCamera* renderedCamera = camInterface::GetDominantRenderedCamera();
				if(renderedCamera && (renderedCamera->GetClassId() == camCinematicMountedCamera::GetStaticClassId()))
				{
					const camCinematicMountedCamera* mountedCamera = static_cast<const camCinematicMountedCamera*>(renderedCamera); 
					if(mountedCamera->IsCameraInsideVehicle())
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}

float camCinematicDirector::GetVehicleImpactShakeMinDamage() const
{
    return m_Metadata.m_VehicleImpactShakeMinDamage;
}

camCinematicMountedCamera* camCinematicDirector::GetFirstPersonVehicleCamera() const
{
	if(m_UpdatedCamera && m_UpdatedCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
	{
		camCinematicMountedCamera* firstPersonVehicleCamera = static_cast<camCinematicMountedCamera*>(m_UpdatedCamera.Get());
		if(firstPersonVehicleCamera->IsFirstPersonCamera())
		{
			return firstPersonVehicleCamera;
		}
	}

	return NULL;
}

camCinematicMountedCamera* camCinematicDirector::GetBonnetVehicleCamera() const
{
    if(camCinematicDirector::ms_UseBonnetCamera || camInterface::GetGameplayDirector().IsForcingFirstPersonBonnet())
    {
        if(m_UpdatedCamera && m_UpdatedCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
        {
            camCinematicMountedCamera* bonnetVehicleCamera = static_cast<camCinematicMountedCamera*>(m_UpdatedCamera.Get());
            return bonnetVehicleCamera;
        }
    }

    return nullptr;
}

bool camCinematicDirector::IsRenderingCinematicIdleCamera(bool shouldTestValidity) const
{
	if(IsAnyCinematicContextActive())
	{
		if(m_Contexts[m_currentContext]->GetClassId()== camCinematicOnFootIdleContext::GetStaticClassId())
		{
			if(!shouldTestValidity || m_Contexts[m_currentContext]->IsValid())
			{
				const camBaseCamera* renderedCamera = camInterface::GetDominantRenderedCamera();
				if(renderedCamera)
				{
					if(renderedCamera->GetClassId() == camCinematicIdleCamera::GetStaticClassId())
					{
						return true;
					}

					if(renderedCamera->GetClassId() == camCinematicFirstPersonIdleCamera::GetStaticClassId())
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}

bool camCinematicDirector::IsRenderingCinematicWaterCrash(bool shouldTestValidity) const
{
	if(IsAnyCinematicContextActive())
	{
		if(m_Contexts[m_currentContext]->GetClassId()== camCinematicWaterCrashContext::GetStaticClassId())
		{
			if(!shouldTestValidity || m_Contexts[m_currentContext]->IsValid())
			{
				const camBaseCamera* renderedCamera = camInterface::GetDominantRenderedCamera();
				if(renderedCamera && (renderedCamera->GetClassId() == camCinematicWaterCrashCamera::GetStaticClassId()))
				{
					return true;
				}
			}
		}
	}
	return false;
}

const camBaseCinematicContext* camCinematicDirector::GetCurrentContext() const
{
	if(m_currentContext >= 0 && m_currentContext < m_Contexts.GetCount())
	{
		return m_Contexts[m_currentContext]; 
	}
	return NULL; 
}

camCinematicDirector::eSideOfTheActionLine camCinematicDirector::GetSideOfActionLine(const CEntity* pNewLookAtEntity) const
{
	if(m_ShouldRespectTheLineOfAction)
	{
		if(m_LastLookAtEntity == NULL || pNewLookAtEntity == NULL ||  pNewLookAtEntity !=m_LastLookAtEntity )
		{
			return ACTION_LINE_ANY; 
		}
		else
		{
			Vector3	LookAtForward = VEC3V_TO_VECTOR3(m_LastLookAtEntity->GetTransform().GetForward());

			//compute the vector from the camera pos to parent
			Vector3 vLastToLookAt = VEC3V_TO_VECTOR3(m_LastLookAtEntity->GetTransform().GetPosition()) - m_LastCameraPosition; 

			vLastToLookAt.NormalizeSafe(); 

			//compute the last angle
			float LastAngleToLookAtTarget = vLastToLookAt.AngleZ(LookAtForward); 

			if(Abs(LastAngleToLookAtTarget) < (m_Metadata.m_MinAngleToRespectTheLineOfAction *DtoR) || Abs(LastAngleToLookAtTarget) > (PI - (m_Metadata.m_MinAngleToRespectTheLineOfAction*DtoR)) )
			{
				return ACTION_LINE_ANY;
			}
			else if(LastAngleToLookAtTarget > 0.0f)
			{
				return ACTION_LINE_LEFT; 
			}
			else
			{
				return ACTION_LINE_RIGHT; 
			}
		}
	}
	else
	{
		return ACTION_LINE_ANY;
	}
}

bool camCinematicDirector::IsCameraPositionValidWithRespectToTheLine( const CEntity* pNewLookAtEntity, const Vector3& desiredCamPosition) const 
{	
	if(GetSideOfActionLine(pNewLookAtEntity) == ACTION_LINE_ANY)
	{
		return true; 
	}

	if(pNewLookAtEntity)
	{
		Vector3	LookAtForward = VEC3V_TO_VECTOR3(pNewLookAtEntity->GetTransform().GetForward());
		
		//compute the vector from the camera pos to parent
		Vector3 vLastToLookAt = VEC3V_TO_VECTOR3(pNewLookAtEntity->GetTransform().GetPosition()) - m_LastCameraPosition; 
		
		vLastToLookAt.NormalizeSafe(); 
		
		//compute the last angle
		float LastAngleToLookAtTarget = vLastToLookAt.AngleZ(LookAtForward); 
		
		if(Abs(LastAngleToLookAtTarget) < (m_Metadata.m_MinAngleToRespectTheLineOfAction *DtoR) || Abs(LastAngleToLookAtTarget) > (PI - (m_Metadata.m_MinAngleToRespectTheLineOfAction*DtoR)) )
		{
			return true; 
		}

		//compute the new desired heading
		Vector3 vDesiredLookAt = VEC3V_TO_VECTOR3(pNewLookAtEntity->GetTransform().GetPosition()) - desiredCamPosition; 
		
		vDesiredLookAt.NormalizeSafe(); 

		float DesiredAngleToLookAtTarget = vDesiredLookAt.AngleZ(LookAtForward); 
		
		if(Abs(DesiredAngleToLookAtTarget) < (m_Metadata.m_MinAngleToRespectTheLineOfAction *DtoR) || Abs(DesiredAngleToLookAtTarget) > (PI - (m_Metadata.m_MinAngleToRespectTheLineOfAction*DtoR)) )
		{
			return true; 
		}

		if (Sign(LastAngleToLookAtTarget) == Sign(DesiredAngleToLookAtTarget))
		{
			return true;
		}
		else
		{
			return false; 
		}
	}

	return true; 
}

bool camCinematicDirector::Update()
{
	bool hasSucceeded = false;
	m_ActiveRenderState = RS_NOT_RENDERING;
	ms_UseBonnetCamera  = (CPauseMenu::GetMenuPreference( PREF_HOOD_CAMERA ) == TRUE) && !m_IsMenuPreferenceForBonnetCameraIgnoredThisUpdate;

	// HACK: cases where bonnet/hood camera needs to be disabled, using pov camera instead.
	const CPed* followPed = camInterface::FindFollowPed();
	if(followPed && followPed->GetPlayerInfo())
	{
		CVehicle* pVehicle = followPed->GetVehiclePedInside();
		if (pVehicle)
		{
			u32 iSeatIndex = followPed->GetAttachCarSeatIndex();
			const CVehicleModelInfo* pModelInfo = pVehicle->GetVehicleModelInfo();
			const CVehicleSeatInfo* pSeatInfo = pVehicle->IsSeatIndexValid(iSeatIndex) ? pVehicle->GetSeatInfo(iSeatIndex) : NULL;
			const bool bIsFrontSeat = pSeatInfo ? pSeatInfo->GetIsFrontSeat() : false;
			const bool bHasRearActivities = pModelInfo ? pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_REAR_SEAT_ACTIVITIES) : false;
			const bool bShouldForceBonnetCamera = pModelInfo ? pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_FORCE_BONNET_CAMERA_INSTEAD_OF_POV) : false;

			if (bShouldForceBonnetCamera)
			{
				ms_UseBonnetCamera = true;
			}

			if (!bIsFrontSeat && bHasRearActivities)
			{
				ms_UseBonnetCamera = false;
			}

			// No bonnet camera for turret seat of DUNE3
			if (MI_CAR_DUNE3.IsValid() && pVehicle->GetModelIndex() == MI_CAR_DUNE3 && iSeatIndex == 1)
			{
				ms_UseBonnetCamera = false;
			}
			// Same with THRUSTER
			else if (MI_JETPACK_THRUSTER.IsValid() && pVehicle->GetModelIndex() == MI_JETPACK_THRUSTER)
			{
				ms_UseBonnetCamera = false;
			}
			// Same with KHANJALI (force POV so we can have individually-attached cameras per seat)
			else if (MI_TANK_KHANJALI.IsValid() && pVehicle->GetModelIndex() == MI_TANK_KHANJALI)
			{
				ms_UseBonnetCamera = false;
			}

			// Disable based on applied vehicle mods flagged in metadata
			// 13th June 2019 - Only doing this for certain vehicle models as this flag has never been hooked up since launch, so don't want to consider old (potentially invalid) data
			if (MI_CAR_HELLION.IsValid() && pVehicle->GetModelIndex() == MI_CAR_HELLION)
			{
				const CVehicleVariationInstance& vehVariation = pVehicle->GetVariationInstance();
				u8 uVehModIndex = vehVariation.GetModIndex(VMT_EXHAUST);
				if (uVehModIndex != INVALID_MOD)
				{
					if (const CVehicleKit* pVehModKit = vehVariation.GetKit())
					{
						const CVehicleModVisible& vehMod = pVehModKit->GetVisibleMods()[uVehModIndex];
						if (vehMod.IsBonnetCameraDisabled())
						{
							ms_UseBonnetCamera = false;
						}
					}
				}
			}
		}

		const bool isRenderingMobilePhoneCamera	= CPhoneMgr::CamGetState();
		if(pVehicle && CVehicle::IsTaxiModelId(pVehicle->GetModelId()))
		{
			// Copied from naEnvironment::AreWeAPassengerInATaxi()
			// see if the player has a car drive task, otherwise they are just a passenger
			if(followPed->GetAttachCarSeatIndex() > 1 && !followPed->GetPedResetFlag(CPED_RESET_FLAG_IsEnteringOrExitingVehicle))
			{
				// No bonnet camera for player if he is a taxi passenger.
				ms_UseBonnetCamera = false;
			}
		}
		
		if(followPed->GetPedIntelligence())
		{
			const bool bWasBonnetCameraDisabledForScriptedAnimTaskLastUpdate = m_DisableBonnetCameraForScriptedAnimTask;
			m_DisableBonnetCameraForScriptedAnimTask = false;
			CTaskScriptedAnimation* pTaskScriptedAnim = static_cast<CTaskScriptedAnimation*>(followPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SCRIPTED_ANIMATION));
			CTaskSynchronizedScene* pTaskSyncScene    = static_cast<CTaskSynchronizedScene*>(followPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SYNCHRONIZED_SCENE));
			if( pTaskScriptedAnim || pTaskSyncScene )
			{
				// No bonnet camera when running scripted animation task or synchonized scene task.
				// Forces camera inside for prostitute interaction and luxe passenger activities.
				m_DisableBonnetCameraForScriptedAnimTask = true;
				ms_UseBonnetCamera = false;
			}
			else if(bWasBonnetCameraDisabledForScriptedAnimTaskLastUpdate)
			{
				// HACK: Disable for 1 extra frame as it take a frame for prositiute interaction to switch from sync scene to scripted anim task.
				ms_UseBonnetCamera = false;
			}
		}
		
		if(isRenderingMobilePhoneCamera)
		{
			ms_UseBonnetCamera = false;
		}
	}

	UpdateContext(); 
	
	if(m_UpdatedCamera)
	{
		m_LastLookAtEntity = m_UpdatedCamera->GetLookAtTarget(); 
		m_LastCameraPosition = m_UpdatedCamera->GetFrame().GetPosition(); 
		
		if(m_UpdatedCamera->GetIsClassId(camCinematicMountedPartCamera::GetStaticClassId()))	
		{
			m_ShouldRespectTheLineOfAction = false;
		}
		else
		{
			m_ShouldRespectTheLineOfAction = true; 
		}
		m_ActiveRenderState = RS_FULLY_RENDERING;

		UpdateRenderedCameraVehicleFlags();

		m_HaveUpdatedACinematicCamera = true; 
	}
	else
	{
		m_ShouldRespectTheLineOfAction = false; 
		m_HaveUpdatedACinematicCamera = false; 
	}

	UpdateCinematicViewModeSlowMotionControl();

    if(m_VehiclePovCameraWasTerminatedForWaterClipping)
    {
        TUNE_GROUP_INT(VEHICLE_POV_CAMERA, iMinTimeAfterCachingVehiclePovWaterClipping, 500, 0, 60000, 1);
        TUNE_GROUP_INT(VEHICLE_POV_CAMERA, iMaxTimeAfterCachingVehiclePovWaterClipping, 5000, 0, 60000, 1);

        camCinematicMountedCamera* pVehicleCamera = GetFirstPersonVehicleCamera();

        if(pVehicleCamera)
        {
            const u32 deltaTime = fwTimer::GetCamTimeInMilliseconds() - m_VehiclePovCameraWasTerminatedForWaterClippingTime;
            if(deltaTime > (u32)iMinTimeAfterCachingVehiclePovWaterClipping)
            {
                if(deltaTime <= (u32)iMaxTimeAfterCachingVehiclePovWaterClipping)
                {
                    pVehicleCamera->SetUseScriptHeading(m_CachedHeadingForVehiclePovCameraDuringWaterClipping);
                    pVehicleCamera->SetUseScriptPitch(m_CachedPitchForVehiclePovCameraDuringWaterClipping);
                }
                
                m_VehiclePovCameraWasTerminatedForWaterClipping = false;
            }
        }
    }

	//reset single frame flags
	m_IsFirstPersonInVehicleDisabledThisUpdate		= false;
	m_IsVehicleIdleModeDisabledThisUpdate	= false; 
	m_ShouldScriptOverrideCinematicInputThisUpdate = false; 
	m_CanActivateNewsChannelContextThisUpdate = false; 
	m_CanActivateMissionCreatorFailContext = false; 
	m_IsScriptDisablingSlowMoThisUpdate = false; 
    m_IsMenuPreferenceForBonnetCameraIgnoredThisUpdate = false;
	m_IsScriptDisablingWaterClippingTestThisUpdate = false;
	
	return hasSucceeded; 
}

void camCinematicDirector::PostUpdate()
{
	camBaseDirector::PostUpdate();

	//If we're rendering a bonnet camera or a vehicle part camera, apply any explosion shakes that are being managed by the gameplay director.

	const bool isRenderingCinematicMountedCamera		= IsRenderingCinematicMountedCamera();
	const bool isRenderingCinematicVehiclePartCamera	= m_RenderedCamera && m_RenderedCamera->GetIsClassId(camCinematicMountedPartCamera::GetStaticClassId());
	if(!isRenderingCinematicMountedCamera && !isRenderingCinematicVehiclePartCamera)
	{
		return;
	}

	DoSecondUpdateForTurretCamera(m_RenderedCamera.Get(), true);

	const atArray<camGameplayDirector::tExplosionSettings>& explosionSettingsList = camInterface::GetGameplayDirector().GetExplosionSettings();

	const s32 numExplosions = explosionSettingsList.GetCount();
	for(s32 explosionIndex=0; explosionIndex<numExplosions; explosionIndex++)
	{
		const camGameplayDirector::tExplosionSettings& explosionSettings	= explosionSettingsList[explosionIndex];
		const camBaseFrameShaker* frameShaker								= explosionSettings.m_FrameShaker;
		if(frameShaker)
		{
			frameShaker->ShakeFrame(m_PostEffectFrame);
		}
	}
}

void camCinematicDirector::DoSecondUpdateForTurretCamera(camBaseCamera* pCamera, bool bDoPostUpdate)
{
	if(pCamera && pCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
	{
		camCinematicMountedCamera* povTurretCamera = (camCinematicMountedCamera*)pCamera;
		if(povTurretCamera->IsVehicleTurretCamera() && povTurretCamera->GetAttachParent() && povTurretCamera->GetAttachParent()->GetIsTypeVehicle())
		{
			CVehicle* pFollowVehicle = const_cast<CVehicle*>((const CVehicle*)povTurretCamera->GetAttachParent());
			camControlHelper* pControlHelper = const_cast<camControlHelper*>(povTurretCamera->GetControlHelper());

			const CPed* followPed = camInterface::FindFollowPed();
			bool isPlayerPed = followPed && followPed->GetPlayerInfo();
			CPed* followPedNonConst = (isPlayerPed) ? const_cast<CPed*>(followPed) : NULL;

			int seatIndex = -1;
			if(pFollowVehicle && followPedNonConst && pControlHelper)
			{
				// Get the vehicle's seat manager
				const CSeatManager* seatManager = pFollowVehicle->GetSeatManager();
				if(seatManager)
				{
					// Get the ped's seat index	
					seatIndex = seatManager->GetPedsSeatIndex(followPedNonConst);
				}

				// Update the vehicle turret, its skeleton, then update the camera a second time.
				if(seatIndex >= 0 && pFollowVehicle->GetVehicleWeaponMgr())
				{		
					CTurret* turret = pFollowVehicle->GetVehicleWeaponMgr()->GetFirstTurretForSeat(seatIndex);
					if(turret && turret->IsPhysicalTurret())
					{
						CTaskMotionInTurret* taskMotionInTurret = NULL;
						CTaskVehicleMountedWeapon* taskVehicleMountedWeapon = NULL;
						if(followPedNonConst->GetPedIntelligence())
						{
							taskMotionInTurret = static_cast<CTaskMotionInTurret*>(followPedNonConst->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_TURRET));
							taskVehicleMountedWeapon = static_cast<CTaskVehicleMountedWeapon*>(followPedNonConst->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_VEHICLE_MOUNTED_WEAPON));
						}

						////followPedNonConst->ProcessPostCamera();
						if(taskVehicleMountedWeapon)
						{
							taskVehicleMountedWeapon->ProcessPostCamera();
						}
						if(taskMotionInTurret)
						{
							taskMotionInTurret->ProcessPostCamera();
						}

						CTurretPhysical* turretPhysical = static_cast<CTurretPhysical*>(turret);
						turretPhysical->ProcessPreRender( pFollowVehicle );

						if(followPed->IsLocalPlayer())
						{
							////followPedNonConst->ProcessControl();
							followPedNonConst->ProcessControl_Turret();
						}

						if(pFollowVehicle->GetSkeleton())
						{
							pFollowVehicle->GetSkeleton()->Update();
						}

						pControlHelper->SkipControlHelperUpdateThisUpdate();		// disable input, already applied in first update

						// Second camera update
						{
							povTurretCamera->BaseUpdate(m_Frame);

							if(bDoPostUpdate)
							{
								camBaseDirector::PostUpdate();
							}
						}
					}
				}
			}
		}
	}
}

void camCinematicDirector::CacheHeadingAndPitchForVehiclePovCameraDuringWaterClip(const float heading, const float pitch)
{
    //make sure we only cache it once at the beginning of the water clipping, this needs to be checked because the 
    //pov camera will persist for few frames after the water clipping has been detected.
    if(!m_VehiclePovCameraWasTerminatedForWaterClipping)
    {
        m_CachedHeadingForVehiclePovCameraDuringWaterClipping = heading;
        m_CachedPitchForVehiclePovCameraDuringWaterClipping = pitch;
        m_VehiclePovCameraWasTerminatedForWaterClipping = true;
        m_VehiclePovCameraWasTerminatedForWaterClippingTime = fwTimer::GetCamTimeInMilliseconds();
    }
}

bool camCinematicDirector::IsRenderingAnyInVehicleCinematicCamera() const
{
	if(IsAnyCinematicContextActive())
	{
		if(m_Contexts[m_currentContext]->GetClassId()== camCinematicInVehicleContext::GetStaticClassId() ||
			m_Contexts[m_currentContext]->GetClassId()== camCinematicInVehicleWantedContext::GetStaticClassId()  )
		{
			if(m_UpdatedCamera)
			{
				return true; 
			}
		}
	}
	return false; 
}

bool camCinematicDirector::IsRenderingAnyInVehicleFirstPersonCinematicCamera() const
{
	if(IsAnyCinematicContextActive())
	{
		if( m_Contexts[m_currentContext]->GetClassId()== camCinematicInVehicleFirstPersonContext::GetStaticClassId() || 
			m_Contexts[m_currentContext]->GetClassId()== camCinematicInVehicleOverriddenFirstPersonContext::GetStaticClassId() )
		{
			if(m_UpdatedCamera)
			{
				return true; 
			}
		}
	}
	return false; 
}


camBaseCinematicContext* camCinematicDirector::FindContext(u32 classId)
{
	for(int i=0; i < m_Contexts.GetCount(); i++)
	{
		if(m_Contexts[i]->GetClassId() == classId)
		{
			return m_Contexts[i]; 
		}
	}
	return NULL; 
}


void camCinematicDirector::UpdateCinematicViewModeSlowMotionControl()
{	
	CControl* control = camInterface::GetGameplayDirector().GetActiveControl();

	if(IsAnyCinematicContextActive() && m_UpdatedCamera && !NetworkInterface::IsGameInProgress() && m_Contexts[m_currentContext]->m_Metadata.m_CanSelectSlowMotionFromControlInput && !m_IsScriptDisablingSlowMoThisUpdate)	
	{
		m_CurrentSlowMotionBlendOutDurationOnShotTermination = 0; 

		float shotSlowMo = m_Contexts[m_currentContext]->GetShotInitalSlowValue(); 
		
		m_CanBlendOutSlowMotionOnShotTermination = m_Contexts[m_currentContext]->CanBlendOutSlowMotionOnShotTermination(); 
		m_SlowMotionBlendOutDurationOnShotTermination = m_Contexts[m_currentContext]->GetShotSlowMotionBlendOutDuration(); 
		
		float envelopeLevel = 1.0f; 
		float stickOffsetY = 0.0f; 
		
		if(control)
		{
			stickOffsetY = -control->GetVehicleSlowMoUpDown().GetNorm();
		}

		//run if in stunt jump
		if(m_Contexts[m_currentContext]->IsInitialSlowMoValueValid(shotSlowMo) && !m_IsSlowMoActive && !m_HaveOverriddenSlowMoShot && !m_HaveCancelledOverridenShot)
		{
			m_desiredSlowMotionScaling = shotSlowMo; //set the desired slow mo to the initial
			m_IsSlowMoActive = true; 
			m_HaveOverriddenSlowMoShot = true; //overriding the cinematic directors initial slow mo value. 
		}

		
		//from stunt jump slow mo to a new shot reset the slow mo
		if(!m_Contexts[m_currentContext]->IsInitialSlowMoValueValid(shotSlowMo))
		{
			if(m_HaveOverriddenSlowMoShot)
			{
				m_IsSlowMoActive = false; 
			}
			m_HaveOverriddenSlowMoShot = false;
			m_HaveCancelledOverridenShot = false; 
		}

		if(control && control->GetCinematicSlowMotion().IsReleased())
		{
			m_HaveCancelledOverridenShot = true; 
			if(m_IsSlowMoActive)
			{
				m_IsSlowMoActive = false;
			}
			else
			{
				m_desiredSlowMotionScaling =  m_Metadata.m_DefaultSlowMotionScaling;
				m_IsSlowMoActive = true; 

			}
		}
		
		if(m_SlowMoEnvelope)
		{
			m_SlowMoEnvelope->AutoStartStop(m_IsSlowMoActive);
	
			envelopeLevel = m_SlowMoEnvelope->Update(); 
		}
		else
		{
			envelopeLevel = m_IsSlowMoActive ? 1.0f : 0.0f; 
		}	

		//listen to the control input to scale slow mo
		if(envelopeLevel >= 1.0f - SMALL_FLOAT)
		{
			if(m_IsSlowMoActive)
			{
				m_desiredSlowMotionScaling = m_desiredSlowMotionScaling + (stickOffsetY  *m_Metadata.m_SlowMotionScalingDelta); 
				m_desiredSlowMotionScaling = Clamp(m_desiredSlowMotionScaling, 0.0f, 1.0f); 		
			}
		}
		
		m_SlowMotionScaling = RampValueSafe(envelopeLevel, 0.0f, 1.0f, 1.0f, m_desiredSlowMotionScaling); 

		m_SlowMotionScaling = Clamp( m_SlowMotionScaling, m_Metadata.m_MinSlowMotionScaling, 1.0f); 

		m_previousSlowMoValue = m_SlowMotionScaling; 
	}
	else
	{
			m_desiredSlowMotionScaling = 1.0f; 
			m_IsSlowMoActive = false; 
			m_HaveOverriddenSlowMoShot = false;
			m_HaveCancelledOverridenShot = false; 
			if(m_SlowMoEnvelope)
			{
				m_SlowMoEnvelope->Stop(); 
			}
			
			if(m_CanBlendOutSlowMotionOnShotTermination && m_SlowMotionBlendOutDurationOnShotTermination > 0)
			{
				m_CurrentSlowMotionBlendOutDurationOnShotTermination += fwTimer::GetCamTimeStepInMilliseconds();  
				
				float LerpValue = (float)m_CurrentSlowMotionBlendOutDurationOnShotTermination / (float)m_SlowMotionBlendOutDurationOnShotTermination; 
				
				LerpValue = Clamp(LerpValue, 0.0f, 1.0f); 

				m_SlowMotionScaling = Lerp(LerpValue ,m_previousSlowMoValue,1.0f);
				
				if(AreNearlyEqual(m_SlowMotionScaling, 1.0f, SMALL_FLOAT))
				{
					m_SlowMotionScaling = 1.0f;
					m_CanBlendOutSlowMotionOnShotTermination = false; 
					m_CurrentSlowMotionBlendOutDurationOnShotTermination = 0; 
				}	
			}
			else
			{
				m_SlowMotionScaling = 1.0f; 
			}

	}

	const float currentSlowMotionScaling = m_Frame.GetCameraTimeScaleThisUpdate();
	const float compoundSlowMotionScaling = currentSlowMotionScaling * m_SlowMotionScaling;

	m_Frame.SetCameraTimeScaleThisUpdate(compoundSlowMotionScaling);

	//Make exclusive use of INPUT_VEH_SLOWMO_UD whenever slow motion is active.
	if(m_IsSlowMoActive && control)
	{
		control->SetInputExclusive(INPUT_VEH_SLOWMO_UD);
	}
}

void camCinematicDirector::UpdateInputForQuickToggleActivationMode()
{
	bool WasCinematicDownOnPreviousUpdate = m_IsCinematicButtonDown; 

	m_ShouldHoldCinematicShot = false; 

	m_IsCinematicButtonDown = false; 

	CControl* control = camInterface::GetGameplayDirector().GetActiveControl();
	
	if(IsCinematicButtonDisabledByScriptState())
	{
		m_WasCinematicButtonAciveAfterScriptStoppedBlocking = true;	 
		m_HaveStoredPressedTime = false; 
		m_TimeCinematicButtonWasHeldPressed = 0; 

#if __BANK
		if(!m_WasCinematicInputBlockedByScriptForDebugOutput)
		{
			cameraDebugf3("CinematicDirector: Script is blocking Cinematic Input");
			m_WasCinematicInputBlockedByScriptForDebugOutput = true; 
		}
#endif
	}
	else if(control && !control->GetVehicleCinematicCam().IsDown())
	{
#if __BANK
		if(m_WasCinematicInputBlockedByScriptForDebugOutput)
		{
			cameraDebugf3("CinematicDirector: Script is not blocking Cinematic Input");
			m_WasCinematicInputBlockedByScriptForDebugOutput = false;
		}
#endif
		m_WasCinematicButtonAciveAfterScriptStoppedBlocking = false; 
		m_HaveStoredPressedTime = false; 
	}
	
	if(fwTimer::GetCamTimeInMilliseconds() - m_TimeCinematicButtonWasHeldPressed < m_Metadata.m_HoldDurationOfCinematicShotForEarlyTermination)
	{
		m_ShouldHoldCinematicShot = true; 
	}

	if(m_IsUsingQuickToggleThisFrame && !IsCinematicButtonDisabledByScriptState())
	{	
		CControl* control = camInterface::GetGameplayDirector().GetActiveControl();
		if(control)
		{
			if(control->GetVehicleCinematicCam().IsPressed())
			{
				if(!m_WasCinematicButtonAciveAfterScriptStoppedBlocking)
				{
					m_TimeCinematicButtonInitalPressed  = fwTimer::GetCamTimeInMilliseconds(); 
				}
			}

			if(control->GetVehicleCinematicCam().IsReleased())
			{
				if(!WasCinematicDownOnPreviousUpdate)
				{
					if(!m_WasCinematicButtonAciveAfterScriptStoppedBlocking)
					{	
						m_LatchedState = !m_LatchedState; 
						m_TimeCinematicButtonInitalPressed = 0; 
						m_HaveJustChangedToToggleContext = false;

#if __BANK
						if(m_LatchedState)
						{
							cameraDebugf3("CinematicDirector: Cinematc mode quick toggled on");
						}
						else
						{
							cameraDebugf3("CinematicDirector: Cinematc mode quick toggled off");
						}
#endif
					}
				}
			}

			if(control->GetVehicleCinematicCam().IsDown())
			{
				if(fwTimer::GetCamTimeInMilliseconds() -  m_TimeCinematicButtonInitalPressed  > m_Metadata.m_QuickToggleTimeThreshold  )
				{
					if(!m_HaveStoredPressedTime)
					{
						//not coming from a latch state
						if(!m_LatchedState)
						{
							m_TimeCinematicButtonWasHeldPressed = fwTimer::GetCamTimeInMilliseconds(); 
						}
						else
						{
							//else previously latched
							m_TimeCinematicButtonWasHeldPressed = 0; 
						}

						m_HaveStoredPressedTime = true; 
						cameraDebugf3("CinematicDirector: Cinematc button held down");
					}

					if(!m_WasCinematicButtonAciveAfterScriptStoppedBlocking)
					{
						m_IsCinematicButtonDown = true;
						m_LatchedState = false; 
						m_HaveJustChangedToToggleContext = false; 
					}

				
				}
			}
			else
			{
				m_HaveJustChangedToToggleContext = false; 
			}

			const bool hasReleasedViewModeInput = control->GetNextCameraMode().IsEnabled() && control->GetNextCameraMode().IsReleased();
			if(hasReleasedViewModeInput)
			{
#if __BANK
				if(m_LatchedState)
				{
					cameraDebugf3("CinematicDirector: Cinematc toggled off because a new mode has been selected");
				}
#endif
				m_LatchedState = false; 
				m_HaveJustChangedToToggleContext = false; 
			}
		}
	}
	else
	{
		//reset the latch state if being blocked by script or the current context is invalid but not invalidated by aiming. 
		if(IsCinematicButtonDisabledByScriptState() || (m_currentContext == -1 && !camInterface::GetGameplayDirector().IsAiming()))
		{
			m_LatchedState = false; 
		}		
		m_HaveJustChangedToToggleContext = false; 
	}
	
}


void camCinematicDirector::UpdateCinematicViewModeControl()
{

	float stickOffsetX = 0.0f;
	float stickOffsetY = 0.0f;

	CControl* control = camInterface::GetGameplayDirector().GetActiveControl();
	
	if(!m_ShouldScriptOverrideCinematicInputThisUpdate)
	{
		m_IsCinematicInputActiveForScript = false; 

		//Right stick normally switches the cinematic camera mode to previous or next.
	
		if(control)
		{
			UpdateInputForQuickToggleActivationMode(); 
			
#if __BANK
			bool WasCinematicModeActive = m_IsCinematicInputActive; 
#endif
			if(m_IsUsingQuickToggleThisFrame && !m_HaveJustChangedToToggleContext)
			{
				m_IsCinematicInputActive = m_LatchedState || m_IsCinematicButtonDown || m_ShouldHoldCinematicShot; 
			}
			else
			{
				m_IsCinematicInputActive = control->GetVehicleCinematicCam().IsDown() || m_ShouldHoldCinematicShot; 
			}

#if __BANK
			if(WasCinematicModeActive != m_IsCinematicInputActive)
			{
				if(m_IsCinematicInputActive)
				{
					cameraDebugf3("CinematicDirector: Cinematic Mode Active");
				}
				else
				{
					cameraDebugf3("CinematicDirector: Cinematic Mode Not Active");
				}
			}
#endif
		}
	}
	else
	{
		m_IsCinematicInputActiveForScript = true; 
	}

	if(control)
	{
		//TODO: Make these control inputs explicit for cinematic camera control.
		stickOffsetX = control->GetVehicleCinematicLeftRight().GetNorm() * 128.0f; // 128.0f to convert to old input range.
		stickOffsetY = -control->GetVehicleCinematicUpDown().GetNorm() * 128.0f;
	}
	
	if(!m_IsSlowMoActive)
	{
		if((Abs(stickOffsetX) < m_Metadata.m_ControlStickDeadZone) && (Abs(stickOffsetY) < m_Metadata.m_ControlStickDeadZone))
		{
			m_HasControlStickBeenReset = true;
			m_InputSelectedShotPreference = SELECTION_INPUT_NONE; 
		}

		if(m_HasControlStickBeenReset)
		{
			//up Y
			if(stickOffsetY > m_Metadata.m_ControlStickTriggerThreshold)
			{
				m_HasControlStickBeenReset = false;
				m_ShouldChangeShot = true;
				m_InputSelectedShotPreference = SELECTION_INPUT_UP; 
			}
			else if(stickOffsetY < -m_Metadata.m_ControlStickTriggerThreshold)
			{
				m_HasControlStickBeenReset = false;
				m_ShouldChangeShot = true;
				m_InputSelectedShotPreference = SELECTION_INPUT_DOWN; 
			}
			else if(stickOffsetX > m_Metadata.m_ControlStickTriggerThreshold)
			{
				m_HasControlStickBeenReset	= false;
				m_ShouldChangeShot = true;
				m_InputSelectedShotPreference = SELECTION_INPUT_RIGHT; 
			}
			else if (stickOffsetX < -m_Metadata.m_ControlStickTriggerThreshold)
			{
				m_HasControlStickBeenReset	= false;
				m_ShouldChangeShot = true; 
				m_InputSelectedShotPreference = SELECTION_INPUT_LEFT; 
			}
		}
	}
}

bool camCinematicDirector::CreateHigherPriorityContext()
{
	if(IsAnyCinematicContextActive())
	{
		for(int i = 0; i < m_currentContext; i++)
		{
			if(m_Contexts[i]->CanAbortOtherContexts() )
			{
				u32 CurrentShotDuration = m_Contexts[m_currentContext]->GetCurrentShotDuration(); 
				
				if(!m_Contexts[m_currentContext]->GetCurrentShot() || CurrentShotDuration > (u32)(m_Contexts[m_currentContext]->GetCurrentShot()->GetMinimumShotDuration() * m_Contexts[i]->GetMinShotScalar()))
				{
					if(m_Contexts[i]->Update())
					{
						if(m_Contexts[i]->GetCamera())
						{
							m_Contexts[m_currentContext]->ClearContext(); 
							
							m_currentContext = i; 
							
							m_UpdatedCamera = m_Contexts[m_currentContext]->GetCamera(); 
						
							return true; 
						}
					}
				}
			}
		}
	}
	return false;
}
	
void camCinematicDirector::PreUpdateContext()
{
	for(int i = 0; i < m_Contexts.GetCount(); i++)
	{
		m_Contexts[i]->PreUpdate(); 
	}
}

void camCinematicDirector::DoesCurrentContextSupportQuickToggle()
{
	if(!m_ShouldScriptOverrideCinematicInputThisUpdate)
	{
		m_IsUsingQuickToggleThisFrame = false; 

		for(int i = 0; i < m_Contexts.GetCount(); i++)
		{
			if(m_Contexts[i]->CanActivateUsingQuickToggle() && m_Contexts[i]->IsValid(false))
			{
				if(m_currentContext != -1)
				{
					if(!m_Contexts[m_currentContext]->CanActivateUsingQuickToggle())
					{
						CControl* control = camInterface::GetGameplayDirector().GetActiveControl();
						if(control && control->GetVehicleCinematicCam().IsDown())
						{
							m_HaveJustChangedToToggleContext = true; 
						}
					}
				}
				
				m_IsUsingQuickToggleThisFrame = true; 
				return; 
			}
		}
	}
}

void camCinematicDirector::UpdateScannedEntitesForWantedClonedPlayer()
{
	const CPed* followPed = camInterface::FindFollowPed();	

	if(followPed && followPed->IsNetworkClone()) 
	{
		if(followPed->GetPlayerWanted() && followPed->GetPlayerWanted()->GetWantedLevel() > WANTED_CLEAN)
		{
			CPed* nonConstPed = const_cast<CPed*>(followPed); 

			followPed->GetPedIntelligence()->GetPedScanner()->ScanForEntitiesInRange(*nonConstPed, false);
		}

	}
}

bool camCinematicDirector::UpdateContext()
{
	bool hasSucceeded = true;
	bool applyAllUpdates = true; 

	m_UpdatedCamera = NULL; 

	//Block all cinematic contexts if the user is Switching characters or triggering a gameplay hint or in replay mode.
	const bool isSwitchActive = g_PlayerSwitch.IsActive();
	const bool isGameplayHintActive = camInterface::GetGameplayDirector().IsHintActive();
	const bool isReplayActive = camInterface::IsRenderingDirector(camInterface::GetReplayDirector());
	if(isSwitchActive || isGameplayHintActive || isReplayActive)
	{
		//Set that the cinematic button has to be released if being held. 
		m_WasCinematicButtonAciveAfterScriptStoppedBlocking = true;	 

		if(m_currentContext >= 0 && m_currentContext < m_Contexts.GetCount())
		{
			m_Contexts[m_currentContext]->ClearContext();
		}

		m_currentContext = -1; 
		hasSucceeded = false;
	}
	else
	{
		DoesCurrentContextSupportQuickToggle();
		
		//cloned peds do not run the entity scanner as part of their AI update so do it here
		UpdateScannedEntitesForWantedClonedPlayer(); 

		PreUpdateContext(); 
		
		UpdateCinematicViewModeControl();

		if(m_currentContext >= 0 && m_currentContext < m_Contexts.GetCount())
		{
			if(!CreateHigherPriorityContext())
			{
				if(m_Contexts[m_currentContext]->IsValid() && m_Contexts[m_currentContext]->CanUpdate() != CSS_MUST_CHANGE_CAMERA)
				{
					if(m_Contexts[m_currentContext]->Update())
					{
						if(!m_Contexts[m_currentContext]->IsUsingGameCameraAsFallBack())	
						{
							m_UpdatedCamera = m_Contexts[m_currentContext]->GetCamera(); 
						
							if(m_UpdatedCamera)
							{
								//dont run a base update if this is first time  round its already been done
								if(m_UpdatedCamera->WasUpdated())
								{
									if(m_UpdatedCamera->IsValid())
									{
										applyAllUpdates = false; 
									}
									else
									{
										m_Contexts[m_currentContext]->ClearContext(); 
										m_currentContext = -1; 
									}
								}
								else
								{
									if(m_UpdatedCamera->BaseUpdate(m_Frame))
									{
										applyAllUpdates = false; 
									}
									else
									{
										m_Contexts[m_currentContext]->ClearContext(); 
										m_currentContext = -1; 
									}
								}
							}
						}
						else
						{
							applyAllUpdates = false;
						}
					}
				}
				else
				{
					m_Contexts[m_currentContext]->ClearContext(); 
					m_currentContext = -1; 
				}
			}
			else
			{
				applyAllUpdates = false; 
			}
		}

		if(applyAllUpdates)
		{
			for(int i = 0; i < m_Contexts.GetCount(); i++)
			{
				if(m_Contexts[i])
				{
					if(m_Contexts[i]->Update())
					{
						m_currentContext = i;

						m_UpdatedCamera = m_Contexts[i]->GetCamera();  
						break;
					}
				}
			}
		}
	}


	
	//reset here as only valid for one frame of a context
	m_ShouldChangeShot = false; 
	
	return hasSucceeded;
}

void camCinematicDirector::CleanUpCinematicContext()
{
	//Clean-up any existing camera.
	if(m_UpdatedCamera)
	{
		delete m_UpdatedCamera;
	}

	//Clean-up any slow motion.
	m_SlowMotionScaling	= 1.0f;
}

#if FPS_MODE_SUPPORTED
void camCinematicDirector::RegisterWeaponFireRemote(const CWeapon& weapon, const CEntity& firingEntity)
{	
	if(IsRenderingAnyInVehicleFirstPersonCinematicCamera())
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
					const float shakeScaling = g_TurretShakeFirstPersonScaling;
					const float shakeAmplitude = Clamp(shakeScaling * intensity, 0.0f, g_TurretShakeMaxAmplitude);

					renderedCamera->Shake(recoilShakeHash, shakeAmplitude);
				}
			}
		}
	}
}

void camCinematicDirector::RegisterWeaponFireLocal(const CWeapon& weapon, const CEntity& firingEntity)
{
	if( IsRenderingAnyInVehicleFirstPersonCinematicCamera() )
	{
		//Only trigger a recoil shake if the follow-ped is firing.
		const CPed* followPed = camInterface::GetGameplayDirector().GetFollowPed();
		if(followPed == &firingEntity)
		{
			bool bTurretCamera = false;
			if(m_UpdatedCamera && m_UpdatedCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
			{
				const camCinematicMountedCamera* pMountedCamera = (const camCinematicMountedCamera*)m_UpdatedCamera.Get();
				bTurretCamera = pMountedCamera->IsVehicleTurretCamera();
			}

			const CWeaponInfo* weaponInfo = weapon.GetWeaponInfo();
			if( weaponInfo && (weaponInfo->GetUseFPSAimIK() || bTurretCamera) )
			{
				//Kick-off a new recoil shake if specified in the weapon metadata.
				u32 recoilShakeHash							= weaponInfo->GetRecoilShakeHash();

				// TODO: should we use first person recoil shake, third person or a custom one?  using first person for now.
				if( weapon.GetFirstPersonRecoilShakeHash() != 0)
					recoilShakeHash							= weapon.GetFirstPersonRecoilShakeHash();

				const camShakeMetadata* recoilShakeMetadata	= camFactory::FindObjectMetadata<camShakeMetadata>(recoilShakeHash);
				if(recoilShakeMetadata)
				{
					//Apply any weapon-specific amplitude scaling to the shakes.
					float amplitude = weapon.GetRecoilShakeAmplitude();

					const u32 minTimeBetweenRecoilShakes = weaponInfo->GetMinTimeBetweenRecoilShakes();
					m_UpdatedCamera->Shake(*recoilShakeMetadata, amplitude, 1, minTimeBetweenRecoilShakes);
				}

				//Kick-off or auto-start the accuracy offset shake if specified in the weapon metadata.
				////const u32 accuracyOffsetShakeHash					= weaponInfo->GetAccuracyOffsetShakeHash();
				////const camShakeMetadata* accuracyOffsetShakeMetadata	= camFactory::FindObjectMetadata<camShakeMetadata>(accuracyOffsetShakeHash);
				////if(accuracyOffsetShakeMetadata)
				////{
				////	camBaseFrameShaker* accuracyOffsetShake = m_UpdatedCamera->FindFrameShaker(accuracyOffsetShakeHash);
				////	if(accuracyOffsetShake)
				////	{
				////		//Auto-start the shake, ensuring that we receive the full hold duration from this point on.
				////		accuracyOffsetShake->AutoStart();
				////	}
				////	else
				////	{
				////		//Start with zero amplitude, as this will be updated to an appropriate level in UpdateAimAccuracyOffsetShake().
				////		m_UpdatedCamera->Shake(*accuracyOffsetShakeMetadata, 0.0f);
				////	}
				////}
			}
		}
	}
}
#endif

#if __BANK
void camCinematicDirector::AddWidgetsForInstance()
{
	if(m_WidgetGroup == NULL)
	{
		bkBank* bank = BANKMGR.FindBank("Camera");
		if(bank != NULL)
		{
			m_WidgetGroup = bank->PushGroup("Cinematic director", false);
			{
				bank->AddCombo( "bonnet camera", &m_DebugBonnetCameraIndex, MAX_NUMBER_BONNET_CAMERAS, BonnetCameraTypes); 
				bank->AddToggle("Use bonnet camera [read-only]", &ms_UseBonnetCamera );
			}
			bank->PopGroup(); //Cutscene director.
		}
	}
}

u32 camCinematicDirector::GetOverriddenBonnetCamera()
{
	if(m_DebugBonnetCameraIndex == 0)
	{
		return 0;
	}
	else
	{
		return atStringHash(BonnetCameraTypes[m_DebugBonnetCameraIndex]); 
	}
}
#endif // __BANK
