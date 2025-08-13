//
// camera/helpers/switch/ShortTranslationSwitchHelper.h
// 
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#ifndef SHORT_TRANSLATION_SWITCH_HELPER_H
#define SHORT_TRANSLATION_SWITCH_HELPER_H

#include "vector/vector3.h"

#include "camera/helpers/switch/BaseSwitchHelper.h"

class camShortTranslationSwitchHelperMetadata; 

class camShortTranslationSwitchHelper : public camBaseSwitchHelper
{
	DECLARE_RTTI_DERIVED_CLASS(camShortTranslationSwitchHelper, camBaseSwitchHelper);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

private:
	camShortTranslationSwitchHelper(const camBaseObjectMetadata& metadata);

public:
	virtual void	ComputeOrbitDistance(float& orbitDistance) const;
	virtual void	ComputeOrbitOrientation(float attachParentHeading, float& orbitHeading, float& orbitPitch) const;
	virtual void	ComputeOrbitRelativePivotOffset(Vector3& orbitRelativeOffset) const;

private:
	const camShortTranslationSwitchHelperMetadata& m_Metadata;
	mutable Vector3	m_DesiredExtraOrbitRelativeOffset;

	//Forbid copy construction and assignment.
	camShortTranslationSwitchHelper(const camShortTranslationSwitchHelper& other);
	camShortTranslationSwitchHelper& operator=(const camShortTranslationSwitchHelper& other);
};

#endif // SHORT_TRANSLATION_SWITCH_HELPER_H
