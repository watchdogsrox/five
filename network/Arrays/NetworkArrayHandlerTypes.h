//
// name:		NetworkArrayHandlerTypes.h
// description:	The different array handler types
// written by:	John Gurney
//

#ifndef NETWORK_ARRAY_HANDLER_TYPES_H
#define NETWORK_ARRAY_HANDLER_TYPES_H

// framework headers
#include "fwnet/netarrayhandlertypes.h"

// game headers
#include "Game/Dispatch/IncidentManager.h"
#include "Game/Dispatch/OrderManager.h"
#include "Network/NetworkInterface.h"
#include "Network/General/NetworkWorldGridOwnership.h"
#include "Network/General/NetworkStreamingRequestManager.h"
#include "Network/General/NetworkSerialisers.h"
#include "PedGroup/PedGroup.h"
#include "Script/Handlers/GameScriptIds.h"
#include "Weapons/Projectiles/ProjectileManager.h"
#include "Weapons/Projectiles/Projectile.h"

class CNetworkArrayMgr;
class CPedGroup;

namespace rage
{
	class datBitBuffer;
}

enum NetworkGameArrayTypes
{
	LOBBY_OPTIONS_ARRAY_HANDLER = NUM_BASE_ARRAY_HANDLER_TYPES,
	PED_GROUPS_ARRAY_HANDLER,
	INCIDENTS_ARRAY_HANDLER,
	ORDERS_ARRAY_HANDLER,
    WORLD_GRID_OWNER_ARRAY_HANDLER,
	STICKY_BOMBS_ARRAY_HANDLER,
	ANIM_REQUEST_HANDLER,
    //TEST_ARRAY_HANDLER,
	NUM_GAME_ARRAY_HANDLER_TYPES,
};

// ================================================================================================================
// CGameHostBroadcastDataHandler
// ================================================================================================================

class CGameHostBroadcastDataHandler : public netHostBroadcastDataHandlerBase
{
public:

	CGameHostBroadcastDataHandler(unsigned* pArray,
								 unsigned sizeOfDataInBytes,
								 const scriptHandler& scriptHandler,
								 unsigned dataId
								 BANK_ONLY(, const char* debugArrayName))
	: netHostBroadcastDataHandlerBase(pArray, sizeOfDataInBytes, scriptHandler)
	, m_Identifier(scriptHandler.GetScriptId(), scriptHandler.GetNetworkComponent()->GetHost(), dataId BANK_ONLY(, debugArrayName))
#if __BANK
	,m_actualMaxParticipants(0)
	,m_cloningTimer(0)
	,m_HasPrintedTTY(false)
#endif
	{
		// the script id must have a timestamp to distinguish it from other scripts that have ran in the past
		gnetAssert(static_cast<CGameScriptId&>(m_Identifier.GetScriptId()).GetTimeStamp() != 0);
	}

	CGameHostBroadcastDataHandler(unsigned sizeOfDataInBytes, unsigned maxNumPlayers)
	: netHostBroadcastDataHandlerBase(sizeOfDataInBytes, maxNumPlayers)
#if __BANK
	,m_actualMaxParticipants(0)
	,m_cloningTimer(0)
	,m_HasPrintedTTY(false)
#endif
	{
	}

	virtual netBroadcastDataArrayIdentifier<CGameScriptId>* GetIdentifier()				{ return &m_Identifier; }
	virtual const netBroadcastDataArrayIdentifier<CGameScriptId>* GetIdentifier() const	{ return &m_Identifier; }
	virtual netBroadcastDataArrayIdentifier<CGameScriptId>* GetStaticIdentifier()		{ return &sm_StaticIdentifier; }

	virtual void Init();
	virtual bool CanSendUpdate(const netPlayer& player) const;
	virtual void SendChecksumToNewArbitrator();
	virtual bool ReadUpdate(const netPlayer& player, datBitBuffer& bitBuffer, unsigned dataSize, netSequence updateSeq);

#if __BANK
	void SetActualMaxParticipants(u32 mp) { m_actualMaxParticipants = mp; }
	u32 GetActualMaxParticipants() const {  return m_actualMaxParticipants; }

	static const char* GetDebugTypeName(){ return "HOST"; }
#endif

private:

