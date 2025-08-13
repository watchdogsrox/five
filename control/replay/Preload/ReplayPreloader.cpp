#include "ReplayPreloader.h"
#include "../Replay.h"
#include "../ReplayInternal.h"
#include "../ReplayEventManager.h"
#include "../ReplayInterfaceManager.h"
#include "../Overlay/ReplayOverlay.h"
#include "streaming/streaming.h"

#if GTA_REPLAY


//////////////////////////////////////////////////////////////////////////
CReplayPreloader::CReplayPreloader(CReplayEventManager& eventManager, CReplayInterfaceManager& interfaceManager)
	: CReplayClipScanner(ScannerType, REPLAY_PRELOAD_TIME)
	, m_eventManager(eventManager)
	, m_interfaceManager(interfaceManager)
{
}


//////////////////////////////////////////////////////////////////////////
bool CReplayPreloader::UpdateScanInternal(ReplayThreadData& threadData, const CPacketFrame* pFramePacket, const CBlockInfo* pBlock, int flags)
{
	bool entityResult = true;
	bool eventResult = true;
	u32 entityCount = 0;
	u32 eventCount = 0;
	u32	entityRequestsCount = 0;
	u32 eventRequestsCount = 0;

	// Preload entities....
	if(flags == 0 || (flags & Entity) != 0)
	{
		// *** Do the actual preloading of packets here ***
		CAddressInReplayBuffer interfaceAddress = CAddressInReplayBuffer((CBlockInfo*)pBlock, (u32)((u8*)pFramePacket - pBlock->GetData()) + pFramePacket->GetPacketSize());
		ReplayController interfaceController(interfaceAddress, threadData.bufferInfo, pFramePacket->GetFrameRef());
		interfaceController.SetPlaybackFlags(CReplayState(threadData.mode == CReplayAdvanceReader::ePreloadDirFwd ? REPLAY_PRELOAD_PACKET_FWD : REPLAY_PRELOAD_PACKET_BACK));
		interfaceController.EnablePlayback();

		entityCount = m_preloadRequestsEntity.GetCount();
		entityResult = m_interfaceManager.FindPreloadRequests(interfaceController, m_preloadRequestsEntity, entityRequestsCount, sysTimer::GetSystemMsTime(), (flags & SingleFrame) != 0);
		//replayDebugf1("Scan found %u requests", m_preloadRequestsEntity.GetCount());
		interfaceController.DisablePlayback();
	}

	// Preload events....
	if(flags == 0 || (flags & Event) != 0)
	{
		CAddressInReplayBuffer eventAddress = CAddressInReplayBuffer((CBlockInfo*)pBlock, (u32)((u8*)pFramePacket - pBlock->GetData()) + pFramePacket->GetOffsetToEvents());
		ReplayController eventController(eventAddress, threadData.bufferInfo, pFramePacket->GetFrameRef());
		eventController.EnablePlayback();

		const CPacketBase* pPacket = (const CPacketBase*)((u8*)pFramePacket + pFramePacket->GetPacketSize());
		while(pPacket->GetPacketID() != PACKETID_GAMETIME)
		{
			pPacket = (const CPacketBase*)((u8*)pPacket + pPacket->GetPacketSize());
		}

		s32 preloadOffsetTime = 0;
		if (pPacket->GetPacketID() == PACKETID_GAMETIME)
		{
			CPacketGameTime const* pGameTimePacket = static_cast<CPacketGameTime const*>(pPacket);
			preloadOffsetTime = pGameTimePacket->GetGameTime() - threadData.currentFrameInfo.GetGameTime();
		}

		eventCount = m_preloadRequestsEvent.GetCount();
		eventResult = m_eventManager.FindPreloadRequests(eventController, m_preloadRequestsEvent, eventRequestsCount, preloadOffsetTime, fwTimer::GetSystemTimer().GetTimeInMilliseconds());

		eventController.DisablePlayback();
	}


	if(entityResult == false || eventResult == false)
	{
		// Couldn't fit all of this frame's preload requests into the array
		// so revert back to last frame's total...we'll try this frame again
		m_preloadRequestsEntity.Resize(entityCount);
		m_preloadRequestsEvent.Resize(eventCount);
		m_reachedExtent = false;
		replayAssertf(entityCount > 0 || eventCount > 0, "Preloader is stuck on frame %u:%u (entityRequestsCount %u, eventRequestsCount %u) ...will skip to the next frame", pFramePacket->GetFrameRef().GetBlockIndex(), pFramePacket->GetFrameRef().GetFrame(), entityRequestsCount, eventRequestsCount);
		if(entityCount > 0 || eventCount > 0)
			return false;
	}

	return true;
}


