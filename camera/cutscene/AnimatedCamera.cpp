//
// camera/cutscene/AnimatedCamera.cpp
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cutscene/AnimatedCamera.h"

#include "fwanimation/animhelpers.h"
#include "fwanimation/directorcomponentsyncedscene.h"
#include "fwsys/timer.h"

#include "ai/debug/types/AnimationDebugInfo.h"
#include "camera/CamInterface.h"
#include "camera/helpers/FramePropagator.h"
#include "camera/system/CameraMetadata.h"
#include "camera/system/CameraManager.h"
#include "camera/viewports/ViewportManager.h"
#include "camera/scripted/ScriptDirector.h"
#include "camera/cutscene/CutsceneDirector.h"
#include "debug/DebugScene.h"
#include "peds/PedDebugVisualiser.h"
//#include "crmetadata/property.h"
//#include "crmetadata/propertyattributes.h"
//#include "grcore/debugdraw.h"
#include "renderer/PostProcessFX.h"
#include "scene/portals/PortalTracker.h"
//#include "text/text.h"
//#include "text/TextConversion.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camAnimatedCamera,0x913FBC61)

#if __BANK

static bool b_AddedWidget = false;

bank_float rotatextweakfloat = -90.0f;
bank_float rotateytweakfloat = -121.0f;
bank_float rotateztweakfloat = 13.0f;

Vector3 camAnimatedCamera::vCamTempOffset = Vector3 (0.0, 0.3f, 0.0);
Vector3 camAnimatedCamera::vCamTempTempAxis = VEC3_ZERO;
Vector3 camAnimatedCamera::fCamTempTempHeading = Vector3 (-3.2f, 4.5, 0.1f);

#endif


camAnimatedCamera::camAnimatedCamera(const camBaseObjectMetadata& metadata)
: camBaseCamera(metadata)
, m_Metadata(static_cast<const camAnimatedCameraMetadata&>(metadata))
, m_OverriddenDofSettings(NULL)
, m_OverriddenDofFocusDistance(0.0f)
, m_OverriddenDofFocusDistanceBlendLevel(0.0f)
, m_iFlags(0)
, m_SyncedScene(INVALID_SYNCED_SCENE_ID)
{
	m_AnimDictIndex = -1;

	m_OverriddenDofSettings = rage_new camDepthOfFieldSettingsMetadata;
	if(m_OverriddenDofSettings)
	{
		//Clone the DOF settings that were pulled from the camera metadata and then redirect to the unique settings for this camera, so that they may be overridden.
		if(m_DofSettings)
		{
			*m_OverriddenDofSettings = *m_DofSettings;
		}

		m_DofSettings = m_OverriddenDofSettings;
	}

	Init();

#if __BANK
	//moved out of Init() because we need to retain these values as init gets called for a new cam anim.
	m_OverriddenMat.Identity();
	m_CanOverrideCamMat = false;

	m_OverriddenNearDof = g_DefaultNearInFocusDofPlane;
	m_OverriddenFarDof = g_DefaultFarInFocusDofPlane;
	m_OverriddenDofStrength = g_DefaultDofStrength;
	m_CanOverrideDof = false;

	m_OverriddenFov = g_DefaultFov;
	m_CanOverrideFov = false;

	m_CanOverrideRawDofPlanes = false;
	m_CanOverrideShallowDof = false;
	m_CanOverrideMotionBlur = false;
	m_CanOverrideSimpleDof = false;
	m_OverrideSimpleDof = false;

	m_OverridenShallowDof = false;
	m_OverridenMotionBlur = 0.0f;

	m_CanOverrideDofStrength = false; 
	m_CanOverrideDofModifier = false; 
	m_DofPlaneOverriddenStrength = 0.0f; 
	m_OverriddenDofStrengthModifier = 0.0f; 

	m_CanOverrideFocusPoint = false; 
	m_OverridenFocusPoint = 0.0f; 
#endif

#if __BANK
	m_fNearClip = 0.1f;
	m_fFaClip = 5.0f;


#endif
}

camAnimatedCamera::~camAnimatedCamera()
{
	if(fwAnimDirectorComponentSyncedScene::IsValidSceneId(m_SyncedScene))
	{
#if __BANK
		fwAnimDirectorComponentSyncedScene::RemoveCameraFromSyncedScene(m_SyncedScene, (void*)this);
#endif //__BANK
		fwAnimDirectorComponentSyncedScene::DecSyncedSceneRef(m_SyncedScene);
	}

	if(m_AnimDictIndex != -1)
	{
		fwAnimManager::RemoveRefWithoutDelete(strLocalIndex(m_AnimDictIndex));
	}

	if(m_OverriddenDofSettings)
	{
		delete m_OverriddenDofSettings;
	}
}

