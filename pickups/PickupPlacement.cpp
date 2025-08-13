// File header
#include "event/EventGroup.h"
#include "event/EventNetwork.h"
#include "frontend/MiniMap.h"
#include "Network/Objects/Entities/NetObjPickupPlacement.h"
#include "fwnet/NetGameObjectBase.h"
#include "network/Events/NetworkEventTypes.h"
#include "Network/Objects/Entities/NetObjGame.h"
#include "Network/Objects/NetworkObjectMgr.h"
#include "peds/PlayerInfo.h"
#include "peds/Ped.h"
#include "pickups/Data/PickupDataManager.h"
#include "pickups/Pickup.h"
#include "pickups/PickupManager.h"
#include "pickups/PickupPlacement.h"
#include "script/script.h"
#include "script/script_channel.h"
#include "script/Handlers/GameScriptHandlerNetwork.h"

WEAPON_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

float	CPickupPlacement::ms_customBlipScale		= 0.0f;
int		CPickupPlacement::ms_customBlipDisplay		= 0;
int		CPickupPlacement::ms_customBlipPriority		= 0;
int		CPickupPlacement::ms_customBlipSprite		= 0;
int		CPickupPlacement::ms_customBlipColour		= 0;

float	CPickupPlacement::ms_generationRangeMultiplier = 1.f;

FW_INSTANTIATE_CLASS_POOL(CPickupPlacement, CONFIGURED_FROM_FILE, atHashString("CPickupPlacement",0xef501806));
FW_INSTANTIATE_CLASS_POOL(CPickupPlacement::CPickupPlacementCustomScriptData, CONFIGURED_FROM_FILE, atHashString("CPickupPlacementCustomScriptData",0xb1786625));

const float CPickupPlacement::DEFAULT_GLOW_OFFSET = 0.5f;

CPickupPlacement::CPickupPlacement(u32 pickupHash, Vector3& position, Vector3& orientation, PlacementFlags flags, u32 customModelIndex) 
: m_pickupPosition(position)
, m_pickupOrientation(orientation)
, m_roomHashkey(0)
, m_pPickup(NULL)
, m_pPickupData(CPickupDataManager::GetPickupData(pickupHash))
, m_pHandler(NULL)
, m_pCustomScriptData(NULL)
, m_flags(flags)
, m_customRegenTime(0)
, m_amount(0)
, m_amountCollected(0)
, m_customWeaponHash(0)
{
	Assertf(!(GetHasComplexBlip() && GetHasSimpleBlip()), "Trying to create a placement with simple & complex blip!");
	
	// create a blip if necessary (the collected flag can be set if this is being created as a clone)
	if (!(flags & PLACEMENT_INTERNAL_FLAG_COLLECTED))
	{
		CreateBlip();
	}

	// map pickups must always be fixed
	if (m_flags & PLACEMENT_CREATION_FLAG_MAP)
	{
		m_flags |= PLACEMENT_CREATION_FLAG_FIXED;
	}

	if (customModelIndex != fwModelId::MI_INVALID && GetOrCreateCustomScriptData())
	{
		 m_pCustomScriptData->m_customModelIndex = customModelIndex; 
	}
}

CPickupPlacement::~CPickupPlacement()
{
	Assert(!GetNetworkObject());

	if (m_pPickup)
	{
		m_pPickup->SetCreatedBlip(false);
		// can't destroy cloned pickups, they will be removed by the owner
		if (!m_pPickup->IsNetworkClone())
		{
			DestroyPickup();
		}
		else
		{
			m_pPickup->SetPlacement(NULL);
			m_pPickup = NULL;
		}
	}

	RemoveBlip();

	if (m_pCustomScriptData)
	{
		delete m_pCustomScriptData;
		m_pCustomScriptData = NULL;
	}
}

u32	CPickupPlacement::GetPickupHash() const
{ 
	Assert(m_pPickupData); 
	return m_pPickupData ? m_pPickupData->GetHash() : 0;
}

void CPickupPlacement::SetPickup(CPickup* pPickup)	
{ 
	Assert(!m_pPickup || !pPickup); 
	m_pPickup = pPickup; 
}

float CPickupPlacement::GetGenerationRange() const
{
	return m_pPickupData->GetGenerationRange() * ms_generationRangeMultiplier;
}

