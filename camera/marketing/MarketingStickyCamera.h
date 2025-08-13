//
// camera/marketing/MarketingStickyCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef MARKETING_STICKY_CAMERA_H
#define MARKETING_STICKY_CAMERA_H

#if __BANK //Marketing tools are only available in BANK builds.

#include "camera/marketing/MarketingFreeCamera.h"

class camMarketingStickyCameraMetadata;

//A marketing camera that allows free movement via control input and additionally can be fixed and made to look at the follow entity.
class camMarketingStickyCamera : public camMarketingFreeCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camMarketingStickyCamera, camMarketingFreeCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	camMarketingStickyCamera(const camBaseObjectMetadata& metadata);

public:
	virtual void		Reset();

private:
	virtual bool		Update();
	bool				UpdateLookAt();

	virtual void		UpdateMiscControl(CPad& pad);

	virtual void		ApplyTranslation();
	virtual void		ApplyRotation();

	const camMarketingStickyCameraMetadata& m_Metadata;

	Vector3				m_RelativeLookAtPosition;
	Vector3				m_RelativeLookAtPositionOffset;
	Vector3				m_WorldLookAtPosition;
	bool				m_ShouldUseStickyMode;

	//Forbid copy construction and assignment.
	camMarketingStickyCamera(const camMarketingStickyCamera& other);
	camMarketingStickyCamera& operator=(const camMarketingStickyCamera& other);
};

#endif // __BANK

#endif // MARKETING_STICKY_CAMERA_H
