//
// camera/scripted/ScriptDirector.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/scripted/ScriptDirector.h"

#include "camera/CamInterface.h"
#include "camera/cinematic/CinematicDirector.h"
#include "camera/cutscene/AnimatedCamera.h"
#include "camera/cutscene/CutsceneDirector.h"
#include "camera/debug/DebugDirector.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera\helpers\ControlHelper.h"
#include "camera/helpers/FrameInterpolator.h"
#include "camera/scripted/ScriptedCamera.h"
#include "camera/system/CameraFactory.h"
#include "camera/system/CameraManager.h"
#include "camera/system/CameraMetadata.h"
#include "camera/viewports/ViewportManager.h"
#include "fwanimation/directorcomponentsyncedscene.h"
#include "peds/ped.h"
#include "script/script.h"
#include "script/handlers/GameScriptResources.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camScriptDirector,0x3A15A7C6)


camScriptDirector::camScriptDirector(const camBaseObjectMetadata& metadata)
: camBaseDirector(metadata)
, m_Metadata(static_cast<const camScriptDirectorMetadata&>(metadata))
{
	Reset();
}

camScriptDirector::~camScriptDirector()
{
	DestroyAllCameras();
}

bool camScriptDirector::Update()
{
	CleanUpCameraReferences();

	m_UpdatedCamera = NULL;

	//Update all active cameras. Note that these cameras are updated in order from the oldest to the most-recently activated.
	const s32 numActiveCameras = m_ActiveCameras.GetCount();
	for(s32 i=0; i<numActiveCameras; i++)
	{
		camBaseCamera* camera = m_ActiveCameras[i];
		if(camera && camera->BaseUpdate(m_Frame))
		{
			m_UpdatedCamera = camera;
		}
	}

#if FPS_MODE_SUPPORTED
	// reset the first person flash effect type
	SetFirstPersonFlashEffectType(camManager::CAM_PUSH_IN_FX_NEUTRAL);
	SetFirstPersonFlashEffectVehicleModelHash(0);
#endif // FPS_MODE_SUPPORTED

	SetRenderOptions(0); 
	return (m_UpdatedCamera.Get() != NULL);
}

void camScriptDirector::UpdateActiveRenderState()
{
	//If the cinematic director is currently rendering, we should not interpolate in or out, so skip the interpolation.
	const bool isCinematicDirectorRendering = camInterface::GetCinematicDirector().IsRendering();

	camFirstPersonAimCamera* pCam = camInterface::GetGameplayDirector().GetFirstPersonAimCamera(); 

	if(isCinematicDirectorRendering || pCam)
	{
		if(m_ActiveRenderState == RS_INTERPOLATING_IN)
		{
			m_ActiveRenderState = RS_FULLY_RENDERING;
		}
		else if(m_ActiveRenderState == RS_INTERPOLATING_OUT)
		{
			m_ActiveRenderState = RS_NOT_RENDERING;
		}
	}

	camBaseDirector::UpdateActiveRenderState();
}

void camScriptDirector::Reset()
{
	BaseReset();

	DestroyAllCameras();

	m_RenderedThreadIds.Reset();

#if FPS_MODE_SUPPORTED
	SetFirstPersonFlashEffectType(camManager::CAM_PUSH_IN_FX_NEUTRAL);
	SetFirstPersonFlashEffectVehicleModelHash(0);
#endif // FPS_MODE_SUPPORTED
#if __BANK
	m_DebugScriptTriggeredShakeName[0]		= '\0';
	m_DebugScriptTriggeredShakeAmplitude	= 0.0f;
#endif // __BANK
	SetRenderOptions(0); 
}

void camScriptDirector::Render(u32 interpolationDuration, bool shouldLockInterpolationSourceFrame)
{
	if(!cameraVerifyf(CTheScripts::GetUpdatingTheThreads(),"Script cameras can only be rendered from the script thread"))
	{
		return; 
	}

	const s32 numActiveCameras = m_ActiveCameras.GetCount();
	if(!cameraVerifyf(numActiveCameras > 0, "Please ensure a scripted camera is active before attempting to enable rendering"))
	{
		return;	//Don't render with no active cameras.
	}

	const GtaThread* activeScriptThread = CTheScripts::GetCurrentGtaScriptThread();
	if(activeScriptThread)
	{
		//Ensure the active script thread is represented in the rendered thread ID list.
		const scrThreadId activeScriptThreadId = activeScriptThread->GetThreadId();
		if(m_RenderedThreadIds.Find(activeScriptThreadId) == -1)
		{
			m_RenderedThreadIds.Grow() = activeScriptThreadId;
		}
	}

	camManager::AbortPendingFirstPersonFlashEffect("New script camera rendering");
	camBaseDirector::Render(interpolationDuration, shouldLockInterpolationSourceFrame);
}

