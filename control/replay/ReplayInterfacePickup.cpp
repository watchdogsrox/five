#include "ReplayInterfacePickup.h"

#if GTA_REPLAY

#include "replay.h"
#include "pickups/Data/PickupDataManager.h"
#include "ReplayInterfaceImpl.h"
#include "Streaming/streaming.h"
#include "scene/world/GameWorld.h"

#include "fwscene/stores/maptypesstore.h"
#include "objects/ProcObjects.h"
#include "pickups/PickupManager.h"
#include "weapons/WeaponFactory.h"
#include "Weapons/components/WeaponComponentFactory.h"
#include "Weapons/components/WeaponComponentManager.h"

const char*	CReplayInterfaceTraits<CPickup>::strShort = "PIC";
const char*	CReplayInterfaceTraits<CPickup>::strLong = "PICKUP";
const char*	CReplayInterfaceTraits<CPickup>::strShortFriendly = "Pic";
const char*	CReplayInterfaceTraits<CPickup>::strLongFriendly = "ReplayInterface-Pickup";

const int	CReplayInterfaceTraits<CPickup>::maxDeletionSize	= MAX_PICKUP_DELETION_SIZE;

const u32 PIC_RECORD_BUFFER_SIZE		= 16*1024;
const u32 PIC_CREATE_BUFFER_SIZE		= 8*1024;
const u32 PIC_FULLCREATE_BUFFER_SIZE	= 8*1024;

//////////////////////////////////////////////////////////////////////////
CReplayInterfacePickup::CReplayInterfacePickup()
	: CReplayInterface<CPickup>(*CPickup::GetPool(), PIC_RECORD_BUFFER_SIZE, PIC_CREATE_BUFFER_SIZE, PIC_FULLCREATE_BUFFER_SIZE)
{
	m_relevantPacketMin = PACKETID_PICKUP_MIN;
	m_relevantPacketMax	= PACKETID_PICKUP_MAX;

	m_createPacketID	= PACKETID_PICKUP_CREATE;
	m_updatePacketID	= PACKETID_PICKUP_UPDATE;
	m_deletePacketID	= PACKETID_PICKUP_DELETE;
	m_packetGroup		= REPLAY_PACKET_GROUP_PICKUP;

	m_enableSizing		= true;
	m_enableRecording	= true;
	m_enablePlayback	= true;			
	m_enabled			= true;

	m_anomaliesCanExist = false;

	// Set up the relevant packets for this interface
	AddPacketDescriptor<CPacketPickupCreate>(	PACKETID_PICKUP_CREATE, 	STRINGIFY(CPacketPickupCreate));
	AddPacketDescriptor<CPacketPickupUpdate>(	PACKETID_PICKUP_UPDATE, 	STRINGIFY(CPacketPickupUpdate));
	AddPacketDescriptor<CPacketPickupDelete>(	PACKETID_PICKUP_DELETE, 	STRINGIFY(CPacketPickupDelete));	
}

//////////////////////////////////////////////////////////////////////////
template<typename PACKETTYPE, typename OBJECTSUBTYPE>
void CReplayInterfacePickup::AddPacketDescriptor(eReplayPacketId packetID, const char* packetName, s32 objectType)
{
	REPLAY_CHECK(m_numPacketDescriptors < maxPacketDescriptorsPerInterface, NO_RETURN_VALUE, "Max number of packet descriptors breached");
	m_packetDescriptors[m_numPacketDescriptors++] = CPacketDescriptor<CPickup>(packetID, 
		objectType, 
		sizeof(PACKETTYPE), 
		packetName,
		&iReplayInterface::template PrintOutPacketDetails<PACKETTYPE>,
		&CReplayInterface<CPickup>::template RecordElementInternal<PACKETTYPE, OBJECTSUBTYPE>,
		&CReplayInterface<CPickup>::template ExtractPacket<PACKETTYPE, OBJECTSUBTYPE>);
}

