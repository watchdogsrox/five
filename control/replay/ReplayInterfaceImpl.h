#ifndef REPLAYINTERFACEIMPL_H
#define REPLAYINTERFACEIMPL_H

#include "bank/bkmgr.h"
#include "scene/world/GameWorld.h"
#include "streaming/streaming.h"
#include "scene/Physical.h"
#include "debug/GtaPicker.h"
#include "script/script.h"
#include "fwscene/stores/maptypesstore.h"

#include "Control/Replay/ReplayController.h"
#include "control/replay/ReplayExtensions.h"
#include "control/replay/ReplayInterface.h"
#include "control/replay/ReplaySupportClasses.h"
#include "Control/replay/ReplayAttachmentManager.h"

#if __BANK
extern void EntityDetails_GetOwner(CEntity* pEntity, atString &ReturnString);
#endif // __BANK

#if GTA_REPLAY && __ASSERT
namespace rage
{
	extern void	SetNoPlacementAssert(bool var);
};
#endif	//GTA_REPLAY && __ASSERT

//////////////////////////////////////////////////////////////////////////
template<typename T>
CReplayInterface<T>::CReplayInterface(typename CReplayInterfaceTraits<T>::tPool& pool, u32 recBufferSize, u32 crBufferSize, u32 fullCrBufferSize)
	: iReplayInterface(recBufferSize, crBufferSize, fullCrBufferSize)
	, m_pool(pool)
	, m_entityRegistry(m_pool.GetSize())
	, m_entitiesDuringRecording(0)
	, m_entitiesDuringPlayback(0)
	, m_numPacketDescriptors(0)
	, m_deletionPacketsSize(0)
{
	m_interper.Reset();

	u32 poolSize = (u32) m_pool.GetSize();
	replayDebugf1("Pool size for %s - %u", GetLongFriendlyStr(), poolSize);
	// This class is allocated using the replay allocator so so will these.
	m_entityDatas	= rage_new CEntityData[poolSize];

	//set a size for these containers
	m_CreatePostUpdate.Reserve(poolSize);

	m_deletionsToRecord.Reserve(poolSize);

	m_currentlyRecorded.Reserve(poolSize);
	m_deferredCreationList.Reserve(poolSize);
	for(int i = 0; i < m_pool.GetSize(); ++i)
	{
		m_currentlyRecorded.Append();
	}

	m_modelManager.Init((u32)m_pool.GetSize());

	Reset();
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
CReplayInterface<T>::~CReplayInterface()
{
	delete[] m_entityDatas;
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
template<typename PACKETTYPE>
void CReplayInterface<T>::AddPacketDescriptor(eReplayPacketId packetID, const char* packetName)
{
	REPLAY_CHECK(m_numPacketDescriptors < maxPacketDescriptorsPerInterface, NO_RETURN_VALUE, "Max number of packet descriptors breached... %s will not be registered!", packetName);
	m_packetDescriptors[m_numPacketDescriptors++] = CPacketDescriptor<T>(	packetID, 
																			-1, 
																			sizeof(PACKETTYPE), 
																			packetName,
																			&iReplayInterface::template PrintOutPacketDetails<PACKETTYPE>,
																			NULL,
																			NULL);
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::Reset()
{
	m_entityRegistry.Reset();

	for(int i = 0; i < m_pool.GetSize(); ++i)
		m_entityDatas[i].Reset();

	m_entitiesDuringPlayback = 0;

	m_deletionsOnRewind.Reset();

	m_recentUnlinks.clear();

	iReplayInterface::Reset();
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::Clear()
{
	Reset();

	ClearPacketInformation();
	m_deletionsToRecord.clear();
	m_deletionPacketsSize = 0;

	m_entitiesDuringRecording = 0;

	m_deletionsOnRewind.Reset();

	iReplayInterface::Clear();
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::RecordFrame(u16 sessionBlockIndex)
{
	m_entitiesDuringRecording = 0;

	CReplayRecorder recorder(m_recordBuffer, m_recordBufferSize);
	CReplayRecorder createPacketRecorder(m_pCreatePacketsBuffer, m_createPacketsBufferSize);
	CReplayRecorder fullCreatePacketRecorder(m_pFullCreatePacketsBuffer, m_fullCreatePacketsBufferSize);

	PreRecordFrame(recorder, fullCreatePacketRecorder);

	u32 m_frameHash = 0;
	tPool& pool = m_pool;
	for (s32 slotIndex = 0; slotIndex < pool.GetSize(); slotIndex++)
	{
		T* pEntity = pool.GetSlot(slotIndex);

		if (pEntity && ShouldRecordElement(pEntity))
		{
			if(RecordElement(pEntity, recorder, createPacketRecorder, fullCreatePacketRecorder, sessionBlockIndex))
			{
				++m_entitiesDuringRecording;

				SetCurrentlyBeingRecorded(slotIndex, pEntity);

				m_frameHash = GetHash(pEntity, m_frameHash);
			}
			else
			{
				SetCurrentlyBeingRecorded(slotIndex, 0);
			}
		}
		else
		{
			SetCurrentlyBeingRecorded(slotIndex, 0);
		}
	}

	m_recordBufferUsed = recorder.GetMemUsed();
	m_createPacketsBufferUsed = createPacketRecorder.GetMemUsed();
	m_fullCreatePacketsBufferUsed = fullCreatePacketRecorder.GetMemUsed();
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::ResetAllEntities()
{
	tPool& pool = m_pool;
	for (s32 slotIndex = 0; slotIndex < pool.GetSize(); slotIndex++)
	{
		T* pEntity = pool.GetSlot(slotIndex);

		//make sure we don't reset anything that isn't being recorded at this moment, 
		//some entities can be still be in the pool while waiting to be deleted
		if(pEntity && ReplayEntityExtension::HasExtension(pEntity))
			ResetEntity(pEntity);
	}
}

//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::StopAllEntitySounds()
{
	tPool& pool = m_pool;
	for (s32 slotIndex = 0; slotIndex < pool.GetSize(); slotIndex++)
	{
		T* pEntity = pool.GetSlot(slotIndex);

		//make sure we don't reset anything that isn't being recorded at this moment, 
		//some entities can be still be in the pool while waiting to be deleted
		if(pEntity && pEntity->GetOwnedBy() == ENTITY_OWNEDBY_REPLAY)
			StopEntitySounds(pEntity);
	}
}

//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::CreateDeferredEntities(ReplayController& controller)
{
	for(s32 i = 0; i < m_deferredCreationList.size(); ++i)
	{
		deferredCreationData& data = m_deferredCreationList[i];
		replayDebugf3("Try creating deferred %s...", GetShortStr());
		if(data.pPacket != NULL)
		{
#if !__NO_OUTPUT
			T* pEntity = EnsureEntityIsCreated(data.pPacket->GetReplayID(), controller.GetPlaybackFlags(), false);
			if(!pEntity)
			{
				for(s32 j = 0; j < m_deferredCreationList.GetCount(); ++j)
				{
					deferredCreationData& data2 = m_deferredCreationList[j];
					if(data2.pPacket != NULL)
					{
						replayDebugf2("Entity in deferred list: 0x%08X", data2.pPacket->GetReplayID().ToInt());
					}
					else
					{
						replayDebugf2("Entity in deferred list had null packet!");
					}
				}
			}
			replayAssertf(pEntity, "Entity not created despite being deferred! %p - 0x%08X", data.pPacket, data.pPacket->GetReplayID().ToInt());
#endif

			PreUpdatePacket(data.pPacket, data.pNextPacket, data.interpValue, controller.GetPlaybackFlags(), false);
			data.pPacket = NULL;
		}
	}
	m_deferredCreationList.clear();
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::AddToCreateLater(const tCreatePacket* pCreatePacket)
{
	if(HasEntityFailedToCreate(pCreatePacket->GetReplayID()))
		return;

	// Check whether we've already requested an entity to be created with this ID
	typename atArray<const tCreatePacket*>::iterator it = m_CreatePostUpdate.begin();
	typename atArray<const tCreatePacket*>::iterator end = m_CreatePostUpdate.end();
	for(; it != end; ++it)
	{
		if((*it)->GetReplayID() == pCreatePacket->GetReplayID())
		{
			// Update the packet to use
			(*it) = pCreatePacket;
			return;
		}
	}

	// Add the create packet to the list
	m_CreatePostUpdate.Push(pCreatePacket);
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::CreateBatchedEntities(s32& createdSoFar, const CReplayState& state)
{
	if(m_CreatePostUpdate.GetCount() < 1)
		return;

	s32 toDelete[entityCreationLimitPerFrame] = {-1};
	s32 createdThisType = 0;
	for(s32 i = 0; i < m_CreatePostUpdate.GetCount() && createdSoFar < entityCreationLimitPerFrame; ++i)
	{
		const tCreatePacket* pCreatePacket = m_CreatePostUpdate[i];
		pCreatePacket->ValidatePacket();

		replayDebugf2("Creating batched entity %p", pCreatePacket);
		if(CreateElement(pCreatePacket, state))
		{
			toDelete[createdThisType] = i;
			++createdThisType;
			++createdSoFar;
		}
	}

	for(s32 i = createdThisType-1; i >= 0; --i)
	{
		m_CreatePostUpdate.Delete(toDelete[i]);
	}
	replayDebugf2("Created %d %s Total %d Batched", createdThisType, GetShortStr(), createdSoFar);
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
bool CReplayInterface<T>::HasBatchedEntitiesToProcess() const
{
	return m_CreatePostUpdate.GetCount() != 0;
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::ConvertMapTypeDefHashToIndex(ReplayController& controller, tCreatePacket const* pCreatePacket)
{
	if(pCreatePacket->UseMapTypeDefHash())
	{
		if(pCreatePacket->GetMapTypeDefHash())
		{
			controller.EnableModify();
			tCreatePacket* pModifiablePacket = controller.GetCurrentPacket<tCreatePacket>();
			controller.DisableModify();

			strLocalIndex mapTypeDefIndex = g_MapTypesStore.FindSlotFromHashKey(pModifiablePacket->GetMapTypeDefHash());

			replayDebugf1("Converting MapTypeDef Hash 0x%08X to Index %u", pModifiablePacket->GetMapTypeDefHash(), mapTypeDefIndex.Get());

			pModifiablePacket->SetMapTypeDefIndex(mapTypeDefIndex);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::PlaybackSetup(ReplayController& controller)
{
	CPacketBase const* pBasePacket = controller.ReadCurrentPacket<CPacketBase>();
	eReplayPacketId packetID = pBasePacket->GetPacketID();

	// Delete Packet
	if(packetID == m_deletePacketID)
	{
		tDeletePacket const* pPacket = controller.ReadCurrentPacket<tDeletePacket>();
		pPacket->ValidatePacket();

 		if(controller.GetCurrentFrameRef().GetAsU32() == 0)
 		{
 			replayDebugf1("Cancelling deletion of %s 0x%X", GetShortStr(), pPacket->GetReplayID().ToInt());

			controller.EnableModify();
			tDeletePacket* pToCancel = controller.GetCurrentPacket<tDeletePacket>();
 			pToCancel->Cancel();
			controller.DisableModify();
 		}
		
		ConvertMapTypeDefHashToIndex(controller, pPacket);

		UnlinkCreatePacket(pPacket->GetReplayID(), false);
	}
	else if(packetID == m_createPacketID)
	{
		sysCriticalSection cs(m_createPacketCS);
		tCreatePacket const* pCreatePacket = controller.ReadCurrentPacket<tCreatePacket>();
		//check to see if we've got a create packet, and if so add a link to it so we can access it without having to search for this packet again

		ConvertMapTypeDefHashToIndex(controller, pCreatePacket);

		if(GetCreatePacket(pCreatePacket->GetReplayID()) == NULL)
		{
			LinkCreatePacket(pCreatePacket);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Brazenly ripped from the ApplyFades() function in the old ReplayMgr...
template<typename T>
void CReplayInterface<T>::ApplyFades(ReplayController& controller, s32 frame)
{
	eReplayPacketId ePacketID = controller.GetCurrentPacketID();
	if(ePacketID == m_createPacketID)
	{
	}
	else if(ePacketID == m_deletePacketID)
	{
		tDeletePacket const* pPacket = controller.ReadCurrentPacket<tDeletePacket>();

		if (pPacket->GetReplayID() == ReplayIDInvalid)
		{
			return;
		}

		CReplayID oReplayID(pPacket->GetReplayID());

		CEntityData& rEntityData = m_entityDatas[oReplayID.GetEntityID()];

		if (rEntityData.m_iInstID == oReplayID.GetInstID())
		{
			// The fadeout packets are not in order so we need to figure out
			// which is the first one and work from there

			for (s32 packet = 0; packet < CEntityData::NUM_FRAMES_TO_FADE; packet++)
			{
				// Adjust fade in.
				tUpdatePacket* pPacketUpdate = (tUpdatePacket*)rEntityData.m_apvPacketFadeOut[packet];
				if(pPacketUpdate)
				{
					pPacketUpdate->ValidatePacket();

					pPacketUpdate->SetFadeOutForward(true);
				}
			}

			// This entity has been done and dusted so reset the data.
			// We'll loop through the others that haven't been here later
			// (i.e. they don't have a deletion packet)
			rEntityData.Reset();
		}
	}
	else	// Update
	{
		tUpdatePacket const* pPacket = controller.ReadCurrentPacket<tUpdatePacket>();

		u32 packetSize = pPacket->GetPacketSize();
		

		if(pPacket->GetReplayID() == ReplayIDInvalid)
		{
			controller.AdvanceUnknownPacket(packetSize);
			return;
		}

		CReplayID oReplayID(pPacket->GetReplayID());

		CEntityData& rEntityData = m_entityDatas[oReplayID.GetEntityID()];

		if (rEntityData.m_iInstID == -1)
		{
			rEntityData.m_iFrame = (s16)frame;
		}

		rEntityData.m_iInstID	= oReplayID.GetInstID();
		rEntityData.m_uPacketID	= (u8)ePacketID;

		// We don't yet know where the deletion is going to occur so store
		// every packet as a potential fade out packet.
		// These packets won't necessarily be in order of age
		rEntityData.m_apvPacketFadeOut[rEntityData.m_iFadeOutCount] = (void*)pPacket;
		rEntityData.m_iFadeOutCount = (rEntityData.m_iFadeOutCount + 1) % CEntityData::NUM_FRAMES_TO_FADE;
	}
}


//////////////////////////////////////////////////////////////////////////
// This is performed to go through all the entities that did not have a 
// deletion packet and therefore did not get their fading handled in the
// CReplayInterface<T>::ApplyFades() function.
template<typename T>
void CReplayInterface<T>::ApplyFadesSecondPass()
{
	// Remove?
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
bool CReplayInterface<T>::TryPreloadPacket(const CPacketBase* pPacket, bool& preloadSuccess, u32 currGameTime PRELOADDEBUG_ONLY(, atString& failureReason))
{
	if(!pPacket || (pPacket->GetPacketID() != m_createPacketID && pPacket->GetPacketID() != m_deletePacketID))
		return false;

	const tCreatePacket* pCreatePacket = (const tCreatePacket*)pPacket;
	u32 modelHash = pCreatePacket->GetModelNameHash();
	strLocalIndex mapTypeDef = pCreatePacket->GetMapTypeDefIndex();

	// See if we already have a preload request for this model...
	m_modelManager.PreloadRequest(modelHash, mapTypeDef, !pCreatePacket->UseMapTypeDefHash(), preloadSuccess, currGameTime);

#if PRELOADDEBUG
	if(!preloadSuccess)
	{
		char message[256] = {0};
		sprintf(message, "Failed to preload model via the %s model manager... hash %u, maptypeDef %d", GetShortStr(), modelHash, mapTypeDef.Get());
		failureReason = message;
	}
#endif // PRELOADDEBUG

	return true;
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
bool CReplayInterface<T>::FindPreloadRequests(ReplayController& controller, tPreloadRequestArray& preloadRequests, u32& requestCount, const u32 systemTime, bool isSingleFrame)
{
	if(isSingleFrame)
	{
		sysCriticalSection cs(m_createPacketCS);
		typename atMap<CReplayID, sCreatePacket>::Iterator currentPacketsIterator = m_CurrentCreatePackets.CreateIterator();
		while(currentPacketsIterator.AtEnd() == false)
		{
			const tCreatePacket* pPacket = currentPacketsIterator->pCreatePacket_const;
			if(pPacket->GetPacketID() == m_createPacketID || pPacket->GetPacketID() == m_deletePacketID)
			{
				++requestCount;
				if(!preloadRequests.IsFull())
				{
					preloadData data;
					data.pPacket = pPacket; 
					data.systemTime = systemTime;
					preloadRequests.Push(data);
				}
				else
				{
					replayAssertf(false, "Preload Requests list is full");
				}
			}
			currentPacketsIterator.Next();
		}
	}
	else
	{
		while(IsRelevantPacket(controller.GetCurrentPacketID()))
		{
			const CPacketBase* pPacket = controller.ReadCurrentPacket<CPacketBase>();
			if(pPacket->ShouldPreload())
			{
				++requestCount;
				if(!preloadRequests.IsFull())
				{
					preloadData data;
					data.pPacket = pPacket; 
					data.systemTime = systemTime;
					preloadRequests.Push(data);
				}
			}

			controller.AdvancePacket();
		}
	}
	
	return !preloadRequests.IsFull();
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::ProcessRewindDeletions(FrameRef frame)
{
	// Iterate through the atMap linearly...
	for(int i = 0; i < m_deletionsOnRewind.GetNumSlots(); ++i)
	{
		const atMapEntry<CReplayID, const tCreatePacket*>* pEntry = m_deletionsOnRewind.GetEntry(i);
		while(pEntry)
		{
			CEntityGet<T> entityGet(pEntry->key);
			GetEntity(entityGet);
			T* pEntity = entityGet.m_pEntity;
			if(pEntity)
			{
				FrameRef entityFrameRef;
				pEntity->GetCreationFrameRef(entityFrameRef);
				if(frame < entityFrameRef)
				{
					PrintEntity(pEntity, "DEL", CReplayInterfaceTraits<T>::strShort);
					UnlinkCreatePacket(pEntity->GetReplayID(), false);
					RemoveElement(pEntity, NULL);
				}
			}
			else
			{
				ProcessRewindDeletionWithoutEntity(pEntry->data, pEntry->key);
			}
			pEntry = pEntry->next;
		}
	}
	m_deletionsOnRewind.Reset();
}


//////////////////////////////////////////////////////////////////////////
// The packets are all in contiguous blocks so we loop here processing packets
// until we come across one this interface does not understand.  This way we
// don't need to search for the correct interface any more than we need to.
template<typename T>
ePlayPktResult CReplayInterface<T>::PreprocessPackets(ReplayController& controller, bool entityMayNotExist)
{
	do 
	{
		eReplayPacketId packetID = controller.GetCurrentPacketID();

		if(packetID == m_deletePacketID)
		{
			const tDeletePacket* pDeletePacket = controller.ReadCurrentPacket<tDeletePacket>();
			HandleCreatePacket(pDeletePacket->GetCreatePacket(), false, controller.GetIsFirstFrame(), controller.GetPlaybackFlags());
			PlayDeletePacket(controller);
		}
		else if(packetID == m_createPacketID)
		{
			HandleCreatePacket(controller.ReadCurrentPacket<tCreatePacket>(), true, controller.GetIsFirstFrame(), controller.GetPlaybackFlags());
			if(!entityMayNotExist || controller.GetPlaybackFlags().IsSet(REPLAY_CURSOR_JUMP_LINEAR) || controller.IsFineScrubbing())
			{
				PlayCreatePacket(controller);
			}
			else
			{
				controller.ReadCurrentPacket<tCreatePacket>();
				controller.AdvancePacket();
			}
		}
		else if(IsRelevantUpdatePacket(packetID))
		{
			bool hasEntityBeenCreated = true;
			// Only call PreUpdatePacket if the create hasn't been deferred.
			const tUpdatePacket* pUpdatepacket = controller.ReadCurrentPacket<tUpdatePacket>();
			
			if(entityMayNotExist)
			{
				CEntityGet<T> entityGet(pUpdatepacket->GetReplayID());
				GetEntity(entityGet);
				if(entityGet.m_pEntity == NULL)
					hasEntityBeenCreated = false;
			}

			if(hasEntityBeenCreated)
			{
				for(int i = 0; i < m_CreatePostUpdate.GetCount(); ++i)
				{
					if(m_CreatePostUpdate[i]->GetReplayID() == pUpdatepacket->GetReplayID())
					{
						hasEntityBeenCreated = false;
						break;
					}
				}
			}
			
			if(hasEntityBeenCreated)
			{
				PreUpdatePacket(controller);
			}
			else
			{
				controller.ReadCurrentPacket<CPacketBase>();
				controller.AdvancePacket();
			}
		}
		else if(IsRelevantPacket(packetID))	// Any other relevant packet...
		{
			controller.ReadCurrentPacket<CPacketBase>();
			controller.AdvancePacket();
		}
		else
		{
			return PACKET_UNHANDLED;
		}

	} while (!controller.IsNextFrame());

	return PACKET_UNHANDLED;
}


//////////////////////////////////////////////////////////////////////////
// The packets are all in contiguous blocks so we loop here processing packets
// until we come across one this interface does not understand.  This way we
// don't need to search for the correct interface any more than we need to.
template<typename T>
ePlayPktResult CReplayInterface<T>::PlayPackets(ReplayController& controller, bool entityMayNotExist)
{
	if(controller.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_BACK))
	{
		CAddressInReplayBuffer deletionAddress = controller.GetAddress();
		ReplayController deleteController(deletionAddress, controller.GetBufferInfo(), controller.GetCurrentFrameRef());
		deleteController.SetPlaybackFlags(controller.GetPlaybackFlags());
		deleteController.EnablePlayback();

		// Process all the update packets first...these may cause deletions
		do 
		{
			eReplayPacketId packetID = controller.GetCurrentPacketID();

			if(packetID == m_deletePacketID)
			{	// Skip the deletion packets
				HandleCreatePacket(controller.ReadCurrentPacket<tDeletePacket>()->GetCreatePacket(), false, controller.GetIsFirstFrame(), controller.GetPlaybackFlags());
				controller.AdvancePacket();
			}
			else if(packetID == m_createPacketID)
			{
				HandleCreatePacket(controller.ReadCurrentPacket<tCreatePacket>(), true, controller.GetIsFirstFrame(), controller.GetPlaybackFlags());
				controller.AdvancePacket();
			}
			else if(IsRelevantUpdatePacket(packetID))
			{
				PlayUpdatePacket(controller, entityMayNotExist);
			}
			else if(IsRelevantPacket(packetID))	// Any other relevant packet...
			{
				PlayPacket(controller);
				controller.AdvancePacket();
			}
			else
			{
				break;
			}

		} while (!controller.IsNextFrame());

		// Then do deletions (which will actually create)
		do 
		{
			eReplayPacketId packetID = deleteController.GetCurrentPacketID();

			if(packetID == m_deletePacketID)
			{
				PlayDeletePacket(deleteController);
			}
			else
			{
				break;
			}

		} while (!deleteController.IsNextFrame());

		deleteController.DisablePlayback();
	}
	else
	{
		// Normal playback...play all packets in order
		do 
		{
			eReplayPacketId packetID = controller.GetCurrentPacketID();

			if(packetID == m_deletePacketID)
			{
				HandleCreatePacket(controller.ReadCurrentPacket<tDeletePacket>()->GetCreatePacket(), false, controller.GetIsFirstFrame(), controller.GetPlaybackFlags());
				controller.AdvancePacket();
			}
			else if(packetID == m_createPacketID)
			{
				HandleCreatePacket(controller.ReadCurrentPacket<tCreatePacket>(), true, controller.GetIsFirstFrame(), controller.GetPlaybackFlags());
				controller.AdvancePacket();
			}
			else if(IsRelevantUpdatePacket(packetID))
			{
				PlayUpdatePacket(controller, entityMayNotExist);
			}
			else if(IsRelevantPacket(packetID))	// Any other relevant packet...
			{
				PlayPacket(controller);
				controller.AdvancePacket();
			}
			else
			{
				break;
			}

		} while (!controller.IsNextFrame());
	}

	if(!entityMayNotExist)
		m_recentUnlinks.clear();

	return PACKET_UNHANDLED;
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::PlayPacket(ReplayController& controller)
{
	controller.ReadCurrentPacket<CPacketBase>();
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::JumpPackets(ReplayController& controller)
{
	if(controller.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_BACK))
	{
		CAddressInReplayBuffer deletionAddress = controller.GetAddress();
		ReplayController deleteController(deletionAddress, controller.GetBufferInfo(), controller.GetCurrentFrameRef());
		deleteController.SetPlaybackFlags(controller.GetPlaybackFlags());
		deleteController.EnablePlayback();

		do 
		{
			eReplayPacketId packetID = controller.GetCurrentPacketID();

			if(packetID == m_deletePacketID)
			{
				tCreatePacket const* pCreatePacket = controller.ReadCurrentPacket<tDeletePacket>()->GetCreatePacket();
				HandleCreatePacket(pCreatePacket, false, controller.GetIsFirstFrame(), controller.GetPlaybackFlags());

				if(controller.IsFineScrubbing() && !pCreatePacket->IsFirstCreationPacket())
				{
					CEntityGet<T> entityGet(pCreatePacket->GetReplayID());
					GetEntity(entityGet);
					T* pEntity = entityGet.m_pEntity;
					if(pEntity == NULL)
					{
						if(!CreateElement(pCreatePacket, controller.GetPlaybackFlags()))
						{
							AddToCreateLater(pCreatePacket);
							replayDebugf2("Adding creation packet to PostUpdate1 %p, 0x%X, count %d", pCreatePacket, pCreatePacket->GetReplayID().ToInt(), m_CreatePostUpdate.GetCount());
						}
					}
				}

				controller.AdvancePacket();
			}
			else if(packetID == m_createPacketID)
			{
				tCreatePacket const *pCreatePacket = controller.ReadCurrentPacket<tCreatePacket>();
				HandleCreatePacket(pCreatePacket, true, controller.GetIsFirstFrame(), controller.GetPlaybackFlags());

				if(DoesObjectExist(pCreatePacket) == false)
				{
					OnJumpCreatePacketBackwardsObjectNotExisting(pCreatePacket);
				}

				PlayCreatePacket(controller);
			}
			else if(IsRelevantUpdatePacket(packetID))
			{
				JumpUpdatePacket(controller);
			}
			else if(IsRelevantPacket(packetID))	// Any other relevant packet...
			{
				if(ShouldRelevantPacketBeProcessedDuringJumpPackets(packetID))
				{
					ProcessRelevantPacketDuringJumpPackets(packetID, controller, true);
				}
				controller.ReadCurrentPacket<CPacketBase>();
				controller.AdvancePacket();
			}
			else
			{
				break;
			}

		} while (!controller.IsNextFrame());

		// Then do deletions (which will actually create)
		do 
		{
			eReplayPacketId packetID = deleteController.GetCurrentPacketID();

			if(packetID == m_deletePacketID)
			{
				PlayDeletePacket(deleteController);
			}
			else
			{
				break;
			}

		} while (!deleteController.IsNextFrame());

		deleteController.DisablePlayback();
	}
	else
	{
		do 
		{
			eReplayPacketId packetID = controller.GetCurrentPacketID();

			if(packetID == m_deletePacketID)
			{
				tCreatePacket const *pCreatePacket = controller.ReadCurrentPacket<tDeletePacket>()->GetCreatePacket();
				HandleCreatePacket(pCreatePacket, false, controller.GetIsFirstFrame(), controller.GetPlaybackFlags());

				if(DoesObjectExist(pCreatePacket) == false)
				{
					OnJumpDeletePacketForwardsObjectNotExisting(pCreatePacket);
				}


				PlayDeletePacket(controller);
			}
			else if(packetID == m_createPacketID)
			{
				HandleCreatePacket(controller.ReadCurrentPacket<tCreatePacket>(), true, controller.GetIsFirstFrame(), controller.GetPlaybackFlags());
				PlayCreatePacket(controller);
			}
			else if(IsRelevantUpdatePacket(packetID))
			{
				JumpUpdatePacket(controller);
			}
			else if(IsRelevantPacket(packetID))	// Any other relevant packet...
			{
				if(ShouldRelevantPacketBeProcessedDuringJumpPackets(packetID))
				{
					ProcessRelevantPacketDuringJumpPackets(packetID, controller, false);
				}
				controller.ReadCurrentPacket<CPacketBase>();
				controller.AdvancePacket();
			}
			else
			{
				break;
			}

		} while (!controller.IsNextFrame());
	}
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
bool CReplayInterface<T>::DoesObjectExist(tCreatePacket const *pCreatePacket)
{
	CEntityGet<T> entityGet(pCreatePacket->GetReplayID());
	GetEntity(entityGet);
	return (entityGet.m_pEntity != NULL) ? true : false;
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::JumpUpdatePacket(ReplayController& controller)
{
	tUpdatePacket const* pPacket = controller.ReadCurrentPacket<tUpdatePacket>();
	m_interper.Init(controller, pPacket);
	INTERPER interper(m_interper, controller);
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::PreUpdatePacket(ReplayController& controller)
{
	tUpdatePacket const* pPacket = controller.ReadCurrentPacket<tUpdatePacket>();
	REPLAY_CHECK(pPacket->GetPacketID() == m_updatePacketID || pPacket->GetPacketID() == m_updateDoorPacketID, NO_RETURN_VALUE, "Incorrect Packet in PreUpdatePacket");

	m_interper.Init(controller, pPacket);
	INTERPER interper(m_interper, controller);

	if(controller.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_BACK) && controller.GetPlaybackFlags().IsSet(REPLAY_CURSOR_JUMP) == false && controller.GetIsFirstFrame() == false)
	{
		if(PlayUpdateBackwards(controller, pPacket))
			return;
	}

	PreUpdatePacket(pPacket, m_interper.GetNextPacket(), controller.GetInterp(), controller.GetPlaybackFlags());
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::PreUpdatePacket(const tUpdatePacket* pPacket, const tUpdatePacket* pNextPacket, float interpValue, const CReplayState& flags, bool canDefer)
{
	T* pEntity = EnsureEntityIsCreated(pPacket->GetReplayID(), flags, !canDefer);
	if(pEntity)
	{
		CInterpEventInfo<tUpdatePacket, T> info;
		info.SetEntity(pEntity);
		info.SetNextPacket(pNextPacket);
		info.SetInterp(interpValue);
		info.SetPlaybackFlags(flags);

		pPacket->PreUpdate(info);
	}
	else if(canDefer)
	{
		deferredCreationData& data = m_deferredCreationList.Append();
		replayDebugf3("Deferring creation of %s .... 0x%08X with packet %p", GetShortStr(), pPacket->GetReplayID().ToInt(), pPacket);
		data.pPacket		= pPacket;
		data.pNextPacket	= pNextPacket;
		data.interpValue	= interpValue;
	}
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
bool CReplayInterface<T>::PlayUpdateBackwards(const ReplayController& controller, const tUpdatePacket* pUpdatePacket)
{
	if(controller.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_BACK) && controller.GetIsFirstFrame() == false)
 	{
		if(controller.GetPlaybackFlags().IsSet(REPLAY_CURSOR_JUMP_LINEAR))
		{
			EnsureEntityIsCreated(pUpdatePacket->GetReplayID(), controller.GetPlaybackFlags(), true);	
		}

		if(pUpdatePacket->GetIsFirstUpdatePacket())
 		{
 			return true;
 		}
 	}

	return false;
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::PlayUpdatePacket(ReplayController& controller, bool entityMayNotExist)
{
	tUpdatePacket const* pPacket = controller.ReadCurrentPacket<tUpdatePacket>();

	m_interper.Init(controller, pPacket);
	INTERPER interper(m_interper, controller);
	
	if(PlayUpdateBackwards(controller, pPacket))
		return;

	CEntityGet<T> entityGet(pPacket->GetReplayID());
	GetEntity(entityGet);
	T* pEntity = entityGet.m_pEntity;

	if(!pEntity && (entityGet.m_alreadyReported || entityMayNotExist))
	{
		// Entity wasn't created for us to update....but we know this already
		return;
	}

	if(Verifyf(pEntity, "Failed to find %s to update, but it wasn't reported as 'failed to create' - 0x%08X - Packet %p", GetShortStr(), (u32)pPacket->GetReplayID(), pPacket))
	{
		tUpdatePacket const* pNextPacket = m_interper.GetNextPacket();

		CInterpEventInfo<tUpdatePacket, T> info;
		info.SetEntity(pEntity);
		info.SetNextPacket(pNextPacket);
		info.SetInterp(controller.GetInterp());
		info.SetFrameInfos(controller.GetFirstFrameInfo(), controller.GetCurrentFrameInfo(), controller.GetNextFrameInfo(), controller.GetLastFrameInfo());

		pPacket->Extract(info);
	}
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::PlayCreatePacket(ReplayController& controller)
{
	tCreatePacket const* pCreatePacket = controller.ReadCurrentPacket<tCreatePacket>();
	REPLAY_CHECK(pCreatePacket->GetPacketID() == m_createPacketID, NO_RETURN_VALUE, "Incorrect Packet");
	controller.AdvancePacket();

	if((controller.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_FWD) && (controller.GetPlaybackFlags().IsSet(REPLAY_CURSOR_JUMP) == false || controller.GetPlaybackFlags().IsSet(REPLAY_CURSOR_JUMP_LINEAR) || controller.IsFineScrubbing())) || 
		(controller.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_BACK) && controller.IsFineScrubbing() && !pCreatePacket->IsFirstCreationPacket()) ||
		controller.GetIsFirstFrame())
	{
		CEntityGet<T> entityGet(pCreatePacket->GetReplayID());
		GetEntity(entityGet);
		T* pEntity = entityGet.m_pEntity;
		if(pEntity == NULL)
		{
 			if(sm_totalEntitiesCreatedThisFrame < entityCreationLimitPerFrame || (controller.GetIsFirstFrame() == false && (controller.GetPlaybackFlags().IsSet(REPLAY_CURSOR_JUMP) == false || controller.GetPlaybackFlags().IsSet(REPLAY_CURSOR_JUMP_LINEAR))))
 			{
				if(!CreateElement(pCreatePacket, controller.GetPlaybackFlags()))
				{
					AddToCreateLater(pCreatePacket);
					replayDebugf2("Adding creation packet to PostUpdate2 %p, 0x%X, count %d", pCreatePacket, pCreatePacket->GetReplayID().ToInt(), m_CreatePostUpdate.GetCount());
				}
				else
				{
					++sm_totalEntitiesCreatedThisFrame;
				}
 			}
 			else
 			{
				if(HasEntityFailedToCreate(pCreatePacket->GetReplayID()) == false)
				{
					AddToCreateLater(pCreatePacket);
					replayDebugf2("Adding creation packet to PostUpdate3 %p, 0x%X, count %d", pCreatePacket, pCreatePacket->GetReplayID().ToInt(), m_CreatePostUpdate.GetCount());
				}
 			}
		}
	}
	else if(controller.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_BACK) && pCreatePacket->IsFirstCreationPacket())
	{
		// Add the deletion of this entity to a list that we might delete later...
		if(m_deletionsOnRewind.Access(pCreatePacket->GetReplayID()) == NULL)
		{
			m_deletionsOnRewind.Insert(pCreatePacket->GetReplayID(), pCreatePacket);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::PlayDeletePacket(ReplayController& controller)
{
	tDeletePacket const* pPacket = controller.ReadCurrentPacket<tDeletePacket>();
	REPLAY_CHECK(pPacket->GetPacketID() == m_deletePacketID, NO_RETURN_VALUE, "Incorrect Packet");
	controller.AdvancePacket();

	if(controller.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_FWD))
	{
		CEntityGet<T> entityGet(pPacket->GetReplayID());
		GetEntity(entityGet);
		T* pEntity = entityGet.m_pEntity;
		if (pEntity)
		{
			REPLAY_CHECK((pPacket->GetReplayID() == pEntity->GetReplayID()), NO_RETURN_VALUE, "CReplayMgr::PlayBackThisFrameInterpolation: Invalid replay ID");
			PrintEntity(pEntity, "DEL", CReplayInterfaceTraits<T>::strShort);
			RemoveElement(pEntity, pPacket, false);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::PostCreateElement(tCreatePacket const* pPacket, T* pElement, const CReplayID& replayID)
{
	SetEntity(replayID, pElement, true);
	pElement->SetReplayID(replayID);

	if(!ReplayEntityExtension::Add(pElement))
	{
		replayAssertf(false, "Failed to add extension to the element");
		return;
	}

	// Set the initial position etc.
	ReplayEntityExtension *pEntityExtention = ReplayEntityExtension::GetExtension(pElement);
	Mat34V entityMatrix = pElement->GetMatrix();
	pEntityExtention->SetMatrix(entityMatrix);

	// Fade the entity into existence
	pElement->GetLodData().SetResetAlpha(true);
	pElement->GetLodData().SetResetDisabled(false);

	tCreatePacket* createPacket = GetCreatePacket(replayID);
	pElement->SetCreationFrameRef(createPacket->GetFrameCreated().GetAsU32());

	PrintEntity(pElement, "NEW", CReplayInterfaceTraits<T>::strShort);

	++m_entitiesDuringPlayback;


	if(pPacket->GetParentID() != ReplayIDInvalid)
	{
		ReplayAttachmentManager::Get().AddAttachment(pPacket->GetReplayID(), pPacket->GetParentID());
	}
}

//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::Process()
{

}

//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::PostProcess()
{
	m_entitiesDuringRecording = (u32)m_pool.GetNoOfUsedSpaces();

	sm_totalEntitiesCreatedThisFrame = 0;
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::ClearWorldOfEntities()
{
	if(m_entitiesDuringPlayback > 0)
	{
		ASSERT_ONLY(u32 numBeforeRemoval = m_entitiesDuringPlayback;)
		s32 numRemoved = 0;
		// Remove replay owned peds
		s32 poolSize = (s32)m_pool.GetSize();
		for(s32 i = 0; i < poolSize; i++)
		{
			T *pEntity = m_pool.GetSlot(i);

			if(pEntity && (pEntity->GetOwnedBy() == ENTITY_OWNEDBY_REPLAY))
			{
				if(RemoveElement(pEntity, NULL))
					++numRemoved;
			}
		}
		replayAssertf(m_entitiesDuringPlayback == 0, "Still have %u replay entities in %s interface (%u before removal, %d removed)", m_entitiesDuringPlayback, GetLongFriendlyStr(), numBeforeRemoval, numRemoved);
	}

	ClearPacketInformation();
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::RegisterAllCurrentEntities()
{
	for(s32 slotIndex = 0; slotIndex < m_pool.GetSize(); ++slotIndex)
	{
		T* pEntity = m_pool.GetSlot(slotIndex);
		if(pEntity && ShouldRecordElement(pEntity))
			RegisterForRecording(pEntity, FrameRef(0, 0));
	}
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::DeregisterAllCurrentEntities()
{
	for(s32 slotIndex = 0; slotIndex < m_pool.GetSize(); ++slotIndex)
	{
		T* pEntity = m_pool.GetSlot(slotIndex);
		if(pEntity && (pEntity->GetOwnedBy() != ENTITY_OWNEDBY_REPLAY))
			DeregisterEntity(pEntity);
	}

	ClearDeletionInformation();

	replayAssert(m_LatestUpdatePackets.GetNumUsed() == 0);
	replayAssert(m_CurrentCreatePackets.GetNumUsed() == 0);
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
bool CReplayInterface<T>::EnsureIsBeingRecorded(const CEntity* pElem)
{
	T* pEntity = (T*)pElem;
	if(ShouldRecordElement(pEntity) == false)
	{
		return false;
	}

	bool hasExtension = ReplayEntityExtension::HasExtension(pEntity);
	replayAssertf(hasExtension, "Entity isn't being recorded for some reason... (%s - %p, 0x%08X)", GetShortStr(), pElem, ((CPhysical*)pElem)->GetReplayID().ToInt());
	if(hasExtension)
		return false;

	return SetCreationUrgent(pEntity->GetReplayID());
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::RegisterForRecording(CPhysical* pEntity, FrameRef creationFrame)
{
	replayFatalAssertf(!ReplayEntityExtension::HasExtension(pEntity), "Entity is already registered?");
	replayAssertf(ShouldRegisterElement((const T*)pEntity), "Registering an entity that should not be registered!");

	for(int i = 0; i < m_pool.GetSize(); ++i)
	{
		if(m_pool.GetSlot(i) == pEntity)
		{
			if(ReplayEntityExtension::HasExtension(pEntity) == false)
			{
				pEntity->SetReplayID(GetNextEntityID((T*)pEntity));

				ReplayEntityExtension::Add(pEntity);
				pEntity->SetCreationFrameRef(creationFrame);
				SetEntity(pEntity->GetReplayID(), (T*)pEntity, false);
			}
			return;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::DeregisterEntity(CPhysical* pEntity)
{
	if(ReplayEntityExtension::HasExtension(pEntity) == true)
	{
		if(m_LatestUpdatePackets.Access(pEntity->GetReplayID()))
		{
			m_LatestUpdatePackets.Delete(pEntity->GetReplayID());
		}

		UnlinkCreatePacket(pEntity->GetReplayID(), false);

		ClrEntity(pEntity->GetReplayID(), (T*)pEntity);

		CReplayExtensionManager::RemoveReplayExtensions(pEntity);
		pEntity->SetReplayID(ReplayIDInvalid);
	}
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::ClearPacketInformation()
{
	m_LatestUpdatePackets.Reset();
	m_CurrentCreatePackets.Reset();
	m_CreatePostUpdate.clear();
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
bool CReplayInterface<T>::EnsureWillBeRecorded(const CEntity* pEntity)
{
	if(ShouldRecordElement((const T*)pEntity) == false)
		return false;

	for(int i = 0; i < m_pool.GetSize(); ++i)
	{
		T* pEntityInPool = m_pool.GetSlot(i);
		if(pEntityInPool == pEntity)
			return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
template<typename T>
bool CReplayInterface<T>::ShouldRecordElement(const CEntity* pEntity) const
{
	return ShouldRecordElement((const T*)pEntity);
}

//////////////////////////////////////////////////////////////////////////
template<typename T>
bool CReplayInterface<T>::SetCreationUrgent(const CReplayID& replayID)
{
	tUpdatePacket** pPacket = m_LatestUpdatePackets.Access(replayID);

	if(pPacket && *pPacket)
	{
		(*pPacket)->SetCreationUrgent();
		return true;
	}

	return false;
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
template<typename PACKETTYPE, typename SUBTYPE>
bool CReplayInterface<T>::RecordElementInternal(T* pElem, CReplayRecorder& recorder, CReplayRecorder& createPacketRecorder, CReplayRecorder& fullCreatePacketRecorder, u16 sessionBlockIndex)
{
	SUBTYPE* pEntity = (SUBTYPE*)pElem;
	replayAssert(pEntity && "CReplayInterface<T>::Record: Invalid entity");
	REPLAY_CHECK(pEntity->GetReplayID() != ReplayIDInvalid && ReplayEntityExtension::HasExtension(pEntity), false, "Can't record an %s because it doesn't have a replay extension (ID was %X)", GetShortStr(), pElem->GetReplayID().ToInt());

	//have we cleared the create packets, because we've moved on to the next block, if so we need to store them again
	bool needsCreatePacket = GetCreatePacket(pEntity->GetReplayID()) == NULL;
	
	//creation frame now get set here rather than when the entity was first registered, this is because otherwise
	//the creation frame would occur before any packets which mean during a rewind it would not get deleted
	//and then any entities that existed before this one that used the same entity ID would fail to be created
	bool firstUpdate = false;
	FrameRef creationFrameRef;
	if(needsCreatePacket && pElem->GetCreationFrameRef(creationFrameRef) == false)
	{
		firstUpdate = true;
	}

	// Full creation packet...used for changing blocks
	if(fullCreatePacketRecorder.RecordPacketWithParams<tCreatePacket>(pEntity, firstUpdate, sessionBlockIndex) == NULL)
		m_fullCreatePacketsBufferFailure = true;

	// Record the creation packet...used for first creation of an entity mid-block
	if(needsCreatePacket)
	{
		if(createPacketRecorder.RecordPacketWithParams<tCreatePacket>(pEntity, firstUpdate, sessionBlockIndex) == NULL)
			m_createPacketsBufferFailure = true;
	}

	// Record the update packet
	if(recorder.RecordPacketWithParams<PACKETTYPE>(pEntity, firstUpdate) == NULL)
		m_recordBufferFailure = true;

	pEntity->m_nDEflags.bReplayWarpedThisFrame = false;

	return true;
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::LinkCreatePacketsForPlayback(ReplayController& controller, u32 endPosition)
{
	while(controller.GetCurrentPosition() != endPosition)
	{
		CPacketBase* pBasePacket = controller.GetCurrentPacket<CPacketBase>();
		
		if(pBasePacket->GetPacketID() == m_createPacketID)
		{
			tCreatePacket* pCreatePacket = (tCreatePacket*)pBasePacket;
			LinkCreatePacket(pCreatePacket);

			CEntityGet<T> entityGet(pCreatePacket->GetReplayID());
			GetEntity(entityGet);
			T* pElem = entityGet.m_pEntity;

			FrameRef createdFrameRef;
			if(pElem->GetCreationFrameRef(createdFrameRef) == false)
			{
				pElem->SetCreationFrameRef(controller.GetCurrentFrameRef());
			}

			pElem->GetCreationFrameRef(createdFrameRef);
			pCreatePacket->SetFrameCreated(createdFrameRef);

			controller.AdvancePacket();
		}
		else if(pBasePacket->GetPacketID() == m_deletePacketID)
		{
			const tDeletePacket* pDeletePacket = (const tDeletePacket*)pBasePacket;
			UnlinkCreatePacket(pDeletePacket->GetReplayID(), controller.GetPlaybackFlags().IsSet(REPLAY_CURSOR_JUMP));

			controller.AdvancePacket();
		}
		else if(IsRelevantPacket(pBasePacket->GetPacketID()))
		{
			controller.AdvancePacket();
		}
		else
		{
			break;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
// Link the update packets from the previous frame to the ones we just recorded.
// Then store the current packets for the same use next frame
template<typename T>
void CReplayInterface<T>::LinkUpdatePackets(ReplayController& controller, u16 previousBlockFrameCount, u32 endPosition)
{
	FrameRef currentFrameRef = controller.GetCurrentFrameRef();
	
	while(controller.GetCurrentPosition() != endPosition)
	{
		CPacketBase* pBasePacket = controller.GetCurrentPacket<CPacketBase>();

		if(IsEntityUpdatePacket(pBasePacket->GetPacketID()) == true)
		{
			tUpdatePacket* pCurrentPacket = static_cast<tUpdatePacket*>(pBasePacket);
			pCurrentPacket->ValidatePacket();
			PostStorePacket(pCurrentPacket, currentFrameRef, previousBlockFrameCount);

			tUpdatePacket** ppPrevious = m_LatestUpdatePackets.Access(pCurrentPacket->GetReplayID());
			if(ppPrevious)
			{
				tUpdatePacket* pPreviousPacket = *ppPrevious;
				if(pPreviousPacket)
				{
					pPreviousPacket->ValidatePacket();
					CBlockInfo const* pCurrBlock = controller.GetCurrentBlock();
					u16 currBlockIndex = pCurrBlock->GetSessionBlockIndex();
					pPreviousPacket->SetNextOffset(currBlockIndex, controller.GetCurrentPosition());
				}
			}

			m_LatestUpdatePackets[pCurrentPacket->GetReplayID()] = pCurrentPacket;
		}

		controller.AdvancePacket();
	}
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::PostStorePacket(tUpdatePacket* pPacket, const FrameRef& currentFrameRef, const u16 previousBlockFrameCount)
{
	CEntityGet<T> entityGet(pPacket->GetReplayID());
	GetEntity(entityGet);
	T* pElem = entityGet.m_pEntity;
	pPacket->PostStore(pElem, currentFrameRef, previousBlockFrameCount);
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
template<typename PACKETTYPE, typename SUBTYPE>
void CReplayInterface<T>::ExtractPacket(T* pElem, CPacketInterp const* pPrevPacket, CPacketInterp const* pNextPacket, const ReplayController& controller)
{
	PACKETTYPE const* pPacket = (PACKETTYPE const*)pPrevPacket;

	CInterpEventInfo<PACKETTYPE, SUBTYPE> info;
	info.SetEntity((SUBTYPE*)pElem);
	info.SetNextPacket((PACKETTYPE const*)pNextPacket);
	info.SetInterp(controller.GetInterp());
	info.SetIsFirstFrame(controller.GetIsFirstFrame());
	//info.SetPlaybackFlags(controller.GetPlaybackFlags()); // This should be here but as we're in lockdown now so leave it commented out for now.

	pPacket->Extract(info);
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
bool CReplayInterface<T>::RemoveElement(T* pElem, const tDeletePacket* /*pDeletePacket*/, bool isRewinding)
{
	(void)isRewinding;
	PrintEntity(pElem, "DEL", CReplayInterfaceTraits<T>::strShort);


	pElem->DetachFromParentAndChildren(0);

	if(ReplayEntityExtension::HasExtension(pElem))
	{
		ClrEntity(pElem->GetReplayID(), pElem);
	}

	CReplayExtensionManager::RemoveReplayExtensions(pElem);
	pElem->SetReplayID(ReplayIDInvalid);

	--m_entitiesDuringPlayback;

	return true;
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::ProcessRewindDeletionWithoutEntity(tCreatePacket const *pCreatePacket, CReplayID const &replayID)
{
	// Most interfaces have nothing to do here.
	(void)pCreatePacket;
	(void)replayID;
	UnlinkCreatePacket(replayID, false);
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::SetEntity(const CReplayID& id, T* pEntity, bool playback)
{
	replayAssertf(id != ReplayIDInvalid && id.GetEntityID() < m_pool.GetSize(), "CReplayMgr::SetEntity: Invalid ID");
	replayAssert(pEntity && "CReplayInterface<T>::SetEntity: Use CReplayMgr::ClrEntity instead");

	if (pEntity && (id.GetEntityID() >= 0) && (id.GetEntityID() < m_pool.GetSize()))
	{
		CTrackerGuard tracker = m_entityRegistry.Get(id.GetEntityID());
		if(!tracker.Get())
		{
			replayAssertf(false, "Tracker is NULL...ID used was %d", id.GetEntityID());
			return;
		}

		CEntityTracker<T>* pTracker = tracker.Get();

		// url:bugstar:7148328 - Sometimes entity behaviour can result in entities that have 0-frame lifespans and/or
		// multiple entities can use the same pool 'slot' in the same frame. This causes issues with creating and deleting
		// entities sometimes as the deletions can be delayed, particularly when scrubbing backwards. Occasionally we hit 
		// this issue where we're trying to create an object in a slot but there's one still there that is about to be deleted
		// but hasn't quite happened yet.
		// Fix of least resistance here, we just force the deletion to happen to make way for the new entity. Will only actually
		// happen if this error is about to happen so shouldn't impact general playback.
		if(playback && pTracker->GetEntity() && pTracker->GetEntity() != pEntity)
		{
			replayDebugf1("Force deleting entity... 0x%X", pTracker->GetEntity()->GetReplayID().ToInt());
			PrintEntity(pTracker->GetEntity(), "FORCE DEL", CReplayInterfaceTraits<T>::strShort);
			UnlinkCreatePacket(pTracker->GetEntity()->GetReplayID(), true);
			RemoveElement(pTracker->GetEntity(), NULL);
		}
		replayAssertf(pTracker->IsEmpty() || (pTracker->GetEntity() == pEntity), "CReplayMgr::SetEntity: ID already assigned, conflicts with 0%X", id.ToInt());
		
		m_entityRegistry.IncrementCount();
		replayDebugf2("SetEntity (%s) 0x%08X, %p, m_used = %u, slots used = %u", GetShortStr(), id.ToInt(), pEntity, (u32)m_entityRegistry.GetCount(), (u32)m_pool.GetNoOfUsedSpaces());
		pTracker->SetEntity(pEntity);
		pTracker->SetInstID(id.GetInstID());
	}
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::ClrEntity(const CReplayID& id, const T* pEntity)
{
	replayAssertf(id != ReplayIDInvalid && id.GetEntityID() < m_pool.GetSize(), "CReplayMgr::ClrEntity: Invalid ID");
	replayAssert(pEntity && "CReplayInterface<T>::ClrEntity: Invalid entity");

	if (pEntity && (id.GetEntityID() >= 0) && (id.GetEntityID() < m_pool.GetSize()))
	{
		CTrackerGuard tracker = m_entityRegistry.Get(id.GetEntityID());
		if(!tracker.Get())
		{
			replayAssertf(false, "Tracker is NULL...ID used was %d", id.GetEntityID());
			return;
		}

		CEntityTracker<T>* pTracker = tracker.Get();
		replayAssertf(pTracker->IsEmpty() || (pTracker->GetEntity() == pEntity), "CReplayMgr::ClrEntity: ID already assigned, conflicts with 0%X", pTracker->GetInstID());

		if ((pTracker->GetEntity() == pEntity) && 
			(pTracker->GetInstID() == id.GetInstID()))
		{
			pTracker->Reset();
			m_entityRegistry.DecrementCount();
			replayDebugf2("ClrEntity (%s) 0x%08X, %p, m_used = %u, slots used = %u", GetShortStr(), id.ToInt(), pEntity, (u32)m_entityRegistry.GetCount(), (u32)m_pool.GetNoOfUsedSpaces()-1);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::GetEntity(CEntityGet<T>& entityGet)
{
	const CReplayID& id = entityGet.m_entityID;
	// Sometimes this happens as not all packets need an entity
	if(id == ReplayIDInvalid)
		return;

	// This can happen if we're checking in the wrong interface...
	if(id.GetEntityID() >= m_pool.GetSize() || id.GetEntityID() < 0)
		return;

	// Check we've not flagged the entity as deleted.
	if(m_deletionsToRecord.Find(id) != -1)
		return;

	CTrackerGuard tracker = m_entityRegistry.Get(id.GetEntityID());
	if(!tracker.Get())
	{
		replayAssertf(false, "Tracker is NULL...ID used was %d", id.GetEntityID());
		return;
	}

	CEntityTracker<T>* pTracker = tracker.Get();
	if(pTracker->GetInstID() == id.GetInstID())
	{
		entityGet.m_pEntity = pTracker->GetEntity();
	}
	else
	{	// Entity wasn't found...set whether we're aware of this...
		entityGet.m_alreadyReported = HasEntityFailedToCreate(entityGet.m_entityID);
		if(entityGet.m_alreadyReported == false)
		{
			 entityGet.m_recentlyUnlinked = m_recentUnlinks.Find(entityGet.m_entityID) != -1;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::FindEntity(CEntityGet<CEntity>& entityGet)
{
	for(int i = 0; i < m_pool.GetSize(); ++i)
	{
		T* pEntity = m_pool.GetSlot(i);
		if(pEntity)
		{
			if(ReplayEntityExtension::HasExtension(pEntity) == true)
			{
				if(pEntity->GetReplayID() == entityGet.m_entityID)
				{
					entityGet.m_pEntity = pEntity;
					return;
				}
			}
		}
	}

	entityGet.m_alreadyReported = HasEntityFailedToCreate(entityGet.m_entityID);
	if(entityGet.m_alreadyReported == false)
	{
		entityGet.m_recentlyUnlinked = m_recentUnlinks.Find(entityGet.m_entityID) != -1;
	}
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::GetEntityAsEntity(CEntityGet<CEntity>& entityGet)
{
	CEntityGet<T> eg(entityGet.m_entityID);
	GetEntity(eg);

	entityGet.m_pEntity = (CEntity*)eg.m_pEntity;
	entityGet.m_alreadyReported = eg.m_alreadyReported;
	entityGet.m_recentlyUnlinked = eg.m_recentlyUnlinked;
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::PrintOutEntities()
{
#if 0// DO_REPLAY_OUTPUT && !__FINAL
	replayDebugf1("Printing out %ss", CReplayInterfaceTraits<T>::strLongFriendly);
	replayDebugf1("");
	replayDebugf1("In the world:");
	for(int i = 0; i < m_pool.GetSize(); ++i)
	{
		T* pEntity = m_pool.GetSlot(i);
		if(pEntity)
		{
			bool registered = (GetEntity(pEntity->GetReplayID()) != NULL);
			bool recorded = IsCurrentlyBeingRecorded(pEntity->GetReplayID());
			bool toBeDeleted = (m_deletionsToRecord.Find(pEntity->GetReplayID().ToInt()) != -1);
			replayDebugf1("0x%08X %s %s %s", pEntity->GetReplayID().ToInt(),
				registered ? " - Registered" : " - NOT Registered",
				recorded ? " - Currently recorded" : " - NOT Currently Recorded",
				toBeDeleted ? " - To Be Deleted!" : "");
		}
	}
#endif	// DO_REPLAY_OUTPUT && !__FINAL
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::OnDelete(CPhysical* pEntity)
{
	CReplayID replayID = pEntity->GetReplayID();

	bool beingRecorded = IsCurrentlyBeingRecorded(replayID);
	if(beingRecorded == false)
	{
		DeregisterEntity(pEntity);
		return;
	}

	replayFatalAssertf(m_deletionsToRecord.Find(replayID) == -1, "Replay ID already set for deletion (0x%X)", replayID.ToInt());
	replayFatalAssertf(replayID != ReplayIDInvalid, "Replay ID was invalid for deletion");
	tDeletionData& data = m_deletionsToRecord.Append();
	data.replayID = replayID;
	data.pEntity = pEntity;

	//pass the in last know good create packet, in case we need to get at data that has been trashed by destructor
	const sCreatePacket* pLastCreatePacketStruct = m_CurrentCreatePackets.Access(replayID);	
	const tCreatePacket* pLastCreatePacket = pLastCreatePacketStruct != NULL ? pLastCreatePacketStruct->pCreatePacket_const : NULL;
	
	replayAssertf(pLastCreatePacket, "Cannot clone packet extension, previous create was NULL");

	((tDeletePacket*)(data.deletePacket))->Store(verify_cast<const T*>(pEntity), pLastCreatePacket);
	((tDeletePacket*)(data.deletePacket))->StoreExtensions(verify_cast<const T*>(pEntity), pLastCreatePacket);

	m_deletionPacketsSize += ((tDeletePacket*)(data.deletePacket))->GetPacketSize();
	replayFatalAssertf(((tDeletePacket*)(data.deletePacket))->GetPacketSize() <= CReplayInterfaceTraits<T>::maxDeletionSize, "Deletion packet is too large (%u bytes) for %s, you just stomped over someones lawn", ((tDeletePacket*)(data.deletePacket))->GetPacketSize(), GetShortStr());
	replayDebugf2("REPLAY: DEL: %s %p: ID: %0X - Bytes %u\n", GetShortStr(), pEntity, replayID.ToInt(), ((tDeletePacket*)(data.deletePacket))->GetPacketSize());

	ClrEntity(replayID, (T*)pEntity);

	if(m_LatestUpdatePackets.Access(replayID))
	{
		m_LatestUpdatePackets[replayID]->SetNextDeleted();
		m_LatestUpdatePackets.Delete(replayID);
	}

	m_recentDeletions.PushAndGrow(replayID);
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::RecordDeletionPackets(ReplayController& controller)
{
	u32 deletesToRecord = m_deletionsToRecord.size();
	m_recentDeletions.clear();
 	for(int i = 0; i < deletesToRecord; ++i)
 	{
		controller.RecordBuffer((u8*)(m_deletionsToRecord[i].deletePacket), ((tDeletePacket*)(m_deletionsToRecord[i].deletePacket))->GetPacketSize());
 	}
 	m_deletionsToRecord.clear();
	m_deletionPacketsSize = 0;

	controller.SetPacket<CPacketEnd>();
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::ClearDeletionInformation()
{
	OUTPUT_ONLY(u32 deletesToRecord = m_deletionsToRecord.size());
	replayDebugf1("Clearing deletions of %s...%u", GetShortStr(), deletesToRecord);
	m_recentDeletions.clear();

	m_deletionsToRecord.clear();
	m_deletionPacketsSize = 0;

	ClearPacketInformation();
	m_deletionsToRecord.ResetCount();

	replayDebugf1("Post Clearing deletions %u, %d, %d, %d", (u32)m_entityRegistry.GetCount(), m_LatestUpdatePackets.GetNumUsed(), m_deletionsToRecord.GetCount(), m_CurrentCreatePackets.GetNumUsed());
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
bool CReplayInterface<T>::IsRecentlyDeleted(const CReplayID& id) const
{
	return m_recentDeletions.Find(id) != -1;
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::OffsetCreatePackets(s64 offset)
{
	typename atMap<CReplayID, sCreatePacket>::Iterator createIter = m_CurrentCreatePackets.CreateIterator();
	while(createIter.AtEnd() == false)
	{
		char* p = (char*)createIter.GetData().pCreatePacket;
		p += offset;
		tCreatePacket* testPtr = (tCreatePacket*)p;
		testPtr->ValidatePacket();
		createIter.GetData() = testPtr;

		createIter.Next();
	}
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::OffsetUpdatePackets(s64 offset, u8* pStart, u32 blockSize)
{
	typename atMap<CReplayID, tUpdatePacket*>::Iterator updateIter = m_LatestUpdatePackets.CreateIterator();
	while(updateIter.AtEnd() == false)
	{
		u8* p = (u8*)updateIter.GetData();
		if(p >= pStart && p < (pStart + blockSize))
		{
			p += offset;
			tUpdatePacket* testPtr = (tUpdatePacket*)p;
			testPtr->ValidatePacket();
			updateIter.GetData() = testPtr;
		}

		updateIter.Next();
	}
}

//////////////////////////////////////////////////////////////////////////
template<typename T>
bool CReplayInterface<T>::IsRelevantPacket(eReplayPacketId packetID) const
{
	const CPacketDescriptor<T>* pPacketDescriptor = NULL;
	if(FindPacketDescriptor(packetID, pPacketDescriptor))
		return true;

	if(packetID == PACKETID_ALIGN)
		return true;

	return false;
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
bool CReplayInterface<T>::FindPacketDescriptor(eReplayPacketId packetID, const CPacketDescriptor<T>*& pDescriptor) const
{
	for(int i = 0; i < m_numPacketDescriptors; ++i)
	{
		const CPacketDescriptor<T>& desc = m_packetDescriptors[i];
		if(desc.GetPacketID() == packetID)
		{
			pDescriptor = &desc;
			return true;
		}
	}

	return false;
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
bool CReplayInterface<T>::FindPacketDescriptorForEntityType(s32 et, const CPacketDescriptor<T>*& pDescriptor) const
{
	for(int i = 0; i < m_numPacketDescriptors; ++i)
	{
		const CPacketDescriptor<T>& desc = m_packetDescriptors[i];
		if(desc.GetEntityType() == et)
		{
			pDescriptor = &desc;
			return true;
		}
	}

	return false;
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
bool CReplayInterface<T>::GetPacketSizeForEntityType(s32 entityType, u32& packetSize) const
{
	for(int i = 0; i < m_numPacketDescriptors; ++i)
	{
		const CPacketDescriptor<T>& desc = m_packetDescriptors[i];
		if(desc.GetEntityType() == entityType)
		{
			packetSize = desc.GetPacketSize();
			return true;
		}
	}
	replayFatalAssertf(false, "Failed to find size for entity type");
	return false;
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
bool CReplayInterface<T>::GetPacketName(eReplayPacketId packetID, const char*& packetName) const
{
	const CPacketDescriptor<T>* pPacketDescriptor = NULL;
	if(FindPacketDescriptor(packetID, pPacketDescriptor))
	{
		packetName = pPacketDescriptor->GetPacketName();
		return true;
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
#if __BANK
template<typename T>
bkGroup* CReplayInterface<T>::AddDebugWidgets()
{
	bkGroup* pGroup = iReplayInterface::AddDebugWidgets(CReplayInterfaceTraits<T>::strLongFriendly);
	if(pGroup == NULL)
		return NULL;

	pGroup->AddToggle("Enabled", &m_enabled);

	atString str(CReplayInterfaceTraits<T>::strShortFriendly);
	str += "s during recording";
	pGroup->AddSlider(str.c_str(), &m_entitiesDuringRecording, 0, (int) m_pool.GetSize(), 1);

	str = CReplayInterfaceTraits<T>::strShortFriendly;
	str += "s during playback";
	pGroup->AddSlider(str.c_str(), &m_entitiesDuringPlayback, 0, (int) m_pool.GetSize(), 1);

	return pGroup;
}
#endif // __BANK


//////////////////////////////////////////////////////////////////////////
template<typename T>
int CReplayInterface<T>::GetNextEntityID(T* pEntity)
{
	size_t count = m_pool.GetSize();
	ASSERT_ONLY(size_t used = m_pool.GetNoOfUsedSpaces();)
	CTrackerGuard lockTracker = m_entityRegistry.Get(0);// This simply locks the registry
	for (s16 entityIndex = 0; entityIndex < (s16)count; ++entityIndex)
	{
		CTrackerGuard tracker = m_entityRegistry.Get(entityIndex, false);
		if(!tracker.Get())
		{
			replayAssertf(false, "Tracker is NULL...ID used was %d", entityIndex);
			return -1;
		}

		CEntityTracker<T>* pTracker = tracker.Get();
		if (pTracker->IsEmpty())
		{
			s16 iInstID = GetNextInstanceID();

			pTracker->SetEntity(pEntity);
			pTracker->SetInstID(iInstID);

			CReplayID oID(entityIndex, iInstID);
			return oID;
		}
	}

	replayFatalAssertf(false, "CReplayMgr::GetNextEntityID: Out of entity IDs %u", (u32)used);
	return -1;
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::PrintEntity(T* pEntity, const char* str1, const char* str2)
{
	(void) pEntity; (void) str1; (void) str2; 

	replayDebugf2("REC: %s %s: %p: ID: 0x%0X: MI: %4d: POS: %5.1f:%5.1f:%5.1f\n", 
		str1, str2,
		pEntity, 
		pEntity->GetReplayID().ToInt(), 
		pEntity->GetModelIndex(), 
		pEntity->GetTransform().GetPosition().GetXf(), 
		pEntity->GetTransform().GetPosition().GetYf(), 
		pEntity->GetTransform().GetPosition().GetZf());
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
T* CReplayInterface<T>::EnsureEntityIsCreated(const CReplayID& id, const CReplayState& state, bool canAddToCreateLater)
{
	CEntityGet<T> entityGet(id);
	GetEntity(entityGet);
	T* pEntity = entityGet.m_pEntity;

	//the entity hasn't been created yet
	if(!pEntity)
	{
		//find the create packet for this vehicle and use create a new version of it
		const tCreatePacket* pCreatePacket = GetCreatePacket(id);
		REPLAY_CHECK(pCreatePacket != NULL, NULL, "Error could not get a create packet for this entity (0x%08X)", id.ToInt());

		pEntity = CreateElement(pCreatePacket, state);
		if(!pEntity && canAddToCreateLater)
		{
			AddToCreateLater(pCreatePacket);
			replayDebugf2("Adding creation packet to PostUpdate4 %p, 0x%X, count %d", pCreatePacket, pCreatePacket->GetReplayID().ToInt(), m_CreatePostUpdate.GetCount());
		}
	}

	return pEntity;
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::LinkCreatePacket(const tCreatePacket* pCreatePacket)
{
	sysCriticalSection cs(m_createPacketCS);
	//check to see if we've got a create packet, and if so add a link to it so we can access it without having to search for this packet again
	if(pCreatePacket != NULL)
	{
		if(m_CurrentCreatePackets[pCreatePacket->GetReplayID()] != pCreatePacket)
		{
			replayDebugf2("Link creation packet %s 0x%X, count %d", GetShortStr(), pCreatePacket->GetReplayID().ToInt(), m_CurrentCreatePackets.GetNumUsed());
			m_CurrentCreatePackets[pCreatePacket->GetReplayID()] = pCreatePacket;
		}
	}
}


template<typename T>
void CReplayInterface<T>::UnlinkCreatePacket(const CReplayID& replayID, bool jumping)
{
	sysCriticalSection cs(m_createPacketCS);

	if(GetCreatePacket(replayID) != NULL)
	{
		replayDebugf2("Unlink creation packet %s 0x%X, count %d", GetShortStr(), replayID.ToInt(), m_CurrentCreatePackets.GetNumUsed());
		m_CurrentCreatePackets.Delete(replayID);

		if(jumping && m_recentUnlinks.Find(replayID) == -1)
			m_recentUnlinks.PushAndGrow(replayID);

		// For safety!
		// If we're unlinking the create packet (it'll no long be available) because we've passed it
		// then check we've not flagged it to be created later on (probably due to too many entities being created in one frame).
		typename atArray<const tCreatePacket*>::iterator it = m_CreatePostUpdate.begin();
		for(; it != m_CreatePostUpdate.end(); )
		{
			if((*it)->GetReplayID() == replayID)
			{
				m_CreatePostUpdate.erase(it);
			}
			else
			{
				++it;
			}
		}
	}
}



//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::HandleCreatePacket(const tCreatePacket* pCreatePacket, bool notDelete, bool firstFrame, const CReplayState& state)
{
	// If we're handling a deletion packet and we're going backwards
	// OR
	// we're handling any other creation packet and we're going forwards
	// then Link the creation packet...
	if((notDelete && state.IsSet(REPLAY_DIRECTION_FWD)) || (!notDelete && state.IsSet(REPLAY_DIRECTION_BACK)) || firstFrame)
	{
		LinkCreatePacket(pCreatePacket);
	}
	else
	{
		// If we're handling a deletion packet and we're going forwards
		// then unlink the creation packet
		if(!notDelete && state.IsSet(REPLAY_DIRECTION_FWD))
		{
			UnlinkCreatePacket(pCreatePacket->GetReplayID(), state.IsSet(REPLAY_CURSOR_JUMP));
		}

		// If we're going backwards and it's the first creation packet then we would have done the same...
		// However we now add the entities to be deleted to a list so they can be deleted on the frame previous
		// to the one the creation packet resides.  The unlink of creation packet is done there.
	}
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
typename CReplayInterface<T>::tCreatePacket* CReplayInterface<T>::GetCreatePacket(const CReplayID& replayID)
{
	sysCriticalSection cs(m_createPacketCS);
	if(HasCreatePacket(replayID) == false)
	{
		return NULL;
	}

	sCreatePacket& createPacket = m_CurrentCreatePackets[replayID];
	replayAssertf(createPacket.pCreatePacket_const->ValidatePacket() && replayID == createPacket.pCreatePacket_const->GetReplayID(), "Create packet isn't valid\n");

	return createPacket.pCreatePacket;
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
const typename CReplayInterface<T>::tCreatePacket* CReplayInterface<T>::GetCreatePacket(const CReplayID& replayID) const
{
	sysCriticalSection cs(m_createPacketCS);
	if(HasCreatePacket(replayID) == false)
	{
		return NULL;
	}

	sCreatePacket& createPacket = m_CurrentCreatePackets[replayID];
	replayAssertf(createPacket.pCreatePacket_const->ValidatePacket() && replayID == createPacket.pCreatePacket_const->GetReplayID(), "Create packet isn't valid\n");

	return createPacket.pCreatePacket_const;
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
bool CReplayInterface<T>::GetBlockDetails(char* pString, bool& err, bool recording) const
{
	sprintf(pString, "%s:\n   %d bytes for %d elements (max %d, in pool %d).\n Record Buffer: %d (c:%d%s)\n Create Buffer: %d (c:%d)\n Full Create Buffer: %d (c:%d)", 
		GetLongFriendlyStr(), 
		GetActualMemoryUsage(), 
		recording ? m_entitiesDuringRecording : m_entitiesDuringPlayback, 
		(int)m_pool.GetSize(), (int)m_pool.GetNoOfUsedSpaces(),
		m_recordBufferUsed, m_recordBufferSize, m_recordBufferFailure ? " EXCEEDED" : "",
		m_createPacketsBufferUsed, m_createPacketsBufferSize,
		m_fullCreatePacketsBufferUsed, m_fullCreatePacketsBufferSize);

	err = m_recordBufferFailure | m_createPacketsBufferFailure | m_fullCreatePacketsBufferFailure;
	return true;
}

#if __BANK
//////////////////////////////////////////////////////////////////////////
template<typename T>
bool CReplayInterface<T>::ScanForAnomalies(char* pString, int& index) const
{
	if(m_anomaliesCanExist)
		return false;

	for(; index < m_pool.GetSize(); )
	{
		T* pEntity = m_pool.GetSlot(index++);
		if(pEntity)
		{
			if(pEntity->GetOwnedBy() != ENTITY_OWNEDBY_REPLAY)
			{
				atString ownedByString;
				EntityDetails_GetOwner(pEntity, ownedByString);

				const char* poptypeString = CTheScripts::GetPopTypeName(pEntity->PopTypeGet());
				sprintf(pString, "%s not owned by replay!  Ownedby %s, Poptype %s, @ %f, %f, %f", 
					GetShortStr(), 
					ownedByString.c_str(), 
					poptypeString,
					pEntity->GetTransform().GetPosition().GetXf(), 
					pEntity->GetTransform().GetPosition().GetYf(), 
					pEntity->GetTransform().GetPosition().GetZf());
				return true;
			}
		}
	}

	return false;
}
#endif // __BANK

//////////////////////////////////////////////////////////////////////////
template<typename T>
bool CReplayInterface<T>::HasCreatePacket(const CReplayID& replayID) const
{
	sysCriticalSection cs(m_createPacketCS);
	return m_CurrentCreatePackets.Access(replayID) != NULL;
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::SetCurrentlyBeingRecorded(int index, T* pEntity)
{
	if(pEntity != NULL)
		m_currentlyRecorded[index] = pEntity->GetReplayID();
	else
		m_currentlyRecorded[index] = 0;
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::HandleFailedToLoadModelError(const tCreatePacket* pPacket)
{
	char err[1024] = {0};
#if __BANK
	const char* pName = "NoName";
	const char* pMount = "NoMount";

	fwModelId iModelId = CModelInfo::GetModelIdFromName(pPacket->GetModelNameHash());
	if(iModelId.IsValid())
	{
		CReplayModelManager::GetModelInfo(iModelId, pName, pMount);

		snprintf(err, 1024, "Failed to load model for (%u - %u - %s - %s).", pPacket->GetModelNameHash(), pPacket->GetMapTypeDefIndex().Get(), pName, pMount);
	}
	else
	{
		snprintf(err, 1024, "Failed to load model for (%u - %s - %s).", pPacket->GetModelNameHash(), pName, pMount);
	}
#endif
	SetEntityFailedToCreate(pPacket->GetReplayID(), err);

	// If we have a post create request for entities using this model then remove them here
	for(int i = m_CreatePostUpdate.GetCount() - 1; i >= 0; --i)
	{
		if(m_CreatePostUpdate[i]->GetModelNameHash() == pPacket->GetModelNameHash())
		{
			m_CreatePostUpdate.Delete(i);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
bool CReplayInterface<T>::IsCurrentlyBeingRecorded(const CReplayID& replayID) const
{
	return m_LatestUpdatePackets.Access(replayID) != NULL;
}


//////////////////////////////////////////////////////////////////////////
template<typename T>
void CReplayInterface<T>::ResetEntityCreationFrames()
{
	ClearDeletionInformation();

	for(int i = 0; i < m_pool.GetSize(); ++i)
	{
		T* pEntity = m_pool.GetSlot(i);
		if(pEntity)
		{
			if(ReplayEntityExtension::HasExtension(pEntity) == true)
			{
				pEntity->SetCreationFrameRef(FrameRef::INVALID_FRAME_REF);
				
				pEntity->m_nDEflags.bReplayWarpedThisFrame = false;
			}
		}
	}
}

#endif
