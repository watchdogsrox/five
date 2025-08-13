#ifndef REPLAY_TRACKING_CONTROLLER_IMPL_H
#define REPLAY_TRACKING_CONTROLLER_IMPL_H

#include "ReplayTrackingController.h"

#include "rmptfx/ptxeffectinst.h"
#include "audio/DynamicMixerPacket.h"
#include "audio/dynamicmixer.h"

//////////////////////////////////////////////////////////////////////////
template<typename TYPE>
CReplayTrackingController<TYPE>::CReplayTrackingController()
	: m_trackablesDuringRecordingCount(0)
	, m_trackingIDsToFreeCount(0)
{
	m_trackablesSetup.Reset();

	for(u32 i = 0; i < TRACKABLES; ++i)
		m_trackingIDsToFree[i] = -1;

#if __ASSERT
	m_HasOutputListWhenFull = false;
#endif //__ASSERT
}


//////////////////////////////////////////////////////////////////////////
template<typename TYPE>
CTrackedEventInfo<TYPE>*	CReplayTrackingController<TYPE>::GetNextTrackableDuringRecording()
{
	sysCriticalSection cs(m_critSectionToken);

	const s32 i = m_trackablesDuringRecordingFreeList.Allocate();

#if __ASSERT
	if( i == -1 && !m_HasOutputListWhenFull )
	{
		replayDebugf1("==============================");		replayDebugf1("Begin trackables dump");
		for(u32 id = 0; id < TRACKABLES; id++)
		{
			CTrackedEventInfo<TYPE>* pEventInfo = &m_trackablesDuringRecording[id];
			if( pEventInfo->m_PacketId != PACKETID_INVALID )
				replayDebugf1("%d: packet - %d", id, pEventInfo->m_PacketId);
		}	
		replayDebugf1("==============================");

		m_HasOutputListWhenFull = true;
	}
#endif

	REPLAY_CHECK(i != -1, NULL, "Ran out of trackables in replay event manager");
	REPLAY_CHECK(m_trackablesDuringRecordingAllocation.IsClear(i), NULL, "Allocation bitset disagrees with freelist, index %d", i);

	CTrackedEventInfo<TYPE>* pEventInfo = &m_trackablesDuringRecording[i];
	m_trackablesDuringRecordingAllocation.Set(i);

	pEventInfo->reset();
	pEventInfo->trackingID = (s16)i;

	++m_trackablesDuringRecordingCount;

	return pEventInfo;
}


//////////////////////////////////////////////////////////////////////////
template<typename TYPE>
void CReplayTrackingController<TYPE>::FreeTrackableDuringRecording(CTrackedEventInfo<TYPE>* pTrackable)
{
	sysCriticalSection cs(m_critSectionToken);

	if(!pTrackable || pTrackable->trackingID == -1)
		return;

	m_trackingIDsToFree[m_trackingIDsToFreeCount++] = pTrackable->trackingID;

	replayFatalAssertf(m_trackablesDuringRecordingAllocation.IsClear(pTrackable->trackingID) == false, "");
	m_trackablesDuringRecordingAllocation.Clear(pTrackable->trackingID);
	m_trackablesDuringRecording[pTrackable->trackingID].reset();
}


//////////////////////////////////////////////////////////////////////////
template<typename TYPE>
void CReplayTrackingController<TYPE>::FreeTrackablesThisFrameDuringRecording()
{
	sysCriticalSection cs(m_critSectionToken);

	for(u32 i = 0; i < m_trackingIDsToFreeCount; ++i)
	{
		s16 trackingID = m_trackingIDsToFree[i];
		replayAssertf(trackingID > -1, "Invalid ID to free");

		m_trackablesDuringRecordingFreeList.Free(trackingID);

		m_trackingIDsToFree[i] = -1;
		--m_trackablesDuringRecordingCount;
	}
	m_trackingIDsToFreeCount = 0;

	for(int i = 0; i < m_trackablesDuringRecording.size(); ++i)
		m_trackablesDuringRecording[i].storedInScratch = false;
}


//////////////////////////////////////////////////////////////////////////
template<> inline
void CReplayTrackingController<ptxEffectInst*>::FreeTrackablesThisFrameDuringRecording()
{
	sysCriticalSection cs(m_critSectionToken);

	for(int i = 0; i < m_trackablesDuringRecording.size(); ++i)
	{
		if(m_trackablesDuringRecordingAllocation.IsSet(i) && m_trackablesDuringRecording[i].updatedThisFrame == false)
		{
			FreeTrackableDuringRecording(&m_trackablesDuringRecording[i]);
			replayDebugf1("Forcing Freeing trackable %d", i);
			replayAssertf(m_trackablesDuringRecordingAllocation.IsClear(i) == true, "Index2 %d", i);
		}
	}

	for(u32 i = 0; i < m_trackingIDsToFreeCount; ++i)
	{
		s16 trackingID = m_trackingIDsToFree[i];
		replayAssertf(trackingID > -1, "Invalid ID to free");

		m_trackablesDuringRecordingFreeList.Free(trackingID);

		m_trackingIDsToFree[i] = -1;
		--m_trackablesDuringRecordingCount;
	}
	m_trackingIDsToFreeCount = 0;

	for(int i = 0; i < m_trackablesDuringRecording.size(); ++i)
	{
		m_trackablesDuringRecording[i].storedInScratch = false;
		m_trackablesDuringRecording[i].updatedThisFrame = false;
	}
}


