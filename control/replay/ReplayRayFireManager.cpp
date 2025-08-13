#include "ReplayRayFireManager.h"

#if GTA_REPLAY

#include "vfx/ptfx/ptfxmanager.h"

// DON'T COMMIT
//OPTIMISATIONS_OFF()

////////////////////////////////////////////////////////////////////////////////////////
//	PreLoad Data Holder
////////////////////////////////////////////////////////////////////////////////////////
void RayFireReplayLoadingData::Load(const PacketRayFirePreLoadData &info)
{
	if( m_ModelInfoHash == info.m_ModelHash )
	{
		int index = m_RefsByUID.Find(info.m_UniqueID);
		if( index < 0 )
		{
			m_RefsByUID.Append() = info.m_UniqueID;
		}
	}
	else
	{
		// It's not already being loaded
		m_RefsByUID.Append() = info.m_UniqueID;
		m_ModelInfoHash = info.m_ModelHash;
		m_bUseImapForStartAndEnd = info.m_bUseImapForStartAndEnd;

		if (info.m_bUseImapForStartAndEnd)
		{
			fwImapGroupMgr& imapGroupMgr = INSTANCE_STORE.GetGroupSwapper();

			// Imap swaps
			u32 startImapHash = info.m_startIMapOrModelHash;
			ASSERT_ONLY(bool bSuccess = )imapGroupMgr.CreateNewSwapPersistent(atHashString((u32) 0), atHashString(startImapHash), m_imapSwapIndex); 
			Assert(bSuccess);

			u32 endImapHash = info.m_endIMapOrModelHash;
			ASSERT_ONLY(bSuccess = )imapGroupMgr.CreateNewSwapPersistent(atHashString((u32) 0), atHashString(endImapHash), m_imapSwapEndIndex); 
			Assert(bSuccess);
		}
		else
		{
			// Model swaps
			fwModelId startModelId;
			CModelInfo::GetBaseModelInfoFromHashKey(info.m_startIMapOrModelHash, &startModelId);
			if (Verifyf(startModelId.IsValid(), "CCompEntity::IssueAssetRequests - couldn't find start model for %s", CModelInfo::GetBaseModelInfoName(startModelId)))
			{
				strLocalIndex transientLocalIdx = strLocalIndex(CModelInfo::AssignLocalIndexToModelInfo(startModelId));
				m_startRequest.Request(transientLocalIdx, CModelInfo::GetStreamingModuleId(), STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD|STRFLAG_DONTDELETE);
			}

			fwModelId endModelId;
			CModelInfo::GetBaseModelInfoFromHashKey(info.m_endIMapOrModelHash, &endModelId);
			if (Verifyf(endModelId.IsValid(), "CCompEntity::IssueAssetRequests - couldn't find end model for %s", CModelInfo::GetBaseModelInfoName(endModelId)))
			{
				strLocalIndex transientLocalIdx = strLocalIndex(CModelInfo::AssignLocalIndexToModelInfo(endModelId));
				m_endRequest.Request(transientLocalIdx, CModelInfo::GetStreamingModuleId(), STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD|STRFLAG_DONTDELETE);
			}
		}

		// setup streaming requests for all the anim models & animation data
		for(u32 i=0; i<info.m_animHashes.size(); i++)
		{
			fwModelId animModelId;
			CModelInfo::GetBaseModelInfoFromHashKey(info.m_animModelHashes[i], &animModelId);
			if (Verifyf(animModelId.IsValid(), "CCompEntity::IssueAssetRequests - couldn't find anim model for %s", CModelInfo::GetBaseModelInfoName(animModelId)))
			{
				s32 transientLocalIdx = CModelInfo::AssignLocalIndexToModelInfo(animModelId).Get();
				m_animRequests.PushRequest(transientLocalIdx, CModelInfo::GetStreamingModuleId(), STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD|STRFLAG_DONTDELETE);
			}

			s32 animSlotIndex = fwAnimManager::FindSlotFromHashKey(info.m_animHashes[i]).Get();
			if (Verifyf(CModelInfo::IsValidModelInfo(animSlotIndex), "CCompEntity::IssueAssetRequests - couldn't find animation"))
			{
				m_animRequests.PushRequest(animSlotIndex, fwAnimManager::GetStreamingModuleId(), STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD|STRFLAG_DONTDELETE);
			}
		}

		// Particles
		strLocalIndex ptfxSlotId = strLocalIndex(-1);
		if (info.m_ptfxAssetNameHash != 0)
		{
			ptfxSlotId = ptfxManager::GetAssetStore().FindSlotFromHashKey(info.m_ptfxAssetNameHash);
			if (Verifyf(ptfxSlotId.Get()>-1, "cannot find particle asset"))
			{
				m_ptfxRequest.Request(ptfxSlotId, ptfxManager::GetAssetStore().GetStreamingModuleId(), STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD|STRFLAG_DONTDELETE);
			}
		}
	}
}

