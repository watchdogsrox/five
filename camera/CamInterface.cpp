//
// camera/CamInterface.cpp
// 
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#include "camera/CamInterface.h"

#include "profile/group.h"
#include "profile/page.h"
#include "system/param.h"

#include "fwsys/timer.h"

#include "camera/animscene/AnimSceneDirector.h"
#include "camera/cinematic/CinematicDirector.h"
#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"
#include "camera/cinematic/context/CinematicOnFootSpectatingContext.h"
#include "camera/cutscene/CutsceneDirector.h"
#include "camera/debug/DebugDirector.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/follow/FollowCamera.h"
#include "camera/gameplay/aim/FirstPersonAimCamera.h"
#include "camera/gameplay/aim/FirstPersonShooterCamera.h"
#include "camera/gameplay/aim/ThirdPersonPedAssistedAimCamera.h"
#include "camera/helpers/FramePropagator.h"
#include "camera/helpers/switch/BaseSwitchHelper.h"
#include "camera/marketing/MarketingDirector.h"
#include "camera/replay/ReplayDirector.h"
#include "camera/scripted/ScriptDirector.h"
#include "camera/scripted/ScriptedCamera.h"
#include "camera/switch/SwitchDirector.h"
#include "camera/syncedscene/SyncedSceneDirector.h"
#include "camera/system/CameraManager.h"
#include "camera/system/CameraMetadata.h"
#include "camera/viewports/ViewportManager.h"
#include "Control/replay/replay.h"
#include "debug/DebugScene.h"
#include "frontend/LoadingScreens.h"
#include "game/ModelIndices.h"
#include "network/Network.h"
#include "network/NetworkInterface.h"
#include "peds/PedIntelligence.h"
#include "physics/gtaInst.h"
#include "physics/physics.h"
#include "scene/EntityIterator.h"
#include "scene/FocusEntity.h"
#include "scene/world/GameWorld.h"
#include "streaming/streamingvisualize.h"
#include "system/controlMgr.h"
#include "text/messages.h"
#include "tools/SectorTools.h"
#include "vehicles/Metadata/VehicleEntryPointInfo.h"
#include "peds/ped.h"
#include "vehicles/vehicle.h"


CAMERA_OPTIMISATIONS()

XPARAM(cameraWidgets);

RAGE_DEFINE_CHANNEL(camera)

PF_PAGE(camInterfacePage, "Camera Interface");

PF_GROUP(camInterfaceFrameDetails);
PF_LINK(camInterfacePage, camInterfaceFrameDetails);

PF_VALUE_FLOAT(camInterfaceFrameVelocityMag, camInterfaceFrameDetails);
PF_VALUE_FLOAT(camInterfaceFrameAccelerationMag, camInterfaceFrameDetails);
PF_VALUE_FLOAT(camInterfaceFrameRateOfChangeOfAccelerationMag, camInterfaceFrameDetails);
PF_VALUE_FLOAT(camInterfaceFrameAngularVelocityMag, camInterfaceFrameDetails);
PF_VALUE_FLOAT(camInterfaceFov, camInterfaceFrameDetails);

#define FIND_DIRECTOR_FATAL(_DirectorClass, _DirectorMember, _DirectorFriendlyName) \
	{ _DirectorMember = camManager::FindDirector<_DirectorClass>(); \
	cameraFatalAssertf(_DirectorMember.Get() != NULL, "The " _DirectorFriendlyName \
	" camera director was not found in the camera metadata." \
	" Please check you have up-to-date camera metadata PSOs or an up-to-date .meta file if you are running with -rawCameraMetadata"); }

#define GET_DIRECTOR_FATAL(_DirectorClass, _DirectorMember, _DirectorFriendlyName) \
	{ cameraFatalAssertf(_DirectorMember.Get() != NULL, "The " _DirectorFriendlyName " camera director is invalid"); \
	return *((_DirectorClass*)_DirectorMember.Get()); }

CViewportFader camInterface::ms_Fader;

camFrame	camInterface::ms_CachedFrame;

#if GTA_REPLAY
RegdConstEnt camInterface::ms_CachedAttachedEntity;
#endif

Vector3		camInterface::ms_FramePositionDelta;
Vector3		camInterface::ms_FrameVelocity;
Vector3		camInterface::ms_FrameAcceleration;
Vector3		camInterface::ms_FrameRateOfChangeOfAcceleration;

RegdCamBaseDirector	camInterface::ms_CinematicDirector;
RegdCamBaseDirector	camInterface::ms_CutsceneDirector;
RegdCamBaseDirector	camInterface::ms_DebugDirector;
RegdCamBaseDirector	camInterface::ms_GameplayDirector;
RegdCamBaseDirector	camInterface::ms_MarketingDirector;
RegdCamBaseDirector	camInterface::ms_ScriptDirector;
RegdCamBaseDirector	camInterface::ms_SwitchDirector;
RegdCamBaseDirector	camInterface::ms_ReplayDirector;
RegdCamBaseDirector	camInterface::ms_SyncedSceneDirector;
RegdCamBaseDirector camInterface::ms_AnimSceneDirector;

RegdConstPed	camInterface::ms_ScriptControlledFollowPedThisUpdate;
s32				camInterface::ms_DeathFailEffectState;
float			camInterface::ms_CachedHeading;
float			camInterface::ms_CachedPitch;
float			camInterface::ms_CachedRoll;
bool			camInterface::ms_IsInitialized				= false;
bool			camInterface::ms_IsOverridingStreamingFocus	= false;
bool			camInterface::ms_IsRenderedCamInVehicle		= false; 

#if FPS_MODE_SUPPORTED
bool			camInterface::ms_SetScriptedCameraIsFirstPersonThisFrame	= false; 
#endif // FPS_MODE_SUPPORTED

#if __BANK
u32			camInterface::ms_DebugRespotCounter						= 10000;
bool		camInterface::ms_DebugFlashLocallyWhileRespotting		= false;
bool		camInterface::ms_DebugFlashRemotelyWhileRespotting		= true;
bool		camInterface::ms_DebugShouldFollowDebugFocusPed			= false;
bool		camInterface::ms_DebugForceFollowPedControlOrientation	= false;
bool		camInterface::ms_DebugShouldDisplayActiveCameraInfo		= false;
bool		camInterface::ms_DebugShouldDisplayGameViewportInfo		= false;
bool		camInterface::ms_DebugShouldRestartSystemAtEndOfFrame	= false;
#endif // __BANK


void camInterface::Init()
{
	//Find and cache our game-specific directors to enable direct access.
	FIND_DIRECTOR_FATAL(camCinematicDirector, ms_CinematicDirector, "cinematic");
	FIND_DIRECTOR_FATAL(camCutsceneDirector, ms_CutsceneDirector, "cutscene");
	FIND_DIRECTOR_FATAL(camDebugDirector, ms_DebugDirector, "debug");
	FIND_DIRECTOR_FATAL(camGameplayDirector, ms_GameplayDirector, "gameplay");
	FIND_DIRECTOR_FATAL(camMarketingDirector, ms_MarketingDirector, "marketing");
	FIND_DIRECTOR_FATAL(camScriptDirector, ms_ScriptDirector, "script");
	FIND_DIRECTOR_FATAL(camSwitchDirector, ms_SwitchDirector, "switch");
	FIND_DIRECTOR_FATAL(camReplayDirector, ms_ReplayDirector, "replay");
	FIND_DIRECTOR_FATAL(camSyncedSceneDirector, ms_SyncedSceneDirector, "synced-scene");
	FIND_DIRECTOR_FATAL(camAnimSceneDirector, ms_AnimSceneDirector, "animscene");

	camFrame defaultFrame;
	ms_CachedFrame.CloneFrom(defaultFrame); //Clone a clean default frame.

	ms_FramePositionDelta.Zero();
	ms_FrameVelocity.Zero();
	ms_FrameAcceleration.Zero();
	ms_FrameRateOfChangeOfAcceleration.Zero();

	ms_CachedHeading	= 0.0f;
	ms_CachedPitch		= 0.0f;
	ms_CachedRoll		= 0.0f;

	ms_DeathFailEffectState = DEATH_FAIL_EFFECT_INACTIVE;

	ms_ScriptControlledFollowPedThisUpdate	= NULL;
	ms_IsInitialized						= true;

#if __BANK
	if(PARAM_cameraWidgets.Get())
	{
		//Enable rendering of camera-related on-screen debug text by default with -cameraWidgets.
		ms_DebugShouldDisplayActiveCameraInfo = true;
	}
#endif // __BANK
}

void camInterface::Shutdown()
{
	ms_IsInitialized = false;
}

const Matrix34& camInterface::GetMat()
{
	return ms_CachedFrame.GetWorldMatrix();
}