void camAnimatedCamera::Init()
{
	if(fwAnimDirectorComponentSyncedScene::IsValidSceneId(m_SyncedScene))
	{
#if __BANK
		fwAnimDirectorComponentSyncedScene::RemoveCameraFromSyncedScene(m_SyncedScene, (void*)this);
#endif //__BANK
		fwAnimDirectorComponentSyncedScene::DecSyncedSceneRef(m_SyncedScene);
	}

	m_SceneMatrix.Identity();
	m_BasePosition				= VEC3_ZERO;
	m_CurrentTime				= 0.0f;
	m_Phase						= 0.0f;
	m_PhaseOnPreviousUpdate		= 0.0f;
	m_OverriddenFarClip			= 0.0f;
	m_OverriddenNearClip		= 0.0f;
	m_DofStrengthModifier		= 0.0f; 
	m_FocusPoint				= 0.0f; 
	m_shouldScriptOverrideFarClip = false;
	m_CorrectNextFrame			= -1.0f;
	m_PreviousKeyFramePhase		= 0.0f;
	m_AnimHash					= 0;
	m_hasCut					= false;
	m_ShouldUseLocalCameraTime	= false;
	m_iJumpCutStatus			= NO_JUMP_CUT;
	m_SyncedScene				= INVALID_SYNCED_SCENE_ID;
	m_bStartLoopNextFrame		= false;
	m_ShouldControlMiniMapHeading = false;
	m_UseDayCoC					 = true; 
	   
#if __BANK
	m_PhaseBeforeJumpCut		= 0.0f;
	m_DebugIsInsideJumpCut		= false;
	m_bIsUnapproved				= false;
	//Camera over ride mode values
	if (!b_AddedWidget)
	{
		b_AddedWidget = true;
	}
#endif
}

bool camAnimatedCamera::Update()
{
	if (cameraVerifyf(GetCutSceneAnimation().GetClip(),"camAnimatedCamera::Update() the clip is null, the camera will use the last valid anim frame"))
	{
		UpdateSceneMatrix();
		UpdatePhase();

		UpdateFrame(m_Phase);

#if !__NO_OUTPUT
		if(m_NumUpdatesPerformed == 0)
		{
			const Matrix34& worldMatrix = m_Frame.GetWorldMatrix();

			Vector3 orientationEulers;
			worldMatrix.ToEulersYXZ(orientationEulers);
			orientationEulers *= RtoD;

			Displayf("Animated camera intial frame: Position = (%f, %f, %f), Orientation Eulers (Deg) = (%f, %f, %f), FOV = %f, Far Clip = %f",
				worldMatrix.d.x, worldMatrix.d.y, worldMatrix.d.z, orientationEulers.x, orientationEulers.y, orientationEulers.z, m_Frame.GetFov(),
				m_Frame.GetFarClip());
		}
#endif // !__NO_OUTPUT

		UpdateOverriddenDrawDistance();

#if __BANK
		UpdateOverriddenCameraMatrix();

		UpdateOverriddenFov();
		UpdateOverriddenDof();
		UpdateOverriddenMotionBlur();
		UpdateOverridenDofs();
#endif

		return true;
	}

	return false;
}

void camAnimatedCamera::ComputeDofOverriddenFocusSettings(Vector3& focusPosition, float& blendLevel) const
{
	//NOTE: We cannot seamlessly blend from one custom focus position and blend level to another without an additional overall blend level, so we just apply any
	//override explicitly for simplicity. This does not allow us to support blending between the animated focus point and an overridden focus distance.

	const float focusDistance = (m_OverriddenDofFocusDistanceBlendLevel >= SMALL_FLOAT) ? m_OverriddenDofFocusDistance : m_FocusPoint;

	Vector3 localFront = YAXIS * focusDistance;
	m_PostEffectFrame.GetWorldMatrix().Transform(localFront, focusPosition); 

	blendLevel = m_OverriddenDofFocusDistanceBlendLevel;

	//if(m_FocusPoint > 0.0f)
	//{
	//	blendLevel = 1.0f; 
	//}
	//else
	//{
	//	blendLevel = 0.0f; 
	//}
}

void camAnimatedCamera::UpdateSceneMatrix()
{
	if (fwAnimDirectorComponentSyncedScene::IsValidSceneId(m_SyncedScene))
	{
		fwAnimDirectorComponentSyncedScene::GetSyncedSceneOrigin(m_SyncedScene, m_SceneMatrix);

		if (GetAnim())
		{
			fwAnimHelpers::ApplyInitialOffsetFromClip(*GetAnim(), m_SceneMatrix);
		}
	}
}

