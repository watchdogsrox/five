//
// name:        NetworkStreamingRequestManager.cpp
// description: Manages streaming requests made by network code based on network messages. This class should be used
//              for cases where a message from another machine is rejected until the model streams in (such as object creation messages)
//              this avoids models streaming in and then being streamed out before the resent network request has been resent
// written by:  Daniel Yelland
//

#include "NetworkStreamingRequestManager.h"

// rage headers
#include "system/timer.h"

// game headers
#include "Network/NetworkInterface.h"
#include "Network/Arrays/NetworkArrayMgr.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
#include "modelinfo/ModelInfo.h"
#include "peds/Ped.h"
#include "peds/PedIntelligence.h"


NETWORK_OPTIMISATIONS()

CNetworkStreamingRequestManager::RequestData CNetworkStreamingRequestManager::m_PendingRequests[CNetworkStreamingRequestManager::MAX_PENDING_MODEL_REQUESTS];

atFixedArray<CNetworkStreamingRequestManager::NetworkAnimRequestData, CNetworkStreamingRequestManager::MAX_NETWORK_ANIM_REQUESTS>	CNetworkStreamingRequestManager::ms_NetSyncedAnimRequests;
atFixedArray<CNetworkStreamingRequestManager::AnimStreamingHelper, CNetworkStreamingRequestManager::NUM_ANIM_REQUEST_HELPERS>		CNetworkStreamingRequestManager::ms_AnimStreamingHelpers;

u32 CNetworkStreamingRequestManager::m_deadPedTimerMS = 0;

#if !__FINAL
CNetworkStreamingRequestManager::RecordDeadPedRequest CNetworkStreamingRequestManager::m_DeadPedInfoArray[CNetworkStreamingRequestManager::MAX_DEAD_PED_RECORD_INFOS];
#endif

CVehicleClipRequestHelper CNetworkStreamingRequestManager::ms_VehicleClipRequestHelper;

void CNetworkStreamingRequestManager::Init()
{
    for(unsigned index = 0; index < MAX_PENDING_MODEL_REQUESTS; index++)
    {
        m_PendingRequests[index].Reset();
    }
#if !__FINAL
	ResetDeadPedInfos(true);
#endif

	ms_NetSyncedAnimRequests.Resize(MAX_NETWORK_ANIM_REQUESTS);
	ms_AnimStreamingHelpers.Resize(NUM_ANIM_REQUEST_HELPERS);

	m_deadPedTimerMS = 0;
}

void CNetworkStreamingRequestManager::Shutdown()
{
    for(unsigned index = 0; index < MAX_PENDING_MODEL_REQUESTS; index++)
    {
        m_PendingRequests[index].Reset();
    }

	for (unsigned index = 0; index < MAX_NETWORK_ANIM_REQUESTS; index++)
	{
		ms_NetSyncedAnimRequests[index].Clear();
	}

	for (unsigned index = 0; index < NUM_ANIM_REQUEST_HELPERS; index++)
	{
		if (ms_AnimStreamingHelpers[index].IsInUse())
		{
			ms_AnimStreamingHelpers[index].Remove();
		}
	}

	ms_NetSyncedAnimRequests.Reset();
	ms_AnimStreamingHelpers.Reset();
}

void CNetworkStreamingRequestManager::Update()
{
	UpdateModels();

	UpdateAnims();
}

void CNetworkStreamingRequestManager::UpdateModels()
{
   unsigned currTime = sysTimer::GetSystemMsTime();

    for(unsigned index = 0; index < MAX_PENDING_MODEL_REQUESTS; index++)
    {
        if(m_PendingRequests[index].m_InUse && currTime >= m_PendingRequests[index].m_TimeToAllowRemoval)
        {
            if(m_PendingRequests[index].m_ModelID.IsValid())
            {
                CModelInfo::ClearAssetRequiredFlag(m_PendingRequests[index].m_ModelID, STRFLAG_DONTDELETE);
            }

            m_PendingRequests[index].Reset();
        }
    }
}

void CNetworkStreamingRequestManager::UpdateAnims()
{
    if(!fwTimer::IsGamePaused())
    {
		//GarbageCollectDeadPedAnimRequests();

#if !__FINAL
		SanityCheckDeadPeds();
#endif /* !__FINAL */

	    RemoveOldStreamingAnims();

	    AddNewStreamingAnims();

	    UpdateStreamingAnims();
    }

#if !__FINAL && DEBUG_RENDER_NETWORK_STREAMING_ANIMS && DEBUG_ENABLE_NETWORK_STREAMING_ANIMS

	DebugRender();

#endif /* !__FINAL && DEBUG_RENDER_NETWORK_STREAMING_ANIMS && DEBUG_ENABLE_NETWORK_STREAMING_ANIMS */
}

void CNetworkStreamingRequestManager::RemoveOldStreamingAnims()
{
	// Go through all anims and release any that are no longer required....
	int numStreams = NUM_ANIM_REQUEST_HELPERS;
	for(int s = 0; s < numStreams; ++s)
	{
		if(ms_AnimStreamingHelpers[s].IsInUse())
		{
			u32 hash = ms_AnimStreamingHelpers[s].GetHash();

			bool foundRequest = GetNetworkAnimRequestIndex(hash) != INVALID_ANIM_REQUEST_INDEX;

			// this anim is not being requested anymore...
			if(!foundRequest)
			{
				// so release it and free up the slot....
				UnstreamAnim(hash);
			}
		}
	}
}

