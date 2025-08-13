#include "ReplayEventManager.h"

#if GTA_REPLAY

#include "audiosoundtypes/sound.h"
#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "bank/group.h"

#include "Control/Replay/ReplayController.h"
#include "Control/Replay/Effects/ParticlePacket.h"
#include "control/replay/Audio/SoundPacket.h"
#include "control/replay/Preload/ReplayPreloader.h"

#include "ReplayTrackingControllerImpl.h"

#include "vfx/VfxHelper.h"
 
//////////////////////////////////////////////////////////////////////////
CReplayEventManager::CReplayEventManager()
: m_interfaceManager(NULL)
, m_blockMonitorBuffer(EVENT_BLOCK_MONITOR_SIZE, "Block Monitor Buffer")
, m_frameMonitorBuffer(EVENT_FRAME_MONITOR_SIZE, "Frame Monitor Buffer")
{
	for(u8 i = 0; i < NUM_EVENT_BUFFERS; ++i)
	{
		m_pEventBuffers[i] = (u8*)sysMemManager::GetInstance().GetReplayAllocator()->Allocate(EVENT_BUFFER_SIZE, 16);
	}

	m_currentEventBuffer = 0;
	m_eventRecorder = CReplayRecorder(GetCurrentEventBuffer(), EVENT_BUFFER_SIZE);

 	m_eventMemPeak = 0;
 	m_eventMemFrame = 0;
}


//////////////////////////////////////////////////////////////////////////
CReplayEventManager::~CReplayEventManager()
{
	for(u8 i = 0; i < NUM_EVENT_BUFFERS; ++i)
	{
		sysMemManager::GetInstance().GetReplayAllocator()->Free((void*)m_pEventBuffers[i]);
		m_pEventBuffers[i] = NULL;
	}

	m_historyPacketControllers.Reset();
}

//////////////////////////////////////////////////////////////////////////
template<>
CReplayTrackingController<tTrackedDecalType>& CReplayEventManager::GetController<tTrackedDecalType>()
{
	return m_decalTrackingController;
}

template<>
const CReplayTrackingController<tTrackedDecalType>& CReplayEventManager::GetController<tTrackedDecalType>() const
{
	return m_decalTrackingController;
}


//////////////////////////////////////////////////////////////////////////
// When we spawn the effects during playback we need to set a flag on them
// to prevent them being spawned multiple times.  However if we loop round
// then the packets would still be flagged as having spawned so we need to 
// reset them here.
void CReplayEventManager::ResetHistoryPacketsAfterTime(ReplayController& controller, u32 time)
{
	// Might want to just null things out as this deallocates memory...

	while(controller.IsEndOfPlayback() == false)
	{
		CPacketBase const* pBasePacket = controller.ReadCurrentPacket<CPacketBase>();
		
		eReplayPacketId packetID = pBasePacket->GetPacketID();
		if(IsRelevantHistoryPacket(packetID))
		{
			CPacketEvent* pHistoryPacket = controller.GetCurrentPacket<CPacketEvent>();
			if(pHistoryPacket->GetGameTime() >= time)
			{
				((CPacketEvent*)pHistoryPacket)->SetPlayed(false);
			}
		}
		controller.AdvanceUnknownPacket(pBasePacket->GetPacketSize());
	}
}


/////////////////////////////////////////////////////////////////////////
// Find the correct controller with which to play back a history packet
// If the correct controller cannot be found then advance over the packet and 
// bail out (This might be taken out in the future).
CPacketBase const* CReplayEventManager::PlayBackHistory(ReplayController& controller, eReplayPacketId packetID, u32 packetSize, const u32 endTimeWindow, bool entityMayBeAbsent)
{
	CHistoryPacketController* pPacketController = m_historyPacketControllers.Access((u32)packetID);

	if(	pPacketController == NULL || 
		(pPacketController->m_playFunction == NULL && pPacketController->m_playTrackedFunction == NULL) || 
		!pPacketController->m_enabled)
	{
		controller.AdvanceUnknownPacket(packetSize);
		return controller.ReadCurrentPacket<CPacketBase>();
	}

	// Check the packet flags against the current controller flags
	if(!pPacketController->ShouldPlay(controller.GetPlaybackFlags().GetState()))
	{
		return (this->*(pPacketController->m_resetFunction))(controller, packetID, endTimeWindow);
	}

	if(pPacketController->m_playTrackedFunction)
		return (this->*(pPacketController->m_playTrackedFunction))(controller, endTimeWindow, pPacketController->m_playbackFlags, entityMayBeAbsent);
	else if(pPacketController->m_playFunction)
		return (this->*(pPacketController->m_playFunction))(controller, packetID, endTimeWindow, pPacketController->m_playbackFlags, entityMayBeAbsent);
	else
	{
		controller.AdvancePacket();
		return controller.ReadCurrentPacket<CPacketBase>();
	}
}


//////////////////////////////////////////////////////////////////////////
bool CReplayEventManager::PlayBackHistory(ReplayController& controller, u32 LastTime, bool entityMayBeAbsent)
{
	u32 endTimeWindow		= LastTime;
	CPacketBase const* pPacket = controller.ReadCurrentPacket<CPacketBase>();
	u32 playbackResult = m_eventPlaybackResults;	// Store incase we want to restore

	while(pPacket->GetPacketID() == PACKETID_END_HISTORY || IsRelevantHistoryPacket(pPacket->GetPacketID()))
	{
		if(controller.IsEndOfPlayback())
			break;
		else if(pPacket->GetPacketID() == PACKETID_END_HISTORY)
			break;

		CPacketEvent const* pHistoryPacket = controller.ReadCurrentPacket<CPacketEvent>();
		if(pHistoryPacket->GetGameTime() > endTimeWindow)
			break;

		pPacket = pHistoryPacket;

		PlayBackHistory(controller, pPacket->GetPacketID(), pPacket->GetPacketSize(), endTimeWindow, entityMayBeAbsent);

		pPacket = controller.ReadCurrentPacket<CPacketBase>();
	}

	if(entityMayBeAbsent)
	{	// Restore the state if this is the case...any results about needing to retry for entities is invalid if we're expecting entities to sometimes be missing
		m_eventPlaybackResults |= playbackResult;	
	}

	return true;
}


//////////////////////////////////////////////////////////////////////////
void CReplayEventManager::PlaybackSetup(ReplayController& controller)
{
	eReplayPacketId packetID = controller.GetCurrentPacketID();
	CHistoryPacketController* pPacketController = m_historyPacketControllers.Access((u32)packetID);

	if(pPacketController == NULL)
		return;

	if(pPacketController->m_setupFunction)
		return (this->*pPacketController->m_setupFunction)(controller);
}


