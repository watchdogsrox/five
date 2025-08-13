/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : VideoFileDataEnumerator.cpp
// PURPOSE : Async enumeration of video files
//
// AUTHOR  : andrew.keeble
//
// Copyright (C) 1999-2015 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "VideoFileDataEnumerator.h"

#if RSG_ORBIS

#if defined( GTA_REPLAY ) && GTA_REPLAY

#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED

#include "Frontend/ui_channel.h"
#include "rline/rlnp.h"
#include "Network/Live/livemanager.h"
#include "control/replay/file/FileData.h"
#include "control/replay/file/ReplayFileManager.h"


CVideoFileDataEnumerator::CVideoFileDataEnumerator()
:sm_infos(NULL)
,sm_threadID(sysIpcThreadIdInvalid)
,sm_triggerRequest(NULL)
,sm_hasFinishedRequest(NULL)
,sm_fileData(NULL) 
,sm_videoData(NULL)
,sm_activeUserId(0)
,sm_running(false)
,sm_result(false)
,sm_haltThread(false)
{

}

CVideoFileDataEnumerator::~CVideoFileDataEnumerator()
{
	StopThread();
	ClearVideoList();
}

void CVideoFileDataEnumerator::StartEnumeration(FileDataStorage* fileData, VideoFileStorage* videoData, const u64 activeUserId)
{
	sm_running = true;
	sm_result = false;

	sm_fileData = fileData; 
	sm_videoData = videoData;

	sm_activeUserId = activeUserId;

	ClearVideoList();

	sysIpcSignalSema(sm_triggerRequest);
}

bool CVideoFileDataEnumerator::CheckEnumeration(bool& result) const
{
	result = !sm_running && sm_result;
	return result;
}

DECLARE_THREAD_FUNC(VideoEnumerationThread)
{
	CVideoFileDataEnumerator* videoDataEnumerator = static_cast<CVideoFileDataEnumerator*>(ptr);
	Assertf(videoDataEnumerator, "CVideoFileDataEnumerator not passed in to run it's thread");
	if (videoDataEnumerator)
	{
		videoDataEnumerator->ProcessVideoEnumerationThread();
	}
}

void CVideoFileDataEnumerator::ProcessVideoEnumerationThread()
{
	while( sm_haltThread == false )
	{
		sysIpcWaitSema(sm_triggerRequest);

		if (sm_running)
		{
			const s64 maxFilesToCheckFor = 10;
			const u32 maxNoofFilesToCheck = sm_fileData->max_size(); // huge
			uiDisplayf("CVideoFileDataEnumerator::ProcessThread - Start Enumeration - maxNoofFilesToCheck %d", maxNoofFilesToCheck);
			s64 offset = 0;
			// need to go through in chunks for memory reasons
			while (BuildVideoList(offset, maxFilesToCheckFor) == maxFilesToCheckFor && offset < maxNoofFilesToCheck )
			{
				offset += maxFilesToCheckFor;
			}

			uiDisplayf("CVideoFileDataEnumerator::ProcessThread - End Enumeration - Nooffiles %d", sm_fileData->size());

			sm_result = true;
			sm_running = false;

			ClearVideoList();
		}
	}

	sysIpcSignalSema(sm_hasFinishedRequest);	
}

void CVideoFileDataEnumerator::StartThread()
{
	sm_haltThread = false;

	//create the semaphores
	sm_triggerRequest = sysIpcCreateSema(0);
	sm_hasFinishedRequest = sysIpcCreateSema(0);

	//create the thread
	if(sm_threadID == sysIpcThreadIdInvalid)
	{
		// use same core as ReplayFileManager. should be a good and safe place to put this
		const int primaryThreadCore = 3;

		sm_threadID = sysIpcCreateThread(VideoEnumerationThread, this, sysIpcMinThreadStackSize, PRIO_LOWEST, "VideoFileDataEnumerator Thread", primaryThreadCore, "VideoFileDataEnumerator");
		uiAssertf(sm_threadID != sysIpcThreadIdInvalid, "Could not create the VideoFileDataEnumerator thread, out of memory?");
	}
}

void CVideoFileDataEnumerator::StopThread()
{
	//stop and remove the thread
	if(sm_threadID != sysIpcThreadIdInvalid)
	{
		sm_haltThread = true;

		if(!sm_running)
		{
			sysIpcSignalSema(sm_triggerRequest);
		}

		//wait to make sure the IO thread has finished its business
		sysIpcWaitSema(sm_hasFinishedRequest);

		sysIpcWaitThreadExit(sm_threadID);
		sm_threadID = sysIpcThreadIdInvalid;
	}

	//tidy up the semaphores
	if( sm_triggerRequest )
	{
		sysIpcDeleteSema(sm_triggerRequest);
		sm_triggerRequest = NULL;
	}

	if( sm_hasFinishedRequest )
	{
		sysIpcDeleteSema(sm_hasFinishedRequest); 
		sm_hasFinishedRequest = NULL;
	}
}


