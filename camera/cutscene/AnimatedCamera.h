//
// camera/cutscene/AnimatedCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef ANIMATED_CAMERA_H
#define ANIMATED_CAMERA_H

#include "fwanimation/animdirector.h"

#include "camera/base/BaseCamera.h"
#include "cutscene/CutSceneAnimation.h"

namespace rage
{
	class crClip;
}

class camAnimatedCameraMetadata;

//A camera that is controlled by an animated cutscene.
class camAnimatedCamera : public camBaseCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camAnimatedCamera, camBaseCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	enum eJumpCutStatus
	{
		NO_JUMP_CUT = 0,
		PRE_JUMP_CUT,
		JUMP_CUT_NEXT_FRAME, 
		JUMP_CUT,
		POST_JUMP_CUT

	};

protected:
	camAnimatedCamera(const camBaseObjectMetadata& metadata);

public:
	~camAnimatedCamera();

	//Setup the class variables
	void			Init();
	
	//Apply the update to the frame based on the anim phase
	void 			UpdateFrame(float phase);
	
	//Returns the current animation frame being played by this camera.
	s32				GetFrameCount() const;
	
	bool 			GetPosition(float phase, Vector3& vpos) const;

	const Matrix34&	GetMatrix(); 
	
	void			SetPhase(float phase);
	float			GetPhase(float fCurrentTime) const {return m_Animation.GetPhase(fCurrentTime); }
	float			GetPhase() const	{ return m_Phase; }

	float			GetOverriddenNearClip() const { return m_OverriddenNearClip; }
	float			GetOverriddenFarClip() const { return m_OverriddenFarClip; }

	bool			HasAnimation() const				{ return (GetCutSceneAnimation().GetClip() != NULL); }
	const crClip*	GetAnim()							{ return GetCutSceneAnimation().GetClip(); }
	
	bool			SetCameraClip(const Vector3& basePosition, const strStreamingObjectName animDictName, const crClip* pClip, float fStartTime, u32 iFlags = 0);

	bool			IsPlayingAnimation(const strStreamingObjectName animDictName, const crClip* pClip) const;

	void			SetDofStrengthModifier(float Dof) { m_DofStrengthModifier = Dof; }

	const CCutsceneAnimation& GetCutSceneAnimation() const { return m_Animation; }

	eJumpCutStatus	GetJumpCutStatus() { return m_iJumpCutStatus; }
	void			SetJumpCutStatus(eJumpCutStatus iJumpcut) { m_iJumpCutStatus = iJumpcut; } 
	void			CalculateJumpCut(); 

	void			SetShouldUseLocalCameraTime(bool state)		{ m_ShouldUseLocalCameraTime = state; }
	bool			ShouldUseLocalCameraTime() const			{ return m_ShouldUseLocalCameraTime; }
	void			SetCurrentTime(float currentTime)			{ m_CurrentTime = currentTime; }
	float			GetCurrentTime() const						{ return m_CurrentTime; }

	void			SetSceneMatrix(const Matrix34& sceneMatrix)	{ m_SceneMatrix = sceneMatrix; }
	const Matrix34& GetSceneMatrix() const						{ return m_SceneMatrix; }

	void			SetSyncedScene(fwSyncedSceneId scene);

	void			SetHasCut(bool hasCut)						{ m_hasCut = hasCut; }

	bool			IsFlagSet(const int iFlag) const {return ((m_iFlags & iFlag) ? true : false);}
	
	void			SetScriptShouldOverrideFarClip(bool state) { m_shouldScriptOverrideFarClip = state; }

	void			OverrideCameraDrawDistance(const float farClip, const float nearClip) { m_OverriddenFarClip = farClip; m_OverriddenNearClip = nearClip; }

	bool			ShouldControlMiniMapHeading() const			{ return m_ShouldControlMiniMapHeading; }
	void			SetShouldControlMiniMapHeading(bool state)	{ m_ShouldControlMiniMapHeading = state; }
	bool 			ExtractFocusPoint	(const float phase, float& focusPoint) const; 

	Vector3			ExtractTranslation	(const float phase, const bool isJumpCut) const;
	Quaternion		ExtractRotation		(const float phase, const bool isJumpCut) const;

	void			SetShouldUseDayCoc(bool DayCoC) { m_UseDayCoC = DayCoC;  } 
	bool			ShouldUseDayCoC()	{ return m_UseDayCoC; }

	float			GetFocusPoint() const { return m_FocusPoint; }

	const camDepthOfFieldSettingsMetadata* GetOverriddenDofSettings() const	{ return m_OverriddenDofSettings; }
	camDepthOfFieldSettingsMetadata* GetOverriddenDofSettings()				{ return m_OverriddenDofSettings; }

	void			SetOverriddenDofFocusDistance(float distance)				{ m_OverriddenDofFocusDistance = distance; }
	void			SetOverriddenDofFocusDistanceBlendLevel(float blendLevel)	{ m_OverriddenDofFocusDistanceBlendLevel = blendLevel; }