void CNetworkStreamingRequestManager::AddNewStreamingAnims()
{
	// Go through requests and start streaming anims in....
	int numRequests = MAX_NETWORK_ANIM_REQUESTS;
	for(int a = 0; a < numRequests; ++a)
	{
		if (ms_NetSyncedAnimRequests[a].m_numAnims > 0)
		{
			CPed* ped = ms_NetSyncedAnimRequests[a].m_Ped.GetPed();

			if( ped && ped->IsNetworkClone() )
			{
				for (int n=0; n<ms_NetSyncedAnimRequests[a].m_numAnims; n++)
				{
					if(!IsAnimStreaming(ms_NetSyncedAnimRequests[a].m_animHashes[n]))
					{
						StreamAnim(ped, ms_NetSyncedAnimRequests[a].m_animHashes[n]); 
					}
				}
			}
		}
	}
}

void CNetworkStreamingRequestManager::UpdateStreamingAnims()
{
	// Go through all anims and release any that are no longer required....
	int numStreams = NUM_ANIM_REQUEST_HELPERS;
	for(int s = 0; s < numStreams; ++s)
	{
		if(ms_AnimStreamingHelpers[s].IsInUse())
		{
			ms_AnimStreamingHelpers[s].Update();
		}
	}

    // Update the streamed vehicle anims
    UpdateVehicleAnimRequests();
}

// Added because the ped and array elements it's responsible for get migrated on different frames.
// A ped would get shot, add the anim requests, migrate and be killed off immediately by machine B 
// before the anim requests had arived and migrated to it which would orphan them.
// This way it doesn't matter if they get orphaned, they'll still get picked up...
void CNetworkStreamingRequestManager::GarbageCollectDeadPedAnimRequests(void)
{
	CPed*						localPlayer			= CGameWorld::FindLocalPlayer();
	CAnimRequestArrayHandler*	animRequestHandler	= NetworkInterface::GetArrayManager().GetAnimRequestArrayHandler();

	if(!localPlayer || !animRequestHandler)
	{
		return;
	}

	for(int i = 0; i < MAX_NETWORK_ANIM_REQUESTS; ++i)
	{
		if( ms_NetSyncedAnimRequests[i].m_Ped.GetPedID() != NETWORK_INVALID_OBJECT_ID)
		{
			CPed const* ped = ms_NetSyncedAnimRequests[i].m_Ped.GetPed();

			if(ped && ped->IsDead())
			{
				if(!ped->IsNetworkClone() && !ped->GetNetworkObject()->IsPendingOwnerChange())
				{
					if(animRequestHandler->GetElementArbitration(i) == localPlayer->GetNetworkObject()->GetPlayerOwner())
					{
						RemoveAllNetworkAnimRequests(ped);
					}
				}
			}
		}
	}
}

void CNetworkStreamingRequestManager::UpdateVehicleAnimRequests()
{
    ms_VehicleClipRequestHelper.ClearAllRequests();

    if(FindPlayerPed())
    {
        Vector3 localPlayerPosition = VEC3V_TO_VECTOR3(FindFollowPed()->GetTransform().GetPosition());

        unsigned lowPriorityPlayers   [MAX_NUM_PHYSICAL_PLAYERS];
        unsigned mediumPriorityPlayers[MAX_NUM_PHYSICAL_PLAYERS];
        unsigned highPriorityPlayers  [MAX_NUM_PHYSICAL_PLAYERS];

        unsigned numLowPriority    = 0;
        unsigned numMediumPriority = 0;
        unsigned numHighPriority   = 0;

        // iterate over all players building a list of anims to stream
        unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
        const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();
        
        for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
        {
            const CNetGamePlayer *remotePlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);

            CPed *playerPed = remotePlayer->GetPlayerPed();

            if(playerPed)
            {
                Vector3 remotePlayerPosition = VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition());

                float flatDistSqr = (remotePlayerPosition - localPlayerPosition).XYMag2();

                const float NO_STREAMING_THRESHOLD_SQR  = rage::square(300.0f);
                const float LOW_STREAMING_THRESHOLD_SQR = rage::square(100.0f);

                if(flatDistSqr < NO_STREAMING_THRESHOLD_SQR)
                {
                    if(flatDistSqr >= LOW_STREAMING_THRESHOLD_SQR)
                    {
                        if(playerPed->GetIsInVehicle())
                        {
                            if(gnetVerifyf(numMediumPriority < MAX_NUM_PHYSICAL_PLAYERS, "Too many players flagged at medium vehicle anim streaming priority!"))
                            {
                                mediumPriorityPlayers[numMediumPriority] = index;
                                numMediumPriority++;
                            }
                        }
                        else
                        {
                            if(gnetVerifyf(numLowPriority < MAX_NUM_PHYSICAL_PLAYERS, "Too many players flagged at low vehicle anim streaming priority!"))
                            {
                                lowPriorityPlayers[numLowPriority] = index;
                                numLowPriority++;
                            }
                        }
                    }
                    else
                    {
                        if(playerPed->GetIsInVehicle())
                        {
                            if(gnetVerifyf(numHighPriority < MAX_NUM_PHYSICAL_PLAYERS, "Too many players flagged at high vehicle anim streaming priority!"))
                            {
                                highPriorityPlayers[numHighPriority] = index;
                                numHighPriority++;
                            }
                        }
                        else
                        {
                            if(gnetVerifyf(numMediumPriority < MAX_NUM_PHYSICAL_PLAYERS, "Too many players flagged at medium vehicle anim streaming priority!"))
                            {
                                mediumPriorityPlayers[numMediumPriority] = index;
                                numMediumPriority++;
                            }
                        }
                    }
                }
            }
        }

        // now make the requests based on priority
        unsigned numRequestsMade = 0;

        RequestVehicleAnims(remotePhysicalPlayers, numRemotePhysicalPlayers, highPriorityPlayers,   numHighPriority,   numRequestsMade);
        RequestVehicleAnims(remotePhysicalPlayers, numRemotePhysicalPlayers, mediumPriorityPlayers, numMediumPriority, numRequestsMade);
        RequestVehicleAnims(remotePhysicalPlayers, numRemotePhysicalPlayers, lowPriorityPlayers,    numLowPriority,    numRequestsMade);
    }
}