u32 CVideoFileDataEnumerator::BuildVideoList(const u32 offset, const u32 limit)
{
	SceUserServiceUserId currentUserId = ReplayFileManager::GetCurrentUserId();

	const u32 maxStringLength = 512;
	char subtitle[maxStringLength] = { 0 };

	u32 noofFiles = 0;
	uiAssertf(!sm_infos, "CVideoFileDataEnumerator::BuildVideoList - sm_infos video list has not been cleared before next get");

	sm_infos = rage_new SceContentSearchContentInfo[limit];
	if ( uiVerifyf(sm_infos, "CVideoFileDataEnumerator::BuildVideoList - sm_infos video list could not be assigned memory") )
	{
		memset(sm_infos, 0, sizeof(SceContentSearchContentInfo) * limit);
		uiDisplayf("CVideoFileDataEnumerator::BuildVideoList - GetVideoInfos");
		if (CContentSearch::GetVideoInfos(offset, limit, &noofFiles, &sm_infos[0]))
		{
			uiDisplayf("CVideoFileDataEnumerator::BuildVideoList - GetVideoInfos - SUCCESS");
			CFileData scratchData;
			CFileDataVideo videoData;
			for (u32 index = 0; index < noofFiles; ++index)
			{
				// first, make sure it's a video and not a photo ...unlikely from other checks, but check anyway
				if (!IsVideo(index))
					continue;

				// is video by the user currently playing?
				if (!IsVideoByUser(index, currentUserId))
					continue;

				u64 lastUpdated = 0;
				u32 duration = 0;
				if (!GetMetadata(index, &lastUpdated, &duration, maxStringLength, subtitle))
					continue;

				// videos created by publishing a replay have a special subtitle which we can filter by
				// issue: sceSearchContent metadata doesn't have keywords like we can create on video recording
				if (strcmp(subtitle, "GTAV Exported Video") != 0)
					continue;

				// store full path and filename in filename
				char path[RAGE_MAX_PATH];
				safecpy(path, GetFilename(index));

				char* trimPoint = strrchr(path, '/');
				const char *filename = trimPoint + 1;
				*trimPoint = '\0';

				scratchData.setFilename( filename );

				// videos to av_content add a * at the end ...remove if that's the case
				char title[maxStringLength] = { 0 };
				safecpy(title, GetTitle(index));
				int length = strlen(title);
				if (length && title[length-1] == '*')
					title[length-1] = '\0';

				scratchData.setDisplayName( title );
				videoData.setPath( path );

				videoData.setThumbnail( GetThumbnailPath(index) );
				videoData.setContentId( sm_infos[index].contentId );

				scratchData.setLastWriteTime( lastUpdated );
				scratchData.setSize( GetFileSizeByIndex(index) );
				scratchData.setExtraDataU32( duration * 1000 );
				scratchData.setUserId(sm_activeUserId);

				sm_fileData->PushAndGrow( scratchData );
				sm_videoData->PushAndGrow( videoData );
			}
		}

		// make sure we destroy the list as we iterate through this function for the next batch
		ClearVideoList();

		uiDisplayf("CVideoFileDataEnumerator::BuildVideoList - Infos found %d. Nooffiles added %d", noofFiles, sm_fileData->size());
		return noofFiles;
	}

	uiDisplayf("CVideoFileDataEnumerator::BuildVideoList - No infos found.");
	return 0;
}

void CVideoFileDataEnumerator::ClearVideoList()
{
	if (sm_infos)
		delete sm_infos;

	sm_infos = NULL;
}

bool CVideoFileDataEnumerator::IsVideo(u32 index)
{
	return (sm_infos[index].contentType == SCE_CONTENT_SEARCH_CONTENT_TYPE_VIDEO);
}

bool CVideoFileDataEnumerator::IsVideoByUser(u32 index, s32 user)
{
	for (u8 userIndex = 0; userIndex < SCE_CONTENT_SEARCH_MAX_ACCOUNTS; ++userIndex)
	{
		replayDebugf1("IsVideoByUser %u, %d, %d", index, user, sm_infos[index].accounts[userIndex]);
		if (sm_infos[index].accounts[userIndex] == user)
			return true;
	}

	return false;
}

const char* CVideoFileDataEnumerator::GetTitle(u32 index)
{
	return sm_infos[index].title;
}

const char* CVideoFileDataEnumerator::GetFilename(u32 index)
{
	return sm_infos[index].contentPath;
}

const char* CVideoFileDataEnumerator::GetThumbnailPath(u32 index)
{
	return sm_infos[index].iconPath;
}

u64 CVideoFileDataEnumerator::GetFileSizeByIndex(u32 index)
{
	return static_cast<u64>(sm_infos[index].size); // sceContentSearch info's file size variable is weirdly s64 ...could be an issue? 
}

bool CVideoFileDataEnumerator::GetMetadata(u32 index, u64* lastUpdated, u32* duration, u32 maxStringLength, char* subtitle)
{
	return CContentSearch::GetMetaData(&sm_infos[index], NULL, lastUpdated, NULL, NULL, NULL, duration, maxStringLength, NULL, subtitle);
}

#endif // defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED

#endif // defined( GTA_REPLAY ) && GTA_REPLAY

#endif // RSG_ORBIS