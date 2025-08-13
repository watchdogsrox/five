//
// camera/cinematic/shot/camCinematicStuntJumpShot.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_STUNT_JUMP_SHOT_H
#define CINEMATIC_STUNT_JUMP_SHOT_H

#include "camera/base/BaseObject.h"
#include "camera/cinematic/shot/BaseCinematicShot.h"

class camCinematicStuntJumpShot : public camBaseCinematicShot
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicStuntJumpShot, camBaseCinematicShot);
public:

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	virtual void InitCamera(); 

	camCinematicStuntJumpShot(const camBaseObjectMetadata& metadata);


private:
	const camCinematicStuntJumpShotMetadata&	m_Metadata;
};

#endif