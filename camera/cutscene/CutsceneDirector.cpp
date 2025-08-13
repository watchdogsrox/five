//
// camera/cutscene/CutsceneDirector.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cutscene/CutsceneDirector.h"

#include "bank/bkmgr.h"

#include "fwsys/timer.h"
#include "grcore/debugdraw.h"

#include "crmetadata/tag.h"
#include "crmetadata/tags.h"
#include "animation/EventTags.h"

#include "camera/cinematic/CinematicDirector.h"
#include "camera/switch/SwitchDirector.h"
#include "camera/scripted/ScriptDirector.h"
#include "camera/switch/SwitchDirector.h"
#include "camera/syncedscene/SyncedSceneDirector.h"
#include "camera/cutscene/AnimatedCamera.h"
#include "camera/system/CameraFactory.h"
#include "camera/system/CameraMetadata.h"
#include "cutscene/CutSceneManagerNew.h"
#include "camera/CamInterface.h"
#include "Cutscene/cutscene_channel.h"
#include "Peds/ped.h"

#include "frontend/HudTools.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camCutsceneDirector,0xCAE82C3C)

static const float g_FovScalingForWidscreenBordersIn4To3 = 1.295f; //Mondo's magic number!

camCutsceneDirector::camCutsceneDirector(const camBaseObjectMetadata& metadata)
: camBaseDirector(metadata)
, m_Metadata(static_cast<const camCutsceneDirectorMetadata&>(metadata))
, m_CurrentPlayingCamera(-1)
, m_OverriddenFarClipThisUpdate(-1.0f)
, m_IsPerformingFirstPersonBlendOut(false)

#if __BANK
, m_DebugOverriddenFarClipThisUpdate(g_DefaultFarClip)
, m_DebugShouldBypassRendering(false)
, m_DebugShouldOverrideFarClipThisUpdate(false)
#endif // __BANK
{
}


camCutsceneDirector::~camCutsceneDirector()
{
	DestroyAllCameras();
}

