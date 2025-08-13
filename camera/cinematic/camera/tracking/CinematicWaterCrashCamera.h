//
// camera/cinematic/CinematicWaterCrashCamera.h
// 
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_WATER_CRASH_CAMERA_H
#define CINEMATIC_WATER_CRASH_CAMERA_H

#include "Camera/Base/BaseCamera.h"
#include "camera/system/CameraMetadata.h"

class camCinematicWaterCrashCameraMetadata;
class camBaseCinematicContext;

class camCinematicWaterCrashCamera : public camBaseCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicWaterCrashCamera, camBaseCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	bool IsValid();

	bool Update();

	void Init(); 

	void SetContext(camBaseCinematicContext* pContext) { m_pContext = pContext; }

protected:
	camCinematicWaterCrashCamera(const camBaseObjectMetadata& metadata);

private:
	bool UpdateCamera();
	bool ShouldUpdate();

	bool UpdateCollision(Vector3& cameraPosition, const Vector3& lookAtPosition);
	bool ComputeCollision(Vector3& cameraPosition, const Vector3& lookAtPosition, const u32 flags);

	bool UpdateOcclusion(const Vector3& cameraPosition, const Vector3& lookAtPosition);
	bool ComputeOcclusion(const Vector3& cameraPosition, const Vector3& lookAtPosition);

	bool IsClipping(const Vector3& position, const u32 flags);

	Vector3 ComputePositionRelativeToAttachParent();

	void UpdateZOffset();

private:
	const camCinematicWaterCrashCameraMetadata& m_Metadata;

	Vector3 m_CamPosition; 

	camBaseCinematicContext* m_pContext;

	float m_initialZOffset;
	float m_zOffset;
	float m_boundRadius;
	u32 m_ShotTimeSpentOccluded;
	u32 m_StartTime;

	bool m_canUpdate : 1;
	bool m_shouldUseAttachParent : 1;
	bool m_catchUpToGameplay : 1;
};

#endif // CINEMATIC_WATER_CRASH_CAMERA_H