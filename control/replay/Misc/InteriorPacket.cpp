#include "InteriorPacket.h"

#if GTA_REPLAY

#include "control/replay/ReplayInteriorManager.h"
#include "control/replay/ReplayIPLManager.h"
#include "camera/CamInterface.h"
#include "camera/viewports/ViewportManager.h"
#include "Objects/Door.h"
#include "Scene/portals/InteriorInst.h"
#include "scene/portals/PortalTracker.h"
#include "scene/world/GameWorld.h"
#include "Renderer/occlusion.h"
#include "Renderer/PostScan.h"

//////////////////////////////////////////////////////////////////////////
void CPacketInterior::Store(const atArray<ReplayInteriorData>& info)
{
	CPacketBase::Store(PACKETID_INTERIOR, sizeof(CPacketInterior));

	replayAssertf(info.size() < MAX_REPLAY_INTERIOR_SAVES, "CPacketInterior: Number of changes (%u) > MAX_REPLAY_INTERIOR_SAVES, increase MAX_REPLAY_INTERIOR_SAVES count", info.size());

	const ReplayInteriorData *pElements = info.GetElements();
	// Guard the count, we don't want to overwrite our array
	m_count = Min(info.size(), MAX_REPLAY_INTERIOR_SAVES);
	if( pElements )
	{
		memcpy(m_InteriorData, pElements, m_count * sizeof(ReplayInteriorData));
	}
}

//////////////////////////////////////////////////////////////////////////
void CPacketInterior::Setup() const
{
	ReplayInteriorManager::SubmitNewInteriorData((u8)m_count, m_InteriorData);

	ReplayInteriorManager::AddInteriorStatePacket(this);
}


//////////////////////////////////////////////////////////////////////////

typedef struct DeferredEntitySetUpdate
{
	u32 m_InteriorProxyID;
	atFixedArray <SEntitySet, MAX_ENTITY_SETS> m_EntitySetHashValues;
} DeferredEntitySetUpdate;
static atFixedArray < DeferredEntitySetUpdate, MAX_ENTITY_SET_UPDATES > ms_DeferredEntitySetUpdates; 

