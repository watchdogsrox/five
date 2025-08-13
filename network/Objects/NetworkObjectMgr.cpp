//
// name:        NetworkObjectMgr.cpp
// description: Project specific object management code
// written by:  Daniel Yelland
//

#include "network/objects/NetworkObjectMgr.h"

// framework includes
#include "fwnet/netblender.h"
#include "fwnet/netlogutils.h"
#include "fwnet/netutils.h"
#include "fwscript/scriptInterface.h"

// game includes
#include "camera/CamInterface.h"
#include "frontend/PauseMenu.h"
#include "glassPaneSyncing/GlassPaneInfo.h"
#include "network/Network.h"
#include "network/Debug/NetworkDebug.h"
#include "network/NetworkInterface.h"
#include "network/Bandwidth/NetworkBandwidthManager.h"
#include "network/Cloud/Tunables.h"
#include "network/Debug/NetworkDebug.h"
#include "network/events/NetworkEventTypes.h"
#include "network/general/NetworkDamageTracker.h"
#include "network/general/NetworkPendingProjectiles.h"
#include "network/general/NetworkSynchronisedScene.h"
#include "network/general/NetworkVehicleModelDoorLockTable.h"
#include "network/general/scriptworldstate/NetworkScriptWorldStateManager.h"
#include "network/general/scriptworldstate/NetworkScriptWorldStateTypes.h"
#include "network/objects/entities/NetObjAutomobile.h"
#include "network/objects/entities/NetObjBike.h"
#include "network/objects/entities/NetObjBoat.h"
#include "network/objects/entities/NetObjDoor.h"
#include "network/objects/entities/NetObjHeli.h"
#include "network/objects/entities/NetObjObject.h"
#include "network/objects/entities/NetObjPed.h"
#include "network/objects/entities/NetObjPickup.h"
#include "network/objects/entities/NetObjPlane.h"
#include "network/objects/entities/NetObjPlayer.h"
#include "network/objects/entities/NetObjTrailer.h"
#include "network/objects/entities/NetObjTrain.h"
#include "network/objects/entities/netObjSubmarine.h"
#include "network/Objects/entities/NetObjGlassPane.h"
#include "network/objects/NetworkObjectPopulationMgr.h"
#include "network/objects/prediction/NetBlenderPed.h"
#include "network/objects/prediction/NetBlenderPhysical.h"
#include "network/players/NetworkPlayerMgr.h"
#include "Network/Live/NetworkTelemetry.h"
#include "objects/Door.h"
#include "peds/Ped.h"
#include "peds/PedFactory.h"
#include "peds/PedIntelligence.h"
#include "peds/PedPopulation.h"
#include "peds/Ped.h"
#include "pickups/Pickup.h"
#include "pickups/PickupManager.h"
#include "physics/Physics.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "scene/world/GameWorld.h"
#include "script/handlers/GameScriptEntity.h"
#include "script/handlers/GameScriptHandler.h"
#include "script/script.h"
#include "task/general/taskbasic.h"
#include "task/Vehicle/TaskCar.h"
#include "vehicles/automobile.h"
#include "vehicles/boat.h"
#include "vehicles/submarine.h"
#include "vehicles/bike.h"
#include "vehicles/heli.h"
#include "vehicles/trailer.h"
#include "vehicles/train.h"
#include "vehicles/planes.h"
#include "vehicles/vehiclefactory.h"
#include "vehicles/vehiclepopulation.h"
#include "fwnet/netchannel.h"
#include "Peds/PopCycle.h"
#include "glassPaneSyncing/GlassPane.h"

NETWORK_OPTIMISATIONS()

RAGE_DEFINE_SUBCHANNEL(net, objectmgr, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_objectmgr

PARAM(logstarvationdata, "[network] logs extended data about starving network objects");
PARAM(logonlyscriptnetregfailures, "[network] will log only script network registration failures");

#if __DEV || __BANK

// compile time assertions
CompileTimeAssert(sizeof(CNetObjAutomobile) <= LARGEST_NETOBJVEHICLE_CLASS);
CompileTimeAssert(sizeof(CNetObjBike)       <= LARGEST_NETOBJVEHICLE_CLASS);
CompileTimeAssert(sizeof(CNetObjBoat)       <= LARGEST_NETOBJVEHICLE_CLASS);
CompileTimeAssert(sizeof(CNetObjHeli)       <= LARGEST_NETOBJVEHICLE_CLASS);
CompileTimeAssert(sizeof(CNetObjPlane)      <= LARGEST_NETOBJVEHICLE_CLASS);
CompileTimeAssert(sizeof(CNetObjTrailer)    <= LARGEST_NETOBJVEHICLE_CLASS);
CompileTimeAssert(sizeof(CNetObjTrain)      <= LARGEST_NETOBJVEHICLE_CLASS);
CompileTimeAssert(sizeof(CNetObjSubmarine)  <= LARGEST_NETOBJVEHICLE_CLASS);
// ensure at least on of the network object vehicle classes is using the specified largest class size (allowing for alignment)
CompileTimeAssert((sizeof(CNetObjAutomobile) == LARGEST_NETOBJVEHICLE_CLASS || (LARGEST_NETOBJVEHICLE_CLASS - sizeof(CNetObjAutomobile)) < 16) ||
                  (sizeof(CNetObjBike)       == LARGEST_NETOBJVEHICLE_CLASS || (LARGEST_NETOBJVEHICLE_CLASS - sizeof(CNetObjBike))       < 16) ||
                  (sizeof(CNetObjBoat)       == LARGEST_NETOBJVEHICLE_CLASS || (LARGEST_NETOBJVEHICLE_CLASS - sizeof(CNetObjBoat))       < 16) ||
                  (sizeof(CNetObjHeli)       == LARGEST_NETOBJVEHICLE_CLASS || (LARGEST_NETOBJVEHICLE_CLASS - sizeof(CNetObjHeli))       < 16) ||
                  (sizeof(CNetObjPlane)      == LARGEST_NETOBJVEHICLE_CLASS || (LARGEST_NETOBJVEHICLE_CLASS - sizeof(CNetObjPlane))      < 16) ||
				  (sizeof(CNetObjTrailer)    == LARGEST_NETOBJVEHICLE_CLASS || (LARGEST_NETOBJVEHICLE_CLASS - sizeof(CNetObjTrailer))    < 16) ||
				  (sizeof(CNetObjSubmarine)  == LARGEST_NETOBJVEHICLE_CLASS || (LARGEST_NETOBJVEHICLE_CLASS - sizeof(CNetObjSubmarine))  < 16) ||
                  (sizeof(CNetObjTrain)      == LARGEST_NETOBJVEHICLE_CLASS || (LARGEST_NETOBJVEHICLE_CLASS - sizeof(CNetObjTrain))      < 16));

CompileTimeAssert(sizeof(CNetObjPlayer) <= LARGEST_NETOBJPED_CLASS);
CompileTimeAssert(sizeof(CNetObjPlayer) == LARGEST_NETOBJPED_CLASS || ((LARGEST_NETOBJPED_CLASS - sizeof(CNetObjPlayer)) < 16));

static const unsigned int SIZEOF_MAX_OBJECTS_PER_PLAYER = 8;
//CompileTimeAssert(CNetworkObjectIDMgr::MAX_NUM_OBJECT_IDS_PER_PLAYER <= 1<<SIZEOF_MAX_OBJECTS_PER_PLAYER);

// this must be a multiple of 16 otherwise we will get alignment problems on Xbox 360
CompileTimeAssert((LARGEST_NETOBJPED_CLASS%16) == 0);
CompileTimeAssert((LARGEST_NETOBJVEHICLE_CLASS%16) == 0);
#endif

#if __BANK

static bool s_displayObjectIdDebug           = false;
static bool s_displayObjectTypeCounts        = false;
static bool s_displayObjectTargetLevels      = false;
static bool s_displayObjectSyncFailureInfo   = false;
static bool s_displayObjectPhysicsDebug      = false;
static bool s_displayObjectReassignmentDebug = false;
static bool s_displayObjectTypeReserveCounts = false;
static bool s_displayObjectCarGen            = false;

#endif // __BANK

bool IsVehiclePredicate(const netObject *networkObject)
{
    if(networkObject && IsVehicleObjectType(networkObject->GetObjectType()))
    {
        return true;
    }

    return false;
}

bool IsParkedVehiclePredicate(const netObject *networkObject)
{
    const CVehicle *vehicle = NetworkUtils::GetVehicleFromNetworkObject(networkObject);

    if(vehicle && vehicle->PopTypeGet() == POPTYPE_RANDOM_PARKED)
    {
        return true;
    }

    return false;
}

u32 CNetworkObjectMgr::m_syncFailureThreshold              = CNetworkObjectMgr::DEFAULT_SYNC_FAILURE_THRESHOLD;
u32 CNetworkObjectMgr::m_maxStarvingObjectThreshold        = CNetworkObjectMgr::DEFAULT_MAX_STARVING_OBJECTS;
u32 CNetworkObjectMgr::m_maxObjectsToDumpPerUpdate         = CNetworkObjectMgr::DEFAULT_MAX_OBJECTS_DUMPED_PER_UPDATE;
u32 CNetworkObjectMgr::m_dumpExcessObjectInterval          = CNetworkObjectMgr::DEFAULT_DUMP_EXCESS_OBJECT_INTERVAL;
u32 CNetworkObjectMgr::m_tooManyObjectsACKTimeoutBalancing = CNetworkObjectMgr::DEFAULT_TOO_MANY_OBJECTS_TIMEOUT_BALANCING;
u32 CNetworkObjectMgr::m_tooManyObjectsACKTimeoutProximity = CNetworkObjectMgr::DEFAULT_TOO_MANY_OBJECTS_TIMEOUT_PROXIMITY;
u32 CNetworkObjectMgr::m_DelayBeforeDeletingExcessObjects  = CNetworkObjectMgr::DEFAULT_TIME_BEFORE_DELETING_EXCESS_OBJECTS;

static const unsigned DEFAULT_TOO_MANY_OBJECTS_WAIT_TIME = 30000;

// Whether or not we'll be collecting samples and sending the metrics, based on the tunables METRIC_IS_CHECK_INVALID_OBJECT_MODEL_ENABLED and METRIC_IS_CHECK_INVALID_OBJECT_MODEL_SAMPLE
static bool s_sendInvalidObjectModelMetrics = false;

struct InvalidObjectReportReason {
	static const u32 INVALID_OBJECT_MODEL = 0x17eacaff;
	static const u32 NOT_ALLOWED_IN_INSTANCED_CONTENT = 0xbce7b5c7;
};

CNetworkObjectMgr::CNetworkObjectMgr(netBandwidthMgr& bandwidthMgr) :
netObjectMgrBase(bandwidthMgr)
#if __BANK
, m_eLastRegistrationFailureReason(REGISTRATION_SUCCESS)
, m_UsingBatchedUpdates(false)
#endif
, m_lastTimeExcessObjectCalled(0)
, m_lastTimeObjectDumped(0)
, m_TimeAllowedToDeleteExcessObjects(0)
, m_TimeToDeletePedsForPretendOccupants(0)
, m_isOwningTooManyObjects(false)
, m_NumCappedCloneCreates(0)
, m_FrameCloneCreateCapReference(0)
, m_NumCappedCloneRemoves(0)
, m_PlayerBatchStart(0)
, m_TooManyObjectsCountdown(DEFAULT_TOO_MANY_OBJECTS_WAIT_TIME)
{
    LOGGING_ONLY(m_enableLoggingToCatchCloseEntities = true);

    netObject::SetStarvationThreshold(DEFAULT_STARVATION_THRESHOLD);
}

CNetworkObjectMgr::~CNetworkObjectMgr()
{
}

bool CNetworkObjectMgr::Init()
{
    bool success = netObjectMgrBase::Init();

    CreateSyncTrees();

    CNetwork::UseNetworkHeap(NET_MEM_NETWORK_OBJECTS);
    InitNetworkObjectPools();
    CNetwork::StopUsingNetworkHeap();

    CNetwork::UseNetworkHeap(NET_MEM_NETWORK_BLENDERS);
    CNetBlenderPed::InitPool( MEMBUCKET_NETWORK );
    CNetBlenderPhysical::InitPool( MEMBUCKET_NETWORK );
    CNetwork::StopUsingNetworkHeap();

    InitSyncDataPools();

    CNetwork::UseNetworkHeap(NET_MEM_OBJECT_MANAGER);
	CNetworkDamageTracker::InitPool( MEMBUCKET_NETWORK );
	CVehicleAppearanceDataNode::CVehicleBadgeData::InitPool( MEMBUCKET_NETWORK );
    CNetworkPendingProjectiles::RemoveAllProjectileData();

    CNetworkKillTracker::Init();
	CNetworkHeadShotTracker::Init();
    CNetworkScriptWorldStateMgr::Init();
    CNetworkSynchronisedScenes::Init();
    CNetworkWorldGridManager::Init();
    NetworkScriptWorldStateTypes::Init();
	CNetworkVehicleModelDoorLockedTableManager::Init();

    netObject::SetLog(&GetLog());

    CNetworkObjectPopulationMgr::Init();

    SetMessageHandlerLog(&CNetwork::GetMessageLog());

#if __BANK || __DEV
    ValidateSyncDatas();
#endif

#if ENABLE_NETWORK_LOGGING
    CVehicleTaskDataNode::SetTaskDataLogFunction(CNetObjVehicle::LogTaskData);
	CVehicleProximityMigrationDataNode::SetTaskDataLogFunction(CNetObjVehicle::LogTaskMigrationData);
	CClonedControlVehicleInfo::SetTaskDataLogFunction(CNetObjVehicle::LogTaskData);
	CPedTaskSpecificDataNode::SetTaskDataLogFunction(CQueriableInterface::LogTaskInfo);
	CPedTaskSequenceDataNode::SetTaskDataLogFunction(CQueriableInterface::LogTaskInfo);
	CObjectGameStateDataNode::SetTaskDataLogFunction(CNetObjObject::LogTaskData);
	CVehicleGadgetDataNode::SetGadgetLogFunction(CNetObjVehicle::LogGadgetData);
#endif // ENABLE_NETWORK_LOGGING

    netSerialiserUtils::SetWorldExtentsCallbacks(CNetObjEntity::GetAdjustedWorldExtents,
                                                 CNetObjEntity::GetRealMapCoords,
                                                 CNetObjEntity::GetAdjustedMapCoords);

    for (u32 i=0; i<MAX_NUM_PHYSICAL_PLAYERS; i++)
    {
        m_timeSinceLastTooManyObjectsACK[i]                                   = 0;
        m_timeSinceLastNewObjectCreationFailure[i][CREATION_FAILURE_PED]      = 0;
        m_timeSinceLastNewObjectCreationFailure[i][CREATION_FAILURE_OBJECT]   = 0;
        m_timeSinceLastNewObjectCreationFailure[i][CREATION_FAILURE_VEHICLE]  = 0;
    }

    m_lastTimeExcessObjectCalled          = 0;
    m_lastTimeObjectDumped                = 0;
    m_TimeAllowedToDeleteExcessObjects    = 0;
    m_TimeToDeletePedsForPretendOccupants = 0;

    m_NumCappedCloneCreates  = 0;
    m_NumCappedCloneRemoves  = 0;
	m_FrameCloneCreateCapReference = 0;

    m_PlayerBatchStart = 0;

    m_TooManyObjectsCountdown = DEFAULT_TOO_MANY_OBJECTS_WAIT_TIME;

    CNetwork::StopUsingNetworkHeap();

	CNetObjPlayer::SetIsCorrupt(false);

	GetReassignMgr().SetTelemetryCallback(CNetworkTelemetry::AppendObjectReassignInfo);

	if(Tunables::GetInstance().TryAccess(MP_GLOBAL_HASH, ATSTRINGHASH("METRIC_IS_CHECK_INVALID_OBJECT_MODEL_ENABLED", 0x95ca0539), false))
	{
		// The tunable is a percentage. we roll a dice and see if we're part of the percentage of players who are going to collect samples
		float percentageOfPlayersCollectingSamples = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("METRIC_IS_CHECK_INVALID_OBJECT_MODEL_SAMPLE", 0xd2ee26a7), 100.0f);
		mthRandom rand( (int) fwTimer::GetSystemTimeInMilliseconds() );
		float diceRoll = rand.GetRanged(0.0f, 100.0f);
		gnetDebug1("METRIC_IS_CHECK_INVALID_OBJECT_MODEL_SAMPLE (%f %%) dice roll (0, 100) = %f   -> %s", percentageOfPlayersCollectingSamples, diceRoll, (diceRoll <= percentageOfPlayersCollectingSamples) ? "succeed" : "failed");
		if(diceRoll <= percentageOfPlayersCollectingSamples)
		{
			s_sendInvalidObjectModelMetrics = true;
		}
	}

    return success;
}

void CNetworkObjectMgr::Shutdown()
{
	gnetDebug1("Shutdown");

    CNetworkObjectPopulationMgr::Shutdown();

    CNetworkScriptWorldStateMgr::Shutdown();
    CNetworkSynchronisedScenes::Shutdown();
    CNetworkWorldGridManager::Shutdown();
    NetworkScriptWorldStateTypes::Shutdown();
	CNetworkVehicleModelDoorLockedTableManager::Shutdown();

	netObjectMgrBase::Shutdown();

    DestroySyncTrees();

    CNetwork::UseNetworkHeap(NET_MEM_NETWORK_OBJECTS);
    ShutdownNetworkObjectPools();
    CNetwork::StopUsingNetworkHeap();

    CNetwork::UseNetworkHeap(NET_MEM_NETWORK_BLENDERS);
    CNetBlenderPhysical::ShutdownPool();
    CNetBlenderPed::ShutdownPool();
    CNetwork::StopUsingNetworkHeap();

    CNetworkDamageTracker::ShutdownPool();
	CVehicleAppearanceDataNode::CVehicleBadgeData::ShutdownPool();
    ShutdownSyncDataPools();
}


bool PredicateIsScript(const netObject *networkObject)
{
	bool isScriptObject = (networkObject->GetGlobalFlags() & CNetObjGame::GLOBALFLAG_SCRIPTOBJECT) ? true : false;
	return isScriptObject;
}
bool PredicateIsAmbient(const netObject *networkObject)
{
	return !PredicateIsScript(networkObject);
}

void CNetworkObjectMgr::Update(bool bUpdateNetworkObjects)
{
    if(bUpdateNetworkObjects)
    {
        UpdateSyncTreeBatches();
    }

    // update the network synchronised scenes - this needs updating before the
    // object manager base update as it can request state to not be sent this frame
    CNetworkSynchronisedScenes::Update();

    netObjectMgrBase::Update(bUpdateNetworkObjects);

	HandleTelemetryMetrics();

    if(bUpdateNetworkObjects)
    {
        CNetObjVehicle::ResetNumVehiclesOutsidePopulationRange();

        CNetworkObjectPopulationMgr::Update();
    }

    // update the pending projectiles list
    CNetworkPendingProjectiles::Update();

    // update the world state manager
    CNetworkScriptWorldStateMgr::Update();

    // update the world grid manager
    CNetworkWorldGridManager::Update();

	// update the ghost collisions
	CNetworkGhostCollisions::Update();

    // update whether we are owning too many objects
    UpdateIsOwningTooManyObjects();

    // try to dump any excess objects if we own too many
    TryToDumpExcessObjects();

#if __DEV
    //timeBarWrapper.StartTimer("Sanity Checking");
    SanityCheckObjectUsage();   
#endif

	if (m_FrameCloneCreateCapReference < fwTimer::GetFrameCount())
	{
		m_NumCappedCloneCreates = 0;	
	}   
	
	m_NumCappedCloneRemoves = 0;
}

CNetObjGame *CNetworkObjectMgr::GetNetworkObjectFromPlayer(const ObjectId objectID, const netPlayer &player, bool includeAll)
{
    netObject *object = netObjectMgrBase::GetNetworkObjectFromPlayer(objectID, player, includeAll);

    return SafeCast(CNetObjGame, object);
}

// Registers a game object with the manager to be cloned on remote machines.
// pGameObject - the game object
// objectType - the type of object
// flags - any flags that need set for the object
bool CNetworkObjectMgr::RegisterGameObject(netGameObjectBase *pGameObject,
                                           const NetObjFlags localFlags,
                                           const NetObjFlags globalFlags,
                                           const bool        makeSpaceForObject)
{
    USE_MEMBUCKET(MEMBUCKET_NETWORK);

    bool bRegistered        = false;
    bool isMissionObject    = (globalFlags & CNetObjGame::GLOBALFLAG_SCRIPTOBJECT) != 0;

    if(gnetVerify(pGameObject))
    {
        NetworkObjectType objectType      = static_cast<NetworkObjectType>(pGameObject->GetNetworkObjectType());

        if(makeSpaceForObject && !CNetworkObjectPopulationMgr::CanRegisterObject(objectType, isMissionObject))
        {
            TryToMakeSpaceForObject(objectType);
        }

        if(CNetworkObjectPopulationMgr::CanRegisterLocalObjectOfType(objectType, isMissionObject) && GetObjectIDManager().HasFreeObjectIdsAvailable(1, isMissionObject))
        {
            // don't register non mission objects if we currently own too many objects
            if(!IsOwningTooManyObjects() || isMissionObject || !CNetworkObjectPopulationMgr::ms_ProcessLocalPopulationLimits || makeSpaceForObject)
            {
                RegisterObject(pGameObject, GetObjectIDManager().GetNewObjectId(), localFlags, globalFlags);

                bRegistered = true;
#if __BANK
                m_eLastRegistrationFailureReason = REGISTRATION_SUCCESS;
#endif
            }
#if __BANK
            else
            {
                m_eLastRegistrationFailureReason = REGISTRATION_FAIL_TOO_MANY_OBJECTS;
            }
#endif
        }
        else
        {
#if __BANK
            if(GetObjectIDManager().HasFreeObjectIdsAvailable(1, isMissionObject))
            {
                m_eLastRegistrationFailureReason = REGISTRATION_FAIL_POPULATION;
            }
            else
            {
                m_eLastRegistrationFailureReason = REGISTRATION_FAIL_OBJECTIDS;
            }
#endif
        }

#if __BANK
		if (!bRegistered)
		{
			if (!PARAM_logonlyscriptnetregfailures.Get() || (PARAM_logonlyscriptnetregfailures.Get() && isMissionObject))
			{
				NetworkLogUtils::WriteLogEvent(GetLog(), "REGISTRATION_FAILURE", NetworkUtils::GetObjectTypeName(objectType, isMissionObject));
				GetLog().WriteDataValue("Failure reason", GetLastRegistrationFailureReason());	
				gnetAssertf(!isMissionObject || bRegistered, "Failed to register script game object. Failure reason: %s", GetLastRegistrationFailureReason());
			}			
		}
#endif    
	}

    return bRegistered;
}

void CNetworkObjectMgr::PlayerHasJoined(const netPlayer& player)
{
	DEV_ONLY(ValidateObjectsClonedState(player));

	netObjectMgrBase::PlayerHasJoined(player);

    CNetworkScriptWorldStateMgr::PlayerHasJoined(player);
    CNetworkWorldGridManager::PlayerHasJoined(player);

	NetworkInterface::BroadcastCachedLocalPlayerHeadBlendData(&player);

	DEV_ONLY(ValidateObjectsClonedState(player));
}

void CNetworkObjectMgr::PlayerHasLeft(const netPlayer& player)
{
	PhysicalPlayerIndex playerIndex = player.GetPhysicalPlayerIndex();

    CNetworkScriptWorldStateMgr::PlayerHasLeft(player);
    CNetworkWorldGridManager::PlayerHasLeft(player);

    netObjectMgrBase::PlayerHasLeft(player);

	NetworkInterface::ClearCachedPlayerHeadBlendData(player.GetPhysicalPlayerIndex());

    m_timeSinceLastTooManyObjectsACK[playerIndex] = 0;
    m_timeSinceLastNewObjectCreationFailure[playerIndex][CREATION_FAILURE_PED]      = 0;
    m_timeSinceLastNewObjectCreationFailure[playerIndex][CREATION_FAILURE_OBJECT]   = 0;
    m_timeSinceLastNewObjectCreationFailure[playerIndex][CREATION_FAILURE_VEHICLE]  = 0;

	DEV_ONLY(ValidateObjectsClonedState(player));
}

 // returns the static sync tree for the given object type
CProjectSyncTree* CNetworkObjectMgr::GetSyncTree(NetworkObjectType objectType)
{
    switch(objectType)
    {
    case NET_OBJ_TYPE_PLAYER:
            return CNetObjPlayer::GetStaticSyncTree();

    case NET_OBJ_TYPE_PED:
            return CNetObjPed::GetStaticSyncTree();

    case NET_OBJ_TYPE_AUTOMOBILE:
            return CNetObjAutomobile::GetStaticSyncTree();

    case NET_OBJ_TYPE_OBJECT:
            return CNetObjObject::GetStaticSyncTree();

    case NET_OBJ_TYPE_PICKUP:
            return CNetObjPickup::GetStaticSyncTree();

    case NET_OBJ_TYPE_PICKUP_PLACEMENT:
            return CNetObjPickupPlacement::GetStaticSyncTree();

    case NET_OBJ_TYPE_BIKE:
            return CNetObjBike::GetStaticSyncTree();

    case NET_OBJ_TYPE_HELI:
            return CNetObjHeli::GetStaticSyncTree();

	case NET_OBJ_TYPE_PLANE:
			return CNetObjPlane::GetStaticSyncTree();

	case NET_OBJ_TYPE_SUBMARINE:
			return CNetObjSubmarine::GetStaticSyncTree();

    case NET_OBJ_TYPE_BOAT:
            return CNetObjBoat::GetStaticSyncTree();

    case NET_OBJ_TYPE_TRAILER:
            return CNetObjTrailer::GetStaticSyncTree();

    case NET_OBJ_TYPE_TRAIN:
            return CNetObjTrain::GetStaticSyncTree();

	case NET_OBJ_TYPE_DOOR:
		return CNetObjDoor::GetStaticSyncTree();

#if GLASS_PANE_SYNCING_ACTIVE
	case NET_OBJ_TYPE_GLASS_PANE:
		return CNetObjGlassPane::GetStaticSyncTree();
#endif /* GLASS_PANE_SYNCING_ACTIVE */
    }

    AssertMsg(0, "Unexpected object type!");

    return NULL;
}

