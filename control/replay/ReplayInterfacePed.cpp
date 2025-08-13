#include "ReplayInterfacePed.h"

#if GTA_REPLAY

#include "replay.h"
#include "ReplayInterfaceImpl.h"
#include "ReplayInterfaceVeh.h"
#include "ReplayLightingManager.h"
#include "streaming/streaming.h"
#include "scene/world/GameWorld.h"

#include "camera/viewports/ViewportManager.h"
#include "Control/Replay/Ped/BonedataMap.h"


const char*	CReplayInterfaceTraits<CPed>::strShort = "PED";
const char*	CReplayInterfaceTraits<CPed>::strLong = "PEDESTRIAN";
const char*	CReplayInterfaceTraits<CPed>::strShortFriendly = "Ped";
const char*	CReplayInterfaceTraits<CPed>::strLongFriendly = "ReplayInterface-Pedestrian";

const int	CReplayInterfaceTraits<CPed>::maxDeletionSize = MAX_PED_DELETION_SIZE;

const u32 PED_RECORD_BUFFER_SIZE		= 256*1024;
const u32 PED_CREATE_BUFFER_SIZE		= 200*1024;	// 256 peds * 788 bytes
const u32 PED_FULLCREATE_BUFFER_SIZE	= 200*1024;	// 256 peds * 788 bytes

