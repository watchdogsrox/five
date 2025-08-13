#include "ReplayMontageGenerator.h"

#if GTA_REPLAY

// rage
#include "file/limits.h"

// game
#include "control/replay/replay.h"
#include "control/replay/ReplaySettings.h"
#include "control/replay/ReplaySupportClasses.h"
#include "control/replay/file/device_replay.h"
#include "replaycoordinator/replay_coordinator_channel.h"
#include "replaycoordinator/ReplayEditorParameters.h"
#include "replaycoordinator/storage/Montage.h"
#include "system/FileMgr.h"

REPLAY_COORDINATOR_OPTIMISATIONS();

u32							CMissionMontageGenerator::m_Duration = 0;
atString					CMissionMontageGenerator::m_Filter = atString("");
atArray<CMissionMontageGenerator::sMontageClipCache>	CMissionMontageGenerator::m_ClipCache;
atArray<CMissionMontageGenerator::sMontageMarker>		CMissionMontageGenerator::sm_ProcessedMarkers;
u32							CMissionMontageGenerator::m_ClipCacheLength = 0;
bool						CMissionMontageGenerator::m_FilterMission = false;
s8							CMissionMontageGenerator::m_FilterPedType = -1;
u32							CMissionMontageGenerator::m_MontageState = MONTAGE_GEN_IDLE;
bool						CMissionMontageGenerator::m_HasFailed = false;
FileDataStorage				CMissionMontageGenerator::sm_DataStorage;
ReplayHeader				CMissionMontageGenerator::sm_CurrentHeader;
u32							CMissionMontageGenerator::sm_CurrentHeaderIndex = 0;
CMontage*					CMissionMontageGenerator::sm_Montage = NULL;


//////////////////////////////////////////////////////////////////////////
CMissionMontageGenerator::CMissionMontageGenerator( CReplayEditorParameters::eAutoGenerationDuration duration, const atString& filter, const bool filterMission, const s8 filterPedType)
{
	m_Duration = CReplayEditorParameters::GetAutoGenerationDuration(duration);
	m_Filter = filter;
	m_ClipCacheLength = 0;
	sm_CurrentHeaderIndex = 0;
	m_ClipCache.Reset();
	sm_ProcessedMarkers.Reset();
	m_FilterMission = filterMission;
	m_FilterPedType = filterPedType;
}

bool CMissionMontageGenerator::StartMontageGeneration()
{
	m_HasFailed = false;
	m_MontageState = MONTAGE_GEN_ENUMERATE_FILES;
	sm_Montage = NULL;
	return true;
}

