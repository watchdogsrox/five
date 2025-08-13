//
// camera/cinematic/camCinematicInVehicleOverriddenFirstPersonContext.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_IN_VEHICLE_OVERRIDDEN_FIRST_PERSON_CONTEXT_H
#define CINEMATIC_IN_VEHICLE_OVERRIDDEN_FIRST_PERSON_CONTEXT_H

#include "camera/cinematic/context/BaseCinematicContext.h"

class camCinematicContextMetadata;
class camBaseCinematicShot; 

class camCinematicInVehicleOverriddenFirstPersonContext : public camBaseCinematicContext
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicInVehicleOverriddenFirstPersonContext, camBaseCinematicContext);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.
	
	virtual bool IsValid(bool shouldConsiderControlInput = true, bool shouldConsiderViewMode = true) const; 
protected:
	
	camCinematicInVehicleOverriddenFirstPersonContext(const camBaseObjectMetadata& metadata);

	const camCinematicInVehicleOverriddenFirstPersonContextMetadata&	m_Metadata;

private:
};


#endif