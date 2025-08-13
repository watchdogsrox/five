//
// camera/cinematic/shot/camInVehicleShot.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef BASE_CINEMATIC_IN_VEHICLE_FORMATION_SHOT_H
#define BASE_CINEMATIC_IN_VEHICLE_FORMATION_SHOT_H

#include "camera/base/BaseObject.h"
#include "camera/cinematic/shot/BaseCinematicShot.h"

class camCinematicVehicleGroupShotMetadata;

class camCinematicVehicleGroupShot : public camBaseCinematicShot
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicVehicleGroupShot, camBaseCinematicShot);
public:

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	virtual bool CanCreate(); 
	
	virtual void PreUpdate(bool IsCurrentShot); 

	virtual bool IsValid() const; 

protected:

	virtual void InitCamera(); 
	 
	virtual void Update(); 

	bool HaveEnoughEntitiesInGroup() const; 

	bool IsPedValidForGroup(const CPed* pPed) const; 
	
	void GetPlayerPositions(); 

	camCinematicVehicleGroupShot(const camBaseObjectMetadata& metadata);

private:
	const camCinematicVehicleGroupShotMetadata&	m_Metadata;

	atVector<RegdConstEnt> m_TargetEntities; 
};

#endif