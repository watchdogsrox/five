/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CCutSceneCameraEntity.cpp
// PURPOSE : 
// AUTHOR  : Thomas French
// STARTED : 
//
/////////////////////////////////////////////////////////////////////////////////

//Rage Headers
#include "cutfile/cutfobject.h"
#include "cutscene/cutsevent.h"
#include "cutscene/cutsmanager.h"
#include "cutfile/cutfeventargs.h"
#include "cutscene/cutseventargs.h"
#include "camera/cutscene/AnimatedCamera.h"

//Game Headers
#include "camera/scripted/ScriptDirector.h"
#include "camera/system/CameraFactory.h"
#include "camera/system/CameraMetadata.h"
#include "cutscene/CutSceneCameraEntity.h"
#include "cutscene/cutscenecustomevents.h"
#include "cutscene/CutSceneAnimManager.h"
#include "cutscene/AnimatedModelEntity.h"
#include "camera/caminterface.h"
#include "camera/cutscene/CutsceneDirector.h"
#include "camera/gameplay/GameplayDirector.h"
#include "cutscene/CutSceneManagerNew.h"
#include "Cutscene/cutscene_channel.h"
#include "renderer/PostProcessFX.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/Water.h"
#include "control/replay/Misc/LightPacket.h"

/////////////////////////////////////////////////////////////////////////////////
ANIM_OPTIMISATIONS()
/////////////////////////////////////////////////////////////////////////////////

#if __BANK
atString CCutSceneCameraEntity::ms_DofState; 
atString CCutSceneCameraEntity::ms_CoCMod;
atString CCutSceneCameraEntity::ms_DofPlanes; 
atString CCutSceneCameraEntity::ms_DofEffect; 
atString CCutSceneCameraEntity::ms_Blending; 
bool CCutSceneCameraEntity::m_RenderCameraStatus = false; 
#endif

CCutSceneCameraEntity::CCutSceneCameraEntity (const cutfObject* pObject)
:cutsUniqueEntity (pObject)
, m_CameraReadyForGame(false)
{
	m_EnableDOF = false; 
	m_CanTerminateThisFrame = false; 
	m_bIsRegisteredEntityScriptControlled = false; 
	m_ShouldUseCatchUpCameraExit = false;
	m_pCameraCutEventArgs = NULL; 
	m_HaveAppliedShadowBounds = false; 
	m_BlendOutEventProcessed = false;
	m_FpsCutFlashTriggered = false;
	m_ShouldProcessFirstPersonBlendOutEvents = false; 
	m_AreCatchingUpFromFirstPerson = false; 
	m_shouldForceFirstPersonModeIfTransitioningToAVehicle = false; 
	m_ReturningToGameState.ClearAllFlags(); 
#if __BANK
	m_DofState = DOF_DATA_DRIVEN; 
	m_pTargetEntity = NULL; 
	m_DrawDistanceEventName[0] = '\0'; 
	m_bFaceModeEnabled = false; 
	m_fZoomDistance = 0.4f; 
	m_bTriggeredFromWidget = false; 
#endif

	m_CascadeShadowFocusEntityForSeamlessExit = NULL;
	m_bAllowSettingOfCascadeShadowFocusEntity = false;
}

CCutSceneCameraEntity::~CCutSceneCameraEntity ()
{
	//cutsceneDisplayf("CCutSceneCameraEntity::%s  %d",  m_pCutfCameraObj->GetDisplayName().c_str(), fwTimer::GetFrameCount()); 
	camInterface::GetCutsceneDirector().DestroyAllCameras();
#if __BANK
	ms_DofState.Clear();
	ms_CoCMod.Clear();  
	ms_DofPlanes.Clear(); 
	ms_DofEffect.Clear();
#endif //__BANK
}

void CCutSceneCameraEntity::UpdateDof()
{
	//bool bUseHqDof = m_EnableDOF; 
	//			if (GetDisableHighQualityDof())
	//			{
	//				bUseHqDof = false;
	//			}

	if(m_EnableDOF)
	{
		camInterface::GetCutsceneDirector().SetHighQualityDofThisUpdate(true);
	}
}

#if __BANK
void CCutSceneCameraEntity::CalculateCameraWorldMatFromFace(Matrix34 &mMat)
{
		Matrix34 CameraMat;
		CameraMat.Identity();

		Vector3 heading = Vector3(-3.2f, 4.5f, 0.1f);

		CameraMat.FromEulersXYZ(heading);

		Vector3 vOffset = Vector3(0.0f, (0.3f + m_fZoomDistance), 0.0f);

		CameraMat.d = vOffset;

		CameraMat.Dot(mMat); 

		mMat = CameraMat; 
}

void CCutSceneCameraEntity::UpdateFaceViewer()
{
	if(m_bFaceModeEnabled && m_pTargetEntity && m_pTargetEntity->GetType() == CUTSCENE_ACTOR_GAME_ENITITY)
	{
		const CCutsceneAnimatedActorEntity* pActor = static_cast<const CCutsceneAnimatedActorEntity*> (m_pTargetEntity);

		Vector3 vPos = VEC3_ZERO; 
		Matrix34 Mat;
		Mat.Identity(); 
		if(pActor->GetActorBonePosition(BONETAG_HEAD, Mat) )
		{
			CalculateCameraWorldMatFromFace(Mat); 
			camInterface::GetCutsceneDirector().OverrideCameraMatrixThisFrame(Mat);
		}
	}
};

void CCutSceneCameraEntity::UpdateDofState(CutSceneManager* pCutsceneManager)
{
	m_DofState	= pCutsceneManager->GetDebugManager().GetDofState(); 

	switch (m_DofState)
	{
	case DOF_DATA_DRIVEN:
		{
			UpdateDof();
		}
		break;

	case DOF_FORCED_ON:
		{
			camInterface::GetCutsceneDirector().SetHighQualityDofThisUpdate(true);
		}
		break;

	case DOF_FORCED_OFF:
		{
			camInterface::GetCutsceneDirector().SetHighQualityDofThisUpdate(false);
		}
		break;

	default:
		{
			UpdateDof(); 
		}
		break; 
	};
}

void CCutSceneCameraEntity::UpdateOverriddenCamera(CutSceneManager* pCutsceneManager)
{
	camInterface::GetCutsceneDirector().OverrideCameraDofThisFrame(pCutsceneManager->GetDebugManager().m_vDofPlanes); 

	camInterface::GetCutsceneDirector().OverrideCameraFovThisFrame(pCutsceneManager->GetDebugManager().m_fFov); 

	camInterface::GetCutsceneDirector().OverrideMotionBlurThisFrame(pCutsceneManager->GetDebugManager().m_fMotionBlurStrength);

	camInterface::GetCutsceneDirector().OverrideDofPlaneStrength((float)pCutsceneManager->GetDebugManager().m_CoCRadius); 

	camInterface::GetCutsceneDirector().OverrideDofPlaneStrengthModifier((float)pCutsceneManager->GetDebugManager().m_CoCModifier);

	camInterface::GetCutsceneDirector().OverrideCameraFocusPointThisFrame((float)pCutsceneManager->GetDebugManager().m_FocusPoint);
}

void CCutSceneCameraEntity::OverrideCameraProperties(cutsManager *pManager)
{
	CutSceneManager* pCutsceneManager = static_cast<CutSceneManager*>(pManager); 
	camFrame Frame; 

	UpdateDofState(pCutsceneManager); 

	if(camInterface::GetCutsceneDirector().GetCamFrame(Frame))
	{
		if(pCutsceneManager->GetDebugManager().ProfilingIsDofDisabled() || pCutsceneManager->GetDebugManager().ProfilingUseDefaultFov())
		{
			// Cutscene profiling
			if (pCutsceneManager->GetDebugManager().ProfilingIsDofDisabled())
			{
				camInterface::GetCutsceneDirector().SetHighQualityDofThisUpdate(false);
				camInterface::GetCutsceneDirector().OverrideSimpleDofThisFrame(false);
			}

			if (pCutsceneManager->GetDebugManager().ProfilingUseDefaultFov())
			{
				camInterface::GetCutsceneDirector().OverrideCameraFovThisFrame(45.0f);
			}
		}
		else if(pCutsceneManager->GetDebugManager().m_bOverrideCamUsingBinary)
		{
			UpdateOverriddenCamera(pCutsceneManager); 
			camInterface::GetCutsceneDirector().OverrideCameraMatrixThisFrame(pCutsceneManager->GetDebugManager().m_binaryCamMatrix); 
		}
		else if(pCutsceneManager->GetDebugManager().m_bOverrideCamUsingMatrix)
		{
			UpdateOverriddenCamera(pCutsceneManager); 
			camInterface::GetCutsceneDirector().OverrideCameraMatrixThisFrame(pCutsceneManager->GetDebugManager().m_cameraMtx);
		}
		else if(pCutsceneManager->GetDebugManager().m_bOverrideCam)
		{
			UpdateOverriddenCamera(pCutsceneManager); 

			Matrix34 mCameraMat; 
			mCameraMat.Identity();

			mCameraMat.FromEulersYXZ(pCutsceneManager->GetDebugManager().m_vCameraRot * DtoR);

			mCameraMat.d = pCutsceneManager->GetDebugManager().m_vCameraPos; 

			camInterface::GetCutsceneDirector().OverrideCameraMatrixThisFrame(mCameraMat); 

	
		} 
		else
		{
			Frame.ComputeDofPlanes(pCutsceneManager->GetDebugManager().m_vDofPlanes); 

			pCutsceneManager->GetDebugManager().m_fFov = Frame.GetFov(); 

			pCutsceneManager->GetDebugManager().m_vCameraPos = Frame.GetPosition(); 
			pCutsceneManager->GetDebugManager().m_vCameraRot.x = Frame.ComputePitch() * RtoD; 
			pCutsceneManager->GetDebugManager().m_vCameraRot.y = Frame.ComputeRoll()* RtoD; 
			pCutsceneManager->GetDebugManager().m_vCameraRot.z = Frame.ComputeHeading()* RtoD; 
			
			pCutsceneManager->GetDebugManager().m_bShouldOverrideUseLightDof = Frame.GetFlags().IsFlagSet(camFrame::Flag_ShouldUseLightDof); 
			pCutsceneManager->GetDebugManager().m_bShouldOverrideUseSimpleDof = Frame.GetFlags().IsFlagSet(camFrame::Flag_ShouldUseSimpleDof); 

			pCutsceneManager->GetDebugManager().m_fMotionBlurStrength = Frame.GetMotionBlurStrength(); 

			float modifiedDofValue = Frame.GetDofBlurDiskRadius(); 
			
			// Compute the non time of day adjusted dof strength
			float ActualDof = modifiedDofValue - m_LastTimeOfDayDofModifier; 
			
			ActualDof = Clamp(ActualDof, 1.0f, 15.0f); 

			pCutsceneManager->GetDebugManager().m_CoCRadius = (int) ActualDof; 

			if(pCutsceneManager->GetDebugManager().m_bOverrideCoCModifier)
			{
				camInterface::GetCutsceneDirector().OverrideDofPlaneStrengthModifier((float)pCutsceneManager->GetDebugManager().m_CoCModifier);
			}
			else
			{
				pCutsceneManager->GetDebugManager().m_CoCModifier = (int)m_LastTimeOfDayDofModifier; 
			}
		}

	
	}
}

