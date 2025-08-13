//
// name:        NetworkObjectMgr.h
// description: Project specific object management code
// written by:  Daniel Yelland
//

#ifndef NETWORK_OBJECT_MGR_H
#define NETWORK_OBJECT_MGR_H

#include "fwnet/netobjectmgrbase.h"
#include "entity/archetypemanager.h"

namespace rage
{
    class scriptHandler;
    class scriptIdBase;
}

class CEntity;
class CObject;
class CPed;
class CProjectSyncTree;
class CNetGamePlayer;
class CNetObjGame;
class CVehicle;
class CGameScriptHandler;
class CGameScriptObjInfo;
class CGameScriptId;

//PURPOSE
//  Predicate function for processing network objects that are vehicles
bool IsVehiclePredicate(const netObject *networkObject);
bool IsParkedVehiclePredicate(const netObject *networkObject);

class CNetworkObjectMgr : public netObjectMgrBase
{
public:

    //PURPOSE
    // Enumeration of reasons network objects have been unregistered, used
    // for logging purposes
    enum UnregistrationReason
    {
        CLONE_CREATE_EXISTING_OBJECT = netObjectMgrBase::USER_REASON,
        CLONE_REMOVAL,
        CLONE_REMOVAL_WRONG_OWNER,
        CLEANUP_SCRIPT_OBJECTS,
		DUPLICATE_SCRIPT_OBJECT,
        OWNERSHIP_CONFLICT,
		EXPIRED_RESPAWN_PED,
		SCRIPT_CLONE_ONLY,
		SCRIPT_UNREGISTRATION,
		AMBIENT_OBJECT_OWNERSHIP_CONFLICT
   };

    CNetworkObjectMgr(netBandwidthMgr& bandwidthMgr);
    ~CNetworkObjectMgr();

    virtual bool Init();
    virtual void Shutdown();

    virtual void Update(bool bUpdateNetworkObjects);

    // Finds and returns the network object with the given object ID if they are owned by the specified network player.
    CNetObjGame* GetNetworkObjectFromPlayer(const ObjectId objectID, const netPlayer &player, bool includeAll = false);

    // Registers a game object with the manager to be cloned on remote machines.
    // pGameObject - the game object
    // objectType - the type of object
    // flags - any flags that need set for the object
    bool RegisterGameObject(netGameObjectBase *pGameObject,
                            const NetObjFlags localFlags = 0,
                            const NetObjFlags globalFlags = 0,
                            const bool        makeSpaceForObject = false);

	// Registers a new network object with the manager and initialises this object. Then clones this object on
	// remote machines if necessary
	virtual void RegisterNetworkObject(netObject* pNetObj);

    // Unregisters a network object with the manager and removes clones on remote machines if necessary
    virtual void UnregisterNetworkObject(netObject* pNetObj, unsigned reason, bool bForce = false, bool bDestroyObject = true);

    // tells the manager about players joining/leaving the game
    virtual void PlayerHasJoined(const netPlayer& player);
    virtual void PlayerHasLeft(const netPlayer& player);

    // returns the static sync tree for the given object type
    CProjectSyncTree* GetSyncTree(NetworkObjectType objectType);

    // conversion functions between ambient and script objects
    void ConvertRandomObjectToScriptObject(CNetObjGame* pObject);

    // returns whether the object manager is controlling more objects that it can handle
    bool IsOwningTooManyObjects();

    // returns the number of objects that are being starved of updates controlled by this player
    u32 GetNumStarvingObjects();

    // function to indicate a give control event has been received with a TOO_MANY_OBJECTS ack code
    void TooManyObjectsOwnedACKReceived(const netPlayer& player);

    // function to indicate a clone create message has been received with a TOO_MANY_OBJECTS ack code
    void TooManyObjectsCreatedACKReceived(const netPlayer& player, NetworkObjectType objectType);

    // returns whether a player can accept more objects
    bool RemotePlayerCanAcceptObjects(const netPlayer& player, bool proximity);

    // returns whether a player can accept new objects from our players
    bool RemotePlayerCanAcceptNewObjectsFromOurPlayer(const netPlayer& player, NetworkObjectType objectType);

    // returns the number of locally owned peds currently in vehicles
    u32 GetNumLocalPedsInVehicles();

    // returns the number of locally owned vehicles currently using pretend occupants
    u32 GetNumLocalVehiclesUsingPretendOccupants();

    // returns the number of players in vehicles not cloned on our machine
    u32 GetNumClosePlayerVehiclesNotOnOurMachine();

	// returns true if any of the re-assignable local objects are still in the local objects list 
	bool AnyLocalObjectPendingTutorialSessionReassign();

