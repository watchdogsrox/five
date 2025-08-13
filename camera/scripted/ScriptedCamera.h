//
// camera/scripted/ScriptedCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef SCRIPTED_CAMERA_H
#define SCRIPTED_CAMERA_H

#include "camera/base/BaseCamera.h"
#include "scene/RegdRefTypes.h"

#define MAX_SCRIPTED_CAM_DEBUG_NAME		32

class camScriptedCameraMetadata;

class camScriptedCamera : public camBaseCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camScriptedCamera, camBaseCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	camScriptedCamera(const camBaseObjectMetadata& metadata);

public:
	virtual ~camScriptedCamera();

	virtual void CloneFrom(const camBaseCamera* const sourceCam);

	enum eLookAtModes
	{
		LOOK_AT_POSITION,
		LOOK_AT_ENTITY,
		LOOK_AT_CAMERA,
		LOOK_AT_NOTHING
	};

	bool		IsAttached() const								{ return (m_AttachParent.Get() != NULL); }
	bool		IsInheritingRoll() const						{ return (m_RollEntity.Get() != NULL); }
	bool		IsLookingAtSomething() const					{ return (m_LookAtMode != LOOK_AT_NOTHING); }
	eLookAtModes GetLookAtMode() const							{ return m_LookAtMode; }

	bool		ShouldAffectAiming() const						{ return m_ShouldAffectAiming; }
	bool		ShouldControlMiniMapHeading() const				{ return m_ShouldControlMiniMapHeading; }
	bool		IsInsideVehicle() const							{ return m_IsInsideVehicle; }

	virtual void AttachTo(const CEntity* parent);
	void		SetAttachBoneTag(s32 boneTag, bool hardAttachment = false);
	void		SetAttachOffset(const Vector3& offset);
	void		SetAttachRotationOffset(const Vector3& offset);
	void		SetAttachOffsetRelativeToMatrixState(bool b);

	const Vector3&	GetLookAtPosition() const					{ return m_LookAtPosition; }
	const Vector3&	GetAttachOffset() const						{ return m_AttachOffset; }
	const Vector3&	GetLookAtOffset() const						{ return m_LookAtOffset; }
	const Vector3&	GetCachedLookAtPosition() const				{ return m_CachedLookAtPosition; }
	bool		IsAttachOffsetRelativeToMatrix() const			{ return m_IsAttachOffsetRelativeToMatrix; }
	bool		IsLookAtOffsetRelativeToMatrix() const			{ return m_IsLookAtOffsetRelativeToMatrix; }

	s32			GetAttachBoneTag() const						{ return m_AttachBoneTag; }
	s32			GetLookAtBoneTag() const						{ return m_LookAtBoneTag; }

	virtual void LookAt(const CEntity* target);
	void		LookAt(const Vector3& position);
	void		LookAt(camBaseCamera* camera);
	void		ClearLookAtTarget();
	void		SetLookAtBoneTag(s32 boneTag);
	void		SetLookAtOffset(const Vector3& offset);
	void		SetLookAtOffsetRelativeToMatrixState(bool b);

	void		SetRollTarget(const CEntity* entity);
	void		ClearRollTarget();
	void		SetRoll(float roll);

	void		SetShouldAffectAiming(bool state)				{ m_ShouldAffectAiming = state; }
	void		SetShouldControlMiniMapHeading(bool state)		{ m_ShouldControlMiniMapHeading = state; }
	void		SetIsInsideVehicle(bool state)					{ m_IsInsideVehicle = state; }

	void		SetCustomMaxNearClip(float nearClip)			{ m_CustomMaxNearClip = nearClip; }
	float		GetCustomMaxNearClip() const					{ return m_CustomMaxNearClip; }

	const camDepthOfFieldSettingsMetadata* GetOverriddenDofSettings() const	{ return m_OverriddenDofSettings; }
	camDepthOfFieldSettingsMetadata* GetOverriddenDofSettings()				{ return m_OverriddenDofSettings; }

	void		SetOverriddenDofFocusDistance(float distance)				{ m_OverriddenDofFocusDistance = distance; }
	void		SetOverriddenDofFocusDistanceBlendLevel(float blendLevel)	{ m_OverriddenDofFocusDistanceBlendLevel = blendLevel; }

	void		SetShouldOverrideFarClip(bool state)						{ m_ShouldOverrideFarClip = state; }

#if __BANK
	void		SetDebugName(const char* name)					{ safecpy(m_szDebugName, name, MAX_SCRIPTED_CAM_DEBUG_NAME); }
	const char* GetDebugName() const							{ return m_szDebugName; }
#endif // __BANK

protected:

	virtual bool Update();
	virtual void ComputeDofOverriddenFocusSettings(Vector3& focusPosition, float& blendLevel) const;

	const camScriptedCameraMetadata& m_Metadata;

	Vector3		m_LookAtPosition;				//A position to look at.
	Vector3		m_CachedLookAtPosition;			//The last valid look at target position.
	Vector3		m_AttachOffset;					//A position offset relative to the attached entity (position or matrix.)
	Vector3		m_AttachRotationOffset;			//A rotation offset relative to the attached bone
	Vector3		m_LookAtOffset;					//A position offset relative to the look at target (position or matrix.)

	RegdConstEnt m_RollEntity;					//An entity to attach our roll to.

	RegdCamBaseCamera m_LookAtCamera;			//A camera to look at.

	camDepthOfFieldSettingsMetadata* m_OverriddenDofSettings;

	eLookAtModes m_LookAtMode;					//Defines whether we are looking at a position, another camera or an entity.

	s32			m_AttachBoneTag;				//An optional bone tag to attach to if the attached entity is a ped (-1 is invalid.)
	s32			m_LookAtBoneTag;				//An optional bone tag to look at if the look-at entity is a ped (-1 is invalid.)

	float		m_OverriddenRoll;				//If valid, this overrides any roll associated with an attachment.
	float		m_CustomMaxNearClip;			//Set in script to push out the maximum near-clip

	float		m_OverriddenDofFocusDistance;
	float		m_OverriddenDofFocusDistanceBlendLevel;

#if __BANK
	char		m_szDebugName[MAX_SCRIPTED_CAM_DEBUG_NAME];
#endif // __BANK

	bool		m_IsAttachOffsetRelativeToMatrix	: 1;	//If true, the attach offset position is relative to the matrix of the attached entity.
	bool		m_IsLookAtOffsetRelativeToMatrix	: 1;	//If true, the look-at offset position is relative to the matrix of the look-at target.
	bool		m_ShouldAffectAiming				: 1;	//If true, the frame of this camera (when rendered) can influence aiming.
	bool		m_ShouldControlMiniMapHeading		: 1;	//If true, the heading of this camera (when rendered) can influence the mini-map.
	bool		m_IsInsideVehicle					: 1;	//If true, this camera will report that it's inside a vehicle, which (when rendered) affects the rendering of vehicle glass.
	bool		m_HardAttachment					: 1;	//If true, the camera will be fully attached to the bone matrix
	bool		m_ShouldOverrideFarClip				: 1;	//If true, the camera will override the far clip plane imposed by the TimeCycle 

private:
	//Forbid copy construction and assignment.
	camScriptedCamera(const camScriptedCamera& other);
	camScriptedCamera& operator=(const camScriptedCamera& other);
};

#endif // SCRIPTED_CAMERA_H
