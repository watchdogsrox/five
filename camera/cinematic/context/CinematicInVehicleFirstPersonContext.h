//
// camera/cinematic/camCinematicInVehicleFirstPersonContext.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_IN_VEHICLE_FIRST_PERSON_CONTEXT_H
#define CINEMATIC_IN_VEHICLE_FIRST_PERSON_CONTEXT_H

#include "camera/cinematic/context/BaseCinematicContext.h"

class camCinematicContextMetadata;
class camBaseCinematicShot; 
class camCinematicMountedCamera; 

class camCinematicInVehicleFirstPersonContext : public camBaseCinematicContext
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicInVehicleFirstPersonContext, camBaseCinematicContext);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	void PreCameraUpdate(camCinematicMountedCamera* pMountedCamera);

protected:
	virtual void PreUpdate();

	virtual bool IsValid(bool shouldConsiderControlInput = true, bool shouldConsiderViewMode = true) const; 

	virtual void ClearContext(); 

	camCinematicInVehicleFirstPersonContext(const camBaseObjectMetadata& metadata);

	const camCinematicInVehicleFirstPersonContextMetadata&	m_Metadata;

private:

	float m_fSeatSwitchSpringResult;
	float m_fSeatSwitchSpringVelocity;

	bool m_IsValid;
	bool m_bIsMountedCameraDataValid;

};


#endif