void CNetworkObjectMgr::ConvertRandomObjectToScriptObject(CNetObjGame* pObject)
{
    if (gnetVerifyf(!pObject->IsClone(), "Trying to convert an ambient clone to a script object"))
    {
        NetworkLogUtils::WriteLogEvent(GetLog(), "RANDOM_TO_SCRIPT_CONVERSION", pObject->GetLogName());

        pObject->SetGlobalFlag(CNetObjGame::GLOBALFLAG_SCRIPTOBJECT, true);
		pObject->SetGlobalFlag(CNetObjGame::GLOBALFLAG_WAS_SCRIPTOBJECT, false);

		// the object has to start synchronising again so that it's new script state is broadcast
		if (!pObject->GetSyncData())
			pObject->StartSynchronising();

        // force an update on all the script nodes to ensure that they are sent in the next update for this object
        if (AssertVerify(pObject->GetSyncTree()))
        {
			static_cast<CProjectSyncTree*>(pObject->GetSyncTree())->DirtyNodesThatDifferFromInitialState(pObject, ACTIVATIONFLAG_SCRIPTOBJECT);
			pObject->GetSyncTree()->ForceSendOfSyncUpdateNodes(SERIALISEMODE_FORCE_SEND_OF_DIRTY, ACTIVATIONFLAG_SCRIPTOBJECT, pObject);
	    }
    }
}

// Name         :   IsOwningTooManyObjects
// Purpose      :   returns whether the object manager is controlling more objects that it can handle
bool CNetworkObjectMgr::IsOwningTooManyObjects()
{
    return m_isOwningTooManyObjects;
}

// Name         :   GetNumStarvingObjects
// Purpose      :   returns the number of objects that are being starved of updates controlled by this player
u32 CNetworkObjectMgr::GetNumStarvingObjects()
{
    u32 numStarvingObjects = 0;

    const atDList<netObject*, datBase> *listsToProcess[MAX_NUM_PHYSICAL_PLAYERS];
    unsigned numListsToProcess = GetLocalObjectLists(listsToProcess);

    for(unsigned index = 0; index < numListsToProcess; index++)
    {
        const atDNode<netObject*, datBase> *node = listsToProcess[index] ? listsToProcess[index]->GetHead() : 0;

        while (node)
        {
            netObject *networkObject = node->Data;
            node = node->GetNext();

            if(networkObject && networkObject->IsStarving())
            {
                if(PARAM_logstarvationdata.Get())
                {
                    gnetDebug2("%s is starving!", networkObject->GetLogName());

                    for(PhysicalPlayerIndex playerIndex = 0; playerIndex < MAX_NUM_PHYSICAL_PLAYERS; playerIndex++)
                    {
                        if(networkObject->IsStarving(playerIndex))
                        {
                            netPlayer *player = netInterface::GetPhysicalPlayerFromIndex(playerIndex);

                            if(player)
                            {
                                gnetDebug2("%s is starving on %s!", networkObject->GetLogName(), player->GetLogName());
                            }
                            else
                            {
                                gnetDebug2("%s is starving on physical player %d (no player object)!", networkObject->GetLogName(), playerIndex);
                            }
                        }
                    }
                }

                numStarvingObjects++;
            }
        }
    }

    return numStarvingObjects;
}

//
// Name         :   TooManyObjectsOwnedACKReceived
// Purpose      :   function to indicate a give control event has been received with a TOO_MANY_OBJECTS ack code
void CNetworkObjectMgr::TooManyObjectsOwnedACKReceived(const netPlayer& player)
{
	if (gnetVerify(player.GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX))
	{
		m_timeSinceLastTooManyObjectsACK[player.GetPhysicalPlayerIndex()] = fwTimer::GetSystemTimeInMilliseconds();
	}
}

//
// Name         :   TooManyObjectsCreatedACKReceived
// Purpose      :   function to indicate a clone create message has been received with a TOO_MANY_OBJECTS ack code
void CNetworkObjectMgr::TooManyObjectsCreatedACKReceived(const netPlayer& player, NetworkObjectType objectType)
{
    CreationFailureTypes creationFailType = GetCreationFailureTypeFromObjectType(objectType);
    gnetAssert(creationFailType != MAX_CREATION_FAILURE_TYPES);

	if(creationFailType != MAX_CREATION_FAILURE_TYPES && gnetVerify(player.GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX))
    {
        m_timeSinceLastNewObjectCreationFailure[player.GetPhysicalPlayerIndex()][creationFailType] = fwTimer::GetSystemTimeInMilliseconds();
    }
}

//
// Name         :   RemotePlayerCanAcceptObjects
// Purpose      :   returns whether a remote player can accept more objects based on the
//                  last time a TOO_MANY_OBJECTS ack code was received
bool CNetworkObjectMgr::RemotePlayerCanAcceptObjects(const netPlayer& player, bool proximity)
{
    bool canAcceptObjects = false;

	if (gnetVerify(player.GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX))
	{
		u32 timeNow          = fwTimer::GetSystemTimeInMilliseconds();
		u32 timeSinceLastACK = (timeNow > m_timeSinceLastTooManyObjectsACK[player.GetPhysicalPlayerIndex()]) ? (timeNow - m_timeSinceLastTooManyObjectsACK[player.GetPhysicalPlayerIndex()]) : 0;
		u32 timeoutValue     = proximity ? m_tooManyObjectsACKTimeoutProximity :m_tooManyObjectsACKTimeoutBalancing;

		if(timeSinceLastACK > timeoutValue)
		{
			canAcceptObjects = true;
		}
	}

    return canAcceptObjects;
}

//
// Name         :   RemotePlayerCanAcceptNewObjectsFromOurPlayer
// Purpose      :   returns whether a player can accept new objects created by out player
//                  last time a TOO_MANY_OBJECTS ack code was received
bool CNetworkObjectMgr::RemotePlayerCanAcceptNewObjectsFromOurPlayer(const netPlayer& player, NetworkObjectType objectType)
{
    bool canAcceptObjects = false;

    CreationFailureTypes creationFailType = GetCreationFailureTypeFromObjectType(objectType);
    gnetAssert(creationFailType != MAX_CREATION_FAILURE_TYPES);

	if(creationFailType != MAX_CREATION_FAILURE_TYPES && gnetVerify(player.GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX))
    {
        u32 timeNow          = fwTimer::GetSystemTimeInMilliseconds();
        u32 timeSinceLastACK = (timeNow > m_timeSinceLastNewObjectCreationFailure[player.GetPhysicalPlayerIndex()][creationFailType]) ? (timeNow - m_timeSinceLastNewObjectCreationFailure[player.GetPhysicalPlayerIndex()][creationFailType]) : 0;

        static const unsigned int TIMEOUT_VALUE = 3000;
        if(timeSinceLastACK > TIMEOUT_VALUE)
        {
            canAcceptObjects = true;
        }
    }

    return canAcceptObjects;
}

u32 CNetworkObjectMgr::GetNumLocalPedsInVehicles()
{
    u32 numLocalPedsInVehicles = 0;

    const atDList<netObject*, datBase> *listsToProcess[MAX_NUM_PHYSICAL_PLAYERS];
    unsigned numListsToProcess = GetLocalObjectLists(listsToProcess);

    for(unsigned index = 0; index < numListsToProcess; index++)
    {
        const atDNode<netObject*, datBase> *node = listsToProcess[index] ? listsToProcess[index]->GetHead() : 0;

        while (node)
        {
            netObject *networkObject = node->Data;

            CPed *ped = NetworkUtils::GetPedFromNetworkObject(networkObject);

            if(ped)
            {
                if(ped->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && ped->GetMyVehicle())
                {
                    numLocalPedsInVehicles++;
                }
            }

            node = node->GetNext();
        }
    }

    return numLocalPedsInVehicles;
}

//
// Name         :   GetNumLocalVehiclesUsingPretendOccupants
// Purpose      :   returns the number of locally owned vehicles currently using pretend occupants
u32 CNetworkObjectMgr::GetNumLocalVehiclesUsingPretendOccupants()
{
    u32 numVehiclesUsingPretendOccupants = 0;

    const atDList<netObject*, datBase> *listsToProcess[MAX_NUM_PHYSICAL_PLAYERS];
    unsigned numListsToProcess = GetLocalObjectLists(listsToProcess);

    for(unsigned index = 0; index < numListsToProcess; index++)
    {
        const atDNode<netObject*, datBase> *node = listsToProcess[index] ? listsToProcess[index]->GetHead() : 0;

        while (node)
        {
            netObject *networkObject = node->Data;

            CVehicle *vehicle = NetworkUtils::GetVehicleFromNetworkObject(networkObject);

            if(vehicle)
            {
                if(vehicle->m_nVehicleFlags.bUsingPretendOccupants)
                {
                    numVehiclesUsingPretendOccupants++;
                }
            }

            node = node->GetNext();
        }
    }

    return numVehiclesUsingPretendOccupants;
}

u32 CNetworkObjectMgr::GetNumClosePlayerVehiclesNotOnOurMachine()
{
    u32 numVehicles = 0;

    if(FindPlayerPed())
    {
        unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
        const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

		Vector3 myPlayerPos = NetworkInterface::GetLocalPlayerFocusPosition();

        for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
        {
	        const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);

            CNetObjPed *netObjPed = pPlayer->GetPlayerPed() ? static_cast<CNetObjPed *>(pPlayer->GetPlayerPed()->GetNetworkObject()) : 0;

            if(netObjPed && netObjPed->GetPedsTargetVehicleId() != NETWORK_INVALID_OBJECT_ID)
            {
                netObject *netObjVehicle = GetNetworkObject(netObjPed->GetPedsTargetVehicleId());

                if(netObjVehicle == 0)
                {
                    // player vehicles are no longer always cloned on every player's machines, so we only
                    // care about vehicles that are approaching us
                    CEntity *entity = netObjPed->GetEntity();

                    float distanceSquared = (VEC3V_TO_VECTOR3(entity->GetTransform().GetPosition()) - myPlayerPos).Mag2();

                    if(distanceSquared < rage::square(CNetObjVehicle::GetStaticScopeData().m_scopeDistance))
                    {
                        numVehicles++;
                    }
                }
            }
        }
    }

    return numVehicles;
}

// Name         :   AnyLocalObjectPendingTutorialSessionReassign
// Purpose      :   returns true if any of the re-assignable local objects are still in the local objects list 
bool CNetworkObjectMgr::AnyLocalObjectPendingTutorialSessionReassign()
{
    const atDList<netObject*, datBase> *listsToProcess[MAX_NUM_PHYSICAL_PLAYERS];
    unsigned numListsToProcess = GetLocalObjectLists(listsToProcess);

    for(unsigned index = 0; index < numListsToProcess; index++)
    {
        const atDNode<netObject*, datBase> *node = listsToProcess[index] ? listsToProcess[index]->GetHead() : 0;

        while (node)
        {
		    netObject *networkObject = node->Data;

		    if(networkObject)
		    {
			    CNetObjGame* pGameObj = static_cast<CNetObjGame*>(networkObject);

			    NetworkObjectType objtype = pGameObj->GetObjectType();

			    bool typeToReassign =	IsAPendingTutorialSessionReassignType(objtype);

			    if( typeToReassign ) 
			    {
				    return true;
			    }
		    }

		    node = node->GetNext();
        }
	}

	return false;
}
//
// Name         :   IsPlayerVehicle
// Purpose      :   Returns true if the given object id is for a vehicle which another player is using
bool CNetworkObjectMgr::IsPlayerVehicle(const ObjectId objectID, const netPlayer** ppPlayer)
{
    unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
    const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

    for(unsigned index = 0; index < numPhysicalPlayers; index++)
    {
        const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);

        if (!pPlayer->IsMyPlayer())
        {
            CNetObjPed* pPedObj = pPlayer->GetPlayerPed() ? static_cast<CNetObjPed*>(pPlayer->GetPlayerPed()->GetNetworkObject()) : 0;

            if (pPedObj && pPedObj->GetPedsTargetVehicleId() == objectID)
            {
                if (ppPlayer)
                {
                    *ppPlayer = pPlayer;
                }

                return true;
            }
        }
    }

    return false;
}

//
// Name         :   IsLocalPlayerPhysicalOnRemoteMachine
// Purpose      :   returns whether the local player is a physical player on the specified
//                  remote player's machine.
bool CNetworkObjectMgr::IsLocalPlayerPhysicalOnRemoteMachine(unsigned peerPlayerIndex, netPlayer &remotePlayer)
{
    CNetGamePlayer *playerToCheck = SafeCast(CNetGamePlayer, NetworkInterface::GetPlayerMgr().GetMyPlayer());

	// if the remote player has no physical player index, then we won't have cloned anything on his machine yet
	if (remotePlayer.GetPhysicalPlayerIndex() == INVALID_PLAYER_INDEX)
		return false;

    if(playerToCheck && peerPlayerIndex > 0)
    {
        playerToCheck = SafeCast(CNetGamePlayer, netInterface::GetPlayerFromPeerPlayerIndex(peerPlayerIndex, playerToCheck->GetGamerInfo().GetPeerInfo()));
        gnetAssert(playerToCheck);
    }

    netObject *playerObject = (playerToCheck && playerToCheck->GetPlayerPed()) ? playerToCheck->GetPlayerPed()->GetNetworkObject() : 0;

    return playerObject ? playerObject->HasBeenCloned(remotePlayer) : false;
}

void CNetworkObjectMgr::CreateSyncTrees()
{
    CNetObjAutomobile::CreateSyncTree();
    CNetObjBike::CreateSyncTree();
	CNetObjBoat::CreateSyncTree();
	CNetObjSubmarine::CreateSyncTree();
    CNetObjHeli::CreateSyncTree();
    CNetObjObject::CreateSyncTree();
    CNetObjPed::CreateSyncTree();
#if GLASS_PANE_SYNCING_ACTIVE
	CNetObjGlassPane::CreateSyncTree();
#endif /* GLASS_PANE_SYNCING_ACTIVE */
    CNetObjPickup::CreateSyncTree();
    CNetObjPickupPlacement::CreateSyncTree();
    CNetObjPlayer::CreateSyncTree();
	CNetObjTrain::CreateSyncTree();
	CNetObjPlane::CreateSyncTree();
	CNetObjDoor::CreateSyncTree();
}

void CNetworkObjectMgr::DestroySyncTrees()
{
    CNetObjAutomobile::DestroySyncTree();
    CNetObjBike::DestroySyncTree();
	CNetObjBoat::DestroySyncTree();
	CNetObjSubmarine::DestroySyncTree();
    CNetObjHeli::DestroySyncTree();
    CNetObjObject::DestroySyncTree();
    CNetObjPed::DestroySyncTree();
#if GLASS_PANE_SYNCING_ACTIVE
	CNetObjGlassPane::DestroySyncTree();
#endif /* GLASS_PANE_SYNCING_ACTIVE */
    CNetObjPickup::DestroySyncTree();
    CNetObjPickupPlacement::DestroySyncTree();
    CNetObjPlayer::DestroySyncTree();
	CNetObjTrain::DestroySyncTree();
	CNetObjPlane::DestroySyncTree();
	CNetObjDoor::DestroySyncTree();
}

void CNetworkObjectMgr::UpdateSyncTreeBatches()
{
    unsigned currTime = netInterface::GetSynchronisationTime();

    CNetObjAutomobile::GetStaticSyncTree()->UpdateCurrentObjectBatches(currTime);
    CNetObjBike::GetStaticSyncTree()->UpdateCurrentObjectBatches(currTime);
	CNetObjBoat::GetStaticSyncTree()->UpdateCurrentObjectBatches(currTime);
	CNetObjSubmarine::GetStaticSyncTree()->UpdateCurrentObjectBatches(currTime);
    CNetObjHeli::GetStaticSyncTree()->UpdateCurrentObjectBatches(currTime);
    CNetObjObject::GetStaticSyncTree()->UpdateCurrentObjectBatches(currTime);
    CNetObjPed::GetStaticSyncTree()->UpdateCurrentObjectBatches(currTime);
#if GLASS_PANE_SYNCING_ACTIVE
	CNetObjGlassPane::GetStaticSyncTree()->UpdateCurrentObjectBatches(currTime);
#endif /* GLASS_PANE_SYNCING_ACTIVE */
    CNetObjPickup::GetStaticSyncTree()->UpdateCurrentObjectBatches(currTime);
    CNetObjPickupPlacement::GetStaticSyncTree()->UpdateCurrentObjectBatches(currTime);
    CNetObjPlayer::GetStaticSyncTree()->UpdateCurrentObjectBatches(currTime);
	CNetObjTrain::GetStaticSyncTree()->UpdateCurrentObjectBatches(currTime);
	CNetObjPlane::GetStaticSyncTree()->UpdateCurrentObjectBatches(currTime);
	CNetObjDoor::GetStaticSyncTree()->UpdateCurrentObjectBatches(currTime);
}

// Name         :   InitNetworkObjectPools
// Purpose      :   initialise the network object pools
void CNetworkObjectMgr::InitNetworkObjectPools()
{
    CNetObjObject::InitPool( MEMBUCKET_NETWORK );
    CNetObjPed::InitPool( MEMBUCKET_NETWORK );
    CNetObjVehicle::InitPool( MEMBUCKET_NETWORK );
#if GLASS_PANE_SYNCING_ACTIVE
	CNetObjGlassPane::InitPool( MEMBUCKET_NETWORK );
#endif /* GLASS_PANE_SYNCING_ACTIVE */
    CNetObjPickup::InitPool( MEMBUCKET_NETWORK );
    CNetObjPickupPlacement::InitPool( MEMBUCKET_NETWORK );
	CNetObjDoor::InitPool( MEMBUCKET_NETWORK );
	CPendingScriptObjInfo::InitPool( MEMBUCKET_NETWORK );
	CNetObjPed::CNetTennisMotionData::InitPool( MEMBUCKET_NETWORK ); 
    CNetViewPortWrapper::InitPool( MEMBUCKET_NETWORK );
}

// Name         :   ShutdownNetworkObjectPools
// Purpose      :   shuts down the network object pools
void CNetworkObjectMgr::ShutdownNetworkObjectPools()
{
    CNetObjObject::ShutdownPool();
    CNetObjPed::ShutdownPool();
    CNetObjVehicle::ShutdownPool();
#if GLASS_PANE_SYNCING_ACTIVE
	CNetObjGlassPane::ShutdownPool();
#endif /* GLASS_PANE_SYNCING_ACTIVE */
    CNetObjPickup::ShutdownPool();
    CNetObjPickupPlacement::ShutdownPool();
	CNetObjDoor::ShutdownPool();
	CPendingScriptObjInfo::ShutdownPool();
	CNetObjPed::CNetTennisMotionData::ShutdownPool();
    CNetViewPortWrapper::ShutdownPool();

    // DAN TEMP - temporary fix for a crash when leaving a network game - will be fixed properly as part of the object manager refactoring effort
    unsigned           numActivePlayers = netInterface::GetNumActivePlayers();
    netPlayer * const *activePlayers    = netInterface::GetAllActivePlayers();
    
    for(unsigned index = 0; index < numActivePlayers; index++)
    {
        netPlayer *pPlayer = activePlayers[index];
        pPlayer->SetNonPhysicalData(0);
    }
    // END DAN TEMP
}

// Name         :   InitDeltaBufferPools
// Purpose      :   initialise the delta buffer pools
void CNetworkObjectMgr::InitSyncDataPools()
{
    CNetwork::UseNetworkHeap(NET_MEM_PLAYER_SYNC_DATA);
    CNetObjPlayer::CPlayerSyncData::InitPool( MEMBUCKET_NETWORK );
    CNetwork::StopUsingNetworkHeap();

    CNetwork::UseNetworkHeap(NET_MEM_PED_SYNC_DATA);
    CNetObjPed::CPedSyncData::InitPool( MEMBUCKET_NETWORK );
    CNetwork::StopUsingNetworkHeap();

    CNetwork::UseNetworkHeap(NET_MEM_VEHICLE_SYNC_DATA);
    CNetObjVehicle::CVehicleSyncData::InitPool( MEMBUCKET_NETWORK );
    CNetwork::StopUsingNetworkHeap();

    CNetwork::UseNetworkHeap(NET_MEM_OBJECT_SYNC_DATA);
    CNetObjObject::CObjectSyncData::InitPool( MEMBUCKET_NETWORK );
    CNetwork::StopUsingNetworkHeap();

	CNetwork::UseNetworkHeap(NET_MEM_DOOR_SYNC_DATA);
	CNetObjDoor::CDoorSyncData::InitPool( MEMBUCKET_NETWORK );
	CNetwork::StopUsingNetworkHeap();

    CNetwork::UseNetworkHeap(NET_MEM_PICKUP_SYNC_DATA);
    CNetObjPickup::CPickupSyncData::InitPool( MEMBUCKET_NETWORK );
    CNetwork::StopUsingNetworkHeap();

    CNetwork::UseNetworkHeap(NET_MEM_PICKUP_PLACEMENT_SYNC_DATA);
    CNetObjPickupPlacement::CPickupPlacementSyncData::InitPool( MEMBUCKET_NETWORK );
    CNetwork::StopUsingNetworkHeap();

#if GLASS_PANE_SYNCING_ACTIVE
    CNetwork::UseNetworkHeap(NET_MEM_GLASS_PANE_SYNC_DATA);
	CNetObjGlassPane::CGlassPaneSyncData::InitPool( MEMBUCKET_NETWORK );
    CNetwork::StopUsingNetworkHeap();
#endif /* GLASS_PANE_SYNCING_ACTIVE */
}

// Name         :   ShutdownDeltaBufferPools
// Purpose      :   shutdown the delta buffer pools
void CNetworkObjectMgr::ShutdownSyncDataPools()
{
    CNetwork::UseNetworkHeap(NET_MEM_PLAYER_SYNC_DATA);
    CNetObjPlayer::CPlayerSyncData::ShutdownPool();
    CNetwork::StopUsingNetworkHeap();

    CNetwork::UseNetworkHeap(NET_MEM_PED_SYNC_DATA);
    CNetObjPed::CPedSyncData::ShutdownPool();
    CNetwork::StopUsingNetworkHeap();

    CNetwork::UseNetworkHeap(NET_MEM_VEHICLE_SYNC_DATA);
    CNetObjVehicle::CVehicleSyncData::ShutdownPool();
    CNetwork::StopUsingNetworkHeap();

    CNetwork::UseNetworkHeap(NET_MEM_OBJECT_SYNC_DATA);
    CNetObjObject::CObjectSyncData::ShutdownPool();
    CNetwork::StopUsingNetworkHeap();

    CNetwork::UseNetworkHeap(NET_MEM_DOOR_SYNC_DATA);
    CNetObjDoor::CDoorSyncData::ShutdownPool();
    CNetwork::StopUsingNetworkHeap();

    CNetwork::UseNetworkHeap(NET_MEM_PICKUP_SYNC_DATA);
    CNetObjPickup::CPickupSyncData::ShutdownPool();
    CNetwork::StopUsingNetworkHeap();

    CNetwork::UseNetworkHeap(NET_MEM_PICKUP_PLACEMENT_SYNC_DATA);
    CNetObjPickupPlacement::CPickupPlacementSyncData::ShutdownPool();
    CNetwork::StopUsingNetworkHeap();

#if GLASS_PANE_SYNCING_ACTIVE
   	CNetwork::UseNetworkHeap(NET_MEM_GLASS_PANE_SYNC_DATA);
   	CNetObjGlassPane::CGlassPaneSyncData::ShutdownPool();
	CNetwork::StopUsingNetworkHeap();
#endif /* GLASS_PANE_SYNCING_ACTIVE */
}

// Name         :   RegisterNetworkObject
// Purpose      :   Registers a network object with the manager to be cloned on remote machines.
// Parameters   :   pNetObj- the network object
void CNetworkObjectMgr::RegisterNetworkObject(netObject* pNetObj)
{
    netObjectMgrBase::RegisterNetworkObject(pNetObj);

    CNetworkObjectPopulationMgr::OnNetworkObjectCreated(*pNetObj);

#if ENABLE_NETWORK_LOGGING
    if (pNetObj && !pNetObj->IsClone())
    {
        CheckNetworkObjectSpawnDistance(pNetObj);
	}
#endif // ENABLE_NETWORK_LOGGING
}

