//
// camera/marketing/MarketingOrbitCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef MARKETING_ORBIT_CAMERA_H
#define MARKETING_ORBIT_CAMERA_H

#if __BANK //Marketing tools are only available in BANK builds.

#include "camera/marketing/MarketingFreeCamera.h"

class camMarketingOrbitCameraMetadata;
class camSpringMount;

//A marketing camera that allows free movement via control input and additionally can be attached to an entity and will orbit that entity.
class camMarketingOrbitCamera : public camMarketingFreeCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camMarketingOrbitCamera, camMarketingFreeCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	camMarketingOrbitCamera(const camBaseObjectMetadata& metadata);

public:
	~camMarketingOrbitCamera();

	virtual void	Reset();
	virtual void	AddWidgetsForInstance();

private:
	virtual void	UpdateMiscControl(CPad& pad);
	virtual void	ApplyTranslation();
	virtual void	ApplyRotation();

	const camMarketingOrbitCameraMetadata& m_Metadata;

	camSpringMount* m_SpringMount;
	Vector3			m_RelativeLookatPosition;		// look-at point on the character, in character space
	Vector3			m_RelativeLookatPositionOffset;
	Vector3			m_RelativeCameraPosition;		// position of camera relative to the look-at point
	float			m_Pitch;
	float			m_Heading;
	float			m_CameraDistance;				// distance from camera to look-at point

	bool			m_ShouldUseSpringMount;

	//Forbid copy construction and assignment.
	camMarketingOrbitCamera(const camMarketingOrbitCamera& other);
	camMarketingOrbitCamera& operator=(const camMarketingOrbitCamera& other);
};

#endif // __BANK

#endif // MARKETING_ORBIT_CAMERA_H
