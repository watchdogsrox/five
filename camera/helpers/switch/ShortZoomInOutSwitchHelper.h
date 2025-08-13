//
// camera/helpers/switch/ShortZoomInOutSwitchHelper.h
// 
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#ifndef SHORT_ZOOM_IN_OUT_SWITCH_HELPER_H
#define SHORT_ZOOM_IN_OUT_SWITCH_HELPER_H

#include "camera/helpers/switch/BaseSwitchHelper.h"

class camShortZoomInOutSwitchHelperMetadata; 

class camShortZoomInOutSwitchHelper : public camBaseSwitchHelper
{
	DECLARE_RTTI_DERIVED_CLASS(camShortZoomInOutSwitchHelper, camBaseSwitchHelper);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

private:
	camShortZoomInOutSwitchHelper(const camBaseObjectMetadata& metadata);

public:
	virtual void	ComputeOrbitDistance(float& orbitDistance) const;
	virtual void	ComputeDesiredFov(float& desiredFov) const;

private:
	float			ComputeZoomFactor(float baseFov) const;

	const camShortZoomInOutSwitchHelperMetadata& m_Metadata;

	//Forbid copy construction and assignment.
	camShortZoomInOutSwitchHelper(const camShortZoomInOutSwitchHelper& other);
	camShortZoomInOutSwitchHelper& operator=(const camShortZoomInOutSwitchHelper& other);
};

#endif // SHORT_ZOOM_IN_OUT_SWITCH_HELPER_H
