//
// camera/helpers/switch/ShortZoomToHeadSwitchHelper.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef SHORT_ZOOM_TO_HEAD_SWITCH_HELPER_H
#define SHORT_ZOOM_TO_HEAD_SWITCH_HELPER_H

#include "camera/helpers/switch/BaseSwitchHelper.h"

class camShortZoomToHeadSwitchHelperMetadata; 

class camShortZoomToHeadSwitchHelper : public camBaseSwitchHelper
{
	DECLARE_RTTI_DERIVED_CLASS(camShortZoomToHeadSwitchHelper, camBaseSwitchHelper);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

private:
	camShortZoomToHeadSwitchHelper(const camBaseObjectMetadata& metadata);

public:
	virtual void	ComputeLookAtFront(const Vector3& cameraPosition, Vector3& lookAtFront) const;
	virtual void	ComputeDesiredFov(float& desiredFov) const;

private:
	virtual bool	ComputeShouldBlendIn() const;
	float			ComputeZoomFactor() const;

	const camShortZoomToHeadSwitchHelperMetadata& m_Metadata;

	//Forbid copy construction and assignment.
	camShortZoomToHeadSwitchHelper(const camShortZoomToHeadSwitchHelper& other);
	camShortZoomToHeadSwitchHelper& operator=(const camShortZoomToHeadSwitchHelper& other);
};

#endif // SHORT_ZOOM_TO_HEAD_SWITCH_HELPER_H
