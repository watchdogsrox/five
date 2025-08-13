/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : VideoFileDataEnumerator.h
// PURPOSE : Async enumeration of video files
//
// AUTHOR  : andrew.keeble
//
// Copyright (C) 1999-2015 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "control/replay/ReplaySettings.h"
#include "video/VideoPlaybackSettings.h"

#if RSG_ORBIS

#if defined( GTA_REPLAY ) && GTA_REPLAY

#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED

#ifndef VIDEO_FILE_DATA_ENUMERATOR_H_
#define VIDEO_FILE_DATA_ENUMERATOR_H_

#include "atl/array.h"
#include "system/ipc.h"

#include "control/replay/File/device_replay.h"
#include "video/contentSearch_Orbis.h"

class CFileData;
class CFileDataVideo;

typedef atArray<CFileDataVideo> VideoFileStorage;

class CVideoFileDataEnumerator
{
public:
	CVideoFileDataEnumerator();
	~CVideoFileDataEnumerator();

	void StartEnumeration(FileDataStorage* m_fileData, VideoFileStorage* m_videoData, const u64 activeUserId);
	bool CheckEnumeration(bool& result) const;

	void StartThread();
	void StopThread();

	void ProcessVideoEnumerationThread();
private:
	u32 BuildVideoList(const u32 offset, const u32 limit);
	void ClearVideoList();

	bool IsVideo(u32 index);
	bool IsVideoByUser(u32 index, s32 user);
	const char* GetTitle(u32 index);
	const char* GetFilename(u32 index);
	const char* GetThumbnailPath(u32 index);
	u64 GetFileSizeByIndex(u32 index);
	bool GetMetadata(u32 index, u64* lastUpdated, u32* duration, u32 maxStringLength, char* subtitle);

	SceContentSearchContentInfo		*sm_infos;

	sysIpcThreadId					sm_threadID;
	sysIpcSema						sm_triggerRequest;
	sysIpcSema						sm_hasFinishedRequest;

	FileDataStorage					*sm_fileData; 
	VideoFileStorage				*sm_videoData;
	u64								sm_activeUserId;

	bool							sm_running;
	bool							sm_result;

	bool							sm_haltThread;
};

#endif // VIDEO_FILE_DATA_ENUMERATOR_H_

#endif // defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED

#endif // defined( GTA_REPLAY ) && GTA_REPLAY

#endif // RSG_ORBIS