#endif

bool CCutSceneCameraEntity::IsCuttingBackEarlyForScript() const
{
	fwFlags32 ScriptExitFlags;
	ScriptExitFlags.SetFlag(SCRIPT_REQUSTED_RETURN);

	if(m_ReturningToGameState == ScriptExitFlags)
	{
		return true;
	}

	return false; 
};

#if FPS_MODE_SUPPORTED
camManager::eFirstPersonFlashEffectType CCutSceneCameraEntity::GetFirstPersonFlashEffectType(const CutSceneManager* pManager)
{
	if (pManager->GetOptionFlags().IsFlagSet(CUTSCENE_PLAYER_FP_FLASH_MICHAEL))
	{
		return camManager::CAM_PUSH_IN_FX_MICHAEL;
	}
	else if (pManager->GetOptionFlags().IsFlagSet(CUTSCENE_PLAYER_FP_FLASH_FRANKLIN))
	{
		return camManager::CAM_PUSH_IN_FX_FRANKLIN;
	}
	else if (pManager->GetOptionFlags().IsFlagSet(CUTSCENE_PLAYER_FP_FLASH_TREVOR))
	{
		return camManager::CAM_PUSH_IN_FX_TREVOR;
	}

	return camManager::CAM_PUSH_IN_FX_NEUTRAL;
}

extern const u32 g_CatchUpHelperRefForFirstPerson;

u32 CCutSceneCameraEntity::GetFirstPersonFlashEffectDelay(const CutSceneManager* pManager, u32 blendOutDuration, bool bCatchup) const
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
		// check the specified blend out duration doesn't go past the end of the scene
		// (the cutscene director does this internally, so we need to do the same).
		u32 timeRemanining = (u32)((pManager->GetEndTime() - pManager->GetCutSceneCurrentTime())*1000.0f);

		if (blendOutDuration>timeRemanining)
		{
			blendOutDuration = timeRemanining;
		}

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

bool CCutSceneCameraEntity::ShouldTriggerFirstPersonCameraFlash(const CutSceneManager* pManager) const
{
	// dont trigger the flash if script told us not to
	if (pManager && pManager->GetOptionFlags().IsFlagSet(CUTSCENE_SUPPRESS_FP_TRANSITION_FLASH))
		return false;

	// never trigger if there's a scripted camera waiting in the wings
	if (camInterface::GetScriptDirector().IsRendering())
		return false;

	return WillExitToFirstPersonCamera(pManager);
}


#endif //FPS_MODE_SUPPORTED