bool camInterface::GetObjectSpaceCameraMatrix(const CEntity* pTargetEntity, Matrix34& mat)
{
	if(!pTargetEntity)
	{
		return false;
	}

	const camBaseCamera* pCamera = GetDominantRenderedCamera();
	if(!pCamera)
	{
		return false;
	}

	if(GetRenderedCameras().GetCount() == 1)
	{
		return pCamera->GetObjectSpaceCameraMatrix(pTargetEntity, mat);
	}
	else if(GetRenderedCameras().GetCount() > 1)
	{
		mat = ms_CachedFrame.GetWorldMatrix();
		mat.DotTranspose(MAT34V_TO_MATRIX34(pTargetEntity->GetMatrix()));
		return true;
	}

	return false;
}

const Vector3& camInterface::GetPos()
{
	return ms_CachedFrame.GetPosition();
}

const Vector3& camInterface::GetFront()
{
	return ms_CachedFrame.GetFront();
}

const Vector3& camInterface::GetRight()
{
	return ms_CachedFrame.GetRight();
}

const Vector3& camInterface::GetUp()
{
	return ms_CachedFrame.GetUp();
}

float camInterface::GetNear()
{
	return ms_CachedFrame.GetNearClip();
}

float camInterface::GetFar()
{
	return ms_CachedFrame.GetFarClip();
}

float camInterface::GetNearDof()
{
	return ms_CachedFrame.GetNearInFocusDofPlane();
}

float camInterface::GetFarDof()
{
	return ms_CachedFrame.GetFarInFocusDofPlane();
}

float camInterface::GetFov()
{
	return ms_CachedFrame.GetFov();
}

float camInterface::GetMotionBlurStrength()
{
	return ms_CachedFrame.GetMotionBlurStrength();
}

bool camInterface::IsDominantRenderedDirector(const camBaseDirector& director)
{
	const camBaseDirector* renderedDirector	= camManager::GetDominantRenderedDirector();
	const bool isTheRenderedDirector		= (&director == renderedDirector);

	return isTheRenderedDirector;
}

const camBaseDirector* camInterface::GetDominantRenderedDirector()
{
	const camBaseDirector* renderedDirector = camManager::GetDominantRenderedDirector();

	return renderedDirector;
}

bool camInterface::IsRenderingDirector(const camBaseDirector& director)
{
	bool isRenderingDirector = false;

	const atArray<tRenderedCameraObjectSettings>& renderedDirectors	= camManager::GetRenderedDirectors();
	const s32 numRenderedDirectors									= renderedDirectors.GetCount();

	for(s32 directorIndex=0; directorIndex<numRenderedDirectors; directorIndex++)
	{
		if(renderedDirectors[directorIndex].m_Object == &director)
		{
			isRenderingDirector = true;
			break;
		}
	}

	return isRenderingDirector;
}

const atArray<tRenderedCameraObjectSettings>& camInterface::GetRenderedDirectors()
{
	const atArray<tRenderedCameraObjectSettings>& renderedDirectors = camManager::GetRenderedDirectors();

	return renderedDirectors;
}

bool camInterface::IsDominantRenderedCamera(const camBaseCamera& camera)
{
	const camBaseCamera* renderedCamera	= camManager::GetDominantRenderedCamera();
	const bool isTheRenderedCamera		= (&camera == renderedCamera);

	return isTheRenderedCamera;
}

const camBaseCamera* camInterface::GetDominantRenderedCamera()
{
	const camBaseCamera* renderedCamera = camManager::GetDominantRenderedCamera();

	return renderedCamera;
}

bool camInterface::IsRenderingCamera(const camBaseCamera& camera)
{
	bool isRenderingCamera = false;

	const atArray<tRenderedCameraObjectSettings>& renderedCameras	= camManager::GetRenderedCameras();
	const s32 numRenderedCameras									= renderedCameras.GetCount();

	for(s32 cameraIndex=0; cameraIndex<numRenderedCameras; cameraIndex++)
	{
		if(renderedCameras[cameraIndex].m_Object == &camera)
		{
			isRenderingCamera = true;
			break;
		}
	}

	return isRenderingCamera;
}

const atArray<tRenderedCameraObjectSettings>& camInterface::GetRenderedCameras()
{
	const atArray<tRenderedCameraObjectSettings>& renderedCameras = camManager::GetRenderedCameras();

	return renderedCameras;
}

#if FPS_MODE_SUPPORTED
bool camInterface::IsRenderingFirstPersonCamera()
{
	const bool isRenderingFirstPersonCamera = camManager::IsRenderingFirstPersonCamera();

	return isRenderingFirstPersonCamera;
}

bool camInterface::IsRenderingFirstPersonShooterCamera()
{
	const bool isRenderingFirstPersonShooterCamera = camManager::IsRenderingFirstPersonShooterCamera();

	return isRenderingFirstPersonShooterCamera;
}

bool camInterface::IsRenderingFirstPersonShooterCustomFallBackCamera()
{
	const bool isRenderingFirstPersonShooterCustomFallBackCamera = camManager::IsRenderingFirstPersonShooterCustomFallBackCamera();

	return isRenderingFirstPersonShooterCustomFallBackCamera;
}

bool camInterface::IsDominantRenderedCameraAnyFirstPersonCamera()
{
#if GTA_REPLAY
	//check we're in replay, we only care about the replay cameras if we are
	const bool isReplayActive = IsRenderingDirector(GetReplayDirector());
	if (isReplayActive)
	{
		if (CReplayMgr::GetCamInFirstPersonFlag())
		{
			return true;
		}
		else
		{
			return false;
		}
	}
#endif //GTA_REPLAY

	const camBaseCamera* dominantCamera = camInterface::GetDominantRenderedCamera();
	if(!dominantCamera)
	{
		return false;
	}

	if (dominantCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()))
	{
		return true;
	}

	if (dominantCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
	{
		const camCinematicMountedCamera* povCamera = static_cast<const camCinematicMountedCamera*>(dominantCamera);
		if(povCamera->IsFirstPersonCamera())
		{
			return true;
		}
	}

	if (ms_SetScriptedCameraIsFirstPersonThisFrame && dominantCamera->GetIsClassId(camScriptedCamera::GetStaticClassId()))
	{
		return true;
	}

	return false;
}

void camInterface::SetScriptedCameraIsFirstPersonThisFrame(bool isFirstPersonThisFrame)
{
	ms_SetScriptedCameraIsFirstPersonThisFrame = isFirstPersonThisFrame;
}
#endif // FPS_MODE_SUPPORTED

camCinematicDirector& camInterface::GetCinematicDirector()
{
	GET_DIRECTOR_FATAL(camCinematicDirector, ms_CinematicDirector, "cinematic");
}

camCutsceneDirector& camInterface::GetCutsceneDirector()
{
	GET_DIRECTOR_FATAL(camCutsceneDirector, ms_CutsceneDirector, "cutscene");
}

camDebugDirector& camInterface::GetDebugDirector()
{
	GET_DIRECTOR_FATAL(camDebugDirector, ms_DebugDirector, "debug");
}

camDebugDirector* camInterface::GetDebugDirectorSafe()
{
	return static_cast<camDebugDirector*>(ms_DebugDirector.Get());
}

camGameplayDirector& camInterface::GetGameplayDirector()
{
	GET_DIRECTOR_FATAL(camGameplayDirector, ms_GameplayDirector, "gameplay");
}

camGameplayDirector* camInterface::GetGameplayDirectorSafe()
{
	return static_cast<camGameplayDirector*>(ms_GameplayDirector.Get());
}

camMarketingDirector& camInterface::GetMarketingDirector()
{
	GET_DIRECTOR_FATAL(camMarketingDirector, ms_MarketingDirector, "marketing");
}

camScriptDirector& camInterface::GetScriptDirector()
{
	GET_DIRECTOR_FATAL(camScriptDirector, ms_ScriptDirector, "script");
}

camSwitchDirector& camInterface::GetSwitchDirector()
{
	GET_DIRECTOR_FATAL(camSwitchDirector, ms_SwitchDirector, "switch");
}

camSyncedSceneDirector& camInterface::GetSyncedSceneDirector()
{
	GET_DIRECTOR_FATAL(camSyncedSceneDirector, ms_SyncedSceneDirector, "syncedscene");
}

camAnimSceneDirector& camInterface::GetAnimSceneDirector()
{
	GET_DIRECTOR_FATAL(camAnimSceneDirector, ms_AnimSceneDirector, "animscene");
}

camReplayDirector& camInterface::GetReplayDirector()
{
	GET_DIRECTOR_FATAL(camReplayDirector, ms_ReplayDirector, "replay");
}