// Name         :   UnregisterNetworkObject
// Purpose      :   Removes the object from the object manager and tells all other managers to remove the clones
//                  corresponding to this object
// Parameters   :   pNetObj - the object being unregistered
//                  bForce - this is set when the object manager is being shutdown and so cannot clean up the
//                           objects properly.
//                  bDestroyObject - removes game object if it is local
void CNetworkObjectMgr::UnregisterNetworkObject(netObject* pNetObj, unsigned reason, bool bForce, bool bDestroyObject)
{
	NETWORK_DEBUG_BREAK_FOR_FOCUS(BREAK_TYPE_FOCUS_DESTROY, *pNetObj);

#if ENABLE_NETWORK_LOGGING

    float distance = 0.0f;
    bool  tooClose = IsObjectTooCloseForCreationOrRemoval(pNetObj, distance);

    if(tooClose)
    {
        sysStack::PrintStackTrace();

        char buffer[128];
        sprintf(buffer, "REMOVED_TOO_CLOSE (%.2fm)", distance);
        NetworkLogUtils::WritePlayerText(GetLog(), pNetObj->GetPhysicalPlayerIndex(), buffer, pNetObj->GetLogName());
        if(IsVehicleObjectType(pNetObj->GetObjectType()) && !pNetObj->IsClone())
        {
            GetLog().WriteDataValue("Reason", CVehiclePopulation::GetLastRemovalReason());
        }
    }

#endif // ENABLE_NETWORK_LOGGING

     CNetworkObjectPopulationMgr::OnNetworkObjectRemoved(*pNetObj);

	if (reason == OWNERSHIP_CONFLICT)
	{
		pNetObj->SetLocalFlag(CNetObjGame::LOCALFLAG_OWNERSHIP_CONFLICT, true);
	}

    // Script objects that are not running the script they are is associated with have to be cleaned up before going back to single player, so that
    // they get cleaned up properly by the ambient population manager.
    if (!bDestroyObject && 
		pNetObj->HasGameObject() && 
		pNetObj->GetScriptObjInfo() && 
		!pNetObj->IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_SCRIPT_MIGRATION)&& 
		!pNetObj->IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_CLONEONLY_SCRIPT))
    {
        if (!CTheScripts::GetScriptHandlerMgr().GetScriptHandler(pNetObj->GetScriptObjInfo()->GetScriptId()))
        {
            static_cast<CNetObjGame*>(pNetObj)->ConvertToAmbientObject();
        }
    }

#if ENABLE_NETWORK_LOGGING
	if(!tooClose && reason == GAME_UNREGISTRATION && IsVehicleObjectType(pNetObj->GetObjectType()) && !pNetObj->IsClone())
	{
		GetLog().WriteDataValue("Game unregistration reason", CVehiclePopulation::GetLastRemovalReason());
	}
#endif // ENABLE_NETWORK_LOGGING

	netObjectMgrBase::UnregisterNetworkObject(pNetObj, reason, bForce, bDestroyObject);
}

// clones an object on the given player
void CNetworkObjectMgr::CloneObject(netObject* pObject, const netPlayer& player, datBitBuffer &createDataBuffer)
{
    if(!RemotePlayerCanAcceptNewObjectsFromOurPlayer(player, pObject->GetObjectType()) && !IsImportantObject(pObject))
        return;

	NETWORK_DEBUG_BREAK_FOR_FOCUS(BREAK_TYPE_FOCUS_CLONE_CREATE, *pObject);

	netObjectMgrBase::CloneObject(pObject, player, createDataBuffer);
}

// Removes an object's clone on the given player (sends a removal message)
bool CNetworkObjectMgr::RemoveClone(netObject *object, const netPlayer &player, bool forceSend)
{
	NETWORK_DEBUG_BREAK_FOR_FOCUS(BREAK_TYPE_FOCUS_CLONE_REMOVE, *object);

	return netObjectMgrBase::RemoveClone(object, player, forceSend);
}

// Name         :   SyncClone
// Purpose      :   Synchronizes an object with its clone on the given player
// Parameters   :   pObject - the object whose clone needs to be synchronized
//                  playerIndex - the player the object's clone is on
// Returns      :   true if a sync message was sent
bool CNetworkObjectMgr::SyncClone(netObject* pObject, const netPlayer& player)
{
	NETWORK_DEBUG_BREAK_FOR_FOCUS(BREAK_TYPE_FOCUS_WRITE_SYNC, *pObject);

    bool success = netObjectMgrBase::SyncClone(pObject, player);

#if ENABLE_NETWORK_LOGGING
	if (gnetVerify(player.GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX))
	{
		if(pObject->IsStarving(player.GetPhysicalPlayerIndex()))
		{
			NetworkLogUtils::WritePlayerText(GetLog(), player.GetPhysicalPlayerIndex(), "OBJECT_STARVING", pObject->GetLogName());
		}
	}
#endif // ENABLE_NETWORK_LOGGING

    return success;
}

#if __BANK

static bool ShouldAddLatencySample(const netObject &networkObject, const netPlayer &player)
{
    // we just add latency samples for the first player connected,for either their player ped or the vehicle they are in
    if(player.IsLocal() || (netInterface::GetNumRemotePhysicalPlayers() > 0 && netInterface::GetRemotePhysicalPlayers()[0] == &player))
    {
        if(networkObject.GetPlayerOwner() == &player)
        {
            if(networkObject.GetObjectType() == NET_OBJ_TYPE_PLAYER)
            {
                const CPed *ped = NetworkUtils::GetPedFromNetworkObject(&networkObject);

                if(ped && !ped->GetIsInVehicle())
                {
                    return true;
                }
            }

            const CVehicle *vehicle = NetworkUtils::GetVehicleFromNetworkObject(&networkObject);

            if(vehicle)
            {
                const CPed *driver = vehicle->GetDriver();

                if(driver && driver->IsPlayer() && driver->GetNetworkObject() && driver->GetNetworkObject()->GetPlayerOwner() == &player)
                {
                    return true;
                }
            }
        }
    }

    return false;
}

#endif // __BANK

void CNetworkObjectMgr::OnSendCloneSync(const netObject *BANK_ONLY(object), unsigned BANK_ONLY(timestamp))
{
#if __BANK
    netPlayer *localPlayer = NetworkInterface::GetLocalPlayer();

    if(object && localPlayer && ShouldAddLatencySample(*object, *localPlayer))
    {
        NetworkDebug::AddOutboundLatencyCloneSyncSample(NetworkInterface::GetNetworkTime() - timestamp);
    }
#endif // __BANK
}

void CNetworkObjectMgr::OnSendConnectionSync(unsigned BANK_ONLY(timestamp))
{
    BANK_ONLY(NetworkDebug::AddOutboundLatencyConnectionSyncSample(NetworkInterface::GetNetworkTime() - timestamp));
}

bool CNetworkObjectMgr::ShouldSendCloneSyncImmediately(const netPlayer &player, const netObject &networkObject)
{
    const float SEND_IMMEDIATE_SPEED_THRESHOLD = rage::square(1.0f);

    // we send immediate clone syncs for moving vehicles containing the remote players ped or if they are specatating the driver
    const CVehicle *vehicle = NetworkUtils::GetVehicleFromNetworkObject(&networkObject);

    if(vehicle && vehicle->GetVelocity().Mag2() > SEND_IMMEDIATE_SPEED_THRESHOLD)
    {
        const CPed          *playerPed    = ((CNetGamePlayer *)&player)->GetPlayerPed();
        const CNetObjPlayer *netObjPlayer = playerPed ? SafeCast(CNetObjPlayer, playerPed->GetNetworkObject()) : 0;

        if(netObjPlayer && netObjPlayer->IsSpectating())
        {
            const CPed *driver = vehicle->GetDriver();

            if(driver && driver->IsPlayer() && driver->GetNetworkObject() && driver->GetNetworkObject()->GetObjectID() == netObjPlayer->GetSpectatorObjectId())
            {
                return true;
            }
        }
        else if(vehicle->ContainsPed(playerPed))
        {
            return true;
        }
    }

    return false;
}

#if ENABLE_NETWORK_LOGGING
void CNetworkObjectMgr::CheckNetworkObjectSpawnDistance(netObject* pNetObj)
{
    if (pNetObj)
    {
        float distance = 0.0f;
        bool  tooClose = IsObjectTooCloseForCreationOrRemoval(pNetObj, distance);

        if (tooClose)
        {
            char buffer[128];
            sprintf(buffer, "CREATED_TOO_CLOSE (%.2fm)", distance);
            NetworkLogUtils::WritePlayerText(GetLog(), pNetObj->GetPhysicalPlayerIndex(), buffer, pNetObj->GetLogName());
        }
        else
        {
            bool  tooFar = IsObjectTooFarForCreation(pNetObj, distance);

            if (tooFar)
            {
                char buffer[128];
                sprintf(buffer, "CREATED_TOO_FAR (%.2fm)", distance);
                NetworkLogUtils::WritePlayerText(GetLog(), pNetObj->GetPhysicalPlayerIndex(), buffer, pNetObj->GetLogName());
            }
        }
    }
}
#endif // ENABLE_NETWORK_LOGGING

//
// name:        ProcessCloneCreateData
// description: processes clone create data
//
void CNetworkObjectMgr::ProcessCloneCreateData(const netPlayer   &fromPlayer,
                                               const netPlayer   &toPlayer,
                                               NetworkObjectType  objectType,
                                               const ObjectId     objectID,
                                               NetObjFlags        objectFlags,
                                               datBitBuffer      &createDataMessageBuffer,
                                               unsigned           timestamp)
{
    NetObjectAckCode ackCode = ACKCODE_NONE;

	bool isScriptObject = (objectFlags & CNetObjGame::GLOBALFLAG_SCRIPTOBJECT) != 0 || (objectFlags & CNetObjGame::GLOBALFLAG_WAS_SCRIPTOBJECT) != 0;
	bool isReservedObject = (objectFlags & CNetObjGame::GLOBALFLAG_RESERVEDOBJECT) != 0;

    CNetwork::GetMessageLog().Log("\tRECEIVED_CLONE_CREATE\t%s_%d\r\n", NetworkUtils::GetObjectTypeName(objectType, isScriptObject), objectID);

    CNetwork::GetMessageLog().WriteDataValue("Timestamp", "%d", timestamp);

	NETWORK_DEBUG_BREAK_FOR_FOCUS_ID(BREAK_TYPE_FOCUS_CREATE, objectID);

    bool createNetworkObject = true;
	netObject *networkObject = 0;

    bool isObjectTypeCapped = UseCloneCreateCapping(objectType);

	bool bCapped = false;

    if(isObjectTypeCapped && m_NumCappedCloneCreates >= DEFAULT_CAPPED_CLONE_CREATE_LIMIT)
    {
        CNetwork::GetMessageLog().WriteDataValue("FAIL", "The limit for clone creates for capped objects types has been reached for this frame!");
		createNetworkObject = false;
		ackCode = ACKCODE_FAIL;
		bCapped = true;
    }
    else
    {
        ActivationFlags actFlags = isScriptObject ? ACTIVATIONFLAG_SCRIPTOBJECT : ACTIVATIONFLAG_AMBIENTOBJECT;

        // read the message data into the nodes
        GetSyncTree(objectType)->Read(SERIALISEMODE_CREATE, actFlags, createDataMessageBuffer, &CNetwork::GetMessageLog());
    }

    // reject clone creates from players that do not have a valid physical player index
    if (fromPlayer.GetPhysicalPlayerIndex() == INVALID_PLAYER_INDEX)
	{
		CNetwork::GetMessageLog().WriteDataValue("FAIL", "Player does not have a valid physical player index");
		createNetworkObject = false;
		ackCode = ACKCODE_FAIL;
	}

	// reject clone creates from players that are not in our roaming bubble
	if (!NetworkInterface::GetLocalPlayer()->IsInSameRoamingBubble(static_cast<const CNetGamePlayer&>(fromPlayer)))
	{
		CNetwork::GetMessageLog().WriteDataValue("FAIL", "Player is not in our roaming bubble");
		createNetworkObject = false;
		ackCode = ACKCODE_FAIL;
	}

	if (fwSceneGraph::IsSceneGraphLocked()) // we can't create objects in certain circumstances (e.g. during a blocking streaming call)
	{
		CNetwork::GetMessageLog().WriteDataValue("FAIL", "Object cannot be created (the scene graph is locked)");
		createNetworkObject = false;
		ackCode = ACKCODE_FAIL;
	}

	// reject clone creates for non-player objects if we haven't started the match yet -
    // all vehicles are forcibly removed in CNetwork::StartMatch() so we don't want any
    // clone vehicles until after this has been done
    if(!CNetwork::HasMatchStarted() && (objectType != NET_OBJ_TYPE_PLAYER))
    {
        CNetwork::GetMessageLog().WriteDataValue("FAIL", "The match has not started!");
		createNetworkObject = false;
        ackCode = ACKCODE_MATCH_NOT_STARTED;
    }

	if (createNetworkObject)
	{
		// check whether an object with this ID already exists
		networkObject = GetNetworkObjectFromPlayer(objectID, fromPlayer);

		if (!networkObject)
		{
			networkObject = GetNetworkObject(objectID, true);

			if (networkObject)
			{
				ackCode = ACKCODE_WRONG_OWNER;
			}
		}

		// check if clone has already been created (this will happen often as the clone create message is sent
		// repeatedly until acked)
		if (networkObject)
		{
			createNetworkObject = false;

			if (networkObject->GetPlayerOwner() != &fromPlayer)
			{
				if (!networkObject->CheckPlayerHasAuthorityOverObject(fromPlayer))
				{
					CNetwork::GetMessageLog().WriteDataValue("IGNORE", "Object has been created by another machine (Rejected)");

					ackCode = ACKCODE_WRONG_OWNER;
				}
				else
				{
                    if(networkObject->IsLocalFlagSet(netObject::LOCALFLAG_BEINGREASSIGNED))
                    {
                        CNetwork::GetMessageLog().WriteDataValue("IGNORE", "Object is being reassigned (Rejected)");

					    ackCode = ACKCODE_FAIL;
                    }
                    else
                    {
					    CNetwork::GetMessageLog().WriteDataValue("CHANGE OWNER", "Object has already been created by another machine. (Accepted)");

					    UnregisterNetworkObject(networkObject, CLONE_CREATE_EXISTING_OBJECT, true, true);
					    networkObject       = 0;
					    ackCode             = ACKCODE_SUCCESS;
					    createNetworkObject = true;
                    }
				}
		   }
		   else
		   {
				CNetwork::GetMessageLog().WriteDataValue("IGNORE", "Object has already been created");

				if (objectType == networkObject->GetObjectType())
				{
					ackCode = ACKCODE_SUCCESS;
				}
				else
				{
					gnetAssertf(0, "Received a clone create for existing network object %s with a different type", networkObject->GetLogName());
					ackCode = ACKCODE_FAIL;
				}
			}
		}
	}

    if (createNetworkObject)
    {
        // check if we are allowed to create any more objects of this type
         bool allowedToCreateObject = CNetworkObjectPopulationMgr::CanRegisterRemoteObjectOfType(objectType, isScriptObject, isReservedObject);

        // player vehicles must be allowed to clone!
        if(!allowedToCreateObject)
        {
            if(isScriptObject || isReservedObject)
            {
                TryToMakeSpaceForObject(objectType);
                allowedToCreateObject = CNetworkObjectPopulationMgr::CanRegisterRemoteObjectOfType(objectType, isScriptObject, isReservedObject);
            }
            else
            {
                if(IsVehicleObjectType(objectType))
                {
                    if(IsPlayerVehicle(objectID))
                    {
                        TryToMakeSpaceForObject(objectType);
                        allowedToCreateObject = CNetworkObjectPopulationMgr::CanRegisterRemoteObjectOfType(objectType, isScriptObject, isReservedObject);
                    }
                }
            }
        }

        if (!allowedToCreateObject)
        {
            CNetwork::GetMessageLog().WriteDataValue("IGNORE", "Too many objects of this type are resident on this machine");

#if ENABLE_NETWORK_LOGGING
			if (isScriptObject)
			{
				NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "SCRIPT_ENTITY_CREATION_FAIL", NetworkUtils::GetObjectTypeName(objectType, isScriptObject));
				CTheScripts::GetScriptHandlerMgr().LogCurrentReservations(NetworkInterface::GetObjectManager().GetLog());
				CDispatchManager::GetInstance().LogNetworkReservations(NetworkInterface::GetObjectManager().GetLog());
			}
#endif

            ackCode = ACKCODE_TOO_MANY_OBJECTS;
            createNetworkObject = false;
        }
    }

    if (createNetworkObject)
    {
        // create and initialise a new network object using the contents of the message buffer
        switch(objectType)
        {
        case NET_OBJ_TYPE_PLAYER:
            {
				if (gnetVerifyf(CNetObjPlayer::GetPool()->GetNoOfFreeSpaces() > 0, "Ran out of CNetObjPlayers"))
				{
					networkObject = CPed::StaticCreatePlayerNetworkObject(objectID,
																		  fromPlayer.GetPhysicalPlayerIndex(),
																		  objectFlags,
																		  MAX_NUM_PHYSICAL_PLAYERS);
				}
            }
            break;
        case NET_OBJ_TYPE_PED:
            {
				if (gnetVerifyf(CNetObjPed::GetPool()->GetNoOfFreeSpaces() > 0, "Ran out of CNetObjPeds"))
				{
				   networkObject = CPed::StaticCreateNetworkObject(objectID,
																	fromPlayer.GetPhysicalPlayerIndex(),
																	objectFlags,
																	MAX_NUM_PHYSICAL_PLAYERS);
				}
            }
            break;
        case NET_OBJ_TYPE_AUTOMOBILE:
            {
				if (gnetVerifyf(CNetObjAutomobile::GetPool()->GetNoOfFreeSpaces() > 0, "Ran out of CNetObjAutomobiles"))
				{
					networkObject = CAutomobile::StaticCreateNetworkObject(objectID,
																		   fromPlayer.GetPhysicalPlayerIndex(),
																		   objectFlags,
																		   MAX_NUM_PHYSICAL_PLAYERS);
				}
            }
            break;
        case NET_OBJ_TYPE_OBJECT:
            {
				if (gnetVerifyf(CNetObjObject::GetPool()->GetNoOfFreeSpaces() > 0, "Ran out of CNetObjObjects"))
				{
				   networkObject = CObject::StaticCreateNetworkObject(objectID,
																	   fromPlayer.GetPhysicalPlayerIndex(),
																	   objectFlags,
																	   MAX_NUM_PHYSICAL_PLAYERS);
				}
            }
            break;
        case NET_OBJ_TYPE_PICKUP:
            {
				if (gnetVerifyf(CNetObjPickup::GetPool()->GetNoOfFreeSpaces() > 0, "Ran out of CNetObjPickups"))
				{
					networkObject = CPickup::StaticCreateNetworkObject(objectID,
																	   fromPlayer.GetPhysicalPlayerIndex(),
																	   objectFlags,
																	   MAX_NUM_PHYSICAL_PLAYERS);
				}
            }
            break;
        case NET_OBJ_TYPE_PICKUP_PLACEMENT:
            {
				if (gnetVerifyf(CNetObjPickupPlacement::GetPool()->GetNoOfFreeSpaces() > 0, "Ran out of CNetObjPickupPlacements"))
				{
				   networkObject = CPickupPlacement::StaticCreateNetworkObject(objectID,
																				fromPlayer.GetPhysicalPlayerIndex(),
																				objectFlags,
																				MAX_NUM_PHYSICAL_PLAYERS);
				}
            }
            break;
        case NET_OBJ_TYPE_BIKE:
            {
				if (gnetVerifyf(CNetObjBike::GetPool()->GetNoOfFreeSpaces() > 0, "Ran out of CNetObjBikes"))
				{
				   networkObject = CBike::StaticCreateNetworkObject(objectID,
																	 fromPlayer.GetPhysicalPlayerIndex(),
																	 objectFlags,
																	 MAX_NUM_PHYSICAL_PLAYERS);
				}
            }
            break;
        case NET_OBJ_TYPE_TRAILER:
            {
				if (gnetVerifyf(CNetObjTrailer::GetPool()->GetNoOfFreeSpaces() > 0, "Ran out of CNetObjTrailers"))
				{
				   networkObject = CTrailer::StaticCreateNetworkObject(objectID,
																		fromPlayer.GetPhysicalPlayerIndex(),
																		objectFlags,
																		MAX_NUM_PHYSICAL_PLAYERS);
				}
            }
            break;
        case NET_OBJ_TYPE_TRAIN:
            {
				if (gnetVerifyf(CNetObjTrain::GetPool()->GetNoOfFreeSpaces() > 0, "Ran out of CNetObjTrains"))
				{
				   networkObject = CTrain::StaticCreateNetworkObject(objectID,
																	  fromPlayer.GetPhysicalPlayerIndex(),
																	  objectFlags,
																	  MAX_NUM_PHYSICAL_PLAYERS);
				}
            }
            break;
        case NET_OBJ_TYPE_HELI:
            {
				if (gnetVerifyf(CNetObjHeli::GetPool()->GetNoOfFreeSpaces() > 0, "Ran out of CNetObjHelis"))
				{
				   networkObject = CHeli::StaticCreateNetworkObject(objectID,
																	 fromPlayer.GetPhysicalPlayerIndex(),
																	 objectFlags,
																	 MAX_NUM_PHYSICAL_PLAYERS);
				}
            }
            break;
        case NET_OBJ_TYPE_BOAT:
            {
				if (gnetVerifyf(CNetObjBoat::GetPool()->GetNoOfFreeSpaces() > 0, "Ran out of CNetObjBoats"))
				{
				   networkObject = CBoat::StaticCreateNetworkObject(objectID,
																	 fromPlayer.GetPhysicalPlayerIndex(),
																	 objectFlags,
																	 MAX_NUM_PHYSICAL_PLAYERS);
				}
            }
            break;
        case NET_OBJ_TYPE_PLANE:
            {
				if (gnetVerifyf(CNetObjPlane::GetPool()->GetNoOfFreeSpaces() > 0, "Ran out of CNetObjPlanes"))
				{
				   networkObject = CPlane::StaticCreateNetworkObject(objectID,
																	  fromPlayer.GetPhysicalPlayerIndex(),
																	  objectFlags,
																	  MAX_NUM_PHYSICAL_PLAYERS);
				}
            }
            break;
		case NET_OBJ_TYPE_SUBMARINE:
			{
				if (gnetVerifyf(CNetObjSubmarine::GetPool()->GetNoOfFreeSpaces() > 0, "Ran out of CNetObjSubmarines"))
				{
					networkObject = CSubmarine::StaticCreateNetworkObject(objectID,
																		  fromPlayer.GetPhysicalPlayerIndex(),
																		  objectFlags,
																		  MAX_NUM_PHYSICAL_PLAYERS);
				}
			}
			break;

		case NET_OBJ_TYPE_DOOR:
			{
				if (gnetVerifyf(CNetObjDoor::GetPool()->GetNoOfFreeSpaces() > 0, "Ran out of CNetObjDoor"))
				{
					networkObject = CDoor::StaticCreateNetworkObject(	objectID,
																		fromPlayer.GetPhysicalPlayerIndex(),
																		objectFlags,
																		MAX_NUM_PHYSICAL_PLAYERS);
				}
			}
			break;
#if GLASS_PANE_SYNCING_ACTIVE
		case NET_OBJ_TYPE_GLASS_PANE:
			{
				if (gnetVerifyf(CNetObjGlassPane::GetPool()->GetNoOfFreeSpaces() > 0, "Ran out of CNetObjGlassPane"))
				{
					networkObject = CGlassPane::StaticCreateNetworkObject(	objectID,
																		fromPlayer.GetPhysicalPlayerIndex(),
																		objectFlags,
																		MAX_NUM_PHYSICAL_PLAYERS);
				}
			}
			break;
#endif /* GLASS_PANE_SYNCING_ACTIVE */			
        default:
            {
                AssertMsg(0, "Unexpected network object type!");
            }
            break;
        }

        if (networkObject)
        {
            if (networkObject->GetSyncTree()->CanApplyNodeData(networkObject))
            {
                networkObject->PreSync();

                BANK_ONLY(sysTimer timer);

                networkObject->GetSyncTree()->ApplyNodeData(networkObject);

                BANK_ONLY(NetworkDebug::AddCloneCreateDuration(timer.GetTime()));

                RegisterNetworkObject(networkObject);

                if(networkObject->GetNetBlender())
                {
                    // need to do this so that the blender data is updated properly in the following PostSync()
                    networkObject->GetNetBlender()->SetLastSyncMessageTime(timestamp);
                }

                networkObject->PostSync();

                if(networkObject->GetNetBlender())
                {
                    // need to do this so that the blender data is updated properly in the following Sync()
                    networkObject->GetNetBlender()->GoStraightToTarget();
                    networkObject->GetNetBlender()->Reset();
                }

                networkObject->PostCreate();

				CheckForInvalidObject(fromPlayer, networkObject);

#if ENABLE_NETWORK_LOGGING            
                networkObject->LogAdditionalData(CNetwork::GetMessageLog());
#endif

                ackCode = ACKCODE_SUCCESS;

                // clones can't load collision
                if(networkObject->GetEntity() && networkObject->GetEntity()->GetIsDynamic())
                {
                    CDynamicEntity *dynamicEntity = static_cast<CDynamicEntity *>(networkObject->GetEntity());
                    CPortalTracker *pPortalTracker = dynamicEntity ? dynamicEntity->GetPortalTracker() : 0;

                    if(pPortalTracker)
                    {
                        pPortalTracker->SetLoadsCollisions(false);
                    }
                }

				CEntity* pEntity = networkObject->GetEntity();

                if (pEntity && (pEntity->GetIsTypePed() || pEntity->GetIsTypeVehicle()))
                {
					float radius = pEntity->GetBoundRadius();
					Vector3 centre;

					pEntity->GetBoundCentre(centre);

					// fade in the entity if on screen
					if (camInterface::IsSphereVisibleInGameViewport(centre, radius))
					{
						networkObject->GetEntity()->GetLodData().SetResetDisabled(false);
						networkObject->GetEntity()->GetLodData().SetResetAlpha(true);
					}
				}

#if ENABLE_NETWORK_LOGGING
                CheckNetworkObjectSpawnDistance(networkObject);
#endif // ENABLE_NETWORK_LOGGING

                if(isObjectTypeCapped)
                {
					if (m_NumCappedCloneCreates == 0)
					{
						m_FrameCloneCreateCapReference = fwTimer::GetFrameCount();
					}
                    m_NumCappedCloneCreates++;
                }
            }
            else
            {
                CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply node data");

                delete networkObject;
                ackCode = ACKCODE_FAIL;
            }
        }
		else
		{
			CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't create network object");
			ackCode = ACKCODE_FAIL;
		}
    }

	if( (bCapped || ackCode == ACKCODE_TOO_MANY_OBJECTS ) && objectType == NET_OBJ_TYPE_PED) 
	{
		if(!CNetObjPlayer::IsInPostTutorialSessionChangeHoldOffPedGenTime())
		{
			CNetObjPlayer::SetHoldOffPedGenTime();
		}
	}


    gnetAssert(ackCode != ACKCODE_NONE);

    GetMessageHandler().AddCreateAck(fromPlayer, toPlayer, objectID, ackCode);
}

