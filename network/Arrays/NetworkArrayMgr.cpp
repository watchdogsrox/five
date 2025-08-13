//
// netArrayMgr.cpp
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#include "network/arrays/NetworkArrayMgr.h"

#include "fwscript/scripthandler.h"
#include "fwscript/scripthandlernetcomponent.h"
#include "fwscript/scriptInterface.h"

#include "game/config.h"
#include "network/Arrays/NetworkArrayHandlerTypes.h"
#include "network/network.h"
#include "network/NetworkInterface.h"
#include "network/players/NetGamePlayer.h"
#include "script/handlers/GameScriptIds.h"

NETWORK_OPTIMISATIONS()

PARAM(ttyScriptArrayHandlerMem, "[network] Displays the memory usage of the script array handlers to the TTY");

// ================================================================================================================
// CGameArrayMgr
// ================================================================================================================
sysMemAllocator* CGameArrayMgr::m_SaveHeap = NULL;

void CGameArrayMgr::Init()
{
	netArrayManager_Script::Init();

	CNetwork::UseNetworkHeap(NET_MEM_SCRIPT_ARRAY_HANDLERS);
	
	int startHeapUseage = int(CNetwork::GetNetworkHeap()->GetHeapSize() - CNetwork::GetNetworkHeap()->GetMemoryAvailable());

	int currentHeapUsage = startHeapUseage;
	CGamePlayerBroadcastDataHandler_Remote::InitPool(MEMBUCKET_NETWORK);
	int newHeapUsage =  int(CNetwork::GetNetworkHeap()->GetHeapSize() - CNetwork::GetNetworkHeap()->GetMemoryAvailable());

	bool displayMem = PARAM_ttyScriptArrayHandlerMem.Get();

	if (displayMem)
	{
		Displayf("\nAllocated remote array handlers. Mem %d. Heap used = %d\n", newHeapUsage-currentHeapUsage, newHeapUsage-startHeapUseage);
	}

	int maxArraySizeInBytes = 0;
	int maxNumPlayers = 0;
	int numInstances = 0;

	// pre-allocate all the script broadcast data handlers to avoid dynamic allocation & memory fragmentation
	// the sizes of the pre-allocated data are declared in gameconfig.xml
	const CConfigNetScriptBroadcastData& configData = CGameConfig::Get().GetConfigNetScriptBroadcastData();

 	for (int i=0; i<configData.m_HostBroadcastData.GetCount(); i++)
	{
		maxArraySizeInBytes = configData.m_HostBroadcastData[i].m_SizeOfData;
		maxNumPlayers = configData.m_HostBroadcastData[i].m_MaxParticipants;
		numInstances = configData.m_HostBroadcastData[i].m_NumInstances;

		if (numInstances > 0)
		{
			// make sure the max size in bytes is a multiple of 4 and greater or equal to the size of CBroadcastDataElement
			if (maxArraySizeInBytes%4 != 0)
			{
				maxArraySizeInBytes += (4-(maxArraySizeInBytes%4));
			}

			if (maxArraySizeInBytes < netScriptBroadcastDataHandlerBase::ELEMENT_SIZE_IN_BYTES)
			{
				maxArraySizeInBytes = netScriptBroadcastDataHandlerBase::ELEMENT_SIZE_IN_BYTES;
			}

			// double the size on 64-bit
			maxArraySizeInBytes *= sizeof(scrValue) / sizeof(int);

			gnetAssertf(maxNumPlayers > 1 && maxNumPlayers <= MAX_NUM_PHYSICAL_PLAYERS, "HostBroadcastData: Invalid number of players for entry %d in gameconfig.xml", i);

			maxNumPlayers = MIN(maxNumPlayers, MAX_NUM_PHYSICAL_PLAYERS);

			while (numInstances > 0)
			{
				currentHeapUsage = int(CNetwork::GetNetworkHeap()->GetHeapSize() - CNetwork::GetNetworkHeap()->GetMemoryAvailable());

				CGameHostBroadcastDataHandler* pNewHandler = rage_new CGameHostBroadcastDataHandler(maxArraySizeInBytes, maxNumPlayers);
				pNewHandler->Init();

				newHeapUsage = int(CNetwork::GetNetworkHeap()->GetHeapSize() - CNetwork::GetNetworkHeap()->GetMemoryAvailable());

				if (displayMem)
				{
					Displayf("Allocated host array handler size %d, num players %d. Mem %d. Heap used = %d\n", maxArraySizeInBytes, maxNumPlayers, newHeapUsage-currentHeapUsage, newHeapUsage-startHeapUseage);
				}

				m_HostBroadcastDataHandlers.PushAndGrow(pNewHandler);

				numInstances--;
			}
		}
	}

	for (int i=0; i<configData.m_PlayerBroadcastData.GetCount(); i++)
	{
		maxArraySizeInBytes = configData.m_PlayerBroadcastData[i].m_SizeOfData;
		maxNumPlayers = configData.m_PlayerBroadcastData[i].m_MaxParticipants;
		numInstances = configData.m_PlayerBroadcastData[i].m_NumInstances;

		if (numInstances > 0)
		{
			// make sure the max size in bytes is a multiple of 4 and greater or equal to the size of CBroadcastDataElement
			if (maxArraySizeInBytes%4 != 0)
			{
				maxArraySizeInBytes += (4-(maxArraySizeInBytes%4));
			}

			if (maxArraySizeInBytes < netScriptBroadcastDataHandlerBase::ELEMENT_SIZE_IN_BYTES)
			{
				maxArraySizeInBytes = netScriptBroadcastDataHandlerBase::ELEMENT_SIZE_IN_BYTES;
			}

			// double the size on 64-bit
			maxArraySizeInBytes *= sizeof(scrValue) / sizeof(int);

			gnetAssertf(maxNumPlayers > 1 && maxNumPlayers <= MAX_NUM_PHYSICAL_PLAYERS, "PlayerBroadcastData: Invalid number of players for entry %d in gameconfig.xml", i);

			maxNumPlayers = MIN(maxNumPlayers, MAX_NUM_PHYSICAL_PLAYERS);

			while (numInstances > 0)
			{
				currentHeapUsage = int(CNetwork::GetNetworkHeap()->GetHeapSize() - CNetwork::GetNetworkHeap()->GetMemoryAvailable());

				CGamePlayerBroadcastDataHandler_Local* pNewHandler = rage_new CGamePlayerBroadcastDataHandler_Local(maxArraySizeInBytes, maxNumPlayers);
				pNewHandler->Init();

				newHeapUsage = int(CNetwork::GetNetworkHeap()->GetHeapSize() - CNetwork::GetNetworkHeap()->GetMemoryAvailable());

				if (displayMem)
				{
					Displayf("Allocated player array handler size %d, num players %d. Mem %d. Heap used = %d\n", maxArraySizeInBytes, maxNumPlayers, newHeapUsage-currentHeapUsage, newHeapUsage-startHeapUseage);
				}

				m_LocalPlayerBroadcastDataHandlers.PushAndGrow(pNewHandler);

				numInstances--;
			}
		}
	}

	if (displayMem)
	{
		gnetAssertf(0, "Script array handlers memory allocated. See TTY.");
	}

    CNetwork::StopUsingNetworkHeap();
}