bool CCutSceneCameraEntity::WillExitToFirstPersonCamera(const CutSceneManager* pManager) const
{
	if(pManager->GetVehicleModelHashPlayerWillExitTheSceneIn() > 0)
	{	
		s32 viewMode = camControlHelperMetadataViewMode::IN_VEHICLE; 

		CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey(pManager->GetVehicleModelHashPlayerWillExitTheSceneIn(), NULL);

		cutsceneAssertf(pModelInfo, "Invalid model hash for players exit %d", pManager->GetVehicleModelHashPlayerWillExitTheSceneIn()); 


		if(pModelInfo)
		{
			if(pModelInfo->GetModelType() == MI_TYPE_VEHICLE)
			{
				viewMode = camControlHelper::ComputeViewModeContextForVehicle(pManager->GetVehicleModelHashPlayerWillExitTheSceneIn()); 

				cutsceneAssertf(viewMode != -1, "Failed to find the correct view mode for %s", 
					pModelInfo->GetModelName()); 
			}
			else
			{
				cutsceneAssertf(0, "The vehicle model hash that the player will use when exiting the custcene is not a vehicle model %d", 
					pManager->GetVehicleModelHashPlayerWillExitTheSceneIn()); 
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
			if(pManager->GetOptionFlags().IsFlagSet(CUTSCENE_EXITS_INTO_COVER) && CPauseMenu::GetMenuPreference( PREF_FPS_THIRD_PERSON_COVER ))
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

void CCutSceneCameraEntity::DispatchEvent(cutsManager* pManager, const cutfObject* pObject, 
										  s32 iEventId, const cutfEventArgs* pEventArgs/* =NULL */, const float UNUSED_PARAM(fTime), const u32 UNUSED_PARAM(StickyId))
{
	const cutfCameraObject *pCutfCameraObj = GetCameraObject();
	FPS_MODE_SUPPORTED_ONLY(const u32 fpsFlashDelay = 300u);

	switch ( iEventId )
	{
	
	case CUTSCENE_START_OF_SCENE_EVENT:
		{
			if (!camInterface::GetCutsceneDirector().IsCameraWithIdActive(pCutfCameraObj->GetObjectId()))
			{
				camInterface::GetCutsceneDirector().CreateCamera(pCutfCameraObj->GetObjectId());	
			}

			camInterface::GetCutsceneDirector().Render();
			camManager::AbortPendingFirstPersonFlashEffect("Cutscene start of scene");

#if __BANK
			CutSceneManager* pCutsManager = static_cast <CutSceneManager*>(pManager);
			Matrix34 Mat; 
			Mat.Identity(); 
			pCutsManager->GetDebugManager().SetFinalFrameCameraMat(Mat); 
			ms_Blending.Clear(); 

#endif

			const CutSceneManager* pConstCutsManager = static_cast <const CutSceneManager*>(pManager);
			atArray<cutfEvent *> AllEventList;
			pConstCutsManager->GetCutfFile()->FindEventsForObjectIdOnly( GetCameraObject()->GetObjectId(), pConstCutsManager->GetCutfFile()->GetEventList(), AllEventList );

			//delete any existing CUTSCENE_FIRST_PERSON_BLENDOUT_CAMERA_EVENT event
			bool hasFirstPersonEvents = false; 

			for(int i =0; i < AllEventList.GetCount(); i++)
			{
				if(AllEventList[i]->GetEventId() == CUTSCENE_FIRST_PERSON_BLENDOUT_CAMERA_EVENT || AllEventList[i]->GetEventId() == CUTSCENE_FIRST_PERSON_CATCHUP_CAMERA_EVENT)
				{
					hasFirstPersonEvents = true; 
					break;
				}
			}
				
			//if script does not register a vehicle hash then assume the player is on foot
			if(hasFirstPersonEvents)
			{				
				m_ShouldProcessFirstPersonBlendOutEvents = WillExitToFirstPersonCamera(pConstCutsManager);
				bool IsInFirstPesonModeOnFoot = camControlHelperMetadataViewMode::FIRST_PERSON == camControlHelper::GetViewModeForContext(camControlHelperMetadataViewMode::ON_FOOT); 
				if(!IsInFirstPesonModeOnFoot && m_ShouldProcessFirstPersonBlendOutEvents)
				{
					//	camControlHelper::SetViewModeForContext(camControlHelperMetadataViewMode::ON_FOOT, camControlHelperMetadataViewMode::FIRST_PERSON);
					m_shouldForceFirstPersonModeIfTransitioningToAVehicle = true; 
					camInterface::GetGameplayDirector().OverrideFPCameraThisUpdate(); 
				}
			}

			bool ShouldUseDayCoC = ShouldUseDayCoCTrack(pManager); 
			camInterface::GetCutsceneDirector().SetUseDayCoCTrack(ShouldUseDayCoC); 
			cutsceneCameraEntityDebugf(pObject, "CUTSCENE_START_OF_SCENE_EVENT");	
		}
		break;

	case CUTSCENE_STOP_EVENT:
		{
			cutsceneCameraEntityDebugf(pObject, "CUTSCENE_STOP_EVENT");	
			m_CanTerminateThisFrame = true; 
		}
		break;

	case CUTSCENE_END_OF_SCENE_EVENT:
		{	
			cutsceneCameraEntityDebugf(pObject, "CUTSCENE_END_OF_SCENE_EVENT");

			//@@: location CCUTSCENECAMERAENTITY_DISPATCHEVENT_CUTSCENE_END_OF_SCENE
			SetCameraReadyForGame(pManager, END_OF_SCENE_RETURN);

			if (camInterface::GetCutsceneDirector().IsCameraWithIdActive(pCutfCameraObj->GetObjectId()))
			{
				camInterface::GetCutsceneDirector().DestroyCamera(pCutfCameraObj->GetObjectId());	
			}

		}
		break;

	case CUTSCENE_SCENE_ORIENTATION_CHANGED_EVENT:
		{

		}
		break;
#if __BANK
	case CUTSCENE_SET_FACE_ZOOM_EVENT:
		{	
			m_bFaceModeEnabled = true; 
			m_pTargetEntity = NULL; 

			//Get hold of the id of the object that we want to look at. 
			if (pEventArgs->GetType() == CUTSCENE_OBJECT_ID_EVENT_ARGS_TYPE)
			{
				const cutfObjectIdEventArgs* pArgs = static_cast<const cutfObjectIdEventArgs*>(pEventArgs);
				if(pArgs)
				{
					s32 id = pArgs->GetObjectId(); 
					m_pTargetEntity = pManager->GetEntityByObjectId(id);

					UpdateFaceViewer(); 

				}
			}
		}
		break;

	case CUTSCENE_CLEAR_FACE_ZOOM_EVENT:
		{
			m_bFaceModeEnabled = false; 
			m_pTargetEntity = NULL; 
		}
		break;
	
	case CUTSCENE_SET_FACE_ZOOM_DISTANCE_EVENT:
		{
			if(m_bFaceModeEnabled)
			{	
				if (pEventArgs->GetType() == CUTSCENE_FLOAT_VALUE_EVENT_ARGS_TYPE)
				{
					const cutfFloatValueEventArgs* pArgs = static_cast<const cutfFloatValueEventArgs*>(pEventArgs); 
					
					m_fZoomDistance = pArgs->GetFloat1(); 
				}
			}
		}
		break;

	case CUTSCENE_SET_FACE_ZOOM_NEAR_DRAW_DISTANCE_EVENT:
		{
			if(m_bFaceModeEnabled)
			{	
				if (pEventArgs->GetType() == CUTSCENE_FLOAT_VALUE_EVENT_ARGS_TYPE)
				{
					const cutfFloatValueEventArgs* pArgs = static_cast<const cutfFloatValueEventArgs*>(pEventArgs); 
					camInterface::GetCutsceneDirector().SetDrawDistance(100.0f,  pArgs->GetFloat1()); 
				}
			}
		}
		break;

	case CUTSCENE_SET_FACE_ZOOM_FAR_DRAW_DISTANCE_EVENT:
		{
			if(m_bFaceModeEnabled)
			{
				if (pEventArgs->GetType() == CUTSCENE_FLOAT_VALUE_EVENT_ARGS_TYPE)
				{
					const cutfFloatValueEventArgs* pArgs = static_cast<const cutfFloatValueEventArgs*>(pEventArgs);
					camInterface::GetCutsceneDirector().SetDrawDistance(pArgs->GetFloat1(), 0.15f); 
				}
			}
		}
		break;

#endif // !__FINAL

//#if !__FINAL		
//	//manual override of the DOF 
//	case CUTSCENE_ENABLE_DEPTH_OF_FIELD_EVENT:
//		{
//			m_OverrideDOF = true; 
//		}
//		break; 
//
//	case CUTSCENE_DISABLE_DEPTH_OF_FIELD_EVENT:
//		{
//			m_OverrideDOF = false; 
//		}
//	break;
//#endif

	//Events stored in the cutfile as exported by the artists 
	case CUTSCENE_ENABLE_DOF_EVENT:
		{
			cutsceneCameraEntityDebugf(pObject, "CUTSCENE_ENABLE_DOF_EVENT");	
			m_EnableDOF = true;
		}
	break;

	case CUTSCENE_DISABLE_DOF_EVENT:
		{
			cutsceneCameraEntityDebugf(pObject, "CUTSCENE_DISABLE_DOF_EVENT");	
			m_EnableDOF = false;

			// This is the start of the exit, we can start returning the cascade shadow entity at this point
			m_bAllowSettingOfCascadeShadowFocusEntity = true;

			if(m_HaveAppliedShadowBounds)
			{
				CRenderPhaseCascadeShadowsInterface::Script_InitSession();

				// If we've specified a specific entity that we want the cascade shadows to focus on during the seamless exit, then we need to turn off dynamic depth mode.
				if (m_CascadeShadowFocusEntityForSeamlessExit != NULL)
				{
					CRenderPhaseCascadeShadowsInterface::Script_EnableDynamicDepthModeInCutscenes(false);
				}

				m_HaveAppliedShadowBounds = false;
			}
			else
			{
				// If we've specified a specific entity that we want the cascade shadows to focus on during the seamless exit, then we need to turn off dynamic depth mode.
				if (m_CascadeShadowFocusEntityForSeamlessExit != NULL)
				{
					CRenderPhaseCascadeShadowsInterface::Script_InitSession();
					CRenderPhaseCascadeShadowsInterface::Script_EnableDynamicDepthModeInCutscenes(false);
				}
			}
		}
	break; 

	case CUTSCENE_CATCHUP_CAMERA_EVENT:
		{
			bool shouldProceesCatchUp = true;
			const CutSceneManager* pConstCutsManager = static_cast <const CutSceneManager*>(pManager);
			bool willExitToFirstPerson = WillExitToFirstPersonCamera(pConstCutsManager);

			if(willExitToFirstPerson && pConstCutsManager->WasSkipped())
			{
				shouldProceesCatchUp = false;
			}

			if(!m_ShouldProcessFirstPersonBlendOutEvents && shouldProceesCatchUp)
			{
				m_ShouldUseCatchUpCameraExit = true;
				m_ReturningToGameState.SetFlag(CATCHING_UP_THIRD_PERSON); 
			}
		}
	break;

	case CUTSCENE_BLENDOUT_CAMERA_EVENT:
		{
			if(!m_ShouldProcessFirstPersonBlendOutEvents)
			{
				cutsceneCameraEntityDebugf(pObject, "CUTSCENE_BLENDOUT_CAMERA_EVENT");	

				const CutSceneManager* pCutsManager = static_cast <const CutSceneManager*>(pManager);
		
	#if __BANK
				if (pCutsManager->GetDebugManager().m_bOverrideCamBlendOutEvents)
					return;
	#endif //__BANK

				if(!pCutsManager->WasSkipped())
				{
					s32 blendOutFrames = pCutsManager->GetCutfFile()->GetBlendOutCutsceneDuration(); 

					//covert the duration in to mili-seconds
					u32 blendOutTime = u32(((float) blendOutFrames / CUTSCENE_FPS) * 1000.0f);

					cutsceneDebugf1("%s CCutSceneCameraEntity::CUTSCENE_BLENDOUT_CAMERA_EVENT BlendOutDuration: %d , BlendOutOffset:%d ", pObject->GetDisplayName().c_str(), blendOutFrames, pCutsManager->GetCutfFile()->GetBlendOutCutsceneOffset()); 

					camInterface::GetCutsceneDirector().InterpolateOutToEndOfScene(blendOutTime, pCutsManager->GetOptionFlags().IsFlagSet(CUTSCENE_USE_FP_CAMERA_BLEND_OUT_MODE) ? WillExitToFirstPersonCamera(pCutsManager) : false);
					m_CanTerminateThisFrame = true;
					m_BlendOutEventProcessed = true;

					m_ReturningToGameState.SetFlag(BLENDING_BACK_TO_THIRD_PERSON);
#if FPS_MODE_SUPPORTED
					if (ShouldTriggerFirstPersonCameraFlash(pCutsManager))
					{
						camManager::TriggerFirstPersonFlashEffect(GetFirstPersonFlashEffectType(pCutsManager), GetFirstPersonFlashEffectDelay(pCutsManager, blendOutTime, false), "Cutscene blend out camera");
						m_FpsCutFlashTriggered = true;
					}
#endif // FPS_MODE_SUPPORTED
				}
			}
		}
		break;


	case CUTSCENE_FIRST_PERSON_BLENDOUT_CAMERA_EVENT:
		{
			if(m_ShouldProcessFirstPersonBlendOutEvents)
			{
				cutsceneCameraEntityDebugf(pObject, "CUTSCENE_BLENDOUT_CAMERA_EVENT");	

				const CutSceneManager* pCutsManager = static_cast <const CutSceneManager*>(pManager);

	#if __BANK
				if (pCutsManager->GetDebugManager().m_bOverrideCamBlendOutEvents)
					return;
	#endif //__BANK

				if(!pCutsManager->WasSkipped())
				{
					const cutfFloatValueEventArgs* pArgs = static_cast<const cutfFloatValueEventArgs*>(pEventArgs); 
				
					u32 blendOutDurationMilliSeconds = 0; 

					if (pEventArgs->GetType() == CUTSCENE_FLOAT_VALUE_EVENT_ARGS_TYPE)
					{
						float blendOutDurationInFrames = pArgs->GetFloat1(); 
						blendOutDurationMilliSeconds =u32(blendOutDurationInFrames * 1000.0f);

						cutsceneDebugf1("%s CCutSceneCameraEntity::CUTSCENE_FIRST_PERSON_BLENDOUT_CAMERA_EVENT BlendOutDuration: %d ", pObject->GetDisplayName().c_str(), blendOutDurationMilliSeconds); 
					}
#if __BANK
					atVarString blending("First Person Blending: %d", blendOutDurationMilliSeconds); 
					ms_Blending = blending; 
#endif
#if FPS_MODE_SUPPORTED
					if (ShouldTriggerFirstPersonCameraFlash(pCutsManager))
					{
						camManager::TriggerFirstPersonFlashEffect(GetFirstPersonFlashEffectType(pCutsManager), GetFirstPersonFlashEffectDelay(pCutsManager, blendOutDurationMilliSeconds, false), "Cutscene fp blend out camera");
						m_FpsCutFlashTriggered = true;
					}
#endif // FPS_MODE_SUPPORTED
					camInterface::GetCutsceneDirector().InterpolateOutToEndOfScene(blendOutDurationMilliSeconds, WillExitToFirstPersonCamera(pCutsManager));
					m_CanTerminateThisFrame = true;
					m_BlendOutEventProcessed = true;

					m_ReturningToGameState.SetFlag(BLENDING_BACK_TO_FIRST_PERSON); 
				}
			}
		}
		break;

	case CUTSCENE_FIRST_PERSON_CATCHUP_CAMERA_EVENT:
		{
			if(m_ShouldProcessFirstPersonBlendOutEvents)
			{
				const camFrame& catchUpSourceFrame = camInterface::GetCutsceneDirector().GetFrame();
				camInterface::GetGameplayDirector().CatchUpFromFrame(catchUpSourceFrame);

				camInterface::GetCutsceneDirector().StopRendering(0);

				m_CanTerminateThisFrame = true;
				m_AreCatchingUpFromFirstPerson = true; 

				m_ReturningToGameState.SetFlag(CATCHING_UP_FIRST_PERSON); 
			}
		}
		break; 

	case CUTSCENE_UPDATE_EVENT:
		{
			const CutSceneManager* pCutsManager = static_cast <const CutSceneManager*>(pManager);
			if(m_ShouldProcessFirstPersonBlendOutEvents && m_shouldForceFirstPersonModeIfTransitioningToAVehicle && !pCutsManager->WasSkipped() && !m_ReturningToGameState.IsFlagSet(CATCHING_UP_FIRST_PERSON))
			{
				camInterface::GetGameplayDirector().OverrideFPCameraThisUpdate(); 
			}

#if FPS_MODE_SUPPORTED
			//Handle flashing on the final cut (i.e. no blend outs or catchups)
			if (
				!(m_ReturningToGameState.IsFlagSet(CATCHING_UP_THIRD_PERSON) && pCutsManager->GetVehicleModelHashPlayerWillExitTheSceneIn()==0) // if we're not doing a catchup to on foot
				&& ShouldTriggerFirstPersonCameraFlash(pCutsManager) // if first person flashes are allowed
				&& !m_ShouldProcessFirstPersonBlendOutEvents && !m_BlendOutEventProcessed // if we're not doing a blend out (the flash will be triggered by the blend out code in that case
				&& !m_FpsCutFlashTriggered // if we haven't already trigered a flash elsewhere
				&& !pCutsManager->WasSkipped()	// if the scene wasn't skipped
				)
			{
				// Handle flashes on the final cut of the cutscene
				if ((pCutsManager->GetEndTime() - pCutsManager->GetCutSceneCurrentTime())<=(((float)fpsFlashDelay)/1000.0f))
				{
					camManager::TriggerFirstPersonFlashEffect(GetFirstPersonFlashEffectType(pCutsManager), 0, "Cutscene final cut");
					m_FpsCutFlashTriggered = true;
				}
			}
#endif // FPS_MODE_SUPPORTED

			bool ShouldUseDayCoC = ShouldUseDayCoCTrack(pManager); 
			camInterface::GetCutsceneDirector().SetUseDayCoCTrack(ShouldUseDayCoC); 
#if !__BANK	
		
			UpdateDof(); 
#else
			OverrideCameraProperties(pManager); 
			UpdateFaceViewer(); 
#endif

		}
		break;

	case CUTSCENE_PAUSE_EVENT:
		{
#if __BANK
			
			OverrideCameraProperties(pManager); 
			UpdateFaceViewer();
#endif
		}
		break; 

	case CUTSCENE_SET_CLIP_EVENT:
		{
			cutsceneCameraEntityDebugf(pObject, "CUTSCENE_SET_CLIP_EVENT");
			if (pEventArgs && pEventArgs->GetType() == CUTSCENE_CLIP_EVENT_ARGS_TYPE)
			{
				const cutsClipEventArgs *pClipEventArgs = static_cast<const cutsClipEventArgs *>( pEventArgs );
				camInterface::GetCutsceneDirector().PlayCameraAnim(pManager->GetSceneOffset(), pClipEventArgs->GetAnimDict(), pClipEventArgs->GetClip(), pObject->GetObjectId(), pManager->GetSectionStartTime(pManager->GetCurrentSection()));
				//Displayf("camera: CUTSCENE_SET_CLIP_EVENT %d", fwTimer::GetFrameCount()); 
			}
		}
		break;

	case CUTSCENE_CLEAR_ANIM_EVENT:
		{
			cutsceneCameraEntityDebugf(pObject, "CUTSCENE_CLEAR_ANIM_EVENT");
		}
		break;

	case CUTSCENE_CAMERA_CUT_EVENT:
		{
			if (pObject)		
			{	
				float TimeOfDayDOfModifier = 0.0f;
				
				if(pEventArgs && pEventArgs->GetType() == CUTSCENE_CAMERA_CUT_EVENT_ARGS_TYPE)
				{
					const cutfCameraCutEventArgs* pCameraEvents = static_cast<const cutfCameraCutEventArgs*>(pEventArgs); 

					camInterface::GetCutsceneDirector().SetDrawDistance( pCameraEvents->GetFarDrawDistance() ,pCameraEvents->GetNearDrawDistance()); 
					cutsceneCameraEntityDebugf(pObject, "CUTSCENE_CAMERA_CUT_EVENT(Far Draw Distance %f, Near Draw Distance %f)", pCameraEvents->GetFarDrawDistance(), pCameraEvents->GetNearDrawDistance());
					
#if __BANK
					//store the name of the event so we can display the current camera cut we are on.
					formatf(m_DrawDistanceEventName, sizeof(m_DrawDistanceEventName), "Cut (%d): %s ", s32(pManager->GetCutSceneTimeWithRangeOffset() * CUTSCENE_FPS), pCameraEvents->GetName().GetCStr());				
#endif
					
					TimeOfDayDOfModifier = CalculateTimeOfDayDofModifier(pCameraEvents); 

					m_pCameraCutEventArgs = pCameraEvents;

#if GTA_REPLAY
					if(CReplayMgr::ShouldRecord())
					{
						CReplayMgr::RecordFx<CPacketCutsceneCameraArgs>(CPacketCutsceneCameraArgs(pCameraEvents));
					}
#endif

#if __BANK
					m_LastTimeOfDayDofModifier = TimeOfDayDOfModifier; 
#endif

					m_CharacterLight.Reset(); 
				}

				camAnimatedCamera* pCamera = static_cast<camAnimatedCamera*>(camInterface::GetCutsceneDirector().GetActiveCutSceneCamera()); 
				if (pCamera)
				{
					pCamera->SetDofStrengthModifier(TimeOfDayDOfModifier); 
					pCamera->SetHasCut(true);
				}

				Lights::ClearCutsceneLightsFromPrevious();

				CutSceneManager* pCutsManager = static_cast <CutSceneManager*>(pManager);
				if(pCutsManager)
				{
					pCutsManager->SetHasCutThisFrame(true);
				}
			}
		}
		break;

	case CUTSCENE_SHOW_DEBUG_LINES_EVENT:
		{
#if __BANK			
			m_RenderCameraStatus = true; 
#endif
		}
		break; 

	case CUTSCENE_HIDE_DEBUG_LINES_EVENT:
	{
#if __BANK			
		m_RenderCameraStatus = false; 
#endif
	}
	break; 

	case CUTSCENE_SET_DRAW_DISTANCE_EVENT:
		{
			if (pObject)		
			{	
				if(pEventArgs && pEventArgs->GetType() == CUTSCENE_TWO_FLOAT_VALUES_EVENT_ARGS_TYPE)
				{
					const cutfTwoFloatValuesEventArgs* pCameraEvents = static_cast<const cutfTwoFloatValuesEventArgs*>(pEventArgs);
					camInterface::GetCutsceneDirector().SetDrawDistance(pCameraEvents->GetFloat2(), pCameraEvents->GetFloat1()  );
					cutsceneCameraEntityDebugf(pObject, "CUTSCENE_CLEAR_ANIM_EVENT(Far draw distance %f, Near draw distance %f)", pCameraEvents->GetFloat2(), pCameraEvents->GetFloat1());
				}
			}
		}
		break;


	//Ok not a hack can change the event type to CUTSCENE_EVENT_TYPE and these will get dispatched to all objects. 
	case CutSceneCustomEvents::CUTSCENE_SET_WATER_REFRACT_INDEX_EVENT:
		{
			if (pObject)		
			{	
				if(pEventArgs && pEventArgs->GetType() == CUTSCENE_FLOAT_VALUE_EVENT_ARGS_TYPE)
				{
					const cutfFloatValueEventArgs* pArgs = static_cast<const cutfFloatValueEventArgs*>(pEventArgs);
					Water::SetUnderwaterRefractionIndex(pArgs->GetFloat1());

					cutsceneCameraEntityDebugf(pObject, "CUTSCENE_SET_WATER_REFRACT_INDEX_EVENT(Underwater Refraction Index %f)", pArgs->GetFloat1());
				}
			}
		}
		break; 
 
	case CUTSCENE_ENABLE_CASCADE_SHADOW_BOUNDS_EVENT:
		{
			if(pEventArgs && pEventArgs->GetType() == CUTSCENE_CASCADE_SHADOW_EVENT_ARGS)
			{
				const cutfCascadeShadowEventArgs* pShadowEventArgs = static_cast<const cutfCascadeShadowEventArgs*>(pEventArgs);
				int index = pShadowEventArgs->GetCascadeShadowIndex(); 
				bool enabled = pShadowEventArgs->GetIsEnabled();
				Vector3 position = pShadowEventArgs->GetPosition(); 
				float radiusScale = pShadowEventArgs->GetRadius(); 
				
				if(index != -1)
				{
					CRenderPhaseCascadeShadowsInterface::Script_SetCascadeBounds(index, enabled, position.x, position.y, position.z, radiusScale);
				}

				cutsceneCameraEntityDebugf(pObject, "CUTSCENE_ENABLE_CASCADE_SHADOW_BOUNDS_EVENT(Index %i, Is Enabled %i, positionX %f, positionY %f, positionZ %f, radius scale %f)",
				index, enabled, position.x, position.y, position.z, radiusScale );
				m_HaveAppliedShadowBounds = true; 
			}
		}
		break;
	
	case CUTSCENE_CASCADE_SHADOWS_ENABLE_ENTITY_TRACKER:
		{
			if(pEventArgs && pEventArgs->GetType() == CUTSCENE_BOOL_VALUE_EVENT_ARGS_TYPE)
			{
				const cutfBoolValueEventArgs* pShadowEventArgs = static_cast<const cutfBoolValueEventArgs*>(pEventArgs);
				
				CRenderPhaseCascadeShadowsInterface::SetEntityTrackerActive(pShadowEventArgs->GetValue());
			}

		}
		break;
	
	case CUTSCENE_CASCADE_SHADOWS_SET_WORLD_HEIGHT_UPDATE:
		{
			if(pEventArgs && pEventArgs->GetType() == CUTSCENE_BOOL_VALUE_EVENT_ARGS_TYPE)
			{
				const cutfBoolValueEventArgs* pShadowEventArgs = static_cast<const cutfBoolValueEventArgs*>(pEventArgs);

				CRenderPhaseCascadeShadowsInterface::Script_SetWorldHeightUpdate(pShadowEventArgs->GetValue());
			}
		}
		break;
	
	case CUTSCENE_CASCADE_SHADOWS_SET_RECEIVER_HEIGHT_UPDATE:
		{
			if(pEventArgs && pEventArgs->GetType() == CUTSCENE_BOOL_VALUE_EVENT_ARGS_TYPE)
			{
				const cutfBoolValueEventArgs* pShadowEventArgs = static_cast<const cutfBoolValueEventArgs*>(pEventArgs);

				CRenderPhaseCascadeShadowsInterface::Script_SetRecvrHeightUpdate(pShadowEventArgs->GetValue());
			}
		}
		break;
	
	case CUTSCENE_CASCADE_SHADOWS_SET_AIRCRAFT_MODE:
		{
			if(pEventArgs && pEventArgs->GetType() == CUTSCENE_BOOL_VALUE_EVENT_ARGS_TYPE)
			{
				const cutfBoolValueEventArgs* pShadowEventArgs = static_cast<const cutfBoolValueEventArgs*>(pEventArgs);

				CRenderPhaseCascadeShadowsInterface::Script_SetAircraftMode(pShadowEventArgs->GetValue());
			}
		}
		break;

	case CUTSCENE_CASCADE_SHADOWS_SET_DYNAMIC_DEPTH_MODE:
		{
			if(pEventArgs && pEventArgs->GetType() == CUTSCENE_BOOL_VALUE_EVENT_ARGS_TYPE)
			{
				const cutfBoolValueEventArgs* pShadowEventArgs = static_cast<const cutfBoolValueEventArgs*>(pEventArgs);

				CRenderPhaseCascadeShadowsInterface::Script_EnableDynamicDepthModeInCutscenes(pShadowEventArgs->GetValue());
			}
		}
		break;

	case CUTSCENE_CASCADE_SHADOWS_SET_FLY_CAMERA_MODE:
		{
			if(pEventArgs && pEventArgs->GetType() == CUTSCENE_BOOL_VALUE_EVENT_ARGS_TYPE)
			{
				const cutfBoolValueEventArgs* pShadowEventArgs = static_cast<const cutfBoolValueEventArgs*>(pEventArgs);

				CRenderPhaseCascadeShadowsInterface::Script_SetFlyCameraMode(pShadowEventArgs->GetValue());
			}
		}
		break;
		
	case CUTSCENE_CASCADE_SHADOWS_SET_CASCADE_BOUNDS_HFOV:
		{
			if(pEventArgs && pEventArgs->GetType() == CUTSCENE_FLOAT_VALUE_EVENT_ARGS_TYPE)
			{
				const cutfFloatValueEventArgs* pShadowEventArgs = static_cast<const cutfFloatValueEventArgs*>(pEventArgs);

				CRenderPhaseCascadeShadowsInterface::Script_SetCascadeBoundsHFOV(pShadowEventArgs->GetFloat1());
			}
		}
		break; 
	
	case CUTSCENE_CASCADE_SHADOWS_SET_CASCADE_BOUNDS_VFOV:
		{
			if(pEventArgs && pEventArgs->GetType() == CUTSCENE_FLOAT_VALUE_EVENT_ARGS_TYPE)
			{
				const cutfFloatValueEventArgs* pShadowEventArgs = static_cast<const cutfFloatValueEventArgs*>(pEventArgs);

				CRenderPhaseCascadeShadowsInterface::Script_SetCascadeBoundsVFOV(pShadowEventArgs->GetFloat1());
			}
		}
		break; 

	case CUTSCENE_CASCADE_SHADOWS_SET_CASCADE_BOUNDS_SCALE:
		{
			if(pEventArgs && pEventArgs->GetType() == CUTSCENE_FLOAT_VALUE_EVENT_ARGS_TYPE)
			{
				const cutfFloatValueEventArgs* pShadowEventArgs = static_cast<const cutfFloatValueEventArgs*>(pEventArgs);

				CRenderPhaseCascadeShadowsInterface::Script_SetCascadeBoundsScale(pShadowEventArgs->GetFloat1());
			}
		}
		break; 
	
	case CUTSCENE_CASCADE_SHADOWS_SET_ENTITY_TRACKER_SCALE:
		{
			if(pEventArgs && pEventArgs->GetType() == CUTSCENE_FLOAT_VALUE_EVENT_ARGS_TYPE)
			{
				const cutfFloatValueEventArgs* pShadowEventArgs = static_cast<const cutfFloatValueEventArgs*>(pEventArgs);

				CRenderPhaseCascadeShadowsInterface::Script_SetEntityTrackerScale(pShadowEventArgs->GetFloat1());
			}
		}
		break; 

	case CUTSCENE_CASCADE_SHADOWS_SET_SPLIT_Z_EXP_WEIGHT:
		{
			if(pEventArgs && pEventArgs->GetType() == CUTSCENE_FLOAT_VALUE_EVENT_ARGS_TYPE)
			{
				const cutfFloatValueEventArgs* pShadowEventArgs = static_cast<const cutfFloatValueEventArgs*>(pEventArgs);

				CRenderPhaseCascadeShadowsInterface::Script_SetSplitZExpWeight(pShadowEventArgs->GetFloat1());
			}
		}
		break; 

	case CUTSCENE_CASCADE_SHADOWS_SET_DITHER_RADIUS_SCALE:
		{
			if(pEventArgs && pEventArgs->GetType() == CUTSCENE_FLOAT_VALUE_EVENT_ARGS_TYPE)
			{
				const cutfFloatValueEventArgs* pShadowEventArgs = static_cast<const cutfFloatValueEventArgs*>(pEventArgs);

				CRenderPhaseCascadeShadowsInterface::Script_SetDitherRadiusScale(pShadowEventArgs->GetFloat1());
			}
		}
		break;

	case CUTSCENE_CASCADE_SHADOWS_SET_WORLD_HEIGHT_MINMAX:
		{
			if(pEventArgs && pEventArgs->GetType() == CUTSCENE_TWO_FLOAT_VALUES_EVENT_ARGS_TYPE)
			{
				const cutfTwoFloatValuesEventArgs* pShadowEventArgs = static_cast<const cutfTwoFloatValuesEventArgs*>(pEventArgs);

				CRenderPhaseCascadeShadowsInterface::Script_SetWorldHeightMinMax(pShadowEventArgs->GetFloat1(), pShadowEventArgs->GetFloat2());
			}
		}
		break; 

	case CUTSCENE_CASCADE_SHADOWS_SET_RECEIVER_HEIGHT_MINMAX:
		{
			if(pEventArgs && pEventArgs->GetType() == CUTSCENE_TWO_FLOAT_VALUES_EVENT_ARGS_TYPE)
			{
				const cutfTwoFloatValuesEventArgs* pShadowEventArgs = static_cast<const cutfTwoFloatValuesEventArgs*>(pEventArgs);

				CRenderPhaseCascadeShadowsInterface::Script_SetRecvrHeightMinMax(pShadowEventArgs->GetFloat1(), pShadowEventArgs->GetFloat2());
			}
		}
		break; 
	
	case CUTSCENE_CASCADE_SHADOWS_SET_DEPTH_BIAS:
		{
			if(pEventArgs && pEventArgs->GetType() == CUTSCENE_FLOAT_BOOL_EVENT_ARGS_TYPE)
			{
				const cutfFloatBoolValueEventArgs* pShadowEventArgs = static_cast<const cutfFloatBoolValueEventArgs*>(pEventArgs);

				CRenderPhaseCascadeShadowsInterface::Script_SetDepthBias(pShadowEventArgs->GetValue(), pShadowEventArgs->GetFloat1());;
			}
		}
		break; 

	case CUTSCENE_CASCADE_SHADOWS_SET_SLOPE_BIAS:
		{
			if(pEventArgs && pEventArgs->GetType() == CUTSCENE_FLOAT_BOOL_EVENT_ARGS_TYPE)
			{
				const cutfFloatBoolValueEventArgs* pShadowEventArgs = static_cast<const cutfFloatBoolValueEventArgs*>(pEventArgs);

				CRenderPhaseCascadeShadowsInterface::Script_SetSlopeBias(pShadowEventArgs->GetValue(), pShadowEventArgs->GetFloat1());;
			}
		}
		break; 

	case CUTSCENE_CASCADE_SHADOWS_SET_SHADOW_SAMPLE_TYPE:
		{
			if(pEventArgs && pEventArgs->GetType() == CUTSCENE_NAME_EVENT_ARGS_TYPE)
			{
				const cutfNameEventArgs* pShadowEventArgs = static_cast<const cutfNameEventArgs*>(pEventArgs);
				CRenderPhaseCascadeShadowsInterface::Cutscene_SetShadowSampleType(pShadowEventArgs->GetName().GetHash());
			}
		}
		break;
	case CUTSCENE_CASCADE_SHADOWS_SET_DYNAMIC_DEPTH_VALUE:
		{
			if(pEventArgs && pEventArgs->GetType() == CUTSCENE_FLOAT_VALUE_EVENT_ARGS_TYPE)
			{
				const cutfFloatValueEventArgs* pShadowEventArgs = static_cast<const cutfFloatValueEventArgs*>(pEventArgs);
				CRenderPhaseCascadeShadowsInterface::SetDynamicDepthValue(pShadowEventArgs->GetFloat1());
			}			
		}
		break;

	case CUTSCENE_CASCADE_SHADOWS_RESET_CASCADE_SHADOWS:
		{
			{
				CRenderPhaseCascadeShadowsInterface::Script_InitSession();	
				CRenderPhaseCascadeShadowsInterface::Script_EnableDynamicDepthModeInCutscenes(false);
				m_HaveAppliedShadowBounds = false;
			}
		}
		break;

	case CUTSCENE_PLAY_EVENT:
		{
			cutsceneCameraEntityDebugf(pObject, "CUTSCENE_PLAY_EVENT");

#if __BANK
			//cache this as the cutscene manager resets 
			CutSceneManager* pCutsceneManager = static_cast<CutSceneManager*>(pManager); 
			m_bTriggeredFromWidget = pCutsceneManager->WasStartedFromWidget(); 
#endif	
		}
		break;
	case CUTSCENE_STEP_FORWARD_EVENT:
	case CUTSCENE_STEP_BACKWARD_EVENT:
	case CUTSCENE_FAST_FORWARD_EVENT:
	case CUTSCENE_REWIND_EVENT:
	case CUTSCENE_RESTART_EVENT:
#if __BANK
		UpdateFaceViewer();
#endif
		break;

	
	case CUTSCENE_UPDATE_LOADING_EVENT:
		break;

	default:
		{
			
		}
		break;
	}

	//this is here to dispatch debug draw event for this entity.
#if __BANK
	cutsEntity::DispatchEvent(pManager, pObject, iEventId, pEventArgs); 
#endif

}

float CCutSceneCameraEntity::CalculateTimeOfDayDofModifier(const cutfCameraCutEventArgs* pCameraEvents)
{
	float DofModifier = 0.0f; 

	for(int i = 0; i < pCameraEvents->GetCoCModifierList().GetCount(); i++)
	{
		u32 hour = CClock::GetHour();

		if (pCameraEvents->GetCoCModifierList()[i].m_TimeOfDayFlags & (1 << hour))
		{
			DofModifier = (float)pCameraEvents->GetCoCModifierList()[i].m_DofStrengthModifier;
		}
	}

	return DofModifier; 
}


bool CCutSceneCameraEntity::ShouldUseDayCoCTrack(const cutsManager* pManager) const 
{
	if(pManager->GetCutfFile())
	{
		u32 CoCHours =  pManager->GetCutfFile()->GetDayCoCHours(); 
		u32 hour = CClock::GetHour();

		if (CoCHours & (1 << hour))
		{
			return true; 
		}
		else
		{
			return false; 
		}
	}

	return false; 
}

//////////////////////////////////////////////////////////////////////////

void CCutSceneCameraEntity::SetCameraReadyForGame(cutsManager* pManager, fwFlags32 State)
{
	if (!m_CameraReadyForGame)
	{
		m_ReturningToGameState.SetFlag(State); 
		
		cutsceneCameraEntityDebugf(GetCameraObject(), "SET_CAMERA_READY_FOR_GAME");

#if __BANK
		if(pManager)	
		{
			CutSceneManager* pCutsManager = static_cast <CutSceneManager*>(pManager);		
			if(pCutsManager)
			{
				pCutsManager->GetDebugManager().SetFinalFrameCameraMat(camInterface::GetMat()); 
				//Displayf("CUTSCENE_STOP_EVENT cam pos: %f, %f, %f", camInterface::GetMat().d.x, camInterface::GetMat().d.y, camInterface::GetMat().d.z);
				//Vector3 Angle = VEC3_ZERO;  
				//camInterface::GetMat().ToEulersYXZ(Angle); 
				//Displayf("CUTSCENE_STOP_EVENT cam angle: %f, %f, %f", Angle.x*RtoD, Angle.y*RtoD, Angle.z*RtoD);
			}
		}
#endif

		camCutsceneDirector& cutsceneDirector = camInterface::GetCutsceneDirector();

		if(m_ShouldUseCatchUpCameraExit && !m_AreCatchingUpFromFirstPerson)
		{
			const camFrame& catchUpSourceFrame = cutsceneDirector.GetFrame();
			camInterface::GetGameplayDirector().CatchUpFromFrame(catchUpSourceFrame);

			CutSceneManager* pCutsManager = static_cast <CutSceneManager*>(pManager);

			// only trigger the catchup flash if we're not exiting into a vehicle
			if(pManager && pCutsManager->GetVehicleModelHashPlayerWillExitTheSceneIn()==0)	
			{
				if (ShouldTriggerFirstPersonCameraFlash(pCutsManager) && !m_ShouldProcessFirstPersonBlendOutEvents && !m_BlendOutEventProcessed && !m_FpsCutFlashTriggered && !pCutsManager->WasSkipped())
				{
					if (m_ReturningToGameState.IsFlagSet(CATCHING_UP_THIRD_PERSON))
					{
						camManager::TriggerFirstPersonFlashEffect(GetFirstPersonFlashEffectType(pCutsManager), GetFirstPersonFlashEffectDelay(pCutsManager, 0 , true), "Cutscene catchup");
						m_FpsCutFlashTriggered = true;
					}
				}
			}
		}

		const CutSceneManager* pCutsManager = static_cast <const CutSceneManager*>(pManager);


		s32 blendOutFrames = pCutsManager->GetCutfFile()->GetBlendOutCutsceneDuration(); 

		u32 blendOutTime = 0;
		if (blendOutFrames > 0 && !m_BlendOutEventProcessed)
		{
			// Assert
			cutsceneWarningf("Cutscene %s has a blend out duration of %d frames, but there have been no CUTSCENE_BLENDOUT_CAMERA_EVENT events.", pCutsManager->GetCutsceneName(), blendOutFrames);

			// Keep blendOutTime at 0
		}
		else
		{
			// Convert the duration in to mili-seconds
			blendOutTime = u32(((float) blendOutFrames / CUTSCENE_FPS) * 1000.0f);
		}

		if(!m_ReturningToGameState.IsFlagSet(BLENDING_BACK_TO_FIRST_PERSON))
		{
			cutsceneDirector.StopRendering(blendOutTime);
		}


		//reset to the default.
		Water::SetUnderwaterRefractionIndex();

		if(m_HaveAppliedShadowBounds)
		{
			CRenderPhaseCascadeShadowsInterface::Script_InitSession(); 
			m_HaveAppliedShadowBounds = false;
		}

		m_CameraReadyForGame = true;
	}
}

/////////////////////////////////////////////////////////////////////////////////

void CCutSceneCameraEntity::SetCascadeShadowFocusEntityForSeamlessExit(const CEntity* pEntity)
{
	m_CascadeShadowFocusEntityForSeamlessExit = pEntity;
}

const CEntity* CCutSceneCameraEntity::GetCascadeShadowFocusEntityForSeamlessExit() const
{
	if (m_bAllowSettingOfCascadeShadowFocusEntity)
	{
		return m_CascadeShadowFocusEntityForSeamlessExit;
	}

	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////

#if __BANK

void CCutSceneCameraEntity::CommonDebugStr(const cutfObject* pObject, char * debugStr)
{
	if (!pObject)
	{
		return;
	}

	sprintf(debugStr, "%s - CT(%6.2f) - CF(%u) - FC(%u) - Camera Entity(%d, %s) - ", 
		CutSceneManager::GetInstance()->GetStateName(CutSceneManager::GetInstance()->GetState()),
		CutSceneManager::GetInstance()->GetCutSceneCurrentTime(),
		CutSceneManager::GetInstance()->GetCutSceneCurrentFrame(),
		fwTimer::GetFrameCount(), 
		pObject->GetObjectId(), 
		pObject->GetDisplayName().c_str());
}

/////////////////////////////////////////////////////////////////////////////////

void CCutSceneCameraEntity::DebugDraw() const 
{
	camInterface::GetCutsceneDirector().DisplayCutSceneCameraDebug(); 
	
	//Render the current camera cut 
	grcDebugDraw::AddDebugOutput(m_DrawDistanceEventName);

	RenderDofInfo();
	
	RenderDofPlanes(); 
}

void CCutSceneCameraEntity::RenderDofInfo() const
{
	if(CutSceneManager::GetInstance())
	{
		CutSceneManager* pCutsceneManager = CutSceneManager::GetInstance(); 

		camFrame Frame; 

		if(camInterface::GetCutsceneDirector().GetCamFrame(Frame))
		{
			u32 dofState	= pCutsceneManager->GetDebugManager().GetDofState(); 

			switch (dofState)
			{
			case DOF_DATA_DRIVEN:
				{
					if(m_EnableDOF)
					{
						ms_DofState = "Cutscene DOF On (Data Driven)"; 
					}
					else
					{
						ms_DofState = "Cutscene DOF Off (Data Driven: Adaptive DOF On)";
					}
				}
				break;

			case DOF_FORCED_ON:
				{
					ms_DofState = "Cutscene DOF On (Forced On Preview Only)"; 
				}
				break;

			case DOF_FORCED_OFF:
				{
					ms_DofState = "Cutscene DOF Off (Forced Off Preview Only: Adaptive DOF On)"; 
				}
			};
			
			grcDebugDraw::AddDebugOutput(ms_DofState.c_str());
			
			if(dofState != DOF_FORCED_OFF)
			{
				if(m_EnableDOF || dofState == DOF_FORCED_ON)
				{
					atVarString cocMOd("COC:%d (Modifier: %d)", pCutsceneManager->GetDebugManager().m_CoCRadius , (int)m_LastTimeOfDayDofModifier); 
					ms_CoCMod = cocMOd; 

					atVarString DofPlane("DOF Planes: %f, %f, %f, %f", pCutsceneManager->GetDebugManager().m_vDofPlanes.x, pCutsceneManager->GetDebugManager().m_vDofPlanes.y
						,pCutsceneManager->GetDebugManager().m_vDofPlanes.z, pCutsceneManager->GetDebugManager().m_vDofPlanes.w);

					ms_DofPlanes = DofPlane; 

					grcDebugDraw::AddDebugOutput(ms_DofState.c_str());
					grcDebugDraw::AddDebugOutput(ms_CoCMod.c_str()); 
					grcDebugDraw::AddDebugOutput(ms_DofPlanes.c_str());

					if(Frame.GetFlags().IsFlagSet(camFrame::Flag_ShouldUseSimpleDof))
					{
						ms_DofEffect = "Using Simple DOF"; 
					}
					else if(Frame.GetFlags().IsFlagSet(camFrame::Flag_ShouldUseLightDof))
					{
						ms_DofEffect = "Using Light DOF"; 
					}
					else
					{
						ms_DofEffect = "Full DOF Effect"; 
					}
					grcDebugDraw::AddDebugOutput(ms_DofEffect.c_str());
					grcDebugDraw::AddDebugOutput("");
				}
			}
			else
			{
				ms_CoCMod = ""; 
				ms_DofPlanes = ""; 
				ms_DofEffect = "";
			}
		}
	}
}


void CCutSceneCameraEntity::RenderDofPlanes() const
{
	 if(CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->GetDebugManager().m_RenderDofPlanes)
	 {
		 camFrame Frame; 

		 if(camInterface::GetCutsceneDirector().GetCamFrame(Frame))
		 {
			Vector4 planes = CutSceneManager::GetInstance()->GetDebugManager().m_vDofPlanes; 
			Frame.ComputeDofPlanes(planes); 
			
			Vector3 Front = Frame.GetFront(); 
			Vector3 Pos = Frame.GetPosition(); 
			
			Vector3 nearOut; 
			Vector3 nearIn; 
			Vector3 farIn; 
			Vector3 farOut; 
			Vector3 focalPoint;  

			//Scale the front vector by the planes size and add to the cameras position
			nearOut.Scale(Front, planes.x); 
			nearOut += Pos; 
			
			nearIn.Scale(Front, planes.y); 
			nearIn += Pos; 

			farIn.Scale(Front, planes.z); 
			farIn += Pos; 

			farOut.Scale(Front, planes.w); 
			farOut += Pos; 
			
			//focal point is the mid point between the near in and far in dof planes
			//float halfway = (planes.z + planes.y) / 2.0f; 
			
			//focalPoint.Scale(Front, halfway); 
			//focalPoint +=Pos; 

			camBaseCamera* pCam = camInterface::GetCutsceneDirector().GetActiveCutSceneCamera(); 

			if(pCam && 	pCam->GetIsClassId(camAnimatedCamera::GetStaticClassId()))
			{
				camAnimatedCamera* pAnimCamera = static_cast<camAnimatedCamera*>(pCam);

				float localFocalPoint = pAnimCamera->GetFocusPoint();
				
				focalPoint = YAXIS * localFocalPoint; 

				Frame.GetWorldMatrix().Transform(focalPoint); 

			}

			Vector3 vMinNearOut(-1.0f, 0.0f, -1.0f); 
			Vector3 vMaxNearOut(1.0f, 0.0f, 1.0f); 
			Vector3 vMinNearIn(-1.0f, 0.0f, -1.0f); 
			Vector3 vMaxNearIn(1.0f, 0.0f, 1.0f); 
			Vector3 vMinFarIn(-1.0f, 0.0f, -1.0f); 
			Vector3 vMaxFarIn(1.0f, 0.0f, 1.0f); 
			Vector3 vMinFarOut(-1.0f, 0.0f, -1.0f); 
			Vector3 vMaxFarOut(1.0f, 0.0f, 1.0f); 
			
			//render the planes so they keep a consistent size with respect to the screen 
			if(CutSceneManager::GetInstance()->GetDebugManager().m_scaleAsScreenRatio)
			{
				float AngleTan = rage::Tanf(0.5f * Frame.GetFov()*DtoR ); 
				float Scalar = 1.0f; 

				if(planes.x  > SMALL_FLOAT)
				{
					Scalar = planes.x * AngleTan; 
					Scalar *= 0.9f; 
					vMinNearOut.Scale(Scalar); 
					vMaxNearOut.Scale(Scalar);
				}

				if(planes.y > SMALL_FLOAT)
				{
					Scalar = planes.y * AngleTan; 
					Scalar *= 0.8f; 
					vMinNearIn.Scale(Scalar); 
					vMaxNearIn.Scale(Scalar);
				}

				if(planes.z > SMALL_FLOAT)
				{
					Scalar = planes.z * AngleTan; 
					Scalar *= 0.7f; 
					vMinFarIn.Scale(Scalar); 
					vMaxFarIn.Scale(Scalar);
				}

				if(planes.w > SMALL_FLOAT)
				{
					Scalar = planes.w * AngleTan; 
					Scalar *= 0.6f; 
					vMinFarOut.Scale(Scalar); 
					vMaxFarOut.Scale(Scalar);
				}
			}
			else
			{
				//sclae the planes absolute size			
				float Scalar = CutSceneManager::GetInstance()->GetDebugManager().m_PlaneScale;
				
				vMinNearOut.Scale(Scalar); 
				vMaxNearOut.Scale(Scalar);
				vMinNearIn.Scale(Scalar); 
				vMaxNearIn.Scale(Scalar);
				vMinFarIn.Scale(Scalar); 
				vMaxFarIn.Scale(Scalar);
				vMinFarOut.Scale(Scalar); 
				vMaxFarOut.Scale(Scalar);
			}
			
			//cploue
			Color32 NearOutColor(224, 137, 240, (int)CutSceneManager::GetInstance()->GetDebugManager().m_NearOutPlaneAlpha); 
			Color32 NearInColor(242, 156, 51,  (int)CutSceneManager::GetInstance()->GetDebugManager().m_NearInPlaneAlpha); 
			Color32 FarInColor(51, 163, 242,  (int)CutSceneManager::GetInstance()->GetDebugManager().m_FarInPlaneAlpha); 
			Color32 FarOutColor(51, 242, 99,  (int)CutSceneManager::GetInstance()->GetDebugManager().m_FarOutPlaneAlpha); 
			Color32 FocalPointColor(0, 255, 0,  (int)CutSceneManager::GetInstance()->GetDebugManager().m_FocalPointAlpha); 

			Matrix34 Draw = Frame.GetWorldMatrix(); 
			Draw.d = nearOut; 

			grcDebugDraw::SetDisableCulling(true); 
			grcDebugDraw::BoxOriented(VECTOR3_TO_VEC3V(vMinNearOut), VECTOR3_TO_VEC3V(vMaxNearOut), MATRIX34_TO_MAT34V(Draw), NearOutColor, CutSceneManager::GetInstance()->GetDebugManager().m_SetPlaneSolid); 
			
			Draw.d = nearIn; 
			grcDebugDraw::BoxOriented(VECTOR3_TO_VEC3V(vMinNearIn), VECTOR3_TO_VEC3V(vMaxNearIn),MATRIX34_TO_MAT34V(Draw), NearInColor, CutSceneManager::GetInstance()->GetDebugManager().m_SetPlaneSolid); 
			
			Draw.d = farIn; 
			grcDebugDraw::BoxOriented(VECTOR3_TO_VEC3V(vMinFarIn), VECTOR3_TO_VEC3V(vMaxFarIn),MATRIX34_TO_MAT34V(Draw), FarInColor, CutSceneManager::GetInstance()->GetDebugManager().m_SetPlaneSolid); 
			
			Draw.d = farOut; 
			grcDebugDraw::BoxOriented(VECTOR3_TO_VEC3V(vMinFarOut), VECTOR3_TO_VEC3V(vMaxFarOut), MATRIX34_TO_MAT34V(Draw), FarOutColor, CutSceneManager::GetInstance()->GetDebugManager().m_SetPlaneSolid); 
			grcDebugDraw::SetDisableCulling(false);  

			Draw.d = focalPoint; 
			grcDebugDraw::Cross(VECTOR3_TO_VEC3V(Draw.d), CutSceneManager::GetInstance()->GetDebugManager().m_FocalPointScale, FocalPointColor, 1,true);
		 }
	 }
}


bool CCutSceneCameraEntity::IsCameraPointingAtThisObject(const cutsEntity* pEnt) const 
{
	if (pEnt && pEnt  == m_pTargetEntity)
	{
		return true;
	}
	return false; 
}

#endif // __BANK

/////////////////////////////////////////////////////////////////////////////////

float CCutSceneCameraEntity::GetMapLodScale() const
{
	if(m_pCameraCutEventArgs)
	{
		return m_pCameraCutEventArgs->GetMapLodScale(); 
	}
	else
	{
		return -1.0f;
	}
}

float CCutSceneCameraEntity::GetReflectionLodRangeStart() const
{
	if(m_pCameraCutEventArgs)
	{
		return m_pCameraCutEventArgs->GetReflectionLodRangeStart(); 
	}
	else
	{
		return -1.0f;
	}
}

float CCutSceneCameraEntity::GetReflectionLodRangeEnd()const
{
	if(m_pCameraCutEventArgs)
	{
		return m_pCameraCutEventArgs->GetReflectionLodRangeEnd();
	}
	else
	{
		return -1.0f;
	}
}

float CCutSceneCameraEntity::GetReflectionSLodRangeStart()const
{
	if(m_pCameraCutEventArgs)
	{
		return m_pCameraCutEventArgs->GetReflectionSLodRangeStart();
	}
	else
	{
		return -1.0f;
	}
}

float CCutSceneCameraEntity::GetReflectionSLodRangeEnd()const
{
	if(m_pCameraCutEventArgs)
	{
		return m_pCameraCutEventArgs->GetReflectionSLodRangeEnd(); 
	}
	else
	{
		return -1.0f;
	}
}

float CCutSceneCameraEntity::GetLodMultHD()const 
{
	if(m_pCameraCutEventArgs)
	{
		return m_pCameraCutEventArgs->GetLodMultHD(); 
	}
	else
	{
		return -1.0f;
	}
}

float CCutSceneCameraEntity::GetLodMultOrphanHD()const
{
	if(m_pCameraCutEventArgs)
	{
		return m_pCameraCutEventArgs->GetLodMultOrphanHD();
	}
	else
	{
		return -1.0f;
	}
}

float CCutSceneCameraEntity::GetLodMultLod()const
{
	if(m_pCameraCutEventArgs)
	{
		return m_pCameraCutEventArgs->GetLodMultLod(); 
	}
	else
	{
		return -1.0f;
	}
}

float CCutSceneCameraEntity::GetLodMultSlod1()const
{
	if(m_pCameraCutEventArgs)
	{
		return m_pCameraCutEventArgs->GetLodMultSlod1(); 
	}
	else
	{
		return -1.0f;
	}
}

float CCutSceneCameraEntity::GetLodMultSlod2()const
{
	if(m_pCameraCutEventArgs)
	{
		return m_pCameraCutEventArgs->GetLodMultSlod2(); 
	}
	else
	{
		return -1.0f;
	}
}

float CCutSceneCameraEntity::GetLodMultSlod3()const
{
	if(m_pCameraCutEventArgs)
	{
		return m_pCameraCutEventArgs->GetLodMultSlod3(); 
	}
	else
	{
		return -1.0f;
	}
}

float CCutSceneCameraEntity::GetLodMultSlod4()const
{
	if(m_pCameraCutEventArgs)
	{
		return m_pCameraCutEventArgs->GetLodMultSlod4(); 
	}
	else
	{
		return -1.0f;
	}
}

float CCutSceneCameraEntity::GetWaterReflectionFarClip()const
{
	if(m_pCameraCutEventArgs)
	{
		return m_pCameraCutEventArgs->GetWaterReflectionFarClip(); 
	}
	else
	{
		return -1.0f;
	}
}

float CCutSceneCameraEntity::GetLightSSAOInten()const
{
	if(m_pCameraCutEventArgs)
	{
		return m_pCameraCutEventArgs->GetLightSSAOInten(); 
	}
	else
	{
		return -1.0f;
	}
}

float CCutSceneCameraEntity::GetExposurePush()const
{
	if(m_pCameraCutEventArgs)
	{
		return m_pCameraCutEventArgs->GetExposurePush(); 
	}
	else
	{
		return 0.0f;
	}
}

bool CCutSceneCameraEntity::GetDisableHighQualityDof()const
{
	if(m_pCameraCutEventArgs)
	{
		return m_pCameraCutEventArgs->GetDisableHighQualityDof(); 
	}
	else
	{
		return false;
	}
}

bool CCutSceneCameraEntity::IsReflectionMapFrozen()const
{
	if(m_pCameraCutEventArgs)
	{
		return m_pCameraCutEventArgs->IsReflectionMapFrozen(); 
	}
	else
	{
		return false;
	}
}

bool CCutSceneCameraEntity::IsDirectionalLightDisabled()const
{
	if(m_pCameraCutEventArgs)
	{
		return m_pCameraCutEventArgs->IsDirectionalLightDisabled(); 
	}
	else
	{
		return false;
	}
}

float CCutSceneCameraEntity::GetDirectionalLightMultiplier()const
{
	if(m_pCameraCutEventArgs)
	{
		return m_pCameraCutEventArgs->GetDirectionalLightMultiplier(); 
	}
	else
	{
		return -1.0f;
	}
}

float CCutSceneCameraEntity::GetLensArtefactMultiplier()const
{
	if(m_pCameraCutEventArgs)
	{
		return m_pCameraCutEventArgs->GetLensArtefactMultiplier(); 
	}
	else
	{
		return -1.0f;
	}
}

float CCutSceneCameraEntity::GetBloomMax()const
{
	if(m_pCameraCutEventArgs)
	{
		return m_pCameraCutEventArgs->GetBloomMax(); 
	}
	else
	{
		return -1.0f;
	}
}




const cutfCameraCutCharacterLightParams* CCutSceneCameraEntity::GetCharacterLightParams() const
{
#if __BANK
	if (CutSceneManager::GetInstance()->GetDebugManager().ProfilingIsOverridingCharacterLight())
	{
		return &CutSceneManager::GetInstance()->GetDebugManager().m_CharacterLight;
	}
#endif //__BANK

	return &m_CharacterLight; 
}

void CCutSceneCameraEntity::SetCharacterLightValues(float currentTime, float LightIntensity, float NightIntensity, const Vector3& Direction, const Vector3& Colour, bool UseTimeCycle, bool UseAsIntensityModifier, bool ConvertToCameraSpace)
{
	m_CharacterLight.m_fIntensity = LightIntensity; 
	m_CharacterLight.m_fNightIntensity = NightIntensity; 
	m_CharacterLight.m_vColour = Colour; 
	m_CharacterLight.m_bUseTimeCycleValues = UseTimeCycle; 
	m_CharacterLight.m_bUseAsIntensityModifier  = UseAsIntensityModifier; 

	if(ConvertToCameraSpace)
	{
		camAnimatedCamera* pCamera = static_cast<camAnimatedCamera*>(camInterface::GetCutsceneDirector().GetActiveCutSceneCamera()); 
		
		if(pCamera)
		{
			Matrix34 mMat(M34_IDENTITY);

			float phase = pCamera->GetCutSceneAnimation().GetPhase(currentTime); 

			Vector3 vPos(VEC3_ZERO);
			Quaternion qRot(Quaternion::sm_I);

			qRot = pCamera->ExtractRotation(phase, false);
			vPos = pCamera->ExtractTranslation(phase, false); 
			
			mMat.FromQuaternion(qRot);
			mMat.d = vPos;
			
			Vector3 CamRelativeDirection; 
			
			//Set the camera into scene space
			Matrix34 SceneMat(M34_IDENTITY);
			CutSceneManager::GetInstance()->GetSceneOrientationMatrix(SceneMat); 
			
			mMat.Dot3x3(SceneMat);

			mMat.UnTransform3x3(Direction, CamRelativeDirection); 

			m_CharacterLight.m_vDirection = CamRelativeDirection; 
		}
	}

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordFx<CPacketCutsceneCharacterLightParams>(CPacketCutsceneCharacterLightParams(m_CharacterLight));
	}
#endif
}