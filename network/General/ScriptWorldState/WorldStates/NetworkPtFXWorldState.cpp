//
// name:		NetworkPopGroupOverrideWorldState.cpp
// description:	Support for network scripts to play particle effects via the script world state management code
// written by:	Daniel Yelland
//
//
#include "NetworkPtFXWorldState.h"

#include "bank/bkmgr.h"
#include "bank/bank.h"
#include "grcore/debugdraw.h"
#include "vector/geometry.h"
#include "vfx/ptfx/ptfxmanager.h"
#include "vfx/ptfx/ptfxscript.h"
#include "vfx/systems/vfxscript.h"

#include "fwnet/NetLogUtils.h"

#include "Network/NetworkInterface.h"
#include "Network/Objects/Entities/NetObjPed.h"
#include "Peds/ped.h"
#include "script/handlers/GameScriptResources.h"
#include "script/Script.h"

#include "control/replay/Effects/ParticleMiscFxPacket.h"

NETWORK_OPTIMISATIONS()

static const unsigned MAX_SCRIPTED_PTFX_DATA = 180;
FW_INSTANTIATE_CLASS_POOL(CNetworkPtFXWorldStateData, MAX_SCRIPTED_PTFX_DATA, atHashString("CNetworkPtFXWorldStateData",0x172c0c01));

CNetworkPtFXWorldStateData::CNetworkPtFXWorldStateData() :
m_PtFXHash(0)
, m_PtFXAssetHash(0)
, m_FxPos(VEC3_ZERO)
, m_FxRot(VEC3_ZERO)
, m_Scale(1.0f)
, m_InvertAxes(0)
, m_ScriptPTFX(0)
, m_StartPTFX(false)
, m_PTFXRefAdded(false)
, m_hasColour(false)
, m_r(0)
, m_g(0)
, m_b(0)
, m_bUseEntity(false)
, m_EntityID(NETWORK_INVALID_OBJECT_ID)
, m_boneIndex(-1)
, m_evoHashA(0)
, m_evoValA(0.f)
, m_evoHashB(0)
, m_evoValB(0.f)
, m_bAttachedToWeapon(false)
, m_bTerminateOnOwnerLeave(false)
, m_ownerPeerID(0)
, m_uniqueID(0)
{
}

CNetworkPtFXWorldStateData::CNetworkPtFXWorldStateData(const CGameScriptId &scriptID,
                                                       bool                 locallyOwned,
                                                       u32                  ptFXHash,
                                                       u32                  ptFXAssetHash,
                                                       const Vector3       &fxPos,
                                                       const Vector3       &fxRot,
                                                       float                scale,
                                                       u8                   invertAxes,
                                                       int                  scriptPTFX,
                                                       bool                 startPTFX,
                                                       bool                 pTFXRefAdded,
													   bool					hasColour,
													   u8					r,
													   u8					g,
													   u8					b,
													   ObjectId				entityId,
													   int					boneIndex,
													   u32					evoHashA,
													   float				evoValA,
													   u32					evoHashB,
													   float				evoValB,
													   bool					attachedToWeapon,
													   bool                 terminateOnOwnerLeave,
													   u64                  ownerPeerID,
													   int					uniqueID) :
CNetworkWorldStateData(scriptID, locallyOwned)
, m_PtFXHash(ptFXHash)
, m_PtFXAssetHash(ptFXAssetHash)
, m_FxPos(fxPos)
, m_FxRot(fxRot)
, m_Scale(scale)
, m_InvertAxes(invertAxes)
, m_ScriptPTFX(scriptPTFX)
, m_StartPTFX(startPTFX)
, m_PTFXRefAdded(pTFXRefAdded)
, m_hasColour(hasColour)
, m_r(r)
, m_g(g)
, m_b(b)
, m_bUseEntity(m_EntityID != NETWORK_INVALID_OBJECT_ID)
, m_EntityID(entityId)
, m_boneIndex(boneIndex)
, m_evoHashA(evoHashA)
, m_evoValA(evoValA)
, m_evoHashB(evoHashB)
, m_evoValB(evoValB)
, m_bAttachedToWeapon(attachedToWeapon)
, m_bTerminateOnOwnerLeave(terminateOnOwnerLeave)
, m_ownerPeerID(ownerPeerID)
, m_uniqueID(uniqueID)
{
	m_evolveCount = 0;

	if (m_evoHashA)
		m_evolveCount++;

	if (m_evoHashB)
		m_evolveCount++;
}