void CGameArrayMgr::Shutdown()
{
	netArrayManager_Script::Shutdown();

    CNetwork::UseNetworkHeap(NET_MEM_SCRIPT_ARRAY_HANDLERS);
	for (int i=0; i<m_HostBroadcastDataHandlers.GetCount(); i++)
	{
		m_HostBroadcastDataHandlers[i]->Shutdown();
		delete m_HostBroadcastDataHandlers[i];
	}

    m_HostBroadcastDataHandlers.Reset();

	for (int i=0; i<m_LocalPlayerBroadcastDataHandlers.GetCount(); i++)
	{
		m_LocalPlayerBroadcastDataHandlers[i]->Shutdown();
		delete m_LocalPlayerBroadcastDataHandlers[i];
	}

    m_LocalPlayerBroadcastDataHandlers.Reset();

	CGamePlayerBroadcastDataHandler_Remote::ShutdownPool();
    CNetwork::StopUsingNetworkHeap();
}

netHostBroadcastDataHandlerBase* CGameArrayMgr::GetScriptHostBroadcastDataArrayHandler(const scriptIdBase& scriptId, unsigned dataId)
{
	netBroadcastDataArrayIdentifier<CGameScriptId> identifier(scriptId, NULL, dataId BANK_ONLY(, NULL));
	return static_cast<netHostBroadcastDataHandlerBase*>(GetArrayHandler(HOST_BROADCAST_DATA_ARRAY_HANDLER, &identifier));
}

