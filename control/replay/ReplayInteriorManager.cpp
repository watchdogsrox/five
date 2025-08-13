#include "ReplayInteriorManager.h"

#if GTA_REPLAY

#include "ReplayController.h"
#include "control/replay/Misc/InteriorPacket.h"
#include "control/replay/PacketBasics.h"
#include "Objects/Door.h"
#include "fwscene/search/SearchVolumes.h"
#include "fwutil/PtrList.h"
#include "scene/portals/InteriorInst.h"
#include "scene/portals/InteriorProxy.h"
#include "scene/portals/portalDebugHelper.h"

//=====================================================================================================
atArray<const CPacketInterior*>	ReplayInteriorManager::m_interiorStates;

CInteriorInst *ReplayInteriorManager::InteriorProxyRequest::GetInterior()
{
	if(m_pInteriorProxy)
	{
		// This test means the interior has loaded fully.
		if(m_pInteriorProxy->GetCurrentState() >= CInteriorProxy::PS_FULL_WITH_COLLISIONS)
		{
			return m_pInteriorProxy->GetInteriorInst();
		}
	}
	return NULL;
}


void ReplayInteriorManager::InteriorProxyRequest::Release()
{
	ReplayInteriorManager::ReleaseInteriorProxy(m_pInteriorProxy);

	m_lastPreloadTime = 0;
	m_ProxyNameHash = 0xffffffff;
	m_pInteriorProxy = NULL;
	m_stateRequest = 0;
	m_RoomRequestTimes.Reset();
}


bool ReplayInteriorManager::InteriorProxyRequest::HasLoaded()
{
	return ReplayInteriorManager::HasInteriorProxyLoaded(m_pInteriorProxy, (CInteriorProxy::eProxy_State)m_stateRequest);
}


//=====================================================================================================

atArray<ReplayInteriorData>	ReplayInteriorManager::ms_InteriorState;
atArray<ReplayInteriorData> ReplayInteriorManager::ms_interiorStateBackup;
CPacketInterior				ReplayInteriorManager::ms_interiorPacket;
bool						ReplayInteriorManager::ms_mismatch = false;
bool						ReplayInteriorManager::ms_extractedPacket = true;

// On record is used to track delta's, on playback is used to restore the states after playback has completed.
atArray<atArray<SEntitySet>>		ReplayInteriorManager::ms_EntitySetLastState;

bool ReplayInteriorManager::m_dt1_03_carparkHack = false;
bool ReplayInteriorManager::m_dt1_03_carparkHackOnlyOnce = true;


void ReplayInteriorManager::Init(u32 noOfInteriorProxyRequests, u32 noOfRoomRequests)
{
	m_InteriorProxyRequests.Reset();
	m_InteriorProxyRequests.Resize(noOfInteriorProxyRequests);
	m_RoomRequests.Reset();
	m_RoomRequests.Resize(noOfRoomRequests);

	m_dt1_03_carparkHack = false;
	m_dt1_03_carparkHackOnlyOnce = true;

	m_interiorStates.clear();
}


void ReplayInteriorManager::OnRecordStart()
{
	ms_InteriorState.Reset();

	ResetEntitySetLastState(false);
}

void ReplayInteriorManager::ResetEntitySetLastState(bool copyLastBlockData)
{
	ms_EntitySetLastState.Reset();
	ms_EntitySetLastState.Resize(CInteriorProxy::GetPool()->GetSize());

	// On a block change we have recorded all the data to the start of the block
	// Set the last state to the current entity set data.
	if(copyLastBlockData)
	{
		BackupEntitySetData(ms_EntitySetLastState);
	}
}

void ReplayInteriorManager::CollectData()
{
	ms_mismatch = false;

	atArray<ReplayInteriorData> currentInteriorState;
	ConsolidateInteriorData(currentInteriorState, true);

	// Does the new list equate to the last list?
	if( currentInteriorState.size() != ms_InteriorState.size() )
	{
		ms_mismatch = true;
	}
}