void CNetworkObjectMgr::ProcessCloneCreateAckData(const netPlayer &fromPlayer, const netPlayer &toPlayer, ObjectId objectID, NetObjectAckCode ackCode)
{
    char logName[netObject::MAX_LOG_NAME];
    GetLogName(logName, sizeof(logName), objectID);
    CNetwork::GetMessageLog().Log("\tRECEIVED_CLONE_CREATE_ACK\t%s\r\n", logName);

	// an ack has arrived for a sync message, tell the object about this ack
    netObject *networkObject = GetNetworkObjectFromPlayer(objectID, toPlayer);

    // log the received clone create ACK information
    if (!networkObject)
        networkObject = GetNetworkObject(objectID, true);

    CNetwork::GetMessageLog().WriteDataValue("Ack code", "%s", GetAckCodeName(ackCode));

    if (!networkObject)
    {
        CNetwork::GetMessageLog().WriteDataValue("IGNORE", "Object does not exist \"%u\"", objectID);
        return;
    }
    else if (networkObject->IsClone())
    {
        CNetwork::GetMessageLog().WriteDataValue("IGNORE", "Object not controlled by this machine");
        return;
    }
    else if (!networkObject->IsPendingCloning(fromPlayer))
    {
        if (networkObject->HasBeenCloned(fromPlayer))
        {
            CNetwork::GetMessageLog().WriteDataValue("IGNORE", "Object has already been cloned on this machine");
            return;
        }
        else
        {
            CNetwork::GetMessageLog().WriteDataValue("IGNORE", "Object is not pending cloning on this machine");
            return;
        }
    }

    switch (ackCode)
    {
    case ACKCODE_SUCCESS :
        {
            // flag this object as being successfully cloned
            networkObject->SetPendingCloning(fromPlayer, false);
            networkObject->ResetCloningTimer(fromPlayer);

            // if an object is unregistering we need to send a remove now
            if (networkObject->IsLocalFlagSet(netObject::LOCALFLAG_UNREGISTERING))
            {
                RemoveClone(networkObject, fromPlayer, true);
            }
            else
            {
                networkObject->SetBeenCloned(fromPlayer, true);
            }
        }
        break;

    case ACKCODE_WRONG_OWNER :
        {
            // if this happens repeatedly then something has gone wrong
            CNetwork::GetMessageLog().WriteDataValue("FAIL", "Creating clone %s on %s failed as player thinks object is owned by another player\r\n", networkObject->GetLogName(), fromPlayer.GetLogName());

            networkObject->SetPendingCloning(fromPlayer, false);
            networkObject->ResetCloningTimer(fromPlayer);

            if(networkObject->IsLocalFlagSet(netObject::LOCALFLAG_UNREGISTERING))
            {
                CheckForUnregistrationProcessComplete(networkObject);
            }
        }
        break;

        // currently the too many objects ACK code is handled the same way as a general fail,
        // more complicated logic to follow...
    case ACKCODE_TOO_MANY_OBJECTS:
    case ACKCODE_FAIL :
        {
            CNetwork::GetMessageLog().WriteDataValue("FAIL", "Clone could not be created\r\n");

            if(ackCode == ACKCODE_TOO_MANY_OBJECTS)
            {
                TooManyObjectsCreatedACKReceived(fromPlayer, networkObject->GetObjectType());
            }

            networkObject->SetPendingCloning(fromPlayer, false);
            networkObject->ResetCloningTimer(fromPlayer);

            if(networkObject->IsLocalFlagSet(netObject::LOCALFLAG_UNREGISTERING))
            {
                CheckForUnregistrationProcessComplete(networkObject);
            }
        }
        break;
    case ACKCODE_MATCH_NOT_STARTED:
        {
            // if this happens repeatedly then something has gone wrong
            CNetwork::GetMessageLog().WriteDataValue("FAIL", "Creating clone %s on %s failed as player has not started the match yet\r\n", networkObject->GetLogName(), fromPlayer.GetLogName());

            networkObject->SetPendingCloning(fromPlayer, false);
            networkObject->ResetCloningTimer(fromPlayer);

            if(networkObject->IsLocalFlagSet(netObject::LOCALFLAG_UNREGISTERING))
            {
                CheckForUnregistrationProcessComplete(networkObject);
            }
        }
        break;
    default:
        gnetAssertf(0, "Unknown ack code received in clone create message");
    }
}

// Name         :   ProcessCloneSync
// Purpose      :   Processes a clone sync that has arrived in a clone sync message
// Parameters   :   playerIndex - the player owning the object generating this clone sync
//                  objectType - the type of the object generating this clone sync
//                  objectId - the id of the object generating this clone sync
//                  msgBuffer - the buffer containing the sync data
//                  seqNum - the sequence number of the message this sync arrived in
// Returns      :   the ack code for this sync - whether it succeeded, failed, etc
NetObjectAckCode CNetworkObjectMgr::ProcessCloneSync(const netPlayer        &fromPlayer,
                                                     const netPlayer        &UNUSED_PARAM(toPlayer), 
                                                     const NetworkObjectType objectType,
                                                     const ObjectId          objectID,
                                                     datBitBuffer           &msgBuffer,
                                                     const netSequence       seqNum,
                                                     u32                     timeStamp)
{
    NetObjectAckCode ackCode = ACKCODE_NONE;

    // read the message data into the nodes
    LOGGING_ONLY(int oldCursorPos = msgBuffer.GetCursorPos(););
	CProjectSyncTree* tree = GetSyncTree(objectType);
	if(!tree)
	{
		CNetwork::GetMessageLog().WriteDataValue("FAIL", "No sync tree found");
		return ACKCODE_FAIL;
	}

    tree->Read(SERIALISEMODE_UPDATE, 0, msgBuffer, 0);

    netObject *pObject = GetNetworkObjectFromPlayer(objectID, fromPlayer, true);

    if(pObject == 0)
    {
        pObject = GetNetworkObject(objectID, true);
    }

	NETWORK_DEBUG_BREAK_FOR_FOCUS(BREAK_TYPE_FOCUS_READ_SYNC, *pObject);

	// log the receive clone sync information
    if (!pObject)
    {
        NetworkLogUtils::WriteMessageHeader(CNetwork::GetMessageLog(), true, seqNum, fromPlayer, "RECEIVED_CLONE_SYNC", "%s_%d", NetworkUtils::GetObjectTypeName(objectType, false), objectID);
    }
    else
    {
        NetworkLogUtils::WriteMessageHeader(CNetwork::GetMessageLog(), true, seqNum, fromPlayer, "RECEIVED_CLONE_SYNC", "%s", pObject->GetLogName());
    }

    CNetwork::GetMessageLog().WriteDataValue("Timestamp", "%d", timeStamp);

#if __BANK
    if(pObject && ShouldAddLatencySample(*pObject, fromPlayer))
    {
        NetworkDebug::AddInboundLatencySample(NetworkInterface::GetNetworkTime() - timeStamp);
    }
#endif // __BANK

	// reject clone syncs from players that are not in our roaming bubble
	if (!NetworkInterface::GetLocalPlayer()->IsInSameRoamingBubble(static_cast<const CNetGamePlayer&>(fromPlayer)))
	{
		CNetwork::GetMessageLog().WriteDataValue("FAIL", "Player is not in our roaming bubble");
		return ACKCODE_FAIL;
	}

#if ENABLE_NETWORK_LOGGING
    // this is required for logging output
    msgBuffer.SetCursorPos(oldCursorPos);
    GetSyncTree(objectType)->Read(SERIALISEMODE_VERIFY, 0, msgBuffer, &CNetwork::GetMessageLog());
#endif

    if (NetworkDebug::ShouldSkipAllSyncs())
    {
        CNetwork::GetMessageLog().WriteDataValue("IGNORE", "Debug skipping sync");
        ackCode = ACKCODE_OUT_OF_SEQUENCE;
    }
	else if (!CNetwork::IsNetworkOpen())
	{
		CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data - network closed");
		ackCode = ACKCODE_MATCH_NOT_STARTED;
	}
    else if(!pObject)
    {
		CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data - no network object");
        ackCode = ACKCODE_NO_OBJECT;
    }
    else
    {
        bool wasLocal = !pObject->IsClone();

        if (pObject->IsLocalFlagSet(netObject::LOCALFLAG_BEINGREASSIGNED))
        {
            CNetwork::GetMessageLog().WriteDataValue("IGNORE", "Object is being reassigned");
            ackCode = ACKCODE_WRONG_OWNER;
        }
        else if (!pObject->CheckPlayerHasAuthorityOverObject(fromPlayer))
        {
            CNetwork::GetMessageLog().WriteDataValue("IGNORE", "Wrong owner");
            ackCode = ACKCODE_WRONG_OWNER;
        }
        
        // The CanApplyNodeData functions is only intended for use when creating objects.
        // This is to avoid failing sync messages which will increase bandwidth and also gives
        // the potential for updates to never be processed if a node is failing while waiting for
        // data from the update being rejected.
	    if (!pObject->HasGameObject() && !pObject->CanSyncWithNoGameObject())
	    {
		    CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data - no game object");
		    ackCode = ACKCODE_CANT_APPLY_DATA;
	    }
		else if (!pObject->GetSyncTree()->CanApplyNodeData(pObject))
		{
			CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data");
			ackCode = ACKCODE_CANT_APPLY_DATA;
		}

		if (ackCode == ACKCODE_NONE)
        {
            if(pObject->IsLocalFlagSet(netObject::LOCALFLAG_UNREGISTERING))
            {
                ForceRemovalOfUnregisteringObject(pObject);
                ackCode = ACKCODE_NO_OBJECT;
            }
            else
            {
                gnetAssert(pObject->IsClone());

                // update the network blender last update timestamp
                if(pObject->GetNetBlender())
                {
                    pObject->GetNetBlender()->SetLastSyncMessageTime(timeStamp);

                    // if this was a local object that has become a clone we need to call
                    // OnOwnerChange again when the blender has a valid last sync message time
                    if(wasLocal)
                    {
                        pObject->GetNetBlender()->OnOwnerChange();
                    }
                }

                pObject->PreSync();

                // apply the data held in the sync tree to the object
                pObject->GetSyncTree()->ApplyNodeData(pObject);

			    pObject->PostSync();

#if ENABLE_NETWORK_LOGGING
                // log any additional data based on the update received
                if(pObject)
                {
                    pObject->LogAdditionalData(CNetwork::GetMessageLog());
                }
#endif
            }
        }
    }

    return ackCode;
}

void CNetworkObjectMgr::ProcessCloneSyncAckData(const netPlayer &fromPlayer, const netPlayer &toPlayer, netSequence UNUSED_PARAM(syncSequence), ObjectId objectID, NetObjectAckCode ackCode)
{
	netObject *networkObject = GetNetworkObjectFromPlayer(objectID, toPlayer);

    if (networkObject)
    {
        if (ackCode == ACKCODE_FAIL)
        {
            CNetwork::GetMessageLog().WriteDataValue(networkObject->GetLogName(), "sync failed");
        }
        else
        {
            CNetwork::GetMessageLog().WriteDataValue(networkObject->GetLogName(), "could not sync (ack code = %s)", GetAckCodeName(ackCode));

            if (ackCode == ACKCODE_NO_OBJECT)
            {
                // we need to do this check to avoid an assert in netObjVehicle when it checks that the clone state of a driver is the same as
                // the vehicle. We can get the situation where the other player gets a give control event then removes the driver and we send an update
                // and get this response, but we have yet to get the give control ack and when we send the event again the assert fires.
                if (!networkObject->IsPendingOwnerChange())
                {
                    networkObject->SetBeenCloned(fromPlayer, false);
                }
            }
        }
    }
    else
    {
        CNetwork::GetMessageLog().WriteDataValue("Unknown object", "%d - ack code %s", objectID, GetAckCodeName(ackCode));
    }
}

void CNetworkObjectMgr::ProcessCloneRemoveData(const netPlayer &fromPlayer, const netPlayer &toPlayer, ObjectId objectID, u32 ownershipToken)
{
	char logName[netObject::MAX_LOG_NAME];
	GetLogName(logName, sizeof(logName), objectID);
	CNetwork::GetMessageLog().Log("\tRECEIVED_CLONE_REMOVE\t%s\r\n", logName);
	CNetwork::GetMessageLog().LineBreak();

	NetObjectAckCode ackCode = ACKCODE_NONE;

	// reject clone removes from players that are not in our roaming bubble
	if (!NetworkInterface::GetLocalPlayer()->IsInSameRoamingBubble(static_cast<const CNetGamePlayer&>(fromPlayer)))
	{
		CNetwork::GetMessageLog().WriteDataValue("FAIL", "Player is not in our roaming bubble");
		ackCode = ACKCODE_FAIL;
	}
	else
	{
		netObject *networkObject = GetNetworkObjectFromPlayer(objectID, fromPlayer);

		// object may have been already removed (this will happen often as the clone remove message is sent
		// repeatedly until acked)
		if (networkObject)
		{
            bool isObjectTypeCapped = UseCloneRemoveCapping(networkObject->GetObjectType());

            if(isObjectTypeCapped && m_NumCappedCloneRemoves >= DEFAULT_CAPPED_CLONE_REMOVE_LIMIT)
            {
                CNetwork::GetMessageLog().WriteDataValue("FAIL", "The limit for clone removes for capped objects types has been reached for this frame!");
		        ackCode = ACKCODE_FAIL;
            }
			else if (networkObject->IsLocalFlagSet(netObject::LOCALFLAG_BEINGREASSIGNED))
			{
				CNetwork::GetMessageLog().WriteDataValue("FAIL", "Object is being reassigned");

				ackCode = ACKCODE_FAIL;
			}
			else if (!networkObject->CanDelete()) // don't ack if we can't delete the object
			{
				CNetwork::GetMessageLog().WriteDataValue("FAIL", "Object cannot be deleted");

				ackCode = ACKCODE_FAIL;
			}
			else if(fwSceneGraph::IsSceneGraphLocked()) // we can't delete objects in certain circumstances (e.g. during a blocking streaming call)
			{
				CNetwork::GetMessageLog().WriteDataValue("FAIL", "Object cannot be deleted (scene graph)");

				ackCode = ACKCODE_FAIL;
			}
			else
			{
                BANK_ONLY(sysTimer timer);

				UnregisterNetworkObject(networkObject, CLONE_REMOVAL);

                BANK_ONLY(NetworkDebug::AddCloneRemoveDuration(timer.GetTime()));

                if(isObjectTypeCapped)
                {
                    m_NumCappedCloneRemoves++;
                }

				ackCode = ACKCODE_SUCCESS;
			}
		}
		else
		{
			networkObject = GetNetworkObject(objectID, true);

			if (networkObject)
			{
                bool isObjectTypeCapped = UseCloneRemoveCapping(networkObject->GetObjectType());

                if(isObjectTypeCapped && m_NumCappedCloneRemoves >= DEFAULT_CAPPED_CLONE_REMOVE_LIMIT)
                {
                    CNetwork::GetMessageLog().WriteDataValue("FAIL", "The limit for clone removes for capped objects types has been reached for this frame!");
		            ackCode = ACKCODE_FAIL;
                }
			    else if (networkObject->IsLocalFlagSet(netObject::LOCALFLAG_BEINGREASSIGNED))
				{
					CNetwork::GetMessageLog().WriteDataValue("FAIL", "Object is being reassigned");

					ackCode = ACKCODE_FAIL;
				}
				else if(netUtils::IsSeqGreater(ownershipToken, networkObject->GetOwnershipToken(), netObject::SIZEOF_OWNERSHIP_TOKEN))
				{
					// check if the player sending the remove has passed a token further along
					// in sequence than we currently have
					if (!networkObject->IsClone())
					{
						networkObject->ClearPendingPlayerIndex();
						CNetwork::GetObjectManager().ChangeOwner(*networkObject, fromPlayer, MIGRATE_FORCED);
					}

					if(fwSceneGraph::IsSceneGraphLocked()) // we can't delete objects in certain circumstances (e.g. during a blocking streaming call)
					{
						CNetwork::GetMessageLog().WriteDataValue("FAIL", "Object cannot be deleted (fwSceneGraph::IsSceneGraphLocked())");

						ackCode = ACKCODE_FAIL;
					}
					else if(!networkObject->CanDelete())
					{
						CNetwork::GetMessageLog().WriteDataValue("FAIL", "Object cannot be deleted");
						ackCode = ACKCODE_FAIL;
					}
					else
					{
                        BANK_ONLY(sysTimer timer);

						UnregisterNetworkObject(networkObject, CLONE_REMOVAL_WRONG_OWNER);

                        BANK_ONLY(NetworkDebug::AddCloneRemoveDuration(timer.GetTime()));

                        if(isObjectTypeCapped)
                        {
                            m_NumCappedCloneRemoves++;
                        }

						ackCode = ACKCODE_SUCCESS;
					}
				}
				else
				{
					CNetwork::GetMessageLog().WriteDataValue("FAIL", "Wrong owner");

					ackCode = ACKCODE_WRONG_OWNER;
				}
			}
			else
			{
				CNetwork::GetMessageLog().Log("\tRECEIVED_CLONE_REMOVE\tUNKNOWN\r\n");
				CNetwork::GetMessageLog().WriteDataValue("IGNORE", "Object does not exist \"%d\"", objectID);

				ackCode = ACKCODE_NO_OBJECT;
			}
		}
	}

    gnetAssert(ackCode != ACKCODE_NONE);

    GetMessageHandler().AddRemoveAck(fromPlayer, toPlayer, objectID, ackCode);
}

void CNetworkObjectMgr::ProcessCloneRemoveAckData(const netPlayer &fromPlayer, const netPlayer &toPlayer, ObjectId objectID, NetObjectAckCode ackCode)
{
    char logName[netObject::MAX_LOG_NAME];
    GetLogName(logName, sizeof(logName), objectID);
    CNetwork::GetMessageLog().Log("\tRECEIVED_CLONE_REMOVE_ACK\t%s\r\n", logName);
    CNetwork::GetMessageLog().LineBreak();

    netObject *networkObject = GetNetworkObjectFromPlayer(objectID, toPlayer, true);

    if (!networkObject)
    {
        CNetwork::GetMessageLog().WriteDataValue("IGNORE", "Object does not exist \"%d\"\r\n", objectID);
        return;
    }
    else if (networkObject->IsClone())
    {
        CNetwork::GetMessageLog().WriteDataValue("IGNORE", "Object not controlled by this machine\r\n");
        return;
    }
    else if (!networkObject->IsPendingRemoval(fromPlayer))
    {
        CNetwork::GetMessageLog().WriteDataValue("IGNORE", "Removal has already been acked\r\n");
        return;
    }

    switch (ackCode)
    {
    case ACKCODE_SUCCESS :
    case ACKCODE_NO_OBJECT :
        {
            gnetAssert(!networkObject->IsPendingCloning(fromPlayer));
            gnetAssert(!networkObject->HasBeenCloned(fromPlayer));

            networkObject->SetPendingRemoval(fromPlayer, false);
            networkObject->ResetCloningTimer(fromPlayer);

            CheckForUnregistrationProcessComplete(networkObject);
        }
        break;

    case ACKCODE_FAIL :
        {
            CNetwork::GetMessageLog().WriteDataValue("FAIL", "Clone could not be removed\r\n");

            // try again...
            RemoveClone(networkObject, fromPlayer, true);
        }
        break;

    case ACKCODE_WRONG_OWNER :
        {
            // if this happens repeatedly then something has gone wrong
            CNetwork::GetMessageLog().WriteDataValue("FAIL", "Removing clone %s on %s failed as player thinks object is owned by another player\r\n", networkObject->GetLogName(), fromPlayer.GetLogName());

            // try again...
            RemoveClone(networkObject, fromPlayer, true);
        }
        break;

    default :
        {
            gnetAssertf(0, "Unknown ack code: %d received in clone remove message", ackCode);
        }
        break;
    }
}

bool CNetworkObjectMgr::UseCloneCreateCapping(NetworkObjectType objectType)
{
    if(IsVehicleObjectType(objectType))
    {
        return true;
    }
    else
    {
        switch(objectType)
        {
        case NET_OBJ_TYPE_PED:
        case NET_OBJ_TYPE_PLAYER:
            return true;
        default:
            return false;
        }
    }
}

bool CNetworkObjectMgr::UseCloneRemoveCapping(NetworkObjectType objectType)
{
    if(IsVehicleObjectType(objectType))
    {
        return true;
    }
    else
    {
        switch(objectType)
        {
        case NET_OBJ_TYPE_PED:
        case NET_OBJ_TYPE_PLAYER:
            return true;
        default:
            return false;
        }
    }
}

// returns whether this network object is important and must be cloned even if the machine is owning too many objects
bool CNetworkObjectMgr::IsImportantObject(netObject *pObject)
{
    if(pObject)
    {
        if(pObject->IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_SCRIPTOBJECT))
        {
            return true;
        }

        CVehicle *pVehicle = NetworkUtils::GetVehicleFromNetworkObject(pObject);

        if(pVehicle && pVehicle->GetDriver() && pVehicle->GetDriver()->IsLocalPlayer())
        {
            return true;
        }
    }

    return false;
}

void CNetworkObjectMgr::UpdateAllObjectsForRemotePlayers(const netPlayer &sourcePlayer)
{
    // update all of the objects owned by local player to the remote players
    unsigned numRemoteActivePlayers = netInterface::GetNumRemoteActivePlayers();

    if(numRemoteActivePlayers > 0)
    {
        // update local player and local vehicle
        CPed      *localPlayerPed     = SafeCast(const CNetGamePlayer, &sourcePlayer)->GetPlayerPed();
        netObject *localPlayer        = localPlayerPed ? localPlayerPed->GetNetworkObject() : 0;
        netObject *localPlayerVehicle = 0;

        if(localPlayer)
        {
            UpdateObjectOnAllPlayers(localPlayer);

            if(localPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
            {
                localPlayerVehicle = localPlayerPed->GetMyVehicle() ? localPlayerPed->GetMyVehicle()->GetNetworkObject() : 0;

                if(localPlayerVehicle && !localPlayerVehicle->IsClone())
                {
                    UpdateObjectOnAllPlayers(localPlayerVehicle);
                }
            }
        }

        PhysicalPlayerIndex sourcePlayerIndex = sourcePlayer.GetPhysicalPlayerIndex();

	    if (!gnetVerifyf(sourcePlayerIndex != INVALID_PLAYER_INDEX, "Trying to update objects for local player with an invalid physical player index!"))
	    {
		    return;
	    }

        // separate all other network objects into prioritised lists
        unsigned numStarvingObjects                   = 0;
        unsigned numMissionObjects                    = 0;
        unsigned numAmbientObjectsInCurrentBatch      = 0;
        unsigned numAmbientObjectsOutsideCurrentBatch = 0;

        netObject *starvingObjects                  [netObjectIDMgr::MAX_TOTAL_OBJECT_IDS];
        netObject *missionObjects                   [netObjectIDMgr::MAX_TOTAL_OBJECT_IDS];
        netObject *ambientObjectsInCurrentBatch     [netObjectIDMgr::MAX_TOTAL_OBJECT_IDS];
        netObject *ambientObjectsOutsideCurrentBatch[netObjectIDMgr::MAX_TOTAL_OBJECT_IDS];

        unsigned totalUpdatesRequired[MAX_NUM_PHYSICAL_PLAYERS];
        memset(totalUpdatesRequired, 0, sizeof(totalUpdatesRequired));

        atDNode<netObject*, datBase> *node = m_aPlayerObjectList[sourcePlayerIndex].GetHead();

        while(node)
        {
            netObject *networkObject = node->Data;
            node = node->GetNext();

            if(networkObject && (networkObject != localPlayer) && (networkObject != localPlayerVehicle))
            {
                PlayerFlags updatePlayers = networkObject->GetSyncData() ? networkObject->GetSyncData()->GetUpdatePlayers() : 0;

                for(unsigned index = 0; index < MAX_NUM_PHYSICAL_PLAYERS; index++)
                {
                    if((updatePlayers & (1<<index)) != 0)
                    {
                        totalUpdatesRequired[index]++;
                    }
                }

                if(networkObject->IsStarving())
                {
                    if(numStarvingObjects < netObjectIDMgr::MAX_TOTAL_OBJECT_IDS)
                    {
                        starvingObjects[numStarvingObjects] = networkObject;
                        numStarvingObjects++;
                    }
                }
                else if(networkObject->IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_SCRIPTOBJECT))
                {
                    if(numMissionObjects < netObjectIDMgr::MAX_TOTAL_OBJECT_IDS)
                    {
                        missionObjects[numMissionObjects] = networkObject;
                        numMissionObjects++;
                    }
                }
                else
                {
                    if(networkObject->IsInCurrentUpdateBatch())
                    {
                        if(numAmbientObjectsInCurrentBatch < netObjectIDMgr::MAX_TOTAL_OBJECT_IDS)
                        {
                            ambientObjectsInCurrentBatch[numAmbientObjectsInCurrentBatch] = networkObject;
                            numAmbientObjectsInCurrentBatch++;
                        }
                    }
                    else
                    {
                        if(numAmbientObjectsOutsideCurrentBatch < netObjectIDMgr::MAX_TOTAL_OBJECT_IDS)
                        {
                            ambientObjectsOutsideCurrentBatch[numAmbientObjectsOutsideCurrentBatch] = networkObject;
                            numAmbientObjectsOutsideCurrentBatch++;
                        }
                    }
                }
            }
        }

        // check if we need to batch the updates by player (done when the game is sending too many updates)
        TUNE_INT(MIN_BATCH_SIZE,  4,  1, 16, 1);
        TUNE_INT(BATCH_THRESHOLD, 20, 1, 100, 1);

        unsigned remotePhysicalPlayersBitmask = NetworkInterface::GetPlayerMgr().GetRemotePhysicalPlayersBitmask();

        unsigned playersToUpdate   = 0;
        unsigned updateCount       = GetNumUpdatesSent();
        unsigned numPlayers        = 0;
        unsigned playerToUpdate    = m_PlayerBatchStart;
        for(unsigned index = 0; index < MAX_NUM_PHYSICAL_PLAYERS; index++)
        {
            if((remotePhysicalPlayersBitmask & (1<<playerToUpdate)) != 0)
            {
                numPlayers++;

                playersToUpdate |= (1<<playerToUpdate);
                updateCount += totalUpdatesRequired[playerToUpdate];

                if(updateCount > BATCH_THRESHOLD && numPlayers >= MIN_BATCH_SIZE)
                {
                    break;
                }
            }

            playerToUpdate++;

            if(playerToUpdate >= MAX_NUM_PHYSICAL_PLAYERS)
            {
                playerToUpdate = 0;
            }
        }

        SetPlayersToUpdate(playersToUpdate);

        // update the objects in priority order
        // note that an object starving on any machine will be updated first for all machines,
        // this is done to simplify code for performance reasons
        for(unsigned index = 0; index < numStarvingObjects; index++)
        {
            netObject *networkObject = starvingObjects[index];
            UpdateObjectOnAllPlayers(networkObject);
        }

        for(unsigned index = 0; index < numMissionObjects; index++)
        {
            netObject *networkObject = missionObjects[index];
            UpdateObjectOnAllPlayers(networkObject);
        }

        for(unsigned index = 0; index < numAmbientObjectsInCurrentBatch; index++)
        {
            netObject *networkObject = ambientObjectsInCurrentBatch[index];
            UpdateObjectOnAllPlayers(networkObject);
        }

        for(unsigned index = 0; index < numAmbientObjectsOutsideCurrentBatch; index++)
        {
            netObject *networkObject = ambientObjectsOutsideCurrentBatch[index];
            UpdateObjectOnAllPlayers(networkObject);
        }

        BANK_ONLY(m_UsingBatchedUpdates = false);

        // if we are not going to update all players move the batch start along. Note
        // that we don't reset the batch start when all players have been updated so
        // when we next switch to batched updates we carry on from where we left off
        if(playersToUpdate != remotePhysicalPlayersBitmask)
        {
            BANK_ONLY(m_UsingBatchedUpdates = true);
            m_PlayerBatchStart = playerToUpdate;
        }
    }
}

