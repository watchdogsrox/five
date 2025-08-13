//
// camera/cinematic/CinematicFallFromHeliContext.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_FALL_FROM_HELI_CONTEXT_H
#define CINEMATIC_FALL_FROM_HELI_CONTEXT_H

#include "camera/cinematic/context/BaseCinematicContext.h"
#include "vehicles/vehicle.h"

class camCinematicContextMetadata;
class camBaseCinematicShot; 

class camCinematicFallFromHeliContext : public camBaseCinematicContext
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicFallFromHeliContext, camBaseCinematicContext);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.
	
	const CEntity* GetTargetEntity() const { return m_AttachVehicle; }
	
protected:
	virtual bool IsValid(bool shouldConsiderControlInput = true, bool shouldConsiderViewMode = true) const; 
	
	virtual bool Update(); 
	
	virtual void PreUpdate(); 
	
	camCinematicFallFromHeliContext(const camBaseObjectMetadata& metadata);

	const camCinematicFallFromHeliContextMetadata&	m_Metadata;

private:
	RegdConstVeh m_AttachVehicle;
	bool m_HaveActiveTask; 
};

#endif