void ReplayInteriorManager::RecordData(CReplayRecorder& fullCreatePacketRecorder, CReplayRecorder& createPacketRecorder)
{
	// Entity set stuff
	PrecalcEntitySets(fullCreatePacketRecorder, createPacketRecorder, ms_EntitySetLastState);
	
	ConsolidateInteriorData(ms_InteriorState, true);

	if( ms_InteriorState.GetCount() > 0)
	{
		ms_interiorPacket.Store(ms_InteriorState);
		// If it's a mismatch then record it in the create recorder, done on update.
		// Note I'm not sure we should be handling this mismatch as it would refresh the interior in the middle of a clip.
		if(ms_mismatch)
		{
			createPacketRecorder.RecordPacket<CPacketInterior>(ms_interiorPacket);
			ms_mismatch = false;
		}
		
		// Add it to the full create so it's added to the start of each block.
		fullCreatePacketRecorder.RecordPacket<CPacketInterior>(ms_interiorPacket);
	}
}

void ReplayInteriorManager::OnEntry()
{
	ms_extractedPacket = false;

	m_dt1_03_carparkHack = false;
	m_dt1_03_carparkHackOnlyOnce = true;

	// Save interior state
	ConsolidateInteriorData(ms_interiorStateBackup, false);

	// Save interior entity sets
	BackupEntitySetData(ms_EntitySetLastState);

	m_interiorStates.clear();
}

void ReplayInteriorManager::OnExit()
{
	if(ms_extractedPacket)
	{
		ms_extractedPacket = false;

		// Restore interior state
		SetInteriorData(ms_interiorStateBackup);
	}

	RestoreEntitySetData(ms_EntitySetLastState);

	m_interiorStates.clear();
}

void ReplayInteriorManager::SubmitNewInteriorData(u8 count, const ReplayInteriorData *pData)
{
	// Restore interior state
	if(!ms_extractedPacket && count > 0)
	{
		ms_InteriorState.Reset();
		ms_extractedPacket = true;
		ms_InteriorState.CopyFrom(pData, count);
		SetInteriorData(ms_InteriorState);
	}
}

void ReplayInteriorManager::SetInteriorData(atArray<ReplayInteriorData>& interiorArray)
{
	//enable everything in the pool first as we only store disabled/capped proxy information.
	int poolSize = CInteriorProxy::GetPool()->GetSize();
	for(int i = 0; i < poolSize; i++)
	{
		CInteriorProxy *pIntProxy = CInteriorProxy::GetPool()->GetSlot(i);
		if (pIntProxy && (pIntProxy->IsContainingImapActive() 
			|| pIntProxy->GetNameHash() == ATSTRINGHASH("v_farmhouse", 0xe3825384) // Hack for B*2556357
			|| pIntProxy->GetNameHash() == 0x6acc1a25)) // Hack for B*5488897 as it was capped by script when exiting the arena
		{
			if(pIntProxy->GetIsDisabled())
			{
				pIntProxy->SetIsDisabled(false);
			}
			if(pIntProxy->GetIsCappedAtPartial())
			{
				pIntProxy->SetIsCappedAtPartial(false);
			}
		}
	}

	for(int i = 0; i < interiorArray.GetCount(); i++)
	{
		Vector3 pos;
		interiorArray[i].m_position.Load(pos);
		// get the interior 
		CInteriorProxy *pIntProxy = GetProxyAtCoords(pos, interiorArray[i].m_hashName);
		if (pIntProxy)
		{
			//Set stored params
			pIntProxy->SetIsDisabled(interiorArray[i].m_disabled);
			pIntProxy->SetIsCappedAtPartial(interiorArray[i].m_capped);
		}
	}
}


