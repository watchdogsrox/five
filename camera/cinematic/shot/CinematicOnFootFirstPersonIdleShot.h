//
// camera/cinematic/shot/camCinematicOnFootFirstPersonIdleShot.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_ON_FOOT_FIRST_PERSON_IDLE_SHOT_H
#define CINEMATIC_ON_FOOT_FIRST_PERSON_IDLE_SHOT_H

#include "camera/base/BaseObject.h"
#include "camera/cinematic/shot/BaseCinematicShot.h"

class camCinematicOnFootFirstPersonIdleShot : public camBaseCinematicShot
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicOnFootFirstPersonIdleShot, camBaseCinematicShot);
public:

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	virtual void InitCamera(); 

	virtual bool CreateAndTestCamera(u32 hash);

	camCinematicOnFootFirstPersonIdleShot(const camBaseObjectMetadata& metadata);

private:
	const camCinematicOnFootFirstPersonIdleShotMetadata&	m_Metadata;
};

#endif