void camScriptDirector::StopRendering(bool gameplayCatchUp, u32 interpolationDuration, bool shouldLockInterpolationSourceFrame, float distanceToBlend, int blendType)
{
	const GtaThread* activeScriptThread = CTheScripts::GetCurrentGtaScriptThread();

	StopRendering(gameplayCatchUp, interpolationDuration, shouldLockInterpolationSourceFrame, activeScriptThread, distanceToBlend, blendType);
}

void camScriptDirector::StopRendering(bool gameplayCatchUp, u32 interpolationDuration, bool shouldLockInterpolationSourceFrame, const GtaThread* scriptThread, float distanceToBlend, int blendType)
{
	if(scriptThread)
	{
		//Remove any references to the script thread from the rendered thread ID list.
		const scrThreadId activeScriptThreadId = scriptThread->GetThreadId();
		m_RenderedThreadIds.DeleteMatches(activeScriptThreadId);
	}

	//NOTE: This request is ignored if any script threads still expect the director to be rendering.
	const s32 numRenderedThreadIds = m_RenderedThreadIds.GetCount();
	if(numRenderedThreadIds == 0)
	{
		if (ShouldRequestFlashForFirstPersonExit(gameplayCatchUp, interpolationDuration))
		{
			camManager::TriggerFirstPersonFlashEffect( (camManager::eFirstPersonFlashEffectType)GetFirstPersonFlashEffectType(), GetFirstPersonFlashEffectDelay(interpolationDuration, gameplayCatchUp), "Script camera blendout");
		}

		camBaseDirector::StopRendering(interpolationDuration, shouldLockInterpolationSourceFrame);

		if (gameplayCatchUp)
		{
			const camFrame& catchUpSourceFrame = GetFrame();
			camInterface::GetGameplayDirector().CatchUpFromFrame(catchUpSourceFrame, distanceToBlend, blendType);
		}
	}
}

bool camScriptDirector::ShouldRequestFlashForFirstPersonExit(bool isCatchup, u32 interpolationDuration) const
{
	const u32 preDelay = 300u;

	// can't flash for catchups or blends into vehicles, because they do an instant cut to first person.
	// script need to add these flashes as appropriate.
	if (GetFirstPersonFlashEffectVehicleModelHash()!=0)
		return false;

	if(isCatchup)
	{
		// on foot catchups result in a 1 second blend before the cut to first person.
		interpolationDuration = 1000u;
	}

	/// can't flash if we're too late to handle the predelay
	if (interpolationDuration<preDelay)
		return false;

	// don't trigger if the cutscene director is rendering
	if (camInterface::GetCutsceneDirector().IsRendering())
		return false;

	// no more script cameras running, we're ready to blend out
	const s32 numRenderedThreadIds = m_RenderedThreadIds.GetCount();
	if(numRenderedThreadIds == 0 && IsRendering() && GetRenderState()!=RS_INTERPOLATING_OUT)
	{
		return WillExitToFirstPersonCamera(GetFirstPersonFlashEffectVehicleModelHash()); 
	}

	return false;
}

