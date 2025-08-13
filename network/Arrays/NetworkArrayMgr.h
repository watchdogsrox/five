//
// netArrayMgr.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef NETWORK_ARRAY_MGR_H
#define NETWORK_ARRAY_MGR_H

// framework headers
#include "fwnet/netarraymgr.h"

// game headers
#include "game/config.h"
#include "network/Arrays/NetworkArrayHandlerTypes.h"
#include "network/Network.h"

class CLobbyOptionsArrayHandler;
class CPedGroupsArrayHandler;
class CIncidentsArrayHandler;
class CPopulationStreamingArrayHandler;
class CStickyBombsArrayHandler;
class CAnimRequestArrayHandler;

// ================================================================================================================
// CGameArrayMgr
// ================================================================================================================

class CGameArrayMgr : public netArrayManager_Script
{
public:

	static const unsigned MAX_NUM_LOCAL_BROADCAST_DATA_HANDLERS = CConfigNetScriptBroadcastData::NUM_BROADCAST_DATA_ENTRIES;
	static const unsigned MAX_NUM_REMOTE_BROADCAST_DATA_HANDLERS = 200;

public:

	CGameArrayMgr(netBandwidthMgr &bandwidthMgr) :
	netArrayManager_Script(bandwidthMgr),
	m_HostBroadcastDataHandlers(MAX_NUM_LOCAL_BROADCAST_DATA_HANDLERS),
	m_LocalPlayerBroadcastDataHandlers(MAX_NUM_LOCAL_BROADCAST_DATA_HANDLERS)
	{
	}

	virtual void Init();
	virtual void Shutdown();

	netHostBroadcastDataHandlerBase* GetScriptHostBroadcastDataArrayHandler(const scriptIdBase& scriptId, unsigned dataId);
	netPlayerBroadcastDataHandlerBase* GetScriptPlayerBroadcastDataArrayHandler(const scriptIdBase& scriptId, unsigned dataId, const netPlayer& player);
	netPlayerBroadcastDataHandlerBase* GetScriptPlayerBroadcastDataArrayHandler(const scriptIdBase& scriptId, unsigned dataId, unsigned clientSlot);

	CPedGroupsArrayHandler*		 GetPedGroupsArrayHandler()	  { return static_cast<CPedGroupsArrayHandler*>(GetArrayHandler(PED_GROUPS_ARRAY_HANDLER));}
	CIncidentsArrayHandler*		 GetIncidentsArrayHandler()	  { return static_cast<CIncidentsArrayHandler*>(GetArrayHandler(INCIDENTS_ARRAY_HANDLER)); }
	COrdersArrayHandler*		 GetOrdersArrayHandler()		  { return static_cast<COrdersArrayHandler*>(GetArrayHandler(ORDERS_ARRAY_HANDLER)); }
    CWorldGridOwnerArrayHandler* GetWorldGridOwnerArrayHandler() { return static_cast<CWorldGridOwnerArrayHandler*>(GetArrayHandler(WORLD_GRID_OWNER_ARRAY_HANDLER)); }
	CStickyBombsArrayHandler*	 GetStickyBombsArrayHandler() { return static_cast<CStickyBombsArrayHandler*>(GetArrayHandler(STICKY_BOMBS_ARRAY_HANDLER)); }
	CAnimRequestArrayHandler*	 GetAnimRequestArrayHandler() { return static_cast<CAnimRequestArrayHandler*>(GetArrayHandler(ANIM_REQUEST_HANDLER)); }
 
	// array registering
	void RegisterAllGameArrays();

	// called when a player switches teams: restores all player broadcast data to its default state
	void ResetAllLocalPlayerBroadcastData();

#if __BANK
	void DisplayHostBroadcastDataAllocations();
	void DisplayPlayerBroadcastDataAllocations();
#endif

protected:

	unsigned GetNumArrayHandlerTypes() const;

	template<class HandlerType>
	void RegisterNewArrayHandler()
	{
		DEV_ONLY(int initHeapUsage = int(CNetwork::GetNetworkHeap()->GetHeapSize() - CNetwork::GetNetworkHeap()->GetMemoryAvailable()));

		CNetwork::UseNetworkHeap(NET_MEM_GAME_ARRAY_HANDLERS);

		netArrayHandlerBase *newArrayHandler = rage_new HandlerType();

		netArrayManager_Script::RegisterArrayHandler( newArrayHandler );

		CNetwork::StopUsingNetworkHeap();

#if __DEV
		int currHeapUsage = int(CNetwork::GetNetworkHeap()->GetHeapSize() - CNetwork::GetNetworkHeap()->GetMemoryAvailable());
		Displayf("Registered array handler %s, size = %d (heap: %d/%" SIZETFMT "d)\r\n", newArrayHandler->GetHandlerName(), currHeapUsage-initHeapUsage, currHeapUsage, CNetwork::GetNetworkHeap()->GetHeapSize());
#endif //__DEV
	}