//As far as player control goes what should we consider the camera heading to be?
float camInterface::GetPlayerControlCamHeading(const CPed& player)
{
	float heading;

	const camThirdPersonCamera* thirdPersonCamera = GetGameplayDirector().GetThirdPersonCamera(&player);
#if FPS_MODE_SUPPORTED
	const camFirstPersonShooterCamera* firstPersonShooterCamera = GetGameplayDirector().GetFirstPersonShooterCamera(&player);
#endif // FPS_MODE_SUPPORTED

	if (player.GetPedResetFlag(CPED_RESET_FLAG_ForcePedToUseScripCamHeading) && GetScriptDirector().IsRendering())
	{
		heading = GetScriptDirector().GetFrame().ComputeHeading();
	} 
	else if(thirdPersonCamera && (BANK_ONLY(ms_DebugForceFollowPedControlOrientation || )(!GetDebugDirector().IsFreeCamActive() &&
		!GetMarketingDirector().IsCameraActive())))
	{
		const Vector3& front = thirdPersonCamera->GetBaseFront();
		heading = camFrame::ComputeHeadingFromFront(front);
	}
#if FPS_MODE_SUPPORTED
	else if (firstPersonShooterCamera)
	{
		heading = firstPersonShooterCamera->GetHeading();
	}
#endif // FPS_MODE_SUPPORTED
	else
	{
		heading = GetHeading();
	}

	return heading;
}

//As far as player control goes what should we consider the camera pitch to be?
float camInterface::GetPlayerControlCamPitch(const CPed& player)
{
	float pitch;

	const camThirdPersonCamera* thirdPersonCamera = GetGameplayDirector().GetThirdPersonCamera(&player);
#if FPS_MODE_SUPPORTED
	const camFirstPersonShooterCamera* firstPersonShooterCamera = GetGameplayDirector().GetFirstPersonShooterCamera(&player);
#endif // FPS_MODE_SUPPORTED

	if(thirdPersonCamera && (BANK_ONLY(ms_DebugForceFollowPedControlOrientation ||) (!GetDebugDirector().IsFreeCamActive() &&
		!GetMarketingDirector().IsCameraActive())))
	{
		const Vector3& front = thirdPersonCamera->GetBaseFront();
		pitch = camFrame::ComputePitchFromFront(front);
	}
#if FPS_MODE_SUPPORTED
	else if (firstPersonShooterCamera)
	{
		pitch = firstPersonShooterCamera->GetPitch();
	}
#endif // FPS_MODE_SUPPORTED
	else
	{
		pitch = GetPitch();
	}

	return pitch;
}

//As far as player control goes what should we consider the camera aim frame to be?
const camFrame& camInterface::GetPlayerControlCamAimFrame()
{
	const camBaseCamera* dominantRenderedCamera = GetDominantRenderedCamera();
	if(dominantRenderedCamera && dominantRenderedCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()) &&
		static_cast<const camCinematicMountedCamera*>(dominantRenderedCamera)->ShouldAffectAiming())
	{
		const camFrame& mountedCameraFrame = dominantRenderedCamera->GetFrameNoPostEffects();
		return mountedCameraFrame;
	}
	else if(dominantRenderedCamera && dominantRenderedCamera->GetIsClassId(camThirdPersonPedAssistedAimCamera::GetStaticClassId()) &&
		static_cast<const camThirdPersonPedAssistedAimCamera*>(dominantRenderedCamera)->ShouldAffectAiming())
	{
		const camFrame& assistedAimFrame = static_cast<const camThirdPersonPedAssistedAimCamera*>(dominantRenderedCamera)->GetFrameForAiming();
		return assistedAimFrame;
	}

	//Use the frame of the rendered scripted camera if it is configured to affect aiming.
	bool shouldUseScriptCameraFrame			= false;
	const bool isScriptDirectorRendering	= GetScriptDirector().IsRendering();
	if(isScriptDirectorRendering)
	{
		const camBaseCamera* renderedCamera = GetScriptDirector().GetRenderedCamera();
		if(renderedCamera && renderedCamera->GetIsClassId(camScriptedCamera::GetStaticClassId()))
		{
			const camScriptedCamera* scriptedCamera	= static_cast<const camScriptedCamera*>(renderedCamera);
			shouldUseScriptCameraFrame				= scriptedCamera->ShouldAffectAiming();
		}
	}

	const camFrame& aimFrame = shouldUseScriptCameraFrame ? GetScriptDirector().GetFrame() : GetGameplayDirector().GetFrame();

	return aimFrame;
}

float camInterface::ComputeMiniMapHeading()
{
	//Use the heading of the debug free camera if it is active.
	const bool isFreeCameraActive = GetDebugDirector().IsFreeCamActive();
	if(isFreeCameraActive)
	{
		const float heading = GetDebugDirector().GetFreeCamFrame().ComputeHeading();

		return heading;
	}

	//Use the heading of the rendered scripted camera if it is configured to control the mini map orientation.
	const bool isScriptDirectorRendering = GetScriptDirector().IsRendering();
	if(isScriptDirectorRendering)
	{
		const camBaseCamera* renderedCamera = GetScriptDirector().GetRenderedCamera();
		if(renderedCamera)
		{
			bool shouldCameraControlMiniMapHeading = false;
			if (renderedCamera->GetIsClassId(camScriptedCamera::GetStaticClassId()))
			{
				const camScriptedCamera* scriptedCamera	= static_cast<const camScriptedCamera*>(renderedCamera);
				shouldCameraControlMiniMapHeading		= scriptedCamera->ShouldControlMiniMapHeading();
			}
			else if (renderedCamera->GetIsClassId(camAnimatedCamera::GetStaticClassId()))
			{
				const camAnimatedCamera* animatedCamera	= static_cast<const camAnimatedCamera*>(renderedCamera);
				shouldCameraControlMiniMapHeading		= animatedCamera->ShouldControlMiniMapHeading();
			}

			if(shouldCameraControlMiniMapHeading)
			{
				//Use the script director's frame, as this can include the result of interpolation not visible in rendered camera itself.
				const float heading = GetScriptDirector().GetFrameNoPostEffects().ComputeHeading();

				return heading;
			}
		}
	}

	const bool isRenderingCinematicCamera = GetCinematicDirector().IsRendering();
	if(isRenderingCinematicCamera)
	{
		const bool isRenderingMountedCamera = (GetCinematicDirector().IsRenderingCinematicMountedCamera());
		if(isRenderingMountedCamera)
		{
			//Use the base front, the no post effects frame has already cloned the post effect frame by this point
			const camBaseCamera* renderedCamera = camInterface::GetDominantRenderedCamera();
			const Vector3& front				= renderedCamera->GetBaseFront();
			const float heading					= camFrame::ComputeHeadingFromFront(front);

			return heading;
		}
		
		const bool isRendingIdleCamera = (GetCinematicDirector().IsRenderingCinematicIdleCamera()); 
		if(isRendingIdleCamera)
		{
			return 0.0f;
		}

		//Lock the mini-map to point north when spectating a remote player that is on-foot. Otherwise the mini-map can spin around in an
		//unintuitive way.
		const camBaseCinematicContext* currentContext = GetCinematicDirector().GetCurrentContext();
		if(currentContext && currentContext->GetIsClassId(camCinematicOnFootSpectatingContext::GetStaticClassId()))
		{
			return 0.0f;
		}

		const camThirdPersonCamera* thirdPersonCamera = GetGameplayDirector().GetThirdPersonCamera();
		if(thirdPersonCamera)
		{
			//Use the base front, as this ignores look-behind and thereby avoids confusing the user when a cinematic camera is rendering.
			const Vector3& front	= thirdPersonCamera->GetBaseFront();
			const float heading		= camFrame::ComputeHeadingFromFront(front);

			return heading;
		}
	}

#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		const bool isRenderingReplayCamera = camManager::FindDirector<camReplayDirector>()->IsRendering();
		if(isRenderingReplayCamera)
		{
			const float heading = camManager::FindDirector<camReplayDirector>()->GetFrameNoPostEffects().ComputeHeading();

			return heading;
		}
	}
#endif // GTA_REPLAY

	const float heading = GetGameplayDirector().GetFrameNoPostEffects().ComputeHeading();

	return heading;
}