//////////////////////////////////////////////////////////////////////////
bool CReplayEventManager::FindPreloadRequests(ReplayController& controller, tPreloadRequestArray& preloadRequests, u32& preloadReqCount, const s32 preloadOffsetTime, const u32 systemTime)
{
	u32 residentCount = preloadRequests.GetCount();
	CPacketBase const* pPacket = controller.ReadCurrentPacket<CPacketBase>();
	bool hasFilledRequestList = false;
	u32 requestCount = 0;

	while(pPacket->GetPacketID() == PACKETID_END_HISTORY || IsRelevantHistoryPacket(pPacket->GetPacketID()))
	{
		CHistoryPacketController* pPacketController = m_historyPacketControllers.Access((u32)pPacket->GetPacketID());

		if(pPacketController->m_preloadFunction)
		{
			++requestCount;
			if(preloadRequests.IsFull())
			{
				hasFilledRequestList = true;
			}
			else
			{
				preloadData data;
				data.pPacket = pPacket;
				data.preloadOffsetTime = preloadOffsetTime;
				data.systemTime = systemTime;
				preloadRequests.Push(data);
			}
		}

		controller.AdvancePacket();
		pPacket = controller.ReadCurrentPacket<CPacketBase>();
	}

	if(hasFilledRequestList)
	{	// Clamp to what we had before this frame
		preloadRequests.Resize(residentCount);
	}
	preloadReqCount = requestCount;	// Return the number of requests that would have been made this frame
	return !hasFilledRequestList;
}


//////////////////////////////////////////////////////////////////////////
int CReplayEventManager::PreloadPackets(tPreloadRequestArray& preloadRequests, tPreloadSuccessArray& successfulPreloads, const CReplayState& replayFlags, const u32 replayTime)
{
	int result = PRELOAD_SUCCESSFUL;
 	for(u32 i = 0; i < preloadRequests.size(); ++i)
 	{
 		const CPacketBase* pPacket = preloadRequests[i].pPacket;
 		
 		CHistoryPacketController* pPacketController = m_historyPacketControllers.Access((u32)pPacket->GetPacketID());

 		if(pPacketController->m_preloadFunction)
		{
			ePreloadResult thisResult = (this->*pPacketController->m_preloadFunction)(pPacket, replayFlags, replayTime, preloadRequests[i].preloadOffsetTime);
			result |= thisResult;
			if(thisResult == PRELOAD_SUCCESSFUL)
			{
				successfulPreloads.Push((s32)i);
			}
			else if(thisResult == PRELOAD_FAIL_MISSING_ENTITY)
			{
				preloadRequests[i].systemTime = fwTimer::GetSystemTimer().GetTimeInMilliseconds();
			}
		}
	}
	return result;
}


//////////////////////////////////////////////////////////////////////////
bool CReplayEventManager::FindPreplayRequests(ReplayController& controller, atArray<const CPacketEvent*>& preplayRequests)
{
	CPacketBase const* pPacket = controller.ReadCurrentPacket<CPacketBase>();

	while(pPacket->GetPacketID() == PACKETID_END_HISTORY || IsRelevantHistoryPacket(pPacket->GetPacketID()))
	{
		CHistoryPacketController* pPacketController = m_historyPacketControllers.Access((u32)pPacket->GetPacketID());

		if(pPacketController->m_playbackFlags & REPLAY_PREPLAY_PACKET)
		{
			const CPacketEvent* pEventPacket = (const CPacketEvent*)pPacket;
			pEventPacket->ValidatePacket();

			if(preplayRequests.Find(pEventPacket) == -1)
			{
				preplayRequests.PushAndGrow(pEventPacket);
			}
		}

		controller.AdvancePacket();
		pPacket = controller.ReadCurrentPacket<CPacketBase>();
	}

	return true;
}


//////////////////////////////////////////////////////////////////////////
int CReplayEventManager::PreplayPackets(const atArray<const CPacketEvent*>& preplayRequests, atArray<s32>& preplaysServiced, const CReplayState& replayFlags, const u32 replayTime, const u32 searchExtentTime)
{
	for(u32 i = 0; i < preplayRequests.size(); ++i)
	{
		const CPacketEvent* pPacket = preplayRequests[i];

		CHistoryPacketController* pPacketController = m_historyPacketControllers.Access((u32)pPacket->GetPacketID());

 		if(pPacketController->m_preplayFunction)
 		{
 			ePreplayResult thisResult = (this->*pPacketController->m_preplayFunction)(pPacket, replayFlags, replayTime, searchExtentTime);
 			if(thisResult == PREPLAY_SUCCESSFUL || thisResult == PREPLAY_FAIL || thisResult == PREPLAY_TOO_LATE)
 			{
 				preplaysServiced.PushAndGrow((s32)i);
 			}
 		}
	}

	return 0;
}


//////////////////////////////////////////////////////////////////////////
eReplayClipTraversalStatus CReplayEventManager::PrintOutHistoryPackets(ReplayController& controller, eReplayPacketOutputMode mode, FileHandle handle)
{
	while(controller.IsEndOfPlayback() == false)
	{
		while(controller.GetCurrentPacketID() != PACKETID_END_HISTORY)
		{
			CHistoryPacketController* pPacketController = m_historyPacketControllers.Access(controller.GetCurrentPacketID());

			if(pPacketController != NULL && pPacketController->m_printFunc != NULL)
			{
				(this->*(pPacketController->m_printFunc))(controller, mode, handle);
			}
			else
			{
				return UNHANDLED_PACKET;
			}
		}
	}
	return END_CLIP;
}

//////////////////////////////////////////////////////////////////////////
#if GTA_REPLAY_OVERLAY
eReplayClipTraversalStatus CReplayEventManager::CalculateBlockStats(ReplayController& controller, CBlockInfo* pBlock)
{
	while(controller.IsEndOfPlayback() == false)
	{
		while(controller.GetCurrentPacketID() != PACKETID_END_HISTORY)
		{
			CHistoryPacketController* pPacketController = m_historyPacketControllers.Access(controller.GetCurrentPacketID());

			if(pPacketController != NULL)
			{
				eReplayPacketId packetId = controller.GetCurrentPacketID();
				const CPacketEvent* pEventPacket = controller.ReadCurrentPacket<CPacketEvent>();
				u32 packetSize = pEventPacket->GetPacketSize();
				bool monitoredPacket = pEventPacket->IsInitialStatePacket();

				pBlock->AddStats(packetId, packetSize, monitoredPacket);
			}
			else
			{
				return UNHANDLED_PACKET;
			}
			controller.AdvancePacket();
		}
	}
	return END_CLIP;
}