	static netBroadcastDataArrayIdentifier<CGameScriptId>	sm_StaticIdentifier;
	netBroadcastDataArrayIdentifier<CGameScriptId>			m_Identifier;

#if __BANK
	u32 m_actualMaxParticipants;
	mutable u32	m_cloningTimer;
	mutable bool m_HasPrintedTTY;
#endif
};

// ================================================================================================================
// CGamePlayerBroadcastDataHandler_Remote
// ================================================================================================================

class CGamePlayerBroadcastDataHandler_Remote : public netPlayerBroadcastDataHandlerBase
{
public:

	CGamePlayerBroadcastDataHandler_Remote(unsigned* pArray,
									unsigned sizeOfDataInBytes,
									const scriptHandler& scriptHandler,
									unsigned dataId,
									unsigned index
									BANK_ONLY(, const char* debugArrayName))
	: netPlayerBroadcastDataHandlerBase(pArray, sizeOfDataInBytes, scriptHandler)
	, m_Identifier(scriptHandler.GetScriptId(), scriptHandler.GetNetworkComponent()->GetParticipantUsingSlot(index), index, dataId BANK_ONLY(, debugArrayName))
	, m_bArbitratorChanged(false)
#if __DEV
	, m_pBDBackupBuffer(NULL)
#endif //DEV
	{
		// the script id must have a timestamp to distinguish it from other scripts that have ran in the past
		gnetAssert(static_cast<CGameScriptId&>(m_Identifier.GetScriptId()).GetTimeStamp() != 0);
	}

	FW_REGISTER_CLASS_POOL(CGamePlayerBroadcastDataHandler_Remote);

	virtual void Init();
	virtual void Shutdown();
	virtual bool ReadUpdate(const netPlayer& player, datBitBuffer& bitBuffer, unsigned dataSize, netSequence updateSeq);

	virtual netBroadcastDataArrayIdentifier<CGameScriptId>* GetIdentifier()				{ return &m_Identifier; }
	virtual const netBroadcastDataArrayIdentifier<CGameScriptId>* GetIdentifier() const	{ return &m_Identifier; }
	virtual netBroadcastDataArrayIdentifier<CGameScriptId>* GetStaticIdentifier()		{ return &sm_StaticIdentifier; }

	virtual void SetPlayerArbitrator(const netPlayer* player);

#if __DEV
	//PURPOSE
	//	If a script requires any local changes of remote BD to be tracked create the required buffer.
	virtual void CreateBDBackupBuffer();
	//PURPOSE
	//	If a script previously created a buffer to track local changes to remote BD remove here.
	virtual void RemoveBDBackupBuffer();

	u8* GetBDBackupBuffer()		{ return m_pBDBackupBuffer; }

private:
	u8*								m_pBDBackupBuffer;							// the raw buffer used by the shadow buffer

#endif //DEV

private:

	static netBroadcastDataArrayIdentifier<CGameScriptId>	sm_StaticIdentifier;
	netBroadcastDataArrayIdentifier<CGameScriptId>			m_Identifier;

	bool m_bArbitratorChanged : 1;
};

// ================================================================================================================
// CGamePlayerBroadcastDataHandler_Local
// ================================================================================================================

class CGamePlayerBroadcastDataHandler_Local : public netPlayerBroadcastDataHandlerBase_Local
{
public:

	CGamePlayerBroadcastDataHandler_Local(unsigned* pArray,
											unsigned sizeOfDataInBytes,
											const scriptHandler& scriptHandler,
											unsigned dataId,
											unsigned index
											BANK_ONLY(, const char* debugArrayName ));

	CGamePlayerBroadcastDataHandler_Local(unsigned sizeOfDataInBytes,
										  unsigned maxNumPlayers)
		: netPlayerBroadcastDataHandlerBase_Local(sizeOfDataInBytes, maxNumPlayers)
		, m_initialStateBuffer(NULL)
	{
	}

	~CGamePlayerBroadcastDataHandler_Local() { gnetAssert(m_initialStateBuffer==NULL); }

	virtual void Init();
	virtual void Shutdown();
	virtual bool ReadUpdate(const netPlayer& player, datBitBuffer& bitBuffer, unsigned dataSize, netSequence updateSeq);