bool camScriptDirector::WillExitToFirstPersonCamera(u32 vehicleModelHash) const
{
	if(vehicleModelHash > 0)
	{	
		s32 viewMode = camControlHelperMetadataViewMode::IN_VEHICLE; 

		CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey(vehicleModelHash, NULL);

		cameraAssertf(pModelInfo, "Invalid model hash for players exit %d", vehicleModelHash); 


		if(pModelInfo)
		{
			if(pModelInfo->GetModelType() == MI_TYPE_VEHICLE)
			{
				viewMode = camControlHelper::ComputeViewModeContextForVehicle(vehicleModelHash); 

				cameraAssertf(viewMode != -1, "Failed to find the correct view mode for %s", 
					pModelInfo->GetModelName()); 
			}
			else
			{
				cameraAssertf(0, "The vehicle model hash that the player will use when exiting the custcene is not a vehicle model %d", 
					vehicleModelHash); 
			}
		}

		if(viewMode > -1 ) 
		{
			if(camControlHelperMetadataViewMode::FIRST_PERSON == camControlHelper::GetViewModeForContext(viewMode))
			{
				return true; 
			}
		}

		bool IsInFirstPesonModeOnFoot = camControlHelperMetadataViewMode::FIRST_PERSON == camControlHelper::GetViewModeForContext(camControlHelperMetadataViewMode::ON_FOOT); 

		if(IsInFirstPesonModeOnFoot && CPauseMenu::GetMenuPreference( PREF_FPS_PERSISTANT_VIEW ) == FALSE)
		{
			return true; 
		}
	}
	else
	{
		if(camControlHelperMetadataViewMode::FIRST_PERSON == camControlHelper::GetViewModeForContext(camControlHelperMetadataViewMode::ON_FOOT))
		{
			if(m_RenderingOptions.IsFlagSet(SCRIPT_DIRECTOR_STOP_RENDERING_OPTION_PLAYER_EXITS_INTO_COVER) && CPauseMenu::GetMenuPreference( PREF_FPS_THIRD_PERSON_COVER ))
			{
				return false;
			}
			else
			{
				return true; 
			}
		}
	}

	return false;
}

u32 camScriptDirector::GetFirstPersonFlashEffectType() const
{
	return m_FirstPersonFlashEffectType;
}

void camScriptDirector::SetFirstPersonFlashEffectType(u32 type)
{
	m_FirstPersonFlashEffectType = type;
}

u32 camScriptDirector::GetFirstPersonFlashEffectVehicleModelHash() const
{
	return m_FirstPersonFlashEffectVehicleModelHash;
}

void camScriptDirector::SetFirstPersonFlashEffectVehicleModelHash(u32 modelHash)
{
	m_FirstPersonFlashEffectVehicleModelHash = modelHash;
}

extern const u32 g_CatchUpHelperRefForFirstPerson;

u32 camScriptDirector::GetFirstPersonFlashEffectDelay(u32 blendOutDuration, bool bCatchup) const
{
	const u32 flashDelay = 300u;

	if (bCatchup)
	{
		const camCatchUpHelperMetadata* catchUpHelperMetadata = camFactory::FindObjectMetadata<camCatchUpHelperMetadata>(g_CatchUpHelperRefForFirstPerson);
		if(catchUpHelperMetadata)
		{
			return catchUpHelperMetadata->m_BlendDuration - flashDelay;
		}
		else
		{
			// for catchups, we trigger a hard coded 1 second blend out when in first person mode.
			return 1000u - flashDelay;
		}
	}
	else
	{
		if (blendOutDuration>=flashDelay)
		{
			return blendOutDuration - flashDelay;
		}
		else
		{
			return 0u;
		}
	}
}

void camScriptDirector::ForceStopRendering(bool gameplayCatchUp, u32 interpolationDuration, bool shouldLockInterpolationSourceFrame, float distanceToBlend, int blendType)
{
	//Clear the list of rendered thread IDs and force rendering to stop.

	m_RenderedThreadIds.Reset();

	if (ShouldRequestFlashForFirstPersonExit(gameplayCatchUp, interpolationDuration))
	{
		camManager::TriggerFirstPersonFlashEffect((camManager::eFirstPersonFlashEffectType)GetFirstPersonFlashEffectType(), GetFirstPersonFlashEffectDelay(interpolationDuration, gameplayCatchUp), "Script camera blendout");
	}

	camBaseDirector::StopRendering(interpolationDuration, shouldLockInterpolationSourceFrame);

	if (gameplayCatchUp)
	{
		const camFrame& catchUpSourceFrame = GetFrame();
		camInterface::GetGameplayDirector().CatchUpFromFrame(catchUpSourceFrame, distanceToBlend, blendType);
	}
}

void camScriptDirector::OnScriptTermination(const GtaThread* scriptThread)
{
	//If we're not already interpolating out, ensure that we don't continue rendering for this script thread.
	//NOTE: Rendering will continue if another script thread still requires it.
	if(m_ActiveRenderState != camBaseDirector::RS_INTERPOLATING_OUT)
	{
		StopRendering(false, 0, false, scriptThread);
	}
}

