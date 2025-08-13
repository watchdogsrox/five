// 
// camera/system/CameraManager.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/system/CameraManager.h"

#include "bank/bkmgr.h"

#include "fwanimation/directorcomponentmotiontree.h"
#include "grcore/debugdraw.h"
#include "grcore/quads.h"
#include "fwsys/gameskeleton.h"
#include "fwsys/timer.h"
#include "system/bootmgr.h"

#include "audio/frontendaudioentity.h"
#include "debug/Bar.h"
#include "camera/CamInterface.h"
#include "camera/base/BaseDirector.h"
#include "camera/cinematic/CinematicDirector.h"
#include "camera/cinematic/camera/animated/CinematicAnimatedCamera.h"
#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"
#include "camera/cinematic/camera/tracking/CinematicTrainTrackCamera.h"
#include "camera/cinematic/camera/tracking/CinematicFirstPersonIdleCamera.h"
#include "camera/cutscene/CutsceneDirector.h"
#include "camera/debug/FreeCamera.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/ThirdPersonCamera.h"
#include "camera/gameplay/aim/FirstPersonShooterCamera.h"
#include "camera/helpers/ControlHelper.h"
#include "camera/helpers/Envelope.h"
#include "camera/helpers/FrameInterpolator.h"
#include "camera/helpers/FramePropagator.h"
#include "camera/helpers/FrameShaker.h"
#include "camera/helpers/NearClipScanner.h"
#include "camera/replay/ReplayDirector.h"
#include "camera/scripted/ScriptDirector.h"
#include "camera/scripted/ScriptedCamera.h"
#include "camera/system/CameraFactory.h"
#include "camera/system/CameraMetadata.h"
#include "camera/viewports/ViewportManager.h"
#include "cutscene/CutSceneManagerNew.h"
#include "peds/ped.h"
#include "renderer/PostProcessFXHelper.h"
#include "script/script.h"
#include "system/controlMgr.h"
#include "scene/EntityIterator.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "text/TextConversion.h"
#include "weapons/Weapon.h"
#include "vehicles/MetaData/VehicleSeatInfo.h"

bool DebugDisplayHangingCamera(void* pItem, void* UNUSED_PARAM(data));

CAMERA_OPTIMISATIONS()

#if __BANK
PARAM(cameraWidgets, "[camManager] Create camera widget bank on game launch");
#endif // __BANK

//NOTE: These thresholds are intended to reject invalid cut reports, rather than detect cuts.
const float g_MinTranslation2ForPositionCut	= (0.01f * 0.01f);
const float g_MinRotationForOrientationCut	= (0.5f * DtoR);

atArray<RegdCamBaseDirector>			camManager::ms_Directors;
atArray<tRenderedCameraObjectSettings>	camManager::ms_RenderedDirectors;
atArray<tRenderedCameraObjectSettings>	camManager::ms_RenderedCameras;
RegdConstCamBaseDirector				camManager::ms_DominantRenderedDirector;
RegdConstCamBaseCamera					camManager::ms_DominantRenderedCamera;

RegdPed				camManager::ms_PedMadeInvisible;
RegdVeh				camManager::ms_VehicleMadeInvisible;

camFramePropagator	camManager::ms_FramePropagator;

#if FPS_MODE_SUPPORTED
atHashString camManager::ms_FirstPersonFlashEffectName[camManager::FIRST_PERSON_FLASH_EFFECT_COUNT] =
{
	atHashString("CamPushInNeutral",	0x15b714f1),	// CAM_PUSH_IN_FX_NEUTRAL
	atHashString("CamPushInMichael",	0xb040f488),	// CAM_PUSH_IN_FX_MICHAEL
	atHashString("CamPushInFranklin",	0xee41dacb),	// CAM_PUSH_IN_FX_FRANKLIN
	atHashString("CamPushInTrevor",		0xe3bb0589)		// CAM_PUSH_IN_FX_TREVOR
};

camManager::eFirstPersonFlashEffectType camManager::ms_CurrentFirstPersonFlashEffect = camManager::CAM_PUSH_IN_FX_NEUTRAL;
camManager::eFirstPersonFlashEffectType camManager::ms_PendingFirstPersonFlashEffect = camManager::CAM_PUSH_IN_FX_NEUTRAL;

bool				camManager::ms_IsRenderingFirstPersonShooterCamera					= false;
bool				camManager::ms_IsRenderingFirstPersonShooterCustomFallBackCamera	= false;
bool				camManager::ms_SuppressFirstPersonFlashEffectThisFrame				= false;
u32					camManager::ms_TriggerFirstPersonFlashEffectTime					= 0;
bool				camManager::ms_WasInFirstPersonMode									= false;

#if __BANK
atHashString		camManager::ms_FirstPersonFlashEffectCallingContext;
atHashString		camManager::ms_LastFirstPersonFlashContext;
u32					camManager::ms_LastFirstPersonFlashTime=0;
atHashString		camManager::ms_LastFirstPersonFlashAbortReason;
u32					camManager::ms_LastFirstPersonFlashAbortTime=0;
#endif //__BANK 
#endif // FPS_MODE_SUPPORTED

#if __BANK
RegdConstCamBaseCamera		camManager::ms_DebugSelectedCamera;
RegdConstCamBaseDirector	camManager::ms_DebugSelectedDirector;
camEnvelopeMetadata	camManager::ms_DebugFullScreenBlurEffectEnvelopeMetadata;
camEnvelope*		camManager::ms_DebugFullScreenBlurEffectEnvelope = NULL;
bool				camManager::ms_ShouldDebugFullScreenBlurEffectUseGameTime = true;
bool				camManager::ms_ShouldDebugRenderCameraTable		= false;
bool				camManager::ms_ShouldDebugRenderCameras			= false;
bool				camManager::ms_ShouldDebugRenderCameraFarClip	= false;
bool				camManager::ms_ShouldDebugRenderCameraNearDof	= false;
bool				camManager::ms_ShouldDebugRenderCameraFarDof	= false;
bool				camManager::ms_ShouldDebugRenderThirdLines		= false;
bool				camManager::ms_ShouldRenderCameraInfo			= false; 
bool				camManager::ms_ShouldRenderUnapprovedAnimatedCameraText			= false;
float				camManager::ms_UnapprovedAnimatedCameraTextTimer = 0.0f;
u8					camManager::ms_DebugRenderOption				= 0;

rage::atVector<camManager::DebugScriptCommand> camManager::ms_DebugRegisteredScriptCommands;

#if GTA_REPLAY
bool				camManager::ms_DebugReplayCameraMovementDisabledThisFrame = false;
bool				camManager::ms_DebugReplayCameraMovementDisabledThisFrameScript = false;
#endif // GTA_REPLAY
#endif // __BANK

#if __BANK
camShakeDebugger	camManager::ms_ShakeDebugger;
#endif

#if __BANK
const u8 g_DebugRenderTableRowOffset = 3;
const u8 g_DebugRenderTableColumnOffset = 14;
const u8 g_DebugRenderTableColumnWidths[22] =
{
	6,	// DIRECTOR_NAME_COLUMN
	3,	// DIRECTOR_BLEND_COLUMN
	3,	// SELECTED_COLUMN
	45,	// CAMERA_NAME_COLUMN
	3,	// UPDATING_COLUMN
	3,	// BLEND_COLUMN
	8,	// POSITION_X_COLUMN
	8,	// POSITION_Y_COLUMN
	7,	// POSITION_Z_COLUMN
	7,	// ORIENTATION_P_COLUMN
	8,	// ORIENTATION_R_COLUMN
	7,	// ORIENTATION_Y_COLUMN
	7,	// NEAR_CLIP_COLUMN
	7,	// FAR_CLIP_COLUMN
	7,	// FLAGS_COLUMN
	8,	// DOF_1_COLUMN
	8,	// DOF_2_COLUMN
	8,	// DOF_3_COLUMN
	8,	// DOF_4_COLUMN
	6,	// FOV_COLUMN
	6,	// MOTION_BLUR_COLUMN
	6,	// INFO_COLUMN
};
const Color32 g_DebugRenderTableColumnColors[22] =
{
	Color_black, // DIRECTOR_NAME_COLUMN
	Color_black, // DIRECTOR_BLEND_COLUMN
	Color_black, // SELECTED_COLUMN
	Color_black, // CAMERA_NAME_COLUMN
	Color_black, // UPDATING_COLUMN
	Color_black, // BLEND_COLUMN
	Color_grey30,// POSITION_X_COLUMN
	Color_grey30,// POSITION_Y_COLUMN
	Color_grey30,// POSITION_Z_COLUMN
	Color_black, // ORIENTATION_P_COLUMN
	Color_black, // ORIENTATION_R_COLUMN
	Color_black, // ORIENTATION_Y_COLUMN
	Color_grey30,// NEAR_CLIP_COLUMN
	Color_grey30,// FAR_CLIP_COLUMN
	Color_black, // FLAGS_COLUMN
	Color_grey30,// DOF_1_COLUMN
	Color_grey30,// DOF_2_COLUMN
	Color_grey30,// DOF_3_COLUMN
	Color_grey30,// DOF_4_COLUMN
	Color_black, // FOV_COLUMN
	Color_grey30,// MOTION_BLUR_COLUMN
	Color_black, // INFO_COLUMN
};
#endif // __BANK

#if RSG_PC
float camManager::ms_fStereoConvergence = 1.0f;
#endif

void camManager::Init(unsigned initMode)
{
	USE_MEMBUCKET(MEMBUCKET_GAMEPLAY);

    if(initMode == INIT_SESSION)
    {
	    camFactory::Init();

		//Create the directors.
		const camMetadataStore* metadataStore = camFactory::GetMetadataStore();
		if(metadataStore)
		{
			const int numDirectors = metadataStore->m_DirectorList.GetCount();
			for(int i=0; i<numDirectors; i++)
			{
				const camBaseDirectorMetadata* metadata = metadataStore->m_DirectorList[i];
				if(metadata)
				{
					camBaseDirector* director = camFactory::CreateObject<camBaseDirector>(*metadata);
					if(cameraVerifyf(director, "Failed to create a camera director (name: %s, hash: %u)",
						SAFE_CSTRING(metadata->m_Name.GetCStr()), metadata->m_Name.GetHash()))
					{
						ms_Directors.Grow() = director;
					}
				}
			}
#if __BANK
			ms_ShakeDebugger.Initialise(*metadataStore);
#endif
		}
#if __BANK
		LoadUnapprovedList();

		DebugInitFullScreenBlurEffect();
#endif // __BANK

		camInterface::Init();

		ms_RenderedDirectors.Reset();
		ms_RenderedCameras.Reset();
		ms_DominantRenderedDirector	= NULL;
		ms_DominantRenderedCamera	= NULL;

	    ms_FramePropagator.Init();
    }
}

void camManager::Shutdown(unsigned shutdownMode)
{
	USE_MEMBUCKET(MEMBUCKET_GAMEPLAY);

    if(shutdownMode == SHUTDOWN_SESSION)
    {
		camInterface::Shutdown();

		//Clean up the directors.
		const int numDirectors = ms_Directors.GetCount();
		for(int i=0; i<numDirectors; i++)
		{
			camBaseDirector* director = ms_Directors[i];
			if(director)
			{
				delete director;
			}
		}

		ms_Directors.Reset();
		ms_RenderedDirectors.Reset();
		ms_RenderedCameras.Reset();
		ms_DominantRenderedDirector	= NULL;
		ms_DominantRenderedCamera	= NULL;
		ms_FramePropagator.Shutdown();
#if __BANK
		ms_UnapprovedCameraLists.m_UnapprovedAnimatedCameras.Reset(); 

		DebugAbortFullScreenBlurEffect();

		ms_ShakeDebugger.Shutdown();
#endif // __BANK

#if __DEV
	    camBaseCamera::GetPool()->ForAll(DebugDisplayHangingCamera, NULL);
#endif // __DEV

	    camBaseCamera::GetPool()->DeleteAll(); //Guarantee the pool is empty.

	    camFactory::Shutdown();
    }
}