void CPickupPlacement::SetAmount(u16 amount)
{ 
	m_amount = amount; 
	
	if (GetPickup()) 
		GetPickup()->SetAmount(amount); 
}

void CPickupPlacement::SetPickupHasBeenDestroyed(bool bLocallyDestroyed)
{
	if (!GetHasPickupBeenDestroyed())
	{
		m_flags |= PLACEMENT_INTERNAL_FLAG_PICKUP_DESTROYED; 

		CPickupManager::RegisterPickupDestruction(this);
	
		if (NetworkInterface::IsGameInProgress())
		{
			netObject* pNetObj = GetNetworkObject();

			if (pNetObj && !pNetObj->IsClone() && !pNetObj->IsPendingOwnerChange())
			{
				// force an immediate send of the game state data so that any explosions are triggered a.s.a.p
				pNetObj->GetSyncTree()->Update(pNetObj, pNetObj->GetActivationFlags(), netInterface::GetSynchronisationTime());
				pNetObj->GetSyncTree()->ForceSendOfNodeData(SERIALISEMODE_FORCE_SEND_OF_DIRTY, pNetObj->GetActivationFlags(), pNetObj, *SafeCast(CPickupPlacementSyncTree, pNetObj->GetSyncTree())->GetStateNode());
			}	
			else
			{
				// send an event informing other machines the pickup is destroyed
				if (bLocallyDestroyed && !GetLocalOnly())
				{
					CPickupDestroyedEvent::Trigger(*this);
				}
			}

			if (m_pPickup && !bLocallyDestroyed)
			{
				m_pPickup->AddExplosion();

				if (m_pPickup)
				{
					DestroyPickup();
				}
			}
		}
	}
}

void CPickupPlacement::SetUncollectable(bool b)			
{ 
	if (b) 
	{
		m_flags |= PLACEMENT_INTERNAL_FLAG_UNCOLLECTABLE; 
	}
	else 
	{
		m_flags &= ~PLACEMENT_INTERNAL_FLAG_UNCOLLECTABLE; 
	}

	if (m_pPickup)
	{
		m_pPickup->SetProhibitLocalCollection(b);
	}
}

void CPickupPlacement::SetTransparentWhenUncollectable(bool b)			
{ 
	if (b) 
	{
		m_flags |= PLACEMENT_INTERNAL_FLAG_FADE_WHEN_UNCOLLECTABLE; 
	}
	else 
	{
		m_flags &= ~PLACEMENT_INTERNAL_FLAG_FADE_WHEN_UNCOLLECTABLE; 
	}

	if (m_pPickup)
	{
		m_pPickup->SetTransparentWhenUncollectable(b);
	}
}

void CPickupPlacement::SetHiddenWhenUncollectable(bool b)				
{ 
	if (b) 
	{
		m_flags |= PLACEMENT_INTERNAL_FLAG_HIDE_WHEN_UNCOLLECTABLE; 

		if (m_pPickup && !CanCollectScript())
		{
			DestroyPickup();
		}
	}
	else 
	{
		m_flags &= ~PLACEMENT_INTERNAL_FLAG_HIDE_WHEN_UNCOLLECTABLE; 

		if (GetInScope() && !GetIsCollected())
		{
			CreatePickup();
		}
	}
}

u32	CPickupPlacement::GetCustomModelHash() const
{
	u32 modelHash = 0;

	if (m_pCustomScriptData && m_pCustomScriptData->m_customModelIndex != fwModelId::MI_INVALID)
	{
		CBaseModelInfo * pModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(m_pCustomScriptData->m_customModelIndex));

		if (AssertVerify(pModelInfo))
		{
			modelHash = pModelInfo->GetModelNameHash();
		}
	}

	return modelHash;
}

void CPickupPlacement::SetRegenerationTime(u32 regenTime)				
{ 
	m_customRegenTime = (u16)(regenTime/1000); // store as seconds
	m_flags |= PLACEMENT_CREATION_FLAG_REGENERATES; 
}

u32	CPickupPlacement::GetRegenerationTime()
{
	u32 regenTime = 0;

	Assert(GetRegenerates());

	if (m_pCustomScriptData && m_customRegenTime != 0)
	{
		regenTime = static_cast<u32>(m_customRegenTime)*1000; // return time in millisecs
	}
	else
	{
		const CPickupData* pPickupData = CPickupDataManager::GetPickupData(GetPickupHash());

		if (AssertVerify(pPickupData))
		{
			regenTime = static_cast<u32>(pPickupData->GetRegenerationTime()) * 1000; // return time in millisecs
		}
	}

	return regenTime; 
}

