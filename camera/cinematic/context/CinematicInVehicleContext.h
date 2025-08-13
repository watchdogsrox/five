//
// camera/cinematic/CinematicInVehicleContext.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_IN_VEHICLE_CONTEXT_H
#define CINEMATIC_IN_VEHICLE_CONTEXT_H

#include "camera/cinematic/context/BaseCinematicContext.h"

class camCinematicContextMetadata;
class camBaseCinematicShot; 


class camCinematicInVehicleContext : public camBaseCinematicContext
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicInVehicleContext, camBaseCinematicContext);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	void InvalidateIdleMode();

private:
	void InitActiveCamera(const CEntity* pEnt); 

protected:
	virtual bool Update(); 

	virtual void PreUpdate();

	bool ComputeIsIdle() const;

	virtual bool IsValid(bool shouldConsiderControlInput = true, bool shouldConsiderViewMode = true) const; 
	
	camCinematicInVehicleContext(const camBaseObjectMetadata& metadata);
	
	const camCinematicInVehicleContextMetadata&	m_Metadata;

private:
	u32				m_PreviousIdleModeInvalidationTime;

	float			m_CinematicHeliDesiredPathHeading; 
};

class camCinematicInVehicleConvertibleRoofContext : public camBaseCinematicContext
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicInVehicleConvertibleRoofContext, camBaseCinematicContext);
public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	virtual bool IsValid(bool shouldConsiderControlInput = true,  bool shouldConsiderViewMode = true) const; 

protected:
	camCinematicInVehicleConvertibleRoofContext(const camBaseObjectMetadata& metadata);

	const camCinematicInVehicleConvertibleRoofContextMetadata&	m_Metadata;

};

class camCinematicInTrainContext : public camCinematicInVehicleContext
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicInTrainContext, camCinematicInVehicleContext);
public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	const CTrain* GetTrainPlayerIsIn() const; 

protected:
	virtual bool IsValid(bool shouldConsiderControlInput = true, bool shouldConsiderViewMode = true) const; 

protected:
	camCinematicInTrainContext(const camBaseObjectMetadata& metadata);

	const camCinematicInTrainContextMetadata&	m_Metadata;
};

class camCinematicInTrainAtStationContext : public camCinematicInTrainContext
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicInTrainAtStationContext, camCinematicInTrainContext);
public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.
	
	enum StationState
	{
		LEAVING_STATION,
		ARRIVING_STATION,
		NONE
	};



protected:
	virtual void PreUpdate();
	
	virtual bool IsValid(bool shouldConsiderControlInput = false, bool shouldConsiderViewMode = false) const; 
	
	bool IsTrainLeavingTheStation() const; 
	
	bool IsTrainApproachingStation(const CTrain* Train) const; 
private: 
	
	u32 m_TimeTrainLeftStation; 

	StationState m_stationState;  
	
	bool m_HaveGotTimeTrainLeftStation : 1; 
	bool m_bTrainIsApproachingStation : 1; 
	bool m_bIsTrainLeavingStation : 1; 


protected:
	camCinematicInTrainAtStationContext(const camBaseObjectMetadata& metadata);

	const camCinematicInTrainAtStationContextMetadata&	m_Metadata;
};

class camCinematicInVehicleMultiplayerPassengerContext : public camBaseCinematicContext
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicInVehicleMultiplayerPassengerContext, camBaseCinematicContext);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	virtual bool IsValid(bool shouldConsiderControlInput = true, bool shouldConsiderViewMode = false) const; 

protected:
	virtual bool Update();

	camCinematicInVehicleMultiplayerPassengerContext(const camBaseObjectMetadata& metadata);

	const camCinematicInVehicleMultiplayerPassengerContextMetadata&	m_Metadata;

private:

};


class camCinematicInVehicleCrashContext : public camBaseCinematicContext
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicInVehicleCrashContext, camBaseCinematicContext);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:

	virtual bool IsValid(bool shouldConsiderControlInput = false, bool shouldConsiderViewMode = false) const; 

	camCinematicInVehicleCrashContext(const camBaseObjectMetadata& metadata);

	const camCinematicInVehicleCrashContextMetadata&	m_Metadata;
	
private:
	u32 m_TerminateTimer; 
	bool m_bCanActivate : 1; 
};


#endif
