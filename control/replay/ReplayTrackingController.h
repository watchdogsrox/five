#ifndef _REPLAYTRACKINGCONTROLLER_H_
#define _REPLAYTRACKINGCONTROLLER_H_

#include "ReplaySettings.h"

#if GTA_REPLAY

#include "replay_channel.h"
#include "ReplayTrackingInfo.h"

#include "atl/map.h"
#include "atl/freelist.h"
#include "atl/bitset.h"

class CPacketEventTracked;

template<typename TYPE>
class CReplayTrackingController
{
public:
	typedef CTrackedEventInfo<TYPE>	tTrackedInfo;

	CReplayTrackingController();

	CTrackedEventInfo<TYPE>*	GetNextTrackableDuringRecording();
	void						FreeTrackableDuringRecording(CTrackedEventInfo<TYPE>* pTrackable);
	void						FreeTrackablesThisFrameDuringRecording();
	CTrackedEventInfo<TYPE>*	FindTrackableDuringRecording(const CTrackedEventInfo<TYPE>& info);
	void						CheckAllTrackablesDuringRecording();
	CTrackedEventInfo<TYPE>*	GetTrackedInfo(s16 id);
	const CTrackedEventInfo<TYPE>*	GetTrackedInfo(s16 id) const;
	void						ClearAllRecordingTrackables();
	void						CleanAllRecordingTrackables();
	void						RemoveInterpEvents();
	void						SetPlaybackTrackablesStale();

	s32							GetTrackablesCount() const		{	return m_trackablesDuringRecordingCount;	}

private:
	atFixedBitSet<TRACKABLES>				m_trackablesDuringRecordingAllocation;
	atRangeArray<tTrackedInfo, TRACKABLES>	m_trackablesDuringRecording;
	atFreeList<s16, TRACKABLES>				m_trackablesDuringRecordingFreeList;
	s32										m_trackablesDuringRecordingCount;

	atRangeArray<s16, TRACKABLES>			m_trackingIDsToFree;
	u32										m_trackingIDsToFreeCount;

	mutable sysCriticalSectionToken			m_critSectionToken;

	// SETUP TRACKABLES (Playback Setup)
public:
	void						SetupTrackable(s16 id, CPacketEventTracked* pPacket)
	{
		sysCriticalSection cs(m_critSectionToken);
		m_trackablesSetup.Set(id);
		m_trackablesSetupPackets[id] = pPacket;
	}
	void						ClearTrackable(s16 id)
	{
		sysCriticalSection cs(m_critSectionToken);
		m_trackablesSetup.Clear(id);
		m_trackablesSetupPackets[id] = NULL;
	}
	bool						IsTrackableBeingTracked(s16 id) const
	{
		sysCriticalSection cs(m_critSectionToken);
		return m_trackablesSetup.IsSet(id);
	}
	void						UpdateTrackable(s16 id, CPacketEventTracked* pPacket)
	{
		sysCriticalSection cs(m_critSectionToken);
		m_trackablesSetupPackets[id] = pPacket;
	}
	CPacketEventTracked*		GetLastTracked(s16 id)
	{
		sysCriticalSection cs(m_critSectionToken);
		return m_trackablesSetupPackets[id];
	}
	void						ResetSetupInformation();

private:
	atFixedBitSet<TRACKABLES>				m_trackablesSetup;
	atRangeArray<CPacketEventTracked*, TRACKABLES>		m_trackablesSetupPackets;

#if __ASSERT
	bool m_HasOutputListWhenFull;
#endif //__ASSERT
};

#endif // GTA_REPLAY

#endif // _REPLAYTRACKINGCONTROLLER_H_
