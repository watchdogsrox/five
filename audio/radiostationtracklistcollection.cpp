// 
// audio/radiostationtracklistcollection.cpp
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
// 

#include "radiostationtracklistcollection.h"
#include "audiohardware/channel.h"

AUDIO_MUSIC_OPTIMISATIONS()

s32 audRadioStationTrackListCollection::ComputeNumTracks(const u32 context /* = g_NullContext */, bool ignoreLocking) const
{
	if(context == kNullContext && (ignoreLocking || !m_EnableLocking || !m_TrackListsLocked.AreAnySet()))
	{
		return m_TotalNumTracks;
	}
	else
	{	
		s32 numTracks = 0;
		for(s32 i = 0; i < m_TrackLists.GetCount(); i++)
		{
			if(!m_EnableLocking || m_TrackListsLocked.IsClear(i))
			{
				const RadioStationTrackList *trackList = m_TrackLists[i];
				Assert(trackList);
		
				//Count the number of tracks for this context.
				for(u32 trackIndex=0; trackIndex < trackList->numTracks; trackIndex++)
				{
					if(context == kNullContext || trackList->Track[trackIndex].Context == context)
					{
						numTracks++;
					}
				}
			}
		}
		return numTracks;
	}
}

s32 audRadioStationTrackListCollection::GetUnlockedListCount() const
{
	s32 count = 0;

	for(s32 i = 0; i < m_TrackLists.GetCount(); i++)
	{
		if(m_TrackListsLocked.IsClear(i))
		{
			count++;
		}
	}

	return count;
}

const RadioStationTrackList::tTrack *audRadioStationTrackListCollection::GetTrack(const s32 index, const u32 context /*= g_NullContext*/, bool ignoreLocking) const
{
	audAssertf(index < m_TotalNumTracks, "Invalid track index %d, total tracks %d", index, m_TotalNumTracks);

	s32 trackIndex = index;
	for(s32 i = 0; i < m_TrackLists.GetCount(); i++)
	{
		const RadioStationTrackList *trackList = m_TrackLists[i];
		if(ignoreLocking || !m_EnableLocking || m_TrackListsLocked.IsClear(i))
		{
			if(context == kNullContext)
			{
				if(trackIndex >= static_cast<s32>(trackList->numTracks))
				{
					trackIndex -= static_cast<s32>(trackList->numTracks);
				}
				else
				{
					return &trackList->Track[trackIndex];
				}
			}
			else
			{
				for(u32 j = 0; j < trackList->numTracks; j++)
				{
					if(trackList->Track[j].Context == context)
					{
						trackIndex--;
						if(trackIndex == 0)
						{
							return &trackList->Track[j];
						}
					}
				}
			}
		}
	}
	return NULL;
}