void CNetworkStreamingRequestManager::RequestVehicleAnims(const netPlayer * const *remotePhysicalPlayers,
                                                          const unsigned           numRemotePhysicalPlayers,
                                                          const unsigned           playersAtPriority[MAX_NUM_PHYSICAL_PLAYERS],
                                                          const unsigned           numPlayersAtPriority,
                                                          unsigned                &numRequestsMade)
{
    const unsigned MAX_REMOTE_VEHICLE_ANIMS_TO_STREAM = 4;

    for(unsigned index = 0; (index < numPlayersAtPriority) && (numRequestsMade < MAX_REMOTE_VEHICLE_ANIMS_TO_STREAM); index++)
    {
        unsigned remotePlayerIndex = playersAtPriority[index];

        if(gnetVerifyf(remotePlayerIndex < numRemotePhysicalPlayers, "Unexpected player index!"))
        {
            const CNetGamePlayer *remotePlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[remotePlayerIndex]);

            CPed *playerPed = remotePlayer->GetPlayerPed();

            if(playerPed && playerPed->GetNetworkObject())
            {
                CNetObjPlayer *netObjPlayer = SafeCast(CNetObjPlayer, playerPed->GetNetworkObject());

                ObjectId targetVehicleID = netObjPlayer->GetTargetVehicleIDForAnimStreaming();

                netObject *networkObject    = NetworkInterface::GetNetworkObject(targetVehicleID);
                CVehicle  *vehicle          = NetworkUtils::GetVehicleFromNetworkObject(networkObject);
                s32        targetEntryPoint = netObjPlayer->GetTargetVehicleEntryPoint();

                if(vehicle && targetEntryPoint != -1)
                {
                    ms_VehicleClipRequestHelper.RequestClipsFromEntryPoint(vehicle, targetEntryPoint);

                    numRequestsMade = ms_VehicleClipRequestHelper.GetNumClipSetsRequested();
                }
            }
        }
    }
}

bool CNetworkStreamingRequestManager::RequestModel(fwModelId modelID)
{
    bool modelHasLoaded = CModelInfo::HaveAssetsLoaded(modelID);

    if(!modelHasLoaded)
	{
        // some requests can complete immediately - so it's worth performing the check again after the initial request
        if(CModelInfo::RequestAssets(modelID, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD))
        {
		    modelHasLoaded = CModelInfo::HaveAssetsLoaded(modelID);
        }

        if(!modelHasLoaded)
        {
            bool     duplicateFound = false;
            unsigned freeSlot       = MAX_PENDING_MODEL_REQUESTS;

            const unsigned TIME_TO_KEEP_MODEL_MS = 5000;

		    for(unsigned index = 0; index < MAX_PENDING_MODEL_REQUESTS && !duplicateFound; index++)
            {
                if(m_PendingRequests[index].m_InUse)
                {
                    if(m_PendingRequests[index].m_ModelID == modelID)
                    {
                        m_PendingRequests[index].m_TimeToAllowRemoval = sysTimer::GetSystemMsTime() + TIME_TO_KEEP_MODEL_MS;

                        duplicateFound = true;
                    }
                }
                else if(freeSlot == MAX_PENDING_MODEL_REQUESTS)
                {
                    freeSlot = index;
                }
            }

            if(!duplicateFound && (freeSlot < MAX_PENDING_MODEL_REQUESTS))
            {
                m_PendingRequests[freeSlot].m_InUse              = true;
                m_PendingRequests[freeSlot].m_ModelID            = modelID;
                m_PendingRequests[freeSlot].m_TimeToAllowRemoval = sysTimer::GetSystemMsTime() + TIME_TO_KEEP_MODEL_MS;

                CModelInfo::SetAssetRequiredFlag(modelID, STRFLAG_DONTDELETE);
            }
        }
	}

    return modelHasLoaded;
}

//----------------------------------------------------------------------------------------