bool CNetworkPtFXWorldStateData::AddEvolve(u32 evoHash, float evoVal)
{
	//already set A - update
	if (evoHash == m_evoHashA)
	{
		if (m_evoValA != evoVal) 
		{
			m_evoValA = evoVal;
			return true;
		}
		else
		{
			return false;
		}
	}

	//already set B - update
	if (evoHash == m_evoHashB)
	{
		if (m_evoValA != evoVal)
		{
			m_evoValA = evoVal;
			return true;
		}
		else
		{
			return false;
		}
	}

	if (m_evolveCount == 0)
	{
		m_evoHashA = evoHash;
		m_evoValA = evoVal;
		m_evolveCount++;
		return true;
	}
	else if (m_evolveCount == 1)
	{
		m_evoHashB = evoHash;
		m_evoValB = evoVal;
		m_evolveCount++;
		return true;
	}
	else
	{
		Assertf(false, "CNetworkPtFXWorldStateData::AddEvolve - not added beyond max values evoHash[%u] evoVal[%f] m_evolveCount[%d] -- might need to sync more evolve values, otherwise this EVO won't show up in remote player's machines!",evoHash,evoVal,m_evolveCount);
	}

	return false;
}

void CNetworkPtFXWorldStateData::SetColour(float r, float g, float b)
{
	m_hasColour = true;
	m_r = (u8) (255 * r);
	m_g = (u8) (255 * g);
	m_b = (u8) (255 * b);
}

void CNetworkPtFXWorldStateData::Update()
{
    if(m_StartPTFX)
    {
        strLocalIndex slot = ptfxManager::GetAssetStore().FindSlotFromHashKey(m_PtFXAssetHash);

        if (gnetVerifyf(slot.IsValid(), "Cannot find particle asset slot (%d)!", m_PtFXAssetHash))
		{
            if(!ptfxManager::GetAssetStore().HasObjectLoaded(slot))
            {
                ptfxManager::GetAssetStore().StreamingRequest(slot, STRFLAG_FORCE_LOAD | STRFLAG_DONTDELETE);
            }
            else
            {
                if(gnetVerifyf(m_ScriptPTFX == 0, "Particle effect has already been created!"))
                {
					CEntity* pEntity = NULL;
					if(m_bUseEntity)
					{
						if(m_EntityID != NETWORK_INVALID_OBJECT_ID)
						{
							netObject* pNetObj = NetworkInterface::GetObjectManager().GetNetworkObject(m_EntityID);
							if(pNetObj)
							{
								if(m_bAttachedToWeapon)
								{
									CNetObjPed* pNetPed = SafeCast(CNetObjPed, pNetObj);
									if(pNetPed)
									{
										CPed* ped = SafeCast(CPed, pNetPed->GetEntity());
										if(ped && ped->GetWeaponManager() && ped->GetWeaponManager()->GetEquippedWeapon())
										{
											pEntity = const_cast<CEntity*>(ped->GetWeaponManager()->GetEquippedWeapon()->GetEntity());
										}
									}
								}
								else
								{
									pEntity = pNetObj->GetEntity();
								}
							}

							//Delay until the entity is replicated - continue allowing the Update to call until the entity exists and this can process
							if (!pEntity)
								return;
						}
					}

                    int scriptPTFXId = ptfxScript::Start(m_PtFXHash, m_PtFXAssetHash, pEntity, m_boneIndex, RC_VEC3V(m_FxPos), RC_VEC3V(m_FxRot), m_Scale, m_InvertAxes);
                    if (scriptPTFXId != 0) // Against everything else I know and believe, Start returns 0 on failure.
					{
						m_ScriptPTFX = scriptPTFXId;
						m_StartPTFX  = false;

						if (m_hasColour)
						{
							ptfxScript::UpdateColourTint(m_ScriptPTFX, Vec3V( (float) (m_r/255.0), (float) (m_g/255.0), (float) (m_b/255.0) ) );
						}

						if (m_evoHashA)
						{
							ptfxScript::UpdateEvo(m_ScriptPTFX, m_evoHashA, m_evoValA);
						}

						if (m_evoHashB)
						{
							ptfxScript::UpdateEvo(m_ScriptPTFX, m_evoHashB, m_evoValB);
						}

						ptfxManager::GetAssetStore().AddRef(slot, REF_OTHER);
						ptfxManager::GetAssetStore().ClearRequiredFlag(slot.Get(), STRFLAG_DONTDELETE);

						m_PTFXRefAdded = true;

#if GTA_REPLAY
						if(CReplayMgr::ShouldRecord())
						{
							CReplayMgr::RecordPersistantFx<CPacketStartNetworkScriptPtFx>(
								CPacketStartNetworkScriptPtFx(m_PtFXHash, m_PtFXAssetHash, m_boneIndex, m_FxPos, m_FxRot, m_Scale, m_InvertAxes,
														m_hasColour, m_r, m_g, m_b,
														m_evoHashA, m_evoValA,
														m_evoHashB, m_evoValB	),
								CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)scriptPTFXId),
								pEntity, 
								true);
						}
#endif
					}
                }
            }
        }
    }
}