void camInterface::CacheFrame(const camFrame& sourceFrame)
{
	ms_FramePositionDelta	= sourceFrame.GetPosition() - ms_CachedFrame.GetPosition();
	float invTimeStep		= fwTimer::GetInvTimeStep();
	Vector3 velocity		= ms_FramePositionDelta * invTimeStep;
	Vector3 acceleration	= (velocity - ms_FrameVelocity) * invTimeStep;
	ms_FrameVelocity		= velocity;
	ms_FrameRateOfChangeOfAcceleration	= (acceleration - ms_FrameAcceleration) * invTimeStep;
	ms_FrameAcceleration	= acceleration;

	PF_SET(camInterfaceFrameVelocityMag, ms_FrameVelocity.Mag());
	PF_SET(camInterfaceFrameAccelerationMag, ms_FrameAcceleration.Mag());
	PF_SET(camInterfaceFrameRateOfChangeOfAccelerationMag, ms_FrameRateOfChangeOfAcceleration.Mag());
	PF_SET(camInterfaceFrameAngularVelocityMag, sourceFrame.GetFront().FastAngle(ms_CachedFrame.GetFront()) * invTimeStep);
	PF_SET(camInterfaceFov, sourceFrame.GetFov());

	ms_CachedFrame.CloneFrom(sourceFrame);

	ms_CachedFrame.ComputeHeadingPitchAndRoll(ms_CachedHeading, ms_CachedPitch, ms_CachedRoll);

	UpdateRenderedCamInVehicle(); 

	//Override the streaming focus, as necessary.
	const bool shouldOverrideStreamingFocus = sourceFrame.GetFlags().IsFlagSet(camFrame::Flag_ShouldOverrideStreamingFocus);
	if(shouldOverrideStreamingFocus)
	{
		CFocusEntityMgr::GetMgr().SetPosAndVel(sourceFrame.GetPosition(), velocity);
	}
	else if(ms_IsOverridingStreamingFocus)
	{
		//Reset the streaming focus, which is persistent.
		CFocusEntityMgr::GetMgr().SetDefault(true);
	}

	ms_IsOverridingStreamingFocus = shouldOverrideStreamingFocus;

	if(IsDeathFailEffectActive())
	{
		cameraDebugf3("Camera death/fail effect is active (state = %d)", ms_DeathFailEffectState);
	}

	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_F3, KEYBOARD_MODE_DEBUG_ALT))
	{
		rage::ioHeadTracking::EnableFPSCamera(!rage::ioHeadTracking::UseFPSCamera());
	}

	CPad *pPad = CControlMgr::GetPlayerPad();
	if (pPad)
	{
#if !__FINAL
		if(pPad->GetLeftShoulder1()  && pPad->GetLeftShoulder2() && rage::ioHeadTracking::IsMotionTrackingEnabled())
		{
			rage::ioHeadTracking::EnableFPSCamera(!rage::ioHeadTracking::UseFPSCamera());
		}
#endif // !__FINAL
		if(pPad->GetDPadLeft() && pPad->ButtonCrossJustDown())
		{
			rage::ioHeadTracking::Reset();
		}
	}

	fwTimer::SetTimeWarpCamera(sourceFrame.GetCameraTimeScaleThisUpdate());
}

void camInterface::SetCachedFrameOnly(const camFrame& sourceFrame)
{
	ms_CachedFrame.CloneFrom(sourceFrame);
}

const CPed* camInterface::FindFollowPed()
{
	const CPed* followPlayer = static_cast<CPed*>(CGameWorld::FindFollowPlayer());

	if(ms_ScriptControlledFollowPedThisUpdate)
	{
		followPlayer = ms_ScriptControlledFollowPedThisUpdate;
	}
	else if(ms_GameplayDirector)
	{
		const camBaseSwitchHelper* switchHelper = static_cast<const camGameplayDirector*>(ms_GameplayDirector.Get())->GetSwitchHelper();
		if(switchHelper)
		{
			const CPed* desiredFollowPedForSwitch = switchHelper->GetDesiredFollowPed();
			if(desiredFollowPedForSwitch)
			{
				followPlayer = desiredFollowPedForSwitch;
			}
		}
	}

#if __BANK
	if(ms_DebugShouldFollowDebugFocusPed)
	{
		CEntity* debugFocusEntity = CDebugScene::FocusEntities_Get(0);
		if(debugFocusEntity)
		{
			if(debugFocusEntity->GetIsTypePed())
			{
				followPlayer = static_cast<CPed*>(debugFocusEntity);
			}
			else if(debugFocusEntity->GetIsTypeVehicle())
			{
				const CPed* driver = static_cast<CVehicle*>(debugFocusEntity)->GetDriver();
				if(driver)
				{
					followPlayer = driver;
				}
			}
		}
	}
#endif // __BANK

	return followPlayer;
}

//Resurrect the main player - cameras associated with this player are cleaned up and reinstated.
void camInterface::ResurrectPlayer() 
{
	SetDeathFailEffectState(DEATH_FAIL_EFFECT_INACTIVE);

	GetGameplayDirector().ResurrectPlayer();
	GetCinematicDirector().InvalidateIdleCamera();
}

void camInterface::ShakeCameraExplosion(u32 shakeNameHash, float explosionStrength, float rollOffScaling, const Vector3& positionOfExplosion)
{
	if(explosionStrength < SMALL_FLOAT)
	{
		return;
	}

	GetGameplayDirector().RegisterExplosion(shakeNameHash, explosionStrength, rollOffScaling, positionOfExplosion);
}

bool camInterface::IsCameraShaking(const bool includeExplosions)
{
	const camBaseDirector* renderedDirector	= GetDominantRenderedDirector();
	if(renderedDirector && renderedDirector->IsShaking())
	{
		return true;
	}

	if(includeExplosions)
	{
		const atArray<camGameplayDirector::tExplosionSettings>& explosionSettingsList = GetGameplayDirector().GetExplosionSettings();
		for(s32 explosionIndex = 0; explosionIndex < explosionSettingsList.GetCount(); ++explosionIndex)
		{
			if(explosionSettingsList[explosionIndex].m_FrameShaker)
			{
				return true;
			}
		}
	}

	return false;
}

void camInterface::RegisterWeaponFire(const CWeapon& weapon, const CEntity& firingEntity, const bool isNetworkClone)
{
	const int numDirectors = camManager::ms_Directors.GetCount();
	for(int i = 0; i < numDirectors; i++)
	{
		camBaseDirector* director = camManager::ms_Directors[i];
		if(director)
		{
			director->RegisterWeaponFire(weapon, firingEntity, isNetworkClone);
		}
	}
}

float camInterface::GetViewportTanFovH()
{
	return gVpMan.GetCurrentViewport()->GetGrcViewport().GetTanHFOV();
}

float camInterface::GetViewportTanFovV()
{
	return gVpMan.GetCurrentViewport()->GetGrcViewport().GetTanVFOV();
}

const Vector3& camInterface::GetViewportUp()
{
	return RCC_VECTOR3(gVpMan.GetCurrentViewport()->GetGrcViewport().GetCameraMtx().GetCol2ConstRef());
}

void camInterface::SetFixedFade(float fadeValue)
{
	SetFixedFade(fadeValue, Color_black);
}

void camInterface::SetFixedFade(float fadeValue, const Color32 &fadeColor)
{
	ms_Fader.SetFixedFade(fadeValue, &fadeColor);
}

void camInterface::FadeOut(s32 duration, bool shouldHoldAtEnd)
{
	FadeOut(duration, shouldHoldAtEnd, Color_black);
}

void camInterface::FadeOut(s32 duration, bool shouldHoldAtEnd, const Color32& fadeColour)
{
	STRVIS_ADD_MARKER(strStreamingVisualize::MARKER);
	STRVIS_SET_MARKER_TEXT("FADE OUT");

	CViewport* viewport = gVpMan.GetPrimaryOrthoViewport();
	if(viewport)
	{
		ms_Fader.Set(duration, false, &fadeColour, shouldHoldAtEnd);

		if (!CLoadingScreens::AreActive())
		{
			CLoadingText::SetActive(true);
		}
	}
}

bool camInterface::IsFadingOut()
{
	CViewport* viewport	= gVpMan.GetPrimaryOrthoViewport();
	bool isFadingOut	= viewport && (ms_Fader.IsFadingOut());
	return isFadingOut;
}

bool camInterface::IsFadedOut()
{
	CViewport* viewport	= gVpMan.GetPrimaryOrthoViewport();
	bool isFadedOut		= viewport && (ms_Fader.IsFadedOut());
	return isFadedOut;
}

void camInterface::FadeIn(s32 duration)
{
	FadeIn(duration, Color_black);
}

void camInterface::FadeIn(s32 duration, const Color32& fadeColour)
{
	STRVIS_ADD_MARKER(strStreamingVisualize::MARKER);
	STRVIS_SET_MARKER_TEXT("FADE IN");
	
	CViewport* viewport = gVpMan.GetPrimaryOrthoViewport();
	if(viewport)
	{
		ms_Fader.Set(duration, true, &fadeColour);

		CLoadingText::SetActive(false);
	}
}

void camInterface::RenderFade()
{
	ms_Fader.Render();
}

bool camInterface::IsFadingIn()
{
	CViewport* viewport	= gVpMan.GetPrimaryOrthoViewport();
	bool isFadingIn		= viewport && (ms_Fader.IsFadingIn());
	return isFadingIn;
}