//
// Name         :   CompareEntityDistanceToPosition
// Purpose      :   qsort comparison function for sorting entities relative to how close they are to other players
static int CompareEntityDistanceToPosition(const void *paramA, const void *paramB)
{
    const CEntity *entityA = *((CEntity **)(paramA));
    const CEntity *entityB = *((CEntity **)(paramB));

    gnetAssert(entityA);
    gnetAssert(entityB);

    if(entityA && entityB)
    {
        CNetObjEntity *networkObjectA = static_cast<CNetObjEntity *>(NetworkUtils::GetNetworkObjectFromEntity(entityA));
        CNetObjEntity *networkObjectB = static_cast<CNetObjEntity *>(NetworkUtils::GetNetworkObjectFromEntity(entityB));

        gnetAssert(networkObjectA);
        gnetAssert(networkObjectB);

        if(networkObjectA && networkObjectB)
        {
            float objectADist = networkObjectA->GetDistanceToNearestPlayerSquared();
            float objectBDist = networkObjectB->GetDistanceToNearestPlayerSquared();

            if(objectADist > objectBDist)
            {
                return -1;
            }
            else if(objectADist < objectBDist)
            {
                return 1;
            }
        }
    }

    return 0;
}

static Vector3 positionToSortBy(VEC3_ZERO);

//
// name:        BuildObjectDeletionLists
// description: build a list of objects suitable for removal ordered by the impact their deletion will have on the game
//
void CNetworkObjectMgr::BuildObjectDeletionLists(CObject   **objectRemovalList,   u32 objectListCapacity,   u32 &numObjectsReturned,
                                                 CPed      **pedRemovalList,      u32 pedListCapacity,      u32 &numPedsReturned,
                                                 CVehicle  **vehicleRemovalList,  u32 vehicleListCapacity,  u32 &numVehiclesReturned)
{
    gnetAssert(objectRemovalList);
    gnetAssert(pedRemovalList);
    gnetAssert(vehicleRemovalList);

    numObjectsReturned  = 0;
    numPedsReturned     = 0;
    numVehiclesReturned = 0;

    if(FindPlayerPed())
    {
        const atDList<netObject*, datBase> *listsToProcess[MAX_NUM_PHYSICAL_PLAYERS];
        unsigned numListsToProcess = GetLocalObjectLists(listsToProcess);

        for(unsigned index = 0; index < numListsToProcess; index++)
        {
            const atDNode<netObject*, datBase> *node = listsToProcess[index] ? listsToProcess[index]->GetHead() : 0;

            while (node)
            {
                netObject *networkObject = node->Data;

                if(networkObject)
                {
                    CObject   *object   = NetworkUtils::GetCObjectFromNetworkObject(networkObject);
                    CPed      *ped      = NetworkUtils::GetPedFromNetworkObject(networkObject);
                    CVehicle  *vehicle  = NetworkUtils::GetVehicleFromNetworkObject(networkObject);


                    if(object && object->CanBeDeleted())
                    {
                        // we only want to consider pickups and objects for removal - other types such as doors need leaving alone
                        if(networkObject->GetObjectType() == NET_OBJ_TYPE_OBJECT || networkObject->GetObjectType() == NET_OBJ_TYPE_PICKUP)
                        {
                            if(numObjectsReturned < objectListCapacity)
                            {
                                objectRemovalList[numObjectsReturned] = object;
                                numObjectsReturned++;
                            }
                            else
                            {
                                bool bAdded = false;

                                for(u32 newIndex = 0; newIndex < numObjectsReturned && !bAdded; newIndex++)
                                {
                                    CObject       *pNewObject      = object;
                                    CNetObjEntity *pNewEntity      = static_cast<CNetObjEntity *>(pNewObject->GetNetworkObject());
                                    CNetObjEntity *pPreviousEntity = static_cast<CNetObjEntity *>(objectRemovalList[newIndex]->GetNetworkObject());

                                    float newDistToPlayer  = pNewEntity->GetDistanceToNearestPlayerSquared();
                                    float prevDistToPlayer = pPreviousEntity->GetDistanceToNearestPlayerSquared();

                                    if(newDistToPlayer > prevDistToPlayer)
                                    {
                                        objectRemovalList[newIndex] = pNewObject;
                                        bAdded = true;
                                    }
                                }
                            }
                        }
                    }
                    else if(ped && ped->CanBeDeleted() && (ped->GetPedType() != PEDTYPE_COP))
                    {
                        if(numPedsReturned < pedListCapacity)
                        {
                            pedRemovalList[numPedsReturned] = ped;
                            numPedsReturned++;
                        }
                        else
                        {
                            bool bAdded = false;

                            for(u32 newIndex = 0; newIndex < numPedsReturned && !bAdded; newIndex++)
                            {
                                CPed          *pNewPed         = ped;
                                CNetObjEntity *pNewEntity      = static_cast<CNetObjEntity *>(pNewPed->GetNetworkObject());
                                CNetObjEntity *pPreviousEntity = static_cast<CNetObjEntity *>(pedRemovalList[newIndex]->GetNetworkObject());

                                float newDistToPlayer  = pNewEntity->GetDistanceToNearestPlayerSquared();
                                float prevDistToPlayer = pPreviousEntity->GetDistanceToNearestPlayerSquared();

                                if(newDistToPlayer > prevDistToPlayer)
                                {
                                    pedRemovalList[newIndex] = pNewPed;
                                    bAdded = true;
                                }
                            }
                        }
                    }
                    else if(vehicle && vehicle->CanBeDeleted() && !vehicle->InheritsFromTrailer())
                    {
                        if(numVehiclesReturned < vehicleListCapacity)
                        {
                            vehicleRemovalList[numVehiclesReturned] = vehicle;
                            numVehiclesReturned++;
                        }
                        else
                        {
                            bool bAdded = false;

                            for(u32 newIndex = 0; newIndex < numVehiclesReturned && !bAdded; newIndex++)
                            {
                                CVehicle      *pNewVehicle     = vehicle;
                                CNetObjEntity *pNewEntity      = static_cast<CNetObjEntity *>(pNewVehicle->GetNetworkObject());
                                CNetObjEntity *pPreviousEntity = static_cast<CNetObjEntity *>(vehicleRemovalList[newIndex]->GetNetworkObject());

                                float newDistToPlayer  = pNewEntity->GetDistanceToNearestPlayerSquared();
                                float prevDistToPlayer = pPreviousEntity->GetDistanceToNearestPlayerSquared();

                                if(newDistToPlayer > prevDistToPlayer)
                                {
                                    vehicleRemovalList[newIndex] = pNewVehicle;
                                    bAdded = true;
                                }
                            }
                        }
                    }
                }

                node = node->GetNext();
            }
        }

        // finally sort the arrays
        positionToSortBy = VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition());
        qsort(objectRemovalList,   numObjectsReturned,   sizeof(CEntity *), CompareEntityDistanceToPosition);
        qsort(pedRemovalList,      numPedsReturned,      sizeof(CEntity *), CompareEntityDistanceToPosition);
        qsort(vehicleRemovalList,  numVehiclesReturned,  sizeof(CEntity *), CompareEntityDistanceToPosition);
    }
}

static int CompareEntityDistanceToPlayerPosition(const void *paramA, const void *paramB)
{
    const CEntity *entityA = *((CEntity **)(paramA));
    const CEntity *entityB = *((CEntity **)(paramB));

    gnetAssert(entityA);
    gnetAssert(entityB);

    if(entityA && entityB)
    {
        float objectADist = (VEC3V_TO_VECTOR3(entityA->GetTransform().GetPosition()) - positionToSortBy).XYMag2();
        float objectBDist = (VEC3V_TO_VECTOR3(entityB->GetTransform().GetPosition()) - positionToSortBy).XYMag2();

        if(objectADist > objectBDist)
        {
            return -1;
        }
        else if(objectADist < objectBDist)
        {
            return 1;
        }
    }

    return 0;
}

void CNetworkObjectMgr::BuildObjectControlPassingLists(CEntity             **entityRemovalList,
                                                       u32                   entityListCapacity,
                                                       u32                  &numEntitiesReturned,
                                                       bool                  includePeds,
                                                       bool                  includeVehicles,
                                                       bool                  includeObjects,
                                                       const CNetGamePlayer &player)
{
    gnetAssert(entityRemovalList);

	bool bLocalPlayerPedAtFocus = false;
	bool bRemotePlayerPedAtFocus = false;

	Vector3 localPlayerPosition = NetworkInterface::GetLocalPlayerFocusPosition(&bLocalPlayerPedAtFocus);
    Vector3 remotePlayerPosition = NetworkInterface::GetPlayerFocusPosition(player, &bRemotePlayerPedAtFocus);

    numEntitiesReturned = 0;

    const atDList<netObject*, datBase> *listsToProcess[MAX_NUM_PHYSICAL_PLAYERS];
    unsigned numListsToProcess = GetLocalObjectLists(listsToProcess);

    for(unsigned index = 0; index < numListsToProcess; index++)
    {
        const atDNode<netObject*, datBase> *node = listsToProcess[index] ? listsToProcess[index]->GetHead() : 0;

        while (node)
        {
            netObject *networkObject = node->Data;

            if(networkObject)
            {
                CEntity *entity = networkObject->GetEntity();

                if(entity)
                {
                    bool typeValid = (entity->GetIsTypePed() && includePeds) || (entity->GetIsTypeVehicle() && includeVehicles) || includeObjects;

                    if(typeValid && networkObject->CanPassControl(player, MIGRATE_PROXIMITY))
                    {
                        CPed *ped = NetworkUtils::GetPedFromNetworkObject(networkObject);

                        bool canPassControl = !(ped && ped->GetIsInVehicle());

                        // don't pass control of network objects to players in a different interior
						if (static_cast<CNetObjDynamicEntity *>(networkObject)->IsInInterior())
						{
							if(bLocalPlayerPedAtFocus && bRemotePlayerPedAtFocus && entity->GetIsDynamic() && player.GetPlayerPed()->GetNetworkObject() && FindPlayerPed()->GetNetworkObject())
							{
								bool localPlayerInSameInterior  = static_cast<CNetObjDynamicEntity *>(networkObject)->IsInSameInterior(*FindPlayerPed()->GetNetworkObject());
								bool remotePlayerInSameInterior = static_cast<CNetObjDynamicEntity *>(networkObject)->IsInSameInterior(*player.GetPlayerPed()->GetNetworkObject());

								if(localPlayerInSameInterior && !remotePlayerInSameInterior)
								{
									canPassControl = false;
								}
							}
						}

                        if(canPassControl)
                        {
                            // don't try to pass control of entities that will immediately be passed back to use based on proximity migration
                            float distToLocalSqr  = (VEC3V_TO_VECTOR3(entity->GetTransform().GetPosition()) - localPlayerPosition).XYMag2();
                            float distToRemoteSqr = (VEC3V_TO_VECTOR3(entity->GetTransform().GetPosition()) - remotePlayerPosition).XYMag2();

                            if(distToLocalSqr  > (CNetObjProximityMigrateable::CONTROL_DIST_MAX_SQR * 1.2f) &&
                               distToRemoteSqr < rage::square(networkObject->GetScopeData().m_scopeDistance * 0.8f))
                            {
                                if(numEntitiesReturned < entityListCapacity)
                                {
                                    entityRemovalList[numEntitiesReturned] = entity;
                                    numEntitiesReturned++;
                                }
                            }
                        }
                    }
                }
            }

            node = node->GetNext();
        }
    }

    // finally sort the array
    positionToSortBy = remotePlayerPosition;
    qsort(entityRemovalList, numEntitiesReturned, sizeof(CEntity *), CompareEntityDistanceToPlayerPosition);
}

//
// name:        TryToDumpExcessObjects
// description: try to pass control or remove any ambient objects if we are struggling to support our current number of locally owned objects
//
void CNetworkObjectMgr::TryToDumpExcessObjects()
{
    // only dump excess object periodically
    u32 currentTime = fwTimer::GetSystemTimeInMilliseconds();
    u32 timeElapsed = currentTime - m_lastTimeExcessObjectCalled;

    if(timeElapsed < m_dumpExcessObjectInterval)
    {
        return;
    }

    const char *dumpReason = "Owning too many objects";

    bool   owningTooManyObjects    = IsOwningTooManyObjects();
    bool   canPassControl          = true;
    bool   canDelete               = true;
    bool   dumpPeds                = owningTooManyObjects;
    bool   dumpVehicles            = owningTooManyObjects;
    bool   dumpObjects             = owningTooManyObjects;
    bool   dumpForPretendOccupants = false;
    u32    numObjectsToDump        = m_maxObjectsToDumpPerUpdate;

    // check if we can't register any vehicles and have players in vehicles not currently
    // cloned on our machine - we need to remove some vehicles in this case
    if(!CNetworkObjectPopulationMgr::CanRegisterRemoteObjectOfType(NET_OBJ_TYPE_AUTOMOBILE, false, false))
    {
        if(GetNumClosePlayerVehiclesNotOnOurMachine() > 0)
        {
            canPassControl       = false;
            dumpVehicles         = true;
            owningTooManyObjects = true;
            numObjectsToDump     = 1;
            dumpReason           = "Make space for remote player vehicles";
        }
    }
    else
    {
        // check if we can't register any peds and have vehicles with pretend occupants - we need to remove some peds in this case
        NetworkInterface::SetProcessLocalPopulationLimits(false);
        bool canRegisterPed = CNetworkObjectPopulationMgr::CanRegisterLocalObjectOfType(NET_OBJ_TYPE_PED, false);
        NetworkInterface::SetProcessLocalPopulationLimits(true);

        if(!canRegisterPed)
        {
            u32 numVehiclesWithPretendOccupants = GetNumLocalVehiclesUsingPretendOccupants();

            if(numVehiclesWithPretendOccupants > 0)
            {
                if(!owningTooManyObjects)
                {
                    dumpPeds                = true;
                    owningTooManyObjects    = true;
                    numObjectsToDump        = 1;
                    dumpReason              = "Make space for pretend occupants";
                    dumpForPretendOccupants = true;

                    if(m_TimeToDeletePedsForPretendOccupants == 0)
                    {
                        const unsigned DELETE_FOR_PRETEND_OCCUPANTS_DELAY = 15000;

                        m_TimeToDeletePedsForPretendOccupants = sysTimer::GetSystemMsTime() + DELETE_FOR_PRETEND_OCCUPANTS_DELAY;
                    }
                }
            }
        }
    }

    // wait a while before deleting peds for vehicles with pretend occupants, we can still try to
    // pass peds on to other machines - this give a chance for the population balancing to kick in.
    // Note the timer is reset when objects are dumped for other reasons - this is intentional
    if(dumpForPretendOccupants)
    {
        if(sysTimer::GetSystemMsTime() < m_TimeToDeletePedsForPretendOccupants)
        {
            canDelete = false;
        }
        else
        {
            m_TimeToDeletePedsForPretendOccupants = 0;
        }
    }
    else
    {
        m_TimeToDeletePedsForPretendOccupants = 0;
    }

    // check if we should rebalance objects to other players
    if(!owningTooManyObjects)
    {
        const unsigned BALANCE_THRESHOLD = 3;

        canPassControl = true;
        canDelete      = false;
        dumpReason     = "Population Balancing";

        // balance vehicles first
        u32 numLocalVehicles   = (u32)CNetworkObjectPopulationMgr::GetNumLocalVehicles();
        u32 numDesiredVehicles = rage::Min(numLocalVehicles, CNetworkObjectPopulationMgr::GetLocalDesiredNumVehicles());

        if((numLocalVehicles - numDesiredVehicles) > BALANCE_THRESHOLD)
        {
            dumpVehicles         = true;
            owningTooManyObjects = true;
            numObjectsToDump     = 1;
        }
        else
        {
            // then peds
            u32 numLocalPeds   = (u32)CNetworkObjectPopulationMgr::GetNumLocalObjects(NET_OBJ_TYPE_PED);
            u32 numDesiredPeds = rage::Min(numLocalPeds, CNetworkObjectPopulationMgr::GetLocalDesiredNumPeds());

            if((numLocalPeds - numDesiredPeds) > BALANCE_THRESHOLD)
            {
                dumpPeds             = true;
                owningTooManyObjects = true;
                numObjectsToDump     = 1;
            }
            else
            {
                // finally objects
                u32 numLocalObjects   = (u32)CNetworkObjectPopulationMgr::GetNumLocalObjects(NET_OBJ_TYPE_OBJECT);
                u32 numDesiredObjects = rage::Min(numLocalObjects, CNetworkObjectPopulationMgr::GetLocalDesiredNumObjects());

                if((numLocalObjects - numDesiredObjects) > BALANCE_THRESHOLD)
                {
                    dumpObjects          = true;
                    owningTooManyObjects = true;
                    numObjectsToDump     = 1;
                }
            }
        }
    }

    m_lastTimeExcessObjectCalled = currentTime;

    if(!owningTooManyObjects)
    {
        m_TimeAllowedToDeleteExcessObjects = currentTime + m_DelayBeforeDeletingExcessObjects;
    }
    else
    {
        // first check how many objects we own are pending owner change, these can be
        // counted as being dumped until we get a reply from the player we are passing control to
        u32 numEntitiesPendingOwnerChange = GetNumLocalObjectsPendingOwnerChange();

        if(numObjectsToDump > numEntitiesPendingOwnerChange)
        {
            numObjectsToDump = numObjectsToDump - numEntitiesPendingOwnerChange;

            u32       numEntitiesDumped  = 0;
            const u32 entitiesToRetrieve = MAX(numObjectsToDump, 20);

            if(canPassControl)
            {
                // calculate the list of players that may be able to accept control of our excess objects
                u32 numPlayersCanPassControlTo = 0;
                const CNetGamePlayer *playersToPassControlTo[MAX_NUM_PHYSICAL_PLAYERS];

                unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
                const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

	            for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
                {
		            const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);

                    if(pPlayer->GetPlayerPed() && RemotePlayerCanAcceptObjects(*pPlayer, false))
                    {
                        if(pPlayer->CanAcceptMigratingObjects())
                        {
                           playersToPassControlTo[numPlayersCanPassControlTo] = pPlayer;
                           numPlayersCanPassControlTo++;
                        }
                        else
                        {
                            // don't remove objects when any of the players are in a temporary state that prevents
                            // them accepting objects (such as respawning of viewing an MP cutscene
                            canDelete = false;
                        }
                    }
                    else
                    {
                        NetworkLogUtils::WriteLogEvent(GetLog(), "FAILED_TO_PASS_CONTROL_EXCESS_OBJECTS", "");
                        GetLog().WriteDataValue("Dump reason", dumpReason);
                        GetLog().WriteDataValue("Target Player", pPlayer->GetLogName());
                        GetLog().WriteDataValue("Last Too Many Objects ACK", "%d", m_timeSinceLastTooManyObjectsACK[pPlayer->GetPhysicalPlayerIndex()]);
                    }
                }

                // try to pass control of any excess objects if we can
                if(numPlayersCanPassControlTo > 0)
                {
                    // pick a player at random to pass the entities to
                    u32                   playerToPassControlTo = fwRandom::GetRandomNumberInRange(0, numPlayersCanPassControlTo);
                    const CNetGamePlayer *player                = playersToPassControlTo[playerToPassControlTo];

                    u32 numEntities = 0;
                    CEntity *entitiesControlPassList[netObjectIDMgr::MAX_TOTAL_OBJECT_IDS];

                    BuildObjectControlPassingLists(entitiesControlPassList, entitiesToRetrieve, numEntities, dumpPeds, dumpVehicles, dumpObjects, *player);

                    for(u32 index = 0; index < numEntities && (numEntitiesDumped < numObjectsToDump); index++)
                    {
                        CEntity *entity = entitiesControlPassList[index];

                        if(entity && ((entity->GetIsTypePed()     && dumpPeds)     ||
                                      (entity->GetIsTypeVehicle() && dumpVehicles) || 
                                      (entity->GetIsTypeObject()  && dumpObjects)))
                        {
                            netObject *networkObject = NetworkUtils::GetNetworkObjectFromEntity(entity);

                            if(entity && networkObject)
                            {
                                unsigned resultCode = CPC_SUCCESS;

                                if(networkObject->CanPassControl(*player, MIGRATE_PROXIMITY, &resultCode))
                                {
                                    NetworkLogUtils::WriteLogEvent(GetLog(), "DUMPING_EXCESS_OBJECT", networkObject->GetLogName());
                                    GetLog().WriteDataValue("Dump reason", dumpReason);
                                    GetLog().WriteDataValue("Dump method", "Passing control");

                                    CGiveControlEvent::Trigger(*player, networkObject, MIGRATE_PROXIMITY);

                                    numEntitiesDumped++;
                                }
                                else
                                {
                                    NetworkLogUtils::WriteLogEvent(GetLog(), "FAILED_TO_DUMP_EXCESS_OBJECT", networkObject->GetLogName());
                                    GetLog().WriteDataValue("Dump reason", dumpReason);
                                    GetLog().WriteDataValue("Dump method", "Passing control");
                                    GetLog().WriteDataValue("Target Player", player->GetLogName());
                                    GetLog().WriteDataValue("Fail Reason", NetworkUtils::GetCanPassControlErrorString(resultCode));
                                }
                            }
                        }
                    }
                }
            }

            gnetAssertf(numEntitiesDumped <= numObjectsToDump, "Dumped an unexpected number of excess objects!");

            // if we can't pass control of objects to any other player or we failed to pass control
            // above we need to dump some objects (Note that the second case should only occur very rarely)
            if(numEntitiesDumped == numObjectsToDump)
            {
                m_TimeAllowedToDeleteExcessObjects = currentTime + m_DelayBeforeDeletingExcessObjects;
                canDelete                          = false;
            }
            else
            {
                canDelete = canDelete && (currentTime >= m_TimeAllowedToDeleteExcessObjects);
            }

            if(canDelete)
            {
                // grab the specified number of entities for each type
                u32 numObjectsReturned   = 0;
                u32 numPedsReturned      = 0;
                u32 numVehiclesReturned  = 0;
                CObject   *objectRemovalList  [MAX_NUM_NETOBJOBJECTS];
                CPed      *pedRemovalList     [MAX_NUM_NETOBJPEDS];
                CVehicle  *vehicleRemovalList [MAX_NUM_NETOBJVEHICLES];

                BuildObjectDeletionLists(objectRemovalList,   entitiesToRetrieve, numObjectsReturned,
                                         pedRemovalList,      entitiesToRetrieve, numPedsReturned,
                                         vehicleRemovalList,  entitiesToRetrieve, numVehiclesReturned);

                // if we want dumping both peds and vehicles prioritise removal based on the current local counts,
                // dumping too many peds can prevent drivers spawning in vehicles etc...
                if(dumpPeds && dumpVehicles)
                {
                    int currentNumLocalPeds     = CNetworkObjectPopulationMgr::GetNumLocalObjects(NET_OBJ_TYPE_PED);
                    int currentNumLocalVehicles = CNetworkObjectPopulationMgr::GetNumLocalVehicles();

                    if(currentNumLocalPeds < (currentNumLocalVehicles - numObjectsToDump))
                    {
                        dumpPeds = false;
                    }
                    else if(currentNumLocalVehicles < (currentNumLocalPeds - numObjectsToDump))
                    {
                        dumpVehicles = false;
                    }
                }

                if(dumpPeds || !dumpVehicles)
                {
                    // if we haven't dumped enough peds dump them next
                    for(u32 pedIndex = 0; pedIndex < numPedsReturned && (numEntitiesDumped < numObjectsToDump); pedIndex++)
                    {
                        CPed *ped = pedRemovalList[pedIndex];

                        dev_float PED_OFFSCREEN_REMOVAL_RANGE = 50.0f;
                        dev_float PED_ONSCREEN_REMOVAL_RANGE  = CNetObjPed::GetStaticScopeData().m_scopeDistance;
                        bool deletePed = !NetworkInterface::IsVisibleOrCloseToAnyPlayer(VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition()), ped->GetBoundRadius(), PED_ONSCREEN_REMOVAL_RANGE, PED_OFFSCREEN_REMOVAL_RANGE);

                        if(deletePed)
                        {
                            NetworkLogUtils::WriteLogEvent(GetLog(), "DUMPING_EXCESS_OBJECT", ped->GetNetworkObject() ? ped->GetNetworkObject()->GetLogName() : "?");
                            GetLog().WriteDataValue("Dump reason", dumpReason);
                            GetLog().WriteDataValue("Dump method", "Object removal");                        
                            GetLog().WriteDataValue("Num Peds Owned", "%d", CNetworkObjectPopulationMgr::GetNumLocalObjects(NET_OBJ_TYPE_PED));
                            GetLog().WriteDataValue("Num Peds Desired", "%d", CNetworkObjectPopulationMgr::GetLocalDesiredNumPeds());

                            gnetAssert(ped->CanBeDeleted());
                            gnetAssert(!ped->IsNetworkClone());
						    ped->FlagToDestroyWhenNextProcessed();

                            numEntitiesDumped++;
                        }
                    }
                }

                // finally start dumping vehicles
                if(dumpVehicles || !dumpPeds)
                {
                    for(u32 vehicleIndex = 0; vehicleIndex < numVehiclesReturned && (numEntitiesDumped < numObjectsToDump); vehicleIndex++)
                    {
                        CVehicle *vehicle = vehicleRemovalList[vehicleIndex];

                        dev_float VEHICLE_OFFSCREEN_REMOVAL_RANGE = 150.0f;
                        dev_float VEHICLE_ONSCREEN_REMOVAL_RANGE  = CNetObjVehicle::GetStaticScopeData().m_scopeDistance;
                        bool deleteVehicle = !NetworkInterface::IsVisibleOrCloseToAnyPlayer(VEC3V_TO_VECTOR3(vehicle->GetTransform().GetPosition()), vehicle->GetBoundRadius(), VEHICLE_ONSCREEN_REMOVAL_RANGE, VEHICLE_OFFSCREEN_REMOVAL_RANGE);

                        if(deleteVehicle)
                        {
                            NetworkLogUtils::WriteLogEvent(GetLog(), "DUMPING_EXCESS_OBJECT", vehicle->GetNetworkObject() ? vehicle->GetNetworkObject()->GetLogName() : "?");
                            GetLog().WriteDataValue("Dump reason", dumpReason);
                            GetLog().WriteDataValue("Dump method", "Object removal");
                            GetLog().WriteDataValue("Num Vehicles Owned", "%d", CNetworkObjectPopulationMgr::GetNumLocalVehicles());
                            GetLog().WriteDataValue("Num Vehicles Desired", "%d", CNetworkObjectPopulationMgr::GetLocalDesiredNumVehicles());

                            gnetAssert(vehicle->CanBeDeleted());
                            gnetAssert(!vehicle->IsNetworkClone());
                            CVehiclePopulation::RemoveExcessNetworkVehicle(vehicle);

                            numEntitiesDumped++;
                        }
                    }
                }

                // always dump some objects if we are struggling for bandwidth
                if(!dumpPeds && !dumpVehicles)
                {
                    for(u32 objectIndex = 0; objectIndex < MIN(numObjectsReturned, numObjectsToDump); objectIndex++)
                    {
                        CObject *object = objectRemovalList[objectIndex];

                        NetworkLogUtils::WriteLogEvent(GetLog(), "DUMPING_EXCESS_OBJECT", object->GetNetworkObject() ? object->GetNetworkObject()->GetLogName() : "?");
                        GetLog().WriteDataValue("Dump reason", dumpReason);
                        GetLog().WriteDataValue("Dump method", "Object removal");

                        gnetAssert(object);
                        gnetAssert(object->CanBeDeleted());
                        gnetAssert(!object->IsNetworkClone());

                        if(object->GetOwnedBy() == ENTITY_OWNEDBY_RANDOM)
                        {
                            NetworkInterface::UnregisterObject(object);
                        }
                        else
                        {
                            CObjectPopulation::DestroyObject(object);
                        }

                     }
                }

                m_TimeAllowedToDeleteExcessObjects = currentTime + m_DelayBeforeDeletingExcessObjects;
            }

            if(IsOwningTooManyObjects())
            {
                if(numEntitiesDumped > 0 || canDelete)
                {
                    CNetworkObjectPopulationMgr::ReduceBandwidthTargetLevels();

                    CNetwork::GetBandwidthManager().ResetAverageFailuresPerUpdate();

                    m_lastTimeObjectDumped = fwTimer::GetSystemTimeInMilliseconds();
                }
            }
        }
    }
}