bool CNetworkPtFXWorldStateData::HasExpired() const
{
	// revert client created PTFX when the player that created them has left the session
	if(m_bTerminateOnOwnerLeave)
	{
		if(!netInterface::GetPlayerFromPeerId(m_ownerPeerID))
		{
			return true;
		}
	}

	return false;
}


CNetworkPtFXWorldStateData *CNetworkPtFXWorldStateData::Clone() const
{
    return rage_new CNetworkPtFXWorldStateData(GetScriptID(), IsLocallyOwned(), m_PtFXHash, m_PtFXAssetHash, m_FxPos, m_FxRot, m_Scale, m_InvertAxes, m_ScriptPTFX, m_StartPTFX, m_PTFXRefAdded, m_hasColour, m_r, m_g, m_b, m_EntityID, m_boneIndex, m_evoHashA, m_evoValA, m_evoHashB, m_evoValB, m_bAttachedToWeapon, m_bTerminateOnOwnerLeave, m_ownerPeerID, m_uniqueID);
}

bool CNetworkPtFXWorldStateData::IsEqualTo(const CNetworkWorldStateData &worldState) const
{
    if(CNetworkWorldStateData::IsEqualTo(worldState))
    {
        const CNetworkPtFXWorldStateData *ptFXState = SafeCast(const CNetworkPtFXWorldStateData, &worldState);

        const float epsilon = 0.2f; // a relatively high epsilon value to take quantisation from network serialising into account

        if(m_PtFXHash == ptFXState->m_PtFXHash      
            && m_PtFXAssetHash == ptFXState->m_PtFXAssetHash 
            && m_FxPos.IsClose(ptFXState->m_FxPos, epsilon)  
            && m_FxRot.IsClose(ptFXState->m_FxRot, epsilon)  
            && IsClose(m_Scale, ptFXState->m_Scale, epsilon) 
            && m_InvertAxes    == ptFXState->m_InvertAxes 
            && m_EntityID == ptFXState->m_EntityID 
            && m_boneIndex == ptFXState->m_boneIndex 
            && m_bAttachedToWeapon == ptFXState->m_bAttachedToWeapon)
        {
			return true;
        }
    }

    return false;
}

void CNetworkPtFXWorldStateData::ChangeWorldState()
{
    NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "STARTING_SCRIPTED_PTFX", "");
    LogState(NetworkInterface::GetObjectManagerLog());

    m_StartPTFX = true;
    
}

