#ifndef BASE_CINEMATIC_TRAIN_SHOT_H
#define BASE_CINEMATIC_TRAIN_SHOT_H

#include "camera/base/BaseObject.h"
#include "camera/cinematic/shot/CinematicInVehicleShot.h"

//class camCinematicTrainPassengerShotMetadata;
//class camCinematicTrainStationShotMetadata;
//class camCinematicTrainRoofMountedShotMetadata;
//class camCinematicTrainTrackShotMetadata; 

class camCinematicTrainStationShot : public camBaseCinematicShot
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicTrainStationShot, camBaseCinematicShot);
public:

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	

protected:
	virtual bool CanCreate();

	virtual void InitCamera(); 

	virtual const CEntity* ComputeAttachEntity() const; 

	camCinematicTrainStationShot(const camBaseObjectMetadata& metadata);


private:
	const camCinematicTrainStationShotMetadata&	m_Metadata;
};

class camCinematicTrainRoofShot : public camBaseCinematicShot
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicTrainRoofShot, camBaseCinematicShot);
public:

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	virtual bool CanCreate();

	virtual void InitCamera(); 
	
	virtual const CEntity* ComputeAttachEntity() const;

	camCinematicTrainRoofShot(const camBaseObjectMetadata& metadata);


private:
	const camCinematicTrainRoofMountedShotMetadata&	m_Metadata;
};


class camCinematicPassengerShot : public camBaseCinematicShot
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicPassengerShot, camBaseCinematicShot);
public:

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	virtual void InitCamera(); 
	
	virtual const CEntity* ComputeAttachEntity() const;
	
	camCinematicPassengerShot(const camBaseObjectMetadata& metadata);
	
private:
	const camCinematicTrainPassengerShotMetadata&	m_Metadata;
};

class camCinematicTrainTrackShot : public camBaseCinematicShot
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicTrainTrackShot, camBaseCinematicShot);
public:

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	virtual void InitCamera(); 
	
	virtual const CEntity* ComputeLookAtEntity() const;

	camCinematicTrainTrackShot(const camBaseObjectMetadata& metadata);

private:
	const camCinematicTrainTrackShotMetadata&	m_Metadata;
};




//class camCinematicPoliceCarMountedShot : public camCinematicInVehicleShot
//{
//	DECLARE_RTTI_DERIVED_CLASS(camCinematicPoliceCarMountedShot, camCinematicInVehicleShot);
//public:
//
//	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.
//
//	bool IsValid(const CPed* ped, const CVehicle* vehicle) const;
//
//	void InitCamera(); 
//protected:
//	camCinematicPoliceCarMountedShot(const camBaseObjectMetadata& metadata);
//
//
//private:
//	const camCinematicPoliceCarMountedShotMetadata&	m_Metadata;
//};
//
//class camCinematicPolicePoliceHeliMountedShot : public camCinematicInVehicleShot
//{
//	DECLARE_RTTI_DERIVED_CLASS(camCinematicPolicePoliceHeliMountedShot, camCinematicInVehicleShot);
//public:
//
//	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.
//
//	bool IsValid(const CPed* ped, const CVehicle* vehicle) const;
//
//	void InitCamera(); 
//protected:
//	camCinematicPolicePoliceHeliMountedShot(const camBaseObjectMetadata& metadata);
//
//
//private:
//	const camCinematicPoliceHeliMountedShotMetadata&	m_Metadata;
//};
//
//
//class camCinematicHeliChaseShot : public camCinematicInVehicleShot
//{
//	DECLARE_RTTI_DERIVED_CLASS(camCinematicPolicePoliceHeliMountedShot, camCinematicInVehicleShot);
//public:
//
//	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.
//
//	bool IsValid(const CPed* ped, const CVehicle* vehicle) const;
//
//	void InitCamera(); 
//protected:
//	camCinematicPolicePoliceHeliMountedShot(const camBaseObjectMetadata& metadata);
//
//
//private:
//	const camCinematicPoliceHeliMountedShotMetadata&	m_Metadata;
//};


#endif