	virtual void SetArrayData(CBroadcastDataElement* arrayData, unsigned sizeOfArrayInBytes); 
	
	virtual netBroadcastDataArrayIdentifier<CGameScriptId>* GetIdentifier()				{ return &m_Identifier; }
	virtual const netBroadcastDataArrayIdentifier<CGameScriptId>* GetIdentifier() const	{ return &m_Identifier; }
	virtual netBroadcastDataArrayIdentifier<CGameScriptId>* GetStaticIdentifier()		{ return &sm_StaticIdentifier; }

	u8* GetInitialStateBuffer() const { return m_initialStateBuffer; }

	void ResetBroadcastData();

#if __BANK
	void SetActualMaxParticipants(u32 mp) { m_actualMaxParticipants = mp; }
	u32 GetActualMaxParticipants() const {  return m_actualMaxParticipants; }

	static const char* GetDebugTypeName(){ return "PLAYER"; }
#endif

private:

	static netBroadcastDataArrayIdentifier<CGameScriptId>	sm_StaticIdentifier;
	netBroadcastDataArrayIdentifier<CGameScriptId>			m_Identifier;

	u8*	m_initialStateBuffer;	// Stores the initial state of the broadcast data. Used to reset the broadcast data of leaving players.

#if __BANK
	u32 m_actualMaxParticipants;
#endif
};

// ================================================================================================================
// CPedGroupsArrayHandler - Deals with an array of CPedGroup
// ================================================================================================================

class CPedGroupsArrayHandler : public netSharedNetObjArrayHandler<CPedGroup, CPedGroupsArrayHandler>
{
private:

	static const unsigned int MAX_ELEMENTS	= CPedGroups::MAX_NUM_GROUPS - MAX_NUM_PHYSICAL_PLAYERS; // player groups are synced via the sync nodes

public:
	CPedGroupsArrayHandler():
	  netSharedNetObjArrayHandler<CPedGroup, CPedGroupsArrayHandler>(PED_GROUPS_ARRAY_HANDLER, CPedGroups::ms_groups+MAX_NUM_PHYSICAL_PLAYERS, MAX_ELEMENTS)
	  {
	  }

	const char* GetHandlerName() const { return "PED_GROUPS"; }

protected:

	netSyncDataBase* GetSyncData() { return &m_syncData; }
	const netSyncDataBase* GetSyncData() const { return &m_syncData; }

	const netPlayer* GetTargetElementArbitration(unsigned index) const;

	virtual void ExtractDataForSerialising(unsigned index);
	virtual void ApplyElementData(unsigned index, const netPlayer& player);

	virtual bool CanHaveEmptyElements() const { return true; }
	virtual bool IsElementEmpty(unsigned index) const;
	virtual void SetElementEmpty(unsigned index);

	virtual void SetElementArbitration(unsigned index, const netPlayer& player);

	virtual bool ElementCanBeCleared(unsigned index) const;

private:

	// converts an array handler index to its corresponding ped groups index (the array handler only deals with non-player groups)
	unsigned GetPedGroupsArrayIndex(unsigned index);

	netSyncData_Static_NoBuffers<MAX_NUM_PHYSICAL_PLAYERS, MAX_ELEMENTS, 0> m_syncData;

};

// ================================================================================================================
// CIncidentsArrayHandler - Deals with an array of CIncident
// ================================================================================================================

class CIncidentsArrayHandler : public netSharedNetObjArrayHandler<CIncidentManager::CIncidentPtr, CIncidentsArrayHandler>
{
public:
	
	static const unsigned WANTED_INCIDENT_SCOPE_SQR = 600*600; 
	static const unsigned int MAX_ELEMENTS	= CIncidentManager::MAX_INCIDENTS;


	CIncidentsArrayHandler():
	  netSharedNetObjArrayHandler<CIncidentManager::CIncidentPtr, CIncidentsArrayHandler>(INCIDENTS_ARRAY_HANDLER, &CIncidentManager::GetInstance().m_Incidents[0], MAX_ELEMENTS)
	  {
	  }

	~CIncidentsArrayHandler()
	{
		sysMemAllocator* currHeap = &sysMemAllocator::GetCurrent();

		// make sure the game heap is the current heap when destroying the incident, it will have been allocated out of that heap
		sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());

		m_Element.Clear();