CInteriorProxy *ReplayInteriorManager::GetProxyAtCoords(const Vector3& vecPositionToCheck, u32 typeKey, bool onlyActive)
{
	// get a list of MLOs at desired position
	fwIsSphereIntersecting intersection(spdSphere(RCC_VEC3V(vecPositionToCheck), ScalarV(V_FLT_SMALL_1)));
	fwPtrListSingleLink	scanList;
	CInteriorProxy::FindActiveIntersecting(intersection, scanList, onlyActive);

	// get the closest interior of the right type
	fwPtrNode* pNode = scanList.GetHeadPtr();
	CInteriorProxy* pClosest = NULL;
	float closestDist2 = FLT_MAX;

	Vec3V vPositionToCheck = RCC_VEC3V(vecPositionToCheck);
	while(pNode)
	{
		CInteriorProxy* pIntProxy = reinterpret_cast<CInteriorProxy*>(pNode->GetPtr());
		if (pIntProxy && (!onlyActive || pIntProxy->IsContainingImapActive()))
		{
			if (pIntProxy->GetNameHash() == typeKey)
			{
				Vec3V proxyPosition;
				pIntProxy->GetPosition(proxyPosition);
				float dist2 = DistSquared(proxyPosition, vPositionToCheck).Getf();
				if (dist2 < closestDist2)
				{
					pClosest = pIntProxy;
					closestDist2 = dist2;
				}
			}
		}

		pNode = pNode->GetNextPtr();
	}

	return pClosest;
}


void ReplayInteriorManager::ProcessInteriorStatePackets()
{
	for(int i = 0; i < m_interiorStates.size(); ++i)
	{
		const CPacketInterior* pPacket = m_interiorStates[i];

		SubmitNewInteriorData((u8)pPacket->GetCount(), pPacket->GetData());
	}

	m_interiorStates.clear();
}


void ReplayInteriorManager::ConsolidateInteriorData(atArray<ReplayInteriorData>& interiorArray, bool onlyForceDisabledCapped)
{
	interiorArray.Reset();
	int poolSize = CInteriorProxy::GetPool()->GetSize();
	for(int i = 0; i < poolSize; i++)
	{
		CInteriorProxy *pIntProxy = CInteriorProxy::GetPool()->GetSlot(i);
		if (pIntProxy && pIntProxy->IsContainingImapActive() &&(!onlyForceDisabledCapped || pIntProxy->GetIsCappedAtPartial() || pIntProxy->GetIsDisabled()))
		{
			ReplayInteriorData interiorData;
			interiorData.m_hashName = pIntProxy->GetNameHash();
			Vec3V proxyPosition;
			pIntProxy->GetPosition(proxyPosition);
			interiorData.m_position.Store(proxyPosition);
			interiorData.m_disabled = pIntProxy->GetIsDisabled();
			interiorData.m_capped = pIntProxy->GetIsCappedAtPartial();
			interiorArray.PushAndGrow(interiorData);
		}
	}
};

// Copy the entire entity set data to an array
void ReplayInteriorManager::BackupEntitySetData(atArray<atArray<SEntitySet>>&backupSetData)
{
	backupSetData.Reset();

	int poolSize = CInteriorProxy::GetPool()->GetSize();
	if( poolSize )
	{
		backupSetData.ResizeGrow(poolSize);
		for(int i = 0; i < poolSize; i++)
		{
			CInteriorProxy *pIntProxy = CInteriorProxy::GetPool()->GetSlot(i);
			if( pIntProxy )
			{
				atArray<SEntitySet>	&proxyEntSet = pIntProxy->GetEntitySets();
				backupSetData[i] = proxyEntSet;
			}
		}
	}
}

void ReplayInteriorManager::RestoreEntitySetData(atArray<atArray<SEntitySet>>&backupSetData)
{
	int poolSize = CInteriorProxy::GetPool()->GetSize();
	for(int i = 0; i < backupSetData.size(); i++)
	{
		if( i < poolSize )
		{
			CInteriorProxy *pIntProxy = CInteriorProxy::GetPool()->GetSlot(i);
			if( pIntProxy )
			{
				atArray<SEntitySet>	&proxyEntSet = pIntProxy->GetEntitySets();
				proxyEntSet.Reset();
				if( backupSetData[i].size() )
				{
					proxyEntSet.Reserve(MAX_ENTITY_SETS);
					for(int j=0; j<backupSetData[i].size(); j++)
					{
						proxyEntSet.Push(backupSetData[i][j]);
					}
				}
			}
		}
	}
}