void camAnimatedCamera::UpdatePhase()
{
	m_PhaseOnPreviousUpdate = m_Phase;

	// if we've supplied a synchronized scene to follow, work out the
	// scene's root matrix and get the phase
	if (fwAnimDirectorComponentSyncedScene::IsValidSceneId(m_SyncedScene))
	{
		if (fwAnimDirectorComponentSyncedScene::IsSyncedSceneAbort(m_SyncedScene) ||
			(!fwAnimDirectorComponentSyncedScene::IsSyncedSceneLooped(m_SyncedScene) &&
			 !fwAnimDirectorComponentSyncedScene::IsSyncedSceneHoldLastFrame(m_SyncedScene) &&
			 m_Phase >= 1.0f))
		{
#if __BANK
			fwAnimDirectorComponentSyncedScene::RemoveCameraFromSyncedScene(m_SyncedScene, (void*)this);
#endif //__BANK
			fwAnimDirectorComponentSyncedScene::DecSyncedSceneRef(m_SyncedScene);

			m_SyncedScene = -1;
		}
		else
		{
			m_Phase = m_Animation.GetPhase(fwAnimDirectorComponentSyncedScene::GetSyncedSceneTime(m_SyncedScene));
		}
	}
	else
	{
		if(m_ShouldUseLocalCameraTime)
		{
			u32 currentTimeInMs	= fwTimer::GetCamTimeInMilliseconds();
			m_CurrentTime		= (float)currentTimeInMs * 0.001f;
		}

		m_Phase = m_Animation.GetPhase(m_CurrentTime);

		if(m_ShouldUseLocalCameraTime && IsFlagSet(APF_ISLOOPED) )
		{
			//There is a frame delay here so that the anim hits phase 1.0 so the script can check it gets to the final phase
			//Can rework to allow for the time step at the loop transition.
			if(m_bStartLoopNextFrame)
			{
				m_Animation.SetStartTime(m_CurrentTime);
				m_Phase = 0.0f;
				m_bStartLoopNextFrame = false;
			}

			if(m_Phase == 1.0f )
			{
				m_bStartLoopNextFrame = true;
			}
		}
	}
}