bool camInterface::IsFadedIn()
{
	CViewport* viewport	= gVpMan.GetPrimaryOrthoViewport();
	bool isFadedIn		= viewport && (ms_Fader.IsFadedIn());
	return isFadedIn;
}

bool camInterface::IsFading()
{
	bool isFading = IsFadingIn() || IsFadingOut();
	return isFading;
}

bool camInterface::IsFadingOrJustFaded()
{
#define NUM_FRAMES_TO_WAIT	(15)
	bool isFadingOrJustFaded = IsFading();

	static u32 releaseFramesRemaining = NUM_FRAMES_TO_WAIT;
	if(isFadingOrJustFaded)
	{
		releaseFramesRemaining = NUM_FRAMES_TO_WAIT;
	}
	else if(releaseFramesRemaining > 0)
	{
		releaseFramesRemaining--;
	}

	if(releaseFramesRemaining > 0)
	{
		isFadingOrJustFaded = true;
	}

	return isFadingOrJustFaded;
}

void camInterface::UpdateFade()
{
	ms_Fader.Process();
}

float camInterface::GetFadeLevel()
{
	float fadeLevel = ms_Fader.GetAudioFade();
	return fadeLevel;
}

#if GTA_REPLAY
void camInterface::SetFader(const CViewportFader& fader)
{
	ms_Fader = fader;
}
#endif

//Returns true if the specified sphere is visible in the game viewport.
bool camInterface::IsSphereVisibleInGameViewport(const Vector3& centre, float radius)
{
	bool isVisible = false;

	CViewport* gameViewport = gVpMan.GetGameViewport();
	if(cameraVerifyf(gameViewport, "camInterface::IsSphereVisibleInGameViewport was called when no game viewport was active"))
	{
		isVisible = (gameViewport->GetGrcViewport().IsSphereVisible(centre.x, centre.y, centre.z, radius) != cullOutside);
	}

	return isVisible;
}

bool camInterface::IsSphereVisibleInGameViewport(Vec4V_In centreAndRadius)
{
	bool isVisible = false;

	CViewport* gameViewport = gVpMan.GetGameViewport();
	if(cameraVerifyf(gameViewport, "camInterface::IsSphereVisibleInGameViewport was called when no game viewport was active"))
	{
		isVisible = (gameViewport->GetGrcViewport().IsSphereVisible(centreAndRadius) != 0);
	}

	return isVisible;
}


bool camInterface::IsSphereVisibleInViewport(int viewportId, const Vector3& centre, float radius)
{
	bool isVisible = false;

	CViewport* viewport = gVpMan.FindById(viewportId);
	if(cameraVerifyf(viewport, "camInterface::IsSphereVisibleInViewport was called with an inactive viewport ID"))
	{
		isVisible = (viewport->GetGrcViewport().IsSphereVisible(centre.x, centre.y, centre.z, radius) != cullOutside);
	}

	return isVisible;
}

bool camInterface::ComputeVehicleEntryExitPointPositionAndOrientation(const CPed& targetPed, const CVehicle& targetVehicle, Vector3& entryExitPointPosition,
	Quaternion& entryExitOrientation, bool& hasClimbDown)
{
	CQueriableInterface* queriableInterface = targetPed.GetPedIntelligence()->GetQueriableInterface();
	if(!queriableInterface)
	{
		return false;
	}

	s32 entryPointId		= -1;
	bool isGetInTaskActive	= queriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE);
	if(isGetInTaskActive)
	{
		entryPointId		= queriableInterface->GetTargetEntryPointForTaskType(CTaskTypes::TASK_ENTER_VEHICLE);
	}

	bool isGetOutTaskActive	= queriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE);
	if(isGetOutTaskActive)
	{
		entryPointId		= queriableInterface->GetTargetEntryPointForTaskType(CTaskTypes::TASK_EXIT_VEHICLE);
	}

	if(entryPointId < 0)
	{
		return false;
	}

	CModelSeatInfo::CalculateEntrySituation(&targetVehicle, &targetPed, entryExitPointPosition, entryExitOrientation, entryPointId);

	const CVehicleEntryPointAnimInfo* clipInfo	= targetVehicle.GetEntryAnimInfo(entryPointId);
	hasClimbDown								= clipInfo && clipInfo->GetHasClimbDown();

	return true;
}

bool camInterface::ComputeShouldPauseCameras()
{
#if SECTOR_TOOLS_EXPORT
	const bool isRunningSectorTools	= CSectorTools::GetEnabled();
#else
	const bool isRunningSectorTools	= false;
#endif // SECTOR_TOOLS_EXPORT

	//Only during a single-player game does the front-end pause the cameras.
	//MF: Also only when the player settings menu is not running, don't forget that.
	//NOTE: Cameras are never paused when the debug sector tools are running, as all cameras must update during the tests.
	const bool shouldPause	= !isRunningSectorTools && BANK_ONLY(!fwTimer::GetSingleStepThisFrame() &&)
								((fwTimer::IsUserPaused() && !NetworkInterface::IsGameInProgress()) || fwTimer::IsGamePaused());

	return shouldPause REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive());
}

struct tCameraFocusPedCandidate
{
	CPed*	m_Ped;
	float	m_ScaledDistance;
};

s32 CameraFocusPedCandidateCompareFunc(const tCameraFocusPedCandidate* candidateA, const tCameraFocusPedCandidate* candidateB)
{
	return (candidateA->m_ScaledDistance < candidateB->m_ScaledDistance) ? -1 : 1;
}

CPed* camInterface::ComputeFocusPedOnScreen(float maxValidDistance, s32 screenPositionTestBoneTag,
	float maxScreenWidthRatioAroundCentreForTestBone, float maxScreenHeightRatioAroundCentreForTestBone, float minRelativeHeadingScore,
	float maxScreenCentreScoreBoost, float maxScreenRatioAroundCentreForScoreBoost, s32 losTestBoneTag1, s32 losTestBoneTag2)
{
	const CPed* followPed			= FindFollowPed();
	const Vector3& cameraPosition	= ms_CachedFrame.GetPosition();
	const Vector3& cameraFront		= ms_CachedFrame.GetFront();
	const float verticalFov			= ms_CachedFrame.GetFov();
	const CViewport* viewport		= gVpMan.GetGameViewport();
	const float aspectRatio			= viewport ? viewport->GetGrcViewport().GetAspectRatio() : (16.0f / 9.0f);
	const float horizontalFov		= verticalFov * aspectRatio;

	const float maxCameraToScreenTestBoneHeadingDelta	= 0.5f * maxScreenWidthRatioAroundCentreForTestBone * horizontalFov * DtoR;
	const float maxCameraToScreenTestBonePitchDelta		= 0.5f * maxScreenHeightRatioAroundCentreForTestBone * verticalFov * DtoR;

	atArray<tCameraFocusPedCandidate> candidatePeds;

	Vec3V vIteratorPos = VECTOR3_TO_VEC3V(cameraPosition);
	CEntityIterator entityIterator(IteratePeds, followPed, &vIteratorPos, maxValidDistance);
	while(true)
	{
		CEntity* nearbyEntity = entityIterator.GetNext();
		if(!nearbyEntity)
		{
			break;
		}

		if(nearbyEntity->GetIsTypePed())
		{
			CPed* ped = static_cast<CPed*>(nearbyEntity);

			if(screenPositionTestBoneTag != BONETAG_INVALID)
			{
				//Ensure that the specified test bone is within the view frustum, including a sensible border width.
				Vector3 screenTestBonePosition;
				const bool hasValidBone = ped->GetBonePosition(screenTestBonePosition, static_cast<eAnimBoneTag>(screenPositionTestBoneTag));
				if(!hasValidBone)
				{
					continue;
				}

				Vector3 cameraToScreenTestBoneFront(screenTestBonePosition - cameraPosition);
				cameraToScreenTestBoneFront.NormalizeSafe(cameraFront);

				const float headingDelta = Abs(camFrame::ComputeHeadingDeltaBetweenFronts(cameraFront, cameraToScreenTestBoneFront));
				if(headingDelta > maxCameraToScreenTestBoneHeadingDelta)
				{
					//This ped's test bone is outside of the screen-space safe zone.
					continue;
				}

				const float pitchDelta = Abs(camFrame::ComputePitchDeltaBetweenFronts(cameraFront, cameraToScreenTestBoneFront));
				if(pitchDelta > maxCameraToScreenTestBonePitchDelta)
				{
					//This ped's test bone is outside of the screen-space safe zone.
					continue;
				}
			}

			//Score the camera-relative heading to the ped, as more centrally framed peds are more relevant.
			const Vector3 pedPosition		= VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition());
			const Vector3 cameraToPedDelta	= pedPosition - cameraPosition;
			Vector3 cameraToPedFront(cameraToPedDelta);
			cameraToPedFront.NormalizeSafe(cameraFront);

			const float headingDelta = Abs(camFrame::ComputeHeadingDeltaBetweenFronts(cameraFront, cameraToPedFront));

			float headingScoreScaling	= RampValueSafe(headingDelta, 0.0f, 0.5f * horizontalFov * DtoR, 1.0f, 0.0f);
			headingScoreScaling			= SlowInOut(headingScoreScaling);
			headingScoreScaling			= RampValue(headingScoreScaling, 0.0f, 1.0f, minRelativeHeadingScore, 1.0f);

			//Compute the any score boost around the screen centre.
			float screenCentreScoreBoost	= RampValueSafe(headingDelta, 0.0f, 0.5f * maxScreenRatioAroundCentreForScoreBoost * horizontalFov * DtoR,
												1.0f, 0.0f);
			screenCentreScoreBoost			= SlowInOut(screenCentreScoreBoost);
			screenCentreScoreBoost			= RampValue(screenCentreScoreBoost, 0.0f, 1.0f, 1.0f, maxScreenCentreScoreBoost);
			headingScoreScaling				*= screenCentreScoreBoost;

			if(headingScoreScaling >= SMALL_FLOAT)
			{
				//Now factor in the distance to the ped.
				tCameraFocusPedCandidate& candidate	= candidatePeds.Grow();
				candidate.m_Ped						= ped;
				candidate.m_ScaledDistance			= cameraToPedDelta.Mag() / headingScoreScaling;
			}
		}
	}

	//Order the candidates by scaled distance, nearest first.
	candidatePeds.QSort(0, -1, CameraFocusPedCandidateCompareFunc);

	CPed* focusPed = NULL;

	//Find the best candidate ped that meets the LOS requirements.
	const int numCandidatePeds = candidatePeds.GetCount();
	for(int i=0; i<numCandidatePeds; i++)
	{
		CPed* candidatePed = candidatePeds[i].m_Ped;

		if(losTestBoneTag1 != BONETAG_INVALID)
		{
			const bool hasClearLosToPed = ComputeHasClearLineOfSightToPedBone(*candidatePed, losTestBoneTag1);
			if(!hasClearLosToPed)
			{
				continue;
			}
		}

		if(losTestBoneTag2 != BONETAG_INVALID)
		{
			const bool hasClearLosToPed = ComputeHasClearLineOfSightToPedBone(*candidatePed, losTestBoneTag2);
			if(!hasClearLosToPed)
			{
				continue;
			}
		}

		focusPed = candidatePed;
		break;
	}

	return focusPed;
}

