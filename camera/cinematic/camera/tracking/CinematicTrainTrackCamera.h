//
// camera/cinematic/CinematicCamManCamera.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CINEMATIC_TRAIN_TRACK_CAMERA_H
#define CINEMATIC_TRAIN_TRACK_CAMERA_H

#include "camera/cinematic/camera/tracking/CinematicCamManCamera.h"

class camCinematicTrainTrackingCameraMetadata;

const u32 g_MaxTrainNodes	= 231;
const u32 g_NumTrainTracks	= 3;


//A cinematic camera that tracks a train from one of a set of fixed positions defined in data.
class camCinematicTrainTrackCamera : public camCinematicCamManCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camCinematicTrainTrackCamera, camCinematicCamManCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

private:
	camCinematicTrainTrackCamera(const camBaseObjectMetadata& metadata);

public:

	enum eTrainTrackingCameraModes
	{
		T_TRACKING,
		T_FIXED,
		T_NUM_MODES
	};

	bool			IsValid();
	
	virtual void	SetMode(s32 mode) { m_RequestedShotMode = mode; }
	
	virtual u32		GetNumOfModes() const { return T_NUM_MODES; }

private:
	virtual bool	Update();
	virtual bool	ComputeIsPositionValid();
	bool			UpdateShotOfType(); 
	bool			ComputeCameraTrackPosition(CTrain* engine, Vector3& Position); 
	void			ComputeLookAtPos(CTrain* engine, Vector3 &NewLookAt, const Vector3& vOffsetVector); 

	const camCinematicTrainTrackingCameraMetadata& m_Metadata;
	Vector3			m_CameraPosition; 
	Vector3			m_LookAtPosition; 
	Matrix34		m_BaseCamMatrix;  
	s32				m_RequestedShotMode; 

	//Forbid copy construction and assignment.
	camCinematicTrainTrackCamera(const camCinematicTrainTrackCamera& other);
	camCinematicTrainTrackCamera& operator=(const camCinematicTrainTrackCamera& other);
};

#endif // CINEMATIC_TRAIN_TRACK_CAMERA_H