bool RayFireReplayLoadingData::HasLoaded()
{
	if (m_bUseImapForStartAndEnd)
	{
		fwImapGroupMgr& imapGroupMgr = INSTANCE_STORE.GetGroupSwapper();

		if( imapGroupMgr.GetIsLoaded(m_imapSwapIndex) == false )
		{
			return false;
		}

		if( imapGroupMgr.GetIsLoaded(m_imapSwapEndIndex) == false )
		{
			return false;
		}

	}
	else
	{
		// Start
		if (!m_startRequest.HasLoaded())
		{
			return false;
		}
		//End
		if (!m_endRequest.HasLoaded())
		{
			return false;
		}
	}

	// Anims
	if ( m_animRequests.GetRequestCount() > 0 && !m_animRequests.HaveAllLoaded())
	{
		return false;
	}
	// Particles
	if ((m_ptfxRequest.IsValid()) && !m_ptfxRequest.HasLoaded())
	{
		return false;
	}

	return true;
}

void RayFireReplayLoadingData::Release(u32 uID)
{
	int index = m_RefsByUID.Find(uID);
	if( index != -1 )		// Depending on the recording packets, we may get a release packet before we actually loaded anything.
	{
		m_RefsByUID.DeleteFast(index);
		if( m_RefsByUID.size() == 0 )
		{
			// Remove it all
			if (m_bUseImapForStartAndEnd)
			{
				// abandon and complete appropriate swaps
				fwImapGroupMgr& imapGroupMgr = INSTANCE_STORE.GetGroupSwapper();

				// Release Imap swaps
				// We need to know which state is active, start or end
				bool isStart = false;
				// Check if the imap is *already* set
				s32 index = imapGroupMgr.GetFinalImapIndex(m_imapSwapIndex);	// This is the final imap of the "start" state, since we swap from nothing -> imap.
				INSTANCE_DEF* pDef = INSTANCE_STORE.GetSlot(strLocalIndex(index));
				if (pDef && pDef->IsLoaded())
				{
					fwStreamedSceneGraphNode* pNode = pDef->m_pObject->GetSceneGraphNode();
					if (pNode && pNode->GetIsInWorld())
					{
						isStart = true;
					}
				}

				// If we're in the start state, we want to complete that swap and abandon the end one
				if( isStart )
				{
					// Remove end imap
					if( fwImapGroupMgr::IsValidIndex(m_imapSwapEndIndex) && imapGroupMgr.GetIsActive(m_imapSwapEndIndex) )
					{
						imapGroupMgr.SetIsPersistent(m_imapSwapEndIndex, false);
						imapGroupMgr.AbandonSwap(m_imapSwapEndIndex);
					}
					// Keep start imap
					if( fwImapGroupMgr::IsValidIndex(m_imapSwapIndex) && imapGroupMgr.GetIsActive(m_imapSwapIndex) )
					{
						imapGroupMgr.SetIsPersistent(m_imapSwapIndex, false);
						imapGroupMgr.CompleteSwap(m_imapSwapIndex);
					}
				}
				else
				{
					// Other way around for end state
					// Remove start imap
					if( fwImapGroupMgr::IsValidIndex(m_imapSwapIndex) && imapGroupMgr.GetIsActive(m_imapSwapIndex) )
					{
						imapGroupMgr.SetIsPersistent(m_imapSwapIndex, false);
						imapGroupMgr.AbandonSwap(m_imapSwapIndex);
					}
					// Keep end imap
					if( fwImapGroupMgr::IsValidIndex(m_imapSwapEndIndex) && imapGroupMgr.GetIsActive(m_imapSwapEndIndex) )
					{
						imapGroupMgr.SetIsPersistent(m_imapSwapEndIndex, false);
						imapGroupMgr.CompleteSwap(m_imapSwapEndIndex);
					}
				}

				m_imapSwapIndex = fwImapGroupMgr::INVALID_INDEX;
				m_imapSwapEndIndex = fwImapGroupMgr::INVALID_INDEX;
			}
			else
			{
				m_startRequest.ClearRequiredFlags(STRFLAG_DONTDELETE);
				m_endRequest.ClearRequiredFlags(STRFLAG_DONTDELETE);
			}

			// Anims
			m_animRequests.ClearRequiredFlags(STRFLAG_DONTDELETE);
			// Particles
			m_ptfxRequest.ClearRequiredFlags(STRFLAG_DONTDELETE);

			m_startRequest.Release(); 
			m_endRequest.Release(); 
			m_animRequests.ReleaseAll(); 
			m_ptfxRequest.Release();

			m_ModelInfoHash = 0;
		}
	}
}