void CPickupPlacement::SetTeamPermits(TeamFlags teams)							
{ 
	if (m_pCustomScriptData || teams != 0)
	{
		if (GetOrCreateCustomScriptData())
		{
			m_pCustomScriptData->m_teamPermits = teams; 

			if (m_pPickup)
			{
				m_pPickup->SetTeamPermits(m_pCustomScriptData->m_teamPermits);
			}
		}
	}
}

void CPickupPlacement::AddTeamPermit(u8 team)							
{ 
	if (GetOrCreateCustomScriptData())
	{
		m_pCustomScriptData->m_teamPermits |= (1<<team); 

		if (m_pPickup)
		{
			m_pPickup->SetTeamPermits(m_pCustomScriptData->m_teamPermits);
		}
	}
}

bool CPickupPlacement::GeneratesNetworkedPickups() const
{
	/*
	// no point in networking fixed pickups as they can't move
	if (GetNetworkObject() && !(GetFlags() & PLACEMENT_CREATION_FLAG_FIXED))
	{
		Assert (!(GetFlags() & PLACEMENT_CREATION_FLAG_LOCAL_ONLY));
		return true;
	}*/

	return false;
}

void CPickupPlacement::SetPickupGlowOffset(float f)						
{ 
	if (m_pCustomScriptData || f != DEFAULT_GLOW_OFFSET)
	{
		if (GetOrCreateCustomScriptData())
		{
			m_pCustomScriptData->m_pickupGlowOffset = f; 
		}
	}
}
/*
void CPickupPlacement::SetPlayersAvailability(PlayerFlags players)
{
	if (AssertVerify(GetIsMapPlacement()) && GetOrCreateCustomScriptData())
	{
		m_pCustomScriptData->m_CloneToPlayersList = players;

		if (GetNetworkObject())
		{
			SafeCast(CNetObjPickupPlacement, GetNetworkObject())->SetPlayersAvailability(players);
		}
	}
}
*/

void CPickupPlacement::Update()
{
	// unregister map placements that have regenerated and synced with all other players in scope
	if (GetNetworkObject() && 
		GetIsMapPlacement() && 
		!GetIsCollected() && 
		!GetNetworkObject()->IsClone() && 
		!GetNetworkObject()->IsPendingOwnerChange())
	{
		if (GetNetworkObject()->IsSyncedWithAllPlayers())
		{
			NetworkInterface::GetObjectManager().UnregisterNetworkObject(GetNetworkObject(), netObjectMgrBase::GAME_UNREGISTRATION);
		}
	}

	// if the pickup has fallen through the map destroy and recreate it fixed
	if (GetPickup())
	{
		Vector3 pickupPos = VEC3V_TO_VECTOR3(GetPickup()->GetTransform().GetPosition());

		if (pickupPos.z < m_pickupPosition.z - 1000.0f)
		{
#if ENABLE_NETWORK_LOGGING
			netLoggingInterface *log = &NetworkInterface::GetObjectManager().GetLog();
			
			if (log && GetNetworkObject())
			{
				NetworkLogUtils::WriteLogEvent(*log, "RECREATING FALLEN PICKUP", "%s", GetNetworkObject()->GetLogName());

				if (GetPickup()->GetNetworkObject())
				{
					log->WriteDataValue("Old pickup", "%s", GetPickup()->GetNetworkObject()->GetLogName());
				}
			}
#endif // ENABLE_NETWORK_LOGGING

			DestroyPickup();

			m_flags |= PLACEMENT_CREATION_FLAG_FIXED;

			CreatePickup();

#if ENABLE_NETWORK_LOGGING
			if (log && GetNetworkObject() && GetPickup())
			{
				log->WriteDataValue("New pickup", "%s", GetPickup()->GetNetworkObject()->GetLogName());
			}
#endif // ENABLE_NETWORK_LOGGING
		}
	}
}