	// called from the framework array mgr:
	void RegisterArrayHandler(netArrayHandlerBase* handler);
	void UnregisterArrayHandler(netArrayHandlerBase* handler, bool bDestroyHandler = true);

	netHostBroadcastDataHandlerBase* CreateHostBroadcastDataArrayHandler(unsigned* pArray,
																			unsigned sizeOfArrayInBytes,
																			const scriptHandler& scriptHandler,
																			unsigned dataId
																			BANK_ONLY(, const char* debugArrayName));

	netPlayerBroadcastDataHandlerBase* CreatePlayerBroadcastDataArrayHandler(unsigned* pArray,
																			unsigned sizeOfArrayInBytes,
																			const scriptHandler& scriptHandler,
																			unsigned dataId,
																			unsigned slot
																			BANK_ONLY(, const char* debugArrayName));

private:

    //PURPOSE
    // Destroys the specified network array handler
    virtual void DestroyArrayHandler(netArrayHandlerBase* handler);

	// PURPOSE
	// Some heap stuff for use with network bots, which allocate their local player broadcast data array handlers out of the game heap
	void UseGameHeap();
	void RestoreMemHeap();
	void SetMemHeapForHandler(const netArrayHandlerBase& handler);

	template<class T, class A>
	T* FindBroadcastDataHandler(A& bdArray, unsigned sizeOfArrayInBytes, unsigned maxNumParticipants)
	{
		T* pFoundHandler = NULL;

		// find the most appropriate handler. Favour the smallest handler that can accommodate our data and number of players
		for (int i=0; i<bdArray.GetCount(); i++)
		{
			T* pHandler = bdArray[i];

			// is the handler already in use?
			if (!pHandler->HasArrayData())
			{
				if (pHandler->GetMaxSizeOfArrayDataInBytes() >= sizeOfArrayInBytes &&
					pHandler->GetMaxNumSyncedPlayers() >= maxNumParticipants)
				{
					if (pFoundHandler)
					{
						if ((pHandler->GetMaxSizeOfArrayDataInBytes() < pFoundHandler->GetMaxSizeOfArrayDataInBytes()) ||
							(pHandler->GetMaxNumSyncedPlayers() < pFoundHandler->GetMaxNumSyncedPlayers()))
						{
							pFoundHandler = pHandler;
						}
					}
					else
					{
						pFoundHandler = pHandler;
					}
				}
			}
		}

#if __BANK
		if (!pFoundHandler)
		{
			for (int i=0; i<bdArray.GetCount(); i++)
			{
				T* pHandler = bdArray[i];

				netBroadcastDataArrayIdentifier<CGameScriptId>* pIdentifier = pHandler->GetIdentifier();

				if (pHandler->HasArrayData())
				{
					Displayf("%s: Broadcast handler %d: Used by script %s (Size : %d, Max Size : %d, max participants: %d)\n", T::GetDebugTypeName(), i, pIdentifier->GetLogName(), pHandler->GetSizeOfArrayDataInBytes(), pHandler->GetMaxSizeOfArrayDataInBytes(), pHandler->GetMaxNumSyncedPlayers());
				}
				else
				{
					Displayf("%s: Broadcast handler %d: **Free** (Max Size : %d, max participants: %d)\n", T::GetDebugTypeName(), i, pHandler->GetMaxSizeOfArrayDataInBytes(), pHandler->GetMaxNumSyncedPlayers());
				}
			}
		}
#endif

		return pFoundHandler;
	}

	atArray<CGameHostBroadcastDataHandler*> m_HostBroadcastDataHandlers;
	atArray<CGamePlayerBroadcastDataHandler_Local*> m_LocalPlayerBroadcastDataHandlers;

	// a pointer to the current heap, which is used when calling SetMemHeapForHandler
	static sysMemAllocator* m_SaveHeap;
};

#endif  // NETWORK_ARRAY_MGR_H
