//
// camera/helpers/switch/LongSwoopSwitchHelper.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef LONG_SWOOP_SWITCH_HELPER_H
#define LONG_SWOOP_SWITCH_HELPER_H

#include "camera/helpers/switch/BaseSwitchHelper.h"

class camLongSwoopSwitchHelperMetadata; 

class camLongSwoopSwitchHelper : public camBaseSwitchHelper
{
	DECLARE_RTTI_DERIVED_CLASS(camLongSwoopSwitchHelper, camBaseSwitchHelper);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

private:
	camLongSwoopSwitchHelper(const camBaseObjectMetadata& metadata);

public:
	virtual void	ComputeOrbitDistance(float& orbitDistance) const;
	virtual void	ComputeOrbitOrientation(float attachParentHeading, float& orbitHeading, float& orbitPitch) const;
	virtual void	ComputeOrbitRelativePivotOffset(Vector3& orbitRelativeOffset) const;
	virtual void	ComputeDesiredFov(float& desiredFov) const;

private:
	const camLongSwoopSwitchHelperMetadata& m_Metadata;

	//Forbid copy construction and assignment.
	camLongSwoopSwitchHelper(const camLongSwoopSwitchHelper& other);
	camLongSwoopSwitchHelper& operator=(const camLongSwoopSwitchHelper& other);
};

#endif // LONG_SWOOP_SWITCH_HELPER_H