    // returns true if the given object id is for a vehicle another player is in
    bool IsPlayerVehicle(const ObjectId objectID, const netPlayer** ppPlayer = 0);

    static bool IsLocalPlayerPhysicalOnRemoteMachine(unsigned peerPlayerIndex, netPlayer &remotePlayer);

    // updates all objects after all code that can affect an entities position has been processed
    void UpdateAfterAllMovement();

	// updates all objects after the script update
	void UpdateAfterScripts();

    // display object info above all network objects
    void DisplayObjectInfo();

	// hides all objects for a cutscene immediately
	void HideAllObjectsForCutscene();

	// exposes all hidden objects after a cutscene
	void ExposeAllObjectsAfterCutscene();

	// possibly detects duplicate script objects and returns false if this one cannot be accepted
	bool CanAcceptScriptObject(CNetObjGame& object, const CGameScriptObjInfo& objectScriptInfo);

	// returns the players nearby within the various entity scope ranges, relative to the local player's position. If pScopePosition is set, this is used instead of the player's position
	void GetPlayersNearbyInScope(PlayerFlags& playersNearbyInPedScope, PlayerFlags& playersNearbyInVehScope, PlayerFlags& playersNearbyInObjScope, Vector3* pScopePosition = nullptr);

	unsigned GetTooManyObjectsCountdown() const { return m_TooManyObjectsCountdown; } // countdown timer until we consider ourselves owning too many objects - to prevent temporary blips kicking in bandwidth throttling

#if __BANK

    static void AddDebugWidgets();

    // display debug object text
    void DisplayDebugInfo();

    // display the names of the objects that have failed to sync
    void DisplayObjectSyncFailureInfo();

    // display physics information about the network objects
    void DisplayPhysicsData();

    // returns whether we are using batched updates this frame
    bool IsUsingBatchedUpdates() const { return m_UsingBatchedUpdates; }
#endif // __BANK

	bool AddInvalidObjectModel(int modelHashKey);				// set invalid object map by background script
	bool RemoveInvalidObjectModel(int modelHashKey);			
	void ClearInvalidObjectModels();
    bool AddModelToInstancedContentAllowList(int modelHashKey);
    void RemoveAllModelsFromInstancedContentAllowList();

#if __BANK
    netBandwidthStatistics& GetBandwidthStatistics() { return GetMessageHandler().GetBandwidthStatistics(); }
#endif // __DEV

    // NETWORK BOTS FUNCTIONALITY
    bool RegisterNetworkBotGameObject(PhysicalPlayerIndex   botPhysicalPlayerIndex,
                                      netGameObjectBase		*pGameObject,
                                      const NetObjFlags		localFlags = 0,
                                      const NetObjFlags		globalFlags = 0);
    // END NETWORK BOTS FUNCTIONALITY

    // cleans up the mission state of any objects belonging to the given script, which has terminated
    void CleanupScriptObjects(const class CGameScriptId& scrId, bool bRemove = false);

    // tries to make space for an object of the specified type
    void TryToMakeSpaceForObject(NetworkObjectType objectType);

    static u32  m_syncFailureThreshold;              // maximum number of average number of sync failures that is considered too high to accept
    static u32  m_maxStarvingObjectThreshold;        // maximum number of concurrently starving objects considered too high to accept
    static u32  m_maxObjectsToDumpPerUpdate;         // maximum number of objects to dump per update if the we control too many objects
    static u32  m_dumpExcessObjectInterval;          // the time interval between dumping objects based on the bandwidth being too high
    static u32  m_tooManyObjectsACKTimeoutBalancing; // time after receiving a TOO_MANY_OBJECTS ACK code from a player that we will attempt to pass control of our excess objects to them
    static u32  m_tooManyObjectsACKTimeoutProximity; // time after receiving a TOO_MANY_OBJECTS ACK code from a player that we will attempt to pass control of our excess objects to them
    static u32  m_DelayBeforeDeletingExcessObjects;  // delay before deleting excess objects (they will be passed on to other players if possible first)

    u32  m_lastTimeExcessObjectCalled;          // the time TryToDumpExcessObjects was last called
    u32  m_lastTimeObjectDumped;                // the last time a local object was dumped due to the local player owning too many
    u32  m_TimeAllowedToDeleteExcessObjects;    // time when we are next allowed to delete excess objects (only when failed to pass on to other players)
    u32  m_TimeToDeletePedsForPretendOccupants; // time when we are allowed to delete peds to make space for pretend occupant conversions (we can pass control of peds anytime for this purpose)