//////////////////////////////////////////////////////////////////////////
bool CReplayPreloader::ProcessResults(const CReplayState& replayFlags, const u32 replayTime, bool forceLoading, int flags)
{
	if(flags == 0)
	{
		flags = Entity | Event;
	}

	bool result1 = true;
	bool result2 = true;

	if(flags & Entity)
	{
		result1 = ProcessEntityPreloadingRequests(forceLoading, replayTime);
	}
	if(flags & Event)
	{
		result2 = ProcessEventPreloadingRequests(replayFlags, replayTime, forceLoading);
	}

	return result1 & result2;
}


//////////////////////////////////////////////////////////////////////////
bool CReplayPreloader::ProcessEntityPreloadingRequests(bool forceLoading, u32 currentGameTime)
{
	m_preloadRequestsEntityCount = m_preloadSuccessEntityCount = 0;

	tPreloadRequestArray& requests = m_preloadRequestsEntity;
	if(requests.size() > 0)
	{
		tPreloadSuccessArray successfulPreloads;
		m_interfaceManager.PreloadPackets(requests, successfulPreloads, currentGameTime);

		for(int i = successfulPreloads.size()-1; i >= 0; --i)
		{
			requests.Delete(successfulPreloads[i]);
		}
			
		//Remove any request that take too long and assert. 
		//Having to use sysTimer due to being in while loop
		u32 sysTime = sysTimer::GetSystemMsTime();
		BANK_ONLY(static bool deleteRequestsOnFail = true;)
		GTA_REPLAY_OVERLAY_ONLY(atString overlayFailedPackets;)
		for(int i = requests.size()-1; i >= 0; --i)
		{
			if((sysTime - requests[i].systemTime) >= REPLAY_PRELOAD_TIME_MAX)
			{
#if PRELOADDEBUG
				replayDebugf1("Preload Failure: %s", requests[i].failureReason.c_str());
#endif // PRELOADDEBUG
				replayAssertf(false, "Failed to preload entity (packet:%i) after %u ms of trying... see log for reason", requests[i].pPacket->GetPacketID(), REPLAY_PRELOAD_TIME_MAX);
				BANK_ONLY(if(deleteRequestsOnFail))
				{
#if GTA_REPLAY_OVERLAY
					if(overlayFailedPackets.GetLength() == 0)
					{
						overlayFailedPackets += " ";
						const char* pPacketName = NULL;
						m_interfaceManager.GeneratePacketNameList(requests[i].pPacket->GetPacketID(), pPacketName);
						overlayFailedPackets += pPacketName;
					}
#endif
					requests.Delete(i);
				}
			}
		}

#if GTA_REPLAY_OVERLAY
		if(overlayFailedPackets.GetLength() != 0)
		{
			CReplayOverlay::SetPrecacheString("Waiting on entity preload packets...", overlayFailedPackets);
		}
#endif //GTA_REPLAY_OVERLAY

		m_preloadRequestsEntityCount = requests.size();
		m_preloadSuccessEntityCount = successfulPreloads.size();
		replayDebugf2("Successful entity preloads = %d, Preloads remaining = %d", successfulPreloads.size(), requests.size());

		if(forceLoading)
		{
			CStreaming::LoadAllRequestedObjects(false);
			CInteriorProxy::ImmediateStateUpdate();
		}
	}

	//Return true if we have finished preloading all requests to full extents of the preloader.
	return m_reachedExtent && requests.size() == 0;
}