netPlayerBroadcastDataHandlerBase* CGameArrayMgr::GetScriptPlayerBroadcastDataArrayHandler(const scriptIdBase& scriptId, unsigned dataId, const netPlayer& player)
{
	netBroadcastDataArrayIdentifier<CGameScriptId> identifier(scriptId, &player, dataId BANK_ONLY(, NULL));
	return static_cast<netPlayerBroadcastDataHandlerBase*>(GetArrayHandler(PLAYER_BROADCAST_DATA_ARRAY_HANDLER, &identifier));
}

netPlayerBroadcastDataHandlerBase* CGameArrayMgr::GetScriptPlayerBroadcastDataArrayHandler(const scriptIdBase& scriptId, unsigned dataId, unsigned clientSlot)
{
	netBroadcastDataArrayIdentifier<CGameScriptId> identifier(scriptId, NULL, clientSlot, dataId BANK_ONLY(, NULL));
	return static_cast<netPlayerBroadcastDataHandlerBase*>(GetArrayHandler(PLAYER_BROADCAST_DATA_ARRAY_HANDLER, &identifier));
}

void CGameArrayMgr::RegisterAllGameArrays()
{
	RegisterNewArrayHandler<CPedGroupsArrayHandler>();
	RegisterNewArrayHandler<CIncidentsArrayHandler>();
	RegisterNewArrayHandler<COrdersArrayHandler>();
    RegisterNewArrayHandler<CWorldGridOwnerArrayHandler>();
    RegisterNewArrayHandler<CStickyBombsArrayHandler>();
	RegisterNewArrayHandler<CAnimRequestArrayHandler>();
}

void CGameArrayMgr::ResetAllLocalPlayerBroadcastData()
{
	for (u32 i=0; i<NUM_UPDATE_BATCH_TYPES; i++)
	{
		arrayHandlerBatchList::iterator curr = m_ArrayHandlerBatches[i].begin();
		arrayHandlerBatchList::const_iterator end = m_ArrayHandlerBatches[i].end();

		for(; curr != end; ++curr)
		{
			netArrayHandlerBase* pHandler = *curr;

			if (pHandler->GetHandlerType() == PLAYER_BROADCAST_DATA_ARRAY_HANDLER)
			{
				netScriptBroadcastDataHandlerBase* pPlayerHandler = SafeCast(netScriptBroadcastDataHandlerBase, pHandler);

				if (pPlayerHandler)
				{
					netBroadcastDataArrayIdentifierBase *pIdentifier = SafeCast(netBroadcastDataArrayIdentifierBase, pPlayerHandler->GetIdentifier());

					if (pIdentifier->GetPlayerArbitrator() && pIdentifier->GetPlayerArbitrator()->IsLocal())
					{
						CGamePlayerBroadcastDataHandler_Local* pLocalPlayerHandler = SafeCast(CGamePlayerBroadcastDataHandler_Local, pHandler);

						if (pLocalPlayerHandler)
						{
							pLocalPlayerHandler->ResetBroadcastData();
						}
					}
				}
			}
		}
	}
}