//////////////////////////////////////////////////////////////////////////
void CReplayInterfacePickup::ClearWorldOfEntities()
{
	// Clear all Replay owned Pickups
	CReplayInterface<CPickup>::ClearWorldOfEntities();

	CPickupManager::RemoveAllPickups(true, true);

	// Update proc obj since objs were destroyed above.
	g_procObjMan.Update();
}


//////////////////////////////////////////////////////////////////////////
bool CReplayInterfacePickup::ShouldRecordElement(const CPickup* pPickup) const
{
	if(pPickup->GetOwnedBy() == ENTITY_OWNEDBY_REPLAY)	// Only for existing clips which already contain this pickups...can't do much about these.
		return true;

	return CheckAllowedModelNameHash(pPickup->GetArchetype()->GetModelNameHash());
}


//////////////////////////////////////////////////////////////////////////
bool CReplayInterfacePickup::CheckAllowedModelNameHash(u32 hash) const
{
	//We don't want to allow multiplayer pickups to be visible during replay playback so reject them here
	//doing on playback in case we decide we want them back at some stage.
	if( hash == ATSTRINGHASH("Prop_MP_Rocket_01", 0xa4b0d6d0) )
	{
		return false;
	}
	if( hash == ATSTRINGHASH("Prop_MP_Boost_01", 0x65eaf4b2) )
	{
		return false;
	}
	if( hash == ATSTRINGHASH("Prop_MP_repair", 0xe6fa7770) )
	{
		return false;
	}
	if( hash == ATSTRINGHASH("prop_ex_b_time", 0x27b24135))
		return false;
	else if( hash == ATSTRINGHASH("prop_ex_b_time_p", 0x6a68a81e))
		return false;
	else if(hash == ATSTRINGHASH("prop_ex_b_shark", 0x632895d1))
		return false;
	else if(hash == ATSTRINGHASH("prop_ex_b_shark_p", 0xe88ed8e6))
		return false;
	else if(hash == ATSTRINGHASH("prop_ex_weed", 0x9c3de483))
		return false;
	else if(hash == ATSTRINGHASH("prop_ex_weed_p", 0x648c821f))
		return false;
	else if(hash == ATSTRINGHASH("prop_ex_bmd", 0x3d7af41d))
		return false;
	else if(hash == ATSTRINGHASH("prop_ex_bmd_p", 0x55a54df8))
		return false;
	else if(hash == ATSTRINGHASH("prop_ex_random", 0xd8333276))
		return false;
	else if(hash == ATSTRINGHASH("prop_ex_random_p", 0x60f079d8))
		return false;
	else if(hash == ATSTRINGHASH("prop_ex_hidden", 0xa8c9393c))
		return false;
	else if(hash == ATSTRINGHASH("prop_ex_hidden_p", 0x17fe267f))
		return false;
	else if(hash == ATSTRINGHASH("prop_ex_swap", 0x3283507))
		return false;
	else if(hash == ATSTRINGHASH("prop_ex_swap_p", 0xf769bc46))
		return false;
	else if(hash == ATSTRINGHASH("prop_ic_hop",0xb63fd127))
		return false;
	else if(hash == ATSTRINGHASH("prop_ic_boost",0xa06a571c))
		return false;
	else if(hash == ATSTRINGHASH("prop_arena_icon_flag_yellow", 0xbee3a0f3))
		return false;
	else if(hash == ATSTRINGHASH("prop_arena_icon_flag_purple", 0x49b8fc76))
		return false;

	return true;
}