s32 camScriptDirector::CreateCamera(const char* cameraName)
{
	if(!cameraVerifyf(CTheScripts::GetUpdatingTheThreads(),"Script cameras can only be created from the script thread"))
	{
		return -1 ; 
	}
	
	const u32 nameHash	= atStringHash(cameraName);
	const s32 poolIndex	= CreateCamera(nameHash);

	cameraAssertf(poolIndex >= 0, "The script director failed to create a scripted camera (%s). Is it a valid camera name?", SAFE_CSTRING(cameraName));

	return poolIndex;
}

s32 camScriptDirector::CreateCamera(u32 cameraNameHash)
{
	if(!cameraVerifyf(CTheScripts::GetUpdatingTheThreads(),"Script cameras can only be created from the script thread"))
	{
		return -1; 
	}

	
	s32 poolIndex = -1;

	camBaseCamera* camera = camFactory::CreateObject<camBaseCamera>(cameraNameHash);
	if(cameraVerifyf(camera, "The script director failed to create a scripted camera (hash: %u)", cameraNameHash))
	{
		//Add a reference to this camera to our array.
		m_Cameras.Grow() = camera;

		poolIndex = camera->GetPoolIndex();
	}

	return poolIndex;
}

void camScriptDirector::DestroyCamera(s32 cameraIndex)
{
	camBaseCamera* camera = camBaseCamera::GetCameraAtPoolIndex(cameraIndex);
	if(camera != NULL)
	{
		//Verify that this camera is actually managed by the script director.
		RegdCamBaseCamera cameraRef(camera);
		bool isScriptManaged = (m_Cameras.Find(cameraRef) >= 0);

		if(cameraVerifyf(isScriptManaged, "A script has attempted to destroy a camera (name: %s, hash: %u) that was not created by script",
			camera->GetName(), camera->GetNameHash()))
		{
			delete camera;

			CleanUpCameraReferences();
		}
	}
}

void camScriptDirector::DestroyAllCameras()
{
	const s32 numCameras = m_Cameras.GetCount();
	//NOTE: We delete the cameras in reverse order so we can safely deal with any dependencies.
	for(s32 i=numCameras - 1; i>=0; i--)
	{
		camBaseCamera* camera = m_Cameras[i];
		if(camera)
		{
			delete camera;
		}
	}

	m_Cameras.Reset();
	m_ActiveCameras.Reset();
}

void camScriptDirector::ActivateCamera(s32 cameraIndex)
{
	DeactivateCamera(cameraIndex); //First ensure that this camera is not present in the list of active cameras.

	camBaseCamera* camera = camBaseCamera::GetCameraAtPoolIndex(cameraIndex);
	if(camera)
	{
		//Add this camera to the end of the list of active cameras, so it is updated last and will be the dominant camera.
		m_ActiveCameras.Grow() = camera;
	}
}

void camScriptDirector::DeactivateCamera(s32 cameraIndex)
{
	camBaseCamera* camera = camBaseCamera::GetCameraAtPoolIndex(cameraIndex);
	if(camera)
	{
		//Remove this camera from our list of active cameras.
		for(s32 i=0; i<m_ActiveCameras.GetCount(); )
		{
			if(m_ActiveCameras[i] == camera)
			{
				//NOTE: We must NULL the camera reference for safety, as Delete just shuffles the array elements.
				m_ActiveCameras[i] = NULL;
				m_ActiveCameras.Delete(i);
			}
			else
			{
				i++;
			}
		}
	}
}

bool camScriptDirector::IsCameraActive(s32 cameraIndex) const
{
	bool isActive = false;

	camBaseCamera* camera = camBaseCamera::GetCameraAtPoolIndex(cameraIndex);
	if(camera)
	{
		//See if this camera is in our list of active cameras.
		RegdCamBaseCamera cameraRef(camera);
		isActive = (m_ActiveCameras.Find(cameraRef) >= 0);
	}

	return isActive;
}

bool camScriptDirector::IsCameraScriptControllable(s32 cameraIndex) const
{
	bool isScriptControllable = false;

	camBaseCamera* camera = camBaseCamera::GetCameraAtPoolIndex(cameraIndex);
	if(camera)
	{
		//Note: The debug free camera is open to script control. Otherwise the camera must be owned by the script director.
		RegdCamBaseCamera cameraRef(camera);
		isScriptControllable = ((camBaseCamera*)camInterface::GetDebugDirector().GetFreeCam() == camera) || (m_Cameras.Find(cameraRef) >= 0);
	}

	return isScriptControllable;
}

