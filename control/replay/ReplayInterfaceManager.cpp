#include "ReplayInterfaceManager.h"

#if GTA_REPLAY

#include "scene/Entity.h"

#include "Replay.h"
#include "Control/Replay/ReplayController.h"
#include "control/replay/ReplayInterface.h"

s16 iReplayInterface::m_instanceID = 0;

//////////////////////////////////////////////////////////////////////////
CReplayInterfaceManager::CReplayInterfaceManager()
	: m_frameHash(0)
{
	m_interfaces.Reserve(10);
}


//////////////////////////////////////////////////////////////////////////
CReplayInterfaceManager::~CReplayInterfaceManager()
{

}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceManager::AddInterface(iReplayInterface* pInterface)
{
	m_interfaces.Push(pInterface);
}


//////////////////////////////////////////////////////////////////////////
iReplayInterface* CReplayInterfaceManager::FindCorrectInterface(const CEntity* pEntity)
{
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];
		if(pInterface->MatchesType(pEntity))
			return pInterface;
	}

	replayDebugf2("Failed to find interface for entity type %d", pEntity->GetType());
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
bool CReplayInterfaceManager::GetPacketGroup(eReplayPacketId packetID, u32& packetGroup)
{
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];

		if(pInterface->GetPacketGroup(packetID, packetGroup))
			return true;
	}

	return false;
}


//////////////////////////////////////////////////////////////////////////
iReplayInterface* CReplayInterfaceManager::FindCorrectInterface(const atHashWithStringNotFinal& type)
{
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];

		if(pInterface->MatchesType(type))
			return pInterface;
	}

	replayDebugf2("Failed to find interface for entity hash %d", type.GetHash());
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
iReplayInterface* CReplayInterfaceManager::FindCorrectInterface(const eReplayPacketId packetType)
{
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		if(m_interfaces[i]->IsRelevantPacket(packetType))
			return m_interfaces[i];
	}

	return NULL;
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceManager::Reset()
{
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];
		pInterface->Reset();
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceManager::Clear()
{
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];
		pInterface->Clear();
	}
}

//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceManager::EnableRecording()
{
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];
		pInterface->EnableRecording();
	}
}

//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceManager::ReleaseAndClear()
{
 	//delete all the interfaces and reset the interface list, these are newed in replay.cpp::CReplayMgr::Init()
 	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
 	{
		m_interfaces[i]->FlushPreloadRequests();

		sysMemAutoUseAllocator replayAlloc(*MEMMANAGER.GetReplayAllocator());
 		delete m_interfaces[i];
 		m_interfaces[i] = NULL;
 	}
 
 	m_interfaces.ResetCount();
}


//////////////////////////////////////////////////////////////////////////
// Grab the memory required for this frame from each interface and add them
// together
s32 CReplayInterfaceManager::GetMemoryUsageForFrame(bool blockChange, atArray<u32>& expectedSizes)
{
	s32 totalUsage = 0;
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];

		if(pInterface->DoSizing())
		{
			s32 usage = pInterface->GetMemoryUsageForFrame(blockChange);

			//replayDisplayf("%s: %d bytes for %d elements.\n", pInterface->GetShortStr(), pInterface->GetEstimatedMemoryUsage(), pInterface->GetNoOfElementsToRecord());
			//pInterface->DisplayEstimatedMemoryUsageBreakdown();
			expectedSizes.PushAndGrow(usage);
			totalUsage += usage;
		}
	}

	return totalUsage;
}


//////////////////////////////////////////////////////////////////////////
// Return true if this interface was able to set up this packet...
// false otherwise
bool CReplayInterfaceManager::PlaybackSetup(ReplayController& controller)
{
	eReplayPacketId packetID = controller.GetCurrentPacketID();
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];

		if(pInterface->IsRelevantPacket(packetID))
		{
			pInterface->PlaybackSetup(controller);
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CReplayInterfaceManager::ApplyFades(ReplayController& controller, s32 frame)
{
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];
		eReplayPacketId packetID = controller.GetCurrentPacketID();
		if(pInterface->IsRelevantUpdatePacket(packetID) || pInterface->IsRelevantDeletePacket(packetID))
		{
			pInterface->ApplyFades(controller, frame);
			return true;
		}
	}

	return false;
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceManager::ApplyFadesSecondPass()
{
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];
		pInterface->ApplyFadesSecondPass();
	}
}