void ReplayInteriorManager::PrecalcEntitySets(CReplayRecorder& fullCreatePacketRecorder, CReplayRecorder& createPacketRecorder, atArray<atArray<SEntitySet>>& sourceData)
{
	// Record all data for the block start
	int poolSize = CInteriorProxy::GetPool()->GetSize();
	for(int i = 0; i < poolSize; i++)
	{
		CInteriorProxy *pIntProxy = CInteriorProxy::GetPool()->GetSlot(i);
		if( pIntProxy && pIntProxy->IsContainingImapActive() && !pIntProxy->GetIsDisabled() )
		{
 			atArray<SEntitySet>	&proxyArray = pIntProxy->GetEntitySets();
 
 			if(proxyArray.size() > 0)
 			{
 				Vec3V position;
 				pIntProxy->GetPosition(position);
 
 				fullCreatePacketRecorder.RecordPacketWithParams<CPacketInteriorEntitySet2, const u32, Vec3V&, const atArray<SEntitySet>&>(pIntProxy->GetNameHash(), position, proxyArray);
 
 				// Update source if different
 				bool isAlreadySet = true;
 				if( sourceData[i].size() == proxyArray.size() )
 				{
 					for(int j=0;j<proxyArray.size();j++)
 					{
 						if( sourceData[i][j] != proxyArray[j] )
 						{
 							isAlreadySet = false;
 							break;
 						}
 					}
 				}
 				else
 				{
 					isAlreadySet = false;
 				}
 
 				// Only record changes from our base set if the IMAP is active.
 				if( isAlreadySet == false )
 				{
 					sourceData[i] = proxyArray;
 
 					Vec3V position;
 					pIntProxy->GetPosition(position);
 
 					createPacketRecorder.RecordPacketWithParams<CPacketInteriorEntitySet2, const u32, Vec3V&, const atArray<SEntitySet>&>(pIntProxy->GetNameHash(), position, proxyArray);
 				}
 			}
		}
	}
}


////////////////////////////////////////////////////////////////////////////////////////
// Interior Proxy related.


bool ReplayInteriorManager::AddInteriorProxyPreloadRequest(u32 interiorProxyName, u32 posHash, CInteriorProxy::eProxy_State stateRequest, u32 currGameTime)
{
	bool ret = true;
	InteriorProxyRequest testReq(interiorProxyName, posHash, stateRequest);
	InteriorProxyRequest *pRequest = FindOrAddPreloadRequest(testReq, ret, currGameTime);

	if(pRequest)
		RequestInteriorProxy(pRequest->m_pInteriorProxy, stateRequest);

	return ret;
}


bool ReplayInteriorManager::AddRoomRequest(u32 interiorProxyName, u32 posHash, u32 roomIndex, u32 currGameTime)
{
	bool ret = true;
	//Default to FULL_WITH_COLLISION but for rooms it's not used.
	InteriorProxyRequest testReq(interiorProxyName, posHash, CInteriorProxy::PS_FULL_WITH_COLLISIONS);
	InteriorProxyRequest *pRequest = FindOrAddPreloadRequest(testReq, ret, currGameTime);

	if(pRequest)
	{
		CInteriorInst *pInterior = pRequest->GetInterior();

		if(pInterior)
		{
			if(pRequest->m_RoomRequestTimes.GetCount() == 0)
				pRequest->m_RoomRequestTimes.Resize(pInterior->GetNumRooms());

			pRequest->m_RoomRequestTimes[roomIndex] = currGameTime;
		}
	}
	return ret;
}