void camScriptDirector::SetCameraFrameParameters(s32 cameraIndex, const Vector3& position, const Vector3& orientation, float fov, u32 duration,
	int graphTypePos, int graphTypeRot, int rotationOrder)
{
	camBaseCamera* camera = camBaseCamera::GetCameraAtPoolIndex(cameraIndex);
	if(camera && camera->GetIsClassId(camScriptedCamera::GetStaticClassId()))
	{
		camScriptedCamera* scriptedCamera = static_cast<camScriptedCamera*>(camera);

		if(duration == 0)
		{
			//Simply apply the frame parameters right away.

			//Terminate any existing interpolation, as the script wants to cut to the new parameters.
			camera->StopInterpolating();

			camFrame& frame = camera->GetFrameNonConst();

			if(!scriptedCamera->IsAttached())
			{
				//The camera position is being modified, so we must report a cut.
				frame.GetFlags().SetFlag(camFrame::Flag_HasCutPosition);

				frame.SetPosition(position);
			}

			if(!scriptedCamera->IsLookingAtSomething() && !scriptedCamera->IsInheritingRoll())
			{
				//The camera orientation is being modified, so we must report a cut.
				frame.GetFlags().SetFlag(camFrame::Flag_HasCutOrientation);

				Matrix34 worldMatrix;
				CScriptEulers::MatrixFromEulers(worldMatrix, orientation, static_cast<EulerAngleOrder>(rotationOrder));

				frame.SetWorldMatrix(worldMatrix);
			}

			frame.SetFov(fov);
		}
		else
		{
			//Interpolate to the frame parameters over the specified duration.

			RegdCamBaseCamera cameraRef(camera);
			int existingCameraIndexInActiveList = m_ActiveCameras.Find(cameraRef);
			if(!cameraVerifyf(existingCameraIndexInActiveList >= 0,
				"A camera must be activated before it is valid to set its parameters with interpolation"))
			{
				return;
			}

			//First clone the existing camera to use as the interpolation source.
			const u32 cameraNameHash = camera->GetNameHash();
			s32 sourceCameraIndex = CreateCamera(cameraNameHash);

			camBaseCamera* sourceCamera = camBaseCamera::GetCameraAtPoolIndex(sourceCameraIndex);
			if(!cameraVerifyf(sourceCamera, "Failed to create an interpolation source camera for script (hash: %u)", cameraNameHash))
			{
				return;
			}

			//NOTE: This should deal with cloning ALL relevant parameters for each camera implementation.
			sourceCamera->CloneFrom(camera);

			//Ensure the interpolation source camera starts active (but not rendering.)

			//Add the source camera immediately prior to the (existing) destination camera in the list of active cameras, so it is updated first.
			//NOTE: atArray does not expose an insert function that handles resizing, so we must implement this manually.
			DeactivateCamera(sourceCameraIndex);
			m_ActiveCameras.Grow();
			m_ActiveCameras.Delete(m_ActiveCameras.GetCount() - 1);
			m_ActiveCameras.Insert(existingCameraIndexInActiveList) = sourceCamera;

			//Apply the requested frame parameters to the (existing) destination camera.
			camFrame& destinationFrame = camera->GetFrameNonConst();

			if(!scriptedCamera->IsAttached())
			{
				destinationFrame.SetPosition(position);
			}

			if(!scriptedCamera->IsLookingAtSomething() && !scriptedCamera->IsInheritingRoll())
			{
				Matrix34 worldMatrix;
				CScriptEulers::MatrixFromEulers(worldMatrix, orientation, static_cast<EulerAngleOrder>(rotationOrder));

				destinationFrame.SetWorldMatrix(worldMatrix);
			}

			destinationFrame.SetFov(fov);

			//Interpolate from the source (clone) camera to the (existing) destination camera.
			//NOTE: The source camera will automatically be deleted on completion of the interpolation. The references to the source camera will then
			//be cleaned up within CleanUpCameraReferences() during the update.
			camera->InterpolateFromCamera(*sourceCamera, duration, true);

			camFrameInterpolator* interpolator = camera->GetFrameInterpolator();
			if(interpolator)
			{
				interpolator->SetCurveTypeForFrameComponent(camFrame::CAM_FRAME_COMPONENT_POS, (camFrameInterpolator::eCurveTypes)graphTypePos);
				interpolator->SetCurveTypeForFrameComponent(camFrame::CAM_FRAME_COMPONENT_MATRIX, (camFrameInterpolator::eCurveTypes)graphTypeRot);
			}
		}
	}
}

