//
// camera/gameplay/aim/FirstPersonPedAimCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef FIRST_PERSON_PED_AIM_CAMERA_H
#define FIRST_PERSON_PED_AIM_CAMERA_H

#include "camera/gameplay/aim/FirstPersonAimCamera.h"

class camFirstPersonPedAimCameraMetadata;

class camFirstPersonPedAimCamera : public camFirstPersonAimCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camFirstPersonPedAimCamera, camFirstPersonAimCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	virtual void	CloneBaseOrientation(const camBaseCamera& sourceCamera);

protected:
	virtual void	UpdateOrientation();
	void			ProcessCoverAutoHeadingCorrection(CPed& rPed, float& heading, bool bFacingLeftInCover, bool bInHighCover, bool bAtCorner, bool bAimDirectly);

private:
	camFirstPersonPedAimCamera(const camBaseObjectMetadata& metadata);
	~camFirstPersonPedAimCamera();

	virtual bool	Update();
	virtual void	ComputeRelativeAttachPosition(const Matrix34& targetMatrix, Vector3& relativeAttachPosition);
	virtual void	UpdateInteriorCollision();
	virtual void	UpdateDof();
	virtual void	ComputeDofSettings(float overriddenFocusDistance, float overriddenFocusDistanceBlendLevel, camDepthOfFieldSettingsMetadata& settings) const;

	void			ReorientCameraToPreviousAimDirection();

	const camFirstPersonPedAimCameraMetadata& m_Metadata;

	Vector3 m_PreviousRelativeAttachPosition;

	//Forbid copy construction and assignment.
	camFirstPersonPedAimCamera(const camFirstPersonPedAimCamera& other);
	camFirstPersonPedAimCamera& operator=(const camFirstPersonPedAimCamera& other);
};

#endif // FIRST_PERSON_PED_AIM_CAMERA_H
