//
//camera/cinematic/CinematicVehicleTrackingCamera.h
//
//Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_VEHICLE_TRACKING_CAMERA_H
#define CINEMATIC_VEHICLE_TRACKING_CAMERA_H

#include "Camera/Base/BaseCamera.h"
#include "camera/cinematic/cinematicdirector.h"
#include "vehicleAi\task\TaskVehicleAnimation.h"

class camCinematicVehicleTrackingCameraMetadata;

class camCinematicVehicleTrackingCamera : public camBaseCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicVehicleTrackingCamera, camBaseCamera);
	
public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.
	
	bool IsValid(); 

	bool Update(); 
	
	void Init(); 

	static void  ComputeWorldSpacePositionRelativeToVehicleBoneWithOffset(const CVehicle* pVehicle, s32 BoneIndex, const Vector3& LocalOffset, Vector3& WorldPos); 
	static bool	 ComputeWorldSpaceOrientationRelativeToVehicleBone(const CVehicle* pVehicle, s32 BoneIndex, Quaternion& WorldOrientation, bool UseXAxis, bool UseYAxis, bool UseZAxis); 

	static void	ComputeRelativePostionToVehicleBoneFromWorldSpacePosition(const CVehicle* pVehicle, s32 BoneIndex, const Vector3& WorldPos, Vector3& LocalOffset); 
protected:
	
	camCinematicDirector::eSideOfTheActionLine ComputeSideOfAction(); 

	bool ComputeStartAndEndPositions(camCinematicDirector::eSideOfTheActionLine sideOfThelineOfAction); 

	bool ComputeLookAtPosition(camCinematicDirector::eSideOfTheActionLine sideOfThelineOfAction); 

	void UpdatePhase(); 
	
	bool HasRoofChangedState() const; 

	bool UpdateCollision(Vector3& cameraPosition, const Vector3& lookAtPosition); 

	bool ComputeCollision(Vector3& cameraPosition, const Vector3& lookAtPosition, const u32 flags);

	bool IsClipping(const Vector3& position, const u32 flags); 

	bool IsHoldAtEndTimeUp(); 
protected:
	camCinematicVehicleTrackingCamera(const camBaseObjectMetadata& metadata);

private:
	Vector3 m_startPos; 
	Vector3 m_endPos; 
	Vector3 m_startLookAtPosition; 
	Vector3 m_endLookAtPosition; 
	
	float m_phase; 

	s32 m_IntitalState; 
	u32 m_HoldAtEndTime; 
	bool m_canUpdate; 


private:
	const camCinematicVehicleTrackingCameraMetadata& m_Metadata;

private:
	//Forbid copy construction and assignment.
	 camCinematicVehicleTrackingCamera(const  camCinematicVehicleTrackingCamera& other);
	 camCinematicVehicleTrackingCamera& operator=(const  camCinematicVehicleTrackingCamera& other);
};
#endif