void CNetworkPtFXWorldStateData::RevertWorldState()
{
    NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "STOPPING_SCRIPTED_PTFX", "");
    LogState(NetworkInterface::GetObjectManagerLog());

    // PTFX are script resources so get cleaned up automatically by the script
    // that created them - we still need to clean the PTFX up on remote machines though
    if(m_ScriptPTFX != 0)
    {
        if(!CTheScripts::GetScriptHandlerMgr().GetScriptResource(CGameScriptResource::SCRIPT_RESOURCE_PTFX, m_ScriptPTFX))
        {
            if(ptfxScript::DoesExist(m_ScriptPTFX))
            {
                // need to ensure the game heap is active here
	            sysMemAllocator *oldHeap = &sysMemAllocator::GetCurrent();
	            sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());

                ptfxScript::Destroy(m_ScriptPTFX);

#if GTA_REPLAY
				if(CReplayMgr::ShouldRecord())
				{
					CReplayMgr::RecordPersistantFx<CPacketDestroyScriptPtFx>(CPacketDestroyScriptPtFx(), CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)m_ScriptPTFX), NULL, false);
					CReplayMgr::StopTrackingFx(CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)m_ScriptPTFX));
				}
#endif

                sysMemAllocator::SetCurrent(*oldHeap);
            }

            if(m_PTFXRefAdded)
            {
                strLocalIndex slot = ptfxManager::GetAssetStore().FindSlotFromHashKey(m_PtFXAssetHash);

                if (gnetVerifyf(slot.IsValid(), "Cannot find particle asset slot (%d)!", m_PtFXAssetHash))
		        {
                    ptfxManager::GetAssetStore().RemoveRef(slot, REF_OTHER);
                }
            }
        }
		m_ScriptPTFX = 0;
    }
}

void CNetworkPtFXWorldStateData::UpdateHostState(CNetworkWorldStateData &hostWorldState)
{
    CNetworkPtFXWorldStateData *hostPtFXData = SafeCast(CNetworkPtFXWorldStateData, &hostWorldState);
    gnetDebug2("PTFXWorldState UniqueID: %d, Updating host script PTFX from ID %d to %d", GetUniqueID(), hostPtFXData->m_ScriptPTFX, m_ScriptPTFX);
    hostPtFXData->m_ScriptPTFX = m_ScriptPTFX;
}

void CNetworkPtFXWorldStateData::AddScriptedPtFX(const CGameScriptId &scriptID,
                                                 u32                  ptFXHash,
                                                 u32                  ptFXAssetHash,
                                                 const Vector3       &fxPos,
                                                 const Vector3       &fxRot,
                                                 float                scale,
                                                 u8                   invertAxes,
                                                 int                  scriptPTFX,
												 CEntity*			  pEntity,
												 int				  boneIndex,
												 float				  r,
												 float				  g,
												 float				  b,
												 bool                 terminateOnOwnerLeave,
												 u64                  ownerPeerID)
{
	bool isAWeapon = false;
	ObjectId entityId = NETWORK_INVALID_OBJECT_ID;
	if(pEntity)
	{
		netObject* pNetObject = NetworkUtils::GetNetworkObjectFromEntity(pEntity);
		if(pNetObject)
			entityId = pNetObject->GetObjectID();

		// In case effects is being attached to the ped's weapon
		if(!pNetObject && pEntity->GetAttachParent())
		{
			CEntity* parentEnt = SafeCast(CEntity, pEntity->GetAttachParent());
			if(parentEnt && parentEnt->GetIsTypePed())
			{
				CPed* parentPed = SafeCast(CPed, parentEnt);
				if(parentPed && parentPed->GetNetworkObject() && parentPed->GetWeaponManager()->GetEquippedWeaponObject() == pEntity)
				{
					entityId = parentPed->GetNetworkObject()->GetObjectID();
					isAWeapon = true;
				}
			}
		}
	}

	fwRandom::SetRandomSeed(fwTimer::GetSystemTimeInMilliseconds());
	int iteFindIUniqueID = 100;
	while ((--iteFindIUniqueID) != 0)
	{
		int uniqueId = fwRandom::GetRandomNumber();
		bool bDuplicateIDFound = false;

		CNetworkPtFXWorldStateData::Pool *pool = CNetworkPtFXWorldStateData::GetPool();
		s32  poolIndex = pool->GetSize();
		while (poolIndex--)
		{
			CNetworkPtFXWorldStateData *ptfxWorldState = pool->GetSlot(poolIndex);
			if (ptfxWorldState && uniqueId == ptfxWorldState->GetUniqueID())
			{
				bDuplicateIDFound = true;
				break;
			}
		}

		if (!bDuplicateIDFound)
		{
			gnetDebug3("CNetworkPtFXWorldStateData::AddScriptedPtFX with UniqueID: %d.", uniqueId);
			CNetworkPtFXWorldStateData worldStateData(scriptID, true, ptFXHash, ptFXAssetHash, fxPos, fxRot, scale, invertAxes, scriptPTFX, false, false, false, 0, 0, 0, entityId, boneIndex, 0, 0.0f, 0, 0.0f, isAWeapon, terminateOnOwnerLeave, ownerPeerID, uniqueId);
			worldStateData.SetColour(r, g, b);
			CNetworkScriptWorldStateMgr::ChangeWorldState(worldStateData, false);
			break;
		}
	}
	gnetAssertf(iteFindIUniqueID != 0, "This should never have happened. Potentially a bug with the random number generator.");
}