void camManager::Update()
{
	PF_PUSH_TIMEBAR_DETAIL("Camera Manager Update");

	const CPed* followPed = camInterface::FindFollowPed();
	bool isPlayerPed = followPed && followPed->GetPlayerInfo();
	bool wasInFirstPersonMode = false;

#if FPS_MODE_SUPPORTED
	if(isPlayerPed)
	{
		wasInFirstPersonMode = ms_WasInFirstPersonMode;
	}
#endif

	UpdateInternal();

	bool inFirstPersonMode = false;
	bool secondUpdateOnPlayerPed = false;
	bool secondUpdateOnPlayerWeapon = false;
	CWeapon* pWeapon = NULL;
	CPed* followPedNonConst = NULL;
	
	if(isPlayerPed)
	{
		followPedNonConst = const_cast<CPed*>(followPed);
		inFirstPersonMode = followPed->IsFirstPersonShooterModeEnabledForPlayer(false, false, true);
		secondUpdateOnPlayerPed = followPed->GetIsInCover() || followPed->GetPedResetFlag(CPED_RESET_FLAG_CheckFPSSwitchInCameraUpdate);

		pWeapon = followPedNonConst->GetWeaponManager() ? followPedNonConst->GetWeaponManager()->GetEquippedWeapon() : NULL;
		if(pWeapon)
		{
			if(pWeapon->GetWeaponHash() == ATSTRINGHASH("WEAPON_MICROSMG", 0x13532244) &&
			   camInterface::GetGameplayDirector().IsFirstPersonModeEnabled() && 
			   (camInterface::GetGameplayDirector().GetFirstPersonShooterCamera() == NULL ||
				 camInterface::IsDominantRenderedDirector(camInterface::GetScriptDirector()) || 
				 camInterface::GetScriptDirector().IsRendering()))
			{
				secondUpdateOnPlayerWeapon = true;
			}
		}

	}

	if((inFirstPersonMode != wasInFirstPersonMode) && followPedNonConst)
	{
		if(secondUpdateOnPlayerPed)
		{
			// We need to set this so CTaskMotionInCover::ShouldRestartStateDueToCameraSwitch restarts the anims this frame
			followPedNonConst->SetPlayerResetFlag(CPlayerResetFlags::PRF_CAMERA_VIEW_MODE_SWITCHED_TO_OR_FROM_FIRST_PERSON);
			if(inFirstPersonMode)
				followPedNonConst->SetPlayerResetFlag(CPlayerResetFlags::PRF_CAMERA_VIEW_MODE_SWITCHED_TO_FIRST_PERSON);

			followPedNonConst->InstantAIUpdate();

			fwAnimDirectorComponentMotionTree::SetLockedGlobal(true, fwAnimDirectorComponent::kPhasePrePhysics);
			followPedNonConst->InstantAnimUpdateStart(); 
			followPedNonConst->InstantAnimUpdateEnd(); 
			fwAnimDirectorComponentMotionTree::SetLockedGlobal(false, fwAnimDirectorComponent::kPhasePrePhysics);

			// Second camera update, now the ped should be posed correctly
			UpdateInternal();
		}

		if(secondUpdateOnPlayerWeapon)
		{
			CWeapon* pWeapon = followPedNonConst->GetWeaponManager() ? followPedNonConst->GetWeaponManager()->GetEquippedWeapon() : NULL; 
			if(pWeapon)
			{
				pWeapon->ProcessAnimation(followPedNonConst);
			}
		}
	}

#if FPS_MODE_SUPPORTED
	if(isPlayerPed)
	{
		ms_WasInFirstPersonMode = inFirstPersonMode;
	}
#endif

#if __BANK
	DebugFlushScriptCommands();
#endif //__BANK

	PF_POP_TIMEBAR_DETAIL();
}

void camManager::UpdateInternal()
{
	USE_MEMBUCKET(MEMBUCKET_GAMEPLAY);

	camInterface::GetCutsceneDirector().UpdateGameplayCameraForFirstPersonBlendOut();

	PreUpdateCameras();

	//NOTE: We use a safe reference to ensure that deletion is tracked during the director updates.
	RegdConstCamBaseCamera dominantRenderedCameraOnPreviousUpdate(ms_DominantRenderedCamera);

	atArray<tRenderedCameraObjectSettings> tempRenderedDirectors;
	atArray<tRenderedCameraObjectSettings> tempRenderedCameras;
	camFrame gameplayFrame;

	const int numDirectors = ms_Directors.GetCount();
	for(int i=0; i<numDirectors; i++)
	{
		camBaseDirector* director = ms_Directors[i];
		if(!director)
		{
			continue;
		}

		director->BaseUpdate(gameplayFrame);

		const float blendLevelOfDirector = director->GetInterpolationBlendLevel();
		if(blendLevelOfDirector < SMALL_FLOAT)
		{
			continue;
		}

		//This director is rendering, so add it to the director list.

		//First we must appropriately scale the blend levels of all directors that rendered earlier in the update.
		const float blendLevelOfExistingRenderedObjects = 1.0f - blendLevelOfDirector;

		const s32 numRenderedDirectors = tempRenderedDirectors.GetCount();
		for(s32 directorIndex=0; directorIndex<numRenderedDirectors; directorIndex++)
		{
			tempRenderedDirectors[directorIndex].m_BlendLevel *= blendLevelOfExistingRenderedObjects;
		}

		tRenderedCameraObjectSettings* directorSettings	= &tempRenderedDirectors.Grow();
		directorSettings->m_Object						= director;
		directorSettings->m_BlendLevel					= blendLevelOfDirector;

		const camBaseCamera* renderedCamera = director->GetRenderedCamera();
		if(!renderedCamera)
		{
			continue;
		}

		//This director is rendering a camera, so add it (and any interpolation source cameras) to the camera list.

		//First we must appropriately scale the blend levels of all cameras that rendered earlier in the update.
		const s32 numRenderedCameras = tempRenderedCameras.GetCount();
		for(s32 cameraIndex=0; cameraIndex<numRenderedCameras; cameraIndex++)
		{
			tempRenderedCameras[cameraIndex].m_BlendLevel *= blendLevelOfExistingRenderedObjects;
		}

		tRenderedCameraObjectSettings* settings	= &tempRenderedCameras.Grow();
		settings->m_Object						= renderedCamera;
		settings->m_BlendLevel					= blendLevelOfDirector;

		//Now deal with any interpolation source cameras.

		const camBaseCamera* interpolationDestinationCamera = renderedCamera;
		while(true)
		{
			const camFrameInterpolator* frameInterpolator	= interpolationDestinationCamera->GetFrameInterpolator();
			const camBaseCamera* interpolationSourceCamera	= frameInterpolator ? frameInterpolator->GetSourceCamera() : NULL;
			if(!interpolationSourceCamera)
			{
				break;
			}

			const float overallBlendLevelForInterpolation	= settings->m_BlendLevel;
			const float blendLevelForDestinationCamera		= frameInterpolator->GetBlendLevel();
			const float blendLevelForSourceCamera			= 1.0f - blendLevelForDestinationCamera;
			if(blendLevelForSourceCamera < SMALL_FLOAT)
			{
				break;
			}

			//Update the blend level for the interpolation destination camera, previously processed.
			settings->m_BlendLevel	*= blendLevelForDestinationCamera;

			//Add the interpolation source camera to the list, with an appropriate blend level (ignoring any further interpolation stages for now.)
			settings				= &tempRenderedCameras.Grow();
			settings->m_Object		= interpolationSourceCamera;
			settings->m_BlendLevel	= overallBlendLevelForInterpolation * blendLevelForSourceCamera;

			interpolationDestinationCamera = interpolationSourceCamera;
		};
	}

	RemoveBlendedOutRenderedObjects(tempRenderedDirectors);
	RemoveBlendedOutRenderedObjects(tempRenderedCameras);

	ms_RenderedDirectors		= tempRenderedDirectors;
	ms_DominantRenderedDirector	= static_cast<const camBaseDirector*>(ComputeDominantRenderedObject(ms_RenderedDirectors));

	ms_RenderedCameras			= tempRenderedCameras;
	ms_DominantRenderedCamera	= static_cast<const camBaseCamera*>(ComputeDominantRenderedObject(ms_RenderedCameras));

	const camFrame& gameplayFrameOnPreviousUpdate = camInterface::GetFrame();

	UpdateCutFlags(dominantRenderedCameraOnPreviousUpdate.Get(), gameplayFrameOnPreviousUpdate, gameplayFrame);

	UpdatePedVisibility();
	UpdateVehicleVisibility();

#if FPS_MODE_SUPPORTED
	UpdateCustomFirstPersonShooterBehaviour();
#endif // FPS_MODE_SUPPORTED

#if __BANK
	DebugUpdateFullScreenBlurEffect(gameplayFrame);
#endif // __BANK

	CViewport* gameViewport = gVpMan.GetGameViewport();
	if(gameViewport != NULL)
	{
		ms_FramePropagator.Propagate(gameplayFrame, ms_DominantRenderedCamera, *gameViewport);

		gVpMan.CacheGameGrcViewport(gameViewport->GetGrcViewport());
		gVpMan.CacheGameViewportFrame(gameViewport->GetFrame());
	}

#if GTA_REPLAY
	// Current camera has tracking entity
	if( ms_DominantRenderedCamera )
	{
		camInterface::CacheCameraAttachParent(ms_DominantRenderedCamera->GetAttachParent());
	}
	else
	{
		camInterface::CacheCameraAttachParent(NULL);
	}
#endif	//GTA_REPLAY


	camInterface::CacheFrame(gameplayFrame);
	camInterface::ClearAutoResetVariables();

	//NOTE: We must clear the cut states of the cameras at the end of the camera system update, rather than the start, as cuts can be reported
	//prior to the camera update, such as during the script update.
	ClearCameraAutoResetFlags();

	camBaseFrameShaker::CleanupUnreferencedFrameShakers();
	camOscillatingFrameShaker::PostUpdateClass();

#if __BANK
	ms_ShouldRenderUnapprovedAnimatedCameraText = ShouldRenderUnapprovedCamerasDebugText(); 

	if(PARAM_cameraWidgets.Get())
	{
		//Ensure the camera widgets have been added.
		AddWidgets();
	}
#endif

#if REGREF_VALIDATION
	DebugDeleteUnreferencedCameras();
#endif // REGREF_VALIDATION

	//NOTE: We have a fundamental update order/dependency issue between the time cycle and camera system updates. The time cycle update is
	//dependent upon an up-to-date viewport (amongst other things) and the far-clip of the viewport is dependent upon the time cycle being up-to-date.
	// - To work around this issue, we force an additional update of the time cycle here whenever we detect that the camera has cut position,
	// as this should allow the time cycle system to take account of any change in modifiers. We then propagate the revised far-clip distance.

	if((gameViewport != NULL) && gameplayFrame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition))
	{
		const bool hasRevisedFarClip = ms_FramePropagator.ComputeAndPropagateRevisedFarClipFromTimeCycle(gameplayFrame, *gameViewport);
		if(hasRevisedFarClip)
		{
			//Cache the revised viewport and camera frame.

			gVpMan.CacheGameGrcViewport(gameViewport->GetGrcViewport());
			gVpMan.CacheGameViewportFrame(gameViewport->GetFrame());

			camInterface::SetCachedFrameOnly(gameplayFrame);
		}
	}