    bool m_isOwningTooManyObjects;            // cached flag indicating we are owning too many objects

private:

    // This is an enumeration of the different types of objects that can
    // fail to be created. This is required as there are multiple vehicle
    // types and we want to treat all vehicles as the same
    enum CreationFailureTypes
    {
        CREATION_FAILURE_PED,
        CREATION_FAILURE_OBJECT,
        CREATION_FAILURE_VEHICLE,
        MAX_CREATION_FAILURE_TYPES
    };

    void CreateSyncTrees();
    void DestroySyncTrees();
    void UpdateSyncTreeBatches();

    // initialise the network object pools
    void InitNetworkObjectPools();

    // shut down the network object pools
    void ShutdownNetworkObjectPools();

    // initialise the delta buffer pools
    void InitSyncDataPools();

    // shutdown the delta buffer pools
    void ShutdownSyncDataPools();

    // clones an object on the given player
    virtual void CloneObject(netObject* pObject, const netPlayer& player, datBitBuffer &createDataBuffer);

	// Removes an object's clone on the given player (sends a removal message)
	virtual bool RemoveClone(netObject *object, const netPlayer &player, bool forceSend = false);

	// syncs an object with its remote clone on the given player
    virtual bool SyncClone(netObject* pObject, const netPlayer& player);

    virtual void OnSendCloneSync(const netObject *object, unsigned timestamp);

    virtual void OnSendConnectionSync(unsigned timestamp);

    virtual bool ShouldSendCloneSyncImmediately(const netPlayer &player, const netObject &networkObject);

#if ENABLE_NETWORK_LOGGING
    void CheckNetworkObjectSpawnDistance(netObject* pNetObj);
#endif 

    void ProcessCloneCreateData(const netPlayer  &fromPlayer,
                                const netPlayer  &toPlayer,
                                NetworkObjectType objectType,
                                const ObjectId    objectId,
                                NetObjFlags       flags,
                                datBitBuffer     &createDataMessageBuffer,
                                unsigned          timestamp);

    void ProcessCloneCreateAckData(const netPlayer &fromPlayer,
                                   const netPlayer &toPlayer,
                                   ObjectId         objectID,
                                   NetObjectAckCode ackCode);

    // processes a clone sync that has arrived in a clone sync message
    virtual NetObjectAckCode ProcessCloneSync(const netPlayer        &fromPlayer,
                                              const netPlayer        &toPlayer,
                                              const NetworkObjectType objectType,
                                              const ObjectId          objectId,
                                              datBitBuffer           &msgBuffer,
                                              const netSequence       seqNum,
                                              u32                     timeStamp);

    void ProcessCloneSyncAckData  (const netPlayer &fromPlayer,
                                   const netPlayer &toPlayer,
                                   netSequence      syncSequence,
                                   ObjectId         objectID,
                                   NetObjectAckCode ackCode);
    void ProcessCloneRemoveData   (const netPlayer &fromPlayer, const netPlayer &toPlayer, ObjectId objectID, u32 ownershipToken);
    void ProcessCloneRemoveAckData(const netPlayer &fromPlayer, const netPlayer &toPlayer, ObjectId objectID, NetObjectAckCode ackCode);

    // message processing caps (used to prevent large spikes in frame rate)
    bool UseCloneCreateCapping(NetworkObjectType objectType);
    bool UseCloneRemoveCapping(NetworkObjectType objectType);

    // returns whether this network object is important and must be cloned even if the machine is owning too many objects
    bool IsImportantObject(netObject *pObject);

    // updates all network objects on all remote players, subject to available bandwidth
    void UpdateAllObjectsForRemotePlayers(const netPlayer &sourcePlayer);

    // build a list of objects suitable for removal ordered by the impact their deletion will have on the game
    void BuildObjectDeletionLists(CObject   **objectRemovalList,   u32 objectListCapacity,   u32 &numObjectsReturned,
                                  CPed      **pedRemovalList,      u32 pedListCapacity,      u32 &numPedsReturned,
                                  CVehicle  **vehicleRemovalList,  u32 vehicleListCapacity,  u32 &numVehiclesReturned);

    // build a list of objects suitable for passing to the specified remote player when we are owning too many objects locally
    void BuildObjectControlPassingLists(CEntity             **entityRemovalList,
                                        u32                   entityListCapacity,
                                        u32                  &numEntitiesReturned,
                                        bool                  includePeds,
                                        bool                  includeVehicles,
                                        bool                  includeObjects,
                                        const CNetGamePlayer &player);

    // try to pass control or remove any ambient objects if we are struggling to support our current number of locally owned objects
    void TryToDumpExcessObjects();