#if DEBUG_ENABLE_NETWORK_STREAMING_ANIMS
bool CNetworkStreamingRequestManager::RequestNetworkAnimRequest(CPed const* pPed, u32 const animHash, bool bRemoveExistingRequests)
#else 
bool CNetworkStreamingRequestManager::RequestNetworkAnimRequest(CPed const* UNUSED_PARAM(pPed), u32 const UNUSED_PARAM(animHash))
#endif /* DEBUG_ENABLE_NETWORK_STREAMING_ANIMS */
{
#if DEBUG_ENABLE_NETWORK_STREAMING_ANIMS

	gnetAssert(pPed && !pPed->IsNetworkClone() && (INVALID_ANIM_REQUEST_HASH != animHash));

	int index = GetNetworkAnimRequestIndex(animHash, pPed);
	if(INVALID_ANIM_REQUEST_INDEX == index)
	{
		CAnimRequestArrayHandler* animRequestHandler = NetworkInterface::GetArrayManager().GetAnimRequestArrayHandler();

		// see if there is already an entry for this ped we can add to
		for(int i = 0; i < MAX_NETWORK_ANIM_REQUESTS; ++i)
		{
			// if we've got the right element
			if(animRequestHandler && animRequestHandler->GetElementIDForIndex(i) == pPed->GetNetworkObject()->GetObjectID())
			{
				// if we're in control of it (i.e the ped and the associated array element can migrate a few frames apart)...
				if(animRequestHandler->IsElementLocallyArbitrated(i))
				{
					ms_NetSyncedAnimRequests[i].m_Ped.SetPedID(pPed->GetNetworkObject()->GetObjectID());

					if (bRemoveExistingRequests)
					{
						ms_NetSyncedAnimRequests[i].m_animHashes[0] = animHash;
						ms_NetSyncedAnimRequests[i].m_numAnims = 1;
					}
					else if (AssertVerify(ms_NetSyncedAnimRequests[i].m_numAnims < MAX_NUM_ANIM_REQUESTS_PER_PED-1))
					{
						ms_NetSyncedAnimRequests[i].m_animHashes[ms_NetSyncedAnimRequests[i].m_numAnims++] = animHash;
					}

					animRequestHandler->SetElementDirty(i);

#if !__FINAL
					SanityCheckEntries();
#endif /* !__FINAL */

					return true;
				}
				else
				{
					return false;
				}
			}
		}

		for(int i = 0; i < MAX_NETWORK_ANIM_REQUESTS; ++i)
		{
			// ( node is empty ) and ( we've either got no array handler or the node is arbitrated localled and it's not dirty)....
			if( (ms_NetSyncedAnimRequests[i].m_numAnims == 0) &&
				(!animRequestHandler || (animRequestHandler->GetElementArbitration(i) == NULL)) )
			{
				ms_NetSyncedAnimRequests[i].m_animHashes[0]	= animHash;	
				ms_NetSyncedAnimRequests[i].m_numAnims = 1;
				ms_NetSyncedAnimRequests[i].m_Ped.SetPedID(pPed->GetNetworkObject()->GetObjectID());

				if(animRequestHandler)
				{
					animRequestHandler->SetElementDirty(i);
				}

#if !__FINAL
				SanityCheckEntries();
#endif /* !__FINAL */

				return true;
			}
		}

		gnetAssertf(false, "CNetworkStreamingRequestManager::RequestNetworkAnimRequest - Streaming request made but no spare slot?!");
	}

#if !__FINAL
	SanityCheckEntries();
#endif /* !__FINAL */

	return false;

#else

	return false;

#endif /* DEBUG_ENABLE_NETWORK_STREAMING_ANIMS */
}

bool CNetworkStreamingRequestManager::RemoveAllNetworkAnimRequests(CPed const* pPed)
{
	gnetAssertf(pPed && !pPed->IsNetworkClone(),  "ERROR : CNetworkStreamingRequestManager::RemoveAllNetworkAnimRequests : Removing Anim Requests From Clone Ped?!");

	CPed *pLocalPlayer								= CPedFactory::GetFactory()->GetLocalPlayer();
	CAnimRequestArrayHandler* animRequestHandler	= NetworkInterface::GetArrayManager().GetAnimRequestArrayHandler();

	if(!pLocalPlayer)
	{
		return false;
	}

	if(pPed && pPed->GetNetworkObject() && !pPed->IsNetworkClone())
	{
		for(int i = 0; i < MAX_NETWORK_ANIM_REQUESTS; ++i)
		{
			if(!ms_NetSyncedAnimRequests[i].IsEmpty())
			{
				if(ms_NetSyncedAnimRequests[i].m_Ped.GetPedID() == pPed->GetNetworkObject()->GetObjectID())
				{
					if(animRequestHandler && animRequestHandler->GetElementArbitration(i) == pLocalPlayer->GetNetworkObject()->GetPlayerOwner())
					{
						animRequestHandler->SetElementDirty(i);
					}

					ms_NetSyncedAnimRequests[i].Clear();	
					return true;
				}
			}
		}
	}
	
	return false;
}

bool CNetworkStreamingRequestManager::StreamAnim(CPed const* NET_ASSERTS_ONLY(pPed), u32 const animHash)
{
	gnetAssert(pPed && pPed->IsNetworkClone() && (INVALID_ANIM_REQUEST_HASH != animHash));

	if(animHash != INVALID_ANIM_REQUEST_HASH)
	{
		eNmBlendOutSet set = eNmBlendOutSet(animHash);
		gnetAssert(set != NMBS_INVALID);

		int streamerIndex = INVALID_ANIM_HELPER_INDEX;

		if(!IsAnimStreaming(animHash))
		{
			streamerIndex = GetFreeAnimStreamingHelperSlot(animHash);			
			gnetAssertf(streamerIndex != INVALID_ANIM_HELPER_INDEX, "ERROR : CNetworkStreamingRequestManager::StreamAnim - too many anims being streamed in at once - will cause popping / pauses in TaskGetUp");
		}
		else
		{
			streamerIndex = GetAnimStreamingHelperSlot(animHash);
			gnetAssert(ms_AnimStreamingHelpers[streamerIndex].m_helper.GetRequiredGetUpSetId().GetHash() == animHash);
		}

		ms_AnimStreamingHelpers[streamerIndex].Request(set);

		return true;
	}

	return false;
}