#if RSG_PC
	if (GRCDEVICE.IsStereoEnabled() && GRCDEVICE.CanUseStereo())
	{
		CViewport* pGameViewport = gVpMan.GetGameViewport();
		if (pGameViewport)
		{
			/*
			 * Don't throw this out ... needed for the future reference
			 */
			const grcViewport &vp = pGameViewport->GetGrcViewport();

			static float g_fminValidDistance = 2.0f; //	where convergence becomes 1.0
			static float fmaxValidDistance = 4.0f;//GRCDEVICE.GetDefaultConvergenceDistance();
			bool bSpecialCam = false;
			Vector3 vCamPos = (RCC_MATRIX44(vp.GetCameraMtx44()).d.GetVector3());
			Vec3V vIteratorPos = VECTOR3_TO_VEC3V(vCamPos);
			CEntityIterator entityIterator(IterateAllTypes, NULL, &vIteratorPos, fmaxValidDistance);
			float fClosestDist = fmaxValidDistance;
			s32 vpId = pGameViewport->GetId();
			static float g_prevSpecialCamConvergence = GRCDEVICE.GetDefaultConvergenceDistance();

			// if playing cutscene, use special cam
			if (CutSceneManager::GetInstance()->IsCutscenePlayingBack())
			{
				while(true)
				{
					CEntity* nearbyEntity = entityIterator.GetNext();
					if(!nearbyEntity)
					{
						break;
					}
					else if (nearbyEntity->GetIsVisibleInViewport(vpId))
					{
						float fDist =  vCamPos.Dist(VEC3V_TO_VECTOR3(nearbyEntity->GetTransform().GetPosition()));
						if (fDist < fClosestDist)
							fClosestDist = fDist;

						bSpecialCam = true;
					}
				}
			}

			// if in first person mode or vehicle interior cam, use special cam
			if (FindPlayerPed()->IsFirstPersonShooterModeEnabledForPlayer() || 
				camInterface::GetCinematicDirector().IsRenderingAnyInVehicleFirstPersonCinematicCamera())
			{
				bSpecialCam = true;
				fClosestDist = g_fminValidDistance;
			}

			bSpecialCam = false;
			if (bSpecialCam)
			{
				float t = 1.0f - ClampRange(fClosestDist, g_fminValidDistance, fmaxValidDistance);
				ms_fStereoConvergence = Lerp(t,GRCDEVICE.GetDefaultConvergenceDistance(),0.0f);
			}
			else
			{
				ms_fStereoConvergence = GRCDEVICE.GetDefaultConvergenceDistance();
			}

			if (g_prevSpecialCamConvergence != ms_fStereoConvergence)
			{
				static float g_fConvergenceStep = 10.0f;//0.025f;
				float fConvergenceStep = g_fConvergenceStep * (g_prevSpecialCamConvergence > ms_fStereoConvergence ? -1.0f : 1.0f);

				if (g_prevSpecialCamConvergence > ms_fStereoConvergence)
					ms_fStereoConvergence = Clamp(g_prevSpecialCamConvergence + fConvergenceStep, ms_fStereoConvergence, g_prevSpecialCamConvergence);
				else
					ms_fStereoConvergence = Clamp(g_prevSpecialCamConvergence + fConvergenceStep, g_prevSpecialCamConvergence, ms_fStereoConvergence);
			}

			g_prevSpecialCamConvergence = ms_fStereoConvergence;

			// force convergence to be 0.0
			//ms_fStereoConvergence = 0.0f;
		}
	}
#endif

#if FPS_MODE_SUPPORTED
	ms_SuppressFirstPersonFlashEffectThisFrame = false;
#endif // FPS_MODE_SUPPORTED
}

void camManager::PreUpdateCameras()
{
	const s32 maxNumCameras = camBaseCamera::GetPool()->GetSize();
	for(s32 i=0; i<maxNumCameras; i++)
	{
		camBaseCamera* camera = camBaseCamera::GetPool()->GetSlot(i);
		if(camera)
		{
			camera->PreUpdate();
		}
	}
}
#if __BANK
bool camManager::ShouldRenderUnapprovedCamerasDebugText()
{
	// Check cutscene is not playing
	CutSceneManager *pManager = CutSceneManager::GetInstance();
	if(!pManager || (pManager && !pManager->IsCutscenePlayingBack()))
	{
		// Check dominant camera is an animated camera
		const camBaseCamera *pCamera = GetDominantRenderedCamera();
		if(pCamera && pCamera->GetIsClassId(camAnimatedCamera::GetStaticClassId()))
		{
			// Check animated camera is not approved
			const camAnimatedCamera *pAnimatedCamera = static_cast< const camAnimatedCamera * >(pCamera);

			if (pAnimatedCamera->IsAnimatedCameraAnimUnapproved())
			{
				ms_UnapprovedAnimatedCameraTextTimer += fwTimer::GetSystemTimeStep();
				if (ms_UnapprovedAnimatedCameraTextTimer <= 10.0f)
				{
					return true;
				}
				else
				{
					return false;
				}
			}
		}
	}

	ms_UnapprovedAnimatedCameraTextTimer = 0.0f;
	return false; 
}
#endif // __BANK

void camManager::RemoveBlendedOutRenderedObjects(atArray<tRenderedCameraObjectSettings>& renderedObjects)
{
	//Remove any objects with negligible blend levels.
	for(s32 i=0; i<renderedObjects.GetCount(); )
	{
		tRenderedCameraObjectSettings& settings = renderedObjects[i];
		if(settings.m_BlendLevel < SMALL_FLOAT)
		{
			//NOTE: We must NULL the object reference for safety, as Delete just shuffles the array elements.
			settings.m_Object = NULL;
			renderedObjects.Delete(i);
		}
		else
		{
			i++;
		}
	}
}

const camBaseObject* camManager::ComputeDominantRenderedObject(const atArray<tRenderedCameraObjectSettings>& renderedObjects)
{
	//Find the rendered object with the highest blend level.

	const s32 numRenderedObjects = renderedObjects.GetCount();
	if(numRenderedObjects == 0)
	{
		return NULL;
	}

	const tRenderedCameraObjectSettings* dominantRenderedObjectSettings = &renderedObjects[0];
	for(s32 objectIndex=1; objectIndex<numRenderedObjects; objectIndex++)
	{
		if(renderedObjects[objectIndex].m_BlendLevel > (dominantRenderedObjectSettings->m_BlendLevel + SMALL_FLOAT))
		{
			dominantRenderedObjectSettings = &renderedObjects[objectIndex];
		}
	}

	return dominantRenderedObjectSettings->m_Object;
}

void camManager::UpdateCutFlags(const camBaseCamera* dominantRenderedCameraOnPreviousUpdate, const camFrame& renderedFrameOnPreviousUpdate,
	camFrame& renderedFrame)
{
	bool hasCutBetweenCameras	= (ms_DominantRenderedCamera != dominantRenderedCameraOnPreviousUpdate) && (!dominantRenderedCameraOnPreviousUpdate ||
									!(ms_DominantRenderedCamera && ms_DominantRenderedCamera->IsInterpolating(dominantRenderedCameraOnPreviousUpdate)));
	if(hasCutBetweenCameras)
	{
		//Check if the dominant rendering director in interpolating from the previous camera.
		const s32 numRenderedCameras = ms_RenderedCameras.GetCount();
		for(s32 cameraIndex=0; cameraIndex<numRenderedCameras; cameraIndex++)
		{
			if(ms_RenderedCameras[cameraIndex].m_Object.Get() == dominantRenderedCameraOnPreviousUpdate)
			{
				hasCutBetweenCameras = false;
				break;
			}
		}

		if(hasCutBetweenCameras)
		{
			//NOTE: We have to assume that both position and orientation have cut when a camera changes occurs without interpolation, but we
			//sanity-check this below.
			renderedFrame.GetFlags().SetFlag(camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation);
		}
	}

	//Finally, sanity-check that the rendered position and orientation have actually changed since the previous update.

	const bool hasCutPosition = renderedFrame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition);
	if(hasCutPosition)
	{
		const Vector3& positionOnPreviousUpdate	= renderedFrameOnPreviousUpdate.GetPosition();
		const Vector3& position					= renderedFrame.GetPosition();
		const float translation2				= positionOnPreviousUpdate.Dist2(position);
		const bool hasPositionChanged			= (translation2 >= g_MinTranslation2ForPositionCut);
		if(!hasPositionChanged)
		{
			renderedFrame.GetFlags().ClearFlag(camFrame::Flag_HasCutPosition);
		}
	}

	const bool hasCutOrientation = renderedFrame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation);
	if(hasCutOrientation)
	{
		const Matrix34& worldMatrixOnPreviousUpdate = renderedFrameOnPreviousUpdate.GetWorldMatrix();
		Quaternion orientationOnPreviousUpdate;
		orientationOnPreviousUpdate.FromMatrix34(worldMatrixOnPreviousUpdate);

		const Matrix34& worldMatrix = renderedFrame.GetWorldMatrix();
		Quaternion orientation;
		orientation.FromMatrix34(worldMatrix);

		const float rotationAngle			= orientationOnPreviousUpdate.RelAngle(orientation);
		const bool hasOrientationChanged	= (Abs(rotationAngle) >= g_MinRotationForOrientationCut);
		if(!hasOrientationChanged)
		{
			renderedFrame.GetFlags().ClearFlag(camFrame::Flag_HasCutOrientation);
		}
	}
}

void camManager::ClearCameraAutoResetFlags()
{
	const s32 maxNumCameras = camBaseCamera::GetPool()->GetSize();
	for(s32 i=0; i<maxNumCameras; i++)
	{
		camBaseCamera* camera = camBaseCamera::GetPool()->GetSlot(i);
		if(camera)
		{
			camera->GetFrameNonConst().GetFlags().ClearFlag(camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation | camFrame::Flag_BypassNearClipScanner);
			camera->GetFrameNonConst().SetCameraTimeScaleThisUpdate(1.0f);
		}
	}
}