//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceManager::RecordFrame(u32 threadIndex, u16 sessionBlockIndex)
{
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];

		if (pInterface->DoRecording() && (pInterface->GetPreferredRecordingThreadIndex() % CReplayMgrInternal::GetRecordingThreadCount()) == (int)threadIndex)
		{
			PIX_AUTO_TAG(1,pInterface->GetLongFriendlyStr());
			pInterface->RecordFrame(sessionBlockIndex);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
u32	CReplayInterfaceManager::UpdateFrameHash()
{
	u32 frameHash = 0;
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];

		u32 h = pInterface->GetFrameHash();
		frameHash = CReplayMgr::hash(&h, 1, frameHash);
	}

	return m_frameHash = frameHash;
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceManager::PostRecordFrame(ReplayController& controller, bool blockChange, atArray<u32>& ASSERT_ONLY(expectedSizes), int& ASSERT_ONLY(expectedSizeIndex))
{
	FrameRef currentFrameRef = controller.GetCurrentFrameRef();
	u16 previousBlockFrameCount = 0;
	if(currentFrameRef.GetBlockIndex() > 0)
	{
		u16 previousBlockIndex = currentFrameRef.GetBlockIndex() - 1;
		CBlockInfo* pPrevBlock = controller.GetBufferInfo().FindBlockFromSessionIndex(previousBlockIndex);
		replayFatalAssertf(pPrevBlock, "pPrevBlock is NULL with session index of %u", previousBlockIndex);
		previousBlockFrameCount = (u16)pPrevBlock->GetFrameCount();
	}

	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];

		if(pInterface->DoRecording())
		{
			ASSERT_ONLY(u32 currPos = controller.GetCurrentPosition(); u32 expectedSize = expectedSizes[expectedSizeIndex++];)

			pInterface->PostRecordFrame(controller, previousBlockFrameCount, blockChange);

			ASSERT_ONLY(s32 sizeDiff = controller.GetCurrentPosition() - currPos;)
			replayFatalAssertf((expectedSize - sizeDiff) <= REPLAY_DEFAULT_ALIGNMENT, "Incorrect size...expected %u, got %u (blockChange=%d)", expectedSize, controller.GetCurrentPosition() - currPos, blockChange == true ? 1 : 0);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceManager::PostRecordOptimizations(ReplayController& controller, const CAddressInReplayBuffer& endAddress)
{
	controller.EnableModify();

	while (controller.GetAddress().GetPosition() < endAddress.GetPosition())
	{
		CPacketBase* pPacket = controller.GetCurrentPacket<CPacketBase>();
		if (!replayVerifyf(pPacket, "Packet is NULL!"))
		{
			break;
		}

		for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
		{
			iReplayInterface* pInterface = m_interfaces[i];

			if(pInterface->DoRecording())
			{
				if (pInterface->IsRelevantOptimizablePacket(pPacket->GetPacketID()))
				{
					pInterface->PostRecordOptimize(pPacket);
					break;
				}
			}
		}

		controller.AdvancePacket();
	}

	controller.DisableModify();

	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];

		if(pInterface->DoRecording())
		{
			pInterface->ResetPostRecordOptimizations();
		}
	}
	
}

//////////////////////////////////////////////////////////////////////////
#define DO_PREUPDATE_ONLY 0	// For debugging purposes
ePlayPktResult CReplayInterfaceManager::PlayPackets(ReplayController& controller, bool preprocessOnly, bool entityMayNotExist)
{
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];

		if(pInterface->DoPlayback() && pInterface->IsRelevantPacket(controller.GetCurrentPacketID()))
		{
			PF_AUTO_PUSH_TIMEBAR(pInterface->GetShortStr());
			if(preprocessOnly)
			{
				pInterface->PreprocessPackets(controller, entityMayNotExist);
			}
			else
			{
#if !DO_PREUPDATE_ONLY
				pInterface->PlayPackets(controller, entityMayNotExist);
#else
				return PACKET_UNHANDLED;
#endif
			}
			return PACKET_PLAYED;
		}
	}

	return PACKET_UNHANDLED;
}