		sysMemAllocator::SetCurrent(*currHeap);
	}

	const char* GetHandlerName() const { return "INCIDENTS"; }

	static u32 GetSizeOfIncidentID() { return SIZEOF_OBJECTID + 8; } // includes the player's incident num

	virtual u32 GetSizeOfElementID() const { return GetSizeOfIncidentID(); } 

	virtual u32 GetElementID(unsigned index) const
	{
		netObject* pNetObj = this->m_Array[index].GetNetworkObject();

		gnetAssertf(pNetObj, "No network object for slot %d in shared array %s", index, this->GetHandlerName());

		if (pNetObj)
		{
			return CIncident::GetIncidentIDFromNetworkObjectID(*pNetObj);
		}
		else
		{
			return this->INVALID_ELEMENT_ID;
		}
	}

	virtual bool SetElementDirty(unsigned index)
	{
		netObject* pNetObj = this->m_Array[index].GetNetworkObject();

		if (pNetObj && pNetObj->GetObjectType() == NET_OBJ_TYPE_PLAYER)
		{
			// skip the netSharedNetObjArrayHandler implementation as it contains an assert that will fire for players with > 1 wanted incident
			return netSharedArrayHandlerWithElementScope<CIncidentManager::CIncidentPtr, CIncidentsArrayHandler>::SetElementDirty(index);
		}
		else
		{
			return netSharedNetObjArrayHandler<CIncidentManager::CIncidentPtr, CIncidentsArrayHandler>::SetElementDirty(index);
		}
	}

	virtual bool CanApplyElementData(unsigned index, const netPlayer& player, bool elementEmpty) 
	{
		bool bCanApplyData = true;

		if (!elementEmpty)
		{
			netLoggingInterface &log = netInterface::GetArrayManager().GetLog();
			netLogSplitter logSplitter(netInterface::GetMessageLog(), log);

			netObject* pNetObj = netInterface::GetNetworkObject( CIncident::GetNetworkObjectIDFromIncidentID(this->m_ElementID) );

			if (!pNetObj)
			{
				logSplitter.WriteDataValue("FAIL", "Network object does not exist");
				bCanApplyData = false;
			}
			else if (!pNetObj->GetPlayerOwner())
			{
				logSplitter.WriteDataValue("FAIL", "%s has no player owner", pNetObj->GetLogName());
				bCanApplyData = false;
			}
			else if (pNetObj->GetPlayerOwner() != &player)
			{
				logSplitter.WriteDataValue("FAIL", "%s is owned by %s", pNetObj->GetLogName(), pNetObj->GetPlayerOwner() ? pNetObj->GetPlayerOwner()->GetLogName() : "Unknown player");
				bCanApplyData = false;
			}
		}

		if (bCanApplyData)
		{
			bCanApplyData = netSharedArrayHandlerWithElementScope<CIncidentManager::CIncidentPtr, CIncidentsArrayHandler>::CanApplyElementData(index, player, elementEmpty);
		}

		if (!bCanApplyData)
		{
			m_Element.Clear();
		}

		return bCanApplyData;
	}

	// used by orders array handler
	bool IsElementInScopeAndSyncedWithPlayer(unsigned index, const netPlayer& player) const;
	
#if __BANK
	void DisplayDebugInfo();
#endif

protected:

	netSyncDataBase* GetSyncData() { return &m_syncData; }
	const netSyncDataBase* GetSyncData() const { return &m_syncData; }

	virtual void ExtractDataForSerialising(unsigned index);
	virtual void ApplyElementData(unsigned index, const netPlayer& player);

	virtual bool IsElementInScope(unsigned index, const netPlayer& player) const;
	virtual bool CanHaveEmptyElements() const { return true; }
	virtual bool IsElementEmpty(unsigned index) const;
	virtual void SetElementEmpty(unsigned index);
	virtual void ElementScopeChanged(unsigned index);

	virtual bool AreElementsAlwaysInScope() const { return false; }
	virtual void SetElementArbitration(unsigned index, const netPlayer& player);

	virtual void WriteElement(datBitBuffer& bitBuffer, unsigned index, netLoggingInterface* pLog = NULL);

	virtual bool RemoveElementWhenArbitratorLeaves(unsigned index) const;