//
// Name         :   TryToMakeSpaceForObject
// Purpose      :   tries to make space for an object of the specified type
void CNetworkObjectMgr::TryToMakeSpaceForObject(NetworkObjectType objectType)
{
    const atDList<netObject*, datBase> *listsToProcess[MAX_NUM_PHYSICAL_PLAYERS];
    unsigned numListsToProcess = GetLocalObjectLists(listsToProcess);

    for(unsigned index = 0; index < numListsToProcess; index++)
    {
        const atDNode<netObject*, datBase> *node = listsToProcess[index] ? listsToProcess[index]->GetHead() : 0;

        while (node)
        {
            netObject *networkObject = node->Data;

            if(networkObject &&
				networkObject->GetObjectType() == objectType &&
				!networkObject->IsLocalFlagSet(netObject::LOCALFLAG_UNREGISTERING) &&
				!networkObject->IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_RESERVEDOBJECT) && 
				!networkObject->IsPendingOwnerChange() &&
				networkObject->CanDelete())
            {
                CNetObjEntity *netObjEntity = static_cast<CNetObjEntity *>(networkObject);

                if (!netObjEntity->GetEntity() || netObjEntity->GetEntity()->CanBeDeleted())
                {
					bool bNearOrVisibleToPlayers = netObjEntity->GetEntity() && (netObjEntity->GetDistanceToNearestPlayerSquared() < 2500.0f || netObjEntity->GetEntity()->GetIsOnScreen(true));

                    // just try to remove any entity more than 50 meters away from any player
                    // we should really pick the furthest entity from any player, but this keeps things fast
                    if (!bNearOrVisibleToPlayers || objectType == NET_OBJ_TYPE_OBJECT)
                    {
                        if(IsVehicleObjectType(objectType))
                        {
                            CVehicle *vehicle = NetworkUtils::GetVehicleFromNetworkObject(netObjEntity);

                            if(vehicle)
                            {
                                gnetAssert(!vehicle->IsNetworkClone());
                                CVehiclePopulation::RemoveExcessNetworkVehicle(vehicle);
                                return;
                            }
                        }
                        else
                        {
                            switch(objectType)
                            {
                            case NET_OBJ_TYPE_PED:
                                {
                                    CPed *ped = NetworkUtils::GetPedFromNetworkObject(netObjEntity);

                                    if(ped)
                                    {
                                        gnetAssert(!ped->IsNetworkClone());
                                        CPedFactory::GetFactory()->DestroyPed(ped);
                                        return;
                                    }
                                }
                                break;
                            case NET_OBJ_TYPE_OBJECT:
                                {
                                    CObject *object = NetworkUtils::GetCObjectFromNetworkObject(netObjEntity);

                                    if(object)
                                    {
                                        gnetAssert(!object->IsNetworkClone());

                                        if(object->GetOwnedBy() == ENTITY_OWNEDBY_RANDOM || object->GetOwnedBy() == ENTITY_OWNEDBY_TEMP)
                                        {
                                            NetworkInterface::UnregisterObject(object);
                                        }
                                        else if (!bNearOrVisibleToPlayers)
                                        {
                                            CObjectPopulation::DestroyObject(object);
                                        }
                                        return;
                                    }
									else if (!netObjEntity->IsScriptObject())
									{
										UnregisterNetworkObject(netObjEntity, GAME_UNREGISTRATION, false, false);
										return;
									}
                                }
                                break;
                            case NET_OBJ_TYPE_PICKUP:
                                {
                                    CPickup *pickup = NetworkUtils::GetPickupFromNetworkObject(netObjEntity);

                                    if (pickup && !pickup->GetPlacement())
                                    {
                                        gnetAssert(!pickup->IsNetworkClone());

										pickup->Destroy();
 
										return;
                                    }
                                }
                                break;
                            default:
                                {
                                    gnetAssertf(0, "Unsupported object type found!");
                                }
                                break;
                            }
                        }
                    }
                }
            }

            node = node->GetNext();
        }
    }

	// we can be more aggressive with ambient objects: removing clones
	if (objectType == NET_OBJ_TYPE_OBJECT)
	{
		for (u32 player = 0; player < MAX_NUM_PHYSICAL_PLAYERS; player++)
		{
			if (player != netInterface::GetLocalPhysicalPlayerIndex())
			{
				const atDNode<netObject*, datBase> *node = m_aPlayerObjectList[player].GetHead();

				while (node)
				{
					netObject *networkObject = node->Data;

					if(networkObject &&
						networkObject->GetObjectType() == NET_OBJ_TYPE_OBJECT &&
						!networkObject->IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_RESERVEDOBJECT) && 
						networkObject->CanDelete())
					{
						CObject *object = NetworkUtils::GetCObjectFromNetworkObject(networkObject);

						if (!object || object->GetOwnedBy() == ENTITY_OWNEDBY_RANDOM)
						{
							UnregisterNetworkObject(networkObject, GAME_UNREGISTRATION, false, false);
							return;
						}
					}

					node = node->GetNext();
				}
			}
		}
	}
}

//
// Name         :   UpdateIsOwningTooManyObjects
// Purpose      :   update the is owning too many object flag
void CNetworkObjectMgr::UpdateIsOwningTooManyObjects()
{
    u32 averageFailuresPerUpdate = CNetwork::GetBandwidthManager().GetAverageFailuresPerUpdate();
    u32 numStarvingObjects       = GetNumStarvingObjects();

    bool tooManyObjectCriteriaMet = false;
    bool tooManySyncFailures      = false;
    bool tooManyStarvingObjects   = false;

    if(m_isOwningTooManyObjects)
    {
        if((averageFailuresPerUpdate < (m_syncFailureThreshold>>1)) && (numStarvingObjects == 0))
        {
            gnetDebug1("We are no longer owning too many objects");
            m_isOwningTooManyObjects = false;
        }
    }
    else
    {
        if(averageFailuresPerUpdate > m_syncFailureThreshold)
        {   
            tooManyObjectCriteriaMet = true;
            tooManySyncFailures      = true;
        }

        if(numStarvingObjects > m_maxStarvingObjectThreshold)
        {
            tooManyObjectCriteriaMet = true;
            tooManyStarvingObjects   = true;
        }
    }

    if(tooManyObjectCriteriaMet)
    {
        m_TooManyObjectsCountdown -= MIN(m_TooManyObjectsCountdown, fwTimer::GetSystemTimeStepInMilliseconds());

        if(m_TooManyObjectsCountdown == 0)
        {
            m_isOwningTooManyObjects = true;

            if(tooManySyncFailures)
            {
                gnetDebug1("We are owning too many objects! Too many sync failures per update (%d)!", averageFailuresPerUpdate);
            }
            else if(tooManyStarvingObjects)
            {
                gnetDebug1("We are owning too many objects! Too many starving objects (%d)!", numStarvingObjects);
            }
        }
    }
    else
    {
        m_TooManyObjectsCountdown = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("TOO_MANY_OBJECTS_WAIT_TIME", 0x2ce1b489), static_cast<int>(DEFAULT_TOO_MANY_OBJECTS_WAIT_TIME));
    }
}

//
// Name         :   GetNumLocalObjectsPendingOwnerChange
// Purpose      :   returns the number of locally owned objects that are pending ownership change to another machine
u32 CNetworkObjectMgr::GetNumLocalObjectsPendingOwnerChange()
{
    u32 numObjectsPendingOwnerChange = 0;

    const atDList<netObject*, datBase> *listsToProcess[MAX_NUM_PHYSICAL_PLAYERS];
    unsigned numListsToProcess = GetLocalObjectLists(listsToProcess);

    for(unsigned index = 0; index < numListsToProcess; index++)
    {
        const atDNode<netObject*, datBase> *node = listsToProcess[index] ? listsToProcess[index]->GetHead() : 0;

        while (node)
        {
            netObject *networkObject = node->Data;

            if(networkObject && networkObject->IsPendingOwnerChange())
            {
                if (RemotePlayerCanAcceptObjects(*networkObject->GetPendingPlayerOwner(), false))
                {
                    numObjectsPendingOwnerChange++;
                }
            }

            node = node->GetNext();
        }
    }

    return numObjectsPendingOwnerChange;
}

// returns the creation failure type for the specified network object type
CNetworkObjectMgr::CreationFailureTypes CNetworkObjectMgr::GetCreationFailureTypeFromObjectType(NetworkObjectType objectType)
{
    if(IsVehicleObjectType(objectType))
    {
        return CREATION_FAILURE_VEHICLE;
    }
    else
    {
        switch(objectType)
        {
        case NET_OBJ_TYPE_PED:
        case NET_OBJ_TYPE_PLAYER:
                return CREATION_FAILURE_PED;
        case NET_OBJ_TYPE_OBJECT:
        case NET_OBJ_TYPE_PICKUP:
        case NET_OBJ_TYPE_PICKUP_PLACEMENT:
		case NET_OBJ_TYPE_DOOR:
		case NET_OBJ_TYPE_GLASS_PANE:
            return CREATION_FAILURE_OBJECT;            
        default:
            return MAX_CREATION_FAILURE_TYPES;
        }
    }
}

const char *CNetworkObjectMgr::GetUnregistrationReason(unsigned reason) const
{
    switch(reason)
    {
    case CLONE_CREATE_EXISTING_OBJECT:
        return "Received create for existing object";
    case CLONE_REMOVAL:
        return "Clone removal";
    case CLONE_REMOVAL_WRONG_OWNER:
        return "Clone removal (wrong owner)";
	case CLEANUP_SCRIPT_OBJECTS:
		return "Cleaned up script object";
	case DUPLICATE_SCRIPT_OBJECT:
		return "Duplicate script object";
    case OWNERSHIP_CONFLICT:
        return "Ownership conflict";
	case EXPIRED_RESPAWN_PED:
		return "Expired respawn ped";
	case SCRIPT_CLONE_ONLY:
		return "Script clone only flag set and no participants left";
	case SCRIPT_UNREGISTRATION:
#if __FINAL
		return "Script unregistration";
#else
		return "Script unregistration (NETWORK_UNREGISTER_NETWORKED_ENTITY)";
#endif // __FINAL
	case AMBIENT_OBJECT_OWNERSHIP_CONFLICT:
		return "Ambient object ownership conflict";
   default:
        return netObjectMgrBase::GetUnregistrationReason(reason);
    }
}

void CNetworkObjectMgr::HandleTelemetryMetrics() const
{
	if (GetReassignMgr().IsReassignmentInProgress())
	{
		bool isReassignmentTimeOutEnabled = Tunables::GetInstance().TryAccess(MP_GLOBAL_HASH, ATSTRINGHASH("MP_IS_REASSIGNMENT_TIME_OUT_ENABLED", 0x02d6b080), false);
		if (isReassignmentTimeOutEnabled)
		{
			u32 reassignmentTimeOut = Tunables::GetInstance().TryAccess(MP_GLOBAL_HASH, ATSTRINGHASH("MP_REASSIGNMENT_TIME_OUT_MS", 0x80ee076d), 300000); // 5 minutes by default
			if (GetReassignMgr().GetReassignmentRunningTimeMs() >= reassignmentTimeOut)
			{
				int restartCount = GetReassignMgr().GetRestartNumber();
				int scriptCount = GetReassignMgr().GetNumObjectsBeingReassigned(PredicateIsScript);
				int ambCount = GetReassignMgr().GetNumObjectsBeingReassigned(PredicateIsAmbient);
				CNetworkTelemetry::AppendObjectReassignFailed(GetReassignMgr().GetNegotiationStartTime(), restartCount, ambCount, scriptCount);
			}
		}
	}
}

#if __BANK || __DEV
static bool gSyncDataValidationFailed = false;

// Name         :   ValidateSyncData
// Purpose      :   helper function for netObjectMgrBase::ValidateDeltaBufferSizes()
template<class T> static void ValidateSyncData(const char *className, CProjectSyncTree* pSyncTree)
{
	T syncDataClass;

	if (syncDataClass.GetBufferSize() != pSyncTree->GetMaxSizeOfBufferDataInBytes())
	{
		if (syncDataClass.GetBufferSize() < pSyncTree->GetMaxSizeOfBufferDataInBytes())
		{
#if __DEV && !RAGE_MINIMAL_ASSERTS
			gnetAssertf(0, "%s's buffer size is too small \"%d\" - should be %d", className, syncDataClass.GetBufferSize(), pSyncTree->GetMaxSizeOfBufferDataInBytes());
#else
			Errorf("%s's buffer size is too small \"%d\" - should be %d", className, syncDataClass.GetBufferSize(), pSyncTree->GetMaxSizeOfBufferDataInBytes());
#endif
			gSyncDataValidationFailed = true;
		}
	}

	if (syncDataClass.GetNumSyncDataUnits() != pSyncTree->GetNumSyncUpdateNodes())
	{
		if(syncDataClass.GetNumSyncDataUnits() < pSyncTree->GetNumSyncUpdateNodes())
		{
#if __DEV && !RAGE_MINIMAL_ASSERTS
			gnetAssertf(0, "%s's num sync data nodes are too small \"%d\" - should be %d", className, syncDataClass.GetNumSyncDataUnits(), pSyncTree->GetNumSyncUpdateNodes());
#else
			Errorf("%s's num sync data nodes are too small \"%d\" - should be %d", className, syncDataClass.GetNumSyncDataUnits(), pSyncTree->GetNumSyncUpdateNodes());
#endif
			gSyncDataValidationFailed = true;
		}
	}
}

void CNetworkObjectMgr::ValidateSyncDatas()
{
    CProjectSyncTree* pPedSyncTree              = CNetObjPed::GetStaticSyncTree();
    CProjectSyncTree* pPlayerSyncTree           = CNetObjPlayer::GetStaticSyncTree();
    CProjectSyncTree* pObjectSyncTree           = CNetObjObject::GetStaticSyncTree();
    CProjectSyncTree* pPickupSyncTree           = CNetObjPickup::GetStaticSyncTree();
    CProjectSyncTree* pPickupPlacementSyncTree  = CNetObjPickupPlacement::GetStaticSyncTree();
    CProjectSyncTree* pAutomobileSyncTree       = CNetObjAutomobile::GetStaticSyncTree();
    CProjectSyncTree* pBikeSyncTree             = CNetObjBike::GetStaticSyncTree();
    CProjectSyncTree* pBoatSyncTree             = CNetObjBoat::GetStaticSyncTree();
    CProjectSyncTree* pHeliSyncTree             = CNetObjHeli::GetStaticSyncTree();
	CProjectSyncTree* pSubmarineSyncTree        = CNetObjSubmarine::GetStaticSyncTree();
	CProjectSyncTree* pPlaneSyncTree			= CNetObjPlane::GetStaticSyncTree();
	CProjectSyncTree* pDoorSyncTree				= CNetObjDoor::GetStaticSyncTree();
#if GLASS_PANE_SYNCING_ACTIVE
	CProjectSyncTree* pGlassPaneSyncTree		= CNetObjGlassPane::GetStaticSyncTree();
#endif /* GLASS_PANE_SYNCING_ACTIVE */

    gSyncDataValidationFailed = false;

	ValidateSyncData<CNetObjDoor::CDoorSyncData>						("CDoorSyncData",				pDoorSyncTree);
#if GLASS_PANE_SYNCING_ACTIVE
	ValidateSyncData<CNetObjGlassPane::CGlassPaneSyncData>				("CGlassPaneSyncData",			pGlassPaneSyncTree);
#endif /* GLASS_PANE_SYNCING_ACTIVE */
    ValidateSyncData<CNetObjPed::CPedSyncData>                          ("CPedSyncData",                pPedSyncTree);
    ValidateSyncData<CNetObjPlayer::CPlayerSyncData>                    ("CPlayerSyncData",             pPlayerSyncTree);
    ValidateSyncData<CNetObjObject::CObjectSyncData>                    ("CObjectSyncData",             pObjectSyncTree);
    ValidateSyncData<CNetObjPickup::CPickupSyncData>                    ("CPickupSyncData",             pPickupSyncTree);
    ValidateSyncData<CNetObjPickupPlacement::CPickupPlacementSyncData>  ("CPickupPlacementSyncData",    pPickupPlacementSyncTree);
    ValidateSyncData<CNetObjVehicle::CVehicleSyncData>                  ("CVehicleSyncData",            pAutomobileSyncTree);
    ValidateSyncData<CNetObjVehicle::CVehicleSyncData>                  ("CVehicleSyncData",            pBikeSyncTree);
	ValidateSyncData<CNetObjVehicle::CVehicleSyncData>                  ("CVehicleSyncData",            pBoatSyncTree);
	ValidateSyncData<CNetObjVehicle::CVehicleSyncData>                  ("CVehicleSyncData",            pSubmarineSyncTree);
	ValidateSyncData<CNetObjVehicle::CVehicleSyncData>                  ("CVehicleSyncData",            pHeliSyncTree);
	ValidateSyncData<CNetObjVehicle::CVehicleSyncData>                  ("CVehicleSyncData",            pPlaneSyncTree);

#if __DEV
	NETWORK_QUITF(!gSyncDataValidationFailed, "One or more sync data buffers is too small! The network game cannot continue!");
#else
	if (gSyncDataValidationFailed)
	{
		Quitf("One or more sync data buffers is too small! The network game cannot continue!");
	}
#endif

    gnetDebug2("Ped                 tree buffer size = %d, num update data nodes = %d\r\n", pPedSyncTree->GetMaxSizeOfBufferDataInBytes(),              pPedSyncTree->GetNumSyncUpdateNodes());
    gnetDebug2("Player              tree buffer size = %d, num update data nodes = %d\r\n", pPlayerSyncTree->GetMaxSizeOfBufferDataInBytes(),           pPlayerSyncTree->GetNumSyncUpdateNodes());
    gnetDebug2("Object              tree buffer size = %d, num update data nodes = %d\r\n", pObjectSyncTree->GetMaxSizeOfBufferDataInBytes(),           pObjectSyncTree->GetNumSyncUpdateNodes());
    gnetDebug2("Pickup              tree buffer size = %d, num update data nodes = %d\r\n", pPickupSyncTree->GetMaxSizeOfBufferDataInBytes(),           pPickupSyncTree->GetNumSyncUpdateNodes());
    gnetDebug2("Pickup placement    tree buffer size = %d, num update data nodes = %d\r\n", pPickupPlacementSyncTree->GetMaxSizeOfBufferDataInBytes(),  pPickupPlacementSyncTree->GetNumSyncUpdateNodes());
    gnetDebug2("Automobile          tree buffer size = %d, num update data nodes = %d\r\n", pAutomobileSyncTree->GetMaxSizeOfBufferDataInBytes(),       pAutomobileSyncTree->GetNumSyncUpdateNodes());
    gnetDebug2("Bike                tree buffer size = %d, num update data nodes = %d\r\n", pBikeSyncTree->GetMaxSizeOfBufferDataInBytes(),             pBikeSyncTree->GetNumSyncUpdateNodes());
	gnetDebug2("Boat                tree buffer size = %d, num update data nodes = %d\r\n", pBoatSyncTree->GetMaxSizeOfBufferDataInBytes(),             pBoatSyncTree->GetNumSyncUpdateNodes());
	gnetDebug2("Submarine           tree buffer size = %d, num update data nodes = %d\r\n", pSubmarineSyncTree->GetMaxSizeOfBufferDataInBytes(),             pSubmarineSyncTree->GetNumSyncUpdateNodes());
    gnetDebug2("Heli                tree buffer size = %d, num update data nodes = %d\r\n", pHeliSyncTree->GetMaxSizeOfBufferDataInBytes(),             pHeliSyncTree->GetNumSyncUpdateNodes());
	gnetDebug2("Plane               tree buffer size = %d, num update data nodes = %d\r\n", pPlaneSyncTree->GetMaxSizeOfBufferDataInBytes(),             pPlaneSyncTree->GetNumSyncUpdateNodes());
	gnetDebug2("Door                tree buffer size = %d, num update data nodes = %d\r\n", pDoorSyncTree->GetMaxSizeOfBufferDataInBytes(),             pDoorSyncTree->GetNumSyncUpdateNodes());
#if GLASS_PANE_SYNCING_ACTIVE
	gnetDebug2("Glass Pane          tree buffer size = %d, num update data nodes = %d\r\n", pGlassPaneSyncTree->GetMaxSizeOfBufferDataInBytes(),             pGlassPaneSyncTree->GetNumSyncUpdateNodes());
#endif /* GLASS_PANE_SYNCING_ACTIVE */
    gnetDebug2("Sizeof(CNetObjPlayer) = %" SIZETFMT "u\r\n",            sizeof(CNetObjPlayer));
    gnetDebug2("Sizeof(CNetObjPed) = %" SIZETFMT "u\r\n",               sizeof(CNetObjPed));
    gnetDebug2("Sizeof(CNetObjObject) = %" SIZETFMT "u\r\n",            sizeof(CNetObjObject));
    gnetDebug2("Sizeof(CNetObjPickup) = %" SIZETFMT "u\r\n",            sizeof(CNetObjPickup));
    gnetDebug2("Sizeof(CNetObjPickupPlacement) = %" SIZETFMT "u\r\n",   sizeof(CNetObjPickupPlacement));
    gnetDebug2("Sizeof(CNetObjVehicle) = %" SIZETFMT "u\r\n",           sizeof(CNetObjVehicle));
    gnetDebug2("Sizeof(CNetObjAutomobile) = %" SIZETFMT "u\r\n",        sizeof(CNetObjAutomobile));
    gnetDebug2("Sizeof(CNetObjBike) = %" SIZETFMT "u\r\n",              sizeof(CNetObjBike));
    gnetDebug2("Sizeof(CNetObjBoat) = %" SIZETFMT "u\r\n",              sizeof(CNetObjBoat));
    gnetDebug2("Sizeof(CNetObjHeli) = %" SIZETFMT "u\r\n",              sizeof(CNetObjHeli));
	gnetDebug2("Sizeof(CNetObjPlane) = %" SIZETFMT "u\r\n",             sizeof(CNetObjPlane));
	gnetDebug2("Sizeof(CNetObjSubmarine) = %" SIZETFMT "u\r\n",         sizeof(CNetObjSubmarine));
    gnetDebug2("Sizeof(CNetObjTrailer) = %" SIZETFMT "u\r\n",           sizeof(CNetObjTrailer));
    gnetDebug2("Sizeof(CNetObjTrain) = %" SIZETFMT "u\r\n",             sizeof(CNetObjTrain));
	gnetDebug2("Sizeof(CNetObjDoor) = %" SIZETFMT "u\r\n",              sizeof(CNetObjDoor));
#if GLASS_PANE_SYNCING_ACTIVE
	gnetDebug2("Sizeof(CNetObjGlassPane) = %" SIZETFMT "u\r\n",         sizeof(CNetObjGlassPane));
#endif /* GLASS_PANE_SYNCING_ACTIVE */
    gnetDebug2("Sizeof(CPlayerSyncData) = %" SIZETFMT "u\r\n",          sizeof(CNetObjPlayer::CPlayerSyncData));
    gnetDebug2("Sizeof(CPedSyncData) = %" SIZETFMT "u\r\n",             sizeof(CNetObjPed::CPedSyncData));
    gnetDebug2("Sizeof(CObjectSyncData) = %" SIZETFMT "u\r\n",          sizeof(CNetObjObject::CObjectSyncData));
    gnetDebug2("Sizeof(CPickupSyncData) = %" SIZETFMT "u\r\n",          sizeof(CNetObjPickup::CPickupSyncData));
    gnetDebug2("Sizeof(CPickupPlacementSyncData) = %" SIZETFMT "u\r\n", sizeof(CNetObjPickupPlacement::CPickupPlacementSyncData));
    gnetDebug2("Sizeof(CVehicleSyncData) = %" SIZETFMT "u\r\n",         sizeof(CNetObjVehicle::CVehicleSyncData));
#if GLASS_PANE_SYNCING_ACTIVE
	gnetDebug2("Sizeof(CGlassPaneSyncData) = %" SIZETFMT "u\r\n",       sizeof(CNetObjGlassPane::CGlassPaneSyncData));
#endif /* GLASS_PANE_SYNCING_ACTIVE */
	gnetDebug2("Sizeof(CDoorSyncData) = %" SIZETFMT "u\r\n",			sizeof(CNetObjDoor::CDoorSyncData));
}
#endif