bool CNetworkStreamingRequestManager::UnstreamAnim(u32 const animHash)
{
	// Can be called by clone or owner so if a ped migrates, the new owner will
	// release it's hold on anims which should be controlled by owner tasks anyway

	gnetAssert(INVALID_ANIM_REQUEST_HASH != animHash);

	u32 animIndex = GetAnimStreamingHelperSlot(animHash);
	
	if(INVALID_ANIM_HELPER_INDEX != animIndex)
	{
		ms_AnimStreamingHelpers[animIndex].Remove();

		return true;
	}

	return false;
}

bool CNetworkStreamingRequestManager::IsAnimStreaming(u32 const animHash)
{
	gnetAssert(INVALID_ANIM_REQUEST_HASH != animHash);

	int numRequestHelpers = NUM_ANIM_REQUEST_HELPERS;

	for(int h = 0; h < numRequestHelpers; ++h)
	{
		if(ms_AnimStreamingHelpers[h].m_helper.GetRequiredGetUpSetId().GetHash() == animHash)
		{
			return true;
		}
	}

	return false;
}

int CNetworkStreamingRequestManager::GetAnimStreamingHelperSlot(u32 const animHash)
{
	gnetAssert(INVALID_ANIM_REQUEST_HASH != animHash);

	// do we already have one of our GetUpHelpers dealing with this request?
	if(IsAnimStreaming(animHash))
	{
		int numRequestHelpers = NUM_ANIM_REQUEST_HELPERS;
		for(int h = 0; h < numRequestHelpers; ++h)
		{
			if(ms_AnimStreamingHelpers[h].m_helper.GetRequiredGetUpSetId().GetHash() == animHash)
			{
				return h;
			}
		}	
	}

	return INVALID_ANIM_HELPER_INDEX;
}


int CNetworkStreamingRequestManager::GetFreeAnimStreamingHelperSlot(u32 const animHash)
{
	gnetAssert(INVALID_ANIM_REQUEST_HASH != animHash);

	// do we already have one of our GetUpHelpers dealing with this request?
	if(!IsAnimStreaming(animHash))
	{
		int numRequestHelpers = NUM_ANIM_REQUEST_HELPERS;
		for(int h = 0; h < numRequestHelpers; ++h)
		{
			if(ms_AnimStreamingHelpers[h].m_helper.GetRequiredGetUpSetId() == NMBS_INVALID)
			{
				return h;
			}
		}	
	}

	return INVALID_ANIM_HELPER_INDEX;
}

int CNetworkStreamingRequestManager::GetNetworkAnimRequestIndex(u32 const animHash, CPed const * pPed /* = NULL */)
{
	gnetAssert(INVALID_ANIM_REQUEST_HASH != animHash);

	for(int i = 0; i < MAX_NETWORK_ANIM_REQUESTS; ++i)
	{
		if( (!pPed) || ( ms_NetSyncedAnimRequests[i].m_Ped.GetPedID() == pPed->GetNetworkObject()->GetObjectID()) ) 
		{
			for (int j=0; j<ms_NetSyncedAnimRequests[i].m_numAnims; j++)
			{
				if (ms_NetSyncedAnimRequests[i].m_animHashes[j] == animHash)
				{
					return i;
				}
			}
		}
	}

	return INVALID_ANIM_REQUEST_INDEX;
}

#if !__FINAL
void CNetworkStreamingRequestManager::ResetDeadPedInfos(bool bForceReset /*= false*/)
{
	if(m_deadPedTimerMS || bForceReset)
	{
		for(unsigned index = 0; index < MAX_DEAD_PED_RECORD_INFOS; index++)
		{
			m_DeadPedInfoArray[index].Reset();
		}
	}

	m_deadPedTimerMS =0;
}

void CNetworkStreamingRequestManager::DebugPrintDeadPedInfos(void)
{
	Displayf("\nDEAD PED INFO:\n");
	for(unsigned index = 0; index < MAX_DEAD_PED_RECORD_INFOS; index++)
	{
		if(m_DeadPedInfoArray[index].m_deadPedID != NETWORK_INVALID_OBJECT_ID)
		{
			Displayf("	[Index %d] Dead Ped ID %d, %s, Valid object %s. Requested more than once %s. At Frame %d, Time MS %d",
				index,
				m_DeadPedInfoArray[index].m_deadPedID,
				m_DeadPedInfoArray[index].m_bClone?"CLONE":"LOCAL",
				m_DeadPedInfoArray[index].m_bValidObject?"TRUE":"FALSE",
				m_DeadPedInfoArray[index].m_bRequestedAgain?"TRUE":"FALSE",
				m_DeadPedInfoArray[index].m_Frame,
				m_DeadPedInfoArray[index].m_deadPedTimerMS);
		}
	}
}

