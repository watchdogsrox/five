//
// camera/cinematic/CinematicIdleCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_IDLE_CAMERA_H
#define CINEMATIC_IDLE_CAMERA_H

#include "camera/base/BaseCamera.h"

class camCinematicIdleCameraMetadata;
class CEntity;

class camCinematicIdleCamera : public camBaseCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicIdleCamera, camBaseCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

	enum ShotType
	{
		WIDE_SHOT = 0,
		MEDIUM_SHOT,
		CLOSE_UP_SHOT,
		NUM_SHOT_TYPES
	};

protected:
	camCinematicIdleCamera(const camBaseObjectMetadata& metadata);

public:
	bool IsValid();

	bool Update(); 

	void Init(const camFrame& frame, const CEntity* target); 

protected:

	const camCinematicIdleCameraMetadata& m_Metadata;

private:
	bool CutToNewShot(const camFrame& Frame); 
	
	bool ComputeOcclusion(const Vector3& vLookat, const Vector3& vCamPosition); 

	bool ComputeDesiredLookAtPosition(); 

	u32 m_ShotType;
	u32 m_ShotEndTime;
	u32 m_ShotTimeSpentOccluded;
	bool m_IsActive; 
	Vector3 m_DesiredLookAtPosition; 

	//Forbid copy construction and assignment.
	camCinematicIdleCamera(const camCinematicIdleCamera& other);
	camCinematicIdleCamera& operator=(const camCinematicIdleCamera& other);
};

#endif // CINEMATIC_IDLE_CAMERA_H