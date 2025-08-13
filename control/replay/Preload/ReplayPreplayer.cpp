#include "ReplayPreplayer.h"
#include "../Misc/ReplayPacket.h"
#include "../ReplayController.h"
#include "../ReplayEventManager.h"

#if GTA_REPLAY


//////////////////////////////////////////////////////////////////////////
CReplayPreplayer::CReplayPreplayer(CReplayEventManager& eventManager)
	: CReplayClipScanner(ScannerType, REPLAY_PREPLAY_TIME)
	, m_eventManager(eventManager)
{
}


//////////////////////////////////////////////////////////////////////////
// Use the event manager to process through the list of event packets we've
// found that need to be played.  If any are successul (or failed badly) then
// remove them from the list.
bool CReplayPreplayer::ProcessResults(const CReplayState& replayFlags, const u32 replayTime, bool /*forceLoading*/, int /*flags*/)
{
	atArray<s32> successfulPreplays;
	m_eventManager.PreplayPackets(m_preplayRequests, successfulPreplays, replayFlags, replayTime, m_readAheadTime);
	
	for(int i = successfulPreplays.size()-1; i >= 0; --i)
	{
		m_preplayRequests.Delete(successfulPreplays[i]);
	}

	return true;
}


//////////////////////////////////////////////////////////////////////////
void CReplayPreplayer::Clear()
{
	m_preplayRequests.clear();
}


//////////////////////////////////////////////////////////////////////////
// Call through to the event manager to find a list of all the events that
// need to be played before their time in the clip.
bool CReplayPreplayer::UpdateScanInternal(ReplayThreadData& threadData, const CPacketFrame* pFramePacket, const CBlockInfo* pBlock, int /*flags*/)
{
	CAddressInReplayBuffer eventAddress = CAddressInReplayBuffer((CBlockInfo*)pBlock, (u32)((u8*)pFramePacket - pBlock->GetData()) + pFramePacket->GetOffsetToEvents());
	ReplayController eventController(eventAddress, threadData.bufferInfo, pFramePacket->GetFrameRef());
	eventController.EnablePlayback();

	// Search for Events that are registered with the 'PrePlay' flag
	m_eventManager.FindPreplayRequests(eventController, m_preplayRequests);

	eventController.DisablePlayback();

	return true;
}


//////////////////////////////////////////////////////////////////////////
void CReplayPreplayer::GetBlockDetails(atString& str) const
{
	char pStr[1024];
	sprintf(pStr, "\tPreplayer: %u\n", m_preplayRequests.GetCount());

	str += pStr;
}


#endif // GTA_REPLAY