//////////////////////////////////////////////////////////////////////////
ePlayPktResult	CReplayInterfaceManager::JumpPackets(ReplayController& controller)
{
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];

		if(pInterface->DoPlayback() && pInterface->IsRelevantPacket(controller.GetCurrentPacketID()))
		{
			pInterface->JumpPackets(controller);
			return PACKET_PLAYED;
		}
	}

	return PACKET_UNHANDLED;
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceManager::Process()
{
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];

		pInterface->Process();
	}
}

//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceManager::PostProcess()
{
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];

		pInterface->PostProcess();
	}
}

//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceManager::PostRender()
{
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];

		pInterface->PostRender();
	}
}

//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceManager::OnCreateEntity(CPhysical* pEntity)
{
	iReplayInterface* pInterface = FindCorrectInterface(pEntity);

	if(pInterface && pInterface->ShouldRegisterElement(pEntity))
	{
		// Set invalid frame ref for now....this will get updated when the first creation packet is recorded
		pInterface->RegisterForRecording(pEntity, FrameRef::INVALID_FRAME_REF);	
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceManager::OnDeleteEntity(CPhysical* pEntity)
{
	static bool typesSeenToFail[ENTITY_TYPE_TOTAL] = {false};
	if(typesSeenToFail[pEntity->GetType()])
	{
		// We already know we've tried to delete this type unsuccessfully
		return;
	}
	iReplayInterface* pInterface = FindCorrectInterface(pEntity);
	// Not the end of the world if this is NULL...an entity is being delete
	// that the replay doesn't care about.
	if(pInterface)
	{
		pInterface->OnDelete(pEntity);

		CReplayExtensionManager::RemoveReplayExtensions(pEntity);
		pEntity->SetReplayID(ReplayIDInvalid);
	}
	else
	{
		if(m_interfaces.GetCount() > 0)
		{
			// Add to the list so we don't unnecessarily check for this type again
			typesSeenToFail[pEntity->GetType()] = true;
		}
		else
		{
			replayAssertf(pEntity->GetReplayID() == ReplayIDInvalid, "Deleting an entity but the ID is still valid 0x%08X", pEntity->GetReplayID().ToInt());
		}
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceManager::ResetAllEntities()
{
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];

		pInterface->ResetAllEntities();
	}
}

//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceManager::StopAllEntitySounds()
{
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];

		pInterface->StopAllEntitySounds();
	}
}

//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceManager::CreateDeferredEntities(ReplayController& controller)
{
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];

		pInterface->CreateDeferredEntities(controller);
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceManager::ProcessRewindDeletions(ReplayController& controller)
{
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];

		pInterface->ProcessRewindDeletions(controller.GetCurrentFrameRef());
	}
}


//////////////////////////////////////////////////////////////////////////
bool CReplayInterfaceManager::CreateBatchedEntities(const CReplayState& state)
{
	s32 entitiesCreated = 0;
	bool stillHasSomeToProcess = false;
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];

		pInterface->CreateBatchedEntities(entitiesCreated, state);

		if(entitiesCreated >= entityCreationLimitPerFrame)
			return true;

		stillHasSomeToProcess |= pInterface->HasBatchedEntitiesToProcess();
	}
	
	return stillHasSomeToProcess;
}


//////////////////////////////////////////////////////////////////////////
bool CReplayInterfaceManager::HasBatchedEntitiesToProcess()
{
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];

		if(pInterface->HasBatchedEntitiesToProcess())
			return true;
	}

	return false;
}