//////////////////////////////////////////////////////////////////////////
template<typename TYPE>
CTrackedEventInfo<TYPE>* CReplayTrackingController<TYPE>::FindTrackableDuringRecording(const CTrackedEventInfo<TYPE>& info)
{
	sysCriticalSection cs(m_critSectionToken);

	int index = m_trackablesDuringRecording.Find(info);
	if(index == -1)
		return NULL;

	replayAssertf(m_trackablesDuringRecordingAllocation.IsClear(index) == false, "Index %d", index);

	return &m_trackablesDuringRecording[index];
}


//////////////////////////////////////////////////////////////////////////
template<typename TYPE>
void CReplayTrackingController<TYPE>::CheckAllTrackablesDuringRecording()
{
	sysCriticalSection cs(m_critSectionToken);

	static int i = 0;
	for(i = 0; i < m_trackablesDuringRecording.size(); ++i)
	{
		if(m_trackablesDuringRecordingAllocation.IsSet(i))
		{
			tTrackedInfo& info = m_trackablesDuringRecording[i];
			if(info.isValid() == false)
			{
				replayDebugf2("Freeing trackable %d", i);
				FreeTrackableDuringRecording(&m_trackablesDuringRecording[i]);
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
template<typename TYPE>
CTrackedEventInfo<TYPE>* CReplayTrackingController<TYPE>::GetTrackedInfo(s16 id)
{
	sysCriticalSection cs(m_critSectionToken);

	if(id >= 0 && id < TRACKABLES)
		return &m_trackablesDuringRecording[id];
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
template<typename TYPE>
const CTrackedEventInfo<TYPE>* CReplayTrackingController<TYPE>::GetTrackedInfo(s16 id) const
{
	sysCriticalSection cs(m_critSectionToken);

	if(id >= 0 && id < TRACKABLES)
		return &m_trackablesDuringRecording[id];
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
template<typename TYPE>
void CReplayTrackingController<TYPE>::ClearAllRecordingTrackables()
{
	sysCriticalSection cs(m_critSectionToken);

	m_trackablesDuringRecordingAllocation.Reset();
	m_trackablesDuringRecordingFreeList.MakeAllFree();
	for(int i = 0; i < m_trackablesDuringRecording.GetMaxCount(); ++i)
		m_trackablesDuringRecording[i].reset();
	m_trackablesDuringRecordingCount = 0;

	m_trackingIDsToFreeCount = 0;
	for(u32 i = 0; i < TRACKABLES; ++i)
		m_trackingIDsToFree[i] = -1;
}

template<typename TYPE>
void CReplayTrackingController<TYPE>::CleanAllRecordingTrackables()
{

}

template<> inline
void CReplayTrackingController<tTrackedSceneType>::CleanAllRecordingTrackables()
{
	sysCriticalSection cs(m_critSectionToken);

	for(int i = 0; i < m_trackablesDuringRecording.GetMaxCount(); ++i)
	{
		if(m_trackablesDuringRecording[i].m_pEffect[0].m_pScene)
		{
			m_trackablesDuringRecording[i].m_pEffect[0].m_pScene->Stop();
			m_trackablesDuringRecording[i].m_pEffect[0].m_pScene = NULL;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
template<typename TYPE>
void CReplayTrackingController<TYPE>::RemoveInterpEvents()
{
	sysCriticalSection cs(m_critSectionToken);

	static int i = 0;
	for(i = 0; i < m_trackablesDuringRecording.size(); ++i)
	{
		if(m_trackablesDuringRecordingAllocation.IsSet(i))
		{
			tTrackedInfo& info = m_trackablesDuringRecording[i];
			if(info.IsInterp() == true && info.isValid() == true)
			{
				replayDebugf2("Removing interp trackable %d", i);
				info.reset();
				FreeTrackableDuringRecording(&m_trackablesDuringRecording[i]);
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
template<typename TYPE>
void CReplayTrackingController<TYPE>::SetPlaybackTrackablesStale()
{
	sysCriticalSection cs(m_critSectionToken);

	for(int i = 0; i < m_trackablesDuringRecording.GetMaxCount(); ++i)
	{
		CTrackedEventInfo<TYPE>* pTrackedInfo = &m_trackablesDuringRecording[i];
		if( pTrackedInfo->CanSetStale() )
			pTrackedInfo->SetStale(true);
	}
}


//////////////////////////////////////////////////////////////////////////
template<typename TYPE>
void CReplayTrackingController<TYPE>::ResetSetupInformation()
{
	sysCriticalSection cs(m_critSectionToken);

 	for(int i = 0; i < TRACKABLES; ++i)
 	{
		if(m_trackablesSetupPackets[i])
		{
			m_trackablesSetupPackets[i] = NULL;
		}
	}
	m_trackablesSetup.Reset();
}

#endif