#if __BANK
void CGameArrayMgr::DisplayHostBroadcastDataAllocations()
{
	grcDebugDraw::AddDebugOutput("-- Host broadcast data --");

	for (int i=0; i<m_HostBroadcastDataHandlers.GetCount(); i++)
	{
		CGameHostBroadcastDataHandler* pHandler = m_HostBroadcastDataHandlers[i];

		if (AssertVerify(pHandler))
		{
			netBroadcastDataArrayIdentifier<CGameScriptId>* pIdentifier = pHandler->GetIdentifier();

			if (pHandler->HasArrayData())
			{
				grcDebugDraw::AddDebugOutput("%d: Used by %s. Size : %d / %d. Participants: %d / %d", i, pIdentifier->GetLogName(), pHandler->GetSizeOfArrayDataInBytes(), pHandler->GetMaxSizeOfArrayDataInBytes(), pHandler->GetActualMaxParticipants(), pHandler->GetMaxNumSyncedPlayers());
			}
			else
			{
				grcDebugDraw::AddDebugOutput("%d: **Free**. Max Size : %d. Max participants: %d", i, pHandler->GetMaxSizeOfArrayDataInBytes(), pHandler->GetMaxNumSyncedPlayers());
			}
		}
	}
}

void CGameArrayMgr::DisplayPlayerBroadcastDataAllocations()
{
	grcDebugDraw::AddDebugOutput("-- Player broadcast data --");

	for (int i=0; i<m_LocalPlayerBroadcastDataHandlers.GetCount(); i++)
	{
		CGamePlayerBroadcastDataHandler_Local* pHandler = m_LocalPlayerBroadcastDataHandlers[i];

		if (AssertVerify(pHandler))
		{
			netBroadcastDataArrayIdentifier<CGameScriptId>* pIdentifier = pHandler->GetIdentifier();

			if (pHandler->HasArrayData())
			{
				grcDebugDraw::AddDebugOutput("%d: Used by %s. Size : %d / %d. Participants: %d / %d", i, pIdentifier->GetLogName(), pHandler->GetSizeOfArrayDataInBytes(), pHandler->GetMaxSizeOfArrayDataInBytes(), pHandler->GetActualMaxParticipants(), pHandler->GetMaxNumSyncedPlayers());
			}
			else
			{
				grcDebugDraw::AddDebugOutput("%d: **Free**. Max Size : %d. Max participants: %d", i, pHandler->GetMaxSizeOfArrayDataInBytes(), pHandler->GetMaxNumSyncedPlayers());
			}
		}
	}
}
#endif // __BANK

unsigned CGameArrayMgr::GetNumArrayHandlerTypes() const
{
	return NUM_GAME_ARRAY_HANDLER_TYPES;
}


void CGameArrayMgr::DestroyArrayHandler(netArrayHandlerBase* handler)
{
	SetMemHeapForHandler(*handler);
	delete handler;
	RestoreMemHeap();
}

void CGameArrayMgr::RegisterArrayHandler(netArrayHandlerBase* handler)
{
	SetMemHeapForHandler(*handler);
	netArrayManager_Script::RegisterArrayHandler(handler);
	RestoreMemHeap();
}

void CGameArrayMgr::UnregisterArrayHandler(netArrayHandlerBase* handler, bool bDestroyHandler)
{
	// local broadcast data handlers persist between use
	bool bHandlerPersists = (handler->GetHandlerType() == HOST_BROADCAST_DATA_ARRAY_HANDLER || 
							(handler->GetHandlerType() == PLAYER_BROADCAST_DATA_ARRAY_HANDLER &&
							static_cast<netPlayerBroadcastDataHandlerBase*>(handler)->IsLocal()));

	SetMemHeapForHandler(*handler);

	if (bHandlerPersists)
	{
		bDestroyHandler = false;
	}

	netArrayManager_Script::UnregisterArrayHandler(handler, bDestroyHandler);

	if (bHandlerPersists)
	{
		netScriptBroadcastDataHandlerBase* pHandlerBase = static_cast<netScriptBroadcastDataHandlerBase*>(handler);

		// clear the array data so the handler can be used later by another script
		pHandlerBase->SetArrayData(NULL, 0);
		pHandlerBase->SetPlayerArbitrator(NULL);
		pHandlerBase->SetScriptHandler(NULL);
	}

	RestoreMemHeap();
}