//////////////////////////////////////////////////////////////////////////
bool CReplayInterfaceManager::GetPacketName(eReplayPacketId packetID, const char*& packetName) const
{
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];

		if(pInterface->GetPacketName(packetID, packetName))
			return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceManager::ResetCreatePacketsForCurrentBlock()
{
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];
		pInterface->ResetCreatePacketsForCurrentBlock();
	}
}

//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceManager::LinkCreatePacketsForNewBlock(ReplayController& /*controller*/)
{
// 	while(controller.IsEndOfBlock() == false)
// 	{
// 		const CPacketBase* pPacket = controller.ReadCurrentPacket<CPacketBase>();
// 		for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
// 		{
// 			iReplayInterface* pInterface = m_interfaces[i];
// 			if(pInterface->IsRelevantPacket(pPacket->GetPacketID()))//  pInterface->IsRelevantUpdatePacket(pPacket->GetPacketID()) || pInterface->IsRelevantDeletePacket(pPacket->GetPacketID()))
// 			{
// 				pInterface->LinkCreatePackets(controller, 0);
// 				break;
// 			}
// 		}
// 		controller.AdvancePacket();
// 	}

// 	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
// 	{
// 		iReplayInterface* pInterface = m_interfaces[i];
// 
// 		if(pInterface->IsRelevantUpdatePacket(pPacket->GetPacketID()) || pInterface->IsRelevantDeletePacket(pPacket->GetPacketID()))
// 		{
// 			pInterface->LinkCreatePacket(pPacket);
// 			return;
// 		}
// 	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceManager::OffsetCreatePackets(s64 offset)
{
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];
		pInterface->OffsetCreatePackets(offset);
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceManager::OffsetUpdatePackets(s64 offset, u8* pStart, u32 blockSize)
{
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];
		pInterface->OffsetUpdatePackets(offset, pStart, blockSize);
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceManager::PreloadPackets(tPreloadRequestArray& preloadRequests, tPreloadSuccessArray& successfulPreloads, u32 currGameTime)
{
	for(u32 i = 0; i < preloadRequests.size(); ++i)
	{
		bool foundInterface = false;
		const CPacketBase* pPacket = preloadRequests[i].pPacket;
		PRELOADDEBUG_ONLY(preloadRequests[i].failureReason.Reset();)
		for(s32 j = 0; j < m_interfaces.GetCount(); ++j)
		{
			iReplayInterface* pInterface = m_interfaces[j];
			bool requestIsLoaded = false;
			if(pInterface->TryPreloadPacket(pPacket, requestIsLoaded, currGameTime PRELOADDEBUG_ONLY(, preloadRequests[i].failureReason)))
			{
				foundInterface = true;
				if(requestIsLoaded)
				{
					successfulPreloads.Push((s32)i);
				}
				break;
			}
		}

		if(!foundInterface)
		{
			replayAssertf(false, "Unable to find interface for preload packet %d", pPacket ? pPacket->GetPacketID() : -1);
			successfulPreloads.Push((s32)i);
		}
	}


	for(s32 j = 0; j < m_interfaces.GetCount(); ++j)
	{
		iReplayInterface* pInterface = m_interfaces[j];
		pInterface->UpdatePreloadRequests(currGameTime);
	}
}


//////////////////////////////////////////////////////////////////////////
bool CReplayInterfaceManager::FindPreloadRequests(ReplayController& controller, tPreloadRequestArray& preloadRequests, u32& preloadReqCount, const u32 systemTime, bool isSingleFrame)
{
	u32 residentCount = preloadRequests.GetCount();
	bool hasFilledRequestList = false;
	u32 requestCount = 0;

	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];
		if(!pInterface->FindPreloadRequests(controller, preloadRequests, requestCount, systemTime, isSingleFrame))
		{
			hasFilledRequestList = true;
		}
	}

	if(hasFilledRequestList)
	{	// Clamp to what we had before this frame
		preloadRequests.Resize(residentCount);
	}
	preloadReqCount = requestCount;	// Return the number of requests that would have been made this frame
	return !hasFilledRequestList;
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceManager::FlushPreloadRequests()
{
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];
		pInterface->FlushPreloadRequests();
	}
}

