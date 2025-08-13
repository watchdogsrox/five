//
// camera/base/BaseCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef BASE_CAMERA_H
#define BASE_CAMERA_H

#include "fwtl/pool.h"

#include "camera/base/BaseObject.h"
#include "camera/helpers/Frame.h"

class camBaseCameraMetadata;
class camBaseFrameShaker;
class camBaseShakeMetadata;
class camCollision;
class camControlHelper;
class camDepthOfFieldSettingsMetadata;
class camFrameInterpolator;
class CEntity;

//The base camera class that describes the basic interface.
class camBaseCamera : public camBaseObject
{
	DECLARE_RTTI_DERIVED_CLASS(camBaseCamera, camBaseObject);

public:
	FW_REGISTER_CLASS_POOL(camBaseCamera);

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	camBaseCamera(const camBaseObjectMetadata& metadata);

public:
	virtual ~camBaseCamera();

	//NOTE: We return the post-effect frame if this camera was updated within the last frame. Otherwise we return the base frame, with no post-effects, as
	//this may have been altered externally with changes that have not yet propagated to the post-effect frame.
	const camFrame&	GetFrame() const 				{ return (m_WasUpdated ? m_PostEffectFrame : m_Frame); }
	const camFrame&	GetFrameNoPostEffects() const	{ return m_Frame; }
	camFrame&		GetFrameNonConst()				{ return m_Frame; }

	virtual const CEntity* GetAttachParent() const	{ return m_AttachParent; }
	virtual const CEntity* GetLookAtTarget() const	{ return m_LookAtTarget; }

	camCollision*	GetCollision() const			{ return m_Collision; }

	virtual const Vector3& GetBaseFront() const		{ return m_Frame.GetFront(); }

	bool			WasUpdated() const				{ return m_WasUpdated; }
	u32				GetNumUpdatesPerformed() const	{ return m_NumUpdatesPerformed; }

	static camFrame& GetScratchFrame()				{ return ms_ScratchFrame; }

	virtual void	SetFrame(const camFrame& frame)	{ m_Frame.CloneFrom(frame); }
	virtual void	CloneFrom(const camBaseCamera* const sourceCam);

	virtual void	AttachTo(const CEntity* parent)	{ m_AttachParent = parent; }
	virtual void	LookAt(const CEntity* target)	{ m_LookAtTarget = target; }

	bool			ShouldPreventNextCameraCloningOrientation() const	{ return m_ShouldPreventNextCameraCloningOrientation; }
	bool			ShouldPreventInterpolationToNextCamera() const		{ return m_ShouldPreventInterpolationToNextCamera; }
	virtual bool	ShouldPreventAnyInterpolation() const				{ return false; }
	void			PreventNextCameraCloningOrientation()				{ m_ShouldPreventNextCameraCloningOrientation = true; }
	void			PreventInterpolationToNextCamera()					{ m_ShouldPreventInterpolationToNextCamera = true; }

	virtual void	InterpolateFromCamera(camBaseCamera& sourceCamera, u32 duration, bool shouldDeleteSourceCamera = false);
	void			InterpolateFromFrame(const camFrame& sourceFrame, u32 duration);
	bool			IsInterpolating(const camBaseCamera* sourceCamera = NULL) const;
	const camFrameInterpolator* GetFrameInterpolator() const	{ return m_FrameInterpolator; }
	camFrameInterpolator* GetFrameInterpolator()				{ return m_FrameInterpolator; }
	void			StopInterpolating();

	camBaseFrameShaker*	Shake(const char* name, float amplitude = 1.0f, u32 maxInstances = 1, u32 minTimeBetweenInstances = 0);
	camBaseFrameShaker*	Shake(u32 nameHash, float amplitude = 1.0f, u32 maxInstances = 1, u32 minTimeBetweenInstances = 0);
	camBaseFrameShaker*	Shake(const camBaseShakeMetadata& metadata, float amplitude = 1.0f, u32 maxInstances = 1, u32 minTimeBetweenInstances = 0);
	camBaseFrameShaker*	Shake(const char* animDictionary, const char* animClip, const char* metadataName, float amplitude = 1.0f);
	bool			IsShaking() const				{ return (m_FrameShakers.GetCount() > 0); }
	camBaseFrameShaker*	FindFrameShaker(u32 nameHash);
	bool			SetShakeAmplitude(float amplitude, int shakeIndex = 0);
	void			StopShaking(bool shouldStopImmediately = true);
	void			BypassFrameShakersThisUpdate();