bool camScriptDirector::AnimateCamera(s32 cameraIndex, const strStreamingObjectName animDictName, const crClip& clip, const Matrix34& sceneOrigin, int sceneId /*=-1*/, u32 iFlags /*= 0*/)
{
	bool hasSucceeded = false;

	camAnimatedCamera* animatedCamera = GetAnimatedCamera(cameraIndex);
	if(animatedCamera)
	{
		animatedCamera->Init();
		animatedCamera->SetShouldUseLocalCameraTime(true);
		//If the clip contains a scene origin/offset, apply it to the script-specified origin.
		Matrix34 finalSceneOrigin(sceneOrigin);
		fwAnimHelpers::ApplyInitialOffsetFromClip(clip, finalSceneOrigin);
		animatedCamera->SetSceneMatrix(finalSceneOrigin);

		float currentTimeInSeconds = 0.0f;
		if(fwAnimDirectorComponentSyncedScene::IsValidSceneId((fwSyncedSceneId)sceneId))
		{
			currentTimeInSeconds = fwAnimDirectorComponentSyncedScene::GetSyncedSceneTime((fwSyncedSceneId)sceneId);
		}
		else
		{
			currentTimeInSeconds = fwTimer::GetCamTimeInMilliseconds() * 0.001f;
		}
		if(cameraVerifyf(animatedCamera->SetCameraClip(VEC3_ZERO, animDictName, &clip, currentTimeInSeconds, iFlags),
			"Failed to initialise the animation for a script animated camera"))
		{
			animatedCamera->SetCurrentTime(currentTimeInSeconds);
			animatedCamera->UpdateFrame(0.0f);	//Set initial phase (for translation, rotation etc.)
			
			if (fwAnimDirectorComponentSyncedScene::IsValidSceneId((fwSyncedSceneId)sceneId))
			{
				animatedCamera->SetSyncedScene((fwSyncedSceneId)sceneId);
			}

			animatedCamera->GetFrameNonConst().GetFlags().SetFlag(camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation);

			hasSucceeded = true;

// #if __BANK
// 			animatedCamera->DebugSpewAnimationFrames();
// #endif // __BANK
		}
	}

	return hasSucceeded;
}

bool camScriptDirector::IsCameraPlayingAnimation(s32 cameraIndex, const strStreamingObjectName animDictName, const crClip& clip)
{
	bool isPlaying = false;

	camAnimatedCamera* animatedCamera = GetAnimatedCamera(cameraIndex);
	if(animatedCamera)
	{
		isPlaying = animatedCamera->IsPlayingAnimation(animDictName, &clip);
	}

	return isPlaying;
}

void camScriptDirector::SetCameraAnimationPhase(s32 cameraIndex, float phase)
{
	camAnimatedCamera* animatedCamera = GetAnimatedCamera(cameraIndex);
	if(animatedCamera)
	{
		animatedCamera->SetPhase(phase);
		//This interface could be used to implement a cut, so we must report one.
		animatedCamera->GetFrameNonConst().GetFlags().SetFlag(camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation);
	}
}

float camScriptDirector::GetCameraAnimationPhase(s32 cameraIndex)
{
	float phase = 0.0f;

	camAnimatedCamera* animatedCamera = GetAnimatedCamera(cameraIndex);
	if(animatedCamera)
	{
		phase = animatedCamera->GetPhase();
	}

	return phase;
}

camAnimatedCamera* camScriptDirector::GetAnimatedCamera(s32 cameraIndex)
{
	camAnimatedCamera* animatedCamera = NULL;

	camBaseCamera* camera = camBaseCamera::GetCameraAtPoolIndex(cameraIndex);
	if(camera && cameraVerifyf(camera->GetIsClassId(camAnimatedCamera::GetStaticClassId()),
		"Animations must be played using an animated camera, e.g. \"DEFAULT_ANIMATED_CAMERA\""))
	{
		animatedCamera = static_cast<camAnimatedCamera*>(camera);
	}

	return animatedCamera;
}

