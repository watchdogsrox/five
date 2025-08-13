//
// camera/cutscene/CutsceneDirector.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CAM_CUTSCENE_DIRECTOR_H
#define CAM_CUTSCENE_DIRECTOR_H

#include "camera/base/BaseDirector.h"

#include "crclip/clip.h"

#include "camera/cutscene/AnimatedCamera.h"

#include "atl/vector.h"
class camCutsceneDirectorMetadata;

class camCutsceneDirector : public camBaseDirector
{
	DECLARE_RTTI_DERIVED_CLASS(camCutsceneDirector, camBaseDirector);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

private:
	camCutsceneDirector(const camBaseObjectMetadata& metadata);

public:
	~camCutsceneDirector();
	
	//Create a cut scene camera
	void	CreateCamera (s32 CameraId);
	
	//Destroy a camera 
	void	DestroyCamera (s32 CameraId);

	//Destroy all animated cameras	
	void	DestroyAllCameras();
	
	//Play an anim on a camera
	bool	PlayCameraAnim(const Vector3 &vCutsceneOffset, const strStreamingObjectName cAnimDict, const crClip* pClip, s32 CameraId, float fStartTime);
	
	//Get the phase of the camera anim
	float	GetCurrentCameraPhaseInUse() const; 
	
	void	SetDrawDistance(const float farClip, const float nearClip);
	void	SetUseDayCoCTrack(bool UseDayCoC); 

	void	OverrideFarClipThisUpdate(float farClip)	{ m_OverriddenFarClipThisUpdate = farClip; }
	float	GetOverriddenFarClipThisUpdate() const		{ return m_OverriddenFarClipThisUpdate; }

	//Set the jump cut status so the the cut scene manager can tell the camera a jump is going to occur
	void	SetJumpCutStatus(camAnimatedCamera::eJumpCutStatus iJumpCutStatus); 
	
	//Check if the camera is playing an animation
	bool	IsCutScenePlaying(); 

	bool GetCameraPosition(Vector3& vPos) const; 
	
	bool GetCameraWorldMatrix(Matrix34& Mat); 

	camAnimatedCamera::eJumpCutStatus GetJumpCutStatus(); 
		
	//bool WillCamerCutThisFrame(float time); 

	bool IsCameraWithIdActive(s32 CamId) const; 

	//Get the phase based on current anim time
	float GetPhase(float fTime) const ; 

	bool IsPerformingFirstPersonBlendOut() const { return m_IsPerformingFirstPersonBlendOut; }
	void UpdateGameplayCameraForFirstPersonBlendOut() const;
	
#if __BANK	
	void OverrideCameraDofThisFrame(float nearDof, float farDof, float dofStrength); 

	void OverrideCameraDofThisFrame(const Vector4 &dofPlanes); 

	void OverrideCameraMatrixThisFrame(const Matrix34 &mMat); 

	void OverrideCameraFovThisFrame(float fov); 

	void OverrideLightDofThisFrame(bool state); 

	void OverrideMotionBlurThisFrame(float motionBlur); 

	void OverrideSimpleDofThisFrame(bool state); 

	void OverrideDofPlaneStrength(float strength); 

	void OverrideDofPlaneStrengthModifier(float strengthModifier); 

	void OverrideCameraFocusPointThisFrame(float strengthModifier); 
#endif

	bool GetCamFrame(camFrame& Frame); 

	void InterpolateOutToEndOfScene(u32 iterpTime, bool toFirstPerson = false);

#if __BANK 
	virtual void AddWidgetsForInstance();

	void	DisplayCutSceneCameraDebug(); 
#endif

	camBaseCamera* GetActiveCutSceneCamera() const; 

private:
	//structure for containing our cam ids and pointers to cams
	struct sCutSceneCams 
	{
		s32 m_CameraId;
		RegdCamBaseCamera m_regdCam;
	};

	virtual bool Update();

	virtual u32 GetTimeInMilliseconds() const;
	
	virtual void UpdateActiveRenderState();

	const camCutsceneDirectorMetadata& m_Metadata;

	atVector<sCutSceneCams> m_sCutSceneCams;
	s32 m_CurrentPlayingCamera;
	float m_OverriddenFarClipThisUpdate;

	bool m_IsPerformingFirstPersonBlendOut;

#if __BANK
	float m_DebugOverriddenFarClipThisUpdate;
	bool m_DebugShouldBypassRendering;
	bool m_DebugShouldOverrideFarClipThisUpdate;
#endif // __BANK

	//Forbid copy construction and assignment.
	camCutsceneDirector(const camCutsceneDirector& other);
	camCutsceneDirector& operator=(const camCutsceneDirector& other);
};

#endif // CAM_CUTSCENE_DIRECTOR_H