    // update the is owning too many object flag
    void UpdateIsOwningTooManyObjects();

    // returns the number of locally owned objects that are pending owner change to another machine
    u32 GetNumLocalObjectsPendingOwnerChange();

    // returns the creation failure type for the specified network object type
    CreationFailureTypes GetCreationFailureTypeFromObjectType(NetworkObjectType objectType);

    // returns a string describing the reason a network object has been unregistered
    const char *GetUnregistrationReason(unsigned reason) const;

	// checks if reassignment related metrics should be fired, and sends them
	void HandleTelemetryMetrics() const;

	static const unsigned DEFAULT_STARVATION_THRESHOLD                = 10;
    static const unsigned DEFAULT_SYNC_FAILURE_THRESHOLD              = 10;
    static const unsigned DEFAULT_MAX_STARVING_OBJECTS                = 5;
    static const unsigned DEFAULT_MAX_OBJECTS_DUMPED_PER_UPDATE       = 5;
    static const unsigned DEFAULT_DUMP_EXCESS_OBJECT_INTERVAL         = 250;
    static const unsigned DEFAULT_TOO_MANY_OBJECTS_TIMEOUT_BALANCING  = 30000;
    static const unsigned DEFAULT_TOO_MANY_OBJECTS_TIMEOUT_PROXIMITY  = 5000;
    static const unsigned DEFAULT_TIME_BEFORE_DELETING_EXCESS_OBJECTS = 5000;
    static const unsigned DEFAULT_CAPPED_CLONE_CREATE_LIMIT           = 4;
    static const unsigned DEFAULT_CAPPED_CLONE_REMOVE_LIMIT           = 4;

    // this stores the system time in milliseconds when the last "too many objects" ack code was returned by a give control event from a player
    u32 m_timeSinceLastTooManyObjectsACK[MAX_NUM_PHYSICAL_PLAYERS];

    // this stores the system time in milliseconds when the last "too many objects" ack code was returned by a clone create ack from a player
    u32 m_timeSinceLastNewObjectCreationFailure[MAX_NUM_PHYSICAL_PLAYERS][MAX_CREATION_FAILURE_TYPES];

    u8       m_NumCappedCloneCreates; // the number of clone create messages for capped object types already processed this frame
    u8       m_NumCappedCloneRemoves; // the number of clone create messages for capped object types already processed this frame
	u32      m_FrameCloneCreateCapReference;

    unsigned m_PlayerBatchStart;      // the player to start the next batch from

    unsigned m_TooManyObjectsCountdown; // countdown timer until we consider ourselves owning too many objects - to prevent temporary blips kicking in bandwidth throttling

	atBinaryMap<int, int> m_invalidObjectModelMap; // contains model hashes for cheat detection set via background script
    atBinaryMap<int, int> m_InstancedContentModelAllowList; // contains model hashes that are expected to be registered in current instanced content

#if __BANK || __DEV
	// validates the sync data sizes of the different network objects
	void ValidateSyncDatas();
#endif

#if __DEV
	// validates that objects are not pending cloning, cloned or pending removal on target player
	void ValidateObjectsClonedState(const netPlayer& player);

    // sanity checks the network object usage
    void SanityCheckObjectUsage();

    // searches the network object pools to display leaked objects
    void DisplayLeakedCObjects();
    void DisplayLeakedPeds();
    void DisplayLeakedVehicles();
#endif // __DEV

#if ENABLE_NETWORK_LOGGING
	bool IsObjectTooCloseForCreationOrRemoval(netObject *pObject, float &distance);
	bool IsObjectTooFarForCreation(netObject *pObject, float &distance);

    bool m_enableLoggingToCatchCloseEntities;
#endif // ENABLE_NETWORK_LOGGING

	void CheckForInvalidObject(const netPlayer& fromPlayer, netObject* networkObject);	// check if the object being created has an invalid model (possible cheat detection)

#if __BANK
    // registration failure tracking
    enum REGISTRATION_FAILURE_REASON
    {
        REGISTRATION_SUCCESS,
        REGISTRATION_FAIL_POPULATION,
        REGISTRATION_FAIL_OBJECTIDS,
        REGISTRATION_FAIL_MISSION_OBJECT_PENDING,
        REGISTRATION_FAIL_TOO_MANY_OBJECTS
    };

public:
    const char *GetLastRegistrationFailureReason();

private:
    REGISTRATION_FAILURE_REASON m_eLastRegistrationFailureReason;
    bool                        m_UsingBatchedUpdates;

#endif // __BANK
};

#endif  // NETWORK_OBJECT_MGR_H