netHostBroadcastDataHandlerBase* CGameArrayMgr::CreateHostBroadcastDataArrayHandler(unsigned* pArray,
																					   unsigned sizeOfArrayInBytes,
																					   const scriptHandler& scriptHandler,
																					   unsigned dataId
																					   BANK_ONLY(, const char* DebugArrayName))
{
	if (!gnetVerifyf(scriptHandler.GetNetworkComponent(), "Trying to create host broadcast data for a non-networked script %s!", scriptHandler.GetLogName()))
	{
		return NULL;
	}

	netHostBroadcastDataHandlerBase* pNewHandler = NULL;

	u32 maxNumParticipants = scriptHandler.GetNetworkComponent()->GetMaxNumParticipants();

	CGameHostBroadcastDataHandler* pFoundHandler = FindBroadcastDataHandler<CGameHostBroadcastDataHandler>(m_HostBroadcastDataHandlers, sizeOfArrayInBytes, maxNumParticipants);

	if (pFoundHandler)
	{
		// setup the identifier for this handler
		netBroadcastDataArrayIdentifier<CGameScriptId>* pIdentifier = pFoundHandler->GetIdentifier();

		pIdentifier->Set(scriptHandler.GetScriptId(), scriptHandler.GetNetworkComponent()->GetHost(), dataId BANK_ONLY(, DebugArrayName));

		// point the handler at the array data. Have to do this after setting up the identifier or the sync data buffers will not be initialised properly 
		// because the array does not think it is locally arbitrated
		pFoundHandler->SetArrayData(reinterpret_cast<CBroadcastDataElement*>(pArray), sizeOfArrayInBytes);

		pFoundHandler->SetScriptHandler(&scriptHandler);

		DEV_ONLY(pFoundHandler->SetActualMaxParticipants(maxNumParticipants));

		pNewHandler = pFoundHandler;
	}
	else
	{
		NETWORK_QUITF(0, "No host broadcast data handler %d available for script %s! (Size : %u, max participants: %d)", dataId, scriptHandler.GetLogName(), sizeOfArrayInBytes, maxNumParticipants);
	}

	return pNewHandler;
}

