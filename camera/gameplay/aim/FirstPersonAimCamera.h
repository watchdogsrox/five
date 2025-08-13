//
// camera/gameplay/aim/FirstPersonAimCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef FIRST_PERSON_AIM_CAMERA_H
#define FIRST_PERSON_AIM_CAMERA_H

#include "camera/gameplay/aim/AimCamera.h"
#include "camera/helpers/DampedSpring.h"

class camFirstPersonAimCameraMetadata;
class camStickyAimHelper;

class camFirstPersonAimCamera : public camAimCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camFirstPersonAimCamera, camAimCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	bool			ShouldMakeAttachedEntityInvisible() const;
	bool			ShouldMakeAttachedPedHeadInvisible() const;
	bool			ShouldDisplayReticule() const;

	float			GetRelativeCoverHeadingSpringVelocity() const { return m_RelativeCoverHeadingSpring.GetVelocity(); }

	camStickyAimHelper* GetStickyAimHelper() { return m_StickyAimHelper; }

protected:
	camFirstPersonAimCamera(const camBaseObjectMetadata& metadata);
	~camFirstPersonAimCamera();

	virtual bool	Update();
	virtual void	UpdatePosition();
	virtual void	UpdateOrientation();
	virtual void	UpdateInteriorCollision()	{ }
	void			UpdateShake();

	void TryAndCreateOrDestroyStickyAimHelper();
	virtual bool CanUpdateStickyAimHelper() { return true; }

	const camFirstPersonAimCameraMetadata& m_Metadata;

	camDampedSpring m_RelativeCoverHeadingSpring;

	camStickyAimHelper* m_StickyAimHelper;

private:
	//Forbid copy construction and assignment.
	camFirstPersonAimCamera(const camFirstPersonAimCamera& other);
	camFirstPersonAimCamera& operator=(const camFirstPersonAimCamera& other);
};

#endif // FIRST_PERSON_AIM_CAMERA_H