bool CPickupPlacement::GetInScope(CPed* pPlayerPed) const
{
	float generationRange = GetGenerationRange();

	Assertf(generationRange > 0.0f, "Generation range is not set for pickup of type %s - the pickups will not appear", m_pPickupData->GetName());

	if (pPlayerPed)
	{
		return GetInScope(*pPlayerPed, m_pickupPosition, generationRange);
	}

	// networked pickups are always in scope
	if (GetNetworkObject() && GeneratesNetworkedPickups()) 
	{
		return true;
	}

	if (FindPlayerPed())
	{
		// use static GetInScope()
		return GetInScope(*FindPlayerPed(), m_pickupPosition, generationRange);
	}

	return false;
}

CPickup* CPickupPlacement::CreatePickup(CNetObjPickup* pNetObj)
{
	CPickup* pPickup = NULL;

	bool bRegisterAsNetworkObject = GeneratesNetworkedPickups();

	if (GetHiddenWhenUncollectable())
	{
		if (!CanCollectScript())
		{
			return NULL;
		}
	}

	if (AssertVerify(!GetPickup()) && AssertVerify(!GetIsCollected()) && AssertVerify(pNetObj || !GetNetworkObject() || !bRegisterAsNetworkObject || !GetNetworkObject()->IsClone()))
	{
		Matrix34 placementM;
		placementM.FromEulersXYZ(m_pickupOrientation);
		placementM.d = m_pickupPosition;

		pPickup = CPickupManager::CreatePickup(GetPickupHash(), placementM, pNetObj, bRegisterAsNetworkObject, m_pCustomScriptData ? m_pCustomScriptData->m_customModelIndex.Get() : fwModelId::MI_INVALID);

		if (pPickup)
		{
			// Should we attempt to snap this to the ground?
			if( GetShouldSnapToGround() )
			{
				pPickup->SetPlaceOnGround(GetShouldOrientToGround(), GetShouldBeUpright());
			}

			// placements are always generated by script commands, so the pickups are mission objects
			pPickup->SetOwnedBy( ENTITY_OWNEDBY_SCRIPT );

			SetPickup(pPickup);
			pPickup->SetPlacement(this);

			if (m_pCustomScriptData)
			{
				if (m_pCustomScriptData->m_teamPermits != 0)
				{
					pPickup->SetTeamPermits(m_pCustomScriptData->m_teamPermits);
				}

				if (m_pCustomScriptData->m_pickupGlowOffset != 0.0f || !( GetShouldSnapToGround() || GetShouldOrientToGround() ))
				{
					pPickup->SetGlowOffset(m_pCustomScriptData->m_pickupGlowOffset);
					pPickup->SetOffsetGlow(true);
				}
			}

			if(m_customWeaponHash != 0)
			{
				pPickup->SetWeaponHash(m_customWeaponHash);
			}

			if (m_amount > 0)
			{
				pPickup->SetAmount(m_amount);
			}

			if ((m_flags & PLACEMENT_CREATION_FLAG_HIDE_IN_PHOTOS) != 0)
			{
				pPickup->SetFlag(CPickup::PF_HideInPhotos);
			}

			if ((m_flags & PLACEMENT_CREATION_FLAG_ON_OBJECT) != 0)
			{
				pPickup->SetLyingOnFixedObject();
			}

			if ((m_flags & PLACEMENT_CREATION_FLAG_COLLECTABLE_IN_VEHICLE) != 0)
			{
				pPickup->SetFlag(CPickup::PF_AllowCollectionInVehicle);
			}

			if (GetGlowWhenInSameTeam())
			{
				pPickup->SetFlag(CPickup::PF_GlowInSameTeam);
			}

			if (GetTransparentWhenUncollectable())
			{
				pPickup->SetTransparentWhenUncollectable(true);
			}

			if (GetUncollectable())
			{
				pPickup->SetProhibitLocalCollection(true);
			}

			if ((m_flags & PLACEMENT_CREATION_FLAG_AUTO_EQUIP) != 0)
			{
				pPickup->SetFlag(CPickup::PF_AutoEquipOnCollect);
			}

			if (bRegisterAsNetworkObject)
			{
				// if this is a map placement, register it as a network object when it first creates a pickup.
				if (NetworkInterface::IsGameInProgress())
				{
					Assert(pPickup->GetNetworkObject());

					if (!GetNetworkObject())
					{
						// only map placements should have no network object at this point
						Assertf(GetIsMapPlacement(), "The pickup placement at %f, %f, %f has no network object!", m_pickupPosition.x, m_pickupPosition.y, m_pickupPosition.z);

						// placements are always created by scripts so they automatically become script network objects 
						NetworkInterface::GetObjectManager().RegisterGameObject(this, 0, CNetObjGame::GLOBALFLAG_SCRIPTOBJECT|CNetObjGame::GLOBALFLAG_SCRIPT_MIGRATION|CNetObjGame::GLOBALFLAG_CLONEONLY_SCRIPT);

						Assert(GetNetworkObject());
					}
					else if (pPickup->GetNetworkObject())
					{
						Assert(pPickup->GetNetworkObject()->GetPlayerOwner() == GetNetworkObject()->GetPlayerOwner());
					}
				}
				else
				{
					Assert(!NetworkInterface::IsNetworkOpen());
				}
			}
		}
	}

	return pPickup;
}