//Updates the camera position, rotation etc from a phase.
void camAnimatedCamera::UpdateFrame(float phase)
{
	Matrix34 mMat(M34_IDENTITY);

	//Create Camera matrix
	Vector3 vPos(VEC3_ZERO);
	Quaternion qRot(Quaternion::sm_I);

	qRot = ExtractRotation(phase, false);
	vPos = ExtractTranslation(phase, false);

	mMat.FromQuaternion(qRot);
	mMat.d = vPos;

	//Set the camera into scene space
	mMat.Dot(m_SceneMatrix);

	//Pass this matrix to camera frame
	m_Frame.SetWorldMatrix(mMat, false);

	//DOF:
	float nearDof;
	float farDof;
	float dofStrength;
	Vector4 DofPlanes(Vector4::ZeroType);
	bool UseAnimatedDofStrength = false; 

	if(ExtractDof(phase, DofPlanes) )
	{
#if __ASSERT
		if(m_Animation.GetClip())
		{

			if(!(DofPlanes.x >= 0.0f))
			{
				cameraWarningf("%s: Near out of focus plane %f must be greater than 0.0 - check that the dof values are correct", m_Animation.GetClip()->GetName(), DofPlanes.x);
			}

			if(!(DofPlanes.y >= DofPlanes.x))
			{
				cameraWarningf("%s: The near in focus plane %f must be greater the near out of focus plane %f", m_Animation.GetClip()->GetName(), DofPlanes.y, DofPlanes.x);
			}
			if(!(DofPlanes.z >= DofPlanes.y))
			{
				cameraWarningf( "%s: The far in focus plane %f must be greater the near in focus plane %f", m_Animation.GetClip()->GetName(), DofPlanes.z, DofPlanes.y);
			}

			if(!(DofPlanes.w >= DofPlanes.z))
			{
				cameraWarningf("%s: The far out of focus plane %f must be greater the far in focus plane %f", m_Animation.GetClip()->GetName(), DofPlanes.w, DofPlanes.z);
			}

		}
		else
		{
			if(!(DofPlanes.x >= 0.0f))
			{
				cameraWarningf("Near out of focus plane %f must be greater than 0.0 - check that the dof values are correct", DofPlanes.x);
			}

			if(!(DofPlanes.y >= DofPlanes.x))
			{
				cameraWarningf("The near in focus plane %f must be greater the near out of focus plane %f", DofPlanes.y, DofPlanes.x);
			}

			if(!(DofPlanes.z >= DofPlanes.y))
			{
				cameraWarningf( "The far in focus plane %f must be greater the near in focus plane %f", DofPlanes.z, DofPlanes.y);
			}

			if(!(DofPlanes.w >= DofPlanes.z))
			{
				cameraWarningf("The far out of focus plane %f must be greater the far in focus plane %f", DofPlanes.w, DofPlanes.z);
			}
		}
#endif
		m_Frame.SetDofPlanes(DofPlanes);
		
		ExtractFocusPoint(phase, m_FocusPoint); 
	
		// if we have a dof strength lets update four planes
		if(ExtractDiskBlurRadius(phase, dofStrength))
		{
			dofStrength += m_DofStrengthModifier;
			dofStrength = Floorf(dofStrength); 

			m_Frame.SetDofBlurDiskRadius(dofStrength); 

			UseAnimatedDofStrength = true; 
		}
	}
	else if(ExtractDof(phase, true, nearDof, farDof, dofStrength))
	{
#if __ASSERT
		if(m_Animation.GetClip())
		{
			cameraAssertf(nearDof >= 0.0f, "Near dof: %f must be greater than 0.0 - check that the dof values are correct in anim: %s", nearDof, m_Animation.GetClip()->GetName());
			cameraAssertf(farDof >0.0f && farDof > nearDof, "farDof: %f must be greater than 0.0f and greater than the near dof: %f (anim: %s) ", farDof, nearDof, m_Animation.GetClip()->GetName());
		}
		else
		{
			cameraAssertf(nearDof >= 0.0f, "Near dof: %f must be greater than 0.0 - check that the dof values are correct", nearDof);
			cameraAssertf(farDof >0.0f && farDof > nearDof, "farDof: %f must be greater than 0.0f and greater than the near dof: %f", farDof, nearDof);
		}
#endif
		//NOTE: Negative near DOF is actually valid.
		if(nearDof < 0.0f)
		{
			nearDof = 0.0f;
		}

		if(farDof <= nearDof)
		{
			farDof = nearDof + SMALL_FLOAT;
		}

		m_Frame.SetNearInFocusDofPlane(nearDof);
		m_Frame.SetFarInFocusDofPlane(farDof);
		m_Frame.SetDofStrength(dofStrength);
	}

	float fov = ExtractFov(phase, true);
#if __ASSERT
	if(m_Animation.GetClip())
	{
		cameraAssertf(fov >= g_MinFov && fov <= g_MaxFov, "fov: %f in the animation must be greater than %f and les than %f (anim:%s)", fov, g_MinFov,g_MaxFov, m_Animation.GetClip()->GetName() );
	}
	else
	{
		cameraAssertf(fov >= g_MinFov && fov <= g_MaxFov, "fov: %f in the animation must be greater than %f and les than %f", fov, g_MinFov,g_MaxFov);
	}
#endif
	fov = Clamp(fov, g_MinFov, g_MaxFov);

	m_Frame.SetFov(fov);


	bool useSimpleDof = ExtractSimpleDof(phase);

	if(useSimpleDof)
	{
		camInterface::GetCutsceneDirector().SetHighQualityDofThisUpdate(false);
		m_Frame.GetFlags().SetFlag(camFrame::Flag_ShouldUseSimpleDof);

	}
	else
	{
		if(UseAnimatedDofStrength)
		{
			m_Frame.GetFlags().SetFlag(camFrame::Flag_ShouldOverrideDofBlurDiskRadius);
			m_Frame.GetFlags().ChangeFlag(camFrame::Flag_ShouldUseLightDof, false);
		}
		else
		{
			m_Frame.GetFlags().ChangeFlag(camFrame::Flag_ShouldUseLightDof, ExtractLightDof(phase));
		}
		
		m_Frame.GetFlags().ClearFlag(camFrame::Flag_ShouldUseSimpleDof);
	}

	const crClip* clip = m_Animation.GetClip();

	if(clip)
	{
		const float time					= clip->ConvertPhaseToTime(phase);
		const float timeOnPreviousUpdate	= clip->ConvertPhaseToTime(m_PhaseOnPreviousUpdate);
		bool hasCut					= clip->CalcBlockPassed(timeOnPreviousUpdate, time);

		hasCut |= m_hasCut;
		if(hasCut)
		{
			m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation);
		}
	}

	m_hasCut = false;
	m_Frame.GetFlags().SetFlag(camFrame::Flag_RequestPortalRescan);

}

#if __BANK
void camAnimatedCamera::UpdateOverriddenCameraMatrix()
{
	if(m_CanOverrideCamMat)
	{
		m_Frame.SetWorldMatrix(m_OverriddenMat, false);
		m_CanOverrideCamMat = false;
	}
}

