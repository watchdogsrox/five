//
// camera/cinematic/CinematicAnimatedCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_ANIMATED_CAMERA_H
#define CINEMATIC_ANIMATED_CAMERA_H

#include "camera/cutscene/AnimatedCamera.h"

class camCinematicAnimatedCameraMetadata;
class CEntity;
class CActionResult;
class CTaskMelee;

const float g_CinematicAnimatedCamCollisionRadius = 0.3f;

class camCinematicAnimatedCamera : public camAnimatedCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicAnimatedCamera, camAnimatedCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.
	
	enum eSideOfTheActionLine
	{
		LEFT_HAND_SIDE,
		RIGHT_HAND_SIDE
	};

protected:
	camCinematicAnimatedCamera(const camBaseObjectMetadata& metadata);

public:
	bool IsValid();

	bool Update(); 

	static u32 ComputeValidCameraAnimId(CEntity* pTargetPed, const CActionResult* pActionResult); 

	static void AddEntityToExceptionList(const CPed* pPed, atArray<const phInst*> &excludeInstances); 

protected:

	const camCinematicAnimatedCameraMetadata& m_Metadata;

private:
	
	static u32 ComputeValidCameraId(const CPed* pTargetPed, eSideOfTheActionLine sideOfTheActionLine,  const CActionResult* pActionResult); 
	
	static void ComputeLookAtPosition(const CPed* pTargetPed, Vector3& lookAtPosition); 

	static bool ComputeIsCameraPositionValid(const CPed* pTargetPed, const Vector3& lookAtPosition, const Vector3& cameraPosition); 

	static bool ComputeIsCameraClipping(const Vector3& cameraPosition); 

	static Vector3 ComputeCameraPositionFromAnim(fwClipSet* pClipSet, fwMvClipId ClipId, CTaskMelee* pMeleeTask); 

	bool m_IsClipping; 

	//Forbid copy construction and assignment.
	camCinematicAnimatedCamera(const camCinematicAnimatedCamera& other);
	camCinematicAnimatedCamera& operator=(const camCinematicAnimatedCamera& other);
};

#endif // CINEMATIC_ANIMATED_CAMERA_H