void camCutsceneDirector::UpdateActiveRenderState()
{
	//If the cinematic director is currently rendering, we should not interpolate in or out, so skip the interpolation.
	const bool isCinematicDirectorRendering = camInterface::GetCinematicDirector().IsRendering();
	const bool isSwitchDirectorRendering = camInterface::GetSwitchDirector().IsRendering();
	const bool isSyncedSceneDirectorRendering = camInterface::GetSyncedSceneDirector().IsRendering();
	const bool isScriptDirectorRendering = camInterface::GetScriptDirector().IsRendering();

	camFirstPersonAimCamera* pCam = camInterface::GetGameplayDirector().GetFirstPersonAimCamera(); 
	
	bool shouldAbortToCinematicOrGameplay = (isCinematicDirectorRendering || pCam) && !isSyncedSceneDirectorRendering && !isScriptDirectorRendering; 
	
	if(isSwitchDirectorRendering || shouldAbortToCinematicOrGameplay)
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

bool camCutsceneDirector::Update()
{
	bool hasSucceeded = false;

#if __BANK
	if(m_DebugShouldOverrideFarClipThisUpdate)
	{
		m_OverriddenFarClipThisUpdate = m_DebugOverriddenFarClipThisUpdate;
	}
#endif // __BANK

	m_UpdatedCamera = GetActiveCutSceneCamera();
	if(m_UpdatedCamera != NULL)
	{
		if(CutSceneManager::GetInstance())
		{
			float currentTime = CutSceneManager::GetInstance()->GetCutSceneCurrentTime();
			
			camAnimatedCamera* pCamera = static_cast<camAnimatedCamera*>(m_UpdatedCamera.Get());
			pCamera->SetCurrentTime(currentTime);

			//Create scene matrix.
			Matrix34 SceneMat(M34_IDENTITY);
			CutSceneManager::GetInstance()->GetSceneOrientationMatrix(SceneMat); 

			pCamera->SetSceneMatrix(SceneMat);
		}

		m_Frame.CloneFrom(m_CachedInitialDestinationFrame);

		//Update the active section camera.
		hasSucceeded = m_UpdatedCamera->BaseUpdate(m_Frame);
		if(hasSucceeded && !CHudTools::GetWideScreen())
		{
			//If we are in 4:3 then adjust the FOV so that we can render cutscene at 16:9 with black borders.
			m_Frame.SetFov(m_Frame.GetFov() * g_FovScalingForWidscreenBordersIn4To3);
		}
	}

#if __BANK
	if(m_DebugShouldBypassRendering)
	{
		BypassRenderingThisUpdate();
	}
#endif // __BANK

	if (!(IsRendering() || IsInterpolating()))
	{
		m_IsPerformingFirstPersonBlendOut = false;
	}

	//Reset to an invalid value at the end of every update.
	m_OverriddenFarClipThisUpdate = -1.0f;

	return hasSucceeded;
}

u32 camCutsceneDirector::GetTimeInMilliseconds() const
{
	u32 timeInMilliseconds = camBaseDirector::GetTimeInMilliseconds();

	const CutSceneManager* cutsceneManager = CutSceneManager::GetInstance();
	if(cutsceneManager)
	{
		const float sceneTime	= cutsceneManager->GetCutSceneCurrentTime();
		timeInMilliseconds		= static_cast<u32>(Floorf(sceneTime * 1000.0f));
	}

	return timeInMilliseconds;
}

camBaseCamera* camCutsceneDirector::GetActiveCutSceneCamera() const
{	
	for (s32 i = 0; i < m_sCutSceneCams.GetCount() ; i++)
	{
		if (m_sCutSceneCams[i].m_CameraId == m_CurrentPlayingCamera)
		{
			camAnimatedCamera* pCamera = static_cast<camAnimatedCamera*>(m_sCutSceneCams[i].m_regdCam.Get());
			
			if (pCamera)
			{
				return pCamera;
			}
		}
	}

	return NULL;
}

//Sets and plays the animations on a camera, the time given is the event time 
bool camCutsceneDirector::PlayCameraAnim(const Vector3 &vCutsceneOffset, const strStreamingObjectName cAnimDict, const crClip* pClip, s32 CameraId, float fStartTime)
{
	for (s32 i = 0; i < m_sCutSceneCams.GetCount(); i++)
	{
		if (m_sCutSceneCams[i].m_CameraId == CameraId)
		{
			camAnimatedCamera* pCamera = static_cast<camAnimatedCamera*>(m_sCutSceneCams[i].m_regdCam.Get());
			if (pCamera)
			{
				m_CurrentPlayingCamera = CameraId; 

				//save overriden clip values
				float overriddenFarClip = pCamera->GetOverriddenFarClip();
				float overriddenNearClip = pCamera->GetOverriddenNearClip();

				//reset our camera data
				pCamera->Init();

				//This assert will mean that something bad has failed, we have reset the camera but have not played an anim.
				if(cameraVerifyf(pCamera->SetCameraClip(vCutsceneOffset, cAnimDict, pClip, fStartTime), "Failed to initialise the animation for a cutscene section camera"))
				{
					// Look for any BlockCutsceneSkipping tags on the clip and set the time on the manager accordingly.
					if (pClip)
					{
						const crTags* pTags = pClip->GetTags();
						if (pTags)
						{
							for (int i=0; i < pTags->GetNumTags(); i++)
							{
								const crTag* pTag = pTags->GetTag(i);
								if (pTag)
								{
									static CClipEventTags::Key blockCutsceneSkippingKey("BlockCutsceneSkipping", 0xf870c7af);
									if (pTag->GetKey() == blockCutsceneSkippingKey)
									{
										const float fStart = pTag->GetStart();
										float fTagTimeInCutsceneTime = fStartTime + pClip->ConvertPhaseToTime(fStart);

										CutSceneManager::GetInstance()->SetSkippedBlockedByCameraAnimTagTime(fTagTimeInCutsceneTime);
										break;
									}
								}
							}
						}
					}

					//restore overridden clip values
					pCamera->OverrideCameraDrawDistance(overriddenFarClip, overriddenNearClip);

					//Set initial phase based on the current time and the event time. This needs to be done on animation section cross overs (for translation, rotation etc.)
					pCamera->UpdateFrame(GetPhase(CutSceneManager::GetInstance()->GetCutSceneCurrentTime()));	
					return true;
				}
				else
				{
					return false;
				}
			}
		}
	}

	return false;
}


//Creates a cut scene camera, with this system the camera will be unique and will have to be reset 
void camCutsceneDirector::CreateCamera(s32 iCameraId)
{
	//cam is initiated in the constructor
	camAnimatedCamera* camera = camFactory::CreateObject<camAnimatedCamera>(m_Metadata.m_AnimatedCameraRef.GetHash());
	if(cameraVerifyf(camera, "Failed to create a cutscene camera (name: %s, hash: %u) - is it a valid camera?",
		SAFE_CSTRING(m_Metadata.m_AnimatedCameraRef.GetCStr()), m_Metadata.m_AnimatedCameraRef.GetHash()))
	{
		//Add a reference to this camera to our array a map. 

		sCutSceneCams& CutSceneCams = m_sCutSceneCams.Grow();

		CutSceneCams.m_CameraId = iCameraId;
		CutSceneCams.m_regdCam = RegdCamBaseCamera(camera);
	}
}

void camCutsceneDirector::DestroyCamera(s32 iCameraId)
{
	for (s32 i = 0; i < m_sCutSceneCams.GetCount(); i++)
	{
		if (m_sCutSceneCams[i].m_CameraId == iCameraId)
		{
			if (m_sCutSceneCams[i].m_regdCam)
			{
				delete m_sCutSceneCams[i].m_regdCam;
			}

			m_sCutSceneCams.Delete(i);
			
			if(iCameraId == m_CurrentPlayingCamera)
			{
				m_CurrentPlayingCamera = -1; 
			}
			break;
		}
	}
}

void camCutsceneDirector::DestroyAllCameras()
{
	for (s32 i = 0; i < m_sCutSceneCams.GetCount(); i++)
	{
		if (m_sCutSceneCams[i].m_regdCam)
		{
			delete m_sCutSceneCams[i].m_regdCam;
		}
	}

	m_sCutSceneCams.Reset();

	m_CurrentPlayingCamera = -1; 
}

//Returns the phase of the camera animation for this section.
float camCutsceneDirector::GetCurrentCameraPhaseInUse() const
{
	const camAnimatedCamera* pCam	= static_cast<camAnimatedCamera*> (GetActiveCutSceneCamera());
	const float phase		= pCam ? pCam->GetPhase() : -1.0f;

	return phase;
}

void camCutsceneDirector::SetDrawDistance(const float farClip, const float nearClip)
{
	camAnimatedCamera* pCam = static_cast<camAnimatedCamera*> (GetActiveCutSceneCamera());
	if (pCam)
	{
		pCam->OverrideCameraDrawDistance(farClip, nearClip); 
	}
}

void camCutsceneDirector::SetUseDayCoCTrack(bool UseDayCoC)
{
	camAnimatedCamera* pCam = static_cast<camAnimatedCamera*> (GetActiveCutSceneCamera());
	if (pCam)
	{
		pCam->SetShouldUseDayCoc(UseDayCoC); 
	}
}

void camCutsceneDirector::SetJumpCutStatus(camAnimatedCamera::eJumpCutStatus iJumpCutStatus)
{
	camAnimatedCamera* pCam = static_cast<camAnimatedCamera*> (GetActiveCutSceneCamera());
	if (pCam)
	{
		pCam->SetJumpCutStatus(iJumpCutStatus); 
	}
}	

bool camCutsceneDirector::IsCutScenePlaying()
{
	camAnimatedCamera* pCam		= static_cast<camAnimatedCamera*> (GetActiveCutSceneCamera());
	const bool isPlaying	= pCam && pCam->GetCutSceneAnimation().GetClip();

	return isPlaying; 
}

camAnimatedCamera::eJumpCutStatus camCutsceneDirector::GetJumpCutStatus()
{
	camAnimatedCamera* pCam = static_cast<camAnimatedCamera*> (GetActiveCutSceneCamera());
	const camAnimatedCamera::eJumpCutStatus status = pCam ? pCam->GetJumpCutStatus() : camAnimatedCamera::NO_JUMP_CUT;

	return status;
}

//bool camCutsceneDirector::WillCamerCutThisFrame(float CurrentTime)
//{
//	camAnimatedCamera* pCam = static_cast<camAnimatedCamera*> (GetActiveCutSceneCamera());
//	bool status = pCam && pCam->WillCamerCutThisFrame(CurrentTime); 
//
//	return status;
//}

bool camCutsceneDirector::GetCameraPosition(Vector3 &vPos) const
{
	const camAnimatedCamera* pCam	= static_cast<camAnimatedCamera*> (GetActiveCutSceneCamera());
	bool hasSucceeded	= false;

	if (pCam && pCam->HasAnimation())
	{
		hasSucceeded = pCam->GetPosition(pCam->GetPhase(), vPos);
	}

	return hasSucceeded;
}

bool camCutsceneDirector::GetCameraWorldMatrix(Matrix34& Mat)
{
	camAnimatedCamera* pCam	= static_cast<camAnimatedCamera*> (GetActiveCutSceneCamera());
	bool hasSucceeded	= false;

	if (pCam)
	{
		Matrix34 mMat(pCam->GetMatrix()); 
		
		Mat = mMat; 
		hasSucceeded = true; 
	}

	return hasSucceeded;
}

bool camCutsceneDirector::IsCameraWithIdActive(s32 CamId) const
{
	const bool isActive = (CamId == m_CurrentPlayingCamera);

	return isActive;
}

float camCutsceneDirector::GetPhase(float fAnimTime) const
{
	camAnimatedCamera* pCam	= static_cast<camAnimatedCamera*> (GetActiveCutSceneCamera());
	const float phase	= pCam ? pCam->GetPhase(fAnimTime) : 0.0f;

	return phase; 
}

#if __BANK	

//Bank: Set the cameras matrix
void camCutsceneDirector::OverrideCameraDofThisFrame(const Vector4 &dofPlanes)
{
	camAnimatedCamera* pCam = static_cast<camAnimatedCamera*> (GetActiveCutSceneCamera());
	if (pCam)
	{
		pCam->OverrideCameraDof(dofPlanes); 
	}
}

void camCutsceneDirector::OverrideCameraDofThisFrame(float nearDof, float farDof, float dofStrength)
{
	camAnimatedCamera* pCam = static_cast<camAnimatedCamera*> (GetActiveCutSceneCamera());
	if (pCam)
	{
		pCam->OverrideCameraDof(nearDof, farDof, dofStrength); 
	}
}

void camCutsceneDirector::OverrideCameraMatrixThisFrame(const Matrix34 &mMat)
{
	camAnimatedCamera* pCam = static_cast<camAnimatedCamera*> (GetActiveCutSceneCamera());
	if (pCam)
	{
		pCam->OverrideCameraMatrix(mMat); 
	}
}

void camCutsceneDirector::OverrideLightDofThisFrame(bool state)
{
	camAnimatedCamera* pCam = static_cast<camAnimatedCamera*> (GetActiveCutSceneCamera());
	if (pCam)
	{
		pCam->OverrideCameraCanUseLightDof(state);
	}
}


void camCutsceneDirector::OverrideSimpleDofThisFrame(bool state)
{
	camAnimatedCamera* pCam = static_cast<camAnimatedCamera*> (GetActiveCutSceneCamera());
	if (pCam)
	{
		pCam->OverrideCameraCanUseSimpleDof(state);
	}
}

void camCutsceneDirector::OverrideDofPlaneStrength(float dofStrength)
{
	camAnimatedCamera* pCam = static_cast<camAnimatedCamera*> (GetActiveCutSceneCamera());
	if (pCam)
	{
		pCam->OverrideCameraDofStrength(dofStrength);
	}
}

void camCutsceneDirector::OverrideDofPlaneStrengthModifier(float dofStrengthModifier)
{
	camAnimatedCamera* pCam = static_cast<camAnimatedCamera*> (GetActiveCutSceneCamera());
	if (pCam)
	{
		pCam->OverrideCameraCoCModifier(dofStrengthModifier);
	}
}


void camCutsceneDirector::OverrideMotionBlurThisFrame(float motionBlur)
{
	camAnimatedCamera* pCam = static_cast<camAnimatedCamera*> (GetActiveCutSceneCamera());
	if (pCam)
	{
		pCam->OverrideCameraMotionBlur(motionBlur);
	}
}

void camCutsceneDirector::OverrideCameraFovThisFrame(float fov)
{
	camAnimatedCamera* pCam = static_cast<camAnimatedCamera*> (GetActiveCutSceneCamera());
	if (pCam)
	{
		pCam->OverrideCameraFov(fov); 
	}
}

void camCutsceneDirector::OverrideCameraFocusPointThisFrame(float focusPoint)
{
	camAnimatedCamera* pCam = static_cast<camAnimatedCamera*> (GetActiveCutSceneCamera());
	if (pCam)
	{
		pCam->OverrideCameraFocusPoint(focusPoint); 
	}
}

#endif

bool camCutsceneDirector::GetCamFrame(camFrame& frame )
{
	camAnimatedCamera* pCam = static_cast<camAnimatedCamera*> (GetActiveCutSceneCamera());
	if (pCam)
	{
		frame = pCam->GetFrameNonConst(); 
		return true;
	}
	return false;
}

void camCutsceneDirector::InterpolateOutToEndOfScene(u32 interpTime, bool toFirstPerson)
{
	const CutSceneManager* cutsceneManager = CutSceneManager::GetInstance();
	if(cutsceneManager)
	{
		u32 interpolationDuration = interpTime; 

		// Calculate remaining time
		const float currentSceneTime	= cutsceneManager->GetCutSceneCurrentTime();
		const float sceneDuration		= cutsceneManager->GetEndTime();
		const float sceneTimeRemaining	= Max(sceneDuration - currentSceneTime, 0.0f);
		const u32 remainingDuration 	= static_cast<u32>(Floorf(sceneTimeRemaining * 1000.0f));
		
		// Do not blend past the end of the cutscene or we'll get an extra 1-frame of cutscene camera.
		if (interpolationDuration > remainingDuration)
		{
			cutsceneWarningf("Cutscene %s being interpolated out to the end of the scene for %u milliseconds, but there are only %u milliseconds remaining.  Clamping to the remaining duration.", cutsceneManager->GetCutsceneName(), interpolationDuration, remainingDuration);
			interpolationDuration = remainingDuration;
		}

		StopRendering(interpolationDuration);

		if (toFirstPerson && interpTime>0)
		{
			m_IsPerformingFirstPersonBlendOut = true;
		}
	}
}

void camCutsceneDirector::UpdateGameplayCameraForFirstPersonBlendOut() const
{
	if (m_IsPerformingFirstPersonBlendOut && IsInterpolating())
	{
		// set the relative heading on the gameplay camera to put it between the cut scene camera and the ped
		Vec3V camPosition = VECTOR3_TO_VEC3V(GetFrameNoPostEffects().GetPosition());

		CPed* pPlayerPed = CGameWorld::FindLocalPlayer();

		if (pPlayerPed)
		{
			camPosition = UnTransformOrtho(pPlayerPed->GetMatrix(), camPosition);
			float gamePlayHeading = fwAngle::GetRadianAngleBetweenPoints(0.0f, 0.0f, camPosition.GetXf(), camPosition.GetYf());
			camInterface::GetGameplayDirector().SetUseFirstPersonFallbackHeading(gamePlayHeading);

			float orbitDistance = Mag(camPosition).Getf()*0.75f;
			Vector2 distanceLimits(orbitDistance, orbitDistance);

			camInterface::GetGameplayDirector().SetScriptOverriddenThirdPersonCameraOrbitDistanceLimitsThisUpdate(distanceLimits);
		}		
	}
}

#if __BANK

void camCutsceneDirector::AddWidgetsForInstance()
{
	if(m_WidgetGroup == NULL)
	{
		bkBank* bank = BANKMGR.FindBank("Camera");
		if(bank != NULL)
		{
			m_WidgetGroup = bank->PushGroup("Cutscene director", false);
			{
				bank->AddToggle("Should bypass rendering", &m_DebugShouldBypassRendering);
				bank->AddToggle("Should override far clip (this update)", &m_DebugShouldOverrideFarClipThisUpdate);
				bank->AddSlider("Overridden far clip (this update)", &m_DebugOverriddenFarClipThisUpdate, -1.0f, 9999.0f, 0.1f);
			}
			bank->PopGroup(); //Cutscene director.
		}
	}
}

//Bank: Render debug camera text
void camCutsceneDirector::DisplayCutSceneCameraDebug()
{
	camAnimatedCamera* pCam = static_cast<camAnimatedCamera*> (GetActiveCutSceneCamera());
	if (pCam && pCam->GetCutSceneAnimation().GetClip())
	{
		pCam->DebugRender(); 
	}
}

#endif // __BANK