//////////////////////////////////////////////////////////////////////////
bool CReplayPreloader::ProcessEventPreloadingRequests(const CReplayState& replayFlags, u32 replayTime, bool forceLoading)
{
	m_preloadRequestsEventCount = m_preloadSuccessEventCount = 0;

	tPreloadRequestArray& requests = m_preloadRequestsEvent;
	int result = PRELOAD_SUCCESSFUL;

	if(requests.size() > 0)
	{
		tPreloadSuccessArray successfulPreloads;
		result = m_eventManager.PreloadPackets(requests, successfulPreloads, replayFlags, replayTime);

		for(int i = successfulPreloads.size()-1; i >= 0; --i)
		{
			requests.Delete(successfulPreloads[i]);
		}

		if(result & PRELOAD_FAIL)
		{
			GTA_REPLAY_OVERLAY_ONLY(atString overlayFailedPackets;)
			u32 sysTime = fwTimer::GetSystemTimer().GetTimeInMilliseconds();
			for(int i = requests.size()-1; i >= 0; --i)
			{
				if((sysTime - requests[i].systemTime) >= REPLAY_PRELOAD_TIME_MAX)
				{
					const char* pPacketName = NULL;
					if(m_eventManager.GetPacketName(requests[i].pPacket->GetPacketID(), pPacketName))
					{
#if GTA_REPLAY_OVERLAY
						if(overlayFailedPackets.GetLength() == 0)
						{
							overlayFailedPackets += " ";
							overlayFailedPackets += pPacketName;
						}
#endif //GTA_REPLAY_OVERLAY
						replayAssertf(false, "Failed to preload event (%s) after %u ms of trying...", pPacketName, REPLAY_PRELOAD_TIME_MAX);
					}
					else
					{
						GTA_REPLAY_OVERLAY_ONLY(overlayFailedPackets += "Unknown";)

						replayFatalAssertf(false, "Failed to preload event (%u) (Don't know the name!) after %u ms of trying...", (u32)(requests[i].pPacket->GetPacketID()), REPLAY_PRELOAD_TIME*2);
					}
					requests.Delete(i);
				}
			}
#if GTA_REPLAY_OVERLAY
			if(overlayFailedPackets.GetLength() != 0)
			{
				CReplayOverlay::SetPrecacheString("Waiting on event preload packets...", overlayFailedPackets);
			}
#endif //GTA_REPLAY_OVERLAY
		}

		m_preloadRequestsEventCount = requests.size();
		m_preloadSuccessEventCount = requests.size();
		replayDebugf2("Successful event preloads = %d, Preloads remaining = %d", successfulPreloads.size(), requests.size());

		if(forceLoading)
			CStreaming::LoadAllRequestedObjects(false);
	}	

	return (requests.size() == 0 || result == PRELOAD_FAIL_MISSING_ENTITY) && m_reachedExtent;
}


//////////////////////////////////////////////////////////////////////////
void CReplayPreloader::ClearPreloadingRequests()
{
	m_preloadRequestsEvent.clear();
	m_preloadRequestsEntity.clear();
	m_preloadRequestsEntityCount = m_preloadRequestsEventCount = m_preloadSuccessEntityCount = m_preloadSuccessEventCount = 0;
}


//////////////////////////////////////////////////////////////////////////
void CReplayPreloader::Clear()
{	
	ClearPreloadingRequests();
}


//////////////////////////////////////////////////////////////////////////
void CReplayPreloader::GetBlockDetails(atString& str) const
{
	char pStr[1024];

	sprintf(pStr, "Preloader: Current Frame %u %u \n         Entity %u req, %u success\n          Event %u req, %u success\n",
		m_currentPreloadExtent.GetFrameRef().GetBlockIndex(), m_currentPreloadExtent.GetFrameRef().GetFrame(),
		m_preloadRequestsEntityCount,
		m_preloadSuccessEntityCount,
		m_preloadRequestsEventCount,
		m_preloadSuccessEventCount);

	str += pStr;
}


#endif // GTA_REPLAY