private:

	netSyncData_Static_NoBuffers<MAX_NUM_PHYSICAL_PLAYERS, MAX_ELEMENTS, 0> m_syncData;
};

// ================================================================================================================
// COrdersArrayHandler - Deals with an array of COrder
// ================================================================================================================

class COrdersArrayHandler : public netSharedNetObjArrayHandler<COrderManager::COrderPtr, COrdersArrayHandler>
{
private:

	static const unsigned int MAX_ELEMENTS	= COrderManager::MAX_ORDERS;

public:
	COrdersArrayHandler():
	  netSharedNetObjArrayHandler<COrderManager::COrderPtr, COrdersArrayHandler>(ORDERS_ARRAY_HANDLER, &COrderManager::GetInstance().m_Orders[0], MAX_ELEMENTS)
	  {
	  }

	~COrdersArrayHandler()
	{
		sysMemAllocator* currHeap = &sysMemAllocator::GetCurrent();

		// make sure the game heap is the current heap when destroying the order, it will have been allocated out of that heap
		sysMemAllocator::SetCurrent(sysMemAllocator::GetMaster());

		m_Element.Clear();

		sysMemAllocator::SetCurrent(*currHeap);
	}

	const char* GetHandlerName() const { return "ORDERS"; }

	void Init();
	void PlayerHasLeft(const netPlayer& player);

	void ClearAllOrdersForIncident(unsigned incidentID);
	void IncidentHasMigrated(CIncident& incident);
	void IncidentScopeHasChanged(CIncident& incident);

#if __BANK
	u32 GetNumOrdersForIncident(CIncident& incident, u32& numAssigned, u32& numEmpty);
#endif

protected:

	netSyncDataBase* GetSyncData() { return &m_syncData; }
	const netSyncDataBase* GetSyncData() const { return &m_syncData; }

	// orders migrate with their incident so do not need to check for migration independently
	virtual bool ElementsCanMigrate() const { return false; }
	virtual const netPlayer* GetTargetElementArbitration(unsigned index) const { return GetElementArbitration(index); }

	virtual void ExtractDataForSerialising(unsigned index);
	virtual void ApplyElementData(unsigned index, const netPlayer& player);

	virtual void ClearElementArbitration(unsigned index);
	virtual bool CanHaveEmptyElements() const { return true; }
	virtual bool IsElementEmpty(unsigned index) const;
	virtual void SetElementEmpty(unsigned index);

	virtual bool AreElementsAlwaysInScope() const { return false; }
	virtual bool IsElementInScope(unsigned index, const netPlayer& player) const;

	virtual bool CanApplyElementData(unsigned index, const netPlayer& player, bool elementEmpty);

	virtual void WriteElement(datBitBuffer& bitBuffer, unsigned index, netLoggingInterface* pLog = NULL);

	virtual bool ElementCanBeCleared(unsigned index) const;

private:

	netSyncData_Static_NoBuffers<MAX_NUM_PHYSICAL_PLAYERS, MAX_ELEMENTS, 0> m_syncData;

	// player flags for each element indicating whether the order's incident has been created on that player's machine
	mutable PlayerFlags m_incidentCreated[MAX_ELEMENTS];
};

// ================================================================================================================
// CStickyBombsArrayHandler - Deals with an array of CProjectilesPtr containing reference to CProjectiles
// ================================================================================================================

class CStickyBombsArrayHandler : public netSharedArrayHandler<CProjectileManager::CProjectilePtr, CStickyBombsArrayHandler>
{
private:

	static const unsigned int MAX_ELEMENTS	= CProjectileManager::MAX_STORAGE; 

public:
	CStickyBombsArrayHandler():
	  netSharedArrayHandler<CProjectileManager::CProjectilePtr, CStickyBombsArrayHandler>(STICKY_BOMBS_ARRAY_HANDLER, CProjectileManager::ms_NetSyncedProjectiles.GetElements(), MAX_ELEMENTS)
	  {
	  }

	const char* GetHandlerName() const { return "STICKY_BOMBS"; }

	void ResendAllStickyBombsToPlayer(const netPlayer& player);

protected:

	netSyncDataBase* GetSyncData() { return &m_syncData; }
	const netSyncDataBase* GetSyncData() const { return &m_syncData; }