void CMissionMontageGenerator::ProcessMontages()
{
	switch(m_MontageState)
	{
		case MONTAGE_GEN_IDLE:
			{
				break;
			}
		case MONTAGE_GEN_ENUMERATE_FILES:
			{
				if(!CReplayMgr::StartEnumerateClipFiles( "", sm_DataStorage ))
				{
					m_HasFailed = true;

					m_MontageState = MONTAGE_GEN_IDLE;
					break;
				}
				m_MontageState = MONTAGE_GEN_CHECK_ENUMERATE;
				break;
			}

		case MONTAGE_GEN_CHECK_ENUMERATE:
			{
				bool result = false;
				if( ReplayFileManager::CheckEnumerateClipFiles(result) )
				{
					if( sm_DataStorage.GetCount() > 0 )
					{
						m_MontageState = MONTAGE_GEN_START_GET_HEADER;
						sm_CurrentHeaderIndex = 0;
					}
					else
					{
						m_HasFailed = true;
						m_MontageState = MONTAGE_GEN_IDLE;
					}
				}
				break;
			} 

		case MONTAGE_GEN_START_GET_HEADER:
			{
				if(!CReplayMgr::StartGetHeader(sm_DataStorage[sm_CurrentHeaderIndex].getFilename()))
				{
					if(sm_CurrentHeaderIndex < sm_DataStorage.GetCount()-1)
					{
						if(sm_ProcessedMarkers.GetCount() == 0)
						{
							sm_CurrentHeaderIndex++;
							break;
						}

						// We've grabbed all the information from the headers. But the last one has failed
						// Process the markers.
						m_MontageState = MONTAGE_GEN_PROCESS_MARKERS;
						break;
					}

					m_HasFailed = true;
					m_MontageState = MONTAGE_GEN_IDLE;
					break;
				}
				m_MontageState = MONTAGE_GEN_CHECK_HEADER;
				break;
			}

		case MONTAGE_GEN_CHECK_HEADER:
			{
				bool result = false;
				
				if(CReplayMgr::CheckGetHeader(result, sm_CurrentHeader))
				{
					if(!result)
					{
						if(sm_CurrentHeaderIndex < sm_DataStorage.GetCount()-1)
						{
							sm_CurrentHeaderIndex++;
							m_MontageState = MONTAGE_GEN_START_GET_HEADER;
							break;
						}

						if(sm_ProcessedMarkers.GetCount() == 0)
						{
							m_HasFailed = true;
							m_MontageState = MONTAGE_GEN_IDLE;
							break;
						}

						m_MontageState = MONTAGE_GEN_PROCESS_MARKERS;
						break;
					}
					
					m_MontageState = MONTAGE_GEN_PROCESS_HEADER;
				}
				break;
			}

		case MONTAGE_GEN_PROCESS_HEADER:
			{
				if(CheckClipCanBeUsed())
				{
					// Extract information needed from header.
					for(int k = 0; k < sm_CurrentHeader.MarkerCount; ++k)
					{
						const ReplayBufferMarker& headerMarker = sm_CurrentHeader.eventMarkers[k];
						
						//Copy over the required data into a small array, some data isn't need past this point
						sMontageMarker montageMarker;
						montageMarker.SetStartTime(headerMarker.GetStartTime());
						montageMarker.SetEndTime(headerMarker.GetEndTime());
						montageMarker.SetImportance((MarkerImportance)headerMarker.GetImportance());
						montageMarker.SetPlayerType(headerMarker.GetPlayerType());

						montageMarker.SetClipTime(sm_CurrentHeader.fClipTime);
						montageMarker.SetData(&sm_DataStorage[sm_CurrentHeaderIndex]);
						montageMarker.SetUnixTime(headerMarker.GetUnixTime());
						montageMarker.SetTimeInFuture(headerMarker.GetTimeInFuture());
						montageMarker.SetTimeInPast(headerMarker.GetTimeInPast());
						montageMarker.SetClipStartTime(headerMarker.GetClipStartTime());
						montageMarker.SetMissionIndex(headerMarker.GetMissionIndex());

						sm_ProcessedMarkers.PushAndGrow(montageMarker);
					}
				}

				if(sm_CurrentHeaderIndex == (u32)(sm_DataStorage.GetCount()-1))
				{
					// We've grabbed all the information from the headers.
					// Process the markers.
					m_MontageState = MONTAGE_GEN_PROCESS_MARKERS;
					break;
				}

				// We're not at the end of the headers, go get next marker.
				sm_CurrentHeaderIndex++;
				m_MontageState = MONTAGE_GEN_START_GET_HEADER;
				break;
			}

		case MONTAGE_GEN_PROCESS_MARKERS:
			{
				//No markers were found in the clip files, fail
				if(sm_ProcessedMarkers.GetCount() == 0)
				{
					rcWarningf("Failed to generate montage, no clips for mission");
					m_HasFailed = true;
					m_MontageState = MONTAGE_GEN_IDLE;
					break;
				}

				//Create montage
				sm_Montage = CMontage::Create();

				//Failed to create CMontage, fail
				if(!sm_Montage)
				{
					rcWarningf("Failed to generate montage, CMontage::Create()");
					m_HasFailed = true;
					m_MontageState = MONTAGE_GEN_IDLE;
					break;
				}
				
				//Process which markers are going to be added to the montage
				AddClipsToMontage(sm_Montage);

				//No markers added so fail
				if( sm_Montage->GetClipCount() == 0 )
				{
					replayWarningf("Failed to save montage");
					CMontage::Destroy(sm_Montage);
					m_HasFailed = true;
					m_MontageState = MONTAGE_GEN_IDLE;
					break;
				}

				// Give the montage a name, but don't save it here. Pass it back up as we may save additional
				// data as part of the UI project.
				char s_Filename[REPLAYFILELENGTH];
				atString dataStamp;
				u32 uid;
				ReplayHeader::GenerateDateString(dataStamp, uid);
				sprintf_s(s_Filename,REPLAYFILELENGTH,"montage_%s",dataStamp.c_str());
				sm_Montage->SetName( s_Filename );

				//Finished, clean up.
				m_HasFailed = false;
				m_MontageState = MONTAGE_GEN_IDLE;
				sm_CurrentHeaderIndex = 0; 
				m_ClipCacheLength = 0;
				m_ClipCache.Reset();
				sm_ProcessedMarkers.Reset();
				break;
			}
	}
}