//////////////////////////////////////////////////////////////////////////
eReplayClipTraversalStatus CReplayInterfaceManager::OutputReplayStatistics(ReplayController& controller, atMap<s32, packetInfo>& infos)
{
	while(controller.IsEndOfPlayback() == false)
	{
		while(controller.GetCurrentPacketID() != PACKETID_END)
		{
			iReplayInterface* pInterface = FindCorrectInterface(controller.GetCurrentPacketID());

			if(pInterface)
			{
				pInterface->OutputReplayStatistics(controller, infos);
			}
			else
			{
				u32 packetSize = controller.ReadCurrentPacket<CPacketBase>()->GetPacketSize();

				switch(controller.GetCurrentPacketID())
				{
				case PACKETID_GAMETIME:
					{
						packetInfo* pInfo = infos.Access(PACKETID_GAMETIME);
						if(pInfo == NULL)
						{
							packetInfo info;
							info.count = 1;
							strcpy_s(info.name, sizeof(info.name), "CPacketGameTime");
							info.size = sizeof(CPacketGameTime);
							infos.Insert(PACKETID_GAMETIME, info);
						}
						else
						{
							++pInfo->count;
						}
						packetSize = sizeof(CPacketGameTime);
					}
				break;
				case PACKETID_WEATHER:
					{
						packetInfo* pInfo = infos.Access(PACKETID_WEATHER);
						if(pInfo == NULL)
						{
							packetInfo info;
							info.count = 1;
							strcpy_s(info.name, sizeof(info.name),  "CPacketWeather");
							info.size = sizeof(CPacketWeather);
							infos.Insert(PACKETID_WEATHER, info);
						}
						else
						{
							++pInfo->count;
						}
						packetSize = sizeof(CPacketWeather);
					}
					break;
				case PACKETID_CLOCK:
					{
						packetInfo* pInfo = infos.Access(PACKETID_CLOCK);
						if(pInfo == NULL)
						{
							packetInfo info;
							info.count = 1;
							strcpy_s(info.name, sizeof(info.name),  "CPacketClock");
							info.size = sizeof(CPacketClock);
							infos.Insert(PACKETID_CLOCK, info);
						}
						else
						{
							++pInfo->count;
						}
						packetSize = sizeof(CPacketClock);
					}
					break;
				case PACKETID_NEXTFRAME:
					{
						packetInfo* pInfo = infos.Access(PACKETID_NEXTFRAME);
						if(pInfo == NULL)
						{
							packetInfo info;
							info.count = 1;
							strcpy_s(info.name, sizeof(info.name),  "CPacketNextFrame");
							info.size = sizeof(CPacketNextFrame);
							infos.Insert(PACKETID_NEXTFRAME, info);
						}
						else
						{
							++pInfo->count;
						}
						packetSize = sizeof(CPacketNextFrame);
					}
					break;
					case PACKETID_FRAME:
					{
						packetInfo* pInfo = infos.Access(PACKETID_FRAME);
						if(pInfo == NULL)
						{
							packetInfo info;
							info.count = 1;
							strcpy_s(info.name, sizeof(info.name),  "CPacketFrame");
							info.size = sizeof(CPacketFrame);
							infos.Insert(PACKETID_FRAME, info);
						}
						else
						{
							++pInfo->count;
						}
						packetSize = sizeof(CPacketFrame);
					}
					break;
				default:
					{
						return UNHANDLED_PACKET;
					}
				}

				controller.AdvanceUnknownPacket(packetSize);
			}
		}
	}

	return END_CLIP;
}