//////////////////////////////////////////////////////////////////////////
CPickup* CReplayInterfacePickup::CreateElement(CReplayInterfaceTraits<CPickup>::tCreatePacket const* pPacket, const CReplayState& /*state*/)
{
	rage::strModelRequest streamingModelReq;
	rage::strRequest streamingReq;
	if(!m_modelManager.LoadModel(pPacket->GetModelNameHash(), pPacket->GetMapTypeDefIndex(), !pPacket->UseMapTypeDefHash(), true, streamingModelReq, streamingReq))
	{
		HandleFailedToLoadModelError(pPacket);
		return NULL;
	}

	u32 pickuphash = pPacket->GetPickupHash();
	const CPickupData* pPickupData = CPickupDataManager::GetPickupData(pickuphash);

	CPickup* pPickup = NULL;

	if (replayVerifyf(pPickupData, "CreateElement - invalid pickup type"))
	{
		Matrix34 createM;
		createM.Identity();

		strLocalIndex customModelIndex = strLocalIndex(fwModelId::MI_INVALID);
		if (pPacket->GetModelNameHash() != 0)
		{
			fwModelId modelId;
			CModelInfo::GetBaseModelInfoFromHashKey((u32)pPacket->GetModelNameHash(), &modelId);

			if (replayVerifyf(modelId.IsValid(), "CREATE_PORTABLE_PICKUP - the custom model is invalid") &&
				replayVerifyf(CModelInfo::HaveAssetsLoaded(modelId), "CREATE_PORTABLE_PICKUP - the custom model is not loaded"))
			{
				customModelIndex = modelId.GetModelIndex();
			}
		}

		pPickup = CPickupManager::CreatePickup(pPickupData->GetHash(), createM, NULL, false, customModelIndex.Get());

		if (pPickup)
		{
			pPickup->SetOwnedBy(ENTITY_OWNEDBY_REPLAY);
			pPickup->SetStatus(STATUS_PLAYER);//todo4five this might need special replay status the old one was		// Don't do any updating stuff
			pPickup->DisableCollision();

			// If the object is a weapon then create it as such...
			if(pPacket->GetWeaponHash() != 0)
			{
				CWeapon* pWeapon = WeaponFactory::Create(pPacket->GetWeaponHash(), CWeapon::INFINITE_AMMO, pPickup);
				if(replayVerify(pWeapon))
				{
					pPickup->SetWeapon(pWeapon);

					pWeapon->UpdateShaderVariables(pPacket->GetWeaponTint());
				}
			}

			CGameWorld::Add(pPickup, CGameWorld::OUTSIDE);

			// Set the object up with replay related info
			PostCreateElement(pPacket, (CPickup*)pPickup, pPacket->GetReplayID());
		}

	}

	return pPickup;
}