#if __DEV

void CNetworkObjectMgr::ValidateObjectsClonedState(const netPlayer& ASSERT_ONLY(player))
{
    const atDList<netObject*, datBase> *listsToProcess[MAX_NUM_PHYSICAL_PLAYERS];
    unsigned numListsToProcess = GetLocalObjectLists(listsToProcess);

    for(unsigned index = 0; index < numListsToProcess; index++)
    {
        const atDNode<netObject*, datBase> *node = listsToProcess[index] ? listsToProcess[index]->GetHead() : 0;

        while (node)
        {
		    netObject *networkObject = node->Data;
		    if(networkObject)
		    {
			    gnetAssertf(!networkObject->IsPendingCloning(player), "ValidateObjectsClonedState :: Object %s Pending Cloning for %s!", networkObject->GetLogName(), player.GetLogName());
			    gnetAssertf(!networkObject->HasBeenCloned(player), "ValidateObjectsClonedState :: Object %s Has Been Cloned for %s!", networkObject->GetLogName(), player.GetLogName());
			    gnetAssertf(!networkObject->IsPendingRemoval(player), "ValidateObjectsClonedState :: Object %s Pending Removal for %s!", networkObject->GetLogName(), player.GetLogName());
		    }
		    node = node->GetNext();
        }
	}
}

void CNetworkObjectMgr::SanityCheckObjectUsage()
{
    unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
    const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

    for(unsigned index = 0; index < numPhysicalPlayers; index++)
    {
        const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);

        const atDNode<netObject*, datBase> *node = m_aPlayerObjectList[pPlayer->GetPhysicalPlayerIndex()].GetHead();

        while(node)
        {
            netObject *pNetObj = node->Data;
            node = node->GetNext();

            for (PhysicalPlayerIndex p=0; p<MAX_NUM_PHYSICAL_PLAYERS; p++)
            {
                netPlayer* pPlayer2 = NetworkInterface::GetPhysicalPlayerFromIndex(p);

                if (pPlayer2 && pPlayer2->IsValid())
                {
                    const atDNode<netObject*, datBase> *pNode2 = m_aPlayerObjectList[pPlayer2->GetPhysicalPlayerIndex()].GetHead();

                    while (pNode2)
                    {
                        netObject* pNetObj2 = pNode2->Data;
                        pNode2 = pNode2->GetNext();

                        if (pNetObj != pNetObj2)
                        {
                            if(pNetObj->GetObjectID() == pNetObj2->GetObjectID())
                            {
                                netPlayer *pPlayer = pNetObj->GetPlayerOwner();
                                GetLog().Log("\t#### ObjectID %d owned by two different players! ####\r\n", pNetObj->GetObjectID());
                                GetLog().Log("\t\t>> Player %s: object %s\r\n", pPlayer ? pPlayer->GetLogName() : "NONE", pNetObj->GetLogName());
                                GetLog().Log("\t\t>> Player %s: object %s\r\n", pPlayer2->GetLogName(), pNetObj2->GetLogName());

                                gnetAssertf(0, "ObjectID %d owned by two different players!", pNetObj->GetObjectID());
                            }
                        }
                    }
                }
            }

            // also check for objects left behind in object list
            if (pNetObj->IsLocalFlagSet(netObject::LOCALFLAG_UNREGISTERING))
            {
                if (!pNetObj->IsPendingCloning() &&
                    !pNetObj->HasBeenCloned() &&
                    !pNetObj->IsPendingRemoval())
                {
                    gnetAssertf(false, "Unregistering network object not cleaned up");

                    GetLog().Log("\t#### Unregistering network object not cleaned up ####\r\n");
                    GetLog().Log("\t\t>> Object : %s\r\n", pNetObj->GetLogName());
                }
            }

            // also check cloned state
            for (PhysicalPlayerIndex p=0; p<MAX_NUM_PHYSICAL_PLAYERS; p++)
            {
                netPlayer* pPlayer2 = NetworkInterface::GetPhysicalPlayerFromIndex(p);

                if (pPlayer2 && pPlayer2->IsValid())
                {
                    if (pNetObj->HasBeenCloned(*pPlayer2))
                    {
                        gnetAssert(!pNetObj->IsPendingCloning(*pPlayer2));
                        gnetAssert(!pNetObj->IsPendingRemoval(*pPlayer2));

                        if(pNetObj->IsPendingCloning(*pPlayer2))
                        {
                            GetLog().Log("\t#### Object pending cloning on connection is already cloned! ####\r\n");
                            GetLog().Log("\t\t>> Object : %s\r\n", pNetObj->GetLogName());
                            GetLog().Log("\t\t>> Player : %s\r\n", pPlayer2->GetLogName());
                        }
                        if(pNetObj->IsPendingRemoval(*pPlayer2))
                        {
                            GetLog().Log("\t#### Object pending removal on connection is still marked as cloned! ####\r\n");
                            GetLog().Log("\t\t>> Object : %s\r\n", pNetObj->GetLogName());
                            GetLog().Log("\t\t>> Player : %s\r\n", pPlayer2->GetLogName());
                        }
                    }
                    else if (pNetObj->IsPendingCloning(*pPlayer2))
                    {
                        gnetAssert(!pNetObj->IsPendingRemoval(*pPlayer2));

                        if(pNetObj->IsPendingRemoval(*pPlayer2))
                        {
                            GetLog().Log("\t#### Object pending cloning on connection is marked as pending removal! ####\r\n");
                            GetLog().Log("\t\t>> Object : %s\r\n", pNetObj->GetLogName());
                            GetLog().Log("\t\t>> Player : %s\r\n", pPlayer2->GetLogName());
                        }
                    }
                }
            }
        }
    }

	// we do not do this when players are in a transitional state - as the object counts are grabbed
	// using the physical player list. There may be a short time where players have been moved off the 
	// physical list before being removed.
	unsigned numExistingPlayers = netInterface::GetNumActivePlayers() + netInterface::GetNumPendingPlayers();
	if(numExistingPlayers == numPhysicalPlayers)
	{
		// ensure we aren't leaking any network objects
		int numPlayers   = CNetworkObjectPopulationMgr::GetTotalNumObjectsOfType(NET_OBJ_TYPE_PLAYER);
		int numPeds      = CNetworkObjectPopulationMgr::GetTotalNumObjectsOfType(NET_OBJ_TYPE_PED);
		int numVehicles  = CNetworkObjectPopulationMgr::GetTotalNumVehicles();
		int numObjects   = CNetworkObjectPopulationMgr::GetTotalNumObjectsOfType(NET_OBJ_TYPE_OBJECT);

		int nObjectsInPool   = CNetObjObject::GetPool()->GetNoOfUsedSpaces();
		int nPedsInPool      = CNetObjPed::GetPool()->GetNoOfUsedSpaces();
		int nVehiclesInPool  = CNetObjVehicle::GetPool()->GetNoOfUsedSpaces();

		if(numObjects != nObjectsInPool)
		{
			DisplayLeakedCObjects();
		}

		if((numPlayers + numPeds) != nPedsInPool)
		{
			DisplayLeakedPeds();
		}

		if(numVehicles != nVehiclesInPool)
		{
			DisplayLeakedVehicles();
		}

		gnetAssertf((numObjects)           == nObjectsInPool, " (numObjects=\"%d\") != (nObjectsInPool=\"%d\")", numObjects, nObjectsInPool);
		gnetAssertf((numPlayers + numPeds) == nPedsInPool, "(numPlayers=\"%d\" + numPeds=\"%d\") != (nPedsInPool=\"%d\")", numPlayers, numPeds, nPedsInPool);
		gnetAssertf((numVehicles)          == nVehiclesInPool, "(numVehicles=\"%d\") != (nVehiclesInPool=\"%d\")", numVehicles, nVehiclesInPool);
	}
}

//
// name:        DisplayLeakedCObjects
// description: searches the network object pools to display leaked CObjects
//
void CNetworkObjectMgr::DisplayLeakedCObjects()
{
    if (!NetworkInterface::GetPlayerMgr().IsInitialized())
        return;

    s32 index = CNetObjObject::GetPool()->GetSize();

    while(index--)
    {
        CNetObjObject *object = CNetObjObject::GetPool()->GetSlot(index);

        if(object)
        {
            bool found = false;

            unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
            const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

            for(unsigned index = 0; index < numPhysicalPlayers; index++)
            {
                const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);

                const atDNode<netObject*, datBase> *node = m_aPlayerObjectList[pPlayer->GetPhysicalPlayerIndex()].GetHead();

                while(node && !found)
                {
                    if (node->Data == object)
                    {
                        found = true;
                    }

                    node = node->GetNext();
                }
            }

            if(!found)
            {
                found = GetReassignMgr().IsObjectOnReassignmentList(*object);
            }

            if(!found)
            {
                NetworkLogUtils::WritePlayerText(GetLog(), NetworkInterface::GetPlayerMgr().GetMyPhysicalPlayerIndex(), "OBJECT_LEAK!", object->GetLogName());
            }
        }
    }
}

//
// name:        DisplayLeakedPeds
// description: searches the network object pools to display leaked peds
//
void CNetworkObjectMgr::DisplayLeakedPeds()
{
    if (!NetworkInterface::GetPlayerMgr().IsInitialized())
        return;

    s32 index = CNetObjPed::GetPool()->GetSize();

    while(index--)
    {
        CNetObjPed *ped = CNetObjPed::GetPool()->GetSlot(index);

        if(ped)
        {
            bool found = false;

            unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
            const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

            for(unsigned index = 0; index < numPhysicalPlayers; index++)
            {
                const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);

                const atDNode<netObject*, datBase> *node = m_aPlayerObjectList[pPlayer->GetPhysicalPlayerIndex()].GetHead();

                while(node && !found)
                {
                    if (node->Data == ped)
                    {
                        found = true;
                    }

                    node = node->GetNext();
                }
            }

            if(!found)
            {
                found = GetReassignMgr().IsObjectOnReassignmentList(*ped);
            }

            if(!found)
            {
                NetworkLogUtils::WritePlayerText(GetLog(), NetworkInterface::GetPlayerMgr().GetMyPhysicalPlayerIndex(), "OBJECT_LEAK!", ped->GetLogName());
            }
        }
    }

	index = CPed::GetPool()->GetSize();

	u32 numLeakedPeds = 0;

	while(index--)
	{
		CPed *ped = CPed::GetPool()->GetSlot(index);

		if (ped && !ped->GetNetworkObject())
		{
			numLeakedPeds++;
		}
	}

	gnetAssertf(numLeakedPeds==0,  "%d peds exist without a network object", numLeakedPeds); 

}

//
// name:        DisplayLeakedVehicles
// description: searches the network object pools to display leaked vehicles
//
void CNetworkObjectMgr::DisplayLeakedVehicles()
{
    if (!NetworkInterface::GetPlayerMgr().IsInitialized())
        return;

    s32 index = CNetObjVehicle::GetPool()->GetSize();

    while(index--)
    {
        CNetObjVehicle *vehicle = CNetObjVehicle::GetPool()->GetSlot(index);

        if(vehicle)
        {
            bool found = false;

            unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
            const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

            for(unsigned index = 0; index < numPhysicalPlayers; index++)
            {
                const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);

                const atDNode<netObject*, datBase> *node = m_aPlayerObjectList[pPlayer->GetPhysicalPlayerIndex()].GetHead();

                while(node && !found)
                {
                    if (node->Data == vehicle)
                    {
                        found = true;
                    }

                    node = node->GetNext();
                }
            }

            if(!found)
            {
                found = GetReassignMgr().IsObjectOnReassignmentList(*vehicle);
            }

            if(!found)
            {
                NetworkLogUtils::WritePlayerText(GetLog(), NetworkInterface::GetPlayerMgr().GetMyPhysicalPlayerIndex(), "OBJECT_LEAK!", vehicle->GetLogName());
            }
        }
    }

	index = (s32) CVehicle::GetPool()->GetSize();

	u32 numLeakedVehs = 0;

	while(index--)
	{
		CVehicle *veh = CVehicle::GetPool()->GetSlot(index);

		if (veh && !veh->GetNetworkObject())
		{
			numLeakedVehs++;
		}
	}

	gnetAssertf(numLeakedVehs==0,  "%d vehicles exist without a network object", numLeakedVehs); 
}

#endif // __DEV

void CNetworkObjectMgr::UpdateAfterAllMovement()
{
    unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
    const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

    for(unsigned index = 0; index < numPhysicalPlayers; index++)
    {
        const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);

        const atDNode<netObject*, datBase> *node = m_aPlayerObjectList[pPlayer->GetPhysicalPlayerIndex()].GetHead();

        while(node)
        {
			CNetObjGame* pGameObj = static_cast<CNetObjGame*>(node->Data);

			if (pGameObj->GetEntity())
			{
				static_cast<CNetObjEntity*>(pGameObj)->UpdateAfterAllMovement();
			}

			node = node->GetNext();
		}
	}
}

// Name         :   UpdateAfterScripts
// Purpose      :   updates all objects after the scripts have been processed
void CNetworkObjectMgr::UpdateAfterScripts()
{
	unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
    const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

    for(unsigned index = 0; index < numPhysicalPlayers; index++)
    {
        const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);

        const atDNode<netObject*, datBase> *node = m_aPlayerObjectList[pPlayer->GetPhysicalPlayerIndex()].GetHead();

        while(node)
        {
			CNetObjGame* pGameObj = static_cast<CNetObjGame*>(node->Data);

			if (pGameObj->GetEntity())
			{
				static_cast<CNetObjEntity*>(pGameObj)->UpdateAfterScripts();
			}

			node = node->GetNext();
		}
	}
}

// Name         :   DisplayObjectInfo
// Purpose      :   Displays object info above all network objects
void CNetworkObjectMgr::DisplayObjectInfo()
{
    if (CPauseMenu::IsActive())
        return;

    unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
    const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

    for(unsigned index = 0; index < numPhysicalPlayers; index++)
    {
        const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);

        const atDNode<netObject*, datBase> *node = m_aPlayerObjectList[pPlayer->GetPhysicalPlayerIndex()].GetHead();

        while(node)
        {
		     node->Data->DisplayNetworkInfo();
	         node = node->GetNext();
        }
    }
}

// hides all objects for a cutscene immediately
void CNetworkObjectMgr::HideAllObjectsForCutscene()
{
	unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
	const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

	for(unsigned index = 0; index < numPhysicalPlayers; index++)
	{
		const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);

		const atDNode<netObject*, datBase> *node = m_aPlayerObjectList[pPlayer->GetPhysicalPlayerIndex()].GetHead();

		while(node)
		{
			if (node->Data->GetEntity())
			{
				static_cast<CNetObjEntity*>(node->Data)->HideForCutscene();
			}

			node = node->GetNext();
		}
	}
}

// exposes all hidden objects after a cutscene
void CNetworkObjectMgr::ExposeAllObjectsAfterCutscene()
{
	unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
    const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

    for(unsigned index = 0; index < numPhysicalPlayers; index++)
    {
        const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);

        const atDNode<netObject*, datBase> *node = m_aPlayerObjectList[pPlayer->GetPhysicalPlayerIndex()].GetHead();

        while(node)
        {
			if (node->Data->GetEntity())
			{
				static_cast<CNetObjEntity*>(node->Data)->ExposeAfterCutscene();
			}

			node = node->GetNext();
		}
	}
}

// possbly detects duplicate script objects and returns false if this one cannot be accepted
bool CNetworkObjectMgr::CanAcceptScriptObject(CNetObjGame& object, const CGameScriptObjInfo& objectScriptInfo)
{
	if (!object.HasGameObject() && objectScriptInfo.IsScriptHostObject())
	{
		unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
		const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

		for(unsigned index = 0; index < numPhysicalPlayers; index++)
		{
			const netPlayer *player = allPhysicalPlayers[index];

			atDNode<netObject*, datBase> *node = m_aPlayerObjectList[player->GetPhysicalPlayerIndex()].GetHead();

			while (node)
			{
				CNetObjGame *thisObject = SafeCast(CNetObjGame, node->Data);

				if (thisObject && thisObject->HasGameObject() && thisObject->IsScriptObject())
				{
					const scriptObjInfoBase *thisScriptInfo = thisObject->GetScriptObjInfo();

					if (thisScriptInfo && (*thisScriptInfo == objectScriptInfo))
					{
						gnetAssertf(thisScriptInfo->GetHostToken() != objectScriptInfo.GetHostToken(), "Two duplicate script objects exist with the same script object id and host token (%s and %s)", thisObject->GetLogName(), object.GetLogName() );

						if (thisScriptInfo->GetHostToken() > objectScriptInfo.GetHostToken())
						{		
							CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, a duplicate script object created by a more recent host exists (%s)\r\n", thisObject->GetLogName());

							const netPlayer* pHostOfScript = scriptInterface::GetHostOfScript(objectScriptInfo.GetScriptId());

							if (pHostOfScript && pHostOfScript->IsLocal())
							{
								// tell the owner to delete the object
								CMarkAsNoLongerNeededEvent::Trigger(object, true);
							}

							return false;
						}
						else if (!thisObject->IsClone() && thisObject->CanDelete())
						{
							NetworkLogUtils::WriteLogEvent(*CTheScripts::GetScriptHandlerMgr().GetLog(), "REMOVE_DUPLICATE", "%s", thisScriptInfo->GetScriptId().GetLogName());

							CTheScripts::GetScriptHandlerMgr().GetLog()->WriteDataValue("Removing", thisObject->GetLogName());
							CTheScripts::GetScriptHandlerMgr().GetLog()->WriteDataValue("Replacement", object.GetLogName());

							netInterface::GetObjectManager().UnregisterNetworkObject(thisObject, CNetworkObjectMgr::DUPLICATE_SCRIPT_OBJECT, false, true);

							return true;
						}
						else 
						{
							CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, waiting for a duplicate script object created by a previous host to be removed (%s)\r\n", thisObject->GetLogName());

							if (thisObject->GetEntity())
							{
								SafeCast(CNetObjEntity, thisObject)->FlagForDeletion();
							}

							const netPlayer* pHostOfScript = scriptInterface::GetHostOfScript(objectScriptInfo.GetScriptId());

							if (pHostOfScript && pHostOfScript->IsLocal())
							{
								// tell the owner to delete the object
								CMarkAsNoLongerNeededEvent::Trigger(object, true);
							}

							return false;
						}
					}
				}

                node = node->GetNext();
			}
		}
	}

	return true;
}

void CNetworkObjectMgr::GetPlayersNearbyInScope(PlayerFlags& playersNearbyInPedScope, PlayerFlags& playersNearbyInVehScope, PlayerFlags& playersNearbyInObjScope, Vector3* pScopePosition)
{
	playersNearbyInPedScope = 0;
	playersNearbyInVehScope = 0;
	playersNearbyInObjScope = 0;

	Vector3 centrePos = pScopePosition ? *pScopePosition : NetworkInterface::GetLocalPlayerFocusPosition();

	unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
	const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

	for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
	{
		const netPlayer *remotePlayer = remotePhysicalPlayers[index];
		u32 playerIndex = (u32)remotePlayer->GetPhysicalPlayerIndex();

		if (!NetworkInterface::ArePlayersInDifferentTutorialSessions(*NetworkInterface::GetLocalPlayer(), (CNetGamePlayer&)*remotePlayer))
		{
			Vector3 remotePlayerPos = NetworkInterface::GetPlayerFocusPosition(*remotePlayer);
			Vector3 diff = remotePlayerPos - centrePos;
			float dist = diff.Mag2();

			float pedScope = CNetObjPed::GetStaticScopeData().m_scopeDistance * 1.1f;
			float vehicleScope = CNetObjVehicle::GetStaticScopeData().m_scopeDistance * 1.1f;
			float objectScope = CNetObjObject::GetStaticScopeData().m_scopeDistance * 1.1f;

			if (dist <= vehicleScope*vehicleScope)
			{
				playersNearbyInVehScope |= (1<<playerIndex);

				if (dist <= pedScope*pedScope)
				{
					playersNearbyInPedScope |= (1<<playerIndex);
				}

				if (dist <= objectScope*objectScope)
				{
					playersNearbyInObjScope |= (1<<playerIndex);
				}	
			}
		}
	}
}


#if __BANK

void CNetworkObjectMgr::AddDebugWidgets()
{
    bkBank *bank = BANKMGR.FindBank("Network");

    if(gnetVerifyf(bank, "Unable to find network bank!"))
    {
        bank->PushGroup("Debug Object Management", false);
            bank->AddToggle("Display object ID info",                 &s_displayObjectIdDebug);
            bank->AddToggle("Display object type counts",             &s_displayObjectTypeCounts);
			bank->AddToggle("Display object Reserved type counts",    &s_displayObjectTypeReserveCounts);
			bank->AddToggle("Display Parked Car Gen",                 &s_displayObjectCarGen);
			bank->AddToggle("Display object target levels",           &s_displayObjectTargetLevels);
            bank->AddToggle("Display object sync failure info",       &s_displayObjectSyncFailureInfo);
            bank->AddToggle("Display object physics debug info",      &s_displayObjectPhysicsDebug);
            bank->AddToggle("Display object reassignment debug info", &s_displayObjectReassignmentDebug);
            CNetworkObjectPopulationMgr::AddDebugWidgets();
            netObjectReassignMgr::AddDebugWidgets();
        bank->PopGroup();
    }
}

static unsigned GetNumParkedCars()
{
    unsigned numParkedCars = 0;

    CNetObjVehicle::Pool &pool = *CNetObjVehicle::GetPool();

	s32 i = pool.GetSize();

    while(i--)
	{
		CNetObjVehicle *netObjVehicle = pool.GetSlot(i);

		if(netObjVehicle && netObjVehicle->GetVehicle() && netObjVehicle->GetVehicle()->PopTypeGet() == POPTYPE_RANDOM_PARKED)
		{
            numParkedCars++;
        }
    }

    return numParkedCars;
}