void CReplayEventManager::GeneratePacketNameList(eReplayPacketId packetID, const char*& packetName)
{
	CHistoryPacketController* pPacketController = m_historyPacketControllers.Access(packetID);

	if(pPacketController != NULL)
	{
		packetName = pPacketController->m_packetName;
	}
}
#endif //GTA_REPLAY_OVERLAY

//////////////////////////////////////////////////////////////////////////
// Find the correct controller with which to Reset a history packet
// If the correct controller cannot be found then advance over the packet and 
// bail out (This might be taken out in the future).
void CReplayEventManager::ResetPacket(ReplayController& controller, eReplayPacketId packetID, u32 packetSize, u32 uTime)
{
	CHistoryPacketController* pPacketController = m_historyPacketControllers.Access((u32)packetID);

	if(pPacketController == NULL || pPacketController->m_playFunction == NULL || !pPacketController->m_enabled)
	{
		controller.AdvanceUnknownPacket(packetSize);
		return;
	}

	if(pPacketController->m_resetFunction != NULL)
		(this->*(pPacketController->m_resetFunction))(controller, packetID, uTime);
}


//////////////////////////////////////////////////////////////////////////
// Find the correct controller with which to Invalidate a history packet
// If the correct controller cannot be found then advance over the packet and 
// bail out (This might be taken out in the future).
void CReplayEventManager::InvalidatePacket(ReplayController& controller, eReplayPacketId packetID, u32 packetSize)
{
	CHistoryPacketController* pPacketController = m_historyPacketControllers.Access((u32)packetID);
	
	if(pPacketController == NULL || pPacketController->m_playFunction == NULL || !pPacketController->m_enabled)
	{
		controller.AdvanceUnknownPacket(packetSize);
		return;
	}

	if(pPacketController->m_invalidateFunction != NULL)
		(this->*(pPacketController->m_invalidateFunction))(controller);
}


//////////////////////////////////////////////////////////////////////////
bool CReplayEventManager::IsRelevantHistoryPacket(eReplayPacketId packetID)
{
	CHistoryPacketController* pController = m_historyPacketControllers.Access((u32)packetID);
	if(pController == NULL)
		return false;
	else
		return true;
}


//////////////////////////////////////////////////////////////////////////
bool CReplayEventManager::ShouldPlayBackPacket(eReplayPacketId packetId, u32 playBackFlags, s32 replayId, u32 packetTime, u32 startTimeWindow)
{	// Todo
	(void)packetId;
	(void)playBackFlags;
	(void)replayId;
	(void)packetTime;
	(void)startTimeWindow;
	return true;
}