void camManager::UpdatePedVisibility()
{
	//NOTE: We break with the convention of treating all game entities as const in the camera update here for the purposes of optimisation.

	//NOTE: We always reset the (camera) visibility state of any ped we made invisible on the previous update. This is an auto-reset mechanism for hiding
	//peds in the camera system, given that the entity visibility masks are persistent.
	if(ms_PedMadeInvisible.Get())
	{
		ModifyPedVisibility(*ms_PedMadeInvisible.Get(), true);
		ms_PedMadeInvisible = NULL;
	}

	if(!ms_DominantRenderedCamera)
	{
		return;
	}

	const CPed* pedToMakeInvisible = NULL;
	bool includeShadows = false;

	if(ms_DominantRenderedCamera->GetIsClassId(camFirstPersonAimCamera::GetStaticClassId()))
	{
		const camFirstPersonAimCamera* pFirstPersonAimCamera = static_cast<const camFirstPersonAimCamera*>(ms_DominantRenderedCamera.Get());
		if (pFirstPersonAimCamera->ShouldMakeAttachedEntityInvisible())
		{
			const CEntity* entityToMakeInvisible = pFirstPersonAimCamera->GetAttachParent();
			if(entityToMakeInvisible && entityToMakeInvisible->GetIsTypePed())
			{
				pedToMakeInvisible = static_cast<const CPed*>(entityToMakeInvisible);
			}
		}
	}
	else if(ms_DominantRenderedCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
	{
		pedToMakeInvisible = static_cast<const camCinematicMountedCamera*>(ms_DominantRenderedCamera.Get())->GetPedToMakeInvisible();
	}
	else if(ms_DominantRenderedCamera->GetIsClassId(camCinematicFirstPersonIdleCamera::GetStaticClassId()))
	{
		pedToMakeInvisible = static_cast<const camCinematicFirstPersonIdleCamera*>(ms_DominantRenderedCamera.Get())->GetPedToMakeInvisible();
	}
#if !__FINAL //No debug cameras in final builds.
	else if(ms_DominantRenderedCamera->GetIsClassId(camFreeCamera::GetStaticClassId()))
	{
		//Hide the follow ped if they are being carried by the debug free camera.
		const bool isCarryingFollowPed = static_cast<const camFreeCamera*>(ms_DominantRenderedCamera.Get())->ComputeIsCarryingFollowPed();
		if(isCarryingFollowPed)
		{
			pedToMakeInvisible = camInterface::FindFollowPed();
		}
	}
#endif // !__FINAL

	if(pedToMakeInvisible)
	{
		ms_PedMadeInvisible = const_cast<CPed*>(pedToMakeInvisible);
		ModifyPedVisibility(*ms_PedMadeInvisible.Get(), false, includeShadows);
	}
}

void camManager::ModifyPedVisibility(CPed& ped, bool isVisible, bool includeShadows)
{
	if(!isVisible)
	{
		// Assume because we're in first-person that PreRender will not be called on the attached physical, so force the physical to be
		// updated in the later PreRender
		ped.SetIsVisibleInSomeViewportThisFrame(true);
	}

	// Don't render the ped in the gbuffer, but do render in mirror reflection etc.
	u16 visibilityFlags = VIS_PHASE_MASK_GBUF|VIS_PHASE_MASK_SEETHROUGH_MAP;
	if (includeShadows || isVisible)
	{
		visibilityFlags |= VIS_PHASE_MASK_SHADOWS;
	}
	ped.ChangeVisibilityMask(visibilityFlags, isVisible, true);

	CObject* weapon = ped.GetWeaponManager()->GetEquippedWeaponObject();
	if (weapon)
	{
		weapon->ChangeVisibilityMask(visibilityFlags, isVisible, true);
	}
}

void camManager::UpdateVehicleVisibility()
{
	//NOTE: We break with the convention of treating all game entities as const in the camera update here for the purposes of optimisation and safety.

	//NOTE: We always reset the (camera) visibility state of any vehicle we made invisible on the previous update. This is an auto-reset mechanism for hiding
	//vehicles in the camera system, given that the entity visibility modules are persistent.
	if(ms_VehicleMadeInvisible)
	{
		ms_VehicleMadeInvisible->SetIsVisibleForModule(SETISVISIBLE_MODULE_CAMERA, true);
		ms_VehicleMadeInvisible = NULL;
	}

	if(!ms_DominantRenderedCamera)
	{
		return;
	}

#if !__FINAL //No debug cameras in final builds.
	if(ms_DominantRenderedCamera->GetIsClassId(camFreeCamera::GetStaticClassId()))
	{
		//Hide the follow ped's vehicle if they are being carried by the debug free camera.
		const bool isCarryingFollowPed = static_cast<const camFreeCamera*>(ms_DominantRenderedCamera.Get())->ComputeIsCarryingFollowPed();
		if(isCarryingFollowPed)
		{
			const CPed* followPed = camInterface::FindFollowPed();
			if(followPed && followPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
			{
				ms_VehicleMadeInvisible = followPed->GetMyVehicle();
			}
		}
	}
#endif // !__FINAL

	if(ms_VehicleMadeInvisible)
	{
		ms_VehicleMadeInvisible->SetIsVisibleForModule(SETISVISIBLE_MODULE_CAMERA, false);
	}
}

#if FPS_MODE_SUPPORTED

atHashString timeElapsedString("Delay timer elapsed");

void camManager::UpdateCustomFirstPersonShooterBehaviour()
{
	ms_IsRenderingFirstPersonShooterCamera					= false;
	ms_IsRenderingFirstPersonShooterCustomFallBackCamera	= false;

	const camGameplayDirector& gameplayDirector = camInterface::GetGameplayDirector();

	const atArray<tRenderedCameraObjectSettings>& renderedCameras	= GetRenderedCameras();
	const s32 numRenderedCameras									= renderedCameras.GetCount();

	for(s32 cameraIndex=0; cameraIndex<numRenderedCameras; cameraIndex++)
	{
		const camBaseObject* object = renderedCameras[cameraIndex].m_Object;
		if(object && object->GetIsClassId(camBaseCamera::GetStaticClassId()))
		{
			const camBaseCamera* camera = static_cast<const camBaseCamera*>(object);
			if(camera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()))
			{
				ms_IsRenderingFirstPersonShooterCamera = true;
				break;
			}
		}
	}

	if(!ms_IsRenderingFirstPersonShooterCamera && camInterface::IsRenderingDirector(gameplayDirector) && gameplayDirector.IsFallingBackToThirdPersonForDirectorBlendCatchUpOrSwitch())
	{
		ms_IsRenderingFirstPersonShooterCustomFallBackCamera = true;
	}

	 if (ms_TriggerFirstPersonFlashEffectTime>0 && ms_TriggerFirstPersonFlashEffectTime<=fwTimer::GetTimeInMilliseconds())
	 {
		 const char * contextString = "";
#if !__FINAL && !__NO_OUTPUT
		 contextString = SAFE_CSTRING(timeElapsedString.GetCStr());
#else // !__FINAL && !__NO_OUTPUT
		 contextString = "";
#endif // !__FINAL && !__NO_OUTPUT

		 TriggerFirstPersonFlashEffect(ms_PendingFirstPersonFlashEffect, 0 , contextString);
	 }
}

const char * camManager::GetFirstPersonFlashEffectName(eFirstPersonFlashEffectType type)
{
	switch (type)
	{
	case CAM_PUSH_IN_FX_NEUTRAL:
		return "NEUTRAL";
	case CAM_PUSH_IN_FX_MICHAEL:
		return "MICHAEL";
	case CAM_PUSH_IN_FX_FRANKLIN:
		return "FRANKLIN";
	case CAM_PUSH_IN_FX_TREVOR:
		return "TREVOR";
	case FIRST_PERSON_FLASH_EFFECT_COUNT:
		return "INVALID!";
	}
	return "";
}

void camManager::TriggerFirstPersonFlashEffect(eFirstPersonFlashEffectType flashType, u32 delay, const char * BANK_ONLY(debugContext))
{
	if (ms_SuppressFirstPersonFlashEffectThisFrame)
	{
		// Flashes suppressed by an external system. Early out now
		if (ms_TriggerFirstPersonFlashEffectTime<=fwTimer::GetTimeInMilliseconds())
		{
			// clear the pendign flash, if appropriate
			AbortPendingFirstPersonFlashEffect("Flashes suppressed");
		}
		return;
	}

#if __BANK
	atHashString oldContext = ms_FirstPersonFlashEffectCallingContext;
	ms_FirstPersonFlashEffectCallingContext.SetFromString(debugContext);
#endif // __BANK

	BANK_ONLY(cameraDisplayf("[%d|%d] First person flash triggered from %s, type:%s delay:%d", fwTimer::GetFrameCount(), fwTimer::GetTimeInMilliseconds(), debugContext, GetFirstPersonFlashEffectName(flashType), delay));
#if __BANK
	ms_LastFirstPersonFlashAbortTime = 0;
#endif //__BANK
	// we're going to trigger the flash in the future.
	// set the timer so we'll trigger it later
	if (delay>0)
	{
		ms_PendingFirstPersonFlashEffect = flashType;
		ms_TriggerFirstPersonFlashEffectTime = fwTimer::GetTimeInMilliseconds()+delay;
	}
	else
	{
		// trigger 
		ANIMPOSTFXMGR.Stop(ms_FirstPersonFlashEffectName[ms_CurrentFirstPersonFlashEffect],AnimPostFXManager::kCameraFlash);
		ms_CurrentFirstPersonFlashEffect = flashType;
		ANIMPOSTFXMGR.Start(ms_FirstPersonFlashEffectName[ms_CurrentFirstPersonFlashEffect], 0, false, false, false, 0, AnimPostFXManager::kCameraFlash);
		// Trigger woosh audio 
		g_FrontendAudioEntity.PlaySound(ATSTRINGHASH("1st_Person_Transition", 0xC8FFEA2F),"PLAYER_SWITCH_CUSTOM_SOUNDSET");

#if __BANK
		// remmeber the time and the context of the last flash so we can debug render it.
		ms_LastFirstPersonFlashTime = fwTimer::GetTimeInMilliseconds();
		if (ms_FirstPersonFlashEffectCallingContext.GetHash()==timeElapsedString.GetHash())
		{
			ms_LastFirstPersonFlashContext = oldContext;
		}
		else
		{
			ms_LastFirstPersonFlashContext = ms_FirstPersonFlashEffectCallingContext;
		}		
#endif // __BANK

		ms_PendingFirstPersonFlashEffect = camManager::CAM_PUSH_IN_FX_NEUTRAL;
		ms_TriggerFirstPersonFlashEffectTime = 0;
		camInterface::GetGameplayDirector().UseSwitchCustomFovBlend();
	}
}

void camManager::AbortPendingFirstPersonFlashEffect(const char * BANK_ONLY(abortReason))
{
	if (ms_TriggerFirstPersonFlashEffectTime>0)
	{
#if __BANK
		ms_LastFirstPersonFlashAbortReason = abortReason;
		ms_LastFirstPersonFlashAbortTime=fwTimer::GetTimeInMilliseconds();
		cameraDisplayf("[%d|%d] First person flash ABORTED by %s (started by %s)", fwTimer::GetFrameCount(), fwTimer::GetTimeInMilliseconds(), abortReason, SAFE_CSTRING(ms_FirstPersonFlashEffectCallingContext.GetCStr()));
#endif // __BANK
		ms_PendingFirstPersonFlashEffect = camManager::CAM_PUSH_IN_FX_NEUTRAL;
		ms_TriggerFirstPersonFlashEffectTime = 0;
	}
}


bool camManager::IsRenderingFirstPersonCamera()
{
	return CGame::IsSessionInitialized() && (ms_IsRenderingFirstPersonShooterCamera || camInterface::GetCinematicDirector().IsRenderingAnyInVehicleFirstPersonCinematicCamera());
}
#endif // FPS_MODE_SUPPORTED

camBaseDirector* camManager::FindDirector(const atHashWithStringNotFinal& classIdToFind)
{
	camBaseDirector* requestedDirector = NULL;

	const int numDirectors = ms_Directors.GetCount();
	for(int i=0; i<numDirectors; i++)
	{
		camBaseDirector* director = ms_Directors[i];
		if(director && (director->GetClassId() == classIdToFind.GetHash()))
		{
			requestedDirector = director;
			break;
		}
	}

	return requestedDirector;
}

u32 camManager::ComputeNumDirectorsInterpolating()
{
	u32 numDirectorsInterpolating = 0;

	const int numDirectors = ms_Directors.GetCount();
	for(int i=0; i<numDirectors; i++)
	{
		camBaseDirector* director = ms_Directors[i];
		if(director && director->IsInterpolating())
		{
			numDirectorsInterpolating++;
		}
	}

	return numDirectorsInterpolating;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debug support
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if REGREF_VALIDATION
void camManager::DebugDeleteUnreferencedCameras()
{
	const s32 maxNumCameras = camBaseCamera::GetPool()->GetSize();
	for(s32 i=0; i<maxNumCameras; i++)
	{
		camBaseCamera* camera = camBaseCamera::GetPool()->GetSlot(i);
		if(camera && (camera->CountAllKnownRefs() == 0))
		{
			cameraWarningf("Found and deleted an unreferenced camera (%s)", SAFE_CSTRING(camera->GetName()));
			delete camera;
		}
	}
}
#endif // REGREF_VALIDATION

#if __BANK
void camManager::DebugRender()
{
	//keyboard input
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_C, KEYBOARD_MODE_DEBUG_ALT, "Cycle through camera debug displays"))
		DebugCycleRenderOptions();
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_C, KEYBOARD_MODE_DEBUG_CNTRL_ALT, "Turn camera debug display off"))
		DebugClearDisplay();
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_UP, KEYBOARD_MODE_DEBUG_ALT, "Select previous camera"))
		DebugSelectPreviousCamera();
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_DOWN, KEYBOARD_MODE_DEBUG_ALT, "Select next camera"))
		DebugSelectNextCamera();

	//NOTE: We sanity check our selected camera and director prior to anything,
	//so that it will work regardless of the camera table being rendered or not.
	if (ms_DebugSelectedDirector == NULL || ms_DebugSelectedCamera == NULL)
	{
		DebugSelectValidation();
	}

	grcDebugDraw::TextFontPush(grcSetup::GetMiniFixedWidthFont());

	if(ms_ShouldDebugRenderCameraTable)
	{
		DebugRenderCameraTable();
	}

	grcDebugDraw::TextFontPop();

	if(ms_ShouldDebugRenderThirdLines)
	{
		DebugRenderThirdLines();
	}

	if(ms_ShouldDebugRenderCameras)
	{
		DebugRenderCameras();
	}
}

void camManager::ResetDebugVariables()
{
#if GTA_REPLAY
	ms_DebugReplayCameraMovementDisabledThisFrame = false;
	ms_DebugReplayCameraMovementDisabledThisFrameScript = false;
#endif // GTA_REPLAY
}

void camManager::Render()
{
	// Check debug text should be displayed
	if(CDebugBar::GetDisplayReleaseDebugText() == DEBUG_DISPLAY_STATE_STANDARD && ms_ShouldRenderUnapprovedAnimatedCameraText == true)
	{
		float CameraUnapproved_X = 0.800f;
		float CameraUnapproved_Y = 0.90f;
		float scaleX = 0.40f;
		float scaleY = 0.40f;

		CTextLayout DebugText;
		DebugText.SetScale(Vector2(scaleX, scaleY));
		DebugText.SetColor(CRGBA(255,255,255,255));
		DebugText.SetDropShadow(true);

		DebugText.Render(Vector2(CameraUnapproved_X, CameraUnapproved_Y), "Camera anim not approved" );
	}
}

