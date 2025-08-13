//
// camera/cinematic/shot/camCinematicMissileKillShot.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_MISSILE_KILL_SHOT_H
#define CINEMATIC_MISSILE_KILL_SHOT_H

#include "camera/base/BaseObject.h"
#include "camera/cinematic/shot/BaseCinematicShot.h"

class camCinematicMissileKillShot : public camBaseCinematicShot
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicMissileKillShot, camBaseCinematicShot);
public:

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	virtual void InitCamera(); 
	
	virtual bool IsValid() const;   

	virtual const CEntity* ComputeAttachEntity() const;
	virtual const CEntity* ComputeLookAtEntity() const;

	camCinematicMissileKillShot(const camBaseObjectMetadata& metadata);

private:
	const camCinematicMissileKillShotMetadata&	m_Metadata;
};

#endif