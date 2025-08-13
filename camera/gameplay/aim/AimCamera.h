//
// camera/gameplay/aim/AimCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef AIM_CAMERA_H
#define AIM_CAMERA_H

#include "camera/base/BaseCamera.h"

class camAimCameraMetadata;
class camControlHelper;

class camAimCamera : public camBaseCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camAimCamera, camBaseCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	camAimCamera(const camBaseObjectMetadata& metadata);

public:
	virtual ~camAimCamera();

	virtual const camControlHelper*	GetControlHelper() const	{ return m_ControlHelper; }
	virtual camControlHelper*		GetControlHelper()			{ return m_ControlHelper; }

	virtual void	CloneBaseOrientation(const camBaseCamera& sourceCamera);
	void			CloneControlSpeeds(const camBaseCamera& sourceCamera);
	void			CloneBaseOrientationFromSeat(const camBaseCamera& sourceCamera);

	void			SetRelativeHeading(float heading)				{ m_OverriddenRelativeHeading = heading; m_ShouldOverrideRelativeHeading = true; }
	void			SetRelativePitch(float pitch)					{ m_OverriddenRelativePitch = pitch; m_ShouldOverrideRelativePitch = true; }
	void			SetRelativeHeadingLimits(const Vector2& limits)	{ m_OverriddenRelativeHeadingLimitsThisUpdate = limits; m_ShouldOverrideRelativeHeadingLimitsThisUpdate = true; }
	void			SetRelativePitchLimits(const Vector2& limits)	{ m_OverriddenRelativePitchLimitsThisUpdate = limits; m_ShouldOverrideRelativePitchLimitsThisUpdate = true; }
	void			SetNearClipThisUpdate(float nearClip)			{ m_OverriddenNearClip = nearClip;  m_ShouldOverrideNearClipThisUpdate = true;}

	bool			GetWasRelativeHeadingLimitsOverriddenThisUpdate() const	{ return m_WasRelativeHeadingLimitsOverriddenThisUpdate; }
	bool			GetWasRelativePitchLimitsOverriddenThisUpdate() const	{ return m_WasRelativePitchLimitsOverriddenThisUpdate; }

#if __BANK
	virtual void	DebugRender();
#endif // __BANK

protected:
	virtual bool	Update();
	virtual void	UpdatePosition() = 0;
	virtual void	ComputeRelativeAttachPosition(const Matrix34& targetMatrix, Vector3& relativeAttachPosition);
	void			TransformAttachPositionToWorldSpace(const Matrix34& targetMatrix, const Vector3& relativeAttachPosition, Vector3& attachPosition) const;
	void			TransformAttachPositionToLocalSpace(const Matrix34& targetMatrix, const Vector3& attachPosition, Vector3& relativeAttachPosition) const;
	void			TransformAttachOffsetToWorldSpace(const Matrix34& targetMatrix, const Vector3& relativeAttachOffset, Vector3& attachOffset) const;
	void			TransformAttachOffsetToLocalSpace(const Matrix34& targetMatrix, const Vector3& attachOffset, Vector3& relativeAttachOffset) const;
	virtual bool	ComputeShouldApplyAttachOffsetRelativeToCamera() const;
	virtual void	UpdateZoom();
	virtual void	UpdateOrientation() = 0;
	virtual void	UpdateControlHelper();
	void			ApplyOverriddenOrientation(float& heading, float& pitch);
	void			ApplyOrientationLimits(const Matrix34& targetMatrix, float& heading, float& pitch) const;
	virtual void	UpdateMotionBlur();

	const camAimCameraMetadata& m_Metadata;

	camControlHelper* m_ControlHelper;
	Vector3			m_AttachPosition;
	Vector2			m_OverriddenRelativeHeadingLimitsThisUpdate;
	Vector2			m_OverriddenRelativePitchLimitsThisUpdate;
	float			m_MinPitch;
	float			m_MaxPitch;
	float			m_DesiredMinPitch;
	float			m_DesiredMaxPitch;
	float			m_OverriddenRelativeHeading;
	float			m_OverriddenRelativePitch;
	float			m_ZoomFovDelta;
	float			m_OverriddenNearClip; 
	bool			m_ShouldOverrideRelativeHeading					: 1;
	bool			m_ShouldOverrideRelativePitch					: 1;
	bool			m_ShouldOverrideRelativeHeadingLimitsThisUpdate	: 1;
	bool			m_ShouldOverrideRelativePitchLimitsThisUpdate	: 1;
	bool			m_ShouldOverrideNearClipThisUpdate				: 1; 
	bool			m_WasRelativeHeadingLimitsOverriddenThisUpdate	: 1;
	bool			m_WasRelativePitchLimitsOverriddenThisUpdate	: 1;

private:
	//Forbid copy construction and assignment.
	camAimCamera(const camAimCamera& other);
	camAimCamera& operator=(const camAimCamera& other);
};

#endif // AIM_CAMERA_H
