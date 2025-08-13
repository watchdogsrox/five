//
// camera/cinematic/CinematicInVehicleWantedContext.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_IN_VEHICLE_WANTED_CONTEXT_H
#define CINEMATIC_IN_VEHICLE_WANTED_CONTEXT_H

#include "camera/cinematic/context/BaseCinematicContext.h"

class camCinematicContextMetadata;
class camBaseCinematicShot; 

enum eTaskDependentShots
{
	TDS_POLICE_HELI_MOUNTED_SHOT = BIT0,
	TDS_POLICE_CAR_MOUNTED_SHOT = BIT1,
	TDS_POLICE_EXIT_VEHICLE_SHOT = BIT2,
	TDS_POLICE_IN_COVER_SHOT = BIT3
};

class camCinematicInVehicleWantedContext : public camBaseCinematicContext
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicInVehicleWantedContext, camBaseCinematicContext);

public:

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	bool IsFlagSet(eTaskDependentShots taskFlag) const { return m_flags.IsFlagSet(taskFlag); }

	//const atVector<const CPed*>& GetPolicePeds() const { return m_PolicePeds; }
	
protected:
	virtual bool IsValid(bool shouldConsiderControlInput = true, bool shouldConsiderViewMode = true) const; 
	
	virtual void PreUpdate(); 

	virtual void ClearContext(); 
	 
	virtual bool CanActivateUsingQuickToggle(); 

	camCinematicInVehicleWantedContext(const camBaseObjectMetadata& metadata);

	const camCinematicInVehicleWantedContextMetadata&	m_Metadata;

private:
	bool IsPlayerWanted() const; 
	
	bool ShouldActivateCinematicMode(bool shouldConsiderControlInput, bool shouldConsiderViewMode) const; 

private:
	
	bool m_ShouldUseBackup;  
	
	fwFlags32 m_flags;

	bool m_WasWanted; 
};

#endif