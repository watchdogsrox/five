//
// camera/cinematic/CinematicMountedPartCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_MOUNTED_PART_CAMERA_H
#define CINEMATIC_MOUNTED_PART_CAMERA_H

#include "camera/base/BaseCamera.h"
#include "camera/helpers/DampedSpring.h"
#include "camera/system/CameraMetadata.h"
#include "renderer/HierarchyIds.h"

class camCinematicMountedCameraPartMetadata;
class camControlHelper;
class camEnvelope;
class camSpringMount;
class CVehicle;

//A cinematic camera that can be mounted at relative to a vehicle part.
class camCinematicMountedPartCamera : public camBaseCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicMountedPartCamera, camBaseCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

private:
	camCinematicMountedPartCamera(const camBaseObjectMetadata& metadata);

public:
	~camCinematicMountedPartCamera();

	bool			IsValid(); 
	bool			IsCameraInsideVehicle() const;
	bool			IsCameraForcingMotionBlur() const;
	bool			ShouldDisplayReticule() const;
	bool			ShouldMakeFollowPedHeadInvisible() const;
	bool			IsValidForReversing() const; 

	static eHierarchyId  ComputeVehicleHieracahyId(eCinematicVehicleAttach CinematicAttachPart); 

private:
	virtual bool	Update();
	void			ComputeAttachMatrix(Matrix34& vehicleMatrix, Matrix34& attachMatrix, Vector3& offsetFromVehicleToPart);
	void			UpdateOrientation(const CPhysical& attachPhysical, const Matrix34& vehicleMatrix, const Matrix34& attachMatrix,
									  const Vector3& cameraPosition, Vector3& lookAtPosition, const Vector3& offsetFromVehicleToPart,
									  bool useWorldUp);

	void			UpdateFov();
	void			UpdateHighSpeedShake(const CVehicle& attachVehicle);
	bool			IsClipping(const Vector3& position, const Vector3& lookAt, const u32 flags);
	bool			ShouldTerminateForSwingingDoors(CVehicle* pVehicle, eHierarchyId eAttachPart); 

	const camCinematicMountedPartCameraMetadata& m_Metadata;

	camDampedSpring m_HighSpeedShakeSpring;
	camDampedSpring m_RelativeAttachPositionSprings[3];
	camSpringMount* m_SpringMount;

	Vector3 m_PositionOffset;
	Vector3 m_LookAtOffset;

	bool m_canUpdate : 1;

	//Forbid copy construction and assignment.
	camCinematicMountedPartCamera(const camCinematicMountedPartCamera& other);
	camCinematicMountedPartCamera& operator=(const camCinematicMountedPartCamera& other);
};

#endif // CINEMATIC_MOUNTED_PART_CAMERA_H