CNetworkStreamingRequestManager::RecordDeadPedRequest* CNetworkStreamingRequestManager::IsInDeadPedInfos(ObjectId pedId)
{
	if(taskVerifyf(pedId != NETWORK_INVALID_OBJECT_ID,"Expect valid ped id") )
	{
		for(unsigned index = 0; index < MAX_DEAD_PED_RECORD_INFOS; index++)
		{
			if(m_DeadPedInfoArray[index].m_deadPedID == pedId)
			{
				m_DeadPedInfoArray[index].m_bRequestedAgain = true; //record a second attempt to add
				return &m_DeadPedInfoArray[index];
			}
		}
	}

	return NULL;
}

bool CNetworkStreamingRequestManager::AddToDeadPedInfos(ObjectId pedId, CPed const* pPed)
{
	if(!IsInDeadPedInfos(pedId))
	{
		for(unsigned index = 0; index < MAX_DEAD_PED_RECORD_INFOS; index++)
		{
			if(m_DeadPedInfoArray[index].m_deadPedID == NETWORK_INVALID_OBJECT_ID)
			{
				m_DeadPedInfoArray[index].m_deadPedID = pedId;
				m_DeadPedInfoArray[index].m_Frame = fwTimer::GetFrameCount();
				m_DeadPedInfoArray[index].m_deadPedTimerMS = fwTimer::GetTimeInMilliseconds();

				if(pPed)
				{
					m_DeadPedInfoArray[index].m_bClone			= pPed->IsNetworkClone();
					m_DeadPedInfoArray[index].m_bValidObject	= true;
				}
				else
				{
					m_DeadPedInfoArray[index].m_bClone			= false;
					m_DeadPedInfoArray[index].m_bValidObject	= false;
				}
				return true;
			}
		}

		//No spare slots found so return false
		return false;
	}

	return true;
}


//Check the current dead ped list against the current existing requests and if it finds a match
//then also checks if the peds time exceeds the DEAD_PED_REQUEST_ORPHAN_TEST_TIME_LIMIT_MS limit.
//If no ped matches both criteria but if ped(s) *are* found that don't exceed DEAD_PED_REQUEST_ORPHAN_TEST_TIME_LIMIT_MS
//then m_deadPedTimerMS is reset to the earliest peds time
bool CNetworkStreamingRequestManager::CheckACurrentDeadPedExceedsTime()
{
	u32 uEarliestDeadPedTimerMS = fwTimer::GetTimeInMilliseconds(); // set to now
	bool bFoundDeadPedInRequests = false;

	for(int i = 0; i < MAX_NETWORK_ANIM_REQUESTS; ++i)
	{
		if( ms_NetSyncedAnimRequests[i].m_Ped.GetPedID() != NETWORK_INVALID_OBJECT_ID)
		{
			ObjectId pedId =  ms_NetSyncedAnimRequests[i].m_Ped.GetPedID();

			RecordDeadPedRequest* pDeadPedRequest = IsInDeadPedInfos(pedId);
			if(pDeadPedRequest)
			{
				if(pDeadPedRequest->m_deadPedTimerMS < uEarliestDeadPedTimerMS)
				{
					bFoundDeadPedInRequests = true;
					uEarliestDeadPedTimerMS = pDeadPedRequest->m_deadPedTimerMS;
				}
			}
		}
	}

	if(bFoundDeadPedInRequests)
	{
		if( (fwTimer::GetTimeInMilliseconds() - uEarliestDeadPedTimerMS) > DEAD_PED_REQUEST_ORPHAN_TEST_TIME_LIMIT_MS )
		{
			//Found a dead in current requests *and* its timer exceed assert limit so return true to allow assert to proceed
			return true;
		}

		//Found a dead in current requests its timer doesn't exceed assert limit so set m_deadPedTimerMS to this
		m_deadPedTimerMS = uEarliestDeadPedTimerMS;
	}

	return false;
}


void CNetworkStreamingRequestManager::SanityCheckEntries(void)
{
	for(int i = 0; i < MAX_NETWORK_ANIM_REQUESTS; ++i)
	{
		if( ms_NetSyncedAnimRequests[i].m_Ped.GetPedID() != NETWORK_INVALID_OBJECT_ID)
		{
			NET_ASSERTS_ONLY(ObjectId pedId = ) ms_NetSyncedAnimRequests[i].m_Ped.GetPedID();

			for(int j = i+1; j < MAX_NETWORK_ANIM_REQUESTS; ++j)
			{
				if( ms_NetSyncedAnimRequests[j].m_Ped.GetPedID() != NETWORK_INVALID_OBJECT_ID)
				{
					NET_ASSERTS_ONLY(ObjectId compId = ) ms_NetSyncedAnimRequests[j].m_Ped.GetPedID();

					gnetAssert(pedId != compId);
				}
			}
		}
	}
}