void camAnimatedCamera::UpdateOverriddenDof()
{
	if(m_CanOverrideFocusPoint)	
	{
		m_FocusPoint = m_OverridenFocusPoint; 
	}
	
	if(m_CanOverrideRawDofPlanes)
	{
		m_Frame.SetDofPlanes(m_OverridenRawDofPlanes);

		if(m_CanOverrideDofStrength)
		{
			m_Frame.GetFlags().SetFlag(camFrame::Flag_ShouldOverrideDofBlurDiskRadius);

			m_Frame.SetDofBlurDiskRadius(m_DofPlaneOverriddenStrength); 
		}
	}
	else if(m_CanOverrideDof)
	{
		m_Frame.SetNearInFocusDofPlane(m_OverriddenNearDof);
		m_Frame.SetFarInFocusDofPlane(m_OverriddenFarDof);
		m_Frame.SetDofStrength(m_OverriddenDofStrength);
		m_CanOverrideDof = false;
	}
	
	if(!m_CanOverrideDof && m_CanOverrideDofModifier)
	{
		if(m_Frame.GetFlags().IsFlagSet(camFrame::Flag_ShouldOverrideDofBlurDiskRadius))
		{
			float dofStrength = m_Frame.GetDofBlurDiskRadius() + m_OverriddenDofStrengthModifier;

			m_Frame.SetDofBlurDiskRadius(dofStrength); 
		}

	}

	m_CanOverrideDof = false;
	m_CanOverrideRawDofPlanes = false;
}


void camAnimatedCamera::UpdateOverriddenFov()
{
	if(m_CanOverrideFov)
	{
		m_OverriddenFov = Clamp(m_OverriddenFov, g_MinFov, g_MaxFov);

		m_Frame.SetFov(m_OverriddenFov);
		m_CanOverrideFov = false;
	}
}

void camAnimatedCamera::UpdateOverriddenMotionBlur()
{
	if(m_CanOverrideMotionBlur)
	{
		m_Frame.SetMotionBlurStrength(m_OverridenMotionBlur);
		m_CanOverrideMotionBlur = false;
	}
}

void camAnimatedCamera::UpdateOverridenDofs()
{
	if(m_CanOverrideShallowDof)
	{
		if(m_OverridenShallowDof)
		{
			m_Frame.GetFlags().ClearFlag(camFrame::Flag_ShouldUseSimpleDof);
			m_Frame.GetFlags().SetFlag(camFrame::Flag_ShouldUseLightDof);
		}
		else
		{
			m_Frame.GetFlags().ClearFlag(camFrame::Flag_ShouldUseSimpleDof);
			m_Frame.GetFlags().ClearFlag(camFrame::Flag_ShouldUseLightDof);
		}

		m_CanOverrideShallowDof = false;
	}

	if(m_CanOverrideSimpleDof)
	{
		if(m_OverrideSimpleDof)
		{
			camInterface::GetCutsceneDirector().SetHighQualityDofThisUpdate(false);
			m_Frame.GetFlags().ClearFlag(camFrame::Flag_ShouldUseLightDof);
			m_Frame.GetFlags().SetFlag(camFrame::Flag_ShouldUseSimpleDof);
		}
		m_CanOverrideSimpleDof = false;
	}
}
#endif

//Updates the draw distance this frame.
void camAnimatedCamera::UpdateOverriddenDrawDistance()
{
	int tempCamIndex = GetPoolIndex();
	if (camInterface::GetScriptDirector().IsCameraScriptControllable(tempCamIndex))
	{
		if(m_shouldScriptOverrideFarClip)
		{
			m_Frame.GetFlags().SetFlag(camFrame::Flag_ShouldIgnoreTimeCycleFarClip);
		}
		else
		{
			return;
		}
	}
	else
	{
		const float overriddenFarClipThisUpdate = camInterface::GetCutsceneDirector().GetOverriddenFarClipThisUpdate();

		//NOTE: This functionality is multiplayer-only, as the single player game is now locked.
		if(NetworkInterface::IsGameInProgress() && (overriddenFarClipThisUpdate >= SMALL_FLOAT))
		{
			m_Frame.GetFlags().SetFlag(camFrame::Flag_ShouldIgnoreTimeCycleFarClip);
			m_Frame.SetFarClip(overriddenFarClipThisUpdate);
		}
		else if(m_OverriddenFarClip == -1.0f )
		{
			m_Frame.GetFlags().ClearFlag(camFrame::Flag_ShouldIgnoreTimeCycleFarClip);
		}
		else
		{
			m_Frame.GetFlags().SetFlag(camFrame::Flag_ShouldIgnoreTimeCycleFarClip);

			if (!cameraVerifyf(m_OverriddenFarClip >=0 , "Far clip must be at least zero, setting the far clip to default value: %f", g_DefaultFarClip ))
			{
				m_OverriddenFarClip = g_DefaultFarClip;
			}

			float farClip = (m_OverriddenFarClip != 0.0f) ? m_OverriddenFarClip : g_DefaultFarClip;
			m_Frame.SetFarClip(farClip);
		}

		if(m_OverriddenNearClip == -1.0f)
		{
			m_Frame.SetNearClip(g_DefaultNearClip);
		}
		else
		{
			if (!cameraVerifyf(m_OverriddenNearClip >= 0 , "Near clip must be at least zero, setting the near clip to default value: %f", g_DefaultNearClip))
			{
				m_OverriddenNearClip = g_DefaultNearClip;
			}

			float nearClip = (m_OverriddenNearClip != 0.0f) ? m_OverriddenNearClip : g_DefaultNearClip;
			m_Frame.SetNearClip(nearClip);
		}
	}
}

