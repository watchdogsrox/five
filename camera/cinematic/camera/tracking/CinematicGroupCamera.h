//
// camera/cinematic/CinematicGroupCamera.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_GROUP_CAMERA_H
#define CINEMATIC_GROUP_CAMERA_H

#include "camera/cinematic/camera/tracking/BaseCinematicTrackingCamera.h"
#include "camera/system/cameraMetadata.h"
#include "scene/RegdRefTypes.h"
#include "atl/vector.h"
#include "camera/helpers/DampedSpring.h"

class camCinematicGroupCameraMetadata;

class camCinematicGroupCamera : public camBaseCinematicTrackingCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicGroupCamera, camBaseCinematicTrackingCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	void Init(const atVector<RegdConstEnt>& Entities); 

	bool IsValid(); 

	bool Update(); 
	
	bool ShouldUpdate(); 

	void SetEntityList(const atVector<RegdConstEnt>& Entities); 

protected:
	//virtual bool ComputeDesiredLookAtPosition(); 

	camCinematicGroupCamera(const camBaseObjectMetadata& metadata);

private:
	void DampGroupCentrePositionChange(); 	
	
	void DampGroupAverageFrontVector(); 
	
	void DampFov(); 

	void DampOrbitDistanceScalar(); 

	void ComputeGroupCentre(); 

	void ComputeGroupAverageFront(); 

	void ComputeDistanceToFurthestEntityFromGroupCentre(); 

	void ComputeFov(); 

	void ComputeCameraPosition(); 

	void ComputeInitalCameraFront(); 
	
	bool HasCameraCollidedBetweenUpdates(const Vector3& PreviousPosition, const Vector3& currentPosition); 

	bool IsClipping(const Vector3& position, const u32 flags); 
	
	bool HaveLineOfSightToEnoughEntites(); 

private:
	const camCinematicGroupCameraMetadata& m_Metadata;
	
	camDampedSpring m_PositionSpringX;
	camDampedSpring m_PositionSpringY;
	camDampedSpring m_PositionSpringZ;

	camDampedSpring m_FovSpring; 
	camDampedSpring m_DistanceScalarSpring; 
	
	camDampedSpring m_AverageFrontDampX; 
	camDampedSpring m_AverageFrontDampY; 
	camDampedSpring m_AverageFrontDampZ; 
	
	atVector<RegdConstEnt> m_Entities; 

	Vector3 m_RelativeOffsetFromTarget; 
	Vector3 m_GroupCentrePosition; 
	Vector3 m_GroupAverageFront; 
	Vector3 m_InitialRelativeCameraFront; 
	Vector3 m_CameraPos; 

	float m_DistanceToFurthestEntityFromGroupCentre; 
	float m_Fov; 
	float m_CameraOrbitDistanceScalar; 
	
	u32 m_ShotTimeSpentOccluded; 

	bool m_CanUpdate : 1; 

private:
	//Forbid copy construction and assignment.
	camCinematicGroupCamera(const camCinematicGroupCamera& other);
	camCinematicGroupCamera& operator=(const camCinematicGroupCamera& other);
};

#endif // CINEMATIC_GROUP_CAMERA_H