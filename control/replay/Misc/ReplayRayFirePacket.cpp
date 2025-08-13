#include "ReplayRayFirePacket.h"

#if GTA_REPLAY

#include "control/replay/ReplayRayFireManager.h"

//OPTIMISATIONS_OFF()


//////////////////////////////////////////////////////////////////////////
// CPacketRayFire
//////////////////////////////////////////////////////////////////////////
CPacketRayFireStatic::CPacketRayFireStatic(u32 modelHash, Vector3 &pos, u32 state, float phase)
	: CPacketEvent(PACKETID_RAYFIRE_STATIC, sizeof(CPacketRayFireStatic))
{
	m_Info.m_ModelHash = modelHash;
	m_Info.m_ModelPos.Store(pos);
	m_Info.m_State = state;
	m_Info.m_Phase = phase;
}

//////////////////////////////////////////////////////////////////////////
void CPacketRayFireStatic::Extract(const CEventInfo<void>&) const
{
	ReplayRayFireManager::SubmitPacket(m_Info);
}


//////////////////////////////////////////////////////////////////////////
CPacketRayFireUpdating::CPacketRayFireUpdating(u32 modelHash, Vector3 &pos, u32 state, float phase, bool stopTracking)
	: CPacketEventInterp(PACKETID_RAYFIRE_UPDATING, sizeof(CPacketRayFireUpdating))
{
	m_Info.m_ModelHash = modelHash;
	m_Info.m_ModelPos.Store(pos);
	m_Info.m_State = state;
	m_Info.m_Phase = phase;

	m_stopTracking = stopTracking;
}


//////////////////////////////////////////////////////////////////////////
void CPacketRayFireUpdating::Extract(const CInterpEventInfo<CPacketRayFireUpdating, void>& info) const
{
	PacketRayFireInfo rayFireInfo = m_Info;
	const CPacketRayFireUpdating* pNextPacket = info.GetNextPacket();
	if(pNextPacket && (pNextPacket->m_Info.m_Phase > m_Info.m_Phase))
	{
		rayFireInfo.m_Phase = (1.0f-info.GetInterp()) * m_Info.m_Phase + info.GetInterp() * pNextPacket->m_Info.m_Phase;
	}
	
	ReplayRayFireManager::SubmitPacket(rayFireInfo);
}


//////////////////////////////////////////////////////////////////////////
// CPacketRayFirePreLoad
//////////////////////////////////////////////////////////////////////////

CPacketRayFirePreLoad::CPacketRayFirePreLoad(bool loading, u32 uid, CCompEntityModelInfo *pCompEntityModelInfo)
	: CPacketEvent(PACKETID_RAYFIRE_PRE_LOAD, sizeof(CPacketRayFirePreLoad))
{
	m_Info.m_Load = loading;
	m_Info.m_ModelHash = pCompEntityModelInfo->GetModelNameHash();
	m_Info.m_UniqueID = uid;
	m_Info.m_bUseImapForStartAndEnd = pCompEntityModelInfo->GetUseImapForStartAndEnd();

	// Models/Imaps
	if( m_Info.m_bUseImapForStartAndEnd )
	{
		m_Info.m_startIMapOrModelHash = pCompEntityModelInfo->GetStartImapFile().GetHash();
		m_Info.m_endIMapOrModelHash = pCompEntityModelInfo->GetEndImapFile().GetHash();
	}
	else
	{
		m_Info.m_startIMapOrModelHash = pCompEntityModelInfo->GetStartModelHash();
		m_Info.m_endIMapOrModelHash = pCompEntityModelInfo->GetEndModelHash();
	}

	// Particles
	m_Info.m_ptfxAssetNameHash = pCompEntityModelInfo->GetPtFxAssetName();

	// Anims/AnimModels
	u32 numAnims = pCompEntityModelInfo->GetNumOfAnims();
	m_Info.m_animModelHashes.Resize(numAnims);
	m_Info.m_animHashes.Resize(numAnims);
	for(int i=0; i<numAnims; i++)
	{
		CCompEntityAnims &animData = pCompEntityModelInfo->GetAnims(i);
		m_Info.m_animModelHashes[i] = atStringHash(animData.GetAnimatedModel());
		m_Info.m_animHashes[i] = atStringHash(animData.GetAnimDict());
	}
}

void CPacketRayFirePreLoad::Extract(const CEventInfo<void>& UNUSED_PARAM(eventInfo)) const
{
/*	// Potentially cause a unload/load situation, leave for the Rayfire to handle.
	// Left for convenience, in case it's ever needed.
	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_FWD)
	{
		if( m_Info.m_Load == false )	// The end of a rayfire, we're going forwards, so unload it's data (if it's not already been done)!
		{
			int loadedIndex = ReplayRayFireManager::FindLoadIndex(m_Info);
			if( loadedIndex )
			{
				ReplayRayFireManager::Release(loadedIndex);
			}
		}
	}
	else if( eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK )
	{
		if( m_Info.m_Load == true )		// The start of the rayfire, we're going backwards, so unload it's data (if it's not already been done)!
		{
			int loadedIndex = ReplayRayFireManager::FindLoadIndex(m_Info);
			if( loadedIndex )
			{
				ReplayRayFireManager::Release(loadedIndex);
			}
		}
	}
*/
}

ePreloadResult CPacketRayFirePreLoad::Preload(const CEventInfo<void>&eventInfo) const
{
	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_FWD)
	{
		if( m_Info.m_Load == true )					// The start of a rayfire, we're going forwards, so load!
		{
			ReplayRayFireManager::AddDeferredLoad(m_Info);		//	ReplayRayFireManager::Load(m_Info);
		}
	}
	else if( eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK )
	{
		if( m_Info.m_Load == false )		// The end of the rayfire, we're going backwards, so load!
		{
			ReplayRayFireManager::AddDeferredLoad(m_Info);		//	ReplayRayFireManager::Load(m_Info);
		}
	}
	
	return PRELOAD_SUCCESSFUL; 
}

#endif //GTA_REPLAY