	void			AllowMotionBlurDecay(bool enable)	{ m_AllowMotionBlurDecay = enable; }

	virtual void	PreUpdate();

	bool			BaseUpdate(camFrame& destinationFrame);
	virtual void	UpdateFrameShakers(camFrame& frameToShake, float amplitudeScalingFactor = 1.0f);
    virtual float   GetFrameShakerScaleFactor() const { return 1.0f; }

	//move to a cinematic base camera.
	virtual u32		GetNumOfModes() const { return 0; }
	virtual bool	IsValid() { return true; }

	s32						GetPoolIndex() const			{ return camBaseCamera::GetPool()->GetIndex(this); }
	static camBaseCamera*	GetCameraAtPoolIndex(s32 index) { return (index >= 0) ? camBaseCamera::GetPool()->GetAt(index) : NULL; }

	// PURPOSE:		Return the matrix of this cameras transform relative to the target entity
	// RETURNS:		True if the target entity is valid and the camera matrix was provided.
	virtual bool	GetObjectSpaceCameraMatrix(const CEntity* pTargetEntity, Matrix34& mat) const;

#if __BANK
	static void		AddWidgets(bkBank &bank);
	static void		DebugRender2dTextBox(s32 minTextX, s32 minTextY, s32 maxTextX, s32 maxTextY, Color32 colour);

	virtual void	DebugRender();
	const char*		DebugGetNameText() const;
	virtual void	DebugRenderInfoTextInWorld(const Vector3& vPos); 

	virtual void	DebugAppendDetailedText(char* string, u32 maxLength) const;
#endif // __BANK

protected:
	virtual bool	Update() { return true; }
	virtual void	UpdateDof();
	virtual void	ComputeDofOverriddenFocusSettings(Vector3& focusPosition, float& blendLevel) const;
	virtual const CEntity* ComputeDofOverriddenFocusEntity() const;
	virtual void	ComputeDofSettings(float overriddenFocusDistance, float overriddenFocusDistanceBlendLevel, camDepthOfFieldSettingsMetadata& settings) const;
	void			BlendDofSettings(float blendLevel, camDepthOfFieldSettingsMetadata& sourceSettings, camDepthOfFieldSettingsMetadata& destinationSettings,
						camDepthOfFieldSettingsMetadata& blendedSettings) const;
	void			ApplyCustomFocusBehavioursToDofSettings(camDepthOfFieldSettingsMetadata& settings) const;
	virtual void	PostUpdate() {}					//NOTE: PostUpdate is called after the camera update and the post-effect camera update.

	u32				m_NumUpdatesPerformed;			//The total number of times this camera has been updated.
	camFrame		m_Frame;  						//The clean camera frame (not subject to post-effects.)
	camFrame		m_PostEffectFrame;  			//A clone of the regular frame affected by post-effects, such as shakes and frame interpolation.
	atArray<RegdCamBaseFrameShaker> m_FrameShakers;	//Optional support for shaking the camera frame.
	camFrameInterpolator* m_FrameInterpolator;		//Optional support for interpolation from another camera;
	const camDepthOfFieldSettingsMetadata* m_DofSettings;
	RegdCamCollision m_Collision;					//Optional support for collision checks.

	const camBaseCameraMetadata& m_Metadata;

	RegdConstEnt	m_AttachParent;
	RegdConstEnt	m_LookAtTarget;

	bool			m_WasUpdated								: 1; //Was this camera updated within the last game frame?
	bool			m_ShouldPreventNextCameraCloningOrientation	: 1;
	bool			m_ShouldPreventInterpolationToNextCamera	: 1;
	bool			m_AllowMotionBlurDecay						: 1;

	static camFrame		ms_ScratchFrame;
	static bank_float	ms_MotionBlurDecayRate;

private:
	//Forbid copy construction and assignment.
	camBaseCamera(const camBaseCamera& other);
	camBaseCamera& operator=(const camBaseCamera& other);
};

#endif // BASE_CAMERA_H