void CNetworkPtFXWorldStateData::ModifyScriptedEvolvePtFX(const CGameScriptId& scriptID, u32 evoHash, float evoVal, int scriptPTFX)
{
	// look up the PTFX world state from the PTFX ID
	CNetworkPtFXWorldStateData::Pool *pool = CNetworkPtFXWorldStateData::GetPool();
	s32  poolIndex = pool->GetSize();
	while (poolIndex--)
	{
		CNetworkPtFXWorldStateData *ptfxWorldState = pool->GetSlot(poolIndex);
		if (ptfxWorldState && ptfxWorldState->m_ScriptPTFX == scriptPTFX && gnetVerifyf(scriptID == ptfxWorldState->GetScriptID(), "Unexpected script ID!"))
		{
            if (ptfxWorldState->IsLocallyOwned()) 
            {
                bool bModified = ptfxWorldState->AddEvolve(evoHash, evoVal);
                if (bModified)
                {
                    gnetDebug3("CNetworkPtFXWorldStateData::ModifyScriptedEvolvePtFX --> uniqueID: %d, evoHash: %u, evoVal: %.2f, sciptPTFX: %d, poolSize:%d", ptfxWorldState->GetUniqueID(), evoHash, evoVal, scriptPTFX, poolIndex);
                    CModifyPtFXWorldStateDataScriptedEvolveEvent::Trigger(evoHash, evoVal, ptfxWorldState->GetUniqueID());
                }
            }
			else
			{
				gnetWarning("CNetworkPtFXWorldStateData::ModifyScriptedEvolvePtFX called on remote WorldState, dropping the command. This won't work! This should only be called from the script host, even if the ptfx was created locally. UniqueID: %d, evoHash: %u, evoVal: %.2f, sciptPTFX: %d, poolSize:%d", ptfxWorldState->GetUniqueID(), evoHash, evoVal, scriptPTFX, poolIndex);
			}
			break;
		}
	}
}

void CNetworkPtFXWorldStateData::ApplyScriptedEvolvePtFXFromNetwork(int worldStateUniqueID, u32 evoHash, float evoVal)
{
	CNetworkPtFXWorldStateData::Pool *pool = CNetworkPtFXWorldStateData::GetPool();
	s32  poolIndex = pool->GetSize();

	while (poolIndex--)
	{
		CNetworkPtFXWorldStateData *ptfxWorldState = pool->GetSlot(poolIndex);
		if (ptfxWorldState && ptfxWorldState->m_uniqueID == worldStateUniqueID)
		{
			gnetDebug3("CNetworkPtFXWorldStateData::ApplyScriptedEvolvePtFXFromNetwork --> uniqueID: %d, evoHash: %u, evoVal: %.2f, poolSize:%d", worldStateUniqueID, evoHash, evoVal, poolIndex);
			if (ptfxWorldState->AddEvolve(evoHash, evoVal) && ptfxWorldState->m_ScriptPTFX != 0)
			{
				ptfxScript::UpdateEvo(ptfxWorldState->m_ScriptPTFX, evoHash, evoVal);
			}
			break;
		}
	}
}

