//
// camera/replay/ReplayRecordedCamera.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef REPLAY_RECORDED_CAMERA_H
#define REPLAY_RECORDED_CAMERA_H

#include "camera/replay/ReplayBaseCamera.h"

#if GTA_REPLAY

#include "scene/RegdRefTypes.h"

class camReplayRecordedCameraMetadata;

class camReplayRecordedCamera : public camReplayBaseCamera
{
	DECLARE_RTTI_DERIVED_CLASS(camReplayRecordedCamera, camReplayBaseCamera);

public:
	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

protected:
	camReplayRecordedCamera(const camBaseObjectMetadata& metadata);

	virtual bool Update();

	// We override this as we do *NOT* want to update the dof setting during a replay playback. The settings are read
	// from the replay file and should not be altered.
	virtual void UpdateDof();

	const camReplayRecordedCameraMetadata& m_Metadata;


private:
	//Forbid copy construction and assignment.
	camReplayRecordedCamera(const camReplayRecordedCamera& other);
	camReplayRecordedCamera& operator=(const camReplayRecordedCamera& other);
};

#endif // GTA_REPLAY

#endif // REPLAY_RECORDED_CAMERA_H