//////////////////////////////////////////////////////////////////////////
eReplayClipTraversalStatus CReplayInterfaceManager::PrintOutReplayPackets(ReplayController& controller, eReplayPacketOutputMode mode, FileHandle handle)
{
	while(controller.IsEndOfPlayback() == false)
	{
		while(controller.GetCurrentPacketID() != PACKETID_END)
		{
			eReplayPacketId packetId = controller.GetCurrentPacketID();
			iReplayInterface* pInterface = FindCorrectInterface(packetId);

			char str[1024];
			if(mode == REPLAY_OUTPUT_XML)
			{
				snprintf(str, 1024, "\t<Packet>\n");
				CFileMgr::Write(handle, str, istrlen(str));

				snprintf(str, 1024, "\t\t<Position>\n");
				CFileMgr::Write(handle, str, istrlen(str));

				snprintf(str, 1024, "\t\t\t<Block>%u</Block>\n", controller.GetCurrentBlockIndex());
				CFileMgr::Write(handle, str, istrlen(str));

				snprintf(str, 1024, "\t\t\t<Address>%p</Address>\n", controller.ReadCurrentPacket<CPacketBase>());
				CFileMgr::Write(handle, str, istrlen(str));

				snprintf(str, 1024, "\t\t</Position>\n");
				CFileMgr::Write(handle, str, istrlen(str));
			}

			if(pInterface)
			{
				pInterface->PrintOutPacket(controller, mode, handle);
			}
			else
			{
				u32 packetSize = controller.ReadCurrentPacket<CPacketBase>()->GetPacketSize();

				switch(packetId)
				{
				case PACKETID_GAMETIME:
					{
						if(mode == REPLAY_OUTPUT_XML)
						{
							snprintf(str, 1024, "\t\t<Name>CPacketGameTime</Name>\n");
							CFileMgr::Write(handle, str, istrlen(str));
						}
						else
						{	replayDebugf1("Packet CPacketGameTime");	}

						CPacketGameTime const* pPacket = controller.ReadCurrentPacket<CPacketGameTime>();
						if(mode == REPLAY_OUTPUT_XML)
							pPacket->PrintXML(handle);
						else
							pPacket->Print();
					}
					break;
				case PACKETID_WEATHER:
					{
						if(mode == REPLAY_OUTPUT_XML)
						{
							snprintf(str, 1024, "\t\t<Name>CPacketWeather</Name>\n");
							CFileMgr::Write(handle, str, istrlen(str));
						}
						else
						{	replayDebugf1("Packet CPacketWeather");	}

						CPacketWeather const* pPacket = controller.ReadCurrentPacket<CPacketWeather>();
						if(mode == REPLAY_OUTPUT_XML)
							pPacket->PrintXML(handle);
						else
							pPacket->Print();
					}
					break;
				case PACKETID_CLOCK:
					{
						if(mode == REPLAY_OUTPUT_XML)
						{
							snprintf(str, 1024, "\t\t<Name>CPacketClock</Name>\n");
							CFileMgr::Write(handle, str, istrlen(str));
						}
						else
						{	replayDebugf1("Packet CPacketClock");	}

						CPacketClock const* pPacket = controller.ReadCurrentPacket<CPacketClock>();
						if(mode == REPLAY_OUTPUT_XML)
							pPacket->PrintXML(handle);
						else
							pPacket->Print();
					}
					break;
				case PACKETID_NEXTFRAME:
					{
						if(mode == REPLAY_OUTPUT_XML)
						{
							snprintf(str, 1024, "\t\t<Name>CPacketNextFrame</Name>\n");
							CFileMgr::Write(handle, str, istrlen(str));
						}
						else
						{	replayDebugf1("Packet CPacketNextFrame");	}

						CPacketNextFrame const* pPacket = controller.ReadCurrentPacket<CPacketNextFrame>();
						if(mode == REPLAY_OUTPUT_XML)
							pPacket->PrintXML(handle);
						else
							pPacket->Print();
					}
					break;
					case PACKETID_FRAME:
					{
						if(mode == REPLAY_OUTPUT_XML)
						{
							snprintf(str, 1024, "\t\t<Name>CPacketFrame</Name>\n");
							CFileMgr::Write(handle, str, istrlen(str));
						}
						else
						{	replayDebugf1("Packet CPacketFrame");	}

						CPacketFrame const* pPacket = controller.ReadCurrentPacket<CPacketFrame>();
						if(mode == REPLAY_OUTPUT_XML)
							pPacket->PrintXML(handle);
						else
							pPacket->Print();
					}
					break;
				default:
					{
						return UNHANDLED_PACKET;
					}
				}
				controller.AdvanceUnknownPacket(packetSize);
			}

			if(mode == REPLAY_OUTPUT_XML)
			{
				char str[1024];
				snprintf(str, 1024, "\t</Packet>\n");
				CFileMgr::Write(handle, str, istrlen(str));
			}
			else
			{	replayDebugf1("");	}
		}
	}

	return END_CLIP;
}