//////////////////////////////////////////////////////////////////////////
CMontage* CMissionMontageGenerator::GetGeneratedMontage() 
{	
	return sm_Montage;
}


void CMissionMontageGenerator::GetValidMissions(atArray<atString>& validMissions, bool includeMissionString)
{
	FileDataStorage fileDataStorage;
	/*CReplayMgr::EnumerateFiles(RAW_CLIP_FILE_EXTENSION, fileDataStorage);*/

	//Got through all the clips files and grab the montage filter
	for(int i = 0; i < fileDataStorage.GetCount(); ++i)
	{
		const ReplayHeader* header = NULL/*CReplayMgr::GetReplayHeader(fileDataStorage[i].getFilename())*/;
		if(header)
		{
			//Split by the delimiter "|"
			atString filter = atString(header->MontageFilter);
			atArray<atString> spiltfilter;
			filter.Split(spiltfilter,"|", true);

			for (int j=0; j<spiltfilter.GetCount(); j++)
			{
				//Check if the element already exists
				if(validMissions.Find(spiltfilter[j]) == -1)
				{
					validMissions.PushAndGrow(spiltfilter[j]);
				}
			}

			//Add mission string is required
			atString missionFilter = atString(header->MissionNameFilter);
			if(includeMissionString && missionFilter.GetLength() != 0)
			{
				if(validMissions.Find(missionFilter) == -1)
				{
					validMissions.PushAndGrow(missionFilter);
				}
			}
		}
	}
}

//ePedType
void CMissionMontageGenerator::GetValidPlayerTypes(atArray<s8>& validPlayers)
{
	FileDataStorage fileDataStorage;
	/*CReplayMgr::EnumerateFiles(RAW_CLIP_FILE_EXTENSION, fileDataStorage);*/

	//Got through all the clips files and grab the montage filter
	for(int i = 0; i < fileDataStorage.GetCount(); ++i)
	{
		const ReplayHeader* header = NULL;//CReplayMgr::GetReplayHeader(fileDataStorage[i].getFilename());
		if(header)
		{
			const ReplayBufferMarker* eventmarkers = &header->eventMarkers[0];

			for(int j = 0; j < REPLAY_HEADER_EVENT_MARKER_MAX; ++j)
			{
				//Check if the element already exists
				if(eventmarkers[j].GetPlayerType() != -1 && validPlayers.Find(eventmarkers[j].GetPlayerType()) == -1)
				{
					validPlayers.PushAndGrow(eventmarkers[j].GetPlayerType());
				}

				++eventmarkers;
			}
		}
	}
}