//////////////////////////////////////////////////////////////////////////
CReplayInterfacePed::CReplayInterfacePed(CReplayInterfaceVeh& vehInterface)
	: CReplayInterface<CPed>(*CPed::GetPool(), PED_RECORD_BUFFER_SIZE, PED_CREATE_BUFFER_SIZE, PED_FULLCREATE_BUFFER_SIZE)
	, m_vehInterface(vehInterface)
	, m_suppressOldPlayerDeletion(false)
{
	m_relevantPacketMin = PACKETID_PED_MIN;
	m_relevantPacketMax	= PACKETID_PED_MAX;

	m_createPacketID	= PACKETID_PEDCREATE;
	m_updatePacketID	= PACKETID_PEDUPDATE;
	m_deletePacketID	= PACKETID_PEDDELETE;
	m_packetGroup		= REPLAY_PACKET_GROUP_PED;

	m_enableSizing		= true;
	m_enableRecording	= true;
	m_enablePlayback	= true;

	m_anomaliesCanExist = false;

	// Set up the relevant packets for this interface
	AddPacketDescriptor<CPacketPedUpdate>(PACKETID_PEDUPDATE, STRINGIFY(CPacketPedUpdate));
	AddPacketDescriptor<CPacketPedCreate>(PACKETID_PEDCREATE, STRINGIFY(CPacketPedCreate));
	AddPacketDescriptor<CPacketPedDelete>(PACKETID_PEDDELETE, STRINGIFY(CPacketPedDelete));
	AddPacketDescriptor<CPacketAlign>(PACKETID_ALIGN, STRINGIFY(CPacketAlign));
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfacePed::PreClearWorld()
{
	CPed* pPlayer = FindPlayerPed();
	CPortalTracker* pPT = pPlayer->GetPortalTracker();
	CInteriorInst* pIntInst = pPT->GetInteriorInst();
	if (pPlayer->m_nFlags.bInMloRoom && pPT && pIntInst && pPT->m_roomIdx != 0) 
	{
		CGameWorld::Remove(pPlayer, true);
		pPT->Teleport();
		CGameWorld::Add(pPlayer, CGameWorld::OUTSIDE, true);
		replayAssertf(pPlayer->m_nFlags.bInMloRoom == false, "Player is still in a room interior before script cleanup - we may get asserts firing");
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfacePed::ClearWorldOfEntities()
{
	// Clear all Replay owned Peds
	CReplayInterface<CPed>::ClearWorldOfEntities();

	CPedPopulation::FlushPedReusePool();

	const Vector3 VecCoors(0.0f, 0.0f, 0.0f);
	const float Radius = 1000000.0f;
	CGameWorld::ClearPedsFromArea(VecCoors, Radius);

	// Remove all weapons and gadgets from the Player
	CPed* pPlayer = FindPlayerPed();
	if(pPlayer)
	{
		if(pPlayer->GetWeaponManager()->GetEquippedWeaponObject())
			pPlayer->GetWeaponManager()->DestroyEquippedWeaponObject(false);
		CObject* pWeapon = pPlayer->GetWeaponManager()->GetEquippedWeaponObject();
		(void)pWeapon;
		pPlayer->GetWeaponManager()->DestroyEquippedGadgets();

		CVehicle* pPlayerVehicle = pPlayer->GetMyVehicle();
		if(pPlayerVehicle)
		{
			pPlayer->SetPedOutOfVehicle();
		}
	}

	m_playerList.clear();
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfacePed::ResetEntity(CPed* pPed)
{
	pPed->ClearWetClothing();
	pPed->m_Buoyancy.ResetBuoyancy();
}


//////////////////////////////////////////////////////////////////////////
bool CReplayInterfacePed::ShouldRecordElement(const CPed* pPed) const
{
	// Don't record peds that are in the population cache or the reuse pool.
	return pPed->GetIsInPopulationCache() == false && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedIsInReusePool) == false;
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfacePed::OnDelete(CPhysical* pEntity)
{
	if(m_suppressOldPlayerDeletion)
		return;

	CReplayInterface<CPed>::OnDelete(pEntity);
}

//////////////////////////////////////////////////////////////////////////
void CReplayInterfacePed::PrintOutPacket(ReplayController& controller, eReplayPacketOutputMode mode, FileHandle handle)
{
	CPacketBase const* pBasePacket = controller.ReadCurrentPacket<CPacketBase>();
	PrintOutPacketHeader(pBasePacket, mode, handle);

	const CPacketDescriptor<CPed>* pPacketDescriptor = NULL;
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
CPed* CReplayInterfacePed::CreateElement(CReplayInterfaceTraits<CPed>::tCreatePacket const* pPacket, const CReplayState& state)
{
	fwModelId modelID = m_modelManager.LoadModel(pPacket->GetModelNameHash(), pPacket->GetMapTypeDefIndex(), !pPacket->UseMapTypeDefHash(), true);
	if(modelID.IsValid() == false)
	{
		HandleFailedToLoadModelError(pPacket);
		return NULL;
	}

	replayDebugf2("Create ped %X", pPacket->GetReplayID().ToInt());
	CPed* pPed = NULL;

	Matrix34 matTemp;
	pPacket->LoadMatrix(matTemp);

	if (pPacket->IsMainPlayer())
	{
		CPed* pCurrPlayer = FindPlayerPed();

		replayFatalAssertf(pCurrPlayer != NULL, "Current player is NULL!");

		const CControlledByInfo localPlayerControl(false, false);

		//g_SpeechManager.SetReplayPainBanks(pPacket->GetPainBank(), pPacket->GetPainBank());
		g_SpeechManager.SetPlayedPainForReplayVars(	AUD_PAIN_VOICE_TYPE_PLAYER, 
													pPacket->GetPainNextVariations(), 
													pPacket->GetPainNumTimesPlayed(), 
													pPacket->GetNextTimeToLoadPain(), 
													pPacket->GetCurrentPainBank());

#if __ASSERT
		CPlayerInfo::SetChangingPlayerModel(true);
#endif
		pPed = (CPed*)CPedFactory::GetFactory()->CreatePed( localPlayerControl,
															modelID,
															&matTemp,
															false, false, false);
#if __ASSERT
		CPlayerInfo::SetChangingPlayerModel(false);
#endif

		// Change the player to this new one...
		CGameWorld::ChangePlayerPed(*pCurrPlayer, *pPed);

		//TODO4FIVE
		/*pPed->SetOrientation(DtoR(0.0f), DtoR(0.0f), DtoR(0.0f));
		pPed->GetWeaponMgr()->SetShootingAccuracy(100);*/

		// for debugging: replay ped ptr
		CReplayMgr::SetMainPlayerPtr(pPed);

		// If there is already a replay entity set as the player ped then store it's ID so we can set it again when rewinding if necessary.
		if(state.IsSet(REPLAY_DIRECTION_FWD) && pCurrPlayer->GetReplayID() != ReplayIDInvalid && m_playerList.GetCount() < MAX_PREV_PLAYERS)
		{
			m_playerList.Push(pCurrPlayer->GetReplayID());
		}

		if(pCurrPlayer->GetOwnedBy() != ENTITY_OWNEDBY_REPLAY || pCurrPlayer->GetReplayID() == ReplayIDInvalid)
		{
			if(pCurrPlayer->GetOwnedBy() == ENTITY_OWNEDBY_REPLAY)
			{
				pCurrPlayer->SetOwnedBy(ENTITY_OWNEDBY_RANDOM);
			}

			// If the old player is the attach entity for the gbuf viewport, then set the attach entity
			// to null so the viewport isn't holding a pointer to a deleted entity
			CViewport *pVp = g_SceneToGBufferPhaseNew->GetViewport();
			if(pVp && pVp->GetAttachEntity() == pCurrPlayer)
			{
				pVp->SetAttachEntity(NULL);
			}

			//flag the old player ped for deletion
			// The replay extensions have already been removed from the old player ped
			// So calling this will delete the old player ped but will assert when looping back to the replay
			// attempting to unregister the ped as it has no replay extensions!
			m_suppressOldPlayerDeletion = true;
			CPedFactory::GetFactory()->DestroyPed(pCurrPlayer);
			m_suppressOldPlayerDeletion = false;
		}
	}
	else
	{
		const CControlledByInfo localNpcControl(false, false);

		pPed = CPedFactory::GetFactory()->CreatePed(localNpcControl,
													modelID,
													&matTemp,
													false,
													false,
													false);
		replayAssert(pPed);
	}

	if (pPed)
	{
		pPed->SetOwnedBy(ENTITY_OWNEDBY_REPLAY);	

		pPacket->GetVariationData()->Extract(pPed, pPacket->GetPacketVersion());			

		pPed->m_nPhysicalFlags.bNotDamagedByAnything = true;

		//TODO4FIVE
		//pPed->SetStatus(STATUS_PLAYER_PLAYBACKFROMBUFFER); // Don't do any updating stuff
		//pPed->SetCharCreatedBy(REPLAY_CHAR);

		CGameWorld::Remove(pPed);
		CGameWorld::Add(pPed, CGameWorld::OUTSIDE );

		// Set the ped up with replay related info
		PostCreateElement(pPacket, pPed, pPacket->GetReplayID());

		pPacket->Extract(pPed);		
		pPed->SetClothForcePin(1); // Initializes cloth apparently.

		pPed->RemoveSceneUpdateFlags(CGameWorld::SU_START_ANIM_UPDATE_PRE_PHYSICS | CGameWorld::SU_END_ANIM_UPDATE_PRE_PHYSICS | CGameWorld::SU_START_ANIM_UPDATE_POST_CAMERA);
	}

	return pPed;
}


//////////////////////////////////////////////////////////////////////////
bool CReplayInterfacePed::RemoveElement(CPed* pElem, const CPacketPedDelete* pDeletePacket, bool isRewinding)
{
	ReplayLightingManager::RemovePed(pElem);

	//we don't want to remove the player during playback, the game doesn't like it when it happens
	bool baseResult = true;
	if(FindPlayerPed() != pElem)
	{
		baseResult = CReplayInterface::RemoveElement(pElem, pDeletePacket, isRewinding);

		CPedFactory::GetFactory()->DestroyPed(pElem);
	}
	else
	{
		bool deletePed = false;
		if(isRewinding && !m_playerList.empty())
		{
			CEntityGet<CPed> eg(m_playerList.Pop());
			GetEntity(eg);
			
			if(eg.m_pEntity && pElem != eg.m_pEntity)
			{
				CGameWorld::ChangePlayerPed(*pElem, *eg.m_pEntity);

				// for debugging: replay ped ptr
				CReplayMgr::SetMainPlayerPtr(eg.m_pEntity);

				deletePed = true;
			}
		}


		replayDebugf1("Player ped being set to Population rather than being destroyed... %p, 0x%08X", pElem, pElem->GetReplayID().ToInt());
		if(ReplayEntityExtension::HasExtension(pElem))
		{
			ClrEntity(pElem->GetReplayID(), pElem);
		}

		pElem->SetOwnedBy(ENTITY_OWNEDBY_POPULATION);
		CReplayExtensionManager::RemoveReplayExtensions(pElem);
		pElem->SetReplayID(ReplayIDInvalid);

		if(deletePed)
			CPedFactory::GetFactory()->DestroyPed(pElem);

		--m_entitiesDuringPlayback;
	}

	return baseResult & true;
}


//////////////////////////////////////////////////////////////////////////
void CReplayInterfacePed::PlayUpdatePacket(ReplayController& controller, bool entityMayNotExist)
{
	CPacketPedUpdate const* pPacket = controller.ReadCurrentPacket<CPacketPedUpdate>();

	m_interper.Init(controller, pPacket);
	InterpWrapper<CPedInterp> interper(m_interper, controller);

	if(PlayUpdateBackwards(controller, pPacket))
		return;

	CEntityGet<CPed> entityGet(pPacket->GetReplayID());
	GetEntity(entityGet);
	CPed* pPed = entityGet.m_pEntity;
	
	if(!pPed && (entityGet.m_alreadyReported || entityMayNotExist))
	{
		// Entity wasn't created for us to update....but we know this already
		return;
	}

	if(Verifyf(pPed, "Failed to find Ped to update, but it wasn't reported as 'failed to create' - 0x%08X - Packet %p", (u32)pPacket->GetReplayID(), pPacket))
	{
		CPacketPedUpdate const* pNextPacket = m_interper.GetNextPacket();

		CVehicle* pVehicle = NULL;
		if(pPacket->GetVehicleID() >= 0)
		{
			CEntityGet<CVehicle> entityGet(pPacket->GetVehicleID());
			m_vehInterface.GetEntity(entityGet);
			pVehicle = entityGet.m_pEntity;
		}

		CInterpEventInfo<CPacketPedUpdate, CPed> info;
		info.SetEntity(pPed);
		info.SetNextPacket(pNextPacket);
		info.SetInterp(controller.GetInterp());

		pPacket->Extract(info, pVehicle);
	}
}


#if REPLAY_DELAYS_QUANTIZATION
//////////////////////////////////////////////////////////////////////////
void CReplayInterfacePed::PostRecordOptimize(CPacketBase* pPacket) const
{
	if (pPacket->GetPacketID() == PACKETID_PEDUPDATE)
	{
		CPacketPedUpdate* pPacketPedUpdate = static_cast<CPacketPedUpdate*>(pPacket);
		pPacketPedUpdate->QuantizeQuaternions();
	}
}

//////////////////////////////////////////////////////////////////////////
void CReplayInterfacePed::ResetPostRecordOptimizations() const
{
	CPacketPedUpdate::ResetDelayedQuaternions();
}
#endif // REPLAY_DELAYS_QUANTIZATION


//////////////////////////////////////////////////////////////////////////
bool CReplayInterfacePed::TryPreloadPacket(const CPacketBase* pPacket, bool& preloadSuccess, u32 currGameTime PRELOADDEBUG_ONLY(, atString& failureReason))
{
	if(pPacket->GetPacketID() == PACKETID_PEDUPDATE)
	{
		const CPacketPedUpdate* pUpdatePacket = static_cast<const CPacketPedUpdate*>(pPacket);
		u32 txdHash = pUpdatePacket->GetOverrideCrewLogoTxd();
		if(txdHash != 0)
		{
			strLocalIndex index = g_TxdStore.FindSlot(txdHash);
			m_modelManager.PreloadRequest(txdHash, index, false, g_TxdStore.GetStreamingModuleId(), preloadSuccess, currGameTime);
		}
		return true;
	}

	if(!CReplayInterface<CPed>::TryPreloadPacket(pPacket, preloadSuccess, currGameTime PRELOADDEBUG_ONLY(, failureReason)))
		return false;

	if(pPacket->GetPacketID() != PACKETID_PEDCREATE || !preloadSuccess)
		return true;

	const CPacketPedCreate *pCreatePacket = static_cast < const CPacketPedCreate * >(pPacket);

	preloadSuccess &= pCreatePacket->GetVariationData()->AddPreloadRequests(m_modelManager, currGameTime);

#if PRELOADDEBUG
	if(!preloadSuccess && failureReason.GetLength() == 0)
	{
		failureReason = "Failed to preload Ped Variation Data";
	}
#endif // PRELOADDEBUG

	return true;
}


//////////////////////////////////////////////////////////////////////////
bool CReplayInterfacePed::WaitingOnLoading()
{
	for(int i = 0; i < m_pool.GetSize(); ++i)
	{
		CPed* pPed = m_pool.GetSlot(i);
		if(pPed)
		{
			if(!pPed->HaveAllStreamingReqsCompleted())
			{
				return true;
			}
		}
	}

	return false;
}

#endif // GTA_REPLAY