void CPickupPlacement::Collect(CPed* pPedWhoGotIt)
{
	if (AssertVerify(!GetIsCollected()))
	{
		if (GetIsMapPlacement() && !GetNetworkObject() && NetworkInterface::IsGameInProgress() && (m_flags & PLACEMENT_CREATION_FLAG_LOCAL_ONLY)==0)
		{
			// register the placement once collected
			NetworkInterface::GetObjectManager().RegisterGameObject(this, 0, CNetObjGame::GLOBALFLAG_SCRIPTOBJECT|CNetObjGame::GLOBALFLAG_SCRIPT_MIGRATION|CNetObjGame::GLOBALFLAG_CLONEONLY_SCRIPT);
		}

		m_flags |= PLACEMENT_INTERNAL_FLAG_COLLECTED;

		Assert(GetPickup() || (GetNetworkObject() && GetNetworkObject()->IsClone()));

		if (GetPickup())
		{
			GetPickup()->SetCreatedBlip(false);
			if (GetPickup()->GetNetworkObject() && IsNetworkClone())
			{
				GetPickup()->SetCollected();
			}
			else
			{
				DestroyPickup();
			}
		}

		RemoveBlip();

		// inform network object about who collected it (to save having to search collection history every time)
		if (GetNetworkObject() && pPedWhoGotIt)
		{
			static_cast<CNetObjPickupPlacement*>(GetNetworkObject())->SetCollector(pPedWhoGotIt);
		}
	}
}

bool CPickupPlacement::Regenerate()
{
	if (AssertVerify(GetIsCollected() || GetHasPickupBeenDestroyed()) && Verifyf(!GetPickup(), "Regenerating placement %s at %0.2f, %0.2f, %0.2f already has a pickup", GetNetworkObject() ? GetNetworkObject()->GetLogName() : "??", m_pickupPosition.x, m_pickupPosition.y, m_pickupPosition.z))
	{
		netObject* pNetObj = GetNetworkObject();

		// migrating placements can't regenerate
		if (pNetObj && pNetObj->IsPendingOwnerChange())
		{
			return false;
		}

		// clear collected flag to prevent assert in CreatePickup()
		m_flags &= ~PLACEMENT_INTERNAL_FLAG_COLLECTED;
		m_flags &= ~PLACEMENT_INTERNAL_FLAG_PICKUP_DESTROYED;

		if (GetInScope() && (!GeneratesNetworkedPickups() || !(GetNetworkObject() && GetNetworkObject()->IsClone())) && !CreatePickup())
		{
			// the placement can fail to regenerate a pickup if the model for the pickup is not streamed in yet
			m_flags |= PLACEMENT_INTERNAL_FLAG_COLLECTED;

			return false;
		}

		CreateBlip();

		// inform network object to clear the collector
		if (GetNetworkObject())
		{
			static_cast<CNetObjPickupPlacement*>(GetNetworkObject())->SetCollector(NULL);

			if (GetIsMapPlacement() && !GetNetworkObject()->IsClone())
			{
				// dirty the state node here so that it will definitely be sent out before the network object is removed for the pickup
				CPickupPlacementSyncTree* pPlacementTree = static_cast<CPickupPlacementSyncTree*>(GetNetworkObject()->GetSyncTree());

				pPlacementTree->DirtyNode(GetNetworkObject(), *pPlacementTree->GetStateNode());
			}
		}

		if (GetScriptInfo() && GetPickup())
		{
			u32 modelHash = 0;

			CBaseModelInfo *modelInfo = GetPickup()->GetBaseModelInfo();
			if(AssertVerify(modelInfo))
			{
				modelHash = modelInfo->GetHashKey();
			}

			CEventNetworkPickupRespawned pickupEvent(GetScriptInfo()->GetObjectId(), GetPickupHash(), modelHash);
			GetEventScriptNetworkGroup()->Add(pickupEvent);
		}

		CPickupManager::RemovePickupPlacementFromCollectionHistory(this);
	}

	return true;
}