void ReplayInteriorManager::UpdateInteriorProxyRequests(u32 time)
{
	for(int i = 0; i < m_InteriorProxyRequests.size(); ++i)
	{
		InteriorProxyRequest &req = m_InteriorProxyRequests[i];

		if(!req.IsFree())
		{
			// Is the proxy still in scope ?
			if(abs((s64)time - (s64)req.m_lastPreloadTime) > (s64)((float)REPLAY_PRELOAD_TIME * 1.25f))
			{
				// No...Release it.
				req.Release();
				continue;
			}

			RequestInteriorProxy(req.m_pInteriorProxy, (CInteriorProxy::eProxy_State)req.m_stateRequest);
			CInteriorInst *pInterior = req.GetInterior();

			if(pInterior)
			{
				// Update room requests.
				for(int roomIndex=0; roomIndex<req.m_RoomRequestTimes.GetCount(); roomIndex++)
				{
					// Is the room request still in scope ?
					if(abs((s64)time - (s64)req.m_RoomRequestTimes[roomIndex]) < (s64)((float)REPLAY_PRELOAD_TIME * 1.25f))
						pInterior->RequestRoom(roomIndex, STRFLAG_NONE);
				}
			}
		}
	}
}


void ReplayInteriorManager::FlushPreloadRequests()
{
	for(int i = 0; i < m_InteriorProxyRequests.size(); ++i)
	{
		InteriorProxyRequest &req = m_InteriorProxyRequests[i];

		if(!req.IsFree())
			req.Release();
	}
}


ReplayInteriorManager::InteriorProxyRequest *ReplayInteriorManager::FindOrAddPreloadRequest(InteriorProxyRequest &inReq, bool& preloadSuccess, u32 currGameTime)
{
	int index = m_InteriorProxyRequests.Find(inReq);
	if(index != -1)
	{
		// We do, update the time on it
		InteriorProxyRequest& req = m_InteriorProxyRequests[index];
		preloadSuccess = req.HasLoaded();
		req.m_lastPreloadTime = currGameTime;
		return &m_InteriorProxyRequests[index];
	}


	// Don't currently have a request so ask to load and create a new request
	InteriorProxyRequest* pNewReq = NULL;
	for(int i = 0; i < m_InteriorProxyRequests.size(); ++i)
	{
		if(m_InteriorProxyRequests[i].IsFree())
		{
			pNewReq = &m_InteriorProxyRequests[i];
			break;
		}
	}

	replayAssert(pNewReq);
	if(pNewReq == NULL)
		return NULL;

	// Record the details.
	pNewReq->m_ProxyNameHash = inReq.m_ProxyNameHash;
	pNewReq->m_positionHash = inReq.m_positionHash;
	// Set the load time.
	pNewReq->m_lastPreloadTime = currGameTime;
	pNewReq->m_RoomRequestTimes.Reset();
	pNewReq->m_pInteriorProxy = NULL;
	pNewReq->m_stateRequest = inReq.m_stateRequest;

	// Obtain a pointer to the interior proxy.
	for(u32 i=0; i<CInteriorProxy::GetPool()->GetSize(); i++)
	{
		CInteriorProxy *pIntProxy = CInteriorProxy::GetPool()->GetSlot(i);

		if(pIntProxy && pIntProxy->GetNameHash() == inReq.m_ProxyNameHash && !pIntProxy->GetIsDisabled())
		{
			// Original return out at the first proxy...
			if(inReq.m_positionHash == 0)
			{
				pNewReq->m_pInteriorProxy = pIntProxy;
				break;
			}

			// New check allows us to search for the correct proxy with a given position.
			Vec3V pos;
			pIntProxy->GetPosition(pos);
			if(CDoorSystem::GenerateHash32(VEC3V_TO_VECTOR3(pos)) == inReq.m_positionHash)
			{
				pNewReq->m_pInteriorProxy = pIntProxy;

				break;
			}
		}
	}
	return pNewReq;
}

//=====================================================================================================

const atArray <CRoomRequest> &ReplayInteriorManager::GetRoomRequests()
{
	return m_RoomRequests;
}


void ReplayInteriorManager::ResetRoomRequests()
{
	// Reset list of per frame room requests.
	m_RoomRequests.ResetCount();
}


