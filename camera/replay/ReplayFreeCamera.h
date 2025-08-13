//
// camera/replay/ReplayFreeCamera.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef REPLAY_FREE_CAMERA_H
#define REPLAY_FREE_CAMERA_H

#include "camera/replay/ReplayBaseCamera.h"

#if GTA_REPLAY

#include "camera/helpers/Interpolator.h"
#include "camera/helpers/DampedSpring.h"
#include "scene/RegdRefTypes.h"
#include "system/control.h"

#include "physics/WorldProbe/shapetestresults.h"

class camReplayFreeCameraMetadata;
struct sReplayMarkerInfo;

namespace rage
{
    class fwEntity;
}

class camReplayFreeCamera : public camReplayBaseCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camReplayFreeCamera, camReplayBaseCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	camReplayFreeCamera(const camBaseObjectMetadata& metadata);

#if KEYBOARD_MOUSE_SUPPORT	
	enum eMouseInputModes
	{
		NO_INPUT,
		ROTATION,
		LOOKAT_ORBIT,
		LOOKAT_OFFSET,
		LOOKAT_VERTICALTRANSLATION
	};
#endif

public:
	void			ResetForNewAttachEntity();
	void			ResetForNewLookAtEntity();
	void			BlendFromCamera(const camBaseCamera& sourceCamera, const s32 sourceMarkerIndex);
	bool			IsBlendingFromCamera() const;
	u32				GetCameraBlendingFromNameHash() const { return m_CameraToBlendFrom ? m_CameraToBlendFrom->GetNameHash() : 0; }
	float			GetMaxPhaseToHaltBlendMovement() const { return m_MaxPhaseToHaltBlendMovement; }
	float			GetCurrentPhaseForBlend(float currentTime, bool shouldClampToMaxPhase = true) const;
	bool			HasFollowOrLookAtTarget() const { return (m_AttachEntity != NULL || m_LookAtEntity != NULL); }

	bool			IsPositionOutsidePlayerLimits(const Vector3& position) const;
    bool            IsBlendingPositionInvalid() const { return m_IsBlendingPositionInvalid; }

	void			GetMinMaxFov(float& minFov, float& maxFov) const;
	void			GetDefaultFov(float& defaultFov) const;

	virtual bool	Init(const s32 markerIndex, const bool shouldForceDefaultFrame = false);
	virtual bool	ShouldFallbackToGameplayCamera() const;
#if GTA_REPLAY
	virtual void	GetDefaultFrame(camFrame& frame) const; 
#endif // GTA_REPLAY
#if __BANK
	void			DebugGetDesiredPosition(Vector3& desiredPosition) const { desiredPosition.Set(m_DesiredPosition); }
	void			DebugGetRelativeAttachHeadingPitchAndRoll(float& heading, float& pitch, float& roll) const { heading = m_AttachEntityRelativeHeading; pitch = m_AttachEntityRelativePitch; roll = m_AttachEntityRelativeRoll; }
	virtual void	DebugRender();
	void			DebugGetBlendType(char* phaseText) const;
#endif // __BANK

protected:
	bool			SetToSafePosition(Vector3& cameraPosition, bool shouldFlagWarning = false, bool shouldFlagBlendingWarning = false, const CEntity* entity = NULL) const;
	virtual bool	Update();
	void			UpdateAttachEntityMatrix();
	bool			ComputeSafePosition(const CEntity& playerPed, Vector3& cameraPosition) const;
	void			UpdateMaxPhaseToHaltBlend();
	void			ResetCamera();
	void			UpdateControl(const CControl& control, Vector3& desiredTranslation, Vector2& desiredLookAtOffsetTranslation, Vector3& desiredRotation, float& desiredZoom);
#if KEYBOARD_MOUSE_SUPPORT
	static bool		IsMouseLookInputActive(const CControl& control);