void camManager::DebugRenderCameraTable()
{
	u8 rowIndex = 0;

	//Draw column titles.
	DebugRenderTableEntry(DIRECTOR_NAME_COLUMN, rowIndex, "");
	DebugRenderTableEntry(DIRECTOR_BLEND_COLUMN, rowIndex, "B");
	DebugRenderTableEntry(SELECTED_COLUMN, rowIndex, "");
	DebugRenderTableEntry(CAMERA_NAME_COLUMN, rowIndex, "Name");
	DebugRenderTableEntry(UPDATING_COLUMN, rowIndex, "U");
	DebugRenderTableEntry(BLEND_COLUMN, rowIndex, "B");
	DebugRenderTableEntry(POSITION_X_COLUMN, rowIndex, "Position", g_DebugRenderTableColumnColors[POSITION_X_COLUMN], ORIENTATION_P_COLUMN);
	DebugRenderTableEntry(ORIENTATION_P_COLUMN, rowIndex, "Orientation P-R-Y", g_DebugRenderTableColumnColors[ORIENTATION_P_COLUMN], NEAR_CLIP_COLUMN);
	DebugRenderTableEntry(NEAR_CLIP_COLUMN, rowIndex, "Clip", g_DebugRenderTableColumnColors[NEAR_CLIP_COLUMN], FLAGS_COLUMN);
	DebugRenderTableEntry(FLAGS_COLUMN, rowIndex, "Flags");
	DebugRenderTableEntry(DOF_1_COLUMN, rowIndex, "DOF", g_DebugRenderTableColumnColors[DOF_1_COLUMN], FOV_COLUMN);
	DebugRenderTableEntry(FOV_COLUMN, rowIndex, "FOV");
	DebugRenderTableEntry(MOTION_BLUR_COLUMN, rowIndex, "MBlur");
	DebugRenderTableEntry(INFO_COLUMN, rowIndex, "Info");

	//Draw rows for each director, in order of update.
	rowIndex = rowIndex + 2; //blank row after headings

	const int numDirectors = ms_Directors.GetCount();
	for(int i=0; i<numDirectors; i++)
	{
		const camBaseDirector* director = ms_Directors[i];
		if(director)
		{
			DebugRenderDirectorText(director, rowIndex);
		}
	}

	++rowIndex;
	DebugRenderFinalFrameText(rowIndex);

	rowIndex += 2; //Blank row after final row
	DebugRenderGlobalState(rowIndex);

	DebugRenderScriptCommands(rowIndex);
}

void camManager::DebugRenderDirectorText(const camBaseDirector* director, u8& rowIndex)
{
	Color32 blendTextColour = Color_default;

	//Draw the director name
	atString directorName = atString(director->GetName());
	directorName.Set(directorName, 0, g_DebugRenderTableColumnWidths[DIRECTOR_NAME_COLUMN] - 1); //trimming it down
	DebugRenderTableEntry(DIRECTOR_NAME_COLUMN, rowIndex, SAFE_CSTRING(directorName.c_str()));

	//Draw the director blend
	char blendText[5] = "X";
	float blendValue = GetBlendValueForDirector(director);
	if ( blendValue > -SMALL_FLOAT )
	{
		const Color32 textRedColour = Color_red3;
		const Color32 textOrangeColour = Color_orange;
		const Color32 textGreenColour = Color_ForestGreen;

		blendTextColour = 
			(blendValue < 0.5f) ? Lerp(blendValue/0.5f, textRedColour, textOrangeColour) 
			: Lerp((blendValue-0.5f)/0.5f, textOrangeColour, textGreenColour);

		if (!AreNearlyEqual(blendValue, 1.0f))
			sprintf(blendText, "%02u", (unsigned int)FloorfFast(blendValue*100));
		else
			sprintf(blendText, "F");
	}
	DebugRenderTableEntry(DIRECTOR_BLEND_COLUMN, rowIndex, blendText, blendTextColour);

	//Get all cameras owned by this director
	atArray<camBaseCamera*> cameras;
	cameras.Reset();
	director->DebugGetCameras(cameras);
	const int numCameras = cameras.GetCount();

	//Draw camera columns
	for(int i=0; i<numCameras; i++)
	{
		DebugRenderCameraText(cameras[i], rowIndex);
	}

	if (numCameras == 0)
	{
		//if there's no cameras here, we draw a blank selected column (unless we're the selected director!)
		DebugRenderTableEntry(SELECTED_COLUMN, rowIndex, (ms_DebugSelectedDirector == director) ? "*" : "");

		for (u8 i = CAMERA_NAME_COLUMN; i < NUM_DEBUG_RENDER_COLUMNS; ++i)
		{
			DebugRenderTableEntry(i, rowIndex, "");
		}

		++rowIndex;
	}
}

void camManager::DebugRenderCameraText(const camBaseCamera* camera, u8& rowIndex, bool isFinal)
{
	const camFrame frame = isFinal ? camInterface::GetFrame() : camera->GetFrame();
	char debugText[10];

	const Color32 textRedColour = Color_red3;
	const Color32 textOrangeColour = Color_orange;
	const Color32 textGreenColour = Color_ForestGreen;
	Color32 blendTextColour = Color_default;

	//Get blend value for this camera
	float blendValue = GetBlendValueForCamera(camera);
	if ( blendValue > -SMALL_FLOAT && !isFinal )
	{
		blendTextColour = 
			(blendValue < 0.5f) ? Lerp(blendValue/0.5f, textRedColour, textOrangeColour) 
			: Lerp((blendValue-0.5f)/0.5f, textOrangeColour, textGreenColour);
	}

	//Draw the selected column.
	if (!isFinal)
	{
		DebugRenderTableEntry(SELECTED_COLUMN, rowIndex, (ms_DebugSelectedCamera == camera) ? "*" : "");
	}

	//Draw the name column.
	atString name = atString(camera->GetName());
	//remove _CAMERA if it's at the end
	if (name.length() > 7)
	{
		atString endOfName;
		endOfName.Set(name, name.length() - 7, name.length());
		if (endOfName == "_CAMERA")
		{
			name.Set(name, 0, name.length() - 7);
		}
	}

	//cut down to max size
	const int maxLength = Min((int)name.length(), g_DebugRenderTableColumnWidths[CAMERA_NAME_COLUMN] - 1); //trimming it down
	name.Set(name, 0, maxLength);
	DebugRenderTableEntry(CAMERA_NAME_COLUMN, rowIndex, SAFE_CSTRING(name.c_str()), blendTextColour);

	//Draw the updating column.
	const bool isUpdating				= camera->WasUpdated();
	const Color32 updatingTextColour	= isUpdating ? textGreenColour : textRedColour;
	const char* updatingText			= isUpdating ? "Y" : "N";
	DebugRenderTableEntry(UPDATING_COLUMN, rowIndex, isFinal ? "" : updatingText, isFinal ? blendTextColour : updatingTextColour);

	//Draw the blend column.
	if ( AreNearlyEqual(blendValue, 1.0f) )
		sprintf(debugText, "F");
	else if (blendValue > -SMALL_FLOAT)
		sprintf(debugText, "%02u", (unsigned int)FloorfFast(blendValue*100));
	else
		sprintf(debugText, "X");
	DebugRenderTableEntry(BLEND_COLUMN, rowIndex, isFinal ? "" : debugText, blendTextColour);

	//Draw the position column.
	const Vector3 &position = frame.GetPosition();
	sprintf(debugText, "%.1f,", IsNearZero(position.x) ? 0.0f : position.x);
	DebugRenderTableEntry(POSITION_X_COLUMN, rowIndex, debugText);
	sprintf(debugText, "%.1f,", IsNearZero(position.y) ? 0.0f : position.y);
	DebugRenderTableEntry(POSITION_Y_COLUMN, rowIndex, debugText);
	sprintf(debugText, "%.1f", IsNearZero(position.z) ? 0.0f : position.z);
	DebugRenderTableEntry(POSITION_Z_COLUMN, rowIndex, debugText);

	//Draw the orientation column.
	Vector3 orientation;
	frame.GetWorldMatrix().ToEulersYXZ(orientation);
	orientation *= RtoD;
	sprintf(debugText, "%.1f,", IsNearZero(orientation.x) ? 0.0f : orientation.x);
	DebugRenderTableEntry(ORIENTATION_P_COLUMN, rowIndex, debugText);
	sprintf(debugText, "%.1f,", IsNearZero(orientation.y) ? 0.0f : orientation.y);
	DebugRenderTableEntry(ORIENTATION_R_COLUMN, rowIndex, debugText);
	sprintf(debugText, "%.1f", IsNearZero(orientation.z) ? 0.0f : orientation.z);
	DebugRenderTableEntry(ORIENTATION_Y_COLUMN, rowIndex, debugText);

	//Draw the clip column.
	sprintf(debugText, "%.2f,", frame.GetNearClip());
	DebugRenderTableEntry(NEAR_CLIP_COLUMN, rowIndex, debugText);
	sprintf(debugText, "%.1f", frame.GetFarClip());
	DebugRenderTableEntry(FAR_CLIP_COLUMN, rowIndex, debugText);

	//Draw the flags column.
	u16 flags = frame.GetFlags();
	sprintf(debugText, "%05u", flags);
	DebugRenderTableEntry(FLAGS_COLUMN, rowIndex, debugText);

	//Draw the DOF column.
	Vector4 dofPlanes;
	frame.ComputeDofPlanes(dofPlanes);

	for(u8 i = DOF_1_COLUMN; i < DOF_4_COLUMN; ++i)
	{
		sprintf(debugText, "%.2f,", dofPlanes[i - DOF_1_COLUMN]);
		DebugRenderTableEntry(i, rowIndex, debugText);
	}
	sprintf(debugText, "%.2f", dofPlanes[3]); //no comma
	DebugRenderTableEntry(DOF_4_COLUMN, rowIndex, debugText);

	//Draw the FOV column.
	sprintf(debugText, "%.1f", frame.GetFov());
	DebugRenderTableEntry(FOV_COLUMN, rowIndex, debugText);

	//Draw the motion blur column.
	sprintf(debugText, "%.2f", frame.GetMotionBlurStrength());
	DebugRenderTableEntry(MOTION_BLUR_COLUMN, rowIndex, debugText);

	//Draw the info column.
	const int iInfoSize = 32;
	char szInfo[iInfoSize]; szInfo[0] = '\0';
	int iInfoLength = 0;
	if(camera->GetIsClassId(camThirdPersonCamera::GetStaticClassId()))
	{
		const camThirdPersonCamera *thirdPersonCamera = static_cast< const camThirdPersonCamera * >(camera);

		if(thirdPersonCamera->GetCatchUpHelper())
		{
			formatf(&szInfo[iInfoLength], iInfoSize - iInfoLength, "C");
			iInfoLength = istrlen(szInfo);
		}

		if(thirdPersonCamera->GetHintHelper())
		{
			formatf(&szInfo[iInfoLength], iInfoSize - iInfoLength, "H");
			iInfoLength = istrlen(szInfo);
		}
	}
#if GTA_REPLAY
	else if (camera->GetIsClassId(camReplayBaseCamera::GetStaticClassId()))
	{
		const camReplayBaseCamera* replayCamera = static_cast<const camReplayBaseCamera*>(camera);

		formatf(&szInfo[iInfoLength], iInfoSize - iInfoLength, "[%i]", replayCamera->GetMarkerIndex());
		iInfoLength = istrlen(szInfo);
	}
#endif // GTA_REPLAY
	sprintf(debugText, "%s", szInfo);
	DebugRenderTableEntry(INFO_COLUMN, rowIndex, debugText);

	//Draw the optional script source.
	if (camera->GetIsClassId(camScriptedCamera::GetStaticClassId()))
	{
		const camScriptedCamera* scriptedCamera	= static_cast<const camScriptedCamera*>(camera);
		const s32 cameraPoolIndex				= scriptedCamera->GetPoolIndex();
		if(const scriptHandler* ownerScriptHandler	= CTheScripts::GetScriptHandlerMgr().GetScriptHandlerForResource(CGameScriptResource::SCRIPT_RESOURCE_CAMERA, cameraPoolIndex))
		{
			if(const GtaThread* ownerScriptThread = static_cast<const GtaThread*>(ownerScriptHandler->GetThread()))
			{
				const char* ownerScriptName = const_cast<GtaThread*>(ownerScriptThread)->GetScriptName();
				DebugRenderTableEntry(CAMERA_NAME_COLUMN, ++rowIndex, ownerScriptName, blendTextColour);
			}
		}
	}

	++rowIndex;
}

float camManager::GetBlendValueForCamera(const camBaseCamera* camera)
{
	const int num = ms_RenderedCameras.GetCount();
	for(int i=0; i<num; i++)
	{
		if (ms_RenderedCameras[i].m_Object.Get() == camera)
		{
			return ms_RenderedCameras[i].m_BlendLevel;
		}
	}
	return -1.0f;
}

float camManager::GetBlendValueForDirector(const camBaseDirector* director)
{
	const int num = ms_RenderedDirectors.GetCount();
	for(int i=0; i<num; i++)
	{
		if (ms_RenderedDirectors[i].m_Object.Get() == director)
		{
			return ms_RenderedDirectors[i].m_BlendLevel;
		}
	}
	return -1.0f;
}

void camManager::DebugRenderFinalFrameText(u8 rowIndex)
{
	DebugRenderTableEntry(DIRECTOR_NAME_COLUMN, rowIndex, "FINAL", g_DebugRenderTableColumnColors[DIRECTOR_NAME_COLUMN], CAMERA_NAME_COLUMN);

	const camBaseCamera* renderedCamera	= GetDominantRenderedCamera();
	if (renderedCamera)
	{
		DebugRenderCameraText(renderedCamera, rowIndex, true);
	}
}