bool RayFireReplayLoadingData::ContainsDataFor(const PacketRayFirePreLoadData &info)
{
	if( m_ModelInfoHash == info.m_ModelHash )
	{
		int index = m_RefsByUID.Find(info.m_UniqueID);
		if( index >= 0 )
		{
			return true;
		}
	}
	return false;
}

void RayFireReplayLoadingData::ReleaseAll()
{
	for(int i=m_RefsByUID.size()-1;i>=0;i--)
	{
		Release(m_RefsByUID[i]);
	}
}


////////////////////////////////////////////////////////////////////////////////////////
//	Replay Ray Fire Manager
////////////////////////////////////////////////////////////////////////////////////////
atArray<PacketRayFireInfo>					ReplayRayFireManager::m_PacketData;
atArray<PacketRayFireReplayHandlingData>	ReplayRayFireManager::m_RayFireData;
RayFireReplayLoadingData					ReplayRayFireManager::m_LoadingData[MAX_RAYFIRES_PRELOAD_COUNT];

bool																		ReplayRayFireManager::m_DefferedPreLoadReset = false;
atFixedArray<PacketRayFirePreLoadData, MAX_RAYFIRES_PRELOAD_REQUEST_COUNT>	ReplayRayFireManager::m_DeferredLoads;

void ReplayRayFireManager::OnEntry()
{
	ResetPacketData();
}

void ReplayRayFireManager::OnExit()
{
	ResetPacketData();
	ResetPreLoadData();
}

void ReplayRayFireManager::Process()
{
	// Go through the packets finding any that may be already registered
	for(int i=0;i<m_PacketData.size();i++)
	{
		bool	found = false;
		PacketRayFireInfo &thisPacket = m_PacketData[i];
		for(int j=0;j<m_RayFireData.size();j++)
		{
			PacketRayFireReplayHandlingData &otherPacket = m_RayFireData[j];
			if( otherPacket == thisPacket )
			{
				found = true;
				otherPacket.m_Info = thisPacket;
			}
		}

		// If we didn't find it, add it
		if( !found )
		{
			m_RayFireData.Grow().m_Info = thisPacket;
		}
	}
	m_PacketData.Reset();

	for(int i=m_RayFireData.size()-1;i>=0;i--)
	{
		PacketRayFireReplayHandlingData &thisPacket = m_RayFireData[i];

		CCompEntity *pCompEntity = FindCompEntity(thisPacket.m_Info);

		if( pCompEntity == NULL )
		{
			// Only delete data if we know we've loaded
			// pCompentity could be NULL until the map gets a chance to load.
			if( !CReplayMgr::IsSettingUpFirstFrame() )
			{
				// The CompEntity is gone... remove this packet
				m_RayFireData.Delete(i);
			}
		}
		else
		{
			if( pCompEntity->GetDidPreLoadComplete() )
			{
				eCompEntityState currState = pCompEntity->GetState();

				switch(thisPacket.m_Info.m_State)
				{
				case PacketRayFireInfo::STATE_START:

					if( currState != CE_STATE_START )
					{
						pCompEntity->SetToStarting();
					}
					break;

				case PacketRayFireInfo::STATE_END:

					if( currState != CE_STATE_END )
					{
						pCompEntity->SetToEnding();
					}
					break;

				case PacketRayFireInfo::STATE_ANIMATING:
					if( currState != CE_STATE_ANIMATING && currState != CE_STATE_PAUSED )
					{
						pCompEntity->TriggerAnim();
					}
					pCompEntity->SetAnimTo(thisPacket.m_Info.m_Phase);
					break;
				}
			}
		}
	}
}

void ReplayRayFireManager::SubmitPacket(const PacketRayFireInfo &packet)
{
	// Find any packets that are the same modelhash and position and update
	int foundIDX = -1;
	for(int i=0;i<m_PacketData.size();i++)
	{
		PacketRayFireInfo &storedPacket = m_PacketData[i];
		if( storedPacket.m_ModelHash == packet.m_ModelHash )
		{
			if( storedPacket.m_ModelPos[0] == packet.m_ModelPos[0] && storedPacket.m_ModelPos[1] == packet.m_ModelPos[1] && storedPacket.m_ModelPos[2] == packet.m_ModelPos[2] )
			{
				foundIDX = i;
				break;
			}
		}
	}

	if( foundIDX != -1)
	{
		// Update existing
		PacketRayFireInfo &storedPacket = m_PacketData[foundIDX];
		storedPacket.m_Phase = packet.m_Phase;
		storedPacket.m_State = packet.m_State;
	}
	else
	{
		// Add a new one
		m_PacketData.PushAndGrow(packet);
	}
}

void ReplayRayFireManager::ResetPacketData()
{
	m_PacketData.Reset();
	m_RayFireData.Reset();
}