static int SortByTime(const CMissionMontageGenerator::sMontageClipCache *a, const CMissionMontageGenerator::sMontageClipCache *b)
{
	if( a->missionIndex > b->missionIndex)
	{
		return 1;
	}
	else if( a->missionIndex < b->missionIndex)
	{
		return -1;
	}

	if( a->writeTime > b->writeTime )
	{
		return 1;
	}
	else if( a->writeTime < b->writeTime )
	{
		return -1;
	}

	if( a->startTime > b->startTime )
	{
		return 1;
	}
	else if( a->startTime < b->startTime )
	{
		return -1;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CMissionMontageGenerator::AddClipsToMontage(CMontage* pMontage)
{
	for(int i = IMPORTANCE_HIGHEST; i >= IMPORTANCE_LOWEST; --i)
	{
		for(int k = 0; k < sm_ProcessedMarkers.GetCount(); ++k)
		{
			const sMontageMarker& marker = sm_ProcessedMarkers[k];

			u32 startTime = marker.GetStartTime();
			u32 endTime = marker.GetEndTime();
			u32 startRelativeTime = marker.GetRelativeStartTime();
			u32 endRelativeTime = marker.GetRelativeEndTime();
			CFileData* data = marker.GetData();

			if(CanMarkerBeAdded(marker, (u8)i, *data, startTime, endTime, startRelativeTime, endRelativeTime))
			{
				AddClipToCache(marker, (u8)i, *data, marker.GetClipTime(), startTime, endTime, startRelativeTime, endRelativeTime);
			}
		}

		//Check if we're added too many clips of this importance and remove randomly for that importance.
		if(IsCacheFull((u8)i))
		{
			break;
		}
	}

	//Sort the clips by start time
	m_ClipCache.QSort(0,-1,SortByTime);

	//Add cache to montage
	PopulateMonageWithClips(pMontage);
}

bool CMissionMontageGenerator::CanMarkerBeAdded(const sMontageMarker& markerToAdd, u8 importance, const CFileData& data, u32& startTime, u32& endTime, u32& startRelativeTime, u32& endRelativeTime)
{
	if(!markerToAdd.IsValid() || markerToAdd.GetImportance() != importance)
	{
		return false;
	}

	if(m_FilterPedType != -1 && markerToAdd.GetPlayerType() != m_FilterPedType)
	{
		return false;
	}

	RemoveOverlappingClips(markerToAdd, data, startTime, endTime, startRelativeTime, endRelativeTime);

	return true;
}

void CMissionMontageGenerator::RemoveOverlappingClips(const sMontageMarker& markerToAdd, const CFileData& data, u32& startTime, u32& endTime, u32& startRelativeTime, u32& endRelativeTime)
{
	for(int i = m_ClipCache.GetCount() - 1; i >= 0; --i)
	{
		if(!IsValid(markerToAdd.GetStartTime(), markerToAdd.GetEndTime(), m_ClipCache[i].startTime, m_ClipCache[i].endTime) && 
			strcmp(m_ClipCache[i].data->getFilename(), data.getFilename()) == 0)
		{
			if(startTime > m_ClipCache[i].startTime)
			{
				startTime = m_ClipCache[i].startTime;
				startRelativeTime = m_ClipCache[i].relativeStartTime;
			}

			if(endTime < m_ClipCache[i].endTime)
			{
				endTime = m_ClipCache[i].endTime;
				endRelativeTime = m_ClipCache[i].relativeEndTime;
			}

			m_ClipCacheLength -= (m_ClipCache[i].relativeEndTime - m_ClipCache[i].relativeStartTime);
			m_ClipCache.Delete(i);
		}
	}
}

void CMissionMontageGenerator::AddClipToCache(const sMontageMarker& marker, u8 importance, CFileData& data, float fClipTime, u32 startTime, u32 endTime, u32 startRelativeTime, u32 endRelativeTime)
{
	sMontageClipCache cache;
	cache.data = &data;
	cache.writeTime = data.getLastWriteTime();
	cache.unixStartTime = marker.GetUnixStartTime();
	cache.unixEndTime = marker.GetUnixEndTime();
	cache.startTime = startTime;
	cache.relativeStartTime = startRelativeTime;
	cache.endTime = endTime;
	cache.relativeEndTime = endRelativeTime;
	cache.length = marker.GetLength();
	cache.clipStartTime = marker.GetClipStartTime();

	cache.importance = importance;
	cache.missionIndex = marker.GetMissionIndex();

	replayDebugf2("Adding marker to montage (importance %d)", importance);
	replayDebugf2("  Absolute: %7u -> %7u",startTime,endTime);
	replayDebugf2("  Relative: %7u -> %7u", startRelativeTime, endRelativeTime);
	replayDebugf2("  (Offset:  %7u)", marker.GetClipStartTime());

	u32 clipLength = (u32)(fClipTime * 1000.0f);

	if (!replayVerifyf(cache.relativeStartTime >= 0 && cache.relativeStartTime <= clipLength, "Marker start time (%u) is not between 0 and %u", cache.relativeStartTime, clipLength))
	{
		cache.relativeStartTime = Max(Min(cache.relativeStartTime, clipLength), (u32)0);
	}

	if (!replayVerifyf(cache.relativeEndTime >= 0 && cache.relativeEndTime <= clipLength, "Marker end time (%u) is not between 0 and %u", cache.relativeEndTime, clipLength))
	{
		cache.relativeEndTime = Max(Min(cache.relativeEndTime, clipLength), (u32)0);
	}

	if (!replayVerifyf(cache.relativeEndTime - cache.relativeStartTime >= MINIMUM_CLIP_LENGTH, "Clip is too short (%ums)!", cache.relativeEndTime - cache.relativeStartTime))
	{
		return;
	}

	m_ClipCache.PushAndGrow(cache);

	m_ClipCacheLength += (endRelativeTime - startRelativeTime);
}

bool CMissionMontageGenerator::IsValid(u64 aStartTime, u64 aEndTime, u64 bStartTime, u64 bEndTime)
{
	if( aStartTime <= bEndTime && aEndTime >= bStartTime )
	{
		return false;
	}
	return true;
}

bool CMissionMontageGenerator::IsCacheFull(u8 importance)
{
	if((m_ClipCacheLength/1000) < m_Duration)
	{
		return false;
	}

	for(int i = m_ClipCache.GetCount()-1; i > 1; --i)
	{
		if(m_ClipCache[i].importance == importance)
		{
			m_ClipCacheLength -= m_ClipCache[i].length;
			m_ClipCache.Delete(i);
		}

		if((m_ClipCacheLength/1000) < m_Duration)
		{
			return true;
		}
	}

	return true;
}

bool CMissionMontageGenerator::CheckClipCanBeUsed()
{
	if(m_Filter.GetLength() == 0)
	{
		return true;
	}

	atString filter = atString(sm_CurrentHeader.MontageFilter);
	atString missionFilter = atString(sm_CurrentHeader.MissionNameFilter);
	if(filter.IndexOf(m_Filter) == -1 && missionFilter.IndexOf(m_Filter) == -1)
	{
		return false;
	}

	return true;
}

void CMissionMontageGenerator::PopulateMonageWithClips(CMontage* pMontage)
{
	CClip clip;
	int clipIndex = 0;
	int missionIndex = -1;
	for(int i = 0; i < m_ClipCache.GetCount(); ++i)
	{
		ReplayMarkerTransitionType fadeIn = MARKER_TRANSITIONTYPE_NONE;
		ReplayMarkerTransitionType fadeOut = MARKER_TRANSITIONTYPE_NONE;

		if(missionIndex < (int)m_ClipCache[i].missionIndex)
		{
			fadeIn = MARKER_TRANSITIONTYPE_FADEIN;
			missionIndex = m_ClipCache[i].missionIndex;
		}

		if(i == m_ClipCache.GetCount()-1 || (int)m_ClipCache[i+1].missionIndex > missionIndex)
		{
			fadeOut = MARKER_TRANSITIONTYPE_FADEOUT;
		}

		clip.Clear();
		replayDisplayf("Filename:%s",m_ClipCache[i].data->getFilename() );
		if(clip.Populate(m_ClipCache[i].data->getFilename(), m_ClipCache[i].data->getUserId(), fadeIn, fadeOut))
		{
			float clipStart = clip.GetStartNonDilatedTimeMs(), clipEnd = clip.GetEndNonDilatedTimeMs();
			float clipLength = clipEnd - clipStart;

			replayDebugf2("Montage clip %d:", clipIndex);
			replayDebugf2("  Whole clip: %f -> %f", clipStart, clipEnd);
			replayDebugf2("    (Length:  %f)", clipLength);

			float segmentStart = (float)m_ClipCache[i].relativeStartTime, segmentEnd = (float)m_ClipCache[i].relativeEndTime;

			replayDebugf2("     Segment: %f -> %f", segmentStart, segmentEnd);
			replayDebugf2("    (Length:  %f)", segmentEnd-segmentStart);


			if (!replayVerifyf(segmentStart >= 0, "Segment start must not be negative!"))
			{
				segmentStart = 0;
			}

			if (!replayVerifyf(segmentStart < clipLength, "Segment start must not be after clip length!"))
			{
				continue;
			}

			if (!replayVerifyf(segmentEnd >= 0, "Segment end must not be negative!"))
			{
				continue;
			}

			if (!replayVerifyf(segmentEnd <= clipLength, "Segment end must not be after clip length!"))
			{
				segmentEnd = clipLength;
			}

			if (!(replayVerifyf(clipLength >= MINIMUM_CLIP_LENGTH, "Clip segment %d is too short! (%fms, %f -> %f)", i, clipLength, clipStart, clipEnd)))
			{
				continue;
			}


			clip.SetAllTimesMs(segmentStart, segmentEnd, segmentEnd, segmentStart, segmentEnd );
			
			replayDebugf2("    Sub-clip: %f -> %f", clip.GetStartNonDilatedTimeMs(), clip.GetEndNonDilatedTimeMs());
			replayDebugf2("    (Length:  %f)", clip.GetRawDurationMs());

			pMontage->AddClipCopy(clip,clipIndex);
			++clipIndex;
		}
	}
}

#endif //GTA_REPLAY