void camManager::DebugRenderGlobalState(u8 rowIndex)
{
#if GTA_REPLAY
	if (CReplayMgr::IsEditModeActive())
	{
		char replayDirectorText[512];
		replayDirectorText[0] = '\0';

		camInterface::GetReplayDirector().GetDebugText(replayDirectorText);

		DebugRenderTableEntry(0, rowIndex, replayDirectorText, Color_black, NUM_DEBUG_RENDER_COLUMNS);
		return;
	}
#endif // GTA_REPLAY

	const int iSize = 512;
	char szGlobalState[iSize]; szGlobalState[0] = '\0';
	int iLength = 0;

	if (CPauseMenu::GetPauseRenderPhasesStatus())
	{
		if(iLength > 0)
		{
			formatf(&szGlobalState[iLength], iSize - iLength, ", ");
			iLength = istrlen(szGlobalState);
		}
		formatf(&szGlobalState[iLength], iSize - iLength, "RENDER_PHASES_PAUSED");
		iLength = istrlen(szGlobalState);
	}

	if(camInterface::GetCinematicDirector().IsFirstPersonInVehicleDisabledThisUpdate())
	{
		if(iLength > 0)
		{
			formatf(&szGlobalState[iLength], iSize - iLength, ", ");
			iLength = istrlen(szGlobalState);
		}
		formatf(&szGlobalState[iLength], iSize - iLength, "DISABLE_CINEMATIC_BONNET_CAMERA_THIS_UPDATE");
		iLength = istrlen(szGlobalState);
	}

	if(camInterface::GetGameplayDirector().DebugWasFirstPersonDisabledThisUpdate())
	{
		if(iLength > 0)
		{
			formatf(&szGlobalState[iLength], iSize - iLength, ", ");
			iLength = istrlen(szGlobalState);
		}
		formatf(&szGlobalState[iLength], iSize - iLength, "DISABLE_ON_FOOT_FIRST_PERSON_VIEW_THIS_UPDATE - %s",
			camInterface::GetGameplayDirector().DebugWasFirstPersonDisabledThisUpdateFromScript() ? "SCRIPT" : "CODE");
		iLength = istrlen(szGlobalState);
	}

#if GTA_REPLAY
	if(ms_DebugReplayCameraMovementDisabledThisFrame)
	{
		if(iLength > 0)
		{
			formatf(&szGlobalState[iLength], iSize - iLength, ", ");
			iLength = istrlen(szGlobalState);
		}
		formatf(&szGlobalState[iLength], iSize - iLength, "REPLAY_DISABLE_CAMERA_MOVEMENT_THIS_FRAME - %s",
			ms_DebugReplayCameraMovementDisabledThisFrameScript ? "SCRIPT" : "CODE");
		iLength = istrlen(szGlobalState);
	}
#endif // GTA_REPLAY

	s32 contextIndex, viewModeForContext;
	if(camControlHelper::DebugHadSetViewModeForContext(contextIndex, viewModeForContext))
	{
		if(iLength > 0)
		{
			formatf(&szGlobalState[iLength], iSize - iLength, ", ");
			iLength = istrlen(szGlobalState);
		}
		formatf(&szGlobalState[iLength], iSize - iLength, "SET_CAM_VIEW_MODE_FOR_CONTEXT (context:%d, view mode:%d)", contextIndex, viewModeForContext);
		iLength = istrlen(szGlobalState);
	}
	camControlHelper::DebugResetHadSetViewModeForContext();

	TUNE_GROUP_BOOL(CAMERA_MANAGER, renderCameraVelocitiesToCameraTable, false);
	const camThirdPersonCamera* thirdPersonCamera = camInterface::GetGameplayDirector().GetThirdPersonCamera();
	if (renderCameraVelocitiesToCameraTable && thirdPersonCamera)
	{
		if(iLength > 0)
		{
			formatf(&szGlobalState[iLength], iSize - iLength, ", ");
			iLength = istrlen(szGlobalState);
		}
		const Vector3& baseAttachVelocity	= thirdPersonCamera->GetBaseAttachVelocityToConsider();
		const camFollowCamera* followCamera	= thirdPersonCamera->GetIsClassId(camFollowCamera::GetStaticClassId()) ? static_cast<const camFollowCamera*>(thirdPersonCamera) : NULL;
		Vector3 pullAroundVelocity = Vector3(VEC3_ZERO);
		if (followCamera)
		{
			followCamera->DebugComputeAttachParentVelocityToConsiderForPullAround(pullAroundVelocity);
		}

		formatf(&szGlobalState[iLength], iSize - iLength, "Velocity = (%.2f, %.2f, %.2f) | Pull Around Velocity = (%.2f, %.2f, %.2f)",
			baseAttachVelocity.x, baseAttachVelocity.y, baseAttachVelocity.z,
			pullAroundVelocity.x, pullAroundVelocity.y, pullAroundVelocity.z);
		iLength = istrlen(szGlobalState);
	}

	TUNE_GROUP_BOOL(CAMERA_MANAGER, renderStuntSettingsToCameraTable, false);
	if (renderStuntSettingsToCameraTable)
	{
		if(iLength > 0)
		{
			formatf(&szGlobalState[iLength], iSize - iLength, ", ");
			iLength = istrlen(szGlobalState);
		}
		formatf(&szGlobalState[iLength], iSize - iLength, "STUNT? %s | REQUESTED? %s | FORCED? %s",
			camInterface::GetGameplayDirector().IsVehicleCameraUsingStuntSettingsThisUpdate(camInterface::GetGameplayDirector().GetRenderedCamera()) ? "ACTIVE" : "NOT ACTIVE",
			camInterface::GetGameplayDirector().IsScriptRequestingVehicleCamStuntSettingsThisUpdate() ? "YES" : "NO",
			camInterface::GetGameplayDirector().IsScriptForcingVehicleCamStuntSettingsThisUpdate() ? "YES" : "NO");
		iLength = istrlen(szGlobalState);
	}

	const char* scriptEnabledShakeName	= camInterface::GetScriptDirector().DebugGetScriptEnabledShakeName();
	float scriptEnabledShakeAmplitude	= camInterface::GetScriptDirector().DebugGetScriptEnabledShakeAmplitude();
	if(scriptEnabledShakeAmplitude > SMALL_FLOAT)
	{
		if(iLength > 0)
		{
			formatf(&szGlobalState[iLength], iSize - iLength, ", ");
			iLength = istrlen(szGlobalState);
		}
		formatf(&szGlobalState[iLength], iSize - iLength, "SCRIPT ENABLED SHAKE (name:%s, amplitude:%.2f)", scriptEnabledShakeName, scriptEnabledShakeAmplitude);
		iLength = istrlen(szGlobalState);
	}

	if (ms_TriggerFirstPersonFlashEffectTime>0)
	{
		if(iLength > 0)
		{
			formatf(&szGlobalState[iLength], iSize - iLength, ", ");
			iLength = istrlen(szGlobalState);
		}
		formatf(&szGlobalState[iLength], iSize - iLength, "FPS Flash (%s): %dms %s%s", SAFE_CSTRING(ms_FirstPersonFlashEffectCallingContext.GetCStr()), fwTimer::GetTimeInMilliseconds() - ms_TriggerFirstPersonFlashEffectTime, GetFirstPersonFlashEffectName(ms_PendingFirstPersonFlashEffect), ms_SuppressFirstPersonFlashEffectThisFrame ? " SUPPRESSED" : " Not suppressed");
		iLength = istrlen(szGlobalState);
	}
	else if (ms_LastFirstPersonFlashAbortTime>0 && ((fwTimer::GetTimeInMilliseconds() - ms_LastFirstPersonFlashAbortTime) < 5000))
	{
		if(iLength > 0)
		{
			formatf(&szGlobalState[iLength], iSize - iLength, ", ");
			iLength = istrlen(szGlobalState);
		}
		formatf(&szGlobalState[iLength], iSize - iLength, "FPS Flash (%s): ABORTED - %s", SAFE_CSTRING(ms_FirstPersonFlashEffectCallingContext.GetCStr()), SAFE_CSTRING(ms_LastFirstPersonFlashAbortReason.GetCStr()));
		iLength = istrlen(szGlobalState);
	}

	if (ms_LastFirstPersonFlashTime>0 && ((fwTimer::GetTimeInMilliseconds() - ms_LastFirstPersonFlashTime) < 5000))
	{
		if(iLength > 0)
		{
			formatf(&szGlobalState[iLength], iSize - iLength, ", ");
			iLength = istrlen(szGlobalState);
		}
		formatf(&szGlobalState[iLength], iSize - iLength, "Last FPS Flash (%s, %d)", SAFE_CSTRING(ms_LastFirstPersonFlashContext.GetCStr()), ms_LastFirstPersonFlashTime);
		iLength = istrlen(szGlobalState);
	}

	DebugRenderTableEntry(0, rowIndex, szGlobalState, Color_black, NUM_DEBUG_RENDER_COLUMNS);

	if (iLength > 0)
	{
		cameraDebugf3("%s", szGlobalState);
	}
}

void camManager::DebugRenderTableEntry(const u8 columnIndex, const u8 rowIndex, const char* text, const Color32 colour, const u8 endColumnIndex)
{
	if (!cameraVerifyf(columnIndex < NUM_DEBUG_RENDER_COLUMNS && endColumnIndex <= NUM_DEBUG_RENDER_COLUMNS, "A column index is out of range when trying to render the debug table!"))
	{
		return;
	}

	//get column colour
	const Color32 backgroundColour = colour != Color_default ? colour : g_DebugRenderTableColumnColors[columnIndex];

	//get vertical and horizontal offsets
	const u8 verticalOffset = g_DebugRenderTableRowOffset + rowIndex;
	u8 horizontalOffset = g_DebugRenderTableColumnOffset;
	for (int i = DIRECTOR_NAME_COLUMN; i < columnIndex; ++i)
	{
		horizontalOffset += g_DebugRenderTableColumnWidths[i];
	}

	//get background box width and heights
	const u8 backgroundBoxHeight = 1;
	u8 backgroundBoxWidth = 0;
	const u8 validEndColumnIndex = endColumnIndex > columnIndex ? endColumnIndex : columnIndex + 1;
	for (int i = columnIndex; i < validEndColumnIndex; ++i)
	{
		backgroundBoxWidth += g_DebugRenderTableColumnWidths[i];
	}

	const float fontWidth = 0.005f;
	const float fontHeight = 0.025f;

	//render background and text
	float boxX =		(float)( (horizontalOffset - 0.6) * fontWidth);
	float boxY =		(float)( (verticalOffset - 0.2) * fontHeight );
	float boxWidth =	(float)( (backgroundBoxWidth - 0.6) * fontWidth );
	float boxHeight =	(float)( (backgroundBoxHeight - 0.2) * fontHeight );
	const Color32 quadColor(backgroundColour.GetRed(), backgroundColour.GetGreen(), backgroundColour.GetBlue(), backgroundColour.GetAlpha()/2);
	grcDebugDraw::Quad(Vec2V(boxX, boxY), Vec2V(boxX+boxWidth, boxY), Vec2V(boxX+boxWidth, boxY+boxHeight), Vec2V(boxX, boxY+boxHeight), quadColor);

	if (*text != '\0')
	{
		const Color32 textColour = Color_white;
		grcDebugDraw::Text(Vector2(horizontalOffset * fontWidth, verticalOffset * fontHeight), textColour, text, false);
	}
}

void camManager::DebugClearDisplay()
{
	ms_DebugRenderOption = DEFAULT_RENDER_NONE;
	ms_ShouldDebugRenderCameraTable = false;
	ms_ShouldDebugRenderCameras = false;
}

void camManager::DebugCycleRenderOptions()
{
	ms_DebugRenderOption = (ms_DebugRenderOption + 1) % NUM_RENDER_OPTIONS;

	switch (ms_DebugRenderOption)
	{
	case RENDER_TABLE_ONLY:
		ms_ShouldDebugRenderCameraTable = true;
		ms_ShouldDebugRenderCameras = false;
		break;
	case RENDER_TABLE_AND_CAMERAS:
		ms_ShouldDebugRenderCameraTable = true;
		ms_ShouldDebugRenderCameras = true;
		break;
	case RENDER_CAMERAS_ONLY:
		ms_ShouldDebugRenderCameraTable = false;
		ms_ShouldDebugRenderCameras = true;
		break;
	default:
		ms_ShouldDebugRenderCameraTable = false;
		ms_ShouldDebugRenderCameras = false;
		break;
	}
}