//Returns the current animation frame being played by this camera.
s32 camAnimatedCamera::GetFrameCount() const
{
	s32 frameCount = -1;
	if (cameraVerifyf(GetCutSceneAnimation().GetClip(), "Animation on camera doesn't exist!"))
	{
		float numInternalFrames = GetCutSceneAnimation().GetClip()->GetNum30Frames();
		frameCount = (s32)rage::Floorf(m_Phase * numInternalFrames);
	}
	return frameCount;
}

//Returns the world position of this camera at the specified animation phase.
bool camAnimatedCamera::GetPosition(float phase, Vector3& vCamPos) const
{
	if(cameraVerifyf(GetCutSceneAnimation().GetClip(), "Camera animation is not set up yet!"))
	{
		Vector3 position(m_BasePosition);
		vCamPos = position + ExtractTranslation(phase, true);
		return true;
	}

	return false;
}

const Matrix34& camAnimatedCamera::GetMatrix()
{
	return GetFrameNonConst().GetWorldMatrix();
}

//Sets the animation that will drive this camera.
bool camAnimatedCamera::SetCameraClip(const Vector3& basePosition, const strStreamingObjectName animDictName, const crClip* pClip, float fStartTimer, u32 iFlags )
{
	if(m_AnimDictIndex != -1)
	{
		//Ensure that we clean up our previous animation (and referencing) before replacing it.
		m_Animation.Remove();

		fwAnimManager::RemoveRefWithoutDelete(strLocalIndex(m_AnimDictIndex));
		m_AnimDictIndex = -1;
	}

	strLocalIndex animDictIndex = fwAnimManager::FindSlot(animDictName);

	if(cameraVerifyf((animDictIndex != -1 && pClip), "Failed to find a slot for anim dictionary %s", SAFE_CSTRING(animDictName.TryGetCStr())))
	{
		m_AnimDictIndex = animDictIndex.Get();
		m_iFlags = iFlags;
		fwAnimManager::AddRef(strLocalIndex(m_AnimDictIndex));

		m_Frame.SetPosition(basePosition);
		m_BasePosition = basePosition;
		m_Animation.Init(pClip, fStartTimer);
#if __BANK
		m_bIsUnapproved = IsAnimatedCameraUnapproved( pClip, animDictName.GetCStr());
#endif
		return true;
	}
	else
	{
		return false;
	}
}

bool camAnimatedCamera::IsPlayingAnimation(const strStreamingObjectName animDictName, const crClip* pClip) const
{
	strLocalIndex animDictIndex	= fwAnimManager::FindSlot(animDictName);
	bool isPlaying		= (animDictIndex.Get() == m_AnimDictIndex) && (pClip == m_Animation.GetClip());

	return isPlaying;
}

void camAnimatedCamera::SetPhase(float phase)
{
	//Find the time at which this phase would be achieved and use this to modify the animation start time according.
	float requestedCurrentTime	= m_Animation.GetTime(phase);
	float timeDelta				= m_CurrentTime - requestedCurrentTime;

	float startTime				= m_Animation.GetStartTime();
	startTime					+= timeDelta;

	m_Animation.SetStartTime(startTime);
}

