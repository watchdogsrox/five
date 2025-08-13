//
// camera/cinematic/shot/camCinematicFallFromHeliShot.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_FALL_FROM_HELI_SHOT_H
#define CINEMATIC_FALL_FROM_HELI_SHOT_H

#include "camera/base/BaseObject.h"
#include "camera/cinematic/shot/BaseCinematicShot.h"

class camCinematicFallFromHeliShot : public camBaseCinematicShot
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicFallFromHeliShot, camBaseCinematicShot);
public:

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	virtual bool IsValid() const;   

	virtual void InitCamera(); 

	camCinematicFallFromHeliShot(const camBaseObjectMetadata& metadata);
	
	const CEntity* ComputeAttachEntity() const;
	
	virtual bool CanCreate();
private:
	const camCinematicFallFromHeliShotMetadata&	m_Metadata;

	RegdConstVeh m_TargetVehicle; 
};

#endif