void camManager::DebugSelectPreviousCamera()
{
	atArray<camBaseCamera*> cameras;
	int directorIndex = -1;
	int cameraIndex = -1;
	DebugSelectValidation(cameras, directorIndex, cameraIndex);

	const int numDirectors = ms_Directors.GetCount();
	int numCameras = cameras.GetCount();

	//we can safely assume cameraIndex and directorIndex >= 0
	if (cameraIndex == 0 || numCameras == 0)
	{
		if (directorIndex == 0)
			directorIndex = numDirectors - 1; //select the last director
		else
			directorIndex--;

		ms_DebugSelectedDirector = ms_Directors[directorIndex];

		//get list of this director's cameras
		cameras.Reset();
		ms_DebugSelectedDirector->DebugGetCameras(cameras);
		numCameras = cameras.GetCount();

		//use last camera for selected camera
		cameraIndex = Max(numCameras - 1,  0);
	}
	else
	{
		cameraIndex--;
	}

	ms_DebugSelectedCamera = cameras.GetCount() ? cameras[cameraIndex] : NULL;
}

void camManager::DebugSelectNextCamera()
{
	atArray<camBaseCamera*> cameras;
	int directorIndex = -1;
	int cameraIndex = -1;
	DebugSelectValidation(cameras, directorIndex, cameraIndex);

	const int numDirectors = ms_Directors.GetCount();
	int numCameras = cameras.GetCount();

	if (cameraIndex == numCameras - 1 || numCameras == 0)
	{
		if (directorIndex == numDirectors - 1)
			directorIndex = 0; //select the first director
		else
			directorIndex++;

		ms_DebugSelectedDirector = ms_Directors[directorIndex];

		//get list of this director's cameras
		cameras.Reset();
		ms_DebugSelectedDirector->DebugGetCameras(cameras);
		numCameras = cameras.GetCount();

		//use first camera for selected camera
		cameraIndex = 0;
	}
	else
	{
		cameraIndex++;
	}

	ms_DebugSelectedCamera = (numCameras > 0) ? cameras[cameraIndex] : NULL;
}

void camManager::DebugSelectValidation()
{
	atArray<camBaseCamera*> cameras;
	int directorIndex = -1;
	int cameraIndex = -1;

	DebugSelectValidation(cameras, directorIndex, cameraIndex); 
}

void camManager::DebugSelectValidation(atArray<camBaseCamera*> &cameras, int &directorIndex, int &cameraIndex)
{
	cameras.Reset();
	if (ms_DebugSelectedDirector)
	{
		//find out the index of the current director
		directorIndex = ms_Directors.Find(RegdCamBaseDirector(const_cast<camBaseDirector*>(ms_DebugSelectedDirector.Get())));
		ms_DebugSelectedDirector->DebugGetCameras(cameras);
	}
	else
	{
		//invalid! select the first director
		directorIndex = 0;
		ms_DebugSelectedDirector = ms_Directors[directorIndex];
		if (ms_DebugSelectedDirector)
		{
			ms_DebugSelectedDirector->DebugGetCameras(cameras);
		}
	}

	if (ms_DebugSelectedCamera)
	{
		//find out the index of the current camera
		cameraIndex = cameras.Find(RegdCamBaseCamera(const_cast<camBaseCamera*>(ms_DebugSelectedCamera.Get())));
	}

	if (cameraIndex == -1)
	{
		//invalid! select the first camera
		cameraIndex = 0;
		ms_DebugSelectedCamera = cameras.GetCount() ? cameras[cameraIndex] : NULL;
	}
}
void camManager::DebugRenderThirdLines()
{
	const Color32 colour	= Color_white;
	const float third		= (1.0f/3.0f);
	const float twoThirds	= (2.0f/3.0f);

	//Vertical lines
	grcDebugDraw::Line(Vector2(third, 0.0f), Vector2(third, 1.0f), colour);
	grcDebugDraw::Line(Vector2(twoThirds, 0.0f), Vector2(twoThirds, 1.0f), colour);

	//Horizontal lines
	grcDebugDraw::Line(Vector2(0.0f, third), Vector2(1.0f, third), colour);
	grcDebugDraw::Line(Vector2(0.0f, twoThirds), Vector2(1.0f, twoThirds), colour);
}

void camManager::DebugRenderCameras()
{
	const s32 maxNumCameras = camBaseCamera::GetPool()->GetSize();
	for(s32 i=0; i<maxNumCameras; i++)
	{
		camBaseCamera* camera = camBaseCamera::GetPool()->GetSlot(i);
		if(camera)
		{
			camera->DebugRender();
		}
	}

	const int numDirectors = ms_Directors.GetCount();
	for(int i=0; i<numDirectors; i++)
	{
		camBaseDirector* director = ms_Directors[i];
		if(director)
		{
			director->DebugRender();
		}
	}
}

bool DebugDisplayHangingCamera(void* pItem, void* UNUSED_PARAM(data))
{
	cameraWarningf("Camera not freed prior to session shutdown: %s (type hash: %u)", static_cast<camBaseCamera*>(pItem)->GetName(), static_cast<camBaseCamera*>(pItem)->GetClassId().GetHash());
	return true;
}