s32 CompareLosProbeIntersections(const WorldProbe::CShapeTestHitPoint* a, const WorldProbe::CShapeTestHitPoint* b)
{
	return (a->GetHitTValue() < b->GetHitTValue()) ? -1 : 1;
}

bool camInterface::ComputeHasClearLineOfSightToPedBone(const CPed& ped, s32 boneTag, bool shouldIgnoreOcclusionDueToFollowPed, bool shouldIgnoreOcclusionDueToFollowVehicle)
{
	Vector3 bonePosition;
	const bool hasValidBone = ped.GetBonePosition(bonePosition, static_cast<eAnimBoneTag>(boneTag));
	if(!hasValidBone)
	{
		return false;
	}

	WorldProbe::CShapeTestProbeDesc probeTest;
	WorldProbe::CShapeTestFixedResults<> probeTestResults;
	probeTest.SetResultsStructure(&probeTestResults);
	probeTest.SetContext(WorldProbe::LOS_Camera);

	const u32 typesToCollideWith = ArchetypeFlags::GTA_ALL_TYPES_WEAPON | ArchetypeFlags::GTA_RAGDOLL_TYPE;
	probeTest.SetIncludeFlags(typesToCollideWith);

	atArray<const phInst*> excludeInstances;

	//Exclude all of the ped's immediate child attachments.
	const fwEntity* childAttachment = ped.GetChildAttachment();
	while(childAttachment)
	{
		const phInst* attachmentInstance = childAttachment->GetCurrentPhysicsInst();
		if(attachmentInstance)
		{
			excludeInstances.Grow() = attachmentInstance;
		}

		childAttachment = childAttachment->GetSiblingAttachment();
	}

	if(shouldIgnoreOcclusionDueToFollowPed)
	{
		//Exclude the follow ped capsule and ragdoll bounds.
		const CPed* followPed = FindFollowPed();
		if(followPed)
		{
			const phInst* animatedInstance = followPed->GetAnimatedInst();
			if(animatedInstance)
			{
				excludeInstances.Grow() = animatedInstance;
			}

			const phInst* ragdollInstance = followPed->GetRagdollInst();
			if(ragdollInstance)
			{
				excludeInstances.Grow() = ragdollInstance;
			}

			//Exclude all of the follow ped's immediate child attachments.
			const fwEntity* childAttachment = followPed->GetChildAttachment();
			while(childAttachment)
			{
				const phInst* attachmentInstance = childAttachment->GetCurrentPhysicsInst();
				if(attachmentInstance)
				{
					excludeInstances.Grow() = attachmentInstance;
				}

				childAttachment = childAttachment->GetSiblingAttachment();
			}
		}
	}

	if(shouldIgnoreOcclusionDueToFollowVehicle)
	{
		const CVehicle* followVehicle = camInterface::GetGameplayDirector().GetFollowVehicle();
		if(followVehicle)
		{
			const phInst* followVehicleInstance = followVehicle->GetCurrentPhysicsInst();
			if(followVehicleInstance)
			{
				excludeInstances.Grow() = followVehicleInstance;
			}
		}
	}

	const int numExcludeInstances = excludeInstances.GetCount();
	if(numExcludeInstances > 0)
	{
		probeTest.SetExcludeInstances(excludeInstances.GetElements(), numExcludeInstances);
	}

	const Vector3& cameraPosition = ms_CachedFrame.GetPosition();
	probeTest.SetStartAndEnd(cameraPosition, bonePosition);

	WorldProbe::GetShapeTestManager()->SubmitTest(probeTest, WorldProbe::PERFORM_SYNCHRONOUS_TEST);

	const u32 numHits = probeTestResults.GetNumHits();
	if(numHits == 0)
	{
		return false;
	}

	if(numHits > 1)
	{
		probeTestResults.SortHits(CompareLosProbeIntersections);
	}

	phInst* hitInstance	= probeTestResults[0].GetHitInst();
	CEntity* hitEntity	= hitInstance ? CPhysics::GetEntityFromInst(hitInstance) : NULL;
	
	const bool hasClearLosToPed = (hitEntity == &ped);

	return hasClearLosToPed;
}

//This is for Alex (and decals for rendering bullet holes and stuff on glass.)
bool camInterface::IsRenderedCameraInsideVehicle()
{
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		return CReplayMgr::IsRenderedCamInVehicle();
	}
	else
#endif
	{
		return ms_IsRenderedCamInVehicle;
	}
}

bool camInterface::IsRenderedCameraForcingMotionBlur()
{
	const camCinematicDirector& cinematicDirector	= GetCinematicDirector();
	const camBaseCamera* renderedCinematicCamera	= cinematicDirector.GetRenderedCamera();
	const bool isActuallyRenderingCinematicCamera	= renderedCinematicCamera && IsDominantRenderedCamera(*renderedCinematicCamera);	

	return isActuallyRenderingCinematicCamera && cinematicDirector.IsRenderedCameraForcingMotionBlur();
}

void camInterface::UpdateRenderedCamInVehicle()
{
	const camCinematicDirector& cinematicDirector	= GetCinematicDirector();
	const camBaseCamera* renderedCinematicCamera	= cinematicDirector.GetRenderedCamera();
	const bool isActuallyRenderingCinematicCamera	= renderedCinematicCamera && IsDominantRenderedCamera(*renderedCinematicCamera);
	ms_IsRenderedCamInVehicle						= isActuallyRenderingCinematicCamera && cinematicDirector.IsRenderedCameraInsideVehicle();

	if(!ms_IsRenderedCamInVehicle)
	{
		//Check if we're rendering a scripted camera that has been specially flagged by script as being inside a vehicle.
		const camBaseCamera* dominantRenderedCamera	= GetDominantRenderedCamera();
		ms_IsRenderedCamInVehicle					= (dominantRenderedCamera && dominantRenderedCamera->GetIsClassId(camScriptedCamera::GetStaticClassId()) &&
														static_cast<const camScriptedCamera*>(dominantRenderedCamera)->IsInsideVehicle());
	}
}

