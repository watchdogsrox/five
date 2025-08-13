//
// camera/marketing/MarketingMountedCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef MARKETING_MOUNTED_CAMERA_H
#define MARKETING_MOUNTED_CAMERA_H

#if __BANK //Marketing tools are only available in BANK builds.

#include "camera/marketing/MarketingFreeCamera.h"

class camMarketingMountedCameraMetadata;
class camSpringMount;

//A marketing camera that allows free movement via control input and additionally can be mounted to an entity.
class camMarketingMountedCamera : public camMarketingFreeCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camMarketingMountedCamera, camMarketingFreeCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	camMarketingMountedCamera(const camBaseObjectMetadata& metadata);

public:
	~camMarketingMountedCamera();

	virtual void	Reset();
	virtual void	AddWidgetsForInstance();

private:
	virtual void	UpdateMiscControl(CPad& pad);
	virtual void	ApplyTranslation();
	virtual void	ApplyRotation();

	const camMarketingMountedCameraMetadata& m_Metadata;

	camSpringMount* m_SpringMount;
	Vector3			m_RelativeAttachPosition;
	Vector3			m_RelativeAttachPositionOffset;
	float			m_RelativeHeading;
	float			m_RelativePitch;
	bool			m_ShouldUseSpringMount;

	//Forbid copy construction and assignment.
	camMarketingMountedCamera(const camMarketingMountedCamera& other);
	camMarketingMountedCamera& operator=(const camMarketingMountedCamera& other);
};

#endif // __BANK

#endif // MARKETING_MOUNTED_CAMERA_H