void CNetworkPtFXWorldStateData::ModifyScriptedPtFXColour(const CGameScriptId &scriptID, float r, float g, float b, int scriptPTFX)
{
	// look up the PTFX world state from the PTFX ID
	CNetworkPtFXWorldStateData::Pool *pool = CNetworkPtFXWorldStateData::GetPool();

	bool foundPTFX = false;
	s32  poolIndex = pool->GetSize();

	while(poolIndex-- && !foundPTFX)
	{
		CNetworkPtFXWorldStateData *ptfxWorldState = pool->GetSlot(poolIndex);

		if(ptfxWorldState && ptfxWorldState->m_ScriptPTFX == scriptPTFX && gnetVerifyf(scriptID == ptfxWorldState->GetScriptID(), "Unexpected script ID!"))
		{
			ptfxWorldState->SetColour(r,g,b);

			foundPTFX = true;
		}
	}
}

void CNetworkPtFXWorldStateData::RemoveScriptedPtFX(const CGameScriptId &scriptID,
                                                    int                  scriptPTFX)
{
    // look up the PTFX world state from the PTFX ID
    CNetworkPtFXWorldStateData::Pool *pool = CNetworkPtFXWorldStateData::GetPool();

    bool foundPTFX = false;
    s32  poolIndex = pool->GetSize();

    while(poolIndex-- && !foundPTFX)
    {
        CNetworkPtFXWorldStateData *ptfxWorldState = pool->GetSlot(poolIndex);

        if(ptfxWorldState && ptfxWorldState->m_ScriptPTFX == scriptPTFX && gnetVerifyf(scriptID == ptfxWorldState->GetScriptID(), "Unexpected script ID!"))
        {
            gnetDebug2("PTFXWorldState UniqueID: %d, Reverting PTFX with scriptPTFX: %d", ptfxWorldState->GetUniqueID(), scriptPTFX);
            CNetworkScriptWorldStateMgr::RevertWorldState(*ptfxWorldState, false);
            foundPTFX = true;
        }
    }

#if !__NO_OUTPUT
    if(!foundPTFX)
    {
        gnetDebug2("Failed to remove PTFX with scriptPTFX: %d", scriptPTFX);
        gnetDebug2("Dump PTFXWorldStates:");
        poolIndex = pool->GetSize();
        while (poolIndex--)
        {
            CNetworkPtFXWorldStateData *ptfxWorldState = pool->GetSlot(poolIndex);
            if (ptfxWorldState)
            {
                gnetDebug2("    - dump PTFXWorldState: UniqueID: %d, scriptPTFX: %d", ptfxWorldState->GetUniqueID(), ptfxWorldState->m_ScriptPTFX);
            }
        }
    }
#endif //!__NO_OUTPUT 
}