//////////////////////////////////////////////////////////////////////////
#if GTA_REPLAY_OVERLAY
eReplayClipTraversalStatus CReplayInterfaceManager::CalculateBlockStats(ReplayController& controller, CBlockInfo* pBlock)
{
	while(controller.IsEndOfPlayback() == false)
	{
		while(controller.GetCurrentPacketID() != PACKETID_END)
		{
			eReplayPacketId packetId = controller.GetCurrentPacketID();
			iReplayInterface* pInterface = FindCorrectInterface(packetId);
			u32 packetSize = controller.ReadCurrentPacket<CPacketBase>()->GetPacketSize();

			if(pInterface)
			{
				pBlock->AddStats(packetId, packetSize, false);
			}
			else
			{				
				switch(packetId)
				{
				case PACKETID_GAMETIME:
				case PACKETID_WEATHER:
				case PACKETID_CLOCK:
				case PACKETID_NEXTFRAME:
				case PACKETID_FRAME:
					pBlock->AddStats(packetId, packetSize, false);
					break;
				default:
					{
						return UNHANDLED_PACKET;
					}
				}				
			}
			controller.AdvanceUnknownPacket(packetSize);
		}
	}

	return END_CLIP;
}

void CReplayInterfaceManager::GeneratePacketNameList(eReplayPacketId packetID, const char*& packetName)
{
	iReplayInterface* pInterface = FindCorrectInterface(packetID);
	if(pInterface)
	{
		pInterface->GetPacketName(packetID, packetName);
	}
}

#endif //GTA_REPLAY_OVERLAY

//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceManager::RegisterAllCurrentEntities()
{
	for(s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];

		pInterface->RegisterAllCurrentEntities();
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceManager::DeregisterAllCurrentEntities()
{
	for(s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];

		pInterface->DeregisterAllCurrentEntities();
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceManager::RemoveRecordingInformation()
{
	for(s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];

		pInterface->ClearPacketInformation();
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceManager::PrintOutEntities()
{
	replayDebugf1("============================");
	replayDebugf1("Printing out all entities...");
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];
		replayDebugf1("");
		pInterface->PrintOutEntities();
	}
	replayDebugf1("============================");
}


//////////////////////////////////////////////////////////////////////////
int CReplayInterfaceManager::GetInterfaceCount() const
{
	return m_interfaces.GetCount();
}


//////////////////////////////////////////////////////////////////////////
bool CReplayInterfaceManager::GetBlockDetails(char* pString, int interfaceIndex, bool& err, bool recording) const
{
	REPLAY_CHECK(interfaceIndex < m_interfaces.GetCount(), false, "Interface index is incorrect");
	if(interfaceIndex < m_interfaces.GetCount())
	{
		iReplayInterface* pInterface = m_interfaces[interfaceIndex];
		return pInterface->GetBlockDetails(pString, err, recording);
	}
	
	return false;
}


//////////////////////////////////////////////////////////////////////////
bool CReplayInterfaceManager::ScanForAnomalies(char* pString, int interfaceIndex, int& index) const
{
	REPLAY_CHECK(interfaceIndex < m_interfaces.GetCount(), false, "Interface index is incorrect");
	if(interfaceIndex < m_interfaces.GetCount())
	{
		iReplayInterface* pInterface = m_interfaces[interfaceIndex];
		return pInterface->ScanForAnomalies(pString, index);
	}

	return false;
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceManager::PreClearWorld()
{
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];
		pInterface->PreClearWorld();
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceManager::ClearWorldOfEntities()
{
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];
		pInterface->ClearWorldOfEntities();
	}
}