#if __BANK
	void			OverrideCameraMatrix(const Matrix34 &mMat) { m_OverriddenMat = mMat; m_CanOverrideCamMat = true; }  
	void			OverrideCameraDof(float nearDof, float farDof, float strength) { m_OverriddenNearDof = nearDof; m_OverriddenFarDof = farDof; m_OverriddenDofStrength = strength; m_CanOverrideDof = true; }
	void			OverrideCameraDof(const Vector4& DofPlanes) { m_OverridenRawDofPlanes = DofPlanes; m_CanOverrideRawDofPlanes = true; }
	void			OverrideCameraFov(float Fov) { m_OverriddenFov = Fov; m_CanOverrideFov = true; } 
	void			OverrideCameraMotionBlur(float MotionBlur) { m_OverridenMotionBlur = MotionBlur; m_CanOverrideMotionBlur = true; }
	void			OverrideCameraCanUseLightDof(bool shallowDof) { m_OverridenShallowDof = shallowDof; m_CanOverrideShallowDof = true; }
	void			OverrideCameraCanUseSimpleDof(bool simpleDof) { m_OverrideSimpleDof = simpleDof; m_CanOverrideSimpleDof = true; }
	void			OverrideCameraDofStrength(float DofStrength) { m_DofPlaneOverriddenStrength = DofStrength; m_CanOverrideDofStrength = true; }
	void			OverrideCameraCoCModifier(float DofStrengthModifier) { m_OverriddenDofStrengthModifier = DofStrengthModifier; m_CanOverrideDofModifier = true;} 
	void			OverrideCameraFocusPoint(float FocusPoint) { m_OverridenFocusPoint = FocusPoint; m_CanOverrideFocusPoint = true; }

	void DebugSpewAnimationFrames();
	
	bool IsAnimatedCameraAnimUnapproved() const  { return m_bIsUnapproved; }

	virtual void	DebugRenderInfoTextInWorld(const Vector3& vPos); 

	static Vector3 vCamTempOffset; 
	static Vector3 vCamTempTempAxis; 
	static Vector3 fCamTempTempHeading; 



#endif // __BANK

protected:
	virtual bool	Update();
	virtual void	ComputeDofOverriddenFocusSettings(Vector3& focusPosition, float& blendLevel) const;

private:

	void			UpdateOverriddenDrawDistance();

#if __BANK
	void			UpdateOverriddenCameraMatrix(); 
	void			UpdateOverriddenDof(); 
	void			UpdateOverriddenFov(); 
	void			UpdateOverriddenMotionBlur();
	void			UpdateOverridenDofs(); 
#endif

	void			UpdatePortalTracker(const bool shouldForceInteriorUpdate);
	void			UpdateSceneMatrix(); 
	void			UpdatePhase(); 

	bool			ExtractDof			(const float phase, const bool isJumpCut, float& nearDof, float& farDof, float& dofStrength) const;
	bool			ExtractDof			(const float phase, Vector4 &dofPlanes); 
	float			ExtractFov			(const float phase, const bool isJumpCut) const;
	bool			ExtractLightDof		(const float phase) const; 
	bool			ExtractSimpleDof	(const float phase) const; 
	bool 			ExtractDiskBlurRadius	(const float phase, float &dofOverrideStrength) const; 
	

	const camAnimatedCameraMetadata& m_Metadata;

	Matrix34 m_SceneMatrix;
	Vector3 m_BasePosition;

	//A cut scene animation class
	CCutsceneAnimation m_Animation;

	camDepthOfFieldSettingsMetadata* m_OverriddenDofSettings;

	//Status of a jump cut
	eJumpCutStatus m_iJumpCutStatus;  


#if __BANK
	//const char* GetDictionaryName(const crClip* pClip); 
	bool IsAnimatedCameraUnapproved(const crClip* pClip, const char* Dict ); 
#endif
	float	m_FocusPoint; 

	float	m_CurrentTime;
	float	m_Phase;
	float	m_PhaseOnPreviousUpdate;
	float	m_CorrectNextFrame;
	float	m_PreviousKeyFramePhase;
	float	m_OverriddenNearClip;
	float	m_OverriddenFarClip;
	float	m_DofStrengthModifier; 

	s32		m_AnimDictIndex;
	u32		m_AnimHash;
	bool	m_hasCut;
	bool	m_ShouldUseLocalCameraTime;
	bool	m_shouldScriptOverrideFarClip;
	bool	m_UseDayCoC; 

	float	m_OverriddenDofFocusDistance;
	float	m_OverriddenDofFocusDistanceBlendLevel;

#if __BANK
	bool	m_CanOverrideDof; 
	float	m_OverriddenNearDof; 
	float	m_OverriddenFarDof; 
	float	m_OverriddenDofStrength; 
	
	Vector4 m_OverridenRawDofPlanes; 
	bool	m_CanOverrideRawDofPlanes; 

	bool	m_CanOverrideFov; 
	float   m_OverriddenFov; 

	bool	m_CanOverrideMotionBlur;
	float	m_OverridenMotionBlur;

	bool	m_OverridenShallowDof; 
	bool	m_CanOverrideShallowDof;

	bool	m_OverrideSimpleDof; 
	bool	m_CanOverrideSimpleDof; 

	bool	m_CanOverrideCamMat; 
	Matrix34 m_OverriddenMat; 

	bool m_CanOverrideDofStrength; 
	bool m_CanOverrideDofModifier; 

	float m_DofPlaneOverriddenStrength; 
	float m_OverriddenDofStrengthModifier; 

	bool m_CanOverrideFocusPoint; 
	float m_OverridenFocusPoint; 
#endif

	fwSyncedSceneId m_SyncedScene;

	s32		m_iFlags; 
	bool	m_bStartLoopNextFrame; 
	bool	m_ShouldControlMiniMapHeading;

#if __BANK
	float	m_PhaseBeforeJumpCut;
	bool	m_DebugIsInsideJumpCut;
	float	m_fNearClip;
	float	m_fFaClip; 
	float	m_fZoomDistance; 
	bool	m_bIsUnapproved; 
#endif // __BANK

private:
	//Forbid copy construction and assignment.
	camAnimatedCamera(const camAnimatedCamera& other);
	camAnimatedCamera& operator=(const camAnimatedCamera& other);
};

#endif // ANIMATED_CAMERA_H