bool camInterface::ComputeShouldMakePedHeadInvisible(const CPed& ped)
{
	const CPed* pedToMakeHeadInvisible = NULL;

	//Hide the follow ped's head if we're fully rendering gameplay and first-person mode is active.
	// TODO: remove if we drop using third person camera for first person mode, head visibility state read from FirstPersonAimCamera metadata
#if FPS_MODE_SUPPORTED
	camGameplayDirector* gameplayDirector	= static_cast<camGameplayDirector*>(ms_GameplayDirector.Get());
	if(gameplayDirector)
	{
		const bool isFirstPersonModeActive		= (gameplayDirector->IsFirstPersonModeEnabled() && gameplayDirector->IsFirstPersonModeAllowed());
		if(isFirstPersonModeActive && gameplayDirector->ShouldUseThirdPersonCameraForFirstPersonMode())
		{
			const atArray<tRenderedCameraObjectSettings>& renderedDirectors = GetRenderedDirectors();
			const int numRenderedDirectors									= renderedDirectors.GetCount();
			if((numRenderedDirectors == 1) && (static_cast<const camBaseDirector*>(renderedDirectors[0].m_Object.Get()) == gameplayDirector))
			{
				const CPed* followPed = gameplayDirector->GetFollowPed();
				if(followPed == &ped)
				{
					return true;
				}
			}
		}
	}
#endif // FPS_MODE_SUPPORTED

	//Otherwise, we only make the ped's head invisible if we are fully rendering a cinematic mounted or first-person camera that has flagged this ped.

	const atArray<tRenderedCameraObjectSettings>& renderedCameras	= GetRenderedCameras();
	const int numRenderedCameras									= renderedCameras.GetCount();
	if(numRenderedCameras < 1)
	{
		return false;
	}

	const camBaseCamera* renderedCamera = static_cast<const camBaseCamera*>(renderedCameras[0].m_Object.Get());
	if(!renderedCamera)
	{
		return false;
	}

	bool isInterpolatingBetweenTableGamesCameras = false;
	if (numRenderedCameras == 2)
	{
		const camBaseCamera* firstCamera = renderedCamera;
		const camBaseCamera* secondCamera = static_cast<const camBaseCamera*>(renderedCameras[1].m_Object.Get());

		const bool isFirstCameraTableGames = 
			firstCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()) &&
			static_cast<const camFirstPersonShooterCamera*>(firstCamera)->IsTableGamesCamera();

		const bool isSecondCameraTableGames = secondCamera &&
			secondCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()) &&
			static_cast<const camFirstPersonShooterCamera*>(secondCamera)->IsTableGamesCamera();

		isInterpolatingBetweenTableGamesCameras = isFirstCameraTableGames || isSecondCameraTableGames;
	}
	
	if (!isInterpolatingBetweenTableGamesCameras)
	{
		// If cinematic camera is rendering and it wants ped head to be invisible, allow head to be hidden while interpolating. (i.e. 2 cameras rendering)
		bool bCinematicCameraWantsPedHeadInvisible = renderedCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()) &&
			static_cast<const camCinematicMountedCamera*>(renderedCamera)->GetPedToMakeHeadInvisible();

		if (numRenderedCameras != 1 && (!bCinematicCameraWantsPedHeadInvisible || numRenderedCameras != 2))
		{
			return false;
		}
	}

	if( renderedCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()) )
	{
		pedToMakeHeadInvisible = static_cast<const camCinematicMountedCamera*>(renderedCamera)->GetPedToMakeHeadInvisible();
	}
	else if( renderedCamera->GetIsClassId(camFirstPersonAimCamera::GetStaticClassId()) )
	{
		const camFirstPersonAimCamera* pFirstPersonAimCamera = static_cast<const camFirstPersonAimCamera*>(renderedCamera);
		if( !pFirstPersonAimCamera->ShouldMakeAttachedEntityInvisible() && pFirstPersonAimCamera->ShouldMakeAttachedPedHeadInvisible() )
		{
			const CEntity* entityToMakeInvisible = pFirstPersonAimCamera->GetAttachParent();
			if(entityToMakeInvisible && entityToMakeInvisible->GetIsTypePed())
			{
				pedToMakeHeadInvisible = static_cast<const CPed*>(entityToMakeInvisible);
			}
		}
	}

#if FPS_MODE_SUPPORTED && 0
	// If free camera is active and only other camera is first person camera, don't render the head.
	camGameplayDirector* gameplayDirector	= static_cast<camGameplayDirector*>(ms_GameplayDirector.Get());
	if(ms_DebugDirector.Get() && GetDebugDirector().IsFreeCamActive() && ms_GameplayDirector.Get())
	{
		const bool isFirstPersonModeActive	= (GetGameplayDirector().IsFirstPersonModeEnabled() && GetGameplayDirector().IsFirstPersonModeAllowed());
		if(isFirstPersonModeActive)
		{
			const int numRenderedDirectors = renderedDirectors.GetCount();
			if((numRenderedDirectors == 2) && (static_cast<const camBaseDirector*>(renderedDirectors[0].m_Object.Get()) == gameplayDirector))
			{
				const CPed* followPed = gameplayDirector->GetFollowPed();
				if(followPed == &ped)
				{
					return true;
				}
			}
		}
	}
#endif // FPS_MODE_SUPPORTED

	const bool shouldMakeInvisible = (pedToMakeHeadInvisible == &ped);
	return shouldMakeInvisible;
}

void camInterface::DisableNearClipScanThisUpdate()
{
	camManager::ms_FramePropagator.DisableNearClipScanThisUpdate();
}

u32 camInterface::ComputePovTurretCameraHash(const CVehicle* pVehicle)
{
	const CPed* pFollowPed = FindFollowPed();
	if(!pVehicle || !pVehicle->GetVehicleModelInfo() || !pFollowPed)
	{
		return 0;
	}

    const s32 seatIndex = camGameplayDirector::ComputeSeatIndexPedIsIn(*pVehicle, *pFollowPed);
    const bool isPedOnATurret = camGameplayDirector::IsTurretSeat(pVehicle, seatIndex);
    if(isPedOnATurret)
    {
        const CVehicleModelInfo* pVehicleModelInfo = pVehicle->GetVehicleModelInfo();
        const CWeaponInfo* pTurretWeaponInfo = camGameplayDirector::GetTurretWeaponInfoForSeat(*pVehicle, seatIndex);

        if(pTurretWeaponInfo && pTurretWeaponInfo->GetPovTurretCameraHash() != 0)
        {
            return pTurretWeaponInfo->GetPovTurretCameraHash();
        }
        else if(pVehicleModelInfo && pVehicleModelInfo->GetPovTurretCameraNameHash() != 0)
        {
            return pVehicleModelInfo->GetPovTurretCameraNameHash();
        }
    }

	return 0;
}

#if RSG_PC
float camInterface::GetStereoConvergence()
{
	return camManager::ms_fStereoConvergence;
}
#endif

void camInterface::SetDeathFailEffectState(s32 state)
{
	if(ms_DeathFailEffectState == state)
	{
		return;
	}

	const bool hasSucceeded = GetGameplayDirector().SetDeathFailEffectState(state);
	if(hasSucceeded)
	{
		ms_DeathFailEffectState = state;
	}
}

#if FPS_MODE_SUPPORTED
void camInterface::ApplyPedBoneOverridesForFirstPerson(const CPed& ped, crSkeleton& skeleton)
{
	//Rotate the attach ped's spine backwards a little if we're rendering an FPS camera that's been pushed back out of collision (towards a safe fall back position.)

	const camBaseCamera* renderedCamera = camManager::GetDominantRenderedCamera();
	if(!renderedCamera || !renderedCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()))
	{
		return;
	}

	const camFirstPersonShooterCamera* fpsCamera	= static_cast<const camFirstPersonShooterCamera*>(renderedCamera);
	const float rotationToApply						= fpsCamera->ComputeRotationToApplyToPedSpineBone(ped);
	if(IsNearZero(rotationToApply))
	{
		return;
	}

	const s32 tempBoneIndex = ped.GetBoneIndexFromBoneTag(BONETAG_SPINE0);
	if(tempBoneIndex == -1)
	{
		return;
	}

	const u32 boneIndex			= static_cast<u32>(tempBoneIndex);
	Mat34V& localBoneMatrix		= skeleton.GetLocalMtx(boneIndex);
	Matrix34 tmpLocalBoneMatrix	= RC_MATRIX34(localBoneMatrix);

	tmpLocalBoneMatrix.RotateLocalZ(rotationToApply);
	localBoneMatrix = MATRIX34_TO_MAT34V(tmpLocalBoneMatrix);
	skeleton.PartialUpdate(boneIndex);
}
#endif // FPS_MODE_SUPPORTED