//////////////////////////////////////////////////////////////////////////
bool CReplayInterfaceManager::EnsureIsBeingRecorded(const CEntity* pElem)
{
	REPLAY_CHECK(pElem != NULL, false, "pElem should not be NULL in CReplayInterfaceManager::EnsureIsBeingRecorded");

	iReplayInterface* pInterface = FindCorrectInterface(pElem);
	REPLAY_CHECK(pInterface != NULL, false, "Failed to find interface for an entity (type:%d)", pElem->GetType());

	return pInterface->EnsureIsBeingRecorded(pElem);
}

//////////////////////////////////////////////////////////////////////////
bool CReplayInterfaceManager::ShouldRegisterElement(const CEntity* pEntity)
{
	REPLAY_CHECK(pEntity != NULL, false, "pElem should not be NULL in CReplayInterfaceManager::ShouldRegisterElement");

	iReplayInterface* pInterface = FindCorrectInterface(pEntity);
	REPLAY_CHECK(pInterface != NULL, false, "Failed to find interface for an entity (type:%d)", pEntity->GetType());

	return pInterface->ShouldRegisterElement(pEntity);
}


//////////////////////////////////////////////////////////////////////////
bool CReplayInterfaceManager::EnsureWillBeRecorded(const CEntity* pElem)
{
	iReplayInterface* pInterface = FindCorrectInterface(pElem);
	REPLAY_CHECK(pInterface != NULL, false, "Failed to find interface for an entity (type:%d)", pElem->GetType());

	return pInterface->EnsureWillBeRecorded(pElem);
}


//////////////////////////////////////////////////////////////////////////
bool CReplayInterfaceManager::IsCurrentlyBeingRecorded(s32 replayID) const
{
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];

		if(pInterface->IsCurrentlyBeingRecorded(replayID))
			return true;
	}

	return false;
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfaceManager::ResetEntityCreationFrames()
{
	ReplayEntityExtension::AllowCreationFrameResetting = true;
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];

		pInterface->ResetEntityCreationFrames();
	}
	ReplayEntityExtension::AllowCreationFrameResetting = false;
}


//////////////////////////////////////////////////////////////////////////
bool CReplayInterfaceManager::SetCreationUrgent(s32 replayID)
{
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];

		if(pInterface->SetCreationUrgent(replayID))
			return true;
	}

	replayFatalAssertf(false, "Failed to find entity to set urgent creation - CReplayInterfaceManager::SetCreationUrgent");
	return false;
}

//////////////////////////////////////////////////////////////////////////
CEntity* CReplayInterfaceManager::GetEntiyAsEntity(s32 id)
{
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];

		CReplayID entityID(id);
		CEntityGet<CEntity> entityGet(entityID);
		pInterface->GetEntityAsEntity(entityGet);
		CEntity* pEntity = entityGet.m_pEntity;
		if(pEntity)
			return pEntity;
	}

	return NULL;
}


//////////////////////////////////////////////////////////////////////////
bool CReplayInterfaceManager::IsRecentlyDeleted(const CReplayID& id)
{
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];

		if(pInterface->IsRecentlyDeleted(id))
			return true;
	}

	return false;
}


//////////////////////////////////////////////////////////////////////////
template<>
void CReplayInterfaceManager::GetEntity<CEntity>(CEntityGet<CEntity>& entityGet)
{
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];

		pInterface->FindEntity(entityGet);
		if(entityGet.m_pEntity || entityGet.m_alreadyReported || entityGet.m_recentlyUnlinked)
		{
			break;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
CEntity* CReplayInterfaceManager::FindEntity(CReplayID id)
{
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];

		CEntityGet<CEntity> eg(id);
		pInterface->FindEntity(eg);

		CEntity* pEntity = eg.m_pEntity;
		if(pEntity)
			return pEntity;
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
#if __BANK
void CReplayInterfaceManager::AddDebugWidgets()
{
	for (s32 i = 0; i < m_interfaces.GetCount(); ++i)
	{
		iReplayInterface* pInterface = m_interfaces[i];

		pInterface->AddDebugWidgets();
	}
}
#endif // __BANK

#endif // GTA_REPLAY