void CNetworkStreamingRequestManager::SanityCheckDeadPeds(void)
{
	//Slight bug in here in that we only need any dead ped to further the timer
	//Ped A might start the timer and get cleaned up but ped B could continue it 
	// and so on. If we get enough peds to be sequentially dead for 5 seconds
	// something is probably up anyway...

	for(int i = 0; i < MAX_NETWORK_ANIM_REQUESTS; ++i)
	{
		ObjectId pedID = ms_NetSyncedAnimRequests[i].m_Ped.GetPedID();

		if( pedID != NETWORK_INVALID_OBJECT_ID)
		{
			CPed* ped = ms_NetSyncedAnimRequests[i].m_Ped.GetPed();

			if(ped && ped->IsDead())
			{
				if(0 == m_deadPedTimerMS)
				{
					AddToDeadPedInfos(pedID, ped);
					m_deadPedTimerMS = fwTimer::GetTimeInMilliseconds();
				}
				else if( (fwTimer::GetTimeInMilliseconds() - m_deadPedTimerMS) > DEAD_PED_REQUEST_ORPHAN_TEST_TIME_LIMIT_MS &&
						 CheckACurrentDeadPedExceedsTime() )
				{
					DebugPrint();
					DebugPrintDeadPedInfos();
					gnetAssertf(false, "Error : There have been dead peds requesting anims for > 5 seconds - anim requests have been orphaned?!");
					ResetDeadPedInfos();
				}
				else
				{   //keep adding info while m_deadPedTimerMS is ticking up
					bool bSuccess = AddToDeadPedInfos(pedID, ped);

					if(!bSuccess)
					{
						DebugPrintDeadPedInfos();
						gnetAssertf(false, "Ran out of MAX_DEAD_PED_RECORD_INFOS %d slots to add %d %s to DeadPedInfos",
							MAX_DEAD_PED_RECORD_INFOS,
							pedID,
							ped->IsNetworkClone()?"CLONE":"LOCAL");
					}
				}

				return;
			}
		}
	}

	// either no requests or no peds dead, reset the dead ped info...
	ResetDeadPedInfos(); 
}

void CNetworkStreamingRequestManager::DebugPrint(void)
{
	if(NetworkInterface::IsGameInProgress() && !fwTimer::IsGamePaused())
	{
		SanityCheckEntries();

		int const numRequests = MAX_NETWORK_ANIM_REQUESTS;

		bool display = false;
		for(int i = 0; i < numRequests; ++i)
		{
			if(!ms_NetSyncedAnimRequests[i].IsEmpty())
			{
				display = true;
			}
		}

		if(!display)
		{
			return;
		}

		CPed *pLocalPlayer = CPedFactory::GetFactory()->GetLocalPlayer();

		if(pLocalPlayer)
		{
			Displayf("\nFrame %d : %s", fwTimer::GetFrameCount(), __FUNCTION__);

			for(int i = 0; i < numRequests; ++i)
			{
				if(!ms_NetSyncedAnimRequests[i].IsEmpty())
				{
					Displayf("Request %d: Num Anims %d", i, ms_NetSyncedAnimRequests[i].m_numAnims);

					CPed* ped = ms_NetSyncedAnimRequests[i].m_Ped.GetPed();

					const netPlayer* arbitrator = NetworkInterface::GetArrayManager().GetAnimRequestArrayHandler()->GetElementArbitration(i);

					for (int j=0; j<ms_NetSyncedAnimRequests[i].m_numAnims; j++)
					{
						Displayf("- %d = %d %s : %p %s : arb = %s", 
						i, 
						ms_NetSyncedAnimRequests[i].m_animHashes[j], 
						atHashWithStringNotFinal(ms_NetSyncedAnimRequests[i].m_animHashes[j]).GetCStr(), 
						ped, 
						BANK_ONLY(ped ? ped->GetDebugName() :) "NULL", 
						arbitrator ? arbitrator->GetLogName() : "NONE");
					}
				}
			}

			Displayf("\n");

			int const numStreams = NUM_ANIM_REQUEST_HELPERS;
			for(int j = 0; j < numStreams; ++j)
			{
				CGetupSetRequestHelper& helperRef = ms_AnimStreamingHelpers[j].m_helper;

				//if(helperRef.GetRequiredGetUpSetId() != NMBS_INVALID)
				{
					Displayf("%s %d\n", helperRef.GetRequiredGetUpSetId().GetCStr(), helperRef.GetRequiredGetUpSetId().GetHash());
				}
			}
		}
	}
}

#endif /*__FINAL */

#if !__FINAL && DEBUG_RENDER_NETWORK_STREAMING_ANIMS

