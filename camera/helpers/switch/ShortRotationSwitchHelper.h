//
// camera/helpers/switch/ShortRotationSwitchHelper.h
// 
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#ifndef SHORT_ROTATION_SWITCH_HELPER_H
#define SHORT_ROTATION_SWITCH_HELPER_H

#include "camera/helpers/switch/BaseSwitchHelper.h"

class camShortRotationSwitchHelperMetadata; 

class camShortRotationSwitchHelper : public camBaseSwitchHelper
{
	DECLARE_RTTI_DERIVED_CLASS(camShortRotationSwitchHelper, camBaseSwitchHelper);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

private:
	camShortRotationSwitchHelper(const camBaseObjectMetadata& metadata);

public:
	virtual void	ComputeOrbitDistance(float& orbitDistance) const;
	virtual void	ComputeOrbitOrientation(float attachParentHeading, float& orbitHeading, float& orbitPitch) const;

private:
	const camShortRotationSwitchHelperMetadata& m_Metadata;
	mutable float	m_DesiredOrbitHeadingOffset;

	//Forbid copy construction and assignment.
	camShortRotationSwitchHelper(const camShortRotationSwitchHelper& other);
	camShortRotationSwitchHelper& operator=(const camShortRotationSwitchHelper& other);
};

#endif // SHORT_ROTATION_SWITCH_HELPER_H
