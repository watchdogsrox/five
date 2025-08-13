//
// camera/cinematic/CinematicTwoShotCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_TWO_SHOT_CAMERA_H
#define CINEMATIC_TWO_SHOT_CAMERA_H

#include "camera/base/BaseCamera.h"
#include "camera/helpers/DampedSpring.h"

class camCinematicTwoShotCameraMetadata;

//A cinematic camera that provides a close-up view of a target ped.
class camCinematicTwoShotCamera : public camBaseCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicTwoShotCamera, camBaseCamera);

public:
	template <class _T> friend class camFactoryHelper;//Allow the factory helper class to access the protected constructor.

private:
	camCinematicTwoShotCamera(const camBaseObjectMetadata& metadata);

public:
	bool			IsValid();
	void			SetAttachEntityPosition(const Vector3& newPosition);
	void			SetCameraIsOnRight(bool isOnRight) { m_IsCameraOnRight = isOnRight;}
#if __BANK
	static void		AddWidgets(bkBank &bank);
#endif // __BANK

private:
	virtual bool	Update();
	void			ComputeLookAtPosition(Vector3& lookAtPosition) const;
	bool			ComputeIsTargetTooClose(const Vector3& basePivotPosition, const Vector3& lookAtPosition) const;
	void			ComputeOrbitOrientation(const Vector3& cameraPosition, const Vector3& lookAtPosition, const bool shouldComputeOrbitRelativePivotOffset, float& orbitHeading, float& orbitPitch) const;
	void			ComputePivotPosition(const Matrix34& orbitMatrix, Vector3& pivotPosition) const;
	void			ComputeOrbitRelativePivotOffset(Vector3& orbitRelativeOffset) const;
	bool			ComputeCameraPosition(const Vector3& basePivotPosition, const float orbitHeading, const float orbitPitch, Vector3& cameraPosition);
	bool			ComputeOcclusion(const Vector3& cameraPosition, const Vector3& lookAtPosition) const;
	bool			ComputeIsCameraClipping(const Vector3& cameraPosition, const Vector3* newCameraPosition = NULL) const;
	float			ComputeFov(const float fovLastUpdate) const;

	const camCinematicTwoShotCameraMetadata& m_Metadata;

	camDampedSpring	m_AttachEntityPositionSpringX;
	camDampedSpring	m_AttachEntityPositionSpringY;
	camDampedSpring	m_AttachEntityPositionSpringZ;
	camDampedSpring m_CameraPositionSpringZ;
	bool			m_IsFramingValid;
	bool			m_IsCameraOnRight;
	bool			m_LockCameraPosition;

#if __BANK
	static bool		ms_DebugRender;
#endif // __BANK

	//Forbid copy construction and assignment.
	camCinematicTwoShotCamera(const camCinematicTwoShotCamera& other);
	camCinematicTwoShotCamera& operator=(const camCinematicTwoShotCamera& other);
};

#endif // CINEMATIC_TWO_SHOT_CAMERA_H