void camAnimatedCamera::SetSyncedScene(fwSyncedSceneId scene)
{
	if (fwAnimDirectorComponentSyncedScene::IsValidSceneId(m_SyncedScene))
	{
#if __BANK
		fwAnimDirectorComponentSyncedScene::RemoveCameraFromSyncedScene(scene, (void*)this);
#endif //__BANK
		fwAnimDirectorComponentSyncedScene::DecSyncedSceneRef(m_SyncedScene);
	}

	if (fwAnimDirectorComponentSyncedScene::IsValidSceneId(scene))
	{
		m_SyncedScene = scene;
		fwAnimDirectorComponentSyncedScene::AddSyncedSceneRef(scene);
#if __BANK
		fwAnimDirectorComponentSyncedScene::RegisterCameraWithSyncedScene(scene, (void*)this, GetAnim());
#endif //__BANK

		if (GetAnim() != NULL)
		{
			if (GetAnim()->GetDuration() > fwAnimDirectorComponentSyncedScene::GetSyncedSceneDuration(scene))
			{
				fwAnimDirectorComponentSyncedScene::SetSyncedSceneDuration(scene, GetAnim()->GetDuration());
			}
		}
	}

}

//Extracts the translation value at the specified phase in the camera track.
Vector3 camAnimatedCamera::ExtractTranslation(const float phase, bool UNUSED_PARAM(isJumpCut)) const
{
	Vector3 vVec(VEC3_ZERO);
	m_Animation.GetVector3Value(m_Animation.GetClip(), kTrackCameraTranslation, phase, vVec);
	return vVec;
}

//Extracts the rotation value at the specified phase in the camera track.
Quaternion camAnimatedCamera::ExtractRotation(const float phase, bool UNUSED_PARAM(isJumpCut)) const
{
	Quaternion QVal (Quaternion::sm_I);
	m_Animation.GetQuaternionValue(m_Animation.GetClip(), kTrackCameraRotation, phase, QVal);
	return QVal;
}

//Extracts the near and far DOF values at the specified phase in the camera track.
bool camAnimatedCamera::ExtractDof(float phase, bool UNUSED_PARAM(isJumpCut), float& nearDof, float& farDof, float& dofStrength) const
{
	Vector3 vDof(VEC3_ZERO);
	if (!m_Animation.GetVector3Value(m_Animation.GetClip(), kTrackCameraDepthOfField, phase, vDof))
	{
		return false;
	}

	nearDof = vDof.x;
	farDof = vDof.y;

	if (!m_Animation.GetFloatValue(m_Animation.GetClip(), kTrackCameraDepthOfFieldStrength, phase, dofStrength))
	{
		return false;
	}

	return true;
}

bool camAnimatedCamera::ExtractDof(const float phase, Vector4 &dofPlanes)
{
	float fNearOutOfFocusPlane=-1.0f;
    float fNearInFocusPlane=-1.0f;
	float fFarOutOfFocusPlane=-1.0f;
	float fFarInFocusPlane=-1.0f;

	if ( !m_Animation.GetFloatValue(m_Animation.GetClip(), kTrackCameraDepthOfFieldNearOutOfFocusPlane, phase, fNearOutOfFocusPlane)
	  || !m_Animation.GetFloatValue(m_Animation.GetClip(), kTrackCameraDepthOfFieldNearInFocusPlane, phase, fNearInFocusPlane)
	  || !m_Animation.GetFloatValue(m_Animation.GetClip(), kTrackCameraDepthOfFieldFarOutOfFocusPlane, phase, fFarOutOfFocusPlane)
	  || !m_Animation.GetFloatValue(m_Animation.GetClip(), kTrackCameraDepthOfFieldFarInFocusPlane, phase, fFarInFocusPlane) )
	{
		return false;
	}

	dofPlanes.Set(fNearOutOfFocusPlane, fNearInFocusPlane, fFarInFocusPlane, fFarOutOfFocusPlane);

	return true;
}

//Extracts the FOV value at the specified phase in the camera track.
float camAnimatedCamera::ExtractFov(float phase, bool UNUSED_PARAM(isJumpCut)) const
{
	float fFloatVal = 0.0f;
	m_Animation.GetFloatValue(m_Animation.GetClip(), kTrackCameraFieldOfView, phase, fFloatVal);
	return fFloatVal;
}