void CPickupPlacement::DestroyPickup()
{
	if (AssertVerify(m_pPickup))
	{
		m_pPickup->Destroy();
		m_pPickup = NULL;
	}
}

s32 CPickupPlacement::CreateBlip()
{
	if ((GetHasComplexBlip() || GetHasSimpleBlip()) &&
		AssertVerify(!m_pCustomScriptData || m_pCustomScriptData->m_radarBlip == INVALID_BLIP_ID) && 
		Verifyf(!(m_flags & PLACEMENT_INTERNAL_FLAG_COLLECTED), "Trying to create a blip for a collected pickup") &&
		GetOrCreateCustomScriptData())
	{
		if (GetHasComplexBlip())
		{
			CEntityPoolIndexForBlip poolIndexForPlacement(this);
#if __DEV
			m_pCustomScriptData->m_radarBlip = CMiniMap::CreateBlip(true, BLIP_TYPE_PICKUP, poolIndexForPlacement, BLIP_DISPLAY_BOTH, CTheScripts::GetCurrentScriptName());
#else
			m_pCustomScriptData->m_radarBlip = CMiniMap::CreateBlip(true, BLIP_TYPE_PICKUP, poolIndexForPlacement, BLIP_DISPLAY_BOTH, NULL);
#endif

			if (ms_customBlipSprite != 0)
				CMiniMap::SetBlipParameter(BLIP_CHANGE_SPRITE, m_pCustomScriptData->m_radarBlip, ms_customBlipSprite);

			if (ms_customBlipScale > 0.0f)
				CMiniMap::SetBlipParameter(BLIP_CHANGE_SCALE, m_pCustomScriptData->m_radarBlip, ms_customBlipScale);

			if (ms_customBlipDisplay != 0)
				CMiniMap::SetBlipParameter(BLIP_CHANGE_DISPLAY, m_pCustomScriptData->m_radarBlip, ms_customBlipDisplay);

			if (ms_customBlipColour != 0)
				CMiniMap::SetBlipParameter(BLIP_CHANGE_COLOUR, m_pCustomScriptData->m_radarBlip, ms_customBlipColour);

			if (ms_customBlipPriority != 0)
				CMiniMap::SetBlipParameter(BLIP_CHANGE_PRIORITY, m_pCustomScriptData->m_radarBlip, ms_customBlipPriority);
		}
		else if (GetHasSimpleBlip())
		{
#if __DEV
			m_pCustomScriptData->m_radarBlip = CMiniMap::CreateSimpleBlip(GetPickupPosition(), "SimplePickup");
#else
			m_pCustomScriptData->m_radarBlip = CMiniMap::CreateSimpleBlip(GetPickupPosition(), NULL);
#endif
		}
		
		return m_pCustomScriptData->m_radarBlip;
	}

	return INVALID_BLIP_ID;
}


void CPickupPlacement::RemoveBlip()
{
	if (m_pCustomScriptData && m_pCustomScriptData->m_radarBlip != INVALID_BLIP_ID)
	{
		CMiniMapBlip *pBlip = CMiniMap::GetBlip(m_pCustomScriptData->m_radarBlip);

		if (pBlip)
		{
			CMiniMap::RemoveBlip(pBlip);
		}
		
		m_pCustomScriptData->m_radarBlip = INVALID_BLIP_ID;
	}
}