void CPacketInteriorEntitySet::Store(const u32 nameHash, Vec3V &position, const atArray<atHashString>& info)
{
	CPacketBase::Store(PACKETID_INTERIOR_ENTITY_SET, sizeof(CPacketInteriorEntitySet));

	m_InteriorProxyNameHash = nameHash;
	m_InteriorProxyPos.Store(position);
	u32	count = info.size();
	if( count )
	{
		m_activeEntitySets.Resize(count);
		for(int i=0; i<count; i++)
		{
			m_activeEntitySets[i] = info[i];
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPacketInteriorEntitySet::Extract(bool isJumping) const
{
	CInteriorProxy *pIntProxy = NULL;

	Vector3 position;
	m_InteriorProxyPos.Load(position);
	pIntProxy = ReplayInteriorManager::GetProxyAtCoords(position, m_InteriorProxyNameHash, false);

	if( pIntProxy )
	{
		if(!AreEntitySetsTheSame(pIntProxy, m_activeEntitySets))
		{
			DeferredEntitySetUpdate *pDeferredUpdate = NULL;
			u32	count = m_activeEntitySets.size();
			int existingIdx = isJumping ? FindDeferredEntry(pIntProxy->GetInteriorProxyPoolIdx()) : -1;

			// Try to re-use existing entry if we're jumping over packets.
			if(isJumping && (existingIdx != -1))
			{
				pDeferredUpdate = &ms_DeferredEntitySetUpdates[existingIdx];
			}
			else
			{
				if(replayVerifyf(!ms_DeferredEntitySetUpdates.IsFull(), "ms_DeferredEntitySetUpdates is full, can't add new entity set update - Capacity:%i", ms_DeferredEntitySetUpdates.max_size()))
				{
					pDeferredUpdate = &ms_DeferredEntitySetUpdates.Append();
				}
			}

			if(pDeferredUpdate != NULL)
			{
				pDeferredUpdate->m_InteriorProxyID = pIntProxy->GetInteriorProxyPoolIdx();
				pDeferredUpdate->m_EntitySetHashValues.Reset();

				for(int i=0; i<count; i++)
					pDeferredUpdate->m_EntitySetHashValues.Push(SEntitySet(atHashString(m_activeEntitySets[i])));
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
int CPacketInteriorEntitySet::FindDeferredEntry(u32 interiorProxyID) const
{
	for(int i=0; i<ms_DeferredEntitySetUpdates.GetCount(); i++)
	{
		if(ms_DeferredEntitySetUpdates[i].m_InteriorProxyID == interiorProxyID)
			return i;
	}
	return -1;
}


//////////////////////////////////////////////////////////////////////////
void CPacketInteriorEntitySet::ProcessDeferredEntitySetUpdates()
{
	for(int i=0; i<ms_DeferredEntitySetUpdates.GetCount(); i++)
	{
 		DeferredEntitySetUpdate &deferredUpdate = ms_DeferredEntitySetUpdates[i];
 
 		CInteriorProxy *pIntProxy = CInteriorProxy::GetPool()->GetSlot(deferredUpdate.m_InteriorProxyID);
 		atArray<SEntitySet>	&proxyEntitySets = pIntProxy->GetEntitySets();
 
 		proxyEntitySets.Reset();
 		proxyEntitySets.Reserve(MAX_ENTITY_SETS);
 
 		for(int j=0; j<deferredUpdate.m_EntitySetHashValues.GetCount(); j++)
 			proxyEntitySets.Push(deferredUpdate.m_EntitySetHashValues[j]);
 
 		// Refresh the interior, to update any active entity sets.
 		pIntProxy->RefreshInterior();
	}

	// Reset for next frame.
	ms_DeferredEntitySetUpdates.Reset();
}


//////////////////////////////////////////////////////////////////////////
bool CPacketInteriorEntitySet::AreEntitySetsTheSame(CInteriorProxy* pIntProxy, atFixedArray <u32, MAX_ENTITY_SETS_OLD> const &entitySetHashValues)
{
	bool	alreadySet = true;
 	atArray<SEntitySet>	&proxyEntitySets = pIntProxy->GetEntitySets();
 
 	// Are these the same info as already set?
 	if( proxyEntitySets.size() == entitySetHashValues.size() )
 	{
 		for( int i=0; i<proxyEntitySets.size(); i++ )
 		{
 			if( proxyEntitySets[i].m_name.GetHash() != entitySetHashValues[i] )
 			{
 				alreadySet = false;
 				break;
 			}
 		}
 	}
 	else
 	{
 		alreadySet = false;
 	}
	return alreadySet;
}

tPacketVersion CPacketInteriorEntitySet2_V1 = 1;
void CPacketInteriorEntitySet2::Store(const u32 nameHash, Vec3V &position, const atArray<SEntitySet>& info)
{
	CPacketBase::Store(PACKETID_INTERIOR_ENTITY_SET2, sizeof(CPacketInteriorEntitySet2), CPacketInteriorEntitySet2_V1);

	m_InteriorProxyNameHash = nameHash;
	m_InteriorProxyPos.Store(position);
	m_numActiveEntitySets = 0;
	u32	count = info.size();
	if( count )
	{
		PackedEntitySet* pEntitySet = GetActiveEntitySetPtrAsPackedEntitySet();
		for(int i=0; i<count; i++)
		{
			pEntitySet[i] = info[i];
			++m_numActiveEntitySets;
		}

		AddToPacketSize(m_numActiveEntitySets * sizeof(PackedEntitySet));
	}
}

//////////////////////////////////////////////////////////////////////////
void CPacketInteriorEntitySet2::Extract(bool isJumping) const
{
	CInteriorProxy *pIntProxy = NULL;

	Vector3 position;
	m_InteriorProxyPos.Load(position);
	pIntProxy = ReplayInteriorManager::GetProxyAtCoords(position, m_InteriorProxyNameHash, false);

	if( pIntProxy )
	{
		if(!AreEntitySetsTheSame(pIntProxy))
		{
			DeferredEntitySetUpdate *pDeferredUpdate = NULL;
			u32	count = m_numActiveEntitySets;
			int existingIdx = isJumping ? FindDeferredEntry(pIntProxy->GetInteriorProxyPoolIdx()) : -1;

			// Try to re-use existing entry if we're jumping over packets.
			if(isJumping && (existingIdx != -1))
			{
				pDeferredUpdate = &ms_DeferredEntitySetUpdates[existingIdx];
			}
			else
			{
				if(replayVerifyf(!ms_DeferredEntitySetUpdates.IsFull(), "ms_DeferredEntitySetUpdates is full, can't add new entity set update - Capacity:%i", ms_DeferredEntitySetUpdates.max_size()))
				{
					pDeferredUpdate = &ms_DeferredEntitySetUpdates.Append();
				}
			}

			if(pDeferredUpdate != NULL)
			{
				pDeferredUpdate->m_InteriorProxyID = pIntProxy->GetInteriorProxyPoolIdx();
				pDeferredUpdate->m_EntitySetHashValues.Reset();

				if(GetPacketVersion() >= CPacketInteriorEntitySet2_V1)
				{
					PackedEntitySet* pEntitySet = GetActiveEntitySetPtrAsPackedEntitySet();
					for(int i=0; i<count; i++)
						pDeferredUpdate->m_EntitySetHashValues.Push(pEntitySet[i]);
				}
				else
				{
					u32* pEntitySet = GetActiveEntitySetPtr();
					for(int i=0; i<count; i++)
						pDeferredUpdate->m_EntitySetHashValues.Push(SEntitySet(atHashString(pEntitySet[i])));
				}
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
int CPacketInteriorEntitySet2::FindDeferredEntry(u32 interiorProxyID) const
{
	for(int i=0; i<ms_DeferredEntitySetUpdates.GetCount(); i++)
	{
		if(ms_DeferredEntitySetUpdates[i].m_InteriorProxyID == interiorProxyID)
			return i;
	}
	return -1;
}


//////////////////////////////////////////////////////////////////////////
void CPacketInteriorEntitySet2::ProcessDeferredEntitySetUpdates()
{
	for(int i=0; i<ms_DeferredEntitySetUpdates.GetCount(); i++)
	{
 		DeferredEntitySetUpdate &deferredUpdate = ms_DeferredEntitySetUpdates[i];
 
 		CInteriorProxy *pIntProxy = CInteriorProxy::GetPool()->GetSlot(deferredUpdate.m_InteriorProxyID);
 		atArray<SEntitySet>	&proxyEntitySets = pIntProxy->GetEntitySets();
 
 		proxyEntitySets.Reset();
 		proxyEntitySets.Reserve(MAX_ENTITY_SETS);
 
 		for(int j=0; j<deferredUpdate.m_EntitySetHashValues.GetCount(); j++)
 			proxyEntitySets.Push(deferredUpdate.m_EntitySetHashValues[j]);
 
 		// Refresh the interior, to update any active entity sets.
 		pIntProxy->RefreshInterior();
	}

	// Reset for next frame.
	ms_DeferredEntitySetUpdates.Reset();
}


//////////////////////////////////////////////////////////////////////////
bool CPacketInteriorEntitySet2::AreEntitySetsTheSame(CInteriorProxy* pIntProxy) const
{
	bool	alreadySet = true;
 	atArray<SEntitySet>	&proxyEntitySets = pIntProxy->GetEntitySets();
 
 	// Are these the same info as already set?
	if(GetPacketVersion() >= CPacketInteriorEntitySet2_V1)
	{
		if( (u32)proxyEntitySets.size() == m_numActiveEntitySets )
		{
			PackedEntitySet* pEntitySet = GetActiveEntitySetPtrAsPackedEntitySet();
			for( int i=0; i<proxyEntitySets.size(); i++ )
			{
				if( proxyEntitySets[i] != pEntitySet[i] )
				{
					alreadySet = false;
					break;
				}
			}
		}
		else
		{
			alreadySet = false;
		}
	}
	else
	{
		if( (u32)proxyEntitySets.size() == m_numActiveEntitySets )
		{
			u32* pEntitySet = GetActiveEntitySetPtr();
			for( int i=0; i<proxyEntitySets.size(); i++ )
			{
				if( proxyEntitySets[i].m_name.GetHash() != pEntitySet[i] )
				{
					alreadySet = false;
					break;
				}
			}
		}
		else
		{
			alreadySet = false;
		}
	}
	return alreadySet;
}

//////////////////////////////////////////////////////////////////////////
tPacketVersion g_PacketInteriorProxyStatesVersion_v1 = 1;
tPacketVersion g_PacketInteriorProxyStatesVersion_v2 = 2;

void CPacketInteriorProxyStates::Store(const atArray <CRoomRequest> &roomRequests)
{
	PACKET_EXTENSION_RESET(CPacketInteriorProxyStates);
	CPacketBase::Store(PACKETID_INTERIOR_PROXY_STATE, sizeof(CPacketInteriorProxyStates), g_PacketInteriorProxyStatesVersion_v2);
	SetShouldPreload(true);

	m_NoOfInteriorProxies = 0;
	m_NoOfRoomRequests = 0;

	// Record active interior proxies.
	u32 *pDest = GetInteriorProxyIDs();

	for(u32 i=0; i<CInteriorProxy::GetPool()->GetSize(); i++)
	{
		CInteriorProxy *pIntProxy = CInteriorProxy::GetPool()->GetSlot(i);
		
		// This packets supports FULL and FULL_WITH_COLLISION as long as the containing imap is active
		if(pIntProxy && pIntProxy->GetCurrentState() >= CInteriorProxy::PS_FULL && pIntProxy->IsContainingImapActive())
		{
			*pDest++ = pIntProxy->GetNameHash();
			*pDest++ = pIntProxy->GetCurrentState();

			Vec3V pos;
			pIntProxy->GetPosition(pos);
			u32 hash = CDoorSystem::GenerateHash32(VEC3V_TO_VECTOR3(pos));
			*pDest++ = hash;
			m_NoOfInteriorProxies++;
		}
	}
	AddToPacketSize(sizeof(u32)*m_NoOfInteriorProxies*3);

	//-----------------------------------------------------------------------------------------------------//

	// Record room requests.
	pDest = GetRoomRequests();
	m_NoOfRoomRequests = roomRequests.GetCount();

	for(u32 i=0; i<roomRequests.GetCount(); i++)
	{
		*pDest++ = roomRequests[i].m_ProxyNameHash;
		*pDest++ = roomRequests[i].m_RoomIndex;
		*pDest++ = roomRequests[i].m_positionHash;
	}
	AddToPacketSize(sizeof(u32)*m_NoOfRoomRequests* 3);
}

u32* CPacketInteriorProxyStates::GetRoomRequests() const
{
	if(GetPacketVersion() >= g_PacketInteriorProxyStatesVersion_v2)
	{
		return (u32 *)(((u8 *)(this)) + GetPacketSize() - sizeof(u32)*m_NoOfRoomRequests*3);
	}

	return (u32 *)(((u8 *)(this)) + GetPacketSize() - sizeof(u32)*m_NoOfRoomRequests*2);
}

u32* CPacketInteriorProxyStates::GetInteriorProxyIDs() const
{
	if(GetPacketVersion() >= g_PacketInteriorProxyStatesVersion_v2)
	{
		return (u32 *)((u8 *)GetRoomRequests() - sizeof(u32)*m_NoOfInteriorProxies*3);
	}

	if(GetPacketVersion() >= g_PacketInteriorProxyStatesVersion_v1)
	{
		return (u32 *)((u8 *)GetRoomRequests() - sizeof(u32)*m_NoOfInteriorProxies*2);
	}

	return (u32 *)((u8 *)GetRoomRequests() - sizeof(u32)*m_NoOfInteriorProxies);
}

bool CPacketInteriorProxyStates::AddPreloadRequests(class ReplayInteriorManager &interiorManager, u32 currGameTime PRELOADDEBUG_ONLY(, atString& failureReason)) const
{
	bool ret = true;

	if(!ReplayIPLManager::IsInitialSetupComplete())
	{
		PRELOADDEBUG_ONLY(failureReason = "Initial setup not complete";)
		return false;
	}
	
	u32 *pSrc = GetInteriorProxyIDs();

	// Add proxy requests.
	for(u32 i=0; i<m_NoOfInteriorProxies; i++)
	{
		u32 interiorHash = *pSrc; pSrc++;
		CInteriorProxy::eProxy_State requestState = CInteriorProxy::PS_FULL_WITH_COLLISIONS;
		if(GetPacketVersion() >= g_PacketInteriorProxyStatesVersion_v1)
		{
			requestState = (CInteriorProxy::eProxy_State)*pSrc; pSrc++;
		}

		u32 positionHash = 0;
		if(GetPacketVersion() >= g_PacketInteriorProxyStatesVersion_v2)
		{
			positionHash = *pSrc; pSrc++;
		}

		ret &= interiorManager.AddInteriorProxyPreloadRequest(interiorHash, positionHash, requestState, currGameTime);
#if PRELOADDEBUG
		if(!ret && failureReason.GetLength() == 0)
		{
			failureReason = "Failed in AddInteriorProxyPreloadRequest";
		}
#endif // PRELOADDEBUG	
	}

	u32 *pRoomSrc = GetRoomRequests();

	// Add room requests.
	for(u32 i=0; i<m_NoOfRoomRequests; i++)
	{
		u32 posHash = 0;
		if(GetPacketVersion() >= g_PacketInteriorProxyStatesVersion_v2)
		{
			posHash = pRoomSrc[(i << 1) + 2];
		}

		ret &= interiorManager.AddRoomRequest(pRoomSrc[i << 1], posHash, pRoomSrc[(i << 1) + 1], currGameTime);
#if PRELOADDEBUG
		if(!ret && failureReason.GetLength() == 0)
		{
			failureReason = "Failed in AddRoomRequest";
		}
#endif // PRELOADDEBUG	
	}
	
	return ret;
}

//////////////////////////////////////////////////////////////////////////
//Will be incremented by the constructor
int CPacketForceRoomForGameViewport::sm_ExpirePrevious = -1;
CPacketForceRoomForGameViewport::CPacketForceRoomForGameViewport(CInteriorProxy* pInteriorProxy, int roomKey)
	: CPacketEvent(PACKETID_FORCE_ROOM_FOR_GAMEVIEWPORT, sizeof(CPacketForceRoomForGameViewport))
	, CPacketEntityInfo()
{
	replayFatalAssertf(pInteriorProxy, "CPacketForceRoomForGameViewport requires valid pInteriorProxy");
	Vec3V position;
	pInteriorProxy->GetPosition(position);
	m_InteriorProxyPos.Store(position);

	m_InteriorProxyNameHash = pInteriorProxy->GetNameHash();	
	m_RoomKey = roomKey;

	++sm_ExpirePrevious;
}

//////////////////////////////////////////////////////////////////////////
void CPacketForceRoomForGameViewport::Extract(const CEventInfo<void>&) const
{
	CInteriorProxy *pInteriorProxy = NULL;

	Vector3 position;
	m_InteriorProxyPos.Load(position);
	pInteriorProxy = ReplayInteriorManager::GetProxyAtCoords(position, m_InteriorProxyNameHash);

	if(pInteriorProxy)
	{
		CInteriorInst* pInterior = pInteriorProxy->GetInteriorInst();
		if (pInterior)
		{
			const s32					roomIdx = pInterior->FindRoomByHashcode( m_RoomKey );
			const fwInteriorLocation	intLoc = CInteriorInst::CreateLocation( pInterior, roomIdx );

			CViewport* gameViewport = gVpMan.GetGameViewport();
			if(gameViewport)
			{
				//Get the portal tracker from the viewport and make sure it is in an interior.
				int count = RENDERPHASEMGR.GetRenderPhaseCount();
				for(s32 i=0; i<count; i++)
				{
					CRenderPhase &renderPhase = (CRenderPhase &) RENDERPHASEMGR.GetRenderPhase(i);

					if (renderPhase.IsScanRenderPhase())
					{
						CRenderPhaseScanned &scanPhase = (CRenderPhaseScanned &) renderPhase;

						if (scanPhase.IsUpdatePortalVisTrackerPhase())
						{
							CPortalTracker* portalTracker = scanPhase.GetPortalVisTracker();
							if(portalTracker)
							{
								Vector3 viewportPosition = VEC3V_TO_VECTOR3(gameViewport->GetGrcViewport().GetCameraPosition());

								Assert( portalTracker->m_pOwner == NULL );
								portalTracker->MoveToNewLocation( intLoc );
								portalTracker->EnableProbeFallback( intLoc );
								portalTracker->Teleport();
								portalTracker->Update(viewportPosition);
								CInteriorInst* pIntInst = portalTracker->GetInteriorInst();
								if(pIntInst) {
									pIntInst->ForceFadeIn();
								}
							}
						}
					}
				}
			}
		}
	}
}

bool CPacketForceRoomForGameViewport::HasExpired(const CEventInfo<void>&) const
{
	bool ret = false;
	if(sm_ExpirePrevious > 0)
	{
		// Only allow one instance of this packet
		ret = true;
	}
	else if(camInterface::GetCachedCameraAttachParent() != NULL)
	{
		// Stop recording the packet if the camera has attached to a parent again
		ret = true;
	}
	else 
	{
		// Stop recording the packet if the interior proxy has changed
		CInteriorProxy *pInteriorProxy = NULL;

		Vector3 position;
		m_InteriorProxyPos.Load(position);
		pInteriorProxy = ReplayInteriorManager::GetProxyAtCoords(position, m_InteriorProxyNameHash);

		if(pInteriorProxy && pInteriorProxy != CPortalVisTracker::GetPrimaryInteriorProxy())
		{
			ret = true;
		}
	}
	
	if(ret) 
	{
		--sm_ExpirePrevious;
	}

	return ret;
}


//////////////////////////////////////////////////////////////////////////

CPacketForceRoomForEntity_OLD::CPacketForceRoomForEntity_OLD(CInteriorProxy* pInteriorProxy, int roomKey)
	: CPacketEvent(PACKETID_FORCE_ROOM_FOR_ENTITY_OLD, sizeof(CPacketForceRoomForEntity_OLD))
	, CPacketEntityInfo()
{
	replayFatalAssertf(pInteriorProxy, "CPacketForceRoomForGameViewport requires valid pInteriorProxy");
	Vec3V position;
	pInteriorProxy->GetPosition(position);
	m_InteriorProxyPos.Store(position);
	m_InteriorProxyNameHash = pInteriorProxy->GetNameHash();	
	m_RoomKey = roomKey;
}

bool CPacketForceRoomForEntity_OLD::HasExpired(const CEventInfo<CEntity>& eventInfo) const
{
	CEntity	*pEntity = eventInfo.GetEntity();

	bool expired = true;
	if( pEntity )
	{
		replayAssertf(pEntity->GetIsPhysical(), "CPacketForceRoomForEntity_OLD::HasExpired() - Entity %s isn't Physical!!", pEntity->GetModelName());

		CPhysical* pPhysicalEnt = static_cast<CPhysical*>(pEntity);
		if( pPhysicalEnt->GetPortalTracker() )
		{
			if( pPhysicalEnt->GetPortalTracker()->IsProbeFallBackEnabled() )
			{
				expired = false;
			}
		}
	}

	return expired;
}

void CPacketForceRoomForEntity_OLD::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	CInteriorProxy *pInteriorProxy = NULL;

	Vector3 position;
	m_InteriorProxyPos.Load(position);
	pInteriorProxy = ReplayInteriorManager::GetProxyAtCoords(position, m_InteriorProxyNameHash);

	if(pInteriorProxy)
	{
		CInteriorInst* pInterior = pInteriorProxy->GetInteriorInst();
		if (pInterior)
		{
			CEntity	*pEntity = eventInfo.GetEntity();
			if( pEntity )
			{
				replayAssertf(pEntity->GetIsPhysical(), "CPacketForceRoomForEntity_OLD::Extract() - Entity %s isn't Physical!!", pEntity->GetModelName());
				CPhysical* pPhysicalEnt = static_cast<CPhysical*>(pEntity);

				const s32					roomIdx = pInterior->FindRoomByHashcode( m_RoomKey );
				const fwInteriorLocation	intLoc = CInteriorInst::CreateLocation( pInterior, roomIdx );

				CGameWorld::Remove( pPhysicalEnt, true );
				pPhysicalEnt->GetPortalTracker()->Teleport();
				CGameWorld::Add( pPhysicalEnt, intLoc, true );
				replayAssertf( pPhysicalEnt->GetInteriorLocation().IsSameLocation( intLoc ), "CPacketForceRoomForEntity_OLD::Extract() - Entity still isn't in the correct room");
				pPhysicalEnt->GetPortalTracker()->EnableProbeFallback( intLoc );
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

CPacketForceRoomForEntity::CPacketForceRoomForEntity(CInteriorProxy* pInteriorProxy, int roomKey)
	: CPacketEventTracked(PACKETID_FORCE_ROOM_FOR_ENTITY, sizeof(CPacketForceRoomForEntity))
	, CPacketEntityInfoStaticAware()
{
	replayFatalAssertf(pInteriorProxy, "CPacketForceRoomForEntity requires valid pInteriorProxy");
	Vec3V position;
	pInteriorProxy->GetPosition(position);
	m_InteriorProxyPos.Store(position);
	m_InteriorProxyNameHash = pInteriorProxy->GetNameHash();	
	m_RoomKey = roomKey;
}

bool CPacketForceRoomForEntity::HasExpired(const CTrackedEventInfo<int>& eventInfo) const
{
	CEntity	*pEntity = eventInfo.pEntity[0];

	bool expired = true;
	if( pEntity )
	{
		replayAssertf(pEntity->GetIsPhysical(), "CPacketForceRoomForEntity::HasExpired() - Entity %s isn't Physical!!", pEntity->GetModelName());

		CPhysical* pPhysicalEnt = static_cast<CPhysical*>(pEntity);
		if( pPhysicalEnt->GetPortalTracker() )
		{
			if( pPhysicalEnt->GetPortalTracker()->IsProbeFallBackEnabled() )
			{
				expired = false;
			}
		}
	}

	return expired;
}

void CPacketForceRoomForEntity::Extract(CTrackedEventInfo<int>& eventInfo) const
{
	CInteriorProxy *pInteriorProxy = NULL;

	Vector3 position;
	m_InteriorProxyPos.Load(position);
	pInteriorProxy = ReplayInteriorManager::GetProxyAtCoords(position, m_InteriorProxyNameHash);

	if(pInteriorProxy)
	{
		CInteriorInst* pInterior = pInteriorProxy->GetInteriorInst();
		if (pInterior && pInterior->CanReceiveObjects())
		{
			CEntity	*pEntity = eventInfo.pEntity[0];
			if( pEntity )
			{
				replayAssertf(pEntity->GetIsPhysical(), "CPacketForceRoomForEntity::Extract() - Entity %s isn't Physical!!", pEntity->GetModelName());
				CPhysical* pPhysicalEnt = static_cast<CPhysical*>(pEntity);

				const s32					roomIdx = pInterior->FindRoomByHashcode( m_RoomKey );
				const fwInteriorLocation	intLoc = CInteriorInst::CreateLocation( pInterior, roomIdx );

				CGameWorld::Remove( pPhysicalEnt, true );
				pPhysicalEnt->GetPortalTracker()->Teleport();
				CGameWorld::Add( pPhysicalEnt, intLoc, true );
				replayAssertf( pPhysicalEnt->GetInteriorLocation().IsSameLocation( intLoc ), "CPacketForceRoomForEntity::Extract() - Entity still isn't in the correct room");
				pPhysicalEnt->GetPortalTracker()->EnableProbeFallback( intLoc );
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
CPacketModelCull::CPacketModelCull(u32 modelHash)
	: CPacketEvent(PACKETID_MODEL_CULL, sizeof(CPacketModelCull))
	, CPacketEntityInfo()
{
	m_ModelHash = modelHash;
}

//////////////////////////////////////////////////////////////////////////
void CPacketModelCull::Extract(const CEventInfo<void>&) const
{
	//when jumping we don't want to add the modelHash more than once, otherwise it might blow the pool
	if(!g_PostScanCull.IsModelCulledThisFrame(m_ModelHash))
	{
		g_PostScanCull.EnableModelCullThisFrame(m_ModelHash);
	}
}

//////////////////////////////////////////////////////////////////////////
CPacketDisableOcclusion::CPacketDisableOcclusion()
	: CPacketEvent(PACKETID_DISABLE_OCCLUSION, sizeof(CPacketDisableOcclusion))
	, CPacketEntityInfo()
{
}

//////////////////////////////////////////////////////////////////////////
void CPacketDisableOcclusion::Extract(const CEventInfo<void>&) const
{
	COcclusion::ScriptDisableOcclusionThisFrame();
}

#endif	//GTA_REPLAY