void CNetworkStreamingRequestManager::DebugRender(void)
{
	if(NetworkInterface::IsGameInProgress())
	{
		SanityCheckEntries();

		CPed *pLocalPlayer = CPedFactory::GetFactory()->GetLocalPlayer();

		if(pLocalPlayer)
		{
			int const numRequests = MAX_NETWORK_ANIM_REQUESTS;
			Vector2 screenPos(0.1f, 0.1f);
			char temp[256] = "\0";

			if(m_deadPedTimerMS != 0)
			{
				sprintf(temp, "Dead ped timer %d\n", fwTimer::GetTimeInMilliseconds() - m_deadPedTimerMS);
				grcDebugDraw::Text(screenPos, Color_white, temp, true, 0.9f, 0.9f);
				screenPos.y += 0.015f;
			}

			for(int i = 0; i < numRequests; ++i)
			{
				CPed* ped = ms_NetSyncedAnimRequests[i].m_Ped.GetPed();

				Color32 color = Color_white;

				if(ped && ped->IsDead())
				{
					color = Color_red;
				}

				if( ms_NetSyncedAnimRequests[i].m_numAnims == 0 && ms_NetSyncedAnimRequests[i].m_Ped.IsValid())
				{
					color = Color_yellow;
				}

				sprintf(temp, "Request %d: NumAnims %d\n", i, ms_NetSyncedAnimRequests[i].m_numAnims);
				grcDebugDraw::Text(screenPos, color, temp, true, 0.9f, 0.9f);
				screenPos.y += 0.015f;

				for (int j=0; j<ms_NetSyncedAnimRequests[i].m_numAnims; j++)
				{
					sprintf(temp, "- %d = %d %s : %p %s\n", 
					i, 
					ms_NetSyncedAnimRequests[i].m_animHashes[j], 
					atHashWithStringNotFinal(ms_NetSyncedAnimRequests[i].m_animHashes[j]).GetCStr(), 
					ped, 
					ped ? ped->GetDebugName() : "NULL");
					
					grcDebugDraw::Text(screenPos, color, temp, true, 0.9f, 0.9f);
					screenPos.y += 0.015f;
				}
			}

			CAnimRequestArrayHandler* animRequestHandler = NetworkInterface::GetArrayManager().GetAnimRequestArrayHandler();

			if(animRequestHandler)
			{
				screenPos.y += 0.015f;

				sprintf(temp, "Elements:\n");
				grcDebugDraw::Text(screenPos, Color_white, temp, true, 0.9f, 0.9f);
				screenPos.y += 0.015f;

				int const numElements = CAnimRequestArrayHandler::MAX_ELEMENTS; /* == NUM_ANIM_REQUEST_HELPERS */
				for(int e = 0; e < numElements; ++e)
				{
					const netPlayer* arbitrator = NetworkInterface::GetArrayManager().GetAnimRequestArrayHandler()->GetElementArbitration(e);

					sprintf(temp, "- %d : ID %d : Arb %s\n", 
					e, 
					animRequestHandler->GetElementIDForIndex(e), 
					arbitrator ? arbitrator->GetLogName() : "NONE");
					grcDebugDraw::Text(screenPos, Color_white, temp, true, 0.9f, 0.9f);
					screenPos.y += 0.015f;
				}
			}

			screenPos.y += 0.015f;

			sprintf(temp, "Streams:\n");
			grcDebugDraw::Text(screenPos, Color_white, temp, true, 0.9f, 0.9f);
			screenPos.y += 0.015f;

			int const numStreams = NUM_ANIM_REQUEST_HELPERS;
			for(int j = 0; j < numStreams; ++j)
			{
				CGetupSetRequestHelper& helperRef = ms_AnimStreamingHelpers[j].m_helper;

				//if(helperRef.GetRequiredGetUpSetId() != NMBS_INVALID)
				{
					sprintf(temp, "- %d : %s %d\n", j, helperRef.GetRequiredGetUpSetId().GetCStr(), helperRef.GetRequiredGetUpSetId().GetHash());
					grcDebugDraw::Text(screenPos, Color_white, temp, true, 0.9f, 0.9f);
					screenPos.y += 0.015f;
				}
			}
		}
	}
}

#endif /* !__FINAL && DEBUG_RENDER_NETWORK_STREAMING_ANIMS */

//----------------------------------------------------------------------------------------

void CNetworkStreamingRequestManager::NetworkAnimRequestData::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned SIZEOF_NUM_ANIMS = datBitsNeeded<MAX_NUM_ANIM_REQUESTS_PER_PED>::COUNT;

	SERIALISE_UNSIGNED(serialiser, m_numAnims, SIZEOF_NUM_ANIMS);

	for (u32 i=0; i<m_numAnims; i++)
	{
		SERIALISE_UNSIGNED(serialiser, m_animHashes[i], 32, "Anim Request Hash");
	}
}

void CNetworkStreamingRequestManager::NetworkAnimRequestData::Clear()
{
	for (u32 i=0; i<MAX_NUM_ANIM_REQUESTS_PER_PED; i++)
	{
		m_animHashes[i] = INVALID_ANIM_REQUEST_HASH;
	}
	m_numAnims = 0;
	m_Ped.Invalidate();
}

void CNetworkStreamingRequestManager::NetworkAnimRequestData::Copy(NetworkAnimRequestData &animRequestData)
{
	m_numAnims = animRequestData.m_numAnims;

	for (u32 i=0; i<m_numAnims; i++)
	{
		m_animHashes[i]	= animRequestData.m_animHashes[i];
	}
}

bool CNetworkStreamingRequestManager::NetworkAnimRequestData::IsEmpty(void) const
{
	return m_numAnims == 0;
}

netObject* CNetworkStreamingRequestManager::NetworkAnimRequestData::GetNetworkObject() const 
{ 
	CPed* pPed = m_Ped.GetPed();

	if (pPed)
	{
		return pPed->GetNetworkObject();
	}

	return NetworkInterface::GetNetworkObject(m_Ped.GetPedID()); 
}


//----------------------------------------------------------------------------------------

void CNetworkStreamingRequestManager::AnimStreamingHelper::Request(u32 const animHash)
{
	m_helper.Request(animHash);
}

void CNetworkStreamingRequestManager::AnimStreamingHelper::Remove(void)
{
	m_helper.Release();
}

void CNetworkStreamingRequestManager::AnimStreamingHelper::Update(void)
{
	m_helper.Request(m_helper.GetRequiredGetUpSetId());
}

bool CNetworkStreamingRequestManager::AnimStreamingHelper::IsInUse() const
{
	return m_helper.GetRequiredGetUpSetId() != INVALID_ANIM_REQUEST_HASH;
}

int CNetworkStreamingRequestManager::AnimStreamingHelper::GetHash() const
{
	return m_helper.GetRequiredGetUpSetId().GetHash();
}

//----------------------------------------------------------------------------------------