netPlayerBroadcastDataHandlerBase* CGameArrayMgr::CreatePlayerBroadcastDataArrayHandler(unsigned* pArray,
																						 unsigned sizeOfArrayInBytes,
																						 const scriptHandler& scriptHandler,
																						 unsigned dataId,
																						 unsigned slot
																						 BANK_ONLY(, const char* DebugArrayName))
{
	if (!gnetVerifyf(scriptHandler.GetNetworkComponent(), "Trying to create player broadcast data for a non-networked script %s!", scriptHandler.GetLogName()))
	{
		return NULL;
	}

	netPlayerBroadcastDataHandlerBase* pNewHandler = NULL;

	const netPlayer* player = scriptHandler.GetNetworkComponent()->GetParticipantUsingSlot(slot);

	u32 maxNumParticipants = scriptHandler.GetNetworkComponent()->GetMaxNumParticipants();

	if (player && player->IsLocal())
	{
		if (player->IsBot())
		{
			// allocate an array handler for the bot out of the game heap
			UseGameHeap();
			pNewHandler = rage_new CGamePlayerBroadcastDataHandler_Local(pArray, sizeOfArrayInBytes, scriptHandler, dataId, slot BANK_ONLY(, DebugArrayName));
			RestoreMemHeap();
		}
		else
		{
			CGamePlayerBroadcastDataHandler_Local* pFoundHandler = FindBroadcastDataHandler<CGamePlayerBroadcastDataHandler_Local>(m_LocalPlayerBroadcastDataHandlers, sizeOfArrayInBytes, maxNumParticipants);

			if (pFoundHandler)
			{
				// setup the identifier for this handler
				netBroadcastDataArrayIdentifier<CGameScriptId>* pIdentifier = pFoundHandler->GetIdentifier();

				pIdentifier->Set(scriptHandler.GetScriptId(), player, slot, dataId BANK_ONLY(, DebugArrayName));

				// point the handler at the array data. Have to do this after setting up the identifier or the sync data buffers will not be initialised properly 
				// because the array does not think it is locally arbitrated
				pFoundHandler->SetArrayData(reinterpret_cast<CBroadcastDataElement*>(pArray), sizeOfArrayInBytes);

				pFoundHandler->SetScriptHandler(&scriptHandler);

				DEV_ONLY(pFoundHandler->SetActualMaxParticipants(maxNumParticipants));

				pNewHandler = pFoundHandler;
			}
			else
			{
				NETWORK_QUITF(0, "No local player broadcast data handler %d available for script %s! (Size : %u, max participants: %d)", dataId, scriptHandler.GetLogName(), sizeOfArrayInBytes, maxNumParticipants);
			}
		}
	}
	else
	{
		CNetwork::UseNetworkHeap(NET_MEM_SCRIPT_ARRAY_HANDLERS);
		if (CGamePlayerBroadcastDataHandler_Remote::GetPool()->GetNoOfFreeSpaces() > 0)
		{
			pNewHandler = rage_new CGamePlayerBroadcastDataHandler_Remote(pArray, sizeOfArrayInBytes, scriptHandler, dataId, slot BANK_ONLY(, DebugArrayName));
		}
		else
		{
			NETWORK_QUITF(0, "Ran out of remote player broadcast data handlers, increase size of GamePlayerBroadcastDataHandler_Remote pool in GameConfig.xml");			
		}
		CNetwork::StopUsingNetworkHeap();
	}


	return pNewHandler;
}

void CGameArrayMgr::UseGameHeap()
{
	// save current heap
	m_SaveHeap = &sysMemAllocator::GetCurrent();

	// set game heap as current heap
	sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());

	Displayf("set game heap = %p", &sysMemAllocator::GetCurrent());
}

void CGameArrayMgr::RestoreMemHeap()
{
	if (m_SaveHeap)
	{
		Displayf("restore save heap = %p", m_SaveHeap);
		sysMemAllocator::SetCurrent(*m_SaveHeap);
		m_SaveHeap = 0;
	}
	else
	{
		CNetwork::StopUsingNetworkHeap();
	}
}

void CGameArrayMgr::SetMemHeapForHandler(const netArrayHandlerBase& handler)
{
	bool bUseGameHeap = false;

	// local bots have their player broadcast data array handlers allocated out of the game heap, to avoid running out of network heap memory
	if (handler.GetHandlerType() == PLAYER_BROADCAST_DATA_ARRAY_HANDLER)
	{
		const netBroadcastDataArrayIdentifierBase* pIdentifier = static_cast<const netBroadcastDataArrayIdentifierBase*>(handler.GetIdentifier());

		if (gnetVerify(pIdentifier) && pIdentifier->GetPlayerArbitrator() && pIdentifier->GetPlayerArbitrator()->IsBot() && pIdentifier->GetPlayerArbitrator()->IsLocal())
		{
			bUseGameHeap = true;
		}
	}

	if (bUseGameHeap)
	{
		UseGameHeap();
	}
	else if (handler.GetHandlerType() == PLAYER_BROADCAST_DATA_ARRAY_HANDLER || handler.GetHandlerType() == HOST_BROADCAST_DATA_ARRAY_HANDLER)
	{
		CNetwork::UseNetworkHeap(NET_MEM_SCRIPT_ARRAY_HANDLERS);
	}
	else
	{
		CNetwork::UseNetworkHeap(NET_MEM_GAME_ARRAY_HANDLERS);
	}
}