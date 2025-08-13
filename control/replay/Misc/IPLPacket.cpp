#include "IPLPacket.h"

#if GTA_REPLAY

#include "control/replay/ReplayIPLManager.h"

//////////////////////////////////////////////////////////////////////////
// Expects an array of ReplayMovieInfo[MAX_ACTIVE_MOVIES]
void CPacketIPL::Store(const atArray<u32>& info, const atArray<u32>& previousinfo)
{
	CPacketEvent::Store(PACKETID_IPL, sizeof(CPacketIPL));
	Assertf(info.size() < MAX_IPL_HASHES, "CPacketIPL: Number of changes > MAX_IPL_HASHES, increase MAX_IPL_HASHES count");

	m_IPLData.m_Count = info.size();
	const u32 *pHashes = info.GetElements();
	if( pHashes )
	{
		memcpy(m_IPLData.m_IPLHashes, pHashes, m_IPLData.m_Count * sizeof(u32));
	}


	m_PreviousIPLData.m_Count = previousinfo.size();
	const u32 *pPreviousHashes = previousinfo.GetElements();
	if( pPreviousHashes )
	{
		memcpy(m_PreviousIPLData.m_IPLHashes, pPreviousHashes, m_PreviousIPLData.m_Count * sizeof(u32));
	}
}


//////////////////////////////////////////////////////////////////////////
void CPacketIPL::Extract(const CEventInfo<void>& eventInfo) const
{
	//Don't extract previous data when playing backwards if this is the first frame. 
	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK && !eventInfo.GetIsFirstFrame())
	{
		if(m_PreviousIPLData.m_Count > 0)
		{
			ReplayIPLManager::SubmitNewIPLData(m_PreviousIPLData.m_Count, m_PreviousIPLData.m_IPLHashes);
			return;
		}
	}

	ReplayIPLManager::SubmitNewIPLData(m_IPLData.m_Count, m_IPLData.m_IPLHashes);
}


#endif	//GTA_REPLAY