#if __BANK
void camInterface::AddWidgets(bkBank &bank)
{
	bank.AddToggle("Display active camera info on-screen", &ms_DebugShouldDisplayActiveCameraInfo);
	bank.AddToggle("Display game viewport info on-screen", &ms_DebugShouldDisplayGameViewportInfo);

	bank.AddButton("Dump spectator ped info (network ped id & spectator cam)", datCallback(CFA(DebugDumpSpectatorPedInfo)));

	bank.AddToggle("Follow debug focus ped", &ms_DebugShouldFollowDebugFocusPed);
	bank.AddToggle("Always take control orientation from follow-ped camera", &ms_DebugForceFollowPedControlOrientation);

	bank.AddToggle("Flash Locally While Respotting", &ms_DebugFlashLocallyWhileRespotting);
	bank.AddToggle("Flash Remotely While Respotting", &ms_DebugFlashRemotelyWhileRespotting);
	bank.AddSlider("Respot counter to apply",		&ms_DebugRespotCounter,			0, 600000, 1);
	bank.AddButton("Set respot counter on debug focus vehicle", datCallback(CFA(DebugSetRespotCounterOnDebugFocusVehicle)));
}

void camInterface::DebugDisplayCameraInfoText()
{
	const int debugStringLength = 256;
	char debugString[debugStringLength];

	if(ms_DebugShouldDisplayActiveCameraInfo)
	{
		const camBaseCamera* renderedCamera	= camManager::GetDominantRenderedCamera();
		const char* renderedCameraName		= SAFE_CSTRING(renderedCamera ? renderedCamera->GetName() : NULL);
		if (renderedCamera && renderedCamera->GetIsClassId(camScriptedCamera::GetStaticClassId()))
		{
			const camScriptedCamera* pScriptedCamera = static_cast<const camScriptedCamera*>(renderedCamera);
			if ( pScriptedCamera->GetDebugName() != NULL && pScriptedCamera->GetDebugName()[0] != 0 )
			{
				renderedCameraName = pScriptedCamera->GetDebugName();
			}
		}
		const Vector3& position				= GetPos();

#if FPS_MODE_SUPPORTED
		if( renderedCamera &&
			(renderedCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()) ||
			 renderedCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId())) )
		{
			formatf(debugString, debugStringLength, "%s: %8.2f %8.2f %8.2f[Clip:%5.2f>%8.1f][DOF:%5.2f>%8.1f][FOV:%5.2f][HDG:%.2f][PCH:%.2f]",
				renderedCameraName, position.x, position.y, position.z, GetNear(), GetFar(), GetNearDof(), GetFarDof(), GetFov(), GetHeading(), GetPitch());

			if(renderedCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()) && Abs(GetRoll()) >= 0.001f)
				safecatf(debugString, debugStringLength, "[RLL:%2.3f]", GetRoll());

			if( GetMotionBlurStrength() > SMALL_FLOAT )
			{
				safecatf(debugString, debugStringLength, "[MB:%2.3f]", GetMotionBlurStrength());
			}

			if(renderedCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()))
			{
				const camFirstPersonShooterCamera* fpsCamera = (const camFirstPersonShooterCamera*)renderedCamera;
				if(Abs(fpsCamera->GetRelativeHeading()) > SMALL_FLOAT)
				{
					safecatf(debugString, debugStringLength, "[rH:%.3f]", fpsCamera->GetRelativeHeading());
				}
				if(Abs(fpsCamera->GetRelativePitch()) > SMALL_FLOAT)
				{
					safecatf(debugString, debugStringLength, "[rP:%.3f]", fpsCamera->GetRelativePitch());
				}
			}
		}
		else
#endif
		{
			formatf(debugString, debugStringLength, "%s: %8.2f %8.2f %8.2f[Clip:%5.2f>%8.2f][DOF:%5.2f>%8.2f][FOV:%5.2f][HDG:%.2f][MB:%2.3f]",
				renderedCameraName, position.x, position.y, position.z, GetNear(), GetFar(), GetNearDof(), GetFarDof(), GetFov(), GetHeading(),
				GetMotionBlurStrength());
		}

		GetScriptDirector().DebugBuildCameraStringForTextDisplay(debugString, debugStringLength);

		grcDebugDraw::AddDebugOutput(debugString);
	}

	if(ms_DebugShouldDisplayGameViewportInfo)
	{
		CViewport *viewport = gVpMan.GetGameViewport();
		if(viewport)
		{
			const grcViewport& grcVp = viewport->GetGrcViewport();
			const Vector2 shear = grcVp.GetPerspectiveShear();
			formatf(debugString, debugStringLength, "Game VPort:[AR %5.3f][WIN %dx%d][FOVY %5.3f][TANHFOV %5.3f][TANVFOV %5.3f][SHEARX %.1f][SHEARY %.1f]",
				grcVp.GetAspectRatio(), grcVp.GetWidth(), grcVp.GetHeight(), grcVp.GetFOVY(), grcVp.GetTanHFOV(), grcVp.GetTanVFOV(), shear.x, shear.y);
		}
		else
		{
			debugString[0] = '\0';
		}

		grcDebugDraw::AddDebugOutput(debugString);
	}

	const bool shouldDisplay = camManager::GetFramePropagator().ComputeDebugOutputText(debugString, debugStringLength);
	if(shouldDisplay)
	{
		grcDebugDraw::AddDebugOutput(debugString);
	}
}

void camInterface::DebugDumpSpectatorPedInfo()
{
	if(NetworkInterface::IsGameInProgress() && NetworkInterface::IsInSpectatorMode())
	{
		ObjectId objid = NetworkInterface::GetSpectatorObjectId();

		Printf("In Spectator Mode - Dump Network Object Id Info\n");
		Printf("================================================\n");
		Printf("Network Object Id=\"%d\"\n", objid);
		Printf("================================================\n");

		netObject* netobj = NetworkInterface::GetNetworkObject(objid);
		if(netobj)
		{
			NetworkObjectType objtype = netobj->GetObjectType();
			if (AssertVerify(objtype == NET_OBJ_TYPE_PLAYER || objtype == NET_OBJ_TYPE_PED))
			{
				CPed* ped = static_cast<CPed*>(static_cast<CNetObjPed*>(netobj)->GetPed());
				if(AssertVerify(ped))
				{
					cameraDisplayf("%s: Network ObjId#%d address %p pedID %d\n", objtype == NET_OBJ_TYPE_PLAYER ? "Player" : "Ped", objid, ped, CPed::GetPool()->GetIndex(ped));
				}
			}
		}
	}
}

void camInterface::DebugSetRespotCounterOnDebugFocusVehicle()
{
	CEntity* debugFocusEntity = CDebugScene::FocusEntities_Get(0);
	if(debugFocusEntity && debugFocusEntity->GetIsTypeVehicle())
	{
		static_cast<CVehicle*>(debugFocusEntity)->SetFlashLocallyDuringRespotting(ms_DebugFlashLocallyWhileRespotting);
		static_cast<CVehicle*>(debugFocusEntity)->SetFlashRemotelyDuringRespotting(ms_DebugFlashRemotelyWhileRespotting);
		static_cast<CVehicle*>(debugFocusEntity)->SetRespotCounter(ms_DebugRespotCounter);
	}
}

void camInterface::UpdateAtEndOfFrame()
{
	if (ms_DebugShouldRestartSystemAtEndOfFrame)
	{
		camManager::Shutdown(SHUTDOWN_SESSION);
		camManager::Init(INIT_SESSION);

		ms_DebugShouldRestartSystemAtEndOfFrame = false;
	}
}
#endif // __BANK

#if RSG_PC
bool camInterface::IsTripleHeadDisplay()
{	
	const float sceneWidth			= static_cast<float>(VideoResManager::GetSceneWidth());
	const float sceneHeight			= static_cast<float>(VideoResManager::GetSceneHeight());
	const float viewportAspect		= sceneWidth / sceneHeight;

    const float baseLineAspectRatio = 4.0f/3.0f;
    const float wideScreenAspectRatio = baseLineAspectRatio * baseLineAspectRatio; //16:9
    const float ultraWideScreenAspectRatio = wideScreenAspectRatio * baseLineAspectRatio; //21:9

    //we arbitrarily pick up a point in between the wide and ultra-wide aspect ratio. This because we often see windowed resolutions 
    //which have an aspect ratio bigger than 16:9 screen but smaller than 21:9
    const float minValueToConsiderTripleHead = (wideScreenAspectRatio + ultraWideScreenAspectRatio) * 0.5f; //mid point

	const bool isTripleHeadDisplay = viewportAspect >= minValueToConsiderTripleHead;

	TUNE_BOOL(FORCE_TRIPLEHEAD_CAMERA_CODE_EVERYWHERE, false);
	return isTripleHeadDisplay || FORCE_TRIPLEHEAD_CAMERA_CODE_EVERYWHERE;
}
#endif