void CNetworkPtFXWorldStateData::LogState(netLoggingInterface &log) const
{
    log.WriteDataValue("Script Name", "%s", GetScriptID().GetLogName());
	log.WriteDataValue("Unique ID", "%d", GetUniqueID());
    log.WriteDataValue("ScriptPTFX (local)", "%d", m_ScriptPTFX);
    log.WriteDataValue("PTFX Hash", "%u", m_PtFXHash);
    log.WriteDataValue("PTFX Asset Hash", "%d", m_PtFXAssetHash);
	log.WriteDataValue("Using Entity", "%s", m_bUseEntity?"TRUE":"FALSE");
	log.WriteDataValue("Entity ID", "%d", m_bUseEntity ? m_EntityID: -1);	
    log.WriteDataValue("Position", "%.2f, %.2f, %.2f", m_FxPos.x, m_FxPos.y, m_FxPos.z);
    log.WriteDataValue("Rotation", "%.2f, %.2f, %.2f", m_FxRot.x, m_FxRot.y, m_FxRot.z);
    log.WriteDataValue("Scale", "%.2f", m_Scale);
    log.WriteDataValue("Invert Axis Flags", "%d", m_InvertAxes);
	log.WriteDataValue("Bone Index", "%d", m_boneIndex);
	log.WriteDataValue("Evolve hash A", "%u", m_evoHashA);
	log.WriteDataValue("Evolve Value A", "%.2f", m_evoValA);
	log.WriteDataValue("Evolve hash B", "%u", m_evoHashB);
	log.WriteDataValue("Evolve Value B", "%.2f", m_evoValB);
	log.WriteDataValue("Attach To Weapon", "%s", m_bAttachedToWeapon ? "TRUE":"FALSE");
	log.WriteDataValue("Has Color", "%s", m_hasColour ? "TRUE":"FALSE");
	log.WriteDataValue("Terminate on Owner Leave", "%s", m_bTerminateOnOwnerLeave ? "TRUE":"FALSE");
	if(m_bTerminateOnOwnerLeave)
	{
		log.WriteDataValue("Owner PeerID", "%u", m_ownerPeerID);
	}
	if(m_hasColour)
	{
		log.WriteDataValue("Red", "%d", (int)m_r);
		log.WriteDataValue("Blue", "%d", (int)m_b);
		log.WriteDataValue("Green", "%d", (int)m_g);
	}
}

void CNetworkPtFXWorldStateData::Serialise(CSyncDataReader &serialiser)
{
    SerialiseWorldState(serialiser);
}

void CNetworkPtFXWorldStateData::Serialise(CSyncDataWriter &serialiser)
{
    SerialiseWorldState(serialiser);
}

