//
// camera/helpers/LookAtDampingHelper.h
// 
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#ifndef LOOK_AT_DAMPING_HELPER_H
#define LOOK_AT_DAMPING_HELPER_H

#include "camera/base/BaseObject.h"
#include "vector/matrix34.h"
#include "camera/helpers/DampedSpring.h"

class camLookAtDampingHelperMetadata; 

class camLookAtDampingHelper : public camBaseObject
{
	DECLARE_RTTI_DERIVED_CLASS(camLookAtDampingHelper, camBaseObject);

public:

	FW_REGISTER_CLASS_POOL(camLookAtDampingHelper);

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

public:

	void Reset(); 
	
	void Update(Matrix34& LookAtMat, float Fov, bool IsFirstUpdate);

private:
	camLookAtDampingHelper(const camBaseObjectMetadata& metadata);

private:
	const camLookAtDampingHelperMetadata& m_Metadata;

	camDampedSpring m_PitchSpring; 
	camDampedSpring m_HeadingSpring; 
	camDampedSpring m_HeadingSpeedSpring;
	camDampedSpring m_PitchSpeedSpring;

	float m_DesiredHeadingOnPreviousUpdate; 
	float m_DesiredPitchOnPreviousUpdate; 

	bool m_IsSecondUpdate;

private:
	//Forbid copy construction and assignment.
	camLookAtDampingHelper(const camLookAtDampingHelper& other);
	camLookAtDampingHelper& operator=(const camLookAtDampingHelper& other);
};

#endif