void ReplayInteriorManager::RecordRoomRequest(u32 proxyNameHash, u32 posHash, u32 roomIndex)
{
	//Check if we are already recording the request for this room
	for(int i= 0; i < m_RoomRequests.GetCount(); ++i)
	{
		if(m_RoomRequests[i].m_ProxyNameHash == proxyNameHash && 
		   m_RoomRequests[i].m_RoomIndex == roomIndex &&
		   m_RoomRequests[i].m_positionHash == posHash)
		{
			return;
		}
	}

	// Append to the list of room requests collected this frame.
	if(replayVerify(m_RoomRequests.GetCount() < m_RoomRequests.GetCapacity()))
	{
		CRoomRequest &newReq = m_RoomRequests.Append();
		newReq.m_ProxyNameHash = proxyNameHash;
		newReq.m_RoomIndex = roomIndex;
		newReq.m_positionHash = posHash;
	}
}


void ReplayInteriorManager::DoHacks(bool isRecordedMP)
{
	// HACK: If this clip contains the dt1_03_carpark then force it to disable and re-enable.
	// This fixes url:bugstar:2359070
	if(m_dt1_03_carparkHack && !isRecordedMP)
	{
		for(u32 i=0; i<CInteriorProxy::GetPool()->GetSize(); i++)
		{
			CInteriorProxy *pIntProxy = CInteriorProxy::GetPool()->GetSlot(i);
					 
			if(pIntProxy && pIntProxy->GetNameHash() == 0x240d2000/*"dt1_03_carpark"*/ && CInteriorProxy::GetPool()->GetJustIndex(pIntProxy) == 259 && pIntProxy->GetGroupId() == 20)
			{
				pIntProxy->SetIsCappedAtPartial(false);

				bool s = pIntProxy->GetIsDisabled();
				pIntProxy->SetIsDisabled(!s);
				pIntProxy->SetIsDisabled(s);

#if __BANK
				CPortalDebugHelper::UpdateMloList();
#endif // __BANK
				replayDebugf1("=== Replay Hack:  Forcing dt1_03_carpark to reboot! ===");

				m_dt1_03_carparkHack = false;
			}
		}
	}
}

//=====================================================================================================

void ReplayInteriorManager::RequestInteriorProxy(CInteriorProxy *pIntProxy, CInteriorProxy::eProxy_State stateRequest)
{
	if(pIntProxy)
		pIntProxy->SetRequestedState(CInteriorProxy::RM_CUTSCENE, stateRequest);
}


void ReplayInteriorManager::ReleaseInteriorProxy(CInteriorProxy *pIntProxy)
{
	if(pIntProxy)
		pIntProxy->SetRequestedState(CInteriorProxy::RM_CUTSCENE, CInteriorProxy::PS_NONE);
}


bool ReplayInteriorManager::HasInteriorProxyLoaded(CInteriorProxy *pIntProxy, CInteriorProxy::eProxy_State stateRequest)
{
	if(pIntProxy)
	{
		// Check the interiors proxy state has been match to the requested state.
		if(pIntProxy->GetCurrentState() >= stateRequest || pIntProxy->GetIsCappedAtPartial() || pIntProxy->GetIsDisabled())
		{
			// HACK: If this interior has loaded and it is this car park then
			// flag that we need to do a little 'extra curricular activity' on it. (see ReplayInteriorManager::DoHacks())
			if(m_dt1_03_carparkHackOnlyOnce)
			{
				if(pIntProxy->GetNameHash() == 0x240d2000/*"dt1_03_carpark"*/ && CInteriorProxy::GetPool()->GetJustIndex(pIntProxy) == 259 && pIntProxy->GetGroupId() == 20)
				{
					m_dt1_03_carparkHack = true;			// Flag to perform the hack
 					m_dt1_03_carparkHackOnlyOnce = false;	// Toggle so we can't do this again this clip
				}
			}

			return true;
		}
		return false;
	}	
	else
	{
		//Don't wait for a proxy that doesn't exist
		return true;
	}
}


#endif	//GTA_REPLAY