void CNetworkObjectMgr::DisplayDebugInfo()
{
    if(s_displayObjectIdDebug)
    {
        grcDebugDraw::AddDebugOutput("Num Free IDs   : %d", GetObjectIDManager().GetNumFreeObjectIDs());
        grcDebugDraw::AddDebugOutput("");
    }

    if(s_displayObjectTypeCounts)
    {
        int numPlayers          = CNetworkObjectPopulationMgr::GetNumLocalObjects(NET_OBJ_TYPE_PLAYER) +
                                  CNetworkObjectPopulationMgr::GetNumRemoteObjects(NET_OBJ_TYPE_PLAYER);
        int numPeds             = CNetworkObjectPopulationMgr::GetNumLocalObjects(NET_OBJ_TYPE_PED) +
                                  CNetworkObjectPopulationMgr::GetNumRemoteObjects(NET_OBJ_TYPE_PED);
        int numVehicles         = CNetworkObjectPopulationMgr::GetNumLocalVehicles() +
                                  CNetworkObjectPopulationMgr::GetNumRemoteVehicles();
        int numObjects          = CNetworkObjectPopulationMgr::GetNumLocalObjects(NET_OBJ_TYPE_OBJECT) +
                                  CNetworkObjectPopulationMgr::GetNumRemoteObjects(NET_OBJ_TYPE_OBJECT);
        int numDoors            = CNetworkObjectPopulationMgr::GetNumLocalObjects(NET_OBJ_TYPE_DOOR) +
                                  CNetworkObjectPopulationMgr::GetNumRemoteObjects(NET_OBJ_TYPE_DOOR);
        int numPickups          = CNetworkObjectPopulationMgr::GetNumLocalObjects(NET_OBJ_TYPE_PICKUP) +
                                  CNetworkObjectPopulationMgr::GetNumRemoteObjects(NET_OBJ_TYPE_PICKUP);
        int numPickupPlacements = CNetworkObjectPopulationMgr::GetNumLocalObjects(NET_OBJ_TYPE_PICKUP_PLACEMENT) +
                                  CNetworkObjectPopulationMgr::GetNumRemoteObjects(NET_OBJ_TYPE_PICKUP_PLACEMENT);
        int numGlassPanes       = CNetworkObjectPopulationMgr::GetNumLocalObjects(NET_OBJ_TYPE_GLASS_PANE) +
                                  CNetworkObjectPopulationMgr::GetNumRemoteObjects(NET_OBJ_TYPE_GLASS_PANE);
        int	numAmbientVehicles  = CVehiclePopulation::ms_numRandomCars;
        int numAmbientPeds      = CPedPopulation::ms_nNumOnFootCop + CPedPopulation::ms_nNumOnFootSwat + CPedPopulation::ms_nNumOnFootAmbient;
   

        grcDebugDraw::AddDebugOutput("Players                 : %d", numPlayers);
        grcDebugDraw::AddDebugOutput("Peds                    : %d (in vehicle, including players: %d, scenario: %d, ambient: %d)", numPeds, GetNumLocalPedsInVehicles(), CPedPopulation::ms_nNumOnFootScenario, numAmbientPeds);
        grcDebugDraw::AddDebugOutput("Vehicles                : %d (with pretend occupants (local): %d, ambient: %d, parked: %d)", numVehicles, GetNumLocalVehiclesUsingPretendOccupants(), numAmbientVehicles, GetNumParkedCars());
        grcDebugDraw::AddDebugOutput("Objects                 : %d", numObjects);
        grcDebugDraw::AddDebugOutput("Doors                   : %d", numDoors);
        grcDebugDraw::AddDebugOutput("Pickups                 : %d", numPickups);
        grcDebugDraw::AddDebugOutput("Pickup Placements : %d", numPickupPlacements);
        grcDebugDraw::AddDebugOutput("Glass Panes           : %d", numGlassPanes);
        grcDebugDraw::AddDebugOutput("");
        grcDebugDraw::AddDebugOutput("Last Fail : %s", GetLastRegistrationFailureReason());
    }

	if (s_displayObjectTypeReserveCounts)
	{
		//adjust object allowance based on reservations from external systems
		u32 numReservedObjects  = 0;
		u32 numReservedPeds     = 0;
		u32 numReservedVehicles = 0;

		CTheScripts::GetScriptHandlerMgr().GetNumRequiredScriptEntities(numReservedPeds, numReservedVehicles, numReservedObjects);
		grcDebugDraw::AddDebugOutput("Script Reserved Peds      : %d", numReservedPeds);
		grcDebugDraw::AddDebugOutput("Script Reserved Vehicles  : %d", numReservedVehicles);
		grcDebugDraw::AddDebugOutput("Script Reserved Objects   : %d", numReservedObjects);

		CNetworkObjectPopulationMgr::GetNumExternallyRequiredEntities(numReservedPeds, numReservedVehicles, numReservedObjects);
		grcDebugDraw::AddDebugOutput("Externally Reserved Peds      : %d", numReservedPeds);
		grcDebugDraw::AddDebugOutput("Externally Reserved Vehicles  : %d", numReservedVehicles);
		grcDebugDraw::AddDebugOutput("Externally Reserved Objects   : %d", numReservedObjects);
		grcDebugDraw::AddDebugOutput("");
	}

	if (s_displayObjectCarGen)
	{
		s32	MaxNumParkedCars = CPopCycle::GetCurrentMaxNumParkedCars();
		if (CVehiclePopulation::ms_overrideNumberOfParkedCars >= 0)
		{
			MaxNumParkedCars = CVehiclePopulation::ms_overrideNumberOfParkedCars;
		}

		s32	TotalCarsOnMap = CVehiclePopulation::GetTotalVehsOnMap();

		grcDebugDraw::AddDebugOutput("Max Num of Cars In Use   : %d", CVehiclePopulation::ms_maxNumberOfCarsInUse);
		grcDebugDraw::AddDebugOutput("Total Cars On Map        : %d", TotalCarsOnMap);
		grcDebugDraw::AddDebugOutput("Can create cars          : %s", TotalCarsOnMap >= CVehiclePopulation::ms_maxNumberOfCarsInUse ? "False" : "True");
		grcDebugDraw::AddDebugOutput("");
		grcDebugDraw::AddDebugOutput("Max Num Parked Cars      : %d", MaxNumParkedCars);
		grcDebugDraw::AddDebugOutput("Total Cars Parked On Map : %d", CVehiclePopulation::ms_numParkedCars);
		grcDebugDraw::AddDebugOutput("Can create parked cars   : %s", CVehiclePopulation::ms_numParkedCars >= MaxNumParkedCars ? "False" : "True");
		grcDebugDraw::AddDebugOutput("");
	}

    if(s_displayObjectTargetLevels)
    {
        // output script reservations
        u32 numUnusedReservedObjects         = 0;
        u32 numUnusedReservedPeds            = 0;
        u32 numUnusedReservedVehicles        = 0;
        CTheScripts::GetScriptHandlerMgr().GetNumRequiredScriptEntities(numUnusedReservedPeds, numUnusedReservedVehicles, numUnusedReservedObjects);

        grcDebugDraw::AddDebugOutput("");
        grcDebugDraw::AddDebugOutput("Target levels (target/actual) (total/desired)");
        grcDebugDraw::AddDebugOutput("Peds      : %d/%d (%d/%d) -> Bandwidth %d (Max %d) -> script reserved %d", CNetworkObjectPopulationMgr::GetLocalTargetNumPeds(),      CNetworkObjectPopulationMgr::GetNumLocalObjects(NET_OBJ_TYPE_PED), CNetworkObjectPopulationMgr::GetTotalNumObjectsOfType(NET_OBJ_TYPE_PED), CNetworkObjectPopulationMgr::GetLocalDesiredNumPeds(), CNetworkObjectPopulationMgr::GetBandwidthTargetNumPeds(), CNetworkObjectPopulationMgr::GetMaxBandwidthTargetNumPeds(), numUnusedReservedPeds);
        grcDebugDraw::AddDebugOutput("Vehicles  : %d/%d (%d/%d) -> Bandwidth %d (Max %d) -> script reserved %d", CNetworkObjectPopulationMgr::GetLocalTargetNumVehicles(),  CNetworkObjectPopulationMgr::GetNumLocalVehicles(), CNetworkObjectPopulationMgr::GetTotalNumVehicles(), CNetworkObjectPopulationMgr::GetLocalDesiredNumVehicles(), CNetworkObjectPopulationMgr::GetBandwidthTargetNumVehicles(), CNetworkObjectPopulationMgr::GetMaxBandwidthTargetNumVehicles(), numUnusedReservedVehicles);
        grcDebugDraw::AddDebugOutput("Objects   : %d/%d (%d/%d) -> Bandwidth %d (Max %d) -> script reserved %d", CNetworkObjectPopulationMgr::GetLocalTargetNumObjects(),   CNetworkObjectPopulationMgr::GetNumLocalObjects(NET_OBJ_TYPE_OBJECT), CNetworkObjectPopulationMgr::GetTotalNumObjectsOfType(NET_OBJ_TYPE_OBJECT), CNetworkObjectPopulationMgr::GetLocalDesiredNumObjects(), CNetworkObjectPopulationMgr::GetBandwidthTargetNumObjects(), CNetworkObjectPopulationMgr::GetMaxBandwidthTargetNumObjects(), numUnusedReservedObjects);
    }

    if(s_displayObjectSyncFailureInfo)
    {
        DisplayObjectSyncFailureInfo();
    }

    if(s_displayObjectPhysicsDebug)
    {
        //DisplayPhysicsData();
		BANK_ONLY(NetworkDebug::PrintPhysicsInfo());
    }

    if(s_displayObjectReassignmentDebug)
    {
        GetReassignMgr().DisplayDebugInfo();
    }
}

void CNetworkObjectMgr::DisplayPhysicsData()
{
    u32 numActiveLocal   = 0;
    u32 numActiveRemote  = 0;
    u32 numLocalObjects  = 0;
    u32 numRemoteObjects = 0;

    unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
    const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

    for(unsigned index = 0; index < numPhysicalPlayers; index++)
    {
        const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);

        const atDNode<netObject*, datBase> *node = m_aPlayerObjectList[pPlayer->GetPhysicalPlayerIndex()].GetHead();

        while(node)
        {
            if(node->Data && node->Data->GetEntity() && node->Data->GetEntity()->GetIsPhysical())
            {
                CPhysical *physical = static_cast<CPhysical *>(node->Data->GetEntity());

                if(physical)
                {
                    if(physical->IsNetworkClone())
                    {
                        if(physical->GetCurrentPhysicsInst() && CPhysics::GetLevel()->IsActive(physical->GetCurrentPhysicsInst()->GetLevelIndex()))
                        {
                            if(!physical->IsAsleep())
                            {
                                numActiveRemote++;
                            }
                        }

                        numRemoteObjects++;
                    }
                    else
                    {
                        if(physical->GetCurrentPhysicsInst() && CPhysics::GetLevel()->IsActive(physical->GetCurrentPhysicsInst()->GetLevelIndex()))
                        {
                            if(!physical->IsAsleep())
                            {
                                numActiveLocal++;
                            }
                        }

                        numLocalObjects++;
                    }
                }
            }

            node = node->GetNext();
        }
    }

    grcDebugDraw::AddDebugOutput("Local  Physics Info : active %d/%d (%.2f%%)", numActiveLocal,  numLocalObjects,  (numLocalObjects > 0)  ? (static_cast<float>(numActiveLocal)  / numLocalObjects)  : 1.0f);
    grcDebugDraw::AddDebugOutput("Remote Physics Info : active %d/%d (%.2f%%)", numActiveRemote, numRemoteObjects, (numRemoteObjects > 0) ? (static_cast<float>(numActiveRemote) / numRemoteObjects) : 1.0f);
}

// Name         :   DisplayObjectSyncFailureInfo
// Purpose      :   display the names of the objects that have failed to sync
void CNetworkObjectMgr::DisplayObjectSyncFailureInfo()
{
    static int START_LINE_Y = 27;
    char str[100];
    int debugTextX = 7;
    int lineNo = START_LINE_Y;

    sprintf(str, "Target bandwidth is : %d Kbps", CNetwork::GetBandwidthManager().GetTargetUpstreamBandwidth());
    grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);

    u32      largestUnackedCount  = 0;
    u32      oldestResend         = 0;
    netSequence oldestResendSeq      = 0;

    unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
    const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

    for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
    {
        const netPlayer *remotePlayer = remotePhysicalPlayers[index];
        const int        cxnId        = remotePlayer->GetConnectionId();

        if(CNetwork::GetConnectionManager().GetUnAckedCount(cxnId) > largestUnackedCount)
        {
            largestUnackedCount = CNetwork::GetConnectionManager().GetUnAckedCount(cxnId);
        }

        if(CNetwork::GetConnectionManager().GetOldestResendCount(cxnId) > oldestResend)
        {
            oldestResend    = CNetwork::GetConnectionManager().GetOldestResendCount(cxnId);
            oldestResendSeq = CNetwork::GetConnectionManager().GetSequenceOfOldestResendCount(cxnId);
        }
    }

    sprintf(str, "Unacked count / Oldest resend is : %d / %d (%d)", largestUnackedCount, oldestResend, oldestResendSeq);
    grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);
    lineNo++;

    sprintf(str, "Average num bandwidth send failures per update : %d  (current %d)", CNetwork::GetBandwidthManager().GetAverageFailuresPerUpdate(), CNetwork::GetBandwidthManager().GetNumFailuresPerUpdate());
    grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);
    sprintf(str, "Num outgoing reliables per second : %d", NetworkDebug::GetNumOutgoingReliables());
    grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);
    lineNo++;

    u32 timeSinceLastObjectDumped = fwTimer::GetSystemTimeInMilliseconds() - m_lastTimeObjectDumped;
    sprintf(str, "Time since last object dumped : %d)", timeSinceLastObjectDumped);
    grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);
    lineNo++;

    if(IsOwningTooManyObjects())
    {
        sprintf(str, "I am owning too many objects!");
        grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);
    }

    for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
    {
        const netPlayer *remotePlayer = remotePhysicalPlayers[index];

        sprintf(str, "Unreliable resend count is %d", CNetwork::GetBandwidthManager().GetUnreliableResendCount(*remotePlayer));

        if(!RemotePlayerCanAcceptObjects(*remotePlayer, false))
        {
            sprintf(str, "%s    %s cannot accept objects!", str, remotePlayer->GetLogName());
        }

        grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);
    }

    const atDList<netObject*, datBase> *listsToProcess[MAX_NUM_PHYSICAL_PLAYERS];
    unsigned numListsToProcess = GetLocalObjectLists(listsToProcess);

    for(unsigned index = 0; index < numListsToProcess; index++)
    {
        const atDNode<netObject*, datBase> *node = listsToProcess[index] ? listsToProcess[index]->GetHead() : 0;

        while (node)
        {
            netObject *networkObject = node->Data;
            node = node->GetNext();

            if(networkObject && networkObject->IsStarving())
            {
                sprintf(str, "%s is starving!", networkObject->GetLogName());
                grcDebugDraw::PrintToScreenCoors(str, debugTextX, lineNo++);
            }
        }
    }
}

const char *CNetworkObjectMgr::GetLastRegistrationFailureReason()
{
    switch(m_eLastRegistrationFailureReason)
    {
    case REGISTRATION_SUCCESS:
        return "Registration successful!";
    case REGISTRATION_FAIL_POPULATION:
        return "This machine owns too many objects of this type!";
    case REGISTRATION_FAIL_OBJECTIDS:
        return "Not enough free object IDs!";
    case REGISTRATION_FAIL_MISSION_OBJECT_PENDING:
        return "There is a mission object of this type pending registration!";
    case REGISTRATION_FAIL_TOO_MANY_OBJECTS:
        return "Too many objects!";
    default:
        gnetAssertf(0, "Unknown last registration failure reason!");
        return "Unknown last registration failure reason!";
    }
}

#endif // __BANK

bool CNetworkObjectMgr::AddInvalidObjectModel(int modelHashKey)
{
	bool result = m_invalidObjectModelMap.SafeInsert(modelHashKey, 0);
	m_invalidObjectModelMap.FinishInsertion();

#if ENABLE_NETWORK_LOGGING
	{
		fwModelId modelId;
		CModelInfo::GetBaseModelInfoFromHashKey(modelHashKey, &modelId);
		NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "ADD_INVALID_OBJECT_MODEL", CModelInfo::GetBaseModelInfoName(modelId));
	}
#endif // ENABLE_NETWORK_LOGGING

	return result;
}

bool CNetworkObjectMgr::RemoveInvalidObjectModel(int modelHashKey)
{
	bool result = m_invalidObjectModelMap.Has(modelHashKey);
	if(result)
	{
		m_invalidObjectModelMap.RemoveKey(modelHashKey);

#if ENABLE_NETWORK_LOGGING
		{
			fwModelId modelId;
			CModelInfo::GetBaseModelInfoFromHashKey(modelHashKey, &modelId);
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "REMOVE_INVALID_OBJECT_MODEL", CModelInfo::GetBaseModelInfoName(modelId));
		}
#endif // ENABLE_NETWORK_LOGGING
	}

	return result;
}

void CNetworkObjectMgr::ClearInvalidObjectModels()
{
	m_invalidObjectModelMap.Reset();
#if ENABLE_NETWORK_LOGGING
	{
		NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "CLEAR_INVALID_OBJECT_MODELS", "");
	}
#endif // ENABLE_NETWORK_LOGGING
}

void CNetworkObjectMgr::CheckForInvalidObject(const netPlayer& fromPlayer, netObject* networkObject)
{
	if(s_sendInvalidObjectModelMetrics)
	{
		CBaseModelInfo *modelInfo = nullptr;
		NetworkObjectType netType = networkObject->GetObjectType();
		if(netType == NET_OBJ_TYPE_PED)
		{
			CPed* pPed = static_cast<CNetObjPed*>(networkObject)->GetPed();
			if (pPed) modelInfo = pPed->GetBaseModelInfo();
		}
		else if(netType == NET_OBJ_TYPE_PICKUP)
		{
			CPickup* pPickup = static_cast<CNetObjPickup*>(networkObject)->GetPickup();
			if (pPickup) modelInfo = pPickup->GetBaseModelInfo();
		}
		else if(netType == NET_OBJ_TYPE_OBJECT)
		{
			CObject* pObject = static_cast<CNetObjObject*>(networkObject)->GetCObject();
			if (pObject) modelInfo = pObject->GetBaseModelInfo();
		}
		else if(IsVehicleObjectType(netType))
		{
			CVehicle* pVehicle = ((CNetObjVehicle*)networkObject)->GetVehicle();
			if (pVehicle) modelInfo = pVehicle->GetBaseModelInfo();
		}

		if(modelInfo)
		{
			u32 modelHash = modelInfo->GetHashKey();
			if(gnetVerifyf(modelHash != 0, "CNetworkObjectMgr::CheckForInvalidObject network object missing model info %s", networkObject->GetLogName()))
			{
				bool result = m_invalidObjectModelMap.Has(modelHash);
				if(result)
				{
					CNetworkTelemetry::AppendMetricReportInvalidModelCreated(fromPlayer.GetGamerInfo().GetGamerHandle(), modelHash, InvalidObjectReportReason::INVALID_OBJECT_MODEL);
					gnetAssertf(0, "Received clone create for invalid model type: %s (%s)", networkObject->GetLogName(), modelInfo->GetModelName());
				}
				else if (m_InstancedContentModelAllowList.GetCount() > 0 && !m_InstancedContentModelAllowList.Has(modelHash))
				{
					CNetworkTelemetry::AppendMetricReportInvalidModelCreated(fromPlayer.GetGamerInfo().GetGamerHandle(), modelHash, InvalidObjectReportReason::NOT_ALLOWED_IN_INSTANCED_CONTENT);
					gnetAssertf(0, "Received clone create for model type not in instanced content allow list: %s (%s)", networkObject->GetLogName(), modelInfo->GetModelName());
				}
			}
		}
	}
}

bool CNetworkObjectMgr::AddModelToInstancedContentAllowList(int modelHashKey)
{
    bool result = m_InstancedContentModelAllowList.SafeInsert(modelHashKey, 0);
    m_InstancedContentModelAllowList.FinishInsertion();

#if ENABLE_NETWORK_LOGGING
    if (result)
    {
        netLoggingInterface* scriptManagerLog = CTheScripts::GetScriptHandlerMgr().GetLog();
        if (scriptManagerLog)
        {
            NetworkLogUtils::WriteLogEvent(*scriptManagerLog, "ADDING_MODEL_TO_INSTANCED_CONTENT_ALLOW_LIST", "Model Hash: %d", modelHashKey);
        }
    }
#endif // ENABLE_NETWORK_LOGGING

    return result;
}

void CNetworkObjectMgr::RemoveAllModelsFromInstancedContentAllowList()
{
#if ENABLE_NETWORK_LOGGING
    netLoggingInterface* scriptManagerLog = CTheScripts::GetScriptHandlerMgr().GetLog();
    if (scriptManagerLog)
    {
        NetworkLogUtils::WriteLogEvent(*scriptManagerLog, "CLEARING_INSTANCED_CONTENT_MODEL_ALLOW_LIST", "Removing %u items from the list", m_InstancedContentModelAllowList.GetCount());
    }
#endif // ENABLE_NETWORK_LOGGING

    m_InstancedContentModelAllowList.Reset();
}

#if ENABLE_NETWORK_LOGGING

bool CNetworkObjectMgr::IsObjectTooCloseForCreationOrRemoval(netObject *networkObject, float &distance)
{
    gnetAssert(networkObject);

    if(!m_enableLoggingToCatchCloseEntities)
        return false;

    if(networkObject && networkObject->GetEntity() && networkObject->GetEntity()->m_nFlags.bInMloRoom)
        return false;

    bool tooClose = false;

	Vector3 myPlayerPos = NetworkInterface::GetLocalPlayerFocusPosition();

    if(networkObject && networkObject->GetEntity())
    {
        Vector3 delta = myPlayerPos - VEC3V_TO_VECTOR3(networkObject->GetEntity()->GetTransform().GetPosition());

        distance = delta.Mag();

        NetworkObjectType objectType = networkObject->GetObjectType();

        if(IsVehicleObjectType(objectType))
        {
            tooClose = /*(delta.Mag2() < 400.0f) || */(networkObject->GetEntity()->GetIsOnScreen() && delta.Mag2() < rage::square(150.0f));
        }
        else if(objectType == NET_OBJ_TYPE_PED)
        {
            tooClose = /*(delta.Mag2() < 400.0f) || */(networkObject->GetEntity()->GetIsOnScreen() && delta.Mag2() < rage::square(25.0f));
        }
    }

    return tooClose;
}

bool CNetworkObjectMgr::IsObjectTooFarForCreation(netObject *networkObject, float &distance)
{
	// we only care about cars just now
	if (networkObject->GetObjectType() != NET_OBJ_TYPE_AUTOMOBILE)
		return false;

	if (static_cast<CNetObjGame*>(networkObject)->IsScriptObject())
		return false;

	if(networkObject && networkObject->GetEntity() && networkObject->GetEntity()->m_nFlags.bInMloRoom)
		return false;

	bool tooFar = false;

	Vector3 myPlayerPos = NetworkInterface::GetLocalPlayerFocusPosition();

	if(networkObject && networkObject->GetEntity())
	{
		Vector3 delta = myPlayerPos - VEC3V_TO_VECTOR3(networkObject->GetEntity()->GetTransform().GetPosition());

		distance = delta.Mag();

		tooFar = distance > static_cast<CNetObjPhysical*>(networkObject)->GetScopeData().m_scopeDistance;
	}

	return tooFar;
}


#endif // ENABLE_NETWORK_LOGGING

// NETWORK BOTS FUNCTIONALITY
bool CNetworkObjectMgr::RegisterNetworkBotGameObject(PhysicalPlayerIndex        botPhysicalPlayerIndex,
                                                     netGameObjectBase *gameObject,
                                                     const NetObjFlags  localFlags,
                                                     const NetObjFlags  globalFlags)
{
    gnetAssert(!gameObject->GetNetworkObject());

    // create the network object corresponding to this game object
    ObjectId objectID = GetObjectIDManager().GetNewObjectId();

    netObject* pNetObj = gameObject->CreateNetworkObject( objectID,
                                                          botPhysicalPlayerIndex,
                                                          localFlags,
                                                          globalFlags,
                                                          MAX_NUM_PHYSICAL_PLAYERS);
    RegisterNetworkObject(pNetObj);

    gnetAssert(gameObject->GetNetworkObject());

    return true;
}

// END NETWORK BOTS FUNCTIONALITY


void CNetworkObjectMgr::CleanupScriptObjects(const CGameScriptId& scrId, bool bRemove)
{
    const atDList<netObject*, datBase> *listsToProcess[MAX_NUM_PHYSICAL_PLAYERS];
    unsigned numListsToProcess = GetLocalObjectLists(listsToProcess);

    for(unsigned index = 0; index < numListsToProcess; index++)
    {
        const atDNode<netObject*, datBase> *node = listsToProcess[index] ? listsToProcess[index]->GetHead() : 0;

        while (node)
        {
            CNetObjGame *networkObject = SafeCast(CNetObjGame, node->Data);
		    node = node->GetNext();

            if (networkObject &&
                networkObject->GetScriptObjInfo() &&
                networkObject->GetScriptObjInfo()->GetScriptId() == scrId)
            {
			    if (bRemove)
			    {
				    UnregisterNetworkObject(networkObject, CLEANUP_SCRIPT_OBJECTS, false, true);
			    }
			    else
			    {
				    networkObject->ConvertToAmbientObject();
			    }
            }
        }
    }
}