bool CPickupPlacement::CanCollectScript() const
{
	if (GetUncollectable())
	{
		return false;
	}

	CNetGamePlayer* pLocalPlayer = NetworkInterface::GetLocalPlayer();

	if (pLocalPlayer)
	{
		if (m_pCustomScriptData && m_pCustomScriptData->m_teamPermits != 0)
		{
			if (pLocalPlayer->GetTeam() == INVALID_TEAM || !(m_pCustomScriptData->m_teamPermits & (1<<pLocalPlayer->GetTeam())))
			{
				return false;
			}
		}

		// not sure if these checks are needed, disable for now
		/*
		// certain players can be prohibited from collecting certain types of pickup in MP
		PhysicalPlayerIndex playerIndex = pLocalPlayer->GetPhysicalPlayerIndex();

		if (playerIndex != INVALID_PLAYER_INDEX && CPickupManager::IsCollectionProhibited(GetPickupHash(), playerIndex))
		{
			return false;
		}

		const CPickupData* pPickupData = CPickupDataManager::GetPickupData(GetPickupHash());

		if (pPickupData)
		{
			if (CPickupManager::IsPickupModelCollectionProhibited(pPickupData->GetModelHash()))
			{
				return false;
			}
		}
		*/
	}	
	
	return true;
}

// scriptHandlerObject methods:
void CPickupPlacement::CreateScriptInfo(const scriptIdBase& scrId, const ScriptObjectId& objectId, const HostToken hostToken)
{
	m_ScriptInfo = CGameScriptObjInfo(scrId, objectId, hostToken);
	Assert(m_ScriptInfo.IsValid());
}

void CPickupPlacement::SetScriptInfo(const scriptObjInfoBase& info)
{
	m_ScriptInfo = info;
	Assert(m_ScriptInfo.IsValid());
}

scriptObjInfoBase*	CPickupPlacement::GetScriptInfo()
{ 
	if (m_ScriptInfo.IsValid())
		return static_cast<scriptObjInfoBase*>(&m_ScriptInfo); 

	return NULL;
}

const scriptObjInfoBase*	CPickupPlacement::GetScriptInfo() const 
{ 
	if (m_ScriptInfo.IsValid())
		return static_cast<const scriptObjInfoBase*>(&m_ScriptInfo); 

	return NULL;
}

void CPickupPlacement::SetScriptHandler(scriptHandler* handler) 
{ 	
	Assert(!handler || handler->GetScriptId() == m_ScriptInfo.GetScriptId());
	m_pHandler = handler; 
}

void CPickupPlacement::OnRegistration(bool LOGGING_ONLY(newObject), bool LOGGING_ONLY(hostObject) )
{
#if ENABLE_NETWORK_LOGGING
	if (m_pHandler)
	{
#if __BANK
		scriptDisplayf("%s : Register %s script pickup %s", m_pHandler->GetLogName(), newObject ? "new" : "existing", m_pPickupData->GetName());
#endif // __BANK

		if (newObject && GetNetworkObject())
		{
			Assertf(m_pHandler->GetNetworkComponent(), "Script %s is not networked but is creating networked pickups. This is not permitted.", m_pHandler->GetScriptId().GetLogName());
		}
	}

	if (NetworkInterface::IsGameInProgress())
	{
		netLoggingInterface *log = CTheScripts::GetScriptHandlerMgr().GetLog();

		if(log)
		{
			char logName[30];
			char header[50];
		
			m_ScriptInfo.GetLogName(logName, 30);

			formatf(header, 50, "REGISTER_%s_%s_PICKUP", newObject ? "NEW" : "EXISTING", hostObject ? "HOST" : "LOCAL");

			NetworkLogUtils::WriteLogEvent(*log, header, "%s      %s", m_ScriptInfo.GetScriptId().GetLogName(), logName);

			log->WriteDataValue("Script id", "%d", m_ScriptInfo.GetObjectId());

			netObject* netObj = GetNetworkObject();

			if (netObj)
			{
				log->WriteDataValue("Net Object",	"%s", netObj->GetLogName());
			}	
		}
	}
#endif // ENABLE_NETWORK_LOGGING
}

void CPickupPlacement::OnUnregistration()
{
#if __BANK
	scriptDisplayf("%s : Unregister script pickup %s", m_ScriptInfo.GetScriptId().GetLogName(), m_pPickupData ? m_pPickupData->GetName() : "?");
#endif // !__FINAL

	netLoggingInterface *log = CTheScripts::GetScriptHandlerMgr().GetLog();

	char logName[30];
	m_ScriptInfo.GetLogName(logName, 30);

	if (log)
	{
		NetworkLogUtils::WriteLogEvent(*log, "UNREGISTER_PICKUP", "%s      %s", m_ScriptInfo.GetScriptId().GetLogName(), logName);

		log->WriteDataValue("Script id", "%d", m_ScriptInfo.GetObjectId());

		if (GetNetworkObject())
		{
			log->WriteDataValue("Net Object", "%s", GetNetworkObject()->GetLogName());
		}
	}
}

