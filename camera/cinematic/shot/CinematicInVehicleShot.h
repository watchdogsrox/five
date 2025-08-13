//
// camera/cinematic/shot/camInVehicleShot.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef BASE_CINEMATIC_IN_VEHICLE_SHOT_H
#define BASE_CINEMATIC_IN_VEHICLE_SHOT_H

#include "camera/base/BaseObject.h"
#include "camera/cinematic/shot/BaseCinematicShot.h"

class camCinematicHeliTrackingShotMetadata;
class camCinematicVehicleOrbitShotMetadata; 
class camCinematicVehicleLowOrbitShotMetadata; 
class camCinematicCameraManShotMetadata;
class camCinematicCraningCameraManShotMetadata;
class camCinematicMountedCamera;
class camCinematicVehicleBonnetShotMetadata; 
class camCinematicVehicleConvertibleRoofShotMetadata; 
class camCinematicInVehicleCrashShotMetadata; 

class camHeliTrackingShot : public camBaseCinematicShot
{
	DECLARE_RTTI_DERIVED_CLASS(camHeliTrackingShot, camBaseCinematicShot);
public:

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.
	
	virtual bool CanCreate(); 

protected:
	virtual void InitCamera(); 
	
	virtual void Update();
	
	camHeliTrackingShot(const camBaseObjectMetadata& metadata);

private:

	u32 m_HeliTrackingAccuracyMode;
	u32 m_DesiredTrackingAccuracyMode; 

private:
	const camCinematicHeliTrackingShotMetadata&	m_Metadata;
};

class camVehicleOrbitShot : public camBaseCinematicShot
{
	DECLARE_RTTI_DERIVED_CLASS(camVehicleOrbitShot, camBaseCinematicShot);
public:

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.
	
	virtual bool CanCreate(); 
protected:
	virtual void InitCamera(); 

	camVehicleOrbitShot(const camBaseObjectMetadata& metadata);


private:
	const camCinematicVehicleOrbitShotMetadata&	m_Metadata;
};

class camVehicleLowOrbitShot : public camBaseCinematicShot
{
	DECLARE_RTTI_DERIVED_CLASS(camVehicleLowOrbitShot, camBaseCinematicShot);
public:

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	virtual bool CanCreate(); 
protected:
	virtual void InitCamera(); 

	camVehicleLowOrbitShot(const camBaseObjectMetadata& metadata);

	virtual bool IsValid() const;

private:
	const camCinematicVehicleLowOrbitShotMetadata&	m_Metadata;
};



class camCameraManShot : public camBaseCinematicShot
{
	DECLARE_RTTI_DERIVED_CLASS(camCameraManShot, camBaseCinematicShot);
public:

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	virtual bool IsValid() const;

protected:
	virtual void InitCamera(); 
	
	virtual bool CanCreate();

	camCameraManShot(const camBaseObjectMetadata& metadata);

private:
	const camCinematicCameraManShotMetadata&	m_Metadata;
};


class camCraningCameraManShot : public camCameraManShot
{
	DECLARE_RTTI_DERIVED_CLASS(camCraningCameraManShot, camBaseCinematicShot);
public:

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	virtual bool IsValid() const;

	virtual void PreUpdate(bool IsCurrentShot);

protected:
	virtual void InitCamera(); 

	virtual bool CanCreate();

	camCraningCameraManShot(const camBaseObjectMetadata& metadata);

private:
	const camCinematicCraningCameraManShotMetadata&	m_Metadata;

	u32 m_TimeSinceVehicleMovedForward;
};


#define MAX_CINEMATIC_MOUNTED_PART_HISTORY	4
class camCinematicVehiclePartShot : public camBaseCinematicShot
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicVehiclePartShot, camBaseCinematicShot);
public:

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	virtual bool IsValid() const;   
	
	virtual void PreUpdate(bool IsCurrentShot); 

protected:
	virtual void InitCamera(); 
	
	camCinematicVehiclePartShot(const camBaseObjectMetadata& metadata);

	virtual u32 GetCameraRef() const { return m_CameraHash; }
	virtual bool CreateAndTestCamera(u32 hash);

	void	AddToHistory(u32 camera, u32 maxHistory);
	bool	IsInHistoryList(u32 camera, u32 maxHistory);
	bool	IsNonAircraftVehicleInTheAir(const CVehicle* pVehicle); 
private:
	const camCinematicVehiclePartShotMetadata&	m_Metadata;

	u32		m_HistoryList[MAX_CINEMATIC_MOUNTED_PART_HISTORY];
	u32		m_HistoryInsertIndex;
	u32		m_CameraHash;	
	u32		m_InAirTimer; 
	bool	m_IsValid; 
};

class camCinematicVehicleBonnetShot : public camBaseCinematicShot
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicVehicleBonnetShot, camBaseCinematicShot);
public:

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	virtual bool IsValid() const;   

	bool HasSeatChanged() const; 

protected:
	virtual bool CanCreate(); 
	
	virtual void InitCamera(); 

	u32 GetCameraRef() const;  

	virtual void PreUpdate(bool sCurrentShot); 

	virtual void ClearShot(bool ResetCameraEndTime = false); 

	camCinematicVehicleBonnetShot(const camBaseObjectMetadata& metadata);

private:
	bool ComputeShouldAdjustForRollCage() const;
	void GetSeatSpecificCameraOffset(Vector3& cameraOffset) const;
	bool ComputeShouldTerminateForLookBehind(const camCinematicMountedCamera* mountedCamera = NULL) const;

	const camCinematicVehicleBonnetShotMetadata&	m_Metadata;

	s32 m_SeatIndex;

	bool m_IsRenderingPOVcamera; 
};


class camCinematicVehicleConvertibleRoofShot : public camBaseCinematicShot
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicVehicleConvertibleRoofShot, camBaseCinematicShot);
public:
	template <class _T> friend class camFactoryHelper; 
	
	bool IsValid() const;

	virtual void PreUpdate(bool OnlyUpdateIfCurrentShot); 

protected:
	virtual void InitCamera(); 

	virtual bool CanCreate(); 

	camCinematicVehicleConvertibleRoofShot(const camBaseObjectMetadata& metadata);
	
private:
	s32 m_InitialRoofState;
	bool m_IsReset :1;
	bool m_CanActivate :1;
	bool m_ValidForVelocity :1;
	bool m_CanCreate :1; 
private:
	const camCinematicVehicleConvertibleRoofShotMetadata&	m_Metadata;
};


class camCinematicInVehicleCrashShot : public camBaseCinematicShot
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicInVehicleCrashShot, camBaseCinematicShot);
public:

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.
	
	bool IsValid() const;
	
	virtual void PreUpdate(bool updateIfCurrentShot); 

protected:
	
	bool CanCreate(); 
	
	virtual void ClearShot(bool ResetCameraEndTime = false); 
	
	virtual void InitCamera(); 

	camCinematicInVehicleCrashShot(const camBaseObjectMetadata& metadata);

private:
	const camCinematicInVehicleCrashShotMetadata&	m_Metadata;
	
	u32 m_TerminateTimer; 
	u32 m_WheelOnGroundTimer; 
	u32 m_MinTimeBetweenShots; 
	u32 m_ShotValidTimer; 
	bool m_IsValid : 1;
};

#endif
