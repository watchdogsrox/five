//
// camera/cinematic/CinematicMissileKillContext.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_MISSILE_KILL_CONTEXT_H
#define CINEMATIC_MISSILE_KILL_CONTEXT_H

#include "camera/cinematic/context/BaseCinematicContext.h"

class camCinematicContextMetadata;
class camBaseCinematicShot; 

class camCinematicMissileKillContext : public camBaseCinematicContext
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicMissileKillContext, camBaseCinematicContext);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.
	
	const CEntity* GetTargetEntity() const { return m_TargetEntity; }
protected:
	virtual bool Update(); 
	virtual void PreUpdate(); 
	
	virtual bool IsValid(bool shouldConsiderControlInput = true, bool shouldConsiderViewMode = true) const; 

	camCinematicMissileKillContext(const camBaseObjectMetadata& metadata);

	const camCinematicMissileKillContextMetadata&	m_Metadata;

private:
	bool m_haveActiveShot; 
	RegdConstEnt m_TargetEntity; 

};


#endif