//////////////////////////////////////////////////////////////////////////
bool CReplayInterfacePickup::RemoveElement(CPickup* pElem, const CPacketPickupDelete* pDeletePacket, bool isRewinding)
{
	if(pElem->IsFlagSet(CPickup::PF_Destroyed))
		return false;

	bool baseResult = CReplayInterface<CPickup>::RemoveElement(pElem, pDeletePacket, isRewinding);

	pElem->Destroy();

	CObjectPopulation::DestroyObject(pElem);

	return baseResult & true;
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfacePickup::ResetAllEntities()
{
	CReplayInterface<CPickup>::ResetAllEntities();
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfacePickup::ResetEntity(CPickup* pObject)
{
	CReplayInterface<CPickup>::ResetEntity(pObject);
}


//////////////////////////////////////////////////////////////////////////
void  CReplayInterfacePickup::PlaybackSetup(ReplayController& controller)
{
	CReplayInterface<CPickup>::PlaybackSetup(controller);
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfacePickup::ExtractObjectType(CPickup* pObject, CPacketInterp const* pPrevPacket, CPacketInterp const* pNextPacket, float interpolationValue)
{
	CInterpEventInfo<CPacketPickupUpdate, CPickup> info;
	info.SetEntity(pObject);
	info.SetNextPacket((CPacketPickupUpdate*)pNextPacket);
	info.SetInterp(interpolationValue);
	((CPacketPickupUpdate*)pPrevPacket)->Extract(info);
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfacePickup::PlayUpdatePacket(ReplayController& controller, bool entityMayNotExist)
{
	CPacketPickupUpdate const* pPacket = controller.ReadCurrentPacket<CPacketPickupUpdate>();

	m_interper.Init(controller, pPacket);
	INTERPER_PICKUP interper(m_interper, controller);

	if(PlayUpdateBackwards(controller, pPacket))
		return;

	CEntityGet<CPickup> entityGet(pPacket->GetReplayID());
	GetEntity(entityGet);
	CPickup* pPickup = entityGet.m_pEntity;

	if(!pPickup && (entityGet.m_alreadyReported || entityMayNotExist))
	{
		// Entity wasn't created for us to update....but we know this already
		return;
	}

	if(Verifyf(pPickup, "Failed to find Pickup to update, but it wasn't reported as 'failed to create' - 0x%08X - Packet %p", (u32)pPacket->GetReplayID(), pPacket))
	{
		CPacketPickupUpdate const* pNextObjectPacket = m_interper.GetNextPacket();

		ExtractObjectType(pPickup, pPacket, pNextObjectPacket, controller.GetInterp());
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfacePickup::PlayDeletePacket(ReplayController& controller)
{
	CReplayInterfaceTraits<CPickup>::tDeletePacket const* pPacket = controller.ReadCurrentPacket<CReplayInterfaceTraits<CPickup>::tDeletePacket>();
	replayAssertf(pPacket->GetPacketID() == m_deletePacketID, "Incorrect Packet");

	if (pPacket->GetReplayID() == ReplayIDInvalid)
	{
		controller.AdvancePacket();
		return;
	}

	CReplayInterface<CPickup>::PlayDeletePacket(controller);
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfacePickup::OnDelete(CPhysical* pEntity)
{
	CReplayInterface<CPickup>::OnDelete(pEntity);
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfacePickup::PrintOutPacket(ReplayController& controller, eReplayPacketOutputMode mode, FileHandle handle)
{
	CPacketBase const* pBasePacket = controller.ReadCurrentPacket<CPacketBase>();
	PrintOutPacketHeader(pBasePacket, mode, handle);

	const CPacketDescriptor<CPickup>* pPacketDescriptor = NULL;
	if(FindPacketDescriptor(pBasePacket->GetPacketID(), pPacketDescriptor))
	{
		pPacketDescriptor->PrintOutPacket(controller, mode, handle);
	}
	else
	{
		u32 packetSize = pBasePacket->GetPacketSize();
		replayAssert(packetSize != 0);
		controller.AdvanceUnknownPacket(packetSize);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CReplayInterfacePickup::LoadArchtype(strLocalIndex index, bool urgent)
{
	strRequest m_assetRequester;
	m_assetRequester.Request(index, CModelInfo::GetStreamingModuleId(), STRFLAG_FORCE_LOAD);

	if(g_MapTypesStore.HasObjectLoaded(index) == false)
	{
		if(urgent)
		{
			g_MapTypesStore.StreamingBlockingLoad(index, STRFLAG_FORCE_LOAD | STRFLAG_PRIORITY_LOAD);
			REPLAY_CHECK(g_MapTypesStore.HasObjectLoaded(index), false, "Pickup has not loaded");
			return true;
		}
		else
			g_MapTypesStore.StreamingRequest(index, STRFLAG_FORCE_LOAD | STRFLAG_PRIORITY_LOAD);	
	}

	return g_MapTypesStore.HasObjectLoaded(index);
}


//////////////////////////////////////////////////////////////////////////
// Get the space required for an update packet for a particular object
u32 CReplayInterfacePickup::GetUpdatePacketSize(CPickup* pObj) const
{
	u32 packetSize = sizeof(CPacketPickupUpdate);
	
	//if we don't currently have a valid create packet, then the next update packet will contain one, so we have to account for the bigger size here
	if(HasCreatePacket(pObj->GetReplayID()) == false)
	{
		packetSize += sizeof(CPacketPickupCreate);
	}

	return packetSize;
}

bool CReplayInterfacePickup::MatchesType(const CEntity* pEntity) const
{	
	if(pEntity->GetType() == ENTITY_TYPE_OBJECT || pEntity->GetType() == ENTITY_TYPE_DUMMY_OBJECT)
	{
		const CObject* pObject = static_cast<const CObject*>(pEntity);
		if(pObject->IsPickup())
		{
			return true;
		}
	}
	return false;
	
}


#endif // GTA_REPLAY