	virtual void ExtractDataForSerialising(unsigned index);
	virtual void ApplyElementData(unsigned index, const netPlayer& player);

	virtual bool CanHaveEmptyElements() const { return true; }
	virtual bool IsElementEmpty(unsigned index) const;
	virtual void SetElementEmpty(unsigned index);
	virtual bool CanApplyElementData(unsigned index, const netPlayer& player, bool elementEmpty);

	virtual bool MoveElementToFreeSlot(unsigned index, unsigned& newSlot);

private:

	netSyncData_Static_NoBuffers<MAX_NUM_PHYSICAL_PLAYERS, MAX_ELEMENTS, 0> m_syncData;

};


// ================================================================================================================
// CWorldGridOwnerArrayHandler - Deals with syncing the the array of world grid squares owned by players in the session
// ================================================================================================================

class CWorldGridOwnerArrayHandler : public netHostArrayHandler<NetworkGridSquareInfo, CWorldGridOwnerArrayHandler>
{
public:

    static const unsigned NUM_ELEMENTS              = CNetworkWorldGridManager::MAX_GRID_SQUARES;
	static const unsigned MAX_ELEMENT_SIZE_IN_BITS  = 24;
    static const unsigned MAX_ELEMENT_SIZE_IN_BYTES = ((MAX_ELEMENT_SIZE_IN_BITS+7)>>3);

public:

	CWorldGridOwnerArrayHandler()
        : netHostArrayHandler<NetworkGridSquareInfo, CWorldGridOwnerArrayHandler>(WORLD_GRID_OWNER_ARRAY_HANDLER, CNetworkWorldGridManager::m_GridSquareInfos, NUM_ELEMENTS)
	{
	}

	const char* GetHandlerName() const { return "WORLD_GRID_OWNER"; }

	virtual bool CanHaveEmptyElements() const { return true; }

	virtual bool IsElementEmpty(unsigned index) const
	{
		return (m_Array[index].m_Owner == INVALID_PLAYER_INDEX);

	}

	virtual void SetElementEmpty(unsigned index)
	{
		m_Array[index].Reset();
	}

protected:

	netSyncDataBase* GetSyncData() { return &m_syncData; }
	const netSyncDataBase* GetSyncData() const { return &m_syncData; }

private:

	netSyncData_Static_NoBuffers<MAX_NUM_PHYSICAL_PLAYERS, NUM_ELEMENTS, 0> m_syncData;
};

// ================================================================================================================
// CAnimRequestArrayHandler - Deals with storing and syncing animation requests made by tasks and script so they get loaded in at the same time....
// ================================================================================================================

class CAnimRequestArrayHandler : public netSharedNetObjArrayHandler<CNetworkStreamingRequestManager::NetworkAnimRequestData, CAnimRequestArrayHandler>
{
public:

	static const unsigned int MAX_ELEMENTS = CNetworkStreamingRequestManager::MAX_NETWORK_ANIM_REQUESTS;
	
	CAnimRequestArrayHandler():
		netSharedNetObjArrayHandler<CNetworkStreamingRequestManager::NetworkAnimRequestData, CAnimRequestArrayHandler>(ANIM_REQUEST_HANDLER, CNetworkStreamingRequestManager::ms_NetSyncedAnimRequests.GetElements(), CNetworkStreamingRequestManager::MAX_NETWORK_ANIM_REQUESTS)
	{
	}

	const char* GetHandlerName() const { return "ANIM_REQUESTS"; }

protected:

	netSyncDataBase* GetSyncData() { return &m_syncData; }
	const netSyncDataBase* GetSyncData() const { return &m_syncData; }

	virtual void ExtractDataForSerialising(unsigned index);
	virtual void ApplyElementData(unsigned index, const netPlayer& player);

	virtual bool CanHaveEmptyElements() const { return true; }
	virtual bool IsElementEmpty(unsigned index) const;
	virtual void SetElementEmpty(unsigned index);
	virtual bool ElementCanBeCleared(unsigned index) const;

private:

	netSyncData_Static_NoBuffers<MAX_NUM_PHYSICAL_PLAYERS, MAX_ELEMENTS, 0> m_syncData;

};

#endif  // NETWORK_ARRAY_HANDLER_TYPES_H
