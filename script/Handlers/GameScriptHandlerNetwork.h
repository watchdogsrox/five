//
// name:		NetGameScriptHandlerNetwork.h
// description:	Project specific script handler, with network component
// written by:	John Gurney
//

#ifndef GAME_SCRIPT_HANDLER_NETWORK_H
#define GAME_SCRIPT_HANDLER_NETWORK_H

// framework includes
#include "fwscript/scriptHandler.h"
#include "fwscript/scriptHandlerNetComponent.h"

// game includes
#include "script/handlers/GameScriptHandler.h"

class CGameScriptHandlerNetComponent;
class CPhysical;
class CNetGamePlayer;
class CNetObjGame;

class CGameScriptHandlerNetwork : public CGameScriptHandler
{
	friend CGameScriptHandlerNetComponent;

public:

	CGameScriptHandlerNetwork(scrThread& scriptThread);

	FW_REGISTER_CLASS_POOL(CGameScriptHandlerNetwork);

	virtual bool RequiresANetworkComponent() const { return true; }
	virtual void CreateNetworkComponent();
	virtual void DestroyNetworkComponent();

	virtual bool Update();
	virtual void Shutdown();
	virtual bool Terminate();

	virtual bool RegisterNewScriptObject(scriptHandlerObject &object, bool hostObject, bool network);
	virtual void UnregisterScriptObject(scriptHandlerObject &object);

	void SetMaxNumParticipants(int n);
	int  GetMaxNumParticipants() const;

	void ReserveLocalPeds(int n);
	void ReserveLocalVehicles(int n);
	void ReserveLocalObjects(int n);
	void ReserveGlobalPeds(int n);
	void ReserveGlobalVehicles(int n);
	void ReserveGlobalObjects(int n);

	u32 GetNumLocalReservedPeds() const			{ return m_NumLocalReservedPeds; }
	u32 GetNumLocalReservedVehicles() const		{ return m_NumLocalReservedVehicles; }
	u32 GetNumLocalReservedObjects() const		{ return m_NumLocalReservedObjects; }
	u32 GetNumGlobalReservedPeds() const		{ return m_NumGlobalReservedPeds; }
	u32 GetNumGlobalReservedVehicles() const	{ return m_NumGlobalReservedVehicles; }
	u32 GetNumGlobalReservedObjects() const		{ return m_NumGlobalReservedObjects; }
	u32 GetTotalNumReservedPeds() const			{ return m_NumLocalReservedPeds + m_NumGlobalReservedPeds; }
	u32 GetTotalNumReservedVehicles() const		{ return m_NumLocalReservedVehicles + m_NumGlobalReservedVehicles; }
	u32 GetTotalNumReservedObjects() const		{ return m_NumLocalReservedObjects + m_NumGlobalReservedObjects; }
	u32 GetNumCreatedPeds() const				{ return m_NumCreatedPeds; }
	u32 GetNumCreatedVehicles() const			{ return m_NumCreatedVehicles; }
	u32 GetNumCreatedObjects() const			{ return m_NumCreatedObjects; }

	// returns the number of entities that the script is potentially going to create (the object pop mgr needs to create space for these objects)
    unsigned GetNumRequiredEntities() const;
	void GetNumRequiredEntities(u32& numRequiredPeds, u32 &numRequiredVehicles, u32 &numRequiredObjects) const;

	void AcceptPlayers(bool bAccept);

	// called when the network component enters a playing state.
	void NetworkGameStarted();

	// TRUE if the network script is safe in a single player environment.
	bool ActiveInSinglePlayer() const	{ return m_ActiveInSinglePlayer; }
	void SetActiveInSinglePlayer()		{ m_ActiveInSinglePlayer = true; }

	void SetInMPCutscene(bool bIn)		{ m_InMPCutscene = bIn; }
	bool GetInMPCutscene() const		{ return m_InMPCutscene; }

	// returns true if the given object belonging to this script handler can migrate to the given player (which may be our own)
	bool CanAcceptMigratingObject(scriptHandlerObject &object, const netPlayer& receivingPlayer, eMigrationType migrationType) const;

	// cleans up any client created script objects associated with a participant slot 
	void CleanupAllClientObjects(ScriptSlotNumber clientSlot);

	// migrates important script objects to other participants when the handler is shutting down
	bool MigrateObjects();

#if __DEV
	// displays debug info on this handler
	virtual void DisplayScriptHandlerInfo() const;

	void MarkCanRegisterMissionEntitiesCalled(u32 numPeds, u32 numVehicles, u32 numObjects);
#endif

#if __BANK
	void DebugPrintUnClonedEntities();
#endif // __BANK

	void ValidateRegistrationRequest(CPhysical* pTargetEntity, bool bNewObject);

	bool HaveAllScriptEntitiesFinishedCloning() const;

protected:

	virtual ScriptResourceId GetNextFreeResourceId(scriptResource& resource);

	//PURPOSE
	// Finds all objects that should be bound to this handler and adds them to the object list. 
	void RegisterAllExistingScriptObjects();

	void RegisterExistingScriptObjectInternal(scriptHandlerObject &object);

	void RemoveOldScriptObject(scriptHandlerObject &object);

	//PURPOSE
	// Removes all objects that have a host id higher than our current one
	void RemoveOldHostObjects();

protected:

	// sequences have their own id system, so that the ids match across the network when the order of sequences is defined in the same 
	// order by the script, irrespective of other resource allocations.
	ScriptResourceId m_nextFreeSequenceId;

	// cached network information for when a network game is not running (currently not required)
	u8			m_MaxNumParticipants;

	u8			m_NumLocalReservedPeds;
	u8			m_NumLocalReservedVehicles;
	u8			m_NumLocalReservedObjects;
	u8			m_NumGlobalReservedPeds;
	u8			m_NumGlobalReservedVehicles;
	u8			m_NumGlobalReservedObjects;
	u8			m_NumCreatedPeds;
	u8			m_NumCreatedVehicles;
	u8			m_NumCreatedObjects;

	bool		m_AcceptingPlayers : 1;		// if set players are permitted to join the script session when a network game is running
	bool        m_ActiveInSinglePlayer : 1; // TRUE if the network script must persist on mp to sp transitions
	bool		m_InMPCutscene : 1;			// if set, the script is running a cutscene in multiplayer

#if __DEV
	 // used to ensure the script is managing population counts correctly
	u8			m_canRegisterMissionObjectCalled;
	u8			m_canRegisterMissionPedCalled;
	u8			m_canRegisterMissionVehicleCalled;
#endif
};

// ================================================================================================================
// CGameScriptHandlerNetComponent
// ================================================================================================================
class CGameScriptHandlerNetComponent: public scriptHandlerNetComponent
{
public:

	CGameScriptHandlerNetComponent(CGameScriptHandler& parentHandler, u32 maxNumParticipants);
	~CGameScriptHandlerNetComponent() {}

	FW_REGISTER_CLASS_POOL(CGameScriptHandlerNetComponent);

	virtual bool Update();
	virtual void PlayerHasJoined(const netPlayer& player);
	virtual void HandleJoinAckMsg(const msgScriptJoinAck& msg, const ReceivedMessageData &messageData);
	virtual void HandleJoinHostAckMsg(const msgScriptJoinHostAck& msg, const ReceivedMessageData &messageData);
	virtual void MigrateScriptHost(const netPlayer& player);

	CNetGamePlayer* GetClosestParticipant(const Vector3 &pos, bool bIncludeMyPlayer) const;

	PlayerFlags			GetParticipantFlags() const; // returns a bitfield of the players running the script

	// returns true if the participant with the given id is active, in our roaming bubble, has a player ped and has had a script join event triggered for him
	bool IsParticipantPhysical(int participantSlot, const CNetGamePlayer** ppParticipant = NULL);

	// called when the ready for the handler to start the joining process, after doing some setup
	void SetScriptReady() { m_bScriptReady = true; }

	// triggers any network game events
	void TriggerJoinEvents();

#if __DEV
	// displays debug info on this handler
	virtual void DisplayScriptHandlerInfo() const;
#endif

	// broadcasts the script info. If pPlayer is set then the script info is only broadcast to that player. 
	bool BroadcastRemoteScriptInfo(const netPlayer* pPlayer = NULL);

protected:

	virtual bool AddPlayerAsParticipant(const netPlayer& player, ScriptParticipantId participantId, ScriptSlotNumber slotNumber);
	virtual void RemovePlayerAsParticipant(const netPlayer& player);
	virtual void HandleLeavingPlayer(const netPlayer& player, bool playerLeavingSession);

	virtual void SetNewScriptHost(const netPlayer* pPlayer, HostToken hostToken, bool bBroadcast = true);

	virtual bool CanStartJoining();
	virtual void RestartJoiningProcess();
	virtual void StartPlaying();

	virtual bool DoLeaveCleanup();
	virtual bool DoTerminationCleanup();

	PlayerFlags	m_JoinEventFlags;	// flags indicating whether we need to trigger a script join event for a new participant

	unsigned m_PendingTimestamp;	// the timestamp of the host who has accepted us into the script session

	bool m_bScriptReady : 1;
	bool m_bStartedPlaying: 1;
	bool m_bDoneLeaveCleanup : 1;
	bool m_bDoneTerminationCleanup : 1;
	bool m_bBroadcastScriptInfo : 1;	// if set, we need to broadcast the script info

#if __BANK
	int	m_joiningTimer;
	int m_noHostTimer;
#endif

};

#endif  // GAME_SCRIPT_HANDLER_NETWORK_H
