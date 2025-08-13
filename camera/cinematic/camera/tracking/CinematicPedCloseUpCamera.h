//
// camera/cinematic/CinematicPedCloseUpCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_PED_CLOSE_UP_CAMERA_H
#define CINEMATIC_PED_CLOSE_UP_CAMERA_H

#include "camera/base/BaseCamera.h"
#include "camera/helpers/DampedSpring.h"

class camCinematicPedCloseUpCameraMetadata;

//A cinematic camera that provides a close-up view of a target ped.
class camCinematicPedCloseUpCamera : public camBaseCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicPedCloseUpCamera, camBaseCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

private:
	camCinematicPedCloseUpCamera(const camBaseObjectMetadata& metadata);

public:
	bool			IsValid();

private:
	virtual bool	Update();
	void			ComputeAttachPosition(Vector3& attachPosition) const;
	void			ComputeLookAtPosition(Vector3& lookAtPosition) const;
	bool			UpdateCameraPosition(const Vector3& attachPosition, const Vector3& lookAtPosition);
	bool			ComputeIsCameraClipping() const;
	void			UpdateCameraOrientation(const Vector3& attachPosition, const Vector3& lookAtPosition);

	bool			UpdateOcclusion(const Vector3& cameraPosition, const Vector3& lookAtPosition); 
	bool			ComputeOcclusion(const Vector3& cameraPosition, const Vector3& lookAtPosition); 
	bool			IsTargetTooFarAway(const Vector3& cameraPosition, const Vector3& lookAtPosition);

	const camCinematicPedCloseUpCameraMetadata& m_Metadata;

	camDampedSpring	m_LookAtHeadingAlignmentSpring;
	camDampedSpring	m_LookAtPitchAlignmentSpring;
	camDampedSpring m_LeadingLookSpring; 

	bool			m_IsFramingValid;
	u32				m_ShotTimeSpentOccluded; 
	//Forbid copy construction and assignment.
	camCinematicPedCloseUpCamera(const camCinematicPedCloseUpCamera& other);
	camCinematicPedCloseUpCamera& operator=(const camCinematicPedCloseUpCamera& other);
};

#endif // CINEMATIC_PED_CLOSE_UP_CAMERA_H
