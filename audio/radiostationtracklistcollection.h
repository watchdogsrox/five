// 
// audio/radiostationtracklistcollection.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_RADIOSTATIONTRACKLISTCOLLECTION_H
#define AUD_RADIOSTATIONTRACKLISTCOLLECTION_H

#include "atl/array.h"
#include "atl/bitset.h"
#include "gameobjects.h"

enum {kNullContext = 0};

class audRadioStationTrackListCollection
{
public:
	audRadioStationTrackListCollection()
		: m_TotalNumTracks(0)
		, m_EnableLocking(true)
	{

	}

	bool AddTrackList(const RadioStationTrackList *trackList, f32 firstTrackProbability = 1.f)
	{
		if(trackList == NULL)
		{
			return false; 
		}
		
		// Don't allow duplicate track lists as it causes problems with history tracking (we think we have more tracks than we actually have)
		for(s32 i = 0; i < m_TrackLists.GetCount(); i++)
		{
			if(m_TrackLists[i] == trackList)
			{
				return false;
			}
		}

		m_TotalNumTracks += trackList->numTracks;
		m_TrackLists.Grow() = trackList;
		m_TrackListFirstTrackProbabilities.Grow() = firstTrackProbability;
		m_TrackListsLocked.Set(m_TrackLists.GetCount() - 1, AUD_GET_TRISTATE_VALUE(trackList->Flags, FLAG_ID_RADIOSTATIONTRACKLIST_LOCKED) == AUD_TRISTATE_TRUE);
		
		return true;
	}

	const RadioStationTrackList::tTrack *GetTrack(const s32 index, const u32 context = kNullContext, bool ignoreLocking = false) const;
	s32 ComputeNumTracks(const u32 context = kNullContext, bool ignoreLocking = false) const;

	const RadioStationTrackList *GetList(const s32 index) const { return m_TrackLists[index]; }
	f32 GetListFirstTrackProbability(const s32 index) const { return m_TrackListFirstTrackProbabilities[index]; }

	s32 GetListCount() const { return m_TrackLists.GetCount(); }
	s32 GetUnlockedListCount() const;

	// PURPOSE
	//	Returns true if locking is enabled and the specified track list is locked
	bool IsListLocked(const s32 index) const { return m_EnableLocking && m_TrackListsLocked.IsSet(index); }

	// PURPOSE
	//	Reset cached lock state to match metadata
	void ResetLockState()
	{
		for(s32 i = 0; i < m_TrackLists.GetCount(); i++)
		{
			m_TrackListsLocked.Set(i, AUD_GET_TRISTATE_VALUE(m_TrackLists[i]->Flags, FLAG_ID_RADIOSTATIONTRACKLIST_LOCKED) == AUD_TRISTATE_TRUE);
		}
	}

	bool IsTrackListUnlocked(const RadioStationTrackList *trackList) const
	{
		for(s32 i = 0; i < m_TrackLists.GetCount(); i++)
		{
			if(m_TrackLists[i] == trackList)
			{
				return !m_TrackListsLocked.IsSet(i);				
			}
		}
		return false;
	}

	bool UnlockTrackList(const RadioStationTrackList *trackList)
	{
		bool found = false;
		for(s32 i = 0; i < m_TrackLists.GetCount(); i++)
		{
			if(m_TrackLists[i] == trackList)
			{
				m_TrackListsLocked.Clear(i);
				found = true;
				break;
			}
		}
		return found;
	}

	bool LockTrackList(const RadioStationTrackList *trackList)
	{
		bool found = false;
		for(s32 i = 0; i < m_TrackLists.GetCount(); i++)
		{
			if(m_TrackLists[i] == trackList)
			{
				m_TrackListsLocked.Set(i);
				found = true;
				break;
			}
		}
		return found;
	}

	void SetLockingEnabled(const bool enabled)
	{
		m_EnableLocking = enabled;
	}

private:

	atArray<const RadioStationTrackList *> m_TrackLists;
	atArray<f32> m_TrackListFirstTrackProbabilities;
	atFixedBitSet<32> m_TrackListsLocked;
	s32 m_TotalNumTracks;
	bool m_EnableLocking;
};

#endif // AUD_RADIOSTATIONTRACKLISTCOLLECTION_H