#endif
	void			UpdateTranslationControl(const CControl& control, Vector3& desiredTranslation, Vector2& desiredLookAtOffsetTranslation);
	void			ComputeTranslationInput(const CControl& control, Vector3& translationInput, Vector2& lookAtOffsetInput KEYBOARD_MOUSE_ONLY(, s32& mouseInputMode)) const;
	void			UpdateRotationControl(const CControl& control, Vector3& desiredRotation);
	void			ComputeRotationInput(const CControl& control, Vector3& rotationInput KEYBOARD_MOUSE_ONLY(, s32& mouseInputMode)) const;
	void			UpdateZoomControl(const CControl& control, float& desiredZoom);
	void			ComputeZoomInput(const CControl& control, float& zoomInput) const;
	bool			GetShouldResetCamera(const CControl& control) const;
	void			UpdateZoom(const float desiredZoom);
	void			UpdateRotation(const Vector3& desiredRotation, const Vector2& desiredLookAtOffsetTranslation);
	void			UpdatePosition(const Vector3& desiredTranslation);
	bool			UpdateCollisionWithAttachEntity(const CEntity& attachEntityToUse, Vector3& cameraPosition);
	void			UpdateCollisionOrigin(const CEntity& attachEntityToUse, float& collisionTestRadius);
	void			ComputeCollisionOrigin(const CEntity& attachEntityToUse, float& collisionTestRadius, Vector3& collisionOrigin) const;
	void			UpdateCollisionRootPosition(const CEntity& attachEntityToUse, float& collisionTestRadius);
	void			ComputeCollisionRootPosition(const CEntity& attachEntityToUse, const Vector3& collisionOrigin, float& collisionTestRadius, Vector3& collisionRootPosition) const;
	bool			UpdateCollision(const Vector3& initialPosition, Vector3& cameraPosition) const;
	float			ComputeFilteredHitTValue(const WorldProbe::CShapeTestFixedResults<>& shapeTestResults, bool isSphereTest) const;
	void			LoadEntities(const sReplayMarkerInfo& markerInfo, camFrame& frame);
	virtual void	PostUpdate();
    void            UpdateBlendPhaseConsideringWater(const Vector3& positionToApply, float& phase);
	void			UpdatePortalTracker();
	float			ComputeBoundRadius(const CEntity* entity);

    void            ComputeLookAtFrameWithOffset( const Vector3& lookAtPoint, 
                                                  const Vector3& cameraPosition, 
                                                  const Vector2& lookAtOffsetScreenSpace, 
                                                  const float fov, 
                                                  const float roll, 
                                                  Matrix34& resultWorldMatrix);

	virtual void	SaveMarkerCameraInfo(sReplayMarkerInfo& markerInfo, const camFrame& camFrame) const;
	virtual void	LoadMarkerCameraInfo(sReplayMarkerInfo& markerInfo, camFrame& camFrame);
	virtual void	ValidateEntities(sReplayMarkerInfo& markerInfo);

	void			RenderCameraConstraintMarker(bool constrainedThisFrame );

	const	camReplayFreeCameraMetadata& m_Metadata;

	mutable camFrame m_ResetFrame;
	camDampedSpring	m_CollisionTestRadiusSpring;
	camDampedSpring m_RollCorrectionBlendLevelSpring;
    camInterpolator m_PhaseInterpolatorIn;
    camInterpolator m_PhaseInterpolatorOut;
    camInterpolator m_PhaseInterpolatorInOut;
	Matrix34		m_AttachEntityMatrix;
    Matrix34        m_PreviousAttachEntityMatrix;
	Vector3			m_CollisionOrigin;
	Vector3			m_CollisionRootPosition;
	Vector3			m_DesiredPosition;
	Vector3			m_TranslationInputThisUpdate;
	Vector2			m_LookAtOffset;
	Vector2			m_CachedNormalisedHorizontalTranslationInput;
	Vector2			m_CachedNormalisedHeadingPitchTranslationInput;
	RegdConstCamBaseCamera m_CameraToBlendFrom;
	RegdConstEnt	m_AttachEntity;
	RegdConstEnt	m_LookAtEntity;
	s32				m_LastComputedEntityForBoundRadius;
	mutable s32		m_LastEditingMarkerIndex;
	s32				m_BlendSourceMarkerIndex;
#if KEYBOARD_MOUSE_SUPPORT
	s32				m_MouseInputMode;
#endif
	u32				m_DurationOfMaxStickInputForRollCorrection;
	u32				m_NumCollisionUpdatesPerformedWithAttachEntity;	
	u32				m_LastConstrainedTime;
    u32             m_PreviousAttachTime;

	float			m_CachedVerticalTranslationInputSign;
	float			m_CachedHorizontalLookAtOffsetInputSign;
	float			m_CachedVerticalLookAtOffsetInputSign;
	float			m_CachedRollRotationInputSign;
	float			m_HorizontalSpeed;
	float			m_VerticalSpeed;
	float			m_HorizontalLookAtOffsetSpeed;
	float			m_VerticalLookAtOffsetSpeed;
	float			m_LookAtRoll;
	float			m_HeadingPitchSpeed;
	float			m_RollSpeed;
	float			m_ZoomSpeed;
	float			m_SmoothedWaterHeight;
    float           m_SmoothedWaterHeightPostUpdate;
	float			m_AttachEntityRelativeHeading;
	float			m_AttachEntityRelativePitch;
	float			m_AttachEntityRelativeRoll;
	float			m_MaxPhaseToHaltBlendMovement;
    //we use this to keep track of the last blend ratio that represent a valid position for the camera. 
    //This will help us during transition in and out of water.
    float           m_LastValidPhase;
	bool			m_AttachEntityIsInVehicle;
	bool			m_BlendHalted;
	bool			m_IsANewMarker;
	bool			m_ShouldUpdateMaxPhaseToHaltBlend;
	bool			m_ResetForNewAttachEntityThisUpdate;
    bool            m_IsBlendingPositionInvalid;

private:
	//Forbid copy construction and assignment.
	camReplayFreeCamera(const camReplayFreeCamera& other);
	camReplayFreeCamera& operator=(const camReplayFreeCamera& other);
};

//Used to disable the free camera from re-probing every frame in certain locations.
#define MAX_INTERIOR_NAME 25
struct sDisableReprobingInteriors
{
	BANK_ONLY(char interiorName[MAX_INTERIOR_NAME];)
	u32 interiorHash;
	s32 interiorRoom;
};

//Used to disable the free camera entering certain locations with incorrect collision data.
struct sAdditionalCollisionInteriorChecks
{
	BANK_ONLY(char interiorName[MAX_INTERIOR_NAME];)
	u32 interiorHash;
	s32 interiorRoom;
	float interiorMaxZHeight;
};

//Used to disable the free camera entering certain locations with incorrect collision data.
#define MAX_MODEL_NAME 64
struct sAdditionalObjectsChecks
{
    BANK_ONLY(char modelName[MAX_MODEL_NAME];)
    u32 modelHash;
    float zOffset;
};

#endif // GTA_REPLAY

#endif // REPLAY_FREE_CAMERA_H