void camManager::DebugOutputNewVehicleMetadata()
{
	bool success = false;
	const camBaseCamera* activeCamera = camInterface::GetDominantRenderedCamera();
	if (activeCamera && activeCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
	{
		const camCinematicMountedCamera* cinematicCamera = static_cast<const camCinematicMountedCamera*>(activeCamera);
		const CEntity* attachParent			= cinematicCamera->GetAttachParent();
		const CVehicle* vehicle				= attachParent && attachParent->GetIsTypeVehicle() ? static_cast<const CVehicle*>(attachParent) : NULL;
		if (vehicle)
		{
			cameraDisplayf("%s", vehicle->GetModelName());
			success = true;
		}

		cameraDisplayf("<povCameraName>%s</povCameraName>", activeCamera->GetName());

		const camCinematicMountedCameraMetadata& metadata = static_cast<const camCinematicMountedCameraMetadata&>(cinematicCamera->GetMetadata());
		const CVehicleModelInfo* modelInfo	= vehicle ? vehicle->GetVehicleModelInfo() : NULL;

		const CPed* followPed				= camInterface::FindFollowPed();
		const bool isFollowPedDriver		= followPed && followPed->GetIsDrivingVehicle();

		Vector3 relativeAttachPosition		= metadata.m_RelativeAttachPosition;
		if (modelInfo)
		{
			if (isFollowPedDriver)
			{
				relativeAttachPosition		+= modelInfo->GetPovCameraOffset();
				cameraDisplayf("<PovCameraOffset x=\"%.6f\" y=\"%.6f\" z=\"%.6f\" />", relativeAttachPosition.x, relativeAttachPosition.y, relativeAttachPosition.z);
			}
			else
			{
				
			    const CVehicleSeatInfo* seatInfo = vehicle ? vehicle->GetSeatInfo(followPed) : NULL;
				if (!seatInfo->GetIsFrontSeat())
				{
					relativeAttachPosition	+= modelInfo->GetPovRearPassengerCameraOffset();
					cameraDisplayf("<PovRearPassengerCameraOffset x=\"%.6f\" y=\"%.6f\" z=\"%.6f\" />", relativeAttachPosition.x, relativeAttachPosition.y, relativeAttachPosition.z);
				}
				else
				{
					relativeAttachPosition	+= modelInfo->GetPovPassengerCameraOffset();
					cameraDisplayf("<PovPassengerCameraOffset x=\"%.6f\" y=\"%.6f\" z=\"%.6f\" />", relativeAttachPosition.x, relativeAttachPosition.y, relativeAttachPosition.z);
				}

				relativeAttachPosition		+= modelInfo->GetPovCameraOffset();
				cameraDisplayf("(Abs x=\"%.3f\" y=\"%.3f\" z=\"%.3f\")", relativeAttachPosition.x, relativeAttachPosition.y, relativeAttachPosition.z);
			}	
		}	
	}

	if (!success)
	{
		cameraDisplayf("DEFAULTS");
		cameraDisplayf("<povCameraName>DEFAULT_POV_CAMERA</povCameraName>");
		cameraDisplayf("<PovCameraOffset x=\"0.000000\" y=\"0.000000\" z=\"0.680000\" />");
		cameraDisplayf("<PovCameraVerticalAdjustmentForRollCage value=\"0.000000\" />");
	}
}

void camManager::InitWidgets()
{
	//Create the camera bank.
	bkBank& bank = BANKMGR.CreateBank("Camera", 0, 0, false); 
	bank.AddButton("Create camera widgets", datCallback(CFA1(camManager::AddWidgets), &bank));
}

void camManager::AddWidgets()
{
	//Destroy first widget which is the create button.
	bkWidget* widget = BANKMGR.FindWidget("Camera/Create camera widgets");
	if(widget == NULL)
	{
		return;
	}

	widget->Destroy();

	//Find the top-level camera bank.
	bkBank* bank = BANKMGR.FindBank("Camera");
	if(bank == NULL)
	{
		return;
	}

	bank->PushGroup("Debug rendering", false);
	{
		bank->AddButton("Cycle through camera debug displays", datCallback(CFA(DebugCycleRenderOptions)), "Cycles through camera debug displays");
		bank->AddButton("^", datCallback(CFA(DebugSelectPreviousCamera)), "Selects the previous camera in the table");
		bank->AddButton("v", datCallback(CFA(DebugSelectNextCamera)), "Selects the next camera in the table");

		bank->AddToggle("Render cameras info", &ms_ShouldRenderCameraInfo);
		bank->PushGroup("Rendering options for selected camera", false);
		{
			bank->AddToggle("Render far clip", &ms_ShouldDebugRenderCameraFarClip);
			bank->AddToggle("Render near DOF", &ms_ShouldDebugRenderCameraNearDof);
			bank->AddToggle("Render far DOF", &ms_ShouldDebugRenderCameraFarDof);
		}
		bank->PopGroup();

		bank->AddButton("Output new vehicle metadata", datCallback(CFA(DebugOutputNewVehicleMetadata)));
		bank->AddToggle("Render third lines", &ms_ShouldDebugRenderThirdLines);
	}
	bank->PopGroup();

	ms_ShakeDebugger.AddWidgets(*bank);

	camFactory::AddWidgets(*bank);

	camInterface::AddWidgets(*bank);

	AddWidgetsForFullScreenBlurEffect(*bank);

	ms_FramePropagator.AddWidgets(*bank);

	bank->PushGroup("Renderer", false);
	{
		CViewport* gameViewport = gVpMan.GetGameViewport();	
		bank->PushGroup("CamMtx", false);
		{
			const Matrix34 *pMat34 = (Matrix34*)&gameViewport->GetGrcViewport().GetCameraMtx();
			bank->AddSlider("WorldMtx a", (Vector3*)&pMat34->a, -9999.0f, 9999.0f, 0.01f);
			bank->AddSlider("WorldMtx b", (Vector3*)&pMat34->b, -9999.0f, 9999.0f, 0.01f);
			bank->AddSlider("WorldMtx c", (Vector3*)&pMat34->c, -9999.0f, 9999.0f, 0.01f);
			bank->AddSlider("WorldMtx d", (Vector3*)&pMat34->d, -9999.0f, 9999.0f, 0.01f);
		}
		bank->PopGroup();

		bank->PushGroup("WorldMtx", false);
		{
			const Matrix34 *pMat34 = (Matrix34*)&gameViewport->GetGrcViewport().GetWorldMtx();
			bank->AddSlider("CamMtx a", (Vector3*)&pMat34->a, -9999.0f, 9999.0f, 0.001f);
			bank->AddSlider("CamMtx b", (Vector3*)&pMat34->b, -9999.0f, 9999.0f, 0.001f);
			bank->AddSlider("CamMtx c", (Vector3*)&pMat34->c, -9999.0f, 9999.0f, 0.001f);
			bank->AddSlider("CamMtx d", (Vector3*)&pMat34->d, -9999.0f, 9999.0f, 0.001f);
		}
		bank->PopGroup();

		bank->PushGroup("ViewMtx", false);
		{
			const Matrix44 *pMat44 = (Matrix44*)&gameViewport->GetGrcViewport().GetViewMtx();
			bank->AddSlider("ViewMtx a", (Vector4*)&pMat44->a, -9999.0f, 9999.0f, 0.001f);
			bank->AddSlider("ViewMtx b", (Vector4*)&pMat44->b, -9999.0f, 9999.0f, 0.001f);
			bank->AddSlider("ViewMtx c", (Vector4*)&pMat44->c, -9999.0f, 9999.0f, 0.001f);
			bank->AddSlider("ViewMtx d", (Vector4*)&pMat44->d, -9999.0f, 9999.0f, 0.001f);
		}
		bank->PopGroup();

		bank->PushGroup("ScreenMtx", false);
		{
			const Matrix44 *pMat44 = (Matrix44*)&gameViewport->GetGrcViewport().GetScreenMtx();
			bank->AddSlider("ScreenMtx a", (Vector4*)&pMat44->a, -9999.0f, 9999.0f, 0.001f);
			bank->AddSlider("ScreenMtx b", (Vector4*)&pMat44->b, -9999.0f, 9999.0f, 0.001f);
			bank->AddSlider("ScreenMtx c", (Vector4*)&pMat44->c, -9999.0f, 9999.0f, 0.001f);
			bank->AddSlider("ScreenMtx d", (Vector4*)&pMat44->d, -9999.0f, 9999.0f, 0.001f);
		}
		bank->PopGroup();

		bank->PushGroup("CompositeMtx", false);
		{
			const Matrix44 *pMat44 = (Matrix44*)&gameViewport->GetGrcViewport().GetCompositeMtx();
			bank->AddSlider("CompositeMtx a", (Vector4*)&pMat44->a, -9999.0f, 9999.0f, 0.001f);
			bank->AddSlider("CompositeMtx b", (Vector4*)&pMat44->b, -9999.0f, 9999.0f, 0.001f);
			bank->AddSlider("CompositeMtx c", (Vector4*)&pMat44->c, -9999.0f, 9999.0f, 0.001f);
			bank->AddSlider("CompositeMtx d", (Vector4*)&pMat44->d, -9999.0f, 9999.0f, 0.001f);
		}
		bank->PopGroup();

		bank->PushGroup("ModelViewMtx", false);
		{
			const Matrix44 *pMat44 = (Matrix44*)&gameViewport->GetGrcViewport().GetModelViewMtx();
			bank->AddSlider("ModelViewMtx a", (Vector4*)&pMat44->a, -9999.0f, 9999.0f, 0.001f);
			bank->AddSlider("ModelViewMtx b", (Vector4*)&pMat44->b, -9999.0f, 9999.0f, 0.001f);
			bank->AddSlider("ModelViewMtx c", (Vector4*)&pMat44->c, -9999.0f, 9999.0f, 0.001f);
			bank->AddSlider("ModelViewMtx d", (Vector4*)&pMat44->d, -9999.0f, 9999.0f, 0.001f);
		}
		bank->PopGroup();
	}
	bank->PopGroup();

	//Add widgets for directors.
	const int numDirectors = ms_Directors.GetCount();
	for(int i=0; i<numDirectors; i++)
	{
		camBaseDirector* director = ms_Directors[i];
		if(director)
		{
			director->AddWidgetsForInstance();
		}
	}

	//Add widgets for cameras.
	const int maxNumCameras = camBaseCamera::GetPool()->GetSize();
	for(int i=0; i<maxNumCameras; i++)
	{
		camBaseCamera* camera = camBaseCamera::GetPool()->GetSlot(i);
		if(camera)
		{
			camera->AddWidgetsForInstance();
		}
	}
}

void camManager::AddWidgetsForFullScreenBlurEffect(bkBank& bank)
{
	bank.PushGroup("Full-screen blur effect", false);
	{
		bank.AddButton("Start", datCallback(CFA(DebugStartFullScreenBlurEffect)));
		bank.AddButton("Release", datCallback(CFA(DebugReleaseFullScreenBlurEffect)));
		bank.AddButton("Abort", datCallback(CFA(DebugAbortFullScreenBlurEffect)));

		bank.AddSlider("Max strength", &(ms_DebugFullScreenBlurEffectEnvelopeMetadata.m_SustainLevel), 0.0f, 1.0f, 0.01f);

		parStructure* structure = ms_DebugFullScreenBlurEffectEnvelopeMetadata.parser_GetStructure();
		if(structure)
		{
			rage::parBuildWidgetsVisitor buildWidgetsVisitor;
			buildWidgetsVisitor.m_CurrBank = &bank;

			rage::parPtrToStructure ptrToStructure = ms_DebugFullScreenBlurEffectEnvelopeMetadata.parser_GetPointer();

			parMember* member = structure->FindMember("AttackDuration");
			if(member)
			{
				buildWidgetsVisitor.VisitMember(ptrToStructure, *member);
			}

			member = structure->FindMember("HoldDuration");
			if(member)
			{
				buildWidgetsVisitor.VisitMember(ptrToStructure, *member);
			}

			member = structure->FindMember("ReleaseDuration");
			if(member)
			{
				buildWidgetsVisitor.VisitMember(ptrToStructure, *member);
			}

			member = structure->FindMember("AttackCurveType");
			if(member)
			{
				buildWidgetsVisitor.VisitMember(ptrToStructure, *member);
			}

			member = structure->FindMember("ReleaseCurveType");
			if(member)
			{
				buildWidgetsVisitor.VisitMember(ptrToStructure, *member);
			}
		}

		bank.AddToggle("Should use game (vs camera) time", &ms_ShouldDebugFullScreenBlurEffectUseGameTime);
	}
	bank.PopGroup(); //Full-screen blur effect.
}

UnapprovedCameraLists camManager::ms_UnapprovedCameraLists;
bool camManager::ms_UnapprovedCameraListsLoaded = false;

void camManager::LoadUnapprovedList()
{
	if (!sysBootManager::IsPackagedBuild() && !ms_UnapprovedCameraListsLoaded)
	{
		ASSET.PushFolder("common:/non_final/anim");

		if (!PARSER.LoadObject("UnapprovedCameraLists", "meta", ms_UnapprovedCameraLists))
		{
			Assertf(0, "Failed to load UnapprovedCameraLists");
		}

		ASSET.PopFolder();

		ms_UnapprovedCameraListsLoaded = true;
	}
}

void camManager::DebugInitFullScreenBlurEffect()
{
	//Assign sensible defaults to the local envelope metadata. This is non-final data and is therefore not in the camera metadata.
	ms_DebugFullScreenBlurEffectEnvelopeMetadata.m_AttackDuration	= 100;
	ms_DebugFullScreenBlurEffectEnvelopeMetadata.m_HoldDuration		= 100;
	ms_DebugFullScreenBlurEffectEnvelopeMetadata.m_ReleaseDuration	= 500;
	ms_DebugFullScreenBlurEffectEnvelopeMetadata.m_AttackCurveType	= CURVE_TYPE_SLOW_IN_OUT;
	ms_DebugFullScreenBlurEffectEnvelopeMetadata.m_ReleaseCurveType	= CURVE_TYPE_SLOW_IN_OUT;
}

void camManager::DebugStartFullScreenBlurEffect()
{
	//Kill any existing effect.
	DebugAbortFullScreenBlurEffect();

	ms_DebugFullScreenBlurEffectEnvelope = camFactory::CreateObject<camEnvelope>(ms_DebugFullScreenBlurEffectEnvelopeMetadata);
	if(ms_DebugFullScreenBlurEffectEnvelope)
	{
		ms_DebugFullScreenBlurEffectEnvelope->SetUseGameTime(ms_ShouldDebugFullScreenBlurEffectUseGameTime);
		ms_DebugFullScreenBlurEffectEnvelope->Start();
	}
}

void camManager::DebugReleaseFullScreenBlurEffect()
{
	if(ms_DebugFullScreenBlurEffectEnvelope)
	{
		ms_DebugFullScreenBlurEffectEnvelope->Release();
	}
}

void camManager::DebugAbortFullScreenBlurEffect()
{
	if(ms_DebugFullScreenBlurEffectEnvelope)
	{
		delete ms_DebugFullScreenBlurEffectEnvelope;
		ms_DebugFullScreenBlurEffectEnvelope = NULL;
	}
}

void camManager::DebugUpdateFullScreenBlurEffect(camFrame& renderedFrame)
{
	if(!ms_DebugFullScreenBlurEffectEnvelope)
	{
		return;
	}

	const float envelopeLevel		= ms_DebugFullScreenBlurEffectEnvelope->Update();
	float fullScreenBlurBlendLevel	= renderedFrame.GetDofFullScreenBlurBlendLevel();
	fullScreenBlurBlendLevel		= Lerp(envelopeLevel, fullScreenBlurBlendLevel, 1.0f);

	renderedFrame.SetDofFullScreenBlurBlendLevel(fullScreenBlurBlendLevel);
}

void camManager::DebugFlushScriptCommands()
{
	for (int index = 0; index < ms_DebugRegisteredScriptCommands.GetCount(); ++index)
	{
		TUNE_GROUP_INT(CAMERA_MANAGER, iDebugScriptCommandFramesToLive, 30, 1, 9999, 1);
		if (fwTimer::GetFrameCount() - ms_DebugRegisteredScriptCommands[index].time >= (u32)iDebugScriptCommandFramesToLive)
		{
			ms_DebugRegisteredScriptCommands.Delete(index);
			--index;
		}
	}
}

void camManager::DebugRenderScriptCommands(u8& rowIndex)
{
	const int registeredCommandsCount = ms_DebugRegisteredScriptCommands.GetCount();
	if (registeredCommandsCount == 0)
	{
		return;
	}

	++rowIndex;

	DebugRenderTableEntry(DIRECTOR_NAME_COLUMN, rowIndex, "Script Commands", g_DebugRenderTableColumnColors[DIRECTOR_NAME_COLUMN], CAMERA_NAME_COLUMN);

	// Render back to front, so we see newest commands first.
	for (int index = registeredCommandsCount - 1; index >= 0; index--)
	{
		rowIndex++;
		atVarString frameText("F:%d", ms_DebugRegisteredScriptCommands[index].time);
		const bool isThisFrame = ms_DebugRegisteredScriptCommands[index].time == fwTimer::GetFrameCount();
		DebugRenderTableEntry(DIRECTOR_BLEND_COLUMN, rowIndex, frameText, isThisFrame ? Color_red : Color_default, CAMERA_NAME_COLUMN);

		atVarString commandText("%s", ms_DebugRegisteredScriptCommands[index].text.c_str());
		DebugRenderTableEntry(CAMERA_NAME_COLUMN, rowIndex, commandText.c_str(), Color_default, FLAGS_COLUMN);
	}
}

namespace {
#if __NO_OUTPUT
	void CameraScriptPrintFunc(const char* UNUSED_PARAM(fmt), ...) {}
#else
	void CameraScriptPrintFunc(const char* fmt, ...)
	{
		va_list args;
		va_start(args, fmt);
		diagLogDefault(Channel_camera, DIAG_SEVERITY_DEBUG3, __FILE__, __LINE__, fmt, args);
		va_end(args);
	}
#endif	// !__NO_OUTPUT
}

void camManager::DebugRegisterScriptCommand(const char* callerName, const char* format, ...)
{
	scrThread* pThread = rage::scrThread::GetActiveThread();

	char argumentsBuffer[512];
	va_list args;
	va_start(args, format);
	vformatf(argumentsBuffer, sizeof(argumentsBuffer), format, args);
	va_end(args);

	atVarString fullText("Script(%s) %s(%s)", pThread ? pThread->GetScriptName() : "Console", callerName, argumentsBuffer);

	//print the script command full text in the output
	TUNE_GROUP_BOOL(CAMERA_MANAGER, bPrintCameraScriptCommands, true);
	if (bPrintCameraScriptCommands)
	{
		cameraDebugf2("%s", fullText.c_str());
	}

	TUNE_GROUP_BOOL(CAMERA_MANAGER, bPrintCameraScriptCommandsCallstack, true);
	if (bPrintCameraScriptCommandsCallstack)
	{
		if (pThread)
		{
			pThread->PrintStackTrace(CameraScriptPrintFunc);
		}
	}

	for (int index = 0; index < ms_DebugRegisteredScriptCommands.GetCount(); ++index)
	{
		DebugScriptCommand& entry = ms_DebugRegisteredScriptCommands[index];
		if (entry.text == fullText)
		{
			//if we find a command entry with the same debug string, we just update the timer and return
			entry.time = fwTimer::GetFrameCount();
			return;
		}
	}


	//allocating a new command
	DebugScriptCommand& entry = ms_DebugRegisteredScriptCommands.Grow();
	entry.text = fullText;
	entry.time = fwTimer::GetFrameCount();
}

#endif // __BANK
