//
// camera/helpers/LookAheadHelper.h
// 
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

#ifndef LOOK_AHEAD_HELPER_H
#define LOOK_AHEAD_HELPER_H

#include "camera/base/BaseObject.h"
#include "camera/helpers/DampedSpring.h"

class camLookAheadHelperMetadata; 
class CPhysical;

namespace rage
{
	class Vector3;
};

class camLookAheadHelper : public camBaseObject
{
	DECLARE_RTTI_DERIVED_CLASS(camLookAheadHelper, camBaseObject);

public:
	FW_REGISTER_CLASS_POOL(camLookAheadHelper);

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	void UpdateConsideringLookAtTarget(const CPhysical* lookAtTarget, bool isFirstUpdate, Vector3& lookAheadPositionOffset);

#if __BANK
	static void	AddWidgets(bkBank &bank);
#endif // __BANK

private:
	camLookAheadHelper(const camBaseObjectMetadata& metadata);

	const camLookAheadHelperMetadata& m_Metadata;
	camDampedSpring m_Spring;

#if __BANK
	static bool ms_DebugEnableDebugDisplay;
#endif // __BANK

	//Forbid copy construction and assignment.
	camLookAheadHelper(const camLookAheadHelper& other);
	camLookAheadHelper& operator=(const camLookAheadHelper& other);
};
#endif //LOOK_AHEAD_HELPER_H