void camScriptDirector::CleanUpCameraReferences()
{
	//Remove any NULL camera references from the storage array.
	for(s32 i=0; i<m_Cameras.GetCount(); )
	{
		if(!m_Cameras[i])
		{
			m_Cameras.Delete(i);
		}
		else
		{
			i++;
		}
	}

	//Also remove any NULL camera references from the list of active cameras.
	for(s32 i=0; i<m_ActiveCameras.GetCount(); )
	{
		if(!m_ActiveCameras[i])
		{
			m_ActiveCameras.Delete(i);
		}
		else
		{
			i++;
		}
	}
}

#if __BANK

const camAnimatedCamera* camScriptDirector::GetAnimatedCamera(s32 cameraIndex) const
{
	const camAnimatedCamera* animatedCamera = NULL;

	camBaseCamera* camera = camBaseCamera::GetCameraAtPoolIndex(cameraIndex);
	if(camera && cameraVerifyf(camera->GetIsClassId(camAnimatedCamera::GetStaticClassId()),
		"Animations must be played using an animated camera, e.g. \"DEFAULT_ANIMATED_CAMERA\""))
	{
		animatedCamera = static_cast<camAnimatedCamera*>(camera);
	}

	return animatedCamera;
}

void camScriptDirector::AddWidgetsForInstance()
{
	if(m_WidgetGroup == NULL)
	{
		bkBank* bank = BANKMGR.FindBank("Camera");
		if(bank != NULL)
		{
			m_WidgetGroup = bank->PushGroup("Script director", false);
			{
				bank->AddButton("Dump root camera frame params to script",
					datCallback(MFA(camScriptDirector::DebugDumpRootCamFrameParamsToScript), this),
					"Dumps the current root camera frame parameters to a script debug file in the form of a set of camera commands");
				bank->AddButton("Dump root camera matrix relative to player to script",
					datCallback(MFA(camScriptDirector::DebugDumpRootCameraMatrixRelativeToPlayerToScript), this),
					"Dumps the current root camera frame parameters (relative to the player ped) to a script debug file in the form of a set of camera commands");

				bank->AddButton("Reset the script director", datCallback(MFA(camScriptDirector::Reset), this),
					"Resets the script director. Deleting all cameras, killing any interpolation and stopping rendering");

				bank->AddButton("Toggle widescreen borders active", datCallback(MFA(camScriptDirector::DebugToggleWidescreenBordersActive), this));
			}
			bank->PopGroup(); //Script director.
		}
	}
}

void camScriptDirector::DebugDumpRootCamFrameParamsToScript()
{
	const Vector3& position		= camInterface::GetPos();
	Vector3 orientationEulers	= camInterface::GetMat().GetEulers("yxz");
	float fov					= camInterface::GetFov();
	DebugDumpCameraFrameParamsToScript(position, orientationEulers, fov);
}

void camScriptDirector::DebugDumpRootCameraMatrixRelativeToPlayerToScript()
{
	Matrix34 camMatrix		= camInterface::GetMat();
	const CPed* followPed	= camInterface::FindFollowPed();
	if(followPed)
	{
		camMatrix.DotTranspose(MAT34V_TO_MATRIX34(followPed->GetMatrix()));
		const Vector3& position = camMatrix.d;
		Vector3 orientationEulers;
		camMatrix.ToEulersYXZ(orientationEulers);

		float fov = camInterface::GetFov();

		const u32 stringLength = 255;
		char str[stringLength];
		formatf(str, stringLength, "SPLINE_NODE	POS		%f	%f	%f		ANG		%f	%f	%f	FOV		%f\n", position.x, position.y, position.z,
			orientationEulers.x, orientationEulers.y, orientationEulers.z, fov);
		cameraDisplayf("%s", str);

		DebugDumpCameraFrameParamsToScript(position, orientationEulers, fov);
	}
}