// Determine whether the object needs to be cleaned up properly. This is not done if the object is still required to persist as a script object
// for the remaining participants, who may still be running the script normally.
bool CPickupPlacement::IsCleanupRequired() const
{
	netLoggingInterface* log = CTheScripts::GetScriptHandlerMgr().GetLog();

	char logName[30];
	logName[0] = 0;

	if (gnetVerify(GetScriptInfo()))
	{
		GetScriptInfo()->GetLogName(logName, 30);
	}

	if (GetIsBeingDestroyed())
	{
		if (log)
		{
			NetworkLogUtils::WriteLogEvent(*log, "DETACH_PICKUP_PLACEMENT", "%s      %s", GetScriptInfo()->GetScriptId().GetLogName(), logName);
			log->WriteDataValue("Net Object", "%s", GetNetworkObject() ? GetNetworkObject()->GetLogName() : "?");
			log->WriteDataValue("Being destroyed", "true");
		}
		return false;
	}

	return CGameScriptHandlerObject::IsCleanupRequired();
}

void CPickupPlacement::Cleanup()
{
	netLoggingInterface* log = CTheScripts::GetScriptHandlerMgr().GetLog();

	if (log)
	{
		char logName[30];
		logName[0] = 0;

		if (gnetVerify(GetScriptInfo()))
		{
			GetScriptInfo()->GetLogName(logName, 30);
		}

		NetworkLogUtils::WriteLogEvent(*log, "CLEANUP_PICKUP_PLACEMENT", "%s      %s", GetScriptInfo()->GetScriptId().GetLogName(), logName);
		log->WriteDataValue("Net Object", "%s", GetNetworkObject() ? GetNetworkObject()->GetLogName() : "?");
	}

	m_ScriptInfo.Invalidate();

	CPickupManager::RemovePickupPlacement(this);
}

#if __BANK
void CPickupPlacement::SpewDebugInfo(const char* scriptName) const
{
	if (scriptName)
	{
		Displayf("%s: %s (%f, %f, %f) %s\n", 
				scriptName, 
				m_pPickupData->GetName(),
				m_pickupPosition.x, m_pickupPosition.y, m_pickupPosition.z,
				GetNetworkObject() ? GetNetworkObject()->GetLogName() : "");
	}
	else
	{
		Displayf("%s (%f, %f, %f) %s\n", 
				m_pPickupData->GetName(),
				m_pickupPosition.x, m_pickupPosition.y, m_pickupPosition.z,
				GetNetworkObject() ? GetNetworkObject()->GetLogName() : "");
	}
}
#endif // __BANK

CPickupPlacement::CPickupPlacementCustomScriptData* CPickupPlacement::GetOrCreateCustomScriptData()
{
	if (!m_pCustomScriptData && CPickupPlacementCustomScriptData::GetPool()->GetNoOfFreeSpaces() > 0)
	{
		m_pCustomScriptData = rage_new CPickupPlacementCustomScriptData();
	}

	Assertf(m_pCustomScriptData, "Ran out of CPickupPlacement::CCustomScriptDatas");
	return m_pCustomScriptData;
}

// static functions:

bool CPickupPlacement::GetInScope(const CPed& playerPed, const Vector3& placementPos, float range) 
{
	Vector3 playerPos = VEC3V_TO_VECTOR3(playerPed.GetTransform().GetPosition());

	// if the local player is spectating, use his camera position or spectated player position instead
	if (playerPed.IsLocalPlayer() && NetworkInterface::IsGameInProgress())
	{
		CNetGamePlayer* localPlayer = NetworkInterface::GetLocalPlayer();

		if (localPlayer)
		{
			playerPos = NetworkInterface::GetPlayerFocusPosition(*localPlayer);
		}
	}

	Vector3 playerDist = playerPos - placementPos;
	float scope = static_cast<float>(range*range);

	return (playerDist.Mag2() < scope);
}