//////////////////////////////////////////////////////////////////////////
template<typename TYPE>
void CReplayEventManager::AddExpiryInfo(eReplayPacketId packetId, CTrackedEventInfo<TYPE>& eventInfoIn)
{
	const CHistoryPacketController* pController = m_historyPacketControllers.Access(packetId);
	const u32 playbackFlags = pController ? pController->m_playbackFlags : 0;

	//only need to add an expiry info if the packet is using the REPLAY_MONITOR_PACKET_JUST_ONE flag.
	if((playbackFlags & REPLAY_MONITOR_PACKET_JUST_ONE) != 0)
	{
		ExpiryInfo* expiryInfo = eventInfoIn.GetExpiryInfo(packetId);

		if( !expiryInfo )
		{
			//Init expiry info
			expiryInfo = eventInfoIn.GetFreeExpiryInfo();
			if(replayVerifyf(expiryInfo, "Failed to an expiry info for packet 0x%08X", packetId))
			{
				expiryInfo->Init(packetId);
			}
		}
		else
		{
			//inc the Expire previous count for this packet
			expiryInfo->Add();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
template<>
void CReplayEventManager::TrackRecordingEvent<tTrackedSoundType>(const CTrackedEventInfo<tTrackedSoundType>& eventInfoIn, CTrackedEventInfo<tTrackedSoundType>*& pEventInfoOut, const bool allowNew, const bool /*endTracking*/, eReplayPacketId packetID)
{
	// Search for the event info...
	CTrackedEventInfo<tTrackedSoundType>* pTrackedInfo = m_audioTrackingController.FindTrackableDuringRecording(eventInfoIn);
	if(pTrackedInfo)
	{
		replayDebugf2("Event Tracking  Old ID %d from sound %p (%p)", pTrackedInfo->trackingID, pTrackedInfo->m_pEffect[0].m_pSound, eventInfoIn.m_pEffect[0].m_pSound);
		
		pEventInfoOut = pTrackedInfo;
	}
	else
	{
		// Event info not found so if we can get a new tracking id and set it
		if(!allowNew)
			return;

		CTrackedEventInfo<tTrackedSoundType>* pTI = m_audioTrackingController.GetNextTrackableDuringRecording();
		if(!pTI)
		{
			replayDebugf1("Can't get a trackable to track an event in %s, %d", __FUNCTION__, __LINE__);
			return;
		}
		pTI->set(eventInfoIn, packetID);
		pEventInfoOut = pTI;
	}

	if(pEventInfoOut != NULL && pEventInfoOut->trackingID != -1)
		AddExpiryInfo(packetID, *pEventInfoOut);

	replayDebugf2("Event Tracking New ID %d from sound %p", pEventInfoOut->trackingID, eventInfoIn.m_pEffect[0].m_pSound);
	return;
}

//////////////////////////////////////////////////////////////////////////
template<>
void CReplayEventManager::TrackRecordingEvent<tTrackedSceneType>(const CTrackedEventInfo<tTrackedSceneType>& eventInfoIn, CTrackedEventInfo<tTrackedSceneType>*& pEventInfoOut, const bool allowNew, const bool /*endTracking*/, eReplayPacketId packetID)
{
	// Search for the event info...
	CTrackedEventInfo<tTrackedSceneType>* pTrackedInfo = m_sceneTrackingController.FindTrackableDuringRecording(eventInfoIn);
	if(pTrackedInfo)
	{
		replayDebugf2("Event Tracking  Old ID %d from scene %p (%p)", pTrackedInfo->trackingID, pTrackedInfo->m_pEffect[0].m_pScene, eventInfoIn.m_pEffect[0].m_pScene);

		pEventInfoOut = pTrackedInfo;
	}
	else
	{
		// Event info not found so if we can get a new tracking id and set it
		if(!allowNew)
			return;

		CTrackedEventInfo<tTrackedSceneType>* pTI = m_sceneTrackingController.GetNextTrackableDuringRecording();
		if(!pTI)
		{
			replayDebugf1("Can't get a trackable to track a scene event in %s, %d", __FUNCTION__, __LINE__);
			return;
		}
		pTI->set(eventInfoIn, packetID);
		pEventInfoOut = pTI;
		replayDebugf2("Event Tracking New ID %d from scene %p", pEventInfoOut->trackingID, eventInfoIn.m_pEffect[0].m_pScene);
	}

	if(pEventInfoOut != NULL && pEventInfoOut->trackingID != -1)
		AddExpiryInfo(packetID, *pEventInfoOut);

	return;
}


//////////////////////////////////////////////////////////////////////////
template<>
void CReplayEventManager::TrackRecordingEvent<ptxEffectInst*>(const CTrackedEventInfo<ptxEffectInst*>& eventInfoIn, CTrackedEventInfo<ptxEffectInst*>*& pEventInfoOut, const bool allowNew, const bool /*endTracking*/, eReplayPacketId packetID)
{
	// Search for the event info...
	CTrackedEventInfo<ptxEffectInst*>* pTrackedInfo = m_ptfxTrackingController.FindTrackableDuringRecording(eventInfoIn);
	if(pTrackedInfo)
	{
		replayDebugf2("Event Tracking  Old ID %d", pTrackedInfo->trackingID);

		pEventInfoOut = pTrackedInfo;
	}
	else
	{
		// Event info not found so if we can get a new tracking id and set it
		if(!allowNew)
			return;

		CTrackedEventInfo<ptxEffectInst*>* pTI = m_ptfxTrackingController.GetNextTrackableDuringRecording();
		if(!pTI)
		{
			replayDebugf1("Can't get a trackable to track an event in %s, %d", __FUNCTION__, __LINE__);
			return;
		}
		pTI->set(eventInfoIn, packetID);
		pEventInfoOut = pTI;
		replayDebugf2("Event Tracking New ID %d", pEventInfoOut->trackingID);
	}

	if(pEventInfoOut != NULL && pEventInfoOut->trackingID != -1)
		AddExpiryInfo(packetID, *pEventInfoOut);

	return;
}

//////////////////////////////////////////////////////////////////////////
template<>
void CReplayEventManager::TrackRecordingEvent<ptxEffectRef>(const CTrackedEventInfo<ptxEffectRef>& eventInfoIn, CTrackedEventInfo<ptxEffectRef>*& pEventInfoOut, const bool allowNew, const bool /*endTracking*/, eReplayPacketId packetID)
{
	// Search for the event info...
	CTrackedEventInfo<ptxEffectRef>* pTrackedInfo = m_ptfxRefTrackingController.FindTrackableDuringRecording(eventInfoIn);
	if(pTrackedInfo)
	{
		replayDebugf2("Event Tracking  Old ID %d", pTrackedInfo->trackingID);

		pEventInfoOut = pTrackedInfo;
	}
	else
	{
		// Event info not found so if we can get a new tracking id and set it
		if(!allowNew)
			return;

		CTrackedEventInfo<ptxEffectRef>* pTI = m_ptfxRefTrackingController.GetNextTrackableDuringRecording();
		if(!pTI)
		{
			replayDebugf1("Can't get a trackable to track an event in %s, %d", __FUNCTION__, __LINE__);
			return;
		}
		pTI->set(eventInfoIn, packetID);
		pEventInfoOut = pTI;
		replayDebugf2("Event Tracking New ID %d", pEventInfoOut->trackingID);
	}

	if(pEventInfoOut != NULL && pEventInfoOut->trackingID != -1)
		AddExpiryInfo(packetID, *pEventInfoOut);
	
	return;
}

//////////////////////////////////////////////////////////////////////////
template<>
void CReplayEventManager::TrackRecordingEvent<ptxFireRef>(const CTrackedEventInfo<ptxFireRef>& eventInfoIn, CTrackedEventInfo<ptxFireRef>*& pEventInfoOut, const bool allowNew, const bool /*endTracking*/, eReplayPacketId packetID)
{
	// Search for the event info...
	CTrackedEventInfo<ptxFireRef>* pTrackedInfo = m_fireRefTrackingController.FindTrackableDuringRecording(eventInfoIn);
	if(pTrackedInfo)
	{
		replayDebugf2("Event Tracking  Old ID %d", pTrackedInfo->trackingID);

		pEventInfoOut = pTrackedInfo;
	}
	else
	{
		// Event info not found so if we can get a new tracking id and set it
		if(!allowNew)
			return;

		CTrackedEventInfo<ptxFireRef>* pTI = m_fireRefTrackingController.GetNextTrackableDuringRecording();
		if(!pTI)
		{
			replayDebugf1("Can't get a trackable to track an event in %s, %d", __FUNCTION__, __LINE__);
			return;
		}
		pTI->set(eventInfoIn, packetID);
		pEventInfoOut = pTI;
		replayDebugf2("Event Tracking New ID %d", pEventInfoOut->trackingID);
	}

	if(pEventInfoOut != NULL && pEventInfoOut->trackingID != -1)
		AddExpiryInfo(packetID, *pEventInfoOut);

	return;
}

//////////////////////////////////////////////////////////////////////////
template<>
void CReplayEventManager::TrackRecordingEvent<tTrackedDecalType>(const CTrackedEventInfo<tTrackedDecalType>& eventInfoIn, CTrackedEventInfo<tTrackedDecalType>*& pEventInfoOut, const bool allowNew, const bool /*endTracking*/, eReplayPacketId packetID)
{
	// Search for the event info...
	CTrackedEventInfo<tTrackedDecalType>* pTrackedInfo = GetController<tTrackedDecalType>().FindTrackableDuringRecording(eventInfoIn);
	if(pTrackedInfo)
	{
		replayDebugf2("Event Tracking  Old ID %d from sound %d (%d)", pTrackedInfo->trackingID, (int)pTrackedInfo->m_pEffect[0], (int)eventInfoIn.m_pEffect[0]);

		pEventInfoOut = pTrackedInfo;
	}
	else
	{
		// Event info not found so if we can get a new tracking id and set it
		if(!allowNew)
			return;

		CTrackedEventInfo<tTrackedDecalType>* pTI = GetController<tTrackedDecalType>().GetNextTrackableDuringRecording();
		if(!pTI)
		{
			replayDebugf1("Can't get a trackable to track an event in %s, %d", __FUNCTION__, __LINE__);
			return;
		}
		pTI->set(eventInfoIn, packetID);
		pEventInfoOut = pTI;
		replayDebugf2("Event Tracking New ID %d from sound %d", pEventInfoOut->trackingID, (int)eventInfoIn.m_pEffect[0]);
	}

	if(pEventInfoOut != NULL && pEventInfoOut->trackingID != -1)
		AddExpiryInfo(packetID, *pEventInfoOut);

	return;
}


//////////////////////////////////////////////////////////////////////////
template<>
void CReplayEventManager::SetTrackedStoredInScratch<tTrackedSoundType>(const CTrackedEventInfo<tTrackedSoundType>& eventInfo)
{
	CTrackedEventInfo<tTrackedSoundType>* pTrackedInfo = m_audioTrackingController.FindTrackableDuringRecording(eventInfo);
	REPLAY_CHECK(pTrackedInfo != NULL, NO_RETURN_VALUE, "Failed to find tracking info");
	pTrackedInfo->storedInScratch = true;
}

//////////////////////////////////////////////////////////////////////////
template<>
void CReplayEventManager::SetTrackedStoredInScratch<tTrackedSceneType>(const CTrackedEventInfo<tTrackedSceneType>& eventInfo)
{
	CTrackedEventInfo<tTrackedSceneType>* pTrackedInfo = m_sceneTrackingController.FindTrackableDuringRecording(eventInfo);
	REPLAY_CHECK(pTrackedInfo != NULL, NO_RETURN_VALUE, "Failed to find tracking info");
	pTrackedInfo->storedInScratch = true;
}

//////////////////////////////////////////////////////////////////////////
template<>
void CReplayEventManager::SetTrackedStoredInScratch<ptxEffectInst*>(const CTrackedEventInfo<ptxEffectInst*>& eventInfo)
{
	CTrackedEventInfo<ptxEffectInst*>* pTrackedInfo = m_ptfxTrackingController.FindTrackableDuringRecording(eventInfo);
	REPLAY_CHECK(pTrackedInfo != NULL, NO_RETURN_VALUE, "Failed to find tracking info");
	pTrackedInfo->storedInScratch = true;
}

//////////////////////////////////////////////////////////////////////////
template<>
void CReplayEventManager::SetTrackedStoredInScratch<ptxEffectRef>(const CTrackedEventInfo<ptxEffectRef>& eventInfo)
{
	CTrackedEventInfo<ptxEffectRef>* pTrackedInfo = m_ptfxRefTrackingController.FindTrackableDuringRecording(eventInfo);
	REPLAY_CHECK(pTrackedInfo != NULL, NO_RETURN_VALUE, "Failed to find tracking info");
	pTrackedInfo->storedInScratch = true;
}

//////////////////////////////////////////////////////////////////////////
template<>
void CReplayEventManager::SetTrackedStoredInScratch<ptxFireRef>(const CTrackedEventInfo<ptxFireRef>& eventInfo)
{
	CTrackedEventInfo<ptxFireRef>* pTrackedInfo = m_fireRefTrackingController.FindTrackableDuringRecording(eventInfo);
	REPLAY_CHECK(pTrackedInfo != NULL, NO_RETURN_VALUE, "Failed to find tracking info");
	pTrackedInfo->storedInScratch = true;
}

//////////////////////////////////////////////////////////////////////////
bool CReplayEventManager::GetPacketName(s32 packetID, const char*& packetName) const
{
	const CHistoryPacketController* pController = m_historyPacketControllers.Access(packetID);
	if(pController == NULL)
		return false;

	packetName = pController->m_packetName;
	return true;
}


//////////////////////////////////////////////////////////////////////////
void CReplayEventManager::GetBlockDetails(char* pStr, bool& err) const
{
	err = false;
	sprintf(pStr, "Event Manager: %d bytes (%d packets), tracking (snd:%d, scene:%d, ptfx:%d, ptfxRef:%d, fireRef:%d, decal:%d), Monitor: b- %d packets, %d bytes, %d peak f- %d packets, %d bytes, %d peak\n\t Peak: %u bytes\n",
		m_blockMonitorBuffer.GetUsed(), 
		GetEventCount(), 
		m_audioTrackingController.GetTrackablesCount(), 
		m_sceneTrackingController.GetTrackablesCount(), 
		m_ptfxTrackingController.GetTrackablesCount(),
		m_ptfxRefTrackingController.GetTrackablesCount(),
		m_fireRefTrackingController.GetTrackablesCount(),
		m_decalTrackingController.GetTrackablesCount(),
		m_blockMonitorBuffer.GetPacketCount(),
		m_blockMonitorBuffer.GetUsed(),
		m_blockMonitorBuffer.GetMemPeak(),
		m_frameMonitorBuffer.GetPacketCount(),
		m_frameMonitorBuffer.GetUsed(),
		m_frameMonitorBuffer.GetMemPeak(),
		m_eventMemPeak);
}


//////////////////////////////////////////////////////////////////////////
void CReplayEventManager::ReleaseAndClear()
{
	m_eventMemFrame = 0;

	ClearAllRecordingTrackables();
}


//////////////////////////////////////////////////////////////////////////
float CReplayEventManager::GetInterpValue(float currVal, float startVal, float endVal, bool clamp)
{
	//this wrapped here as including it raw in the head caused orbis weirdness
	return CVfxHelper::GetInterpValue(currVal, startVal, endVal, clamp);
}


//////////////////////////////////////////////////////////////////////////
void CReplayEventManager::CheckMonitoredEvents(bool notRecording)
{
	sysCriticalSection cs(m_eventBufferCS);
	
	CheckMonitoredEvents(m_blockMonitorBuffer, notRecording);
	CheckMonitoredEvents(m_frameMonitorBuffer, notRecording);

	if(notRecording)
	{
		FreeTrackablesThisFrameDuringRecording();
	}
}


//////////////////////////////////////////////////////////////////////////
// Here we go through all the packets in the buffer and perform tests...
void CReplayEventManager::CheckMonitoredEvents(CReplayPacketBuffer& buffer, bool notRecording)
{
	if(buffer.GetUsed() == 0)
		return;		// No packets to check...

	CPacketEvent* pPreviousPacket = NULL;
	CPacketEvent* pMonitoredPacket = buffer.GetFirstPacket<CPacketEvent>();
	while(pMonitoredPacket != NULL)
	{
		CHistoryPacketController* pController = m_historyPacketControllers.Access(pMonitoredPacket->GetPacketID());
		if(!pController)
		{
			// Something went badly wrong here...
			replayFatalAssertf((u8*)pMonitoredPacket > buffer.GetBuffer(), "Invalid pointer %p, %p", pMonitoredPacket, buffer.GetBuffer());
			BANK_ONLY(DumpReplayPackets((char*)pMonitoredPacket, (u32)((u8*)pMonitoredPacket - buffer.GetBuffer())));
			replayAssertf(false, "Failed to find controller for a packet...see log");

			buffer.Reset();	// Reset the buffer...
			return;
		}

		// Update Monitored packet data
		if(pController->m_UpdateMonitoredPacketFunc)
			(this->*(pController->m_UpdateMonitoredPacketFunc))(pMonitoredPacket, *pController);

		// Check whether the packet has 'Expired'...
		replayAssert(pController && pController->m_checkExpiryFunc);
		bool removePacket = (this->*(pController->m_checkExpiryFunc))(pMonitoredPacket, *pController);

		// If it has just been put in the buffer then we need to check the tracking...
		if(notRecording && pController->m_checkTrackingFunc && buffer.IsPacketRecent((u8*)pMonitoredPacket))
		{
			(this->*(pController->m_checkTrackingFunc))(pMonitoredPacket);
		}

		// If the buffer is nearly full and it's a low priority packet then we can remove it
		if(!removePacket && (pController->m_playbackFlags & REPLAY_MONITOR_PACKET_LOW_PRIO) && IsRecorderNearlyFull(buffer.GetRecorder()))
		{
			removePacket = true;
		}

		// Remove or go to the next packet in the buffer
		if(removePacket)
		{
			pMonitoredPacket = buffer.RemovePacket<CPacketEvent>(pMonitoredPacket);
		}
		else
		{
			pPreviousPacket = pMonitoredPacket;
			pMonitoredPacket = buffer.GetNextPacket<CPacketEvent>(pMonitoredPacket);
		}

		// Run some checks...
		if(pPreviousPacket && pMonitoredPacket)
		{
			replayAssertf(pPreviousPacket != pMonitoredPacket, "Previous packet should not equal current %p and %p", pPreviousPacket, pMonitoredPacket);
			pPreviousPacket->ValidatePacket();
			pMonitoredPacket->ValidatePacket();
			replayAssertf(((u8*)pPreviousPacket + pPreviousPacket->GetPacketSize()) == (u8*)pMonitoredPacket, "Previous and current packet mismatch... %p size - %u, %p", pPreviousPacket, pPreviousPacket->GetPacketSize(), pMonitoredPacket);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
bool CReplayEventManager::ShouldProceedWithPacket(const CPacketEvent& packet, bool recordPacket, bool monitorPacket) const
{
	if(m_historyPacketControllers.GetNumUsed() < 1)
		return false;	// No controllers set up yet so bail

	u32 packetSize = packet.GetPacketSize();
	bool shouldProceed = false;
	if(recordPacket)
	{
		if(m_eventRecorder.IsSpaceForPacket(packet.GetPacketSize()) == true)
		{
			shouldProceed = true;
		}
	}

	if(!shouldProceed && monitorPacket)
	{
		const CHistoryPacketController* pPacketController = m_historyPacketControllers.Access((u32)packet.GetPacketID());
		replayFatalAssertf(pPacketController, "Failed to find controller for packet %u...did you forget to register it?", (u32)packet.GetPacketID());
		if(pPacketController && pPacketController->m_playbackFlags & REPLAY_MONITOR_PACKET_FLAGS)
		{
			if(pPacketController->m_playbackFlags & REPLAY_MONITOR_PACKET_FRAME)
			{
				if(m_frameMonitorBuffer.GetRecorder().IsSpaceForPacket(packetSize))
				{
					shouldProceed = true;
				}
				else
				{
					if(pPacketController->m_playbackFlags & REPLAY_MONITOR_PACKET_HIGH_PRIO)
					{
						static bool allowForce = true;
						DumpMonitoredEventsToTTY(allowForce);
						allowForce = false;
						replayAssertf(false, "Can't store this high priority packet (%s) in the FRAME monitor buffer as it is full!  (See Log for the spam)", pPacketController->m_packetName);
					}
				}
			}
			else if(m_blockMonitorBuffer.GetRecorder().IsSpaceForPacket(packetSize))
			{
				shouldProceed = true;
			}
			else
			{
				if(pPacketController->m_playbackFlags & REPLAY_MONITOR_PACKET_HIGH_PRIO)
				{
					static bool allowForce = true;
					DumpMonitoredEventsToTTY(allowForce);
					allowForce = false;
					replayAssertf(false, "Can't store this high priority packet (%s) in the BLOCK monitor buffer as it is full!  (See Log for the spam)", pPacketController->m_packetName);
				}
			}
		}
	}

	return shouldProceed;
}


//////////////////////////////////////////////////////////////////////////
void CReplayEventManager::DumpMonitoredEventsToTTY(bool force) const
{
	DumpMonitoredEventsToTTY(m_blockMonitorBuffer, force);
	DumpMonitoredEventsToTTY(m_frameMonitorBuffer, force);
}


//////////////////////////////////////////////////////////////////////////
void CReplayEventManager::DumpMonitoredEventsToTTY(const CReplayPacketBuffer& BANK_ONLY(buffer), bool BANK_ONLY(force)) const
{
#if __BANK
	if(buffer.ShouldSpamTTY() == false && !force)
		return;

	sysCriticalSection cs(m_eventBufferCS);

	replayLogf(DIAG_SEVERITY_DEBUG1, "==============================");
	replayLogf(DIAG_SEVERITY_DEBUG1, "Begin monitored events dump... %s", buffer.GetName());
	const CPacketEvent* pMonitoredPacket = buffer.GetFirstPacket<CPacketEvent>();
	while(pMonitoredPacket != NULL)
	{
		const CHistoryPacketController* pController = m_historyPacketControllers.Access(pMonitoredPacket->GetPacketID());

		replayLogf(DIAG_SEVERITY_DEBUG1, "%s", pController->m_packetName);

		pMonitoredPacket = buffer.GetNextPacket<CPacketEvent>(pMonitoredPacket);
	}
	replayLogf(DIAG_SEVERITY_DEBUG1, "==============================");
#endif // __BANK
}


//////////////////////////////////////////////////////////////////////////
void CReplayEventManager::GetMonitorStats(u32& count, u32& bytes, u32& count1, u32& bytes1) const
{
	count = m_blockMonitorBuffer.GetPacketCount();
	bytes = m_blockMonitorBuffer.GetUsed();
	count1 = m_frameMonitorBuffer.GetPacketCount();
	bytes1 = m_frameMonitorBuffer.GetUsed();
}


//////////////////////////////////////////////////////////////////////////
void CReplayEventManager::RecordEvents(ReplayController& controller, bool blockChange, u32 ASSERT_ONLY(expectedSize))
{
	u8* pEventBuffer = m_eventRecorder.GetBuffer();
	m_eventMemUsed = m_eventRecorder.GetMemUsed();

	CycleEventBuffers();

	m_eventBufferCS.Unlock();

	ASSERT_ONLY(u32 currPos = controller.GetCurrentPosition();)

	CAddressInReplayBuffer address = controller.GetAddress();
	ReplayController positionBeforeRecord(address, controller.GetBufferInfo(), controller.GetCurrentFrameRef());

	controller.RecordBuffer(pEventBuffer, m_eventMemUsed);

	if(blockChange)
		controller.RecordBuffer(m_blockMonitorBuffer.GetBuffer(), m_blockMonitorBuffer.GetUsed());
	
	controller.RecordBuffer(m_frameMonitorBuffer.GetBuffer(), m_frameMonitorBuffer.GetUsed());

	controller.DisableRecording();
	positionBeforeRecord.EnableModify();
	ProcessRecordedEvents(positionBeforeRecord, controller.GetCurrentPosition());
	positionBeforeRecord.DisableModify();
	controller.RenableRecording();

	FreeTrackablesThisFrameDuringRecording();

	replayFatalAssertf((controller.GetCurrentPosition() - currPos) == expectedSize, "Incorrect size...expected %u, got %u (blockChange=%d)", expectedSize, controller.GetCurrentPosition() - currPos, blockChange == true ? 1 : 0);
}


//////////////////////////////////////////////////////////////////////////
void CReplayEventManager::ProcessRecordedEvents(ReplayController& controller, u32 endPosition)
{
	m_eventCount = 0;
	while(controller.GetCurrentPosition() != endPosition)
	{
		CPacketEvent* pBasePacket = controller.GetCurrentPacket<CPacketEvent>();

		CHistoryPacketController* pController = m_historyPacketControllers.Access(pBasePacket->GetPacketID());
		pBasePacket->ValidatePacket();
		replayAssertf(pController, "Failed to find controller for packet ID %u", pBasePacket->GetPacketID());
		if(!pController)
		{
			DumpReplayPackets((char*)pBasePacket, pBasePacket->GetPacketSize());
		}
		++m_eventCount;

		if(pController)
		{
			CReplayID replayID;
			u8 i = 0;
			while((this->*(pController->m_getReplayIDFunc))(pBasePacket, i++, replayID))
			{
				if(replayID != ReplayIDInvalid && replayID != NoEntityID)
				{
					if(m_interfaceManager->IsCurrentlyBeingRecorded(replayID) == false)
					{
						// Entity for this event is not currently being recorded....why?
						if(m_interfaceManager->IsRecentlyDeleted(replayID))
						{	// Entity was deleted this frame
							replayDebugf1("Entity was deleted this frame 0x%08X, packet %s cancelled", replayID.ToInt(), pController->m_packetName);
						}
						else
						{
							if(m_interfaceManager->FindEntity(replayID))
							{	// Entity exists but not been recorded yet...
								replayDebugf1("Entity is in the pool but not recently recorded....just created? %d, packet %s cancelled", replayID.ToInt(), pController->m_packetName);
							}
							else
							{	// Hmmm....
								replayDebugf1("I've no idea where this packet's entity has come from or where it's gone (0x%08X), packet %s cancelled", replayID.ToInt(), pController->m_packetName);
							}
						}

						pBasePacket->Cancel();
					}
					else
					{
						m_interfaceManager->SetCreationUrgent(replayID);
					}
				}
			}


			// Link with other packets
			// If there is a link function pointer call it
			// 
			if(pController->m_linkPacketsFunc != NULL)
			{
				(this->*(pController->m_linkPacketsFunc))(pBasePacket, controller);
			}
		}

		controller.AdvancePacket();
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayEventManager::FreeTrackablesThisFrameDuringRecording()
{
	m_audioTrackingController.FreeTrackablesThisFrameDuringRecording();
	m_sceneTrackingController.FreeTrackablesThisFrameDuringRecording();
	m_ptfxTrackingController.FreeTrackablesThisFrameDuringRecording();
	m_ptfxRefTrackingController.FreeTrackablesThisFrameDuringRecording();
	m_fireRefTrackingController.FreeTrackablesThisFrameDuringRecording();
	m_decalTrackingController.FreeTrackablesThisFrameDuringRecording();
}


//////////////////////////////////////////////////////////////////////////
void CReplayEventManager::ClearAllRecordingTrackables()
{
	m_audioTrackingController.ClearAllRecordingTrackables();
	m_sceneTrackingController.ClearAllRecordingTrackables();
	m_ptfxTrackingController.ClearAllRecordingTrackables();
	m_ptfxRefTrackingController.ClearAllRecordingTrackables();
	m_fireRefTrackingController.ClearAllRecordingTrackables();
	m_decalTrackingController.ClearAllRecordingTrackables();
}

//////////////////////////////////////////////////////////////////////////
void CReplayEventManager::CleanAllTrackables()
{
	m_sceneTrackingController.CleanAllRecordingTrackables();
}

//////////////////////////////////////////////////////////////////////////
void CReplayEventManager::SetPlaybackTrackablesStale()
{
	m_audioTrackingController.SetPlaybackTrackablesStale();
	m_sceneTrackingController.SetPlaybackTrackablesStale();
	m_ptfxTrackingController.SetPlaybackTrackablesStale();
	m_ptfxRefTrackingController.SetPlaybackTrackablesStale();
	m_fireRefTrackingController.SetPlaybackTrackablesStale();
	m_decalTrackingController.SetPlaybackTrackablesStale();
}


//////////////////////////////////////////////////////////////////////////
void CReplayEventManager::ResetSetupInformation()
{
	m_audioTrackingController.ResetSetupInformation();
	m_sceneTrackingController.ResetSetupInformation();
	m_ptfxTrackingController.ResetSetupInformation();
	m_ptfxRefTrackingController.ResetSetupInformation();
	m_fireRefTrackingController.ResetSetupInformation();
	m_decalTrackingController.ResetSetupInformation();
}


//////////////////////////////////////////////////////////////////////////
void CReplayEventManager::CheckAllTrackablesDuringRecording()
{
	m_audioTrackingController.CheckAllTrackablesDuringRecording();
	m_sceneTrackingController.CheckAllTrackablesDuringRecording();
	m_ptfxTrackingController.CheckAllTrackablesDuringRecording();
	m_ptfxRefTrackingController.CheckAllTrackablesDuringRecording();
	m_fireRefTrackingController.CheckAllTrackablesDuringRecording();
	m_decalTrackingController.CheckAllTrackablesDuringRecording();
}


//////////////////////////////////////////////////////////////////////////
s32 CReplayEventManager::GetTrackablesCount() const
{
	return m_audioTrackingController.GetTrackablesCount() + m_sceneTrackingController.GetTrackablesCount() + m_ptfxTrackingController.GetTrackablesCount() + m_ptfxRefTrackingController.GetTrackablesCount() + m_fireRefTrackingController.GetTrackablesCount() + m_decalTrackingController.GetTrackablesCount();
}


//////////////////////////////////////////////////////////////////////////
void CReplayEventManager::RemoveInterpEvents()
{
	m_audioTrackingController.RemoveInterpEvents();
	m_sceneTrackingController.RemoveInterpEvents();
	m_ptfxTrackingController.RemoveInterpEvents();
	m_ptfxRefTrackingController.RemoveInterpEvents();
	m_fireRefTrackingController.RemoveInterpEvents();
	m_decalTrackingController.RemoveInterpEvents();
}


//////////////////////////////////////////////////////////////////////////
template<>
CReplayTrackingController<tTrackedSoundType>& CReplayEventManager::GetController<tTrackedSoundType>()
{
	return m_audioTrackingController;
}

template<>
const CReplayTrackingController<tTrackedSoundType>& CReplayEventManager::GetController<tTrackedSoundType>() const
{
	return m_audioTrackingController;
}

//////////////////////////////////////////////////////////////////////////
template<>
CReplayTrackingController<tTrackedSceneType>& CReplayEventManager::GetController<tTrackedSceneType>()
{
	return m_sceneTrackingController;
}

template<>
const CReplayTrackingController<tTrackedSceneType>& CReplayEventManager::GetController<tTrackedSceneType>() const
{
	return m_sceneTrackingController;
}

//////////////////////////////////////////////////////////////////////////
template<>
CReplayTrackingController<ptxEffectInst*>& CReplayEventManager::GetController<ptxEffectInst*>()
{
	return m_ptfxTrackingController;
}

template<>
const CReplayTrackingController<ptxEffectInst*>& CReplayEventManager::GetController<ptxEffectInst*>() const
{
	return m_ptfxTrackingController;
}
//////////////////////////////////////////////////////////////////////////
template<>
CReplayTrackingController<ptxEffectRef>& CReplayEventManager::GetController<ptxEffectRef>()
{
	return m_ptfxRefTrackingController;
}

template<>
const CReplayTrackingController<ptxEffectRef>& CReplayEventManager::GetController<ptxEffectRef>() const
{
	return m_ptfxRefTrackingController;
}

//////////////////////////////////////////////////////////////////////////
template<>
CReplayTrackingController<ptxFireRef>& CReplayEventManager::GetController<ptxFireRef>()
{
	return m_fireRefTrackingController;
}

template<>
const CReplayTrackingController<ptxFireRef>& CReplayEventManager::GetController<ptxFireRef>() const
{
	return m_fireRefTrackingController;
}


//////////////////////////////////////////////////////////////////////////
void CReplayEventManager::Reset()
{
	ClearAllRecordingTrackables();
	m_blockMonitorBuffer.Reset();
	m_frameMonitorBuffer.Reset();

	// The monitor buffers have been reset so reset all the guards for the packet types that only allow one in the monitor buffer
	for(int i = 0; i < m_historyPacketControllers.GetNumSlots(); ++i)
	{
		atMap<u32, CHistoryPacketController>::Entry* entry = m_historyPacketControllers.GetEntry(i);
		if( entry )
		{
			CHistoryPacketController& pController = entry->data;
			pController.m_expirePrevious = 0;
		}
		
	}
}

void CReplayEventManager::Cleanup()
{
	CleanAllTrackables();
}

void CReplayEventManager::ClearCurrentEvents()
{
	CycleEventBuffers();
}

//////////////////////////////////////////////////////////////////////////
#if __BANK
void CReplayEventManager::AddDebugWidgets()
{
	bkBank* pBank = BANKMGR.FindBank("Replay");
	if(pBank == NULL)
		return;

	bkGroup* pGroup = pBank->AddGroup("Events", false);
	(void)pGroup;
}
#endif // __BANK

//////////////////////////////////////////////////////////////////////////
template<>
CEntity* CReplayEventManager::FindEntity<CEntity>(s32 entityID, u16 staticIndex, bool mustExist) const
{
	if(entityID == ReplayIDInvalid || entityID == NoEntityID)
		return NULL;

	CEntityGet<CEntity> entityGet(entityID);
	m_interfaceManager->GetEntity<CEntity>(entityGet);
	CEntity* pEntity = entityGet.m_pEntity;

	if(!pEntity && entityID != -1 && entityGet.m_alreadyReported == false)
	{
		if(staticIndex == NON_STATIC_BOUNDS_BUILDING)
		{
			CBuilding* pBuilding = CReplayMgr::GetBuilding(entityID);	// Fugly going back to CReplayMgr...
			if(pBuilding)
			{
				pEntity = pBuilding;
			}
		}

		if(!pEntity)
		{
			strLocalIndex slot = g_StaticBoundsStore.FindSlotFromHashKey(entityID);
			if(slot.IsValid())
			{
				rage::fwBoundData* pData = g_StaticBoundsStore.GetSlotData(slot.Get());
				if (pData && pData->m_physInstCount > 0)
				{
					s32 index = staticIndex;
					replayAssertf(index >= 0 && index < pData->m_physInstCount, "Incorrect PhysicsInst Index");
					if(index < 0 || index >= pData->m_physInstCount)
						return NULL;

					phInst* pInst = pData->m_physicsInstArray[index];
					pEntity = (CEntity*)pInst->GetUserData();
				}
				else
				{
					replayDebugf1("fwBoundDef is empty (entityID %d, staticIndex %u, slot %d) - this effect will not appear, probably due to the entity not streaming in fast enough.", entityID, staticIndex, slot.Get());
					return NULL;
				}
			}
		}
	}

	if(mustExist)
	{
		replayAssertf(pEntity || entityGet.m_alreadyReported, "Failed to find entity for event 0x%08X", entityID);
	}
	return pEntity;
}


//////////////////////////////////////////////////////////////////////////
template<>
void* CReplayEventManager::FindEntity<void>(s32, u16, bool) const
{	replayFatalAssertf(false, "Should not get here");	return NULL;	}


template<>
void CReplayEventManager::ValidateEntityCount<void>(u8 ASSERT_ONLY(numEntities), const char* ASSERT_ONLY(pPacketName))
{	replayFatalAssertf(numEntities == 0, "Error validating %s - Number of entities is incorrect", pPacketName);	}

#endif // GTA_REPLAY