void ReplayRayFireManager::ResetPreLoadData()
{
	m_DefferedPreLoadReset = true;
}

CCompEntity	*ReplayRayFireManager::FindCompEntity(PacketRayFireInfo &packet)
{
	const float Radius = 1.0f;
	u32 poolSize = CCompEntity::GetPool()->GetSize();
	ScalarV vRadiusSquared = ScalarVFromF32(Radius*Radius);

	for(u32 i=0; i< poolSize; i++)
	{
		CCompEntity* pCompEntity = CCompEntity::GetPool()->GetSlot(i); 

		if (pCompEntity)
		{
			CBaseModelInfo* pModelInfo = pCompEntity->GetBaseModelInfo();
			if (pModelInfo && (pModelInfo->GetHashKey() == packet.m_ModelHash))
			{
				Vec3V modelPos;
				packet.m_ModelPos.Load(modelPos);

				if (IsLessThanAll(DistSquared(pCompEntity->GetTransform().GetPosition(), modelPos), vRadiusSquared))
				{
					return pCompEntity;	
				}

			}
		}
	}
	return NULL;
}

int ReplayRayFireManager::Load(const PacketRayFirePreLoadData &info)
{
	// Find an unused entry
	for(int i=0;i<MAX_RAYFIRES_PRELOAD_COUNT;i++)
	{
		RayFireReplayLoadingData *pData = &m_LoadingData[i];
		if( pData->m_ModelInfoHash == info.m_ModelHash )
		{
			// It's already being loaded
			pData->Load(info);
			return i;
		}
	}

	// We couldn't find one, find an empty one
	for(int i=0;i<MAX_RAYFIRES_PRELOAD_COUNT;i++)
	{
		RayFireReplayLoadingData *pData = &m_LoadingData[i];
		if( pData->m_RefsByUID.size() == 0 )
		{
			// Empty
			pData->Load(info);
			return i;
		}
	}

	Assertf(0, "ReplayRayFireManager::Load() - Unable to find free loading slot");
	return -1;
}

bool ReplayRayFireManager::HasLoaded( int loadIndex )
{
	if( loadIndex >=0 && loadIndex < MAX_RAYFIRES_PRELOAD_COUNT )
	{
		RayFireReplayLoadingData *pData = &m_LoadingData[loadIndex];
		return pData->HasLoaded();
	}

	return false;
}


void ReplayRayFireManager::Release(int loadIndex, u32 uID)
{
	// The uid may be -1 if the rayfire never actually ran (out of visible range, see CCompentity::Update()), and was never linked up
	// In this case either the pre-load "unload" packet will be acted on, or the clip will end and that data will be unloaded then.
	if( loadIndex >=0 && loadIndex < MAX_RAYFIRES_PRELOAD_COUNT)
	{
		RayFireReplayLoadingData *pData = &m_LoadingData[loadIndex];
		pData->Release(uID);
	}
}

int ReplayRayFireManager::FindLoadIndex(const PacketRayFirePreLoadData &info)
{
	for(int i=0;i<MAX_RAYFIRES_PRELOAD_COUNT;i++)
	{
		RayFireReplayLoadingData *pData = &m_LoadingData[i];
		if( pData->ContainsDataFor(info) )
		{
			return i;
		}
	}
	return -1;
}

void	ReplayRayFireManager::AddDeferredLoad(const PacketRayFirePreLoadData &info)
{
	if( Verifyf(m_DeferredLoads.IsFull() == false, "ReplayRayFireManager::AddDeferredLoad() - m_DeferredLoads array is full, continue to get the array contents in TTY"))
	{
		m_DeferredLoads.Append() = info;
	}
#if !__FINAL
	else
	{
		// It's full, dump out the contents of the array
		Displayf("m_DeferredLoads array FULL, contents follows:-");
		for( int i=0; i<m_DeferredLoads.size(); i++ )
		{
			Displayf("%d - ModelHash = 0x%x - UiD = 0x%x", i, m_DeferredLoads[i].m_ModelHash, m_DeferredLoads[i].m_UniqueID );
		}
	}
#endif

}

void	ReplayRayFireManager::ProcessDeferred()
{
	// Handle any deferred loads
	while (m_DeferredLoads.size())
	{
		Load(m_DeferredLoads[0]);
		m_DeferredLoads.Delete(0);
	}

	// And cleanups
	if( m_DefferedPreLoadReset )
	{
		for(int i=0;i<MAX_RAYFIRES_PRELOAD_COUNT;i++)
		{
			RayFireReplayLoadingData *pData = GetLoadingData(i);
			if( pData->m_ModelInfoHash != 0 )
			{
				pData->ReleaseAll();
			}
		}

		m_DefferedPreLoadReset = false;
	}
}


#endif	//GTA_REPLAY