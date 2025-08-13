//
// camera/replay/ReplayRecordedCamera.cpp
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#include "camera/replay/ReplayRecordedCamera.h"

#if GTA_REPLAY

#include "camera/CamInterface.h"
#include "camera/system/CameraMetadata.h"
#include "control/replay/replay.h"
#include "frontend/VideoEditor/VideoEditorInterface.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camReplayRecordedCamera,0xA5DB40AE)


camReplayRecordedCamera::camReplayRecordedCamera(const camBaseObjectMetadata& metadata)
: camReplayBaseCamera(metadata)
, m_Metadata(static_cast<const camReplayRecordedCameraMetadata&>(metadata))
{
}

bool camReplayRecordedCamera::Update()
{
	camReplayBaseCamera::Update();

	// Always assume we are playing the recorded camera.
	const replayCamFrame& replayFrame = CReplayMgr::GetReplayFrameData().m_cameraData; 
	m_Frame.CloneFrom(replayFrame.m_Frame);
	m_AttachParent = replayFrame.m_Target;
	if(!CVideoEditorInterface::ShouldShowLoadingScreen())
	{
		camInterface::SetFader(replayFrame.m_Fader);
	}
	return true;
}

// We override this as we do *NOT* want to update the dof setting during a replay playback. The settings are read
// from the replay file and should not be altered, unless the player has chosen any non-default dof settings.
void camReplayRecordedCamera::UpdateDof()
{
	const IReplayMarkerStorage* markerStorage = CReplayMarkerContext::GetMarkerStorage();
	if (!markerStorage)
	{
		return;
	}

	const sReplayMarkerInfo* markerInfo = CReplayMarkerContext::GetMarkerStorage()->TryGetMarker(m_MarkerIndex);
	if (!markerInfo)
	{
		return;
	}

	const u8 focusMode	= markerInfo->GetFocusMode();
	const u8 dofMode	= markerInfo->GetDofMode();
	
	if (focusMode != DEFAULT_MARKER_DOF_FOCUS || dofMode != DEFAULT_MARKER_DOF_MODE)
	{
		camReplayBaseCamera::UpdateDof();
	}
}

#endif // GTA_REPLAY