bool camAnimatedCamera::ExtractLightDof(float phase) const
{
	float fFloatVal = 0.0f;
	m_Animation.GetFloatValue(m_Animation.GetClip(), kTrackCameraShallowDepthOfField, phase, fFloatVal);

	if(fFloatVal > 0.5f)
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool camAnimatedCamera::ExtractSimpleDof(float phase) const
{
	float fFloatVal = 0.0f;
	m_Animation.GetFloatValue(m_Animation.GetClip(), kTrackCameraSimpleDepthOfField, phase, fFloatVal);

	if(fFloatVal > 0.5f)
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool  camAnimatedCamera::ExtractDiskBlurRadius(float phase, float &dofStrength) const
{
	u32 Track = kTrackCameraCoC; 

	TUNE_GROUP_BOOL(CUTSCENE, UseNightTrack ,true);
	if(UseNightTrack && !m_UseDayCoC)
	{
		Track = kTrackCameraNightCoC; 
	}

	float fFloatVal = 0.0f; 
	if(m_Animation.GetFloatValue(m_Animation.GetClip(), Track, phase, fFloatVal))
	{
		dofStrength = fFloatVal; 
		return true; 
	}
	return false; 
}

bool  camAnimatedCamera::ExtractFocusPoint(const float phase,  float &focusPoint) const
{
	float fvalue = 0.0f; 
	if(m_Animation.GetFloatValue(m_Animation.GetClip(), kTrackCameraFocus, phase, fvalue))
	{
		//need to convert the focus point to meters exported as cm
		focusPoint = fvalue / 100.0f; 
		return true; 
	}
	return false; 
}

#if __BANK
void camAnimatedCamera::DebugSpewAnimationFrames()
{
	const crClip* pClip = m_Animation.GetClip();
	if(pClip)
	{
		const float fNumberOfFrames = pClip->GetNum30Frames();
		float f;
		for(f = 0.0f; f < fNumberOfFrames; f += 1.0f)
		{
			// Get the current phase
			float fCurrentPhase = pClip->Convert30FrameToPhase(f);

			Vector3 translation = ExtractTranslation(fCurrentPhase, false);
			Displayf("Frame: %f - Phase: %f - Translation: %f, %f, %f", f, fCurrentPhase, translation.x, translation.y, translation.z);

			Quaternion rotation = ExtractRotation(fCurrentPhase, false);
			Vector3 rotationEulers;
			rotation.ToEulers(rotationEulers, "yxz");
			Displayf("Frame: %f - Phase: %f - Rotation: %f, %f, %f", f, fCurrentPhase, rotationEulers.x, rotationEulers.y, rotationEulers.z);
		}
	}
}

void camAnimatedCamera::DebugRenderInfoTextInWorld(const Vector3& vPos)
{
	grcDebugDraw::Text(vPos, CRGBA(0,0,0,255),0, grcDebugDraw::GetScreenSpaceTextHeight(), SAFE_CSTRING(m_Metadata.m_Name.GetCStr()) );

	if(m_Animation.GetClip())
	{
		const crClip* pClip = m_Animation.GetClip();
		bool addedContent = false;
		bool preview = false;

		atString DebugText = CAnimClipDebugIterator::GetClipDebugInfo(pClip, pClip->ConvertPhaseToTime(GetPhase()), 1.0f, addedContent, preview);

		Color32 colour = addedContent ? CRGBA(180,192,96,255) : CRGBA(255,192,96,255);
		grcDebugDraw::Text( vPos, colour, 0, 2*grcDebugDraw::GetScreenSpaceTextHeight(), DebugText);
	}
}

bool camAnimatedCamera::IsAnimatedCameraUnapproved(const crClip* pClip, const char* Dict )
{
	if(pClip)
	{
		atString clipName(pClip->GetName());
		clipName.Replace("pack:/", "");
		clipName.Replace(".clip", "");

		if(Dict)
		{
			atVarString animAndDictName("%s%s",Dict, clipName.c_str());

			atHashString CameraAnimName(animAndDictName);

			for(int i = 0; i < camManager::GetUnapprovedCameraList().m_UnapprovedAnimatedCameras.GetCount(); i ++)
			{
				u32 Unapprovedhash  = camManager::GetUnapprovedCameraList().m_UnapprovedAnimatedCameras[i].GetHash();
				if( Unapprovedhash == CameraAnimName.GetHash())
				{
					return true;
				}
			}
		}
	}

	return false;
}
//
//const char* camAnimatedCamera::GetDictionaryName(const crClip* pClip)
//{
//	const char *szClipDictionaryName = NULL;
//	for(int clipDictionaryIndex = 0; clipDictionaryIndex < g_ClipDictionaryStore.GetSize(); clipDictionaryIndex ++)
//	{
//		if(g_ClipDictionaryStore.IsValidSlot(clipDictionaryIndex))
//		{
//			crClipDictionary *pClipDictionary = g_ClipDictionaryStore.Get(clipDictionaryIndex);
//			if(pClipDictionary == pClip->GetDictionary())
//			{
//				szClipDictionaryName = g_ClipDictionaryStore.GetName(clipDictionaryIndex);
//				break;
//			}
//		}
//	}
//	return szClipDictionaryName;
//}

#endif // __BANK