template <class Serialiser> void CNetworkPtFXWorldStateData::SerialiseWorldState(Serialiser &serialiser)
{
	const unsigned SIZEOF_UNIQUE_ID			= 32;
    const unsigned SIZEOF_PTFX_HASH         = 32;
    const unsigned SIZEOF_PTFX_ASSET_HASH   = 32;
	const unsigned SIZEOF_BONE_INDEX		= 32;
    const unsigned SIZEOF_PTFX_SCALE        = 10;
    const unsigned SIZEOF_INVERT_AXES_FLAGS = 3;  // 1 bit for each axis inverted
	const unsigned SIZEOF_OFFSET_POSITION	= 19;
	const float MAX_SIZE_OFFSET_POSITION	= 100.0f;

    const float    MAX_PTFX_SCALE = 10.0f;

    GetScriptID().Serialise(serialiser);
	SERIALISE_UNSIGNED(serialiser, m_uniqueID,		SIZEOF_UNIQUE_ID, "Unique ID");
    SERIALISE_UNSIGNED(serialiser, m_PtFXHash,      SIZEOF_PTFX_HASH, "PTFX Hash");
    SERIALISE_UNSIGNED(serialiser, m_PtFXAssetHash, SIZEOF_PTFX_ASSET_HASH, "PTFX Asset Hash");
    SERIALISE_POSITION(serialiser, m_FxRot, "Rotation"); // Rotation stored in Vector3 so can be synced the same way as a position
    SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_Scale, MAX_PTFX_SCALE, SIZEOF_PTFX_SCALE, "Scale");
    SERIALISE_UNSIGNED(serialiser, m_InvertAxes, SIZEOF_INVERT_AXES_FLAGS, "Invert Axes Flags");

	SERIALISE_BOOL(serialiser, m_bUseEntity, "Using Entity");
	if(m_bUseEntity || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_OBJECTID(serialiser, m_EntityID, "Entity");
		SERIALISE_VECTOR(serialiser, m_FxPos, MAX_SIZE_OFFSET_POSITION, SIZEOF_OFFSET_POSITION, "Offset Position");
	}
	else
	{
		m_EntityID = NETWORK_INVALID_OBJECT_ID;
		SERIALISE_POSITION(serialiser, m_FxPos, "Position");
	}

	SERIALISE_BOOL(serialiser, m_hasColour);
	if (m_hasColour || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_r, 8, "red");
		SERIALISE_UNSIGNED(serialiser, m_g, 8, "green");
		SERIALISE_UNSIGNED(serialiser, m_b, 8, "blue");		
	}

	bool bHasBoneIndex = (m_boneIndex != -1);
	SERIALISE_BOOL(serialiser, bHasBoneIndex);
	if (bHasBoneIndex || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_INTEGER(serialiser, m_boneIndex, SIZEOF_BONE_INDEX, "Bone Index");
	}
	else
	{
		m_boneIndex = -1;
	}

	bool bHasEvoIDA = (m_evoHashA != 0);
	SERIALISE_BOOL(serialiser, bHasEvoIDA);
	if (bHasEvoIDA || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_evoHashA, SIZEOF_PTFX_HASH, "Evolve hash A");
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_evoValA, MAX_PTFX_SCALE, SIZEOF_PTFX_SCALE, "Evolve Value A");
	}
	else
	{
		m_evoHashA = 0;
		m_evoValA = 0.f;
	}

	bool bHasEvoIDB = (m_evoHashB != 0);
	SERIALISE_BOOL(serialiser, bHasEvoIDB);
	if (bHasEvoIDB || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_evoHashB, SIZEOF_PTFX_HASH, "Evolve hash B");
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_evoValB, MAX_PTFX_SCALE, SIZEOF_PTFX_SCALE, "Evolve Value B");
	}
	else
	{
		m_evoHashB = 0;
		m_evoValB = 0.f;
	}

	SERIALISE_BOOL(serialiser, m_bAttachedToWeapon, "Attach To Weapon");
	SERIALISE_BOOL(serialiser, m_bTerminateOnOwnerLeave, "Terminate on Owner Leave");

	if(m_bTerminateOnOwnerLeave)
	{
		SERIALISE_UNSIGNED(serialiser, m_ownerPeerID, 64);
	}
	else
	{
		m_ownerPeerID = 0;
	}
}

#if __DEV

bool CNetworkPtFXWorldStateData::IsConflicting(const CNetworkWorldStateData &worldState) const
{
    bool conflicting = IsEqualTo(worldState);
    return conflicting;
}

#endif // __DEV

#if __BANK

void CNetworkPtFXWorldStateData::AddDebugWidgets()
{
    bkBank *bank = BANKMGR.FindBank("Network");

    if(gnetVerifyf(bank, "Unable to find network bank!"))
    {
        bank->PushGroup("Scripted PTFX", false);
        {
        }
        bank->PopGroup();
    }
}

void CNetworkPtFXWorldStateData::DisplayDebugText()
{
    CNetworkPtFXWorldStateData::Pool *pool = CNetworkPtFXWorldStateData::GetPool();

	s32 i = pool->GetSize();

	while(i--)
	{
		CNetworkPtFXWorldStateData *worldState = pool->GetSlot(i);

		if(worldState)
		{
            grcDebugDraw::AddDebugOutput("Scripted PtFX: %s PTFX:%d (%d) (EntityID: %d), (Pos:%.2f, %.2f, %.2f),(Rot:%.2f, %.2f, %.2f), (Scale:%.2f), IAF:%d, BoneIndex:%d, UniqueID:%d",
                                         worldState->GetScriptID().GetLogName(),
                                         worldState->m_PtFXHash,
                                         worldState->m_PtFXAssetHash,
										 worldState->m_EntityID,
                                         worldState->m_FxPos.x, worldState->m_FxPos.y, worldState->m_FxPos.z,
                                         worldState->m_FxRot.x, worldState->m_FxRot.y, worldState->m_FxRot.z,
                                         worldState->m_Scale,
                                         worldState->m_InvertAxes,
										 worldState->m_boneIndex,
										 worldState->GetUniqueID());
        }
    }
}
#endif // __BANK
