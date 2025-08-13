//
// camera/cinematic/shot/camCinematicOnFootMeleeShot.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_ON_FOOT_MELEE_SHOT_H
#define CINEMATIC_ON_FOOT_MELEE_SHOT_H

#include "camera/base/BaseObject.h"
#include "camera/cinematic/shot/BaseCinematicShot.h"

class camCinematicOnFootMeleeShot : public camBaseCinematicShot
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicOnFootMeleeShot, camBaseCinematicShot);
public:

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.
	
	virtual bool IsValid() const; 

protected:
	virtual void InitCamera(); 

	virtual bool CanCreate(); 
	
	virtual void Update(); 
	
	camCinematicOnFootMeleeShot(const camBaseObjectMetadata& metadata);


private:
	const camCinematicOnFootMeleeShotMetadata&	m_Metadata;
};

#endif