void camScriptDirector::DebugDumpCameraFrameParamsToScript(const Vector3& position, const Vector3& orientationEulers, const float fov)
{
	const u32 lineLength = 255;
	char line[lineLength];
	FileHandle file;

	file = CFileMgr::OpenFileForAppending(CScriptDebug::GetNameOfScriptDebugOutputFile());
	if(cameraVerifyf(CFileMgr::IsValidFileHandle(file), "Failed to open the script dump file for camera commands (%s)", CScriptDebug::GetNameOfScriptDebugOutputFile()))
	{
		formatf(line, lineLength, "SET_CAM_COORD(_cams_index,<<%f,%f,%f>>)\r\n", position.x, position.y, position.z);
		if(cameraVerifyf(CFileMgr::Write(file, line, istrlen(line)) > 0, "Failed to write a camera command to the script dump file"))
		{
			Vector3 rotation = orientationEulers * RtoD;
			formatf(line, lineLength, "SET_CAM_ROT(_cams_index,<<%f,%f,%f>>)\r\n", rotation.x, rotation.y, rotation.z);
			if(cameraVerifyf(CFileMgr::Write(file, line, istrlen(line)) > 0, "Failed to write a camera command to the script dump file"))
			{
				if(fov > 0.0f)
				{
					formatf(line, lineLength, "SET_CAM_FOV(_cams_index,%f)\r\n", fov);
					if(CFileMgr::Write(file, line, istrlen(line)) <= 0)
					{
						cameraErrorf("Failed to write a camera command to the script dump file");
					}
				}
				formatf(line, lineLength, "\nSET_CAM_PARAMS(_cams_index,<<%f,%f,%f>>,<<%f,%f,%f>>,%f)\r\n\n", position.x, position.y, position.z, rotation.x, rotation.y, rotation.z, fov);
				(void)cameraVerifyf(CFileMgr::Write(file, line, istrlen(line)) > 0, "Failed to write a camera command to the script dump file");
			}
		}

		CFileMgr::CloseFile(file);
	}
}

void camScriptDirector::DebugToggleWidescreenBordersActive()
{
	gVpMan.SetWidescreenBorders(!gVpMan.AreWidescreenBordersActive());
}

const char* camScriptDirector::DebugBuildCameraStringForTextDisplay(char* string, u32 maxLength)
{
	const bool isRendering = IsRendering();
	if(isRendering)
	{
		safecatf(string, maxLength, " - Script cams: %d (%d active)", m_Cameras.GetCount(), m_ActiveCameras.GetCount());
	}

	return string;
}

const char*	camScriptDirector::DebugAppendScriptControlledCameraDetails(const camBaseCamera& camera, char* string, u32 maxLength) const
{
	const s32 cameraPoolIndex				= camera.GetPoolIndex();
	const scriptHandler* ownerScriptHandler	= CTheScripts::GetScriptHandlerMgr().GetScriptHandlerForResource(
												CGameScriptResource::SCRIPT_RESOURCE_CAMERA, cameraPoolIndex);
	if(ownerScriptHandler)
	{
		const GtaThread* ownerScriptThread = static_cast<const GtaThread*>(ownerScriptHandler->GetThread());
		if(ownerScriptThread)
		{
			const char* ownerScriptName = const_cast<GtaThread*>(ownerScriptThread)->GetScriptName();
			safecatf(string, maxLength, ", ScriptThread=%s, CameraId=%d", ownerScriptName ? ownerScriptName : "Unknown", cameraPoolIndex);
		}
	}

	return string;
}

void camScriptDirector::DebugGetCameras(atArray<camBaseCamera*> &cameraList) const
{
	if(m_Cameras.GetCount() > 0)
	{
		for(s32 i=0; i<m_Cameras.GetCount(); i++)
		{
			camBaseCamera* camera = m_Cameras[i];
			if(camera)
			{
				cameraList.PushAndGrow(camera);
			}
		}
	}
}

void camScriptDirector::DebugSetScriptEnabledShakeName(const char* shakeName)
{
	sprintf(m_DebugScriptTriggeredShakeName, "%s", shakeName);
}

void camScriptDirector::DebugSetScriptEnabledShakeAmplitude(float amplitudeScalar)
{
	m_DebugScriptTriggeredShakeAmplitude = amplitudeScalar;
}

void camScriptDirector::DebugSetScriptDisabledShake()
{
	m_DebugScriptTriggeredShakeName[0]		= '\0';
	m_DebugScriptTriggeredShakeAmplitude	= 0.0f;
}

const char* camScriptDirector::DebugGetScriptEnabledShakeName() const
{
	return m_DebugScriptTriggeredShakeName;
}

float camScriptDirector::DebugGetScriptEnabledShakeAmplitude() const
{
	return m_DebugScriptTriggeredShakeAmplitude;
}

#endif // __BANK
