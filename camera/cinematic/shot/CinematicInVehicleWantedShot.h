//
// camera/cinematic/shot/camInVehicleWantedShot.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef BASE_CINEMATIC_IN_VEHICLE_WANTED_SHOT_H
#define BASE_CINEMATIC_IN_VEHICLE_WANTED_SHOT_H

#include "camera/base/BaseObject.h"
#include "camera/cinematic/shot/BaseCinematicShot.h"

class camCinematicPoliceCarMountedShotMetadata;
class camCinematicPoliceHeliMountedShotMetadata;
class camCinematicPoliceInCoverShotMetadata; 
class camCinematicPoliceExitVehicleShotMetadata; 
class camCinematicPoliceRoadBlockShotMetadata; 

class camPoliceCarMountedShot : public camBaseCinematicShot
{
	DECLARE_RTTI_DERIVED_CLASS(camPoliceCarMountedShot, camBaseCinematicShot);
public:

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.
	
	bool IsValid() const;  

protected:
	virtual void InitCamera(); 
	
	virtual const CEntity* ComputeAttachEntity() const;
	
	virtual u32 GetCameraRef() const; 

	camPoliceCarMountedShot(const camBaseObjectMetadata& metadata);

private:
	bool IsCopChasingInCar(const CPed* pPed) const; 

	const camCinematicPoliceCarMountedShotMetadata&	m_Metadata;
};

class camPoliceHeliMountedShot : public camBaseCinematicShot
{
	DECLARE_RTTI_DERIVED_CLASS(camPoliceHeliMountedShot, camBaseCinematicShot);
public:

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	bool IsValid() const;  

protected:
	virtual void InitCamera(); 
	
	virtual const CEntity* ComputeAttachEntity() const;

	camPoliceHeliMountedShot(const camBaseObjectMetadata& metadata);
	
private:
	bool IsCopInHeli(const CPed* pPed) const;

	const camCinematicPoliceHeliMountedShotMetadata& m_Metadata;
};

class camPoliceInCoverShot : public camBaseCinematicShot
{
	DECLARE_RTTI_DERIVED_CLASS(camPoliceInCoverShot, camBaseCinematicShot);
public:

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	virtual void InitCamera(); 

	camPoliceInCoverShot(const camBaseObjectMetadata& metadata);

private:
	const camCinematicPoliceInCoverShotMetadata&	m_Metadata;

	bool GetHeadingAndPositionOfCover(const CPed* pPed, Vector3 &position, float &heading); 
};

class camPoliceExitVehicleShot : public camBaseCinematicShot
{
	DECLARE_RTTI_DERIVED_CLASS(camPoliceExitVehicleShot, camBaseCinematicShot);
public:

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	virtual void InitCamera(); 
	
	camPoliceExitVehicleShot(const camBaseObjectMetadata& metadata);

private:
	const camCinematicPoliceExitVehicleShotMetadata&	m_Metadata;
};


class camCinematicPoliceRoadBlockShot : public camBaseCinematicShot
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicPoliceRoadBlockShot, camBaseCinematicShot);
public:

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.
	
	virtual bool IsValid() const;  

protected:
	virtual void InitCamera(); 
	
	virtual bool CanCreate();
	
	camCinematicPoliceRoadBlockShot(const camBaseObjectMetadata& metadata);

private:
	bool ComputeHaveRoadBlock(Vector3 &RoadBlockPos); 
	
	Vector3 m_RoadBlockPosition; 
	float MaxVehIcleHeightInRoadBlock; 
	const camCinematicPoliceRoadBlockShotMetadata&	m_Metadata;
#if GTA_REPLAY
	int m_iReplayRecordRoadBlock;
#endif
};


#endif